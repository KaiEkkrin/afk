/* AFK
 * Copyright (C) 2013, Alex Holloway.
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

#ifndef _AFK_WORLD_H_
#define _AFK_WORLD_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/atomic.hpp>

#include "3d_edge_compute_queue.hpp"
#include "3d_edge_shape_base.hpp"
#include "3d_vapour_compute_queue.hpp"
#include "async/async.hpp"
#include "async/work_queue.hpp"
#include "camera.hpp"
#include "cell.hpp"
#include "clock.hpp"
#include "computer.hpp"
#include "config.hpp"
#include "core.hpp"
#include "data/evictable_cache.hpp"
#include "data/fair.hpp"
#include "data/moving_average.hpp"
#include "data/stage_timer.hpp"
#include "def.hpp"
#include "entity.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw_collection.hpp"
#include "landscape_display_queue.hpp"
#include "landscape_tile.hpp"
#include "rng/rng.hpp"
#include "shader.hpp"
#include "shape.hpp"
#include "terrain_base_tile.hpp"
#include "terrain_compute_queue.hpp"
#include "tile.hpp"
#include "work.hpp"

/* The world of AFK. */

class AFK_LandscapeDisplayQueue;
class AFK_LandscapeTile;
class AFK_WorldCell;


/* This is the cell generating worker function */
bool afk_generateWorldCells(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    const struct AFK_WorldWorkThreadLocal& threadLocal,
    AFK_WorldWorkQueue& queue);

/* This is the cell-generation-finished check */
bool afk_worldGenerationFinished(void);
extern AFK_AsyncTaskFinishedFunc afk_worldGenerationFinishedFunc;

/* This is the world.  AFK_Core will have one of these.
 */
class AFK_World
{
protected:
    /* These derived results are things I want to be carefully
     * managed now that I'm trying to adjust the LOD on the fly.
     */

    /* The maximum number of pixels each detail point on the
     * world should occupy on screen.  Determines when
     * to split a cell into smaller ones.
     * (I'm expecting to have to number-crunch a lot with this
     * value to get the right number to pass to the points
     * generator, but hopefully I'll only have to do that
     * once per world cell)
     */
    const float startingDetailPitch;
    const float maxDetailPitch;

    /* This is my means of tracking whether the current world (and shape)
     * enumeration is finished or not.
     */
    boost::atomic_uint_fast64_t volumeLeftToEnumerate;

    /* Gather statistics.  (Useful.)
     */
    boost::atomic_uint_fast64_t cellsInvisible;
    boost::atomic_uint_fast64_t cellsResumed;
    boost::atomic_uint_fast64_t tilesQueued;
    boost::atomic_uint_fast64_t tilesResumed;
    boost::atomic_uint_fast64_t tilesComputed;
    boost::atomic_uint_fast64_t tilesRecomputedAfterSweep;
    boost::atomic_uint_fast64_t entitiesQueued;
    boost::atomic_uint_fast64_t entitiesMoved;

    /* These ones are updated by the shape worker. */
    boost::atomic_uint_fast64_t shapeCellsInvisible;
    boost::atomic_uint_fast64_t shapeCellsResumed;
    boost::atomic_uint_fast64_t shapeVapoursComputed;
    boost::atomic_uint_fast64_t shapeEdgesComputed;

    /* ... and this by that little vapour descriptor worker */
    boost::atomic_uint_fast64_t separateVapoursComputed;

    /* Concurrency stats */
    boost::atomic_uint_fast64_t dependenciesFollowed;
    boost::atomic_uint_fast64_t threadEscapes;

    /* Landscape shader details. */
    AFK_ShaderProgram *landscape_shaderProgram;
    AFK_ShaderLight *landscape_shaderLight;
    GLuint landscape_clipTransformLocation;
    GLuint landscape_skyColourLocation;
    GLuint landscape_farClipDistanceLocation;
    Vec3<float> landscape_baseColour;

