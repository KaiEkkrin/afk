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
    Vec3<float> location;
    Vec3<float> colour;
    Vec3<float> normal;
};

/* Nasty nasty.  I can't refer to the singleton afk_core from
 * inside a template
 */
void afk_displayedBufferGlBuffersForDeletion(GLuint *bufs, size_t bufsSize);

/* A useful wrapper around an array of things to be buffered
 * to GL and its GL equivalent.
 */
template<typename T>
class AFK_DisplayedBuffer
{
public:
    T *ts;
    unsigned int count;
    GLuint buf;

    AFK_DisplayedBuffer(): ts(NULL), count(0), buf(0) {}
    virtual ~AFK_DisplayedBuffer()
    {
        /* Because of GLUT wanting all GL calls in the same thread */
        if (buf) afk_displayedBufferGlBuffersForDeletion(&buf, 1);
        if (ts) delete[] ts;
    }

    void alloc(unsigned int _count)
    {
        count = _count;
        ts = new T[_count];
    }

    void initGL(GLenum target, GLenum usage)
    {
        glGenBuffers(1, &buf);
        glBindBuffer(target, buf);
        glBufferData(target, count * sizeof(T), ts, usage);
    }
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

/* GLUT callback functions. */
void afk_display(void);
void afk_reshape(int width, int height);

#endif /* _AFK_DISPLAY_H_ */

