/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <stdlib.h>
#include <time.h>

#include <boost/atomic.hpp>
#include <boost/memory_order.hpp>

#include "core.hpp"
#include "exception.hpp"
#include "landscape.hpp"


/* TODO remove debug?  (or something) */
#define PROTAGONIST_CELL_DEBUG 1

/* More debugging */
#define CELL_BOUNDARIES_IN_RED 1


/* AFK_DisplayedLandscapeCell workers. */

static void computeFlatTriangle(
    float *vertices,
    float *colours,
    const Vec3<unsigned int>& indices,
    struct AFK_VcolPhongVertex *triangleVPos)
{
    Vec3<float> triVs[3];
    Vec3<float> triCs[3];
    struct AFK_VcolPhongVertex *outStructs[3];
    unsigned int i;

    for (i = 0; i < 3; ++i)
    {
        triVs[i].fromArray(&vertices[indices.v[i]]);
        triCs[i].fromArray(&colours[indices.v[i]]);
        outStructs[i] = triangleVPos + i;
    }

    Vec3<float> crossP = ((triVs[1] - triVs[0]).cross(triVs[2] - triVs[0]));

    /* Try to sort out the winding order so that under GL_CCW,
     * all triangles are facing upwards
     */
    if (crossP.v[1] >= 0.0f)
    {
        outStructs[1] = triangleVPos + 2;
        outStructs[2] = triangleVPos + 1;
    }   

    Vec3<float> normal = crossP.normalise();

    for (i = 0; i < 3; ++i)
    {
        triVs[i].toArray(&outStructs[i]->location[0]);
        triCs[i].toArray(&outStructs[i]->colour[0]);
        normal.toArray(&outStructs[i]->normal[0]);
    }
}

/* Turns a vertex grid into a landscape of flat triangles.
 * (Each triangle has different vertices because the normals
 * are different, so no index array is used.)
 * Call with NULL `triangleVs' to find out how large the
 * triangles array ought to be.
 */
static void vertices2FlatTriangles(
    float *vertices,
    float *colours,
    unsigned int verticesCount,
    unsigned int pointSubdivisionFactor,
    struct AFK_VcolPhongVertex *triangleVs,
    unsigned int *triangleVsCount)
{
    /* Each vertex generates 2 triangles (i.e. 6 triangle vertices) when
     * combined with the 3 vertices adjacent to it.
     */
    unsigned int expectedTriangleVsCount = SQUARE(pointSubdivisionFactor) * 6;

    if (!triangleVs)
    {
        /* Just output the required size */
        *triangleVsCount = expectedTriangleVsCount;
    }
    else
    {
        if (*triangleVsCount < expectedTriangleVsCount)
        {
            std::ostringstream ss;
            ss << "vertices2FlatTriangles: supplied " << *triangleVsCount << " triangle vertices (needed " << expectedTriangleVsCount << ")";
            throw AFK_Exception(ss.str());
        }

        *triangleVsCount = expectedTriangleVsCount;

        /* To make the triangles, I chew one row and the next at once.
         * Each triangle pair is:
         * ((row2, col1), (row1, col1), (row1, col2)),
         * ((row2, col1), (row1, col2), (row2, col2)).
         * The grid given to me goes from 0 to pointSubdivisionFactor+1
         * in both directions, so as to join up with adjacent cells.
         */
        struct AFK_VcolPhongVertex *triangleVPos = triangleVs;
        for (unsigned int row = 0; row < pointSubdivisionFactor; ++row)
        {
            for (unsigned int col = 0; col < pointSubdivisionFactor; ++col)
            {
                unsigned int i_r1c1 = 3 * row * (pointSubdivisionFactor+1) + 3 * col;
                unsigned int i_r1c2 = 3 * row * (pointSubdivisionFactor+1) + 3 * (col + 1);
                unsigned int i_r2c1 = 3 * (row + 1) * (pointSubdivisionFactor+1) + 3 * col;
                unsigned int i_r2c2 = 3 * (row + 1) * (pointSubdivisionFactor+1) + 3 * (col + 1);

                Vec3<unsigned int> tri1Is(i_r2c1, i_r1c1, i_r1c2);
                computeFlatTriangle(vertices, colours, tri1Is, triangleVPos);

                triangleVPos += 3;

                Vec3<unsigned int> tri2Is(i_r2c1, i_r1c2, i_r2c2);
                computeFlatTriangle(vertices, colours, tri2Is, triangleVPos);

                triangleVPos += 3;
            }
        }
    }
}


