/* AFK (c) Alex Holloway 2013 */

#include "display.hpp"
#include "jigsaw.hpp"
#include "landscape_display_queue.hpp"
#include "landscape_tile.hpp"

AFK_LandscapeDisplayUnit::AFK_LandscapeDisplayUnit() {}

AFK_LandscapeDisplayUnit::AFK_LandscapeDisplayUnit(
    const Vec4<float>& _cellCoord,
    const Vec2<float>& _jigsawPieceST,
    float _yBoundLower,
    float _yBoundUpper):
        cellCoord(_cellCoord), jigsawPieceST(_jigsawPieceST), yBoundLower(_yBoundLower), yBoundUpper(_yBoundUpper)
{
}

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeDisplayUnit& unit)
{
    os << "(LDU: ";
    os << "cellCoord=" << std::dec << unit.cellCoord;
    os << ", jigsawPieceST=" << unit.jigsawPieceST;
    os << ", yBoundLower=" << unit.yBoundLower;
    os << ", yBoundUpper=" << unit.yBoundUpper;
    os << ")";
    return os;
}


AFK_LandscapeDisplayQueue::AFK_LandscapeDisplayQueue():
    buf(0),
    jigsawPiecePitchLocation(0),
    jigsawYDispTexSamplerLocation(0),
    jigsawColourTexSamplerLocation(0),
    jigsawNormalTexSamplerLocation(0),
    displayTBOSamplerLocation(0)
{
}

AFK_LandscapeDisplayQueue::~AFK_LandscapeDisplayQueue()
{
    if (buf) glDeleteBuffers(1, &buf);
}

void AFK_LandscapeDisplayQueue::add(const AFK_LandscapeDisplayUnit& _unit, const AFK_LandscapeTile *landscapeTile)
{
    boost::unique_lock<boost::mutex> lock(mut);

    queue.push_back(_unit);
    landscapeTiles.push_back(landscapeTile);
}

#define COPY_TO_GL_CULLING 1

void AFK_LandscapeDisplayQueue::draw(AFK_ShaderProgram *shaderProgram, AFK_Jigsaw* jigsaw, const AFK_LandscapeSizes& lSizes)
{
    /* First, check there's anything to draw at all ... */
#if COPY_TO_GL_CULLING
    for (unsigned int i = 0; i < queue.size(); ++i)
    {
        if (landscapeTiles[i]->realCellWithinYBounds(queue[i].cellCoord))
            culledQueue.push_back(queue[i]);
    }

    unsigned int instanceCount = culledQueue.size();
#else
    unsigned int instanceCount = queue.size();
#endif
    if (instanceCount == 0) return;

    /* Check I've initialised the various locations in the
     * shader program that I need
     */
    if (!jigsawPiecePitchLocation)
    {
        jigsawPiecePitchLocation = glGetUniformLocation(shaderProgram->program, "JigsawPiecePitch");
        jigsawYDispTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawYDispTex");
        jigsawColourTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawColourTex");
        jigsawNormalTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawNormalTex");
        displayTBOSamplerLocation = glGetUniformLocation(shaderProgram->program, "DisplayTBO");
    }

    /* Fill out ye olde uniform variable with the jigsaw
     * piece pitch.
     */
    Vec2<float> jigsawPiecePitchST = jigsaw->getPiecePitchST();
    glUniform2fv(jigsawPiecePitchLocation, 1, &jigsawPiecePitchST.v[0]);

    /* The first texture is the jigsaw Y-displacement */
    glActiveTexture(GL_TEXTURE0);
    jigsaw->bindTexture(0);
    /* Interestingly, if I use nearest-neighbour sampling here I get
     * artifacts that suggest it's rounding across to the wrong sample
     * on one side of the tiles.  If I use linear sampling
     * the artifacts go away...
     */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, /* GL_NEAREST */ GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, /* GL_NEAREST */ GL_LINEAR);
    glUniform1i(jigsawYDispTexSamplerLocation, 0);

    /* The second texture is the jigsaw colour */
    glActiveTexture(GL_TEXTURE1);
    jigsaw->bindTexture(1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(jigsawColourTexSamplerLocation, 1);

    /* The third texture is the jigsaw normal */
    glActiveTexture(GL_TEXTURE2);
    jigsaw->bindTexture(2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(jigsawNormalTexSamplerLocation, 2);

    /* The fourth texture is the landscape display texbuf,
     * which explains to the vertex shader which tile it's
     * drawing and where in the jigsaw to look.
     */
    glActiveTexture(GL_TEXTURE3);
    if (!buf) glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(
        GL_TEXTURE_BUFFER,
#if COPY_TO_GL_CULLING
        culledQueue.size() * sizeof(AFK_LandscapeDisplayUnit),
        &culledQueue[0],
#else
        queue.size() * sizeof(AFK_LandscapeDisplayUnit),
        &queue[0],
#endif
        GL_STREAM_DRAW);
    glTexBuffer(
        GL_TEXTURE_BUFFER,
        GL_RGBA32F,
        buf);
    AFK_GLCHK("landscape display queue texBuffer")
    glUniform1i(displayTBOSamplerLocation, 3);

#if AFK_GL_DEBUG
    shaderProgram->Validate();
#endif
    glDrawElementsInstanced(GL_TRIANGLES, lSizes.iCount * 3, GL_UNSIGNED_SHORT, 0, instanceCount);
    AFK_GLCHK("landscape drawElementsInstanced")
}

bool AFK_LandscapeDisplayQueue::empty(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return queue.empty();
}

void AFK_LandscapeDisplayQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    queue.clear();
    landscapeTiles.clear();
    culledQueue.clear();
}

