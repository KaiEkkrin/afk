/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_COMPUTER_H_
#define _AFK_COMPUTER_H_

#include "afk.hpp"

#include <string>

#include <boost/thread/mutex.hpp>

#include "config.hpp"
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

/* Collections of properties that might be useful.
 */
class AFK_ClPlatformProperties
{
public:
    char *versionStr;
    size_t versionStrSize;
    unsigned int majorVersion;
    unsigned int minorVersion;

    AFK_ClPlatformProperties(cl_platform_id platform);
    ~AFK_ClPlatformProperties();
};

class AFK_ClDeviceProperties
{
public:
    cl_ulong    globalMemSize;
    size_t      image2DMaxWidth;
    size_t      image2DMaxHeight;
    size_t      image3DMaxWidth;
    size_t      image3DMaxHeight;
    size_t      image3DMaxDepth;
    cl_ulong    localMemSize;
    cl_uint     maxConstantArgs;
    cl_uint     maxConstantBufferSize;
    cl_ulong    maxMemAllocSize;
    size_t      maxParameterSize;
    size_t      maxWorkGroupSize;
    cl_uint     maxWorkItemDimensions;

    size_t *    maxWorkItemSizes;

    char *      extensions;
    size_t      extensionsSize;
    
    AFK_ClDeviceProperties(cl_device_id device);
    virtual ~AFK_ClDeviceProperties();

    /* TODO Method for querying for a particular extension? */
};

std::ostream& operator<<(std::ostream& os, const AFK_ClDeviceProperties& p);

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
 * - Reading up: With cl_gl, I need to create the gl buffer
 * first, so I'm going to have to do plenty of buffer pre-
 * creation.  That's fine.
 * For now I'm going to serialise all OpenCL to a single
 * queue by the lock() and unlock() functions below --
 * maybe I can toy with multiple contexts (should be fine
 * x-thread) later? (TODO?)
 */

class AFK_Computer
{
protected:
    cl_platform_id platform;

    AFK_ClPlatformProperties *platformProps;
    bool platformIsAMD;

    /* The IDs of the devices that I'm using.
     */
    cl_device_id *devices;
    unsigned int devicesSize;

    AFK_ClDeviceProperties *firstDeviceProps;

    cl_context ctxt;
    cl_command_queue q;

    boost::mutex mut;

    /* Helper functions */
#if 0
    void inspectDevices(cl_platform_id platform, cl_device_type deviceType);
#endif
    bool findClGlDevices(cl_platform_id platform);
    void loadProgramFromFile(const AFK_Config *config, struct AFK_ClProgram *p);
public:
    AFK_Computer();
    virtual ~AFK_Computer();

    /* Loads all CL programs from disk.  Call before doing
     * anything else.
     */
    void loadPrograms(const AFK_Config *config);

    /* Returns the first device's detected properties. */
    const AFK_ClDeviceProperties& getFirstDeviceProps(void) const;

    /* To use this class, call the following functions
     * to identify where you want to compute, and then do
     * the rest of the OpenCL yourself.
     */
    bool findKernel(const std::string& kernelName, cl_kernel& o_kernel) const;

    /* Tests the CL version, returning true if it's at
     * least the one you asked for, else false.
     */
    bool testVersion(unsigned int majorVersion, unsigned int minorVersion) const;

    /* Returns true if on an AMD platform, else false. */
    bool isAMD(void) const;

    /* Locks the CL and gives you back the context and
     * queue.  Be quick, enqueue your thing and release
     * it!
     */
    void lock(cl_context& o_ctxt, cl_command_queue& o_q);

    /* Release it when you're done with this. */
    void unlock(void);
};


#endif /* _AFK_COMPUTER_H_ */