    /* Entity shader details. */
    AFK_ShaderProgram *entity_shaderProgram;
    AFK_ShaderLight *entity_shaderLight;
    GLuint entity_projectionTransformLocation;
    GLuint entity_windowSizeLocation;
    GLuint entity_skyColourLocation;
    GLuint entity_farClipDistanceLocation;

    /* The cache of world cells we're tracking.
     */
    AFK_WORLD_CACHE *worldCache;

    /* These jigsaws form the computed landscape artwork. */
    AFK_JigsawCollection *landscapeJigsaws;

    /* The cache of landscape cells we're tracking.
     * TODO: I think at some point, all of this should
     * move to a separate `landscape' module (one per
     * world) that handles the landscape, otherwise
     * I'm going to get a bit of overload?
     */
    AFK_LANDSCAPE_CACHE *landscapeCache;

    /* The terrain computation fair.  Yeah, yeah. */
    AFK_Fair<AFK_TerrainComputeQueue> landscapeComputeFair;

    /* The landscape render fair.  In jigsaw order, just
     * like the terrain compute fair, above.
     * These are transient objects -- delete them after
     * rendering.
     */
    AFK_Fair<AFK_LandscapeDisplayQueue> landscapeDisplayFair;

    /* The basic landscape tile geometry. */
    GLuint landscapeTileArray;
    AFK_TerrainBaseTile *landscapeTerrainBase;

    /* This describes the entity shapes.
     * TODO -- as written in `shape' itself -- move the
     * below into its purview, cleaning the World up and
     * making everything make a bit more sense.
     */
    AFK_Shape shape;

    /* The entity render fair -- a queue of Entities to
     * display onscreen.
     */
    AFK_Fair<AFK_EntityDisplayQueue> entityDisplayFair;

    /* These jigsaws form the computed shape artwork. */
    AFK_JigsawCollection *vapourJigsaws;
    AFK_JigsawCollection *edgeJigsaws;

    /* For when I'm making new shapes, the vapour and edge
     * computation fairs.
     */
    AFK_Fair<AFK_3DVapourComputeQueue> vapourComputeFair;
    AFK_Fair<AFK_3DEdgeComputeQueue> edgeComputeFair;

    /* Edge computation and entity display are across
     * (vapour x edge).
     */
    AFK_Fair2DIndex entityFair2DIndex;
#define AFK_MAX_VAPOUR 4

    /* The basic shape geometry. */
    GLuint edgeShapeBaseArray;
    AFK_3DEdgeShapeBase *edgeShapeBase;

    /* The cell generating gang */
    AFK_AsyncGang<union AFK_WorldWorkParam, bool, struct AFK_WorldWorkThreadLocal, afk_worldGenerationFinishedFunc> *genGang;

    /* Cell generation worker delegates. */

    /* Makes sure a landscape tile has a terrain descriptor,
     * and checks if its geometry needs generating.
     * Returns true if this thread is to generate the tile's
     * geometry, else false.
     */
    bool checkClaimedLandscapeTile(
        const AFK_Tile& tile,
        AFK_LandscapeTile& landscapeTile,
        bool display);

    /* Generates a landscape tile's geometry. */
    void generateLandscapeArtwork(
        const AFK_Tile& tile,
        AFK_LandscapeTile& landscapeTile,
        unsigned int threadId);

    /* Pushes a landscape tile into the display queue. */
    void displayLandscapeTile(
        const AFK_Cell& cell,
        const AFK_Tile& tile,
        const AFK_LandscapeTile& landscapeTile,
        unsigned int threadId);

    /* Makes one starting entity for a world cell, including generating
     * the shape as required.
     */
    void generateStartingEntity(
        unsigned int shapeKey,
        AFK_WorldCell& worldCell,
        unsigned int threadId,
        AFK_RNG& rng);

    /* Generates this world cell, as necessary. */
    bool generateClaimedWorldCell(
        AFK_CLAIM_OF(WorldCell)& claim,
        unsigned int threadId,
        const struct AFK_WorldWorkParam::World& param,
        const struct AFK_WorldWorkThreadLocal& threadLocal,
        AFK_WorldWorkQueue& queue);

public:
    /* Overall world parameters. */

