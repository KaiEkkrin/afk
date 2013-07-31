/* AFK (c) Alex Holloway 2013 */

/* TODO: This is an intermediate step to a fully OpenCL
 * landscape tile pipeline.
 * Don't know if it'll get kept.
 * Transforms a grid of displaced terrain points into
 * vertices and indices for rendering as flat triangles.
 * (Flat triangles are a bit easier because they don't
 * involve barrier-ing the normals?)
 */

/* TODO Make sure the shader is aware that it's now
 * getting a buffer with OpenCL vector packing
 * (these float3's are actually 4-vectors in size)
 */
struct AFK_VcolPhongVertex
{
    float3  location;
    float3  colour;
    float3  normal;
};

void computeFlatTriangle(
    __global const float3 *rawVertices,
    __global const float3 *rawColours,
    unsigned int rawI0,
    unsigned int rawI1,
    unsigned int rawI2,
    __global struct AFK_VcolPhongVertex *vs,
    __global unsigned int *is,
    unsigned int offset)
{
    float3 crossP = cross(
        rawVertices[rawI1] - rawVertices[rawI0],
        rawVertices[rawI2] - rawVertices[rawI0]);

    /* Try to make all triangles face upwards under
     * GL_CCW winding order
     */
    is[offset * 3] = offset;
    if (crossP.y >= 0.0f)
    {
        is[offset * 3 + 1] = offset + 2;
        is[offset * 3 + 2] = offset + 1;
    }
    else
    {
        is[offset * 3 + 1] = offset + 1;
        is[offset * 3 + 2] = offset + 2;
    }

    vs[offset].location     = rawVertices[rawI0];
    vs[offset].colour       = rawColours[rawI0];
    vs[offset].normal       = crossP;

    vs[offset+1].location   = rawVertices[rawI1];
    vs[offset+1].colour     = rawColours[rawI1];
    vs[offset+1].normal     = crossP;

    vs[offset+2].location   = rawVertices[rawI2];
    vs[offset+2].colour     = rawColours[rawI2];
    vs[offset+2].normal     = crossP;
} 

/* This kernel expects to be mapped across the
 * (pointSubdivisionFactor by pointSubdivisionFactor)
 * point subdivision space (2 dimensions).
 * Each of those points is transformed into 2 triangles
 * along with the other vertices that fit into said
 * triangles.
 *
 * TODO: I'm ignoring the y bounds values for now.  I
 * need to learn how to reduce those values in OpenCL
 * in a cunning and quick manner (can't be that hard),
 * but I shouldn't be tackling that along with making
 * the rest of this stuff (which is easier) work in the
 * first place.
 *
 * TODO *2: remember to change `rawVertices' and
 * `rawColours' to OpenCL friendly formats
 * (i.e. actually 4-vectors).
 */
__kernel void vs2FlatTriangles(
    __global const float3 *rawVertices,
    __global const float3 *rawColours,
    __global struct AFK_VcolPhongVertex *vs,
    __global unsigned int *is,              /* 3 ints to a triangle */
    const unsigned int pointSubdivisionFactor)
{
    const unsigned int xdim = get_global_id(0);
    const unsigned int ydim = get_global_id(1);

    if (xdim < pointSubdivisionFactor && ydim < pointSubdivisionFactor)
    {
        /* Identify the vertices that match
         * my triangles.
         */
        unsigned int i_r1c1 = xdim * (pointSubdivisionFactor + 1) + ydim;
        unsigned int i_r1c2 = xdim * (pointSubdivisionFactor + 1) + (ydim + 1);
        unsigned int i_r2c1 = (xdim + 1) * (pointSubdivisionFactor + 1) + ydim;
        unsigned int i_r2c2 = (xdim + 1) * (pointSubdivisionFactor + 1) + (ydim + 1);

        /* Identify the offsets into vs and is that I'm aiming at.
         * Because each index is used exactly once, they are the
         * same for flat triangles.
         */
        unsigned int offset1 = (xdim * pointSubdivisionFactor * 6) + (ydim * 6);
        unsigned int offset2 = (xdim * pointSubdivisionFactor * 6) + (ydim * 6) + 3;

        computeFlatTriangle(rawVertices, rawColours, i_r2c1, i_r1c1, i_r1c2, vs, is, offset1);
        computeFlatTriangle(rawVertices, rawColours, i_r2c1, i_r1c2, i_r2c2, vs, is, offset2);
    }
}

