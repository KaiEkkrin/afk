/* AFK (c) Alex Holloway 2013 */

#include "config.h"
#include "event.h"
#include "state.h"

void afk_keyboard(unsigned char key, int x, int y)
{
    enum AFK_Controls control = afk_state.config->controlMapping[key];
    afk_state.controlsEnabled |= (unsigned long long)control;
}

void afk_keyboardUp(unsigned char key, int x, int y)
{
    enum AFK_Controls control = afk_state.config->controlMapping[key];
    afk_state.controlsEnabled &= ~((unsigned long long)control);
}

