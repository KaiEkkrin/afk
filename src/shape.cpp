/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>

#include "core.hpp"
#include "def.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw.hpp"
#include "rng/boost_taus88.hpp"
#include "shape.hpp"


/* AFK_Shape implementation */

AFK_Shape::AFK_Shape():
    AFK_Claimable(),
    haveShrinkformDescriptor(false),
    jigsaws(NULL)
{
}

AFK_Shape::~AFK_Shape()
{
}

bool AFK_Shape::hasShrinkformDescriptor() const
{
    return haveShrinkformDescriptor;
}

void AFK_Shape::makeShrinkformDescriptor(
    unsigned int shapeKey,
    const AFK_ShapeSizes& sSizes)
{
    if (!haveShrinkformDescriptor)
    {
        /* TODO non-remaking of RNGs? */
        AFK_Boost_Taus88_RNG rng;

        /* TODO Making a single cube for now.  In future, I need
         * a whole skeleton!
         */
        boost::hash<unsigned int> uiHash;
        rng.seed(uiHash(shapeKey));
        Vec4<float> coord = afk_vec4<float>(0.0f, 0.0f, 0.0f, 1.0f);
        AFK_ShrinkformCube cube;
        cube.make(
            shrinkformPoints,
            coord,
            sSizes,
            rng);
        shrinkformCubes.push_back(cube);

        haveShrinkformDescriptor = true;
    }
}

void AFK_Shape::buildShrinkformList(
    AFK_ShrinkformList& list)
{
    list.extend(shrinkformPoints, shrinkformCubes);
}

AFK_JigsawPiece AFK_Shape::getJigsawPiece(unsigned int threadId, int minJigsaw, AFK_JigsawCollection *_jigsaws)
{
    if (artworkState() == AFK_SHAPE_HAS_ARTWORK) throw AFK_Exception("Tried to overwrite a shape's artwork");
    jigsaws = _jigsaws;
    jigsawPiece = jigsaws->grab(threadId, minJigsaw, jigsawPieceTimestamp);
    return jigsawPiece;
}

enum AFK_ShapeArtworkState AFK_Shape::artworkState() const
{
    if (!hasShrinkformDescriptor() || jigsawPiece == AFK_JigsawPiece()) return AFK_SHAPE_NO_PIECE_ASSIGNED;
    AFK_Frame rowTimestamp = jigsaws->getPuzzle(jigsawPiece)->getTimestamp(jigsawPiece.piece);
    return (rowTimestamp == jigsawPieceTimestamp) ? AFK_SHAPE_HAS_ARTWORK : AFK_SHAPE_PIECE_SWEPT;
}

/* Fixed transforms for the 5 faces other than the bottom face
 * (base.)
 * TODO: I'm probably going to get some of these wrong and will
 * need to tweak them
 */
static struct FaceTransforms
{
    /* These are, in order:
     * - bottom
     * - left
     * - front
     * - back
     * - right
     * - top
     */
    AFK_Object obj[6];
    Mat4<float> trans[6];

    FaceTransforms()
    {
        obj[1].adjustAttitude(AXIS_ROLL, -M_PI_2);
        obj[1].displace(afk_vec3<float>(-1.0f, 0.0f, 0.0f));
        obj[2].adjustAttitude(AXIS_PITCH, M_PI_2);
        obj[2].displace(afk_vec3<float>(0.0f, 0.0f, -1.0f));
        obj[3].adjustAttitude(AXIS_PITCH, -M_PI_2);
        obj[3].displace(afk_vec3<float>(0.0f, 1.0f, 0.0f));
        obj[4].adjustAttitude(AXIS_ROLL, M_PI_2);
        obj[4].displace(afk_vec3<float>(0.0f, 1.0f, 0.0f));
        obj[5].adjustAttitude(AXIS_PITCH, M_PI);
        obj[5].displace(afk_vec3<float>(0.0f, 1.0f, -1.0f));

        for (int i = 0; i < 6; ++i)
            trans[i] = obj[i].getTransformation();
    }
} faceTransforms;

void AFK_Shape::enqueueDisplayUnits(
    const AFK_Object& object,
    AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair)
{
    /* TODO: A couple of things to note:
     * - 1: When I actually have a shrinkform (as opposed to a
     * test cube), each face will be associated with a jigsaw
     * piece.  I need to enqueue the faces into the queues
     * that match those pieces' puzzles.
     * - 2: When I have composite shapes, I'm going to have more
     * than six faces here.
     */
    boost::shared_ptr<AFK_EntityDisplayQueue> q = entityDisplayFair.getUpdateQueue(0);
    Mat4<float> objTransform = object.getTransformation();

    for (int face = 0; face < 6; ++face)
    {
        Mat4<float> totalTransform = objTransform * faceTransforms.trans[face];
        q->add(AFK_EntityDisplayUnit(
            totalTransform,
            jigsaws->getPuzzle(jigsawPiece)->getTexCoordST(jigsawPiece)));
    }
}

AFK_Frame AFK_Shape::getCurrentFrame(void) const
{
    return afk_core.computingFrame;
}

bool AFK_Shape::canBeEvicted(void) const
{
    /* This is a tweakable value ... */
    bool canEvict = ((afk_core.computingFrame - lastSeen) > 10);
    return canEvict;
}

std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape)
{
    os << "Shape";
    if (shape.haveShrinkformDescriptor) os << " (Shrinkform)";
    if (shape.artworkState() == AFK_SHAPE_HAS_ARTWORK) os << " (Computed)";
    return os;
}

