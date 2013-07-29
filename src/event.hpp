/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_EVENT_H_
#define _AFK_EVENT_H_

void afk_keyboard(unsigned int key);
void afk_keyboardUp(unsigned int key);
void afk_mouse(unsigned int button);
void afk_mouseUp(unsigned int button);
void afk_motion(int x, int y);
void afk_windowReshape(unsigned int width, unsigned int height);

#endif /* _AFK_EVENT_H_ */

