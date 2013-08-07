/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_CLEARABLE_QUEUE_H_
#define _AFK_DATA_CLEARABLE_QUEUE_H_

#include <exception>

#include <boost/lockfree/queue.hpp>

/* This is a really simplistic wrapper around the boost
 * lockfree queue that gives it a clear() function that
 * throws if there's stuff left, and a default constructor.
 * This allows it to be used in an AFK_Fair.
 */

class AFK_ClearableQueueNotEmptyException: public std::exception {};

template<typename T, size_t size>
class AFK_ClearableQueue: public boost::lockfree::queue<T>
{
public:
    AFK_ClearableQueue(): boost::lockfree::queue<T>(size) {}

    void clear(void)
    {
        /* Because of the queue semantics, if the caller has
         * used the contents, it should be empty already
         */
        if (!boost::lockfree::queue<T>::empty()) throw AFK_ClearableQueueNotEmptyException();
    }
};

#endif /* _AFK_DATA_CLEARABLE_QUEUE_H_ */

