/* AFK (c) Alex Holloway 2013 */

#include <cstring>
#include <iostream>
#include <sstream>

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#include "computer.hpp"
#include "exception.hpp"
#include "file/readfile.hpp"


/* The set of known programs, just like the shaders doodah. */

struct AFK_ClProgram programs[] = {
    {   0,  "test.cl"           },
    {   0,  ""                  }
};

struct AFK_ClKernel kernels[] = {
    {   0,  "test.cl",              "vector_add_gpu"                },
    {   0,  "",                     ""                              }
};


void afk_handleClError(cl_int error)
{
    if (error != CL_SUCCESS)
    {
        std::ostringstream ss;
        ss << "AFK_Computer: Error occurred: " << error;
        throw new AFK_Exception(ss.str());
    }
}

/* Incidental functions */

static void printBuildLog(std::ostream& s, cl_program program, cl_device_id device)
{
    char *buildLog;
    size_t buildLogSize;

    AFK_CLCHK(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &buildLogSize))
    buildLog = new char[buildLogSize+1];
    AFK_CLCHK(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, buildLogSize, buildLog, NULL))
    buildLog[buildLogSize] = '\0'; /* paranoia */
    s << buildLog << std::endl;
    delete[] buildLog;
}


/* AFK_Computer implementation */

#if 0
void AFK_Computer::inspectDevices(cl_platform_id platform, cl_device_type deviceType)
{
    cl_int result;
    cl_device_id *devices = NULL;
    unsigned int deviceCount;
    std::ostringstream ss;

    result = clGetDeviceIDs(platform, deviceType, 0, NULL, &deviceCount);
    switch (result)
    {
    case CL_SUCCESS:
        devices = (cl_device_id *)malloc(sizeof(cl_device_id) * deviceCount);
        if (!devices) throw AFK_Exception("Unable to allocate memory to inspect OpenCL devices");
        AFK_CLCHK(clGetDeviceIDs(platform, deviceType, deviceCount, devices, &deviceCount))

        /* Use the first GPU device, if I have one. */
        if (deviceType == CL_DEVICE_TYPE_GPU && deviceCount > 0)
            activeDevice = devices[0];

        for (unsigned int i = 0; i < deviceCount; ++i)
        {
            char *deviceName = NULL;
            size_t deviceNameSize;

            AFK_CLCHK(clGetDeviceInfo(devices[i], CL_DEVICE_NAME, 0, NULL, &deviceNameSize))
            deviceName = (char *)malloc(deviceNameSize);
            if (!deviceName) throw AFK_Exception("Unable to allocate memory for device name");
            AFK_CLCHK(clGetDeviceInfo(devices[i], CL_DEVICE_NAME, deviceNameSize, deviceName, &deviceNameSize))

            std::cout << "AFK: Found device: " << devices[i] << " with name " << deviceName << std::endl;

            char *extList = NULL;
            size_t extListSize;

            AFK_CLCHK(clGetDeviceInfo(devices[i], CL_DEVICE_EXTENSIONS, 0, NULL, &extListSize))
            extList = (char *)malloc(extListSize);
            if (!extList) throw AFK_Exception("Unable to allocate memory for CL extensions");
            AFK_CLCHK(clGetDeviceInfo(devices[i], CL_DEVICE_EXTENSIONS, extListSize, extList, &extListSize))

            std::cout << "AFK: Device has extensions: " << extList << std::endl;

            if (deviceType == CL_DEVICE_TYPE_GPU && i == 0)
                std::cout << "AFK: Setting this CL device as the active device" << std::endl;

            free(deviceName);
            free(extList);
        }

        free(devices);
        break;

    case CL_DEVICE_NOT_FOUND:
        std::cout << "No devices of type " << deviceType << " found" << std::endl;
        break;

    default:
        ss << "AFK_Computer: Failed to get device IDs: " << result;
        throw AFK_Exception(ss.str());
    }
}
#endif

