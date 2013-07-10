/* AFK (c) Alex Holloway 2013 */

#include "stats.hpp"

AFK_StructureStats::AFK_StructureStats()
{
    size.store(0);
    contention.store(0);
    contentionSampleSize.store(0);
}

void AFK_StructureStats::insertedOne(unsigned int tries)
{
    /* TODO Right now this doesn't guard against concurrent
     * access of separate fields, but I don't think I really
     * need that level of accuracy (?)
     */
    size.fetch_add(1);
    contention.fetch_add(tries);
    contentionSampleSize.fetch_add(1);
}

void AFK_StructureStats::erasedOne(void)
{
    size.fetch_sub(1);
}

size_t AFK_StructureStats::getSize(void) const
{
    return size.load();
}

unsigned int AFK_StructureStats::getContentionAndReset(void)
{
    unsigned int oldContention = contention;
    unsigned int oldContentionSampleSize = contentionSampleSize;

    contentionSampleSize.store(0);
    contention.store(0);

    return oldContentionSampleSize == 0 ? 0 : (oldContention / oldContentionSampleSize);
}

void AFK_StructureStats::printStats(std::ostream& os, const std::string& prefix) const
{
    os << prefix << ": Size: " << size << std::endl;
    os << prefix << ": Contention: " << (
        contentionSampleSize == 0 ? 0.0f : ((float)contention / (float)contentionSampleSize)) << std::endl;
}

