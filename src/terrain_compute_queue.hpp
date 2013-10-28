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

#ifndef _AFK_TERRAIN_COMPUTE_QUEUE_H_
#define _AFK_TERRAIN_COMPUTE_QUEUE_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "computer.hpp"
#include "def.hpp"
#include "landscape_sizes.hpp"
#include "terrain.hpp"
#include "yreduce.hpp"

class AFK_LandscapeTile;

/* This module make something like a render list, but rather more
 * complicated, for the purpose of queueing up the terrain
 * computation tasks into a form suitable for shoveling straight
 * into the CL without further re-arranging in the compute
 * thread.
 * Sadly, it does have to be internally synchronized.
 * It's specialized right now, but I might want to consider
 * deriving something when I come to queueing up other
 * CL tasks.
 */

class AFK_TerrainComputeUnit
{
public:
    int tileOffset;
    int tileCount;
    Vec2<int> piece;

    AFK_TerrainComputeUnit(
        int _tileOffset,
        int _tileCount,
        const Vec2<int>& _piece);

    friend std::ostream& operator<<(std::ostream& os, const AFK_TerrainComputeUnit& unit);
};

std::ostream& operator<<(std::ostream& os, const AFK_TerrainComputeUnit& unit);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_TerrainComputeUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_TerrainComputeUnit>::value));

class AFK_TerrainComputeQueue: protected AFK_TerrainList
{
protected:
    /* In a TerrainComputeQueue, the TerrainList members are
     * actually a concatenated sequence of terrain features and
     * tiles.  The following vector describes each unit of
     * computation, allowing it to seek correctly within the
     * other two lists.
     */
    std::vector<AFK_TerrainComputeUnit> units;

    /* Used for internal synchronization, because the various
     * cell evaluator threads will be hitting a single one
     * of these.
     */
    boost::mutex mut;

    /* Compute stuff. */
    cl_kernel terrainKernel, surfaceKernel;
    AFK_YReduce *yReduce;
    AFK_ComputeDependency *postTerrainDep;

    /* In this vector, we store the in-order list of pointers
     * to the source LandscapeTiles, so that the yreduce
     * module can feed the computed y bounds back in easily.
     */
    std::vector<AFK_LandscapeTile*> landscapeTiles;

public:
    AFK_TerrainComputeQueue();
    virtual ~AFK_TerrainComputeQueue();

    /* Pushes a terrain list into the queue, making a Unit for it.
     * The Unit goes in too, but we return it as well so you can
     * instantly debug.
     */
    AFK_TerrainComputeUnit extend(const AFK_TerrainList& list, const Vec2<int>& piece, AFK_LandscapeTile *landscapeTile, const AFK_LandscapeSizes& lSizes);

    /* This prints lots of debug info about the given terrain unit. */
    std::string debugTerrain(const AFK_TerrainComputeUnit& unit, const AFK_LandscapeSizes& lSizes) const;

    /* Computes the terrain.
     */
    void computeStart(AFK_Computer *computer, AFK_Jigsaw *jigsaw, const AFK_LandscapeSizes& lSizes, const Vec3<float>& baseColour);
    void computeFinish(AFK_Jigsaw *jigsaw);

    bool empty(void);

    /* Clears everything out ready for the next evaluation phase.
     * Make sure the CL tasks are finished first! :P
     */
    void clear(void);
};

#endif /* _AFK_TERRAIN_COMPUTE_QUEUE_H_ */

