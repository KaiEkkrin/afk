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

#ifndef _AFK_DATA_POLYMER_H_
#define _AFK_DATA_POLYMER_H_

#include <array>
#include <cassert>
#include <exception>
#include <sstream>

#include <boost/atomic.hpp>

#include "claimable.hpp"
#include "data.hpp"
#include "monomer.hpp"
#include "stats.hpp"

/* The get() function throws this when there's nothing at the
 * requested cell.
 */
class AFK_PolymerOutOfRange: public std::exception {};

/* This is an atomically accessible hash map.  It should be quick to
 * add, retrieve and delete, cope with a lot of value turnover, and
 * be able to iterate through the entries (including randomly, with
 * certainty of hitting each entry once).  It's okay for it to end
 * up including duplicate entries if several threads try to add the
 * same one at once; I'm just going to assume they won't do this
 * very often.
 *
 * It's called "polymer" because it's a chain of atoms...  err, yeah.
 */

/* An entry looks like this.  We need the value for the obvious
 * storage reason, and the key in order to retrieve entries that
 * have had a hash conflict.
 *
 * Requirements are:
 * - KeyType: should be copy constructable and assignable, *volatile* comparable
 * (with ==) and hashable (with a non-member hash_value() function,
 * `size_t hash_value(const KeyType&)'.
 * - ValueType: needs to have a default constructor so that the
 * default PolymerChainFactory can make a chain of monomers of them;
 * or, supply your own PolymerChainFactory
 * - The Polymer also needs to be built with a default KeyType that
 * can be used as the "nothing here" value.
 */

#define AFK_DEBUG_POLYMER 1

#if AFK_DEBUG_POLYMER
#include "../debug.hpp"
#define AFK_DEBUG_PRINTL_POLYMER(expr) if (debug) { AFK_DEBUG_PRINTL(expr) }
#else
#define AFK_DEBUG_PRINTL_POLYMER(expr)
#endif

/* A forward declaration or two */
template<typename KeyType, typename ValueType, const KeyType& unassigned, unsigned int hashBits, bool debug>
class AFK_PolymerChain;

template<typename KeyType, typename ValueType, const KeyType& unassigned, unsigned int hashBits, bool debug>
std::ostream& operator<<(std::ostream& os, const AFK_PolymerChain<KeyType, ValueType, unassigned, hashBits, debug>& _chain);

/* The hash map is stored internally as these chains of
 * links to Monomers.  When too much contention is deemed to be
 * going on, a new block is added.
 */
template<typename KeyType, typename ValueType, const KeyType& unassigned, unsigned int hashBits, bool debug>
class AFK_PolymerChain
{
protected:
    typedef AFK_PolymerChain<KeyType, ValueType, unassigned, hashBits, debug> PolymerChain;

#define CHAIN_SIZE (1u<<hashBits)
#define HASH_MASK ((1u<<hashBits)-1)

    // The Polymer will now need to be templated with hashBits;
    // sensible values appear to be
    // world: 22 or above
    // landscape and others: maybe 16.
    std::array<AFK_Monomer<KeyType, ValueType, unassigned>, CHAIN_SIZE> chain;
    boost::atomic<PolymerChain*> nextChain;

    /* What position in the sequence we appear to be.  Used for
     * swizzling the chain offset around so that different chains
     * come out different.
     */
    unsigned int index;

    /* Calculates the chain index for a given hash. */
    size_t chainOffset(unsigned int hops, size_t hash) const afk_noexcept
    {
        size_t offset = ((hash + hops) & HASH_MASK);
        return offset;
    }
    
public:
    AFK_PolymerChain():
        nextChain (nullptr), index (0)
    {
    }

    virtual ~AFK_PolymerChain()
    {
        PolymerChain *next = nextChain.exchange(nullptr);
        if (next) delete next;
    }
    
    /* Appends a new chain. */
    void extend(PolymerChain *newChain, unsigned int _index) afk_noexcept
    {
        assert(index == _index);

        PolymerChain *expected = nullptr;
        bool gotIt = nextChain.compare_exchange_strong(expected, newChain);
        if (gotIt)
        {
            newChain->index = _index + 1;
        }
        else
        {
            PolymerChain *next = nextChain.load();
            assert(next);
            next->extend(newChain, _index + 1);
        }
    }

    /* Gets a monomer from a specific place. */
    bool get(unsigned int threadId, unsigned int hops, size_t baseHash, const KeyType& key, ValueType **o_valuePtr) afk_noexcept
    {
        size_t offset = chainOffset(hops, baseHash);
        if (chain[offset].get(threadId, key, o_valuePtr))
        {
            AFK_DEBUG_PRINTL_POLYMER("key " << key << " found at offset " << offset << " (hops " << hops << ", baseHash " << baseHash << ", chain " << index)
            return true;
        }
        else return false;
    }
    
