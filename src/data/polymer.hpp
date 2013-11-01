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

#include <boost/atomic.hpp>
#include <boost/iterator/indirect_iterator.hpp>
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
template<typename KeyType, typename MonomerType, bool debug>
class AFK_PolymerChain;

template<typename KeyType, typename MonomerType, bool debug>
std::ostream& operator<<(std::ostream& os, const AFK_PolymerChain<KeyType, MonomerType, debug>& _chain);

/* The hash map is stored internally as these chains of
 * links to MonomerType.  When too much contention is deemed to be
 * going on, a new block is added.
 */
template<typename KeyType, typename MonomerType, bool debug>
class AFK_PolymerChain
{
protected:
    KeyType unassigned;
    MonomerType *chain;
    boost::atomic<AFK_PolymerChain<KeyType, MonomerType, debug>*> nextChain;

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
        const KeyType& _unassigned,
        const unsigned int _hashBits):
            unassigned(_unassigned), nextChain (nullptr), hashBits (_hashBits), index (0)
    {
        chain = new MonomerType[CHAIN_SIZE](); /* note the default-constructing () */
    }

    virtual ~AFK_PolymerChain()
    {
        AFK_PolymerChain<KeyType, MonomerType, debug> *next = nextChain.exchange(nullptr);
        if (next) delete next;

        delete[] chain;
    }
    
    /* Appends a new chain. */
    void extend(AFK_PolymerChain<KeyType, MonomerType, debug> *chain, unsigned int _index)
    {
        assert(index == _index);

        AFK_PolymerChain<KeyType, MonomerType, debug> *expected = nullptr;
        bool gotIt = nextChain.compare_exchange_strong(expected, chain);
        if (gotIt)
        {
            chain->index = _index + 1;
        }
        else
        {
            AFK_PolymerChain<KeyType, MonomerType, debug> *next = nextChain.load();
            assert(next);
            next->extend(chain, _index + 1);
        }
    }

    /* Gets a monomer from a specific place. */
    bool get(unsigned int hops, size_t baseHash, const KeyType& key, MonomerType **o_monomerPtr) const
    {
        size_t offset = chainOffset(hops, baseHash);
        if (chain[offset].key.load() == key)
        {
            *o_monomerPtr = &chain[offset];
            if (debug)
                AFK_DEBUG_PRINTL(*this << " get(): " << key << " (offset " << offset << ") -> " << **o_monomerPtr)
            return true;
        }
        else
        {
            if (debug)
                AFK_DEBUG_PRINTL(*this << " get(): " << key << " (offset " << offset << ") no match")
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
        if (chain[offset].key.compare_exchange_strong(unassigned, key))
        {
            *o_monomerPtr = &chain[offset];
            if (debug)
                AFK_DEBUG_PRINTL(*this << " insert(): " << key << " (offset " <<  offset << ") <- " << **o_monomerPtr)
            return true;
        }
        else
        {
            if (debug)
                AFK_DEBUG_PRINTL(*this << " insert(): " << key << " (offset " << offset << ") taken")
            return false;
        }
    }

    /* Returns the next chain, or nullptr if we're at the end. */
    AFK_PolymerChain<KeyType, MonomerType, debug> *next(void) const
    {
        return nextChain.load();
    }

    unsigned int getIndex(void) const
    {
        return index;
    }

    unsigned int getCount(void) const
    {
        AFK_PolymerChain<KeyType, MonomerType, debug> *next = nextChain.load();
        if (next)
            return 1 + next->getCount();
        else
            return 1;
    }

    /* This iterator goes through the elements present in a
     * traditional manner.
     */
    class iterator:
        public boost::iterator_facade<
            AFK_PolymerChain<KeyType, MonomerType, debug>::iterator,
            MonomerType&,
            boost::forward_traversal_tag>
    {
    private:
        friend class boost::iterator_core_access;

        AFK_PolymerChain<KeyType, MonomerType, debug> *currentChain;
        unsigned int index;
    
        /* iterator implementation */

        void increment()
        {
            do
            {
                ++index;
                if (index == (1u << (currentChain->hashBits))) /* CHAIN_SIZE */
                {
                    index = 0;
                    currentChain = currentChain->next();
                }
            } while (currentChain && currentChain->chain[index].load() == nullptr);
        }

        bool equal(AFK_PolymerChain<KeyType, MonomerType, debug>::iterator const& other) const
        {
            return other.index == index && other.currentChain == currentChain;
        }

        MonomerType& dereference() const
        {
            return currentChain->chain[index];
        }

        /* to help out with initialisation */

        void forwardToFirst()
        {
            if (currentChain && currentChain->chain[index].load() == nullptr) increment();
        }

    public:
        iterator(AFK_PolymerChain<KeyType, MonomerType, debug> *chain):
            currentChain (chain), index (0) { forwardToFirst(); }
    
        iterator(AFK_PolymerChain<KeyType, MonomerType, debug> *chain, unsigned int _index):
            currentChain (chain), index (_index) { forwardToFirst(); }
    };

    /* Methods for supporting direct-slot access. */

    bool atSlot(size_t slot, MonomerType **o_monomerPtr) const
    {
        if (slot & ~HASH_MASK)
        {
            AFK_PolymerChain<KeyType, MonomerType, debug> *next = nextChain.load();
            if (next)
                return next->atSlot(slot - CHAIN_SIZE, o_monomerPtr);
            else
                return false;
        }
        else
        {
            *o_monomerPtr = &chain[slot];
            return true;
        }
    }

    bool eraseSlot(size_t slot, const KeyType& key)
    {
        if (slot & ~HASH_MASK)
        {
            AFK_PolymerChain<KeyType, MonomerType, debug> *next = nextChain.load();
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

    friend std::ostream& operator<< <KeyType, MonomerType, debug>(std::ostream& os, const AFK_PolymerChain<KeyType, MonomerType, debug>& _chain);
};

template<typename KeyType, typename MonomerType, bool debug>
std::ostream& operator<<(std::ostream& os, const AFK_PolymerChain<KeyType, MonomerType, debug>& _chain)
{
    os << "PolymerChain(index=" << _chain.index << ", addr=" << &_chain << ")";
    return os;
}

template<typename KeyType, typename MonomerType, typename Hasher, bool debug>
class AFK_Polymer {
protected:
    AFK_PolymerChain<KeyType, MonomerType, debug> *chains;

    /* The unassigned key. */
    const KeyType unassigned;

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
    AFK_PolymerChain<KeyType, MonomerType, debug> *addChain()
    {
        /* TODO Make this have prepared one already (in a different
         * thread), because making a new chain is slow
         */
        AFK_PolymerChain<KeyType, MonomerType, debug> *newChain =
            new AFK_PolymerChain<KeyType, MonomerType, debug>(unassigned, hashBits);
        chains->extend(newChain, 0);
        return newChain;
    }

    /* Retrieves an existing monomer. */
    bool retrieveMonomer(const KeyType& key, size_t hash, MonomerType **o_monomerPtr) const
    {
        /* Try a small number of hops first, then expand out.
         */
        for (unsigned int hops = 0; hops < targetContention; ++hops)
        {
            for (AFK_PolymerChain<KeyType, MonomerType, debug> *chain = chains;
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
        AFK_PolymerChain<KeyType, MonomerType, debug> *startChain = chains;

        while (!inserted)
        {
            for (unsigned int hops = 0; hops < targetContention && !inserted; ++hops)
            {
                for (AFK_PolymerChain<KeyType, MonomerType, debug> *chain = startChain;
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
    AFK_Polymer(const KeyType& _unassigned, unsigned int _hashBits, unsigned int _targetContention, Hasher _hasher):
        unassigned (_unassigned), hashBits (_hashBits), targetContention (_targetContention), hasher (_hasher)
    {
        /* Start off with just one chain. */
        chains = new AFK_PolymerChain<KeyType, MonomerType, debug>(_unassigned, _hashBits);
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
    MonomerType *get(const KeyType& key) const
    {
        size_t hash = wring(hasher(key));
        MonomerType *monomer;
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
        MonomerType *monomer;

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

    typename AFK_PolymerChain<KeyType, MonomerType, debug>::iterator begin() const
    {
        return typename AFK_PolymerChain<KeyType, MonomerType, debug>::iterator(chains);
    }

    typename AFK_PolymerChain<KeyType, MonomerType, debug>::iterator end() const
    {
        return typename AFK_PolymerChain<KeyType, MonomerType, debug>::iterator(nullptr);
    }

    /* For accessing the chain slots directly.  Use carefully (it's really
     * just for the eviction thread).
     * Here chain slots are numbered 0 to (CHAIN_SIZE * chain count).
     */
    size_t slotCount() const
    {
        return chains->getCount() * CHAIN_SIZE;
    }

    bool getSlot(size_t slot, MonomerType **o_monomerPtr) const
    {
        return chains->atSlot(slot, o_monomerPtr);
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

