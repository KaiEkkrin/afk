/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_ASYNC_POLYMER_H_
#define _AFK_ASYNC_POLYMER_H_

#include <vector>

#include <boost/atomic.hpp>
#include <boost/thread/mutex.hpp>

#include "stats.hpp"
#include "substrate.hpp"

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
template<class KeyType, class ValueType>
struct AFK_Monomer
{
    KeyType     key;
    ValueType   value;
};

template<class KeyType, class ValueType>
class AFK_Polymer
{
protected:
    /* The hash map is stored internally as a vector of chains of
     * AFK_Monomers.  When too much contention is deemed to be
     * going on, a new block is added.
     */
    std::vector<boost::atomic<struct AFK_Monomer<KeyType, ValueType>*>*> chains;

    /* This is the substrate from which all monomers get allocated. */
    AFK_Substrate<struct AFK_Monomer<KeyType, ValueType> > substrate;

    /* This mutex is used only to guard changing the `chains'
     * structure itself, which happens relatively infrequently.
     */
    boost::mutex chainsMut;

    /* These values define the behaviour of this polymer. */
    const unsigned int hashBits; /* Each chain is 1<<hashBits long */
#define CHAIN_SIZE (1<<hashBits)
    const unsigned int targetContention; /* The contention level at which we make a new chain */

    /* Stats for the chain */
    AFK_StructureStats stats;

    /* Adds a new chain. */
    void unsafe_addChain()
    {
        boost::atomic<struct AFK_Monomer *> *chain =
            new boost::atomic<struct AFK_Monomer *>[CHAIN_SIZE];
        for (unsigned int i = 0; i < CHAIN_SIZE; ++i)
            chain[i].store(NULL);

        chains.push_back(chain);
    }

    /* Calculates the chain index for a given hash. */
    size_t chainOffset(unsigned int ch, unsigned int hops, size_t hash) const
    {
        /* I'm going to swizzle the hash around from one chain to
         * the next in an attempt to average out any contention
         * causing anomalies.
         */
        size_t thisHash = rotateLeft(baseHash, ch * hashBits);
        return (thisHash + hops) & (CHAIN_SIZE - 1);
    }

    /* Attempts to push a new monomer into place in a chain.
     * Returns true if success, else false.
     */
    bool tryInsert(unsigned int ch, unsigned int hops, size_t baseHash, struct AFK_Monomer *monomer)
    {
        struct AFK_Monomer *expected = NULL;
        bool gotIt = chains[ch][chainOffset(ch, hops, baseHash)].compare_exchange_weak(
            expected, monomer);
        if (gotIt)
        {
            /* Update statistics. */
            size.fetch_add(1);
            contention.fetch_add(hops);
            contentionSampleSize.fetch_add(1);
        }

        return gotIt;
    }

public:
    AFK_Polymer(unsigned int _hashBits, unsigned int _targetContention):
        substrate(_hashBits, _targetContention), hashBits (_hashBits), targetContention (_targetContention)
    {
        /* Start off with just one chain. */
        addChain();
    }

    virtual ~AFK_Polymer()
    {
        /* Wipeout time.  I hope nobody is attempting concurrent access now.
         * TODO: Clear all entries first?
         */

        boost::unique_lock<boost::mutex> lock(chainsMut);
        for (std::vector<boost::atomic<struct AFK_Monomer *>*>::iterator chIt = chains.begin();
            chIt != chains.end(); ++chIt)
        {
            delete[] *chIt;
        }

        chains.clear();
    }

    /* TODO: no no no, this isn't the interface that I want.  I want an
     * operator[] that will return a reference to the existing value if
     * it's there, or initialise a new one, put it in and return a
     * reference to that if it wasn't.  And in order to be able to do
     * that locklessly I definitely want my lockless memory substrate,
     * so I should write that first, then re-think a whole lot of the
     * pointers that I put in this structure (eww).
     * Gnargle.
     */
    void insert(struct AFK_Monomer *monomer)
    {
        size_t hash = hash_value(*monomer);
        bool inserted = false;
        unsigned int hops = 0;

        /* Try to insert it at its proper place first.  If that fails,
         * attempt to go through hops.
         */
        while (!inserted)
        {
            for (hops = 0; hops < targetContention && !inserted; ++hops)
            {
                for (unsigned int ch = 0; ch < chains.size() && !inserted; ++ch)
                {
                    inserted = tryInsert(ch, hops, hash, monomer);
                }
            }

            if (!inserted)
            {
                /* Oh dear.  We need a new chain now */
                boost::unique_lock<boost::mutex> lock(chainsMut);
                unsafe_addChain();
            }
        }

    }

    /* Returns a reference to a map entry.  Inserts a new one if
     * it couldn't find one.
     */
    struct ValueType& operator[](const KeyType& key)
    {
        size_t hash = hash_value(key);

        
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
        {
            std::ostringstream ss;
            ss << prefix << ": Polymer";
            stats.printStats(os, ss.str());
        }

        {
            std::ostringstream ss;
            ss << prefix << ": Substrate";
            substrate.printStats(os, ss.str());
        }
    }
};

#endif /* _AFK_ASYNC_POLYMER_H_ */

