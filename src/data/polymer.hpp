/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_POLYMER_H_
#define _AFK_DATA_POLYMER_H_

#define DREADFUL_POLYMER_DEBUG 0

#if DREADFUL_POLYMER_DEBUG
#include <iostream>
#endif

#include <assert.h>
#include <sstream>

#include <boost/atomic.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include "stats.hpp"

#if DREADFUL_POLYMER_DEBUG
boost::mutex dpdMut;
#endif

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
 * - ValueType: should be copy constructable and assignable.
 */
template<typename KeyType, typename ValueType>
class AFK_Monomer
{
public:
    KeyType     key;
    ValueType   value;

    AFK_Monomer(KeyType _key):
        key(_key), value() {}
};

/* The hash map is stored internally as these chains of
 * links to AFK_Monomers.  When too much contention is deemed to be
 * going on, a new block is added.
 */
template<typename KeyType, typename ValueType>
class AFK_PolymerChain
{
protected:
    boost::atomic<AFK_Monomer<KeyType, ValueType>*> *chain;
    boost::atomic<AFK_PolymerChain<KeyType, ValueType>*> nextChain;

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
    AFK_PolymerChain(const unsigned int _hashBits): nextChain (NULL), hashBits (_hashBits), index (0)
    {
        chain = new boost::atomic<AFK_Monomer<KeyType, ValueType>*>[CHAIN_SIZE];
        for (unsigned int i = 0; i < CHAIN_SIZE; ++i)
            chain[i].store(NULL);
    }

    virtual ~AFK_PolymerChain()
    {
        AFK_PolymerChain<KeyType, ValueType> *next = nextChain.load();
        if (next)
        {
            delete next;
            nextChain.store(NULL);
        }

        for (unsigned int i = 0; i < CHAIN_SIZE; ++i)
        {
            AFK_Monomer<KeyType, ValueType> *monomer = chain[i].load();
            if (monomer) delete monomer;
        }
    }
    
    /* Appends a new chain. */
    void extend(AFK_PolymerChain<KeyType, ValueType> *chain, unsigned int _index)
    {
        AFK_PolymerChain<KeyType, ValueType> *expected = NULL;
        bool gotIt = nextChain.compare_exchange_strong(expected, chain);
        if (gotIt)
        {
            chain->index = _index + 1;
        }
        else
        {
            AFK_PolymerChain<KeyType, ValueType> *next = nextChain.load();
            assert(next);
            next->extend(chain, _index + 1);
        }
    }

    /* Gets a monomer from a specific place. */
    AFK_Monomer<KeyType, ValueType> *at(unsigned int hops, size_t baseHash)
    {
        size_t offset = chainOffset(hops, baseHash);
        return chain[offset].load();
    }
    
    /* Inserts a monomer into a specific place.
     * Returns true if successful, else false.
     */
    bool insert(unsigned int hops, size_t baseHash, AFK_Monomer<KeyType, ValueType> *monomer)
    {
        AFK_Monomer<KeyType, ValueType> *expected = NULL;
        size_t offset = chainOffset(hops, baseHash);
        return chain[offset].compare_exchange_weak(expected, monomer);
    }

    /* Returns the next chain, or NULL if we're at the end. */
    AFK_PolymerChain<KeyType, ValueType> *next(void) const
    {
        return nextChain.load();
    }

    unsigned int getIndex(void) const
    {
        return index;
    }

    unsigned int getCount(void) const
    {
        AFK_PolymerChain<KeyType, ValueType> *next = nextChain.load();
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
            AFK_PolymerChain<KeyType, ValueType>::iterator,
            boost::atomic<AFK_Monomer<KeyType, ValueType>*>,
            boost::forward_traversal_tag>
    {
    private:
        friend class boost::iterator_core_access;

        AFK_PolymerChain<KeyType, ValueType> *currentChain;
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
            } while (currentChain && currentChain->chain[index].load() == NULL);
        }

        bool equal(AFK_PolymerChain<KeyType, ValueType>::iterator const& other) const
        {
            return other.index == index && other.currentChain == currentChain;
        }

        boost::atomic<AFK_Monomer<KeyType, ValueType>*>& dereference() const
        {
            return currentChain->chain[index];
        }

