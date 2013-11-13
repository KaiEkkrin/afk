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

#include "afk.hpp"

#include <cassert>
#include <climits>
#include <cmath>
#include <cstring>
#include <iostream>

#include "computer.hpp"
#include "core.hpp"
#include "debug.hpp"
#include "display.hpp"
#include "exception.hpp"
#include "jigsaw.hpp"
#include "rng/hash.hpp"


#define GRAB_DEBUG 0
#define CUBOID_DEBUG 0


/* AFK_JigsawPiece implementation */

AFK_JigsawPiece::AFK_JigsawPiece():
    u(INT_MIN), v(INT_MIN), w(INT_MIN), puzzle(INT_MIN)
{
}

AFK_JigsawPiece::AFK_JigsawPiece(int _u, int _v, int _w, int _puzzle):
    u(_u), v(_v), w(_w), puzzle(_puzzle)
{
}

AFK_JigsawPiece::AFK_JigsawPiece(const Vec3<int>& _piece, int _puzzle):
    u(_piece.v[0]), v(_piece.v[1]), w(_piece.v[2]), puzzle(_puzzle)
{
}

bool AFK_JigsawPiece::operator==(const AFK_JigsawPiece& other) const
{
    return (u == other.u && v == other.v && w == other.w && puzzle == other.puzzle);
}

bool AFK_JigsawPiece::operator!=(const AFK_JigsawPiece& other) const
{
    return (u != other.u || v != other.v || w != other.w || puzzle != other.puzzle);
}

size_t hash_value(const AFK_JigsawPiece& jigsawPiece)
{
    /* Inspired by hash_value(const AFK_Cell&), just in case I
     * need to use this in anger
     */
    size_t hash = 0;
    hash = afk_hash_swizzle(hash, jigsawPiece.u, afk_factor1);
    hash = afk_hash_swizzle(hash, jigsawPiece.v, afk_factor2);
    hash = afk_hash_swizzle(hash, jigsawPiece.w, afk_factor3);
    hash = afk_hash_swizzle(hash, jigsawPiece.puzzle, afk_factor4);
    return hash;
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece)
{
    return os << "JigsawPiece(u=" << std::dec << piece.u << ", v=" << piece.v << ", w=" << piece.w << ", puzzle=" << piece.puzzle << ")";
}


/* AFK_Jigsaw implementation */

enum AFK_JigsawPieceGrabStatus AFK_Jigsaw::grabPieceFromCuboid(
    AFK_JigsawCuboid& cuboid,
    unsigned int threadId,
    Vec3<int>& o_uvw,
    AFK_Frame *o_timestamp)
{
#if GRAB_DEBUG
    AFK_DEBUG_PRINTL("grabPieceFromCuboid: with cuboid " << cuboid << "( " << cuboid << " and threadId " << threadId)
#endif

    assert(threadId < concurrency);

    if ((int)threadId >= cuboid.rows)
    {
        /* There aren't enough rows in this cuboid, try the
         * next one.
         */
#if GRAB_DEBUG
        AFK_DEBUG_PRINTL("  out of rows")
#endif
        return AFK_JIGSAW_CUBOID_OUT_OF_SPACE;
    }

    /* If I get here, `threadId' is a valid row within the current
     * cuboid.
     * Let's see if I've got enough columns left...
     */
    int row = (int)threadId + cuboid.r;
    int slice = cuboid.s;
    if (rowUsage[row][slice] < jigsawSize.v[1])
    {
        /* We have!  Grab one. */
        o_uvw = afk_vec3<int>(row, rowUsage[row][slice]++, slice);
        *o_timestamp = rowTimestamp[row][slice];
        piecesGrabbed.fetch_add(1);

        /* If I just gave the cuboid another column, update its columns
         * field to match
         * (I can do this atomically and don't need a lock)
         */
        int cuboidColumns = o_uvw.v[1] - cuboid.c;
        cuboid.columns.compare_exchange_strong(cuboidColumns, cuboidColumns + 1);

#if GRAB_DEBUG
        AFK_DEBUG_PRINTL("  grabbed: " << o_uvw)
#endif

        return AFK_JIGSAW_CUBOID_GRABBED;
    }
    else
    {
        /* We ran out of columns, you need to start a new cuboid.
         */
#if GRAB_DEBUG
        AFK_DEBUG_PRINTL("  out of columns")
#endif
        return AFK_JIGSAW_CUBOID_OUT_OF_COLUMNS;
    }
}

