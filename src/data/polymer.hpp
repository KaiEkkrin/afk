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

#include <assert.h>
#include <exception>
#include <sstream>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include "../debug.hpp"
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
 * - KeyType: should be copy constructable, assignable, comparable
 * (with ==) and hashable (with a non-member hash_value() function,
 * `size_t hash_value(const KeyType&)'.
 * - MonomerType: needs to have a boost::atomic<KeyType> field `key' that the
 * Polymer can access *and that is initialised to the unassigned
 * value when the MonomerType is constructed*
 * - The Polymer also needs to be built with a default KeyType that
 * can be used as the "nothing here" value.
 */

/* A forward declaration or two */
template<typename KeyType, typename MonomerType, const KeyType& unassigned, bool debug>
class AFK_PolymerChain;

template<typename KeyType, typename MonomerType, const KeyType& unassigned, bool debug>
std::ostream& operator<<(std::ostream& os, const AFK_PolymerChain<KeyType, MonomerType, unassigned, debug>& _chain);

/* The hash map is stored internally as these chains of
 * links to MonomerType.  When too much contention is deemed to be
 * going on, a new block is added.
 */
template<typename KeyType, typename MonomerType, const KeyType& unassigned, bool debug>
class AFK_PolymerChain
{
protected:
    typedef AFK_PolymerChain<KeyType, MonomerType, unassigned, debug> PolymerChain;

    // TODO This can't work as a std::vector because of the need
    // to move chain elements.  But, it might work as a std::array,
    // so long as those move constructors get called and refreshed
    // values for every field get committed at create time.
    // Try.
    // The Polymer will now need to be templated with hashBits;
    // sensible values appear to be
    // world: 22 or above
    // landscape and others: maybe 16.
    std::vector<MonomerType> chain;
    boost::atomic<PolymerChain*> nextChain;

    /* Various parameters (replicated from below) */
    const unsigned int hashBits;
#define CHAIN_SIZE (1u<<hashBits)
#define HASH_MASK ((1u<<hashBits)-1)

    /* What position in the sequence we appear to be.  Used for
     * swizzling the chain offset around so that different chains
     * come out different.
     */
    unsigned int index;

    /* Calculates the chain index for a given hash. */
    size_t chainOffset(unsigned int hops, size_t hash) const
    {
        size_t offset = ((hash + hops) & HASH_MASK);
        return offset;
    }
    
public:
    AFK_PolymerChain(
        const unsigned int _hashBits):
            nextChain (nullptr), hashBits (_hashBits), index (0)
    {
        for (unsigned int i = 0; i < CHAIN_SIZE; ++i)
            chain.push_back(MonomerType());
    }

    virtual ~AFK_PolymerChain()
    {
        PolymerChain *next = nextChain.exchange(nullptr);
        if (next) delete next;
    }
    
    /* Appends a new chain. */
    void extend(PolymerChain *newChain, unsigned int _index)
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
    bool get(unsigned int hops, size_t baseHash, const KeyType& key, MonomerType **o_monomerPtr)
    {
        size_t offset = chainOffset(hops, baseHash);
        KeyType found = chain[offset].key.load();
        if (found == key)
        {
            *o_monomerPtr = chain.data() + offset;
            return true;
        }
        else
        {
            return false;
        }
    }
    
    /* Inserts a monomer into a specific place.
     * Returns true if successful, else false.
     * `o_monomer' gets a reference to the monomer.
     */
    bool insert(unsigned int hops, size_t baseHash, const KeyType& key, MonomerType **o_monomerPtr)
    {
        size_t offset = chainOffset(hops, baseHash);
        KeyType expected = unassigned;
        if (chain[offset].key.compare_exchange_strong(expected, key))
        {
            *o_monomerPtr = chain.data() + offset;
            return true;
        }
        else
        {
            return false;
        }
    }

    /* Returns the next chain, or nullptr if we're at the end. */
    PolymerChain *next(void) const
    {
        PolymerChain *nextCh = nextChain.load();
        return nextCh;
    }

    unsigned int getIndex(void) const
    {
        return index;
    }

    unsigned int getCount(void) const
    {
        PolymerChain *next = nextChain.load();
        if (next)
            return 1 + next->getCount();
        else
            return 1;
    }

    /* Methods for supporting direct-slot access. */

    bool atSlot(size_t slot, MonomerType **o_monomerPtr)
    {
        if (slot & ~HASH_MASK)
        {
            PolymerChain *next = nextChain.load();
            if (next)
                return next->atSlot(slot - CHAIN_SIZE, o_monomerPtr);
            else
                return false;
        }
        else
        {
            *o_monomerPtr = chain.data() + slot;
            return true;
        }
    }

    bool eraseSlot(size_t slot, const KeyType& key)
    {
        if (slot & ~HASH_MASK)
        {
            PolymerChain *next = nextChain.load();
            if (next)
                return next->eraseSlot(slot - CHAIN_SIZE, key);
            else
                return false;
        }
        else
        {
            KeyType expected = key;
            if (chain[slot].key.compare_exchange_strong(expected, unassigned))
            {
                if (debug)
                    AFK_DEBUG_PRINTL(*this << " eraseSlot(): " << key << " (offset " << slot << ") erased")
                return true;
            }
            else return false;
        }
    }

    friend std::ostream& operator<< <KeyType, MonomerType, unassigned, debug>(std::ostream& os, const PolymerChain& _chain);
};

template<typename KeyType, typename MonomerType, const KeyType& unassigned, bool debug>
std::ostream& operator<<(std::ostream& os, const AFK_PolymerChain<KeyType, MonomerType, unassigned, debug>& _chain)
{
    os << "PolymerChain(index=" << std::dec << _chain.index << ", addr=" << std::hex << _chain.chain.data() << ")";
    return os;
}

template<typename KeyType, typename MonomerType, typename Hasher, const KeyType& unassigned, bool debug>
class AFK_Polymer {
protected:
    typedef AFK_PolymerChain<KeyType, MonomerType, unassigned, debug> PolymerChain;
    PolymerChain *chains;

    /* These values define the behaviour of this polymer. */
    const unsigned int hashBits; /* Each chain is 1<<hashBits long */
    const unsigned int targetContention; /* The contention level at which we make a new chain */

    /* The hasher to use. */
    Hasher hasher;

    /* Analysis */
    AFK_StructureStats stats;

    /* This wrings as many bits out of a hash as I can
     * within the `hashBits' limit
     */
    size_t wring(size_t hash) const
    {
        size_t wrung = 0;
        for (size_t hOff = 0; hOff < (sizeof(size_t) * 8); hOff += hashBits)
            wrung ^= (hash >> hOff);

        return (wrung & HASH_MASK);
    }

    /* Adds a new chain, returning a pointer to it. */
    PolymerChain *addChain()
    {
        /* TODO Make this have prepared one already (in a different
         * thread), because making a new chain is slow
         */
        PolymerChain *newChain =
            new PolymerChain(hashBits);
        chains->extend(newChain, 0);
        return newChain;
    }

    /* Retrieves an existing monomer. */
    bool retrieveMonomer(const KeyType& key, size_t hash, MonomerType **o_monomerPtr)
    {
        /* Try a small number of hops first, then expand out.
         */
        for (unsigned int hops = 0; hops < targetContention; ++hops)
        {
            for (PolymerChain *chain = chains;
                chain; chain = chain->next())
            {
                if (chain->get(hops, hash, key, o_monomerPtr)) return true;
            }
        }

        return false;
    }

    /* Inserts a new monomer, creating a new chain
     * if necessary.
     */
    void insertMonomer(const KeyType& key, size_t hash, MonomerType **o_monomerPtr)
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
                    inserted = chain->insert(hops, hash, key, o_monomerPtr);

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
    AFK_Polymer(unsigned int _hashBits, unsigned int _targetContention, Hasher _hasher):
        hashBits (_hashBits), targetContention (_targetContention), hasher (_hasher)
    {
        /* Start off with just one chain. */
        chains = new PolymerChain(_hashBits);
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

    /* Returns a pointer to a map entry.  Throws AFK_PolymerOutOfRange
     * if it can't find it.
     */
    MonomerType *get(const KeyType& key)
    {
        size_t hash = wring(hasher(key));
        MonomerType *monomer = nullptr;
        if (!retrieveMonomer(key, hash, &monomer)) throw AFK_PolymerOutOfRange();
        return monomer;
    }

    /* Returns a pointer to a map entry.  Inserts a new one if
     * it couldn't find one.
     * This will occasionally generate duplicates.  That should be
     * okay.
     */
    MonomerType *insert(const KeyType& key)
    {
        size_t hash = wring(hasher(key));
        MonomerType *monomer = nullptr;

        /* I'm going to assume it's probably there already, and first
         * just do a basic search.
         */
        if (!retrieveMonomer(key, hash, &monomer))
        {
            /* Make a new one. */
            insertMonomer(key, hash, &monomer);
        }

        return monomer;
    }

    /* For accessing the chain slots directly.  Use carefully (it's really
     * just for the eviction thread).
     * Here chain slots are numbered 0 to (CHAIN_SIZE * chain count).
     */
    size_t slotCount() const
    {
        return chains->getCount() * CHAIN_SIZE;
    }

    bool getSlot(size_t slot, MonomerType **o_monomerPtr)
    {
        /* Don't return unassigneds */
        return (chains->atSlot(slot, o_monomerPtr) &&
            (*o_monomerPtr)->key.load() != unassigned);
    }

    /* Removes from a slot so long as it contains the key specified
     * (previously retrieved with atSlot() ).  Returns true if it
     * removed something, else false.
     * After you've successfully done this with a monomer retrieved
     * by atSlot(), you can free heap pointers in the monomer, and
     * suchlike.
     * *DON'T HAVE MORE THAN ONE THREAD CALLING THIS PER POLYMER*
     */
    bool eraseSlot(size_t slot, const KeyType& key)
    {
        bool success = chains->eraseSlot(slot, key);
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

