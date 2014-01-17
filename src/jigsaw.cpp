/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#include "afk.hpp"

#include "jigsaw.hpp"
#include "rng/hash.hpp"


/* AFK_JigsawPiece implementation */

AFK_JigsawPiece::AFK_JigsawPiece():
    u(INT_MIN), v(INT_MIN), w(INT_MIN), puzzle(INT_MIN)
{
}

AFK_JigsawPiece::AFK_JigsawPiece(int _u, int _v, int _w, int _puzzle):
    u(_u), v(_v), w(_w), puzzle(_puzzle)
{
}

AFK_JigsawPiece::AFK_JigsawPiece(const Vec3<int>& _piece, int _puzzle):
    u(_piece.v[0]), v(_piece.v[1]), w(_piece.v[2]), puzzle(_puzzle)
{
}

AFK_JigsawPiece::operator bool() const
{
    return operator!=(AFK_JigsawPiece());
}

bool AFK_JigsawPiece::operator==(const AFK_JigsawPiece& other) const
{
    return (u == other.u && v == other.v && w == other.w && puzzle == other.puzzle);
}

bool AFK_JigsawPiece::operator!=(const AFK_JigsawPiece& other) const
{
    return (u != other.u || v != other.v || w != other.w || puzzle != other.puzzle);
}

size_t hash_value(const AFK_JigsawPiece& jigsawPiece)
{
    /* Inspired by hash_value(const AFK_Cell&), just in case I
     * need to use this in anger
     */
    size_t hash = 0;
    hash = afk_hash_swizzle(hash, jigsawPiece.u);
    hash = afk_hash_swizzle(hash, jigsawPiece.v);
    hash = afk_hash_swizzle(hash, jigsawPiece.w);
    hash = afk_hash_swizzle(hash, jigsawPiece.puzzle);
    return hash;
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece)
{
    return os << "JigsawPiece(u=" << std::dec << piece.u << ", v=" << piece.v << ", w=" << piece.w << ", puzzle=" << piece.puzzle << ")";
}


/* AFK_Jigsaw implementation */

AFK_Jigsaw::AFK_Jigsaw(
    AFK_Computer *_computer,
    const Vec3<int>& _jigsawSize,
    const std::vector<AFK_JigsawImageDescriptor>& _desc):
        jigsawSize(_jigsawSize),
        desc(_desc),
        map(_jigsawSize)
{
}

AFK_Jigsaw::~AFK_Jigsaw()
{
    for (auto image : images) delete image;
}

bool AFK_Jigsaw::grab(Vec3<int>& o_uvw, AFK_Frame *o_timestamp)
{
    return map.grab(o_uvw, o_timestamp);
}

AFK_Frame AFK_Jigsaw::getTimestamp(const AFK_JigsawPiece& piece) const
{
    Vec3<int> uvw = afk_vec3<int>(piece.u, piece.v, piece.w);
    return map.getTimestamp(uvw);
}

Vec2<float> AFK_Jigsaw::getTexCoordST(const AFK_JigsawPiece& piece) const
{
    return afk_vec2<float>(
        (float)piece.u / (float)jigsawSize.v[0],
        (float)piece.v / (float)jigsawSize.v[1]);
}

Vec3<float> AFK_Jigsaw::getTexCoordSTR(const AFK_JigsawPiece& piece) const
{
    return afk_vec3<float>(
        (float)piece.u / (float)jigsawSize.v[0],
        (float)piece.v / (float)jigsawSize.v[1],
        (float)piece.w / (float)jigsawSize.v[2]);
}

Vec2<float> AFK_Jigsaw::getPiecePitchST(void) const
{
    return afk_vec2<float>(
        1.0f / (float)jigsawSize.v[0],
        1.0f / (float)jigsawSize.v[1]);
}

Vec3<float> AFK_Jigsaw::getPiecePitchSTR(void) const
{
    return afk_vec3<float>(
        1.0f / (float)jigsawSize.v[0],
        1.0f / (float)jigsawSize.v[1],
        1.0f / (float)jigsawSize.v[2]);
}

void AFK_Jigsaw::setupImages(AFK_Computer *computer)
{
    if (images.size() == 0)
    {
        for (auto d : desc)
            images.push_back(new AFK_JigsawImage(computer, jigsawSize, d));
    }
}

Vec2<int> AFK_Jigsaw::getFake3D_size(unsigned int tex) const
{
    return images.at(tex)->getFake3D_size();
}

int AFK_Jigsaw::getFake3D_mult(unsigned int tex) const
{
    return images.at(tex)->getFake3D_mult();
}

cl_mem AFK_Jigsaw::acquireForCl(unsigned int tex, AFK_ComputeDependency& o_dep)
{
    return images.at(tex)->acquireForCl(o_dep);
}

void AFK_Jigsaw::releaseFromCl(unsigned int tex, const AFK_ComputeDependency& dep)
{
    if (!havePushList)
    {
        map.copyDrawPlaces(pushList);
        havePushList = true;
    }

    images.at(tex)->releaseFromCl(pushList, dep);
}

void AFK_Jigsaw::bindTexture(unsigned int tex)
{
    if (!havePushList)
    {
        map.copyDrawPlaces(pushList);
        havePushList = true;
    }

    images.at(tex)->bindTexture(pushList);
}

void AFK_Jigsaw::flip(const AFK_Frame& currentFrame)
{
    /* Make sure any changes are really, properly finished */
    for (auto image : images) image->waitForAll();

    /* Invalidate the push list. */
    havePushList = false;
    pushList.clear();

    /* Flip the map. */
    map.flip(currentFrame);
}

void AFK_Jigsaw::printStats(std::ostream& os, const std::string& prefix)
{
    map.printStats(os, prefix);
}