void AFK_Jigsaw::pushNewCuboid(AFK_JigsawCuboid cuboid)
{
    cuboids[updateCs].push_back(cuboid);
    for (int row = cuboid.r; row < (cuboid.r + cuboid.rows); ++row)
    {
        for (int slice = cuboid.s; slice < (cuboid.s + cuboid.slices); ++slice)
        {
            rowUsage[row][slice] = cuboid.c;
        }
    }

#if CUBOID_DEBUG
    AFK_DEBUG_PRINTL("  new cuboid at: " << cuboid)
#endif
}

bool AFK_Jigsaw::startNewCuboid(const AFK_JigsawCuboid& lastCuboid, bool startNewRow)
{
#if CUBOID_DEBUG
    AFK_DEBUG_PRINTL("startNewCuboid: starting new cuboid after " << lastCuboid)
#endif

    /* Update the column counts with the last cuboid's. */
    columnCounts.push(lastCuboid.columns.load());

    /* Cuboids are always the same size right now, which makes things easier: */
    int newRowCount = concurrency;
    int newSliceCount = 1;

    int nextFreeColumn = lastCuboid.c + lastCuboid.columns.load();

    if (startNewRow ||
        newRowCount > (jigsawSize.v[1] - nextFreeColumn) ||
        columnCounts.get() > (jigsawSize.v[1] - nextFreeColumn))
    {
        /* Find a place to start a new cuboid.
         * If I can go one row up, use that:
         */
        int newRow, newSlice;
        if ((lastCuboid.r + lastCuboid.rows + newRowCount) <= jigsawSize.v[0])
        {
            /* There is room on the next row up. */
            newRow = lastCuboid.r + lastCuboid.rows;
            newSlice = lastCuboid.s;
        }
        else
        {
            /* Try the next slice along. */
            newRow = 0;
            newSlice = lastCuboid.s + lastCuboid.slices;
            if ((newSlice + newSliceCount) >= jigsawSize.v[2]) newSlice = 0;
        }

        /* Check I'm not running into the sweep position. */
        if ((newSlice <= sweepPosition.v[1] && sweepPosition.v[1] < (newSlice + newSliceCount)) &&
            (newRow <= sweepPosition.v[0] && sweepPosition.v[0] < (newRow + newRowCount)))
        {
            /* Poo. */
#if CUBOID_DEBUG
            AFK_DEBUG_PRINTL("  new cuboid at " << newRow << ", " << newSlice << " ran into sweep position at " << sweepPosition)
#endif
            return false;
        }

        pushNewCuboid(AFK_JigsawCuboid(newRow, 0, newSlice, newRowCount, newSliceCount));
    }
    else
    {
        /* I'm going to pack a new cuboid onto the same row as the existing one.
         * This operation also closes down the last cuboid (if you add columns to
         * it you'll trample the new one).
         */
        pushNewCuboid(AFK_JigsawCuboid(lastCuboid.r, lastCuboid.c + lastCuboid.columns.load(), lastCuboid.s, newRowCount, newSliceCount));
    }

    cuboidsStarted.fetch_add(1);
    return true;
}

int AFK_Jigsaw::roundUpToConcurrency(int r) const
{
    int rmodc = r % concurrency;
    if (rmodc > 0) r += (concurrency - rmodc);
    return r;
}

#define SWEEP_DEBUG 0

Vec2<int> AFK_Jigsaw::getSweepTarget(const Vec2<int>& latest) const
{
    Vec2<int> target;

    if (jigsawSize.v[2] > 1)
    {
        /* Hop across slices. */
        int targetSlice = latest.v[1] + (jigsawSize.v[2] / 8) + 1;
        if (targetSlice >= jigsawSize.v[2]) targetSlice -= jigsawSize.v[2];
        target = afk_vec2<int>(latest.v[0], targetSlice);
    }
    else
    {
        /* Hop across rows. */
        int targetRow = roundUpToConcurrency(latest.v[0] + (jigsawSize.v[0] / 8));
        if (targetRow >= jigsawSize.v[0]) targetRow -= jigsawSize.v[0];
        target = afk_vec2<int>(targetRow, latest.v[1]);
    }

#if SWEEP_DEBUG
    AFK_DEBUG_PRINTL("sweepTarget: " << sweepPosition << " -> " << target << " (latest: " << latest << ", jigsawSize " << jigsawSize << ", concurrency " << concurrency << ")")
#endif

    return target;
}

