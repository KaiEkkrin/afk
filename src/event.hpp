/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_EVENT_H_
#define _AFK_EVENT_H_

/* Event-handling functions for GLUT. */
void afk_entry(int state);
void afk_keyboard(unsigned char key, int x, int y);
void afk_keyboardUp(unsigned char key, int x, int y);
void afk_mouse(int button, int state, int x, int y);
void afk_motion(int x, int y);
void afk_special(int key, int x, int y);
void afk_specialUp(int key, int x, int y);

#endif /* _AFK_EVENT_H_ */

