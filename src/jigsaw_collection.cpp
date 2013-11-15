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


/* AFK_JigsawFactory implementation */

AFK_JigsawFactory::AFK_JigsawFactory(
    AFK_Computer *_computer,
    const Vec3<int>& _jigsawSize,
    const std::vector<AFK_JigsawImageDescriptor>& _desc,
    const std::vector<unsigned int>& _threadIds):
        computer(_computer),
        jigsawSize(_jigsawSize),
        desc(_desc),
        threadIds(_threadIds)
{
}

AFK_Jigsaw *AFK_JigsawFactory::operator()() const
{
    return new AFK_Jigsaw(
        computer,
        jigsawSize,
        desc,
        threadIds);
}

/* AFK_JigsawCollection implementation */

AFK_JigsawCollection::AFK_JigsawCollection(
    AFK_Computer *_computer,
    const AFK_JigsawMemoryAllocation::Entry& _e,
    const AFK_ClDeviceProperties& _clDeviceProps,
    const std::vector<unsigned int>& _threadIds,
    unsigned int _maxPuzzles):
        maxPuzzles(_maxPuzzles)
{
    assert(maxPuzzles == 0 || maxPuzzles >= _e.getPuzzleCount());

    std::vector<AFK_JigsawImageDescriptor> desc;
    for (auto d = _e.beginDescriptors(); d != _e.endDescriptors(); ++d)
        desc.push_back(*d);

    jigsawFactory = std::make_shared<AFK_JigsawFactory>(
        _computer, _e.getJigsawSize(), desc, _threadIds);

    /* There should always be at least one puzzle. */
    assert(_e.getPuzzleCount() > 0);
    puzzles = new Puzzle(jigsawFactory);
    for (unsigned int j = 1; j < _e.getPuzzleCount(); ++j)
        puzzles->extend();
}

AFK_JigsawCollection::~AFK_JigsawCollection()
{
    delete puzzles;
}

void AFK_JigsawCollection::grab(
    unsigned int threadId,
    int minJigsaw,
    AFK_JigsawPiece *o_pieces,
    AFK_Frame *o_timestamps,
    int count)
{
    Puzzle *start = puzzles;
    while (minJigsaw-- > 0) start = start->extend();

    int numGrabbed = 0;
    while (numGrabbed < count)
    {
        /* Grab pieces from this puzzle or any puzzle after it,
         * creating as necessary.
         */
        int puzzle = minJigsaw + 1; /* the above logic ends with minJigsaw == -1 */

        for (Puzzle *chain = start; numGrabbed < count; chain = chain->extend(), ++puzzle)
        {
            AFK_Jigsaw *jigsaw = chain->get();
            auto lock = jigsaw->lockUpdate();

            while (numGrabbed < count)
            {
                Vec3<int> uvw;
                if (jigsaw->grab(threadId, uvw, &o_timestamps[numGrabbed]))
                {
                    o_pieces[numGrabbed] = AFK_JigsawPiece(uvw, puzzle);
                    ++numGrabbed;
                }
                else break; /* this puzzle couldn't give us a piece */
            }
        }
    }
}

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(const AFK_JigsawPiece& piece)
{
    return getPuzzle(piece.puzzle);
}

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(int puzzle)
{
    for (Puzzle *chain = puzzles; chain; chain = chain->next())
        if (puzzle-- == 0) return chain->get();

    return nullptr;
}

int AFK_JigsawCollection::acquireAllForCl(
    AFK_Computer *computer,
    unsigned int tex,
    cl_mem *allMem,
    int count,
    Vec2<int>& o_fake3D_size,
    int& o_fake3D_mult,
    AFK_ComputeDependency& o_dep)
{
    assert(count > 0);

    int i = 0;
    for (Puzzle *chain = puzzles; chain; chain = chain->next(), ++i)
    {
        AFK_Jigsaw *jigsaw = chain->get();
        auto lock = jigsaw->lockDraw();
        jigsaw->setupImages(computer);
        allMem[i] = jigsaw->acquireForCl(tex, o_dep);

        if (i == 0)
        {
            o_fake3D_size = jigsaw->getFake3D_size(0);
            o_fake3D_mult = jigsaw->getFake3D_mult(0);
        }
        else
        {
            /* Right now, the fake 3D info must be consistent across
             * images in the same jigsaw.  Make sure this is the
             * case.
             */
            assert(o_fake3D_size == jigsaw->getFake3D_size(i));
            assert(o_fake3D_mult == jigsaw->getFake3D_mult(i));
        }
    }

    /* If there's a smaller number of puzzles than the count
     * requested, fill in the remaining memory references with
     * copies of the first one -- it needs to be something valid,
     * the OpenCL ought to not try to hit it (e.g. no jigsaw
     * pieces referring to that puzzle)
     */
    for (int extra = i; extra < count; ++extra)
    {
        allMem[extra] = allMem[0];
    }

    /* However, only return `i' the number of puzzles that
     * actually need releasing
     */
    return i;
}

void AFK_JigsawCollection::releaseAllFromCl(
    unsigned int tex,
    int count,
    const AFK_ComputeDependency& dep)
{
    int i = 0;
    for (Puzzle *chain = puzzles; chain && i < count; chain = chain->next(), ++i)
    {
        AFK_Jigsaw *jigsaw = chain->get();
        auto lock = jigsaw->lockDraw();
        jigsaw->releaseFromCl(tex, dep);
    }

    assert(i == count);
}

void AFK_JigsawCollection::flipCuboids(const AFK_Frame& currentFrame)
{
    for (Puzzle *chain = puzzles; chain; chain = chain->next())
        chain->get()->flipCuboids(currentFrame); /* This one is internally locked */
}

void AFK_JigsawCollection::printStats(std::ostream& os, const std::string& prefix)
{
    int i = 0;
    for (Puzzle *chain = puzzles; chain; chain = chain->next(), ++i)
    {
        std::ostringstream puzPf;
        puzPf << prefix << " " << std::dec << i;
        chain->get()->printStats(os, puzPf.str());
    }
}

