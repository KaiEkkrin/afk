/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_VAPOUR_CELL_H_
#define _AFK_VAPOUR_CELL_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include "3d_solid.hpp"
#include "cell.hpp"
#include "data/claimable.hpp"
#include "data/frame.hpp"
#include "data/polymer_cache.hpp"
#include "shape_sizes.hpp"

#ifndef AFK_VAPOUR_CELL_CACHE
#define AFK_VAPOUR_CELL_CACHE AFK_PolymerCache<AFK_Cell, AFK_VapourCell, AFK_HashCell>
#endif

/* To access a vapour cell cache, use this to transform shape
 * cell coords to vapour cell.
 */
AFK_Cell afk_vapourCell(const AFK_Cell& cell);

/* A VapourCell describes a cell in a shape with its vapour
 * features and cubes.  I'm making it distinct from a ShapeCell
 * because VapourCells exist at larger scales, crossing multiple
 * ShapeCells.
 */
class AFK_VapourCell: public AFK_Claimable
{
protected:
    AFK_Cell cell;

    bool haveDescriptor;
    std::vector<AFK_3DVapourFeature> features;
    std::vector<AFK_3DVapourCube> cubes;

public:
    /* Binds a shape cell to the shape. */
    void bind(const AFK_Cell& _cell);

    bool hasDescriptor(void) const;
    void makeDescriptor(
        unsigned int shapeKey,
        const AFK_ShapeSizes& sSizes);

    /* Builds the 3D list for this cell.
     * TODO: Here, for each pass, I should come up with a
     * way of caching the place in the vapour compute queue
     * where this list has gone.  Lists are shared between
     * many shape cells, so it's an important improvement.
     */
    void build3DList(
        unsigned int threadId,
        AFK_3DList& list,
        unsigned int subdivisionFactor,
        const AFK_VAPOUR_CELL_CACHE *cache);

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_VapourCell& vapourCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_VapourCell& vapourCell);

#endif /* _AFK_VAPOUR_CELL_H_ */

