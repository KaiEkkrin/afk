/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_H_
#define _AFK_SHAPE_H_

#include "afk.hpp"

#include <vector>

#include "data/fair.hpp"
#include "data/frame.hpp"
#include "entity_display_queue.hpp"
#include "object.hpp"
#include "jigsaw.hpp"
#include "shrinkform.hpp"

enum AFK_ShapeArtworkState
{
    AFK_SHAPE_NO_PIECE_ASSIGNED,
    AFK_SHAPE_PIECE_SWEPT,
    AFK_SHAPE_HAS_ARTWORK
};

/* A Shape describes a single shrinkform shape, which
 * might be instanced many times by means of Entities.
 *
 * TODO: Should a shape be cached, and Claimable, as well?
 * I suspect it should.  But to test just a single Shape,
 * I don't need that.
 */
class AFK_Shape
{
protected:
    /* This is a little like the landscape tiles.
     * TODO: In addition to this, when I have more than one cube,
     * I'm going to have the whole skeletons business to think about!
     * Parallelling the landscape tiles in the first instance so that
     * I can get the basic thing working.
     */
    bool haveShrinkformDescriptor;
    std::vector<AFK_ShrinkformPoint> shrinkformPoints;
    std::vector<AFK_ShrinkformCube> shrinkformCubes;

    AFK_JigsawPiece jigsawPiece;
    AFK_JigsawCollection *jigsaws;
    AFK_Frame jigsawPieceTimestamp;

public:
    AFK_Shape();
    virtual ~AFK_Shape();

    bool hasShrinkformDescriptor() const;

    void makeShrinkformDescriptor(
        const AFK_ShapeSizes& sSizes);

    AFK_JigsawPiece getJigsawPiece(unsigned int threadId, int minJigsaw, AFK_JigsawCollection *_jigsaws);

    enum AFK_ShapeArtworkState artworkState() const;

    /* Enqueues the display units for an entity of this shape. */
    void enqueueDisplayUnits(
        const AFK_Object& object,
        AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair);
};

#endif /* _AFK_SHAPE_H_ */