/* Turns a vertex grid into a landscape of curved triangles,
 * producing an array of AFK_VcolPhongVertex and also an
 * index array.
 * Again, call with NULL pointers to find out how big the
 * arrays ought to be.
 */
#if 0
static void vertices2CurvedTriangles(
    float *vertices,
    size_t verticesCount,
    struct AFK_VcolPhongVertex *triangles,
    size_t *io_trianglesCount,
    unsigned int *indices,
    size_t *indicesCount)
{
    /* TODO. */
}
#endif

/* AFK_DisplayedLandscapeCell implementation */

AFK_DisplayedLandscapeCell::AFK_DisplayedLandscapeCell(
    GLuint _program,
    GLuint _worldTransformLocation,
    GLuint _clipTransformLocation,
    const AFK_RealCell& cell,
    unsigned int pointSubdivisionFactor,
    const AFK_Terrain& terrain,
    AFK_RNG& rng)
{
    program = _program;
    worldTransformLocation = _worldTransformLocation;
    clipTransformLocation = _clipTransformLocation;
    shaderLight = new AFK_ShaderLight(_program); /* TODO heap thrashing :P */

    /* Seed the RNG for this cell. */
    rng.seed(cell.worldCell.rngSeed());

    /* I'm going to need to sample the edges of the next cell along
     * the +ve x and z too, in order to join up with it.
     * -- TODO: That needs to change (see: stitching).
     */
    vertexCount = SQUARE(pointSubdivisionFactor + 1);
    float *rawVertices = NULL;
    float *rawColours = NULL;
    unsigned int rawVerticesCount = 3 * vertexCount;

    /* TODO This is thrashing the heap.  Try making a single
     * scratch place for this in thread-local storage
     */
    rawVertices = new float[rawVerticesCount];
    rawColours = new float[rawVerticesCount];

    /* Populate the vertex array. */
    float *rawVerticesPos = rawVertices;

    for (unsigned int xi = 0; xi < pointSubdivisionFactor + 1; ++xi)
    {
        for (unsigned int zi = 0; zi < pointSubdivisionFactor + 1; ++zi)
        {
            /* The geometry in a cell goes from its (0,0,0) point to
             * just before its (coord.v[3], coord.v[3], coord.v[3]
             * point (in cell space)
             */
            float xf = (float)xi * cell.coord.v[3] / (float)pointSubdivisionFactor;
            float zf = (float)zi * cell.coord.v[3] / (float)pointSubdivisionFactor;

            /* Jitter the vertices inside the cell around a bit. 
             * Not the edge ones; that will cause a join-up
             * headache (especially at different levels of detail).
             * TODO Of course, I'm going to have a headache on a
             * transition between LoDs anyway, but I'm trying not
             * to think about that too hard right now :P
             */ 
            float xdisp = 0.0f, zdisp = 0.0f;
#if 0
            if (xi > 0 && xi <= pointSubdivisionFactor)
                xdisp = rng.frand() * cell.coord.v[3] / ((float)pointSubdivisionFactor * 2.0f);

            if (zi > 0 && zi <= pointSubdivisionFactor)
                zdisp = rng.frand() * cell.coord.v[3] / ((float)pointSubdivisionFactor * 2.0f);
#endif

            /* And shunt them into world space */
            *(rawVerticesPos++) = xf + xdisp + cell.coord.v[0];
            *(rawVerticesPos++) = cell.coord.v[1];
            *(rawVerticesPos++) = zf + zdisp + cell.coord.v[2];
        }
    }

    /* Now apply the terrain transform.
     * TODO This may be slow -- obvious candidate for OpenCL?
     * But, the cache may rescue me; profile first!
     */
    for (unsigned int i = 0; i < vertexCount; ++i)
    {
        Vec3<float> rawVertex(
            *(rawVertices + 3 * i),
            *(rawVertices + 3 * i + 1),
            *(rawVertices + 3 * i + 2));

        Vec3<float> rawColour = Vec3<float>(1.0f, 1.0f, 1.0f);

        terrain.compute(rawVertex, rawColour);

        for (unsigned int j = 0; j < 3; ++j)
        {
            *(rawVertices + 3 * i + j) = rawVertex.v[j];
            *(rawColours + 3 * i + j) = rawColour.v[j];
        }
    }

    /* I've completed my vertex array!  Transform this into an
     * array of VcolPhongVertex by computing colours and normals
     */
    struct AFK_VcolPhongVertex *triangleVs = NULL;
    unsigned int triangleVsCount;

    vertices2FlatTriangles(rawVertices, rawColours, rawVerticesCount, pointSubdivisionFactor, NULL, &triangleVsCount);
    triangleVs = new struct AFK_VcolPhongVertex[triangleVsCount];
    vertices2FlatTriangles(rawVertices, rawColours, rawVerticesCount, pointSubdivisionFactor, triangleVs, &triangleVsCount);

    /* I don't need this any more... */
    delete[] rawVertices;
    delete[] rawColours;

    /* Turn this into an OpenGL vertex buffer */
    glGenBuffers(1, &vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glBufferData(GL_ARRAY_BUFFER, triangleVsCount * sizeof(struct AFK_VcolPhongVertex), triangleVs, GL_STATIC_DRAW);
    vertexCount = triangleVsCount;

    delete[] triangleVs;
}

AFK_DisplayedLandscapeCell::~AFK_DisplayedLandscapeCell()
{
    glDeleteBuffers(1, &vertices);
}

void AFK_DisplayedLandscapeCell::display(const Mat4<float>& projection)
{
    if (vertices != 0)
    {
        /* TODO Do I have to do `glUseProgram' once per cell, really?
          * Can't I omit it from here and have the AFK_Landscape
         * object do it before displaying its cells?
         */
        glUseProgram(program);

        updateTransform(projection);

        shaderLight->setupLight(afk_core.sun);

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER, vertices);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(3 * sizeof(float)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(6 * sizeof(float)));

        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
}

std::ostream& operator<<(std::ostream& os, const AFK_DisplayedLandscapeCell& dlc)
{
    return os << "Displayed landscape cell with " << dlc.vertexCount << " vertices";
}


/* AFK_LandscapeCell implementation. */

AFK_LandscapeCell::AFK_LandscapeCell():
    lastSeen (), displayed () {}

AFK_LandscapeCell::AFK_LandscapeCell(const AFK_LandscapeCell& c):
    lastSeen (c.lastSeen), displayed (c.displayed) {}

AFK_LandscapeCell& AFK_LandscapeCell::operator=(const AFK_LandscapeCell& c)
{
    lastSeen = c.lastSeen;
    displayed = c.displayed;
    return *this;
}

bool AFK_LandscapeCell::claim(void)
{
    boost::thread::id myThreadId = boost::this_thread::get_id();

    if (lastSeen == afk_core.renderingFrame) return false;

    /* Grab it */
    lastSeen = afk_core.renderingFrame;
    claimingThreadId = myThreadId;
    boost::atomic_thread_fence(boost::memory_order_seq_cst); /* Gremlin */

    /* Was I the grabber? */
    if (claimingThreadId == myThreadId) return true;

    return false;
}

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeCell& landscapeCell)
{
    /* TODO Something more descriptive might be nice */
    return os << "Landscape cell (last seen " << landscapeCell.lastSeen << ", claimed by " << landscapeCell.claimingThreadId << ")";
}


