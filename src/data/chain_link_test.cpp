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

#include <deque>
#include <iostream>
#include <memory>
#include <thread>
#include <boost/random/random_device.hpp>

#include "chain_link_test.hpp"
#include "../rng/boost_taus88.hpp"

AFK_ChainLinkTestLink::AFK_ChainLinkTestLink() :
rnum(0), usageCount(0)
{
    for (int i = 0; i < AFK_CLTL_FACTOR_COUNT; ++i)
    {
        factors[i] = 0;
    }
}

void AFK_ChainLinkTestLink::test(int index)
{
    AFK_Boost_Taus88_RNG rng;
    rng.seed(AFK_RNG_Value(0, index, ++usageCount));
    
    for (uint32_t it = 0; it < usageCount; ++it)
    {
        rnum = rng.uirand();
    }

    if (rnum == 0)
    {
        /* Yes, this special case is the same as the uninitialised case.
         * It's okay, it'll be rare
         */
        for (int i = 0; i < AFK_CLTL_FACTOR_COUNT; ++i)
        {
            factors[i] = 0;
        }
    }
    else
    {
        uint32_t tryDiv = 1;
        for (int i = 0; i < AFK_CLTL_FACTOR_COUNT; ++i)
        {
            if (tryDiv >= rnum)
            {
                /* This is never going to produce any more even factors,
                 * just propagate the last number
                 */
                factors[i] = (i == 0 ? 1 : factors[i - 1]);
            }
            else
            {
                while ((rnum % tryDiv) != 0) ++tryDiv;
                factors[i] = rnum / tryDiv;
            }

            ++tryDiv;
        }
    }
}

bool AFK_ChainLinkTestLink::verify(int index) const
{
    bool ok;

    if (usageCount == 0)
    {
        /* Unused, expect all zeroes. */
        ok = (rnum == 0);
        for (int i = 0; i < AFK_CLTL_FACTOR_COUNT; ++i)
        {
            ok &= (factors[i] == 0);
        }
    }
    else
    {
        AFK_Boost_Taus88_RNG rng;
        rng.seed(AFK_RNG_Value(0, index, usageCount));

        uint32_t expectedRNum = 0;
        for (uint32_t it = 0; it < usageCount; ++it)
        {
            expectedRNum = rng.uirand();
        }

        ok = (rnum == expectedRNum);
        for (int i = 0; i < AFK_CLTL_FACTOR_COUNT; ++i)
        {
            ok &= ((rnum % factors[i]) == 0);
        }
    }

    return ok;
}

std::ostream& operator<<(std::ostream& os, const AFK_ChainLinkTestLink& link)
{
    os << "Link(rnum=" << link.rnum << ", usageCount=" << link.usageCount << ",";
    for (int i = 0; i < AFK_CLTL_FACTOR_COUNT; ++i)
    {
        os << "factors[" << i << "]=" << link.factors[i] << ", ";
    }

    os << ")";
    return os;
}

void afk_testChainLink_worker(int threadId, int64_t rngSeed, int iterations, int maxChainLength, AFK_ChainLinkTestChain *testChain)
{
    AFK_Boost_Taus88_RNG rng;
    rng.seed(AFK_RNG_Value(0, threadId, rngSeed));

    for (int i = 0; i < iterations; ++i)
    {
        float fMaxChainLength = static_cast<float>(maxChainLength);
        float longestChainWanted = fMaxChainLength * static_cast<float>(i + 1) / static_cast<float>(iterations);
        int index = static_cast<int>(
            fmod(rng.frand() * fMaxChainLength, longestChainWanted));

        AFK_ClaimableChainLinkTestLink *link = testChain->lengthen(index);
        {
            auto linkClaim = link->claim(threadId, AFK_CL_SPIN);
            linkClaim.get().test(index);
            std::cout << threadId;
        }
    }

    std::cout << std::endl;
}

void afk_testChainLink(void)
{
    boost::random::random_device rdev;
    int64_t rngSeed = (static_cast<int64_t>(rdev()) |
        (static_cast<int64_t>(rdev())) << 32);

    const int iterations = 10;
    const int maxChainLength = 3;

    std::shared_ptr<AFK_BasicLinkFactory<AFK_ClaimableChainLinkTestLink> > linkFactory =
        std::make_shared<AFK_BasicLinkFactory<AFK_ClaimableChainLinkTestLink> >();
    AFK_ChainLinkTestChain *testChain = new AFK_ChainLinkTestChain(linkFactory);

    /* 24 workers should be a reasonable number, right? :P */
    std::deque<std::thread*> workers;
    for (int i = 0; i < 2; ++i)
    {
        std::thread *worker = new std::thread(
            afk_testChainLink_worker,
            i + 1,
            rngSeed,
            iterations,
            maxChainLength,
            testChain
            );
        workers.push_back(worker);
    }

    for (auto worker : workers)
    {
        worker->join();
        delete worker;
    }

    /* Verify that whole thing */
    int index = 0;
    int fails = 0;
    for (AFK_ChainLinkTestChain *chain = testChain; chain; chain = chain->next())
    {
        std::cout << "verify test chain link: index " << index << ": ";
        
        auto claim = chain->get()->claim(1, AFK_CL_SPIN);
        std::cout << claim.getShared();
        if (claim.getShared().verify(index))
        {
            std::cout << " (verify ok)" << std::endl;
        }
        else
        {
            std::cout << " (verify FAILED)" << std::endl;
            ++fails;
        }

        ++index;
    }

    delete testChain;
    std::cout << "Chain link test finished with " << iterations << "iterations, " << fails << " fails." << std::endl;
}
