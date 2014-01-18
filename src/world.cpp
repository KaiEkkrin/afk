/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#include "afk.hpp"

#include <cassert>
#include <cfloat>
#include <cmath>
#include <memory>

#include "core.hpp"
#include "debug.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
#include "rng/boost_taus88.hpp"
#include "rng/rng.hpp"
#include "work.hpp"
#include "world.hpp"
#include "world_cell.hpp"


#define PRINT_CHECKPOINTS 1
#define PRINT_CACHE_STATS 0
#define PRINT_JIGSAW_STATS 0

#define PROTAGONIST_CELL_DEBUG 0


/* The AFK_WorldCellGenParam flags. */
#define AFK_WCG_FLAG_ENTIRELY_VISIBLE   2 /* Cell is already known to be entirely within the viewing frustum */
#define AFK_WCG_FLAG_TERRAIN_RENDER     4 /* Render the terrain regardless of visibility or LoD */
#define AFK_WCG_FLAG_RESUME             8 /* This is a resume after dependent cells were computed */

/* The cell generating worker. */

/* TODO I probably want to part-revert the work in here, to reflect
 * the new upgrade() that changes an existing Claim rather than
 * making a new one.
 */

bool afk_generateWorldCells(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    const struct AFK_WorldWorkThreadLocal& threadLocal,
    AFK_WorldWorkQueue& queue)
{
    const AFK_Cell cell                 = param.world.cell;
    AFK_World *world                    = afk_core.world;

    bool renderTerrain                  = ((param.world.flags & AFK_WCG_FLAG_TERRAIN_RENDER) != 0);
    bool resume                         = ((param.world.flags & AFK_WCG_FLAG_RESUME) != 0);

    bool retval = true;

    /* I want an exclusive claim on world cells to stop me from
     * repeating the recursive search process.
     * The render flag means "definitely render this cell's terrain
     * regardless of visibility" (for landscape composition purposes), and the
     * resume flag means "you needed to wait for dependencies, keep
     * trying"; in those cases I don't want an exclusive claim, and
     * I won't do a recursive search.
     */
    unsigned int claimFlags = AFK_CL_BLOCK;
    if (!renderTerrain && !resume) claimFlags |= AFK_CL_EXCLUSIVE;

    auto worldCellClaim = world->worldCache->insertAndClaim(threadId, cell, claimFlags);
    if (worldCellClaim.isValid())
    {
        retval = world->generateClaimedWorldCell(
            worldCellClaim, threadId, param.world, threadLocal, queue);
    }
    else
    {
        /* This cell is busy, try again in a moment --
         * and track its volume again
         */
        world->volumeLeftToEnumerate.fetch_add(CUBE(cell.coord.v[3]));

        AFK_WorldWorkQueue::WorkItem resumeItem;
        resumeItem.func = afk_generateWorldCells;
        resumeItem.param.world = param.world;

        if (param.world.dependency) param.world.dependency->retain();
        queue.push(resumeItem);
        world->cellsResumed.fetch_add(1);
    }

    /* If this cell had a dependency ... */
    if (param.world.dependency)
    {
        if (param.world.dependency->check(queue))
        {
            world->dependenciesFollowed.fetch_add(1);
               delete param.world.dependency;
        }
    }

    /* I enumerated this cell */
    world->volumeLeftToEnumerate.fetch_sub(CUBE(cell.coord.v[3]));

    return retval;
}

/* The cell-generation-finished check. */
bool afk_worldGenerationFinished(void)
{
    return (afk_core.world->volumeLeftToEnumerate.load() == 0);
}

AFK_AsyncTaskFinishedFunc afk_worldGenerationFinishedFunc = afk_worldGenerationFinished;


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
        enum AFK_LandscapeTileArtworkState artworkState = landscapeTile.artworkState(landscapeJigsaws);
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


/* Don't enable these unless you want mega spam */
#define DEBUG_JIGSAW_ASSOCIATION 0
#define DEBUG_JIGSAW_ASSOCIATION_GL 0

#define DEBUG_TERRAIN_COMPUTE_QUEUE 0

bool AFK_World::generateLandscapeArtwork(
    const AFK_Tile& tile,
    AFK_LandscapeTile& landscapeTile,
    unsigned int threadId,
    std::vector<AFK_Tile>& missingTiles)
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
        landscapeCache,
        missingTiles);

    /* If we have any missing, we need to return with a `needsResume'
     * status right away
     */
    if (!missingTiles.empty()) return true;

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
    std::shared_ptr<AFK_TerrainComputeQueue> computeQueue = landscapeComputeFair.getUpdateQueue(jigsawPiece.puzzle);
    Vec2<int> piece2D = afk_vec2<int>(jigsawPiece.u, jigsawPiece.v);
#if DEBUG_TERRAIN_COMPUTE_QUEUE
    AFK_TerrainComputeUnit unit = computeQueue->extend(
        terrainList, piece2D, tile, lSizes);
    AFK_DEBUG_PRINTL("Pushed to queue for " << tile << ": " << unit << ": " << std::endl)
    //AFK_DEBUG_PRINTL(computeQueue->debugTerrain(unit, lSizes))
