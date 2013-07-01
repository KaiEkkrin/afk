/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <stdlib.h>
#include <time.h>

#include "core.hpp"
#include "exception.hpp"
#include "landscape.hpp"


/* TODO remove debug?  (or something) */
#define PROTAGONIST_CELL_DEBUG 1


/* AFK_LandscapeCell implementation */

AFK_LandscapeCell::AFK_LandscapeCell(
    GLuint _program,
    GLuint _transformLocation,
    GLuint _fixedColorLocation,
    const AFK_RealCell& cell,
    unsigned int pointSubdivisionFactor,
    const std::vector<AFK_TerrainFeature>& terrain,
    AFK_RNG& rng)
{
    program = _program;
    transformLocation = _transformLocation;
    fixedColorLocation = _fixedColorLocation;

    object.displace(Vec3<float>(cell.coord.v[0], cell.coord.v[1], cell.coord.v[2]));

    /* Seed the RNG for this cell. */
    rng.seed(cell.worldCell.rngSeed());

    vertexCount = SQUARE(pointSubdivisionFactor);
    float *rawVertices = NULL;
    size_t rawVerticesSize = sizeof(float) * 3 * vertexCount;

    /* TODO This is thrashing the heap.  Try making a single
     * scratch place for this in thread-local storage
     */
    rawVertices = (float *)malloc(rawVerticesSize);
    if (!rawVertices)
    {
        std::ostringstream ss;
        ss << "Unable to allocate vertex array for landscape cell " << cell.worldCell;
        throw AFK_Exception(ss.str());
    }

    /* Populate the vertex array.
     * TODO This is currently entirely flat for testing.  I need
     * to replace this with code that calls through the
     * terrain vector.
     */
    float *rawVerticesPos = rawVertices;

    for (unsigned int xi = 0; xi < pointSubdivisionFactor; ++xi)
    {
        for (unsigned int zi = 0; zi < pointSubdivisionFactor; ++zi)
        {
            /* The geometry in a cell goes from its (0,0,0) point to
             * just before its (coord.v[3], coord.v[3], coord.v[3]
             * point (in cell space)
             */
            float xf = (float)xi * cell.coord.v[3] / (float)pointSubdivisionFactor;
            float zf = (float)zi * cell.coord.v[3] / (float)pointSubdivisionFactor;

            /* Here's a non-cumulative use of the RNG (unlike the
             * upwards displacement).  Jitter all the raw vertex
             * locations around a little bit, in order to avoid
             * aliasing along the axes.
             * Of course, sampling the vertices themselves may
             * require *one reseed per vertex*! (hence the fast-
             * reseeding RNG)
             * But this tweak makes the flat landscape look SO MUCH
             * BETTER that I might get away with a lower detailPitch
             */
            float xdisp = rng.frand() * cell.coord.v[3] / (float)pointSubdivisionFactor;
            float zdisp = rng.frand() * cell.coord.v[3] / (float)pointSubdivisionFactor;

            *(rawVerticesPos++) = xf + xdisp;
            *(rawVerticesPos++) = 0.0f;
            *(rawVerticesPos++) = zf + zdisp;
        }
    }

    /* Now that I've populated the (x, z) co-ordinates of the
     * vertex buffer with the locations I want to sample the
     * terrain at, fill in the y co-ordinates with the actual
     * terrain samples.
     * TODO This may be slow -- obvious candidate for OpenCL?
     * But, the cache may rescue me; profile first!
     */
    for (std::vector<AFK_TerrainFeature>::const_iterator tIt = terrain.begin();
        tIt != terrain.end(); ++tIt)
    {
        for (rawVerticesPos = rawVertices; rawVerticesPos < (rawVertices + vertexCount * 3); rawVerticesPos += 3)
        {
            float *xPos = rawVerticesPos;
            float *yPos = rawVerticesPos + 1;
            float *zPos = rawVerticesPos + 2;

            /* Terrain is computed in world co-ordinates.
             * TODO That's something I need to change: I'll get
             * dreadful aliasing as I depart from the origin.
             * Or can I get away with sometimes rebasing the
             * whole terrain?
             */
#if 0
            *yPos = tIt->compute(Vec3<float>(
                *xPos + cell.coord.v[0],
                *yPos,
                *zPos + cell.coord.v[2]));
#else
            /* TODO Massive debugging */
            *yPos = *xPos + *zPos / 2.0f;
#endif
        }
    }

    /* Turn this into an OpenGL vertex buffer */
    glGenBuffers(1, &vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glBufferData(GL_ARRAY_BUFFER, rawVerticesSize, rawVertices, GL_STATIC_DRAW);

    free(rawVertices);

    /* TODO For testing. Maybe I can come up with something neater; OTOH, maybe
     * I don't want to :P
     */
    rng.seed(cell.worldCell.rngSeed());
    colour = Vec3<float>(rng.frand(), rng.frand(), rng.frand());
}

AFK_LandscapeCell::~AFK_LandscapeCell()
{
    glDeleteBuffers(1, &vertices);
}

void AFK_LandscapeCell::display(const Mat4<float>& projection)
{
    if (vertices != 0)
    {
        /* TODO Do I have to do `glUseProgram' once per cell, really?
          * Can't I omit it from here and have the AFK_Landscape
         * object do it before displaying its cells?
         */
        glUseProgram(program);
        glUniform3f(fixedColorLocation, colour.v[0], colour.v[1], colour.v[2]);

        updateTransform(projection);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertices);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glDrawArrays(GL_POINTS, 0, vertexCount);

        glDisableVertexAttribArray(0);
    }
}

