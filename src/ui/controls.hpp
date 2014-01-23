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

/* Low-number controls index bits in the control bit field (above).
* High-number controls (which wouldn't affect the bit field) include
* axes.
*/
#define AFK_TEST_CONTROL_BIT(field, bit) ((field) & (1uLL<<(static_cast<int>(bit))))
#define AFK_SET_CONTROL_BIT(field, bit) ((field) |= (1uLL<<(static_cast<int>(bit))))
#define AFK_CLEAR_CONTROL_BIT(field, bit) ((field) &= ~(1uLL<<(static_cast<int>(bit))))

#define AFK_CONTROL_AXIS_BIT 0x100

enum class AFK_Control : int
{
    NONE = 0x0,
    MOUSE_CAPTURE = 0x1,
    PITCH_UP = 0x2,
    PITCH_DOWN = 0x3,
    YAW_RIGHT = 0x4,
    YAW_LEFT = 0x5,
    ROLL_RIGHT = 0x6,
    ROLL_LEFT = 0x7,
    THRUST_FORWARD = 0x8,
    THRUST_BACKWARD = 0x9,
    THRUST_RIGHT = 0xa,
    THRUST_LEFT = 0xb,
    THRUST_UP = 0xc,
    THRUST_DOWN = 0xd,
    PRIMARY_FIRE = 0xe,
    SECONDARY_FIRE = 0xf,
    FULLSCREEN = 0x10,
    AXIS_PITCH = 0x100,
    AXIS_YAW = 0x101,
    AXIS_ROLL = 0x102
};

#define AFK_CONTROL_IS_TOGGLE(bit) ((bit) == AFK_Control::MOUSE_CAPTURE || (bit) == AFK_Control::FULLSCREEN)

enum class AFK_MouseAxis : int
{
    X = 1,
    Y = 2
};

enum class AFK_InputType : int
{
    NONE = 0,
    KEYBOARD = 1,
    MOUSE = 2,
    MOUSE_AXIS = 3
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
    AFK_Control             control;
    std::string             name;
    AFK_InputType           defaultType;
    std::string             defaultValue;

    AFK_ConfigOptionName getName(void) const { return AFK_ConfigOptionName(name); }
};

std::list<struct AFK_DefaultControl>& afk_getDefaultControls(void);

/* A general way of configuring controls. */
class AFK_ConfigOptionControl : public AFK_ConfigOptionBase
{
protected:
    const AFK_InputType inputType;

    /* Used to track state while parsing. */
    AFK_Control matchedControl;

    /* Call from subclass constructors that override map().  Can't call from the
     * default constructor, because the subclass instance doesn't exist yet
     */
    void setupDefaultMapping(void);

    /* Utility for the save() functions. */
    std::string spellingOf(AFK_Control control) const;

    virtual void saveInternal(std::ostream& os) const = 0;

public:
    AFK_ConfigOptionControl(const std::string& _name, const std::string& _help, AFK_InputType _inputType, std::list<AFK_ConfigOptionBase *> *_options);

    /* Maps a string input of this type to the control.  Returns false if
     * the string isn't valid or it otherwise can't use it
     */
    virtual bool map(const std::string& input, AFK_Control control) = 0;

    bool nameMatches(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) override;
    bool matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) override;

    void printHelp(std::ostream& os) const override;
};

/* The keyboard mapping is built as a single string that
* we search for the key, and a matchingly indexed list
* of the controls.
* Because really, there are few enough keys mapped that
* doing a full search each time should be no trouble.
*/
class AFK_KeyboardControls : public AFK_ConfigOptionControl
{
protected:
    /* These are used to build the mapping. */
    std::list<char> keyList;
    std::list<AFK_Control> controlList;

    /* This is the "live mapping". */
    std::string keys;
    std::vector<AFK_Control> controls;
    bool upToDate = false;

    bool replaceInList(char key, AFK_Control control);
    void appendToList(char key, AFK_Control control);
    void updateMapping(void);

    void saveInternal(std::ostream& os) const override;

public:
    AFK_KeyboardControls(std::list<AFK_ConfigOptionBase *> *options);

    bool map(const std::string& keys, AFK_Control control) override;
    
    // TODO: At some point I'll want a setter (too intricate for an operator[] )
    AFK_Control get(const std::string& key);
};

class AFK_MouseControls : public AFK_ConfigOptionControl
{
protected:
    std::map<int, AFK_Control> mapping;

    void saveInternal(std::ostream& os) const override;

public:
    AFK_MouseControls(std::list<AFK_ConfigOptionBase *> *options);

    bool map(const std::string& keys, AFK_Control control) override;

    AFK_Control& operator[](int button);
};

class AFK_MouseAxisControls : public AFK_ConfigOptionControl
{
protected:
    std::map<AFK_MouseAxis, AFK_Control> mapping;

    void saveInternal(std::ostream& os) const override;

public:
    AFK_MouseAxisControls(std::list<AFK_ConfigOptionBase *> *options);

    bool map(const std::string& keys, AFK_Control control) override;

    AFK_Control& operator[](AFK_MouseAxis axis);
};

#endif /* _AFK_UI_CONTROLS_H_ */
