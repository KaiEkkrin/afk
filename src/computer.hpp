/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_COMPUTER_H_
#define _AFK_COMPUTER_H_

#include "afk.hpp"

#include "exception.hpp"

/* A useful wrapper around the OpenCL stuff (I'm using the
 * C bindings, not the C++ ones, which caused mega-tastic
 * build issues)
 */

class AFK_Computer
{
protected:
    /* The ID of the device that I'm using.
     * TODO: Support many ?
     */
    cl_device_id activeDevice;

    void inspectDevices(cl_platform_id platform, cl_device_type deviceType);
public:
    AFK_Computer();

    /* Runs my test. */
    void test(void);
};

#endif /* _AFK_COMPUTER_H_ */

