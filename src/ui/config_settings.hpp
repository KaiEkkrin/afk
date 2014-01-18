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

#define AFK_CONFIG_FIELD(type, name, defaultValue) AFK_ConfigOption< type > name = AFK_ConfigOption< type >(#name, &options, defaultValue)
#define AFK_CONFIG_FIELD_NOSAVE(type, name, defaultValue) AFK_ConfigOption< type > name = AFK_ConfigOption< type >(#name, &options, defaultValue, true)

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

    /* Nobody else needs to know about this */
    AFK_CONFIG_FIELD_NOSAVE(bool,   saveOnQuit,                 false);

    // TODO: Add a special "help" option that prints all the other options and quits!

    void loadConfigFromFile(void);

public:
    /* All the configuration is public so that values can be accessed conveniently.
     * They're also non-constant, because some (e.g. the keyboard mapping) have
     * non-const accessors.
     * BE CAREFUL, don't mess up the settings object itself
     */

    AFK_CONFIG_FIELD_NOSAVE(int64_t, masterSeedLow, -1ll);
    AFK_CONFIG_FIELD_NOSAVE(int64_t, masterSeedHigh, -1ll);
    AFK_CONFIG_FIELD_NOSAVE(unsigned int, concurrency, std::thread::hardware_concurrency() + 1);

    // Graphics settings

    AFK_CONFIG_FIELD(std::string,   shadersDir,                 "shaders");
    AFK_CONFIG_FIELD(float,         fov,                        90.0f);
    AFK_CONFIG_FIELD(float,         zNear,                      0.5f);
    AFK_CONFIG_FIELD(float,         zFar,                       (float)(1 << 20));

    AFK_CONFIG_FIELD(unsigned int,  windowWidth,                0); // TODO make it possible to include a function for getting the default value?
    AFK_CONFIG_FIELD(unsigned int,  windowHeight,               0);

    // Input settings

    AFK_CONFIG_FIELD(float,         rotateButtonSensitivity,    0.01f);
    AFK_CONFIG_FIELD(float,         thrustButtonSensitivity,    0.001f);
    AFK_CONFIG_FIELD(float,         mouseAxisSensitivity,       0.001f);
    AFK_CONFIG_FIELD(bool,          pitchAxisInverted,          true);
    AFK_CONFIG_FIELD(bool,          rollAxisInverted,           false);
    AFK_CONFIG_FIELD(bool,          yawAxisInverted,            false);

    // TODO: In here goes an AFK_CONFIG_FIELD(AFK_KeyboardMapping, ...) that configures the keyboard mapping in the same way -- I'll want a new specialisation of AFK_ConfigOption :)

    // Engine calibration

    AFK_CONFIG_FIELD(float,         targetFrameTimeMillis,      16.5f);
    AFK_CONFIG_FIELD(unsigned int,  framesPerCalibration,       8);
    AFK_CONFIG_FIELD(bool,          vsync,                      true);

    // Compute settings

    AFK_CONFIG_FIELD(std::string,   clLibDir,                   "");
    AFK_CONFIG_FIELD(std::string,   clProgramsDir,              "compute");
    AFK_CONFIG_FIELD(bool,          clGlSharing,                false);
    AFK_CONFIG_FIELD(bool,          clSeparateQueues,           true); // requires clUseEvents
    AFK_CONFIG_FIELD(bool,          clOutOfOrder,               false);
    AFK_CONFIG_FIELD(bool,          clSyncReadWrite,            false);
    AFK_CONFIG_FIELD(bool,          clUseEvents,                true);
    AFK_CONFIG_FIELD(bool,          forceFake3DImages,          false);
    AFK_CONFIG_FIELD(float,         jigsawUsageFactor,          0.5f);

    // World settings

    AFK_CONFIG_FIELD(float,         startingDetailPitch,        512.0f);
    AFK_CONFIG_FIELD(float,         maxDetailPitch,             1536.0f);
    AFK_CONFIG_FIELD(float,         minDetailPitch,             64.0f);
    AFK_CONFIG_FIELD(float,         detailPitchStickiness,      0.08f);
    AFK_CONFIG_FIELD(float,         minCellSize,                1.0f);
    AFK_CONFIG_FIELD(unsigned int,  subdivisionFactor,          2);
    AFK_CONFIG_FIELD(unsigned int,  entitySubdivisionFactor,    4);
    AFK_CONFIG_FIELD(unsigned int,  entitySparseness,           1024);

    // Shape settings

    AFK_CONFIG_FIELD(unsigned int,  shape_skeletonMaxSize,      24);
    AFK_CONFIG_FIELD(float,         shape_edgeThreshold,        0.03f);

    // Controls

    AFK_KeyboardControls keyboardControls = AFK_KeyboardControls(&options);
    AFK_MouseControls mouseControls = AFK_MouseControls(&options);
    AFK_MouseAxisControls mouseAxisControls = AFK_MouseAxisControls(&options);

    float getAxisInversion(AFK_Control axis) const;

    AFK_ConfigSettings();
    virtual ~AFK_ConfigSettings();

    bool parseCmdLine(int *argcp, char **argv);
    bool parseConfigFile(void);
    bool saveConfigFile(void) const;
};

#endif /* _AFK_UI_CONFIG_SETTINGS_H_ */