        /* to help out with initialisation */

        void forwardToFirst()
        {
            if (currentChain && currentChain->chain[index].load() == NULL) increment();
        }

    public:
        iterator(AFK_PolymerChain<KeyType, ValueType> *chain):
            currentChain (chain), index (0) { forwardToFirst(); }
    
        iterator(AFK_PolymerChain<KeyType, ValueType> *chain, unsigned int _index):
            currentChain (chain), index (_index) { forwardToFirst(); }
    };

    /* Methods for supporting direct-slot access. */

    AFK_Monomer<KeyType, ValueType> *atSlot(size_t slot) const
    {
        if (slot & ~HASH_MASK)
        {
            AFK_PolymerChain<KeyType, ValueType> *next = nextChain.load();
            if (next)
                return next->atSlot(slot - CHAIN_SIZE);
            else
                return NULL;
        }
        else
        {
            return chain[slot].load();
        }
    }

    bool eraseSlot(size_t slot, AFK_Monomer<KeyType, ValueType> *monomer)
    {
        if (slot & ~HASH_MASK)
        {
            AFK_PolymerChain<KeyType, ValueType> *next = nextChain.load();
            if (next)
                return next->eraseSlot(slot - CHAIN_SIZE, monomer);
            else
                return false;
        }
        else
        {
            if (chain[slot].compare_exchange_weak(monomer, NULL))
            {
                delete monomer;
                return true;
            }
            else return false;
        }
    }
};

template<typename KeyType, typename ValueType, typename Hasher>
class AFK_Polymer {
protected:
    AFK_PolymerChain<KeyType, ValueType> *chains;

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

    /* Adds a new chain, returning a pointer to the old one. */
    AFK_PolymerChain<KeyType, ValueType> *addChain()
    {
        /* TODO Make this have prepared one already (in a different
         * thread), because making a new chain is slow
         */
        AFK_PolymerChain<KeyType, ValueType> *newChain =
            new AFK_PolymerChain<KeyType, ValueType>(hashBits);
        chains->extend(newChain, 0);
        return newChain;
    }

public:
    AFK_Polymer(unsigned int _hashBits, unsigned int _targetContention, Hasher _hasher):
        hashBits (_hashBits), targetContention (_targetContention), hasher (_hasher)
    {
        /* Start off with just one chain. */
        chains = new AFK_PolymerChain<KeyType, ValueType>(_hashBits);
    }

    virtual ~AFK_Polymer()
    {
#if 0
        /* Wipeout time.  I hope nobody is attempting concurrent access now.
         */
        delete chains;
#endif
    }

    size_t size() const
    {
        return stats.getSize();
    }

