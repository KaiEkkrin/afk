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

// The sky colour, which I fade out to at long distance.
uniform vec3 SkyColour;

// The far clip plane.  (I don't think I need to worry about the near
// one for fade-to-sky.)
uniform float FarClipDistance;

in GeometryData
{
    vec3 colour;
    vec3 normal;
} inData;

out vec4 fragColour;

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
    vec3 colour = inData.colour;
    vec3 normal = normalize(inData.normal);

    vec3 AmbientColour = gLight.Colour * gLight.Ambient;
    vec3 DiffuseColour = gLight.Colour * gLight.Diffuse * max(dot(normal, -gLight.Direction), 0.0);
    float DiffuseFactor = dot(normal, -gLight.Direction);
    vec3 CombinedColour = colour * (AmbientColour + DiffuseColour * DiffuseFactor);

    float depth = (gl_FragCoord.z / gl_FragCoord.w) / FarClipDistance;
    fragColour = mix(
        vec4(CombinedColour, 1.0),
        vec4(SkyColour, 1.0),
        depth);
}

