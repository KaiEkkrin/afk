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

#include "afk.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <utility>

#include <ctype.h>

#include <boost/random/random_device.hpp>

#include "config.hpp"
#include "exception.hpp"
#include "file/readfile.hpp"


/* AFK_KeyboardMapping implementation */

bool AFK_KeyboardMapping::replaceInList(char key, enum AFK_Controls control)
{
    auto keyIt = keyList.begin();
    auto controlIt = controlList.begin();

    while (keyIt != keyList.end())
    {
        if (*keyIt == key)
        {
            *controlIt = control;
            return true;
        }

        ++keyIt;
        ++controlIt;
    }

    assert(controlIt == controlList.end());
    return false;
}

void AFK_KeyboardMapping::appendToList(char key, enum AFK_Controls control)
{
    keyList.push_back(key);
    controlList.push_back(control);
}

void AFK_KeyboardMapping::updateMapping(void)
{
    std::ostringstream keySS;
    for (auto key : keyList) keySS << key;
    keys = keySS.str();

    controls.clear();
    controls.insert(controls.end(), controlList.begin(), controlList.end());

    upToDate = true;
}

void AFK_KeyboardMapping::map(const std::string& keys, enum AFK_Controls control)
{
    for (char key : keys)
    {
        if (!replaceInList(key, control)) appendToList(key, control);
    }

    upToDate = false;
}

enum AFK_Controls AFK_KeyboardMapping::find(const std::string& key)
{
    if (!upToDate) updateMapping();
    size_t pos = keys.find(key);
    if (pos != std::string::npos)
        return controls[pos];
    else
        return CTRL_NONE;
}


/* This utility function pulls AFK's executable path and names a directory
 * in the same location.
 */
static char *getDirAtExecPath(const char *leafname, const char *execname)
{
    size_t dirnameMaxSize = strlen(execname) + strlen(leafname) + 2;
    char *dirname = (char *)malloc(dirnameMaxSize);
#ifdef _WIN32
    strcpy_s(dirname, dirnameMaxSize, execname);
#else
    strcpy(dirname, execname);
#endif
    int i;
    for (i = static_cast<int>(strlen(execname)) - 1; i >= 0; --i)
    {
        if (dirname[i] == '/' || dirname[i] == '\\') break;
    }

    if (i > -1)
    {
        /* Found the lowest level directory path.  Copy the leafname
         * in here.
         */
#ifdef _WIN32
        strcpy_s(&dirname[i+1], dirnameMaxSize - (i+1), leafname);
#else
        strcpy(&dirname[i+1], leafname);
#endif
    }
    else
    {
        /* I can't find a lowest level directory path, use the CWD
         * instead.
         */
        char *currentDir = afk_getCWD();
        size_t cwdDirnameMaxSize = strlen(currentDir) + strlen(leafname) + 2;
        if (cwdDirnameMaxSize > dirnameMaxSize) dirname = (char *)realloc(dirname, cwdDirnameMaxSize);
#ifdef _WIN32
        sprintf_s(dirname, cwdDirnameMaxSize, "%s/%s", currentDir, leafname);
#else
        sprintf(dirname, "%s/%s", currentDir, leafname);
#endif
        free(currentDir);
    }

    return dirname;
}

#define REQUIRE_ARGUMENT(option) \
    ++argi;\
    if (argi == *argcp)\
        throw AFK_Exception(std::string("AFK_Config: ") + option + " without argument");

#define DEFAULT_KEYBOARD_CONTROL(keys, ctrl) \
    keyboardMapping.map(keys, ctrl);

#define DEFAULT_MOUSE_CONTROL(button, ctrl) \
    mouseMapping.insert(std::pair<const int, enum AFK_Controls>((button), (ctrl)));

#define DEFAULT_MOUSE_AXIS_CONTROL(axis, ctrl) \
    mouseAxisMapping.insert(std::pair<const enum AFK_Mouse_Axes, enum AFK_Control_Axes>((axis), (ctrl)));

/* AFK_Config implementation */

