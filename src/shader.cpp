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

#include "afk.hpp"

#include <cassert>
#include <iostream>
#include <sstream>

#include "exception.hpp"
#include "file/readfile.hpp"
#include "shader.hpp"

struct shaderSpec shaders[] = {
    {   GL_FRAGMENT_SHADER, 0,  "landscape_fragment",   {   "landscape_fragment.glsl",  "", "", "", "", }, },
    {   GL_GEOMETRY_SHADER, 0,  "landscape_geometry",   {   "landscape_geometry.glsl",  "", "", "", "", }, },
    {   GL_VERTEX_SHADER,   0,  "landscape_vertex",     {   "landscape_vertex.glsl",    "", "", "", "", }, },
    {   GL_FRAGMENT_SHADER, 0,  "protagonist_fragment", {   "protagonist_fragment.glsl","", "", "", "", }, },
    {   GL_VERTEX_SHADER,   0,  "protagonist_vertex",   {   "protagonist_vertex.glsl",  "", "", "", "", }, },
    {   GL_FRAGMENT_SHADER, 0,  "shape_fragment",       {   "shape_fragment.glsl",      "", "", "", "", }, },
    {   GL_GEOMETRY_SHADER, 0,  "shape_geometry",       {   "shape_geometry.glsl",      "", "", "", "", }, },
    {   GL_VERTEX_SHADER,   0,  "shape_vertex",         {   "shape_vertex.glsl",        "", "", "", "", }, },
    {   0,                  0,  "",                     {   "", "", "", "", "", }, }
};


/* Loads a shader from the given files. */
static void loadShaderFromFiles(struct shaderSpec *s)
{
    GLchar **sources;
    size_t *sourceLengths;

    int success = 0;
    std::ostringstream errStream;

    int sourceCount;
    for (sourceCount = 0; sourceCount < AFK_GLSL_MAX_SOURCE_FILES && s->filenames[sourceCount].length() > 0; ++sourceCount);

    assert(sourceCount > 0);
    sources = new GLchar *[sourceCount];
    sourceLengths = new size_t[sourceCount];

    for (int i = 0; i < sourceCount; ++i)
    {
        std::cout << "AFK: Loading file for shader " << s->shaderName << ": " << s->filenames[i] << std::endl;

        if (!afk_readFileContents(s->filenames[i], &sources[i], &sourceLengths[i], errStream))
            throw AFK_Exception("AFK_Shader: " + errStream.str());
    }

    /* Compile the shader */
    GLint *sourceLengthsInt = new GLint[sourceCount];
    for (int i = 0; i < sourceCount; ++i)
        sourceLengthsInt[i] = (GLint)sourceLengths[i];

    s->obj = glCreateShader(s->shaderType);
    if (!s->obj) throw AFK_Exception("Failed to create shader");

    glShaderSource(s->obj, sourceCount, (const GLchar **)sources, sourceLengthsInt);
    glCompileShader(s->obj);
    glGetShaderiv(s->obj, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLchar infoLog[1024];
        glGetShaderInfoLog(s->obj, 1024, nullptr, infoLog);
        throw AFK_Exception("Error compiling shader " + s->shaderName + ": " + infoLog);
    }

    delete[] sources;
    delete[] sourceLengths;
    delete[] sourceLengthsInt;
}

void afk_loadShaders(const std::string& shadersDir)
{
    int shIdx;
    std::ostringstream errStream;

    /* We chdir() into the shaders directory to load this stuff, so save
     * out the previous directory to go back to afterwards. */
    
    if (!afk_pushDir(shadersDir, errStream))
        throw AFK_Exception("AFK_Shader: Unable to switch to shaders dir: " + errStream.str());

    for (shIdx = 0; !shaders[shIdx].shaderName.empty(); ++shIdx)
        loadShaderFromFiles(&shaders[shIdx]);

    if (!afk_popDir(errStream))
        throw AFK_Exception("AFK_Shader: Unable to leave shaders dir: " + errStream.str());
}


AFK_ShaderProgram::AFK_ShaderProgram()
{
    program = glCreateProgram();
}

AFK_ShaderProgram::~AFK_ShaderProgram()
{
    GLuint shaders[10];
    GLsizei shaderCount = 0;

    glGetAttachedShaders(program, 10, &shaderCount, shaders);
    for (int i = 0; i < shaderCount; ++i)
        glDeleteShader(shaders[i]);

    glDeleteProgram(program);
}

AFK_ShaderProgram& AFK_ShaderProgram::operator<<(const std::string& shaderName)
{
    int foundIt = 0;

    for (int i = 0; !foundIt && !shaders[i].shaderName.empty(); ++i)
    {
        if (shaders[i].shaderName.compare(0, shaderName.length(), shaderName) == 0)
        {
            glAttachShader(program, shaders[i].obj);
            foundIt = 1;
        }
    }

    if (!foundIt) throw AFK_Exception("AFK_Shader: Couldn\'t find shader " + shaderName);
    return *this;
}

void AFK_ShaderProgram::Link(void)
{
    int success = 0;
    GLchar errorLog[1024] = { 0 };

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == 0) {
        glGetProgramInfoLog(program, sizeof(errorLog), nullptr, errorLog);
        throw AFK_Exception(std::string("AFK_Shader: Error linking program: ") + errorLog);
    }

    /* It's a good bet I'm about to try to use this for something */
    glUseProgram(program);
}

void AFK_ShaderProgram::Validate(void)
{
    int success = 0;
    GLchar errorLog[1024] = { 0 };

    glValidateProgram(program);
    glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(errorLog), nullptr, errorLog);
        throw AFK_Exception(std::string("AFK_Shader: Invalid shader program: ") + errorLog);
    }
}

