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

#include <cassert>

#include "thread_allocation.hpp"

AFK_ThreadAllocation::AFK_ThreadAllocation():
    next(0)
{
}

unsigned int AFK_ThreadAllocation::getNewId(void)
{
    assert(next < 63);
    unsigned int id = next++;
    return id;
}

unsigned int AFK_ThreadAllocation::getMaxNewIds(void) const
{
    return (62 - next);
}

