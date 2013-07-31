/* AFK (c) Alex Holloway 2013 */

/* This test program is intended to mangle the landscape tile vertex
 * data a little to ensure that my OpenCL is go.
 */

struct AFK_VcolPhongVertex
{
    float3  location;
    float3  colour;
    float3  normal;
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
        vs[i].location = sourceVs[i].location;
        /* make it obvious I did something */
        vs[i].location.y += 1000.0f;

        vs[i].colour = (1.0f, 1.0f, 1.0f);

        vs[i].normal = sourceVs[i].normal;
    }
}

