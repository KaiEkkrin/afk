/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_H_
#define _AFK_SHAPE_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "3d_solid.hpp"
#include "data/claimable.hpp"
#include "data/fair.hpp"
#include "data/frame.hpp"
#include "entity_display_queue.hpp"
#include "object.hpp"
#include "jigsaw_collection.hpp"

enum AFK_ShapeArtworkState
{
    AFK_SHAPE_NO_PIECE_ASSIGNED,
    AFK_SHAPE_PIECE_SWEPT,
    AFK_SHAPE_HAS_ARTWORK
};

/* This describes one cube in a shape. */
class AFK_ShapeCube
{
public:
    Vec4<float> location;

    /* This is where the 3D vapour numbers get computed.
     * TODO: Right now it's long lived.  It's also going to
     * be quite large.  Will I be wanting to make it more
     * transient?  (I *am* going to want the vapour at
     * some point to render directly, of course!)
     */
    AFK_JigsawPiece vapourJigsawPiece;
    AFK_Frame vapourJigsawPieceTimestamp;

    /* This is where the edge data has gone (for drawing the
     * shape as a solid).
     */
    AFK_JigsawPiece edgeJigsawPiece;
    AFK_Frame edgeJigsawPieceTimestamp;

    AFK_ShapeCube(const Vec4<float>& _location);

	friend std::ostream& operator<<(std::ostream& os, const AFK_ShapeCube& cube);
};

std::ostream& operator<<(std::ostream& os, const AFK_ShapeCube& cube);

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_ShapeCube>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_ShapeCube>::value));

enum AFK_SkeletonFlag
{
    AFK_SKF_OUTSIDE_GRID,
    AFK_SKF_CLEAR,
    AFK_SKF_SET
};

/* This object describes, across a grid of theoretical cubes,
 * which cubes the skeleton is present in.
 */
class AFK_SkeletonFlagGrid
{
protected:
    unsigned long long **grid;
    int gridDim;

public:
    AFK_SkeletonFlagGrid(int _gridDim);
    virtual ~AFK_SkeletonFlagGrid();

    /* Basic operations on the grid. */
    enum AFK_SkeletonFlag testFlag(const Vec3<int>& cube) const;
    void setFlag(const Vec3<int>& cube);
    void clearFlag(const Vec3<int>& cube);
};

/* A Shape describes a single 3D shape, which
 * might be instanced many times by means of Entities.
 *
 * TODO: Should a shape be cached, and Claimable, as well?
 * I suspect it should.  But to test just a single Shape,
 * I don't need that.
 */
class AFK_Shape: public AFK_Claimable
{
protected:
    /* This grid of flags describes, for each point, whether the
     * skeleton should include a cube there.
     * It's x by y by (bitfield) z.
     * TODO: With LoDs, I'm going to need several ...
     */
    AFK_SkeletonFlagGrid *cubeGrid;

    /* These grids, which are in order biggest -> smallest,
     * describe whether to create vapour features at the
     * particular cubes in the grids.
     * Each one's resolution is 2x as coarse along each axis
     * as the next one.
     */
    std::vector<AFK_SkeletonFlagGrid *> pointGrids;

    /* Recursively builds a skeleton, filling in the flag grid.
     * The point co-ordinates include a scale co-ordinate; this isn't used to refer to
     * grids, which are xyz only.
     */
    void makeSkeleton(
        AFK_RNG& rng,
        const AFK_ShapeSizes& sSizes,
        Vec3<int> cube,
        unsigned int *cubesLeft,
        std::vector<Vec3<int> >& o_skeletonCubes,
        std::vector<Vec4<int> >& o_skeletonPointCubes);

    /* This is a little like the landscape tiles.
     * TODO: In addition to this, when I have more than one cube,
     * I'm going to have the whole skeletons business to think about!
     * Parallelling the landscape tiles in the first instance so that
     * I can get the basic thing working.
     */
    bool have3DDescriptor;
    std::vector<AFK_3DVapourFeature> vapourFeatures;
    std::vector<AFK_3DVapourCube> vapourCubes;

    std::vector<AFK_ShapeCube> cubes;
    AFK_JigsawCollection *vapourJigsaws;
    AFK_JigsawCollection *edgeJigsaws;

public:
    AFK_Shape();
    virtual ~AFK_Shape();

    bool has3DDescriptor() const;

    void make3DDescriptor(
        unsigned int shapeKey,
        const AFK_ShapeSizes& sSizes);

    void build3DList(
        AFK_3DList& list);

    /* Fills out `o_cubes' with the cubes that
     * need to be computed.
     * TODO This would be faster/eat up less memory if it was
     * in iterator format, right?  (but more mind bending to
     * code :P )
     */
    void getCubesForCompute(
        unsigned int threadId,
        int minVapourJigsaw,
        int minEdgeJigsaw,
        AFK_JigsawCollection *_vapourJigsaws,
        AFK_JigsawCollection *_edgeJigsaws,
        std::vector<AFK_ShapeCube>& o_cubes);

    /* This function always returns AFK_SHAPE_PIECE_SWEPT if
     * at least one piece has been swept.
     * Call getCubesForCompute() and enqueue all the pieces
     * that come back in the array for computation.
     * TODO: This checks the edge jigsaws only.  For rendering
     * the vapour directly we will need a different function.
     */
    enum AFK_ShapeArtworkState artworkState() const;

    /* Enqueues the display units for an entity of this shape.
     * (Right now, this refers to enqueueing the *edge shapes*.
     * A render of a vapour cube will require a different
     * function!)
     */
    void enqueueDisplayUnits(
        const AFK_Object& object,
        AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair) const;

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape);
};

std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape);


#endif /* _AFK_SHAPE_H_ */

