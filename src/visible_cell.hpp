/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_VISIBLE_CELL_H_
#define _AFK_VISIBLE_CELL_H_

#include "afk.hpp"

#include <sstream>

#include "camera.hpp"
#include "cell.hpp"
#include "def.hpp"

/* A VisibleCell is any cell whose apparent size to the viewer needs to
 * be detected.  It encapsulates a transformation of a Cell into world
 * co-ordinates and has some functions for operating on them.
 */
class AFK_VisibleCell
{
protected:
    /* This array contains all 8 vertices of the cell. */
    Vec3<float> vertices[2][2][2];

    /* This is the midpoint. */
    Vec3<float> midpoint;

    void calculateMidpoint(void);

public:
    /* Call these to set up. */
    void bindToCell(const AFK_Cell& cell, float worldScale);
    void bindToCell(const AFK_Cell& cell, const Mat4<float>& worldTransform);

    /* Returns (x, y, z, scale), just like a Cell. */
    Vec4<float> getRealCoord() const;

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

    friend std::ostream& operator<<(std::ostream& os, const AFK_VisibleCell& visibleCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_VisibleCell& visibleCell);

#endif /* _AFK_VISIBLE_CELL_H_ */

