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
    AFK_SHO_POINT_R         = 3,
    AFK_SHO_POINT_G         = 4,
    AFK_SHO_POINT_B         = 5,
    AFK_SHO_POINT_WEIGHT    = 6,
    AFK_SHO_POINT_RANGE     = 7
    /* The rest are reserved for now. */
};

struct AFK_ShrinkformPoint
{
    unsigned char           s[8];
};

/* ---Like landscape_terrain--- */
__constant float maxFeatureSize = 1.0f / ((float)POINT_SUBDIVISION_FACTOR);
__constant float minFeatureSize = (1.0f / ((float)POINT_SUBDIVISION_FACTOR)) / ((float)SUBDIVISION_FACTOR);

void computeShrinkformPoint(
    float3 *vl,
    float3 *vc,
    __global const struct AFK_ShrinkformPoint *points,
    int i)
{
    float pointRange = (float)points[i].s[AFK_SHO_POINT_RANGE] * (maxFeatureSize - minFeatureSize) / 256.0f + minFeatureSize;
    float minPointLocation = pointRange;
    float maxPointLocation = 1.0f - pointRange;

    /* TODO Try using the builtin vload() for the below ? */
    float3 pointLocation = (float3)(
        (float)points[i].s[AFK_SHO_POINT_X],
        (float)points[i].s[AFK_SHO_POINT_Y],
        (float)points[i].s[AFK_SHO_POINT_Z]) * (maxPointLocation - minPointLocation) / 256.0f + minPointLocation;

    float3 pointColour = (float3)(
        (float)points[i].s[AFK_SHO_POINT_R],
        (float)points[i].s[AFK_SHO_POINT_G],
        (float)points[i].s[AFK_SHO_POINT_B]) / 256.0f;

    float pointWeight = (float)points[i].s[AFK_SHO_POINT_WEIGHT] / 256.0f;

    /* I'm going to make it so that each point is actually a
     * repeating sequence of points separated by `distance'
     * in all dimensions -- let's see if the obvious gridding
     * effect is offset by the improved appearance from
     * fewer points ...
     */
    float dist = fmod(distance(pointLocation, *vl), pointRange * 4.0f);

    if (dist < pointRange)
    {
        /* `vl' is yanked towards the point by an amount of the
         * total distance dependent on `pointWeight'.  The amount of
         * influence each point can have is scaled down; I'm not yet
         * sure how much to scale it by ...
         */
        *vl += normalize(pointLocation - *vl) * dist * pointWeight * pointRange;
        *vc += pointColour / ((dist + 0.1f) * pointWeight);
    }
}

/* TODO Pick one or the other.
 * This function makes averages of the vectors to, and distances
 * to, the target for each point, so that we can then pull to
 * an average point.
 */
void computeAverageShrinkformPoint(
    float3 startLocation,
    __global const struct AFK_ShrinkformPoint *points,
    int i,
    float3 *o_direction,
    float *o_displacement,
    float3 *o_colour)
{
    float pointRange = (float)points[i].s[AFK_SHO_POINT_RANGE] * (maxFeatureSize - minFeatureSize) / 256.0f + minFeatureSize;
    float minPointLocation = pointRange;
    float maxPointLocation = 1.0f - pointRange;

    /* TODO Try using the builtin vload() for the below ? */
    float3 pointLocation = (float3)(
        (float)points[i].s[AFK_SHO_POINT_X],
        (float)points[i].s[AFK_SHO_POINT_Y],
        (float)points[i].s[AFK_SHO_POINT_Z]) * (maxPointLocation - minPointLocation) / 256.0f + minPointLocation;

    float3 pointColour = (float3)(
        (float)points[i].s[AFK_SHO_POINT_R],
        (float)points[i].s[AFK_SHO_POINT_G],
        (float)points[i].s[AFK_SHO_POINT_B]) / 256.0f;

    float pointWeight = (float)points[i].s[AFK_SHO_POINT_WEIGHT] / 256.0f;

    float pointDistance = distance(pointLocation, startLocation);
    if (pointDistance < pointRange)
    {
        *o_direction += normalize(pointLocation - startLocation) * pointWeight;
        *o_displacement += distance(pointLocation, startLocation);
        *o_colour += pointColour * pointWeight;
    }
}

struct AFK_ShrinkformCube
{
    float4                  coord; /* x, y, z, scale */
};

void transformCubeToCube(
    float3 *vl,
    /* float3 *vc, */
    __global const struct AFK_ShrinkformCube *cubes,
    unsigned int cFrom,
    unsigned int cTo)
{
    float4 fromCoord = cubes[cFrom].coord;
    float4 toCoord = cubes[cTo].coord;

    *vl = (*vl * fromCoord.w + fromCoord.xyz - toCoord.xyz) / toCoord.w;
    //*vc = *vc + fromCoord.w / toCoord.w;
}

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
    float3 vc = (float3)(1.0f, 1.0f, 1.0f);
        
    /* The cube list starts with the biggest one and gets smaller.
     * Therefore, I first need to transform into the space of the
     * biggest cube.
     * TODO: I suspect that this loop is a candidate for parallelising
     * using another dimension of the kernel, somehow; however, don't
     * worry about it until it becomes a performance issue...  (shrapnel?)
     */
    transformCubeToCube(&vl, /* &vc, */ cubes,
        units[unitOffset].cubeOffset + units[unitOffset].cubeCount - 1, /* smallest cube */
        units[unitOffset].cubeOffset);

    /* running notes:
     */
    int i;
    for (i = units[unitOffset].cubeOffset; i < (units[unitOffset].cubeOffset + units[unitOffset].cubeCount); ++i)
    {
        if (i > units[unitOffset].cubeOffset)
        {
            transformCubeToCube(&vl, /* &vc, */ cubes, i-1, i);
        }

        float3 direction = (float3)(0.0f, 0.0f, 0.0f);
        float displacement = 0.0f;
        float3 colour = (float3)(0.0f, 0.0f, 0.0f);

        for (int j = i * POINT_COUNT_PER_CUBE; j < ((i + 1) * POINT_COUNT_PER_CUBE); ++j)
        {
            computeAverageShrinkformPoint(vl, points, j, &direction, &displacement, &colour);
        }

        vl += normalize(direction) * displacement / (float)POINT_COUNT_PER_CUBE;
        vc += colour;
    }

    // TODO remove debug
    //transformCubeToCube(&vl, cubes, i, units[unitOffset].cubeOffset + units[unitOffset].cubeCount - 1);

    /* At the end I don't need to transform again, because I'm in the
     * space of the smallest cube (the native cube for this face).
     */

    vc = normalize(vc);

    int2 jigsawCoord = units[unitOffset].piece * TDIM + (int2)(xdim, zdim);
    write_imagef(jigsawDisp, jigsawCoord,
        (float4)(vl, 1.0f));
    write_imagef(jigsawColour, jigsawCoord,
        (float4)(vc, 1.0f));
}


