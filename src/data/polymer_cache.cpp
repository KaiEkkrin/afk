/* AFK (c) Alex Holloway 2013 */

unsigned int afk_suggestCacheBitness(unsigned int entries)
{
    unsigned int bitness;
    for (bitness = 0; (1u << bitness) < (entries << 1); ++bitness);
    return bitness;
}

