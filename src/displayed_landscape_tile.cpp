/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "displayed_landscape_tile.hpp"

AFK_DisplayedLandscapeTile::AFK_DisplayedLandscapeTile(
    boost::shared_ptr<AFK_LANDSCAPE_VERTEX_BUF> _vs,
    boost::shared_ptr<AFK_LANDSCAPE_INDEX_BUF> _is,
    float _cellBoundLower,
    float _cellBoundUpper):
        vs(_vs), is(_is), cellBoundLower(_cellBoundLower), cellBoundUpper(_cellBoundUpper)
{
}

void AFK_DisplayedLandscapeTile::initGL(void)
{
    vs->initGL(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    is->initGL(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
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

    glBindBuffer(GL_ARRAY_BUFFER, vs->buf);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(sizeof(Vec3<float>)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct AFK_VcolPhongVertex), (const GLvoid*)(2 * sizeof(Vec3<float>)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, is->buf);

    glDrawElements(GL_TRIANGLES, is->t.size() * 3, GL_UNSIGNED_INT, 0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}

