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

#include <boost/atomic.hpp>

#include "data/claimable.hpp"
#include "core.hpp"
#include "entity.hpp"
#include "rng/rng.hpp"
#include "shape.hpp"
#include "shape_sizes.hpp"
#include "visible_cell.hpp"
#include "world.hpp"


#define AFK_ENTITY_LIST std::list<AFK_Claimable<AFK_Entity, afk_getComputingFrameFunc> >

/* Describes one cell in the world.  This is the value that we
 * cache in the big ol' WorldCache.
 */
class AFK_WorldCell
{
protected:
    /* Describes the cell's visibility in the world. */
    /* TODO: This needs to change upon a rebase ... */
    AFK_VisibleCell visibleCell;

    /* The list of Entities currently homed to this cell. */
    AFK_ENTITY_LIST *entities;

    /* For generating the shapes for our starting entities. */
    bool checkClaimedShape(unsigned int shapeKey, AFK_Shape& shape, const AFK_ShapeSizes& sSizes);
    void generateShapeArtwork(unsigned int shapeKey, AFK_Shape& shape, unsigned int threadId, const AFK_ShapeSizes& sSizes);

public:
    AFK_WorldCell();
    virtual ~AFK_WorldCell();

    Vec4<float> getRealCoord(void) const;

    /* Binds a world cell to the world.  Needs to be called
     * before anything else gets done, because WorldCells
     * are created uninitialised in the cache.
     */
    void bind(const AFK_Cell& cell, float worldScale);

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

    /* Check this before iterating. */
    bool hasEntities(void) const;

    /* Iterates through this cell's entities. */
    AFK_ENTITY_LIST::iterator entitiesBegin(void);
    AFK_ENTITY_LIST::iterator entitiesEnd(void);

    /* Removes an entity from this cell's list.  Call along with
     * calling moveEntity() on the new cell.
     */
    AFK_ENTITY_LIST::iterator eraseEntity(AFK_ENTITY_LIST::iterator eIt);

    /* Evicts the cell. */
    void evict(void);

    friend std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& worldCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& worldCell);

#endif /* _AFK_WORLD_CELL_H_ */