AFK_Config::AFK_Config(int *argcp, char **argv)
{
    shadersDir  = nullptr;
    fov         = 90.0f;
    zNear       = 0.5f;
    zFar        = (float)(1 << 20);

    rotateButtonSensitivity     = 0.01f;
    thrustButtonSensitivity     = 0.001f;
    mouseAxisSensitivity        = 0.001f;
    axisInversionMap = 0uLL;
    AFK_SET_BIT(axisInversionMap, CTRL_AXIS_PITCH);
    //AFK_SET_BIT(axisInversionMap, CTRL_AXIS_ROLL);
    //AFK_SET_BIT(axisInversionMap, CTRL_AXIS_YAW);

    /* TODO Make all the random twiddlies configurable via a
     * settings file.  And in fact, all the rest I think, apart
     * from a few things like the seed which could do with being
     * command line based ...
     */
    targetFrameTimeMillis       = 16.5f;
    framesPerCalibration        = 8;
    vsync                       = false;

    windowWidth                 = 0;
    windowHeight                = 0;

    clLibDir                    = nullptr;
    concurrency                 = std::thread::hardware_concurrency() + 1;
    clProgramsDir               = nullptr;
    clGlSharing                 = false; /* TODO Find hardware this actually improves performance on and default-true for that */
    async                       = true;
    forceFake3DImages           = false;
    jigsawUsageFactor           = 0.5f;

    startingDetailPitch         = 512.0f;
    maxDetailPitch              = 1536.0f;
    minDetailPitch              = 64.0f;
    detailPitchStickiness       = 0.08f;
    minCellSize                 = 1.0f;
    subdivisionFactor           = 2;
    entitySubdivisionFactor     = 4;

    shape_skeletonMaxSize               = 24;
    shape_edgeThreshold                 = 0.03f;

    entitySparseness            = 1024;
    

    /* Some hand rolled command line parsing, because it's not very
     * hard, and there's no good cross platform one by default */
    for (int argi = 1; argi < *argcp; ++argi)
    {
        if (strcmp(argv[argi], "--shaders-dir") == 0)
        {
            REQUIRE_ARGUMENT("--shaders-dir")
            shadersDir = afk_strdup(argv[argi]);
        }
        else if (strcmp(argv[argi], "--seed") == 0)
        {
            /* Require two arguments! */
            REQUIRE_ARGUMENT("--seed (1)")
            REQUIRE_ARGUMENT("--seed (2)")
            masterSeed = AFK_RNG_Value(argv[argi-1], argv[argi]);
        }
        else if (strcmp(argv[argi], "--fov") == 0)
        {
            REQUIRE_ARGUMENT("--fov")
            fov = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--zNear") == 0)
        {
            REQUIRE_ARGUMENT("--zNear")
            zNear = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--zFar") == 0)
        {
            REQUIRE_ARGUMENT("--zFar")
            zFar = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--target-frame-time-millis") == 0)
        {
            REQUIRE_ARGUMENT("--target-frame-time-millis")
            targetFrameTimeMillis = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--frames-per-calibration") == 0)
        {
            REQUIRE_ARGUMENT("--frames-per-calibration")
            framesPerCalibration = strtoul(argv[argi], NULL, 0);
        }
        else if (strcmp(argv[argi], "--vsync") == 0)
        {
            vsync = true;
        }
        else if (strcmp(argv[argi], "--window-width") == 0)
        {
            REQUIRE_ARGUMENT("--window-width")
            windowWidth = strtoul(argv[argi], NULL, 0);
        }
        else if (strcmp(argv[argi], "--window-height") == 0)
        {
            REQUIRE_ARGUMENT("--window-height")
            windowHeight = strtoul(argv[argi], NULL, 0);
        }
        else if (strcmp(argv[argi], "--cl-lib-dir") == 0)
        {
            REQUIRE_ARGUMENT("--cl-lib-dir")
            clProgramsDir = afk_strdup(argv[argi]);
        }
        else if (strcmp(argv[argi], "--concurrency") == 0)
        {
            REQUIRE_ARGUMENT("--concurrency")
            concurrency = strtoul(argv[argi], NULL, 0);
        }
        else if (strcmp(argv[argi], "--cl-programs-dir") == 0)
        {
            REQUIRE_ARGUMENT("--cl-programs-dir")
            clProgramsDir = afk_strdup(argv[argi]);
        }
        else if (strcmp(argv[argi], "--cl-gl-sharing") == 0)
        {
            clGlSharing = true;
        }
        /* TODO: I could do with standard forms for command line
         * args, a configuration file etc etc
         */
        else if (strcmp(argv[argi], "--cl-sync") == 0)
        {
            async = false;
        }
        else if (strcmp(argv[argi], "--force-fake-3D-images") == 0)
        {
            forceFake3DImages = true;
        }
        else if (strcmp(argv[argi], "--jigsaw-usage-factor") == 0)
        {
            REQUIRE_ARGUMENT("--jigsaw-usage-factor")
            jigsawUsageFactor = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--starting-detail-pitch") == 0)
        {
            REQUIRE_ARGUMENT("--starting-detail-pitch")
            startingDetailPitch = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--max-detail-pitch") == 0)
        {
            REQUIRE_ARGUMENT("--max-detail-pitch")
            maxDetailPitch = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--min-detail-pitch") == 0)
        {
            REQUIRE_ARGUMENT("--min-detail-pitch")
            minDetailPitch = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--detail-pitch-stickiness") == 0)
        {
            REQUIRE_ARGUMENT("--detail-pitch-stickiness")
            detailPitchStickiness = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--min-cell-size") == 0)
        {
            REQUIRE_ARGUMENT("--min-cell-size")
            minCellSize = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--subdivision-factor") == 0)
        {
            REQUIRE_ARGUMENT("--subdivision-factor")
            subdivisionFactor = strtoul(argv[argi], NULL, 0);
        }
        else if (strcmp(argv[argi], "--entity-subdivision-factor") == 0)
        {
            REQUIRE_ARGUMENT("--entity-subdivision-factor")
            entitySubdivisionFactor = strtoul(argv[argi], NULL, 0);
        }
        else if (strcmp(argv[argi], "--shape-skeleton-max-size") == 0)
        {
            REQUIRE_ARGUMENT("--shape-skeleton-max-size")
            shape_skeletonMaxSize = strtoul(argv[argi], NULL, 0);
        }
        else if (strcmp(argv[argi], "--shape-edge-threshold") == 0)
        {
            REQUIRE_ARGUMENT("--shape-edge-threshold")
            shape_edgeThreshold = strtof(argv[argi], NULL);
        }
        else if (strcmp(argv[argi], "--entity-sparseness") == 0)
        {
            REQUIRE_ARGUMENT("--entity-sparseness")
            entitySparseness = strtoul(argv[argi], NULL, 0);
        }
        else
        {
            std::ostringstream ss;
            ss << "Unrecognised argument: " << argv[argi];
            std::cerr << ss.str() << std::endl;
            throw AFK_Exception(ss.str());
        }
    }

    /* Apply defaults. */
    if (!shadersDir) shadersDir = getDirAtExecPath("shaders", argv[0]);
    if (!clProgramsDir) clProgramsDir = getDirAtExecPath("compute", argv[0]);

    if (!masterSeed.v.ull[0] && !masterSeed.v.ull[1])
    {
        boost::random::random_device rdev;
        masterSeed.v.ui[0] = rdev();
        masterSeed.v.ui[1] = rdev();
        masterSeed.v.ui[2] = rdev();
        masterSeed.v.ui[3] = rdev();
    }

    /* TODO: Come up with a way of inputting the control mapping.
     * (and editing it within AFK and saving it later!)
     */
    {
        /* TODO These are observed X keycodes.  I'll need different
         * ones for Windows at any rate ...
         */
        DEFAULT_KEYBOARD_CONTROL("wW", CTRL_THRUST_FORWARD)
        DEFAULT_KEYBOARD_CONTROL("sS", CTRL_THRUST_BACKWARD)
        DEFAULT_KEYBOARD_CONTROL("dD", CTRL_THRUST_RIGHT)
        DEFAULT_KEYBOARD_CONTROL("aA", CTRL_THRUST_LEFT)
        DEFAULT_KEYBOARD_CONTROL("rR", CTRL_THRUST_UP)
        DEFAULT_KEYBOARD_CONTROL("fF", CTRL_THRUST_DOWN)
        DEFAULT_KEYBOARD_CONTROL("eE", CTRL_YAW_RIGHT)
        DEFAULT_KEYBOARD_CONTROL("qQ", CTRL_YAW_LEFT)
        DEFAULT_KEYBOARD_CONTROL("mM", CTRL_MOUSE_CAPTURE)

        /* TODO: Sort out the function key code.  (Localization?  Eww) */
        //DEFAULT_KEYBOARD_CONTROL(95 /* f11 */, CTRL_FULLSCREEN)
        DEFAULT_KEYBOARD_CONTROL("0)", CTRL_FULLSCREEN)
    }

    if (mouseMapping.empty())
    {
        DEFAULT_MOUSE_CONTROL(1 /* left button */,     CTRL_PRIMARY_FIRE)
        DEFAULT_MOUSE_CONTROL(3 /* right button */,    CTRL_SECONDARY_FIRE)
        DEFAULT_MOUSE_CONTROL(2 /* middle button */,   CTRL_MOUSE_CAPTURE)
    }

    if (mouseAxisMapping.empty())
    {
        DEFAULT_MOUSE_AXIS_CONTROL(MOUSE_AXIS_X, CTRL_AXIS_ROLL)
        DEFAULT_MOUSE_AXIS_CONTROL(MOUSE_AXIS_Y, CTRL_AXIS_PITCH)
    }

    /* Print a little dump. */
    std::cout << "AFK:Loaded configuration:" << std::endl;
    std::cout << "  shadersDir:     " << shadersDir << std::endl;
    std::cout << "  clProgramsDir:  " << clProgramsDir << std::endl;
}

AFK_Config::~AFK_Config()
{
    if (clProgramsDir) free(clProgramsDir);
    if (shadersDir) free(shadersDir);
}

