/* AFK
 * Copyright (C) 2013, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#ifndef _AFK_WORLD_CELL_H_
#define _AFK_WORLD_CELL_H_

#include "afk.hpp"

#include <list>

#include <boost/lockfree/queue.hpp>

#include "data/claimable.hpp"
#include "entity.hpp"
#include "rng/rng.hpp"
#include "shape.hpp"
#include "shape_sizes.hpp"
#include "visible_cell.hpp"
#include "world.hpp"


#define AFK_ENTITY_LIST std::list<AFK_Entity*>
#define AFK_ENTITY_MOVE_QUEUE boost::lockfree::queue<AFK_Entity*>

/* Describes one cell in the world.  This is the value that we
 * cache in the big ol' WorldCache.
 */
class AFK_WorldCell: public AFK_Claimable
{
private:
    /* These things shouldn't be going on */
    AFK_WorldCell(const AFK_WorldCell& other) {}
    AFK_WorldCell& operator=(const AFK_WorldCell& other) { return *this; }

protected:
    /* The obvious. */
    AFK_Cell cell;

    /* Describes the cell's visibility in the world. */
    /* TODO: This needs to change upon a rebase ... */
    AFK_VisibleCell visibleCell;

    /* The list of Entities currently homed to this cell. */
    AFK_ENTITY_LIST entities;

    /* The queue of Entities that are supposed to be moved
     * into this cell's list.  I've done it like this so
     * that I don't need to worry about claiming a cell in
     * order to move an entity into it (which would result
     * in deadlock issues, I suspect).
     * TODO: This is currently inactive.  I'm sure I'm going
     * to want something similar after I've sorted out
     * OpenCL entity movement, but it probably won't be
     * exactly this, and I suspect I'll want to keep the
     * entity list entirely in OpenCL buffers and not spread
     * it back out to the CPU.
     */
    AFK_ENTITY_MOVE_QUEUE moveQueue;

    /* For generating the shapes for our starting entities. */
    bool checkClaimedShape(unsigned int shapeKey, AFK_Shape& shape, const AFK_ShapeSizes& sSizes);
    void generateShapeArtwork(unsigned int shapeKey, AFK_Shape& shape, unsigned int threadId, const AFK_ShapeSizes& sSizes);

public:
    AFK_WorldCell();
    virtual ~AFK_WorldCell();

    const AFK_Cell& getCell(void) const { return cell; }
    Vec4<float> getRealCoord(void) const;

    /* Binds a world cell to the world.  Needs to be called
     * before anything else gets done, because WorldCells
     * are created uninitialised in the cache.
     */
    void bind(const AFK_Cell& _cell, float worldScale);

    /* Tests whether this cell is within the specified detail pitch
     * when viewed from the specified location.
     */
    bool testDetailPitch(
        float detailPitch,
        const AFK_Camera& camera,
        const Vec3<float>& viewerLocation) const;

    /* Tests whether none, some or all of this cell's vertices are
     * visible when projected with the supplied camera.
     */
    void testVisibility(
        const AFK_Camera& camera,
        bool& io_someVisible,
        bool& io_allVisible) const;

    /* For giving this cell a starting entity set. */
    unsigned int getStartingEntitiesWanted(
        AFK_RNG& rng,
        unsigned int maxEntitiesPerCell,
        unsigned int entitySparseness) const;

    unsigned int getStartingEntityShapeKey(AFK_RNG& rng);

    void addStartingEntity(
        unsigned int shapeKey,
        const AFK_ShapeSizes& sSizes,
        AFK_RNG& rng);

    /* Iterates through this cell's entities. */
    AFK_ENTITY_LIST::iterator entitiesBegin(void);
    AFK_ENTITY_LIST::iterator entitiesEnd(void);

    /* Removes an entity from this cell's list.  Call along with
     * calling moveEntity() on the new cell.
     */
    AFK_ENTITY_LIST::iterator eraseEntity(AFK_ENTITY_LIST::iterator eIt);

    /* Pushes an entity into this cell's move queue.  It's okay
     * to call this without having the claim.
     */
    void moveEntity(AFK_Entity *entity);

    /* Pops the contents of the move queue into this cell's list.
     */
    void popMoveQueue(void);

    /* AFK_Claimable implementation. */

    /* Says whether this cell can be evicted from the cache.
     */
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& worldCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& worldCell);

#endif /* _AFK_WORLD_CELL_H_ */

