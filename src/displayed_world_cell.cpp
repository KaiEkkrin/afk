/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include "core.hpp"
#include "debug.hpp"
#include "displayed_world_cell.hpp"
#include "exception.hpp"


#define FIND_INDEX_BUFFER_DEBUG 0


void AFK_DisplayedWorldCell::computeFlatTriangle(
    const Vec3<float> *vertices,
    const Vec3<float> *colours,
    const Vec3<unsigned int>& indices,
    unsigned int triangleVOff,
    AFK_DWC_INDEX_BUF& triangleIs)
{
    unsigned int i;

    Vec3<float> crossP = ((vertices[indices.v[1]] - vertices[indices.v[0]]).cross(
        vertices[indices.v[2]] - vertices[indices.v[0]]));

    /* Try to sort out the winding order so that under GL_CCW,
     * all triangles are facing upwards
     */
    Vec3<unsigned int> triangle;
    triangle.v[0] = triangleVOff;
    if (crossP.v[1] >= 0.0f)
    {
        triangle.v[1] = triangleVOff + 2;
        triangle.v[2] = triangleVOff + 1;
    }   
    else
    {
        triangle.v[1] = triangleVOff + 1;
        triangle.v[2] = triangleVOff + 2;
    }

    triangleIs.t.push_back(triangle);

    Vec3<float> normal = crossP.normalise();

    for (i = 0; i < 3; ++i)
    {
        struct AFK_VcolPhongVertex vertex;
        vertex.location = vertices[indices.v[i]];
        vertex.colour = colours[indices.v[i]];
        vertex.normal = normal;
        vs->t.push_back(vertex);
    }
}

AFK_DWC_INDEX_BUF& AFK_DisplayedWorldCell::findIndexBuffer(
    const Vec3<float>& vertex,
    const AFK_Cell& baseCell,
    float minCellSize,
    float sizeHint)
{
#if 1
    /* The operation that rounds a float to a long long will
     * always round towards 0, and I want to round negatives
     * down to the next negative number.
     * Also: I want to make cells of the same scale as the
     * base cell, aligned to the cell grid at that scale,
     * which means I need to round down the nearest multiple of
     * the base cell's scale.
     * TODO Test this with minCellSize != 1, I'm not sure I
     * did it right.
     */
    float fy = vertex.v[1] / minCellSize;
    float fyScaled = (floor(fy / baseCell.coord.v[3]) * baseCell.coord.v[3]);
    long long y = (long long)fyScaled * MIN_CELL_PITCH;
    if (y == baseCell.coord.v[1])
    {
#if FIND_INDEX_BUFFER_DEBUG
        AFK_DEBUG_PRINTL(vertex << " --> base cell " << baseCell)
#endif
        return *(spillIs[0]);
    }

    AFK_Cell vertexCell = afk_cell(afk_vec4<long long>(
        baseCell.coord.v[0],
        /* TODO Rebase -- this will need transforming */
        y,
        baseCell.coord.v[2],
        baseCell.coord.v[3]));

    for (unsigned int i = 1; i < spillCellsSize; ++i)
    {
        if (spillCells[i] == vertexCell)
        {
#if FIND_INDEX_BUFFER_DEBUG
            AFK_DEBUG_PRINTL(vertex << " --> " << spillCells[i] << " (existing at " << i << ")")
#endif
            return *(spillIs[i]);
        }
    }

    spillCells.push_back(vertexCell);
    boost::shared_ptr<AFK_DWC_INDEX_BUF> spillBuffer(new AFK_DWC_INDEX_BUF(sizeHint));
    spillIs.push_back(spillBuffer);
#if FIND_INDEX_BUFFER_DEBUG
    AFK_DEBUG_PRINTL(vertex << " --> " << vertexCell << " (new at " << spillCellsSize << ")")
#endif
    ++spillCellsSize;
    return *spillBuffer;
#else
    return *(spillIs[0]);
#endif
}

/* Turns a vertex grid into a world of flat triangles.
 * (Each triangle has different vertices because the normals
 * are different, so no index array is used.)
 * Call with NULL `triangleVs' to find out how large the
 * triangles array ought to be; that's an upper limit,
 * however, the real arrays may turn out smaller (esp. the
 * index array)
 */
