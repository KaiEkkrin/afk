
#ifndef _AFK_JIGSAW_H_
#define _AFK_JIGSAW_H_

#include "afk.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

#include <boost/lockfree/queue.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "computer.hpp"
#include "def.hpp"

/* This module encapsulates the idea of having a large, "heapified"
 * texture (collection of textures in fact) that I feed to the shaders
 * and access indirectly via another texture describing which offsets
 * to use.
 *
 * Whilst it might notionally be lovely to use the OpenGL mip-mapping
 * features here, that doesn't really seem to play well with an
 * infinite landscape and the juggled caching model that I'm using.
 * So for now, this will be a flat 2D texture with no mip levels.
 */

/* This enumeration describes the jigsaw's texture format.  I'll
 * need to add more here as I support more formats.
 */
enum AFK_JigsawFormat
{
    AFK_JIGSAW_FLOAT32,
    AFK_JIGSAW_555A1,
    AFK_JIGSAW_101010A2,
    AFK_JIGSAW_4FLOAT8_UNORM,
    AFK_JIGSAW_4FLOAT8_SNORM,
    AFK_JIGSAW_4FLOAT32
};

class AFK_JigsawFormatDescriptor
{
public:
    GLint glInternalFormat;
    GLenum glFormat;
    GLenum glDataType;
    cl_image_format clFormat;
    size_t texelSize;

    AFK_JigsawFormatDescriptor(enum AFK_JigsawFormat);
    AFK_JigsawFormatDescriptor(const AFK_JigsawFormatDescriptor& _fd);
};

/* This token represents which "piece" of the jigsaw an object might
 * be associated with.
 */
class AFK_JigsawPiece
{
public:
    Vec2<int> piece;   /* u, v of the jigsaw piece within the jigsaw, in piece units */
    int puzzle;  /* which jigsaw buffer */

    /* This constructor makes the "null" jigsaw piece, which isn't in
     * any puzzle.  Compare against this to decide if a piece has
     * been assigned or not.
     */
    AFK_JigsawPiece();

    AFK_JigsawPiece(const Vec2<int>& _piece, int _puzzle);

    bool operator==(const AFK_JigsawPiece& other) const;
    bool operator!=(const AFK_JigsawPiece& other) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece);
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece);

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_JigsawPiece>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_JigsawPiece>::value));


/* This encapsulates a single jigsawed texture, which may contain
 * multiple textures, one for each format in the list: they will all
 * have identical layouts and be referenced with the same jigsaw
 * pieces.
 */
class AFK_Jigsaw
{
protected:
    GLuint *glTex;
    cl_mem *clTex;
    const AFK_JigsawFormatDescriptor *format;
    const unsigned int texCount;

    const Vec2<int> pieceSize;
    const Vec2<int> jigsawSize; /* number of pieces horizontally and vertically */
    const bool clGlSharing;

    /* If clGlSharing is disabled, this is the list of pieces that
     * have been changed in the CL and need to be pushed to the GL,
     * along with the change data that I've read out (in the same
     * order).
     */
    std::vector<Vec2<int> > changedPieces;
    std::vector<unsigned char> *changes; /* one per texture */

public:
    AFK_Jigsaw(
        cl_context ctxt,
        const Vec2<int>& _pieceSize,
        const Vec2<int>& _jigsawSize,
        const AFK_JigsawFormatDescriptor *_format,
        unsigned int _texCount,
        bool _clGlSharing);
    virtual ~AFK_Jigsaw();

    unsigned int getTexCount(void) const;

    /* Returns the (s, t) texture co-ordinates for a given piece
     * within the jigsaw.  These will be in the range (0, 1).
     */
    Vec2<float> getTexCoordST(const AFK_JigsawPiece& piece) const;

    /* Returns the (s, t) dimensions of one piece within the jigsaw. */
    Vec2<float> getPiecePitchST(void) const;

    /* Acquires the buffers for the CL. */
    cl_mem *acquireForCl(cl_context ctxt, cl_command_queue q);

    /* Tells the jigsaw you've changed a piece (if we're not doing
     * buffer sharing, that means we will need to sync.)
     * TODO I think I'm going to want to end up supplying a list...
     */
    void pieceChanged(const Vec2<int>& piece);