/* AFK_Landscape implementation */

AFK_Landscape::AFK_Landscape(
    float _maxDistance,
    unsigned int _subdivisionFactor,
    unsigned int _detailPitch):
        renderQueue(10000) /* TODO make a better guess */
{
    maxDistance         = _maxDistance;
    subdivisionFactor   = _subdivisionFactor;
    detailPitch         = _detailPitch;

    /* Set up the landscape shader. */
    shaderProgram = new AFK_ShaderProgram();
    *shaderProgram << "vcol_phong_fragment" << "vcol_phong_vertex";
    shaderProgram->Link();

    worldTransformLocation = glGetUniformLocation(shaderProgram->program, "WorldTransform");
    clipTransformLocation = glGetUniformLocation(shaderProgram->program, "ClipTransform");

    /* TODO Make this a parameter. */
    pointSubdivisionFactor = 8;

    /* Guess at the minimum point separation.
     * Explanation on a piece of paper I'll lose quickly
     * TODO: This changes with window dimensions.  Move
     * this calculation elsewhere so that it can be
     * changed as we go too :/
     */
    float tanHalfFov = tanf((afk_core.config->fov / 2.0f) * M_PI / 180.0f);
    float minPointSeparation = ((float)_detailPitch * afk_core.config->zNear * tanHalfFov) / (float)glutGet(GLUT_WINDOW_HEIGHT);

    /* The target minimum cell size is that multiplied by
     * the point subdivision factor.
     */
    float targetMinCellSize = minPointSeparation * pointSubdivisionFactor;

    /* Now, start with the obvious max cell size and subdivide
     * until I've got a minimum cell size that roughly matches
     * the intended detail pitch.
     */
    maxSubdivisions = 1;

    for (minCellSize = maxDistance; minCellSize > targetMinCellSize; minCellSize = minCellSize / subdivisionFactor)
        ++maxSubdivisions;

    std::cout << "AFK: Landscape using minCellSize " << minCellSize << std::endl;
}

