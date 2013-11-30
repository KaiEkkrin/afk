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

#include <cassert>
#include <memory>

#include <boost/atomic.hpp>

/* A Chain is a simple single-linked list of atomic pointers.  It
 * provides lockless access at the expense of occasionally bumping the
 * list length a bit much!
 */

template<typename Link>
class AFK_BasicLinkFactory
{
public:
    Link *operator()() const
    {
        return new Link();
    }
};

/* The placeholder (see below). */
extern int placeholderBase;

template<typename Link, typename LinkFactory = AFK_BasicLinkFactory<Link> >
class AFK_Chain
{
protected:
    std::shared_ptr<LinkFactory> linkFactory;
    Link *const link; /* TODO make this a shared_ptr ? */
    boost::atomic<AFK_Chain*> chain;

    /* Hack: To avoid multiple allocations (the factory might be slow)
     * upon a contended chain extend, we swap in the placeholder pointer
     * first.
     * This pointer totally does not point to a real Link.
     */
    AFK_Chain *placeholder;

public:
    AFK_Chain(std::shared_ptr<LinkFactory> _linkFactory):
        linkFactory(_linkFactory), link((*_linkFactory)()), chain(nullptr)
    {
        placeholder = reinterpret_cast<AFK_Chain*>(&placeholderBase);
    }

    virtual ~AFK_Chain()
    {
        AFK_Chain *ch = chain.exchange(nullptr);
        if (ch && ch != placeholder) delete ch;
        delete link;
    }

    Link *get(void) const afk_noexcept
    {
        return link;
    }

    AFK_Chain *next(void) const afk_noexcept
    {
        AFK_Chain *nextChain;
        do
        {
            nextChain = chain.load();
        } while (nextChain == placeholder);
        return nextChain;
    }

    /* Extends an end-of-chain by one link, returning next.
     */
    AFK_Chain *extend(void)
    {
        AFK_Chain *ch = next();
        if (ch)
        {
            return ch;
        }
        else
        {
            /* Avoid proliferation, as above. */
            AFK_Chain *expected = nullptr;
            if (chain.compare_exchange_strong(expected, placeholder))
            {
                /* I won the toss, make the real new chain. */
                AFK_Chain *newChain = new AFK_Chain(linkFactory);
                while (!chain.compare_exchange_strong(placeholder, newChain));
                return newChain;
            }
            else return extend();
        }
    }

    Link *at(unsigned int index) const afk_noexcept
    {
        if (index == 0) return link;
        else
        {
            AFK_Chain *ch = next();
            if (ch) return ch->at(index - 1);
            else return nullptr;
        }
    }
};

#endif /* _AFK_DATA_CHAIN_H_ */

