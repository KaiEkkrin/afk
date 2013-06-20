/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_CONFIG_H_
#define _AFK_CONFIG_H_

#include <map>

enum AFK_Controls
{
    CTRL_NONE               = 0,
    CTRL_PITCH_UP           = 1,
    CTRL_PITCH_DOWN         = 2,
    CTRL_YAW_RIGHT          = 4,
    CTRL_YAW_LEFT           = 8,
    CTRL_ROLL_RIGHT         = 16,
    CTRL_ROLL_LEFT          = 32,
    CTRL_OPEN_THROTTLE      = 64,
    CTRL_CLOSE_THROTTLE     = 128
};

class AFK_Config
{
public:
    char    *shadersDir;

    /* The viewing frustum. */
    float   fov;
    float   zNear;
    float   zFar;

    /* The keyboard map */
    std::map<const char, enum AFK_Controls> controlMapping;

    float   keyboardRotateSensitivity;
    float   keyboardThrottleSensitivity;

    /* Initialises AFK configuration, based on command line
     * arguments. */
    AFK_Config(int *argcp, char **argv);

    ~AFK_Config();
};

#endif /* _AFK_CONFIG_H_ */