AFK_Landscape::~AFK_Landscape()
{
    delete shaderProgram;
}

#define ENQUEUE_DEBUG_COLOURS 0

void AFK_Landscape::enqueueSubcells(
    const AFK_Cell& cell,
    const AFK_Cell& parent,
    const Vec3<float>& viewerLocation,
    const AFK_Camera& camera,
    bool parentEntirelyVisible)
{
    /* If this cell is outside the given parent, stop right
     * away.
     */
    if (!cell.isParent(parent)) return;

    /* Find out if we've already processed this frame this time
     * around.
     * TODO: I need to change this so that instead of using
     * `renderingFrame', it uses a `computingFrame tracking
     * one step behind, and so on
     */
    AFK_LandscapeCell& landscapeCell = cache[cell];
    if (!landscapeCell.claim()) return;

    /* Is there any terrain here?
     * TODO: If there isn't, I still want to evaluate
     * contained objects, and any terrain geometry that
     * might have "bled" into this cell.  But for now,
     * just dump it.
     */
    if (cell.coord.v[1] != 0)
    {
        ++landscapeCellsEmpty;
    }
    else
    {
        /* Work out the real-world cell that corresponds to this
         * abstract one.
         */
        AFK_RealCell realCell(cell, minCellSize);

        /* Check for visibility. */
        bool someVisible = false;
        bool allVisible = parentEntirelyVisible;

        realCell.testVisibility(camera, someVisible, allVisible);
        if (!someVisible)
        {
            ++landscapeCellsInvisible;

            /* Nothing else to do with it now either. */
        }
        else
        {
            /* Add the terrain for this cell to the terrain vector. */
            Vec3<float> *debugColour = NULL;

#if ENQUEUE_DEBUG_COLOURS
            /* Here's an LoD one... (apparently) */
#if 0
            Vec3<float> tint =
                cell.coord.v[3] == MIN_CELL_PITCH ? Vec3<float>(0.0f, 0.0f, 1.0f) :
                cell.coord.v[3] == MIN_CELL_PITCH * subdivisionFactor ? Vec3<float>(0.0f, 1.0f, 0.0f) :
                Vec3<float>(1.0f, 0.0f, 0.0f);
#else
            /* And here's a location one... */
            Vec3<float> tint(
                0.0f,
                fmod(realCell.coord.v[0], 16.0),
                fmod(realCell.coord.v[2], 16.0));
#endif
            debugColour = &tint;
#endif /* ENQUEUE_DEBUG_COLOURS */

            realCell.makeTerrain(pointSubdivisionFactor, subdivisionFactor, minCellSize, terrain, *(afk_core.rng), debugColour);

            /* If it's the smallest possible cell, or its detail pitch
             * is at the target detail pitch, include it as-is
             */
            if (cell.coord.v[3] == MIN_CELL_PITCH || realCell.testDetailPitch(detailPitch, camera, viewerLocation))
            {
                /* If the displayed cell isn't there already, better make it now.
                 * TODO Expensive: enqueue as separate work item (maybe even in OpenCL?)
                 */
                if (!landscapeCell.displayed)
                {
                    boost::shared_ptr<AFK_DisplayedLandscapeCell> newD(new AFK_DisplayedLandscapeCell(
                        shaderProgram->program,
                        worldTransformLocation,
                        clipTransformLocation,
                        realCell,
                        pointSubdivisionFactor,
                        terrain,
                        *(afk_core.rng)));
                    landscapeCell.displayed = newD;
                }
                else
                {
                    ++landscapeCellsCached;
                }
        
                /* Now, push it into the queue as well */
                AFK_DisplayedLandscapeCell *dptr = landscapeCell.displayed.get();
                renderQueue.push(dptr);
                ++landscapeCellsQueued;
            }
            else
            {
                /* Recurse through the subcells. */
                size_t subcellsSize = CUBE(subdivisionFactor);
                AFK_Cell *subcells = new AFK_Cell[subcellsSize]; /* TODO avoid heap thrashing somehow */
                unsigned int subcellsCount = cell.subdivide(subcells, subcellsSize);

                if (subcellsCount == subcellsSize)
                {
                    for (unsigned int i = 0; i < subcellsCount; ++i)
                    {
                        enqueueSubcells(subcells[i], cell, viewerLocation, camera, allVisible);
                    }
                }
                else
                {
                    /* That's clearly a bug :P */
                    std::ostringstream ss;
                    ss << "Cell " << cell << " subdivided into " << subcellsCount << " not " << subcellsSize;
                    throw AFK_Exception(ss.str());
                }
            
                delete[] subcells;
            }
        
            /* Back out the terrain changes for the next cell
             * to try.
             */
            realCell.removeTerrain(terrain);   
        }
    }
}

