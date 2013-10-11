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

/* This is the common part of the vapour stuff, declaring the
 * input units, etc.
 */

struct AFK_3DVapourComputeUnit
{
    float4 location;
    float4 baseColour;
    int4 vapourPiece;
    int adjacency; /* TODO: Once thought I'd want this but now unused -- needs removing. */
    int cubeOffset;
    int cubeCount;
};

