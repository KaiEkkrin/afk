/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_CACHE_H_
#define _AFK_DATA_CACHE_H_

#include <sstream>

/* The data cache model for AFK. */

template<class Key, class Value>
class AFK_Cache
{
public:
    AFK_Cache() {}
    virtual ~AFK_Cache() {}

    /* Returns the number of elements in the cache. */
    virtual size_t size() const = 0;

    /* Returns the value at this key, or inserts a new one and
     * returns that if there wasn't one.
     */
    virtual Value& operator[](const Key& key) = 0;

    /* For debugging. */
    virtual void printEverything(std::ostream& os) const = 0;

    /* TODO: I need a way of analysing the duplication rate.  Right now,
     * there isn't anything.
     */

    virtual void printStats(std::ostream& os, const std::string& prefix) const = 0;
};

#endif /* _AFK_DATA_CACHE_H_ */

