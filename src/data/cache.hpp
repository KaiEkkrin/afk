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

    /* Returns the value at this key, or inserts a new one and
     * returns that if there wasn't one.
     */
    virtual Value& operator[](const Key& key) = 0;

    /* TODO: Iterators. */

    /* TODO: Cache eviction. */

    virtual void printStats(std::ostream& os, const std::string& prefix) const = 0;
};

#endif /* _AFK_DATA_CACHE_H_ */