void AFK_Landscape::enqueueSubcells(
    const AFK_Cell& cell,
    const Vec3<long long>& modifier,
    const Vec3<float>& viewerLocation,
    const AFK_Camera& camera)
{
    AFK_Cell modifiedCell(Vec4<long long>(
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[0],
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[1],
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[2],
        cell.coord.v[3]));
    enqueueSubcells(modifiedCell, modifiedCell.parent(), viewerLocation, camera, false);
}

void AFK_Landscape::updateLandMap(void)
{
    /* Do the statistics thing for this pass. */
    landscapeCellsEmpty = 0;
    landscapeCellsInvisible = 0;
    landscapeCellsCached = 0;
    landscapeCellsQueued = 0;

    /* First, transform the protagonist location and its facing
     * into integer cell-space.
     * TODO: This is *really* going to want arbitrary
     * precision arithmetic, eventually.
     */
    Mat4<float> protagonistTransformation = afk_core.protagonist->object.getTransformation();
    Vec4<float> hgProtagonistLocation = protagonistTransformation *
        Vec4<float>(0.0f, 0.0f, 0.0f, 1.0f);
    Vec3<float> protagonistLocation = Vec3<float>(
        hgProtagonistLocation.v[0] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[1] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[2] / hgProtagonistLocation.v[3]);
    Vec4<long long> csProtagonistLocation = Vec4<long long>(
        (long long)(protagonistLocation.v[0] / minCellSize),
        (long long)(protagonistLocation.v[1] / minCellSize),
        (long long)(protagonistLocation.v[2] / minCellSize),
        1);

    AFK_Cell protagonistCell(csProtagonistLocation);

#ifdef PROTAGONIST_CELL_DEBUG
    {
        std::ostringstream ss;
        ss << "Protagonist in cell: " << protagonistCell;
        afk_core.occasionallyPrint(ss.str());
    }
#endif

    /* Wind up the cell tree and find its largest parent,
     * then go one step smaller -- because a too-big cell
     * just won't be seen by the projection and will
     * therefore be culled
     */
    AFK_Cell cell, parentCell;
    for (parentCell = protagonistCell;
        (float)cell.coord.v[3] < maxDistance;
        cell = parentCell, parentCell = cell.parent());

    /* Draw that cell and the other cells around it.
     * TODO Can I optimise this?  It's going to attempt
     * cells that are very very far away.  Mind you,
     * that might be okay.
     */
    for (long long i = -1; i <= 1; ++i)
        for (long long j = -1; j <= 1; ++j)
            for (long long k = -1; k <= 1; ++k)
                enqueueSubcells(cell, Vec3<long long>(i, j, k), protagonistLocation, *(afk_core.camera));

#ifdef PROTAGONIST_CELL_DEBUG
    {
        std::ostringstream ss;
        ss << "Queued " << landscapeCellsQueued << " cells";
    
        if (landscapeCellsQueued > 0)
            ss << " (" << (100 * landscapeCellsCached / landscapeCellsQueued) << "\% cached)";

        ss << " (" << landscapeCellsInvisible << " invisible, " << landscapeCellsEmpty << " empty)";
        afk_core.occasionallyPrint(ss.str());
    }
#endif
}

void AFK_Landscape::display(const Mat4<float>& projection)
{
    AFK_DisplayedLandscapeCell *displayedCell;
    while (renderQueue.pop(displayedCell))
        displayedCell->display(projection);
}

