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

    if ((*geometry)->vs.bindGLBuffer(GL_ARRAY_BUFFER, GL_STATIC_DRAW))
    {
        glBufferData(GL_ARRAY_BUFFER, (*geometry)->vs.t.size() * sizeof(struct AFK_VcolPhongVertex),
            &((*geometry)->vs.t[0]), GL_DYNAMIC_DRAW);
        /* TODO Put back the CL testing after I've made sure
         * that I haven't b0rked this stuff.
         */
#if 0
        /* TODO Here's the time to test that cl_gl stuff.
         * Let's bind this bunch of unsuspecting vertices to
         * the CL ...
         */
        cl_context ctxt;
        cl_command_queue q;
        cl_int error;
        afk_core.computer->lock(ctxt, q);

        cl_mem clVs = clCreateFromGLBuffer(
            ctxt, CL_MEM_READ_WRITE, (*geometry)->vs.buf, &error);

        /* TODO Why is this sometimes failing?
         * I appear to be getting out-of-resources or
         * out-of-host-memory :-(
         *
         * ...  The problem is quite likely to be incomplete
         * GL operations.  And the solution to that is, sadly,
         * pre-allocation of everything.  Which I guess I'm
         * going to need to do anyway.  But it's a fuckload of
         * code to have to write just in order to verify that
         * cl_gl sharing is working at all :-(
         * Here's a better thing to try:
         * - Create a pre-made GL buffers queue wrapper.
         * Because of the way GL buffers work, it'll be
         * pre-buffering all of them with zeroes.
         * - Create a CL program that accepts the original data
         * (I'll have to make temporary "real" CL buffers) and
         * the GL buffer objects, and copies the data across
         * (mangling the colour too just to verify).
         * - In AFK_DisplayedBuffer, the `vs' buffers become
         * those pre-made GL buffers, and I run the CL
         * program to fill them.
         */
        if (error == CL_SUCCESS)
        {
            AFK_CLCHK(clEnqueueAcquireGLObjects(q, 1, &clVs, 0, 0, 0))
        
            /* TODO Do an actual thing to it here. */
            AFK_CLCHK(clSetKernelArg(afk_core.testKernel, 0, sizeof(cl_mem), &clVs))
            size_t local_ws = (*geometry)->vs.t.size();
            size_t global_ws = local_ws;
            AFK_CLCHK(clEnqueueNDRangeKernel(q, afk_core.testKernel, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL))

            AFK_CLCHK(clEnqueueReleaseGLObjects(q, 1, &clVs, 0, 0, 0))
            AFK_CLCHK(clFlush(q))

            AFK_CLCHK(clReleaseMemObject(clVs))
        }
        else afk_handleClError(error);
            
        afk_core.computer->unlock();
        ((*geometry)->vs.bindGLBuffer(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW));
#endif
    }

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(sizeof(Vec3<float>)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(2 * sizeof(Vec3<float>)));

    if ((*geometry)->is.bindGLBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_DYNAMIC_DRAW))
    {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (*geometry)->is.t.size() * sizeof(Vec3<unsigned int>),
            &((*geometry)->is.t[0]), GL_DYNAMIC_DRAW);
    }

    glDrawElements(GL_TRIANGLES, (*geometry)->is.t.size() * 3, GL_UNSIGNED_INT, 0);
}

