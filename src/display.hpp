/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DISPLAY_H_
#define _AFK_DISPLAY_H_

#include "afk.hpp"
#include "light.hpp"
#include "object.hpp"
#include "shader.hpp"


/* Describes the layout of a vertex for the vcol_phong
 * shader sequence.
 */
struct AFK_VcolPhongVertex
{
    float       location[3];
    float       colour[3];
    float       normal[3];
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
     * TODO: With instancing, I'm no doubt going to end
     * up wanting many of these sometimes; consider this.
     */
    AFK_Object object;

    AFK_DisplayedObject();
    virtual ~AFK_DisplayedObject();

    virtual void display(const Mat4<float>& projection) = 0;
};

class AFK_DisplayedProtagonist: public AFK_DisplayedObject
{
public:
    AFK_DisplayedProtagonist();
    virtual ~AFK_DisplayedProtagonist();

    virtual void display(const Mat4<float>& projection);
};

/* GLUT callback functions. */
void afk_display(void);
void afk_reshape(int width, int height);

#endif /* _AFK_DISPLAY_H_ */

