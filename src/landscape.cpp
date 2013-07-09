/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <boost/atomic.hpp>
#include <boost/memory_order.hpp>

#include "core.hpp"
#include "exception.hpp"
#include "landscape.hpp"
#include "rng/boost_taus88.hpp"


/* TODO remove debug?  (or something) */
#define PROTAGONIST_CELL_DEBUG 1

/* More debugging */
#define CELL_BOUNDARIES_IN_RED 1


/* AFK_DisplayedLandscapeCell workers. */

static void computeFlatTriangle(
    const Vec3<float> *vertices,
    const Vec3<float> *colours,
    const Vec3<unsigned int>& indices,
    struct AFK_VcolPhongVertex *triangleVPos)
{
    struct AFK_VcolPhongVertex *outStructs[3];
    unsigned int i;

    for (i = 0; i < 3; ++i)
        outStructs[i] = triangleVPos + i;

    Vec3<float> crossP = ((vertices[indices.v[1]] - vertices[indices.v[0]]).cross(
        vertices[indices.v[2]] - vertices[indices.v[0]]));

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
        outStructs[i]->location = vertices[indices.v[i]];
        outStructs[i]->colour = colours[indices.v[i]];
        outStructs[i]->normal = normal;
    }
}

/* Turns a vertex grid into a landscape of flat triangles.
 * (Each triangle has different vertices because the normals
 * are different, so no index array is used.)
 * Call with NULL `triangleVs' to find out how large the
 * triangles array ought to be.
 */
static void vertices2FlatTriangles(
    Vec3<float> *vertices,
    Vec3<float> *colours,
    unsigned int vertexCount,
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
                unsigned int i_r1c1 = row * (pointSubdivisionFactor+1) + col;
                unsigned int i_r1c2 = row * (pointSubdivisionFactor+1) + (col + 1);
                unsigned int i_r2c1 = (row + 1) * (pointSubdivisionFactor+1) + col;
                unsigned int i_r2c2 = (row + 1) * (pointSubdivisionFactor+1) + (col + 1);

                Vec3<unsigned int> tri1Is = afk_vec3<unsigned int>(i_r2c1, i_r1c1, i_r1c2);
                computeFlatTriangle(vertices, colours, tri1Is, triangleVPos);

                triangleVPos += 3;

                Vec3<unsigned int> tri2Is = afk_vec3<unsigned int>(i_r2c1, i_r1c2, i_r2c2);
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
    size_t vertexCount,
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
    const AFK_LandscapeCell& landscapeCell,
    unsigned int pointSubdivisionFactor,
    AFK_CACHE& cache):
        triangleVs(NULL),
        triangleVsCount(0),
        program(0),
        vertices(0),
        vertexCount(0)
{
    Vec4<float> realCoord = landscapeCell.getRealCoord();

    /* I'm going to need to sample the edges of the next cell along
     * the +ve x and z too, in order to join up with it.
     * -- TODO: That needs to change (see: stitching).
     */
    /* TODO This is thrashing the heap.  Try making a single
     * scratch place for this in thread-local storage
     */
    vertexCount = SQUARE(pointSubdivisionFactor + 1);
    Vec3<float> *rawVertices = new Vec3<float>[vertexCount];
    Vec3<float> *rawColours = new Vec3<float>[vertexCount];

    /* Populate the vertex and colour arrays. */
    size_t rawIndex = 0;    

    for (unsigned int xi = 0; xi < pointSubdivisionFactor + 1; ++xi)
    {
        for (unsigned int zi = 0; zi < pointSubdivisionFactor + 1; ++zi)
        {
            /* The geometry in a cell goes from its (0,0,0) point to
             * just before its (coord.v[3], coord.v[3], coord.v[3]
             * point (in cell space)
             */
            float xf = (float)xi * realCoord.v[3] / (float)pointSubdivisionFactor;
            float zf = (float)zi * realCoord.v[3] / (float)pointSubdivisionFactor;

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
                xdisp = rng.frand() * landscapeCell.coord.v[3] / ((float)pointSubdivisionFactor * 2.0f);

            if (zi > 0 && zi <= pointSubdivisionFactor)
                zdisp = rng.frand() * landscapeCell.coord.v[3] / ((float)pointSubdivisionFactor * 2.0f);
#endif

            /* And shunt them into world space */
            rawVertices[rawIndex] = afk_vec3<float>(
                xf + xdisp + realCoord.v[0],
                realCoord.v[1],
                zf + zdisp + realCoord.v[2]);

            rawColours[rawIndex] = afk_vec3<float>(0.1f, 0.1f, 0.1f);

            ++rawIndex;
        }
    }

    /* Now apply the terrain transform.
     * TODO This may be slow -- obvious candidate for OpenCL?
     * But, the cache may rescue me; profile first!
     */
    for (unsigned int i = 0; i < vertexCount; ++i)
        landscapeCell.computeTerrain(rawVertices[i], rawColours[i], cache);

    /* I've completed my vertex array!  Transform this into an
     * array of VcolPhongVertex by computing colours and normals
     */
    vertices2FlatTriangles(rawVertices, rawColours, vertexCount, pointSubdivisionFactor, NULL, &triangleVsCount);
    triangleVs = new struct AFK_VcolPhongVertex[triangleVsCount];
    vertices2FlatTriangles(rawVertices, rawColours, vertexCount, pointSubdivisionFactor, triangleVs, &triangleVsCount);

    /* I don't need this any more... */
    delete[] rawVertices;
    delete[] rawColours;
}

