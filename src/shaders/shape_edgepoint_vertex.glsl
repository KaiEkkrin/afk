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

// Shape vertex shader for rendering as points.
// Depends on shape_edgepoint_base.

layout (location = 0) in vec2 TexCoord;
layout (location = 1) in ivec2 Meta;

out VertexData
{
    vec3 vapourJigsawPieceCoord;
    vec3 cubeCoord;
    vec4 position;
    vec2 screenXY;
    int instanceId;
    float edgeStepsBack;
} outData;

void main()
{
    /* Reconstruct this point's jigsaw piece coords.
     */
    outData.vapourJigsawPieceCoord = texelFetch(DisplayTBO, gl_InstanceID * 7 + 5).xyz;
    vec2 edgeJigsawPieceCoord = texelFetch(DisplayTBO, gl_InstanceID * 7 + 6).xy;

    int face = Meta.s; /* t unused */
    vec2 texCoordDisp = getTexCoordDisp(face);
    
    /* Reconstruct those transforms */
    mat4 WorldTransform = getWorldTransform(gl_InstanceID);
    mat4 ClipTransform = ProjectionTransform * WorldTransform;

    /* Locate the 3D edge point */
    vec2 edgeJigsawCoord = makeEdgeJigsawCoordNearest(edgeJigsawPieceCoord, TexCoord.st + texCoordDisp);
    outData.edgeStepsBack = textureLod(JigsawESBTex, edgeJigsawCoord, 0);
    outData.cubeCoord = makeCubeCoordFromESB(TexCoord.st, outData.edgeStepsBack, face);
    outData.position = makePositionFromCubeCoord(ClipTransform, outData.cubeCoord, edgeJigsawCoord, gl_InstanceID);

    /* Calculate its screen co-ordinates too.  The geometry shader
     * wants this in order to figure out how much it overlaps its
     * neighbours.
     */
    outData.screenXY = clipToScreenXY(outData.position);

    outData.instanceId = gl_InstanceID;
}