    /* Inserts a monomer into a specific place.
     * Returns true if successful, else false.
     * `o_value' gets a reference to the monomer.
     */
    bool insert(unsigned int threadId, unsigned int hops, size_t baseHash, const KeyType& key, ValueType **o_valuePtr) afk_noexcept
    {
        size_t offset = chainOffset(hops, baseHash);
        if (chain[offset].insert(threadId, key, o_valuePtr))
        {
            AFK_DEBUG_PRINTL_POLYMER("key " << key << " inserted at offset " << offset << " (hops " << hops << ", baseHash " << baseHash << ", chain " << index)
            return true;
        }
        else return false;
    }

    /* Returns the next chain, or nullptr if we're at the end. */
    PolymerChain *next(void) const afk_noexcept
    {
        PolymerChain *nextCh = nextChain.load();
        return nextCh;
    }

    unsigned int getIndex(void) const afk_noexcept
    {
        return index;
    }

    unsigned int getCount(void) const afk_noexcept
    {
        PolymerChain *next = nextChain.load();
        if (next)
            return 1 + next->getCount();
        else
            return 1;
    }

    /* Methods for supporting direct-slot access. */

    bool atSlot(unsigned int threadId, size_t slot, bool acceptUnassigned, KeyType *o_key, ValueType **o_valuePtr) afk_noexcept
    {
        if (slot & ~HASH_MASK)
        {
            PolymerChain *next = nextChain.load();
            if (next)
                return next->atSlot(threadId, slot - CHAIN_SIZE, acceptUnassigned, o_key, o_valuePtr);
            else
                return false;
        }
        else
        {
            return chain[slot].here(threadId, acceptUnassigned, o_key, o_valuePtr);
        }
    }

    bool eraseSlot(unsigned int threadId, size_t slot, const KeyType& key) afk_noexcept
    {
        if (slot & ~HASH_MASK)
        {
            PolymerChain *next = nextChain.load();
            if (next)
                return next->eraseSlot(threadId, slot - CHAIN_SIZE, key);
            else
                return false;
        }
        else
        {
            if (chain[slot].erase(threadId, key))
            {
                AFK_DEBUG_PRINTL_POLYMER("key " << key << " erased at slot " << slot << ", chain " << index)
                return true;
            }
            else return false;
        }
    }

    friend std::ostream& operator<< <KeyType, ValueType, unassigned, hashBits, debug>(std::ostream& os, const PolymerChain& _chain);
};

template<typename KeyType, typename ValueType, const KeyType& unassigned, unsigned int hashBits, bool debug>
std::ostream& operator<<(std::ostream& os, const AFK_PolymerChain<KeyType, ValueType, unassigned, hashBits, debug>& _chain)
{
    os << "PolymerChain(index=" << std::dec << _chain.index << ", addr=" << std::hex << _chain.chain.data() << ")";
    return os;
}

/* The chain factory is an extensible way of initialising a chain.
 * The basic one does very little.
 */
template<
    typename KeyType,
    typename ValueType,
    const KeyType& unassigned,
    unsigned int hashBits,
    bool debug>
class AFK_BasePolymerChainFactory
{
public:
    AFK_PolymerChain<KeyType, ValueType, unassigned, hashBits, debug> *operator()() const
    {
        return new AFK_PolymerChain<KeyType, ValueType, unassigned, hashBits, debug>();
    }
};

template<
    typename KeyType,
    typename ValueType,
    typename Hasher,
    const KeyType& unassigned,
    unsigned int hashBits,
    bool debug,
    typename ChainFactory = AFK_BasePolymerChainFactory<
        KeyType, ValueType, unassigned, hashBits, debug>
        >
class AFK_Polymer
{
public:
    typedef AFK_PolymerChain<KeyType, ValueType, unassigned, hashBits, debug> PolymerChain;

protected:
    PolymerChain *chains;

    /* These values define the behaviour of this polymer. */
    const unsigned int targetContention; /* The contention level at which we make a new chain */

    /* The hasher to use. */
    Hasher hasher;

    /* How to make new chains. */
    ChainFactory chainFactory;

    /* Analysis */
    AFK_StructureStats stats;

    /* This wrings as many bits out of a hash as I can
     * within the `hashBits' limit
     */
    size_t wring(size_t hash) const afk_noexcept
    {
        /* Now that I've fixed the hash function, this
         * is properly unnecessary.
         * TODO: However, to mitigate clashes, I ought to
         * rotate the hash by (say) 7 every time I hop
         * along a chain ...
         */
#if 0
        size_t wrung = 0;
        for (size_t hOff = 0; hOff < (sizeof(size_t) * 8); hOff += hashBits)
            wrung ^= (hash >> hOff);

        return (wrung & HASH_MASK);
#else
        return (hash & HASH_MASK);
#endif
    }

