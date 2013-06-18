/* AFK (c) Alex Holloway 2013 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <GL/glew.h>

#include "shader.h"

struct shaderSpec
{
    GLuint shaderType;
    GLuint obj;
    char filename[256];
};

struct shaderSpec shaders[] = {
    {   GL_VERTEX_SHADER,   0,  "test_vertex.glsl"   },
    {   GL_FRAGMENT_SHADER, 0,  "test_fragment.glsl" },
    {   0,                  0,  "\0"                }
};

/* Loads a shader from the given file. */
static int loadShaderFromFile(GLuint shaderProgram, struct shaderSpec *s)
{
    FILE *f = NULL;
    GLchar *data[1] = { 0 };
    GLint lengths[1] = { 0 };
    int success = 0;

    printf("AFK: Loading shader: %s\n", s->filename);

    /* Load the shader text from the file */

    f = fopen(s->filename, "rb");
    if (!f)
    {
        fprintf(stderr, "Failed to open %s: %s\n", s->filename, strerror(errno));
        goto finished;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fprintf(stderr, "Failed to seek to end of %s: %s\n", s->filename, strerror(errno));
        goto finished;
    }

    lengths[0] = ftell(f);
    if (lengths[0] < 0)
    {
        fprintf(stderr, "Failed to find size of %s: %s\n", s->filename, strerror(errno));
        goto finished;
    }

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, "Failed to seek to beginning of %s: %s\n", s->filename, strerror(errno));
        goto finished;
    }

    data[0] = (char *) malloc(sizeof(char) * lengths[0]);
    if (!data[0])
    {
        fprintf(stderr, "Failed to allocate %d bytes for file %s\n", lengths[0], s->filename);
        goto finished;
    }

    {
        char *read_pos = data[0];
        int length_left = lengths[0];
        int length_read;

        while (!feof(f) && length_left > 0)
        {
            length_read = fread(read_pos, 1, length_left, f);
            if (length_read == 0 && ferror(f))
            {
                fprintf(stderr, "Failed to read from %s: %s\n", s->filename, strerror(errno));
                goto finished;
            }

            read_pos += length_read;
            length_left -= length_read;
        }
    }

    /* Compile the shader */

    s->obj = glCreateShader(s->shaderType);
    if (!s->obj)
    {
        fprintf(stderr, "Failed to create shader\n");
        goto finished;
    }

    glShaderSource(s->obj, 1, (const GLchar **)data, lengths);
    glCompileShader(s->obj);
    glGetShaderiv(s->obj, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLchar infoLog[1024];
        glGetShaderInfoLog(s->obj, 1024, NULL, infoLog);
        fprintf(stderr, "Error compiling shader %s: %s\n", s->filename, infoLog);
        goto finished;
    }

    glAttachShader(shaderProgram, s->obj);
    success = 1;

finished:
    if (data[0]) free(data[0]);
    if (f) fclose(f);
    return success;
}

int afk_compileShaders(const char *shadersDir, GLuint *o_shaderProgram)
{
    int shIdx;
    int success = 0;
    char *savedDir = NULL;
    GLchar errorLog[1024] = { 0 };

    /* We chdir() into the shaders directory to load this stuff, so save
     * out the previous directory to go back to afterwards. */
    savedDir = get_current_dir_name();
    if (chdir(shadersDir) == -1)
    {
        fprintf(stderr, "Unable to switch to shaders dir %s: %s\n", shadersDir, strerror(errno));
        goto finished;
    }

    *o_shaderProgram = glCreateProgram();

    for (shIdx = 0; shaders[shIdx].filename[0] != '\0'; ++shIdx)
    {
        if (!loadShaderFromFile(*o_shaderProgram, &shaders[shIdx])) goto finished;
    }

    /* TODO This code nabbed from a sample.  Do more centralised, abstracted
     * error reporting; test for features present or not; all that stuff. */

    printf("AFK: Setting up shader program...\n");
    
    glLinkProgram(*o_shaderProgram);
    glGetProgramiv(*o_shaderProgram, GL_LINK_STATUS, &success);
    if (success == 0) {
        glGetProgramInfoLog(*o_shaderProgram, sizeof(errorLog), NULL, errorLog);
        fprintf(stderr, "Error linking shader program: '%s'\n", errorLog);
        goto finished;
    }

    glValidateProgram(*o_shaderProgram);
    glGetProgramiv(*o_shaderProgram, GL_VALIDATE_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(*o_shaderProgram, sizeof(errorLog), NULL, errorLog);
        fprintf(stderr, "Invalid shader program: '%s'\n", errorLog);
        goto finished;
    }

    glUseProgram(*o_shaderProgram);
    success = 1;

finished:
    chdir(savedDir);
    free(savedDir);

    return success;
}