void AFK_Jigsaw::sweep(const Vec2<int>& sweepTarget, const AFK_Frame& currentFrame)
{
    while (sweepPosition != sweepTarget)
    {
        /* Sweep the rows up to the top, then flip to the next
         * slice.
         */
        rowTimestamp[sweepPosition.v[0]][sweepPosition.v[1]] = currentFrame;
        ++(sweepPosition.v[0]);
        piecesSwept.fetch_add(jigsawSize.v[0]);

        if (sweepPosition.v[0] == jigsawSize.v[0])
        {
            /* I hit the top of the row, set back down again and
             * shift to the next slice.
             */
            sweepPosition.v[0] = 0;
            ++(sweepPosition.v[1]);
            if (sweepPosition.v[1] == jigsawSize.v[2])
            {
                /* I hit the end of the jigsaw, wrap around. */
                sweepPosition.v[1] = 0;
            }
        }
    }
}

void AFK_Jigsaw::doSweep(const Vec2<int>& nextFreeRow, const AFK_Frame& currentFrame)
{
    /* Let's try to keep the sweep row some way ahead of the
     * next free row.
     */
    if (jigsawSize.v[2] > 2)
    {
        int sweepSliceCmp = (sweepPosition.v[1] < nextFreeRow.v[1] ? (sweepPosition.v[1] + jigsawSize.v[2]) : sweepPosition.v[1]) + 1;
        if ((sweepSliceCmp - nextFreeRow.v[1]) < 2)
        {
            sweep(getSweepTarget(afk_vec2<int>(nextFreeRow.v[0], sweepSliceCmp)), currentFrame);
        }
    }
    else
    {
        int sweepRowCmp = roundUpToConcurrency((sweepPosition.v[0] < nextFreeRow.v[0] ? (sweepPosition.v[0] + jigsawSize.v[0]) : sweepPosition.v[0]));
        if ((sweepRowCmp - nextFreeRow.v[0]) < (jigsawSize.v[0] / (int)concurrency))
        {
            sweep(getSweepTarget(afk_vec2<int>(sweepRowCmp, nextFreeRow.v[1])), currentFrame);
        }
    }
}

AFK_Jigsaw::AFK_Jigsaw(
    AFK_Computer *_computer,
    const Vec3<int>& _jigsawSize,
    const std::vector<AFK_JigsawImageDescriptor>& _desc,
    const std::vector<unsigned int>& _threadIds):
        jigsawSize(_jigsawSize),
        concurrency(_threadIds.size()),
        updateCs(0),
        drawCs(1),
        columnCounts(8, 0)
{
    /* Sort out the mapping from the supplied jigsaw user thread IDs
     * to 0..concurrency.
     */
    unsigned int threadIdInternal = 0;
    for (auto id : _threadIds)
        threadIdMap[id] = threadIdInternal++;

    /* Make the images. */
    for (auto d : _desc)
        images.push_back(new AFK_JigsawImage(_computer, jigsawSize, d));

    /* Now that I've got the textures, fill out the jigsaw state. */
    rowTimestamp = new AFK_Frame*[jigsawSize.v[0]];
    rowUsage = new int*[jigsawSize.v[0]];

    for (int row = 0; row < jigsawSize.v[0]; ++row)
    {
        rowTimestamp[row] = new AFK_Frame[jigsawSize.v[2]];
        rowUsage[row] = new int[jigsawSize.v[2]];

        for (int slice = 0; slice < jigsawSize.v[2]; ++slice)
        {
            rowTimestamp[row][slice] = AFK_Frame();
            rowUsage[row][slice] = 0;
        }
    }

    /* Start the sweep position as far through the jigsaw as I
     * can whilst still being on a concurrency boundary
     * (which is expected, because cuboids occupy `concurrency'
     * rows each)
     */
    sweepPosition = afk_vec2<int>(
        jigsawSize.v[0] - 1,
        jigsawSize.v[2] - 1);
    sweepPosition.v[0] -= (sweepPosition.v[0] % concurrency);

    /* Make a starting update cuboid. */
    if (!startNewCuboid(
        AFK_JigsawCuboid(0, 0, 0, concurrency, 1), false))
    {
        throw AFK_Exception("Cannot make starting cuboid");
    }

    piecesGrabbed.store(0);
    cuboidsStarted.store(0);
    piecesSwept.store(0);
}

