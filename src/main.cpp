/* AFK (c) Alex Holloway 2013 */

#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GL/glut.h>

#include "config.h"
#include "display.h"
#include "event.h"
#include "shader.h"

/* AFK main and related stuff. */

static void afk_idle(void)
{
    afk_display();
}

int main(int argc, char **argv)
{
    GLenum res;
    GLuint shaderProgram;
    AFK_Config *config;

    glutInit(&argc, argv); // TODO check what exactly this does
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

    /* TODO Full screen by default?  Pull configuration from file / from cmdline? */
    glutCreateWindow("AFK");
    
    /* These functions from the `display' module. */
    glutDisplayFunc(afk_display);
    glutReshapeFunc(afk_reshape);

    /* These functions from the `event' module.
     * TODO Mouse support. */
    glutKeyboardFunc(afk_keyboard);
    glutKeyboardUpFunc(afk_keyboardUp);

    glutIdleFunc(afk_idle);

    /* Local configuration. */
    config = new AFK_Config(&argc, argv);

    /* Extension detection. */
    res = glewInit();
    if (res != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialise GLEW: %s\n", glewGetErrorString(res));
        exit(1);
    }

    /* Shader setup. */
    if (!afk_compileShaders(config->shadersDir, &shaderProgram)) exit(1);

    /* Display setup. */
    afk_displayInit();

    glutMainLoop();
    return 0;
}

