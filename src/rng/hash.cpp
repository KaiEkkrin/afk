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

#include <functional>

#include "hash.hpp"

static constexpr double phi = (1.0 + sqrt(5.0)) / 2.0;
static constexpr double phi_rec = pow(2.0, 64.0) / phi;
static constexpr uint64_t magic = static_cast<uint64_t>(phi_rec);

#define FIRST_32(n) ((n) & 0xffffffffull)
#define SECOND_32(n) (((n) & 0xffffffffull) >> 32)

#define FLIP(n) \
    ((((n) & 0xffffffffull) << 32) | \
     (((n) & 0xffffffff00000000ull) >> 32))

uint64_t add(uint64_t a, uint64_t b) { return a + b; }
uint64_t multiply(uint64_t a, uint64_t b) { return a * b; }

std::function<uint64_t (uint64_t, uint64_t)> addOp = add;
std::function<uint64_t (uint64_t, uint64_t)> multiplyOp = multiply;

template<std::function<uint64_t (uint64_t, uint64_t)>& op>
uint64_t four_times(uint64_t a, uint64_t b)
{
    return op(FIRST_32(a), FIRST_32(b)) ^
        FLIP(op(FIRST_32(a), SECOND_32(b))) ^
        FLIP(op(SECOND_32(a), FIRST_32(b))) ^
        op(SECOND_32(a), SECOND_32(b));
}

uint64_t afk_hash_swizzle(uint64_t a, uint64_t b, uint64_t factor)
{
    return four_times<addOp>(a, magic) ^
        AFK_LROTATE_UNSIGNED(four_times<multiplyOp>(b, factor), 7) ^
        AFK_LROTATE_UNSIGNED(four_times<multiplyOp>(b, factor), 37);
}

