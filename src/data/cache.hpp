/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_CACHE_H_
#define _AFK_DATA_CACHE_H_

#include <sstream>

/* The data cache model for AFK. */

template<class Key, class Value>
class AFK_Cache
{
public:
    /* TODO:
     * It would make sense for eviction to be a function of this class
     * itself.  Supply to the constructor a functor that decides whether a
     * value is ready for eviction; the class can manage a thread that
     * wanders about evicting things.
     * But I'm not going to make that yet.
     */
    AFK_Cache() {}
    virtual ~AFK_Cache() {}

    /* Returns the number of elements in the cache. */
    virtual size_t size() const = 0;

    /* Returns the value at this key, or inserts a new one and
     * returns that if there wasn't one.
     */
    virtual Value& operator[](const Key& key) = 0;

#if 0
    /* If the given key has a duplicate "earlier" in the cache than
     * the supplied value, gets it back and returns true.  Otherwise,
     * returns false.
     */
    virtual bool findDuplicate(const Key& key, const Value& value, Value& o_duplicateValue) = 0;
#endif

    /* For debugging. */
    virtual void printEverything(std::ostream& os) const = 0;

    /* TODO: Cache eviction. */

    /* TODO: I need a way of analysing the duplication rate.  Right now,
     * there isn't anything.
     */

    virtual void printStats(std::ostream& os, const std::string& prefix) const = 0;
};

#endif /* _AFK_DATA_CACHE_H_ */

