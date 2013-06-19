/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_STATE_H_
#define _AFK_STATE_H_

#include "config.h"

/* This is where I record AFK's global state.  I've got to do it like this,
 * because glut expects program state to be global (its callbacks give me
 * no way of passing a parameter.)
 */
struct AFK_State
{
    struct AFK_Config   *config;
    GLuint              shaderProgram;
};

extern struct AFK_State afk_state;

#endif /* _AFK_STATE_H_ */

