/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "core.hpp"
#include "displayed_landscape_tile.hpp"
#include "exception.hpp"

AFK_DisplayedLandscapeTile::AFK_DisplayedLandscapeTile(
    AFK_LandscapeGeometry **_geometry,
    float _cellBoundLower,
    float _cellBoundUpper):
        geometry(_geometry), cellBoundLower(_cellBoundLower), cellBoundUpper(_cellBoundUpper)
{
}

#define FIRST_DEBUG 0

#if FIRST_DEBUG
/* TODO Nasty debug !
 * Fixing it so it ignores everything apart from
 * the first thing, so I can tell if the hang-up
 * is a queues-overfull type bug or something
 * else...
 * (damn, I wish OpenCL's error conditions were
 * less mysterious)
 *
 * -- okay, even if I only try to compute a single
 * thing, I'm still hanging on the chrono wait in
 * the main loop, waiting until we're nearly ready
 * to flip buffers.
 * *So weird*
 * Let's try rendering nothing except the protagonist,
 * instead.
 */
static bool computeFirst = true;
static bool displayFirst = false;
#endif

void AFK_DisplayedLandscapeTile::compute(unsigned int pointSubdivisionFactor)
{
    bool vsNeedBufferPush = (*geometry)->vs.initGLBuffer();
    bool isNeedBufferPush = (*geometry)->is.initGLBuffer();
    if (vsNeedBufferPush != isNeedBufferPush) throw AFK_Exception("vs/is inconsistency");
    if (!vsNeedBufferPush) return;

    /* Use the CL to turn the raw data into geometry for the
     * GL.
     */
    cl_context ctxt;
    cl_command_queue q;
    cl_int error;
    afk_core.computer->lock(ctxt, q);

#if FIRST_DEBUG
    /* TODO Diagnosis. */
    displayFirst = true;
    //if (!computeFirst) return;
    if (computeFirst)
    {
        std::cout << "First raw vertices: ";
        for (unsigned int idx = pointSubdivisionFactor + 4; idx < (2 * pointSubdivisionFactor) + 4; ++idx)
            std::cout << (*geometry)->rawVertices[idx] << ", ";
        std::cout << std::endl;
    }
#endif

    cl_mem rawVerticesMem = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        (*geometry)->rawVertexCount * sizeof(Vec3<float>),
        (*geometry)->rawVertices,
        &error);
    afk_handleClError(error);

    cl_mem rawColoursMem = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        (*geometry)->rawVertexCount * sizeof(Vec3<float>),
        (*geometry)->rawColours,
        &error);
    afk_handleClError(error);

    /* TODO I'm actually ignoring the structure members of
     * `vs' and `is' now.  I'll soon be able to get rid of
     * them.
     */
    cl_mem clVAndI[2];

    clVAndI[0] = clCreateFromGLBuffer(
        ctxt, CL_MEM_READ_WRITE, (*geometry)->vs.buf, &error);
    afk_handleClError(error);

    clVAndI[1] = clCreateFromGLBuffer(
        ctxt, CL_MEM_READ_WRITE, (*geometry)->is.buf, &error);
    afk_handleClError(error);

    AFK_CLCHK(clEnqueueAcquireGLObjects(q, 2, &clVAndI[0], 0, 0, 0))
        
    AFK_CLCHK(clSetKernelArg(afk_core.v2stKernel, 0, sizeof(cl_mem), &rawVerticesMem))
    AFK_CLCHK(clSetKernelArg(afk_core.v2stKernel, 1, sizeof(cl_mem), &rawColoursMem))
    AFK_CLCHK(clSetKernelArg(afk_core.v2stKernel, 2, sizeof(cl_mem), &clVAndI[0]))
    AFK_CLCHK(clSetKernelArg(afk_core.v2stKernel, 3, sizeof(cl_mem), &clVAndI[1]))
    AFK_CLCHK(clSetKernelArg(afk_core.v2stKernel, 4, sizeof(unsigned int), &pointSubdivisionFactor))

    size_t workDim[2];
    workDim[0] = workDim[1] = pointSubdivisionFactor + 1;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, afk_core.v2stKernel, 2, NULL, &workDim[0], NULL, 0, NULL, NULL))