#else
    computeQueue->extend(terrainList, piece2D, tile, lSizes);
#endif

    tilesComputed.fetch_add(1);
    return false;
}

void AFK_World::displayLandscapeTile(
    const AFK_Cell& cell,
    const AFK_Tile& tile,
    const AFK_LandscapeTile& landscapeTile,
    unsigned int threadId)
{
#if DEBUG_JIGSAW_ASSOCIATION
    AFK_DEBUG_PRINTL("Display: " << tile << " (" << landscapeTile << ")")
#endif
    assert(landscapeTile.artworkState(landscapeJigsaws) == AFK_LANDSCAPE_TILE_HAS_ARTWORK);

    /* Get it to make us a unit to
     * feed into the display queue.
     * The reason I go through the landscape tile here is so
     * that it has the opportunity to reject the tile for display
     * because the cell is entirely outside the y bounds.
     */
    AFK_JigsawPiece jigsawPiece;
    AFK_LandscapeDisplayUnit unit;
    bool reallyDisplayThisTile = landscapeTile.makeDisplayUnit(cell, minCellSize, landscapeJigsaws, jigsawPiece, unit);

    if (reallyDisplayThisTile)
    {
#if DEBUG_JIGSAW_ASSOCIATION
        AFK_DEBUG_PRINTL("Display: " << tile << " -> " << landscapeTile << " -> " << unit)
#endif
        std::shared_ptr<AFK_LandscapeDisplayQueue> ldq =
            landscapeDisplayFair.getUpdateQueue(jigsawPiece.puzzle);

        ldq->add(unit, tile);
        tilesQueued.fetch_add(1);
    }
}

void AFK_World::generateStartingEntity(
    unsigned int shapeKey,
    AFK_WorldCell& worldCell,
    unsigned int threadId,
    AFK_RNG& rng)
{
    /* I don't need to initialise it here.  We'll come to that when
     * we try to draw it
     */
    worldCell.addStartingEntity(threadId, shapeKey, sSizes, rng);
}

