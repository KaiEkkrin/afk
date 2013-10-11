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

#include <iostream>

#include <boost/random/random_device.hpp>
#include <boost/unordered_map.hpp>

#include "computer.hpp"
#include "data/frame.hpp"
#include "def.hpp"
#include "exception.hpp"
#include "jigsaw_collection.hpp"

void afk_testJigsaw(
    AFK_Computer *computer,
    const AFK_Config *config)
{
    boost::random::random_device rdev;
    srand(rdev());

    /* Make a jigsaw with a plausible shape size, and allocate lots
     * of pieces out of it in multiple iterations.  Upon each of
     * these simulated frames, check that the set of available
     * pieces appears sane and hasn't gotten trampled, etc.
     * TODO -- Improvements:
     * - Test a 3D one too (pull out the test into a function I
     * can call with several)
     * - Multi-threaded test
     * - Test OpenCL program that writes known values to the jigsaw
     * texture -- verify that the values come out OK and don't
     * trample each other either.
     */
    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    const int testIterations = 1000;
    int startingPieceCount = 400;

    AFK_JigsawCollection testCollection(
        ctxt,
        afk_vec3<int>(9, 9, 1),
        startingPieceCount,
        4,
        AFK_JIGSAW_2D,
        { AFK_JIGSAW_4FLOAT32 },
        computer->getFirstDeviceProps(),
        AFK_JIGSAW_BU_CL_ONLY,
        config->concurrency,
        false);

    int pieceCount = testCollection.getPieceCount();
    assert(pieceCount >= startingPieceCount);

    AFK_Frame frame;
    frame.increment();
    testCollection.flipCuboids(ctxt, frame);

    for (int i = 0; i < testIterations; ++i)
    {
        int piecesThisFrame = rand() % (config->concurrency * pieceCount / 4);
        std::cout << "Test frame " << frame << ": Getting " << piecesThisFrame << " pieces" << std::endl;

        /* Here, I map each piece that I've drawn to its timestamp. */
        boost::unordered_map<AFK_JigsawPiece, AFK_Frame> piecesMap;

        try
        {
            for (int p = 0; p < piecesThisFrame; ++p)
            {
                AFK_JigsawPiece jigsawPiece;
                AFK_Frame pieceFrame;
    
                testCollection.grab(rand() % config->concurrency, 0, &jigsawPiece, &pieceFrame, 1);
                //std::cout << "Grabbed piece " << jigsawPiece << " with frame " << pieceFrame << std::endl;

                auto existing = piecesMap.find(jigsawPiece);
                if (existing != piecesMap.end()) assert(existing->second != pieceFrame);
                piecesMap[jigsawPiece] = pieceFrame;
            }
        }
        catch (AFK_Exception e)
        {
            if (e.getMessage() == "Jigsaw ran out of room")
            {
                /* I'll forgive this. */
                std::cout << "Out of room -- OK -- continuing" << std::endl;
            }
            else
            {
                throw e;
            }
        }

        frame.increment();
        testCollection.flipCuboids(ctxt, frame);
    }

    std::cout << "Jigsaw test completed" << std::endl;
        
    computer->unlock();
}

