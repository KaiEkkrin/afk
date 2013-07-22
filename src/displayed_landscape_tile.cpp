/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "displayed_landscape_tile.hpp"
#include "exception.hpp"

AFK_DisplayedLandscapeTile::AFK_DisplayedLandscapeTile(
    AFK_LandscapeGeometry **_geometry,
    float _cellBoundLower,
    float _cellBoundUpper):
        geometry(_geometry), cellBoundLower(_cellBoundLower), cellBoundUpper(_cellBoundUpper)
{
}

void AFK_DisplayedLandscapeTile::initGL(void)
{
    if (!(*geometry)) throw new AFK_Exception("Got NULL geometry");
    (*geometry)->vs.initGL(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    (*geometry)->is.initGL(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
}

void AFK_DisplayedLandscapeTile::display(
    AFK_ShaderProgram *shaderProgram,
    AFK_ShaderLight *shaderLight,
    const struct AFK_Light& globalLight,
    GLuint clipTransformLocation,
    GLuint yCellMinLocation,
    GLuint yCellMaxLocation,
    const Mat4<float>& projection)
{
    glUseProgram(shaderProgram->program);
    shaderLight->setupLight(globalLight);
    glUniformMatrix4fv(clipTransformLocation, 1, GL_TRUE, &projection.m[0][0]);
    glUniform1f(yCellMinLocation, cellBoundLower);
    glUniform1f(yCellMaxLocation, cellBoundUpper);

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

