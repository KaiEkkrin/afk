/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "visible_cell.hpp"


/* AFK_VisibleCell implementation. */

void AFK_VisibleCell::calculateMidpoint(void)
{
    midpoint = vertices[0][0][0] +
        (vertices[1][0][0] - vertices[0][0][0]) / 2.0f +
        (vertices[0][1][0] - vertices[0][0][0]) / 2.0f +
        (vertices[0][0][1] - vertices[0][0][0]) / 2.0f;
}

void AFK_VisibleCell::bindToCell(const AFK_Cell& cell, float worldScale)
{
    /* Work out the vertices by computing cell adjacency. */
    for (long long x = 0; x <= 1; ++x)
    {
        for (long long y = 0; y <= 1; ++y)
        {
            for (long long z = 0; z <= 1; ++z)
            {
                AFK_Cell adjCell = afk_cell(afk_vec4<long long>(
                    cell.coord.v[0] + x * cell.coord.v[3],
                    cell.coord.v[1] + y * cell.coord.v[3],
                    cell.coord.v[2] + z * cell.coord.v[3],
                    cell.coord.v[3]));

                Vec4<float> realCoord = adjCell.toWorldSpace(worldScale);
                vertices[x][y][z] = afk_vec3<float>(
                    realCoord.v[0], realCoord.v[1], realCoord.v[2]);
            }
        }
    }

    calculateMidpoint();
}

void AFK_VisibleCell::bindToCell(const AFK_KeyedCell& cell, float worldScale, const Mat4<float>& worldTransform)
{
    /* Work out the vertices by computing cell adjacency. */
    for (long long x = 0; x <= 1; ++x)
    {
        for (long long y = 0; y <= 1; ++y)
        {
            for (long long z = 0; z <= 1; ++z)
            {
                AFK_Cell adjCell = afk_cell(afk_vec4<long long>(
                    cell.c.coord.v[0] + x * cell.c.coord.v[3],
                    cell.c.coord.v[1] + y * cell.c.coord.v[3],
                    cell.c.coord.v[2] + z * cell.c.coord.v[3],
                    cell.c.coord.v[3]));

                Vec4<float> realCoord = adjCell.toWorldSpace(worldScale);
                realCoord.v[3] = 1.0f;
                Vec4<float> transCoord = worldTransform * realCoord;
                vertices[x][y][z] = afk_vec3<float>(
                    transCoord.v[0] / transCoord.v[3],
                    transCoord.v[1] / transCoord.v[3],
                    transCoord.v[2] / transCoord.v[3]);
            }
        }
    }

    calculateMidpoint();
}

Vec4<float> AFK_VisibleCell::getRealCoord(void) const
{
    return afk_vec4<float>(
        vertices[0][0][0].v[0],
        vertices[0][0][0].v[1],
        vertices[0][0][0].v[2],
        (vertices[1][0][0] - vertices[0][0][0]).magnitude());
}

bool AFK_VisibleCell::testDetailPitch(
    float detailPitch,
    const AFK_Camera& camera,
    const Vec3<float>& viewerLocation) const
{
    /* The cell detail pitch shall be the average of the
     * detail pitches as seen of the 8 vertices.
     */
    float scale = (vertices[1][0][0] - vertices[0][0][0]).magnitude();
    float accDP = 0.0f;

    for (int x = 0; x <= 1; ++x)
    {
        for (int y = 0; y <= 1; ++y)
        {
            for (int z = 0; z <= 1; ++z)
            {
                float distanceToViewer = (vertices[x][y][z] - (viewerLocation - camera.separation)).magnitude();
                accDP += camera.getDetailPitchAsSeen(scale, distanceToViewer);
            }
        }
    }

    return (accDP / 8.0f) < detailPitch;
}

static void testPointVisible(
    const Vec3<float>& point,
    const AFK_Camera& camera,
    bool& io_someVisible,
    bool& io_allVisible)
{
    /* Project the point.  This perspective projection
     * yields a viewport with the x co-ordinates
     * (-ar, ar) and the y co-ordinates (-1.0, 1.0).
     */
    Vec4<float> projectedPoint = camera.getProjection() * afk_vec4<float>(
        point.v[0], point.v[1], point.v[2], 1.0f);
    bool visible = camera.projectedPointIsVisible(projectedPoint);

    io_someVisible |= visible;
    io_allVisible &= visible;
}

void AFK_VisibleCell::testVisibility(
    const AFK_Camera& camera,
    bool& io_someVisible,
    bool& io_allVisible) const
{
    for (int x = 0; x <= 1; ++x)
    {
        for (int y = 0; y <= 1; ++y)
        {
            for (int z = 0; z <= 1; ++z)
            {
                testPointVisible(vertices[x][y][z], camera, io_someVisible, io_allVisible);
            }
        }
    }

    testPointVisible(midpoint, camera, io_someVisible, io_allVisible);
}

std::ostream& operator<<(std::ostream& os, const AFK_VisibleCell& visibleCell)
{
    os << "VisibleCell(";
    os << "base=" << visibleCell.vertices[0][0][0] << ", ";
    os << "right leg=" << visibleCell.vertices[1][0][0] << ", ";
    os << "top leg=" << visibleCell.vertices[0][1][0] << ", ";
    os << "back leg=" << visibleCell.vertices[0][0][1] << ", ";
    os << "midpoint=" << visibleCell.midpoint;
    os << ")";
    return os;
}