AFK_DisplayedLandscapeCell::~AFK_DisplayedLandscapeCell()
{
    delete[] triangleVs;
    
    /* TODO Cache eviction is going to cause some nasty OpenGL
     * wrong-thread problem here, isn't it?  Should I enqueue
     * these into a to-be-deleted queue that's scraped by
     * the main thread?
     */
    if (vertices) glDeleteBuffers(1, &vertices);
}

void AFK_DisplayedLandscapeCell::initGL(void)
{
    program = afk_core.landscape->shaderProgram->program;
    worldTransformLocation = afk_core.landscape->worldTransformLocation;
    clipTransformLocation = afk_core.landscape->clipTransformLocation;
    shaderLight = new AFK_ShaderLight(program); /* TODO heap thrashing :P */

    glGenBuffers(1, &vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glBufferData(GL_ARRAY_BUFFER, triangleVsCount * sizeof(struct AFK_VcolPhongVertex), triangleVs, GL_STATIC_DRAW);
    vertexCount = triangleVsCount;
}

void AFK_DisplayedLandscapeCell::displaySetup(const Mat4<float>& projection)
{
    if (!shaderLight) initGL();

    /* A single DisplayedLandscapeCell will do this common stuff;
     * the others' display() functions will assume they were called
     * right after and don't have to do it again
     */
    glUseProgram(program);
    shaderLight->setupLight(afk_core.sun);
}

void AFK_DisplayedLandscapeCell::display(const Mat4<float>& projection)
{
    if (!vertices) initGL();

    if (vertices)
    {
        updateTransform(projection);

        /* Cell specific stuff will go here ... */

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER, vertices);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(sizeof(Vec3<float>)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(2 * sizeof(Vec3<float>)));

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

void AFK_LandscapeCell::computeTerrainRec(Vec3<float>& position, Vec3<float>& colour, AFK_CACHE& cache) const
{
    /* Co-ordinates arrive in the context of this cell's
     * first terrain element.
     * Compute all of them, transforming between them as
     * required
     */
    for (unsigned int i = 0; i < TERRAIN_CELLS_PER_CELL; ++i)
    {
        if (i > 0)
        {
            /* Need to do the next transformation. */
            terrain[i-1].transformCellToCell(position, terrain[i]);
        }

        terrain[i].compute(position, colour);
    }

    if (topLevel)
    {
        /* Transform back into world space. */
        Vec4<float> cellCoord = terrain[TERRAIN_CELLS_PER_CELL-1].getCellCoord();
        position = (position * cellCoord.v[3]) + afk_vec3<float>(cellCoord.v[0], cellCoord.v[1], cellCoord.v[2]);
    }
    else
    {
        /* Find the next cell up.
         * If it isn't there something has gone rather wrong,
         * because I should have touched it earlier
         */
        AFK_LandscapeCell& parentLandscapeCell = cache[cell.parent()];

        /* Transform into the co-ordinates of this cell's
         * first terrain element
         */
        terrain[TERRAIN_CELLS_PER_CELL-1].transformCellToCell(
            position, parentLandscapeCell.terrain[0]);

        /* ...and compute its terrain too */
        parentLandscapeCell.computeTerrainRec(position, colour, cache);
    }
}

AFK_LandscapeCell::AFK_LandscapeCell(const AFK_LandscapeCell& c):
    lastSeen (c.lastSeen), cell (c.cell), topLevel (c.topLevel), realCoord (c.realCoord),
    hasTerrain (c.hasTerrain), displayed (c.displayed)
{
    for (unsigned int i = 0; i < TERRAIN_CELLS_PER_CELL; ++i)
        terrain[i] = c.terrain[i];
}

AFK_LandscapeCell& AFK_LandscapeCell::operator=(const AFK_LandscapeCell& c)
{
    lastSeen = c.lastSeen;
    cell = c.cell;
    topLevel = c.topLevel;
    realCoord = c.realCoord;
    hasTerrain = c.hasTerrain;

    for (unsigned int i = 0; i < TERRAIN_CELLS_PER_CELL; ++i)
        terrain[i] = c.terrain[i];

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

    /* Was I the grabber? */
    if (claimingThreadId == myThreadId) return true;

    return false;
}

void AFK_LandscapeCell::bind(const AFK_Cell& _cell, bool _topLevel, float _worldScale)
{
    cell = _cell;
    topLevel = _topLevel;
    /* TODO If this actually *changes* something, I need to
     * clear the terrain so that it can be re-made.
     */
    realCoord = _cell.toWorldSpace(_worldScale);
}

bool AFK_LandscapeCell::testDetailPitch(
    unsigned int detailPitch,
    const AFK_Camera& camera,
    const Vec3<float>& viewerLocation) const
{
    /* Sample the centre of the cell.  This is wrong: it
     * will cause artifacts if you manage to get to exactly
     * the middle of a cell (I can probably test this with
     * the start position (8192, 8192, 8192)
     * TODO To fix it properly, I need to pick three points
     * displaced along the 3 axes by the dot pitch from the
     * centre of the cell, project them through the camera,
     * and compare those distances to the detail pitch,
     * no?
     * (in fact I'd probably get away with just the x and
     * z axes)
     */
    Vec3<float> centre = afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3] / 2.0f,
        realCoord.v[1] + realCoord.v[3] / 2.0f,
        realCoord.v[2] + realCoord.v[3] / 2.0f);
    Vec3<float> facing = centre - viewerLocation;
    float distanceToViewer = facing.magnitude();

    /* Magic */
    float cellDetailPitch = camera.windowHeight * realCoord.v[3] /
        (camera.tanHalfFov * distanceToViewer);
    
    return cellDetailPitch < (float)detailPitch;
}