    /* The distance to the furthest world cell to consider.
     * (You may want to use zFar, or maybe not.)
     */
    const float maxDistance;

    /* The size of the biggest cell (across one dimension) shall
     * be equal to maxDistance; this is also the furthest-
     * away cell we'll consider drawing.  From there, cells get
     * subdivided by splitting each edge this number of times --
     * thus each cell gets split into this number cubed more
     * detailed cells
     */
    const unsigned int subdivisionFactor;

    /* The size of the smallest cell. */
    const float minCellSize;

    /* These parameters define the sizes of the landscape tiles. */
    const AFK_LandscapeSizes lSizes;

    /* These parameters define the sizes of the shapes in the world. */
    const AFK_ShapeSizes sSizes;

    /* These parameters define how to initially populate a world
     * cell with entities.
     */
    const unsigned int entitySparseness;


    AFK_World(
        const AFK_Config *config,
        AFK_Computer *computer,
        AFK_ThreadAllocation& threadAlloc,
        float _maxDistance,
        unsigned int worldCacheSize, /* in bytes */
        unsigned int tileCacheSize, /* also in bytes */
        unsigned int shapeCacheSize, /* likewise */
        cl_context ctxt,
        AFK_RNG *setupRng);
    virtual ~AFK_World();

    /* Helper for the below -- requests a particular cell
     * at an offset (in cell scale multiples) from the
     * supplied one.
     */
    void enqueueSubcells(
        const AFK_Cell& cell,
        const Vec3<int64_t>& modifier);

    /* Call when we're about to start a new frame. */
    void flipRenderQueues(const AFK_Frame& newFrame);

    /* TODO: I'm moving the detail pitch calculation out to the
     * DetailAdjuster object ...
     */
#if 0
    /* For changing the level of detail.  Values >1 decrease
     * it.  Values <1 increase it.
     * I'm not entirely sure what the correlation between the
     * detail pitch and the amount of processing power required
     * is, but I expect it's in the neighbourhood of
     * (1 / detailPitch ** 2) ...
     */
    void alterDetail(float adjustment);

    float getLandscapeDetailPitch(void) const;
#endif
    float getEntityDetailPitch(float landscapeDetailPitch) const;

    /* This function drives the cell generating worker to
     * update the world cache and enqueue visible
     * cells.  Returns a future that becomes available
     * when we're done.
     */
    std::future<bool> updateWorld(
        const AFK_Camera& camera, const AFK_Object& protagonistObj, float detailPitch);

    /* CL-tasks-at-start-of-frame function. */
    void doComputeTasks(unsigned int threadId);

    /* Draws the world in the current OpenGL context.
     * (There's no AFK_DisplayedObject for the world.)
     */
    void display(
        unsigned int threadId,
        const Mat4<float>& projection,
        const Vec2<float>& windowSize,
        const AFK_Light &globalLight);

    /* Takes a world checkpoint. */
    void checkpoint(afk_duration_mfl& timeSinceLastCheckpoint);

    void printCacheStats(std::ostream& ss, const std::string& prefix);
    void printJigsawStats(std::ostream& ss, const std::string& prefix);

    friend class AFK_Shape;

    friend bool afk_generateEntity(
        unsigned int threadId,
        const union AFK_WorldWorkParam& param,
        const struct AFK_WorldWorkThreadLocal& threadLocal,
        AFK_WorldWorkQueue& queue);

    friend bool afk_generateShapeCells(
        unsigned int threadId,
        const union AFK_WorldWorkParam& param,
        const struct AFK_WorldWorkThreadLocal& threadLocal,
        AFK_WorldWorkQueue& queue);

    friend bool afk_generateWorldCells(
        unsigned int threadId,
        const union AFK_WorldWorkParam& param,
        const struct AFK_WorldWorkThreadLocal& threadLocal,
        AFK_WorldWorkQueue& queue);

    friend bool afk_worldGenerationFinished(void);
};

#endif /* _AFK_WORLD_H_ */

