/* AFK (c) Alex Holloway 2013 */

#include "display.hpp"
#include "entity_display_queue.hpp"

AFK_EntityDisplayUnit::AFK_EntityDisplayUnit(
    const Mat4<float>& _transform,
    const Vec2<float>& _jigsawPieceST):
        transform(_transform), jigsawPieceST(_jigsawPieceST)
{
}

AFK_EntityDisplayQueue::AFK_EntityDisplayQueue():
    buf(0),
    displayTBOSamplerLocation(0)
{
}

AFK_EntityDisplayQueue::~AFK_EntityDisplayQueue()
{
    if (buf)
        glDeleteBuffers(1, &buf);
}

void AFK_EntityDisplayQueue::add(const AFK_EntityDisplayUnit& _unit)
{
    boost::unique_lock<boost::mutex> lock(mut);

    queue.push_back(_unit);
}

void AFK_EntityDisplayQueue::draw(AFK_ShaderProgram *shaderProgram, AFK_Jigsaw *jigsaw, const AFK_ShapeSizes& sSizes)
{
    unsigned int instanceCount = queue.size();
    if (instanceCount == 0) return;

    if (!displayTBOSamplerLocation)
    {
        displayTBOSamplerLocation = glGetUniformLocation(shaderProgram->program, "DisplayTBO");
    }

    /* TODO: Here, when I have it, the jigsaw initialisation will go. */

    /* Set up the entity display texbuf. */
    glActiveTexture(GL_TEXTURE0);
    if (!buf) glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(
        GL_TEXTURE_BUFFER,
        queue.size() * ENTITY_DISPLAY_UNIT_SIZE,
        &queue[0],
        GL_STREAM_DRAW);
    glTexBuffer(
        GL_TEXTURE_BUFFER,
        GL_RGBA32F,
        buf);
    AFK_GLCHK("entity display queue texBuffer")
    glUniform1i(displayTBOSamplerLocation, 0);

#if AFK_GL_DEBUG
    shaderProgram->Validate();
#endif
    glDrawElementsInstanced(GL_TRIANGLES, sSizes.iCount * 3, GL_UNSIGNED_SHORT, 0, instanceCount);
    AFK_GLCHK("entity drawElementsInstanced")
}

bool AFK_EntityDisplayQueue::empty(void)
{
    boost::unique_lock<mutex> lock(mut);

    return queue.empty();
}

void AFK_EntityDisplayQueue::clear(void)
{
    boost::unique_lock<mutex> lock(mut);

    queue.clear();
}

