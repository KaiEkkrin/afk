/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cstdlib>
#include <iostream>

#include <boost/random/random_device.hpp>

#include "test_jigsaw_fake3d.hpp"

static void tryConvert(
    const Vec3<int>& _fakeCoord,
    const AFK_JigsawFake3DDescriptor& testFake)
{
    Vec3<int> realCoord = testFake.fake3DTo2D(_fakeCoord);
    Vec3<int> convertedCoord = testFake.fake3DFrom2D(realCoord);
    if (convertedCoord != _fakeCoord) std::cout << "FAILED: ";
    std::cout << _fakeCoord << " -> " << realCoord << " -> " << convertedCoord << std::endl;
}

static void tryOneJigsaw(const Vec3<int>& testSize)
{
    std::cout << "Testing fake 3D jigsaw: " << testSize << std::endl;
    AFK_JigsawFake3DDescriptor testFake(true, testSize);

    Vec3<int> testZero = afk_vec3<int>(0, 0, 0);
    Vec3<int> testMax = testSize - afk_vec3<int>(1, 1, 1);
    tryConvert(testZero, testFake);
    tryConvert(testMax, testFake);

    for (int i = 0; i < 12; ++i)
    {
        Vec3<int> testVal = afk_vec3<int>(
            random() % testSize.v[0],
            random() % testSize.v[1],
            random() % testSize.v[2]);
        tryConvert(testVal, testFake);
    }

    std::cout << std::endl;
}

void test_jigsawFake3D(void)
{
    boost::random::random_device rdev;
    srand(rdev());

    /* Try the degenerate case: */
    tryOneJigsaw(afk_vec3<int>(1, 1, 1));

    /* Try the one I expect I'll actually need to use: */
    tryOneJigsaw(afk_vec3<int>(7, 7, 7));

    /* And then a few randoms: */
    for (int i = 0; i < 4; ++i)
    {
        tryOneJigsaw(afk_vec3<int>(
            (random() % 25) + 1,
            (random() % 25) + 1,
            (random() % 25) + 1));
    }
}

