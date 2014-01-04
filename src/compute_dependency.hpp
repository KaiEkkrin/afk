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

#ifndef _AFK_COMPUTE_DEPENDENCY_H_
#define _AFK_COMPUTE_DEPENDENCY_H_

#include "afk.hpp"

#include <deque>
#include <memory>
#include <vector>

#include "computer.hpp"

#define AFK_CDEP_SMALLEST 4

/* This class wraps the cl_event lists typically passed around by
 * OpenCL functions, giving a way of expressing how compute operations
 * depend on each other.
 * It assumes it pertains to a single queue and a single host thread.
 */

/* TODO: This doesn't work.  I don't truly understand why it doesn't work.
 * Shouldn't the OpenCL be copying event lists that go into clEnqueue()
 * functions anyway?  At any rate, when I enable eventing (and especially
 * when I enable multiple queues) I get use-after-free bug-outs and similar.
 * Could it be that I'm giving the OpenCL references to other things that
 * stop being valid in between the time of enqueue and the time the
 * implementation gets around to using them?  Seems likely.  But I thought
 * I cleared all of those out, so I'm not sure what.
 */

class AFK_ComputeDependency;

class AFK_ComputeDependency
{
protected:
    /* The list of events mustn't move in memory after I've used it,
     * yet I want to be able to produce a contiguous block of all
     * events.
     * I resolve this problem by starting with an array of size
     * AFK_CDEP_SMALLEST.  Every time the current array runs out
     * of space, I shunt it aside, allocate a new array
     * double its size *and copy the old array into it*.
     * When all events are waited for and cleared, I can free the
     * old array(s) (but not before).
     */

    class EventList
    {
    protected:
        /* The current event array is stored as a shared_ptr, so that
         * the backing memory will survive being arbitrarily copied about.
         * The cl_event container within is a vector because the size
         * varies between arrays and because I want a contiguous-memory
         * structure.
         */
        std::shared_ptr<std::vector<cl_event> > events;

        /* The events vector size is the maximum number of events it
         * can store, so I store the current number of events here.
         * I need to reallocate and copy whenever this hits maximum.
         * A negative value means that this compute dependency has been
         * invalidated and shouldn't be used again.
         */
        int eventCount;

        AFK_Computer *computer;

    public:
        EventList(AFK_Computer *_computer, size_t maxEventCount);

        /* See AFK_ComputeDependency constructor descriptions. */
        EventList(const EventList& _list);
        EventList& operator=(const EventList& _list);

        EventList(EventList&& _list);
        EventList& operator=(EventList&& _list);

        /* This function assumes there is room in the destination list
         * for both
         */
        EventList& operator+=(const EventList& _list);

        virtual ~EventList();

        /* See descriptions on AFK_ComputeDependency */
        cl_event *addEvent(void);
        cl_uint getEventCount(void) const;
        const cl_event *getEvents(void) const;
        void waitFor(void) const;

        bool valid(void) const;
        size_t getMaxEventCount(void) const;
        void reset(void);
    };

    EventList eventList;

    /* As dependencies outgrow their event arrays, they go into the
     * graveyard, here, and are overwritten by new ones with larger
     * event arrays.
     * The graveyard can contain multiple dependencies because of
     * the operator+= function (merge two dependencies, each of
     * which could have graveyards).
     */
    std::deque<EventList> graveyard;

    AFK_Computer *computer;
    const bool useEvents;

    /* This function chucks the current dependency into the
     * graveyard and re-makes it with the new event count,
     * below.
     */
    void expandEvents(size_t newMaxEventCount);

public:
    AFK_ComputeDependency(AFK_Computer *_computer);
    AFK_ComputeDependency(const AFK_ComputeDependency& _dep);

    /* This overwrites the current dependency with the supplied one,
     * effectively dereferencing the current one.
     */
    AFK_ComputeDependency& operator=(const AFK_ComputeDependency& _dep);

    /* The move constructor and assignment operator can avoid retain
     * calls.  Hopefully the compiler will know to use these instead
     * sometimes :)
     */
    AFK_ComputeDependency(AFK_ComputeDependency&& _dep);
    AFK_ComputeDependency& operator=(AFK_ComputeDependency&& _dep);

    /* This merges two dependencies. */
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

    /* Handy wrapper -- wait for these events. */
    void waitFor(void) const;

    /* Resets the dependency, getting rid of all current events.
     * *Don't call unless you're sure the queues are clear*
     */
    void reset(void);

};

#endif /* _AFK_COMPUTE_DEPENDENCY_H_ */

