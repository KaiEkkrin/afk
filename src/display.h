/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DISPLAY_H_
#define _AFK_DISPLAY_H_

void afk_displayInit(void);

/* Display and reshape functions for GLUT. */
void afk_display(void);
void afk_reshape(int x, int y);

/* Calculates the intended contents of the next frame. */
void afk_nextFrame(void);

#endif /* _AFK_DISPLAY_H_ */

