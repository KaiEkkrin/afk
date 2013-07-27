/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_H_
#define _AFK_SHAPE_H_

#include "afk.hpp"

#include "data/render_list.hpp"
#include "def.hpp"
#include "display.hpp"
#include "light.hpp"
#include "shader.hpp"

/* A Shape describes an entity shape -- the geometry
 * of an object that might be instanced and moved about
 * around the world.
 * There won't be many instances of the Shape class around
 * -- just one per object type.
 * In future, I want to come up with a cunning algorithm
 * for randomly generating Shapes, just like I'm now
 * randomly generating Terrain.  Also for doing shape
 * LoD splitting and all sorts.  But for now, just this
 * is fine ...
 */
class AFK_Shape
{
protected:
    /* The shape's geometry. */
    AFK_DisplayedBuffer<struct AFK_VcolPhongVertex>     vs;
    AFK_DisplayedBuffer<Vec3<unsigned int> >            is;

    /* I'm going to try making the Shapes themselves
     * responsible for drawing.
     * Each shape has a pair of flippable draw lists.
     * The draw lists are further split, one for each
     * worker thread so that I don't get concurrency
     * issues.  What they are is a list of transformations
     * of the objects to the world.
     */
    AFK_RenderList<Mat4<float> > renderList;

    /* The contents of the draw lists get plugged into these
     * things for transferral to the shape shaders.
     * There's one per thread id (i.e. one per simultaneous
     * draw list).
     */
    GLuint *drawListBuf;
    GLuint *drawListTex;

    unsigned int threadCount;

public:
    AFK_Shape(size_t vCount, size_t iCount, unsigned int _threadCount);
    virtual ~AFK_Shape();

    /* A kind-of-slow function that resizes all the geometry
     * into (0, 1) bounds.  I'm going to want this for testing,
     * because it makes sure the shape fits within a sort of
     * "shape space".  I don't think I want to keep this once
     * I'm doing random shape generation though...
     */
    void resizeToShapeSpace(void);

    /* Call this function to enqueue an instance of the shape
     * for rendering.
     */
    void updatePush(unsigned int threadId, const Mat4<float>& transform);

    /* Call this function to display all instances of the
     * shapes.  worldTransformListLocation should point to
     * the uniform buffer variable I fill out with the
     * transform matrices.  projectionTransformLocation points
     * to the uniform buffer variable for the projection matrix --
     * the vertex shader will need to compose the clip transform
     * itself, from that projection matrix and the relevant
     * world transform matrix for the instance.
     */
    void display(
        AFK_ShaderLight *shaderLight,
        const struct AFK_Light& globalLight,
        GLuint projectionTransformLocation,
        const Mat4<float>& projection);
};

/* Here I'll describe some fixed-geometry Shapes that I'm
 * using to test.
 */
class AFK_ShapeChevron: public AFK_Shape
{
public:
    AFK_ShapeChevron(unsigned int _threadCount);
};

#endif /* _AFK_SHAPE_H_ */

