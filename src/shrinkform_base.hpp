/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHRINKFORM_BASE_H_
#define _AFK_SHRINKFORM_BASE_H_

#include "afk.hpp"

#include <vector>

#include "def.hpp"
#include "shape_sizes.hpp"

/* This class defines the basis of a shrinkform cube, used to
 * make random Shapes.
 */

class AFK_ShrinkformBaseVertex
{
public:
    Vec3<float> location;
    Vec2<float> texCoord;

    AFK_ShrinkformBaseVertex(
        const Vec3<float>& _location,
        const Vec2<float>& _texCoord);
} __attribute__((aligned(16)));

#define SIZEOF_BASE_VERTEX 32

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_ShrinkformBaseVertex>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_ShrinkformBaseVertex>::value));

class AFK_ShrinkformBase
{
protected:
    std::vector<AFK_ShrinkformBaseVertex> vertices;
    std::vector<unsigned short> indices;

    GLuint vertexArray;
    GLuint *bufs;
public:
    AFK_ShrinkformBase(const AFK_ShapeSizes& sSizes);
    virtual ~AFK_ShrinkformBase();

    /* Assuming you've got the right VAO selected, sets up the vertices
     * and indices.
     */
    void initGL(void);

    void teardownGL(void) const;
};

#endif /* _AFK_SHRINKFORM_BASE_H_ */

