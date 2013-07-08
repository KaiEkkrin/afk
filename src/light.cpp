/* AFK (c) Alex Holloway 2013 */

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