    /* Releases the buffer from the CL. */
    void releaseFromCl(cl_command_queue q);

    /* If clGlSharing is disabled, this function debugs the changed
     * pieces as read back from the CL, interpreting them as
     * templated.
     */
    template<typename TexelType>
    void debugReadChanges(
        std::vector<Vec2<int> >& _changedPieces,
        std::vector<TexelType>& _changes,
        unsigned int tex)
    {
        if (sizeof(TexelType) != format[tex].texelSize)
        {
            std::ostringstream ss;
            ss << "jigsaw debugReadChanges: have texelSize " << std::dec << format[tex].texelSize << " and sizeof(TexelType) " << sizeof(TexelType);
            throw AFK_Exception(ss.str());
        }

        _changedPieces.resize(changedPieces.size());
        _changes.resize(changes[tex].size() / sizeof(TexelType));
        std::copy(changedPieces.begin(), changedPieces.end(), _changedPieces.begin());

        size_t pieceSizeInTexels = pieceSize.v[0] * pieceSize.v[1];
        size_t pieceSizeInBytes = format[tex].texelSize * pieceSizeInTexels;
        memcpy(&_changes[0], &changes[tex][0], pieceSizeInBytes * changedPieces.size());
    }

    /* This does the obvious opposite.
     * The caller is responsible for calling it once for each texture,
     * otherwise things will get out of sync and pigeons will smoke
     * cigars and dragons will land on your rooftop and light it on
     * fire.
     */
    template<typename TexelType>
    void debugWriteChanges(
        const std::vector<Vec2<int> >& _changedPieces,
        const std::vector<TexelType>& _changes,
        unsigned int tex)
    {
        if (sizeof(TexelType) != format[tex].texelSize)
        {
            std::ostringstream ss;
            ss << "jigsaw debugWriteChanges: have texelSize " << std::dec << format[tex].texelSize << " and sizeof(TexelType) " << sizeof(TexelType);
            throw AFK_Exception(ss.str());
        }

        changedPieces.resize(changedPieces.size());
        changes[tex].resize(changes[tex].size() * sizeof(TexelType));
        std::copy(_changedPieces.begin(), _changedPieces.end(), changedPieces.begin());

        size_t pieceSizeInTexels = pieceSize.v[0] * pieceSize.v[1];
        size_t pieceSizeInBytes = format[tex].texelSize * pieceSizeInTexels;
        memcpy(&changes[tex][0], &_changes[tex][0], pieceSizeInBytes * changedPieces.size());
    }

    /* Binds a buffer to the GL as a texture.
     * Call once for each texture, having set glActiveTexture()
     * appropriately each time.
     */
    void bindTexture(unsigned int tex);
};

/* This encapsulates a collection of jigsawed textures, which are used
 * to give out pieces of the same size and usage.
 * You may get a piece in any of the puzzles.
 */
class AFK_JigsawCollection
{
protected:
    std::vector<AFK_JigsawFormatDescriptor> format;
    const unsigned int texCount;

    const Vec2<int> pieceSize;
    Vec2<int> jigsawSize;
    int pieceCount;
    const bool clGlSharing;

    std::vector<AFK_Jigsaw*> puzzles;
    boost::mutex mut;

    /* The pieces that are free to be given out. */
    boost::lockfree::queue<AFK_JigsawPiece> box;

public:
    AFK_JigsawCollection(
        cl_context ctxt,
        const Vec2<int>& _pieceSize,
        int _pieceCount,
        enum AFK_JigsawFormat *texFormat,
        unsigned int _texCount,
        const AFK_ClDeviceProperties& _clDeviceProps,
        bool _clGlSharing);
    virtual ~AFK_JigsawCollection();

    int getPieceCount(void) const;

    /* Gives you a piece.  This will usually be quick,
     * but it may stall if we need to add a new jigsaw
     * to the collection.
     */
    AFK_JigsawPiece grab(void);

    /* Returns a piece to the box. */
    void replace(AFK_JigsawPiece piece);

    /* Gets you the puzzle that matches a particular piece. */
    AFK_Jigsaw *getPuzzle(const AFK_JigsawPiece& piece) const;

    /* Gets you a numbered puzzle. */
    AFK_Jigsaw *getPuzzle(int puzzle) const;
};


#endif /* _AFK_JIGSAW_H_ */

