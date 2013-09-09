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
#include "data/polymer_cache.hpp"
#include "entity_display_queue.hpp"
#include "object.hpp"
#include "jigsaw_collection.hpp"
#include "shape_cell.hpp"
#include "vapour_cell.hpp"
#include "work.hpp"

enum AFK_ShapeArtworkState
{
    AFK_SHAPE_NO_PIECE_ASSIGNED,
    AFK_SHAPE_PIECE_SWEPT,
    AFK_SHAPE_HAS_ARTWORK
};

#ifndef AFK_SHAPE_CELL_CACHE
#define AFK_SHAPE_CELL_CACHE AFK_PolymerCache<AFK_Cell, AFK_ShapeCell, AFK_HashCell>
#endif

#ifndef AFK_VAPOUR_CELL_CACHE
#define AFK_VAPOUR_CELL_CACHE AFK_PolymerCache<AFK_Cell, AFK_VapourCell, AFK_HashCell>
#endif

/* This is a really simple worker that just generates the
 * vapour descriptor and nothing else.
 * Needed to fill out 3D lists for more detailed vapour
 * cells whose less-detailed versions have never been hit
 */
bool afk_generateVapourDescriptor(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    AFK_WorldWorkQueue& queue);

/* Queued into the world work queue, this function generates
 * shape cells.
 */
bool afk_generateShapeCells(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    AFK_WorldWorkQueue& queue);

/* This describes one cube in a shape. */
/* TODO: This wants moving.  Its data should go into the small
 * cached objects, ShapeCell.
 */
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
 * TODO: I need to think about this thing's relationship to the
 * new, LoD'd world of vapour cells and edge cells ...
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

    /* This array lists all the individual cubes in the skeleton
     * in cell co-ordinates.
     * TODO REMOVEME Use detail pitch / visibility tests instead
     */
    std::vector<Vec4<int> > skeletonPointCubes;

    /* Recursively builds a skeleton, filling in the flag grid.
     * The point co-ordinates include a scale co-ordinate; this isn't used to refer to
     * grids, which are xyz only.
     */
    void makeSkeleton(
        AFK_RNG& rng,
        const AFK_ShapeSizes& sSizes,
        Vec3<int> cube,
        unsigned int *cubesLeft,
        std::vector<Vec3<int> >& o_skeletonCubes);

    bool have3DDescriptor;

    /* Builds the vapour for a claimed shape cell.  Sorts out the
     * vapour cell business.
     * Returns true if it completed; else false (in that case you
     * should push in a resume)
     */
    bool enqueueVapourCell(
        unsigned int threadId,
        unsigned int shapeKey,
        AFK_ShapeCell& shapeCell,
        const struct AFK_WorldWorkParam::Shape& param,
        AFK_WorldWorkQueue& queue);

    /* Generates a claimed shape cell at its level of detail. */
    void generateClaimedShapeCell(
        AFK_ShapeCell& shapeCell,
        enum AFK_ClaimStatus& claimStatus,
        unsigned int threadId,
        const struct AFK_WorldWorkParam::Shape& param,
        AFK_WorldWorkQueue& queue);

#if 0
    /* This is a little like the landscape tiles.
     * TODO -- except, they're going away, because they are moving
     * into ShapeCell in a cell-by-cell piecemeal manner to be composed
     * at compute enqueue time a bit like the way the terrain is
     * composed out of landscape tiles.
     * TODO: In addition to this, when I have more than one cube,
     * I'm going to have the whole skeletons business to think about!
     * Parallelling the landscape tiles in the first instance so that
     * I can get the basic thing working.
     */
    std::vector<AFK_3DVapourFeature> vapourFeatures;
    std::vector<AFK_3DVapourCube> vapourCubes;

    std::vector<AFK_ShapeCube> cubes;
    AFK_JigsawCollection *vapourJigsaws;
    AFK_JigsawCollection *edgeJigsaws;

    /* TODO: Cube link-up and LoD.
     * I think that rather than the above, I want to have a polymer
     * backed cache here, caching the vapour piece (and edge pieces)
     * by co-ordinate within the skeleton.
     * Which means I'm going to want to divide the skeleton up into
     * Cells (!) and do a whole lot of things that are similar to
     * what I do with the World, w.r.t. level of detail, etc etc.
     * I also observe that in the world I didn't do adjacency, which
     * is hard, and maybe I should reverse my decision on that here
     * too.  I can use a global space for edge claiming by faces,
     * it's probably no big deal ...
     * I need to go back through the way the world enumerator works
     * and write this down on paper before proceeding.  I'm going
     * to want a detail pitch for the cubes just like I have a detail
     * pitch for world cells (no doubt something like
     * [world]detailPitch / sSizes.skeletonMaxSize ) ...
     */
#else
    AFK_SHAPE_CELL_CACHE *shapeCellCache;
    AFK_VAPOUR_CELL_CACHE *vapourCellCache;
#endif

public:
    AFK_Shape();
    virtual ~AFK_Shape();

    bool has3DDescriptor(void) const;

    /* Makes the 3D descriptor.  Assumes the shape has been claimed
     * exclusively.
     */
    void make3DDescriptor(
        unsigned int threadId,
        unsigned int shapeKey,
        const AFK_ShapeSizes& sSizes);

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend bool afk_generateVapourDescriptor(
        unsigned int threadId,
        const union AFK_WorldWorkParam& param,
        AFK_WorldWorkQueue& queue);

    friend bool afk_generateShapeCells(
        unsigned int threadId,
        const union AFK_WorldWorkParam& param,
        AFK_WorldWorkQueue& queue);

    friend std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape);
};

std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape);


#endif /* _AFK_SHAPE_H_ */

