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

#ifndef _AFK_DATA_CHAIN_H_
#define _AFK_DATA_CHAIN_H_


#include <deque>
#include <functional>
#include <memory>
#include <mutex>

#include "data.hpp"

/* A Chain is a simple dynamically-growing linked list of pointers to
 * things.
 * The user needs to provide concurrency control for the things themselves.
 * TODO: Change the Fair to be a flipping pair of these, I think.
 */

template<typename Link>
class AFK_BasicLinkFactory
{
public:
    std::shared_ptr<Link> operator()() const
    {
        return std::make_shared<Link>();
    }
};

template<typename Link, typename LinkFactory = AFK_BasicLinkFactory<Link> >
class AFK_Chain
{
protected:
    mutable std::mutex mut;

    std::shared_ptr<LinkFactory> linkFactory;
    std::deque<std::shared_ptr<Link> > chain;

public:
    AFK_Chain(std::shared_ptr<LinkFactory> _linkFactory, unsigned int startingCount = 0):
        linkFactory(_linkFactory)
    {
        std::unique_lock<std::mutex> lock(mut);

        for (unsigned int s = 0; s < startingCount; ++s)
            chain.push_back((*linkFactory)());
    }

    std::shared_ptr<Link> at(unsigned int index) const
    {
        std::unique_lock<std::mutex> lock(mut);

        return (index < chain.size() ? chain.at(index) : std::shared_ptr<Link>());
    }

    std::shared_ptr<Link> lengthen(unsigned int index)
    {
        std::unique_lock<std::mutex> lock(mut);

        while (chain.size() <= index) chain.push_back((*linkFactory)());
        return chain.at(index);
    }

    // Convenient, but holds on to the lock while the function is called,
    // so don't use this for stuff that takes a long time!
    template<typename Function>
    void foreach(Function function, unsigned int startIndex = 0)
    {
        std::unique_lock<std::mutex> lock(mut);

        for (auto it = chain.begin() + startIndex; it != chain.end(); ++it)
            function(*it);
    }
};

#endif /* _AFK_DATA_CHAIN_H_ */
