/* AFK (c) Alex Holloway 2013 */

#include <math.h>

#include <GL/glew.h>
#include <GL/glut.h>

#include "def.h"
#include "display.h"
#include "state.h"

/* TODO Rather naughty: for now I'm going to use some statics
 * to describe the stuff I'm drawing. */
static GLuint vObj;

/* TODO REMOVEME For the uniform variable test. */
static GLuint transformLocation;
static float translate;
static float rotate;

/* TODO To test, splicing in some example code from the Web.
 * Remove this and replace with real stuff :P */

void afk_displayInit(void)
{
    float vertices[] = {
        -1.0f,  -1.0f,  0.0f,
        1.0f,   -1.0f,  0.0f,
        0.0f,   1.0f,   0.0f
    };

    glGenBuffers(1, &vObj);
    glBindBuffer(GL_ARRAY_BUFFER, vObj);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    transformLocation = glGetUniformLocation(afk_state.shaderProgram, "transform");
    translate = 0.0f;
    rotate = 0.0f;
}

void afk_display(void)
{
    /* TODO compute the concatenation of these.  With OpenCL.  Because,
     * after all, for each scene I'm going to be updating the positions
     * of a large number of moving objects. */
    /*
    Mat4<float> rotateMatrix(
        cosf(rotate),   -sinf(rotate),  0.0f,   0.0f,
        sinf(rotate),   cosf(rotate),   0.0f,   0.0f,
        0.0f,           0.0f,           1.0f,   0.0f,
        0.0f,           0.0f,           0.0f,   1.0f
    );
    */
    Mat4<float> rotateMatrix(
        1.0f,           0.0f,           0.0f,           0.0f,
        0.0f,           cosf(rotate),   sinf(rotate),   0.0f,
        0.0f,           -sinf(rotate),  cosf(rotate),   0.0f,
        0.0f,           0.0f,           0.0f,           1.0f
    );

    /* TODO Using a static camera position.  Sort out. */
    Mat4<float> translateMatrix(
        1.0f,   0.0f,   0.0f,   0.0f,
        0.0f,   1.0f,   0.0f,   0.0f,
        0.0f,   0.0f,   1.0f,   5.0f,
        0.0f,   0.0f,   0.0f,   1.0f
    );

    /* Magic perspective projection. */
    int windowWidth = glutGet(GLUT_WINDOW_WIDTH);
    int windowHeight = glutGet(GLUT_WINDOW_HEIGHT);
    float ar = ((float)windowWidth) / ((float)windowHeight);
    float zNear = afk_state.config->zNear;
    float zFar = afk_state.config->zFar;
    float zRange = zNear - zFar;
    float tanHalfFov = tanf((afk_state.config->fov / 2.0f) * M_PI / 180.0f);

    Mat4<float> projectMatrix(
        1.0f / (tanHalfFov * ar),   0.0f,                   0.0f,                       0.0f,
        0.0f,                       1.0f / tanHalfFov,      0.0f,                       0.0f,
        0.0f,                       0.0f,                   (-zNear - zFar) / zRange,   2.0f * zFar * zNear / zRange,
        0.0f,                       0.0f,                   1.0f,                       0.0f
    );

    Mat4<float> transformMatrix = projectMatrix * translateMatrix * rotateMatrix;

    glClear(GL_COLOR_BUFFER_BIT);

    glUniformMatrix4fv(transformLocation, 1, GL_TRUE, &transformMatrix.m[0][0]);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vObj);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(0);

    glFlush();
    glutSwapBuffers();
}

void afk_nextFrame(void)
{
    translate += 0.01f;
    rotate += 0.02f;
}

