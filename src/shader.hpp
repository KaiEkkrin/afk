/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHADER_H_
#define _AFK_SHADER_H_

#include "afk.hpp"

#include <string>

/* TODO Sort out detecting support for required OpenGL features / extensions ... */

/* This is a list of all the shaders that I know about.
 * To ask for particular shaders to be compiled into a
 * program, make a new AFK_ShaderProgram(), push the
 * desired shaders in by string name (using the << operator),
 * and then call Link() to get back a GLuint referring to
 * the program itself. */
struct shaderSpec
{
    GLuint shaderType;
    GLuint obj;
    std::string filename;
};


/* Loads all the known shaders from disk and compiles them
 * individually.
 */
void afk_loadShaders(const char *shadersDir);

class AFK_ShaderProgram
{
public:
    GLuint program;

    AFK_ShaderProgram();
    ~AFK_ShaderProgram();

    AFK_ShaderProgram& operator<<(const std::string& shaderName);
    void Link(void);
};

#endif /* _AFK_SHADER_H_ */