bool AFK_World::generateClaimedWorldCell(
    AFK_WORLD_CACHE::Claim& claim,
    unsigned int threadId,
    const struct AFK_WorldWorkParam::World& param,
    const struct AFK_WorldWorkThreadLocal& threadLocal,
    AFK_WorldWorkQueue& queue)
{
    const AFK_Cell& cell                = param.cell;
    const Vec3<float>& viewerLocation   = threadLocal.viewerLocation;
    const AFK_Camera& camera            = threadLocal.camera;

    bool entirelyVisible                = ((param.flags & AFK_WCG_FLAG_ENTIRELY_VISIBLE) != 0);
    bool renderTerrain                  = ((param.flags & AFK_WCG_FLAG_TERRAIN_RENDER) != 0);
    bool resume                         = ((param.flags & AFK_WCG_FLAG_RESUME) != 0);

    bool retval = true;

    AFK_WorldCell& worldCell = claim.get();
    worldCell.bind(cell, minCellSize);

    /* Check for visibility. */
    bool someVisible = entirelyVisible;
    bool allVisible = entirelyVisible;

    if (!entirelyVisible) worldCell.testVisibility(camera, someVisible, allVisible);
    if (!someVisible && !renderTerrain)
    {
        cellsInvisible.fetch_add(1);
    }
    else /* if (cell.coord.v[1] == 0) */
    {
        bool needsResume = false;
        std::vector<AFK_Tile> missingTiles;

        /* We display geometry at a cell if its detail pitch is at the
         * target detail pitch, or if it's already the smallest
         * possible cell.
         * Note that the smallest possible cell is 2.  I can't go to 1,
         * because it will preclude landscape half-tiles (which would
         * be at +/- 0.5).
         */
        bool display = (cell.coord.v[3] == 2 ||
            worldCell.testDetailPitch(threadLocal.detailPitch, camera, viewerLocation));

        /* Find the tile where any landscape at this cell would be
         * homed
         */
        AFK_Tile tile = afk_tile(cell);

        /* I'm going to want this in a moment.
         */
        float landscapeTileUpperYBound = FLT_MAX;

        /* We always at least touch the landscape.  Higher detailed
         * landscape tiles are dependent on lower detailed ones for their
         * terrain description.
         */
        auto landscapeClaim = landscapeCache->insertAndClaim(threadId, tile, AFK_CL_BLOCK | AFK_CL_UPGRADE);
        if (landscapeClaim.isValid())
        {
            landscapeTileUpperYBound = landscapeClaim.getShared().getYBoundUpper();
        
            if (!landscapeClaim.getShared().hasTerrainDescriptor() ||
                ((renderTerrain || display) && landscapeClaim.getShared().artworkState(landscapeJigsaws) != AFK_LANDSCAPE_TILE_HAS_ARTWORK))
            {
                /* In order to generate this tile we need to upgrade
                 * our claim if we can.
                 */
                if (landscapeClaim.upgrade())
                {
                    AFK_LandscapeTile& landscapeTile = landscapeClaim.get();
                    if (checkClaimedLandscapeTile(tile, landscapeTile, display))
                        needsResume = generateLandscapeArtwork(tile, landscapeTile, threadId, missingTiles);
        
                    if (!needsResume && display)
                        displayLandscapeTile(cell, tile, landscapeTile, threadId);
                }
                else
                {
                    needsResume = true;
                }
            }
            else if (display)
            {
                displayLandscapeTile(cell, tile, landscapeClaim.getShared(), threadId);
            }
        }
        else
        {
            needsResume = true;
        }

        /* If I need a resume for the landscape tile, push it in */
        if (needsResume)
        {
            /* Because I'm doing a resume, I need to account for it in the
             * remaining volume counter
             */
            volumeLeftToEnumerate.fetch_add(CUBE(cell.coord.v[3]));

            AFK_WorldWorkQueue::WorkItem resumeItem;
            resumeItem.func = afk_generateWorldCells;
            resumeItem.param.world = param;
            resumeItem.param.world.flags |= AFK_WCG_FLAG_RESUME;
            if (param.dependency) param.dependency->retain();

            /* If we have missing tiles, that resume needs to be a
             * dependency of those missing tiles:
             */
            if (!missingTiles.empty())
            {
                AFK_WorldWorkParam::Dependency *dep = new AFK_WorldWorkParam::Dependency(resumeItem);
                dep->retain(missingTiles.size());

                for (auto m : missingTiles)
                {
                    volumeLeftToEnumerate.fetch_add(CUBE(m.coord.v[2]));

                    AFK_WorldWorkQueue::WorkItem missingItem;
                    missingItem.func = afk_generateWorldCells;
                    missingItem.param.world.cell        = afk_cell(m, 0);
                    missingItem.param.world.flags       = AFK_WCG_FLAG_ENTIRELY_VISIBLE | AFK_WCG_FLAG_TERRAIN_RENDER;
                    missingItem.param.world.dependency  = dep;
                    queue.push(missingItem);
                }

                tilesResumed.fetch_add(missingTiles.size());
            }
            else
            {
                /* We enqueue the resume directly */
                queue.push(resumeItem);
            }

            tilesResumed.fetch_add(1);
        }

        if (!resume && !renderTerrain)
        {
            /* Now that I've done all that, I can get to the
             * business of handling the entities within the cell.
             * Firstly, so long as it's above the landscape, give it
             * some starting entities if it hasn't got them
             * already.
             */
        
            if (worldCell.getRealCoord().v[1] >= landscapeTileUpperYBound)
            {
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
                int startingEntityCount = worldCell.getStartingEntitiesWanted(
                    staticRng, entitySparseness);
        
                for (int e = 0; e < startingEntityCount; ++e)
                {
                    int shapeKey = worldCell.getStartingEntityShapeKey(staticRng);
                    generateStartingEntity(shapeKey, worldCell, threadId, staticRng);
                }
            }
        
            for (int eI = 0; eI < worldCell.getEntityCount(); ++eI)
            {
                AFK_Entity& e = worldCell.getEntityAt(eI);
                if (e.notProcessedYet(afk_core.computingFrame))
                {
                    /* Account for this shape in the volume left to enumerate */
                    volumeLeftToEnumerate.fetch_add(CUBE(SHAPE_CELL_MAX_DISTANCE));

                    /* Make sure everything I need in that shape
                     * has been computed ...
                     */
                    AFK_WorldWorkQueue::WorkItem shapeCellItem;
                    shapeCellItem.func                          = afk_generateEntity;
                    shapeCellItem.param.shape.cell              = afk_keyedCell(afk_vec4<int64_t>(
                                                                    0, 0, 0, SHAPE_CELL_MAX_DISTANCE), e.shapeKey);
                    shapeCellItem.param.shape.transformation    = e.getTransformation();
                    shapeCellItem.param.shape.flags             = 0;
        
#if AFK_SHAPE_ENUM_DEBUG
                    shapeCellItem.param.shape.asedWorldCell     = cell;
                    shapeCellItem.param.shape.asedCounter       = eI;
                    AFK_DEBUG_PRINTL("ASED: Enqueued entity: worldCell=" << cell << ", entity counter=" << eI)
#endif
        
                    shapeCellItem.param.shape.dependency        = nullptr;
                    queue.push(shapeCellItem);
            
                    entitiesQueued.fetch_add(1);
                }
            }
        }

        /* We don't need this any more */
        claim.release();

        /* If the terrain here was at too coarse a resolution to
         * be displayable, recurse through the subcells
         */
        if (!display && !renderTerrain && someVisible && !resume)
        {
            /* I'm about to enumerate this cell's volume in subcells */
            volumeLeftToEnumerate.fetch_add(CUBE(cell.coord.v[3]));

            size_t subcellsSize = CUBE(subdivisionFactor);
            AFK_Cell *subcells = new AFK_Cell[subcellsSize]; /* TODO avoid heap thrashing somehow.  Maybe make it an iterator */
            unsigned int subcellsCount = cell.subdivide(subcells, subcellsSize, subdivisionFactor);
            assert(subcellsCount == subcellsSize);

            for (unsigned int i = 0; i < subcellsCount; ++i)
            {
                AFK_WorldWorkQueue::WorkItem subcellItem;
                subcellItem.func                     = afk_generateWorldCells;
                subcellItem.param.world.cell         = subcells[i];
                subcellItem.param.world.flags        = (allVisible ? AFK_WCG_FLAG_ENTIRELY_VISIBLE : 0);
                subcellItem.param.world.dependency   = nullptr;
                queue.push(subcellItem);
            }

            delete[] subcells;
        }
    }

    return retval;
}

