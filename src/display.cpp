/* AFK (c) Alex Holloway 2013 */

#include <math.h>

#include <vector>

#include <GL/glew.h>
#include <GL/glut.h>

#include "camera.h"
#include "display.h"
#include "object.h"
#include "state.h"


void AFK_DisplayedObject::updateTransform(const Mat4<float>& projection)
{
    Mat4<float> objectTransform = projection * object.getTransformation();
    glUniformMatrix4fv(transformLocation, 1, GL_TRUE, &objectTransform.m[0][0]);
}

void AFK_DisplayedTestObject::init(void)
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
}

void AFK_DisplayedTestObject::display(const Mat4<float>& projection)
{
    glUseProgram(shaderProgram.program);
    glUniform3f(fixedColorLocation, 1.0f, 0.0f, 0.0f);

    updateTransform(projection);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(0);
}

void AFK_DisplayedLandscapeObject::init(void)
{
    /* Link up the shader program I want. */
    shaderProgram << "basic_fragment" << "basic_vertex";
    shaderProgram.Link();

    transformLocation = glGetUniformLocation(shaderProgram.program, "transform");
    fixedColorLocation = glGetUniformLocation(shaderProgram.program, "fixedColor");

    /* This one will just be a massive field of points for now,
     * so that I can visualise the flight nicely, etc. */
    float *rawVertices = NULL;
    const int points_x = 100;
    const int points_z = 100;
    const float point_separation = 1.0f;
    vertCount = points_x * points_z;
    int rawVerticesSize = sizeof(float) * vertCount * 3;
    rawVertices = (float *)malloc(rawVerticesSize);
    for (int i = 0; i < points_x; ++i)
    {
        for (int j = 0; j < points_z; ++j)
        {
            float *x = rawVertices + 300 * i + 3 * j;
            float *y = rawVertices + 300 * i + 3 * j + 1;
            float *z = rawVertices + 300 * i + 3 * j + 2;

            *x = (float)(i - points_x / 2) * point_separation;
            *y = -1.0f;
            *z = (float)(j - points_z / 2) * point_separation;
        }
    }

    glGenBuffers(1, &vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glBufferData(GL_ARRAY_BUFFER, rawVerticesSize, rawVertices, GL_STATIC_DRAW);
}

void AFK_DisplayedLandscapeObject::display(const Mat4<float>& projection)
{
    glUseProgram(shaderProgram.program);
    glUniform3f(fixedColorLocation, 0.0f, 0.7f, 0.0f);

    updateTransform(projection);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_POINTS, 0, vertCount);

    glDisableVertexAttribArray(0);
}

void AFK_DisplayedProtagonist::init(void)
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
}

void AFK_DisplayedProtagonist::display(const Mat4<float>& projection)
{
    glUseProgram(shaderProgram.program);
    glUniform3f(fixedColorLocation, 0.2f, 1.0f, 1.0f);

    updateTransform(projection);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[1]);

    glDrawElements(GL_TRIANGLES, 8*3, GL_UNSIGNED_INT, 0);

    glDisableVertexAttribArray(0);
}


/* For now, I shall have a little static list of objects
 * here.
 * TODO: A proper one of these, in an AFK in which objects are
 * created and destroyed, really should probably use auto_ptr /
 * unique_ptr or some similar reference counting mechanism :/ */

static std::vector<AFK_DisplayedObject *> dos;

static AFK_DisplayedTestObject *dTO;
static AFK_DisplayedLandscapeObject *dLO;
static AFK_DisplayedProtagonist *dP;

void afk_displayInit(void)
{
    dTO = new AFK_DisplayedTestObject();
    dLO = new AFK_DisplayedLandscapeObject();
    dP = new AFK_DisplayedProtagonist();

    dTO->init();
    dLO->init();
    dP->init();

    dos.push_back(dTO);
    dos.push_back(dLO);
    dos.push_back(dP);

    afk_state.protagonist = &dP->object;
}

void afk_display(void)
{
    Mat4<float> projection = afk_state.camera.getProjection();

    glClear(GL_COLOR_BUFFER_BIT);

    for (std::vector<AFK_DisplayedObject *>::iterator do_it = dos.begin(); do_it != dos.end(); ++do_it)
        (*do_it)->display(projection);

    glFlush();
    glutSwapBuffers();
}

void afk_reshape(int width, int height)
{
    glViewport(0, 0, width, height);
    afk_state.camera.setWindowDimensions(width, height);
}

void afk_nextFrame(void)
{
    /* Oscillate my test object about in a silly way */
    dTO->object.adjustAttitude(AXIS_ROLL, 0.02f); 
    dTO->object.displace(AXIS_YAW, 0.06f);
}

