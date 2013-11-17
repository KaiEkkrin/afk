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
    const std::vector<AFK_JigsawImageDescriptor>& _desc):
        computer(_computer),
        jigsawSize(_jigsawSize),
        desc(_desc)
{
}

#if AFK_JIGSAW_COLLECTION_CHAIN
AFK_Jigsaw *AFK_JigsawFactory::operator()() const
{
    return new AFK_Jigsaw(
        computer,
        jigsawSize,
        desc);
}
#else
std::shared_ptr<AFK_Jigsaw> AFK_JigsawFactory::operator()() const
{
    return std::make_shared<AFK_Jigsaw>(
        computer,
        jigsawSize,
        desc);
}
#endif

/* AFK_JigsawCollection implementation */

AFK_JigsawCollection::AFK_JigsawCollection(
    AFK_Computer *_computer,
    const AFK_JigsawMemoryAllocation::Entry& _e,
    const AFK_ClDeviceProperties& _clDeviceProps,
    int _maxPuzzles):
        maxPuzzles(_maxPuzzles)
{
    assert(maxPuzzles == 0 || maxPuzzles >= (int)_e.getPuzzleCount());

    std::vector<AFK_JigsawImageDescriptor> desc;
    for (auto d = _e.beginDescriptors(); d != _e.endDescriptors(); ++d)
        desc.push_back(*d);

    jigsawFactory = std::make_shared<AFK_JigsawFactory>(
        _computer, _e.getJigsawSize(), desc);

    /* There should always be at least one puzzle. */
    assert(_e.getPuzzleCount() > 0);
#if AFK_JIGSAW_COLLECTION_CHAIN
    puzzles = new Puzzle(jigsawFactory);
    for (unsigned int j = 1; j < _e.getPuzzleCount(); ++j)
        puzzles->extend();
#else
    for (unsigned int j = 0; j < _e.getPuzzleCount(); ++j)
        puzzles.push_back((*jigsawFactory)());
#endif
}

AFK_JigsawCollection::~AFK_JigsawCollection()
{
#if AFK_JIGSAW_COLLECTION_CHAIN
    delete puzzles;
#endif
}

void AFK_JigsawCollection::grab(
    int minJigsaw,
    AFK_JigsawPiece *o_pieces,
    AFK_Frame *o_timestamps,
    int count)
{
#if AFK_JIGSAW_COLLECTION_CHAIN
    Puzzle *start = puzzles;

    int numGrabbed = 0;
    while (numGrabbed < count)
    {
        /* Grab pieces from this puzzle or any puzzle after it,
         * creating as necessary.
         */
        int puzzleIdx = 0;

        for (Puzzle *chain = start; numGrabbed < count; chain = chain->extend())
        {
            AFK_Jigsaw *jigsaw = chain->get();

            while (puzzleIdx >= minJigsaw && numGrabbed < count)
            {
                Vec3<int> uvw;
                if (jigsaw->grab(uvw, &o_timestamps[numGrabbed]))
                {
                    o_pieces[numGrabbed] = AFK_JigsawPiece(uvw, puzzleIdx);
                    ++numGrabbed;
                }
                else break; /* this puzzle couldn't give us a piece */
            }

            ++puzzleIdx;
            if (maxPuzzles > 0 && puzzleIdx >= maxPuzzles &&
                numGrabbed < count)
            {
                /* Oh dear, we've actually hit a wall. */
                std::ostringstream ss;
                ss << "AFK_JigsawCollection: Jigsaw hit maxPuzzles " << maxPuzzles;
                throw AFK_Exception(ss.str());
            }
        }
    }
#else
    /* TODO Can I make this better? */
    std::unique_lock<std::mutex> lock(mut);

    auto puzzleIt = puzzles.begin();
    int numGrabbed = 0;
    int puzzleIdx = 0;
    while (numGrabbed < count)
    {
        if (puzzleIt != puzzles.end())
        {
            /* Grab as many pieces from this puzzle as I can. */
            while (puzzleIdx >= minJigsaw && numGrabbed < count)
            {
                Vec3<int> uvw;
                if ((*puzzleIt)->grab(uvw, &o_timestamps[numGrabbed]))
                {
                    o_pieces[numGrabbed] = AFK_JigsawPiece(uvw, puzzleIdx);
                    ++numGrabbed;
                }
                else break; /* this puzzle couldn't give us a piece */
            }

            ++puzzleIt;
            ++puzzleIdx;
            if (maxPuzzles > 0 && puzzleIdx >= maxPuzzles &&
                numGrabbed < count)
            {
                /* Oh dear, we've actually hit a wall. */
                std::ostringstream ss;
                ss << "AFK_JigsawCollection: Jigsaw hit maxPuzzles " << maxPuzzles;
                throw AFK_Exception(ss.str());
            }
        }
        else
        {
            /* If I get here, I need to add another puzzle. */
            std::shared_ptr<AFK_Jigsaw> newPuzzle = (*jigsawFactory)();
            puzzleIt = puzzles.insert(puzzles.end(), newPuzzle);
        }
    }
#endif
}

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(const AFK_JigsawPiece& piece)
{
    return getPuzzle(piece.puzzle);
}

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(int puzzle)
{
#if AFK_JIGSAW_COLLECTION_CHAIN
    for (Puzzle *chain = puzzles; chain; chain = chain->next())
        if (puzzle-- == 0) return chain->get();

    return nullptr;
#else
    std::unique_lock<std::mutex> lock(mut);
    return puzzles.at(puzzle).get();
#endif
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
#if AFK_JIGSAW_COLLECTION_CHAIN
    for (Puzzle *chain = puzzles; chain; chain = chain->next(), ++i)
    {
        AFK_Jigsaw *jigsaw = chain->get();
#else
    std::unique_lock<std::mutex> lock(mut);
    for (auto jigsaw : puzzles)
    {
#endif
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
#if AFK_JIGSAW_COLLECTION_CHAIN
    for (Puzzle *chain = puzzles; chain && i < count; chain = chain->next(), ++i)
    {
        AFK_Jigsaw *jigsaw = chain->get();
#else
    std::unique_lock<std::mutex> lock(mut);
    for (auto jigsaw : puzzles)
    {
#endif
        jigsaw->releaseFromCl(tex, dep);
    }

    assert(i == count);
}

void AFK_JigsawCollection::flip(const AFK_Frame& currentFrame)
{
#if AFK_JIGSAW_COLLECTION_CHAIN
    for (Puzzle *chain = puzzles; chain; chain = chain->next())
        chain->get()->flip(currentFrame); /* This one is internally locked */
#else
    std::unique_lock<std::mutex> lock(mut);
    for (auto jigsaw : puzzles)
        jigsaw->flip(currentFrame);
#endif
}

void AFK_JigsawCollection::printStats(std::ostream& os, const std::string& prefix)
{
    int i = 0;
#if AFK_JIGSAW_COLLECTION_CHAIN
    for (Puzzle *chain = puzzles; chain; chain = chain->next(), ++i)
    {
        std::ostringstream puzPf;
        puzPf << prefix << " " << std::dec << i;
        chain->get()->printStats(os, puzPf.str());
    }
#else
    for (auto jigsaw : puzzles)
    {
        std::ostringstream puzPf;
        puzPf << prefix << " " << std::dec << i;
        jigsaw->printStats(os, puzPf.str());
        ++i;
    }
#endif
}

