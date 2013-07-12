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
    struct AFK_VcolPhongVertex *triangleVPos)
{
    struct AFK_VcolPhongVertex *outStructs[3];
    unsigned int i;

    for (i = 0; i < 3; ++i)
        outStructs[i] = triangleVPos + i;

    Vec3<float> crossP = ((vertices[indices.v[1]] - vertices[indices.v[0]]).cross(
        vertices[indices.v[2]] - vertices[indices.v[0]]));

    /* Try to sort out the winding order so that under GL_CCW,
     * all triangles are facing upwards
     */
    if (crossP.v[1] >= 0.0f)
    {
        outStructs[1] = triangleVPos + 2;
        outStructs[2] = triangleVPos + 1;
    }   

    Vec3<float> normal = crossP.normalise();

    for (i = 0; i < 3; ++i)
    {
        outStructs[i]->location = vertices[indices.v[i]];
        outStructs[i]->colour = colours[indices.v[i]];
        outStructs[i]->normal = normal;
    }
}

/* Turns a vertex grid into a world of flat triangles.
 * (Each triangle has different vertices because the normals
 * are different, so no index array is used.)
 * Call with NULL `triangleVs' to find out how large the
 * triangles array ought to be.
 */
static void vertices2FlatTriangles(
    Vec3<float> *vertices,
    Vec3<float> *colours,
    unsigned int vertexCount,
    unsigned int pointSubdivisionFactor,
    struct AFK_VcolPhongVertex *triangleVs,
    unsigned int *triangleVsCount)
{
    /* Each vertex generates 2 triangles (i.e. 6 triangle vertices) when
     * combined with the 3 vertices adjacent to it.
     */
    unsigned int expectedTriangleVsCount = SQUARE(pointSubdivisionFactor) * 6;

    if (!triangleVs)
    {
        /* Just output the required size */
        *triangleVsCount = expectedTriangleVsCount;
    }
    else
    {
        if (*triangleVsCount < expectedTriangleVsCount)
        {
            std::ostringstream ss;
            ss << "vertices2FlatTriangles: supplied " << *triangleVsCount << " triangle vertices (needed " << expectedTriangleVsCount << ")";
            throw AFK_Exception(ss.str());
        }

        *triangleVsCount = expectedTriangleVsCount;

        /* To make the triangles, I chew one row and the next at once.
         * Each triangle pair is:
         * ((row2, col1), (row1, col1), (row1, col2)),
         * ((row2, col1), (row1, col2), (row2, col2)).
         * The grid given to me goes from 0 to pointSubdivisionFactor+1
         * in both directions, so as to join up with adjacent cells.
         */
        struct AFK_VcolPhongVertex *triangleVPos = triangleVs;
        for (unsigned int row = 0; row < pointSubdivisionFactor; ++row)
        {
            for (unsigned int col = 0; col < pointSubdivisionFactor; ++col)
            {
                unsigned int i_r1c1 = row * (pointSubdivisionFactor+1) + col;
                unsigned int i_r1c2 = row * (pointSubdivisionFactor+1) + (col + 1);
                unsigned int i_r2c1 = (row + 1) * (pointSubdivisionFactor+1) + col;
                unsigned int i_r2c2 = (row + 1) * (pointSubdivisionFactor+1) + (col + 1);

                Vec3<unsigned int> tri1Is = afk_vec3<unsigned int>(i_r2c1, i_r1c1, i_r1c2);
                computeFlatTriangle(vertices, colours, tri1Is, triangleVPos);

                triangleVPos += 3;

                Vec3<unsigned int> tri2Is = afk_vec3<unsigned int>(i_r2c1, i_r1c2, i_r2c2);
                computeFlatTriangle(vertices, colours, tri2Is, triangleVPos);

                triangleVPos += 3;
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
        triangleVs(NULL),
        triangleVsCount(0),
        program(0),
        vertices(0),
        vertexCount(0)
{
    Vec4<float> realCoord = worldCell->getRealCoord();

    /* I'm going to need to sample the edges of the next cell along
     * the +ve x and z too, in order to join up with it.
     * -- TODO: That needs to change (see: stitching).
     */
    /* TODO This is thrashing the heap.  Try making a single
     * scratch place for this in thread-local storage
     */
    vertexCount = SQUARE(pointSubdivisionFactor + 1);
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
    vertices2FlatTriangles(rawVertices, rawColours, vertexCount, pointSubdivisionFactor, NULL, &triangleVsCount);
    triangleVs = new struct AFK_VcolPhongVertex[triangleVsCount];
    vertices2FlatTriangles(rawVertices, rawColours, vertexCount, pointSubdivisionFactor, triangleVs, &triangleVsCount);

    /* I don't need this any more... */
    delete[] rawVertices;
    delete[] rawColours;
}

AFK_DisplayedWorldCell::~AFK_DisplayedWorldCell()
{
    delete[] triangleVs;
    
    //if (vertices) glDeleteBuffers(1, &vertices);
    if (vertices) afk_core.glBuffersForDeletion(&vertices, 1);
}

void AFK_DisplayedWorldCell::initGL(void)
{
    program = afk_core.world->shaderProgram->program;
    worldTransformLocation = afk_core.world->worldTransformLocation;
    clipTransformLocation = afk_core.world->clipTransformLocation;
    shaderLight = new AFK_ShaderLight(program); /* TODO heap thrashing :P */

    glGenBuffers(1, &vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glBufferData(GL_ARRAY_BUFFER, triangleVsCount * sizeof(struct AFK_VcolPhongVertex), triangleVs, GL_STATIC_DRAW);
    vertexCount = triangleVsCount;
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
    if (!vertices) initGL();

    if (vertices)
    {
        updateTransform(projection);

        /* Cell specific stuff will go here ... */

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER, vertices);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(sizeof(Vec3<float>)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(2 * sizeof(Vec3<float>)));

        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
}

std::ostream& operator<<(std::ostream& os, const AFK_DisplayedWorldCell& dlc)
{
    return os << "Displayed world cell with " << dlc.vertexCount << " vertices";
}
