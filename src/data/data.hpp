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

#ifndef _AFK_DATA_DATA_H_
#define _AFK_DATA_DATA_H_

/* Common things to the `data' subfolder. */

/* Work around some compiler differences... */

#ifndef afk_noexcept
#ifdef __GNUC__
#define afk_noexcept noexcept
#endif
#ifdef _WIN32
//#define afk_noexcept __declspec(nothrow)
#define afk_noexcept
#endif
#endif /* afk_noexcept */

#endif /* _AFK_DATA_DATA_H_ */
