/* AFK (c) Alex Holloway 2013 */

/* This program computes the displacements of a shrinkform shape. */

/* Like in terrain.cl, the input information is in bytes that
 * turn into floating point numbers between 0 and 1.
 * Here are the fields...
 */
enum AFK_ShrinkformOffset
{
    AFK_SHO_POINT_X         = 0,
    AFK_SHO_POINT_Y         = 1,
    AFK_SHO_POINT_Z         = 2,
    AFK_SHO_POINT_WEIGHT    = 3,
    AFK_SHO_POINT_RANGE     = 4
    /* The rest are reserved for now. */
};

struct AFK_ShrinkformPoint
{
    unsigned char           s[8];
};

struct AFK_ShrinkformCube
{
    float4                  coord; /* x, y, z, scale */
};

/* TODO Quaternion utilities -- Prime candidates for library functions */
float4 quaternion_hamilton_product(float4 q1, float4 q2)
{
    return (float4)(
        q1.x * q2.x - q1.y * q2.y - q1.z * q2.z - q1.w * q2.w,
        q1.x * q2.y + q1.y * q2.x + q1.z * q2.w - q1.w * q2.z,
        q1.x * q2.z - q1.y * q2.w + q1.z * q2.x + q1.w * q2.y,
        q1.x * q2.w + q1.y * q2.z - q1.z * q2.y + q1.w * q2.x);
}

float4 quaternion_conjugate(float4 q)
{
    return (float4)(q.x, -q.y, -q.z, -q.w);
}

float3 quaternion_rotate(float4 rotation, float3 v)
{
    float4 vq = (float4)(0.0f, v.x, v.y, v.z);
    
    float4 rq = quaternion_hamilton_product(
        quaternion_hamilton_product(rotation, vq),
        quaternion_conjugate(rotation));

    return rq.yzw;
}

struct AFK_ShrinkformComputeUnit
{
    float4 location;
    float4 rotation; /* quaternion. */
    int cubeOffset;
    int cubeCount;
    int2 piece;
};

/* `makeShapeShrinkform' operates across the 2 dimensions of a
 * shrinkform shape's face.
 * Like `makeTerrain', the third dimension is the unit
 * offset: we take a list of units and fill out the jigsaws
 * with the shrinkform of those units.
 */
__kernel void makeShapeShrinkform(
    __global const struct AFK_ShrinkformPoint *points,
    __global const struct AFK_ShrinkformCube *cubes,
    __global const struct AFK_ShrinkformComputeUnit *units,
    __write_only image2d_t jigsawDisp,
    __write_only image2d_t jigsawColour)
{
    const int xdim = get_global_id(0);
    const int zdim = get_global_id(1);
    const int unitOffset = get_global_id(2);

    /* Initialise the face co-ordinate that corresponds to my texels.
     * TODO: A bit excitingly, these are going to need to wrap around the
     * cube in order to produce un-seamed normals ......
     */
    float3 vl = (float3)(
        ((float)(xdim + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR),
        0.0f,
        ((float)(zdim + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR));

    vl = quaternion_rotate(units[unitOffset].rotation, vl);
    vl += units[unitOffset].location.xyz;

    /* Initialise the colour of this co-ordinate */
    float3 vc = (float3)(0.0f, 0.0f, 0.0f);
        

    int2 jigsawCoord = units[unitOffset].piece * TDIM + (int2)(xdim, zdim);
    write_imagef(jigsawDisp, jigsawCoord,
        (float4)(vl, 1.0f));
    write_imagef(jigsawColour, jigsawCoord,
        (float4)((float)xdim / (float)TDIM, 0.0f, (float)zdim / (float)TDIM, 0.0f));
}


