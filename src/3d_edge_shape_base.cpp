/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "3d_edge_shape_base.hpp"
#include "display.hpp"

#define RESTART_INDEX 65535

/* This utility function gets two-dimensional face co-ordinates.
 */
static void make2DTexCoords(
    const AFK_ShapeSizes& sSizes,
    unsigned int sOffset,
    unsigned int tOffset,
    unsigned int s,
    unsigned int t,
    float& o_sTex,
    float& o_tTex)
{
    /* The texture co-ordinates for each jigsaw piece range
     * from (0, 1), with eDim texels along each side, including
     * the join-up to the next face.  Therefore, to access the
     * correct texels, I need to skip the padding, like so:
     * (The +0.25f offset is to stop nvidia cards from nearest-
     * neighbour sampling down to the *previous* point, which they
     * seem to tend to do with no offset ... )
     */
    o_sTex = (float)sOffset + ((float)s + 0.25f) / (float)sSizes.eDim;
    o_tTex = (float)tOffset + ((float)t + 0.25f) / (float)sSizes.eDim;
}

void AFK_3DEdgeShapeBase::pushBaseFace(unsigned int sOffset, unsigned int tOffset, bool flip, const AFK_ShapeSizes& sSizes)
{
    unsigned short texOffset = (unsigned short)vertices.size();

    for (unsigned int x = 0; x < sSizes.eDim; ++x)
    {
        for (unsigned int z = 0; z < sSizes.eDim; ++z)
        {
            float sTex, tTex;
            make2DTexCoords(sSizes, sOffset, tOffset, x, z, sTex, tTex);
            vertices.push_back(afk_vec2<float>(sTex, tTex));
        }
    }

    for (unsigned short s = 0; s < sSizes.pointSubdivisionFactor; ++s)
    {
        for (unsigned short t = 0; t < sSizes.pointSubdivisionFactor; ++t)
        {
            unsigned short i_r1c1 = texOffset + s * sSizes.eDim + t;
            unsigned short i_r2c1 = texOffset + (s + 1) * sSizes.eDim + t;
            unsigned short i_r1c2 = texOffset + s * sSizes.eDim + (t + 1);
            unsigned short i_r2c2 = texOffset + (s + 1) * sSizes.eDim + (t + 1);

            indices.push_back(i_r1c1);

            if (flip)
            {
                indices.push_back(i_r2c1);
                indices.push_back(i_r1c2);
            }
            else
            {
                indices.push_back(i_r1c2);
                indices.push_back(i_r2c1);
            }

            indices.push_back(i_r2c2);
            indices.push_back(RESTART_INDEX);
        }
    }
}
    
AFK_3DEdgeShapeBase::AFK_3DEdgeShapeBase(const AFK_ShapeSizes& sSizes):
    bufs(NULL)
{
    /* A base cube has six faces:
     * - bottom (normal -y)     t=0, s=0    noflip
     * - left (normal -x)       t=0, s=1    flip
     * - front (normal -z)      t=0, s=2    flip
     * - back (normal +z)       t=1, s=0    noflip
     * - right (normal +x)      t=1, s=1    noflip
     * - top (normal +y)        t=1, s=2    flip
     * shape_3dedge arranges these in a 3x2 sized jigsaw piece.  I just
     * need to write the vertices (texture co-ordinates) and indices
     * to match.
     */

    /* TODO: I think I want to change this to instead have just one base
     * face (the bottom one), and use geometry shader instancing to
     * make the other 5.
     * That will give me a convenient integer to check overlap with, but
     * I'll need to include jigsaw sizes as uniform variables so that I
     * can compute the correct texture offsets on the fly.
     */
    for (unsigned int t = 0; t < 2; ++t)
    {
        for (unsigned int s = 0; s < 3; ++s)
        {
            pushBaseFace(s, t, 
                (t == 0 && s > 0) || (t == 1 && s == 2), sSizes);
        }
    }
}

AFK_3DEdgeShapeBase::~AFK_3DEdgeShapeBase()
{
    if (bufs)
    {
        glDeleteBuffers(2, &bufs[0]);
        delete[] bufs;
    }
}

void AFK_3DEdgeShapeBase::initGL()
{
    bool needBufferPush = (bufs == NULL);
    if (needBufferPush)
    {
        bufs = new GLuint[2];
        glGenBuffers(2, bufs);
    }

    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
    if (needBufferPush)
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vec2<float>), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[1]);
    if (needBufferPush)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2<float>), 0);

    /* Overlap works based on squares of 2 triangles.  Since I
     * want to be able to cull in pairs, I'm going to use
     * triangle strips with primitive restart as my base
     * geometry type.
     */
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(RESTART_INDEX);
}

void AFK_3DEdgeShapeBase::draw(unsigned int instanceCount) const
{
    glDrawElementsInstanced(GL_LINE_STRIP_ADJACENCY, indices.size(), GL_UNSIGNED_SHORT, 0, instanceCount);
    AFK_GLCHK("3d edge shape draw")
}

void AFK_3DEdgeShapeBase::teardownGL(void) const
{
    glDisableVertexAttribArray(0);
}

