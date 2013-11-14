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

    /* Returns the value at this key. */
    virtual Value& get(unsigned int threadId, const Key& key) = 0;

    /* Returns the value at this key, or inserts a new one and
     * returns that if there wasn't one.
     */
    virtual Value& insert(unsigned int threadId, const Key& key) = 0;

    /* TODO: I need a way of analysing the duplication rate.  Right now,
     * there isn't anything.
     */

    virtual void printStats(std::ostream& os, const std::string& prefix) const = 0;
};

#endif /* _AFK_DATA_CACHE_H_ */

