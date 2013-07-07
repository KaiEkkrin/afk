/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_POLYMER_H_
#define _AFK_DATA_POLYMER_H_

#define DREADFUL_POLYMER_DEBUG 0

#if DREADFUL_POLYMER_DEBUG
#include <iostream>
#endif

#include <sstream>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/functional/hash/hash.hpp>
#include <boost/thread/mutex.hpp>

#include "stats.hpp"

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

template<typename KeyType, typename ValueType, typename Hasher>
class AFK_Polymer {
protected:
    /* The hash map is stored internally as a vector of chains of
     * links to AFK_Monomers.  When too much contention is deemed to be
     * going on, a new block is added.
     */
    std::vector<boost::atomic<AFK_Monomer<KeyType, ValueType> *>*> chains;

    /* I should do this, because I can't be sure of the thread safety
     * of chains.size() ...
     */
    boost::atomic<size_t> chainSize;

    /* This mutex is used only to guard changing the `chains'
     * structure itself, which happens relatively infrequently.
     */
    boost::mutex chainsMut;

    /* These values define the behaviour of this polymer. */
    const unsigned int hashBits; /* Each chain is 1<<hashBits long */
#define CHAIN_SIZE (1u<<hashBits)
#define HASH_MASK ((1u<<hashBits)-1)
    const unsigned int targetContention; /* The contention level at which we make a new chain */

    /* The hasher to use. */
    Hasher hasher;

    /* Stats for the chain */
    AFK_StructureStats stats;

    /* Adds a new chain, returning the old chain size. */
    size_t addChain()
    {
        /* TODO This is slow.  Consider keeping a spare around to
         * swap to immediately, and spawning a new thread to slowly
         * build the next spare.
         */
        boost::atomic<AFK_Monomer<KeyType, ValueType>*> *chain =
            new boost::atomic<AFK_Monomer<KeyType, ValueType>*>[CHAIN_SIZE];
        for (unsigned int i = 0; i < CHAIN_SIZE; ++i)
            chain[i].store(NULL);

        {
            boost::unique_lock<boost::mutex> lock(chainsMut);
            chains.push_back(chain);
            return chainSize.fetch_add(1);
        }
    }

    /* Calculates the chain index for a given hash. */
    size_t chainOffset(unsigned int ch, unsigned int hops, size_t hash) const
    {
        /* I'm going to swizzle the hash around from one chain to
         * the next in an attempt to average out any contention
         * causing anomalies.
         */
        size_t leftPart = hash << (ch * hashBits);
        size_t rightPart = hash >> ((sizeof(size_t) * 8) - (ch * hashBits));
        size_t thisHash = (leftPart | rightPart) & HASH_MASK;
        return ((thisHash + hops) & HASH_MASK) +
            ((thisHash + hops) >> hashBits);
    }

    /* Gets a monomer from a specific place in a chain. */
    AFK_Monomer<KeyType, ValueType> *at(unsigned int ch, unsigned int hops, size_t baseHash)
    {
        return chains[ch][chainOffset(ch, hops, baseHash)].load();
    }

    /* Attempts to push a new monomer into place in a chain by index.
     * Returns true if success, else false.
     */
    bool insert(unsigned int ch, unsigned int hops, size_t baseHash, AFK_Monomer<KeyType, ValueType> *monomer)
    {
        AFK_Monomer<KeyType, ValueType> *expected = NULL;
        bool gotIt = chains[ch][chainOffset(ch, hops, baseHash)].compare_exchange_weak(
            expected, monomer);
        if (gotIt)
        {
            /* Update statistics. */
            stats.insertedOne(hops);
        }

        return gotIt;
    }

public:
    AFK_Polymer(unsigned int _hashBits, unsigned int _targetContention, Hasher _hasher):
        hashBits (_hashBits), targetContention (_targetContention), hasher (_hasher)
    {
        /* Start off with just one chain. */
        chainSize.store(0);
        addChain();
    }

    virtual ~AFK_Polymer()
    {
        /* Wipeout time.  I hope nobody is attempting concurrent access now.
         * TODO: Clear all entries first?
         */

        /* TODO: The segfault is NOT caused by a premature delete.  I repeat,
         * the segfault is NOT caused by a premature delete.
         * :-(
         */

        boost::unique_lock<boost::mutex> lock(chainsMut);
        for (typename std::vector<boost::atomic<AFK_Monomer<KeyType, ValueType> *>*>::iterator chIt = chains.begin();
            chIt != chains.end(); ++chIt)
        {
            for (unsigned int i = 0; i < CHAIN_SIZE; ++i)
            {
                delete (*chIt)[i].load();
            }

            delete[] *chIt;
        }

        chains.clear();
    }

    /* Returns a reference to a map entry.  Inserts a new one if
     * it couldn't find one.
     * This will occasionally generate duplicates.  That should be
     * okay.
     */
    ValueType& operator[](const KeyType& key)
    {
        size_t hash = hasher(key);
        AFK_Monomer<KeyType, ValueType> *monomer = NULL;

        /* I'm going to assume it's probably there already, and first
         * just do a basic search.
         */
        for (unsigned int hops = 0; hops < targetContention && monomer == NULL; ++hops)
        {
            for (unsigned int ch = 0; ch < chainSize.load() && monomer == NULL; ++ch)
            {
                AFK_Monomer<KeyType, ValueType> *candidateMonomer = at(ch, hops, hash);
#if DREADFUL_POLYMER_DEBUG
                std::cout << key << "(ch " << ch << ", hops " << hops << ") -> " << candidateMonomer;
                if (candidateMonomer) std::cout << "(key " << candidateMonomer->key << ")";
                std::cout << std::endl;
#endif
                if (candidateMonomer && candidateMonomer->key == key) monomer = candidateMonomer;
            }
        }

        if (monomer == NULL)
        {
            /* Make a new one. */
            AFK_Monomer<KeyType, ValueType> *newMonomer = new AFK_Monomer<KeyType, ValueType>(key);
            bool inserted = false;
            unsigned int chStart = 0;

            while (!inserted)
            {
                for (unsigned int hops = 0; hops < targetContention && !inserted; ++hops)
                {
                    for (unsigned int ch = chStart; ch < chainSize.load() && !inserted; ++ch)
                    {
                        inserted = insert(ch, hops, hash, newMonomer);

#if DREADFUL_POLYMER_DEBUG
                        if (inserted)
                        {
                            std::cout << newMonomer << "(key " << newMonomer->key << ") -> (ch " << ch << ", hops " << hops << ")" << std::endl;
                        }
#endif
                    }
                }

                if (!inserted)
                {
                    /* Add a new chain for it */
                    size_t oldSize = addChain();
                    stats.getContentionAndReset();

                    /* When I re-try, use the new chain */
                    chStart = oldSize;
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

    void printStats(std::ostream& os, const std::string& prefix) const
    {
        stats.printStats(os, prefix);
        os << prefix << ": Chain count: " << chainSize.load() << std::endl;
    }
};

#endif /* _AFK_DATA_POLYMER_H_ */