/* Helper for the below. */
static void testPointVisible(const Vec3<float>& point, const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible)
{
    /* Project the point.  This perspective projection
     * yields a viewport with the x co-ordinates
     * (-ar, ar) and the y co-ordinates (-1.0, 1.0).
     */
    Vec4<float> projectedPoint = camera.getProjection() * afk_vec4<float>(
        point.v[0], point.v[1], point.v[2], 1.0f);
    bool visible = (
        (projectedPoint.v[0] / projectedPoint.v[2]) >= -camera.ar &&
        (projectedPoint.v[0] / projectedPoint.v[2]) <= camera.ar &&
        (projectedPoint.v[1] / projectedPoint.v[2]) >= -1.0f &&
        (projectedPoint.v[1] / projectedPoint.v[2]) <= 1.0f);

    io_someVisible |= visible;
    io_allVisible &= visible;
}

void AFK_LandscapeCell::testVisibility(const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible) const
{
    /* Check whether this cell is actually visible, by
     * testing all 8 vertices and its midpoint.
     * TODO Is that enough?
     */
    testPointVisible(afk_vec3<float>(
        realCoord.v[0],
        realCoord.v[1],
        realCoord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3],
        realCoord.v[1],
        realCoord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0],
        realCoord.v[1] + realCoord.v[3],
        realCoord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3],
        realCoord.v[1] + realCoord.v[3],
        realCoord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0],
        realCoord.v[1],
        realCoord.v[2] + realCoord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3],
        realCoord.v[1],
        realCoord.v[2] + realCoord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0],
        realCoord.v[1] + realCoord.v[3],
        realCoord.v[2] + realCoord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3],
        realCoord.v[1] + realCoord.v[3],
        realCoord.v[2] + realCoord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3] / 2.0f,
        realCoord.v[1] + realCoord.v[3] / 2.0f,
        realCoord.v[2] + realCoord.v[3] / 2.0f),
        camera, io_someVisible, io_allVisible);
}