    /* Adds a new chain, returning a pointer to it. */
    PolymerChain *addChain()
    {
        /* TODO Make this have prepared one already (in a different
         * thread), because making a new chain is slow
         */
        PolymerChain *newChain = chainFactory();
        chains->extend(newChain, 0);
        return newChain;
    }

    /* Retrieves an existing monomer. */
    bool retrieveMonomer(unsigned int threadId, const KeyType& key, size_t hash, ValueType **o_valuePtr) afk_noexcept
    {
        /* Try a small number of hops first, then expand out.
         */
        for (unsigned int hops = 0; hops < targetContention; ++hops)
        {
            for (PolymerChain *chain = chains;
                chain; chain = chain->next())
            {
                if (chain->get(threadId, hops, hash, key, o_valuePtr)) return true;
            }
        }

        return false;
    }

    /* Inserts a new monomer, creating a new chain
     * if necessary.
     */
    void insertMonomer(unsigned int threadId, const KeyType& key, size_t hash, ValueType **o_valuePtr) afk_noexcept
    {
        bool inserted = false;
        PolymerChain *startChain = chains;

        while (!inserted)
        {
            for (unsigned int hops = 0; hops < targetContention && !inserted; ++hops)
            {
                for (PolymerChain *chain = startChain;
                    chain != nullptr && !inserted; chain = chain->next())
                {
                    inserted = chain->insert(threadId, hops, hash, key, o_valuePtr);

                    if (inserted) stats.insertedOne(hops);
                }
            }

            if (!inserted)
            {
                /* Add a new chain for it */
                startChain = addChain();
                //stats.getContentionAndReset();
            }
        }
    }

public:
    AFK_Polymer(unsigned int _targetContention, Hasher _hasher):
        targetContention (_targetContention), hasher (_hasher)
    {
        /* Start off with just one chain. */
        chains = chainFactory();
    }

    virtual ~AFK_Polymer()
    {
        /* Wipeout time.  I hope nobody is attempting concurrent access now.
         */
        delete chains;
    }

    size_t size() const
    {
        return stats.getSize();
    }

    /* Returns a reference to a map entry.  Throws AFK_PolymerOutOfRange
     * if it can't find it.
     */
    ValueType& get(unsigned int threadId, const KeyType& key)
    {
        size_t hash = wring(hasher(key));
        ValueType *value = nullptr;
        if (!retrieveMonomer(threadId, key, hash, &value)) throw AFK_PolymerOutOfRange();
        return *value;
    }

    /* Returns a reference to a map entry.  Inserts a new one if
     * it couldn't find one.
     * This will occasionally generate duplicates.  That should be
     * okay.
     */
    ValueType& insert(unsigned int threadId, const KeyType& key)
    {
        size_t hash = wring(hasher(key));
        ValueType *value = nullptr;

        /* I'm going to assume it's probably there already, and first
         * just do a basic search.
         */
        if (!retrieveMonomer(threadId, key, hash, &value))
        {
            /* Make a new one. */
            insertMonomer(threadId, key, hash, &value);
        }

        return *value;
    }

    /* For accessing the chain slots directly.  Use carefully (it's really
     * just for the eviction thread).
     * Here chain slots are numbered 0 to (CHAIN_SIZE * chain count).
     */
    size_t slotCount() const
    {
        return chains->getCount() * CHAIN_SIZE;
    }

    bool getSlot(unsigned int threadId, size_t slot, KeyType *o_key, ValueType **o_valuePtr) afk_noexcept
    {
        /* Don't return unassigneds */
        return (chains->atSlot(threadId, slot, false, o_key, o_valuePtr));
    }

    /* Removes from a slot so long as it contains the key specified
     * (previously retrieved with atSlot() ).  Returns true if it
     * removed something, else false.
     * After you've successfully done this with a monomer retrieved
     * by atSlot(), you can free heap pointers in the monomer, and
     * suchlike.
     * *DON'T HAVE MORE THAN ONE THREAD CALLING THIS PER POLYMER*
     */
    bool eraseSlot(unsigned int threadId, size_t slot, const KeyType& key) afk_noexcept
    {
        bool success = chains->eraseSlot(threadId, slot, key);
        if (success) stats.erasedOne();
        return success;
    }

    void printStats(std::ostream& os, const std::string& prefix) const
    {
        stats.printStats(os, prefix);
        os << prefix << ": Chain count: " << chains->getCount() << std::endl;
    }
};

#endif /* _AFK_DATA_POLYMER_H_ */

