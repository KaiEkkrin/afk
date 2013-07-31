/* AFK (c) Alex Holloway 2013 */

/* This test program is intended to mangle the landscape tile vertex
 * data a little to ensure that my OpenCL is go.
 */

/* TODO Decide what to do about the CL vec3 packing business
 * (basically, a CL vec3 is actually a vec4).
 */
struct AFK_VcolPhongVertex
{
    float   locationX;
    float   locationY;
    float   locationZ;

    float   colourX;
    float   colourY;
    float   colourZ;

    float   normalX;
    float   normalY;
    float   normalZ; 
};

__kernel void mangle_vs(
    __global const struct AFK_VcolPhongVertex* sourceVs,
    __global struct AFK_VcolPhongVertex* vs,
    const int vsSize)
{
    /* I'll address the vertex at my global id */
    const int i = get_global_id(0);

    if (i < vsSize)
    {
        vs[i].locationX = sourceVs[i].locationX;
        vs[i].locationY = sourceVs[i].locationY + 1000.0f; /* make it obvious */
        vs[i].locationZ = sourceVs[i].locationZ;

        vs[i].colourX = 1.0f;
        vs[i].colourY = 1.0f;
        vs[i].colourZ = 1.0f;

        vs[i].normalX = sourceVs[i].normalX;
        vs[i].normalY = sourceVs[i].normalY;
        vs[i].normalZ = sourceVs[i].normalZ;
    }
}