AFK_World::AFK_World(
    const AFK_ConfigSettings& settings,
    AFK_Computer *computer,
    AFK_ThreadAllocation& threadAlloc,
    float _maxDistance,
    size_t worldCacheSize,
    size_t tileCacheSize,
    size_t shapeCacheSize,
    AFK_RNG *setupRng):
        startingDetailPitch         (settings.startingDetailPitch),
        maxDetailPitch              (settings.maxDetailPitch),
        shape                       (settings, threadAlloc, shapeCacheSize),
        entityFair2DIndex           (AFK_MAX_VAPOUR),
        maxDistance                 (_maxDistance),
        subdivisionFactor           (settings.subdivisionFactor),
        minCellSize                 (settings.minCellSize),
        lSizes                      (settings),
        sSizes                      (settings),
        entitySparseness            (settings.entitySparseness)

{
    /* Declare the jigsaw images and decide how big things can be. */
    Vec3<int> tpSize = afk_vec3<int>((int)lSizes.tDim, (int)lSizes.tDim, 1);
    AFK_JigsawBufferUsage tBu = settings.clGlSharing ?
        AFK_JigsawBufferUsage::CL_GL_SHARED : AFK_JigsawBufferUsage::CL_GL_COPIED;

    Vec3<int> vpSize = afk_vec3<int>((int)sSizes.tDim, (int)sSizes.tDim, (int)sSizes.tDim);

    /* Each edge piece will have 3 faces horizontally by 2 vertically to
     * cram the 6 faces together in a better manner than stringing them
     * in a line.
     */
    Vec3<int> epSize = afk_vec3<int>(sSizes.eDim * 3, sSizes.eDim * 2, 1);

    /* I can't use cl_gl sharing along with fake 3D,
     * because of the need to convert it to proper 3D
     * for the GL.
     */
    AFK_JigsawBufferUsage vBu = (settings.clGlSharing && !computer->useFake3DImages(settings) ?
        AFK_JigsawBufferUsage::CL_GL_SHARED : AFK_JigsawBufferUsage::CL_GL_COPIED);

    /* Packed 8-bit signed doesn't seem to work with cl_gl sharing */
    AFK_JigsawFormat normalFormat = settings.clGlSharing ?
        AFK_JigsawFormat::FLOAT32_4 : AFK_JigsawFormat::FLOAT8_SNORM_4;

    AFK_JigsawMemoryAllocation jigsawAlloc(
        {
            /* 0: Landscape */
            AFK_JigsawMemoryAllocation::Entry(
                {
                    AFK_JigsawImageDescriptor( /* Y displacement */
                        tpSize,
                        AFK_JigsawFormat::FLOAT32,
                        AFK_JigsawDimensions::TWO,
                        tBu
                    ),
                    AFK_JigsawImageDescriptor( /* Colour */
                        tpSize,
                        AFK_JigsawFormat::FLOAT8_UNORM_4,
                        AFK_JigsawDimensions::TWO,
                        tBu
                    ),
                    AFK_JigsawImageDescriptor( /* Normal */
                        tpSize,
                        normalFormat,
                        AFK_JigsawDimensions::TWO,
                        tBu
                    ),
                },    
                2,
                1.0f),

            /* 1: Vapour */
            AFK_JigsawMemoryAllocation::Entry(
                {
                    AFK_JigsawImageDescriptor( /* Feature: colour and density (TODO: try to split these, colour needs only 8bpc) */
                        /* TODO *2: Density won't need to be copied to the GL */
                        vpSize,
                        AFK_JigsawFormat::FLOAT32_4,
                        AFK_JigsawDimensions::THREE,
                        vBu
                    ),
                    AFK_JigsawImageDescriptor( /* Normal */
                        vpSize,
                        normalFormat,
                        AFK_JigsawDimensions::THREE,
                        vBu
                    )
                },
                1, /* TODO: Might want to try the same by-LoD caching trick as with the landscape;
                    * try counting recomputes in the vapour first
                    */
                2.0f),

            /* 2: Edges */
            AFK_JigsawMemoryAllocation::Entry(
                {
                    AFK_JigsawImageDescriptor( /* Overlap */
                        epSize,
                        AFK_JigsawFormat::UINT32_2,
                        AFK_JigsawDimensions::TWO,
                        tBu
                    )
                },
                1,
                2.0f)
        },
        settings.concurrency,
        computer->useFake3DImages(settings),
        settings.jigsawUsageFactor,
        computer->getFirstDeviceProps());

    /* Set up the caches and generator gang. */

    size_t tileCacheEntries = tileCacheSize / lSizes.tSize;

    tileCacheEntries = jigsawAlloc.at(0).getPieceCount();

    landscapeCache = new AFK_LANDSCAPE_CACHE(
        8,
        AFK_HashTile(),
        tileCacheEntries / 2,
        threadAlloc.getNewId());

    /* TODO Right now I don't have a sensible value for the
     * world cache size, because I'm not doing any exciting
     * geometry work with it.  When I am, I'll get that.
     * But for now, just make a guess.
     */
    unsigned int worldCacheEntrySize = SQUARE(lSizes.pointSubdivisionFactor);
    size_t worldCacheEntries = worldCacheSize / worldCacheEntrySize;

    worldCache = new AFK_WORLD_CACHE(
        8,
        AFK_HashCell(),
        worldCacheEntries,
        threadAlloc.getNewId());

    // TODO: Fix the size of the shape cache (which is no doubt
    // in a huge mess)
    //unsigned int shapeCacheEntries = shapeCacheSize / (32 * SQUARE(sSizes.eDim) * 6 + 16 * CUBE(sSizes.tDim));

    genGang = new AFK_AsyncGang<union AFK_WorldWorkParam, bool, struct AFK_WorldWorkThreadLocal, afk_worldGenerationFinishedFunc>(
        100, threadAlloc, settings.concurrency);
    volumeLeftToEnumerate.store(0);

    std::cout << "AFK_World: Configuring landscape jigsaws with: " << jigsawAlloc.at(0) << std::endl;
    landscapeJigsaws = new AFK_JigsawCollection(
        computer,
        jigsawAlloc.at(0),
        computer->getFirstDeviceProps(),
        0);

    std::cout << "AFK_World: Configuring vapour jigsaws with: " << jigsawAlloc.at(1) << std::endl;
    vapourJigsaws = new AFK_JigsawCollection(
        computer,
        jigsawAlloc.at(1),
        computer->getFirstDeviceProps(),
        AFK_MAX_VAPOUR);

    std::cout << "AFK_World: Configuring edge jigsaws with: " << jigsawAlloc.at(2) << std::endl;
    edgeJigsaws = new AFK_JigsawCollection(
        computer,
        jigsawAlloc.at(2),
        computer->getFirstDeviceProps(),
        0);

    /* Set up the jigsaws (which need to be aware of which thread IDs
     * the gang is using).
     */

    /* Set up the landscape shader. */
    landscape_shaderProgram = new AFK_ShaderProgram();
    *landscape_shaderProgram << "landscape_fragment" << "landscape_geometry" << "landscape_vertex";
    landscape_shaderProgram->Link();

    landscape_shaderLight = new AFK_ShaderLight(landscape_shaderProgram->program);
    landscape_clipTransformLocation = glGetUniformLocation(landscape_shaderProgram->program, "ClipTransform");
    landscape_skyColourLocation = glGetUniformLocation(landscape_shaderProgram->program, "SkyColour");
    landscape_farClipDistanceLocation = glGetUniformLocation(landscape_shaderProgram->program, "FarClipDistance");

    entity_shaderProgram = new AFK_ShaderProgram();
    *entity_shaderProgram << "shape_fragment" << "shape_geometry" << "shape_vertex";
    entity_shaderProgram->Link();

    entity_shaderLight = new AFK_ShaderLight(entity_shaderProgram->program);
    entity_projectionTransformLocation = glGetUniformLocation(entity_shaderProgram->program, "ProjectionTransform");
    entity_windowSizeLocation = glGetUniformLocation(entity_shaderProgram->program, "WindowSize");
    entity_skyColourLocation = glGetUniformLocation(entity_shaderProgram->program, "SkyColour");
    entity_farClipDistanceLocation = glGetUniformLocation(entity_shaderProgram->program, "FarClipDistance");

    glGenVertexArrays(1, &landscapeTileArray);
    glBindVertexArray(landscapeTileArray);
    landscapeTerrainBase = new AFK_TerrainBaseTile(lSizes);
    landscapeTerrainBase->initGL();

    /* Done. */
    glBindVertexArray(0);
    landscapeTerrainBase->teardownGL();

    glGenVertexArrays(1, &edgeShapeBaseArray);
    glBindVertexArray(edgeShapeBaseArray);
    edgeShapeBase = new AFK_3DEdgeShapeBase(sSizes);
    edgeShapeBase->initGL();

    glBindVertexArray(0);
    edgeShapeBase->teardownGL();

    /* Make the base colour for the landscape here */
    landscape_baseColour = afk_vec3<float>(
        setupRng->frand(), setupRng->frand(), setupRng->frand());

    /* Initialise the statistics. */
    cellsInvisible.store(0);
    cellsResumed.store(0);
    tilesQueued.store(0);
    tilesResumed.store(0);
    tilesComputed.store(0);
    tilesRecomputedAfterSweep.store(0);
    entitiesQueued.store(0);
    entitiesMoved.store(0);
    shapeCellsInvisible.store(0);
    shapeCellsReducedOut.store(0);
    shapeCellsResumed.store(0);
    shapeVapoursComputed.store(0);
    shapeEdgesComputed.store(0);
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

    if (edgeJigsaws) delete edgeJigsaws;
    if (vapourJigsaws) delete vapourJigsaws;

    delete landscape_shaderProgram;
    delete landscape_shaderLight;

    delete entity_shaderProgram;
    delete entity_shaderLight;

    delete edgeShapeBase;
    glDeleteVertexArrays(1, &edgeShapeBaseArray);

    delete landscapeTerrainBase;
    glDeleteVertexArrays(1, &landscapeTileArray);
}

