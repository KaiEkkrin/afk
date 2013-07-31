/* AFK (c) Alex Holloway 2013 */

/* TODO: This is an intermediate step to a fully OpenCL
 * landscape tile pipeline.
 * Don't know if it'll get kept.
 * Transforms a grid of displaced terrain points into
 * vertices and indices for rendering as smooth triangles.
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

void computeSmoothTriangle(
    __global const float3 *rawVertices,
    __global const float3 *rawColours,
    int i0, int i1, int i2,
    __global struct AFK_VcolPhongVertex *vs,
    __global unsigned int *is,
    int iOffset,
    bool emitVertex)
{
    float3 crossP = cross(
        rawVertices[i1] - rawVertices[i0],
        rawVertices[i2] - rawVertices[i0]);

    if (emitVertex)
    {
        /* TODO Add cleverness, accumulate nearby colours and
         * normals to make smooth triangles.
         * (Harder.)
         */
        vs[i0].location = rawVertices[i0];
        vs[i0].colour = rawColours[i0] + rawColours[i1] + rawColours[i2];
        vs[i0].normal = crossP;
    }
    
    /* Try to make all triangles face upwards under
     * GL_CCW winding order
     */
    is[iOffset] = i0;
    if (crossP.y >= 0.0f)
    {
        is[iOffset + 1] = i2;
        is[iOffset + 2] = i1;
    }
    else
    {
        is[iOffset + 1] = i1;
        is[iOffset + 2] = i2;
    }
}

/* This kernel expects to be mapped across the
 * (pointSubdivisionFactor+1 by pointSubdivisionFactor+1)
 * point subdivision space (2 dimensions).
 * rawVertices and rawColours actually take into account
 * the extra vertices all around the tile
 * (pointSubdivisionFactor+2 by pointSubdivisionFactor+2).
 * Each of the core points is transformed into 2 triangles
 * along with the other vertices that fit into said
 * triangles.
 * The extra points are used for triangles on the
 * positive edges, and for adding into the colour and
 * normals on the negative edges only.
 *
 * TODO: I'm ignoring the y bounds values for now.  I
 * need to learn how to reduce those values in OpenCL
 * in a cunning and quick manner (can't be that hard),
 * but I shouldn't be tackling that along with making
 * the rest of this stuff (which is easier) work in the
 * first place.
 */
__kernel void vs2SmoothTriangles(
    __global const float3 *rawVertices,
    __global const float3 *rawColours,
    __global struct AFK_VcolPhongVertex *vs, /* contains as many vertices as `rawVertices' for simplicity */
    __global unsigned int *is,              /* 3 ints to a triangle */
    const unsigned int pointSubdivisionFactor)
{
    /* These will be in the range (0 ... pointSubdivisionFactor+1). */
    const int xdim = get_global_id(0);
    const int ydim = get_global_id(1);

    /* The input grid is (pointSubdivisionFactor + 2) on a
     * side, containing extra vertices all the way around
     * for the purpose of making smooth triangles.
     */
    const int dimSize = pointSubdivisionFactor + 2;

    /* For now, I'm going to try to output the simplest
     * possible vertices and triangles, with no accumulation
     * of properties.  I'm going to ignore the triangles
     * along the bottom and left gutters.
     */

    /* Here are the 4 input vertices that make up
     * those 2 triangles...
     */
    int i_r1c1 = (xdim + 1) * dimSize + (ydim + 1);
    int i_r1c2 = (xdim + 2) * dimSize + (ydim + 1);
    int i_r2c1 = (xdim + 1) * dimSize + (ydim + 2);
    int i_r2c2 = (xdim + 2) * dimSize + (ydim + 2);

    /* There are (pointSubdivisionFactor + 1) * 6 triangles in
     * the output indices array, indexed like this.
     */
    int t1 = (xdim * (pointSubdivisionFactor + 1) + ydim) * 6;
    int t2 = (xdim * (pointSubdivisionFactor + 1) + ydim) * 6 + 3;

    /* Make the first triangle,
     * including emitting the vertex...
     */
    computeSmoothTriangle(
        rawVertices, rawColours,
        i_r1c1, i_r1c2, i_r2c1,
        vs, is, t1, true);

    /* Now, make the second one */
    computeSmoothTriangle(
        rawVertices, rawColours,
        i_r1c2, i_r2c1, i_r2c2,
        vs, is, t2, false);
}

