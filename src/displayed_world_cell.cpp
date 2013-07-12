/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "core.hpp"
#include "displayed_world_cell.hpp"
#include "exception.hpp"


/* AFK_DisplayedWorldCell workers. */

static void computeFlatTriangle(
    const Vec3<float> *vertices,
    const Vec3<float> *colours,
    const Vec3<unsigned int>& indices,
    struct AFK_VcolPhongVertex *triangleVs,
    unsigned int triangleVOff,
    Vec3<unsigned int> *triangleIs,
    unsigned int triangleIOff)
{
    unsigned int i;

    Vec3<float> crossP = ((vertices[indices.v[1]] - vertices[indices.v[0]]).cross(
        vertices[indices.v[2]] - vertices[indices.v[0]]));

    /* Try to sort out the winding order so that under GL_CCW,
     * all triangles are facing upwards
     */
    triangleIs[triangleIOff].v[0] = triangleVOff;
    if (crossP.v[1] >= 0.0f)
    {
        triangleIs[triangleIOff].v[1] = triangleVOff + 2;
        triangleIs[triangleIOff].v[2] = triangleVOff + 1;
    }   
    else
    {
        triangleIs[triangleIOff].v[1] = triangleVOff + 1;
        triangleIs[triangleIOff].v[2] = triangleVOff + 2;
    }

    Vec3<float> normal = crossP.normalise();

    for (i = 0; i < 3; ++i)
    {
        triangleVs[triangleVOff + i].location = vertices[indices.v[i]];
        triangleVs[triangleVOff + i].colour = colours[indices.v[i]];
        triangleVs[triangleVOff + i].normal = normal;
    }
}

/* Turns a vertex grid into a world of flat triangles.
 * (Each triangle has different vertices because the normals
 * are different, so no index array is used.)
 * Call with NULL `triangleVs' to find out how large the
 * triangles array ought to be; that's an upper limit,
 * however, the real arrays may turn out smaller (esp. the
 * index array)
 */
