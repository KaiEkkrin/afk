/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DISPLAY_H_
#define _AFK_DISPLAY_H_

#include <GL/gl.h>

#include "object.h"
#include "shader.h"

class AFK_DisplayedObject
{
public:
    /* The shader program this object uses. */
    AFK_ShaderProgram shaderProgram;

    /* The uniform variable in the shader program that
     * contains the transform to apply to this object when
     * rendered. */
    GLuint transformLocation;

    /* The "Object" describes how this object is scaled,
     * rotated and transformed, etc.
     * TODO: With instancing, I'm no doubt going to end
     * up wanting many of these sometimes; consider this.
     */
    AFK_Object object;

    AFK_DisplayedObject() {}

    /* TODO This class needs a destructor that frees the
     * OpenGL related resources, and so forth. */

    virtual void init(void) = 0;
    virtual void updateTransform(const Mat4<float>& projection);
    virtual void display(void) = 0;
};

/* TODO Now follow declarations of some things I'm
 * using to test.  Maybe I want to move these, etc? */
class AFK_DisplayedTestObject: public AFK_DisplayedObject
{
public:
    /* The vertex set. */
    GLuint vertices;

    AFK_DisplayedTestObject() {}

    void init(void);
    void display(void);
};

class AFK_DisplayedLandscapeObject: public AFK_DisplayedObject
{
public:
    /* The vertex set. */
    GLuint vertices;

    /* The size of the vertex set. */
    int vertCount;

    AFK_DisplayedLandscapeObject() {}

    void init(void);
    void display(void);
};

void afk_displayInit(void);

/* GLUT callback functions. */
void afk_display(void);
void afk_reshape(int width, int height);

/* Calculates the intended contents of the next frame. */
void afk_nextFrame(void);

#endif /* _AFK_DISPLAY_H_ */

