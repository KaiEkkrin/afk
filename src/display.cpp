/* AFK (c) Alex Holloway 2013 */

#include "afk.h"

#include <math.h>

#include "camera.h"
#include "core.h"
#include "display.h"
#include "exception.h"
#include "object.h"


void AFK_DisplayedObject::updateTransform(const Mat4<float>& projection)
{
    Mat4<float> objectTransform = projection * object.getTransformation();
    glUniformMatrix4fv(transformLocation, 1, GL_TRUE, &objectTransform.m[0][0]);
}

AFK_DisplayedTestObject::AFK_DisplayedTestObject()
{
    /* Link up the shader program I want. */
    shaderProgram << "basic_fragment" << "basic_vertex";
    shaderProgram.Link();

    transformLocation = glGetUniformLocation(shaderProgram.program, "transform");
    fixedColorLocation = glGetUniformLocation(shaderProgram.program, "fixedColor");

    /* Setup the test object's data. */
    float rawVertices[] = {
        -1.0f,  -1.0f,  0.0f,
        1.0f,   -1.0f,  0.0f,
        0.0f,   1.0f,   0.0f
    };

    glGenBuffers(1, &vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rawVertices), rawVertices, GL_STATIC_DRAW);

    colour = Vec3<float>(1.0f, 0.0f, 0.0f);
}

AFK_DisplayedTestObject::~AFK_DisplayedTestObject()
{
    glDeleteBuffers(1, &vertices);
}

void AFK_DisplayedTestObject::display(const Mat4<float>& projection)
{
    glUseProgram(shaderProgram.program);
    glUniform3f(fixedColorLocation, colour.v[0], colour.v[1], colour.v[2]);

    updateTransform(projection);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(0);
}

AFK_DisplayedProtagonist::AFK_DisplayedProtagonist()
{
    /* Link up the shader program I want. */
    shaderProgram << "basic_fragment" << "basic_vertex";
    shaderProgram.Link();

    transformLocation = glGetUniformLocation(shaderProgram.program, "transform");
    fixedColorLocation = glGetUniformLocation(shaderProgram.program, "fixedColor");

    /* For now, I'm going to make a simple flat chevron
     * facing in the travel direction, because I can't
     * generate cool ones yet.
     */
    float rawVertices[] = {
        /* bow */
        0.0f,   0.0f,   2.0f,   /* 0 */

        /* top and bottom */
        0.0f,   0.3f,   -0.5f,  /* 1 */
        0.0f,   -0.3f,  -0.5f,  /* 2 */

        /* wingtips */
        1.0f,   0.0f,   -0.5f,  /* 3 */
        -1.0f,  0.0f,   -0.5f,  /* 4 */

        /* concave stern */
        0.0f,   0.0f,   0.0f,   /* 5 */
    };

    /* TODO Only the top two triangles are rendering.  Why? */
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

    colour = Vec3<float>(0.2f, 1.0f, 1.0f);
}

AFK_DisplayedProtagonist::~AFK_DisplayedProtagonist()
{
    glDeleteBuffers(2, &bufs[0]);
}

void AFK_DisplayedProtagonist::display(const Mat4<float>& projection)
{
    glUseProgram(shaderProgram.program);
    glUniform3f(fixedColorLocation, colour.v[0], colour.v[1], colour.v[2]);

    updateTransform(projection);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[1]);

    glDrawElements(GL_TRIANGLES, 8*3, GL_UNSIGNED_INT, 0);

    glDisableVertexAttribArray(0);
}


void afk_display(void)
{
    Mat4<float> projection = afk_core.camera.getProjection();

    glClear(GL_COLOR_BUFFER_BIT);

    afk_core.landscape->display(projection);
    afk_core.testObject->display(projection);
    afk_core.protagonist->display(projection);

    glFlush();

    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR)
    {
        /* TODO Non-fatal errors? (How should I handle out-of-memory?) */
        std::ostringstream ss;
        ss << "AFK: Got GL error: " << gluErrorString(glErr);
        throw AFK_Exception(ss.str());
    }

    glutSwapBuffers();
}

void afk_reshape(int width, int height)
{
    glViewport(0, 0, width, height);
    afk_core.camera.setWindowDimensions(width, height);
}

