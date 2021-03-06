/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#include <cstdio>
#include <cstring>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/tokenizer.hpp>

#include "compute_queue.hpp"
#include "computer.hpp"
#include "exception.hpp"
#include "file/logstream.hpp"
#include "file/readfile.hpp"
#include "landscape_sizes.hpp"
#include "shape_sizes.hpp"


AFK_ClProgram::AFK_ClProgram(const std::string& _programName, const std::initializer_list<std::string>& _filenames) :
program(0), programName(_programName), filenames(_filenames)
{
}

AFK_ClKernel::AFK_ClKernel(const std::string& _programName, const std::string& _kernelName) :
kernel(0), programName(_programName), kernelName(_kernelName)
{
}

void afk_handleClError(cl_int error, const char *_file, const int _line)
{
    if (error != CL_SUCCESS)
    {
#if 1
        afk_out << "AFK_Computer: Error " << std::dec << error << " occurred at " << _file << ":" << _line << std::endl;
        assert(error == CL_SUCCESS);
#else
        std::ostringstream ss;
        ss << "AFK_Computer: Error " << std::dec << error << " occurred at " << _file << ":" << _line;
        throw AFK_Exception(ss.str());
#endif
    }
}

/* Incidental functions */


/* AFK_ClPlatformProperties implementation */

AFK_ClPlatformProperties::AFK_ClPlatformProperties(AFK_Computer *computer, cl_platform_id platform):
    versionStr(nullptr)
{
    AFK_CLCHK(computer->oclShim.GetPlatformInfo()(platform, CL_PLATFORM_VERSION, 0, NULL, &versionStrSize))
    versionStr = new char[versionStrSize];
    AFK_CLCHK(computer->oclShim.GetPlatformInfo()(platform, CL_PLATFORM_VERSION, versionStrSize, versionStr, &versionStrSize))

#ifdef _WIN32
    if (sscanf_s(versionStr, "OpenCL %d.%d", &majorVersion, &minorVersion) != 2)
#else
    if (sscanf(versionStr, "OpenCL %d.%d", &majorVersion, &minorVersion) != 2)
#endif
    {
        std::ostringstream ss;
        ss << "Incomprehensible OpenCL platform version: " << versionStr;
        throw AFK_Exception(ss.str());
    }
}

AFK_ClPlatformProperties::~AFK_ClPlatformProperties()
{
    if (versionStr) delete[] versionStr;
}

/* AFK_ClDeviceProperties implementation. */

AFK_ClDeviceProperties::AFK_ClDeviceProperties(AFK_Computer *computer, cl_device_id device):
    maxWorkItemSizes(nullptr)
{
    computer->getClDeviceInfoFixed<cl_ulong>(device, CL_DEVICE_GLOBAL_MEM_SIZE, &globalMemSize, 0);
    computer->getClDeviceInfoFixed<size_t>(device, CL_DEVICE_IMAGE2D_MAX_WIDTH, &image2DMaxWidth, 0);
    computer->getClDeviceInfoFixed<size_t>(device, CL_DEVICE_IMAGE2D_MAX_HEIGHT, &image2DMaxHeight, 0);
    computer->getClDeviceInfoFixed<size_t>(device, CL_DEVICE_IMAGE3D_MAX_WIDTH, &image3DMaxWidth, 0);
    computer->getClDeviceInfoFixed<size_t>(device, CL_DEVICE_IMAGE3D_MAX_HEIGHT, &image3DMaxHeight, 0);
    computer->getClDeviceInfoFixed<size_t>(device, CL_DEVICE_IMAGE3D_MAX_DEPTH, &image3DMaxDepth, 0);
    computer->getClDeviceInfoFixed<cl_ulong>(device, CL_DEVICE_LOCAL_MEM_SIZE, &localMemSize, 0);
    computer->getClDeviceInfoFixed<cl_uint>(device, CL_DEVICE_MAX_CONSTANT_ARGS, &maxConstantArgs, 0);
    computer->getClDeviceInfoFixed<cl_uint>(device, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, &maxConstantBufferSize, 0);
    computer->getClDeviceInfoFixed<cl_ulong>(device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, &maxMemAllocSize, 0);
    computer->getClDeviceInfoFixed<size_t>(device, CL_DEVICE_MAX_PARAMETER_SIZE, &maxParameterSize, 0);
    computer->getClDeviceInfoFixed<size_t>(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, &maxWorkGroupSize, 0);
    computer->getClDeviceInfoFixed<cl_uint>(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, &maxWorkItemDimensions, 0);

    if (maxWorkItemDimensions > 0)
    {
        maxWorkItemSizes = new size_t[maxWorkItemDimensions];
        cl_int error = computer->oclShim.GetDeviceInfo()(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, maxWorkItemDimensions * sizeof(size_t), maxWorkItemSizes, NULL);
        if (error != CL_SUCCESS)
        {
            afk_out << "Couldn't get max work item sizes: " << error << std::endl;
            memset(maxWorkItemSizes, 0, maxWorkItemDimensions * sizeof(size_t));
        }
    }

    size_t extArrSize;
    cl_int error;
    error = computer->oclShim.GetDeviceInfo()(device, CL_DEVICE_EXTENSIONS, 0, NULL, &extArrSize);
    if (error == CL_SUCCESS)
    {
        char *extArr = new char[extArrSize];
        error = computer->oclShim.GetDeviceInfo()(device, CL_DEVICE_EXTENSIONS, extArrSize, extArr, NULL);
        if (error == CL_SUCCESS)
        {
            extensions = std::string(extArr);
        }

        delete[] extArr;
    }
    if (error != CL_SUCCESS)
    {
        afk_out << "Couldn't get extensions: " << error << std::endl;
    }
}

