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

void AFK_DisplayedLandscapeTile::compute(void)
{
    static size_t cCount = 0;

    bool needBufferPush = (*geometry)->vs.initGLBuffer();
    if (!needBufferPush) return;
    if ((cCount & 0x1) == 0)
    {
        /* TODO Here's the time to test that cl_gl stuff.
         * Let's bind this bunch of unsuspecting vertices to
         * the CL ...
         */

        cl_context ctxt;
        cl_command_queue q;
        cl_int error;
        afk_core.computer->lock(ctxt, q);

        size_t vsSize = (*geometry)->vs.t.size();

        cl_mem clSourceVs = clCreateBuffer(
            ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            vsSize * sizeof(struct AFK_VcolPhongVertex),
            &((*geometry)->vs.t[0]),
            &error);
        afk_handleClError(error);

        /* Note that if a previous kernel overwrites its buffer,
         * the symptom can be a -5 error here when trying to
         * make the next one (ewch)
         */
        cl_mem clVs = clCreateFromGLBuffer(
            ctxt, CL_MEM_READ_WRITE, (*geometry)->vs.buf, &error);
        afk_handleClError(error);

        AFK_CLCHK(clEnqueueAcquireGLObjects(q, 1, &clVs, 0, 0, 0))
        
        AFK_CLCHK(clSetKernelArg(afk_core.testKernel, 0, sizeof(cl_mem), &clSourceVs))
        AFK_CLCHK(clSetKernelArg(afk_core.testKernel, 1, sizeof(cl_mem), &clVs))

        int iVsSize = (int)vsSize;
        AFK_CLCHK(clSetKernelArg(afk_core.testKernel, 2, sizeof(int), &iVsSize))
        size_t global_ws = vsSize;
        AFK_CLCHK(clEnqueueNDRangeKernel(q, afk_core.testKernel, 1, NULL, &global_ws, NULL /* &local_ws */, 0, NULL, NULL))

        AFK_CLCHK(clEnqueueReleaseGLObjects(q, 1, &clVs, 0, 0, 0))
        AFK_CLCHK(clReleaseMemObject(clVs))
        AFK_CLCHK(clReleaseMemObject(clSourceVs))
        AFK_CLCHK(clFlush(q))
            
        afk_core.computer->unlock();
    }
    else (*geometry)->vs.bindBufferAndPush(true, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

    ++cCount;
}

void AFK_DisplayedLandscapeTile::display(
    AFK_ShaderLight *shaderLight,
    const struct AFK_Light& globalLight,
    GLuint clipTransformLocation,
    GLuint yCellMinLocation,
    GLuint yCellMaxLocation,
    const Mat4<float>& projection)
{
    shaderLight->setupLight(globalLight);
    glUniformMatrix4fv(clipTransformLocation, 1, GL_TRUE, &projection.m[0][0]);
    glUniform1f(yCellMinLocation, cellBoundLower);
    glUniform1f(yCellMaxLocation, cellBoundUpper);

    (*geometry)->vs.bindBufferAndPush(false, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(sizeof(Vec3<float>)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(2 * sizeof(Vec3<float>)));

    (*geometry)->is.bindBufferAndPush((*geometry)->is.initGLBuffer(), GL_ELEMENT_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

    glDrawElements(GL_TRIANGLES, (*geometry)->is.t.size() * 3, GL_UNSIGNED_INT, 0);
}

