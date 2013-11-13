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

#ifndef _AFK_HASH_H_
#define _AFK_HASH_H_

#include <cmath>

#define AFK_LROTATE_UNSIGNED(v, r) (((v) << (r)) | ((v) >> (sizeof((v)) * 8 - (r))))

/* I'm fed up with link errors from boost::hash_combine(), and I want one
 * that is happy with 64-bit anyway
 */

const uint64_t afk_factor1 = 0x0040000100400001ull;
const uint64_t afk_factor2 = 0x0008002000080020ull;
const uint64_t afk_factor3 = 0x0010000800100008ull;
const uint64_t afk_factor4 = 0x0002000400020004ull;
const uint64_t afk_factor5 = 0x0100040001000400ull;

uint64_t afk_hash_swizzle(uint64_t a, uint64_t b, uint64_t factor);

#endif /* _AFK_HASH_H_ */

