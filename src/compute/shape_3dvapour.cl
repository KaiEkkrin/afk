/* AFK (c) Alex Holloway 2013 */

/* This is the common part of the vapour stuff, declaring the
 * input units, etc.
 */

struct AFK_3DVapourComputeUnit
{
    float4 location;
    float4 baseColour;
    int4 vapourPiece;
    int adjacency;
    int cubeOffset;
    int cubeCount;
};

