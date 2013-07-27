/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_COMPUTER_H_
#define _AFK_COMPUTER_H_

#include "afk.hpp"

#include "exception.hpp"

/* A useful wrapper around the OpenCL stuff (I'm using the
 * C bindings, not the C++ ones, which caused mega-tastic
 * build issues)
 */

/* Test findings:
 * - Creating many CL contexts: I quickly got an error
 * -6 (out of memory).  I guess I don't want to be doing
 * that, anyway.
 * - One context per Computer (i.e. thread global):
 * Works fine.  Simultaneous access to contexts (with a
 * different queue created each time) appears to be okay.
 * - I don't think I want to try using one global queue
 * anyway.
 * - One queue per thread:
 * Works fine.  I think this is probably the configuration
 * I want to use.  Rah!
 */

class AFK_Computer
{
protected:
    /* The ID of the device that I'm using.
     * TODO: Support many ?  Would need that many queues
     * per device?
     */
    cl_device_id activeDevice;

    cl_context ctxt;
    cl_command_queue *q;
    unsigned int qCount;

    void inspectDevices(cl_platform_id platform, cl_device_type deviceType);
public:
    AFK_Computer(unsigned int _qCount);
    virtual ~AFK_Computer();

    /* Runs my test. */
    void test(unsigned int qId);
};

#endif /* _AFK_COMPUTER_H_ */

