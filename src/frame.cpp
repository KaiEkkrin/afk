/* AFK (c) Alex Holloway 2013 */

#include "frame.hpp"

std::ostream& operator<<(std::ostream& os, const AFK_Frame& frame)
{
    os << "Frame " << frame.id;
    if (frame.never) os << " (Never seen)";
    return os;
}

