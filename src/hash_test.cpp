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

#include "afk.hpp"

#include <iostream>

#include "hash_test.hpp"
#include "rng/rng.hpp"

void test_cellHash(void)
{
    std::cout << "Cell hash test" << std::endl;
    std::cout << "--------------" << std::endl;

    for (int64_t x = -1; x <= 1; ++x)
    {
        for (int64_t y = -1; y <= 1; ++y)
        {
            for (int64_t z = -1; z <= 1; ++z)
            {
                for (int64_t scale = 2; scale <= 8; scale *= 2)
                {
                    AFK_Cell testCell = afk_cell(afk_vec4<int64_t>(x, y, z, scale));
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

    for (int64_t x = -1; x <= 1; ++x)
    {
        for (int64_t z = -1; z <= 1; ++z)
        {
            for (int64_t scale = 2; scale <= 16; scale *= 2)
            {
                AFK_Tile testTile = afk_tile(afk_vec3<int64_t>(x, z, scale));
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

    const int64_t test_values[] = {
        1, -1, 0x040004000400040ll, -0x0400040000400040ll, 0x4000000000000000ll, -0x4000000000000000ll, 0
    };

    for (unsigned int i = 0; test_values[i] != 0; ++i)
    {
        for (unsigned int rot = 3; rot < 19; rot += 4)
        {
            std::cout << std::hex << test_values[i] <<
                "<<" << std::dec << rot <<
                " = " << std::hex << LROTATE_UNSIGNED((uint64_t)test_values[i], rot) << std::endl;
        }
    }

    std::cout << std::endl;
}

