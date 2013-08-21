/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include <boost/shared_ptr.hpp>

#include "def.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw.hpp"

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
        obj[1].adjustAttitude(AXIS_ROLL, M_PI_2);
        obj[2].adjustAttitude(AXIS_PITCH, M_PI_2);
        obj[3].adjustAttitude(AXIS_PITCH, -M_PI_2);
        obj[3].displace(afk_vec3<float>(0.0f, 1.0f, 1.0f));
        obj[4].adjustAttitude(AXIS_ROLL, -M_PI_2);
        obj[4].displace(afk_vec3<float>(1.0f, 1.0f, 0.0f));
        obj[5].adjustAttitude(AXIS_YAW, M_PI);
        obj[5].displace(afk_vec3<float>(1.0f, 1.0f, 1.0f));

        for (int i = 0; i < 6; ++i)
            trans[i] = obj[i].getTransformation();
    }
} faceTransforms;

 AFK_Object frontFaceObj = 

void enqueueDisplayUnits(
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
        Mat4<float> totalTransform = faceTransforms[face] * objTransform;
        q.add(AFK_EntityDisplayUnit(
            totalTransform,
            AFK_JigsawPiece()));
    }
}

#include "shape.hpp"