void AFK_DisplayedWorldCell::vertices2FlatTriangles(
    Vec3<float> *vertices,
    Vec3<float> *colours,
    unsigned int vertexCount,
    const AFK_Cell& baseCell,
    unsigned int pointSubdivisionFactor,
    float minCellSize)
{
    /* A heuristic size for the index buffers (it's actually 1/2 the max
     * required, which won't always be required).
     */
    size_t indexSizeHint = SQUARE(pointSubdivisionFactor);

    /* Each vertex generates 2 triangles (i.e. 6 triangle vertices) when
     * combined with the 3 vertices adjacent to it.
     */
    size_t expectedTriangleVsCount = indexSizeHint * 6;

    if (vs)
    {
        /* TODO Is this possible?  Partial vertex population then a barf? */
        throw new AFK_Exception("Partial population problem");
    }

    /* Set things up */
    boost::shared_ptr<AFK_DWC_VERTEX_BUF> newVs(new AFK_DWC_VERTEX_BUF(expectedTriangleVsCount));
    vs = newVs;
    spillCells.push_back(baseCell);

    boost::shared_ptr<AFK_DWC_INDEX_BUF> newIs(new AFK_DWC_INDEX_BUF(indexSizeHint));
    spillIs.push_back(newIs);
    spillCellsSize = 1;

    /* To make the triangles, I chew one row and the next at once.
     * Each triangle pair is:
     * ((row2, col1), (row1, col1), (row1, col2)),
     * ((row2, col1), (row1, col2), (row2, col2)).
     * The grid given to me goes from 0 to pointSubdivisionFactor+1
     * in both directions, so as to join up with adjacent cells.
     */
    unsigned int triangleVOff = 0;
    for (unsigned int row = 0; row < pointSubdivisionFactor; ++row)
    {
        for (unsigned int col = 0; col < pointSubdivisionFactor; ++col)
        {
            unsigned int i_r1c1 = row * (pointSubdivisionFactor+1) + col;
            unsigned int i_r1c2 = row * (pointSubdivisionFactor+1) + (col + 1);
            unsigned int i_r2c1 = (row + 1) * (pointSubdivisionFactor+1) + col;
            unsigned int i_r2c2 = (row + 1) * (pointSubdivisionFactor+1) + (col + 1);

            /* The location of the vertex at the first row governs
             * which cell the triangle belongs to.
             */

            Vec3<unsigned int> indices1 = afk_vec3<unsigned int>(i_r2c1, i_r1c1, i_r1c2);
            computeFlatTriangle(
                vertices, colours, indices1, triangleVOff,
                findIndexBuffer(vertices[i_r1c1], baseCell, minCellSize, indexSizeHint));

            triangleVOff += 3;

            Vec3<unsigned int> indices2 = afk_vec3<unsigned int>(i_r2c1, i_r1c2, i_r2c2);
            computeFlatTriangle(
                vertices, colours, indices2, triangleVOff,
                findIndexBuffer(vertices[i_r1c2], baseCell, minCellSize, indexSizeHint));

            triangleVOff += 3;
        }
    }
}


/* Turns a vertex grid into a world of curved triangles,
 * producing an array of AFK_VcolPhongVertex and also an
 * index array.
 * Again, call with NULL pointers to find out how big the
 * arrays ought to be.
 */
#if 0
static void vertices2CurvedTriangles(
    float *vertices,
    size_t vertexCount,
    struct AFK_VcolPhongVertex *triangles,
    size_t *io_trianglesCount,
    unsigned int *indices,
    size_t *indicesCount)
{
    /* TODO. */
}
#endif

