/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "core.hpp"
#include "displayed_entity.hpp"
#include "entity.hpp"
#include "exception.hpp"


/* AFK_EntityGeometry implementation */

AFK_EntityGeometry::AFK_EntityGeometry(size_t vCount, size_t iCount):
    vs(vCount), is(iCount)
{
}


/* AFK_Entity implementation */

AFK_Entity::AFK_Entity():
    geometry(NULL)
{
}

AFK_Entity::~AFK_Entity()
{
    if (geometry) delete geometry;
}

void AFK_Entity::make(
    unsigned int pointSubdivisionFactor,
    unsigned int subdivisionFactor,
    AFK_RNG& rng)
{
    unsigned int decider = rng.uirand();

    /* TODO: Fill out `geometry' with, you know, actual
     * geometry for the entity.  I'm going to have to do
     * some thinking about this.
     * I think that for real, it won't matter if all objects
     * aren't different: I ought to have a few patterns
     * (a "Shape" class that contains the geometry?) with
     * many duplicates, and use that GPU geometry instancing
     * feature ...?  After all, I want a great big swam of
     * objects, not just a few of them.
     */

    geometry = new AFK_EntityGeometry(6, 8);

    /* Stealing the protagonist chevron as an initial
     * test.
     */
    struct AFK_VcolPhongVertex rawVertices[] = {
        /* location ...                             colour ...                  normal */

        /* bow */
        {{{  0.0f,   0.0f,   2.0f,  }},  /* 0 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  0.0f, 0.0f, 1.0f,  }}},

        /* top and bottom */
        {{{  0.0f,   0.3f,   -0.5f, }},  /* 1 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  0.0f, 1.0f, 0.0f,  }}},
        {{{  0.0f,   -0.3f,  -0.5f, }},  /* 2 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  0.0f, -1.0f, 0.0f, }}},

        /* wingtips */
        {{{  1.0f,   0.0f,   -0.5f, }},  /* 3 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  1.0f, 0.0f, 0.0f,  }}},
        {{{  -1.0f,  0.0f,   -0.5f, }},  /* 4 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  -1.0f, 0.0f, 0.0f, }}},

        /* concave stern */
        {{{  0.0f,   0.0f,   0.0f,  }},  /* 5 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  0.0f, 0.0f, -1.0f, }}},
    };

    unsigned int rawIndices[] = {
        /* bow/top/wingtips */
        0, 1, 3,
        0, 4, 1,

        /* bow/bottom/wingtips */
        0, 3, 2,
        0, 2, 4,

        /* stern */
        4, 5, 1,
        1, 5, 3,
        3, 5, 2,
        2, 5, 4,
    };

    geometry->vs.t.resize(6);
    memcpy(&geometry->vs.t[0], rawVertices, 6 * sizeof(struct AFK_VcolPhongVertex));
    geometry->is.t.resize(8);
    memcpy(&geometry->is.t[0], rawIndices, 8 * 3 * sizeof(unsigned int));

    /* TODO: Having made my basic shape, I need to:
     * Fix the above geometry so that it fits neatly between
     * 0 and 1, while I wait to have proper generated object
     * geometry.
     * - Choose a scale for it, between the cell scale and
     * the cell scale divided by the subdivision factor.
     * - Choose a location for it, anywhere within the
     * cell bounds (which in cell space are between 0 and 1).
     * - obj displace it into the cell.
     * - obj scale it (TODO add method) to the right size for
     * its cell.
     * Or should the entity not know about that?
     */

    /* TODO: Decide what I want to do here, w.r.t.
     * randomly spawned entities.  For now, I'm going to
     * give everything a default movement ...
     */
    velocity = afk_vec3<float>(0.0f, 0.0f, 0.0f);
    angularVelocity = afk_vec3<float>(0.0f, 0.0f, 0.0f);
    angularVelocity.v[decider % 3] = rng.frand() / 1000.0f;

    obj.displace(afk_vec3<float>(0, 0, -16));
}

bool AFK_Entity::animate(
    const boost::posix_time::ptime& now,
    const AFK_Cell& cell,
    float minCellSize,
    AFK_Cell& o_newCell)
{
    bool moved = false;

    if (!lastMoved.is_not_a_date_time())
    {
        boost::posix_time::time_duration sinceLastMoved = now - lastMoved;
        unsigned int slmMicros = sinceLastMoved.total_microseconds();
        float fSlmMillis = (float)slmMicros / 1000.0f;

        obj.drive(velocity * fSlmMillis, angularVelocity * fSlmMillis);
    
        /* Now that I've driven the object, check its real world
         * position to see if it's still within cell bounds.
         * TODO: For complex entities that are split across cells,
         * I'm going to want to do this for all the entity cells;
         * but I don't have that concept right now.
         */
        Vec4<float> newLocationH = obj.getTransformation() *
            afk_vec4<float>(0.0f, 0.0f, 0.0f, 1.0f);
        Vec3<float> newLocation = afk_vec3<float>(
            newLocationH.v[0] * newLocationH.v[3],
            newLocationH.v[1] * newLocationH.v[3],
            newLocationH.v[2] * newLocationH.v[3]);
    
        o_newCell = afk_cellContaining(newLocation, cell.coord.v[3], minCellSize);
        moved = (o_newCell != cell);
    }

    lastMoved = now;
    return moved;
}

AFK_DisplayedEntity *AFK_Entity::makeDisplayedEntity(void)
{
    AFK_DisplayedEntity *de = NULL;

    if (geometry)
    {
        de = new AFK_DisplayedEntity(&geometry, &obj);
    }

    return de;
}

AFK_Frame AFK_Entity::getCurrentFrame(void) const
{
    return afk_core.computingFrame;
}

bool AFK_Entity::canBeEvicted(void) const
{
    /* We shouldn't actually be calling this.  I'm using
     * Claimable for the frame-tracking claim utility,
     * not as an evictable thing.
     */
    throw new AFK_Exception("Wanted to evict an Entity");
}

