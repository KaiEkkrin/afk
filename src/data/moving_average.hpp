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

#ifndef _AFK_DATA_MOVING_AVERAGE_H_
#define _AFK_DATA_MOVING_AVERAGE_H_

#include <cassert>
#include <vector>

/* Does what it says on the tin. */

template<typename T>
class AFK_MovingAverage
{
protected:
    typename std::vector<T> vals;
    typename std::vector<T>::iterator head;

    unsigned int count;
    T total;

public:
    AFK_MovingAverage(unsigned int _count, T startingVal):
        count(_count)
    {
        for (unsigned int i = 0; i < count; ++i)
            vals.push_back(startingVal);

        head = vals.begin();
        total = startingVal * count;
    }

    AFK_MovingAverage(const AFK_MovingAverage&& _av) :
        vals(_av.vals),
        count(_av.count)
    {
        assert(count == vals.size());

        head = vals.begin();
        total = 0;
        for (auto val : vals)
            total += val;
    }

    AFK_MovingAverage& operator=(const AFK_MovingAverage&& _av)
    {
        vals = _av.vals;
        count = _av.count;

        assert(count == vals.size());

        head = vals.begin();
        total = 0;
        for (auto val : vals)
            total += val;

        return *this;
    }

    /* Pushes a new value into the moving average,
     * removing the oldest one.  Returns the current
     * average.
     */
    T push(T newVal)
    {
        total -= *head;
        *head = newVal;
        total += newVal;

        ++head;
        if (head == vals.end()) head = vals.begin();
        return (total / count);
    }

    T get(void) const
    {
        return (total / count);
    }
};

#endif /* _AFK_MOVING_AVERAGE_H_ */

