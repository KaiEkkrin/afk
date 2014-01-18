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

#ifndef _AFK_UI_CONTROLS_H_
#define _AFK_UI_CONTROLS_H_

#include <map>
#include <vector>

#include "config_option.hpp"

/* New controls replacing what's in the old config module.  WIP --
 * disabled for now.
 */

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

enum AFK_Input_Types
{
    INPUT_TYPE_KEYBOARD,
    INPUT_TYPE_MOUSE,
    INPUT_TYPE_MOUSE_AXIS,
    INPUT_TYPE_NONE,
};

/* This is where the defaults go.
 * Each control can actually be mapped to any input type;
 * the one listed here is the default.
 * To have several defaults of different types, list it several times; to have
 * no default, list an empty value along with
 * INPUT_TYPE_NONE.
 */
struct AFK_DefaultControl
{
    enum AFK_Controls       control;
    AFK_ConfigOptionName    name;
    AFK_Input_Types         defaultType;
    std::string             defaultValue;
};
extern std::list<struct AFK_DefaultControl> afk_defaultControls;

/* The keyboard mapping is built as a single string that
* we search for the key, and a matchingly indexed list
* of the controls.
* Because really, there are few enough keys mapped that
* doing a full search each time should be no trouble.
*/
class AFK_KeyboardControls : public AFK_ConfigOptionBase
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

    /* Some state tracking when parsing arguments. */
    enum AFK_Controls matchedControl;

public:
    AFK_KeyboardControls(std::list<AFK_ConfigOptionBase *> *options);

    bool nameMatches(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) override;
    bool matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) override;

    void map(const std::string& keys, enum AFK_Controls control);
    
    enum AFK_Controls operator[](const std::string& key);

    void save(std::ostream& os) const override;
};

class AFK_MouseControls : public AFK_ConfigOptionBase
{
protected:
    std::map<int, enum AFK_Controls> mapping;

public:
    AFK_MouseControls(std::list<AFK_ConfigOptionBase *> *options);
    
    bool matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) override;

    enum AFK_Controls operator[](int button) const;

    void save(std::ostream& os) const override;
};

class AFK_MouseAxisControls : public AFK_ConfigOptionBase
{
protected:
    std::map<enum AFK_Control_Axes, enum AFK_Controls> mapping;

public:
    AFK_MouseAxisControls(std::list<AFK_ConfigOptionBase *> *options);

    bool matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) override;

    enum AFK_Controls operator[](enum AFK_Control_Axes axis) const;

    void save(std::ostream& os) const override;
};

#endif /* _AFK_UI_CONTROLS_H_ */

