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

#ifndef _AFK_ASYNC_THREAD_ALLOCATION_H_
#define _AFK_ASYNC_THREAD_ALLOCATION_H_

/* This module defines a way of giving out unique thread IDs that
 * fit into a uint64_t flag set, keeping the top bit reserved.
 * You should probably have only one of these around at once...
 */
class AFK_ThreadAllocation
{
protected:
    unsigned int next;

public:
    AFK_ThreadAllocation();

    /* Gets a new unique thread ID. */
    unsigned int getNewId(void);

    /* Tells you how many unique thread IDs are left. */
    unsigned int getMaxNewIds(void) const;
};

#endif /* _AFK_ASYNC_THREAD_ALLOCATION_H_ */

