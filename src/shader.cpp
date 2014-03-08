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

#include "afk.hpp"

#include <cassert>
#include <cstdarg>
#include <iostream>
#include <sstream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#include "display.hpp"
#include "exception.hpp"
#include "file/filter.hpp"
#include "file/logstream.hpp"
#include "file/readfile.hpp"
#include "shader.hpp"
#include "shape_sizes.hpp"

/* TODO: Remove this, and activate the new distant, swarm, net shaders --
 * starting to add them below.
 */
#define RENDER_SHAPE_AS_POINTS 1

AFK_ShaderSpec::AFK_ShaderSpec(
    GLuint _shaderType,
    const std::string& _shaderName,
    const std::initializer_list<std::string>& _filenames
    ) :
    shaderType(_shaderType),
    obj(0),
    shaderName(_shaderName),
    filenames(_filenames)
{
}

std::vector<AFK_ShaderSpec> shaders = {
    AFK_ShaderSpec(GL_FRAGMENT_SHADER, "landscape_fragment", { "landscape_fragment.glsl" }),
    AFK_ShaderSpec(GL_GEOMETRY_SHADER, "landscape_geometry", { "landscape_geometry.glsl" }),
    AFK_ShaderSpec(GL_VERTEX_SHADER, "landscape_vertex", { "landscape_vertex.glsl" }),
    AFK_ShaderSpec(GL_FRAGMENT_SHADER, "protagonist_fragment", { "protagonist_fragment.glsl" }),
    AFK_ShaderSpec(GL_VERTEX_SHADER, "protagonist_vertex", { "protagonist_vertex.glsl" }),
    AFK_ShaderSpec(GL_FRAGMENT_SHADER, "shape_fragment", { "shape_fragment.glsl" }),
    AFK_ShaderSpec(GL_GEOMETRY_SHADER, "distant_shape_geometry", { "shape_point_size.glsl", "distant_shape_geometry.glsl" }),
    AFK_ShaderSpec(GL_VERTEX_SHADER, "distant_shape_vertex", { "distant_shape_vertex.glsl" }),
    AFK_ShaderSpec(GL_GEOMETRY_SHADER, "swarm_shape_geometry", { "shape_point_size.glsl", "swarm_shape_geometry.glsl" }),
    AFK_ShaderSpec(GL_VERTEX_SHADER, "swarm_shape_vertex", { "swarm_shape_vertex.glsl" }),
};


/* Loads a shader from the given files.
 * `filters' is a list of pairs (shader name prefix, FileFilter).
 */
static void loadShaderFromFiles(
    std::vector<AFK_ShaderSpec>::iterator& s,
    const std::vector<std::pair<std::string, AFK_FileFilter *> >& filters)
{
    GLchar **sources;
    size_t *sourceLengths;

    int success = 0;
    std::ostringstream errStream;

    size_t sourceCount = s->filenames.size();

    assert(sourceCount != 0);
    sources = new GLchar *[sourceCount];
    sourceLengths = new size_t[sourceCount];

    for (size_t i = 0; i < sourceCount; ++i)
    {
        afk_out << "AFK: Loading file for shader " << s->shaderName << "( type " << s->shaderType << "): " << s->filenames[i] << std::endl;

        if (!afk_readFileContents(s->filenames[i], &sources[i], &sourceLengths[i], errStream))
            throw AFK_Exception("AFK_Shader: " + errStream.str());
    }

    for (auto f : filters)
    {
        if (boost::starts_with(s->shaderName, f.first))
        {
            const AFK_FileFilter& filter = *(f.second);
            afk_out << "AFK: Shader " << s->shaderName << ": Applying filter: " << filter << std::endl;
            filter.filter(sourceCount, sources, sourceLengths);
        }
    }

    /* Compile the shader */
    GLint *sourceLengthsInt = new GLint[sourceCount];
    for (size_t i = 0; i < sourceCount; ++i)
        sourceLengthsInt[i] = (GLint)sourceLengths[i];

    s->obj = glCreateShader(s->shaderType);
    if (!s->obj) AFK_GLCHK("Create shader");

    glShaderSource(s->obj, static_cast<GLsizei>(sourceCount), (const GLchar **)sources, sourceLengthsInt);
    glCompileShader(s->obj);
    glGetShaderiv(s->obj, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLchar infoLog[1024];
        glGetShaderInfoLog(s->obj, 1024, nullptr, infoLog);
        throw AFK_Exception("Error compiling shader " + s->shaderName + ": " + infoLog);
    }

    for (size_t i = 0; i < sourceCount; ++i)
    {
        free(sources[i]);
    }
    delete[] sources;
    delete[] sourceLengths;
    delete[] sourceLengthsInt;
}

void afk_loadShaders(const AFK_ConfigSettings& settings)
{
    std::ostringstream errStream;

    /* We chdir() into the shaders directory to load this stuff, so save
     * out the previous directory to go back to afterwards. */
    
    if (!afk_pushDir(settings.shadersDir, errStream))
        throw AFK_Exception("AFK_Shader: Unable to switch to shaders dir: " + errStream.str());

    /* For the shape shaders, I want to apply a filter: */
    AFK_ShapeSizes sSizes(settings);
    AFK_FileFilter shapeFilter = {
        "\\bVDIM\\b",                       boost::lexical_cast<std::string>(sSizes.vDim),
        "\\bEDIM\\b",                       boost::lexical_cast<std::string>(sSizes.eDim),
        "\\bTDIM\\b",                       boost::lexical_cast<std::string>(sSizes.tDim),
        "\\bLAYERS\\b",                     boost::lexical_cast<std::string>(sSizes.layers),
        "\\bLAYER_BITNESS\\b",              boost::lexical_cast<std::string>(sSizes.layerBitness)
    };

    std::vector<std::pair<std::string, AFK_FileFilter *> > filters = {
        {   "shape_",       &shapeFilter    },
    };

    for (auto shaderIt = shaders.begin(); shaderIt != shaders.end(); ++shaderIt)
        loadShaderFromFiles(shaderIt, filters);

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
    bool foundIt = false;

    for (auto s : shaders)
    {
        if (s.shaderName.compare(0, s.shaderName.length(), shaderName) == 0)
        {
            glAttachShader(program, s.obj);
            foundIt = true;
            break;
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

