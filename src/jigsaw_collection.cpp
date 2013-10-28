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

#include "exception.hpp"
#include "jigsaw_collection.hpp"

#include <cassert>
#include <climits>
#include <cstring>
#include <iostream>



/* AFK_JigsawCollection implementation */

bool AFK_JigsawCollection::grabPieceFromPuzzle(
    unsigned int threadId,
    int puzzle,
    AFK_JigsawPiece *o_piece,
    AFK_Frame *o_timestamp)
{
    Vec3<int> uvw;
    if (puzzles[puzzle]->grab(threadId, uvw, o_timestamp))
    {
        /* This check has caught a few bugs in the past... */
        assert(uvw.v[0] >= 0 && uvw.v[0] < jigsawSize.v[0] &&
            uvw.v[1] >= 0 && uvw.v[1] < jigsawSize.v[1] &&
            uvw.v[2] >= 0 && uvw.v[2] < jigsawSize.v[2]);

        *o_piece = AFK_JigsawPiece(uvw, puzzle);
        return true;
    }

    return false;
}

AFK_Jigsaw *AFK_JigsawCollection::makeNewJigsaw(AFK_Computer *computer) const
{
    return new AFK_Jigsaw(
        computer,
        jigsawSize,
        desc,
        concurrency);
}

AFK_JigsawCollection::AFK_JigsawCollection(
    AFK_Computer *_computer,
    const AFK_JigsawMemoryAllocation::Entry& _e,
    const AFK_ClDeviceProperties& _clDeviceProps,
    unsigned int _concurrency,
    unsigned int _maxPuzzles):
        jigsawSize(_e.getJigsawSize()),
        concurrency(_concurrency),
        maxPuzzles(_maxPuzzles)
{
    assert(maxPuzzles == 0 || maxPuzzles >= _e.getPuzzleCount());

    for (auto d = _e.beginDescriptors(); d != _e.endDescriptors(); ++d)
        desc.push_back(*d);

    for (unsigned int j = 0; j < _e.getPuzzleCount(); ++j)
        puzzles.push_back(makeNewJigsaw(_computer));

    spare = makeNewJigsaw(_computer);

    spills.store(0);
}

AFK_JigsawCollection::~AFK_JigsawCollection()
{
    for (auto p : puzzles) delete p;
    if (spare) delete spare;
}

void AFK_JigsawCollection::grab(
    unsigned int threadId,
    int minJigsaw,
    AFK_JigsawPiece *o_pieces,
    AFK_Frame *o_timestamps,
    size_t count)
{
    bool gotUpgradeLock = false;
    if (mut.try_lock_upgrade()) gotUpgradeLock = true;
    else mut.lock_shared();

    int puzzle;
    AFK_JigsawPiece piece;
    for (puzzle = minJigsaw; puzzle < (int)puzzles.size(); ++puzzle)
    {
        bool haveAllPieces = true;
        for (size_t pieceIdx = 0; haveAllPieces && pieceIdx < count; ++pieceIdx)
        {
            haveAllPieces &= grabPieceFromPuzzle(threadId, puzzle, &o_pieces[pieceIdx], &o_timestamps[pieceIdx]);
            
        }

        if (haveAllPieces) goto grab_return;
    }

    /* If I get here I've run out of room entirely.  See if I have
     * a spare jigsaw to push in.
     */
    if (!gotUpgradeLock)
    {
        mut.unlock_shared();
        mut.lock_upgrade();
        gotUpgradeLock = true;
    }

    mut.unlock_upgrade_and_lock();

    /* remember to re-check whether we tried all puzzles, someone else
     * might've just gone through here and chucked the spare in
     */
    if (puzzle == (int)puzzles.size())
    {
        if (spare)
        {
            puzzles.push_back(spare);
            spare = nullptr;
        }
        else
        {
            /* Properly out of room.  Failed. */
            mut.unlock();
            throw AFK_Exception("Jigsaw ran out of room");
        }
    }

    mut.unlock();
    return grab(threadId, puzzle, o_pieces, o_timestamps, count);

grab_return:
    if (gotUpgradeLock) mut.unlock_upgrade();
    else mut.unlock_shared();
}

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(const AFK_JigsawPiece& piece)
{
    if (piece == AFK_JigsawPiece()) throw AFK_Exception("AFK_JigsawCollection: Called getPuzzle() with the null piece");
    boost::shared_lock<boost::upgrade_mutex> lock(mut);
    return puzzles.at(piece.puzzle);
}

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(int puzzle)
{
    boost::shared_lock<boost::upgrade_mutex> lock(mut);
    return puzzles.at(puzzle);
}

int AFK_JigsawCollection::acquireAllForCl(
    unsigned int tex,
    cl_mem *allMem,
    int count,
    AFK_ComputeDependency& o_dep)
{
    boost::shared_lock<boost::upgrade_mutex> lock(mut);

    int i;
    int puzzleCount = (int)puzzles.size();

    for (i = 0; i < puzzleCount; ++i)
        allMem[i] = puzzles.at(i)->acquireForCl(tex, o_dep);
    for (int excess = i; excess < count; ++excess)
        allMem[excess] = allMem[0];

    return i;
}

void AFK_JigsawCollection::releaseAllFromCl(
    unsigned int tex,
    int count,
    const AFK_ComputeDependency& dep)
{
    boost::shared_lock<boost::upgrade_mutex> lock(mut);
    
    for (int i = 0; i < count && i < (int)puzzles.size(); ++i)
        puzzles.at(i)->releaseFromCl(tex, dep);
}

void AFK_JigsawCollection::flipCuboids(AFK_Computer *computer, const AFK_Frame& currentFrame)
{
    boost::upgrade_lock<boost::upgrade_mutex> ulock(mut);
    boost::upgrade_to_unique_lock<boost::upgrade_mutex> utoulock(ulock);

    for (auto puzzle : puzzles)
        puzzle->flipCuboids(currentFrame);

    if (!spare && (maxPuzzles == 0 || puzzles.size() < maxPuzzles))
    {
        /* Make a new one to push along. */
        spare = makeNewJigsaw(computer);
    }
}

void AFK_JigsawCollection::printStats(std::ostream& os, const std::string& prefix)
{
    boost::shared_lock<boost::upgrade_mutex> lock(mut);
    std::cout << prefix << "\t: Spills:               " << spills.exchange(0) << std::endl;
    for (int puzzle = 0; puzzle < (int)puzzles.size(); ++puzzle)
    {
        std::ostringstream puzPf;
        puzPf << prefix << " " << std::dec << puzzle;
        puzzles.at(puzzle)->printStats(os, puzPf.str());
    }
}

