/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DISPLAYED_LANDSCAPE_TILE_H_
#define _AFK_DISPLAYED_LANDSCAPE_TILE_H_

#include "afk.hpp"

#include "cell.hpp"
#include "def.hpp"
#include "jigsaw.hpp"

/* Enables a render of a pre-prepared landscape tile, as fitting into
 * one world cell.
 * This object is not a DisplayedObject!  It doesn't fit.  The landscape
 * is entirely in world space, so there is no movement matrix to
 * track.
 */
class AFK_DisplayedLandscapeTile
{
public:
    /* This tile's location, so that I can displace the
     * instanced base tile onto it.
     */
    const Vec4<float> coord;

    /* The jigsaw piece for this tile. */
    const AFK_JigsawPiece jigsawPiece;

    float cellBoundLower;
    float cellBoundUpper;

    AFK_DisplayedLandscapeTile(
        const Vec4<float>& _coord,
        const AFK_JigsawPiece& _jigsawPiece,
        float _cellBoundLower,
        float _cellBoundUpper);
};

#endif /* _AFK_DISPLAYED_LANDSCAPE_TILE_H_ */