#if 0
std::ostream& operator<<(std::ostream& os, const AFK_LandscapeCell& cell)
{
    return os << "LandscapeCell(" <<
        cell.coord.v[0] << ", " <<
        cell.coord.v[1] << ", " <<
        cell.coord.v[2] << ", scale " <<
        cell.coord.v[3] << ")";
}
#endif

/* AFK_Landscape implementation */

AFK_Landscape::AFK_Landscape(size_t _cacheSize, float _maxDistance, unsigned int _subdivisionFactor, unsigned int _detailPitch)
{
    cacheSize           = _cacheSize;
    maxDistance         = _maxDistance;
    subdivisionFactor   = _subdivisionFactor;
    detailPitch         = _detailPitch;

    /* Set up the landscape shader. */
    shaderProgram = new AFK_ShaderProgram();
    *shaderProgram << "basic_fragment" << "basic_vertex";
    shaderProgram->Link();

    transformLocation = glGetUniformLocation(shaderProgram->program, "transform");
    fixedColorLocation = glGetUniformLocation(shaderProgram->program, "fixedColor");

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

    /* TODO Here's another thing that should be implemented as a
     * configuration parameter
     */
    maxFeaturesPerCell = 4;

    /* Initialise the terrain vector with a size that ought to
     * be large enough for all iterations
     * Multiply by five because the terrain for each cell is
     * determined by that cell itself plus the four 1/2-offset
     * pseudo-cells at (-0.5, 0, -0.5), (-0.5, 0, 0.5),
     * (0.5, 0, -0.5) and (0.5, 0, 0.5).
     */
    terrain = std::vector<AFK_TerrainFeature>(maxSubdivisions * maxFeaturesPerCell * 5);
}

AFK_Landscape::~AFK_Landscape()
{
    for (boost::unordered_map<const AFK_Cell, AFK_LandscapeCell*>::iterator lmIt = landMap.begin(); lmIt != landMap.end(); ++lmIt)
        delete lmIt->second;

    landMap.clear();    
    landQueue.clear();

    delete shaderProgram;
}

