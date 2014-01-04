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

AFK_ComputeDependency::EventList::EventList(AFK_Computer *_computer, size_t maxEventCount):
    eventCount(0),
    computer(_computer)
{
    events = std::make_shared<std::vector<cl_event> >(maxEventCount, static_cast<cl_event>(0));
}

AFK_ComputeDependency::EventList::EventList(const EventList& _list):
    events(_list.events),
    eventCount(_list.eventCount),
    computer(_list.computer)
{
    /* I need to retain a reference to all those events */
    for (int i = 0; i < eventCount; ++i)
    {
        computer->oclShim.RetainEvent()(events->at(i));
    }
}

AFK_ComputeDependency::EventList& AFK_ComputeDependency::EventList::operator=(const EventList& _list)
{
    /* This overwrites whatever was here already */
    reset();

    computer = _list.computer;
    events = _list.events;
    eventCount = _list.eventCount;

    /* Again, I need to retain all those references */
    for (int i = 0; i < eventCount; ++i)
    {
        computer->oclShim.RetainEvent()(events->at(i));
    }

    return *this;
}

AFK_ComputeDependency::EventList::EventList(EventList&& _list) :
    events(std::move(_list.events)),
    eventCount(_list.eventCount),
    computer(_list.computer)
{
    /* I can just invalidate the old list, it won't
    * do any releases on delete and all will be happy
    */
    _list.eventCount = -1;
}

AFK_ComputeDependency::EventList& AFK_ComputeDependency::EventList::operator=(EventList&& _list)
{
    /* This also overwrites whatever was here already */
    reset();

    computer = _list.computer;
    events = std::move(_list.events);
    eventCount = _list.eventCount;

    /* Again, just invalidate the old list */
    _list.eventCount = -1;

    return *this;
}

AFK_ComputeDependency::EventList& AFK_ComputeDependency::EventList::operator+=(const EventList& _list)
{
    assert(valid());

    /* There really ought to be room for both in the list */
    assert((eventCount + _list.eventCount) <= static_cast<int>(events->size()));

    /* I need to retain and copy each event in the other list */
    for (int i = 0; i < _list.eventCount; ++i)
    {
        computer->oclShim.RetainEvent()(_list.events->at(i));
        events->at(eventCount++) = _list.events->at(i);
    }

    return *this;
}

AFK_ComputeDependency::EventList::~EventList()
{
    reset();
}

cl_event *AFK_ComputeDependency::EventList::addEvent(void)
{
    assert(valid());

    /* If there's room in the list... */
    if (eventCount < static_cast<int>(events->size()))
    {
        return &events->at(eventCount++);
    }
    else return nullptr; /* the AFK_ComputeDependency will have to do an extend */
}

cl_uint AFK_ComputeDependency::EventList::getEventCount(void) const
{
    assert(valid());
    return static_cast<cl_uint>(eventCount);
}

const cl_event *AFK_ComputeDependency::EventList::getEvents(void) const
{
    assert(valid());
    return eventCount > 0 ? events->data() : nullptr;
}

void AFK_ComputeDependency::EventList::waitFor(void) const
{
    if (eventCount > 0)
    {
        AFK_CLCHK(computer->oclShim.WaitForEvents()(getEventCount(), getEvents()))
    }
}

bool AFK_ComputeDependency::EventList::valid(void) const
{
    return (eventCount >= 0);
}

size_t AFK_ComputeDependency::EventList::getMaxEventCount(void) const
{
    return events->size();
}

void AFK_ComputeDependency::EventList::reset(void)
{
    for (int i = 0; i < eventCount; ++i)
    {
        computer->oclShim.ReleaseEvent()(events->at(i));
        events->at(i) = 0;
    }

    eventCount = 0;
}

