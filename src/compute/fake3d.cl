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

/* This utility emulates 3D images for platforms that don't support them --
 * or not, depending whether you defined AFK_FAKE3D.
 */

#if AFK_FAKE3D

#define AFK_IMAGE3D image2d_t

int2 afk_from3DTo2DCoord(int4 coord, int2 fake3D_size, int fake3D_mult)
{
    return (int2)(
        coord.x + fake3D_size.x * (coord.z % fake3D_mult),
        coord.y + fake3D_size.y * (coord.z / fake3D_mult));
}

int2 afk_make3DJigsawCoord(int4 pieceCoord, int4 pointCoord, int2 fake3D_size, int fake3D_mult)
{
    return afk_from3DTo2DCoord(pieceCoord + pointCoord, fake3D_size, fake3D_mult);
}

#else
#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

#define AFK_IMAGE3D image3d_t

int4 afk_make3DJigsawCoord(int4 pieceCoord, int4 pointCoord, int2 ignored0, int ignored1)
{
    return pieceCoord * TDIM + pointCoord;
}

#endif /* AFK_FAKE3D */

