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

#ifndef _AFK_UI_CONFIG_SETTINGS_H_
#define _AFK_UI_CONFIG_SETTINGS_H_

#include <list>
#include <thread>

#include "config_option.hpp"
#include "controls.hpp"
#include "help_option.hpp"

#define AFK_CONFIG_FIELD(type, name, help, defaultValue) AFK_ConfigOption< type > name = AFK_ConfigOption< type >(#name, help, &options, defaultValue)
#define AFK_CONFIG_FIELD_NOSAVE(type, name, help, defaultValue) AFK_ConfigOption< type > name = AFK_ConfigOption< type >(#name, help, &options, defaultValue, true)

/* Describes the AFK configuration. */

class AFK_ConfigSettings
{
protected:
    /* For easy iteration (initialisation, loading, saving). */
    std::list<AFK_ConfigOptionBase *> options;

    /* This option wants to be hidden, not-saved, and also needs some logic to
     * start it up (where should the configuration go?  OS dependent)
     */
    AFK_ConfigOption<std::string> *configFile;

    /* Nobody else needs to know about these */
    AFK_CONFIG_FIELD_NOSAVE(bool,   saveOnQuit,                 "Save configuration on quit",                 false);
    AFK_ConfigOptionHelp help = AFK_ConfigOptionHelp(&options);

    // TODO: Add a special "help" option that prints all the other options and quits!

    void loadConfigFromFile(void);

public:
    /* All the configuration is public so that values can be accessed conveniently.
     * They're also non-constant, because some (e.g. the keyboard mapping) have
     * non-const accessors.
     * BE CAREFUL, don't mess up the settings object itself
     */

    AFK_CONFIG_FIELD_NOSAVE(int64_t, masterSeedLow,             "Low part of master seed (64 bits)",        -1ll);
    AFK_CONFIG_FIELD_NOSAVE(int64_t, masterSeedHigh,            "High part of master seed (64 bits)",       -1ll);
    AFK_CONFIG_FIELD_NOSAVE(unsigned int, concurrency,          "Number of worker threads",                 std::thread::hardware_concurrency() + 1);
    AFK_CONFIG_FIELD_NOSAVE(std::string, logFile,               "Log file",                                 "");

    // Graphics settings

    AFK_CONFIG_FIELD(std::string,   shadersDir,                 "Location of shader sources",               "src/shaders");
    AFK_CONFIG_FIELD(float,         fov,                        "Vertical field of view (degrees)",         90.0f);
    AFK_CONFIG_FIELD(float,         zNear,                      "Near clip plane distance",                 0.5f);
    AFK_CONFIG_FIELD(float,         zFar,                       "Far clip plane distance",                  (float)(1 << 20));

    AFK_CONFIG_FIELD(unsigned int,  windowWidth,                "Starting window width",                    0); // TODO make it possible to include a function for getting the default value?
    AFK_CONFIG_FIELD(unsigned int,  windowHeight,               "Starting window height",                   0);

    // Input settings

    AFK_CONFIG_FIELD(float,         rotateButtonSensitivity,    "Rotate button sensitivity",                0.01f);
    AFK_CONFIG_FIELD(float,         thrustButtonSensitivity,    "Thrust button sensitivity",                0.001f);
    AFK_CONFIG_FIELD(float,         mouseAxisSensitivity,       "Mouse axis sensitivity",                   0.001f);
    AFK_CONFIG_FIELD(bool,          pitchAxisInverted,          "Invert pitch axis",                        true);
    AFK_CONFIG_FIELD(bool,          rollAxisInverted,           "Invert roll axis",                         false);
    AFK_CONFIG_FIELD(bool,          yawAxisInverted,            "Invert yaw axis",                          false);

    // TODO: In here goes an AFK_CONFIG_FIELD(AFK_KeyboardMapping, ...) that configures the keyboard mapping in the same way -- I'll want a new specialisation of AFK_ConfigOption :)

    // Engine calibration

    AFK_CONFIG_FIELD(float,         targetFrameTimeMillis,      "Frame time target (milliseconds)",         16.5f);
    AFK_CONFIG_FIELD(unsigned int,  framesPerCalibration,       "Number of frames between calibrations",    8);
    AFK_CONFIG_FIELD(bool,          vsync,                      "Use vsync if AFK can control it",          true);

    // Compute settings

    AFK_CONFIG_FIELD(std::string,   clLibDir,                   "Broken, do not use",                       "");
    AFK_CONFIG_FIELD(std::string,   clProgramsDir,              "Location of OpenCL sources",               "src/compute");
#ifdef _DEBUG
    AFK_CONFIG_FIELD(bool,          clOptDisable,               "Disable OpenCL optimisation",              true);
#else
    AFK_CONFIG_FIELD(bool,          clOptDisable,               "Disable OpenCL optimisation",              false);
#endif
    AFK_CONFIG_FIELD(bool,          clGlSharing,                "Use cl-gl sharing if supported",           true);
    AFK_CONFIG_FIELD(bool,          clSeparateQueues,           "Use multiple OpenCL queues",               true); // requires clUseEvents
    AFK_CONFIG_FIELD(bool,          clOutOfOrder,               "Use out-of-order OpenCL queues",           false);
    AFK_CONFIG_FIELD(bool,          clSyncReadWrite,            "Use synchronous OpenCL reads and writes",  false);
    AFK_CONFIG_FIELD(bool,          clUseEvents,                "Use OpenCL events",                        true);
    AFK_CONFIG_FIELD(bool,          forceFake3DImages,          "Force all OpenCL images to 2D",            false);
    AFK_CONFIG_FIELD(float,         jigsawUsageFactor,          "You probably shouldn't touch this",        0.5f);

    // World settings

    AFK_CONFIG_FIELD(float,         startingDetailPitch,        "Starting detail pitch (lower is finer)",   512.0f);
    AFK_CONFIG_FIELD(float,         maxDetailPitch,             "Coarsest detail pitch",                    1536.0f);
    AFK_CONFIG_FIELD(float,         minDetailPitch,             "Finest detail pitch",                      64.0f);
    AFK_CONFIG_FIELD(float,         detailPitchStickiness,      "Minimum detail pitch adjustment ratio",    0.08f);
    AFK_CONFIG_FIELD(float,         minCellSize,                "You definitely shouldn't touch this",      1.0f);
    AFK_CONFIG_FIELD(unsigned int,  subdivisionFactor,          "Or this",                                  2);
    AFK_CONFIG_FIELD(unsigned int,  entitySubdivisionFactor,    "World cell to entity scale ratio",         4);
    AFK_CONFIG_FIELD(unsigned int,  entitySparseness,           "Cells have 1 in this chance of containing entities",   1024);

    // Shape settings

    AFK_CONFIG_FIELD(unsigned int,  shape_skeletonMaxSize,      "Maximum number of cubes per skeleton",     24);
    AFK_CONFIG_FIELD(float,         shape_edgeThreshold,        "Density at which vapour becomes solid",    0.03f);

    // Controls

    AFK_KeyboardControls keyboardControls = AFK_KeyboardControls(&options);
    AFK_MouseControls mouseControls = AFK_MouseControls(&options);
    AFK_MouseAxisControls mouseAxisControls = AFK_MouseAxisControls(&options);

    float getAxisInversion(AFK_Control axis) const;

    AFK_ConfigSettings();
    virtual ~AFK_ConfigSettings();

    bool parseCmdLine(int *argcp, char **argv);
    void saveTo(std::ostream& os) const;
};

#endif /* _AFK_UI_CONFIG_SETTINGS_H_ */
