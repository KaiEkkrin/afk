/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LIGHT_H_
#define _AFK_LIGHT_H_

#include "afk.hpp"

#include "def.hpp"

/* This is the structure that gets passed into the shader.
 * Basic description of a light.
 */
struct AFK_Light
{
    Vec3<float> colour;
    Vec3<float> direction;
    float       ambient;    
    float       diffuse;
};

/* Wraps the light as it exists in the shader and allows
 * field assignment.
 */
class AFK_ShaderLight
{
protected:
    GLuint      colourField;
    GLuint      directionField;
    GLuint      ambientField;
    GLuint      diffuseField;

public:
    AFK_ShaderLight(GLuint program);

    void setupLight(const struct AFK_Light& light);
};

/* TODO: At some point I'm going to want to have an exciting
 * kind of "light vector", with the sun at the front (the
 * only light I have right now), and cell-wise lights after
 * it!
 * For now, though, I'll just have a single one of these.
 */

#endif /* _AFK_LIGHT_H_ */