AFK_Jigsaw::~AFK_Jigsaw()
{
    for (auto image : images) delete image;

    for (int row = 0; row < jigsawSize.v[0]; ++row)
    {
        delete[] rowUsage[row];
        delete[] rowTimestamp[row];
    }

    delete[] rowUsage;
    delete[] rowTimestamp;
}

bool AFK_Jigsaw::grab(unsigned int threadId, Vec3<int>& o_uvw, AFK_Frame *o_timestamp)
{
    bool grabSuccessful = false;
    bool gotUpgradeLock = false;
    if (cuboidMuts[updateCs].try_lock_upgrade()) gotUpgradeLock = true;
    else cuboidMuts[updateCs].lock_shared();

    /* Let's see if I can use an existing cuboid. */
    unsigned int cI;
    for (cI = 0; cI < cuboids[updateCs].size(); ++cI)
    {
        switch (grabPieceFromCuboid(cuboids[updateCs][cI], threadIdMap.at(threadId), o_uvw, o_timestamp))
        {
#if 0
        case AFK_JIGSAW_CUBOID_OUT_OF_SPACE:
            /* This means I'm out of room in the jigsaw. */
            goto grab_return;
#endif

        case AFK_JIGSAW_CUBOID_GRABBED:
            /* I got one! */
            grabSuccessful = true;
            goto grab_return;

        default:
            /* Keep looking. */
            break;
        }
    }

    /* I ran out of cuboids.  Try to start a new one. */
    if (cI == 0) goto grab_return;

    if (!gotUpgradeLock)
    {
        cuboidMuts[updateCs].unlock_shared();
        cuboidMuts[updateCs].lock_upgrade();
        gotUpgradeLock = true;
    }

    cuboidMuts[updateCs].unlock_upgrade_and_lock();

    {
        bool retry = true;
        if (cI == cuboids[updateCs].size())
            retry = startNewCuboid(cuboids[updateCs][cI-1], true);

        cuboidMuts[updateCs].unlock();
        if (!retry) return false;
    }

    return grab(threadId, o_uvw, o_timestamp);

grab_return:
    if (gotUpgradeLock) cuboidMuts[updateCs].unlock_upgrade();
    else cuboidMuts[updateCs].unlock_shared();
    return grabSuccessful;
}

AFK_Frame AFK_Jigsaw::getTimestamp(const AFK_JigsawPiece& piece) const
{
    return rowTimestamp[piece.u][piece.w];
}

unsigned int AFK_Jigsaw::getTexCount(void) const
{
    return images.size();
}

Vec2<float> AFK_Jigsaw::getTexCoordST(const AFK_JigsawPiece& piece) const
{
    return afk_vec2<float>(
        (float)piece.u / (float)jigsawSize.v[0],
        (float)piece.v / (float)jigsawSize.v[1]);
}

Vec3<float> AFK_Jigsaw::getTexCoordSTR(const AFK_JigsawPiece& piece) const
{
    return afk_vec3<float>(
        (float)piece.u / (float)jigsawSize.v[0],
        (float)piece.v / (float)jigsawSize.v[1],
        (float)piece.w / (float)jigsawSize.v[2]);
}

Vec2<float> AFK_Jigsaw::getPiecePitchST(void) const
{
    return afk_vec2<float>(
        1.0f / (float)jigsawSize.v[0],
        1.0f / (float)jigsawSize.v[1]);
}

Vec3<float> AFK_Jigsaw::getPiecePitchSTR(void) const
{
    return afk_vec3<float>(
        1.0f / (float)jigsawSize.v[0],
        1.0f / (float)jigsawSize.v[1],
        1.0f / (float)jigsawSize.v[2]);
}

Vec2<int> AFK_Jigsaw::getFake3D_size(unsigned int tex) const
{
    return images.at(tex)->getFake3D_size();
}

