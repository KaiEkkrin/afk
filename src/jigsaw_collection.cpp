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

std::shared_ptr<AFK_Jigsaw> AFK_JigsawFactory::operator()() const
{
    return std::make_shared<AFK_Jigsaw>(
        computer,
        jigsawSize,
        desc);
}

/* AFK_JigsawCollection implementation */

AFK_JigsawCollection::AFK_JigsawCollection(
    AFK_Computer *_computer,
    const AFK_JigsawMemoryAllocation::Entry& _e,
    const AFK_ClDeviceProperties& _clDeviceProps,
    unsigned int _maxPuzzles):
        maxPuzzles(_maxPuzzles)
{
    assert(maxPuzzles == 0 || maxPuzzles >= _e.getPuzzleCount());

    std::vector<AFK_JigsawImageDescriptor> desc;
    for (auto d = _e.beginDescriptors(); d != _e.endDescriptors(); ++d)
        desc.push_back(*d);

    /* There should always be at least one puzzle. */
    assert(_e.getPuzzleCount() > 0);

    chain = new AFK_Chain<AFK_Jigsaw, AFK_JigsawFactory>(
        std::make_shared<AFK_JigsawFactory>(_computer, _e.getJigsawSize(), desc),
        _e.getPuzzleCount());
}

AFK_JigsawCollection::~AFK_JigsawCollection()
{
    delete chain;
}

void AFK_JigsawCollection::grab(
    unsigned int minJigsaw,
    AFK_JigsawPiece *o_pieces,
    AFK_Frame *o_timestamps,
    unsigned int count)
{
    unsigned int puzzleIdx = 0;
    unsigned int numGrabbed = 0;
    while (numGrabbed < count)
    {
        chain->foreach([&puzzleIdx, &numGrabbed, count, o_pieces, o_timestamps](std::shared_ptr<AFK_Jigsaw> jigsaw)
        {
            /* Grab as many pieces as I can. */
            while (numGrabbed < count)
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
        }, puzzleIdx);

        if (numGrabbed < count)
        {
            /* I need to add a new puzzle. */
            if (maxPuzzles == 0 || puzzleIdx < maxPuzzles)
            {
                chain->lengthen(puzzleIdx);
            }
            else
            {
                /* Oh dear, we've actually hit a wall. */
                std::ostringstream ss;
                ss << "AFK_JigsawCollection: Jigsaw hit maxPuzzles " << maxPuzzles;
                throw AFK_Exception(ss.str());
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
    return chain->at(puzzle).get();
}

unsigned int AFK_JigsawCollection::acquireAllForCl(
    AFK_Computer *computer,
    unsigned int tex,
    cl_mem *allMem,
    unsigned int count,
    Vec2<int>& o_fake3D_size,
    int& o_fake3D_mult,
    AFK_ComputeDependency& o_dep)
{
    assert(count > 0);

    /* This can be a bit time consuming, so I don't want to call
     * chain->foreach() here
     */
    unsigned int i = 0;
    while (i < count)
    {
        std::shared_ptr<AFK_Jigsaw> jigsaw = chain->at(i);
        if (!jigsaw) break;

        // TODO: due to locking, merge these functions?
        jigsaw->setupImages(computer);
        allMem[i] = jigsaw->acquireForCl(tex, o_dep);

        if (i == 0)
        {
            o_fake3D_size = jigsaw->getFake3D_size(tex);
            o_fake3D_mult = jigsaw->getFake3D_mult(tex);
        }
        else
        {
            /* Right now, the fake 3D info must be consistent across
            * images in the same jigsaw.  Make sure this is the
            * case.
            */
            assert(o_fake3D_size == jigsaw->getFake3D_size(tex));
            assert(o_fake3D_mult == jigsaw->getFake3D_mult(tex));
        }

        ++i;
    }

    /* If there's a smaller number of puzzles than the count
     * requested, fill in the remaining memory references with
     * copies of the first one -- it needs to be something valid,
     * the OpenCL ought to not try to hit it (e.g. no jigsaw
     * pieces referring to that puzzle)
     */
    for (unsigned int extra = i; extra < count; ++extra)
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
    unsigned int count,
    const AFK_ComputeDependency& dep)
{
    /* As above, no chain->foreach() here */
    for (unsigned int i = 0; i < count; ++i)
    {
        std::shared_ptr<AFK_Jigsaw> jigsaw = chain->at(i);
        if (jigsaw)
        {
            jigsaw->releaseFromCl(tex, dep);
        }
    }
}

void AFK_JigsawCollection::flip(const AFK_Frame& currentFrame)
{
    chain->foreach([&currentFrame](std::shared_ptr<AFK_Jigsaw> jigsaw)
    {
        jigsaw->flip(currentFrame);
    });
}

void AFK_JigsawCollection::printStats(std::ostream& os, const std::string& prefix)
{
    int i = 0;
    chain->foreach([&os, &prefix, &i](std::shared_ptr<AFK_Jigsaw> jigsaw)
    {
        std::ostringstream puzPf;
        puzPf << prefix << " " << std::dec << i++;
        jigsaw->printStats(os, puzPf.str());
    });
}
