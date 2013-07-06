/* AFK (c) Alex Holloway 2013 */

#include "stats.hpp"

void AFK_StructureStats::insertedOne(unsigned int tries)
{
    /* TODO Right now this doesn't guard against concurrent
     * access of separate fields, but I don't think I really
     * need that level of accuracy (?)
     */
    size.fetch_add(1);
    contention.fetch_add(tries);
    contentionSampleSize.fetch_add(tries);
}

void AFK_StructureStats::erasedOne(void)
{
    size.fetch_sub(1);
}

unsigned int AFK_StructureStats::getContentionAndReset(void)
{
    unsigned int oldContention = contention;
    unsigned int oldContentionSampleSize = contentionSampleSize;

    contentionSampleSize.store(0);
    contention.store(0);

    return oldContention / oldContentionSampleSize;
}

void AFK_StructureStats::printStats(std::ostream& os, const std::string& prefix) const
{
    os << prefix << ": Size: " << size << std::endl;
    os << prefix << ": Contention: " << contention / contentionSampleSize << std::endl;
}

