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
#include <cmath>
#include <iostream>

#include "debug.hpp"
#include "jigsaw_map.hpp"


/* AFK_JigsawPlace implementation */

AFK_JigsawPlace::AFK_JigsawPlace():
    cuboid(), timestamp()
{
    next = afk_vec3<int>(0, 0, 0);
}

AFK_JigsawPlace::AFK_JigsawPlace(const AFK_JigsawCuboid& _cuboid):
    cuboid(_cuboid), timestamp()
{
    next = afk_vec3<int>(0, 0, 0);
}

bool AFK_JigsawPlace::grab(Vec3<int>& o_piece, AFK_Frame *o_timestamp)
{
    assert(cuboid);

    if (next.v[2] < cuboid.size.v[2])
    {
        o_piece = cuboid.location + next;
        ++(next.v[0]);
        if (next.v[0] == cuboid.size.v[0])
        {
            next.v[0] = 0;
            ++(next.v[1]);
            if (next.v[1] == cuboid.size.v[1])
            {
                next.v[1] = 0;
                ++(next.v[2]);
            }
        }
        *o_timestamp = timestamp;
        return true;
    }
    else return false;
}

void AFK_JigsawPlace::clear(const AFK_Frame& newTimestamp)
{
    next = afk_vec3<int>(0, 0, 0);
    timestamp = newTimestamp;
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPlace& place)
{
    os << "Place(" << place.cuboid << ", timestamp " << place.timestamp << ", next " << place.next << ")";
    return os;
}


/* AFK_JigsawMap implementation */

void AFK_JigsawMap::getPlaceDimension(int jigsawDim, int& o_placeDim, int& o_placeSize, int& o_endPlaceSize) const
{
    /* This is a little arbitrary right now, let's try it out
     * and see how well it works.
     */
    int placeDim = static_cast<int>(std::floor(sqrt(jigsawDim)));
    if (placeDim == 0) placeDim = 1;

    int placeSize = jigsawDim / placeDim;
    if (placeSize == 0)
    {
        /* Oh dear.  Output just one place. */
        o_placeDim = 1;
        o_placeSize = 0;
        o_endPlaceSize = jigsawDim;
    }
    else
    {
        /* TODO After this is working, finesse for no tiny end
         * places?
         */
        int endPlaceSize = jigsawDim % placeSize;
        if (endPlaceSize == 0)
        {
            o_placeDim = placeDim;
            o_placeSize = placeSize;
            o_endPlaceSize = placeSize;
        }
        else
        {
            o_placeDim = placeDim + 1;
            o_placeSize = placeSize;
            o_endPlaceSize = endPlaceSize;
        }
    }

    /* This is what I intended */
    assert(((o_placeDim - 1) * o_placeSize + o_endPlaceSize) == jigsawDim);
}

int AFK_JigsawMap::getPlaceSizeToUse(int dim, int idx) const
{
    if (dim == (placeDim.v[idx] - 1)) return endPlaceSize.v[idx];
    else return placeSize.v[idx];
}

AFK_JigsawPlace *AFK_JigsawMap::findPiece(const Vec3<int>& piece) const
{
    Vec3<int> place = afk_vec3<int>(
        piece.v[0] / placeSize.v[0],
        piece.v[1] / placeSize.v[1],
        piece.v[2] / placeSize.v[2]);
    
    assert(place.v[0] < placeDim.v[0] &&
        place.v[1] < placeDim.v[1] &&
        place.v[2] < placeDim.v[2]);

    return &places[place.v[2]][place.v[1]][place.v[0]];
}

AFK_JigsawMap::AFK_JigsawMap(const Vec3<int>& _jigsawSize):
    piecesGrabbed(0), placesUpdated(0), placesCleared(0),
    cuboidsSquashed(0), cuboidsPushed(0)
{
    std::unique_lock<std::mutex> lock(mut);

    /* Work out how big the map shall be... */
    for (int i = 0; i < 3; ++i)
        getPlaceDimension(_jigsawSize.v[i], placeDim.v[i], placeSize.v[i], endPlaceSize.v[i]);

    AFK_DEBUG_PRINTL("AFK_JigsawMap: Mapping jigsaw size " << _jigsawSize << " to " << placeDim << " places")

    int placeCount = placeDim.v[0] * placeDim.v[1] * placeDim.v[2];
    minCleared = placeCount / 8;
    maxCleared = placeCount / 4;
    if (minCleared == 0) minCleared = 1;
    if (maxCleared == 0) maxCleared = 1;

    /* Now I can make and initialise it! */
    places = new AFK_JigsawPlace**[placeDim.v[2]];
    int sLoc = 0;
    for (int s = 0; s < placeDim.v[2]; ++s)
    {
        places[s] = new AFK_JigsawPlace*[placeDim.v[1]];

        int cLoc = 0;
        for (int c = 0; c < placeDim.v[1]; ++c)
        {
            places[s][c] = new AFK_JigsawPlace[placeDim.v[0]];

            int rLoc = 0;
            for (int r = 0; r < placeDim.v[0]; ++r)
            {
                Vec3<int> location = afk_vec3<int>(rLoc, cLoc, sLoc);
                Vec3<int> size = afk_vec3<int>(
                    getPlaceSizeToUse(r, 0),
                    getPlaceSizeToUse(c, 1),
                    getPlaceSizeToUse(s, 2));

                //afk_out << "AFK_JigsawMap: Making place at location " << location << ", size " << size << std::endl;
                AFK_JigsawCuboid cuboid(location, size);
                places[s][c][r] = AFK_JigsawPlace(cuboid);

                /* To begin with, all places are in the cleared state
                 * (yeah, despite maxCleared :) )
                 */
                cleared.push_back(&places[s][c][r]);

                rLoc += getPlaceSizeToUse(r, 0);
                assert(rLoc <= _jigsawSize.v[0]);
            }

            cLoc += getPlaceSizeToUse(c, 1);
            assert(cLoc <= _jigsawSize.v[1]);
        }

        sLoc += getPlaceSizeToUse(s, 2);
        assert(sLoc <= _jigsawSize.v[2]);
    }
}

