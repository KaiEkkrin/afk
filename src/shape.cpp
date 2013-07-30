/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cfloat>
#include <cmath>
#include <cstring>

#include "shape.hpp"

void afk_getShapeSizes(
    unsigned int pointSubdivisionFactor,
    unsigned int& o_shapeVsSize,
    unsigned int& o_shapeIsSize)
{
    /* TODO Stuck with the static chevron for now.
     * I want to move to a future in which each shape
     * (LoD) has a fixed amount of geometry, just like
     * the landscape tiles.
     */
    o_shapeVsSize = 6 * sizeof(struct AFK_VcolPhongVertex);
    o_shapeIsSize = 8 * sizeof(Vec3<unsigned int>);
}

AFK_Shape::AFK_Shape(size_t vCount, size_t iCount,
    AFK_GLBufferQueue *vSource, AFK_GLBufferQueue *iSource, unsigned int _threadCount):
    vs(vCount, vSource), is(iCount, iSource), renderList(_threadCount), threadCount(_threadCount)
{
    drawListBuf = new GLuint[threadCount];
    drawListTex = new GLuint[threadCount];

    glGenBuffers(threadCount, drawListBuf);
    glGenTextures(threadCount, drawListTex);
}

AFK_Shape::~AFK_Shape()
{
    glDeleteTextures(threadCount, drawListTex);
    glDeleteBuffers(threadCount, drawListBuf);

    delete[] drawListTex;
    delete[] drawListBuf;
}

void AFK_Shape::resizeToShapeSpace(void)
{
    /* Work out a bounding box for the vertices. */
    Vec3<float> boundingBoxMin = afk_vec3<float>(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3<float> boundingBoxMax = afk_vec3<float>(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (std::vector<struct AFK_VcolPhongVertex>::iterator vIt = vs.t.begin();
        vIt != vs.t.end(); ++vIt)
    {
        for (unsigned int i = 0; i < 3; ++i)
        {
            if (vIt->location.v[i] < boundingBoxMin.v[i])
                boundingBoxMin.v[i] = vIt->location.v[i];

            if (vIt->location.v[i] > boundingBoxMax.v[i])
                boundingBoxMax.v[i] = vIt->location.v[i];
        }
    }

    /* Fit the biggest of the 3 dimensions within (0, 1),
     * and scale the other 3 to match.
     */
    Vec3<float> boundingBoxSize = boundingBoxMax - boundingBoxMin;
    float scale = -FLT_MAX;
    for (unsigned int i = 0; i < 3; ++i)
        if (boundingBoxSize.v[i] > scale) scale = boundingBoxSize.v[i];

    /* Also, move all co-ordinates so that the bounding
     * box starts at (0, 0).
     */
    for (std::vector<struct AFK_VcolPhongVertex>::iterator vIt = vs.t.begin();
        vIt != vs.t.end(); ++vIt)
    {
        vIt->location = (vIt->location - boundingBoxMin) / scale;
    }
}

void AFK_Shape::updatePush(unsigned int threadId, const Mat4<float>& transform)
{
    renderList.updatePush(threadId, transform);
}

void AFK_Shape::display(
    AFK_ShaderLight *shaderLight,
    const struct AFK_Light& globalLight,
    GLuint projectionTransformLocation,
    const Mat4<float>& projection)
{
    renderList.flipLists();

    /* For each list, I need to push its contents into the
     * world transform matrix buffer (which describes how
     * each instance differs!), set up the other globals and
     * then make my instanced render call.
     */

    /* TODO What with the gl buffer queue stuff, I've gone
     * and broken this.  I need to resurrect it, but for
     * now these things are disabled anyway, so I will
     * leave it and work on the landscape again first.
     */
    for (unsigned int threadId = 0; threadId < renderList.getThreadCount(); ++threadId)
    {
        shaderLight->setupLight(globalLight);
        glUniformMatrix4fv(projectionTransformLocation, 1, GL_TRUE, &projection.m[0][0]);

        vs.bindGLBuffer(GL_ARRAY_BUFFER, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(sizeof(Vec3<float>)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(2 * sizeof(Vec3<float>)));

        is.bindGLBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);

        /* Maybe (if this turns out slow?) I should try doing
         * this with a uniform array of the matrices.  But
         * those things look really hard (standard paddings,
         * argh) and have a size limit.  Mmmmmmh
         */
        glActiveTexture(GL_TEXTURE0);
        glBindBuffer(GL_TEXTURE_BUFFER, drawListBuf[threadId]);
        glBufferData(
            GL_TEXTURE_BUFFER,
            renderList.getDrawListSize(threadId) * sizeof(Mat4<float>), 
            renderList.getDrawListBase(threadId),
            GL_STREAM_DRAW /* I'm going to use it once then throw it away */ );

        glBindTexture(GL_TEXTURE_BUFFER, drawListTex[threadId]);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F /* 4 floats */, drawListBuf[threadId]);

        glDrawElementsInstanced(GL_TRIANGLES, is.t.size() * 3, GL_UNSIGNED_INT, 0, renderList.getDrawListSize(threadId));
    }
}

AFK_ShapeChevron::AFK_ShapeChevron(AFK_GLBufferQueue *vSource, AFK_GLBufferQueue *iSource, unsigned int _threadCount):
    AFK_Shape(6, 8, vSource, iSource, _threadCount)
{
    /* Stealing the protagonist chevron as an initial
     * test.
     */
    struct AFK_VcolPhongVertex rawVertices[] = {
        /* location ...                             colour ...                  normal */

        /* bow */
        {{{  0.0f,   0.0f,   2.0f,  }},  /* 0 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  0.0f, 0.0f, 1.0f,  }}},

        /* top and bottom */
        {{{  0.0f,   0.3f,   -0.5f, }},  /* 1 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  0.0f, 1.0f, 0.0f,  }}},
        {{{  0.0f,   -0.3f,  -0.5f, }},  /* 2 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  0.0f, -1.0f, 0.0f, }}},

        /* wingtips */
        {{{  1.0f,   0.0f,   -0.5f, }},  /* 3 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  1.0f, 0.0f, 0.0f,  }}},
        {{{  -1.0f,  0.0f,   -0.5f, }},  /* 4 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  -1.0f, 0.0f, 0.0f, }}},

        /* concave stern */
        {{{  0.0f,   0.0f,   0.0f,  }},  /* 5 */     {{  0.2f, 1.0f, 1.0f,  }},  {{  0.0f, 0.0f, -1.0f, }}},
    };

    unsigned int rawIndices[] = {
        /* bow/top/wingtips */
        0, 1, 3,
        0, 4, 1,

        /* bow/bottom/wingtips */
        0, 3, 2,
        0, 2, 4,

        /* stern */
        4, 5, 1,
        1, 5, 3,
        3, 5, 2,
        2, 5, 4,
    };

    vs.t.resize(6);
    memcpy(&vs.t[0], rawVertices, 6 * sizeof(struct AFK_VcolPhongVertex));
    is.t.resize(8);
    memcpy(&is.t[0], rawIndices, 8 * 3 * sizeof(unsigned int));

    resizeToShapeSpace();
}

