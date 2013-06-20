/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_CONFIG_H_
#define _AFK_CONFIG_H_

#include <map>

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
    CTRL_SECONDARY_FIRE
};

#define AFK_IS_TOGGLE(bit) ((bit) == CTRL_MOUSE_CAPTURE)

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

class AFK_Config
{
public:
    char    *shadersDir;

    /* The viewing frustum. */
    float   fov;
    float   zNear;
    float   zFar;

    /* The controls */
    std::map<const char, enum AFK_Controls> keyboardMapping;
    std::map<const int, enum AFK_Controls> mouseMapping;
    std::map<const enum AFK_Mouse_Axes, enum AFK_Control_Axes> mouseAxisMapping;

    float   rotateButtonSensitivity;
    float   thrustButtonSensitivity;
    float   mouseAxisSensitivity;
    unsigned long long  axisInversionMap; /* bitfield of axes that are inverted */

    /* Initialises AFK configuration, based on command line
     * arguments. */
    AFK_Config(int *argcp, char **argv);

    ~AFK_Config();
};

#endif /* _AFK_CONFIG_H_ */

