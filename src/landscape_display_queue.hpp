/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LANDSCAPE_DISPLAY_QUEUE_H_
#define _AFK_LANDSCAPE_DISPLAY_QUEUE_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "def.hpp"

class AFK_LandscapeTile;

/* This module is like terrain_compute_queue, but for queueing up
 * the cell-specific landscape details -- cell coord, jigsaw STs,
 * y bounds -- in a texture buffer that can be shoveled into the
 * GL in one go.
 * The world will have one of these queues per jigsaw organised in
 * a fair, just like terrain_compute_queue.
 */

class AFK_LandscapeDisplayUnit
{
public:
    Vec4<float>     cellCoord;
    Vec2<float>     jigsawPieceST; /* between 0 and 1 in jigsaw space */
    float           yBoundLower; /* TODO do I actually want this and the next ? */
    float           yBoundUpper;

    AFK_LandscapeDisplayUnit();
    AFK_LandscapeDisplayUnit(
        const Vec4<float>& _cellCoord,
        const Vec2<float>& _jigsawPieceST,
        float _yBoundLower,
        float _yBoundUpper);

    friend std::ostream& operator<<(std::ostream& os, const AFK_LandscapeDisplayUnit& unit);
};

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeDisplayUnit& unit);

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_LandscapeDisplayUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_LandscapeDisplayUnit>::value));

class AFK_LandscapeDisplayQueue
{
protected:
    std::vector<AFK_LandscapeDisplayUnit> queue;
    std::vector<const AFK_LandscapeTile*> landscapeTiles; /* for last-moment y bounds fetch */
    GLuint buf;
    boost::mutex mut;

    /* After culling cells that are entirely outside y-bounds, the shortened
     * queue goes here
     */
    std::vector<AFK_LandscapeDisplayUnit> culledQueue;

public:
    AFK_LandscapeDisplayQueue();
    virtual ~AFK_LandscapeDisplayQueue();

    /* The cell enumerator workers should call this to add a unit
     * for rendering.
     */
    void add(const AFK_LandscapeDisplayUnit& _unit, const AFK_LandscapeTile *landscapeTile);

    /* The world display function should call this, having made the
     * appropriate texture active, to get the queue bound as a
     * texture buffer.
     * Returns the number of units actually copied into the buffer.
     */
    unsigned int copyToGl(void);

    unsigned int getUnitCount(void);
    AFK_LandscapeDisplayUnit getUnit(unsigned int u);
    bool empty(void);

    void clear(void);
};

#endif /* _AFK_LANDSCAPE_DISPLAY_QUEUE_H_ */

