/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <utility>

#include "config.hpp"
#include "exception.hpp"

#define REQUIRE_ARGUMENT(option) \
    ++argi;\
    if (argi == *argcp)\
        throw AFK_Exception(std::string("AFK_Config: ") + option + " without argument");

#define DEFAULT_KEYBOARD_CONTROL(chr, ctrl) \
    keyboardMapping.insert(std::pair<const char, enum AFK_Controls>((chr), (ctrl)));\
    keyboardMapping.insert(std::pair<const char, enum AFK_Controls>(toupper((chr)), (ctrl)));

#define DEFAULT_MOUSE_CONTROL(button, ctrl) \
    mouseMapping.insert(std::pair<const int, enum AFK_Controls>((button), (ctrl)));

#define DEFAULT_MOUSE_AXIS_CONTROL(axis, ctrl) \
    mouseAxisMapping.insert(std::pair<const enum AFK_Mouse_Axes, enum AFK_Control_Axes>((axis), (ctrl)));

AFK_Config::AFK_Config(int *argcp, char **argv)
{
    shadersDir  = NULL;
    fov         = 90.0f;
    zNear       = 0.5f;
    zFar        = 16384.0f;

    rotateButtonSensitivity     = 0.01f;
    thrustButtonSensitivity     = 0.01f;
    mouseAxisSensitivity        = 0.001f;
    axisInversionMap = 0uLL;
    AFK_SET_BIT(axisInversionMap, CTRL_AXIS_YAW);

    /* Some hand rolled command line parsing, because it's not very
     * hard, and there's no good cross platform one by default */
    for (int argi = 0; argi < *argcp; ++argi)
    {
        if (strcmp(argv[argi], "--shaders-dir") == 0)
        {
            REQUIRE_ARGUMENT("--shaders-dir")
            shadersDir = strdup(argv[argi]);
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

        /* Ignore other arguments. */
    }

    /* Apply defaults. */
    if (!shadersDir)
    {
        char *currentDir;
        const char *shadersDirLeafName = "shaders";
        size_t shadersDirLength;

        currentDir = get_current_dir_name();
        shadersDirLength = strlen(currentDir) + 1 + strlen(shadersDirLeafName) + 1;
        shadersDir = (char *) malloc(sizeof(char) * shadersDirLength);
        snprintf(shadersDir, shadersDirLength, "%s/%s", currentDir, shadersDirLeafName);
        free(currentDir);
    }

    /* TODO: Come up with a way of inputting the control mapping.
     * (and editing it within AFK and saving it later!)
     */
    if (keyboardMapping.empty())
    {
        /* TODO Swap wsad to mouse controls by default, so I can put
         * the others somewhere more sensible!
         */
        DEFAULT_KEYBOARD_CONTROL('w', CTRL_THRUST_FORWARD)
        DEFAULT_KEYBOARD_CONTROL('s', CTRL_THRUST_BACKWARD)
        DEFAULT_KEYBOARD_CONTROL('d', CTRL_THRUST_RIGHT)
        DEFAULT_KEYBOARD_CONTROL('a', CTRL_THRUST_LEFT)
        DEFAULT_KEYBOARD_CONTROL('r', CTRL_THRUST_UP)
        DEFAULT_KEYBOARD_CONTROL('f', CTRL_THRUST_DOWN)
        DEFAULT_KEYBOARD_CONTROL('e', CTRL_YAW_RIGHT)
        DEFAULT_KEYBOARD_CONTROL('q', CTRL_YAW_LEFT)
        DEFAULT_KEYBOARD_CONTROL('m', CTRL_MOUSE_CAPTURE)
    }

    if (mouseMapping.empty())
    {
        DEFAULT_MOUSE_CONTROL(GLUT_LEFT_BUTTON,     CTRL_PRIMARY_FIRE)
        DEFAULT_MOUSE_CONTROL(GLUT_RIGHT_BUTTON,    CTRL_SECONDARY_FIRE)
        DEFAULT_MOUSE_CONTROL(GLUT_MIDDLE_BUTTON,   CTRL_MOUSE_CAPTURE)
    }

    if (mouseAxisMapping.empty())
    {
        DEFAULT_MOUSE_AXIS_CONTROL(MOUSE_AXIS_X, CTRL_AXIS_ROLL)
        DEFAULT_MOUSE_AXIS_CONTROL(MOUSE_AXIS_Y, CTRL_AXIS_PITCH)
    }

    /* Print a little dump. */
    std::cout << "AFK:Loaded configuration:" << std::endl;
    std::cout << "  shadersDir:     " << shadersDir << std::endl;
}

AFK_Config::~AFK_Config()
{
    if (shadersDir) free(shadersDir);
}