static void vertices2FlatTriangles(
    Vec3<float> *vertices,
    Vec3<float> *colours,
    unsigned int vertexCount,
    unsigned int pointSubdivisionFactor,
    struct AFK_VcolPhongVertex *triangleVs,
    unsigned int *triangleVsCount,
    Vec3<unsigned int> *triangleIs,
    unsigned int *triangleIsCount)
{
    /* Each vertex generates 2 triangles (i.e. 6 triangle vertices) when
     * combined with the 3 vertices adjacent to it.
     */
    unsigned int expectedTriangleVsCount = SQUARE(pointSubdivisionFactor) * 6;

    if (!triangleVs)
    {
        /* Just output the required size */
        *triangleVsCount = expectedTriangleVsCount;
        *triangleIsCount = expectedTriangleVsCount / 3;
    }
    else
    {
        if (*triangleVsCount < expectedTriangleVsCount ||
            *triangleIsCount < (expectedTriangleVsCount / 3))
        {
            std::ostringstream ss;
            ss << "vertices2FlatTriangles: supplied " << *triangleVsCount << " triangle vertices and " << *triangleIsCount << " triangle indices (needed " << expectedTriangleVsCount << ")";
            throw AFK_Exception(ss.str());
        }

        *triangleVsCount = expectedTriangleVsCount;
        *triangleIsCount = expectedTriangleVsCount / 3;

        /* To make the triangles, I chew one row and the next at once.
         * Each triangle pair is:
         * ((row2, col1), (row1, col1), (row1, col2)),
         * ((row2, col1), (row1, col2), (row2, col2)).
         * The grid given to me goes from 0 to pointSubdivisionFactor+1
         * in both directions, so as to join up with adjacent cells.
         */
        unsigned int triangleVOff = 0, triangleIOff = 0;
        for (unsigned int row = 0; row < pointSubdivisionFactor; ++row)
        {
            for (unsigned int col = 0; col < pointSubdivisionFactor; ++col)
            {
                unsigned int i_r1c1 = row * (pointSubdivisionFactor+1) + col;
                unsigned int i_r1c2 = row * (pointSubdivisionFactor+1) + (col + 1);
                unsigned int i_r2c1 = (row + 1) * (pointSubdivisionFactor+1) + col;
                unsigned int i_r2c2 = (row + 1) * (pointSubdivisionFactor+1) + (col + 1);

                Vec3<unsigned int> indices1 = afk_vec3<unsigned int>(i_r2c1, i_r1c1, i_r1c2);
                computeFlatTriangle(vertices, colours, indices1, triangleVs, triangleVOff, triangleIs, triangleIOff);

                triangleVOff += 3;
                ++triangleIOff;

                Vec3<unsigned int> indices2 = afk_vec3<unsigned int>(i_r2c1, i_r1c2, i_r2c2);
                computeFlatTriangle(vertices, colours, indices2, triangleVs, triangleVOff, triangleIs, triangleIOff);

                triangleVOff += 3;
                ++triangleIOff;
            }
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

/* AFK_DisplayedWorldCell implementation */

AFK_DisplayedWorldCell::AFK_DisplayedWorldCell(
    const AFK_WorldCell *worldCell,
    unsigned int pointSubdivisionFactor,
    AFK_WorldCache *cache):
        vs(NULL),
        program(0)
{
    vs = new AFK_DisplayedBuffer<struct AFK_VcolPhongVertex>();

    Vec4<float> realCoord = worldCell->getRealCoord();

    /* I'm going to need to sample the edges of the next cell along
     * the +ve x and z too, in order to join up with it.
     * -- TODO: That needs to change (see: stitching).
     */
    /* TODO This is thrashing the heap.  Try making a single
     * scratch place for this in thread-local storage
     */
    unsigned int vertexCount = SQUARE(pointSubdivisionFactor + 1);
    Vec3<float> *rawVertices = new Vec3<float>[vertexCount];
    Vec3<float> *rawColours = new Vec3<float>[vertexCount];

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
            float xf = (float)xi * realCoord.v[3] / (float)pointSubdivisionFactor;
            float zf = (float)zi * realCoord.v[3] / (float)pointSubdivisionFactor;

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
                xf + xdisp + realCoord.v[0],
                realCoord.v[1],
                zf + zdisp + realCoord.v[2]);

            rawColours[rawIndex] = afk_vec3<float>(0.1f, 0.1f, 0.1f);

            ++rawIndex;
        }
    }

    /* Now apply the terrain transform.
     * TODO This may be slow -- obvious candidate for OpenCL?
     * But, the cache may rescue me; profile first!
     */
    worldCell->computeTerrain(rawVertices, rawColours, vertexCount, cache);

    /* I've completed my vertex array!  Transform this into an
     * array of VcolPhongVertex by computing colours and normals
     */
    unsigned int triangleVsCount, triangleIsCount;
    vertices2FlatTriangles(rawVertices, rawColours, vertexCount, pointSubdivisionFactor, NULL, &triangleVsCount, NULL, &triangleIsCount);

    /* TODO This will be different for non y=0 */
    vs->alloc(triangleVsCount);
    is.alloc(triangleIsCount);

    vertices2FlatTriangles(rawVertices, rawColours, vertexCount, pointSubdivisionFactor, vs->ts, &vs->count, is.ts, &is.count);

    /* I don't need this any more... */
    delete[] rawVertices;
    delete[] rawColours;
}

AFK_DisplayedWorldCell::~AFK_DisplayedWorldCell()
{
    /* I don't delete `vs' here, I don't necessarily own it.
     * The WorldCell will call deleteVs() first if it thinks
     * I should.
     */
}

void AFK_DisplayedWorldCell::initGL(void)
{
    program = afk_core.world->shaderProgram->program;
    worldTransformLocation = afk_core.world->worldTransformLocation;
    clipTransformLocation = afk_core.world->clipTransformLocation;
    shaderLight = new AFK_ShaderLight(program); /* TODO heap thrashing :P */

    vs->initGL(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    is.initGL(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
}

void AFK_DisplayedWorldCell::displaySetup(const Mat4<float>& projection)
{
    if (!shaderLight) initGL();

    /* A single DisplayedWorldCell will do this common stuff;
     * the others' display() functions will assume they were called
     * right after and don't have to do it again
     */
    glUseProgram(program);
    shaderLight->setupLight(afk_core.sun);
}

void AFK_DisplayedWorldCell::display(const Mat4<float>& projection)
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

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, is.buf);

    glDrawElements(GL_TRIANGLES, is.count * 3, GL_UNSIGNED_INT, 0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}

void AFK_DisplayedWorldCell::deleteVs(void)
{
    if (vs) delete vs;
}

std::ostream& operator<<(std::ostream& os, const AFK_DisplayedWorldCell& dlc)
{
    return os << "Displayed world cell with " << dlc.is.count << " triangle indices";
}