void AFK_Landscape::enqueueSubcells(
    const AFK_Cell& cell,
    const AFK_Cell& parent,
    float terrainAtZero,
    const Vec3<float>& viewerLocation,
    const AFK_Camera& camera,
    bool parentEntirelyVisible)
{
    /* If this cell is outside the given parent, stop right
     * away.
     */
    if (!cell.isParent(parent)) return;

    /* Find out if it's enqueued already */
    std::pair<bool, AFK_LandscapeCell*>& enqueued = landQueue[cell];
    
    /* If we've seen it already, stop */
    if (enqueued.first) return;

    /* Otherwise, flag this cell as having been looked at
     * (regardless of whether we add a landscape, we don't
     * want to come back here)
     */
    enqueued.first = true;
    enqueued.second = NULL;

    /* Work out the real-world cell that corresponds to this
     * abstract one.
     */
    AFK_RealCell realCell(cell, minCellSize);

    /* Add the terrain for this cell to the terrain vector. */
    realCell.makeTerrain(terrainAtZero, terrain, *(afk_core.rng));

    /* If this cell isn't empty ... */
    if (realCell.testCellEmpty(terrainAtZero))
    {
        ++landscapeCellsEmpty;

        /* Nothing else to do with it. */
    }
    else
    {
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
            /* If it's the smallest possible cell, or its detail pitch
             * is at the target detail pitch, include it as-is
             */
            if (cell.coord.v[3] == MIN_CELL_PITCH || realCell.testDetailPitch(detailPitch, camera, viewerLocation))
            {
                /* Find out if it's already in the cache */
                AFK_LandscapeCell*& landscapeCell = landMap[cell];
                if (!landscapeCell)
                    /* TODO Pass the terrain in here so it can be computed
                     * across all of this cell
                     */
                    landscapeCell = new AFK_LandscapeCell(
                        shaderProgram->program,
                        transformLocation,
                        fixedColorLocation,
                        realCell,
                        pointSubdivisionFactor,
                        terrain,
                        *(afk_core.rng));
                else
                    ++landscapeCellsCached;
        
                /* Now, push it into the queue as well */
                enqueued.second = landscapeCell;
                ++landscapeCellsQueued;
            }
            else
            {
                /* Pull out the augmented set of subcells.  This set
                 * includes all direct subcells and also the shell of
                 * subcells one step + along each axis; I've done this
                 * because I want to enqueue a cell if any of its
                 * vertices are within fov, and a cell is described
                 * by its lowest vertex (along each axis).
                 */
                size_t augmentedSubcellsSize = CUBE(subdivisionFactor + 1);
                AFK_Cell *augmentedSubcells = new AFK_Cell[augmentedSubcellsSize]; /* TODO thread local storage */
                unsigned int augmentedSubcellsCount = cell.augmentedSubdivide(augmentedSubcells, augmentedSubcellsSize);
        
                if (augmentedSubcellsCount == augmentedSubcellsSize)
                {
                    for (unsigned int i = 0; i < augmentedSubcellsCount; ++i)
                    {
                        enqueueSubcells(augmentedSubcells[i], cell, terrainAtZero, viewerLocation, camera, allVisible);
                    }
                }
                else
                {
                    /* That's clearly a bug :P */
                    std::ostringstream ss;
                    ss << "Cell " << cell << " subdivided into " << augmentedSubcellsCount << " not " << augmentedSubcellsSize;
                    throw AFK_Exception(ss.str());
                }
            
                delete[] augmentedSubcells;
            }
        }
    }
        
    /* Back out the terrain changes for the next cell
     * to try.
     */
    realCell.removeTerrain(terrain);   
}

void AFK_Landscape::enqueueSubcells(
    const AFK_Cell& cell,
    const Vec3<long long>& modifier,
    float terrainAtZero,
    const Vec3<float>& viewerLocation,
    const AFK_Camera& camera)
{
    AFK_Cell modifiedCell(Vec4<long long>(
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[0],
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[1],
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[2],
        cell.coord.v[3]));
    enqueueSubcells(modifiedCell, modifiedCell.parent(), terrainAtZero, viewerLocation, camera, false);
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
                enqueueSubcells(cell, Vec3<long long>(i, j, k), 0.0f, protagonistLocation, *(afk_core.camera));

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
    for (boost::unordered_map<const AFK_Cell, std::pair<bool, AFK_LandscapeCell*> >::iterator lqIt = landQueue.begin(); lqIt != landQueue.end(); ++lqIt)
        if (lqIt->second.second) lqIt->second.second->display(projection);

    landQueue.clear();
}

