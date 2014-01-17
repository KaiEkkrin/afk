/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#include "stats.hpp"

AFK_StructureStats::AFK_StructureStats()
{
    size.store(0);
    contention.store(0);
    contentionSampleSize.store(0);
}

void AFK_StructureStats::insertedOne(unsigned int tries)
{
    /* Right now this doesn't guard against concurrent
     * access of separate fields, but I don't think I really
     * need that level of accuracy (?)
     */
    size.fetch_add(1);
    contention.fetch_add(tries);
    contentionSampleSize.fetch_add(1);
}

void AFK_StructureStats::erasedOne(void)
{
    size.fetch_sub(1);
}

size_t AFK_StructureStats::getSize(void) const
{
    return size.load();
}

unsigned int AFK_StructureStats::getContentionAndReset(void)
{
    unsigned int oldContention = contention;
    unsigned int oldContentionSampleSize = contentionSampleSize;

    contentionSampleSize.store(0);
    contention.store(0);

    return oldContentionSampleSize == 0 ? 0 : (oldContention / oldContentionSampleSize);
}

void AFK_StructureStats::printStats(std::ostream& os, const std::string& prefix) const
{
    os << prefix << ": Size: " << size << std::endl;
    os << prefix << ": Contention: " << (
        contentionSampleSize == 0 ? 0.0f : ((float)contention / (float)contentionSampleSize)) << std::endl;
}

