/* AFK
 * Copyright (C) 2013, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#ifndef _AFK_COMPUTER_H_
#define _AFK_COMPUTER_H_

#include "afk.hpp"

#include <iostream>
#include <string>
#include <vector>

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>

#include "config.hpp"
#include "def.hpp"
#include "exception.hpp"
#include "ocl_shim.hpp"

class AFK_Computer;

/* This defines a list of programs that I know about. */

struct AFK_ClProgram
{
    cl_program program;
    std::string programName; /* friendly name referred to below */
    std::vector<std::string> filenames;
};

/* This defines a list of all the kernels I know about */
struct AFK_ClKernel
{
    cl_kernel kernel;
    std::string programName;
    std::string kernelName;
};

/* Handles an OpenCL error. */
void afk_handleClError(cl_int error, const char *_file, const int _line);

#define AFK_CLCHK(call) \
    { \
        cl_int error = call; \
        if (error != CL_SUCCESS) afk_handleClError(error, __FILE__, __LINE__); \
    }

#define AFK_HANDLE_CL_ERROR(error) afk_handleClError(error, __FILE__, __LINE__)

/* Collections of properties that might be useful.
 */
class AFK_ClPlatformProperties
{
public:
    char *versionStr;
    size_t versionStrSize;
    unsigned int majorVersion;
    unsigned int minorVersion;

    AFK_ClPlatformProperties(AFK_Computer *computer, cl_platform_id platform);
    virtual ~AFK_ClPlatformProperties();
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

    std::string extensions;
    
    AFK_ClDeviceProperties(AFK_Computer *computer, cl_device_id device);
    virtual ~AFK_ClDeviceProperties();

    bool        supportsExtension(const std::string& ext) const;
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
 * x-thread) later?
 *
 * TODO: Right now I'm going to remove the concurrency control, it's
 * meaningless because only the main thread is calling into the
 * OpenCL anyway (which is OK, it's not CPU bound).  I'll no
 * doubt end up with separate contexts if I use several threads.
 */

/* This utility wraps programBuilt() so that the CL can call it
 * as a callback function.
 */
void afk_programBuiltNotify(cl_program program, void *user_data);

class AFK_Computer
{
protected:
    /* The set of known programs, just like the shaders doodah. */

    std::vector<struct AFK_ClProgram> programs = {
        {   0,  "landscape_surface",    { "landscape_surface.cl" }, },
        {   0,  "landscape_terrain",    { "landscape_terrain.cl" }, },
        {   0,  "landscape_yreduce",    { "landscape_yreduce.cl" }, },
        {   0,  "shape_3dedge",         { "fake3d.cl", "shape_3dedge.cl" }, },
        {   0,  "shape_3dvapour_feature",   { "fake3d.cl", "shape_3dvapour.cl", "shape_3dvapour_feature.cl" }, },
        {   0,  "shape_3dvapour_normal",    { "fake3d.cl", "shape_3dvapour.cl", "shape_3dvapour_normal.cl" }, }
    };

    std::vector<struct AFK_ClKernel> kernels = { 
        {   0,  "landscape_surface",        "makeLandscapeSurface"          },
        {   0,  "landscape_terrain",        "makeLandscapeTerrain"          },
        {   0,  "landscape_yreduce",        "makeLandscapeYReduce"          },
        {   0,  "shape_3dedge",             "makeShape3DEdge"               },
        {   0,  "shape_3dvapour_feature",   "makeShape3DVapourFeature"      },
        {   0,  "shape_3dvapour_normal",    "makeShape3DVapourNormal"       }
    };

    cl_platform_id platform;
    AFK_ClPlatformProperties *platformProps;
    bool platformIsAMD;
    bool async;

    /* The IDs of the devices that I'm using.
     */
    cl_device_id *devices;
    unsigned int devicesSize;

    AFK_ClDeviceProperties *firstDeviceProps;

    /* For tracking CL program compilation. */
    boost::condition_variable buildCond;
    boost::mutex buildMut;
    unsigned int stillBuilding;

    cl_context ctxt;
    cl_command_queue q;

    /* Helper functions */
    bool findClGlDevices(cl_platform_id platform);
    void loadProgramFromFiles(const AFK_Config *config, std::vector<struct AFK_ClProgram>::iterator& p);
    void programBuilt(void);
    void waitForBuild(void);
    void printBuildLog(std::ostream& os, const struct AFK_ClProgram& p, cl_device_id device);
public:
    /* TODO: Make this protected, and make a cleaner wrapper around the
     * OpenCL functions for the rest of AFK to use.
     * (Abstract away from all those event lists, etc.)
     */
    AFK_OclShim oclShim;

    AFK_Computer(const AFK_Config *config);
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

    /* Returns true if we should use asynchronous queueing,
     * else false.
     */
    bool useAsync(void) const;

    /* Returns true if we should use fake 3D images,
     * else false.
     */
    bool useFake3DImages(const AFK_Config *config) const;

    /* For fetching fixed-size device properties. */
    template<typename PropType>
    void getClDeviceInfoFixed(
        cl_device_id device,
        cl_device_info paramName,
        PropType *field,
        PropType failValue)
    {
        cl_int error = oclShim.GetDeviceInfo()(device, paramName, sizeof(PropType), field, 0);
        if (error != CL_SUCCESS)
        {
            std::cout << "getClDeviceInfoFixed: Couldn't get property for param " << std::dec << paramName << ": " << error << std::endl;
            *field = failValue;
        }
    }

    /* Locks the CL and gives you back the context and
     * queue.  Be quick, enqueue your thing and release
     * it!
     * TODO: Really just a convenient way of accessing the
     * context and queue right now: see comment at top of
     * class description.  Also, current usage implies
     * re-entrancy of any actual lock that might be here.
     * I really ought to overhaul this interface.
     */
    void lock(cl_context& o_ctxt, cl_command_queue& o_q);

    /* Release it when you're done with this. */
    void unlock(void);

    friend void afk_programBuiltNotify(cl_program program, void *user_data);
};


#endif /* _AFK_COMPUTER_H_ */

