/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "core.hpp"
#include "displayed_landscape_tile.hpp"
#include "exception.hpp"

AFK_DisplayedLandscapeTile::AFK_DisplayedLandscapeTile(
    const Vec4<float>& _coord,
    const AFK_JigsawPiece& _jigsawPiece,
    float _cellBoundLower,
    float _cellBoundUpper):
        coord(_coord),
        jigsawPiece(_jigsawPiece),
        cellBoundLower(_cellBoundLower),
        cellBoundUpper(_cellBoundUpper)
{
}

