/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "displayed_entity.hpp"
#include "exception.hpp"

AFK_DisplayedEntity::AFK_DisplayedEntity(
    AFK_EntityGeometry **_geometry,
    const AFK_Object *_obj):
        geometry(_geometry), obj(_obj)
{
}

void AFK_DisplayedEntity::initGL(void)
{
    if (!(*geometry)) throw new AFK_Exception("AFK_DisplayedEntity: Got NULL geometry");
    (*geometry)->vs.initGL(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    (*geometry)->is.initGL(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
}

void AFK_DisplayedEntity::display(
    AFK_ShaderProgram *shaderProgram,
    AFK_ShaderLight *shaderLight,
    const struct AFK_Light& globalLight,
    GLuint worldTransformLocation,
    GLuint clipTransformLocation,
    const Mat4<float>& projection)
{
    glUseProgram(shaderProgram->program);
    shaderLight->setupLight(globalLight);

    Mat4<float> worldTransformation = obj->getTransformation();
    glUniformMatrix4fv(worldTransformLocation, 1, GL_TRUE, &worldTransformation.m[0][0]);

    Mat4<float> clipTransformation = projection * worldTransformation;
    glUniformMatrix4fv(clipTransformLocation, 1, GL_TRUE, &clipTransformation.m[0][0]);
    
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, (*geometry)->vs.buf);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(sizeof(Vec3<float>)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(2 * sizeof(Vec3<float>)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (*geometry)->is.buf);

    glDrawElements(GL_TRIANGLES, (*geometry)->is.t.size() * 3, GL_UNSIGNED_INT, 0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}

