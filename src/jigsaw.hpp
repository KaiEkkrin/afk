/* AFK
 * Copyright (C) 2013, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#ifndef _AFK_JIGSAW_H_
#define _AFK_JIGSAW_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/type_traits.hpp>

#include "computer.hpp"
#include "data/frame.hpp"
#include "def.hpp"
#include "jigsaw_cuboid.hpp"
#include "jigsaw_image.hpp"
#include "jigsaw_map.hpp"


/* This module encapsulates the idea of having a large, "heapified"
 * texture (collection of textures in fact) that I feed to the shaders
 * and access indirectly via another texture describing which offsets
 * to use.
 *
 * Whilst it might notionally be lovely to use the OpenGL mip-mapping
 * features here, that doesn't really seem to play well with an
 * infinite landscape and the juggled caching model that I'm using.
 * So for now, this will be a flat texture with no mip levels.
 */

/* In order to support both 2D and 3D textures, I'm changing Jigsaw so
 * that it is inherently 3D.  2D textures will be supported by setting
 * the `w' co-ordinate (`slice') to 0 at all times, and having a 3rd
 * jigsaw size dimension of 1.
 */

/* This token represents which "piece" of the jigsaw an object might
 * be associated with.
 */
class AFK_JigsawPiece
{
public:
    /* Co-ordinates within the jigsaw, in piece units */
    int u;
    int v;
    int w;

    /* Which jigsaw puzzle. */
    int puzzle;

    /* This constructor makes the "null" jigsaw piece, which isn't in
     * any puzzle.  Compare against this to decide if a piece has
     * been assigned or not.
     */
    AFK_JigsawPiece();

    AFK_JigsawPiece(int _u, int _v, int _w, int _puzzle);
    AFK_JigsawPiece(const Vec3<int>& _piece, int _puzzle);

    operator bool() const;

    bool operator==(const AFK_JigsawPiece& other) const;
    bool operator!=(const AFK_JigsawPiece& other) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece);
};

size_t hash_value(const AFK_JigsawPiece& jigsawPiece);

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
    const Vec3<int> jigsawSize; /* number of pieces in each dimension */
    const std::vector<AFK_JigsawImageDescriptor>& desc;

    /* The images are made in time for the first render; a new
     * jigsaw might be created by a worker thread that doesn't have
     * GL / CL contexts.
     */
    std::vector<AFK_JigsawImage*> images;

    /* The Map tracks piece assignment. */
    AFK_JigsawMap map;

    /* I shall write the push list for the frame here. */
    std::vector<AFK_JigsawCuboid> pushList;
    bool havePushList;

public:
    AFK_Jigsaw(
        AFK_Computer *_computer,
        const Vec3<int>& _jigsawSize,
        const std::vector<AFK_JigsawImageDescriptor>& _desc);
    virtual ~AFK_Jigsaw();

    /* Acquires a new piece.
     * If this function returns false, this jigsaw has run out of
     * space and the JigsawCollection needs to use a different one.
     */
    bool grab(Vec3<int>& o_uvw, AFK_Frame *o_timestamp);

    /* Returns the time your piece was last swept.
     * If you retrieved it at that time or later, you're OK and you
     * can hang on to it.
     */
    AFK_Frame getTimestamp(const AFK_JigsawPiece& piece) const;

    /* TODO change this to a size_t ? */
    unsigned int getTexCount(void) const { return static_cast<unsigned int>(desc.size()); }

    /* Returns the (s, t) texture co-ordinates for a given piece
     * within the jigsaw.  These will be in the range (0, 1).
     */
    Vec2<float> getTexCoordST(const AFK_JigsawPiece& piece) const;

    /* Returns the (s, t, r) texture co-ordinates for a given piece
     * within the jigsaw.  These will be in the range (0, 1).
     */
    Vec3<float> getTexCoordSTR(const AFK_JigsawPiece& piece) const;

    /* Returns the (s, t) dimensions of one piece within the jigsaw. */
    Vec2<float> getPiecePitchST(void) const;

    /* Returns the (s, t, r) dimensions of one piece within the jigsaw. */
    Vec3<float> getPiecePitchSTR(void) const;

    /* --- These next functions should always be called from the render thread --- */

    /* Ensures this jigsaw has images set up.  Call the first time
     * you draw.
     */
    void setupImages(AFK_Computer *computer);

    /* Get fake 3D info for the jigsaw in a format suitable for
     * sending to the CL.  Return -1s in the case of no fake 3D.
     */
    Vec2<int> getFake3D_size(unsigned int tex) const;
    int getFake3D_mult(unsigned int tex) const;

    /* TODO: For the next two functions -- I'm now going around re-
     * acquiring jigsaws for read after having first acquired them
     * for write, and all sorts.  In which case I don't need to re-blit
     * all the same data.  Consider optimising so that data is only
     * copied over to the GL after a write acquire is released?
     */

    /* Acquires an image for the CL.
     * Fills out the given compute dependency as required.
     */
    cl_mem acquireForCl(unsigned int tex, AFK_ComputeDependency& o_dep);

    /* Releases an image from the CL,
     * waiting for the given dependency.
     */
    void releaseFromCl(unsigned int tex, const AFK_ComputeDependency& dep);

    /* Binds an image to the GL as a texture.
     * Expects you to have set glActiveTexture() appropriately first!
     */
    void bindTexture(unsigned int tex);

    /* Signals to the jigsaw that the update and draw phases are
     * finished, and to prepare for the next frame.
     */
    void flip(const AFK_Frame& currentFrame);

    void printStats(std::ostream& os, const std::string& prefix);
};

#endif /* _AFK_JIGSAW_H_ */

