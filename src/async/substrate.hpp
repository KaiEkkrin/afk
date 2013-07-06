/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_ASYNC_SUBSTRATE_H_
#define _AFK_ASYNC_SUBSTRATE_H_

#include <exception>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/taus88.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include "stats.hpp"

/* This is a (mostly) lockfree heap of identical objects that should
 * avoid lots of overhead of going to kernel mode, locking the real
 * heap etc, when allocating and freeing many objects of the same
 * type.
 * It's sparsely allocated, so it won't achieve maximal memory
 * usage, but hopefully it will be OK...
 */

/* Set this to make it print whenever it expands */
#define DEBUG_PRINT_ON_EXPAND 0

class AFK_SubstrateDoubleFreeException: public std::exception {};
class AFK_BadSubstrateIndexException: public std::exception {};

/* The substrate "invalid" value, equivalent to a NULL pointer. */
#define SUBSTRATE_INVALID (((size_t)1) << ((sizeof(size_t) * 8) - 1))

template<class T>
struct AFK_SubstrateEntry
{
    T t;
    boost::atomic<bool> inUse;
};

template<class T>
class AFK_SingleSubstrate
{
protected:
    /* This is the block of Ts that make up the substrate */
    struct AFK_SubstrateEntry<T> *s;

    /* How many bits are in the indices to the block */
    size_t subBits;

    /* How big the block is */
    size_t size;

    /* This is used to pick usage entries to try when allocating
     * from the heap.
     */
    boost::random::taus88 rng;

    /* The maximum number of tries before I give up. */
    unsigned int maxTries;

    /* Stats. */
    AFK_StructureStats stats;

public:
    AFK_SingleSubstrate(unsigned int _subBits, unsigned int rngSeed, unsigned int _maxTries):
        subBits(_subBits), size(1 << _subBits), rng(rngSeed), maxTries(_maxTries)
    {
        s = new AFK_SubstrateEntry<T>[size];

        /* Each one starts out free */
        for (size_t i = 0; i < size; ++i)
            s[i].inUse.store(false);
    }

    virtual ~AFK_SingleSubstrate()
    {
        /* This will explode quite significantly if other threads
         * are still using the thing!
         */
        delete[] s;
    }

    /* Allocates an element in the substrate.  Fills out o_index
     * with the index of the newly allocated element.  If it
     * couldn't allocate one, returns 0.
     */
    bool alloc(size_t& o_index)
    {
        bool allocated = false;
        unsigned int tries = 0;

        for (; !allocated && tries < maxTries; ++tries)
        {
            size_t tryIndex = rng() & ((1 << subBits) - 1);
            bool wasInUse = false;
            bool gotIt = s[tryIndex].inUse.compare_exchange_weak(wasInUse, true);
            if (gotIt && !wasInUse)
            {
                o_index = tryIndex;
                stats.insertedOne(tries);
                allocated = true;
            }
        }

        return allocated;
    }

    /* Frees up an element in the substrate, returning it to
     * the free queue.
     */
    void free(size_t index)
    {
        if (index == SUBSTRATE_INVALID) throw AFK_BadSubstrateIndexException();

        bool wasInUse = true;

        /* It's quite important that I succeed at this */
        s[index].inUse.compare_exchange_strong(wasInUse, false);
        if (!wasInUse) throw AFK_SubstrateDoubleFreeException();
    }

    /* Accesses an element by index. */
    T& operator[](size_t index) const
    {
        if (index == SUBSTRATE_INVALID || s[index].inUse == false) throw AFK_BadSubstrateIndexException();
        return s[index].t;
    }

    /* Prints usage stats. */
    void printStats(std::ostream& os, const std::string& prefix) const
    {
        stats.printStats(os, prefix);
    }
};

template<class T>
class AFK_Substrate
{
protected:
    /* So that we can grow, each AFK_Substrate is actually a list
     * of them.  If one of them runs out and I need a new one,
     * I add another to the list.
     */
    std::vector<AFK_SingleSubstrate<T> *> s;

