/* AFK (c) Alex Holloway 2013 */

#include <math.h>

#include <GL/glew.h>
#include <GL/glut.h>

#include "camera.h"
#include "display.h"
#include "object.h"
#include "state.h"

/* TODO Rather naughty: for now I'm going to use some statics
 * to describe the stuff I'm drawing. */
static GLuint vObj;

/* TODO REMOVEME For the uniform variable test.
 * Want to move these things somewhere.  `state'?  Not sure yet. */
static GLuint transformLocation;
static AFK_Object testObject;
static AFK_Camera camera;

/* TODO To test, splicing in some example code from the Web.
 * Remove this and replace with real stuff :P */

void afk_displayInit(void)
{
    /* Setup the camera. */
    camera.fov      = afk_state.config->fov;
    camera.zNear    = afk_state.config->zNear;
    camera.zFar     = afk_state.config->zFar;

    camera.windowWidth  = glutGet(GLUT_WINDOW_WIDTH);
    camera.windowHeight = glutGet(GLUT_WINDOW_HEIGHT);

    /* Move that camera back so I can see.
     * (The camera moves in inverse, because it's effectively
     * moving the world.) */
    camera.translate = Vec3<float>(0.0f, 0.0f, 5.0f);

    /* Setup the test object's data. */
    float vertices[] = {
        -1.0f,  -1.0f,  0.0f,
        1.0f,   -1.0f,  0.0f,
        0.0f,   1.0f,   0.0f
    };

    glGenBuffers(1, &vObj);
    glBindBuffer(GL_ARRAY_BUFFER, vObj);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    transformLocation = glGetUniformLocation(afk_state.shaderProgram, "transform");
}

void afk_display(void)
{
    Mat4<float> projection = camera.getProjection();

    /* Some day, I'll have more than one object here. */
    Mat4<float> objectTransform = projection * testObject.getTranslation() * testObject.getRotation() * testObject.getScaling();

    glClear(GL_COLOR_BUFFER_BIT);

    glUniformMatrix4fv(transformLocation, 1, GL_TRUE, &objectTransform.m[0][0]);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vObj);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(0);

    glFlush();
    glutSwapBuffers();
}

void afk_reshape(int width, int height)
{
    camera.windowWidth  = width;
    camera.windowHeight = height;
    glViewport(0, 0, width, height);
}

void afk_nextFrame(void)
{
    /* Oscillate my test object about in a silly way */
    static float transPoint = 0.0f;

    transPoint += 0.01f;
    testObject.translate.v[0] = sinf(transPoint);

    testObject.adjustAttitude(AXIS_YAW, 0.02f); 
}

