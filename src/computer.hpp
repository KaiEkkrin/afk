/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_COMPUTER_H_
#define _AFK_COMPUTER_H_

#include "afk.hpp"

#include <string>

#include "exception.hpp"

/* This defines a list of programs that I know about. */
struct AFK_ClProgram
{
    cl_program program;
    std::string filename;
};

/* This defines a list of all the kernels I know about */
struct AFK_ClKernel
{
    cl_kernel kernel;
    std::string programFilename;
    std::string kernelName;
};

/* Handles an OpenCL error. */
void afk_handleClError(cl_int error);

#define AFK_CLCHK(call) \
    { \
        cl_int error = call; \
        if (error != CL_SUCCESS) afk_handleClError(error); \
    }

/* A useful wrapper around the OpenCL stuff (I'm using the
 * C bindings, not the C++ ones, which caused mega-tastic
 * build issues)
 */

/* Test findings:
 * - Creating many CL contexts: I quickly got an error
 * -6 (out of memory).  I guess I don't want to be doing
 * that, anyway.
 * - One context per Computer, one queue per thread:
 * Bugs out.  It looks like my earlier test was invalid:
 * there is a concurrency issue here.
 * If I externally lock it, it's okay.
 * The question is, can I have one context per thread
 * (which no doubt would avoid the need for the mutex)?
 * Can I transfer buffers between contexts easily?  What
 * about cl_gl?
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

    /* Helper functions */
    void inspectDevices(cl_platform_id platform, cl_device_type deviceType);
    void loadProgramFromFile(struct AFK_ClProgram *p);
public:
    AFK_Computer(unsigned int _qCount);
    virtual ~AFK_Computer();

    /* Loads all CL programs from disk.  Call before doing
     * anything else.
     */
    void loadPrograms(const std::string& programsDir);

    /* To use this class, call the following functions
     * to identify where you want to compute, and then do
     * the rest of the OpenCL yourself.
     */
    bool findKernel(const std::string& kernelName, cl_kernel& o_kernel) const;
    cl_context getContext(void) const;
    cl_command_queue getCommandQueue(unsigned int threadId) const;
};

#endif /* _AFK_COMPUTER_H_ */

