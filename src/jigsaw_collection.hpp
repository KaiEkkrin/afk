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

#ifndef _AFK_JIGSAW_COLLECTION_H_
#define _AFK_JIGSAW_COLLECTION_H_

#include "afk.hpp"

#include <memory>
#include <mutex>
#include <vector>

#include "data/chain.hpp"
#include "jigsaw.hpp"
#include "jigsaw_image.hpp"


/* I'm deeply suspicious of the chain, so I'm going to make
 * it on/off to see what happens...
 *
 * ... TODO: Okay, so it mostly works.  However, "mostly" isn't "entirely".
 * Occasionally I get an uninitialised Jigsaw appear in the render thread
 * (uninitialised as in bad `images' vector -- constructor's memory changes
 * not synchronized to the core the render thread is on).  I need to think
 * about how to deal with this.  Maybe even get rid of Jigsaw as a construct
 * and have the Chain be entirely of JigsawMaps, with a simple vector of
 * JigsawImages managed entirely by the render thread?
 *
 * Here's a hack to try for situations like that: include a mutex in the
 * object that's used by one thread but constructed by another.  Acquire
 * it on construction, of course.  When the user thread gets to the object,
 * lock that mutex and never unlock it (except in the object's destructor,
 * controlled by a flag).  That ought to sort things out, assuming there isn't
 * a penalty for holding a few extra mutexes the whole time...!
 *
 * TODO *2: With the locked version enabled, when flying very close to an
 * object (something I wasn't really able to try out with the chain version)
 * all shape renders became corrupt afterwards...
 */
#define AFK_JIGSAW_COLLECTION_CHAIN 1


/* How to make a new Jigsaw. */
class AFK_JigsawFactory
{
protected:
    AFK_Computer *computer;
    Vec3<int> jigsawSize;
    std::vector<AFK_JigsawImageDescriptor> desc;

public:
    AFK_JigsawFactory(
        AFK_Computer *_computer,
        const Vec3<int>& _jigsawSize,
        const std::vector<AFK_JigsawImageDescriptor>& _desc);

#if AFK_JIGSAW_COLLECTION_CHAIN
    AFK_Jigsaw *operator()() const;
#else
    std::shared_ptr<AFK_Jigsaw> operator()() const;
#endif
};


/* This encapsulates a collection of jigsawed textures, which are used
 * to give out pieces of the same size and usage.
 * You may get a piece in any of the puzzles.
 */
class AFK_JigsawCollection
{
protected:
    const int maxPuzzles;

    std::shared_ptr<AFK_JigsawFactory> jigsawFactory;
#if AFK_JIGSAW_COLLECTION_CHAIN
    typedef AFK_Chain<AFK_Jigsaw, AFK_JigsawFactory> Puzzle;
    Puzzle *puzzles;
#else
    std::vector<std::shared_ptr<AFK_Jigsaw> > puzzles;
    std::mutex mut;
#endif

public:
    AFK_JigsawCollection(
        AFK_Computer *_computer,
        const AFK_JigsawMemoryAllocation::Entry& _e,
        const AFK_ClDeviceProperties& _clDeviceProps,
        int _maxPuzzles /* 0 for no maximum */);
    virtual ~AFK_JigsawCollection();

    /* Gives you a some pieces.  This will usually be quick,
     * but it may stall if we need to add a new jigsaw
     * to the collection.
     * The pieces are guaranteed to all come from the same
     * jigsaw.
     * Also fills out `o_timestamp' with the timestamp of the row
     * your piece came from so you can find out when it's
     * going to be swept.
     * `minJigsaw' tells it which index jigsaw to try grabbing
     * from.  This is an attempt at a cunning trick by which I
     * can separate long-lived pieces (to start at jigsaw 0)
     * from shorter-lived pieces (to start at a higher jigsaw)
     * to avoid repeatedly sweeping out long-lived pieces only
     * to re-create them the same.
     */
    void grab(
        int minJigsaw,
        AFK_JigsawPiece *o_pieces,
        AFK_Frame *o_timestamps,
        int count);

    /* These next functions will throw std::out_of_range if they
     * can't find a particular puzzle...
     */

    /* Gets you the puzzle that matches a particular piece. */
    AFK_Jigsaw *getPuzzle(const AFK_JigsawPiece& piece);

    /* Gets you a numbered puzzle. */
    AFK_Jigsaw *getPuzzle(int puzzle);

    /* Acquires all puzzles for the CL (up to `count').
     * Complains if there are more than `count' puzzles.
     * If there are fewer puzzles, fills out the remaining
     * fields of the array with the first one.
     * Fills out the given compute dependency as required,
     * and also the fake 3D info.
     * Returns the actual number of puzzles acquired.
     */
    int acquireAllForCl(
        AFK_Computer *computer,
        unsigned int tex,
        cl_mem *allMem,
        int count,
        Vec2<int>& o_fake3D_size,
        int& o_fake3D_mult,
        AFK_ComputeDependency& o_dep);

    /* Releases all puzzles from the CL, when acquired with the above.
     * `count' should be the number returned by acquireAllFromCl.
     * Waits for the given compute dependency before proceeding.
     */
    void releaseAllFromCl(
        unsigned int tex,
        int count,
        const AFK_ComputeDependency& dep);

    /* Flips the cuboids in all the jigsaws. */
    void flip(const AFK_Frame& currentFrame);

    void printStats(std::ostream& os, const std::string& prefix);
};

#endif /* _AFK_JIGSAW_COLLECTION_H_ */

