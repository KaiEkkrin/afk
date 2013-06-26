/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_EVENT_H_
#define _AFK_EVENT_H_

/* Event-handling functions for GLUT. */
void afk_keyboard(unsigned char key, int x, int y);
void afk_keyboardUp(unsigned char key, int x, int y);
void afk_mouse(int button, int state, int x, int y);
void afk_motion(int x, int y);
void afk_entry(int state);

#endif /* _AFK_EVENT_H_ */

