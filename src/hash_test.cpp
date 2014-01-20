/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#include "file/logstream.hpp"
#include "hash_test.hpp"
#include "rng/rng.hpp"

void test_cellHash(int64_t startVal, int64_t incrVal)
{
    afk_out << "Cell hash test" << std::endl;
    afk_out << "--------------" << std::endl;

    for (int64_t x = startVal; x <= (startVal + 2 * incrVal); x += incrVal)
    {
        for (int64_t y = startVal; y <= (startVal + 2 * incrVal); y += incrVal)
        {
            for (int64_t z = startVal; z <= (startVal + 2 * incrVal); z += incrVal)
            {
                for (int64_t scale = 2; scale <= 8; scale *= 2)
                {
                    AFK_Cell testCell = afk_cell(afk_vec4<int64_t>(x, y, z, scale));
                    afk_out << testCell << " hashes to \t" << std::hex << hash_value(testCell) << std::endl;
                }
            }
        }
    }

    afk_out << std::endl;
}

void test_tileHash(int64_t startVal, int64_t incrVal)
{
    /* To begin with, I'll just go through a few standard
     * tiles making sure the hashes are as varied as they
     * can be (which I bet they aren't).
     */

    afk_out << "Tile hash test" << std::endl;
    afk_out << "--------------" << std::endl;

    for (int64_t x = startVal; x <= (startVal + 2 * incrVal); x += incrVal)
    {
        for (int64_t z = startVal; z <= (startVal + 2 * incrVal); z += incrVal)
        {
            for (int64_t scale = 2; scale <= 16; scale *= 2)
            {
                AFK_Tile testTile = afk_tile(afk_vec3<int64_t>(x, z, scale));
                afk_out << testTile << " hashes to \t" << std::hex << hash_value(testTile) << std::endl;
            }
        }
    }

    afk_out << std::endl;
}

void test_cellHash(void)
{
    test_cellHash(-1, 1);
    test_cellHash(-262144, 131072);
    test_cellHash(131072, 2);
}

void test_tileHash(void)
{
    test_tileHash(-1, 1);
    test_tileHash(-262144, 131072);
    test_tileHash(131072, 2);
}

void test_rotate(void)
{
    afk_out << "Rotate test" << std::endl;
    afk_out << "-----------" << std::endl;

    const int64_t test_values[] = {
        1, -1, 0x040004000400040ll, -0x0400040000400040ll, 0x4000000000000000ll, -0x4000000000000000ll, 0
    };

    for (unsigned int i = 0; test_values[i] != 0; ++i)
    {
        for (unsigned int rot = 3; rot < 19; rot += 4)
        {
            afk_out << std::hex << test_values[i] <<
                "<<" << std::dec << rot <<
                " = " << std::hex << AFK_LROTATE_UNSIGNED((uint64_t)test_values[i], rot) << std::endl;
        }
    }

    afk_out << std::endl;
}

