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

#ifndef _AFK_SHADER_H_
#define _AFK_SHADER_H_

#include "afk.hpp"

#include <string>
#include <vector>

/* This is a list of all the shaders that I know about.
 * To ask for particular shaders to be compiled into a
 * program, make a new AFK_ShaderProgram(), push the
 * desired shaders in by string name (using the << operator),
 * and then call Link() to get back a GLuint referring to
 * the program itself.
 */
struct AFK_ShaderSpec
{
    GLuint shaderType;
    GLuint obj;
    std::string shaderName; /* friendly name to ask for */
    std::vector<std::string> filenames;
};


/* Loads all the known shaders from disk and compiles them
 * individually.
 */
void afk_loadShaders(const std::string& shadersDir);

class AFK_ShaderProgram
{
public:
    GLuint program;

    AFK_ShaderProgram();
    ~AFK_ShaderProgram();

    AFK_ShaderProgram& operator<<(const std::string& shaderName);
    void Link(void);

    /* Call right before drawing -- the validate is dependent on
     * how uniform variables, textures, samplers are set up
     */
    void Validate(void);
};

#endif /* _AFK_SHADER_H_ */