    /* The number of bits in the index that index into an
     * individual substrate, as described below.
     */
    unsigned int subBits;

    /* Used only for creating and removing substrates. */
    boost::mutex sMut;

    /* The target level of contention in the substrates. */
    unsigned int targetContention;

    /* Where we get the substrate RNG seeds from. */
    boost::random::random_device rdev;

    /* Adds a new substrate to the vector. */
    void addSubstrate(void)
    {
        boost::unique_lock<boost::mutex> lock(sMut);
        s.push_back(new AFK_SingleSubstrate<T>(subBits, rdev(), targetContention * targetContention));
    }

    /* Splits an index into its substrate number and within-
     * substrate index.
     */
    void splitIndex(size_t index, size_t& o_substrateNumber, size_t& o_substrateIndex) const
    {
        o_substrateNumber = index >> subBits;
        o_substrateIndex = index & ((1 << subBits) - 1);

        if (o_substrateNumber >= s.size()) throw AFK_BadSubstrateIndexException();
    }

    /* Makes an index out of a substrate number and index. */
    size_t makeIndex(size_t substrateNumber, size_t substrateIndex) const
    {
        return (substrateNumber << subBits) | substrateIndex;
    }

public:
    /* An AFK_Substrate is initialised with the number of
     * bits in the index that correspond to where in the individual
     * substrate a structure is located.  The number of structures
     * per substrate is 2**subBits; thus, this governs how big
     * each substrate will be.  Try to supply a value that doesn't
     * result in huge amounts of excess space being allocated, or
     * too many substrates in the list.
     */
    AFK_Substrate(unsigned int _subBits, unsigned int _targetContention):
        subBits(_subBits), targetContention(_targetContention)
    {
        /* Start with one. */
        addSubstrate();
    }

    virtual ~AFK_Substrate()
    {
        boost::unique_lock<boost::mutex> lock(sMut);
        for (typename std::vector<AFK_SingleSubstrate<T> *>::iterator sIt = s.begin();
            sIt != s.end(); ++sIt)
        {
            delete *sIt;
        }

        s.clear();
    }

    /* Allocates a new element in the substrate, returning the index. */
    size_t alloc(void)
    {
        long long substrateNumber;
        size_t substrateIndex;
        bool allocated = false;

        while (!allocated)
        {
            /* I try the substrates in inverse order, because newer ones
             * are likely to be less busy
             */    
            for (substrateNumber = s.size() - 1; substrateNumber >= 0; --substrateNumber)
            {
                allocated = s[substrateNumber]->alloc(substrateIndex);
                if (allocated) break;
            }

            if (!allocated)
            {
#if DEBUG_PRINT_ON_EXPAND   
                printStats(std::cout, "expanding");
#endif
                addSubstrate();
            }
        }

        return makeIndex((size_t)substrateNumber, substrateIndex);
    }

    void free(size_t index)
    {
        if (index == SUBSTRATE_INVALID) throw AFK_BadSubstrateIndexException();

        size_t substrateNumber, substrateIndex;

        splitIndex(index, substrateNumber, substrateIndex);
        s[substrateNumber]->free(substrateIndex);
    }

    T& operator[](size_t index) const
    {
        if (index == SUBSTRATE_INVALID) throw AFK_BadSubstrateIndexException();

        size_t substrateNumber, substrateIndex;

        splitIndex(index, substrateNumber, substrateIndex);
        return (*(s[substrateNumber]))[substrateIndex];
    }

    /* Prints usage stats. */
    void printStats(std::ostream& os, const std::string& prefix) const
    {
        for (unsigned int i = 0; i < s.size(); ++i)
        {
            std::ostringstream ss;
            ss << prefix << " " << i;
            s[i]->printStats(os, ss.str());
        }
    }
};

#endif /* _AFK_ASYNC_SUBSTRATE_H_ */

