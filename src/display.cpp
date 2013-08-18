/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <math.h>

#include "camera.hpp"
#include "core.hpp"
#include "display.hpp"
#include "exception.hpp"
#include "object.hpp"


void afk_displayedBufferGlBuffersForDeletion(GLuint *bufs, size_t bufsSize)
{
    afk_core.glBuffersForDeletion(bufs, bufsSize);
}


AFK_DisplayedObject::AFK_DisplayedObject():
    shaderProgram(NULL),
    worldTransformLocation(0),
    clipTransformLocation(0),
    shaderLight(NULL)
{
    bufs[0] = 0;
    bufs[1] = 0;
}

AFK_DisplayedObject::~AFK_DisplayedObject()
{
    if (shaderLight) delete shaderLight;
    if (shaderProgram) delete shaderProgram;
    if (bufs[0] && bufs[1]) glDeleteBuffers(2, &bufs[0]);
}

void AFK_DisplayedObject::updateTransform(const Mat4<float>& projection)
{
    Mat4<float> worldTransform = object.getTransformation();
    Mat4<float> clipTransform = projection * worldTransform;

    glUniformMatrix4fv(worldTransformLocation, 1, GL_TRUE, &worldTransform.m[0][0]);
    glUniformMatrix4fv(clipTransformLocation, 1, GL_TRUE, &clipTransform.m[0][0]);
}

void AFK_DisplayedProtagonist::initGL(void)
{
    /* Link up the shader program I want. */
    shaderProgram = new AFK_ShaderProgram();
    *shaderProgram << "vcol_phong_fragment" << "vcol_phong_vertex";
    shaderProgram->Link();

    worldTransformLocation = glGetUniformLocation(shaderProgram->program, "WorldTransform");
    clipTransformLocation = glGetUniformLocation(shaderProgram->program, "ClipTransform");

    shaderLight = new AFK_ShaderLight(shaderProgram->program);

    /* For now, I'm going to make a simple flat chevron
     * facing in the travel direction, because I can't
     * generate cool ones yet.
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

    glGenBuffers(2, &bufs[0]);

    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rawVertices), rawVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rawIndices), rawIndices, GL_STATIC_DRAW);
}

void AFK_DisplayedProtagonist::display(const Mat4<float>& projection)
{
    if (!shaderProgram) initGL();
    glUseProgram(shaderProgram->program);

    updateTransform(projection);

    shaderLight->setupLight(afk_core.sun);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(sizeof(Vec3<float>)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(2 * sizeof(Vec3<float>)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[1]);

    glDrawElements(GL_TRIANGLES, 8*3, GL_UNSIGNED_INT, 0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}


void afk_display(void)
{
    afk_core.world->doComputeTasks();

    Mat4<float> projection = afk_core.camera->getProjection();

    /* Make sure the display size is right */
    static unsigned int lastWindowWidth = 0, lastWindowHeight = 0;
    unsigned int windowWidth = afk_core.window->getWindowWidth();
    unsigned int windowHeight = afk_core.window->getWindowHeight();
    if (windowWidth != lastWindowWidth || windowHeight != lastWindowHeight)
    {
        glViewport(0, 0, windowWidth, windowHeight);
        afk_core.camera->setWindowDimensions(windowWidth, windowHeight);
        lastWindowWidth = windowWidth;
        lastWindowHeight = windowHeight;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    afk_core.world->display(projection, afk_core.sun);

    /* TODO Ignoring the protagonist for now.  I think it's served
     * its purpose: I need to come up with a way of making objects,
     * and then make a new protagonist that tracks through the
     * cells.
     * I do need to fix the camera transform accuracy bug first
     * though ...
     */
    afk_core.protagonist->display(projection);

    /* glFinish here behaves *very* badly along with vsync,
     * entirely screws up the detail calibrator
     */
    glFlush();

    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR)
    {
        /* TODO Non-fatal errors? (How should I handle out-of-memory?) */
        std::ostringstream ss;
        ss << "AFK: Got GL error: " << gluErrorString(glErr);
        throw AFK_Exception(ss.str());
    }
}

