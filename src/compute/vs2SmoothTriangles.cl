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
    bool emitTriangle)
{
    float3 crossP = cross(
        rawVertices[i1] - rawVertices[i0],
        rawVertices[i2] - rawVertices[i0]);

    vs[i0].colour += rawColours[i0] + rawColours[i1] + rawColours[i2];
    vs[i0].normal += crossP;

    barrier(CLK_GLOBAL_MEM_FENCE);

    vs[i1].colour += rawColours[i0] + rawColours[i1] + rawColours[i2];
    vs[i1].normal += crossP;

    barrier(CLK_GLOBAL_MEM_FENCE);

    vs[i2].colour += rawColours[i0] + rawColours[i1] + rawColours[i2];
    vs[i2].normal += crossP;
    
    /* Try to make all triangles face upwards under
     * GL_CCW winding order
     */
    if (emitTriangle)
    {
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
}

/* TODO: I'm ignoring the y bounds values for now.  I
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
    /* These will be in the range (0 ... pointSubdivisionFactor+1).
     */
    const int xdim = get_global_id(0);
    const int zdim = get_global_id(1);

    /* The input grid is (pointSubdivisionFactor + 2) on a
     * side, containing extra vertices all the way around
     * for the purpose of making smooth triangles and joining
     * the tiles together.
     */
    const int dimSize = pointSubdivisionFactor + 2;

    /* Here are the 4 input vertices that make up
     * those 2 triangles...
     */
    int i_r1c1 = xdim * dimSize + zdim;
    int i_r2c1 = (xdim + 1) * dimSize + zdim;
    int i_r1c2 = xdim * dimSize + (zdim + 1);
    int i_r2c2 = (xdim + 1) * dimSize + (zdim + 1);

    /* This is "my" vertex.  I'll also initialise "my"
     * colour and normal, which will get updated in a
     * short while.
     */
    vs[i_r1c1].location = rawVertices[i_r1c1];
    vs[i_r1c1].colour = (0.0f, 0.0f, 0.0f);
    vs[i_r1c1].normal = (0.0f, 0.0f, 0.0f);

    /* If I'm on the +x or the +z side, I may own an extra
     * vertex (two if I'm on a corner).
     */
    if (xdim == pointSubdivisionFactor)
    {
        vs[i_r2c1].location = rawVertices[i_r2c1];
        vs[i_r2c1].colour = (0.0f, 0.0f, 0.0f);
        vs[i_r2c1].normal = (0.0f, 0.0f, 0.0f);
    }

    if (zdim == pointSubdivisionFactor)
    {
        vs[i_r1c2].location = rawVertices[i_r1c2];
        vs[i_r1c2].colour = (0.0f, 0.0f, 0.0f);
        vs[i_r1c2].normal = (0.0f, 0.0f, 0.0f);
    }

    if (xdim == pointSubdivisionFactor && zdim == pointSubdivisionFactor)
    {
        vs[i_r2c2].location = rawVertices[i_r2c2];
        vs[i_r2c2].colour = (0.0f, 0.0f, 0.0f);
        vs[i_r2c2].normal = (0.0f, 0.0f, 0.0f);
    }

    barrier(CLK_GLOBAL_MEM_FENCE);

    /* There are (pointSubdivisionFactor) * 2 triangles in
     * the output indices array, indexed like this.
     */
    int t1 = ((xdim - 1) * pointSubdivisionFactor + (zdim - 1)) * 6;

    /* TODO Don't emit the bottom and left rows of triangles!
     * Their tile co-ordinates are negative and they'd overlap with
     * the next tile.
     */

    /* Make the first triangle,
     * including emitting the vertex...
     */
    computeSmoothTriangle(
        rawVertices, rawColours,
        i_r1c1, i_r1c2, i_r2c1,
        vs, is, t1, xdim > 0 && zdim > 0);

    barrier(CLK_GLOBAL_MEM_FENCE);

    /* Now, make the second one */
    int t2 = t1 + 3;
    computeSmoothTriangle(
        rawVertices, rawColours,
        i_r1c2, i_r2c2, i_r2c1,
        vs, is, t2, xdim > 0 && zdim > 0);
     
}

