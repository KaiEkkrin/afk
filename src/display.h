/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DISPLAY_H_
#define _AFK_DISPLAY_H_

void afk_displayInit(void);

/* GLUT callback functions. */
void afk_display(void);
void afk_reshape(int width, int height);

/* Calculates the intended contents of the next frame. */
void afk_nextFrame(void);

#endif /* _AFK_DISPLAY_H_ */

