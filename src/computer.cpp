/* AFK (c) Alex Holloway 2013 */

#include <cstring>
#include <iostream>
#include <sstream>

#include "computer.hpp"
#include "exception.hpp"


/* Incidental functions */

static void handleError(cl_int error)
{
    if (error != CL_SUCCESS)
    {
        std::ostringstream ss;
        ss << "AFK_Computer: Error occurred: " << error;
        throw new AFK_Exception(ss.str());
    }
}

#define CLCHK(call) \
    { \
        cl_int error = call; \
        if (error != CL_SUCCESS) handleError(error); \
    }

static void printBuildLog(std::ostream& s, cl_program program, cl_device_id device)
{
    char *buildLog;
    size_t buildLogSize;

    CLCHK(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &buildLogSize))
    buildLog = new char[buildLogSize+1];
    CLCHK(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, buildLogSize, buildLog, NULL))
    buildLog[buildLogSize] = '\0'; /* paranoia */
    s << buildLog << std::endl;
    delete[] buildLog;
}

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
        CLCHK(clGetDeviceIDs(platform, deviceType, deviceCount, devices, &deviceCount))

        /* Use the first GPU device, if I have one. */
        if (deviceType == CL_DEVICE_TYPE_GPU && deviceCount > 0)
            activeDevice = devices[0];

        for (unsigned int i = 0; i < deviceCount; ++i)
        {
            char *deviceName = NULL;
            size_t deviceNameSize;

            CLCHK(clGetDeviceInfo(devices[i], CL_DEVICE_NAME, 0, NULL, &deviceNameSize))
            deviceName = (char *)malloc(deviceNameSize);
            if (!deviceName) throw AFK_Exception("Unable to allocate memory for device name");
            CLCHK(clGetDeviceInfo(devices[i], CL_DEVICE_NAME, deviceNameSize, deviceName, &deviceNameSize))

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


/* AFK_Computer implementation */

AFK_Computer::AFK_Computer(unsigned int _qCount):
    activeDevice(0), qCount(_qCount)
{
    cl_platform_id *platforms;
    unsigned int platformCount;

    CLCHK(clGetPlatformIDs(0, NULL, &platformCount))
    platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * platformCount);
    if (!platforms) throw AFK_Exception("Unable to allocate memory to inspect OpenCL platforms");
    CLCHK(clGetPlatformIDs(platformCount, platforms, &platformCount))

    std::cout << "AFK: Found " << platformCount << " OpenCL platforms" << std::endl;

    for (unsigned int pI = 0; pI < platformCount; ++pI)
    {
        inspectDevices(platforms[pI], CL_DEVICE_TYPE_GPU);
        inspectDevices(platforms[pI], CL_DEVICE_TYPE_CPU);
    }

    /* Make up the context and queues. */
    cl_int error;
    ctxt = clCreateContext(0, 1, &activeDevice, NULL, NULL, &error);
    handleError(error);

    q = new cl_command_queue[qCount];
    for (unsigned int qId = 0; qId < qCount; ++qId)
    {
        q[qId] = clCreateCommandQueue(ctxt, activeDevice, 0, &error);
        handleError(error);
    }
}

AFK_Computer::~AFK_Computer()
{
    for (unsigned int qId = 0; qId < qCount; ++qId)
        clReleaseCommandQueue(q[qId]);
    delete[] q;
    clReleaseContext(ctxt);
}

void AFK_Computer::test(unsigned int qId)
{
    cl_int error = 0;
    
    const int size = 10000;
    float* src_a_h = new float[size];
    float* src_b_h = new float[size];
    float* res_h = new float[size];
    // Initialize both vectors
    for (int i = 0; i < size; i++) {
        src_a_h[i] = src_b_h[i] = (float) i;
    }

    cl_mem src_a_b = clCreateBuffer(ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, size * sizeof(float), src_a_h, &error);
    handleError(error);

    cl_mem src_b_b = clCreateBuffer(ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, size * sizeof(float), src_b_h, &error);
    handleError(error);
    
    cl_mem res_b = clCreateBuffer(ctxt, CL_MEM_WRITE_ONLY, size * sizeof(float), NULL, &error);
    handleError(error);
    
    /* TODO Load program from file (like the shaders) */
    const char *source =
"__kernel void vector_add_gpu (__global const float* src_a,"
"                     __global const float* src_b,"
"                     __global float* res,"
"           const int num)"
"{"
"   /* get_global_id(0) returns the ID of the thread in execution."
"   As many threads are launched at the same time, executing the same kernel,"
"   each one will receive a different ID, and consequently perform a different computation.*/"
"   const int idx = get_global_id(0);"
""
"   /* Now each work-item asks itself: ""is my ID inside the vector's range?"""
"   If the answer is YES, the work-item performs the corresponding computation*/"
"   if (idx < num)"
"      res[idx] = src_a[idx] + src_b[idx];"
"}";
    size_t sourceLength = strlen(source);

    cl_program testProgram = clCreateProgramWithSource(
        ctxt, 1, &source, &sourceLength, &error);
    handleError(error);

    CLCHK(clBuildProgram(testProgram, 1, &activeDevice, NULL, NULL, NULL))
            
    printBuildLog(std::cout, testProgram, activeDevice);

    cl_kernel testKernel = clCreateKernel(testProgram, "vector_add_gpu", &error);
    handleError(error);

    CLCHK(clSetKernelArg(testKernel, 0, sizeof(cl_mem), &src_a_b))
    CLCHK(clSetKernelArg(testKernel, 1, sizeof(cl_mem), &src_b_b))
    CLCHK(clSetKernelArg(testKernel, 2, sizeof(cl_mem), &res_b))
    CLCHK(clSetKernelArg(testKernel, 3, sizeof(int), &size))

    /* TODO Tweak the doodahs ? */
    size_t local_ws = 512;
    size_t global_ws = size + (512 - (size % 512));
    CLCHK(clEnqueueNDRangeKernel(q[qId], testKernel, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL))

    float *readBack = new float[size];
    CLCHK(clEnqueueReadBuffer(q[qId], res_b, CL_TRUE, 0, size * sizeof(float), readBack, 0, NULL, NULL))
    /* TODO Verify that read buffer */
    std::cout << "AFK_Computer: Completed execution of test program" << std::endl;

    std::cout << "AFK_Computer: Start of readback: ";
    for (int i = 0; i < 10; ++i)
        std::cout << readBack[i] << " ";
    std::cout << std::endl;

    for (int i = 0; i < size; ++i)
        if (readBack[i] != (src_a_h[i] + src_b_h[i])) throw new AFK_Exception("Failed!");

    std::cout << "AFK_Computer: Test program completed successfully" << std::endl;

    delete[] readBack;

    clReleaseKernel(testKernel);
    clReleaseMemObject(res_b);
    clReleaseMemObject(src_b_b);
    clReleaseMemObject(src_a_b);

    delete[] res_h;
    delete[] src_b_h;
    delete[] src_a_h;
}