void AFK_LandscapeCell::makeTerrain(
    unsigned int pointSubdivisionFactor,
    unsigned int subdivisionFactor,
    float minCellSize,
    const Vec3<float> *forcedTint)
{
    if (cell.coord.v[1] == 0 && !hasTerrain)
    {
        /* TODO RNG in thread local storage so I don't have to re-make
         * ones on the stack?  (inefficient?)
         */
        AFK_Boost_Taus88_RNG rng;

        /* Make the terrain cell for this actual cell. */
        rng.seed(cell.rngSeed());
        terrain[0].make(cell.toWorldSpace(minCellSize), pointSubdivisionFactor, subdivisionFactor, minCellSize, rng, forcedTint);

        /* Now, make the terrain for the four 1/2-cells that
         * overlap this cell
         */
        AFK_Cell halfCells[4];
        cell.enumerateHalfCells(&halfCells[0], 4);
        for (unsigned int i = 0; i < 4; ++i)
        {
            rng.seed(halfCells[i].rngSeed());
            terrain[i+1].make(halfCells[i].toWorldSpace(minCellSize), pointSubdivisionFactor, subdivisionFactor, minCellSize, rng, forcedTint);
        }

        hasTerrain = true;
    }
}

void AFK_LandscapeCell::computeTerrain(Vec3<float>& position, Vec3<float>& colour, AFK_CACHE& cache) const
{
    if (terrain)
    {
        /* Make a position in the space of the first terrain cell */
        Vec4<float> firstTerrainCoord = terrain[0].getCellCoord();
        Vec3<float> poscs = afk_vec3<float>(
            (position.v[0] - firstTerrainCoord.v[0]) / firstTerrainCoord.v[3],
            (position.v[1] - firstTerrainCoord.v[1]) / firstTerrainCoord.v[3],
            (position.v[2] - firstTerrainCoord.v[2]) / firstTerrainCoord.v[3]);

        computeTerrainRec(poscs, colour, cache);

        /* computeTerrainRec finishes by putting the co-ordinates back into
         * world space.
         * However, since I'm not really wanting the terrain to modify x and
         * z right now, I'll just ignore those
         */
        position.v[1] = poscs.v[1];

        /* Put the colour into shape.
         * TODO This really wants tweaking; the normalise produces
         * pastels.  Maybe something logarithmic?
         */
        colour.normalise();
        colour = colour + 1.0f / 2.0f;
    }
}

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeCell& landscapeCell)
{
    /* TODO Something more descriptive might be nice */
    return os << "Landscape cell (last seen " << landscapeCell.lastSeen << ", claimed by " << landscapeCell.claimingThreadId << ")";
}


/* The cell generating worker */


#define ENQUEUE_DEBUG_COLOURS 0