AFK_ClDeviceProperties::~AFK_ClDeviceProperties()
{
    if (maxWorkItemSizes) delete[] maxWorkItemSizes;
}

bool AFK_ClDeviceProperties::supportsExtension(const std::string& ext) const
{
    boost::char_separator<char> spcSep(" ");
    boost::tokenizer<boost::char_separator<char> > extTok(extensions, spcSep);
    for (auto testExt : extTok)
    {
        if (testExt == ext) return true;
    }

    return false;
}

std::ostream& operator<<(std::ostream& os, const AFK_ClDeviceProperties& p)
{
    os << std::dec;
    os << "Global mem size:                 " << p.globalMemSize << std::endl;
    os << "2D image maximum width:          " << p.image2DMaxWidth << std::endl;
    os << "2D image maximum height:         " << p.image2DMaxHeight << std::endl;
    os << "3D image maximum width:          " << p.image3DMaxWidth << std::endl;
    os << "3D image maximum height:         " << p.image3DMaxHeight << std::endl;
    os << "3D image maximum depth:          " << p.image3DMaxDepth << std::endl;
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

    os << "Extensions:                      " << p.extensions << std::endl;

    return os;
}


/* AFK_Computer implementation */

void afk_programBuiltNotify(cl_program program, void *user_data)
{
    ((AFK_Computer *)user_data)->programBuilt();
}

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

