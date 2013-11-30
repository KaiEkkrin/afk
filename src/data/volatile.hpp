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

#ifndef _AFK_DATA_VOLATILE_H_
#define _AFK_DATA_VOLATILE_H_

/* These utilities transfer the volatile shared structures into
 * usable things and back again.
 */

#include <cassert>

template<typename T, typename I>
void afk_grabSharedIntegral(T *mine, const volatile T *shared, size_t& offset) afk_noexcept
{
    *(reinterpret_cast<I*>(mine) + offset / sizeof(I)) =
        *(reinterpret_cast<const volatile I*>(shared) + offset / sizeof(I));
    offset += sizeof(I);
}

template<typename T, size_t count = 1>
void afk_grabShared(T *mine, const volatile T *shared) afk_noexcept
{
    size_t offset = 0;
    while (offset < sizeof(T) * count)
    {
        switch (sizeof(T) * count - offset)
        {
        case 1:
            afk_grabSharedIntegral<T, uint8_t>(mine, shared, offset);
            break;

        case 2: case 3:
            afk_grabSharedIntegral<T, uint16_t>(mine, shared, offset);
            break;

        case 4: case 5: case 6: case 7:
            afk_grabSharedIntegral<T, uint32_t>(mine, shared, offset);
            break;

        default:
            afk_grabSharedIntegral<T, uint64_t>(mine, shared, offset);
            break;
        }
    }

    assert(offset == sizeof(T) * count);
}

template<typename T, typename I>
void afk_returnSharedIntegral(const T *mine, volatile T *shared, size_t& offset) afk_noexcept
{
    *(reinterpret_cast<volatile I*>(shared) + offset / sizeof(I)) =
        *(reinterpret_cast<const I*>(mine) + offset / sizeof(I));
    offset += sizeof(I);
}

template<typename T, size_t count = 1>
void afk_returnShared(const T *mine, volatile T *shared) afk_noexcept
{
    size_t offset = 0;
    while (offset < sizeof(T) * count)
    {
        switch (sizeof(T) * count - offset)
        {
        case 1:
            afk_returnSharedIntegral<T, uint8_t>(mine, shared, offset);
            break;

        case 2: case 3:
            afk_returnSharedIntegral<T, uint16_t>(mine, shared, offset);
            break;

        case 4: case 5: case 6: case 7:
            afk_returnSharedIntegral<T, uint32_t>(mine, shared, offset);
            break;

        default:
            afk_returnSharedIntegral<T, uint64_t>(mine, shared, offset);
            break;
        }
    }

    assert(offset == sizeof(T) * count);
}

#endif /* _AFK_DATA_VOLATILE_H_ */