bool afk_generateLandscapeCells(
    struct AFK_LandscapeCellGenParam param,
    ASYNC_QUEUE_TYPE(struct AFK_LandscapeCellGenParam)& queue)
{
    unsigned int recCount               = param.recCount;
    const AFK_Cell& cell                = param.cell;
    bool topLevel                       = param.topLevel;
    AFK_Landscape *landscape            = param.landscape;
    const Vec3<float>& viewerLocation   = param.viewerLocation;
    const AFK_Camera *camera            = param.camera;
    bool entirelyVisible                = param.entirelyVisible; 

    /* Find out if we've already processed this cell this frame.
     * TODO: I need to change this so that instead of using
     * `renderingFrame', it uses a `computingFrame tracking
     * one step behind, and so on
     */
    AFK_LandscapeCell& landscapeCell = landscape->cache[cell];
    if (!landscapeCell.claim()) return true;
    landscapeCell.bind(cell, topLevel, landscape->minCellSize);

    /* Is there any terrain here?
     * TODO: If there isn't, I still want to evaluate
     * contained objects, and any terrain geometry that
     * might have "bled" into this cell.  But for now,
     * just dump it.
     */
    if (cell.coord.v[1] != 0)
    {
        landscape->cellsEmpty.fetch_add(1);
    }
    else
    {
        /* Check for visibility. */
        bool someVisible = false;
        bool allVisible = entirelyVisible;

        landscapeCell.testVisibility(*camera, someVisible, allVisible);
        if (!someVisible)
        {
            landscape->cellsInvisible.fetch_add(1);

            /* Nothing else to do with it now either. */
        }
        else
        {
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
            /* Make sure there's terrain here. */
            landscapeCell.makeTerrain(
                landscape->pointSubdivisionFactor,
                landscape->subdivisionFactor,
                landscape->minCellSize,
                debugColour);

            /* If it's the smallest possible cell, or its detail pitch
             * is at the target detail pitch, include it as-is
             */
            if (cell.coord.v[3] == MIN_CELL_PITCH ||
                landscapeCell.testDetailPitch(landscape->detailPitch, *camera, viewerLocation))
            {
                /* If the displayed cell isn't there already, better make it now.
                 * TODO Expensive: enqueue as separate work item (maybe even in OpenCL?)
                 */
                if (!landscapeCell.displayed)
                {
                    boost::shared_ptr<AFK_DisplayedLandscapeCell> newD(new AFK_DisplayedLandscapeCell(
                        landscapeCell,
                        landscape->pointSubdivisionFactor,
                        landscape->cache));
                    landscapeCell.displayed = newD;
                    landscape->cellsGenerated.fetch_add(1);
                }
                else
                {
                    landscape->cellsCached.fetch_add(1);
                }
        
                /* Now, push it into the queue as well */
                AFK_DisplayedLandscapeCell *dptr = landscapeCell.displayed.get();
                landscape->renderQueue.update_push(dptr);
                landscape->cellsQueued.fetch_add(1);
            }
            else
            {
                /* Recurse through the subcells. */
                size_t subcellsSize = CUBE(landscape->subdivisionFactor);
                AFK_Cell *subcells = new AFK_Cell[subcellsSize]; /* TODO avoid heap thrashing somehow.  Maybe make it an iterator */
                unsigned int subcellsCount = cell.subdivide(subcells, subcellsSize);

                if (subcellsCount == subcellsSize)
                {
                    for (unsigned int i = 0; i < subcellsCount; ++i)
                    {
                        struct AFK_LandscapeCellGenParam subcellParam;
                        subcellParam.recCount           = recCount + 1;
                        subcellParam.cell               = subcells[i];
                        subcellParam.topLevel           = false;
                        subcellParam.landscape          = landscape;
                        subcellParam.viewerLocation     = viewerLocation;
                        subcellParam.camera             = camera;
                        subcellParam.entirelyVisible    = allVisible;

#if AFK_NO_THREADING
                        afk_generateLandscapeCells(subcellParam, queue);
#else
                        if (subcellParam.recCount == landscape->recursionsPerTask)
                        {
                            /* I've hit the task recursion limit, queue this as a new task */
                            subcellParam.recCount = 0;
                            queue.push(subcellParam);
                        }
                        else
                        {
                            /* Use a direct function call */
                            afk_generateLandscapeCells(subcellParam, queue);
                        }
#endif
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
        }
    }

    return true;
}


/* AFK_Landscape implementation */

AFK_Landscape::AFK_Landscape(
    float _maxDistance,
    unsigned int _subdivisionFactor,
    unsigned int _detailPitch):
#if AFK_USE_POLYMER_CACHE
        cache(AFK_HashCell()),
#endif
        renderQueue(10000), /* TODO make a better guess */
        genGang(
            boost::function<bool (const struct AFK_LandscapeCellGenParam,
                ASYNC_QUEUE_TYPE(struct AFK_LandscapeCellGenParam)&)>(afk_generateLandscapeCells),
            100 /* Likewise */ ),
#if AFK_NO_THREADING
        fakeQueue(100),
        fakePromise(NULL),
#endif
        /* On HyperThreading systems, can ignore the hyper threads:
         * there appears to be zero benefit.  However, I can't figure
         * out how to identify this.
         */
        recursionsPerTask(1) /* This seems to do better than higher numbers */
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

    /* Initialise the statistics. */
    cellsEmpty.store(0);
    cellsInvisible.store(0);
    cellsQueued.store(0);
    cellsCached.store(0);
    cellsGenerated.store(0);
}

AFK_Landscape::~AFK_Landscape()
{
    delete shaderProgram;
#if AFK_NO_THREADING
    if (fakePromise) delete fakePromise;
#endif
}

void AFK_Landscape::enqueueSubcells(
    const AFK_Cell& cell,
    const Vec3<long long>& modifier,
    const Vec3<float>& viewerLocation,
    const AFK_Camera& camera)
{
    AFK_Cell modifiedCell = afk_cell(afk_vec4<long long>(
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[0],
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[1],
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[2],
        cell.coord.v[3]));

    struct AFK_LandscapeCellGenParam cellParam;
    cellParam.recCount              = 0;
    cellParam.cell                  = modifiedCell;
    cellParam.topLevel              = true;
    cellParam.landscape             = this;
    cellParam.viewerLocation        = viewerLocation;
    cellParam.camera                = &camera;
    cellParam.entirelyVisible       = false;

#if AFK_NO_THREADING
    afk_generateLandscapeCells(cellParam, fakeQueue);
#else
    genGang << cellParam;
#endif
}

void AFK_Landscape::flipRenderQueues(void)
{
    renderQueue.flipQueues();
}

boost::unique_future<bool> AFK_Landscape::updateLandMap(void)
{

    /* First, transform the protagonist location and its facing
     * into integer cell-space.
     * TODO: This is *really* going to want arbitrary
     * precision arithmetic, eventually.
     */
    Mat4<float> protagonistTransformation = afk_core.protagonist->object.getTransformation();
    Vec4<float> hgProtagonistLocation = protagonistTransformation *
        afk_vec4<float>(0.0f, 0.0f, 0.0f, 1.0f);
    Vec3<float> protagonistLocation = afk_vec3<float>(
        hgProtagonistLocation.v[0] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[1] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[2] / hgProtagonistLocation.v[3]);
    Vec4<long long> csProtagonistLocation = afk_vec4<long long>(
        (long long)(protagonistLocation.v[0] / minCellSize),
        (long long)(protagonistLocation.v[1] / minCellSize),
        (long long)(protagonistLocation.v[2] / minCellSize),
        1);

    AFK_Cell protagonistCell = afk_cell(csProtagonistLocation);

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
                enqueueSubcells(cell, afk_vec3<long long>(i, j, k), protagonistLocation, *(afk_core.camera));

#if AFK_NO_THREADING
    if (fakePromise) delete fakePromise;
    fakePromise = new boost::promise<bool>();
    fakePromise->set_value(true);
    return fakePromise->get_future();
#else
    return genGang.start();
#endif
}

void AFK_Landscape::display(const Mat4<float>& projection)
{
    AFK_DisplayedLandscapeCell *displayedCell;
    bool first;
    while (renderQueue.draw_pop(displayedCell))
    {
        /* All cells are the same in many ways, so I can do this
         * just the once
         * TODO: I really ought to move this out into a drawable
         * single object related to AFK_Landscape instead
         */
        if (first)
        {
            displayedCell->displaySetup(projection);
            first = false;
        }

        displayedCell->display(projection);
    }
}

/* Worker for the below. */
static float toRatePerSecond(unsigned long long quantity, boost::posix_time::time_duration& interval)
{
    return (float)quantity * 1000.0f / (float)interval.total_milliseconds();
}

void AFK_Landscape::checkpoint(boost::posix_time::time_duration& timeSinceLastCheckpoint)
{
    std::cout << "Cells found empty:        " << toRatePerSecond(cellsEmpty.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Cells found invisible:    " << toRatePerSecond(cellsInvisible.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Cells queued:             " << toRatePerSecond(cellsQueued.load(), timeSinceLastCheckpoint) << "/second (" << (100 * cellsCached.load() / cellsQueued.load()) << "\% cached)" << std::endl;
    std::cout << "Cells generated:          " << toRatePerSecond(cellsGenerated.load(), timeSinceLastCheckpoint) << "/second" << std::endl;

    cellsEmpty.store(0);
    cellsInvisible.store(0);
    cellsQueued.store(0);
    cellsCached.store(0);
    cellsGenerated.store(0);
}

