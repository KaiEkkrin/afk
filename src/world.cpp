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

#define PROTAGONIST_CELL_DEBUG 0


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
    landscapeTile.makeTerrainDescriptor(
        lSizes,
        tile,
        minCellSize);

    /* Find out whether I'm going to need to be giving this
     * tile some artwork
     */
    bool needsArtwork = false;
    if (display)
    {
        enum AFK_LandscapeTileArtworkState artworkState = landscapeTile.artworkState();
        switch (artworkState)
        {
        case AFK_LANDSCAPE_TILE_NO_PIECE_ASSIGNED:
            needsArtwork = true;
            break;

        case AFK_LANDSCAPE_TILE_PIECE_SWEPT:
            /* I'd like some stats on how often this happens */
            tilesRecomputedAfterSweep.fetch_add(1);
            needsArtwork = true;
            break;

        case AFK_LANDSCAPE_TILE_HAS_ARTWORK:
            needsArtwork = false;
            break;

        default:
            std::ostringstream ss;
            ss << "Unrecognised landscape tile artwork state: " << artworkState;
            throw AFK_Exception(ss.str());
        }
    }
    return needsArtwork;
}

bool AFK_World::checkClaimedShape(
    unsigned int shapeKey,
    AFK_Shape& shape)
{
    shape.makeShrinkformDescriptor(shapeKey, sSizes);

    bool needsArtwork = false;
    enum AFK_ShapeArtworkState artworkState = shape.artworkState();
    switch (artworkState)
    {
    case AFK_SHAPE_NO_PIECE_ASSIGNED:
        needsArtwork = true;
        break;

    case AFK_SHAPE_PIECE_SWEPT:
        shapesRecomputedAfterSweep.fetch_add(1);
        needsArtwork = true;
        break;

    case AFK_SHAPE_HAS_ARTWORK:
        needsArtwork = false;
        break;

    default:
        std::ostringstream ss;
        ss << "Unrecognised shape artwork state: " << artworkState;
        throw AFK_Exception(ss.str());
    }

    return needsArtwork;
}


/* Don't enable these unless you want mega spam */
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
    AFK_JigsawPiece jigsawPiece = landscapeTile.getJigsawPiece(
        threadId,
        /* Put bigger tiles only in the first jigsaw, where they won't
         * get swept out as often (they should be longer lived.)
         */
        tile.coord.v[2] >= 256 ? 0 : 1,
        landscapeJigsaws);
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

void AFK_World::generateShapeArtwork(
    unsigned int shapeKey,
    AFK_Shape& shape,
    unsigned int threadId)
{
    /* Make the list of shrinkform points and cubes. */
    AFK_ShrinkformList shrinkformList;
    shape.buildShrinkformList(shrinkformList);

    /* Assign a jigsaw piece.
     * TODO Different jigsaws for long lived shapes, vs.
     * ephemeral ones, when I have a distinction --
     * unless I'm going to remove all this so that I can
     * be triggering it in the OpenCL instead?
     */
    AFK_JigsawPiece jigsawPiece = shape.getJigsawPiece(threadId, 0, shapeJigsaws);

    /* And now, enqueue this stuff into the compute fair! */
    boost::shared_ptr<AFK_ShrinkformComputeQueue> computeQueue = shapeComputeFair.getUpdateQueue(jigsawPiece.puzzle);
    computeQueue->extend(
        shrinkformList, jigsawPiece.piece, sSizes);

    shapesComputed.fetch_add(1);
}

