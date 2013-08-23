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

struct AFK_ShrinkformComputeUnit
{
    int cubeOffset;
    int cubeCount;
    int2 piece;
};

/* `makeShrinkform' operates across the 2 dimensions of a
 * shrinkform shape's face.
 * Like `makeTerrain', the third dimension is the unit
 * offset: we take a list of units and fill out the jigsaws
 * with the shrinkform of those units.
 */
__kernel void makeShrinkform(
    __global const struct AFK_ShrinkformPoint *points,
    __global const struct AFK_ShrinkformCube *cubes,
    __global const struct AFK_ShrinkformComputeUnit *units,
    __write_only image2d_t jigsawDisp,
    __write_only image2d_t jigsawColour)
{
    const int xdim = get_global_id(0);
    const int ydim = get_global_id(1);
    const int unitOffset = get_global_id(2);

    // TODO: I'm going to need to specify to this kernel what
    // orientation the face has, and I'm going to need to test
    // the vertex initialisation here to make sure it's all
    // coming out in the same order as the matrix-transformed
    // face geometry.
    // For now, since I'm going to output zeroes to test the
    // pipeline, it doesn't matter.
    // Interestingly, what I think I need to do is compute all
    // the displacements in "face space", which means retaining
    // the "bottom" orientation base tile, and applying the
    // relevant transformation to the *shrinkform points*
    // instead.
    // So each compute unit should come with a transformation
    // matrix?
    // Although: the matrix is not a good use of space, and this
    // requires an inverse transformation for the points compared
    // to the face geometry, anyway.  Perhaps this is a good
    // time to explore an alternative way of describing
    // translations and rotations (quaternions?)
    int2 jigsawCoord = units[unitOffset].piece * TDIM + (int2)(xdim, ydim);
    write_imagef(jigsawDisp, jigsawCoord,
        (float4)(0.0f, 0.0f, 0.0f, 1.0f));
    write_imagef(jigsawColour, jigsawCoord,
        (float4)(0.0f, xdim, ydim, 0.0f));
}