int AFK_Jigsaw::getFake3D_mult(unsigned int tex) const
{
    return images.at(tex)->getFake3D_mult();
}

cl_mem AFK_Jigsaw::acquireForCl(unsigned int tex, AFK_ComputeDependency& o_dep)
{
    boost::upgrade_lock<boost::upgrade_mutex> lock(cuboidMuts[drawCs]);
    return images.at(tex)->acquireForCl(o_dep);
}

void AFK_Jigsaw::releaseFromCl(unsigned int tex, const AFK_ComputeDependency& dep)
{
    boost::upgrade_lock<boost::upgrade_mutex> lock(cuboidMuts[drawCs]);
    images.at(tex)->releaseFromCl(cuboids[drawCs], dep);
}

void AFK_Jigsaw::bindTexture(unsigned int tex)
{
    boost::upgrade_lock<boost::upgrade_mutex> lock(cuboidMuts[drawCs]);
    images.at(tex)->bindTexture(cuboids[drawCs]);
}

#define FLIP_DEBUG 0

void AFK_Jigsaw::flipCuboids(const AFK_Frame& currentFrame)
{
    /* Make sure any changes are really, properly finished */
    for (auto image : images) image->waitForAll();

    boost::upgrade_lock<boost::upgrade_mutex> ulock0(cuboidMuts[0]);
    boost::upgrade_to_unique_lock<boost::upgrade_mutex> utoulock0(ulock0);

    boost::upgrade_lock<boost::upgrade_mutex> ulock1(cuboidMuts[1]);
    boost::upgrade_to_unique_lock<boost::upgrade_mutex> utoulock1(ulock0);

    updateCs = (updateCs == 0 ? 1 : 0);
    drawCs = (drawCs == 0 ? 1 : 0);

    /* Clear out the old draw cuboids, and create a new update
     * cuboid following on from the last one.
     */
    cuboids[updateCs].clear();

    /* Do we have an existing cuboid to continue from? */
    bool continued = false;
    if (cuboids[drawCs].size() > 0)
    {
        /* Sweep in front of the cuboid I'm about to make */
        std::vector<AFK_JigsawCuboid>::reverse_iterator lastCuboid = cuboids[drawCs].rbegin();
        Vec2<int> nextFree = afk_vec2<int>(lastCuboid->r + lastCuboid->rows, lastCuboid->s);
        if (nextFree.v[0] >= jigsawSize.v[0])
        {
            nextFree.v[0] = 0;
            nextFree.v[1] = lastCuboid->s + lastCuboid->slices;
            if (nextFree.v[1] >= jigsawSize.v[2]) nextFree.v[1] = 0;
        }
        doSweep(nextFree, currentFrame);

#if FLIP_DEBUG
        AFK_DEBUG_PRINTL("Flipping from draw cuboid " << *lastCuboid << " (with sweep position " << sweepPosition << ")")
#endif
        continued = startNewCuboid(*lastCuboid, false);
    }
    else
    {
#if FLIP_DEBUG
        AFK_DEBUG_PRINTL("Trying to start fresh cuboid")
#endif
        /* This should be a rare case: I previously ran out of
         * cuboids in this jigsaw.
         * Pick a place to try to start from (this may not succeed).
         * I need to ask for a new row to trigger the eviction stuff.
         */
        continued = startNewCuboid(AFK_JigsawCuboid(0, 0, 0, concurrency, 1), true);

        if (continued)
        {
            /* Now, reset the sweep and sweep across that cuboid. */
            sweepPosition = afk_vec2<int>(0, 0);
            Vec2<int> nextFree = afk_vec2<int>(0, 0);
            doSweep(nextFree, currentFrame);
        }
    }

    /* This shouldn't happen, *but* ... */
    if (!continued) throw AFK_Exception("flipCuboids() failed");
}

void AFK_Jigsaw::printStats(std::ostream& os, const std::string& prefix)
{
    /* TODO Time intervals or something?  (Really the relative numbers are more interesting) */
    std::cout << prefix << "\t: Pieces grabbed:       " << piecesGrabbed.exchange(0) << std::endl;
    std::cout << prefix << "\t: Cuboids started:      " << cuboidsStarted.exchange(0) << std::endl;
    std::cout << prefix << "\t: Pieces swept:         " << piecesSwept.exchange(0) << std::endl;  
}

