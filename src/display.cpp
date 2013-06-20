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

/* TODO To test, splicing in some example code from the Web.
 * Remove this and replace with real stuff :P */

void afk_displayInit(void)
{
    /* Setup the camera with respect to the window. */
    afk_state.camera.setWindowDimensions(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));

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
    Mat4<float> projection = afk_state.camera.getProjection();

    /* Some day, I'll have more than one object here. */
    /* TODO I'm pretty sure my projection or something to do with my
     * camera is wrong but it's very hard to tell with a single,
     * untextured, constantly moving triangle.  Replace this with a
     * proper nice lit shape (so I CAN tell) and try again. */
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
    afk_state.camera.setWindowDimensions(width, height);
    glViewport(0, 0, width, height);
}

void afk_nextFrame(void)
{
    /* Oscillate my test object about in a silly way */
    testObject.adjustAttitude(AXIS_YAW, 0.02f); 
    testObject.displace(AXIS_PITCH, 0.02f);
}

