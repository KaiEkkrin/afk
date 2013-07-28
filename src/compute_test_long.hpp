/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_COMPUTE_TEST_LONG_HPP_
#define _AFK_COMPUTE_TEST_LONG_HPP_

#include "computer.hpp"

/* The point of this one is to determine how OpenCL contexts and
 * queues react to multiple threads.
 */
void afk_testComputeLong(AFK_Computer *computer, unsigned int concurrency);

#endif /* _AFK_COMPUTE_TEST_LONG_HPP_ */

