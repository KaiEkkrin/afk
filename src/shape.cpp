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


/* Fixed transforms for the six faces of a base cube.
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
        obj[1].displace(afk_vec3<float>(0.0f, 1.0f, 0.0f));
        obj[2].adjustAttitude(AXIS_PITCH, M_PI_2);
        obj[2].displace(afk_vec3<float>(0.0f, 1.0f, 0.0f));
        obj[3].adjustAttitude(AXIS_PITCH, -M_PI_2);
        obj[3].displace(afk_vec3<float>(0.0f, 0.0f, 1.0f));
        obj[4].adjustAttitude(AXIS_ROLL, M_PI_2);
        obj[4].displace(afk_vec3<float>(1.0f, 0.0f, 0.0f));
        obj[5].adjustAttitude(AXIS_PITCH, M_PI);
        obj[5].displace(afk_vec3<float>(0.0f, 1.0f, 1.0f));

        for (int i = 0; i < 6; ++i)
            trans[i] = obj[i].getTransformation();
    }
} faceTransforms;


/* AFK_ShapeFace implementation */

AFK_ShapeFace::AFK_ShapeFace(
    const Vec4<float>& _location,
    const Quaternion<float>& _rotation):
        location(_location), rotation(_rotation),
        jigsawPiece(), jigsawPieceTimestamp()
{
}


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
        boost::hash<unsigned int> uiHash;
        rng.seed(uiHash(shapeKey));

        /* TODO: For now, I'm going to make a whole bunch of larger
         * nested cubes, ending in the target cube.
         * I need to have a good think about what to *really* build
         * and how to correctly wrap a skeleton.
         */
        for (int cubeScale = 32; cubeScale >= 1; cubeScale = cubeScale / 2)
        {
            AFK_ShrinkformCube cube;
            cube.make(
                shrinkformPoints,
                afk_vec4<float>(
                    0.5f - ((float)cubeScale * 0.5f),
                    0.5f - ((float)cubeScale * 0.5f),
                    0.5f - ((float)cubeScale * 0.5f),
                    (float)cubeScale),
                sSizes,
                rng);
            shrinkformCubes.push_back(cube);
        }

        /* Push the six faces of the cube into the face list.
         * These will be inverses, because they're going to
         * be used to re-orient the points, not the face
         * itself
         */
        for (unsigned int face = 0; face < 6; ++face)
        {
            faces.push_back(AFK_ShapeFace(
                afk_vec4<float>(faceTransforms.obj[face].getTranslation(), 1.0f),
                faceTransforms.obj[face].getRotation()));
        }

        haveShrinkformDescriptor = true;
    }
}

void AFK_Shape::buildShrinkformList(
    AFK_ShrinkformList& list)
{
    list.extend(shrinkformPoints, shrinkformCubes);
}

void AFK_Shape::getFacesForCompute(
    unsigned int threadId,
    int minJigsaw,
    AFK_JigsawCollection *_jigsaws,
    std::vector<AFK_ShapeFace>& o_faces)
{
    /* Sanity check */
    if (!jigsaws) jigsaws = _jigsaws;
    else if (jigsaws != _jigsaws) throw AFK_Exception("AFK_Shape: Mismatched jigsaw collections");

    for (std::vector<AFK_ShapeFace>::iterator faceIt = faces.begin(); faceIt != faces.end(); ++faceIt)
    {
        if (faceIt->jigsawPiece == AFK_JigsawPiece() ||
            faceIt->jigsawPieceTimestamp != jigsaws->getPuzzle(faceIt->jigsawPiece)->getTimestamp(faceIt->jigsawPiece.piece))
        {
            /* This face needs computing. */
            faceIt->jigsawPiece = jigsaws->grab(threadId, minJigsaw, faceIt->jigsawPieceTimestamp);
            o_faces.push_back(*faceIt);
        }
    }
}

enum AFK_ShapeArtworkState AFK_Shape::artworkState() const
{
    if (!hasShrinkformDescriptor()) return AFK_SHAPE_NO_PIECE_ASSIGNED;

    /* Scan the faces and check for status. */
    enum AFK_ShapeArtworkState status = AFK_SHAPE_HAS_ARTWORK;
    for (std::vector<AFK_ShapeFace>::const_iterator faceIt = faces.begin(); faceIt != faces.end(); ++faceIt)
    {
        if (faceIt->jigsawPiece == AFK_JigsawPiece()) return AFK_SHAPE_NO_PIECE_ASSIGNED;
        if (faceIt->jigsawPieceTimestamp != jigsaws->getPuzzle(faceIt->jigsawPiece)->getTimestamp(faceIt->jigsawPiece.piece))
            status = AFK_SHAPE_PIECE_SWEPT;
    }
    return status;
}

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
        AFK_JigsawPiece jigsawPiece = faces[face].jigsawPiece;
        q->add(AFK_EntityDisplayUnit(
            objTransform, /* The face's orientation is already fixed by the CL */
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

