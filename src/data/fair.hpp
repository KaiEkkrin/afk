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

#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

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
    std::vector<boost::shared_ptr<QueueType> > queues;
    unsigned int updateInc; /* 0 or 1 */
    unsigned int drawInc; /* The opposite */

    boost::mutex mut; /* I'm afraid so */

public:
    AFK_Fair(): updateInc(0), drawInc(1) {}

    /* Call this when you're an evaluator thread.  This method
     * gives you a pointer to the queue you should use.
     */
    boost::shared_ptr<QueueType> getUpdateQueue(
        unsigned int q)
    {
        boost::unique_lock<boost::mutex> lock(mut);

        unsigned int qIndex = q * 2 + updateInc;
        /* Pad the queues up to the next multiple of 2 */
        unsigned int qCount = qIndex + (2 - (qIndex % 2));
        while (queues.size() < qCount)
        {
            boost::shared_ptr<QueueType> newQ(new QueueType());
            queues.push_back(newQ);
        }

        return queues[qIndex];
    }

    /* Fills out the supplied vector with all the draw queues. */
    void getDrawQueues(std::vector<boost::shared_ptr<QueueType> >& o_drawQueues)
    {
        boost::unique_lock<boost::mutex> lock(mut);

        o_drawQueues.reserve(queues.size() / 2);
        for (unsigned int dI = drawInc; dI < queues.size(); dI += 2)
            o_drawQueues.push_back(queues[dI]);
    }
        
    void flipQueues(void)
    {
        boost::unique_lock<boost::mutex> lock(mut);

        updateInc = (updateInc == 0 ? 1 : 0);
        drawInc = (drawInc == 0 ? 1 : 0);

        for (unsigned int uI = updateInc; uI < queues.size(); uI += 2)
            queues[uI]->clear();
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

