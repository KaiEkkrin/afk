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

#ifndef _AFK_COMPUTE_DEPENDENCY_H_
#define _AFK_COMPUTE_DEPENDENCY_H_

#include "afk.hpp"

#include <vector>

#include "computer.hpp"

/* This class wraps the cl_event lists typically passed around by
 * OpenCL functions, giving a way of expressing how compute operations
 * depend on each other.
 * It assumes it pertains to a single queue and a single host thread.
 */

class AFK_ComputeDependency
{
protected:
    std::vector<cl_event> e;
    AFK_Computer *computer;
    const bool useEvents;

    void clearEvents(void);
    void copyEvents(const std::vector<cl_event>& _e);

public:
    AFK_ComputeDependency(AFK_Computer *_computer);
    AFK_ComputeDependency(const AFK_ComputeDependency& _dep);
    AFK_ComputeDependency& operator=(const AFK_ComputeDependency& _dep);
    AFK_ComputeDependency& operator+=(const AFK_ComputeDependency& _dep);

    virtual ~AFK_ComputeDependency();

    /* Pushes a new event into this dependency and returns
     * the cl_event pointer (so that an OpenCL function can
     * fill it out).
     */
    cl_event *addEvent(void);

    /* Accesses the events, so that they can be waited for. */
    cl_uint getEventCount(void) const;
    const cl_event *getEvents(void) const;

    /* Handy wrapper -- wait for these events and clear them. */
    void waitFor(void);
};

#endif /* _AFK_COMPUTE_DEPENDENCY_H_ */

