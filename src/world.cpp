/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include "core.hpp"
#include "debug.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
#include "rng/boost_taus88.hpp"
#include "rng/rng.hpp"
#include "world.hpp"


#define PRINT_CHECKPOINTS 1
#define PRINT_CACHE_STATS 0


/* TODO remove debug?  (or something) */
#define PROTAGONIST_CELL_DEBUG 1


/* The AFK_WorldCellGenParam flags. */
#define AFK_WCG_FLAG_ENTIRELY_VISIBLE   2 /* Cell is already known to be entirely within the viewing frustum */
#define AFK_WCG_FLAG_TERRAIN_RENDER     4 /* Render the terrain regardless of visibility or LoD */
#define AFK_WCG_FLAG_RESUME             8 /* This is a resume after dependent cells were computed */

/* The cell generating worker. */

bool afk_generateWorldCells(
    unsigned int threadId,
    struct AFK_WorldCellGenParam param,
    AFK_WorkQueue<struct AFK_WorldCellGenParam, bool>& queue)
{
    const AFK_Cell& cell                = param.cell;
    AFK_World *world                    = param.world;

    bool renderTerrain                  = ((param.flags & AFK_WCG_FLAG_TERRAIN_RENDER) != 0);
    bool resume                         = ((param.flags & AFK_WCG_FLAG_RESUME) != 0);

    bool retval = true;

    AFK_WorldCell& worldCell = (*(world->worldCache))[cell];

    /* I want an exclusive claim on world cells to stop me from
     * repeating the recursive search process.
     * The render flag means "definitely render this cell's terrain
     * regardless of visibility" (for landscape composition purposes), and the
     * resume flag means "you needed to wait for dependencies, keep
     * trying"; in those cases I don't want an exclusive claim, and
     * I won't do a recursive search.
     */
    if (worldCell.claimYieldLoop(threadId,
        (renderTerrain || resume) ? AFK_CLT_NONEXCLUSIVE : AFK_CLT_EXCLUSIVE) == AFK_CL_CLAIMED)
    {
        /* This releases the world cell itself when it's done
         * (which isn't at the end of its processing).
         */
        retval = world->generateClaimedWorldCell(
            worldCell, threadId, param, queue);
    }

    /* If this cell had a dependency ... */
    if (param.dependency)
    {
        if (param.dependency->count.fetch_sub(1) == 1)
        {
            /* That dependency is complete.  Enqueue the final cell
             * and delete it.
             */
            queue.push(*(param.dependency->finalCell));
            delete param.dependency->finalCell;
            delete param.dependency;
        }
    }

    return retval;
}


/* AFK_World implementation */

bool AFK_World::checkClaimedLandscapeTile(
    const AFK_Tile& tile,
    AFK_LandscapeTile& landscapeTile,
    bool display)
{
    /* A LandscapeTile has several stages of creation.
     * First, make sure it's got a terrain descriptor, which
     * will tell us what terrain features go here.
     */

    /* Tile colour debugging goes here. */
    Vec3<float>* forcedTint = NULL;
    landscapeTile.makeTerrainDescriptor(
        lSizes.pointSubdivisionFactor,
        subdivisionFactor,
        tile,
        minCellSize,
        forcedTint);

    /* Find out whether I'm going to need to be giving this
     * tile some artwork
     */
    return (display && !landscapeTile.hasArtwork());
}

/* TODO this will make mega spam */
#define DEBUG_JIGSAW_ASSOCIATION 0
#define DEBUG_JIGSAW_ASSOCIATION_GL 0

#define DEBUG_TERRAIN_COMPUTE_QUEUE 0

void AFK_World::generateLandscapeArtwork(
    const AFK_Tile& tile,
    AFK_LandscapeTile& landscapeTile,
    unsigned int threadId)
{
    /* Create the terrain list, which is composed out of
     * the terrain descriptor for this landscape tile, and
     * those of all the parent tiles (so that child tiles
     * just add detail to an existing terrain).
     */
    AFK_TerrainList terrainList;
    landscapeTile.buildTerrainList(
        threadId,
        terrainList,
        tile,
        subdivisionFactor,
        maxDistance,
        landscapeCache);

    /* Assign a jigsaw piece for this tile, so that the
     * compute phase has somewhere to paint.
     */
    AFK_JigsawPiece jigsawPiece = landscapeTile.getJigsawPiece(threadId, landscapeJigsaws);
#if DEBUG_JIGSAW_ASSOCIATION
    AFK_DEBUG_PRINTL("Compute: " << tile << " -> " << jigsawPiece)
#endif

    /* Now, I need to enqueue the terrain list and the
     * jigsaw piece into one of the rides at the compute fair...
     */
    boost::shared_ptr<AFK_TerrainComputeQueue> computeQueue = landscapeComputeFair.getUpdateQueue(jigsawPiece.puzzle);
#if DEBUG_TERRAIN_COMPUTE_QUEUE
    AFK_TerrainComputeUnit unit = computeQueue->extend(
        terrainList, jigsawPiece.piece, &landscapeTile, lSizes);
    AFK_DEBUG_PRINTL("Pushed to queue for " << tile << ": " << unit << ": " << std::endl)
    //AFK_DEBUG_PRINTL(computeQueue->debugTerrain(unit, lSizes))
#else
    computeQueue->extend(terrainList, jigsawPiece.piece, &landscapeTile, lSizes);
#endif

    tilesComputed.fetch_add(1);
}

