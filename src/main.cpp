/* AFK (c) Alex Holloway 2013 */

/* TODO Convert this stuff to C++11.  It's genuinely better and I'm already
 * noticing places where I would benefit from using it instead:
 * - use unique_ptr instead of auto_ptr
 * - use unordered_map instead of map
 * - use { ... } initialisation assignments instead of cumbersome messes
 */

#include "afk.h"

#include <iostream>

#include "core.h"
#include "exception.h"

/* This is the AFK global core declared in core.h */
AFK_Core afk_core;


int main(int argc, char **argv)
{
    int retcode = 0;

    try
    {
        std::cout << "AFK initalising graphics" << std::endl;
        afk_core.initGraphics(&argc, argv);

        //std::cout << "AFK initialising compute" << std::endl;
        //afk_core.initCompute();

        std::cout << "AFK configuring" << std::endl;
        afk_core.configure(&argc, argv);

        std::cout << "AFK starting loop" << std::endl;
        afk_core.loop();
    }
    catch (AFK_Exception e)
    {
        retcode = 1;
        std::cerr << "AFK Error: " << e.what() << std::endl;
    }

    std::cout << "AFK exiting" << std::endl;
    return retcode;
}

