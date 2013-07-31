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
    {   0,  "test.cl"                   },
    {   0,  "vs2SmoothTriangles.cl",    },
    {   0,  "vs_test.cl"                },
    {   0,  ""                          }
};

struct AFK_ClKernel kernels[] = {
    {   0,  "test.cl",                  "vector_add_gpu"                },
    {   0,  "vs2SmoothTriangles.cl",    "vs2SmoothTriangles",           },
    {   0,  "vs_test.cl",               "mangle_vs"                     },
    {   0,  "",                         ""                              }
};


void afk_handleClError(cl_int error)
{
    if (error != CL_SUCCESS)
    {
        std::ostringstream ss;
        ss << "AFK_Computer: Error occurred: " << error;
        throw AFK_Exception(ss.str());
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

/* This helper is used to cram ostensibly-64 bit pointer types from GLX
 * into the 32-bit fields that they actually fit into without causing
 * compiler warnings.
 * Squick.
 */
template<typename SourceType, typename DestType>
DestType firstOf(SourceType s)
{
    union
    {
        SourceType      s;
        DestType        d[sizeof(SourceType) / sizeof(DestType)];
    } u;
    u.s = s;

    if ((sizeof(DestType) * 2) <= sizeof(SourceType))
    {
        assert(u.d[1] == 0);
    }

    return u.d[0];
}

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

#if AFK_GLX
    Display *dpy = glXGetCurrentDisplay();
    GLXContext glxCtx = glXGetCurrentContext();

    const cl_context_properties clGlProperties[] = {
        CL_GL_CONTEXT_KHR,      firstOf<GLXContext, cl_context_properties>(glxCtx),
        CL_GLX_DISPLAY_KHR,     firstOf<Display *, cl_context_properties>(dpy),
        CL_CONTEXT_PLATFORM,    (cl_context_properties)platform,
        0
    }; 
#else
#error "cl_gl for other platforms unimplemented"
#endif

    AFK_CLCHK(clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, NULL, &devicesSize))
    if (devicesSize > 0)
    {
        devices = new cl_device_id[devicesSize];
        AFK_CLCHK(clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, devicesSize, devices, &devicesSize))

        std::cout << "Found " << devicesSize << " GPU devices. " << std::endl;

        /* TODO Try to fall back to un-shared buffers (copying everything)
         * if this fails.  And, err, yeah, that'll probably be wildly
         * slow...  :/
         */
        cl_int error;
        ctxt = clCreateContext(clGlProperties, devicesSize, devices, NULL, NULL, &error);
        afk_handleClError(error);
        return true;
    }
    else return false;
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

    error = clBuildProgram(p->program, devicesSize, devices, NULL, NULL, NULL);
    if (error == CL_SUCCESS || error == CL_BUILD_PROGRAM_FAILURE)
        for (size_t dI = 0; dI < devicesSize; ++dI)
            printBuildLog(std::cout, p->program, devices[dI]);

    afk_handleClError(error);
}

AFK_Computer::AFK_Computer():
    devices(NULL), devicesSize(0), ctxt(0), q(0)
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
            if (kernels[i].kernel) clReleaseKernel(kernels[i].kernel);

        for (unsigned int i = 0; programs[i].filename.size() != 0; ++i)
            if (programs[i].program) clReleaseProgram(programs[i].program);

        if (q) clReleaseCommandQueue(q);
        if (ctxt) clReleaseContext(ctxt);

        delete[] devices;
    }
}

void AFK_Computer::loadPrograms(const std::string& programsDir)
{
    cl_int error;
    std::ostringstream errStream;

    /* Swap to the right directory. */
    if (!afk_pushDir(programsDir, errStream))
        throw AFK_Exception("AFK_Computer: Unable to switch to programs dir: " + errStream.str());

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
        throw AFK_Exception("AFK_Computer: Unable to switch out of programs dir: " + errStream.str());
}

unsigned int AFK_Computer::clGlMaxAllocSize(void) const
{
    /* For now, I'm going to assume you meant the first device.
     * After all, that's all there will be usually,
     * and it's probably not sensible to go overloading
     * other devices with stuff because of data locality
     * issues...?
     */
    unsigned int maxAllocSize;
    size_t maxAllocSizeSize = sizeof(unsigned int);

    AFK_CLCHK(clGetDeviceInfo(devices[0], CL_DEVICE_MAX_MEM_ALLOC_SIZE, maxAllocSizeSize, &maxAllocSize, NULL))
    if (maxAllocSizeSize != sizeof(unsigned int)) throw AFK_Exception("Something weird happened fetching cl_gl max allocsize");
    return maxAllocSize;
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
     * TODO *2: I'm actually temporarily ignoring the mutex
     * here.  I'm only using one thread for CL right now,
     * after all.
     */
    //mut.lock();
    o_ctxt = ctxt;
    o_q = q;
}

void AFK_Computer::unlock(void)
{
    //mut.unlock();
}

