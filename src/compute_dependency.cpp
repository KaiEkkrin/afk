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

#include <algorithm>
#include <cassert>

#include "compute_dependency.hpp"

void AFK_ComputeDependency::expandEvents(int newMaxEventCount)
{
    if (useEvents)
    {
        /* Make a gravestone for my old, defunct self: */
        AFK_ComputeDependency gravestone = std::move(*this);

        /* Re-initialise myself */
        events = std::make_shared<std::vector<cl_event> >(newMaxEventCount);
        eventCount = gravestone.eventCount;
        computer = gravestone.computer;
        assert(useEvents);

        /* Stick that stone in the graveyard */
        graveyard.push_front(std::move(gravestone));
    }
}

void AFK_ComputeDependency::clearEvents(void)
{
    if (useEvents)
    {
        /* Release everything I had */
        for (int i = 0; i < eventCount; ++i)
        {
            computer->oclShim.ReleaseEvent()(events->at(i));
            events->at(i) = 0;
        }

        /* The graveyard gets released too */
        graveyard.clear();
    }
}

AFK_ComputeDependency::AFK_ComputeDependency(AFK_Computer *_computer):
    eventCount(0),
    computer(_computer),
    useEvents(_computer->getUseEvents())
{
    if (useEvents)
    {
        events = std::make_shared<std::vector<cl_event> >(AFK_CDEP_SMALLEST);
    }
}

AFK_ComputeDependency::AFK_ComputeDependency(const AFK_ComputeDependency& _dep):
    events(_dep.events),
    eventCount(_dep.eventCount),
    computer(_dep.computer),
    useEvents(_dep.useEvents)
{
    if (useEvents)
    {
        /* I need to retain all these events I just gained a reference to... */
        for (int i = 0; i < eventCount; ++i)
        {
            computer->oclShim.RetainEvent()(events->at(i));
        }

        /* ...and I need to keep the graveyard, too */
        std::copy(_dep.graveyard.begin(), _dep.graveyard.end(), graveyard.begin());
    }
}

AFK_ComputeDependency& AFK_ComputeDependency::operator=(const AFK_ComputeDependency& _dep)
{
    assert(_dep.useEvents == useEvents);

    /* Assignment replaces whatever was here, of course */
    clearEvents();

    computer = _dep.computer;
    if (useEvents)
    {
        /* Copy and reference everything in _dep */
        events = _dep.events;
        eventCount = _dep.eventCount;

        for (int i = 0; i < eventCount; ++i)
        {
            computer->oclShim.RetainEvent()(events->at(i));
        }

        std::copy(_dep.graveyard.begin(), _dep.graveyard.end(), graveyard.begin());
    }

    return *this;
}

AFK_ComputeDependency::AFK_ComputeDependency(AFK_ComputeDependency&& _dep):
    events(std::move(_dep.events)),
    eventCount(_dep.eventCount),
    computer(_dep.computer),
    useEvents(_dep.useEvents)
{
    if (useEvents)
    {
        /* I can just invalidate the dependency, it won't
         * do any releases on delete and all will be happy
         */
        _dep.eventCount = -1;

        /* ...and I can move the graveyard. */
        std::move(_dep.graveyard.begin(), _dep.graveyard.end(), graveyard.begin());
    }
}

AFK_ComputeDependency& AFK_ComputeDependency::operator=(AFK_ComputeDependency&& _dep)
{
    assert(_dep.useEvents == useEvents);

    /* Like the other assignment operator: */
    clearEvents();

    computer = _dep.computer;
    if (useEvents)
    {
        events = std::move(_dep.events);
        eventCount = _dep.eventCount;

        /* Again, I can just invalidate _dep */
        _dep.eventCount = -1;

        std::move(_dep.graveyard.begin(), _dep.graveyard.end(), graveyard.begin());
    }

    return *this;
}

AFK_ComputeDependency& AFK_ComputeDependency::operator+=(const AFK_ComputeDependency& _dep)
{
    /* Make sure these are compatible */
    assert(computer == _dep.computer);
    assert(useEvents == _dep.useEvents);

    if (useEvents)
    {
        assert(eventCount >= 0);
        assert(_dep.eventCount >= 0);

        int combinedEventCount = eventCount + _dep.eventCount;
        assert(combinedEventCount >= 0);
        if (static_cast<size_t>(combinedEventCount) > events->size())
        {
            /* Try to hold to the doubling pattern, rather than
             * making a new array just big enough, which would no
             * doubt cause me to run out of space again right away
             */
            int newMaxEventCount = events->size();
            while (newMaxEventCount < combinedEventCount) newMaxEventCount *= 2;

            expandEvents(newMaxEventCount);
        }

        /* Retain the other events, and splice them in... */
        for (int i = 0; i < _dep.eventCount; ++i)
        {
            computer->oclShim.RetainEvent()(_dep.events->at(i));
            events->at(eventCount + i) = _dep.events->at(i);
        }

        eventCount += _dep.eventCount;

        /* ...and keep the other's graveyard */
        graveyard.insert(graveyard.begin(), _dep.graveyard.begin(), _dep.graveyard.end());
    }

    return *this;
}

AFK_ComputeDependency::~AFK_ComputeDependency()
{
    if (useEvents) clearEvents();
}

cl_event *AFK_ComputeDependency::addEvent(void)
{
    if (useEvents)
    {
        assert(eventCount >= 0 && static_cast<size_t>(eventCount) <= events->size());
        if (static_cast<size_t>(eventCount) == events->size())
        {
            /* Make some more space. */
            expandEvents(events->size() * 2);
        }

        assert(static_cast<size_t>(eventCount) < events->size());

        /* Give back the next event. */
        return &events->at(eventCount++);
    }
    else return nullptr;
}

cl_uint AFK_ComputeDependency::getEventCount(void) const
{
    if (useEvents)
    {
        assert(eventCount >= 0);
        return static_cast<cl_uint>(eventCount);
    }
    else return 0;
}

const cl_event *AFK_ComputeDependency::getEvents(void) const
{
    if (useEvents)
    {
        assert(eventCount >= 0);
        return (eventCount > 0) ? events->data() : nullptr;
    }
    else return nullptr;
}

void AFK_ComputeDependency::waitFor(void)
{
    if (useEvents)
    {
        assert(eventCount >= 0);
        if (eventCount > 0)
        {
            AFK_CLCHK(computer->oclShim.WaitForEvents()(getEventCount(), getEvents()))
        }

        clearEvents();
    }
}

