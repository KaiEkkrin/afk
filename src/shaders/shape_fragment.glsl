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

// Shape vertex colour and lighting.

#version 400

// This is the density texture.  I'll use the first 3 components
// only, which represent the colour.
// TODO: change the vapour to have separate colour (3-uint8) and
// density (1-float) textures?
uniform sampler3D JigsawDensityTex;

// The sky colour, which I fade out to at long distance.
uniform vec3 SkyColour;

// The far clip plane.  (I don't think I need to worry about the near
// one for fade-to-sky.)
uniform float FarClipDistance;

in GeometryData
{
    vec3 normal;
    vec3 jigsawCoord;
} inData;

struct Light
{
    vec3 Colour;
    vec3 Direction; 
    float Ambient;
    float Diffuse;
};

uniform Light gLight;

void main()
{
    // TODO Debugging -- let's see if absent vapours is the cause
    // of the flicker.
    //vec3 colour = textureLod(JigsawDensityTex, inData.jigsawCoord, 0).xyz;
    vec3 colour = normalize(vec3(1.0, 1.0, 1.0));
    vec3 normal = normalize(inData.normal);

    vec3 AmbientColour = gLight.Colour * gLight.Ambient;
    vec3 DiffuseColour = gLight.Colour * gLight.Diffuse * max(dot(normal, -gLight.Direction), 0.0);
    float DiffuseFactor = dot(normal, -gLight.Direction);
    vec3 CombinedColour = colour * (AmbientColour + DiffuseColour * DiffuseFactor);

    float depth = (gl_FragCoord.z / gl_FragCoord.w) / FarClipDistance;
    gl_FragColor = mix(
        vec4(CombinedColour, 1.0),
        vec4(SkyColour, 1.0),
        depth);
}

