/* AFK (c) Alex Holloway 2013 */

#include <iostream>
#include <sstream>

#include "computer.h"
#include "exception.h"

static void inspectDevices(cl_platform_id platform, cl_device_type deviceType)
{
    int result;
    cl_device_id *devices = NULL;
    unsigned int deviceCount;
    std::ostringstream ss;

    result = clGetDeviceIDs(platform, deviceType, 0, NULL, &deviceCount);
    switch (result)
    {
    case CL_SUCCESS:
        devices = (cl_device_id *)malloc(sizeof(cl_device_id) * deviceCount);
        if (!devices) throw AFK_Exception("Unable to allocate memory to inspect OpenCL devices");
        if ((result = clGetDeviceIDs(platform, deviceType, deviceCount, devices, &deviceCount)) != CL_SUCCESS)
        {
            ss << "AFK_Computer: Failed to enumerate devices for platform " << platform << ": error " <<  result;
            throw AFK_Exception(ss.str());
        }

        for (unsigned int i = 0; i < deviceCount; ++i)
        {
            char *deviceName = NULL;
            size_t deviceNameSize;

            if ((result = clGetDeviceInfo(devices[i], CL_DEVICE_NAME, 0, NULL, &deviceNameSize)) != CL_SUCCESS)
            {
                ss << "AFK_Computer: Failed to get device name size: " << result;
                throw AFK_Exception(ss.str());
            }
            deviceName = (char *)malloc(deviceNameSize);
            if (!deviceName) throw AFK_Exception("Unable to allocate memory for device name");
            if ((result = clGetDeviceInfo(devices[i], CL_DEVICE_NAME, deviceNameSize, deviceName, &deviceNameSize)) != CL_SUCCESS)
            {
                ss << "AFK_Computer: Failed to get device name: " << result;
                throw AFK_Exception(ss.str());
            }

            std::cout << "AFK: Found device: " << devices[i] << " with name " << deviceName << std::endl;

            /* TODO I should stash devices for later use! */
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

AFK_Computer::AFK_Computer()
{
    cl_platform_id *platforms;
    unsigned int platformCount;

    clGetPlatformIDs(0, NULL, &platformCount);
    platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * platformCount);
    if (!platforms) throw AFK_Exception("Unable to allocate memory to inspect OpenCL platforms");
    clGetPlatformIDs(platformCount, platforms, &platformCount);

    std::cout << "AFK: Found " << platformCount << " OpenCL platforms" << std::endl;

    for (unsigned int pI = 0; pI < platformCount; ++pI)
    {
        inspectDevices(platforms[pI], CL_DEVICE_TYPE_GPU);
        inspectDevices(platforms[pI], CL_DEVICE_TYPE_CPU);
    }
}