void AFK_World::generateStartingEntity(
    unsigned int shapeKey,
    AFK_WorldCell& worldCell,
    unsigned int threadId,
    AFK_RNG& rng)
{
    AFK_Shape& shape = (*shapeCache)[shapeKey];
    AFK_ClaimStatus shapeClaimStatus = shape.claimYieldLoop(
        threadId, AFK_CLT_NONEXCLUSIVE_SHARED);

    /* Figure out whether I need to generate its artwork. */
    bool generateArtwork = false;
    if (!shape.hasShrinkformDescriptor() || shape.artworkState() != AFK_SHAPE_HAS_ARTWORK)
    {
        /* Try to upgrade our claim to the shape. */
        if (shapeClaimStatus == AFK_CL_CLAIMED_UPGRADABLE)
            shapeClaimStatus = shape.upgrade(threadId, shapeClaimStatus);

        if (shapeClaimStatus == AFK_CL_CLAIMED)
        {
            generateArtwork = checkClaimedShape(shapeKey, shape);
        }
        else
        {
            /* TODO: I should be able to count on that shape getting
             * generated in time to be used.  I'm not going to try to
             * resume anything.
             * Let's see what happens.
             */
        }
    }

    if (generateArtwork)
        generateShapeArtwork(shapeKey, shape, threadId);

    shape.release(threadId, shapeClaimStatus);

    /* Having done all of that, I should be able to foist this
     * shape upon the world cell.
     */
    worldCell.addStartingEntity(&shape, sSizes, rng);
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
    else /* if (cell.coord.v[1] == 0) */
    {
        /* We display geometry at a cell if its detail pitch is at the
         * target detail pitch, or if it's already the smallest
         * possible cell
         */
        bool display = (cell.coord.v[3] == MIN_CELL_PITCH ||
            worldCell.testDetailPitch(averageDetailPitch.get(), *camera, viewerLocation));

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
            ((renderTerrain || display) && landscapeTile.artworkState() != AFK_LANDSCAPE_TILE_HAS_ARTWORK))
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
            if (landscapeTile.artworkState() == AFK_LANDSCAPE_TILE_HAS_ARTWORK)
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

        if (/* !landscapeTile.hasArtwork() || */
            worldCell.getRealCoord().v[1] >= landscapeTile.getYBoundUpper())
        {
            //if (cell.coord.v[3] == 1024)
            //if (cell.coord.v[0] == 0 && cell.coord.v[1] == 0 && cell.coord.v[2] == 0)
            //if (abs(cell.coord.v[0]) <= (1 * cell.coord.v[3]) &&
            //    abs(cell.coord.v[1]) <= (1 * cell.coord.v[3]) &&
            //    abs(cell.coord.v[2]) <= (1 * cell.coord.v[3]))
            //if (cell.coord.v[3] < 64)
            {
                unsigned int startingEntityCount = worldCell.getStartingEntitiesWanted(
                    staticRng, maxEntitiesPerCell, entitySparseness);

                for (unsigned int e = 0; e < startingEntityCount; ++e)
                {
                    unsigned int shapeKey = worldCell.getStartingEntityShapeKey(staticRng);
                    generateStartingEntity(shapeKey, worldCell, threadId, staticRng);
                }
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
                /* TODO: Movement wants to move into OpenCL.
                 * I don't want to perpetuate the below.
                 */
#if 0
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
#endif
                e->enqueueDisplayUnits(entityDisplayFair);
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

AFK_World::AFK_World(
    const AFK_Config *config,
    const AFK_Computer *computer,
    float _maxDistance,
    unsigned int worldCacheSize,
    unsigned int tileCacheSize,
    unsigned int shapeCacheSize,
    cl_context ctxt):
        startingDetailPitch         (config->startingDetailPitch),
        maxDetailPitch              (config->maxDetailPitch),
        detailPitch                 (config->startingDetailPitch), /* This is a starting point */
        averageDetailPitch          (config->framesPerCalibration, config->startingDetailPitch),
        maxDistance                 (_maxDistance),
        subdivisionFactor           (config->subdivisionFactor),
        minCellSize                 (config->minCellSize),
        lSizes                      (config->subdivisionFactor, config->pointSubdivisionFactor),
        sSizes                      (config->subdivisionFactor,
                                     config->entitySubdivisionFactor,
                                     config->pointSubdivisionFactor),
        maxEntitiesPerCell          (config->maxEntitiesPerCell),
        entitySparseness            (config->entitySparseness)

{
    /* Set up the caches and generator gang. */

    unsigned int tileCacheEntries = tileCacheSize / lSizes.tSize;
    Vec2<int> tilePieceSize = afk_vec2<int>((int)lSizes.tDim, (int)lSizes.tDim);

    enum AFK_JigsawFormat tileTexFormat[3];
    tileTexFormat[0] = AFK_JIGSAW_FLOAT32;          /* Y displacement */
    tileTexFormat[1] = AFK_JIGSAW_4FLOAT8_UNORM;    /* Colour */

    /* Normal: The packed 8-bit signed format doesn't seem to play nicely with
     * cl_gl buffer sharing; I don't know why...
     */
    if (config->clGlSharing)
        tileTexFormat[2] = AFK_JIGSAW_4FLOAT32;
    else
        tileTexFormat[2] = AFK_JIGSAW_4FLOAT8_SNORM;

    landscapeJigsaws = new AFK_JigsawCollection(
        ctxt,
        tilePieceSize,
        (int)tileCacheEntries,
        2, /* I want at least two, so I can put big tiles only into the first one */
        tileTexFormat,
        3,
        computer->getFirstDeviceProps(),
        config->clGlSharing,
        config->concurrency);

    tileCacheEntries = landscapeJigsaws->getPieceCount();
    unsigned int tileCacheBitness = calculateCacheBitness(tileCacheEntries);

    landscapeCache = new AFK_LANDSCAPE_CACHE(
        tileCacheBitness, 8, AFK_HashTile(), tileCacheEntries / 2, 0xffffffffu);

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

    unsigned int shapeCacheEntries = shapeCacheSize / sSizes.tSize / 6;
    Vec2<int> shapePieceSize = afk_vec2<int>(sSizes.tDim, sSizes.tDim);

    enum AFK_JigsawFormat shapeTexFormat[3];
    shapeTexFormat[0] = AFK_JIGSAW_4FLOAT32;        /* Displacement */
    shapeTexFormat[1] = AFK_JIGSAW_4FLOAT8_UNORM;   /* Colour */

    /* Normal: The packed 8-bit signed format doesn't seem to play nicely with
     * cl_gl buffer sharing; I don't know why...
     */
    if (config->clGlSharing)
        shapeTexFormat[2] = AFK_JIGSAW_4FLOAT32;
    else
        shapeTexFormat[2] = AFK_JIGSAW_4FLOAT8_SNORM;

    shapeJigsaws = new AFK_JigsawCollection(
        ctxt,
        shapePieceSize,
        (int)shapeCacheEntries,
        1, /* TODO I'll no doubt be expanding on this */
        shapeTexFormat,
        3,
        computer->getFirstDeviceProps(),
        config->clGlSharing,
        config->concurrency);

    shapeCacheEntries = shapeJigsaws->getPieceCount();
    unsigned int shapeCacheBitness = calculateCacheBitness(shapeCacheEntries);

    shapeCache = new AFK_SHAPE_CACHE(
        shapeCacheBitness, 8, boost::hash<unsigned int>(), shapeCacheEntries / 2, 0xfffffffdu); 

    genGang = new AFK_AsyncGang<struct AFK_WorldCellGenParam, bool>(
            boost::function<bool (unsigned int, const struct AFK_WorldCellGenParam,
                AFK_WorkQueue<struct AFK_WorldCellGenParam, bool>&)>(afk_generateWorldCells),
            100, config->concurrency);

    /* Set up the landscape shader. */
    landscape_shaderProgram = new AFK_ShaderProgram();
    *landscape_shaderProgram << "landscape_fragment" << "landscape_geometry" << "landscape_vertex";
    landscape_shaderProgram->Link();

    landscape_shaderLight = new AFK_ShaderLight(landscape_shaderProgram->program);
    landscape_clipTransformLocation = glGetUniformLocation(landscape_shaderProgram->program, "ClipTransform");

    entity_shaderProgram = new AFK_ShaderProgram();
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

    glGenVertexArrays(1, &shrinkformBaseArray);
    glBindVertexArray(shrinkformBaseArray);
    shrinkformBase = new AFK_ShrinkformBase(sSizes);
    shrinkformBase->initGL();

    glBindVertexArray(0);
    shrinkformBase->teardownGL();

    /* Initialise the statistics. */
    cellsInvisible.store(0);
    tilesQueued.store(0);
    tilesResumed.store(0);
    tilesComputed.store(0);
    tilesRecomputedAfterSweep.store(0);
    entitiesQueued.store(0);
    entitiesMoved.store(0);
    shapesComputed.store(0);
    shapesRecomputedAfterSweep.store(0);
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

    if (shapeJigsaws) delete shapeJigsaws;
    delete shapeCache;

    delete landscape_shaderProgram;
    delete landscape_shaderLight;

    delete entity_shaderProgram;
    delete entity_shaderLight;

    delete shrinkformBase;
    glDeleteVertexArrays(1, &shrinkformBaseArray);

    delete landscapeTerrainBase;
    glDeleteVertexArrays(1, &landscapeTileArray);
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

void AFK_World::flipRenderQueues(cl_context ctxt, const AFK_Frame& newFrame)
{
    /* Verify that the concurrency control business has done
     * its job correctly.
     */
    if (!genGang->assertNoQueuedWork())
        threadEscapes.fetch_add(1);

    landscapeComputeFair.flipQueues();
    landscapeDisplayFair.flipQueues();
    landscapeJigsaws->flipRects(ctxt, newFrame);

    shapeComputeFair.flipQueues();
    entityDisplayFair.flipQueues();
    shapeJigsaws->flipRects(ctxt, newFrame);
}

void AFK_World::alterDetail(float adjustment)
{
    float adj = std::max(std::min(adjustment, 1.2f), 0.85f);

    if (adj > 1.0f || !worldCache->wayOutsideTargetSize())
        detailPitch = std::min(detailPitch * adjustment, maxDetailPitch);

    averageDetailPitch.push(detailPitch);
}

boost::unique_future<bool> AFK_World::updateWorld(void)
{
    /* Maintenance. */
    landscapeCache->doEvictionIfNecessary();
    worldCache->doEvictionIfNecessary();
    shapeCache->doEvictionIfNecessary();

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
     */
    for (long long i = -1; i <= 1; ++i)
        for (long long j = -1; j <= 1; ++j)
            for (long long k = -1; k <= 1; ++k)
                enqueueSubcells(cell, afk_vec3<long long>(i, j, k), protagonistLocation, *(afk_core.camera));

    return genGang->start();
}

void AFK_World::doComputeTasks(void)
{
    /* The fair organises the terrain lists and jigsaw pieces by
     * puzzle, so that I can easily batch up work that applies to the
     * same jigsaw:
     */
    std::vector<boost::shared_ptr<AFK_TerrainComputeQueue> > terrainComputeQueues;
    landscapeComputeFair.getDrawQueues(terrainComputeQueues);

    /* The fair's queues are in the same order as the puzzles in
     * the jigsaw collection.
     */
    for (unsigned int puzzle = 0; puzzle < terrainComputeQueues.size(); ++puzzle)
    {
        terrainComputeQueues[puzzle]->computeStart(afk_core.computer, landscapeJigsaws->getPuzzle(puzzle), lSizes);
    }

    std::vector<boost::shared_ptr<AFK_ShrinkformComputeQueue> > shrinkformComputeQueues;
    shapeComputeFair.getDrawQueues(shrinkformComputeQueues);

    for (unsigned int puzzle = 0; puzzle < shrinkformComputeQueues.size(); ++puzzle)
    {
        shrinkformComputeQueues[puzzle]->computeStart(afk_core.computer, shapeJigsaws->getPuzzle(puzzle), sSizes);
    }

    /* If I finalise stuff now, the y-reduce information will
     * be in the landscape tiles in time for the display
     * to edit out any cells I now know to be empty of terrain.
     */
    for (unsigned int puzzle = 0; puzzle < terrainComputeQueues.size(); ++puzzle)
    {
        terrainComputeQueues[puzzle]->computeFinish();
    }

    for (unsigned int puzzle = 0; puzzle < shrinkformComputeQueues.size(); ++puzzle)
    {
        shrinkformComputeQueues[puzzle]->computeFinish();
    }
}

void AFK_World::display(const Mat4<float>& projection, const AFK_Light &globalLight)
{
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
    std::vector<boost::shared_ptr<AFK_LandscapeDisplayQueue> > landscapeDrawQueues;
    landscapeDisplayFair.getDrawQueues(landscapeDrawQueues);

    /* Those queues are in puzzle order. */
    for (unsigned int puzzle = 0; puzzle < landscapeDrawQueues.size(); ++puzzle)
    {
        landscapeDrawQueues[puzzle]->draw(landscape_shaderProgram, landscapeJigsaws->getPuzzle(puzzle), lSizes);
    }

    glBindVertexArray(0);

    /* Render the shapes */
    glUseProgram(entity_shaderProgram->program);
    entity_shaderLight->setupLight(globalLight);
    glUniformMatrix4fv(entity_projectionTransformLocation, 1, GL_TRUE, &projection.m[0][0]);
    AFK_GLCHK("shape uniforms")

    glBindVertexArray(shrinkformBaseArray);
    AFK_GLCHK("shape bindVertexArray")

    std::vector<boost::shared_ptr<AFK_EntityDisplayQueue> > entityDrawQueues;
    entityDisplayFair.getDrawQueues(entityDrawQueues);

    for (unsigned int puzzle = 0; puzzle < entityDrawQueues.size(); ++puzzle)
    {
        entityDrawQueues[puzzle]->draw(entity_shaderProgram, shapeJigsaws->getPuzzle(puzzle), sSizes);
    }

    glBindVertexArray(0);
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
    std::cout << "Detail pitch:                 " << detailPitch << std::endl;
    PRINT_RATE_AND_RESET("Cells found invisible:        ", cellsInvisible)
    PRINT_RATE_AND_RESET("Tiles queued:                 ", tilesQueued)
    PRINT_RATE_AND_RESET("Tiles resumed:                ", tilesResumed)
    PRINT_RATE_AND_RESET("Tiles computed:               ", tilesComputed)
    PRINT_RATE_AND_RESET("Tiles recomputed after sweep: ", tilesRecomputedAfterSweep)
    PRINT_RATE_AND_RESET("Entities queued:              ", entitiesQueued)
    PRINT_RATE_AND_RESET("Entities moved:               ", entitiesMoved)
    PRINT_RATE_AND_RESET("Shapes computed:              ", shapesComputed)
    PRINT_RATE_AND_RESET("Shapes recomputed after sweep:", shapesRecomputedAfterSweep)
    std::cout <<         "Cumulative thread escapes:    " << threadEscapes.load() << std::endl;
#endif
}

void AFK_World::printCacheStats(std::ostream& ss, const std::string& prefix)
{
#if PRINT_CACHE_STATS
    worldCache->printStats(ss, "World cache");
    landscapeCache->printStats(ss, "Landscape cache");
    shapeCache->printStats(ss, "Shape cache");
#endif
}

