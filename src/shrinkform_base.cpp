/* AFK (c) Alex Holloway 2013 */

#include "shrinkform_base.hpp"

AFK_ShrinkformBaseVertex::AFK_ShrinkformBaseVertex(
    const Vec3<float>& _location,
    const Vec2<float>& _texCoord):
        location(_location), texCoord(_texCoord)
{
}

/* This utility function gets two-dimensional face co-ordinates.
 */
static void make2DFace(
    const AFK_ShapeSizes& sSizes,
    unsigned int s,
    unsigned int t,
    float& o_sCoord,
    float& o_tCoord,
    float& o_sTex,
    float& o_tTex)
{
    o_sCoord = (float)s / (float)sSizes.pointSubdivisionFactor;
    o_tCoord = (float)t / (float)sSizes.pointSubdivisionFactor;

    /* The texture co-ordinates for each jigsaw piece range
     * from (0, 1), with tDim texels along each side, including
     * the padding all the way round.  Therefore, to access the
     * correct texels, I need to skip the padding, like so:
     */
    o_sTex = ((float)s - (float)sSizes.tDimStart) / (float)sSizes.tDim;
    o_tTex = ((float)t - (float)sSizes.tDimStart) / (float)sSizes.tDim;
}
    
AFK_ShrinkformBase::AFK_ShrinkformBase(const AFK_ShapeSizes& sSizes):
    bufs(NULL)
{
    /* A shrinkform base cube has six faces:
     * - bottom (normal -y)
     * - left (normal -x)
     * - front (normal -z)
     * - back (normal +z)
     * - right (normal +x)
     * - top (normal +y)
     * I'm only going to define the bottom face.  The Shape can transform
     * it to make the others.
     */

    for (unsigned int x = 0; x < sSizes.vDim; ++x)
    {
        for (unsigned int z = 0; z < sSizes.vDim; ++z)
        {
            float sCoord, tCoord, sTex, tTex;
            make2DFace(sSizes, x, z, sCoord, tCoord, sTex, tTex);
            vertices.push_back(AFK_ShrinkformBaseVertex(
                afk_vec3<float>(sCoord, 0.0f, tCoord),
                afk_vec2<float>(sTex, tTex)));
        }
    }

    for (unsigned short s = 0; s < sSizes.pointSubdivisionFactor; ++s)
    {
        for (unsigned short t = 0; t < sSizes.pointSubdivisionFactor; ++t)
        {
            unsigned short i_r1c1 = s * sSizes.vDim + t;
            unsigned short i_r2c1 = (s + 1) * sSizes.vDim + t;
            unsigned short i_r1c2 = s * sSizes.vDim + (t + 1);
            unsigned short i_r2c2 = (s + 1) * sSizes.vDim + (t + 1);

            indices.push_back(i_r1c1);
            indices.push_back(i_r2c1);
            indices.push_back(i_r1c2);

            indices.push_back(i_r1c2);
            indices.push_back(i_r2c1);
            indices.push_back(i_r2c2);
        }
    }
}

AFK_ShrinkformBase::~AFK_ShrinkformBase()
{
    if (bufs)
    {
        glDeleteBuffers(2, &bufs[0]);
        delete[] bufs;
    }
}

void AFK_ShrinkformBase::initGL()
{
    bool needBufferPush = (bufs == NULL);
    if (needBufferPush)
    {
        bufs = new GLuint[2];
        glGenBuffers(2, bufs);
    }

    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
    if (needBufferPush)
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * SIZEOF_BASE_VERTEX, &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[1]);
    if (needBufferPush)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, SIZEOF_BASE_VERTEX, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, SIZEOF_BASE_VERTEX, (GLvoid *)sizeof(Vec3<float>));
}

void AFK_ShrinkformBase::teardownGL(void) const
{
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}
