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

int afk_combineTwoPuzzleFairQueue(int puzzle1, int puzzle2)
{
    int queue = 1;
    for (int pc1 = 0; pc1 < puzzle1; ++pc1) queue = queue * 2;
    for (int pc2 = 0; pc2 < puzzle2; ++pc2) queue = queue * 3;
    return queue;
}

void afk_splitTwoPuzzleFairQueue(int queue, int& o_puzzle1, int& o_puzzle2)
{
    o_puzzle1 = 0;
    o_puzzle2 = 0;

    while (queue > 0 && (queue % 2) == 0)
    {
        queue = queue / 2;
        ++o_puzzle1;
    }

    while (queue > 0 && (queue % 3) == 0)
    {
        queue = queue / 3;
        ++o_puzzle2;
    }
}

