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

#include <cassert>

#include "fair.hpp"

AFK_Fair2DIndex::AFK_Fair2DIndex(unsigned int _dim1Max):
    dim1Max(_dim1Max)
{
}

unsigned int AFK_Fair2DIndex::get1D(unsigned int q, unsigned int r) const
{
    assert(q < dim1Max);
    return q + dim1Max * r;
}

void AFK_Fair2DIndex::get2D(unsigned int i, unsigned int& o_q, unsigned int& o_r) const
{
    o_q = i % dim1Max;
    o_r = i / dim1Max;
}

