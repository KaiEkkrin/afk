/* AFK (c) Alex Holloway 2013 */

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

#include "computer.hpp"
#include "exception.hpp"
#include "file/readfile.hpp"
#include "landscape_sizes.hpp"


/* The set of known programs, just like the shaders doodah. */

struct AFK_ClProgram programs[] = {
    {   0,  "surface.cl"                },
    {   0,  "terrain.cl"                },
    {   0,  "yreduce.cl"                },
    {   0,  "test.cl"                   },
    {   0,  "vs_test.cl"                },
    {   0,  ""                          }
};

struct AFK_ClKernel kernels[] = {
    {   0,  "surface.cl",               "makeSurface"                   },
    {   0,  "terrain.cl",               "makeTerrain"                   },
    {   0,  "yreduce.cl",               "yReduce"                       },
    {   0,  "test.cl",                  "vector_add_gpu"                },
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

/* For fetching fixed-size device properties. */
template<typename PropType>
void getClDeviceInfoFixed(
    cl_device_id device,
    cl_device_info paramName,
    PropType *field,
    PropType failValue)
{
    cl_int error = clGetDeviceInfo(device, paramName, sizeof(PropType), field, 0);
    if (error != CL_SUCCESS)
    {
        std::cout << "getClDeviceInfoFixed: Couldn't get property for param " << std::dec << paramName << ": " << error << std::endl;
        *field = failValue;
    }
}


/* AFK_ClPlatformProperties implementation */

AFK_ClPlatformProperties::AFK_ClPlatformProperties(cl_platform_id platform)
{
    AFK_CLCHK(clGetPlatformInfo(platform, CL_PLATFORM_VERSION, 0, NULL, &versionStrSize))
    versionStr = new char[versionStrSize];
    AFK_CLCHK(clGetPlatformInfo(platform, CL_PLATFORM_VERSION, versionStrSize, versionStr, &versionStrSize))

    if (sscanf(versionStr, "OpenCL %d.%d", &majorVersion, &minorVersion) != 2)
    {
        std::ostringstream ss;
        ss << "Incomprehensible OpenCL platform version: " << versionStr;
        throw AFK_Exception(ss.str());
    }
}

/* AFK_ClDeviceProperties implementation. */

AFK_ClDeviceProperties::AFK_ClDeviceProperties(cl_device_id device):
    maxWorkItemSizes(NULL)
{
    getClDeviceInfoFixed<cl_ulong>(device, CL_DEVICE_GLOBAL_MEM_SIZE, &globalMemSize, 0);
    getClDeviceInfoFixed<size_t>(device, CL_DEVICE_IMAGE2D_MAX_WIDTH, &image2DMaxWidth, 0);
    getClDeviceInfoFixed<size_t>(device, CL_DEVICE_IMAGE2D_MAX_HEIGHT, &image2DMaxHeight, 0);
    getClDeviceInfoFixed<cl_ulong>(device, CL_DEVICE_LOCAL_MEM_SIZE, &localMemSize, 0);
    getClDeviceInfoFixed<cl_uint>(device, CL_DEVICE_MAX_CONSTANT_ARGS, &maxConstantArgs, 0);
    getClDeviceInfoFixed<cl_uint>(device, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, &maxConstantBufferSize, 0);
    getClDeviceInfoFixed<cl_ulong>(device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, &maxMemAllocSize, 0);
    getClDeviceInfoFixed<size_t>(device, CL_DEVICE_MAX_PARAMETER_SIZE, &maxParameterSize, 0);
    getClDeviceInfoFixed<size_t>(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, &maxWorkGroupSize, 0);
    getClDeviceInfoFixed<cl_uint>(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, &maxWorkItemDimensions, 0);

    if (maxWorkItemDimensions > 0)
    {
        maxWorkItemSizes = new size_t[maxWorkItemDimensions];
        cl_int error = clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, maxWorkItemDimensions * sizeof(size_t), maxWorkItemSizes, NULL);
        if (error != CL_SUCCESS)
        {
            std::cout << "Couldn't get max work item sizes: " << error << std::endl;
            memset(maxWorkItemSizes, 0, maxWorkItemDimensions * sizeof(size_t));
        }
    }
}

AFK_ClDeviceProperties::~AFK_ClDeviceProperties()
{
    if (maxWorkItemSizes) delete[] maxWorkItemSizes;
}