void AFK_World::enqueueSubcells(
    const AFK_Cell& cell,
    const Vec3<int64_t>& modifier)
{
    AFK_Cell modifiedCell = afk_cell(afk_vec4<int64_t>(
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[0],
        cell.coord.v[1] + cell.coord.v[3] * modifier.v[1],
        cell.coord.v[2] + cell.coord.v[3] * modifier.v[2],
        cell.coord.v[3]));

    /* Submit the volume I'm about to generate to the enumeration tracker */
    volumeLeftToEnumerate.fetch_add(CUBE(cell.coord.v[3]));

    AFK_WorldWorkQueue::WorkItem cellItem;
    cellItem.func                        = afk_generateWorldCells;
    cellItem.param.world.cell            = modifiedCell;
    cellItem.param.world.flags           = 0;
    cellItem.param.world.dependency      = nullptr;
    (*genGang) << cellItem;
}

void AFK_World::flipRenderQueues(const AFK_Frame& newFrame)
{
    /* Verify that the concurrency control business has done
     * its job correctly.
     */
    //if (!genGang->noQueuedWork())
    //    threadEscapes.fetch_add(1);
    assert(genGang->noQueuedWork());

    landscapeComputeFair.flipQueues();
    landscapeDisplayFair.flipQueues();
    landscapeJigsaws->flip(newFrame);

    vapourComputeFair.flipQueues();
    edgeComputeFair.flipQueues();
    entityDisplayFair.flipQueues();
    vapourJigsaws->flip(newFrame);
    edgeJigsaws->flip(newFrame);
}