bool AFK_Computer::findClGlDevices(AFK_ConfigSettings& settings, cl_platform_id platform)
{
    char *platformName;
    size_t platformNameSize;

    AFK_CLCHK(oclShim.GetPlatformInfo()(
        platform,
        CL_PLATFORM_NAME,
        0, NULL, &platformNameSize))
    platformName = new char[platformNameSize];
    AFK_CLCHK(oclShim.GetPlatformInfo()(
        platform,
        CL_PLATFORM_NAME,
        platformNameSize, platformName, &platformNameSize))

    platformIsAMD = (strstr(platformName, "AMD") != NULL);

    if (platformIsAMD) afk_out << "AMD platform detected!" << std::endl;
    afk_out << "Finding OpenCL devices for platform " << platformName << std::endl;
    delete[] platformName;

    if (oclPlatformExtensionShim) delete oclPlatformExtensionShim;
    oclPlatformExtensionShim = new AFK_OclPlatformExtensionShim(platform, &oclShim);

#if AFK_GLX
    Display *dpy = glXGetCurrentDisplay();
    GLXContext glxCtx = glXGetCurrentContext();

    cl_context_properties clGlProperties[] = {
        CL_GL_CONTEXT_KHR,      firstOf<GLXContext, cl_context_properties>(glxCtx),
        CL_GLX_DISPLAY_KHR,     firstOf<Display *, cl_context_properties>(dpy),
        CL_CONTEXT_PLATFORM,    (cl_context_properties)platform,
        0
    }; 
#endif

#if AFK_WGL
    cl_context_properties clGlProperties[] = {
        CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
        CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
        0
    };
#endif

    if (settings.clGlSharing)
    {
        devicesSize = 1;
        devices = new cl_device_id[devicesSize];

        try
        {
            AFK_CLCHK(oclPlatformExtensionShim->GetGLContextInfoKHR()(clGlProperties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), devices, NULL))
            afk_out << "Found a supported cl_gl device." << std::endl;
        }
        catch (AFK_Exception&)
        {
            afk_out << "Platform doesn't appear to support cl_gl, falling back." << std::endl;
            delete[] devices;
            devices = nullptr;
            settings.clGlSharing.set(false);
        }
    }

    if (!settings.clGlSharing)
    {
        AFK_CLCHK(oclShim.GetDeviceIDs()(platform, CL_DEVICE_TYPE_GPU, 0, NULL, &devicesSize))

        if (devicesSize > 0)
        {
            devices = new cl_device_id[devicesSize];
            AFK_CLCHK(oclShim.GetDeviceIDs()(platform, CL_DEVICE_TYPE_GPU, devicesSize, devices, &devicesSize))
            afk_out << "Found " << devicesSize << " supported OpenCL devices. " << std::endl;
        }
    }

    if (devicesSize > 0)
    {

        char *deviceName;
        size_t deviceNameSize;

        AFK_CLCHK(oclShim.GetDeviceInfo()(devices[0], CL_DEVICE_NAME, 0, NULL, &deviceNameSize))
        deviceName = new char[deviceNameSize];
        AFK_CLCHK(oclShim.GetDeviceInfo()(devices[0], CL_DEVICE_NAME, deviceNameSize, deviceName, &deviceNameSize))
        afk_out << "First device is a " << deviceName << std::endl;

        delete[] deviceName;

        firstDeviceProps = new AFK_ClDeviceProperties(this, devices[0]);
        afk_out << "Device properties: " << std::endl << *firstDeviceProps;

        cl_int error;
        ctxt = oclShim.CreateContext()(clGlProperties, static_cast<cl_uint>(devicesSize), devices, NULL, NULL, &error);
        AFK_HANDLE_CL_ERROR(error);
        return true;
    }
    else return false;
}

void AFK_Computer::loadProgramFromFiles(const AFK_ConfigSettings& settings, std::vector<AFK_ClProgram>::iterator& p)
{
    char **sources;
    size_t *sourceLengths;
    std::ostringstream errStream;
    cl_int error;

    size_t sourceCount = p->filenames.size();

    assert(sourceCount > 0);
    sources = new char *[sourceCount];
    sourceLengths = new size_t[sourceCount];

    for (size_t s = 0; s < sourceCount; ++s)
    {
        afk_out << "AFK: Loading file for CL program " << p->programName << ": " << p->filenames[s] << std::endl;

        if (!afk_readFileContents(p->filenames[s], &sources[s], &sourceLengths[s], errStream))
            throw AFK_Exception("AFK_Computer: " + errStream.str());
    }

    p->program = oclShim.CreateProgramWithSource()(ctxt, static_cast<cl_uint>(sourceCount), (const char **)sources, sourceLengths, &error);
    AFK_HANDLE_CL_ERROR(error);

    for (size_t s = 0; s < sourceCount; ++s) free(sources[s]);
    delete[] sources;
    delete[] sourceLengths;

    /* Compiler arguments here... */
    std::ostringstream args;
    AFK_LandscapeSizes lSizes(settings);
    AFK_ShapeSizes sSizes(settings);

    if (boost::starts_with(p->programName, "landscape_"))
    {
        args << "-D SUBDIVISION_FACTOR="        << lSizes.subdivisionFactor      << " ";
        args << "-D POINT_SUBDIVISION_FACTOR="  << lSizes.pointSubdivisionFactor << " ";
        args << "-D TDIM="                      << lSizes.tDim                   << " ";
        args << "-D TDIM_START="                << lSizes.tDimStart              << " ";
        args << "-D FEATURE_COUNT_PER_TILE="    << lSizes.featureCountPerTile    << " ";
        args << "-D REDUCE_ORDER="              << lSizes.getReduceOrder()       << " ";
    }
    else if (boost::starts_with(p->programName, "shape_"))
    {
        args << "-D SUBDIVISION_FACTOR="        << sSizes.subdivisionFactor      << " ";
        args << "-D POINT_SUBDIVISION_FACTOR="  << sSizes.pointSubdivisionFactor << " ";
        args << "-D VDIM="                      << sSizes.vDim                   << " ";
        args << "-D EDIM="                      << sSizes.eDim                   << " ";
        args << "-D TDIM="                      << sSizes.tDim                   << " ";
        args << "-D FEATURE_COUNT_PER_CUBE="    << sSizes.featureCountPerCube    << " ";
        args << "-D FEATURE_MAX_SIZE="          << sSizes.featureMaxSize         << " ";
        args << "-D FEATURE_MIN_SIZE="          << sSizes.featureMinSize         << " ";
        args << "-D THRESHOLD="                 << sSizes.edgeThreshold          << " ";
        args << "-D REDUCE_ORDER="              << sSizes.getReduceOrder()       << " ";
        args << "-D LAYERS="                    << sSizes.layers                 << " ";
        args << "-D LAYER_BITNESS="             << sSizes.layerBitness           << " ";
        if (useFake3DImages(settings))
            args << "-D AFK_FAKE3D=1 ";
        else
            args << "-D AFK_FAKE3D=0 ";
    }

    args << "-cl-mad-enable -cl-strict-aliasing -Werror";

    std::string argsStr = args.str();
    if (argsStr.size() > 0)
        afk_out << "AFK: Passing compiler arguments: " << argsStr << std::endl;
    error = oclShim.BuildProgram()(
        p->program,
        static_cast<cl_uint>(devicesSize),
        devices,
        argsStr.size() > 0 ? argsStr.c_str() : NULL,
        afk_programBuiltNotify,
        this);

    AFK_HANDLE_CL_ERROR(error);
}