    /* Returns a reference to a map entry.  Inserts a new one if
     * it couldn't find one.
     * This will occasionally generate duplicates.  That should be
     * okay.
     */
    ValueType& operator[](const KeyType& key)
    {
        size_t hash = wring(hasher(key));
        AFK_Monomer<KeyType, ValueType> *monomer = NULL;

        /* I'm going to assume it's probably there already, and first
         * just do a basic search.
         * Try a small number of hops first, then expand out.
         */
        for (unsigned int hops = 0; hops < targetContention && !monomer; ++hops)
        {
            for (AFK_PolymerChain<KeyType, ValueType> *chain = chains;
                chain && !monomer; chain = chain->next())
            {
                AFK_Monomer<KeyType, ValueType> *candidateMonomer = chain->at(hops, hash);
#if DREADFUL_POLYMER_DEBUG
                if (candidateMonomer)
                {
                    boost::unique_lock<boost::mutex> lock(dpdMut);
                    std::cout << boost::this_thread::get_id() << ": FOUND ";
                    std::cout << key << " (ch " << chain->getIndex() << ", hops " << hops << ") -> " << candidateMonomer;
                    std::cout << " (key " << candidateMonomer->key << ")";
                    std::cout << std::endl;
                }
#endif
                if (candidateMonomer && candidateMonomer->key == key) monomer = candidateMonomer;
            }
        }

        if (monomer == NULL)
        {
            /* Make a new one. */
            AFK_Monomer<KeyType, ValueType> *newMonomer = new AFK_Monomer<KeyType, ValueType>(key);
            bool inserted = false;
            AFK_PolymerChain<KeyType, ValueType> *startChain = chains;

            while (!inserted)
            {
                for (unsigned int hops = 0; hops < targetContention && !inserted; ++hops)
                {
                    for (AFK_PolymerChain<KeyType, ValueType> *chain = startChain;
                        chain != NULL && !inserted; chain = chain->next())
                    {
                        inserted = chain->insert(hops, hash, newMonomer);

                        if (inserted)
                        {
                            stats.insertedOne(hops);
#if DREADFUL_POLYMER_DEBUG
                            {
                                boost::unique_lock<boost::mutex> lock(dpdMut);
                                std::cout << boost::this_thread::get_id() << ": ADDED ";
                                std::cout << newMonomer << "(key " << newMonomer->key << ") -> (ch " << chain->getIndex() << ", hops " << hops << ")" << std::endl;
                            }
#endif
                        }
                        else
                        {
#if DREADFUL_POLYMER_DEBUG
                            {
                                boost::unique_lock<boost::mutex> lock(dpdMut);
                                AFK_Monomer<KeyType, ValueType> *conflictMonomer = chain->at(hops, hash);
                                std::cout << boost::this_thread::get_id() << ": CONFLICTED ";
                                std::cout << key << " (ch " << chain->getIndex() << ", hops " << hops << ") -> " << conflictMonomer;
                                if (conflictMonomer) std::cout << " (key " << conflictMonomer->key << ")";
                                std::cout << std::endl;
                            }
#endif
                        }
                    }
                }

                if (!inserted)
                {
                    /* Add a new chain for it */
                    startChain = addChain();
                    //stats.getContentionAndReset();

#if DREADFUL_POLYMER_DEBUG
                    {
                        boost::unique_lock<boost::mutex> lock(dpdMut);
                        std::cout << boost::this_thread::get_id() << ": EXTENDED ";
                        std::cout << startChain->getIndex() << std::endl;
                    }
#endif
                }
            }

            monomer = newMonomer;
        }

        return monomer->value;
    }

    /* TODO: Other functions I need to support:
     * - Iteration through all elements
     * - Removal of a specific element
     * It would seem helpful to be able to return the internal
     * location to a caller of these functions, so that they
     * don't have to do any further searching of the map.  An
     * iterator value, in fact.  How do I make one of those?
     * - The boost iterator stuff is probably what I want here;
     * it appears that std::iterator is insane.
     * See http://www.boost.org/doc/libs/1_54_0/libs/iterator/doc/iterator_facade.html#tutorial-example
     * and http://www.boost.org/doc/libs/1_54_0/libs/iterator/doc/iterator_adaptor.html#tutorial-example
     * I should perhaps test these things on a trivial class first.
     */

    typename AFK_PolymerChain<KeyType, ValueType>::iterator begin() const
    {
        return typename AFK_PolymerChain<KeyType, ValueType>::iterator(chains);
    }

    typename AFK_PolymerChain<KeyType, ValueType>::iterator end() const
    {
        return typename AFK_PolymerChain<KeyType, ValueType>::iterator(NULL);
    }

    /* For accessing the chain slots directly.  Use carefully (it's really
     * just for the eviction thread).
     * Here chain slots are numbered 0 to (CHAIN_SIZE * chain count).
     */
    size_t slotCount() const
    {
        return chains->getCount() * CHAIN_SIZE;
    }

    /* If there's nothing in the slot, returns NULL. */
    AFK_Monomer<KeyType, ValueType> *atSlot(size_t slot) const
    {
        return chains->atSlot(slot);
    }

    /* Removes from a slot so long as it contains the monomer specified
     * (previously retrieved with atSlot() ).  Returns true if it
     * removed something, else false.
     * Because the memory is managed by the polymer itself, this
     * function call also causes `monomer' to have been freed if it
     * returns true.
     */
    bool eraseSlot(size_t slot, AFK_Monomer<KeyType, ValueType> *monomer)
    {
        bool success = chains->eraseSlot(slot, monomer);
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

