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

#ifndef _AFK_DATA_FAIR_H_
#define _AFK_DATA_FAIR_H_

#include <memory>
#include <vector>

#include <boost/atomic.hpp>

#include "chain.hpp"

/* A "Fair" is a collection of queues (clearly).
 * It maintains a mirrored set of queues, one for the update
 * phase and one for the draw phase, which can be flipped
 * with flipQueues().
 * The underlying queue type must:
 * - have a sensible default constructor (no arguments)
 * - have a clear() function to clear it
 * - have an empty() function that returns true if the
 * queue is empty.
 */
template<typename QueueType>
class AFK_Fair
{
protected:
    AFK_Chain<QueueType> queues[2];

    /* This atomic controls which queue is which, by indicating
     * the current update queue; the draw queue is the other
     * one.
     */
    boost::atomic_uint updateQ;

public:
    AFK_Fair() : updateQ(0) {}

    /* Call this when you're an evaluator thread.  This method
     * gives you a pointer to the queue you should use.
     */
    std::shared_ptr<QueueType> getUpdateQueue(
        unsigned int q)
    {
        return queues[updateQ.load()].lengthen(q);
    }

    /* Fills out the supplied vector with all the draw queues.
     * TODO: can I make this better?  It seems silly to do this
     * copy; can I do the draw itself within a foreach()?
     */
    void getDrawQueues(std::vector<std::shared_ptr<QueueType> >& o_drawQueues)
    {
        queues[1 - updateQ.load()].foreach([&o_drawQueues](std::shared_ptr<QueueType>& queue)
        {
            o_drawQueues.push_back(queue);
        });
    }
        
    void flipQueues(void)
    {
        updateQ.fetch_xor(1);

        /* Clear the update queue afresh. */
        queues[updateQ.load()].foreach([](std::shared_ptr<QueueType>& queue)
        {
            queue->clear();
        });
    }
};

/* This utility lets you turn a notional 2-dimensional index
 * for the Fair into a compatible 1-dimensional index, and
 * back again.
 */
class AFK_Fair2DIndex
{
protected:
    const unsigned int dim1Max;

public:
    AFK_Fair2DIndex(unsigned int _dim1Max);

    unsigned int get1D(unsigned int q, unsigned int r) const;
    void get2D(unsigned int i, unsigned int& o_q, unsigned int& o_r) const;
};

#endif /* _AFK_DATA_FAIR_H_ */