void AFK_Computer::programBuilt(void)
{
    std::unique_lock<std::mutex> lock(buildMut);
    --stillBuilding;
    buildCond.notify_all();
}

void AFK_Computer::waitForBuild(void)
{
    std::unique_lock<std::mutex> lock(buildMut);
    while (stillBuilding > 0)
    {
        buildCond.wait(lock);
    }
}

void AFK_Computer::printBuildLog(std::ostream& s, const AFK_ClProgram& p, cl_device_id device)
{
    char *buildLog;
    size_t buildLogSize;

    AFK_CLCHK(oclShim.GetProgramBuildInfo()(p.program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &buildLogSize))
    buildLog = new char[buildLogSize+1];
    AFK_CLCHK(oclShim.GetProgramBuildInfo()(p.program, device, CL_PROGRAM_BUILD_LOG, buildLogSize, buildLog, NULL))
    buildLog[buildLogSize] = '\0'; /* paranoia */

    s << "--- Build log for " << p.programName << " ---" << std::endl;
    s << buildLog << std::endl << std::endl;
    delete[] buildLog;
}

AFK_Computer::AFK_Computer(AFK_ConfigSettings& settings):
    platform(0),
    platformProps(NULL),
    useEvents(settings.clUseEvents),
    devices(NULL),
    devicesSize(0),
    firstDeviceProps(NULL),
    ctxt(0),
    kernelQueue(nullptr),
    readQueue(nullptr),
    writeQueue(nullptr),
    oclShim(settings),
    oclPlatformExtensionShim(nullptr)
{
    cl_platform_id *platforms;
    unsigned int platformCount;

    programs = {
        AFK_ClProgram("landscape_surface", { "landscape_surface.cl" }),
        AFK_ClProgram("landscape_terrain", { "landscape_terrain.cl" }),
        AFK_ClProgram("landscape_yreduce", { "landscape_yreduce.cl" }),
#if AFK_RENDER_ENTITIES
        AFK_ClProgram("shape_3dedge", { "fake3d.cl", "shape_3dedge.cl" }),
        AFK_ClProgram("shape_3dvapour_dreduce", { "fake3d.cl", "shape_3dvapour.cl", "shape_3dvapour_dreduce.cl" }),
        AFK_ClProgram("shape_3dvapour_feature", { "fake3d.cl", "shape_3dvapour.cl", "shape_3dvapour_feature.cl" }),
        AFK_ClProgram("shape_3dvapour_normal", { "fake3d.cl", "shape_3dvapour.cl", "shape_3dvapour_normal.cl" })
#endif
    };

    kernels = {
        AFK_ClKernel("landscape_surface", "makeLandscapeSurface"),
        AFK_ClKernel("landscape_terrain", "makeLandscapeTerrain"),
        AFK_ClKernel("landscape_yreduce", "makeLandscapeYReduce"),
#if AFK_RENDER_ENTITIES
        AFK_ClKernel("shape_3dedge", "makeShape3DEdge"),
        AFK_ClKernel("shape_3dvapour_dreduce", "makeShape3DVapourDReduce"),
        AFK_ClKernel("shape_3dvapour_feature", "makeShape3DVapourFeature"),
        AFK_ClKernel("shape_3dvapour_normal", "makeShape3DVapourNormal")
#endif
    };

    AFK_CLCHK(oclShim.GetPlatformIDs()(0, NULL, &platformCount))
    platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * platformCount);
    if (!platforms) throw AFK_Exception("Unable to allocate memory to inspect OpenCL platforms");
    AFK_CLCHK(oclShim.GetPlatformIDs()(platformCount, platforms, &platformCount))

    afk_out << "AFK: Found " << platformCount << " OpenCL platforms" << std::endl;

    for (unsigned int pI = 0; pI < platformCount; ++pI)
    {
        if (findClGlDevices(settings, platforms[pI]))
        {
            platform = platforms[pI];
            platformProps = new AFK_ClPlatformProperties(this, platform);
            afk_out << "AFK: Using OpenCL platform version " << platformProps->majorVersion << "." << platformProps->minorVersion << std::endl;
            break;
        }
    }

    free(platforms);

    if (!devices) throw AFK_Exception("No cl_gl devices found");

    /* Make my compute queues. */
    if (settings.clSeparateQueues)
    {
        kernelQueue = std::make_shared<AFK_ComputeQueue>(
            &oclShim, ctxt, devices[0], settings, AFK_CQ_KERNEL_COMMAND_SET);
        readQueue = std::make_shared<AFK_ComputeQueue>(
            &oclShim, ctxt, devices[0], settings, AFK_CQ_READ_COMMAND_SET);
        writeQueue = std::make_shared<AFK_ComputeQueue>(
            &oclShim, ctxt, devices[0], settings, AFK_CQ_WRITE_COMMAND_SET);
    }
    else
    {
        kernelQueue = readQueue = writeQueue =
            std::make_shared<AFK_ComputeQueue>(
                &oclShim, ctxt, devices[0], settings,
                AFK_CQ_KERNEL_COMMAND_SET | AFK_CQ_READ_COMMAND_SET | AFK_CQ_WRITE_COMMAND_SET);
    }
}

