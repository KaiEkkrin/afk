/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <iostream>

#include "hash_test.hpp"
#include "rng/rng.hpp"

void test_cellHash(void)
{
    std::cout << "Cell hash test" << std::endl;
    std::cout << "--------------" << std::endl;

    for (long long x = -1; x <= 1; ++x)
    {
        for (long long y = -1; y <= 1; ++y)
        {
            for (long long z = -1; z <= 1; ++z)
            {
                for (long long scale = 2; scale <= 8; scale *= 2)
                {
                    AFK_Cell testCell = afk_cell(afk_vec4<long long>(x, y, z, scale));
                    std::cout << testCell << " hashes to " << std::hex << hash_value(testCell) << std::endl;
                }
            }
        }
    }

    std::cout << std::endl;
}

void test_tileHash(void)
{
    /* To begin with, I'll just go through a few standard
     * tiles making sure the hashes are as varied as they
     * can be (which I bet they aren't).
     */

    std::cout << "Tile hash test" << std::endl;
    std::cout << "--------------" << std::endl;

    for (long long x = -1; x <= 1; ++x)
    {
        for (long long z = -1; z <= 1; ++z)
        {
            for (long long scale = 2; scale <= 16; scale *= 2)
            {
                AFK_Tile testTile = afk_tile(afk_vec3<long long>(x, z, scale));
                std::cout << testTile << " hashes to " << std::hex << hash_value(testTile) << std::endl;
            }
        }
    }

    std::cout << std::endl;
}

void test_rotate(void)
{
    std::cout << "Rotate test" << std::endl;
    std::cout << "-----------" << std::endl;

    const long long test_values[] = {
        1, -1, 0x040004000400040ll, -0x0400040000400040ll, 0x4000000000000000ll, -0x4000000000000000ll, 0
    };

    for (unsigned int i = 0; test_values[i] != 0; ++i)
    {
        for (unsigned int rot = 3; rot < 19; rot += 4)
        {
            std::cout << std::hex << test_values[i] <<
                "<<" << std::dec << rot <<
                " = " << std::hex << LROTATE_UNSIGNED((unsigned long long)test_values[i], rot) << std::endl;
        }
    }

    std::cout << std::endl;
}

