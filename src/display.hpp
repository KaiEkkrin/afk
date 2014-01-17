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

#ifndef _AFK_DISPLAY_H_
#define _AFK_DISPLAY_H_

#include "afk.hpp"

#include <vector>

#include "exception.hpp"
#include "light.hpp"
#include "object.hpp"
#include "shader.hpp"


#define AFK_GLCHK(message) \
    { \
        GLenum glErr = glGetError(); \
        if (glErr != GL_NO_ERROR) \
        { \
            std::ostringstream ss; \
            ss << "AFK: Got GL error from " << message << ": " << gluErrorString(glErr); \
            throw AFK_Exception(ss.str()); \
        } \
    }


/* Describes the layout of a vertex for the protagonist
 * shader sequence.
 */
struct AFK_ProtagonistVertex
{
    Vec3<float> location;
    Vec3<float> colour;
    Vec3<float> normal;
};

struct AFK_ProtagonistIndex
{
    unsigned int i[3];
};


class AFK_DisplayedObject
{
protected:
    /* Call this to write the correct transform to the
     * `transform' uniform variable. */
    void updateTransform(const Mat4<float>& projection);

public:
    /* The shader program this object uses (and manages).
     * Use NULL if the shader program comes from elsewhere.
     */
    AFK_ShaderProgram *shaderProgram;

    /* The world transform goes here. */
    GLuint worldTransformLocation;

    /* The clip transform goes here. */
    GLuint clipTransformLocation;

    /* The light goes here. */
    AFK_ShaderLight *shaderLight;

    /* Vertices and indices */
    GLuint bufs[2];

    /* The "Object" describes how this object is scaled,
     * rotated and transformed, etc.
     */
    AFK_Object object;

    AFK_DisplayedObject();
    virtual ~AFK_DisplayedObject();

    /* Run these two in the main thread: OpenGL seems to be
     * unhappy about being called from other threads...
     */
    virtual void initGL(void) = 0;

    virtual void display(const Mat4<float>& projection) = 0;
};

class AFK_DisplayedProtagonist: public AFK_DisplayedObject
{
public:
    virtual void initGL(void);
    virtual void display(const Mat4<float>& projection);
};

void afk_display(unsigned int threadId);

#endif /* _AFK_DISPLAY_H_ */