AFK_JigsawMap::~AFK_JigsawMap()
{
    std::unique_lock<std::mutex> lock(mut);

    for (int s = 0; s < placeDim.v[2]; ++s)
    {
        for (int c = 0; c < placeDim.v[1]; ++c)
        {
            delete[] places[s][c];
        }

        delete[] places[s];
    }

    delete[] places;
}

AFK_Frame AFK_JigsawMap::getTimestamp(const Vec3<int>& piece) const
{
    std::unique_lock<std::mutex> lock(mut);
    return findPiece(piece)->getTimestamp();
}

bool AFK_JigsawMap::grab(Vec3<int>& o_piece, AFK_Frame *o_timestamp)
{
    std::unique_lock<std::mutex> lock(mut);
    if (!updating.empty() && updating.back()->grab(o_piece, o_timestamp))
    {
        /* We got a piece from the currently updating place! */
        ++piecesGrabbed;
        return true;
    }
    else
    {
        /* That one must be done; take the next place from the cleared
         * queue and push it to the updating queue
         */
        if (!cleared.empty())
        {
            AFK_JigsawPlace *newPlace = cleared.front();

            /* Make sure I can use it -- a failure here would be
             * insane
             */
            bool success = newPlace->grab(o_piece, o_timestamp);
            assert(success);

            /* Make it the currently updating place */
            //afk_out << "Using new place: " << *newPlace << std::endl;
            updating.push_back(newPlace);
            cleared.pop_front();

            ++piecesGrabbed;
            ++placesUpdated;
            return success;
        }
        else
        {
            /* This map ran out of space, you'll need to try elsewhere. */
            return false;
        }
    }
}

void AFK_JigsawMap::copyDrawPlaces(std::vector<AFK_JigsawCuboid>& o_drawList) const
{
    std::unique_lock<std::mutex> lock(mut);

    /* I'm going to try to crunch together places that are in the
     * same row so that they can be copied together.
     * Theoretically I could do this for the other dimensions too,
     * but that's messy and it almost certainly wouldn't be worth
     * it.
     */
    Vec3<int> thisLocation;
    Vec3<int> thisSize;
    bool keptOne = false;

    for (auto place : pushed)
    {
        AFK_JigsawCuboid placeCuboid = place->getCuboid();

        /* If it follows straight along from the previous one,
         * squash them together
         */
        if (keptOne &&
            placeCuboid.location.v[0] == (thisLocation.v[0] + thisSize.v[0]) &&
            placeCuboid.location.v[1] == thisLocation.v[1] &&
            placeCuboid.location.v[2] == thisLocation.v[2] &&
            placeCuboid.size.v[1] == thisSize.v[1] &&
            placeCuboid.size.v[2] == thisSize.v[2])
        {
            thisSize.v[0] += placeCuboid.size.v[0];
            ++cuboidsSquashed;
        }
        else
        {
            /* Push back the last cuboid I was tracking and track
             * this one instead.
             */
            if (keptOne)
            {
                o_drawList.push_back(AFK_JigsawCuboid(thisLocation, thisSize));
                ++cuboidsPushed;
            }

            thisLocation = placeCuboid.location;
            thisSize = placeCuboid.size;
            keptOne = true;
        }
    }

    /* If I've got a leftover one, push it in. */
    if (keptOne)
    {
        o_drawList.push_back(AFK_JigsawCuboid(thisLocation, thisSize));
        ++cuboidsPushed;
    }
}

void AFK_JigsawMap::flip(const AFK_Frame& frame)
{
    std::unique_lock<std::mutex> lock(mut);

    /* Updating Places become Pushed (they now need to go to the GPU).
     * Pushed Places become Idle (they now contain valid data that's
     * already on the GPU).
     */
    idle.insert(idle.end(), pushed.begin(), pushed.end());
    pushed.clear();
    pushed.insert(pushed.end(), updating.begin(), updating.end());
    updating.clear();

    /* If the cleared list has fallen below minimum, clear
     * up to maximum.
     */
    size_t clearedSize = cleared.size();
    if (clearedSize < minCleared)
    {
        for (size_t i = clearedSize; i < maxCleared && !idle.empty(); ++i)
        {
            AFK_JigsawPlace *clearedPlace = idle.front();
            idle.pop_front();
            clearedPlace->clear(frame);
            cleared.push_back(clearedPlace);

            ++placesCleared;
        }
    }
}

void AFK_JigsawMap::printStats(std::ostream& os, const std::string& prefix)
{
    std::unique_lock<std::mutex> lock(mut);
    os << prefix << ": Pieces grabbed:  " << piecesGrabbed      << std::endl;
    os << prefix << ": Places updated:  " << placesUpdated      << std::endl;
    os << prefix << ": Places cleared:  " << placesCleared      << std::endl;
    os << prefix << ": Cuboids squashed:" << cuboidsSquashed    << std::endl;
    os << prefix << ": Cuboids pushed:  " << cuboidsPushed      << std::endl;
    piecesGrabbed = 0;
    placesUpdated = 0;
    placesCleared = 0;
    cuboidsSquashed = 0;
    cuboidsPushed = 0;
}

