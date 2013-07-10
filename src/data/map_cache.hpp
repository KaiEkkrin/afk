/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_MAP_CACHE_H_
#define _AFK_DATA_MAP_CACHE_H_

#include <boost/thread/mutex.hpp>
#include <boost/unordered_map.hpp>

#include "cache.hpp"

/* This defines the AFK cache as a simple boost::unordered_map
 * guarded by a mutex.
 */

template<class Key, class Value>
class AFK_MapCache: public AFK_Cache<Key, Value>
{
protected:
    boost::unordered_map<Key, Value> map;
    boost::mutex mut;

public:
    AFK_MapCache() {}
    virtual ~AFK_MapCache() {}

    virtual size_t size() const
    {
        return map.size();
    }

    virtual Value& operator[](const Key& key)
    {
        boost::unique_lock<boost::mutex> lock(mut);
        return map[key];
    }

#if 0
    virtual bool findDuplicate(const Key& key, const Value& value, Value& o_duplicateValue)
    {
        /* mapCache is uniquely keyed. */
        return false;
    }
#endif

    virtual void printEverything(std::ostream& os) const
    {
        for (typename boost::unordered_map<Key, Value>::const_iterator mapIt = map.begin();
            mapIt != map.end(); ++mapIt)
        {
            std::pair<Key, Value> entry = *mapIt;
            os << entry.first << " -> " << entry.second << std::endl;
        }
    }

    virtual void printStats(std::ostream& os, const std::string& prefix) const
    {
    }
};

#endif /* _AFK_DATA_MAP_CACHE_H_ */

