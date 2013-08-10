/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_MOVING_AVERAGE_H_
#define _AFK_MOVING_AVERAGE_H_

#include <vector>

/* Does what it says on the tin. */

template<typename T, int count>
class AFK_MovingAverage
{
protected:
    typename std::vector<T> vals;
    typename std::vector<T>::iterator head;

    T total;

public:
    AFK_MovingAverage(T startingVal)
    {
        for (unsigned int i = 0; i < count; ++i)
            vals.push_back(startingVal);

        head = vals.begin();
        total = startingVal * count;
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