void AFK_DisplayedWorldCell::makeRawTerrain(
    const Vec4<float>& baseCoord,
    unsigned int pointSubdivisionFactor)
{
    /* I'm going to need to sample the edges of the next cell along
     * the +ve x and z too, in order to join up with it.
     * -- TODO: That needs to change (see: stitching).
     */
    /* TODO This is thrashing the heap.  Try making a single
     * scratch place for this in thread-local storage
     */
    rawVertexCount = SQUARE(pointSubdivisionFactor + 1);
    rawVertices = new Vec3<float>[rawVertexCount];
    rawColours = new Vec3<float>[rawVertexCount];

    /* Populate the vertex and colour arrays. */
    size_t rawIndex = 0;    

    for (unsigned int xi = 0; xi < pointSubdivisionFactor + 1; ++xi)
    {
        for (unsigned int zi = 0; zi < pointSubdivisionFactor + 1; ++zi)
        {
            /* The geometry in a cell goes from its (0,0,0) point to
             * just before its (coord.v[3], coord.v[3], coord.v[3]
             * point (in cell space)
             */
            float xf = (float)xi * baseCoord.v[3] / (float)pointSubdivisionFactor;
            float zf = (float)zi * baseCoord.v[3] / (float)pointSubdivisionFactor;

            /* Jitter the vertices inside the cell around a bit. 
             * Not the edge ones; that will cause a join-up
             * headache (especially at different levels of detail).
             * TODO Of course, I'm going to have a headache on a
             * transition between LoDs anyway, but I'm trying not
             * to think about that too hard right now :P
             */ 
            float xdisp = 0.0f, zdisp = 0.0f;
#if 0
            if (xi > 0 && xi <= pointSubdivisionFactor)
                xdisp = rng.frand() * worldCell->coord.v[3] / ((float)pointSubdivisionFactor * 2.0f);

            if (zi > 0 && zi <= pointSubdivisionFactor)
                zdisp = rng.frand() * worldCell->coord.v[3] / ((float)pointSubdivisionFactor * 2.0f);
#endif

            /* And shunt them into world space */
            rawVertices[rawIndex] = afk_vec3<float>(
                xf + xdisp + baseCoord.v[0],
                baseCoord.v[1],
                zf + zdisp + baseCoord.v[2]);

            rawColours[rawIndex] = afk_vec3<float>(0.1f, 0.1f, 0.1f);

            ++rawIndex;
        }
    }
}

AFK_DisplayedWorldCell::AFK_DisplayedWorldCell():
    AFK_Claimable(),
    rawVertices(NULL),
    rawColours(NULL),
    rawVertexCount(0),
    spillCellsSize(0),
    spillFinished(false),
    program(0)
{
}

AFK_DisplayedWorldCell::~AFK_DisplayedWorldCell()
{
    if (rawVertices) delete[] rawVertices;
    if (rawColours) delete[] rawColours;
}

bool AFK_DisplayedWorldCell::hasRawTerrain(
    const Vec4<float>& baseCoord,
    unsigned int pointSubdivisionFactor)
{
    /* Check for ...
     * - It's a base cell.  If not, it won't have raw terrain.
     * - It doesn't have a raw terrain already.
     * - It doesn't have `vs' (computed geometry), which implies we
     * already completed the raw terrain pass.
     */
    if (baseCoord.v[1] == 0 && !rawVertices && !vs) makeRawTerrain(baseCoord, pointSubdivisionFactor);
    return (rawVertices != NULL);
}

void AFK_DisplayedWorldCell::computeGeometry(
    unsigned int pointSubdivisionFactor,
    const AFK_Cell& baseCell,
    float minCellSize,
    const AFK_TerrainList& terrain)
{
    if (!rawVertices || !rawColours) return;

    /* Apply the terrain transform.
     * TODO This may be slow -- obvious candidate for OpenCL?  * But, the cache may rescue me; profile first!
     */
    terrain.compute(rawVertices, rawColours, rawVertexCount);

    /* I've completed my vertex array!  Now, compute the triangles
     * and choose the actual world cells that they ought to be
     * matched with
     */
    vertices2FlatTriangles(
        rawVertices, rawColours, rawVertexCount, baseCell, pointSubdivisionFactor, minCellSize);

    /* I don't need this any more... */
    delete[] rawVertices;
    delete[] rawColours;

    rawVertices = NULL;
    rawColours = NULL;
    rawVertexCount = 0;
}

std::vector<AFK_Cell>::iterator AFK_DisplayedWorldCell::spillCellsBegin()
{
    if (hasGeometry())
    {
        std::vector<AFK_Cell>::iterator spillCellsIt = spillCells.begin();
        ++spillCellsIt;
        return spillCellsIt;
    }
    else return spillCells.end();
}