bool AFK_Computer::findClGlDevices(cl_platform_id platform)
{
    char *platformName;
    size_t platformNameSize;

    AFK_CLCHK(clGetPlatformInfo(
        platform,
        CL_PLATFORM_NAME,
        0, NULL, &platformNameSize))
    platformName = new char[platformNameSize];
    AFK_CLCHK(clGetPlatformInfo(
        platform,
        CL_PLATFORM_NAME,
        platformNameSize, platformName, &platformNameSize))

    std::cout << "Finding cl_gl devices for platform " << platformName << std::endl;
    delete[] platformName;

    /* First, get this platform's info, make sure it
     * supports cl_gl sharing, and find the cl_gl
     * context info function.
     */
    char *extensions;
    size_t extensionsSize;

    AFK_CLCHK(clGetPlatformInfo(
        platform,
        CL_PLATFORM_EXTENSIONS,
        0, NULL, &extensionsSize))
    extensions = new char[extensionsSize];
    AFK_CLCHK(clGetPlatformInfo(
        platform,
        CL_PLATFORM_EXTENSIONS,
        extensionsSize, extensions, &extensionsSize))

    std::cout << "Platform supports extensions: " << extensions << std::endl;

    bool clGlSharingSupported = false;
    boost::char_separator<char> sep(" ");
    std::string extensionsStr(extensions);
    boost::tokenizer<boost::char_separator<char> > extTok(extensionsStr, sep);
    BOOST_FOREACH(const std::string& ext, extTok)
    {
        if (ext == "cl_khr_gl_sharing")
        {
            std::cout << "Platform supports cl_khr_gl_sharing" << std::endl;
            clGlSharingSupported = true;
        }
    }

    delete[] extensions;

    if (!clGlSharingSupported) return false;

    /* Find the cl_gl context info function */
    cl_int (*clGetGLContextInfoKHRFunc)(
        const cl_context_properties *,
        cl_gl_context_info,
        size_t, void *, size_t *
        ) = clGetExtensionFunctionAddress("clGetGLContextInfoKHR");

#if AFK_GLX
    /* TODO YUCK YUCK -- this is requiring -fpermissive --
     * looks like it's trying to cram 64 bit pointers into
     * 32 bits, wtf!
     */
    Display *dpy = glXGetCurrentDisplay();
    GLXContext glxCtx = glXGetCurrentContext();

    const cl_context_properties properties[] = {
        CL_GL_CONTEXT_KHR,      (cl_context_properties)glxCtx,
        CL_GLX_DISPLAY_KHR,     (cl_context_properties)dpy,
        CL_CONTEXT_PLATFORM,    platform,
        0
    }; 
    cl_gl_context_info clGlParamName = CL_DEVICES_FOR_GL_CONTEXT_KHR;
#else
#error "cl_gl for other platforms unimplemented"
#endif

    /* TODO Maybe this function just doesn't like filling out
     * the size field
     */
#if 0
    AFK_CLCHK((*clGetGLContextInfoKHRFunc)(
        properties,
        clGlParamName,
        0, NULL, &devicesSize))
    if (devicesSize > 0)
    {
#else
        devicesSize = 8;
#endif
        devices = new cl_device_id[devicesSize];
        AFK_CLCHK((*clGetGLContextInfoKHRFunc)(
            properties,
            clGlParamName,
            devicesSize, devices, &devicesSize))

        for (size_t dI = 0; dI < devicesSize; ++dI)
        {
            char *deviceName = NULL;
            size_t deviceNameSize;
            cl_int error;

            error = clGetDeviceInfo(devices[dI], CL_DEVICE_NAME, 0, NULL, &deviceNameSize);
            if (error == CL_SUCCESS)
            {
                deviceName = new char[deviceNameSize];
                AFK_CLCHK(clGetDeviceInfo(devices[dI], CL_DEVICE_NAME, deviceNameSize, deviceName, &deviceNameSize))

                std::cout << "AFK: Found cl_gl device: " << deviceName << std::endl;
            }

            delete[] deviceName;
        }

        /* TODO Is there a problem with using all the devices?
         * (Lots of buffers to move about...)  Will I even
         * get more than one?
         */
        cl_int error;
        ctxt = clCreateContext(properties, devices, devicesSize, NULL, 0, &error);
        afk_handleClError(error);
        return true;
#if 0
    }
    else
    {
        std::cout << "No cl_gl devices found for platform " << platform << std::endl;
        return false;
    }
#endif
}

