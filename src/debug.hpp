/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DEBUG_H_
#define _AFK_DEBUG_H_

#include <iostream>
#include <sstream>

void afk_debugPrint(const std::string& s);

#define AFK_DEBUG_PRINT(expr) \
    { \
        std::ostringstream ss; \
        ss << expr; \
        afk_debugPrint(ss.str()); \
    }

#define AFK_DEBUG_PRINTL(expr) \
    { \
        std::ostringstream ss; \
        ss << expr << std::endl; \
        afk_debugPrint(ss.str()); \
    }

#endif /* _AFK_DEBUG_H_ */

