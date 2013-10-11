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

#ifndef _AFK_DATA_STATS_H_
#define _AFK_DATA_STATS_H_

#include <sstream>

#include <boost/atomic.hpp>

/* Useful tracking stats module for the async structures. */

class AFK_StructureStats
{
protected:
    /* The number of things in use in the structure. */
    boost::atomic<unsigned int> size;

    /* Accumulated contention: the number of times we
     * had to retry to slot a new thing.
     */
    boost::atomic<unsigned int> contention;

    /* The number of things that have been accumulated into
     * the `contention' value.  Reset to re-begin
     * contention sampling
     */
    boost::atomic<unsigned int> contentionSampleSize;

public:
    AFK_StructureStats();

    void insertedOne(unsigned int tries);
    void erasedOne(void);
    size_t getSize(void) const;
    unsigned int getContentionAndReset(void);
    void printStats(std::ostream& os, const std::string& prefix) const;
};

#endif /* _AFK_DATA_STATS_H_ */