void AFK_Computer::loadProgramFromFile(struct AFK_ClProgram *p)
{
    char *source;
    size_t sourceLength;
    std::ostringstream errStream;
    cl_int error;

    std::cout << "AFK: Loading CL program: " << p->filename << std::endl;

    if (!afk_readFileContents(p->filename, &source, &sourceLength, errStream))
        throw AFK_Exception("AFK_Computer: " + errStream.str());

    p->program = clCreateProgramWithSource(ctxt, 1, (const char **)&source, &sourceLength, &error);
    afk_handleClError(error);

    AFK_CLCHK(clBuildProgram(p->program, devicesSize, devices, NULL, NULL, NULL))
    for (size_t dI = 0; dI < devicesSize; ++dI)
        printBuildLog(std::cout, p->program, devices[dI]);
}

AFK_Computer::AFK_Computer():
    devices(NULL), devicesSize(0)
{
    cl_platform_id *platforms;
    unsigned int platformCount;

    AFK_CLCHK(clGetPlatformIDs(0, NULL, &platformCount))
    platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * platformCount);
    if (!platforms) throw AFK_Exception("Unable to allocate memory to inspect OpenCL platforms");
    AFK_CLCHK(clGetPlatformIDs(platformCount, platforms, &platformCount))

    std::cout << "AFK: Found " << platformCount << " OpenCL platforms" << std::endl;

    for (unsigned int pI = 0; pI < platformCount; ++pI)
    {
        //inspectDevices(platforms[pI], CL_DEVICE_TYPE_GPU);
        //inspectDevices(platforms[pI], CL_DEVICE_TYPE_CPU);
        if (findClGlDevices(platforms[pI])) break;
    }

    if (!devices) throw AFK_Exception("No cl_gl devices found");

    /* TODO Multiple queues for multiple devices? */
    cl_int error;
    q = clCreateCommandQueue(ctxt, devices[0], 0, &error);
    afk_handleClError(error);
}

AFK_Computer::~AFK_Computer()
{
    if (devices)
    {
        for (unsigned int i = 0; kernels[i].kernelName.size() != 0; ++i)
            clReleaseKernel(kernels[i].kernel);

        for (unsigned int i = 0; programs[i].filename.size() != 0; ++i)
            clReleaseProgram(programs[i].program);

        clReleaseCommandQueue(q);
        clReleaseContext(ctxt);

        delete[] devices;
    }
}

void AFK_Computer::loadPrograms(const std::string& programsDir)
{
    cl_int error;
    std::ostringstream errStream;

    /* Swap to the right directory. */
    if (!afk_pushDir(programsDir, errStream))
        throw new AFK_Exception("AFK_Computer: Unable to switch to programs dir: " + errStream.str());

    /* Load all the programs I know about. */
    for (unsigned int i = 0; programs[i].filename.size() != 0; ++i)
        loadProgramFromFile(&programs[i]);

    /* ...and all the kernels... */
    for (unsigned int i = 0; kernels[i].kernelName.size() != 0; ++i)
    {
        bool identified = false;
        for (unsigned int j = 0; programs[j].filename.size() != 0; ++j)
        {
            if (kernels[i].programFilename == programs[j].filename)
            {
                kernels[i].kernel = clCreateKernel(programs[j].program, kernels[i].kernelName.c_str(), &error);
                afk_handleClError(error);
                identified = true;
            }
        }

        if (!identified) throw AFK_Exception("AFK_Computer: Unidentified compute kernel: " + kernels[i].kernelName);
    }

    /* Swap back out again. */
    if (!afk_popDir(errStream))
        throw new AFK_Exception("AFK_Computer: Unable to switch out of programs dir: " + errStream.str());
}

bool AFK_Computer::findKernel(const std::string& kernelName, cl_kernel& o_kernel) const
{
    bool found = false;
    for (unsigned int i = 0; !found && kernels[i].kernelName.size() != 0; ++i)
    {
        if (kernels[i].kernelName == kernelName)
        {
            o_kernel = kernels[i].kernel;
            found = true;
        }
    }

    return found;
}

void AFK_Computer::lock(cl_context& o_ctxt, cl_command_queue& o_q)
{
    /* TODO Multiple devices and queues: can I identify
     * the least busy device here, and pass out its
     * queue?
     */
    mut.lock();
    o_ctxt = ctxt;
    o_q = q;
}

void AFK_Computer::unlock(void)
{
    mut.unlock();
}

