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
public:
    /* This cell's triangle data. */
    struct AFK_VcolPhongVertex *triangleVs;
    unsigned int triangleVsCount;

    /* This will be a reference to the overall world
     * shader program.
     */
    GLuint program;

    /* The vertex set. */
    GLuint vertices;
    unsigned int vertexCount;

    AFK_DisplayedWorldCell(
        const AFK_WorldCell *worldCell,
        unsigned int pointSubdivisionFactor,
        AFK_WorldCache *cache);
    virtual ~AFK_DisplayedWorldCell();

    virtual void initGL(void);
    virtual void displaySetup(const Mat4<float>& projection);
    virtual void display(const Mat4<float>& projection);
};

std::ostream& operator<<(std::ostream& os, const AFK_DisplayedWorldCell& dlc);

#endif /* _AFK_DISPLAYED_WORLD_CELL_H_ */
