/* AFK (c) Alex Holloway 2013 */

/* This test program is intended to mangle the landscape tile vertex
 * data a little to ensure that my OpenCL is go.
 */

typedef struct AFK_VcolPhongVertex
{
    float3   location;
    float3   colour;
    float3   normal;
} AFK_VcolPhongVertex;

__kernel void mangle_vs(__global struct AFK_VcolPhongVertex* vs)
{
    /* I'll address the vertex at my global id */
    const int i = get_global_id(0);

    vs[i].colour = (1.0, 0.0, 0.0);
}

