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

#ifndef _AFK_CONFIG_H_
#define _AFK_CONFIG_H_

#include "afk.hpp"

#include <list>
#include <map>
#include <sstream>
#include <vector>

#include "rng/rng.hpp"

/* TODO Wouldn't it be great to support gamepads?  And a
 * joystick?  Wow, joysticks.  I should get a modern one.
 */

#define AFK_TEST_BIT(field, bit) ((field) & (1uLL<<(bit)))
#define AFK_SET_BIT(field, bit) ((field) |= (1uLL<<(bit)))
#define AFK_CLEAR_BIT(field, bit) ((field) &= ~(1uLL<<(bit)))

enum AFK_Controls
{
    CTRL_NONE,
    CTRL_MOUSE_CAPTURE,
    CTRL_PITCH_UP,
    CTRL_PITCH_DOWN,
    CTRL_YAW_RIGHT,
    CTRL_YAW_LEFT,
    CTRL_ROLL_RIGHT,
    CTRL_ROLL_LEFT,
    CTRL_THRUST_FORWARD,
    CTRL_THRUST_BACKWARD,
    CTRL_THRUST_RIGHT,
    CTRL_THRUST_LEFT,
    CTRL_THRUST_UP, 
    CTRL_THRUST_DOWN,
    CTRL_PRIMARY_FIRE,
    CTRL_SECONDARY_FIRE,
    CTRL_FULLSCREEN
};

#define AFK_IS_TOGGLE(bit) ((bit) == CTRL_MOUSE_CAPTURE || (bit) == CTRL_FULLSCREEN)

enum AFK_Control_Axes
{
    CTRL_AXIS_NONE,
    CTRL_AXIS_PITCH,
    CTRL_AXIS_YAW,
    CTRL_AXIS_ROLL
};

enum AFK_Mouse_Axes
{
    MOUSE_AXIS_X,
    MOUSE_AXIS_Y
};

/* The keyboard mapping is built as a single string that
 * we search for the key, and a matchingly indexed list
 * of the controls.
 * Because really, there are few enough keys mapped that
 * doing a full search each time should be no trouble.
 */
class AFK_KeyboardMapping
{
protected:
    /* These are used to build the mapping. */
    std::list<char> keyList;
    std::list<enum AFK_Controls> controlList;

    /* This is the "live mapping". */
    std::string keys;
    std::vector<enum AFK_Controls> controls;
    bool upToDate = false;

    bool replaceInList(char key, enum AFK_Controls control);
    void appendToList(char key, enum AFK_Controls control);
    void updateMapping(void);

public:
    void map(const std::string& keys, enum AFK_Controls control);
    enum AFK_Controls find(const std::string& key);
};

class AFK_Config
{
public:
    char    *shadersDir;

    AFK_RNG_Value   masterSeed;

    /* The viewing frustum. */
    float   fov;
    float   zNear;
    float   zFar;

    /* The controls */
    //std::map<const int, enum AFK_Controls> keyboardMapping;
    AFK_KeyboardMapping keyboardMapping;
    std::map<const int, enum AFK_Controls> mouseMapping;
    std::map<const enum AFK_Mouse_Axes, enum AFK_Control_Axes> mouseAxisMapping;

    float   rotateButtonSensitivity;
    float   thrustButtonSensitivity;
    float   mouseAxisSensitivity;
    uint64_t    axisInversionMap; /* bitfield of axes that are inverted */

    /* Engine calibration */
    float targetFrameTimeMillis;
    unsigned int framesPerCalibration;
    bool vsync;

    /* Graphics setup */
    unsigned int windowWidth;
    unsigned int windowHeight;

    /* Computing setup
     * TODO Bools are particularly nasty with the current command line
     * configuration system.  I really want to make a better arguments
     * system for AFK, including configuration files, etc!
     */
    char        *clLibDir; /* Place to find libOpenCL -- leave null to search
                            * standard library path */
    unsigned int concurrency;
    char        *clProgramsDir;
    bool        clGlSharing;
    bool        clSeparateQueues;
    bool        clOutOfOrder;
    bool        clSyncReadWrite;
    bool        clUseEvents;
    bool        forceFake3DImages;
    float       jigsawUsageFactor; /* Proportion of available CL memory to
                                    * use for jigsaws */

    /* World setup */
    float startingDetailPitch;
    float maxDetailPitch;
    float minDetailPitch;
    float detailPitchStickiness;
    float minCellSize;
    unsigned int subdivisionFactor;
    unsigned int entitySubdivisionFactor;

    /* Shape setup */
    unsigned int shape_skeletonMaxSize;
    float shape_edgeThreshold;

    /* Entities.
     * (TODO: These are going to want splitting up into
     * static/background features, interesting objects,
     * that kind of thing; and have different configurations
     * each.)
     */
    unsigned int entitySparseness; /* 1 -- always an entity; 16 -- 1/16th chance of an entity, etc */

    /* Initialises AFK configuration, based on command line
     * arguments. */
    AFK_Config(int *argcp, char **argv);

    ~AFK_Config();

    /* Returns 1.0f for a non-inverted axis and -1.0f for an inverted one */
    float getAxisInversion(int axis) { return (AFK_TEST_BIT(axisInversionMap, axis) ? -1.0f : 1.0f); }
};

#endif /* _AFK_CONFIG_H_ */

