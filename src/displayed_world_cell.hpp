/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DISPLAYED_WORLD_CELL_H_
#define _AFK_DISPLAYED_WORLD_CELL_H_

#include "afk.hpp"

#include <vector>

#include <boost/shared_ptr.hpp>

#include "cell.hpp"
#include "data/claimable.hpp"
#include "display.hpp"
#include "world.hpp"


#define AFK_DWC_VERTEX_BUF AFK_DisplayedBuffer<struct AFK_VcolPhongVertex>
#define AFK_DWC_INDEX_BUF AFK_DisplayedBuffer<Vec3<unsigned int> >

/* Describes the current state of one cell in the world.
 * TODO: Whilst AFK_DisplayedObject is the right abstraction,
 * I might be replicating too many fields here, making these
 * too big and clunky
 */
class AFK_DisplayedWorldCell:
    public AFK_DisplayedObject, public AFK_Claimable
{
private:
    /* These things shouldn't be going on */
    AFK_DisplayedWorldCell(const AFK_DisplayedWorldCell& other) {}
    AFK_DisplayedWorldCell& operator=(const AFK_DisplayedWorldCell& other) { return *this; }

protected:
    /* This cell's vertex data.
     * If this cell only has migrated terrain geometry,
     * it won't have any of this; instead the index list
     * will be referring to the geometry in the cell with
     * y=0.
     * TODO: What I need to do here is:
     * - In the y=0 cell, store a list of the cells that
     * geometry was spilled to.
     * - In non y=0 cells, store the cell that geometry was
     * spilled to me from.
     * - In those cells' canEvict() (see WorldCell), before
     * saying yes, check whether the connected cells can be
     * evicted.
     * - At that point, the eviction thread should take the
     * opportunity to evict the entire chain, to make up for
     * the extra work it just had to do.
     */

    /* The raw geometry, before I've completed computation of
     * the actual terrain.
     * It's kind of sad that I have to allocate this on the heap
     * and store it here, but it looks like I do.
     * This stuff gets deleted when computeGeometry() is complete.
     */
    Vec3<float> *rawVertices;
    Vec3<float> *rawColours;
    unsigned int rawVertexCount;

    /* This is the same between all cells that share a
     * common (x, z, scale).
     */
    boost::shared_ptr<AFK_DWC_VERTEX_BUF> vs;

    /* For a cell y=0, this is the spill cell list.  Otherwise,
     * this contains one member, the cell this cell's terrain
     * was spilled from.
     * For the y=0 cell, the first member is always y=0 anyway.
     * It's a list that I search every time, because there
     * will typically be very few spill cells.
     */
    std::vector<AFK_Cell> spillCells;

    /* In the same order as the spill cells list, this is the y=0
     * cell's holding buffer of individual index buffers destined
     * for the spill cells.
     * When these are moved to the spill cells they start being
     * owned by them and no longer by this (they're moved into the
     * `is' position.)
     * The first member is always this cell's own indices, which
     * don't get moved out.
     */
    std::vector<boost::shared_ptr<AFK_DWC_INDEX_BUF> > spillIs;

    /* For tracking, so I don't have to keep calling lots of
     * STL methods.  Also flags whether there's any geometry to
     * be drawn here at all (spillCellsSize > 0).
     */
    unsigned int spillCellsSize;

    /* This will be a reference to the overall world
     * shader program.
     */
    GLuint program;
    
    /* Internal helper.
     * Computes a single flat triangle, filling out
     * the indicated vertex and index fields.
     */
    void computeFlatTriangle(
        const Vec3<float> *vertices,
        const Vec3<float> *colours,
        const Vec3<unsigned int>& indices,
        unsigned int triangleVOff,
        AFK_DWC_INDEX_BUF& triangleIs);

    /* Internal helper.
     * Finds this vertex in the spill cells list and returns the
     * corresponding buffer (inserting it if required).
     */
    AFK_DWC_INDEX_BUF& findIndexBuffer(
        const Vec3<float>& vertex,
        const AFK_Cell& baseCell,
        float minCellSize,
        float sizeHint);

    /* Internal helper.  Call for y=0 only.
     * Turns a vertex grid into a world of flat triangles,
     * filling out `vs', `is' and the spill structures.
     */
    void vertices2FlatTriangles(
        Vec3<float> *vertices,
        Vec3<float> *colours,
        unsigned int vertexCount,
        const AFK_Cell& baseCell,
        unsigned int pointSubdivisionFactor,
        float minCellSize);

    /* Internal helper.  Call for y=0 only.  Makes this cell's
     * raw terrain data (vertices and colours).
     */
    void makeRawTerrain(const Vec4<float>& baseCoord, unsigned int pointSubdivisionFactor);

public:
    AFK_DisplayedWorldCell();
    virtual ~AFK_DisplayedWorldCell();

    /* Returns true if there is raw terrain here that needs computing,
     * else false.  As a side effect, computes the initial raw
     * terrain grid if required.
     */
    bool hasRawTerrain(
        const Vec4<float>& baseCoord,
        unsigned int pointSubdivisionFactor);

    /* Makes this cell's terrain geometry.  Call right after
     * constructing.
     */
    void computeGeometry(
        unsigned int pointSubdivisionFactor,
        const AFK_Cell& baseCell,
        float minCellSize,
        const AFK_TerrainList& terrain);

    bool hasGeometry() const { return (spillCellsSize > 0); }

    /* The world enumerating worker thread should use these
     * to perform the spillage.  These iterators skip the
     * first element, which belongs to this cell and shouldn't
     * be spilled!
     */
    std::vector<AFK_Cell>::iterator spillCellsBegin();
    std::vector<boost::shared_ptr<AFK_DWC_INDEX_BUF> >::iterator spillIsBegin();
    std::vector<AFK_Cell>::iterator spillCellsEnd();
    std::vector<boost::shared_ptr<AFK_DWC_INDEX_BUF> >::iterator spillIsEnd();

    /* Call this on the spill cell, with the source cell
     * and indices.
     */
    void spill(
        const AFK_DisplayedWorldCell& source,
        const AFK_Cell& cell,
        boost::shared_ptr<AFK_DWC_INDEX_BUF> indices);

    virtual void initGL(void);
    virtual void displaySetup(const Mat4<float>& projection);
    virtual void display(const Mat4<float>& projection);

    virtual AFK_Frame getCurrentFrame(void) const;

    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_DisplayedWorldCell& dlc);
};

std::ostream& operator<<(std::ostream& os, const AFK_DisplayedWorldCell& dlc);

#endif /* _AFK_DISPLAYED_WORLD_CELL_H_ */
