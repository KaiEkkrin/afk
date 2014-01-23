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

#ifndef _AFK_UI_HELP_OPTION_H_
#define _AFK_UI_HELP_OPTION_H_

#include "config_option.hpp"

/* This special config option gets AFK to print the help text for the
 * other options and exit.
 */

class AFK_ConfigOptionHelp : public AFK_ConfigOptionBase
{
protected:
    /* I retain a pointer to the whole option list so that I can
     * iterate through it later!
     */
    std::list<AFK_ConfigOptionBase *> *options;

    void saveInternal(std::ostream& os) const override;

public:
    AFK_ConfigOptionHelp(std::list<AFK_ConfigOptionBase *> *_options);

    bool matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg) override;

    void printHelp(std::ostream& os) const override;
};

#endif /* _AFK_UI_HELP_OPTION_H_ */
