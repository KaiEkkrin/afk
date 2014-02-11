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

// A utility for doing point size calculations for shape renders.

#version 400

uniform vec2 WindowSize;

bool withinView(vec4 clipPosition)
{
    return (abs(clipPosition.x) <= clipPosition.w &&
        abs(clipPosition.y) <= clipPosition.w &&
        abs(clipPosition.z) <= clipPosition.w);
}

vec2 clipToScreenXY(vec4 clipPosition)
{
    /* Make normalized device co-ordinates
     * (between -1 and 1):
     */
    vec2 ndcXY = clipPosition.xy / clipPosition.w;

    /* Now, scale this to the screen: */
    return vec2(
        WindowSize.x * (ndcXY.x + 1.0) / 2.0,
        WindowSize.y * (ndcXY.y + 1.0) / 2.0);
}


float ContributeToPointSize(float oldPointSize, vec2 position, vec2 nearPosition)
{
    float newPointSize = distance(position, nearPosition);
    if (newPointSize > oldPointSize)
    {
        return newPointSize;
    }
    else
    {
        return oldPointSize;
    }
}

