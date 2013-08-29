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

    void teardownGL(void) const;
};

#endif /* _AFK_3DEDGESHAPE_BASE_H_ */

