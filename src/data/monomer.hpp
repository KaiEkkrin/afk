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

#ifndef _AFK_DATA_MONOMER_H_
#define _AFK_DATA_MONOMER_H_

#include "data.hpp"

/* The Monomer is the single unit that goes into a Polymer. */

#define AFK_MONOMER_CLAIMABLE 1

#if AFK_MONOMER_CLAIMABLE

#include "claimable_volatile.hpp"

template<typename KeyType, typename ValueType, const KeyType& unassigned>
class AFK_Monomer
{
protected:
    AFK_VolatileClaimable<KeyType> key;
    ValueType value;

public:
    AFK_Monomer(): key(), value()
    {
        auto keyClaim = key.claim(1, 0);
        keyClaim.get() = unassigned;
    }

    bool here(unsigned int threadId, bool acceptUnassigned, KeyType *o_key, ValueType **o_valuePtr) afk_noexcept
    {
        auto keyClaim = key.claim(threadId, AFK_CL_SHARED);
        if (keyClaim.isValid() &&
            (acceptUnassigned || !(keyClaim.getShared() == unassigned)))
        {
            *o_key = keyClaim.getShared();
            *o_valuePtr = &value;
            return true;
        }
        else return false;
    }

    bool get(unsigned int threadId, const KeyType& _key, ValueType **o_valuePtr) afk_noexcept
    {
        auto keyClaim = key.claimInplace(threadId, AFK_CL_SHARED);
        if (keyClaim.isValid() && keyClaim.getShared() == _key)
        {
            *o_valuePtr = &value;
            return true;
        }
        else return false;
    }

    bool insert(unsigned int threadId, const KeyType& _key, ValueType **o_valuePtr) afk_noexcept
    {
        auto keyClaim = key.claim(threadId, 0);
        if (keyClaim.isValid() && keyClaim.getShared() == unassigned)
        {
            keyClaim.get() = _key;
            *o_valuePtr = &value;
            return true;
        }
        else return false;
    }

    bool erase(unsigned int threadId, const KeyType& _key) afk_noexcept
    {
        auto keyClaim = key.claim(threadId, 0);
        if (keyClaim.isValid() && keyClaim.get() == _key)
        {
            keyClaim.get() = unassigned;
            return true;
        }
        else return false;
    }
};

#else /* AFK_MONOMER_CLAIMABLE */

#include <boost/atomic.hpp>

template<typename KeyType, typename ValueType, const KeyType& unassigned>
class AFK_Monomer
{
protected:
    boost::atomic<KeyType> key;
    ValueType value;

public:
    AFK_Monomer(): key(unassigned), value() {}

    bool here(unsigned int threadId, bool acceptUnassigned, KeyType *o_key, ValueType **o_valuePtr) afk_noexcept
    {
        if (acceptUnassigned)
        {
            *o_key = key.load();
            *o_valuePtr = &value;
            return true;
        }
        else
        {
            KeyType theKey = key.load();
            if (theKey != unassigned)
            {
                *o_key = theKey;
                *o_valuePtr = &value;
                return true;
            }
            else return false;
        }
    }

    bool get(unsigned int threadId, const KeyType& _key, ValueType **o_valuePtr) afk_noexcept
    {
        if (key.load() == _key)
        {
            *o_valuePtr = &value;
            return true;
        }
        else return false;
    }

    bool insert(unsigned int threadId, const KeyType& _key, ValueType **o_valuePtr) afk_noexcept
    {
        KeyType expected = unassigned;
        if (key.compare_exchange_strong(expected, _key))
        {
            *o_valuePtr = &value;
            return true;
        }
        else return false;
    }

    bool erase(unsigned int threadId, const KeyType& _key) afk_noexcept
    {
        KeyType expected = _key;
        return key.compare_exchange_strong(expected, unassigned);
    }
};

#endif /* AFK_MONOMER_CLAIMABLE */

#endif /* _AFK_DATA_MONOMER_H_ */