std::vector<boost::shared_ptr<AFK_DWC_INDEX_BUF> >::iterator AFK_DisplayedWorldCell::spillIsBegin()
{
    if (hasGeometry())
    {
        std::vector<boost::shared_ptr<AFK_DWC_INDEX_BUF> >::iterator spillIsIt = spillIs.begin();
        ++spillIsIt;
        return spillIsIt;
    }
    else return spillIs.end();
}

std::vector<AFK_Cell>::iterator AFK_DisplayedWorldCell::spillCellsEnd()
{
    return spillCells.end();
}

std::vector<boost::shared_ptr<AFK_DWC_INDEX_BUF> >::iterator AFK_DisplayedWorldCell::spillIsEnd()
{
    return spillIs.end();
}

void AFK_DisplayedWorldCell::spill(
    AFK_DisplayedWorldCell& source,
    const AFK_Cell& cell,
    boost::shared_ptr<AFK_DWC_INDEX_BUF> indices)
{
    if (spillCellsSize == 0)
    {
        vs = source.vs;
        spillCells.push_back(cell);
        spillIs.push_back(indices);
        spillCellsSize = 1;
    }   
}

void AFK_DisplayedWorldCell::spillFrom(
    AFK_DisplayedWorldCell& source,
    const AFK_Cell& thisCell)
{
    /* Sanity check. */
    if (spillCellsSize != 0)
    {
        std::ostringstream ss;
        ss << "Tried to spill into " << thisCell << " (already with " << spillCellsSize << " indexes)";
        throw new AFK_Exception(ss.str());
    }

    if (!spillFinished)
    {
        std::vector<AFK_Cell>::iterator spillCellsIt = source.spillCellsBegin();
        std::vector<boost::shared_ptr<AFK_DWC_INDEX_BUF> >::iterator spillIsIt = source.spillIsBegin();

        while (spillCellsIt != source.spillCellsEnd())
        {
            if (spillIsIt == source.spillIsEnd()) throw AFK_Exception("Cell and index buffers size mismatch");

            if (*spillCellsIt == thisCell)
            {
                spill(source, thisCell, *spillIsIt);
                break;
            }

            ++spillCellsIt;
            ++spillIsIt;
        }

        spillFinished = true;
    }
}

bool AFK_DisplayedWorldCell::haveSpilledTo(void) const
{
    return spillFinished;
}

void AFK_DisplayedWorldCell::initGL(void)
{
    if (hasGeometry())
    {
        program = afk_core.world->shaderProgram->program;
        worldTransformLocation = afk_core.world->worldTransformLocation;
        clipTransformLocation = afk_core.world->clipTransformLocation;
        shaderLight = new AFK_ShaderLight(program); /* TODO heap thrashing :P */

        vs->initGL(GL_ARRAY_BUFFER, GL_STATIC_DRAW);

        /* My indices are always the first in the list */
        spillIs[0]->initGL(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
    }
}

void AFK_DisplayedWorldCell::displaySetup(const Mat4<float>& projection)
{
    if (hasGeometry())
    {
        if (!shaderLight) initGL();

        /* A single DisplayedWorldCell will do this common stuff;
         * the others' display() functions will assume they were called
         * right after and don't have to do it again
         */
        glUseProgram(program);
        shaderLight->setupLight(afk_core.sun);
    }
}

void AFK_DisplayedWorldCell::display(const Mat4<float>& projection)
{
    if (hasGeometry())
    {
        if (!shaderLight) initGL();

        updateTransform(projection);

        /* Cell specific stuff will go here ... */

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER, vs->buf);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(sizeof(Vec3<float>)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(2 * sizeof(Vec3<float>)));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spillIs[0]->buf);

        glDrawElements(GL_TRIANGLES, spillIs[0]->t.size() * 3, GL_UNSIGNED_INT, 0);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
    else
    {
        throw new AFK_Exception("Enqueued geometry-less cell");
    }
}

AFK_Frame AFK_DisplayedWorldCell::getCurrentFrame(void) const
{
    return afk_core.computingFrame;
}

bool AFK_DisplayedWorldCell::canBeEvicted(void) const
{
    /* This is a tweakable value ... */
    bool canEvict = ((afk_core.computingFrame - lastSeen) > 10);
    return canEvict;
}

std::ostream& operator<<(std::ostream& os, const AFK_DisplayedWorldCell& dlc)
{
    return os << "Displayed world cell with " << dlc.spillCellsSize << " spill cells";
}

