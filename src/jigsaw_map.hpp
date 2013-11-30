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

#ifndef _AFK_JIGSAW_MAP_H_
#define _AFK_JIGSAW_MAP_H_

#include "afk.hpp"

#include <deque>
#include <mutex>
#include <sstream>
#include <vector>

#include "data/frame.hpp"
#include "def.hpp"
#include "jigsaw_cuboid.hpp"

/* A Place is a single unit of copying to the GPU, from which Pieces
 * are assigned.
 * It's not locked -- that's done at the Map level.
 */
class AFK_JigsawPlace
{
protected:
    AFK_JigsawCuboid cuboid;

    /* Valid-from. */
    AFK_Frame timestamp;

    /* We walk this from (0, 0, 0) to size to use up pieces. */
    Vec3<int> next;

public:
    AFK_JigsawPlace();
    AFK_JigsawPlace(const AFK_JigsawCuboid& _cuboid);

    AFK_JigsawCuboid getCuboid(void) const { return cuboid; }
    AFK_Frame getTimestamp(void) const { return timestamp; }

    /* This function gets the next piece from this Place and returns
     * true if there was one left.  If it's full, it returns false.
     */
    bool grab(Vec3<int>& o_piece, AFK_Frame *o_timestamp);

    /* This clears the place and applies a new timestamp. */
    void clear(const AFK_Frame& newTimestamp);

    friend std::ostream& operator<<(std::ostream& os, const AFK_JigsawPlace& place);
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPlace& place);

/* A JigsawMap describes how the jigsaw is split up into Places, each
 * being a unit of copying to the GPU containing many Pieces.
 */
class AFK_JigsawMap
{
protected:
    /* Access to the Map is internally synchronized. */
    mutable std::mutex mut;

    /* The map dimensions, in place units.
     * These go (rows, columns, slices), because I'm confusing.
     */
    Vec3<int> placeDim;
    Vec3<int> placeSize;
    Vec3<int> endPlaceSize;

    /* The minimum and maximum numbers of cleared pieces I want. */
    int minCleared;
    int maxCleared;

    /* All the places in the map, in a 3D array (slices, columns, rows).
     * These will all be the same size, apart from possibly the final
     * ones along each slice/column/row.
     */
    AFK_JigsawPlace ***places;

    /* The queue of Places that have been cleared for use on the
     * next update.
     */
    std::deque<AFK_JigsawPlace*> cleared;

    /* The queue of Places that are currently being updated and
     * will need to be copied to the GPU on the next frame.  The
     * currently updating Place is at the back of this queue,
     * and when it fills a new one is added from the front of the
     * `cleared' queue.
     */
    std::deque<AFK_JigsawPlace*> updating;

    /* The queue of Places that are being pushed to the GPU. */
    std::deque<AFK_JigsawPlace*> pushed;

    /* The rest. */
    std::deque<AFK_JigsawPlace*> idle;

    /* Stats. */
    mutable int64_t piecesGrabbed;
    mutable int64_t placesUpdated;
    mutable int64_t placesCleared;
    mutable int64_t cuboidsSquashed;
    mutable int64_t cuboidsPushed;

    /* Helper functions... */

    /* For initialisation, this works out a "good" place dimension
     * given a jigsaw dimension, also outputting the regular and
     * end place sizes.
     */
    void getPlaceDimension(int jigsawDim, int& o_placeDim, int& o_placeSize, int& o_endPlaceSize) const;

    /* Another initialisation useful. */
    int getPlaceSizeToUse(int dim, int idx) const;

    /* This one gets a pointer to the place a piece is in. */
    AFK_JigsawPlace *findPiece(const Vec3<int>& piece) const;

public:
    AFK_JigsawMap(const Vec3<int>& _jigsawSize);
    virtual ~AFK_JigsawMap();

    /* Gets the timestamp for an existing piece. */
    AFK_Frame getTimestamp(const Vec3<int>& piece) const;

    /* Grabs a new piece, returning it and the timestamp.
     * Returns true if successful, false if this map ran out of
     * cleared pieces (you should try the next jigsaw)
     */
    bool grab(Vec3<int>& o_piece, AFK_Frame *o_timestamp);

    /* Copies out the list of draw places so that the jigsaw image
     * can be updated.
     */
    void copyDrawPlaces(std::vector<AFK_JigsawCuboid>& o_drawList) const;

    /* Rolls the queues about and prepares for the next frame. */
    void flip(const AFK_Frame& frame);

    /* Prints and resets the stats. */
    void printStats(std::ostream& os, const std::string& prefix);
};

#endif /* _AFK_JIGSAW_MAP_H_ */

