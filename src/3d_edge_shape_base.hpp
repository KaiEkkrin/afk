/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_3DEDGESHAPE_BASE_H_
#define _AFK_3DEDGESHAPE_BASE_H_

#include "afk.hpp"

#include <vector>

#include "def.hpp"
#include "shape_sizes.hpp"

/* This class defines the basis of a 3D edge-detected cube, used to
 * make random Shapes.
 * TODO: As I've defined in world and shape_3dedge.cl: This base shape needs to change to
 * paint six faces, whose co-ordinates are arranged in the edge jigsaw
 * in a 3 (x) by 2 (y) formation:
 * back     right   top
 * bottom   left    front
 */

class AFK_3DEdgeShapeBase
{
protected:
    std::vector<Vec2<float> > vertices;
    std::vector<unsigned short> indices;

    GLuint vertexArray;
    GLuint *bufs;
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

