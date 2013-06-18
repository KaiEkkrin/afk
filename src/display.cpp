/* AFK (c) Alex Holloway 2013 */

#include "display.h"

#include <GL/glew.h>
#include <GL/glut.h>

/* TODO Rather naughty: for now I'm going to use some statics
 * to describe the stuff I'm drawing. */
static GLuint vObj;

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
}

void afk_display(void)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vObj);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(0);

    glFlush();
    glutSwapBuffers();
}

void afk_reshape(int x, int y)
{
    /* TODO. */
}

