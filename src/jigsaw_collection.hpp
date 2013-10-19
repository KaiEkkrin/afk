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

#include "jigsaw.hpp"

enum AFK_JigsawDimensions
{
    AFK_JIGSAW_2D,
    AFK_JIGSAW_3D
};

/* This encapsulates a collection of jigsawed textures, which are used
 * to give out pieces of the same size and usage.
 * You may get a piece in any of the puzzles.
 */
class AFK_JigsawCollection
{
protected:
    enum AFK_JigsawDimensions dimensions;
    std::vector<AFK_JigsawFormatDescriptor> format;
    AFK_JigsawFake3DDescriptor fake3D;
    const unsigned int texCount;

    Vec3<int> pieceSize;
    Vec3<int> jigsawSize;
    int pieceCount;
    const enum AFK_JigsawBufferUsage bufferUsage;
    const unsigned int concurrency;
    const unsigned int maxPuzzles;

    std::vector<AFK_Jigsaw*> puzzles;
    AFK_Jigsaw *spare;

    boost::upgrade_mutex mut;

    /* Internal helpers. */
    GLuint getGlTextureTarget(void) const;
    std::string getDimensionalityStr(void) const;
    bool grabPieceFromPuzzle(
        unsigned int threadId,
        int puzzle,
        AFK_JigsawPiece *o_piece,
        AFK_Frame *o_timestamp);

    AFK_Jigsaw *makeNewJigsaw(AFK_Computer *computer) const;

    /* For stats. */
    boost::atomic<uint64_t> spills;

public:
    AFK_JigsawCollection(
        AFK_Computer *_computer,
        const Vec3<int>& _pieceSize,
        int _pieceCount,
        unsigned int minPuzzleCount,
        enum AFK_JigsawDimensions _dimensions,
        const std::vector<AFK_JigsawFormat>& texFormat,
        const AFK_ClDeviceProperties& _clDeviceProps,
        enum AFK_JigsawBufferUsage _bufferUsage,
        unsigned int concurrency,
        bool useFake3D,
        unsigned int _maxPuzzles /* 0 for no maximum */);
    virtual ~AFK_JigsawCollection();

    int getPieceCount(void) const;

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
        unsigned int threadId,
        int minJigsaw,
        AFK_JigsawPiece *o_pieces,
        AFK_Frame *o_timestamps,
        size_t count);

    /* Gets you the puzzle that matches a particular piece. */
    AFK_Jigsaw *getPuzzle(const AFK_JigsawPiece& piece);

    /* Gets you a numbered puzzle. */
    AFK_Jigsaw *getPuzzle(int puzzle);

    /* Acquires all puzzles for the CL (up to `count').
     * Complains if there are more than `count' puzzles.
     * If there are fewer puzzles, fills out the remaining
     * fields of the array with the first one.
     * Returns the actual number of puzzles acquired.
     */
    int acquireAllForCl(
        unsigned int tex,
        cl_mem *allMem,
        int count,
        std::vector<cl_event>& o_events);

    /* Releases all puzzles from the CL, when acquired with the above.
     * `count' should be the number returned by acquireAllFromCl.
     */
    void releaseAllFromCl(
        unsigned int tex,
        cl_mem *allMem,
        int count,
        const std::vector<cl_event>& eventWaitList);    

    /* Flips the cuboids in all the jigsaws. */
    void flipCuboids(AFK_Computer *computer, const AFK_Frame& currentFrame);

    void printStats(std::ostream& os, const std::string& prefix);
};

#endif /* _AFK_JIGSAW_COLLECTION_H_ */

