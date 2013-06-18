/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHADER_H_
#define _AFK_SHADER_H_

#include <GL/gl.h>

/* TODO Sort out detecting support for required OpenGL features / extensions ... */

/* For loading the shader set.
 * Returns 1 on success, 0 on error */
int afk_compileShaders(const char *shadersDir, GLuint *o_shaderProgram);

#endif /* _AFK_SHADER_H_ */

