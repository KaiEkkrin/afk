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

#include <array>
#include <cstdint>
#include <functional>

#include "hash.hpp"

/* This lookup table generated with tools/hash_lut.py */
static const std::array<uint8_t, 256> lut = {
	205,
	105,
	150,
	17,
	140,
	154,
	42,
	181,
	194,
	95,
	202,
	238,
	93,
	199,
	133,
	161,
	114,
	104,
	108,
	36,
	20,
	35,
	240,
	116,
	59,
	183,
	218,
	85,
	54,
	8,
	147,
	125,
	26,
	67,
	144,
	71,
	88,
	5,
	190,
	122,
	160,
	174,
	44,
	232,
	179,
	29,
	15,
	25,
	45,
	121,
	204,
	127,
	49,
	97,
	21,
	235,
	87,
	156,
	152,
	241,
	186,
	0,
	175,
	60,
	28,
	225,
	37,
	34,
	51,
	43,
	169,
	249,
	64,
	213,
	96,
	163,
	73,
	65,
	178,
	151,
	75,
	101,
	86,
	131,
	214,
	230,
	149,
	98,
	148,
	76,
	220,
	61,
	177,
	81,
	110,
	211,
	90,
	79,
	63,
	115,
	244,
	4,
	16,
	173,
	206,
	142,
	107,
	47,
	9,
	139,
	250,
	103,
	112,
	130,
	159,
	109,
	132,
	226,
	100,
	217,
	72,
	19,
	12,
	89,
	233,
	41,
	192,
	216,
	201,
	106,
	158,
	255,
	164,
	162,
	68,
	119,
	53,
	246,
	143,
	66,
	222,
	224,
	245,
	239,
	50,
	251,
	209,
	138,
	185,
	171,
	99,
	83,
	182,
	166,
	24,
	123,
	92,
	69,
	38,
	70,
	55,
	157,
	102,
	56,
	176,
	1,
	129,
	242,
	236,
	13,
	48,
	3,
	223,
	10,
	247,
	82,
	145,
	62,
	167,
	80,
	2,
	23,
	22,
	91,
	168,
	94,
	134,
	30,
	77,
	237,
	39,
	253,
	193,
	141,
	155,
	187,
	124,
	84,
	180,
	33,
	74,
	248,
	126,
	203,
	207,
	221,
	27,
	146,
	196,
	6,
	191,
	231,
	198,
	111,
	243,
	210,
	165,
	46,
	227,
	113,
	78,
	229,
	184,
	57,
	200,
	197,
	172,
	208,
	52,
	118,
	215,
	188,
	195,
	137,
	170,
	7,
	136,
	11,
	254,
	31,
	32,
	58,
	234,
	128,
	117,
	153,
	135,
	252,
	14,
	189,
	212,
	219,
	120,
	40,
	228,
	18,
};

#define GET_BYTE(n, b) static_cast<uint8_t>(((n) >> ((b) * 8)) & 0xff)

static void setByte(uint64_t& n, uint8_t val, int b)
{
    uint64_t mask = ~(0xffull << (b * 8));
    n = (n & mask) |
        (static_cast<uint64_t>(val) << (b * 8));
}

uint64_t afk_hash_swizzle(uint64_t a, uint64_t b)
{
    /* This is essentially a CBC mode substitution cipher with a
     * 1 byte block size and the above fixed lookup table.
     * To make sure all blocks affect all other blocks I need
     * two passes
     */
    uint64_t out = a;
    uint8_t lastByte = (GET_BYTE(a, 0) ^ lut[GET_BYTE(b, 0)]);

    for (int i = 0; i < 16; ++i)
    {
        setByte(out, lastByte, i % 8);
        lastByte = (GET_BYTE(a, (i + 1) % 8) ^ lut[lastByte ^ GET_BYTE(b, (i + 1) % 8)]);
    }

    return out;
}

