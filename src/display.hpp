/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DISPLAY_H_
#define _AFK_DISPLAY_H_

#include "afk.hpp"
#include "object.hpp"
#include "shader.hpp"

class AFK_DisplayedObject
{
protected:
    /* Call this to write the correct transform to the
     * `transform' uniform variable. */
    void updateTransform(const Mat4<float>& projection);

public:
    /* The shader program this object uses. */
    AFK_ShaderProgram shaderProgram;

    /* The uniform variable in the shader program that
     * contains the transform to apply to this object when
     * rendered. */
    GLuint transformLocation;

    /* The location of the `fixed color' variable. */
    GLuint fixedColorLocation;

    /* The "Object" describes how this object is scaled,
     * rotated and transformed, etc.
     * TODO: With instancing, I'm no doubt going to end
     * up wanting many of these sometimes; consider this.
     */
    AFK_Object object;

    /* This object's base colour. */
    Vec3<float> colour;

    AFK_DisplayedObject() {}
    virtual ~AFK_DisplayedObject() {}

    virtual void display(const Mat4<float>& projection) = 0;
};

/* TODO Now follow declarations of some things I'm
 * using to test.  Maybe I want to move these, etc? */
class AFK_DisplayedTestObject: public AFK_DisplayedObject
{
public:
    /* The vertex set. */
    GLuint vertices;

    AFK_DisplayedTestObject();
    virtual ~AFK_DisplayedTestObject();

    virtual void display(const Mat4<float>& projection);
};

class AFK_DisplayedProtagonist: public AFK_DisplayedObject
{
public:
    /* Vertices and indices. */
    GLuint bufs[2];

    AFK_DisplayedProtagonist();
    virtual ~AFK_DisplayedProtagonist();

    virtual void display(const Mat4<float>& projection);
};

/* GLUT callback functions. */
void afk_display(void);
void afk_reshape(int width, int height);

#endif /* _AFK_DISPLAY_H_ */
