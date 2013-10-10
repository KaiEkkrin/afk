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

// Shape vertex shader.

#version 400

layout (location = 0) in vec2 TexCoord;

out VertexData
{
    int instanceId; /* Because gl_InstanceID isn't accessible in a
                     * geometry shader
                     */
    vec2 texCoord;
} outData;

void main()
{
    /* All the hard work is now done by the geometry shader.  This
     * vertex shader is a passthrough only.
     */
    outData.instanceId = gl_InstanceID;
    outData.texCoord = TexCoord.st;
}

