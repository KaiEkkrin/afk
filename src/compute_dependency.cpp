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

#include "afk.hpp"

#include <cassert>

#include "compute_dependency.hpp"

void AFK_ComputeDependency::clearEvents(void)
{
    for (auto ev : e)
    {
        AFK_CLCHK(computer->oclShim.ReleaseEvent()(ev))
    }
    e.clear();
}

void AFK_ComputeDependency::copyEvents(const std::vector<cl_event>& _e)
{
    e.reserve(e.size() + _e.size());
    for (auto ev : _e)
    {
        AFK_CLCHK(computer->oclShim.RetainEvent()(ev))
        e.push_back(ev);
    }
}

AFK_ComputeDependency::AFK_ComputeDependency(AFK_Computer *_computer):
    computer(_computer),
    async(_computer->useAsync())
{
}

AFK_ComputeDependency::AFK_ComputeDependency(const AFK_ComputeDependency& _dep):
    computer(_dep.computer),
    async(_dep.async)
{
    if (async)
    {
        clearEvents();
        copyEvents(_dep.e);
    }
}

AFK_ComputeDependency& AFK_ComputeDependency::operator=(const AFK_ComputeDependency& _dep)
{
    assert(computer == _dep.computer);
    assert(async == _dep.async);
    if (async)
    {
        clearEvents();
        copyEvents(_dep.e);
    }

    return *this;
}

AFK_ComputeDependency& AFK_ComputeDependency::operator+=(const AFK_ComputeDependency& _dep)
{
    assert(computer == _dep.computer);
    assert(async == _dep.async);
    if (async) copyEvents(_dep.e);
    return *this;
}

AFK_ComputeDependency::~AFK_ComputeDependency()
{
    if (async) clearEvents();
}

cl_event *AFK_ComputeDependency::addEvent(void)
{
    if (async)
    {
        size_t oldSize = e.size();
        e.push_back(0);
        return &e[oldSize];
    }
    else return nullptr;
}

unsigned int AFK_ComputeDependency::getEventCount(void) const
{
    if (async) return e.size();
    else return 0;
}

const cl_event *AFK_ComputeDependency::getEvents(void) const
{
    if (async && e.size() > 0) return &e[0];
    else return nullptr;
}

void AFK_ComputeDependency::waitFor(void)
{
    if (async && e.size() > 0)
    {
        AFK_CLCHK(computer->oclShim.WaitForEvents()(e.size(), &e[0]))
        clearEvents();
    }
}
