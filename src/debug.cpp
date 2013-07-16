/* AFK (c) Alex Holloway 2013 */

#include <boost/thread/mutex.hpp>

#include "debug.hpp"

boost::mutex coutMut;

void afk_debugPrint(const std::string& s)
{
    boost::unique_lock<boost::mutex> lock(coutMut);
    std::cout << s;
}

