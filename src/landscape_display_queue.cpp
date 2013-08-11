/* AFK (c) Alex Holloway 2013 */

#include "display.hpp"
#include "landscape_display_queue.hpp"
#include "landscape_tile.hpp"

AFK_LandscapeDisplayUnit::AFK_LandscapeDisplayUnit() {}

AFK_LandscapeDisplayUnit::AFK_LandscapeDisplayUnit(
    const Vec4<float>& _cellCoord,
    const Vec2<float>& _jigsawPieceST,
    float _yBoundLower,
    float _yBoundUpper):
        cellCoord(_cellCoord), jigsawPieceST(_jigsawPieceST), yBoundLower(_yBoundLower), yBoundUpper(_yBoundUpper)
{
}

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeDisplayUnit& unit)
{
    os << "(LDU: ";
    os << "cellCoord=" << std::dec << unit.cellCoord;
    os << ", jigsawPieceST=" << unit.jigsawPieceST;
    os << ", yBoundLower=" << unit.yBoundLower;
    os << ", yBoundUpper=" << unit.yBoundUpper;
    os << ")";
    return os;
}


AFK_LandscapeDisplayQueue::AFK_LandscapeDisplayQueue():
    buf(0)
{
}

AFK_LandscapeDisplayQueue::~AFK_LandscapeDisplayQueue()
{
    if (buf) glDeleteBuffers(1, &buf);
}

void AFK_LandscapeDisplayQueue::add(const AFK_LandscapeDisplayUnit& _unit, const AFK_LandscapeTile *landscapeTile)
{
    boost::unique_lock<boost::mutex> lock(mut);

    queue.push_back(_unit);
    landscapeTiles.push_back(landscapeTile);
}

unsigned int AFK_LandscapeDisplayQueue::copyToGl(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    /* I won't upload the full queue, but only a culled queue with any
     * landscape tiles that suddenly became out of y-bounds removed.
     */
    for (unsigned int i = 0; i < queue.size(); ++i)
    {
        queue[i].yBoundLower = landscapeTiles[i]->getYBoundLower();
        queue[i].yBoundUpper = landscapeTiles[i]->getYBoundUpper();

        //if (landscapeTiles[i]->realCellWithinYBounds(queue[i].cellCoord))
            culledQueue.push_back(queue[i]);
    }

    if (!buf) glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(
        GL_TEXTURE_BUFFER,
        culledQueue.size() * sizeof(AFK_LandscapeDisplayUnit),
        &culledQueue[0],
        GL_STREAM_DRAW);
    glTexBuffer(
        GL_TEXTURE_BUFFER,
        GL_RGBA32F,
        buf);
    AFK_GLCHK("landscape display queue texBuffer")

    return culledQueue.size();
}

unsigned int AFK_LandscapeDisplayQueue::getUnitCount(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return queue.size();
}

AFK_LandscapeDisplayUnit AFK_LandscapeDisplayQueue::getUnit(unsigned int u)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return queue[u];
}

bool AFK_LandscapeDisplayQueue::empty(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return queue.empty();
}

void AFK_LandscapeDisplayQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    queue.clear();
    landscapeTiles.clear();
    culledQueue.clear();
}

