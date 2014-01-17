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

#ifndef _AFK_HASH_H_
#define _AFK_HASH_H_

#include <cmath>
#include <cstdint>

#define AFK_LROTATE_UNSIGNED(v, r) (((v) << (r)) | ((v) >> (sizeof((v)) * 8 - (r))))

/* boost::hash_combine() is not good enough.  This one is.
 * Start it off with a==0 ?
 */

static_assert(sizeof(size_t) == sizeof(uint64_t), "not a 64 bit system");
uint64_t afk_hash_swizzle(uint64_t a, uint64_t b);

#endif /* _AFK_HASH_H_ */

