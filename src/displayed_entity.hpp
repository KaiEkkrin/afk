/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DISPLAYED_ENTITY_H_
#define _AFK_DISPLAYED_ENTITY_H_

#include "afk.hpp"

#include "def.hpp"
#include "entity.hpp"
#include "light.hpp"
#include "object.hpp"
#include "shader.hpp"

/* Enables the rendering of an entity. */
class AFK_DisplayedEntity
{
protected:
    /* I don't own this, the Entity does. */
    AFK_EntityGeometry **geometry;

    /* Likewise. */
    const AFK_Object *obj;

public:
    /* TODO I'm going to want to include the light list here,
     * I suspect.
     */
    AFK_DisplayedEntity(AFK_EntityGeometry **_geometry, const AFK_Object *_obj);

    /* Makes the GL render calls to display this object.
     * The entity shader program and the world light are
     * supplied here.
     */
    void display(
        AFK_ShaderProgram *shaderProgram,
        AFK_ShaderLight *shaderLight,
        const struct AFK_Light& globalLight,
        GLuint worldTransformLocation,
        GLuint clipTransformLocation,
        const Mat4<float>& projection);
};

#endif /* _AFK_DISPLAYED_ENTITY_H_ */

