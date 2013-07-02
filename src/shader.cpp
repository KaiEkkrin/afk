/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include "exception.hpp"
#include "shader.hpp"

struct shaderSpec shaders[] = {
    {   GL_FRAGMENT_SHADER, 0,  "basic_fragment.glsl"           },
    {   GL_VERTEX_SHADER,   0,  "basic_vertex.glsl"             },
    {   GL_FRAGMENT_SHADER, 0,  "vcol_phong_fragment.glsl"      },
    {   GL_VERTEX_SHADER,   0,  "vcol_phong_vertex.glsl"        },
    {   GL_FRAGMENT_SHADER, 0,  "vertexColour_fragment.glsl"    },
    {   GL_VERTEX_SHADER,   0,  "vertexColour_vertex.glsl"      },
    {   0,                  0,  ""                              }
};


/* Loads a shader from the given file. */
static void loadShaderFromFile(struct shaderSpec *s)
{
    FILE *f = NULL;
    GLchar *data[1] = { 0 };
    GLint lengths[1] = { 0 };
    int success = 0;

    std::cout << "AFK: Loading shader: " << s->filename << std::endl;

    /* Load the shader text from the file */

    f = fopen(s->filename.c_str(), "rb");
    if (!f) throw AFK_Exception("AFK_Shader: Failed to open " + s->filename + ": " + strerror(errno));

    if (fseek(f, 0, SEEK_END) != 0)
        throw AFK_Exception("AFK_Shader: Failed to seek to end of " + s->filename + ": " + strerror(errno));

    lengths[0] = ftell(f);
    if (lengths[0] < 0)
        throw AFK_Exception("AFK_Shader: Failed to find size of " + s->filename + ": " + strerror(errno));

    if (fseek(f, 0, SEEK_SET) != 0)
        throw AFK_Exception("AFK_Shader: Failed to seek to beginning of " + s->filename + ": " + strerror(errno));

    data[0] = (char *) malloc(sizeof(char) * lengths[0]);
    if (!data[0])
    {
        std::ostringstream errmess;
        errmess << "AFK_Shader: Failed to allocate " << lengths[0] << "bytes for file " << s->filename;
        throw AFK_Exception(errmess.str());
    }

    {
        char *read_pos = data[0];
        int length_left = lengths[0];
        int length_read;

        while (!feof(f) && length_left > 0)
        {
            length_read = fread(read_pos, 1, length_left, f);
            if (length_read == 0 && ferror(f))
                throw AFK_Exception("AFK_Shader: Failed to read from " + s->filename + ": " + strerror(errno));

            read_pos += length_read;
            length_left -= length_read;
        }
    }

    /* Compile the shader */

    s->obj = glCreateShader(s->shaderType);
    if (!s->obj) throw AFK_Exception("Failed to create shader");

    glShaderSource(s->obj, 1, (const GLchar **)data, lengths);
    glCompileShader(s->obj);
    glGetShaderiv(s->obj, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLchar infoLog[1024];
        glGetShaderInfoLog(s->obj, 1024, NULL, infoLog);
        throw AFK_Exception("Error compiling shader " + s->filename + ": " + infoLog);
    }

    free(data[0]);
}

void afk_loadShaders(const char *shadersDir)
{
    int shIdx;
    char *savedDir = NULL;

    /* We chdir() into the shaders directory to load this stuff, so save
     * out the previous directory to go back to afterwards. */
    savedDir = get_current_dir_name();
    if (chdir(shadersDir) == -1)
        throw AFK_Exception(std::string("AFK_Shader: Unable to switch to shaders dir ") + shadersDir + ": " + strerror(errno));

    for (shIdx = 0; !shaders[shIdx].filename.empty(); ++shIdx)
        loadShaderFromFile(&shaders[shIdx]);

    if (chdir(savedDir) == -1)
        std::cerr << "Couldn\'t return to saved directory " << savedDir << "; ignoring" << std::endl;

    free(savedDir);
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

    for (int i = 0; !foundIt && !shaders[i].filename.empty(); ++i)
    {
        if (shaders[i].filename.compare(0, shaderName.length(), shaderName) == 0)
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
        glGetProgramInfoLog(program, sizeof(errorLog), NULL, errorLog);
        throw AFK_Exception(std::string("AFK_Shader: Error linking program: ") + errorLog);
    }

    glValidateProgram(program);
    glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(errorLog), NULL, errorLog);
        throw AFK_Exception(std::string("AFK_Shader: Invalid shader program: ") + errorLog);
    }
}

