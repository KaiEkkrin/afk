/* AFK (c) Alex Holloway 2013 */

#include "display.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw.hpp"

AFK_EntityDisplayUnit::AFK_EntityDisplayUnit(
    const Mat4<float>& _transform,
    const Vec2<float>& _jigsawPieceST):
        transform(_transform), jigsawPieceST(_jigsawPieceST)
{
}

AFK_EntityDisplayQueue::AFK_EntityDisplayQueue():
    buf(0),
    jigsawPiecePitchLocation(0),
    jigsawDispTexSamplerLocation(0),
    jigsawColourTexSamplerLocation(0),
    jigsawNormalTexSamplerLocation(0),
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

void AFK_EntityDisplayQueue::draw(AFK_ShaderProgram *shaderProgram, AFK_Jigsaw *jigsaw, const AFK_3DEdgeShapeBase *baseShape, const AFK_ShapeSizes& sSizes)
{
    unsigned int instanceCount = queue.size();
    if (instanceCount == 0) return;

    if (!displayTBOSamplerLocation)
    {
        jigsawPiecePitchLocation = glGetUniformLocation(shaderProgram->program, "JigsawPiecePitch");
        jigsawDispTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawDispTex");
        jigsawColourTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawColourTex");
        jigsawNormalTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawNormalTex");
        displayTBOSamplerLocation = glGetUniformLocation(shaderProgram->program, "DisplayTBO");
    }

    /* Jigsaw initialisation. */
    Vec2<float> jigsawPiecePitchST = jigsaw->getPiecePitchST();

    /* Subtlety: that's a piece pitch for the big 3x2 pieces that
     * correspond to each cube.  However, 0-1 texture co-ordinates in
     * the base shape are for each *face*, so I need to divide the
     * pitch down:
     */
    jigsawPiecePitchST = afk_vec2<float>(
        jigsawPiecePitchST.v[0] / 3.0f,
        jigsawPiecePitchST.v[1] / 2.0f);
    glUniform2fv(jigsawPiecePitchLocation, 1, &jigsawPiecePitchST.v[0]);

    glActiveTexture(GL_TEXTURE0);
    jigsaw->bindTexture(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform1i(jigsawDispTexSamplerLocation, 0);

    glActiveTexture(GL_TEXTURE1);
    jigsaw->bindTexture(1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(jigsawColourTexSamplerLocation, 1);

    glActiveTexture(GL_TEXTURE2);
    jigsaw->bindTexture(2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(jigsawNormalTexSamplerLocation, 2);
    
    /* Set up the entity display texbuf. */
    glActiveTexture(GL_TEXTURE3);
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
    glUniform1i(displayTBOSamplerLocation, 3);

#if AFK_GL_DEBUG
    shaderProgram->Validate();
#endif

    baseShape->draw(instanceCount);
}

bool AFK_EntityDisplayQueue::empty(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return queue.empty();
}

void AFK_EntityDisplayQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    queue.clear();
}

