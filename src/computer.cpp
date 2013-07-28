/* AFK (c) Alex Holloway 2013 */

#include <cstring>
#include <iostream>
#include <sstream>

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
            if (deviceType == CL_DEVICE_TYPE_GPU && i == 0)
                std::cout << "AFK: Setting this CL device as the active device" << std::endl;

            free(deviceName);
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

    AFK_CLCHK(clBuildProgram(p->program, 1, &activeDevice, NULL, NULL, NULL))
    printBuildLog(std::cout, p->program, activeDevice);
}

AFK_Computer::AFK_Computer(unsigned int _qCount):
    activeDevice(0), qCount(_qCount)
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
        inspectDevices(platforms[pI], CL_DEVICE_TYPE_GPU);
        inspectDevices(platforms[pI], CL_DEVICE_TYPE_CPU);
    }

    /* Make up the context and queues. */
    cl_int error;
    ctxt = clCreateContext(0, 1, &activeDevice, NULL, NULL, &error);
    afk_handleClError(error);

    q = new cl_command_queue[qCount];
    for (unsigned int qId = 0; qId < qCount; ++qId)
    {
        q[qId] = clCreateCommandQueue(ctxt, activeDevice, 0, &error);
        afk_handleClError(error);
    }
}

AFK_Computer::~AFK_Computer()
{
    for (unsigned int i = 0; kernels[i].kernelName.size() != 0; ++i)
        clReleaseKernel(kernels[i].kernel);

    for (unsigned int i = 0; programs[i].filename.size() != 0; ++i)
        clReleaseProgram(programs[i].program);

    for (unsigned int qId = 0; qId < qCount; ++qId)
        clReleaseCommandQueue(q[qId]);
    delete[] q;
    clReleaseContext(ctxt);
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

cl_context AFK_Computer::getContext(void) const
{
    return ctxt;
}

cl_command_queue AFK_Computer::getCommandQueue(unsigned int threadId) const
{
    if (threadId >= qCount)
    {
        std::ostringstream ss;
        ss << "Requested command queue with thread ID out of range: " << threadId << " (" << qCount << ")";
        throw new AFK_Exception(ss.str());
    }

    return q[threadId];
}