void AFK_ComputeDependency::expandEvents(size_t newMaxEventCount)
{
    if (useEvents)
    {
        /* Make a gravestone for my old, defunct event list: */
        EventList gravestone(std::move(eventList));

        /* Re-initialise my own event list as a larger one
         * containing the old event list:
         */
        eventList = std::move(EventList(computer, newMaxEventCount));
        eventList += gravestone;

        /* Stick that stone in the graveyard */
        graveyard.push_front(std::move(gravestone));
    }
}

AFK_ComputeDependency::AFK_ComputeDependency(AFK_Computer *_computer):
    eventList(_computer, AFK_CDEP_SMALLEST),
    computer(_computer),
    useEvents(_computer->getUseEvents())
{
}

AFK_ComputeDependency::AFK_ComputeDependency(const AFK_ComputeDependency& _dep):
    eventList(_dep.eventList),
    computer(_dep.computer),
    useEvents(_dep.useEvents)
{
    if (useEvents)
    {
        /* I need to keep the graveyard, too */
        std::copy(_dep.graveyard.begin(), _dep.graveyard.end(), graveyard.begin());
    }
}

AFK_ComputeDependency& AFK_ComputeDependency::operator=(const AFK_ComputeDependency& _dep)
{
    assert(_dep.useEvents == useEvents);

    computer = _dep.computer;
    if (useEvents)
    {
        eventList = _dep.eventList;
        std::copy(_dep.graveyard.begin(), _dep.graveyard.end(), graveyard.begin());
    }

    return *this;
}

AFK_ComputeDependency::AFK_ComputeDependency(AFK_ComputeDependency&& _dep):
    eventList(std::move(_dep.eventList)),
    computer(_dep.computer),
    useEvents(_dep.useEvents)
{
    if (useEvents)
    {
        /* ...and I can move the graveyard. */
        std::move(_dep.graveyard.begin(), _dep.graveyard.end(), graveyard.begin());
    }
}

AFK_ComputeDependency& AFK_ComputeDependency::operator=(AFK_ComputeDependency&& _dep)
{
    assert(_dep.useEvents == useEvents);

    computer = _dep.computer;
    if (useEvents)
    {
        eventList = std::move(_dep.eventList);
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
        cl_uint combinedEventCount = getEventCount() + _dep.getEventCount();
        if (static_cast<size_t>(combinedEventCount) > eventList.getMaxEventCount())
        {
            /* Try to hold to the doubling pattern, rather than
             * making a new array just big enough, which would no
             * doubt cause me to run out of space again right away
             */
            size_t newMaxEventCount = eventList.getMaxEventCount();
            while (newMaxEventCount < combinedEventCount) newMaxEventCount *= 2;

            expandEvents(newMaxEventCount);
        }

        /* Now, the append should work. */
        eventList += _dep.eventList;

        /* ...Also keep the other's graveyard */
        graveyard.insert(graveyard.begin(), _dep.graveyard.begin(), _dep.graveyard.end());
    }

    return *this;
}

AFK_ComputeDependency::~AFK_ComputeDependency()
{
    reset();
}

cl_event *AFK_ComputeDependency::addEvent(void)
{
    if (useEvents)
    {
        cl_event *newEvent;
        while ((newEvent = eventList.addEvent()) == nullptr)
        {
            /* I need to expand that event list */
            expandEvents(eventList.getMaxEventCount() * 2);
        }

        return newEvent;
    }
    else return nullptr;
}

cl_uint AFK_ComputeDependency::getEventCount(void) const
{
    if (useEvents)
    {
        return eventList.getEventCount();
    }
    else return 0;
}

const cl_event *AFK_ComputeDependency::getEvents(void) const
{
    if (useEvents)
    {
        return eventList.getEvents();
    }
    else return nullptr;
}

void AFK_ComputeDependency::waitFor(void) const
{
    if (useEvents)
    {
        eventList.waitFor();
    }
}

void AFK_ComputeDependency::reset(void)
{
    if (useEvents)
    {
        /* Release everything I had */
        eventList.reset();

        /* The graveyard gets released too */
        graveyard.clear();
    }
}
