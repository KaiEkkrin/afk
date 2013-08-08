/* AFK (c) Alex Holloway 2013 */

#include "display.hpp"
#include "landscape_display_queue.hpp"

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

void AFK_LandscapeDisplayQueue::add(const AFK_LandscapeDisplayUnit& _unit)
{
    boost::unique_lock<boost::mutex> lock(mut);

    queue.push_back(_unit);
}

void AFK_LandscapeDisplayQueue::copyToGl(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    if (!buf) glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(
        GL_TEXTURE_BUFFER,
        queue.size() * sizeof(AFK_LandscapeDisplayUnit),
        &queue[0],
        GL_STREAM_DRAW);
    glTexBuffer(
        GL_TEXTURE_BUFFER,
        GL_RGBA32F,
        buf);
    AFK_GLCHK("landscape display queue texBuffer")
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
}

