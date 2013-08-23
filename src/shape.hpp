/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_H_
#define _AFK_SHAPE_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "data/claimable.hpp"
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

/* This describes one face of a shape. */
class AFK_ShapeFace
{
public:
    Vec4<float> location;
    Quaternion<float> rotation;
    AFK_JigsawPiece jigsawPiece;
    AFK_Frame jigsawPieceTimestamp;

    AFK_ShapeFace(
        const Vec4<float>& _location,
        const Quaternion<float>& _rotation);
};

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_ShapeFace>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_ShapeFace>::value));

/* A Shape describes a single shrinkform shape, which
 * might be instanced many times by means of Entities.
 *
 * TODO: Should a shape be cached, and Claimable, as well?
 * I suspect it should.  But to test just a single Shape,
 * I don't need that.
 */
class AFK_Shape: public AFK_Claimable
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

    std::vector<AFK_ShapeFace> faces;
    AFK_JigsawCollection *jigsaws;

public:
    AFK_Shape();
    virtual ~AFK_Shape();

    bool hasShrinkformDescriptor() const;

    void makeShrinkformDescriptor(
        unsigned int shapeKey,
        const AFK_ShapeSizes& sSizes);

    void buildShrinkformList(
        AFK_ShrinkformList& list);

    /* Fills out `o_faces' with the faces that
     * need to be computed.
     * TODO This would be faster/eat up less memory if it was
     * in iterator format, right?  (but more mind bending to
     * code :P )
     */
    void getFacesForCompute(
        unsigned int threadId,
        int minJigsaw,
        AFK_JigsawCollection *_jigsaws,
        std::vector<AFK_ShapeFace>& o_faces);

    /* This function always returns AFK_SHAPE_PIECE_SWEPT if
     * at least one piece has been swept.
     * Call getFaces() and enqueue all the pieces
     * that come back in the array for computation.
     */
    enum AFK_ShapeArtworkState artworkState() const;

    /* Enqueues the display units for an entity of this shape. */
    void enqueueDisplayUnits(
        const AFK_Object& object,
        AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair);

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape);
};

std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape);


#endif /* _AFK_SHAPE_H_ */