std::ostream& operator<<(std::ostream& os, const AFK_ClDeviceProperties& p)
{
    os << std::dec;
    os << "Global mem size:                 " << p.globalMemSize << std::endl;
    os << "2D image maximum width:          " << p.image2DMaxWidth << std::endl;
    os << "2D image maximum height:         " << p.image2DMaxHeight << std::endl;
    os << "Local mem size:                  " << p.localMemSize << std::endl;
    os << "Max constant args:               " << p.maxConstantArgs << std::endl;
    os << "Max constant buffer size:        " << p.maxConstantBufferSize << std::endl;
    os << "Max mem alloc size:              " << p.maxMemAllocSize << std::endl;
    os << "Max parameter size:              " << p.maxParameterSize << std::endl;
    os << "Max work group size:             " << p.maxWorkGroupSize << std::endl;
    os << "Max work item dimensions:        " << p.maxWorkItemDimensions << std::endl;

    if (p.maxWorkItemSizes)
    {
        os << "Max work item sizes:             [";
        for (unsigned int i = 0; i < p.maxWorkItemDimensions; ++i)
        {
            if (i > 0) os << ", ";
            os << p.maxWorkItemSizes[i];
        }
        os << "]" << std::endl;
    } 

    return os;
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

        char *deviceName;
        size_t deviceNameSize;

        AFK_CLCHK(clGetDeviceInfo(devices[0], CL_DEVICE_NAME, 0, NULL, &deviceNameSize))
        deviceName = new char[deviceNameSize];
        AFK_CLCHK(clGetDeviceInfo(devices[0], CL_DEVICE_NAME, deviceNameSize, deviceName, &deviceNameSize))
        std::cout << "First device is a " << deviceName << std::endl;

        delete[] deviceName;

        firstDeviceProps = new AFK_ClDeviceProperties(devices[0]);
        std::cout << "Device properties: " << std::endl << *firstDeviceProps;

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

void AFK_Computer::loadProgramFromFile(const AFK_Config *config, struct AFK_ClProgram *p)
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

    /* Compiler arguments here... */
    std::ostringstream args;
    if (p->filename == "terrain.cl" || p->filename == "surface.cl")
    {
        AFK_LandscapeSizes lSizes(config->pointSubdivisionFactor);
        args << "-D POINT_SUBDIVISION_FACTOR="  << lSizes.pointSubdivisionFactor << " ";
        args << "-D TDIM="                      << lSizes.tDim                   << " ";
        args << "-D TDIM_START="                << lSizes.tDimStart              << " ";
        args << "-D FEATURE_COUNT_PER_TILE="    << lSizes.featureCountPerTile    << " ";
    }
    else if (p->filename == "yreduce.cl")
    {
        AFK_LandscapeSizes lSizes(config->pointSubdivisionFactor);
        args << "-D TDIM="                      << lSizes.tDim                   << " ";
        args << "-D REDUCE_ORDER="              << lSizes.getReduceOrder()       << " ";
    }

    std::string argsStr = args.str();
    if (argsStr.size() > 0)
        std::cout << "AFK: Passing compiler arguments: " << argsStr << std::endl;
    error = clBuildProgram(p->program, devicesSize, devices, argsStr.size() > 0 ? argsStr.c_str() : NULL, NULL, NULL);
    for (size_t dI = 0; dI < devicesSize; ++dI)
        printBuildLog(std::cout, p->program, devices[dI]);

    afk_handleClError(error);
}

AFK_Computer::AFK_Computer():
    platform(0), platformProps(NULL), devices(NULL), devicesSize(0), firstDeviceProps(NULL), ctxt(0), q(0)
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
        if (findClGlDevices(platforms[pI]))
        {
            platform = platforms[pI];
            platformProps = new AFK_ClPlatformProperties(platform);
            std::cout << "AFK: Using OpenCL platform version " << platformProps->majorVersion << "." << platformProps->minorVersion << std::endl;
            break;
        }
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
        if (firstDeviceProps) delete firstDeviceProps;

        delete[] devices;
    }
}

void AFK_Computer::loadPrograms(const AFK_Config *config)
{
    cl_int error;
    std::ostringstream errStream;

    /* Swap to the right directory. */
    if (!afk_pushDir(config->clProgramsDir, errStream))
        throw AFK_Exception("AFK_Computer: Unable to switch to programs dir: " + errStream.str());

    /* Load all the programs I know about. */
    for (unsigned int i = 0; programs[i].filename.size() != 0; ++i)
        loadProgramFromFile(config, &programs[i]);

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

const AFK_ClDeviceProperties& AFK_Computer::getFirstDeviceProps(void) const
{
    return *firstDeviceProps;
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

bool AFK_Computer::testVersion(unsigned int majorVersion, unsigned int minorVersion) const
{
    return (platformProps->majorVersion > majorVersion ||
        (platformProps->majorVersion == majorVersion && platformProps->minorVersion >= minorVersion));
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

