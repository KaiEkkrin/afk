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

#include "light.hpp"

AFK_ShaderLight::AFK_ShaderLight(GLuint program)
{
    colourField     = glGetUniformLocation(program, "gLight.Colour");
    directionField  = glGetUniformLocation(program, "gLight.Direction");
    ambientField    = glGetUniformLocation(program, "gLight.Ambient");
    diffuseField    = glGetUniformLocation(program, "gLight.Diffuse");
}

void AFK_ShaderLight::setupLight(const struct AFK_Light& light)
{
    glUniform3f(colourField,    light.colour.v[0],      light.colour.v[1],      light.colour.v[2]);
    glUniform3f(directionField, light.direction.v[0],   light.direction.v[1],   light.direction.v[2]);
    glUniform1f(ambientField,   light.ambient);
    glUniform1f(diffuseField,   light.diffuse);
}

