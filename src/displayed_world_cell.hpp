/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DISPLAYED_WORLD_CELL_H_
#define _AFK_DISPLAYED_WORLD_CELL_H_

#include "afk.hpp"

#include "display.hpp"
#include "world.hpp"


class AFK_WorldCache;
class AFK_WorldCell;


/* Describes the current state of one cell in the world.
 * TODO: Whilst AFK_DisplayedObject is the right abstraction,
 * I might be replicating too many fields here, making these
 * too big and clunky
 */
class AFK_DisplayedWorldCell: public AFK_DisplayedObject
{
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

    /* Deleted when the y=0 cell is deleted, not when any of
     * the others are.  By the eviction rules above, this
     * should be OK.
     */
    AFK_DisplayedBuffer<struct AFK_VcolPhongVertex> *vs;

    /* This cell's index data.  Each cell has its own one
     * of these.
     */
    AFK_DisplayedBuffer<Vec3<unsigned int> > is;

    /* This will be a reference to the overall world
     * shader program.
     */
    GLuint program;

public:
    AFK_DisplayedWorldCell(
        const AFK_WorldCell *worldCell,
        unsigned int pointSubdivisionFactor,
        AFK_WorldCache *cache);
    virtual ~AFK_DisplayedWorldCell();

    virtual void initGL(void);
    virtual void displaySetup(const Mat4<float>& projection);
    virtual void display(const Mat4<float>& projection);
    virtual void deleteVs(void);

    friend std::ostream& operator<<(std::ostream& os, const AFK_DisplayedWorldCell& dlc);
};

std::ostream& operator<<(std::ostream& os, const AFK_DisplayedWorldCell& dlc);

#endif /* _AFK_DISPLAYED_WORLD_CELL_H_ */
