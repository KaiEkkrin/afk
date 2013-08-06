/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LANDSCAPE_SIZES_H_
#define _AFK_LANDSCAPE_SIZES_H_

/* This utility returns the sizes of the various landscape
 * elements, so that I can configure the cache correctly, and correctly
 * drive the drawing functions.
 */
class AFK_LandscapeSizes
{
public:
    const unsigned int pointSubdivisionFactor;
    const unsigned int vDim; /* Number of vertices along an edge in the base tile */
    const unsigned int iDim; /* Number of triangles along an edge */
    const unsigned int tDim; /* Number of vertices along an edge in a jigsaw piece */
    const int          tDimStart; /* What index to start the jigsaw pieces at in relation to the tile */
    const unsigned int vCount; /* Total number of vertex structures in the instanced base tile */
    const unsigned int iCount; /* Total number of index structures */
    const unsigned int tCount; /* Total number of vertex structures in a jigsaw piece */
    const unsigned int vSize; /* Size of base tile vertex array in bytes */
    const unsigned int iSize; /* Size of index array in bytes */
    const unsigned int tSize; /* Size of a jigsaw piece in bytes */
    const unsigned int featureCountPerTile; /* Number of terrain features per tile */
    const unsigned int reduceOrder; /* 1<<reduceOrder is the power of two above tDim*tDim */

    AFK_LandscapeSizes(unsigned int pointSubdivisionFactor);
};

#endif /* _AFK_LANDSCAPE_SIZES_H_ */