#if FIRST_DEBUG
    if (computeFirst)
    {
        /* TODO debugging by enqueueing a read-back. */
        unsigned int vsCount, isCount, vsSize, isSize;
        afk_getLandscapeSizes(pointSubdivisionFactor, vsCount, isCount, vsSize, isSize);

        struct AFK_VcolPhongVertex *vsReadback = new struct AFK_VcolPhongVertex[vsCount];
        struct AFK_VcolPhongIndex *isReadback = new struct AFK_VcolPhongIndex[isCount];

        AFK_CLCHK(clEnqueueReadBuffer(q, clVAndI[0], CL_TRUE, 0, vsSize, vsReadback, 0, NULL, NULL))
        AFK_CLCHK(clEnqueueReadBuffer(q, clVAndI[1], CL_TRUE, 0, isSize, isReadback, 0, NULL, NULL))

        /* That ought to have made it finish the work ... */
        std::cout << "First computed vertices: ";
        for (unsigned int idx = pointSubdivisionFactor + 4; idx < (2 * pointSubdivisionFactor) + 4; ++idx)
            std::cout << vsReadback[idx].location << ", ";
        std::cout << std::endl;

        std::cout << "First computed colours: ";
        for (unsigned int idx = pointSubdivisionFactor + 4; idx < (2 * pointSubdivisionFactor) + 4; ++idx)
            std::cout << vsReadback[idx].colour << ", ";
        std::cout << std::endl;

        std::cout << "First computed normals: ";
        for (unsigned int idx = pointSubdivisionFactor + 4; idx < (2 * pointSubdivisionFactor) + 4; ++idx)
            std::cout << vsReadback[idx].normal << ", ";
        std::cout << std::endl;

        std::cout << "First computed indices: ";
        for (unsigned int idx = 0; idx < pointSubdivisionFactor; ++idx)
            std::cout << "(" << isReadback[idx].i[0] << ", " << isReadback[idx].i[1] << ", " << isReadback[idx].i[2] << "), ";
        std::cout << std::endl;

        std::cout << "Last computed indices: ";
        for (unsigned int idx = isCount - pointSubdivisionFactor; idx < isCount; ++idx)
            std::cout << "(" << isReadback[idx].i[0] << ", " << isReadback[idx].i[1] << ", " << isReadback[idx].i[2] << "), ";
        std::cout << std::endl;

        delete[] vsReadback;
        delete[] isReadback;
        computeFirst = false;
    }
#endif

    AFK_CLCHK(clEnqueueReleaseGLObjects(q, 2, &clVAndI[0], 0, 0, 0))
    AFK_CLCHK(clReleaseMemObject(clVAndI[0]))
    AFK_CLCHK(clReleaseMemObject(clVAndI[1]))
    AFK_CLCHK(clReleaseMemObject(rawColoursMem))
    AFK_CLCHK(clReleaseMemObject(rawVerticesMem))
    AFK_CLCHK(clFlush(q))
            
    afk_core.computer->unlock();
}

void AFK_DisplayedLandscapeTile::display(
    AFK_ShaderLight *shaderLight,
    const struct AFK_Light& globalLight,
    GLuint clipTransformLocation,
    GLuint yCellMinLocation,
    GLuint yCellMaxLocation,
    const Mat4<float>& projection)
{
#if FIRST_DEBUG
    /* TODO remove nasty debug!  :P */
    computeFirst = true;

    //if (!displayFirst) return;
#endif

    shaderLight->setupLight(globalLight);
    glUniformMatrix4fv(clipTransformLocation, 1, GL_TRUE, &projection.m[0][0]);
    glUniform1f(yCellMinLocation, cellBoundLower);
    glUniform1f(yCellMaxLocation, cellBoundUpper);

    (*geometry)->vs.bindBufferAndPush(false, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(sizeof(Vec3<float>)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(2 * sizeof(Vec3<float>)));

    (*geometry)->is.bindBufferAndPush(false, GL_ELEMENT_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

    glDrawElements(GL_TRIANGLES, (*geometry)->is.sizeHint * 2, GL_UNSIGNED_INT, 0);
#if FIRST_DEBUG
    displayFirst = false;
#endif
}

