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

#ifndef _AFK_DATA_CHAIN_LINK_TEST_H_
#define _AFK_DATA_CHAIN_LINK_TEST_H_

#include <sstream>

#include "chain.hpp"
#include "claimable_locked.hpp"
#include "claimable_volatile.hpp"

/* So here's what we're going to do:
* - At each step, each thread draws a (small) random number.  It
* pulls the chain link corresponding to that number.
* - The cap for the random chain link number increases with each
* iteration, so that I continue creating links after the first few
* iterations.
* - Once a thread has a random chain link, it seeds the RNG with
* the index of that link, and generates a large random number.  If
* the chain link has been used before, it increments the usage count
* and iterates the RNG usage number of times before settling on its
* new number.
* - Then, it fills out the rest of the structure with the result of
* dividing the chosen number by 2, 3, 4, 5, ... before continuing.
* - The verifier iterates through the chain links.  For each one,
* it checks that:
* - The random number was generated by the RNG seeded by the chain
* link index, iterated the correct number of times
* - The quotients are correct.
*/

/* TODO: So.  So far, with the chain link test, I've basically just
 * demonstrated that AFK_Chain fails completely as soon as I get even
 * a moderate level of contention -- it spins constantly with the
 * threads competing for extend().  So instead, I want to replace it with
 * a mutex-protected std::deque<std::shared_ptr<thing> > and then try it
 * out, with lots of iterations, both VolatileClaimable and LockedClaimable,
 * and hopefully make sure that both of those are OK or fix them if they
 * aren't...  (in fact I'd be happier if I broke one or both of them)
 */

#define AFK_CLTL_QUOTIENT_COUNT 6
#define AFK_CLTL_LOCKED_CLAIMABLE 1

class AFK_ChainLinkTestLink
{
protected:
    uint32_t rnum;
    uint32_t usageCount;
    uint32_t quotients[AFK_CLTL_QUOTIENT_COUNT];

public:
    AFK_ChainLinkTestLink();

    void test(int index);
    bool verify(int index) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_ChainLinkTestLink& link);
};

std::ostream& operator<<(std::ostream& os, const AFK_ChainLinkTestLink& link);

#if AFK_CLTL_LOCKED_CLAIMABLE
typedef AFK_LockedClaimable<AFK_ChainLinkTestLink> AFK_ClaimableChainLinkTestLink;
#else
typedef AFK_VolatileClaimable<AFK_ChainLinkTestLink> AFK_ClaimableChainLinkTestLink;
#endif

typedef AFK_Chain<AFK_ClaimableChainLinkTestLink> AFK_ChainLinkTestChain;

void afk_testChainLink_worker(int threadId, int64_t rngSeed, int iterations, int maxChainLength, AFK_ChainLinkTestChain *testChain);

/* Returns the number of failures seen. */
int afk_testChainLink(void);

#endif /* _AFK_DATA_CHAIN_LINK_TEST_H_ */
