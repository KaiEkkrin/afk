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

#include <cstdlib>
#include <iostream>

#include <boost/random/random_device.hpp>

#include "file/logstream.hpp"
#include "jigsaw_collection.hpp"
#include "rng/boost_taus88.hpp"

static void tryConvert(
    const Vec3<int>& _fakeCoord,
    const AFK_JigsawFake3DDescriptor& testFake)
{
    Vec2<int> realCoord = testFake.fake3DTo2D(_fakeCoord);
    Vec3<int> convertedCoord = testFake.fake3DFrom2D(realCoord);
    if (convertedCoord != _fakeCoord) afk_out << "FAILED: ";

    Vec3<int> realSize = testFake.get2DSize();
    if (realCoord.v[0] >= realSize.v[0] || realCoord.v[1] >= realSize.v[1] || realSize.v[2] != 1)
        afk_out << "FAILED: ";

    afk_out << _fakeCoord << " -> " << realCoord << " -> " << convertedCoord << std::endl;
}

static void tryOneJigsaw(const Vec3<int>& testSize, AFK_RNG& rng)
{
    AFK_JigsawFake3DDescriptor testFake(true, testSize);
    Vec3<int> realSize = testFake.get2DSize();
    afk_out << "Testing fake 3D jigsaw: " << testSize << " (Real size: " << realSize << ")" << std::endl;

    Vec3<int> testZero = afk_vec3<int>(0, 0, 0);
    Vec3<int> testOneX = afk_vec3<int>(1, 0, 0);
    Vec3<int> testOneY = afk_vec3<int>(0, 1, 0);
    Vec3<int> testOneZ = afk_vec3<int>(0, 0, 1);
    Vec3<int> testAllOnes = afk_vec3<int>(1, 1, 1);
    Vec3<int> testMax = testSize - afk_vec3<int>(1, 1, 1);
    tryConvert(testZero, testFake);
    tryConvert(testOneX, testFake);
    tryConvert(testOneY, testFake),
    tryConvert(testOneZ, testFake);
    tryConvert(testAllOnes, testFake);
    tryConvert(testMax, testFake);

    for (int i = 0; i < 10; ++i)
    {
        Vec3<int> testVal = afk_vec3<int>(
            rng.uirand() % testSize.v[0],
            rng.uirand() % testSize.v[1],
            rng.uirand() % testSize.v[2]);
        tryConvert(testVal, testFake);
    }

    afk_out << std::endl;
}

void test_jigsawFake3D(void)
{
    boost::random::random_device rdev;
    AFK_Boost_Taus88_RNG rng;
    rng.seed(AFK_RNG_Value(
        (uint64_t)rdev() | ((uint64_t)rdev() << 32)));

    /* Try the degenerate case: */
    tryOneJigsaw(afk_vec3<int>(1, 1, 1), rng);

    /* Try the one I expect I'll actually need to use: */
    tryOneJigsaw(afk_vec3<int>(7, 7, 7), rng);

    /* And then a few randoms: */
    for (int i = 0; i < 4; ++i)
    {
        tryOneJigsaw(afk_vec3<int>(
            (rng.uirand() % 25) + 1,
            (rng.uirand() % 25) + 1,
            (rng.uirand() % 25) + 1), rng);
    }
}