bool AFK_World::generateClaimedWorldCell(
    AFK_WorldCell& worldCell,
    unsigned int threadId,
    struct AFK_WorldCellGenParam param,
    AFK_WorkQueue<struct AFK_WorldCellGenParam, bool>& queue)
{
    const AFK_Cell& cell                = param.cell;
    const Vec3<float>& viewerLocation   = param.viewerLocation;
    const AFK_Camera *camera            = param.camera;

    bool entirelyVisible                = ((param.flags & AFK_WCG_FLAG_ENTIRELY_VISIBLE) != 0);
    bool renderTerrain                  = ((param.flags & AFK_WCG_FLAG_TERRAIN_RENDER) != 0);
    bool resume                         = ((param.flags & AFK_WCG_FLAG_RESUME) != 0);

    bool retval = true;

    worldCell.bind(cell, minCellSize);

    /* Check for visibility. */
    bool someVisible = entirelyVisible;
    bool allVisible = entirelyVisible;

    if (!entirelyVisible) worldCell.testVisibility(*camera, someVisible, allVisible);
    if (!someVisible && !renderTerrain)
    {
        cellsInvisible.fetch_add(1);

        /* Nothing else to do with it now either. */
        worldCell.release(threadId, AFK_CL_CLAIMED);
    }
    else /* if (cell.coord.v[1] == 0) */ /* TODO when I remove this, I get bits of landscape
                                    * floating around at different heights -- looks like
                                    * I've managed to let the height I enumerate a tile at
                                    * have an effect on where it's computed or something.
                                    * Eww.  Try to track through the places `cell' and
                                    * `tile' might be accidentally swapped.
                                    * I'm reasonably confident the last minute y-cull
                                    * is correct. */
    {
        /* We display geometry at a cell if its detail pitch is at the
         * target detail pitch, or if it's already the smallest
         * possible cell
         */
        bool display = (cell.coord.v[3] == MIN_CELL_PITCH ||
            worldCell.testDetailPitch(averageDetailPitch.get(), *camera, viewerLocation));

        /* TODO: Non-landscape stuff goes here.  :-) */

        /* Find the tile where any landscape at this cell would be
         * homed
         */
        AFK_Tile tile = afk_tile(cell);

        /* We always at least touch the landscape.  Higher detailed
         * landscape tiles are dependent on lower detailed ones for their
         * terrain description.
         */
        AFK_LandscapeTile& landscapeTile = (*landscapeCache)[tile];
        AFK_ClaimStatus landscapeClaimStatus = landscapeTile.claimYieldLoop(
            threadId, AFK_CLT_NONEXCLUSIVE_SHARED);

        bool generateArtwork = false;
        if (!landscapeTile.hasTerrainDescriptor() ||
            ((renderTerrain || display) && !landscapeTile.hasArtwork()))
        {
            /* In order to generate this tile we need to upgrade
             * our claim if we can.
             */
            if (landscapeClaimStatus == AFK_CL_CLAIMED_UPGRADABLE)
                landscapeClaimStatus = landscapeTile.upgrade(threadId, landscapeClaimStatus);

            if (landscapeClaimStatus == AFK_CL_CLAIMED)
            {
                generateArtwork = checkClaimedLandscapeTile(
                    tile, landscapeTile, display);
            }
            else
            {
                /* Uh oh.  Having an un-generated tile here is not
                 * an option.  Queue a resume of this cell.
                 * Some thread or other will have got the upgradable
                 * claim and will fix it so that when we come back
                 * here, the tile will be generated.
                 */
                struct AFK_WorldCellGenParam resumeParam(param);
                resumeParam.flags |= AFK_WCG_FLAG_RESUME;
                queue.push(resumeParam);
                tilesResumed.fetch_add(1);
            }
        }

        /* If I was picked to generate the geometry, do that. */
        if (generateArtwork)
            generateLandscapeArtwork(tile, landscapeTile, threadId);

        /* At any rate, I should relinquish my claim on the tile
         * now.
         */
        landscapeTile.release(threadId, landscapeClaimStatus);

        if (display)
        {
            if (landscapeTile.hasArtwork())
            {
                /* Get it to make us a unit to
                 * feed into the display queue.
                 * The reason I go through the landscape tile here is so
                 * that it has the opportunity to reject the tile for display
                 * because the cell is entirely outside the y bounds.
                 */
                landscapeClaimStatus = landscapeTile.claimYieldLoop(
                    threadId, AFK_CLT_NONEXCLUSIVE_SHARED);
                AFK_JigsawPiece jigsawPiece;
                AFK_LandscapeDisplayUnit unit;
                bool displayThisTile = landscapeTile.makeDisplayUnit(cell, minCellSize, jigsawPiece, unit);
                landscapeTile.release(threadId, landscapeClaimStatus);

                if (displayThisTile)
                {
#if DEBUG_JIGSAW_ASSOCIATION
                    AFK_DEBUG_PRINTL("Display: " << tile << " -> " << jigsawPiece << " -> " << unit)
#endif

                    boost::shared_ptr<AFK_LandscapeDisplayQueue> ldq =
                        landscapeDisplayFair.getUpdateQueue(jigsawPiece.puzzle);
                    ldq->add(unit, &landscapeTile);
                    tilesQueued.fetch_add(1);
                }
            }
        }

        /* Now that I've done all that, I can get to the
         * business of handling the entities within the cell.
         * Firstly, so long as it's above the landscape, give it
         * some starting entities if it hasn't got them
         * already.
         */

        /* TODO: This RNG will make the same set of entities every
         * time the cell is re-displayed.  This is fine for probably
         * static, landscape decoration type objects.  But I'm also
         * going to want to have transient objects popping into
         * existence, and those should be done with a different,
         * long-period RNG that I retain (in thread local storage),
         * perhaps a Mersenne Twister.
         */
        AFK_Boost_Taus88_RNG staticRng;
        staticRng.seed(cell.rngSeed());

        if (!landscapeTile.hasArtwork() ||
            worldCell.getRealCoord().v[1] >= landscapeTile.getYBoundUpper())
        {
            /* TODO For now, I'm going to just build entities at 
             * one particular LoD.  I need to consider how I
             * work with this ...
             */
            //if (cell.coord.v[3] == 1024)
            //if (cell.coord.v[0] == 0 && cell.coord.v[1] == 0 && cell.coord.v[2] == 0)
            //if (abs(cell.coord.v[0]) <= (1 * cell.coord.v[3]) &&
            //    abs(cell.coord.v[1]) <= (1 * cell.coord.v[3]) &&
            //    abs(cell.coord.v[2]) <= (1 * cell.coord.v[3]))
            {
                worldCell.doStartingEntities(
                    shape, /* TODO vary shapes! :P */
                    minCellSize,
                    lSizes.pointSubdivisionFactor,
                    subdivisionFactor,
                    staticRng);
            }
        }

        AFK_ENTITY_LIST::iterator eIt = worldCell.entitiesBegin();
        while (eIt != worldCell.entitiesEnd())
        {
            AFK_Entity *e = *eIt;
            AFK_ENTITY_LIST::iterator nextEIt = eIt;
            bool updatedEIt = false;
            if (e->claimYieldLoop(threadId, AFK_CLT_EXCLUSIVE) == AFK_CL_CLAIMED)
            {
                AFK_Cell newCell;
                if (e->animate(afk_core.getStartOfFrameTime(), cell, minCellSize, newCell))
                {
                    /* This entity has moved to a different cell.
                     * Dig it up and feed it into its entity list.
                     */
                    
                    /* Yes, I'm going to be creating this cell if
                     * it doesn't exist.  And not claiming it.  So
                     * there's a chance the entity gets cleaned up
                     * by the evictor.  I can handle with that.
                     * TODO: Permanent entities -- should prevent
                     * the evictor from getting rid of cells that
                     * contain them, and should also cause their
                     * cells to always get enumerated regardless of
                     * angle and distance.
                     */
                    nextEIt = worldCell.eraseEntity(eIt);
                    updatedEIt = true;

                    /* TODO: There is a very tiny possibility of the
                     * following scenario here:
                     * - cell gets created in the cache
                     * - cell gets grabbed by the eviction thread and
                     * destroyed
                     * - moveEntity() gets called on a dead cell and
                     * crashes
                     * To avoid this I need to ensure that cells are
                     * naturally created in an un-evictable state.  I'm
                     * not quite sure how to ensure that.
                     */
                    (*worldCache)[newCell].moveEntity(e);
                    entitiesMoved.fetch_add(1);
                }

                /* TODO: Collisions probably go here.  I'll no doubt
                 * want to do that in OpenCL for performance, and
                 * all sorts.
                 */

                /* Entities are always displayed.  They don't
                 * exist at multiple LoDs.
                 * TODO: Maybe some day I'll want to make very
                 * large entities out of component parts so
                 * that I can zoom in on their LoD just like
                 * I can with the landscape ?
                 */

                /* TODO: Figure out the list of lights that apply.
                 * And some day, the list of shadows that apply,
                 * too.  Rah!
                 */
                e->enqueueForDrawing(threadId);
                entitiesQueued.fetch_add(1);

                e->release(threadId, AFK_CL_CLAIMED);
            }

            /* Above, we might have updated nextEIt to point to the
             * next item to look at (e.g. if we erased an item).
             * But if we haven't, update the iterator now.
             */
            if (updatedEIt) eIt = nextEIt;
            else ++eIt;
        }

        /* Pop any new entities out from the move queue into the
         * proper list.
         */
        worldCell.popMoveQueue();

        /* We don't need this any more */
        worldCell.release(threadId, AFK_CL_CLAIMED);

        /* If the terrain here was at too coarse a resolution to
         * be displayable, recurse through the subcells
         */
        if (!display && !renderTerrain && someVisible && !resume)
        {
            size_t subcellsSize = CUBE(subdivisionFactor);
            AFK_Cell *subcells = new AFK_Cell[subcellsSize]; /* TODO avoid heap thrashing somehow.  Maybe make it an iterator */
            unsigned int subcellsCount = cell.subdivide(subcells, subcellsSize, subdivisionFactor);

            if (subcellsCount == subcellsSize)
            {
                for (unsigned int i = 0; i < subcellsCount; ++i)
                {
                    struct AFK_WorldCellGenParam subcellParam;
                    subcellParam.cell               = subcells[i];
                    subcellParam.world              = param.world;
                    subcellParam.viewerLocation     = viewerLocation;
                    subcellParam.camera             = camera;
                    subcellParam.flags              = (allVisible ? AFK_WCG_FLAG_ENTIRELY_VISIBLE : 0);
                    subcellParam.dependency         = NULL;
                    queue.push(subcellParam);
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
#if 0
    else
    {
        worldCell.release(threadId, AFK_CL_CLAIMED);
    }
#endif

    return retval;
}

/* Utility -- tries to come up with a sensible cache bitness,
 * given the number of entries.
 */
static unsigned int calculateCacheBitness(unsigned int entries)
{
    unsigned int bitness;
    for (bitness = 0; (1u << bitness) < (entries << 1); ++bitness);
    return bitness;
}

AFK_World::AFK_World( const AFK_Config *config,
    const AFK_Computer *computer,
    float _maxDistance,
    unsigned int worldCacheSize,
    unsigned int tileCacheSize,
    unsigned int maxShapeSize):
        startingDetailPitch         (config->startingDetailPitch),
        maxDetailPitch              (config->maxDetailPitch),
        detailPitch                 (config->startingDetailPitch), /* This is a starting point */
        averageDetailPitch          (config->framesPerCalibration, config->startingDetailPitch),
        maxDistance                 (_maxDistance),
        subdivisionFactor           (config->subdivisionFactor),
        minCellSize                 (config->minCellSize),
        lSizes                      (config->pointSubdivisionFactor)
{
    /* Set up the caches and generator gang. */

    tileCacheEntries = tileCacheSize / lSizes.tSize;
    unsigned int tileCacheBitness = calculateCacheBitness(tileCacheEntries);

    landscapeCache = new AFK_LANDSCAPE_CACHE(
        tileCacheBitness, 8, AFK_HashTile());

    /* TODO Right now I don't have a sensible value for the
     * world cache size, because I'm not doing any exciting
     * geometry work with it.  When I am, I'll get that.
     * But for now, just make a guess.
     */
    unsigned int worldCacheEntrySize = SQUARE(lSizes.pointSubdivisionFactor);
    unsigned int worldCacheEntries = worldCacheSize / worldCacheEntrySize;
    unsigned int worldCacheBitness = calculateCacheBitness(worldCacheEntries);

    worldCache = new AFK_WORLD_CACHE(
        worldCacheBitness, 8, AFK_HashCell(), worldCacheEntries, 0xfffffffeu);

    /* TODO change this into a thing like AFK_LandscapeSizes */
    unsigned int shapeVsSize, shapeIsSize;
    afk_getShapeSizes(lSizes.pointSubdivisionFactor, shapeVsSize, shapeIsSize);
    unsigned int shapeEntrySize = shapeVsSize + shapeIsSize;
    unsigned int maxShapes = maxShapeSize / shapeEntrySize;

    shapeVsQueue = new AFK_GLBufferQueue(shapeVsSize, maxShapes, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
    shapeIsQueue = new AFK_GLBufferQueue(shapeIsSize, maxShapes, GL_ELEMENT_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

    genGang = new AFK_AsyncGang<struct AFK_WorldCellGenParam, bool>(
            boost::function<bool (unsigned int, const struct AFK_WorldCellGenParam,
                AFK_WorkQueue<struct AFK_WorldCellGenParam, bool>&)>(afk_generateWorldCells),
            100, config->concurrency);

    /* Set up the shapes.  TODO more than one ?! */
    shape = new AFK_ShapeChevron(shapeVsQueue, shapeIsQueue, config->concurrency);

    /* Set up the landscape shader. */
    landscape_shaderProgram = new AFK_ShaderProgram();
    *landscape_shaderProgram << "landscape_fragment" << "landscape_geometry" << "landscape_vertex";
    landscape_shaderProgram->Link();

    landscape_shaderLight = new AFK_ShaderLight(landscape_shaderProgram->program);
    landscape_jigsawPiecePitchLocation = glGetUniformLocation(landscape_shaderProgram->program, "JigsawPiecePitch");
    landscape_clipTransformLocation = glGetUniformLocation(landscape_shaderProgram->program, "ClipTransform");
    landscape_jigsawYDispTexSamplerLocation = glGetUniformLocation(landscape_shaderProgram->program, "JigsawYDispTex");
    landscape_jigsawColourTexSamplerLocation = glGetUniformLocation(landscape_shaderProgram->program, "JigsawColourTex");
    landscape_jigsawNormalTexSamplerLocation = glGetUniformLocation(landscape_shaderProgram->program, "JigsawNormalTex");
    landscape_displayTBOSamplerLocation = glGetUniformLocation(landscape_shaderProgram->program, "DisplayTBO");

    entity_shaderProgram = new AFK_ShaderProgram();
    /* TODO How much stuff will I need here ? */
    /* TODO I'm going to need to change this to its own
     * program.  And sort out the whole ruddy business
     * with the uniform buffer into which I pass the
     * transformation matrices (and pretty soon, the
     * light list).  Narghle narghle!!
     */
    *entity_shaderProgram << "shape_fragment" << "shape_vertex";
    entity_shaderProgram->Link();

    entity_shaderLight = new AFK_ShaderLight(entity_shaderProgram->program);
    entity_projectionTransformLocation = glGetUniformLocation(entity_shaderProgram->program, "ProjectionTransform");

    glGenVertexArrays(1, &landscapeTileArray);
    glBindVertexArray(landscapeTileArray);
    landscapeTerrainBase = new AFK_TerrainBaseTile(lSizes);
    landscapeTerrainBase->initGL();

    /* Done. */
    glBindVertexArray(0);
    landscapeTerrainBase->teardownGL();

    /* Sort out the compute kernels */
    if (!computer->findKernel("makeSurface", surfaceKernel))
        throw AFK_Exception("Cannot find surface kernel");
    if (!computer->findKernel("makeTerrain", terrainKernel))
        throw AFK_Exception("Cannot find terrain kernel");

    landscapeYReduce = new AFK_YReduce(computer);

    /* Set up that stage timer.
     * TODO With the way it's set up right now,
     * I can only have one puzzle
     */
    std::vector<std::string> stageNames;
    stageNames.push_back("acquire for cl"); /* 0 */
    stageNames.push_back("terrain");        /* 1 */
    stageNames.push_back("surface");        /* 2 */
    stageNames.push_back("y reduce");       /* 3 */
    stageNames.push_back("release from cl");/* 4 */
    stageNames.push_back("copy to gl");     /* 5 */
    stageNames.push_back("draw elements");  /* 6 */
    displayTimer = new AFK_StageTimer("Display", stageNames, 60);

    /* Initialise the statistics. */
    cellsInvisible.store(0);
    tilesQueued.store(0);
    tilesResumed.store(0);
    tilesComputed.store(0);
    entitiesQueued.store(0);
    entitiesMoved.store(0);
    threadEscapes.store(0);
}

AFK_World::~AFK_World()
{
    /* Delete the generator gang first, so that I know that
     * any worker threads have stopped before I start deleting
     * the things they're using.
     */
    delete genGang;

    if (landscapeJigsaws) delete landscapeJigsaws;
    delete landscapeCache;
    delete worldCache;
    delete shape;

    delete shapeVsQueue;
    delete shapeIsQueue;

    delete landscape_shaderProgram;
    delete landscape_shaderLight;

    delete entity_shaderProgram;
    delete entity_shaderLight;

    delete landscapeTerrainBase;
    glDeleteVertexArrays(1, &landscapeTileArray);

    delete landscapeYReduce;

    delete displayTimer;
}

void AFK_World::initJigsaw(cl_context ctxt, const AFK_Computer *computer, const AFK_Config *config)
{
    Vec2<int> pieceSize = afk_vec2<int>((int)lSizes.tDim, (int)lSizes.tDim);

    /* TODO: The below switch from GL_RGBA32F to GL_RGBA fixed things.
     * I get the impression that having more than 32 bits per pixel
     * might be unwise.  So next, I should try splitting the jigsaw
     * into one RGB colour jigsaw and one R32F y-displacement jigsaw.
     */
    enum AFK_JigsawFormat texFormat[3];
    texFormat[0] = AFK_JIGSAW_FLOAT32;          /* Y displacement */
    texFormat[1] = AFK_JIGSAW_4FLOAT8_UNORM;    /* Colour */
    texFormat[2] = AFK_JIGSAW_4HALF32;          /* Normal */

    landscapeJigsaws = new AFK_JigsawCollection(
        ctxt,
        pieceSize,
        (int)tileCacheEntries,
        texFormat,
        3,
        computer->getFirstDeviceProps(),
        config->clGlSharing,
        config->concurrency,
        afk_newLandscapeJigsawMeta,
        afk_core.computingFrame);
}

void AFK_World::enqueueSubcells(
    const AFK_Cell& cell,
    const Vec3<long long>& modifier,
    const Vec3<float>& viewerLocation,
    const AFK_Camera& camera)
{
    AFK_Cell modifiedCell = afk_cell(afk_vec4<long long>(
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[0],
        cell.coord.v[1] + cell.coord.v[3] * modifier.v[1],
        cell.coord.v[2] + cell.coord.v[3] * modifier.v[2],
        cell.coord.v[3]));

    struct AFK_WorldCellGenParam cellParam;
    cellParam.cell                  = modifiedCell;
    cellParam.world                 = this;
    cellParam.viewerLocation        = viewerLocation;
    cellParam.camera                = &camera;
    cellParam.flags                 = 0;
    cellParam.dependency            = NULL;
    (*genGang) << cellParam;
}

void AFK_World::flipRenderQueues(const AFK_Frame& newFrame)
{
    /* Verify that the concurrency control business has done
     * its job correctly.
     */
    if (!genGang->assertNoQueuedWork())
        threadEscapes.fetch_add(1);

    landscapeComputeFair.flipQueues();
    landscapeDisplayFair.flipQueues();
    landscapeJigsaws->flipRects(newFrame);
}

void AFK_World::alterDetail(float adjustment)
{
    /* TODO Better way of stopping afk from hogging the entirety
     * of system memory?
     * :(
     */
    float adj = std::max(std::min(adjustment, 1.2f), 0.85f);

    if (adj > 1.0f || !worldCache->wayOutsideTargetSize())
        detailPitch = std::min(detailPitch * adjustment, maxDetailPitch);

    averageDetailPitch.push(detailPitch);
}

boost::unique_future<bool> AFK_World::updateWorld(void)
{
    /* Maintenance. */
    worldCache->doEvictionIfNecessary();

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
        cell = parentCell, parentCell = cell.parent(subdivisionFactor));

    /* Draw that cell and the other cells around it.
     * TODO Can I optimise this?  It's going to attempt
     * cells that are very very far away.  Mind you,
     * that might be okay.
     */
    for (long long i = -1; i <= 1; ++i)
        for (long long j = -1; j <= 1; ++j)
            for (long long k = -1; k <= 1; ++k)
                enqueueSubcells(cell, afk_vec3<long long>(i, j, k), protagonistLocation, *(afk_core.camera));

    return genGang->start();
}

void AFK_World::doComputeTasks(void)
{
    displayTimer->restart();

    /* The fair organises the terrain lists and jigsaw pieces by
     * puzzle, so that I can easily batch up work that applies to the
     * same jigsaw:
     */
    std::vector<boost::shared_ptr<AFK_TerrainComputeQueue> > computeQueues;
    landscapeComputeFair.getDrawQueues(computeQueues);

    cl_context ctxt;
    cl_command_queue q;
    afk_core.computer->lock(ctxt, q);

    /* The fair's queues are in the same order as the puzzles in
     * the jigsaw collection.
     */
    for (unsigned int puzzle = 0; puzzle < computeQueues.size(); ++puzzle)
    {
        cl_int error;
        unsigned int unitCount = computeQueues[puzzle]->getUnitCount();
        if (unitCount == 0) continue;

        cl_mem terrainBufs[3];
        computeQueues[puzzle]->copyToClBuffers(ctxt, terrainBufs);

        AFK_Jigsaw *jigsaw = landscapeJigsaws->getPuzzle(puzzle);
        cl_mem *jigsawMem = jigsaw->acquireForCl(ctxt, q);

        displayTimer->hitStage(0);

        AFK_CLCHK(clSetKernelArg(terrainKernel, 0, sizeof(cl_mem), &terrainBufs[0]))
        AFK_CLCHK(clSetKernelArg(terrainKernel, 1, sizeof(cl_mem), &terrainBufs[1]))
        AFK_CLCHK(clSetKernelArg(terrainKernel, 2, sizeof(cl_mem), &terrainBufs[2]))
        AFK_CLCHK(clSetKernelArg(terrainKernel, 3, sizeof(cl_mem), &jigsawMem[0]))
        AFK_CLCHK(clSetKernelArg(terrainKernel, 4, sizeof(cl_mem), &jigsawMem[1]))

        size_t terrainDim[3];
        terrainDim[0] = terrainDim[1] = lSizes.tDim;
        terrainDim[2] = unitCount;
        AFK_CLCHK(clEnqueueNDRangeKernel(q, terrainKernel, 3, NULL, &terrainDim[0], NULL, 0, NULL, NULL))

        displayTimer->hitStage(1);

        /* For the next two I'm going to need this ...
         */
        cl_sampler jigsawYDispSampler = clCreateSampler(
            ctxt,
            CL_FALSE,
            CL_ADDRESS_CLAMP_TO_EDGE,
            CL_FILTER_NEAREST,
            &error);
        afk_handleClError(error);

        /* Now, I need to run the kernel to bake the surface normals.
         */
        AFK_CLCHK(clSetKernelArg(surfaceKernel, 0, sizeof(cl_mem), &terrainBufs[2]))
        AFK_CLCHK(clSetKernelArg(surfaceKernel, 1, sizeof(cl_mem), &jigsawMem[0]))
        AFK_CLCHK(clSetKernelArg(surfaceKernel, 2, sizeof(cl_sampler), &jigsawYDispSampler))
        AFK_CLCHK(clSetKernelArg(surfaceKernel, 3, sizeof(cl_mem), &jigsawMem[2]))

        size_t surfaceGlobalDim[3];
        surfaceGlobalDim[0] = surfaceGlobalDim[1] = lSizes.tDim - 1;
        surfaceGlobalDim[2] = unitCount;

        size_t surfaceLocalDim[3];
        surfaceLocalDim[0] = surfaceLocalDim[1] = lSizes.tDim - 1;
        surfaceLocalDim[2] = 1;

        AFK_CLCHK(clEnqueueNDRangeKernel(q, surfaceKernel, 3, 0, &surfaceGlobalDim[0], &surfaceLocalDim[0], 0, NULL, NULL))

        displayTimer->hitStage(2);

        /* Finally, do the y reduce. */
        landscapeYReduce->compute(
            ctxt,
            q,
            puzzle,
            unitCount,
            &terrainBufs[2],
            &jigsawMem[0],
            &jigsawYDispSampler,
            lSizes);

        displayTimer->hitStage(3);

        /* TODO Can I keep this thing lying around long term ? */
        AFK_CLCHK(clReleaseSampler(jigsawYDispSampler))

        for (unsigned int i = 0; i < 3; ++i)
        {
            AFK_CLCHK(clReleaseMemObject(terrainBufs[i]))
        }

        jigsaw->releaseFromCl(q);

        displayTimer->hitStage(4);

        /* TODO REMOVEME (somehow)
         * Debug this a little bit.
         */
#if 0
        std::vector<Vec2<int> > changedPiecesDebug;
        std::vector<Vec4<float> > changesDebug;
        jigsaw->debugReadChanges<Vec4<float> >(changedPiecesDebug, changesDebug);
        for (unsigned int cp = 0; cp < changedPiecesDebug.size(); ++cp)
        {
            std::cout << "Computed piece " << changedPiecesDebug[cp] << " from jigsaw puzzle " << puzzle << std::endl;
            for (int x = 0; x < 3; ++x)
            {
                for (int z = 0; z < 3; ++z)
                {
                    std::cout << "(" << x << ", " << z << "): ";
                    std::cout << changesDebug[x * lSizes.tDim + z] << std::endl;
                }
            }
        }
#endif
    }

    afk_core.computer->unlock();
}

void AFK_World::display(const Mat4<float>& projection, const AFK_Light &globalLight)
{
    /* TODO In this function, don't render cells that were
     * completely culled by a y-bound computation that
     * happened this frame.
     * I need to:
     * - Pass the landscape tile pointer into the displayed
     * landscape queue at enumeration time
     * - At this point, before drawing, iterate through the
     * displayed landscape queue and update the y-bounds
     * from the tile objects
     * - Don't begin render of any tiles that are entirely
     * excluded.  That means copying the list before upload,
     * because there's no glDrawRangeElementsInstanced.
     * I'm sure I'll cope.
     */

    /* Render the landscape */
    glUseProgram(landscape_shaderProgram->program);
    landscape_shaderLight->setupLight(globalLight);
    glUniformMatrix4fv(landscape_clipTransformLocation, 1, GL_TRUE, &projection.m[0][0]);
    AFK_GLCHK("landscape uniforms")

    glBindVertexArray(landscapeTileArray);
    AFK_GLCHK("landscape bindVertexArray")


    /* Now that I've set that up, make the texture that describes
     * where the tiles are in space ...
     */
    std::vector<boost::shared_ptr<AFK_LandscapeDisplayQueue> > drawQueues;
    landscapeDisplayFair.getDrawQueues(drawQueues);

    /* Those queues are in puzzle order. */
    for (unsigned int puzzle = 0; puzzle < drawQueues.size(); ++puzzle)
    {
        if (drawQueues[puzzle]->getUnitCount() == 0) continue;

        AFK_Jigsaw *jigsaw = landscapeJigsaws->getPuzzle(puzzle);

        /* Fill out ye olde uniform variable with the jigsaw
         * piece pitch.
         */
        Vec2<float> jigsawPiecePitchST = jigsaw->getPiecePitchST();
        glUniform2fv(landscape_jigsawPiecePitchLocation, 1, &jigsawPiecePitchST.v[0]);

        /* The first texture is the jigsaw Y-displacement */
        glActiveTexture(GL_TEXTURE0);
        jigsaw->bindTexture(0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUniform1i(landscape_jigsawYDispTexSamplerLocation, 0);

        /* The second texture is the jigsaw colour */
        glActiveTexture(GL_TEXTURE1);
        jigsaw->bindTexture(1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(landscape_jigsawColourTexSamplerLocation, 1);

        /* The third texture is the jigsaw normal */
        glActiveTexture(GL_TEXTURE2);
        jigsaw->bindTexture(2);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(landscape_jigsawNormalTexSamplerLocation, 2);

        /* The fourth texture is the landscape display texbuf,
         * which explains to the vertex shader which tile it's
         * drawing and where in the jigsaw to look.
         */
        glActiveTexture(GL_TEXTURE3);
        unsigned int instanceCount = drawQueues[puzzle]->copyToGl();
        glUniform1i(landscape_displayTBOSamplerLocation, 3);

        displayTimer->hitStage(5);

        /* TODO remove debug */
        //std::cout << "copyToGl() reduced " << std::dec << drawQueues[puzzle]->getUnitCount() << " units to " << instanceCount << std::endl;

#if DEBUG_JIGSAW_ASSOCIATION_GL
        AFK_DEBUG_PRINTL("Drawing cell 0: " << drawQueues[puzzle]->getUnit(0) << " of puzzle=" << puzzle)
#endif

#if AFK_GL_DEBUG
        landscape_shaderProgram->Validate();
#endif
        glDrawElementsInstanced(GL_TRIANGLES, lSizes.iCount * 3, GL_UNSIGNED_SHORT, 0, instanceCount);
        AFK_GLCHK("landscape cell drawElementsInstanced")

        displayTimer->hitStage(6);
    }

    glBindVertexArray(0);

    /* Render the shapes */
    /* TODO re-enable this later */
#if 0
    glUseProgram(entity_shaderProgram->program);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    shape->display(
        entity_shaderLight,
        globalLight,
        entity_projectionTransformLocation,
        projection);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
#endif
}

void AFK_World::finaliseComputeTasks(void)
{
    std::vector<boost::shared_ptr<AFK_TerrainComputeQueue> > computeQueues;
    landscapeComputeFair.getDrawQueues(computeQueues);

    for (unsigned int puzzle = 0; puzzle < computeQueues.size(); ++puzzle)
    {
        unsigned int unitCount = computeQueues[puzzle]->getUnitCount();
        if (unitCount == 0) continue;

        landscapeYReduce->readBack(
            unitCount,
            &computeQueues[puzzle]->landscapeTiles);
    }
}

/* Worker for the below. */
#if PRINT_CHECKPOINTS
static float toRatePerSecond(unsigned long long quantity, boost::posix_time::time_duration& interval)
{
    return (float)quantity * 1000.0f / (float)interval.total_milliseconds();
}

#define PRINT_RATE_AND_RESET(s, v) std::cout << s << toRatePerSecond((v).load(), timeSinceLastCheckpoint) << "/second" << std::endl; \
    (v).store(0);
#endif

void AFK_World::checkpoint(boost::posix_time::time_duration& timeSinceLastCheckpoint)
{
#if PRINT_CHECKPOINTS
    std::cout << "Detail pitch:             " << detailPitch << std::endl;
    PRINT_RATE_AND_RESET("Cells found invisible:    ", cellsInvisible)
    PRINT_RATE_AND_RESET("Tiles queued:             ", tilesQueued)
    PRINT_RATE_AND_RESET("Tiles resumed:            ", tilesResumed)
    PRINT_RATE_AND_RESET("Tiles computed:           ", tilesComputed)
    PRINT_RATE_AND_RESET("Entities queued:          ", entitiesQueued)
    PRINT_RATE_AND_RESET("Entities moved:           ", entitiesMoved)
    std::cout <<         "Cumulative thread escapes:" << threadEscapes.load() << std::endl;
#endif
}

void AFK_World::printCacheStats(std::ostream& ss, const std::string& prefix)
{
#if PRINT_CACHE_STATS
    worldCache->printStats(ss, "World cache");
    landscapeCache->printStats(ss, "Landscape cache");
#endif
}

