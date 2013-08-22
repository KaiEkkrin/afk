/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_SIZES_H_
#define _AFK_SHAPE_SIZES_H_

/* This utility returns the sizes of various parts of the
 * shrinkform shape constructs.
 * It's rather similar to LandscapeSizes, but not the same...
 */
class AFK_ShapeSizes
{
public:
    const unsigned int subdivisionFactor; /* same as in landscape_sizes and the world ? */
    const unsigned int entitySubdivisionFactor; /* ratio of cell size to the size of an
                                                 * entity that fits into that cell
                                                 */
    const unsigned int pointSubdivisionFactor;
    const unsigned int vDim; /* Number of vertices along an edge in the basic face */
    const unsigned int iDim; /* Number of triangles along an edge */
    const unsigned int tDim; /* Number of vertices along an edge in a jigsaw piece */
    const int          tDimStart; /* What index to start the jigsaw pieces at in relation to the face geometry */
    const unsigned int vCount; /* Total number of vertex structures in the instanced base face */
    const unsigned int iCount; /* Total number of index structures */
    const unsigned int tCount; /* Total number of vertex structures in a jigsaw piece */
    const unsigned int vSize; /* Size of base face vertex array in bytes */
    const unsigned int iSize; /* Size of index array in bytes */
    const unsigned int tSize; /* Size of a jigsaw piece in bytes */
    const unsigned int pointCountPerCube; /* Number of deformation points per shrinkform cube */

    AFK_ShapeSizes(
        unsigned int subdivisionFactor,
        unsigned int entitySubdivisionFactor,
        unsigned int pointSubdivisionFactor);
};

#endif /* _AFK_SHAPE_SIZES_H_ */
