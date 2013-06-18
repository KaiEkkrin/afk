/* AFK (c) Alex Holloway 2013 */

#include <GL/glut.h>

#include "display.h"
#include "event.h"

/* AFK main and related stuff. */

static void afk_idle(void)
{
    /* TODO. */
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv); // TODO check what exactly this does (want to use my own cmdline parser! :p )
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

    /* TODO Full screen by default?  Pull configuration from file / from cmdline? */
    glutInitWindowSize(1920, 1080); 
    glutCreateWindow("AFK");
    
    /* These functions from the `display' module. */
    glutDisplayFunc(afk_display);
    glutReshapeFunc(afk_reshape);

    /* These functions from the `event' module.
     * TODO Mouse support. */
    glutKeyboardFunc(afk_keyboard);
    glutKeyboardUpFunc(afk_keyboardUp);

    glutIdleFunc(afk_idle);
    glutMainLoop();
    return 0;
}