#if 0
void AFK_World::alterDetail(float adjustment)
{
    float adj = std::max(std::min(adjustment, 1.2f), 0.85f);

    if (adj > 1.0f || !worldCache->wayOutsideTargetSize())
        detailPitch = std::min(detailPitch * adjustment, maxDetailPitch);

    averageDetailPitch.push(detailPitch);
}

float AFK_World::getLandscapeDetailPitch(void) const
{
    return averageDetailPitch.get();
}
#endif

float AFK_World::getEntityDetailPitch(float landscapeDetailPitch) const
{
    return landscapeDetailPitch *
        (float)sSizes.pointSubdivisionFactor / (float)lSizes.pointSubdivisionFactor;
}

std::future<bool> AFK_World::updateWorld(
    const AFK_Camera& camera,
    const AFK_Object& protagonistObj,
    float detailPitch)
{
    /* Maintenance. */
    landscapeCache->doEvictionIfNecessary();
    worldCache->doEvictionIfNecessary();
    shape.updateWorld();

    /* First, transform the protagonist location and its facing
     * into integer cell-space.
     */
    Mat4<float> protagonistTransformation = protagonistObj.getTransformation();
    Vec4<float> hgProtagonistLocation = protagonistTransformation *
        afk_vec4<float>(0.0f, 0.0f, 0.0f, 1.0f);
    Vec3<float> protagonistLocation = afk_vec3<float>(
        hgProtagonistLocation.v[0] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[1] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[2] / hgProtagonistLocation.v[3]);
    Vec4<int64_t> csProtagonistLocation = afk_vec4<int64_t>(
        (int64_t)(protagonistLocation.v[0] / minCellSize),
        (int64_t)(protagonistLocation.v[1] / minCellSize),
        (int64_t)(protagonistLocation.v[2] / minCellSize),
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
    for (cell = parentCell = protagonistCell;
        (float)cell.coord.v[3] < maxDistance;
        cell = parentCell, parentCell = cell.parent(subdivisionFactor));

    /* Draw that cell and the other cells around it.
     */
    for (int64_t i = -1; i <= 1; ++i)
        for (int64_t j = -1; j <= 1; ++j)
            for (int64_t k = -1; k <= 1; ++k)
                enqueueSubcells(cell, afk_vec3<int64_t>(i, j, k));

    struct AFK_WorldWorkThreadLocal threadLocal;
    threadLocal.camera = camera;
    threadLocal.viewerLocation = protagonistLocation;
    threadLocal.detailPitch = detailPitch;

    return genGang->start(threadLocal);
}

void AFK_World::doComputeTasks(unsigned int threadId)
{
    /* The fair organises the terrain lists and jigsaw pieces by
     * puzzle, so that I can easily batch up work that applies to the
     * same jigsaw:
     */
    std::vector<std::shared_ptr<AFK_TerrainComputeQueue> > terrainComputeQueues;
    landscapeComputeFair.getDrawQueues(terrainComputeQueues);

    /* The fair's queues are in the same order as the puzzles in
     * the jigsaw collection.
     * Convention: the queue functions lock the jigsaws.  (Those locks aren't
     * directly interdicting the jigsaws from the update threads, the queue
     * flip is doing that.  This is more of a make-sure.)
     */
    for (unsigned int puzzle = 0; puzzle < terrainComputeQueues.size(); ++puzzle)
    {
        terrainComputeQueues.at(puzzle)->computeStart(afk_core.computer, landscapeJigsaws->getPuzzle(puzzle), lSizes, landscape_baseColour);
    }

#if AFK_RENDER_ENTITIES
    std::vector<std::shared_ptr<AFK_3DVapourComputeQueue> > vapourComputeQueues;
    vapourComputeFair.getDrawQueues(vapourComputeQueues);
    assert(vapourComputeQueues.size() <= 1);
    if (vapourComputeQueues.size() == 1)
        vapourComputeQueues.at(0)->computeStart(afk_core.computer, vapourJigsaws, sSizes);

    std::vector<std::shared_ptr<AFK_3DEdgeComputeQueue> > edgeComputeQueues;
    edgeComputeFair.getDrawQueues(edgeComputeQueues);

    for (unsigned int i = 0; i < edgeComputeQueues.size(); ++i)
    {
        unsigned int vapourPuzzle, edgePuzzle;
        entityFair2DIndex.get2D(i, vapourPuzzle, edgePuzzle);
        AFK_Jigsaw *vapourJigsaw = vapourJigsaws->getPuzzle(vapourPuzzle);
        AFK_Jigsaw *edgeJigsaw = edgeJigsaws->getPuzzle(edgePuzzle);
        if (vapourJigsaw && edgeJigsaw)
        {
            edgeComputeQueues.at(i)->computeStart(
                afk_core.computer,
                vapourJigsaw,
                edgeJigsaw,
                sSizes);
        }
    }
#endif

    /* If I finalise stuff now, the y-reduce information will
     * be in the landscape tiles in time for the display
     * to edit out any cells I now know to be empty of terrain.
     */
    for (unsigned int puzzle = 0; puzzle < terrainComputeQueues.size(); ++puzzle)
    {
        terrainComputeQueues.at(puzzle)->computeFinish(threadId, landscapeJigsaws->getPuzzle(puzzle), landscapeCache);
    }

#if AFK_RENDER_ENTITIES
    if (vapourComputeQueues.size() == 1)
        vapourComputeQueues.at(0)->computeFinish(threadId, vapourJigsaws, shape.shapeCellCache);

    for (unsigned int i = 0; i < edgeComputeQueues.size(); ++i)
    {
        unsigned int vapourPuzzle, edgePuzzle;
        entityFair2DIndex.get2D(i, vapourPuzzle, edgePuzzle);
        AFK_Jigsaw *vapourJigsaw = vapourJigsaws->getPuzzle(vapourPuzzle);
        AFK_Jigsaw *edgeJigsaw = edgeJigsaws->getPuzzle(edgePuzzle);
        if (vapourJigsaw && edgeJigsaw)
        {
            edgeComputeQueues.at(i)->computeFinish(
                vapourJigsaw,
                edgeJigsaw);
        }
    }
#endif
}

void AFK_World::display(
    unsigned int threadId,
    const Mat4<float>& projection,
    const Vec2<float>& windowSize,
    const AFK_Light &globalLight)
{
    /* Render the landscape */
    glUseProgram(landscape_shaderProgram->program);
    landscape_shaderLight->setupLight(globalLight);
    glUniformMatrix4fv(landscape_clipTransformLocation, 1, GL_TRUE, &projection.m[0][0]);
    glUniform3fv(landscape_skyColourLocation, 1, &afk_core.skyColour.v[0]);
    glUniform1f(landscape_farClipDistanceLocation, afk_core.settings.zFar);
    AFK_GLCHK("landscape uniforms")

    glBindVertexArray(landscapeTileArray);
    AFK_GLCHK("landscape bindVertexArray")


    /* Now that I've set that up, make the texture that describes
     * where the tiles are in space ...
     */
    std::vector<std::shared_ptr<AFK_LandscapeDisplayQueue> > landscapeDrawQueues;
    landscapeDisplayFair.getDrawQueues(landscapeDrawQueues);

    /* Those queues are in puzzle order. */
    for (unsigned int puzzle = 0; puzzle < landscapeDrawQueues.size(); ++puzzle)
    {
        landscapeDrawQueues.at(puzzle)->draw(
            threadId,
            landscape_shaderProgram,
            landscapeJigsaws->getPuzzle(puzzle),
            landscapeCache,
            landscapeTerrainBase,
            lSizes);
    }

    glBindVertexArray(0);

#if AFK_RENDER_ENTITIES
    /* Render the shapes */
    glUseProgram(entity_shaderProgram->program);
    entity_shaderLight->setupLight(globalLight);
    glUniformMatrix4fv(entity_projectionTransformLocation, 1, GL_TRUE, &projection.m[0][0]);
    glUniform2fv(entity_windowSizeLocation, 1, &windowSize.v[0]);
    glUniform3fv(entity_skyColourLocation, 1, &afk_core.skyColour.v[0]);
    glUniform1f(entity_farClipDistanceLocation, afk_core.settings.zFar);
    AFK_GLCHK("shape uniforms")

    glBindVertexArray(edgeShapeBaseArray);
    AFK_GLCHK("edge shape bindVertexArray")

    std::vector<std::shared_ptr<AFK_EntityDisplayQueue> > entityDrawQueues;
    entityDisplayFair.getDrawQueues(entityDrawQueues);

    for (unsigned int i = 0; i < entityDrawQueues.size(); ++i)
    {
        unsigned int vapourPuzzle, edgePuzzle;
        entityFair2DIndex.get2D(i, vapourPuzzle, edgePuzzle);
        AFK_Jigsaw *vapourJigsaw = vapourJigsaws->getPuzzle(vapourPuzzle);
        AFK_Jigsaw *edgeJigsaw = edgeJigsaws->getPuzzle(edgePuzzle);
        if (vapourJigsaw && edgeJigsaw)
        {
           entityDrawQueues.at(i)->draw(
               entity_shaderProgram,
               vapourJigsaw,
               edgeJigsaw,
               edgeShapeBase,
               sSizes);
        }
    }

    glBindVertexArray(0);
#endif
}

/* Worker for the below. */
#if PRINT_CHECKPOINTS
static float toRatePerSecond(uint64_t quantity, const afk_duration_mfl& interval)
{
    return (float)quantity * 1000.0f / interval.count();
}

#define PRINT_RATE_AND_RESET(s, v) std::cout << s << toRatePerSecond((v).exchange(0), timeSinceLastCheckpoint) << "/second" << std::endl;
#endif

void AFK_World::checkpoint(afk_duration_mfl& timeSinceLastCheckpoint)
{
#if PRINT_CHECKPOINTS
    PRINT_RATE_AND_RESET("Cells found invisible:        ", cellsInvisible)
    PRINT_RATE_AND_RESET("Cells resumed:                ", cellsResumed)
    PRINT_RATE_AND_RESET("Tiles queued:                 ", tilesQueued)
    PRINT_RATE_AND_RESET("Tiles resumed:                ", tilesResumed)
    PRINT_RATE_AND_RESET("Tiles computed:               ", tilesComputed)
    PRINT_RATE_AND_RESET("Tiles recomputed after sweep: ", tilesRecomputedAfterSweep)
    PRINT_RATE_AND_RESET("Entities queued:              ", entitiesQueued)
    PRINT_RATE_AND_RESET("Entities moved:               ", entitiesMoved)
    PRINT_RATE_AND_RESET("Shape cells found invisible:  ", shapeCellsInvisible)
    PRINT_RATE_AND_RESET("Shape cells reduced out:      ", shapeCellsReducedOut)
    PRINT_RATE_AND_RESET("Shape cells resumed:          ", shapeCellsResumed)
    PRINT_RATE_AND_RESET("Shape vapours computed:       ", shapeVapoursComputed)
    PRINT_RATE_AND_RESET("Shape edges computed:         ", shapeEdgesComputed)
    PRINT_RATE_AND_RESET("Separate vapours computed:    ", separateVapoursComputed)
    PRINT_RATE_AND_RESET("Dependencies followed:        ", dependenciesFollowed)
    std::cout <<         "Cumulative thread escapes:    " << threadEscapes.load() << std::endl;
#endif
}

void AFK_World::printCacheStats(std::ostream& ss, const std::string& prefix)
{
#if PRINT_CACHE_STATS
    worldCache->printStats(ss, "World cache");
    landscapeCache->printStats(ss, "Landscape cache");
    shape.printCacheStats(ss, prefix);
#endif
}

void AFK_World::printJigsawStats(std::ostream& ss, const std::string& prefix)
{
#if PRINT_JIGSAW_STATS
    landscapeJigsaws->printStats(ss, "Landscape");
    vapourJigsaws->printStats(ss, "Vapour");
    edgeJigsaws->printStats(ss, "Edge");
#endif
}