AFK_Computer::~AFK_Computer()
{
    if (oclPlatformExtensionShim) delete oclPlatformExtensionShim;

    if (devices)
    {
        kernelQueue.reset();
        readQueue.reset();
        writeQueue.reset();

        for (auto k : kernels)
            if (k.kernel) oclShim.ReleaseKernel()(k.kernel);

        for (auto p : programs)
            if (p.program) oclShim.ReleaseProgram()(p.program);

        if (ctxt) oclShim.ReleaseContext()(ctxt);
        if (firstDeviceProps) delete firstDeviceProps;

        delete[] devices;
    }

    if (platformProps) delete platformProps;
}

void AFK_Computer::loadPrograms(const AFK_ConfigSettings& settings)
{
    cl_int error;
    std::ostringstream errStream;

    /* Swap to the right directory. */
    if (!afk_pushDir(settings.clProgramsDir, errStream))
        throw AFK_Exception("AFK_Computer: Unable to switch to programs dir: " + errStream.str());

    /* Load all the programs I know about. */
    stillBuilding = programs.size();
    for (auto pIt = programs.begin(); pIt != programs.end(); ++pIt)
        loadProgramFromFiles(settings, pIt);
    waitForBuild();
    for (auto p : programs)
        for (size_t dI = 0; dI < devicesSize; ++dI)
            printBuildLog(afk_out, p, devices[dI]);

    /* ...and all the kernels... */
    for (auto kIt = kernels.begin(); kIt != kernels.end(); ++kIt)
    {
        bool identified = false;
        for (auto p : programs)
        {
            if (kIt->programName == p.programName)
            {
                kIt->kernel = oclShim.CreateKernel()(p.program, kIt->kernelName.c_str(), &error);
                AFK_HANDLE_CL_ERROR(error);
                identified = true;
            }
        }

        if (!identified) throw AFK_Exception("AFK_Computer: Unidentified compute kernel: " + kIt->kernelName);
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
    for (auto k : kernels)
    {
        if (k.kernelName == kernelName)
        {
            o_kernel = k.kernel;
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

bool AFK_Computer::useFake3DImages(const AFK_ConfigSettings& settings) const
{
    return (settings.forceFake3DImages ||
        !firstDeviceProps->supportsExtension("cl_khr_3d_image_writes"));
}

void AFK_Computer::finish(void)
{
    kernelQueue->finish();
    if (readQueue != kernelQueue) readQueue->finish();
    if (writeQueue != kernelQueue && writeQueue != readQueue) writeQueue->finish();
}
