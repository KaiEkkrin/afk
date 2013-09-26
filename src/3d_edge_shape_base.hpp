/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_3DEDGESHAPE_BASE_H_
#define _AFK_3DEDGESHAPE_BASE_H_

#include "afk.hpp"

#include <vector>

#include "def.hpp"
#include "shape_sizes.hpp"

/* This class defines the basis of a 3D edge-detected cube, used to
 * make random Shapes.
 * To match the output of shape_3dedge, this base shape will
 * paint six faces, whose co-ordinates are arranged in the edge jigsaw
 * in a 3 (x) by 2 (y) formation:
 * back     right   top
 * bottom   left    front
 * The vertices are texture co-ordinates only, with the full 3D world
 * co-ordinates read from the texture.  I've done this even though
 * many of them are predictable to avoid really torturous logic in
 * the shaders.
 *
 * In order to avoid overlap of triangles from different faces, the
 * overlap edge texture will contain the face identifier each triangle strip
 * is homed to, so that the geometry shader can drop it from other faces.
 *
 * In an immense hack, the GLSL can calculate which face it is at by
 * computing (texcoord.x / vdim) % 3, and (texcoord.y / vdim) % 2 ...
 * saving me from having to do nasty things like make one draw call per face,
 * etc.
 */

class AFK_3DEdgeShapeBase
{
protected:
    std::vector<Vec2<float> > vertices;
    std::vector<unsigned short> indices;

    GLuint vertexArray;
    GLuint *bufs;

    /* Initialisation utility. */
    void pushBaseFace(unsigned int sOffset, unsigned int tOffset, bool flip, const AFK_ShapeSizes& sSizes);

public:
    AFK_3DEdgeShapeBase(const AFK_ShapeSizes& sSizes);
    virtual ~AFK_3DEdgeShapeBase();

    /* Assuming you've got the right VAO selected, sets up the vertices
     * and indices.
     */
    void initGL(void);

    /* Assuming all the textures are set up, issues the draw call. */
    void draw(unsigned int instanceCount) const;

    /* Tears down the VAO. */
    void teardownGL(void) const;
};

#endif /* _AFK_3DEDGESHAPE_BASE_H_ */

