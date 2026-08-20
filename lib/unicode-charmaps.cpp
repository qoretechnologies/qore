/*
  unicode-charmaps.cpp

  Qore Programming Language

  Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#include <qore/Qore.h>

#include "qore/intern/unicode-case-data.h"

#include <cctype>

//! the number of elements in a statically-sized array
#define QORE_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

typedef std::map<unsigned, const char*> unicodecharmap_t;
static unicodecharmap_t accent_map;

void init_charmaps() {
   // originally taken from http://d3s.mff.cuni.cz/~holub/sw/phpaccents/
   accent_map[0xc0] = "A";   // "À" -> "A"
   accent_map[0xc1] = "A";   // "Á" -> "A"
   accent_map[0xc2] = "A";   // "Â" -> "A"
   accent_map[0xc3] = "A";   // "Ã" -> "A"
   accent_map[0xc4] = "A";   // "Ä" -> "A"
   accent_map[0xc5] = "A";   // "Å" -> "A"
   accent_map[0xc7] = "C";   // "Ç" -> "C"
   accent_map[0xc8] = "E";   // "È" -> "E"
   accent_map[0xc9] = "E";   // "É" -> "E"
   accent_map[0xca] = "E";   // "Ê" -> "E"
   accent_map[0xcb] = "E";   // "Ë" -> "E"
   accent_map[0xcc] = "I";   // "Ì" -> "I"
   accent_map[0xcd] = "I";   // "Í" -> "I"
   accent_map[0xce] = "I";   // "Î" -> "I"
   accent_map[0xcf] = "I";   // "Ï" -> "I"
   accent_map[0xd1] = "N";   // "Ñ" -> "N"
   accent_map[0xd2] = "O";   // "Ò" -> "O"
   accent_map[0xd3] = "O";   // "Ó" -> "O"
   accent_map[0xd4] = "O";   // "Ô" -> "O"
   accent_map[0xd5] = "O";   // "Õ" -> "O"
   accent_map[0xd6] = "O";   // "Ö" -> "O"
   accent_map[0xd8] = "O";   // "Ø" -> "O"
   accent_map[0xd9] = "U";   // "Ù" -> "U"
   accent_map[0xda] = "U";   // "Ú" -> "U"
   accent_map[0xdb] = "U";   // "Û" -> "U"
   accent_map[0xdc] = "U";   // "Ü" -> "U"
   accent_map[0xdd] = "Y";   // "Ý" -> "Y"
   accent_map[0xdf] = "ss";   // "ß" -> "ss"
   accent_map[0xe0] = "a";   // "à" -> "a"
   accent_map[0xe1] = "a";   // "á" -> "a"
   accent_map[0xe2] = "a";   // "â" -> "a"
   accent_map[0xe3] = "a";   // "ã" -> "a"
   accent_map[0xe4] = "a";   // "ä" -> "a"
   accent_map[0xe5] = "a";   // "å" -> "a"
   accent_map[0xe6] = "ae";   // "æ" -> "ae"
   accent_map[0xe7] = "c";   // "ç" -> "c"
   accent_map[0xe8] = "e";   // "è" -> "e"
   accent_map[0xe9] = "e";   // "é" -> "e"
   accent_map[0xea] = "e";   // "ê" -> "e"
   accent_map[0xeb] = "e";   // "ë" -> "e"
   accent_map[0xec] = "i";   // "ì" -> "i"
   accent_map[0xed] = "i";   // "í" -> "i"
   accent_map[0xee] = "i";   // "î" -> "i"
   accent_map[0xef] = "i";   // "ï" -> "i"
   accent_map[0xf1] = "n";   // "ñ" -> "n"
   accent_map[0xf2] = "o";   // "ò" -> "o"
   accent_map[0xf3] = "o";   // "ó" -> "o"
   accent_map[0xf4] = "o";   // "ô" -> "o"
   accent_map[0xf5] = "o";   // "õ" -> "o"
   accent_map[0xf6] = "o";   // "ö" -> "o"
   accent_map[0xf8] = "o";   // "ø" -> "o"
   accent_map[0xf9] = "u";   // "ù" -> "u"
   accent_map[0xfa] = "u";   // "ú" -> "u"
   accent_map[0xfb] = "u";   // "û" -> "u"
   accent_map[0xfc] = "u";   // "ü" -> "u"
   accent_map[0xfd] = "y";   // "ý" -> "y"
   accent_map[0xff] = "y";   // "ÿ" -> "y"
   accent_map[0x100] = "A";   // "Ā" -> "A"
   accent_map[0x101] = "a";   // "ā" -> "a"
   accent_map[0x102] = "A";   // "Ă" -> "A"
   accent_map[0x103] = "a";   // "ă" -> "a"
   accent_map[0x104] = "A";   // "Ą" -> "A"
   accent_map[0x105] = "a";   // "ą" -> "a"
   accent_map[0x106] = "C";   // "Ć" -> "C"
   accent_map[0x107] = "c";   // "ć" -> "c"
   accent_map[0x108] = "C";   // "Ĉ" -> "C"
   accent_map[0x109] = "c";   // "ĉ" -> "c"
   accent_map[0x10a] = "C";   // "Ċ" -> "C"
   accent_map[0x10b] = "c";   // "ċ" -> "c"
   accent_map[0x10c] = "C";   // "Č" -> "C"
   accent_map[0x10d] = "c";   // "č" -> "c"
   accent_map[0x10e] = "D";   // "Ď" -> "D"
   accent_map[0x10f] = "d";   // "ď" -> "d"
   accent_map[0x110] = "D";   // "Đ" -> "D"
   accent_map[0x111] = "d";   // "đ" -> "d"
   accent_map[0x112] = "E";   // "Ē" -> "E"
   accent_map[0x113] = "e";   // "ē" -> "e"
   accent_map[0x114] = "E";   // "Ĕ" -> "E"
   accent_map[0x115] = "e";   // "ĕ" -> "e"
   accent_map[0x116] = "E";   // "Ė" -> "E"
   accent_map[0x117] = "e";   // "ė" -> "e"
   accent_map[0x118] = "E";   // "Ę" -> "E"
   accent_map[0x119] = "e";   // "ę" -> "e"
   accent_map[0x11a] = "E";   // "Ě" -> "E"
   accent_map[0x11b] = "e";   // "ě" -> "e"
   accent_map[0x11c] = "G";   // "Ĝ" -> "G"
   accent_map[0x11d] = "g";   // "ĝ" -> "g"
   accent_map[0x11e] = "G";   // "Ğ" -> "G"
   accent_map[0x11f] = "g";   // "ğ" -> "g"
   accent_map[0x120] = "G";   // "Ġ" -> "G"
   accent_map[0x121] = "g";   // "ġ" -> "g"
   accent_map[0x122] = "G";   // "Ģ" -> "G"
   accent_map[0x123] = "g";   // "ģ" -> "g"
   accent_map[0x124] = "H";   // "Ĥ" -> "H"
   accent_map[0x125] = "h";   // "ĥ" -> "h"
   accent_map[0x126] = "H";   // "Ħ" -> "H"
   accent_map[0x127] = "h";   // "ħ" -> "h"
   accent_map[0x128] = "I";   // "Ĩ" -> "I"
   accent_map[0x129] = "i";   // "ĩ" -> "i"
   accent_map[0x12a] = "I";   // "Ī" -> "I"
   accent_map[0x12b] = "i";   // "ī" -> "i"
   accent_map[0x12c] = "I";   // "Ĭ" -> "I"
   accent_map[0x12d] = "i";   // "ĭ" -> "i"
   accent_map[0x12e] = "I";   // "Į" -> "I"
   accent_map[0x12f] = "i";   // "į" -> "i"
   accent_map[0x130] = "I";   // "İ" -> "I"
   accent_map[0x131] = "i";   // "ı" -> "i"
   accent_map[0x134] = "J";   // "Ĵ" -> "J"
   accent_map[0x135] = "j";   // "ĵ" -> "j"
   accent_map[0x136] = "K";   // "Ķ" -> "K"
   accent_map[0x137] = "k";   // "ķ" -> "k"
   accent_map[0x139] = "L";   // "Ĺ" -> "L"
   accent_map[0x13a] = "l";   // "ĺ" -> "l"
   accent_map[0x13b] = "L";   // "Ļ" -> "L"
   accent_map[0x13c] = "l";   // "ļ" -> "l"
   accent_map[0x13d] = "L";   // "Ľ" -> "L"
   accent_map[0x13e] = "l";   // "ľ" -> "l"
   accent_map[0x13f] = "L";   // "Ŀ" -> "L"
   accent_map[0x140] = "l";   // "ŀ" -> "l"
   accent_map[0x141] = "L";   // "Ł" -> "L"
   accent_map[0x142] = "l";   // "ł" -> "l"
   accent_map[0x143] = "N";   // "Ń" -> "N"
   accent_map[0x144] = "n";   // "ń" -> "n"
   accent_map[0x145] = "N";   // "Ņ" -> "N"
   accent_map[0x146] = "n";   // "ņ" -> "n"
   accent_map[0x147] = "N";   // "Ň" -> "N"
   accent_map[0x148] = "n";   // "ň" -> "n"
   accent_map[0x149] = "n";   // "ŉ" -> "n"
   accent_map[0x14c] = "O";   // "Ō" -> "O"
   accent_map[0x14d] = "o";   // "ō" -> "o"
   accent_map[0x14e] = "O";   // "Ŏ" -> "O"
   accent_map[0x14f] = "o";   // "ŏ" -> "o"
   accent_map[0x150] = "O";   // "Ő" -> "O"
   accent_map[0x151] = "o";   // "ő" -> "o"
   accent_map[0x153] = "oe";   // "œ" -> "oe"
   accent_map[0x154] = "R";   // "Ŕ" -> "R"
   accent_map[0x155] = "r";   // "ŕ" -> "r"
   accent_map[0x156] = "R";   // "Ŗ" -> "R"
   accent_map[0x157] = "r";   // "ŗ" -> "r"
   accent_map[0x158] = "R";   // "Ř" -> "R"
   accent_map[0x159] = "r";   // "ř" -> "r"
   accent_map[0x15a] = "S";   // "Ś" -> "S"
   accent_map[0x15b] = "s";   // "ś" -> "s"
   accent_map[0x15c] = "S";   // "Ŝ" -> "S"
   accent_map[0x15d] = "s";   // "ŝ" -> "s"
   accent_map[0x15e] = "S";   // "Ş" -> "S"
   accent_map[0x15f] = "s";   // "ş" -> "s"
   accent_map[0x160] = "S";   // "Š" -> "S"
   accent_map[0x161] = "s";   // "š" -> "s"
   accent_map[0x162] = "T";   // "Ţ" -> "T"
   accent_map[0x163] = "t";   // "ţ" -> "t"
   accent_map[0x164] = "T";   // "Ť" -> "T"
   accent_map[0x165] = "t";   // "ť" -> "t"
   accent_map[0x166] = "T";   // "Ŧ" -> "T"
   accent_map[0x167] = "t";   // "ŧ" -> "t"
   accent_map[0x168] = "U";   // "Ũ" -> "U"
   accent_map[0x169] = "u";   // "ũ" -> "u"
   accent_map[0x16a] = "U";   // "Ū" -> "U"
   accent_map[0x16b] = "u";   // "ū" -> "u"
   accent_map[0x16c] = "U";   // "Ŭ" -> "U"
   accent_map[0x16d] = "u";   // "ŭ" -> "u"
   accent_map[0x16e] = "U";   // "Ů" -> "U"
   accent_map[0x16f] = "u";   // "ů" -> "u"
   accent_map[0x170] = "U";   // "Ű" -> "U"
   accent_map[0x171] = "u";   // "ű" -> "u"
   accent_map[0x172] = "U";   // "Ų" -> "U"
   accent_map[0x173] = "u";   // "ų" -> "u"
   accent_map[0x174] = "W";   // "Ŵ" -> "W"
   accent_map[0x175] = "w";   // "ŵ" -> "w"
   accent_map[0x176] = "Y";   // "Ŷ" -> "Y"
   accent_map[0x177] = "y";   // "ŷ" -> "y"
   accent_map[0x178] = "Y";   // "Ÿ" -> "Y"
   accent_map[0x179] = "Z";   // "Ź" -> "Z"
   accent_map[0x17a] = "z";   // "ź" -> "z"
   accent_map[0x17b] = "Z";   // "Ż" -> "Z"
   accent_map[0x17c] = "z";   // "ż" -> "z"
   accent_map[0x17d] = "Z";   // "Ž" -> "Z"
   accent_map[0x17e] = "z";   // "ž" -> "z"
   accent_map[0x180] = "b";   // "ƀ" -> "b"
   accent_map[0x181] = "B";   // "Ɓ" -> "B"
   accent_map[0x182] = "B";   // "Ƃ" -> "B"
   accent_map[0x183] = "b";   // "ƃ" -> "b"
   accent_map[0x187] = "C";   // "Ƈ" -> "C"
   accent_map[0x188] = "c";   // "ƈ" -> "c"
   accent_map[0x18a] = "D";   // "Ɗ" -> "D"
   accent_map[0x18b] = "D";   // "Ƌ" -> "D"
   accent_map[0x18c] = "d";   // "ƌ" -> "d"
   accent_map[0x191] = "F";   // "Ƒ" -> "F"
   accent_map[0x192] = "f";   // "ƒ" -> "f"
   accent_map[0x193] = "G";   // "Ɠ" -> "G"
   accent_map[0x197] = "I";   // "Ɨ" -> "I"
   accent_map[0x198] = "K";   // "Ƙ" -> "K"
   accent_map[0x199] = "k";   // "ƙ" -> "k"
   accent_map[0x19a] = "l";   // "ƚ" -> "l"
   accent_map[0x19d] = "N";   // "Ɲ" -> "N"
   accent_map[0x19e] = "n";   // "ƞ" -> "n"
   accent_map[0x19f] = "O";   // "Ɵ" -> "O"
   accent_map[0x1a0] = "O";   // "Ơ" -> "O"
   accent_map[0x1a1] = "o";   // "ơ" -> "o"
   accent_map[0x1a4] = "P";   // "Ƥ" -> "P"
   accent_map[0x1a5] = "p";   // "ƥ" -> "p"
   accent_map[0x1ab] = "t";   // "ƫ" -> "t"
   accent_map[0x1ac] = "T";   // "Ƭ" -> "T"
   accent_map[0x1ad] = "t";   // "ƭ" -> "t"
   accent_map[0x1ae] = "T";   // "Ʈ" -> "T"
   accent_map[0x1af] = "U";   // "Ư" -> "U"
   accent_map[0x1b0] = "u";   // "ư" -> "u"
   accent_map[0x1b2] = "V";   // "Ʋ" -> "V"
   accent_map[0x1b3] = "Y";   // "Ƴ" -> "Y"
   accent_map[0x1b4] = "y";   // "ƴ" -> "y"
   accent_map[0x1b5] = "Z";   // "Ƶ" -> "Z"
   accent_map[0x1b6] = "z";   // "ƶ" -> "z"
   accent_map[0x1c5] = "D";   // "ǅ" -> "D"
   accent_map[0x1c8] = "L";   // "ǈ" -> "L"
   accent_map[0x1cb] = "N";   // "ǋ" -> "N"
   accent_map[0x1cd] = "A";   // "Ǎ" -> "A"
   accent_map[0x1ce] = "a";   // "ǎ" -> "a"
   accent_map[0x1cf] = "I";   // "Ǐ" -> "I"
   accent_map[0x1d0] = "i";   // "ǐ" -> "i"
   accent_map[0x1d1] = "O";   // "Ǒ" -> "O"
   accent_map[0x1d2] = "o";   // "ǒ" -> "o"
   accent_map[0x1d3] = "U";   // "Ǔ" -> "U"
   accent_map[0x1d4] = "u";   // "ǔ" -> "u"
   accent_map[0x1d5] = "U";   // "Ǖ" -> "U"
   accent_map[0x1d6] = "u";   // "ǖ" -> "u"
   accent_map[0x1d7] = "U";   // "Ǘ" -> "U"
   accent_map[0x1d8] = "u";   // "ǘ" -> "u"
   accent_map[0x1d9] = "U";   // "Ǚ" -> "U"
   accent_map[0x1da] = "u";   // "ǚ" -> "u"
   accent_map[0x1db] = "U";   // "Ǜ" -> "U"
   accent_map[0x1dc] = "u";   // "ǜ" -> "u"
   accent_map[0x1de] = "A";   // "Ǟ" -> "A"
   accent_map[0x1df] = "a";   // "ǟ" -> "a"
   accent_map[0x1e0] = "A";   // "Ǡ" -> "A"
   accent_map[0x1e1] = "a";   // "ǡ" -> "a"
   accent_map[0x1e4] = "G";   // "Ǥ" -> "G"
   accent_map[0x1e5] = "g";   // "ǥ" -> "g"
   accent_map[0x1e6] = "G";   // "Ǧ" -> "G"
   accent_map[0x1e7] = "g";   // "ǧ" -> "g"
   accent_map[0x1e8] = "K";   // "Ǩ" -> "K"
   accent_map[0x1e9] = "k";   // "ǩ" -> "k"
   accent_map[0x1ea] = "O";   // "Ǫ" -> "O"
   accent_map[0x1eb] = "o";   // "ǫ" -> "o"
   accent_map[0x1ec] = "O";   // "Ǭ" -> "O"
   accent_map[0x1ed] = "o";   // "ǭ" -> "o"
   accent_map[0x1f0] = "j";   // "ǰ" -> "j"
   accent_map[0x1f2] = "D";   // "ǲ" -> "D"
   accent_map[0x1f4] = "G";   // "Ǵ" -> "G"
   accent_map[0x1f5] = "g";   // "ǵ" -> "g"
   accent_map[0x1f8] = "N";   // "Ǹ" -> "N"
   accent_map[0x1f9] = "n";   // "ǹ" -> "n"
   accent_map[0x1fa] = "A";   // "Ǻ" -> "A"
   accent_map[0x1fb] = "a";   // "ǻ" -> "a"
   accent_map[0x1fe] = "O";   // "Ǿ" -> "O"
   accent_map[0x1ff] = "o";   // "ǿ" -> "o"
   accent_map[0x200] = "A";   // "Ȁ" -> "A"
   accent_map[0x201] = "a";   // "ȁ" -> "a"
   accent_map[0x202] = "A";   // "Ȃ" -> "A"
   accent_map[0x203] = "a";   // "ȃ" -> "a"
   accent_map[0x204] = "E";   // "Ȅ" -> "E"
   accent_map[0x205] = "e";   // "ȅ" -> "e"
   accent_map[0x206] = "E";   // "Ȇ" -> "E"
   accent_map[0x207] = "e";   // "ȇ" -> "e"
   accent_map[0x208] = "I";   // "Ȉ" -> "I"
   accent_map[0x209] = "i";   // "ȉ" -> "i"
   accent_map[0x20a] = "I";   // "Ȋ" -> "I"
   accent_map[0x20b] = "i";   // "ȋ" -> "i"
   accent_map[0x20c] = "O";   // "Ȍ" -> "O"
   accent_map[0x20d] = "o";   // "ȍ" -> "o"
   accent_map[0x20e] = "O";   // "Ȏ" -> "O"
   accent_map[0x20f] = "o";   // "ȏ" -> "o"
   accent_map[0x210] = "R";   // "Ȑ" -> "R"
   accent_map[0x211] = "r";   // "ȑ" -> "r"
   accent_map[0x212] = "R";   // "Ȓ" -> "R"
   accent_map[0x213] = "r";   // "ȓ" -> "r"
   accent_map[0x214] = "U";   // "Ȕ" -> "U"
   accent_map[0x215] = "u";   // "ȕ" -> "u"
   accent_map[0x216] = "U";   // "Ȗ" -> "U"
   accent_map[0x217] = "u";   // "ȗ" -> "u"
   accent_map[0x218] = "S";   // "Ș" -> "S"
   accent_map[0x219] = "s";   // "ș" -> "s"
   accent_map[0x21a] = "T";   // "Ț" -> "T"
   accent_map[0x21b] = "t";   // "ț" -> "t"
   accent_map[0x21e] = "H";   // "Ȟ" -> "H"
   accent_map[0x21f] = "h";   // "ȟ" -> "h"
   accent_map[0x220] = "N";   // "Ƞ" -> "N"
   accent_map[0x221] = "d";   // "ȡ" -> "d"
   accent_map[0x224] = "Z";   // "Ȥ" -> "Z"
   accent_map[0x225] = "z";   // "ȥ" -> "z"
   accent_map[0x226] = "A";   // "Ȧ" -> "A"
   accent_map[0x227] = "a";   // "ȧ" -> "a"
   accent_map[0x228] = "E";   // "Ȩ" -> "E"
   accent_map[0x229] = "e";   // "ȩ" -> "e"
   accent_map[0x22a] = "O";   // "Ȫ" -> "O"
   accent_map[0x22b] = "o";   // "ȫ" -> "o"
   accent_map[0x22c] = "O";   // "Ȭ" -> "O"
   accent_map[0x22d] = "o";   // "ȭ" -> "o"
   accent_map[0x22e] = "O";   // "Ȯ" -> "O"
   accent_map[0x22f] = "o";   // "ȯ" -> "o"
   accent_map[0x230] = "O";   // "Ȱ" -> "O"
   accent_map[0x231] = "o";   // "ȱ" -> "o"
   accent_map[0x232] = "Y";   // "Ȳ" -> "Y"
   accent_map[0x233] = "y";   // "ȳ" -> "y"
   accent_map[0x234] = "l";   // "ȴ" -> "l"
   accent_map[0x235] = "n";   // "ȵ" -> "n"
   accent_map[0x236] = "t";   // "ȶ" -> "t"
   accent_map[0x237] = "j";   // "ȷ" -> "j"
   accent_map[0x23a] = "A";   // "Ⱥ" -> "A"
   accent_map[0x23b] = "C";   // "Ȼ" -> "C"
   accent_map[0x23c] = "c";   // "ȼ" -> "c"
   accent_map[0x23d] = "L";   // "Ƚ" -> "L"
   accent_map[0x23e] = "T";   // "Ⱦ" -> "T"
   accent_map[0x23f] = "s";   // "ȿ" -> "s"
   accent_map[0x240] = "z";   // "ɀ" -> "z"
   accent_map[0x243] = "B";   // "Ƀ" -> "B"
   accent_map[0x244] = "U";   // "Ʉ" -> "U"
   accent_map[0x246] = "E";   // "Ɇ" -> "E"
   accent_map[0x247] = "e";   // "ɇ" -> "e"
   accent_map[0x248] = "J";   // "Ɉ" -> "J"
   accent_map[0x249] = "j";   // "ɉ" -> "j"
   accent_map[0x24b] = "q";   // "ɋ" -> "q"
   accent_map[0x24c] = "R";   // "Ɍ" -> "R"
   accent_map[0x24d] = "r";   // "ɍ" -> "r"
   accent_map[0x24e] = "Y";   // "Ɏ" -> "Y"
   accent_map[0x24f] = "y";   // "ɏ" -> "y"
   accent_map[0x253] = "b";   // "ɓ" -> "b"
   accent_map[0x255] = "c";   // "ɕ" -> "c"
   accent_map[0x256] = "d";   // "ɖ" -> "d"
   accent_map[0x257] = "d";   // "ɗ" -> "d"
   accent_map[0x25f] = "j";   // "ɟ" -> "j"
   accent_map[0x260] = "g";   // "ɠ" -> "g"
   accent_map[0x266] = "h";   // "ɦ" -> "h"
   accent_map[0x268] = "i";   // "ɨ" -> "i"
   accent_map[0x26b] = "l";   // "ɫ" -> "l"
   accent_map[0x26c] = "l";   // "ɬ" -> "l"
   accent_map[0x26d] = "l";   // "ɭ" -> "l"
   accent_map[0x271] = "m";   // "ɱ" -> "m"
   accent_map[0x272] = "n";   // "ɲ" -> "n"
   accent_map[0x273] = "n";   // "ɳ" -> "n"
   accent_map[0x275] = "o";   // "ɵ" -> "o"
   accent_map[0x27c] = "r";   // "ɼ" -> "r"
   accent_map[0x27d] = "r";   // "ɽ" -> "r"
   accent_map[0x27e] = "r";   // "ɾ" -> "r"
   accent_map[0x282] = "s";   // "ʂ" -> "s"
   accent_map[0x284] = "j";   // "ʄ" -> "j"
   accent_map[0x288] = "t";   // "ʈ" -> "t"
   accent_map[0x289] = "u";   // "ʉ" -> "u"
   accent_map[0x28b] = "v";   // "ʋ" -> "v"
   accent_map[0x290] = "z";   // "ʐ" -> "z"
   accent_map[0x291] = "z";   // "ʑ" -> "z"
   accent_map[0x29d] = "j";   // "ʝ" -> "j"
   accent_map[0x2a0] = "q";   // "ʠ" -> "q"
   accent_map[0x363] = "a";   // "ͣ" -> "a"
   accent_map[0x364] = "e";   // "ͤ" -> "e"
   accent_map[0x365] = "i";   // "ͥ" -> "i"
   accent_map[0x366] = "o";   // "ͦ" -> "o"
   accent_map[0x367] = "u";   // "ͧ" -> "u"
   accent_map[0x368] = "c";   // "ͨ" -> "c"
   accent_map[0x369] = "d";   // "ͩ" -> "d"
   accent_map[0x36a] = "h";   // "ͪ" -> "h"
   accent_map[0x36b] = "m";   // "ͫ" -> "m"
   accent_map[0x36c] = "r";   // "ͬ" -> "r"
   accent_map[0x36d] = "t";   // "ͭ" -> "t"
   accent_map[0x36e] = "v";   // "ͮ" -> "v"
   accent_map[0x36f] = "x";   // "ͯ" -> "x"
   accent_map[0x1d62] = "i";   // "ᵢ" -> "i"
   accent_map[0x1d63] = "r";   // "ᵣ" -> "r"
   accent_map[0x1d64] = "u";   // "ᵤ" -> "u"
   accent_map[0x1d65] = "v";   // "ᵥ" -> "v"
   accent_map[0x1d6c] = "b";   // "ᵬ" -> "b"
   accent_map[0x1d6d] = "d";   // "ᵭ" -> "d"
   accent_map[0x1d6e] = "f";   // "ᵮ" -> "f"
   accent_map[0x1d6f] = "m";   // "ᵯ" -> "m"
   accent_map[0x1d70] = "n";   // "ᵰ" -> "n"
   accent_map[0x1d71] = "p";   // "ᵱ" -> "p"
   accent_map[0x1d72] = "r";   // "ᵲ" -> "r"
   accent_map[0x1d73] = "r";   // "ᵳ" -> "r"
   accent_map[0x1d74] = "s";   // "ᵴ" -> "s"
   accent_map[0x1d75] = "t";   // "ᵵ" -> "t"
   accent_map[0x1d76] = "z";   // "ᵶ" -> "z"
   accent_map[0x1d7b] = "i";   // "ᵻ" -> "i"
   accent_map[0x1d7d] = "p";   // "ᵽ" -> "p"
   accent_map[0x1d7e] = "u";   // "ᵾ" -> "u"
   accent_map[0x1d80] = "b";   // "ᶀ" -> "b"
   accent_map[0x1d81] = "d";   // "ᶁ" -> "d"
   accent_map[0x1d82] = "f";   // "ᶂ" -> "f"
   accent_map[0x1d83] = "g";   // "ᶃ" -> "g"
   accent_map[0x1d84] = "k";   // "ᶄ" -> "k"
   accent_map[0x1d85] = "l";   // "ᶅ" -> "l"
   accent_map[0x1d86] = "m";   // "ᶆ" -> "m"
   accent_map[0x1d87] = "n";   // "ᶇ" -> "n"
   accent_map[0x1d88] = "p";   // "ᶈ" -> "p"
   accent_map[0x1d89] = "r";   // "ᶉ" -> "r"
   accent_map[0x1d8a] = "s";   // "ᶊ" -> "s"
   accent_map[0x1d8c] = "v";   // "ᶌ" -> "v"
   accent_map[0x1d8d] = "x";   // "ᶍ" -> "x"
   accent_map[0x1d8e] = "z";   // "ᶎ" -> "z"
   accent_map[0x1d8f] = "a";   // "ᶏ" -> "a"
   accent_map[0x1d91] = "d";   // "ᶑ" -> "d"
   accent_map[0x1d92] = "e";   // "ᶒ" -> "e"
   accent_map[0x1d96] = "i";   // "ᶖ" -> "i"
   accent_map[0x1d99] = "u";   // "ᶙ" -> "u"
   accent_map[0x1dca] = "r";   // "᷊" -> "r"
   accent_map[0x1dd7] = "c";   // "ᷗ" -> "c"
   accent_map[0x1dda] = "g";   // "ᷚ" -> "g"
   accent_map[0x1ddc] = "k";   // "ᷜ" -> "k"
   accent_map[0x1ddd] = "l";   // "ᷝ" -> "l"
   accent_map[0x1de0] = "n";   // "ᷠ" -> "n"
   accent_map[0x1de3] = "r";   // "ᷣ" -> "r"
   accent_map[0x1de4] = "s";   // "ᷤ" -> "s"
   accent_map[0x1de6] = "z";   // "ᷦ" -> "z"
   accent_map[0x1e00] = "A";   // "Ḁ" -> "A"
   accent_map[0x1e01] = "a";   // "ḁ" -> "a"
   accent_map[0x1e02] = "B";   // "Ḃ" -> "B"
   accent_map[0x1e03] = "b";   // "ḃ" -> "b"
   accent_map[0x1e04] = "B";   // "Ḅ" -> "B"
   accent_map[0x1e05] = "b";   // "ḅ" -> "b"
   accent_map[0x1e06] = "B";   // "Ḇ" -> "B"
   accent_map[0x1e07] = "b";   // "ḇ" -> "b"
   accent_map[0x1e08] = "C";   // "Ḉ" -> "C"
   accent_map[0x1e09] = "c";   // "ḉ" -> "c"
   accent_map[0x1e0a] = "D";   // "Ḋ" -> "D"
   accent_map[0x1e0b] = "d";   // "ḋ" -> "d"
   accent_map[0x1e0c] = "D";   // "Ḍ" -> "D"
   accent_map[0x1e0d] = "d";   // "ḍ" -> "d"
   accent_map[0x1e0e] = "D";   // "Ḏ" -> "D"
   accent_map[0x1e0f] = "d";   // "ḏ" -> "d"
   accent_map[0x1e10] = "D";   // "Ḑ" -> "D"
   accent_map[0x1e11] = "d";   // "ḑ" -> "d"
   accent_map[0x1e12] = "D";   // "Ḓ" -> "D"
   accent_map[0x1e13] = "d";   // "ḓ" -> "d"
   accent_map[0x1e14] = "E";   // "Ḕ" -> "E"
   accent_map[0x1e15] = "e";   // "ḕ" -> "e"
   accent_map[0x1e16] = "E";   // "Ḗ" -> "E"
   accent_map[0x1e17] = "e";   // "ḗ" -> "e"
   accent_map[0x1e18] = "E";   // "Ḙ" -> "E"
   accent_map[0x1e19] = "e";   // "ḙ" -> "e"
   accent_map[0x1e1a] = "E";   // "Ḛ" -> "E"
   accent_map[0x1e1b] = "e";   // "ḛ" -> "e"
   accent_map[0x1e1c] = "E";   // "Ḝ" -> "E"
   accent_map[0x1e1d] = "e";   // "ḝ" -> "e"
   accent_map[0x1e1e] = "F";   // "Ḟ" -> "F"
   accent_map[0x1e1f] = "f";   // "ḟ" -> "f"
   accent_map[0x1e20] = "G";   // "Ḡ" -> "G"
   accent_map[0x1e21] = "g";   // "ḡ" -> "g"
   accent_map[0x1e22] = "H";   // "Ḣ" -> "H"
   accent_map[0x1e23] = "h";   // "ḣ" -> "h"
   accent_map[0x1e24] = "H";   // "Ḥ" -> "H"
   accent_map[0x1e25] = "h";   // "ḥ" -> "h"
   accent_map[0x1e26] = "H";   // "Ḧ" -> "H"
   accent_map[0x1e27] = "h";   // "ḧ" -> "h"
   accent_map[0x1e28] = "H";   // "Ḩ" -> "H"
   accent_map[0x1e29] = "h";   // "ḩ" -> "h"
   accent_map[0x1e2a] = "H";   // "Ḫ" -> "H"
   accent_map[0x1e2b] = "h";   // "ḫ" -> "h"
   accent_map[0x1e2c] = "I";   // "Ḭ" -> "I"
   accent_map[0x1e2d] = "i";   // "ḭ" -> "i"
   accent_map[0x1e2e] = "I";   // "Ḯ" -> "I"
   accent_map[0x1e2f] = "i";   // "ḯ" -> "i"
   accent_map[0x1e30] = "K";   // "Ḱ" -> "K"
   accent_map[0x1e31] = "k";   // "ḱ" -> "k"
   accent_map[0x1e32] = "K";   // "Ḳ" -> "K"
   accent_map[0x1e33] = "k";   // "ḳ" -> "k"
   accent_map[0x1e34] = "K";   // "Ḵ" -> "K"
   accent_map[0x1e35] = "k";   // "ḵ" -> "k"
   accent_map[0x1e36] = "L";   // "Ḷ" -> "L"
   accent_map[0x1e37] = "l";   // "ḷ" -> "l"
   accent_map[0x1e38] = "L";   // "Ḹ" -> "L"
   accent_map[0x1e39] = "l";   // "ḹ" -> "l"
   accent_map[0x1e3a] = "L";   // "Ḻ" -> "L"
   accent_map[0x1e3b] = "l";   // "ḻ" -> "l"
   accent_map[0x1e3c] = "L";   // "Ḽ" -> "L"
   accent_map[0x1e3d] = "l";   // "ḽ" -> "l"
   accent_map[0x1e3e] = "M";   // "Ḿ" -> "M"
   accent_map[0x1e3f] = "m";   // "ḿ" -> "m"
   accent_map[0x1e40] = "M";   // "Ṁ" -> "M"
   accent_map[0x1e41] = "m";   // "ṁ" -> "m"
   accent_map[0x1e42] = "M";   // "Ṃ" -> "M"
   accent_map[0x1e43] = "m";   // "ṃ" -> "m"
   accent_map[0x1e44] = "N";   // "Ṅ" -> "N"
   accent_map[0x1e45] = "n";   // "ṅ" -> "n"
   accent_map[0x1e46] = "N";   // "Ṇ" -> "N"
   accent_map[0x1e47] = "n";   // "ṇ" -> "n"
   accent_map[0x1e48] = "N";   // "Ṉ" -> "N"
   accent_map[0x1e49] = "n";   // "ṉ" -> "n"
   accent_map[0x1e4a] = "N";   // "Ṋ" -> "N"
   accent_map[0x1e4b] = "n";   // "ṋ" -> "n"
   accent_map[0x1e4c] = "O";   // "Ṍ" -> "O"
   accent_map[0x1e4d] = "o";   // "ṍ" -> "o"
   accent_map[0x1e4e] = "O";   // "Ṏ" -> "O"
   accent_map[0x1e4f] = "o";   // "ṏ" -> "o"
   accent_map[0x1e50] = "O";   // "Ṑ" -> "O"
   accent_map[0x1e51] = "o";   // "ṑ" -> "o"
   accent_map[0x1e52] = "O";   // "Ṓ" -> "O"
   accent_map[0x1e53] = "o";   // "ṓ" -> "o"
   accent_map[0x1e54] = "P";   // "Ṕ" -> "P"
   accent_map[0x1e55] = "p";   // "ṕ" -> "p"
   accent_map[0x1e56] = "P";   // "Ṗ" -> "P"
   accent_map[0x1e57] = "p";   // "ṗ" -> "p"
   accent_map[0x1e58] = "R";   // "Ṙ" -> "R"
   accent_map[0x1e59] = "r";   // "ṙ" -> "r"
   accent_map[0x1e5a] = "R";   // "Ṛ" -> "R"
   accent_map[0x1e5b] = "r";   // "ṛ" -> "r"
   accent_map[0x1e5c] = "R";   // "Ṝ" -> "R"
   accent_map[0x1e5d] = "r";   // "ṝ" -> "r"
   accent_map[0x1e5e] = "R";   // "Ṟ" -> "R"
   accent_map[0x1e5f] = "r";   // "ṟ" -> "r"
   accent_map[0x1e60] = "S";   // "Ṡ" -> "S"
   accent_map[0x1e61] = "s";   // "ṡ" -> "s"
   accent_map[0x1e62] = "S";   // "Ṣ" -> "S"
   accent_map[0x1e63] = "s";   // "ṣ" -> "s"
   accent_map[0x1e64] = "S";   // "Ṥ" -> "S"
   accent_map[0x1e65] = "s";   // "ṥ" -> "s"
   accent_map[0x1e66] = "S";   // "Ṧ" -> "S"
   accent_map[0x1e67] = "s";   // "ṧ" -> "s"
   accent_map[0x1e68] = "S";   // "Ṩ" -> "S"
   accent_map[0x1e69] = "s";   // "ṩ" -> "s"
   accent_map[0x1e6a] = "T";   // "Ṫ" -> "T"
   accent_map[0x1e6b] = "t";   // "ṫ" -> "t"
   accent_map[0x1e6c] = "T";   // "Ṭ" -> "T"
   accent_map[0x1e6d] = "t";   // "ṭ" -> "t"
   accent_map[0x1e6e] = "T";   // "Ṯ" -> "T"
   accent_map[0x1e6f] = "t";   // "ṯ" -> "t"
   accent_map[0x1e70] = "T";   // "Ṱ" -> "T"
   accent_map[0x1e71] = "t";   // "ṱ" -> "t"
   accent_map[0x1e72] = "U";   // "Ṳ" -> "U"
   accent_map[0x1e73] = "u";   // "ṳ" -> "u"
   accent_map[0x1e74] = "U";   // "Ṵ" -> "U"
   accent_map[0x1e75] = "u";   // "ṵ" -> "u"
   accent_map[0x1e76] = "U";   // "Ṷ" -> "U"
   accent_map[0x1e77] = "u";   // "ṷ" -> "u"
   accent_map[0x1e78] = "U";   // "Ṹ" -> "U"
   accent_map[0x1e79] = "u";   // "ṹ" -> "u"
   accent_map[0x1e7a] = "U";   // "Ṻ" -> "U"
   accent_map[0x1e7b] = "u";   // "ṻ" -> "u"
   accent_map[0x1e7c] = "V";   // "Ṽ" -> "V"
   accent_map[0x1e7d] = "v";   // "ṽ" -> "v"
   accent_map[0x1e7e] = "V";   // "Ṿ" -> "V"
   accent_map[0x1e7f] = "v";   // "ṿ" -> "v"
   accent_map[0x1e80] = "W";   // "Ẁ" -> "W"
   accent_map[0x1e81] = "w";   // "ẁ" -> "w"
   accent_map[0x1e82] = "W";   // "Ẃ" -> "W"
   accent_map[0x1e83] = "w";   // "ẃ" -> "w"
   accent_map[0x1e84] = "W";   // "Ẅ" -> "W"
   accent_map[0x1e85] = "w";   // "ẅ" -> "w"
   accent_map[0x1e86] = "W";   // "Ẇ" -> "W"
   accent_map[0x1e87] = "w";   // "ẇ" -> "w"
   accent_map[0x1e88] = "W";   // "Ẉ" -> "W"
   accent_map[0x1e89] = "w";   // "ẉ" -> "w"
   accent_map[0x1e8a] = "X";   // "Ẋ" -> "X"
   accent_map[0x1e8b] = "x";   // "ẋ" -> "x"
   accent_map[0x1e8c] = "X";   // "Ẍ" -> "X"
   accent_map[0x1e8d] = "x";   // "ẍ" -> "x"
   accent_map[0x1e8e] = "Y";   // "Ẏ" -> "Y"
   accent_map[0x1e8f] = "y";   // "ẏ" -> "y"
   accent_map[0x1e90] = "Z";   // "Ẑ" -> "Z"
   accent_map[0x1e91] = "z";   // "ẑ" -> "z"
   accent_map[0x1e92] = "Z";   // "Ẓ" -> "Z"
   accent_map[0x1e93] = "z";   // "ẓ" -> "z"
   accent_map[0x1e94] = "Z";   // "Ẕ" -> "Z"
   accent_map[0x1e95] = "z";   // "ẕ" -> "z"
   accent_map[0x1e96] = "h";   // "ẖ" -> "h"
   accent_map[0x1e97] = "t";   // "ẗ" -> "t"
   accent_map[0x1e98] = "w";   // "ẘ" -> "w"
   accent_map[0x1e99] = "y";   // "ẙ" -> "y"
   accent_map[0x1e9a] = "a";   // "ẚ" -> "a"
   accent_map[0x1ea0] = "A";   // "Ạ" -> "A"
   accent_map[0x1ea1] = "a";   // "ạ" -> "a"
   accent_map[0x1ea2] = "A";   // "Ả" -> "A"
   accent_map[0x1ea3] = "a";   // "ả" -> "a"
   accent_map[0x1ea4] = "A";   // "Ấ" -> "A"
   accent_map[0x1ea5] = "a";   // "ấ" -> "a"
   accent_map[0x1ea6] = "A";   // "Ầ" -> "A"
   accent_map[0x1ea7] = "a";   // "ầ" -> "a"
   accent_map[0x1ea8] = "A";   // "Ẩ" -> "A"
   accent_map[0x1ea9] = "a";   // "ẩ" -> "a"
   accent_map[0x1eaa] = "A";   // "Ẫ" -> "A"
   accent_map[0x1eab] = "a";   // "ẫ" -> "a"
   accent_map[0x1eac] = "A";   // "Ậ" -> "A"
   accent_map[0x1ead] = "a";   // "ậ" -> "a"
   accent_map[0x1eae] = "A";   // "Ắ" -> "A"
   accent_map[0x1eaf] = "a";   // "ắ" -> "a"
   accent_map[0x1eb0] = "A";   // "Ằ" -> "A"
   accent_map[0x1eb1] = "a";   // "ằ" -> "a"
   accent_map[0x1eb2] = "A";   // "Ẳ" -> "A"
   accent_map[0x1eb3] = "a";   // "ẳ" -> "a"
   accent_map[0x1eb4] = "A";   // "Ẵ" -> "A"
   accent_map[0x1eb5] = "a";   // "ẵ" -> "a"
   accent_map[0x1eb6] = "A";   // "Ặ" -> "A"
   accent_map[0x1eb7] = "a";   // "ặ" -> "a"
   accent_map[0x1eb8] = "E";   // "Ẹ" -> "E"
   accent_map[0x1eb9] = "e";   // "ẹ" -> "e"
   accent_map[0x1eba] = "E";   // "Ẻ" -> "E"
   accent_map[0x1ebb] = "e";   // "ẻ" -> "e"
   accent_map[0x1ebc] = "E";   // "Ẽ" -> "E"
   accent_map[0x1ebd] = "e";   // "ẽ" -> "e"
   accent_map[0x1ebe] = "E";   // "Ế" -> "E"
   accent_map[0x1ebf] = "e";   // "ế" -> "e"
   accent_map[0x1ec0] = "E";   // "Ề" -> "E"
   accent_map[0x1ec1] = "e";   // "ề" -> "e"
   accent_map[0x1ec2] = "E";   // "Ể" -> "E"
   accent_map[0x1ec3] = "e";   // "ể" -> "e"
   accent_map[0x1ec4] = "E";   // "Ễ" -> "E"
   accent_map[0x1ec5] = "e";   // "ễ" -> "e"
   accent_map[0x1ec6] = "E";   // "Ệ" -> "E"
   accent_map[0x1ec7] = "e";   // "ệ" -> "e"
   accent_map[0x1ec8] = "I";   // "Ỉ" -> "I"
   accent_map[0x1ec9] = "i";   // "ỉ" -> "i"
   accent_map[0x1eca] = "I";   // "Ị" -> "I"
   accent_map[0x1ecb] = "i";   // "ị" -> "i"
   accent_map[0x1ecc] = "O";   // "Ọ" -> "O"
   accent_map[0x1ecd] = "o";   // "ọ" -> "o"
   accent_map[0x1ece] = "O";   // "Ỏ" -> "O"
   accent_map[0x1ecf] = "o";   // "ỏ" -> "o"
   accent_map[0x1ed0] = "O";   // "Ố" -> "O"
   accent_map[0x1ed1] = "o";   // "ố" -> "o"
   accent_map[0x1ed2] = "O";   // "Ồ" -> "O"
   accent_map[0x1ed3] = "o";   // "ồ" -> "o"
   accent_map[0x1ed4] = "O";   // "Ổ" -> "O"
   accent_map[0x1ed5] = "o";   // "ổ" -> "o"
   accent_map[0x1ed6] = "O";   // "Ỗ" -> "O"
   accent_map[0x1ed7] = "o";   // "ỗ" -> "o"
   accent_map[0x1ed8] = "O";   // "Ộ" -> "O"
   accent_map[0x1ed9] = "o";   // "ộ" -> "o"
   accent_map[0x1eda] = "O";   // "Ớ" -> "O"
   accent_map[0x1edb] = "o";   // "ớ" -> "o"
   accent_map[0x1edc] = "O";   // "Ờ" -> "O"
   accent_map[0x1edd] = "o";   // "ờ" -> "o"
   accent_map[0x1ede] = "O";   // "Ở" -> "O"
   accent_map[0x1edf] = "o";   // "ở" -> "o"
   accent_map[0x1ee0] = "O";   // "Ỡ" -> "O"
   accent_map[0x1ee1] = "o";   // "ỡ" -> "o"
   accent_map[0x1ee2] = "O";   // "Ợ" -> "O"
   accent_map[0x1ee3] = "o";   // "ợ" -> "o"
   accent_map[0x1ee4] = "U";   // "Ụ" -> "U"
   accent_map[0x1ee5] = "u";   // "ụ" -> "u"
   accent_map[0x1ee6] = "U";   // "Ủ" -> "U"
   accent_map[0x1ee7] = "u";   // "ủ" -> "u"
   accent_map[0x1ee8] = "U";   // "Ứ" -> "U"
   accent_map[0x1ee9] = "u";   // "ứ" -> "u"
   accent_map[0x1eea] = "U";   // "Ừ" -> "U"
   accent_map[0x1eeb] = "u";   // "ừ" -> "u"
   accent_map[0x1eec] = "U";   // "Ử" -> "U"
   accent_map[0x1eed] = "u";   // "ử" -> "u"
   accent_map[0x1eee] = "U";   // "Ữ" -> "U"
   accent_map[0x1eef] = "u";   // "ữ" -> "u"
   accent_map[0x1ef0] = "U";   // "Ự" -> "U"
   accent_map[0x1ef1] = "u";   // "ự" -> "u"
   accent_map[0x1ef2] = "Y";   // "Ỳ" -> "Y"
   accent_map[0x1ef3] = "y";   // "ỳ" -> "y"
   accent_map[0x1ef4] = "Y";   // "Ỵ" -> "Y"
   accent_map[0x1ef5] = "y";   // "ỵ" -> "y"
   accent_map[0x1ef6] = "Y";   // "Ỷ" -> "Y"
   accent_map[0x1ef7] = "y";   // "ỷ" -> "y"
   accent_map[0x1ef8] = "Y";   // "Ỹ" -> "Y"
   accent_map[0x1ef9] = "y";   // "ỹ" -> "y"
   accent_map[0x1efe] = "Y";   // "Ỿ" -> "Y"
   accent_map[0x1eff] = "y";   // "ỿ" -> "y"
   accent_map[0x2071] = "i";   // "ⁱ" -> "i"
   accent_map[0x207f] = "n";   // "ⁿ" -> "n"
   accent_map[0x2090] = "a";   // "ₐ" -> "a"
   accent_map[0x2091] = "e";   // "ₑ" -> "e"
   accent_map[0x2092] = "o";   // "ₒ" -> "o"
   accent_map[0x2093] = "x";   // "ₓ" -> "x"
   accent_map[0x249c] = "a";   // "⒜" -> "a"
   accent_map[0x249d] = "b";   // "⒝" -> "b"
   accent_map[0x249e] = "c";   // "⒞" -> "c"
   accent_map[0x249f] = "d";   // "⒟" -> "d"
   accent_map[0x24a0] = "e";   // "⒠" -> "e"
   accent_map[0x24a1] = "f";   // "⒡" -> "f"
   accent_map[0x24a2] = "g";   // "⒢" -> "g"
   accent_map[0x24a3] = "h";   // "⒣" -> "h"
   accent_map[0x24a4] = "i";   // "⒤" -> "i"
   accent_map[0x24a5] = "j";   // "⒥" -> "j"
   accent_map[0x24a6] = "k";   // "⒦" -> "k"
   accent_map[0x24a7] = "l";   // "⒧" -> "l"
   accent_map[0x24a8] = "m";   // "⒨" -> "m"
   accent_map[0x24a9] = "n";   // "⒩" -> "n"
   accent_map[0x24aa] = "o";   // "⒪" -> "o"
   accent_map[0x24ab] = "p";   // "⒫" -> "p"
   accent_map[0x24ac] = "q";   // "⒬" -> "q"
   accent_map[0x24ad] = "r";   // "⒭" -> "r"
   accent_map[0x24ae] = "s";   // "⒮" -> "s"
   accent_map[0x24af] = "t";   // "⒯" -> "t"
   accent_map[0x24b0] = "u";   // "⒰" -> "u"
   accent_map[0x24b1] = "v";   // "⒱" -> "v"
   accent_map[0x24b2] = "w";   // "⒲" -> "w"
   accent_map[0x24b3] = "x";   // "⒳" -> "x"
   accent_map[0x24b4] = "y";   // "⒴" -> "y"
   accent_map[0x24b5] = "z";   // "⒵" -> "z"
   accent_map[0x24b6] = "A";   // "Ⓐ" -> "A"
   accent_map[0x24b7] = "B";   // "Ⓑ" -> "B"
   accent_map[0x24b8] = "C";   // "Ⓒ" -> "C"
   accent_map[0x24b9] = "D";   // "Ⓓ" -> "D"
   accent_map[0x24ba] = "E";   // "Ⓔ" -> "E"
   accent_map[0x24bb] = "F";   // "Ⓕ" -> "F"
   accent_map[0x24bc] = "G";   // "Ⓖ" -> "G"
   accent_map[0x24bd] = "H";   // "Ⓗ" -> "H"
   accent_map[0x24be] = "I";   // "Ⓘ" -> "I"
   accent_map[0x24bf] = "J";   // "Ⓙ" -> "J"
   accent_map[0x24c0] = "K";   // "Ⓚ" -> "K"
   accent_map[0x24c1] = "L";   // "Ⓛ" -> "L"
   accent_map[0x24c2] = "M";   // "Ⓜ" -> "M"
   accent_map[0x24c3] = "N";   // "Ⓝ" -> "N"
   accent_map[0x24c4] = "O";   // "Ⓞ" -> "O"
   accent_map[0x24c5] = "P";   // "Ⓟ" -> "P"
   accent_map[0x24c6] = "Q";   // "Ⓠ" -> "Q"
   accent_map[0x24c7] = "R";   // "Ⓡ" -> "R"
   accent_map[0x24c8] = "S";   // "Ⓢ" -> "S"
   accent_map[0x24c9] = "T";   // "Ⓣ" -> "T"
   accent_map[0x24ca] = "U";   // "Ⓤ" -> "U"
   accent_map[0x24cb] = "V";   // "Ⓥ" -> "V"
   accent_map[0x24cc] = "W";   // "Ⓦ" -> "W"
   accent_map[0x24cd] = "X";   // "Ⓧ" -> "X"
   accent_map[0x24ce] = "Y";   // "Ⓨ" -> "Y"
   accent_map[0x24cf] = "Z";   // "Ⓩ" -> "Z"
   accent_map[0x24d0] = "a";   // "ⓐ" -> "a"
   accent_map[0x24d1] = "b";   // "ⓑ" -> "b"
   accent_map[0x24d2] = "c";   // "ⓒ" -> "c"
   accent_map[0x24d3] = "d";   // "ⓓ" -> "d"
   accent_map[0x24d4] = "e";   // "ⓔ" -> "e"
   accent_map[0x24d5] = "f";   // "ⓕ" -> "f"
   accent_map[0x24d6] = "g";   // "ⓖ" -> "g"
   accent_map[0x24d7] = "h";   // "ⓗ" -> "h"
   accent_map[0x24d8] = "i";   // "ⓘ" -> "i"
   accent_map[0x24d9] = "j";   // "ⓙ" -> "j"
   accent_map[0x24da] = "k";   // "ⓚ" -> "k"
   accent_map[0x24db] = "l";   // "ⓛ" -> "l"
   accent_map[0x24dc] = "m";   // "ⓜ" -> "m"
   accent_map[0x24dd] = "n";   // "ⓝ" -> "n"
   accent_map[0x24de] = "o";   // "ⓞ" -> "o"
   accent_map[0x24df] = "p";   // "ⓟ" -> "p"
   accent_map[0x24e0] = "q";   // "ⓠ" -> "q"
   accent_map[0x24e1] = "r";   // "ⓡ" -> "r"
   accent_map[0x24e2] = "s";   // "ⓢ" -> "s"
   accent_map[0x24e3] = "t";   // "ⓣ" -> "t"
   accent_map[0x24e4] = "u";   // "ⓤ" -> "u"
   accent_map[0x24e5] = "v";   // "ⓥ" -> "v"
   accent_map[0x24e6] = "w";   // "ⓦ" -> "w"
   accent_map[0x24e7] = "x";   // "ⓧ" -> "x"
   accent_map[0x24e8] = "y";   // "ⓨ" -> "y"
   accent_map[0x24e9] = "z";   // "ⓩ" -> "z"
   accent_map[0x2c60] = "L";   // "Ⱡ" -> "L"
   accent_map[0x2c61] = "l";   // "ⱡ" -> "l"
   accent_map[0x2c62] = "L";   // "Ɫ" -> "L"
   accent_map[0x2c63] = "P";   // "Ᵽ" -> "P"
   accent_map[0x2c64] = "R";   // "Ɽ" -> "R"
   accent_map[0x2c65] = "a";   // "ⱥ" -> "a"
   accent_map[0x2c66] = "t";   // "ⱦ" -> "t"
   accent_map[0x2c67] = "H";   // "Ⱨ" -> "H"
   accent_map[0x2c68] = "h";   // "ⱨ" -> "h"
   accent_map[0x2c69] = "K";   // "Ⱪ" -> "K"
   accent_map[0x2c6a] = "k";   // "ⱪ" -> "k"
   accent_map[0x2c6b] = "Z";   // "Ⱬ" -> "Z"
   accent_map[0x2c6c] = "z";   // "ⱬ" -> "z"
   accent_map[0x2c6e] = "M";   // "Ɱ" -> "M"
   accent_map[0x2c71] = "v";   // "ⱱ" -> "v"
   accent_map[0x2c72] = "W";   // "Ⱳ" -> "W"
   accent_map[0x2c73] = "w";   // "ⱳ" -> "w"
   accent_map[0x2c74] = "v";   // "ⱴ" -> "v"
   accent_map[0x2c78] = "e";   // "ⱸ" -> "e"
   accent_map[0x2c7a] = "o";   // "ⱺ" -> "o"
   accent_map[0x2c7c] = "j";   // "ⱼ" -> "j"
   accent_map[0xa740] = "K";   // "Ꝁ" -> "K"
   accent_map[0xa741] = "k";   // "ꝁ" -> "k"
   accent_map[0xa742] = "K";   // "Ꝃ" -> "K"
   accent_map[0xa743] = "k";   // "ꝃ" -> "k"
   accent_map[0xa744] = "K";   // "Ꝅ" -> "K"
   accent_map[0xa745] = "k";   // "ꝅ" -> "k"
   accent_map[0xa748] = "L";   // "Ꝉ" -> "L"
   accent_map[0xa749] = "l";   // "ꝉ" -> "l"
   accent_map[0xa74a] = "O";   // "Ꝋ" -> "O"
   accent_map[0xa74b] = "o";   // "ꝋ" -> "o"
   accent_map[0xa74c] = "O";   // "Ꝍ" -> "O"
   accent_map[0xa74d] = "o";   // "ꝍ" -> "o"
   accent_map[0xa750] = "P";   // "Ꝑ" -> "P"
   accent_map[0xa751] = "p";   // "ꝑ" -> "p"
   accent_map[0xa752] = "P";   // "Ꝓ" -> "P"
   accent_map[0xa753] = "p";   // "ꝓ" -> "p"
   accent_map[0xa754] = "P";   // "Ꝕ" -> "P"
   accent_map[0xa755] = "p";   // "ꝕ" -> "p"
   accent_map[0xa756] = "Q";   // "Ꝗ" -> "Q"
   accent_map[0xa757] = "q";   // "ꝗ" -> "q"
   accent_map[0xa758] = "Q";   // "Ꝙ" -> "Q"
   accent_map[0xa759] = "q";   // "ꝙ" -> "q"
   accent_map[0xa75a] = "R";   // "Ꝛ" -> "R"
   accent_map[0xa75b] = "r";   // "ꝛ" -> "r"
   accent_map[0xa75e] = "V";   // "Ꝟ" -> "V"
   accent_map[0xa75f] = "v";   // "ꝟ" -> "v"
   accent_map[0xff21] = "A";   // "Ａ" -> "A"
   accent_map[0xff22] = "B";   // "Ｂ" -> "B"
   accent_map[0xff23] = "C";   // "Ｃ" -> "C"
   accent_map[0xff24] = "D";   // "Ｄ" -> "D"
   accent_map[0xff25] = "E";   // "Ｅ" -> "E"
   accent_map[0xff26] = "F";   // "Ｆ" -> "F"
   accent_map[0xff27] = "G";   // "Ｇ" -> "G"
   accent_map[0xff28] = "H";   // "Ｈ" -> "H"
   accent_map[0xff29] = "I";   // "Ｉ" -> "I"
   accent_map[0xff2a] = "J";   // "Ｊ" -> "J"
   accent_map[0xff2b] = "K";   // "Ｋ" -> "K"
   accent_map[0xff2c] = "L";   // "Ｌ" -> "L"
   accent_map[0xff2d] = "M";   // "Ｍ" -> "M"
   accent_map[0xff2e] = "N";   // "Ｎ" -> "N"
   accent_map[0xff2f] = "O";   // "Ｏ" -> "O"
   accent_map[0xff30] = "P";   // "Ｐ" -> "P"
   accent_map[0xff31] = "Q";   // "Ｑ" -> "Q"
   accent_map[0xff32] = "R";   // "Ｒ" -> "R"
   accent_map[0xff33] = "S";   // "Ｓ" -> "S"
   accent_map[0xff34] = "T";   // "Ｔ" -> "T"
   accent_map[0xff35] = "U";   // "Ｕ" -> "U"
   accent_map[0xff36] = "V";   // "Ｖ" -> "V"
   accent_map[0xff37] = "W";   // "Ｗ" -> "W"
   accent_map[0xff38] = "X";   // "Ｘ" -> "X"
   accent_map[0xff39] = "Y";   // "Ｙ" -> "Y"
   accent_map[0xff3a] = "Z";   // "Ｚ" -> "Z"
   accent_map[0xff41] = "a";   // "ａ" -> "a"
   accent_map[0xff42] = "b";   // "ｂ" -> "b"
   accent_map[0xff43] = "c";   // "ｃ" -> "c"
   accent_map[0xff44] = "d";   // "ｄ" -> "d"
   accent_map[0xff45] = "e";   // "ｅ" -> "e"
   accent_map[0xff46] = "f";   // "ｆ" -> "f"
   accent_map[0xff47] = "g";   // "ｇ" -> "g"
   accent_map[0xff48] = "h";   // "ｈ" -> "h"
   accent_map[0xff49] = "i";   // "ｉ" -> "i"
   accent_map[0xff4a] = "j";   // "ｊ" -> "j"
   accent_map[0xff4b] = "k";   // "ｋ" -> "k"
   accent_map[0xff4c] = "l";   // "ｌ" -> "l"
   accent_map[0xff4d] = "m";   // "ｍ" -> "m"
   accent_map[0xff4e] = "n";   // "ｎ" -> "n"
   accent_map[0xff4f] = "o";   // "ｏ" -> "o"
   accent_map[0xff50] = "p";   // "ｐ" -> "p"
   accent_map[0xff51] = "q";   // "ｑ" -> "q"
   accent_map[0xff52] = "r";   // "ｒ" -> "r"
   accent_map[0xff53] = "s";   // "ｓ" -> "s"
   accent_map[0xff54] = "t";   // "ｔ" -> "t"
   accent_map[0xff55] = "u";   // "ｕ" -> "u"
   accent_map[0xff56] = "v";   // "ｖ" -> "v"
   accent_map[0xff57] = "w";   // "ｗ" -> "w"
   accent_map[0xff58] = "x";   // "ｘ" -> "x"
   accent_map[0xff59] = "y";   // "ｙ" -> "y"
   accent_map[0xff5a] = "z";   // "ｚ" -> "z"
}

static int apply_unicode_charmap(const unicodecharmap_t& umap, QoreString& str, const QoreString& src, ExceptionSink* xsink) {
   assert(str.empty());
   assert(str.getEncoding() == src.getEncoding());

   //printd(5, "apply_unicode_map() source: '%s' (%s)\n", src.getBuffer(), src.getEncoding()->getCode());

   for (const char* p = src.getBuffer(), *e = p + src.size(); p < e; ++p) {
      // if we discover a non-ASCII character, then we have to start worrying about conversions
      if ((*p) & 0x80) {
         unsigned len;
         unsigned uc = src.getUnicodePointFromBytePos(p - src.getBuffer(), len, xsink);
         if (*xsink)
            return -1;
         // see if there is a mapping
         unicodecharmap_t::const_iterator i = umap.find(uc);
         // if the character was not found, then just add the original character
         if (i == umap.end()) {
            //printd(5, "apply_unicode_charmap() no match found for %x (%d)\n", uc, ulmap.size());
            for (unsigned j = 0; j < len; ++j) {
               str.concat(*(p + j));
            }
         }
         else {
            // otherwise concatenate the new character
            str.concat(i->second);
         }
         p += (len - 1);
         continue;
      }
      str.concat(*p);
   }

   return 0;
}

int do_unaccent(QoreString& str, const QoreString& src, ExceptionSink* xsink) {
   return apply_unicode_charmap(accent_map, str, src, xsink);
}

//! returns the ASCII lower-case mapping of the given ASCII character
static int q_ascii_tolower(int c) {
    return c > 64 && c < 91 ? c + 32 : c;
}

//! returns the ASCII upper-case mapping of the given ASCII character
static int q_ascii_toupper(int c) {
    return c > 96 && c < 123 ? c - 32 : c;
}

//! returns the simple (1-to-1) case mapping for the codepoint or 0 if there is none
static unsigned q_find_simple_case_map(const q_simple_case_map_t* map, size_t len, unsigned cp) {
    size_t lo = 0;
    size_t hi = len;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) >> 1);
        if (map[mid].cp < cp) {
            lo = mid + 1;
        } else if (map[mid].cp > cp) {
            hi = mid;
        } else {
            return map[mid].mapped;
        }
    }
    return 0;
}

//! returns the full (1-to-many) case mapping for the codepoint or nullptr if there is none
static const q_full_case_map_t* q_find_full_case_map(const q_full_case_map_t* map, size_t len,
        unsigned cp) {
    size_t lo = 0;
    size_t hi = len;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) >> 1);
        if (map[mid].cp < cp) {
            lo = mid + 1;
        } else if (map[mid].cp > cp) {
            hi = mid;
        } else {
            return &map[mid];
        }
    }
    return nullptr;
}

//! returns True if the codepoint is covered by the given sorted list of inclusive ranges
static bool q_cp_in_ranges(const q_cp_range_t* ranges, size_t len, unsigned cp) {
    size_t lo = 0;
    size_t hi = len;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) >> 1);
        if (ranges[mid].last < cp) {
            lo = mid + 1;
        } else if (ranges[mid].first > cp) {
            hi = mid;
        } else {
            return true;
        }
    }
    return false;
}

//! returns the full case mapping for the codepoint in the given direction or nullptr if there is none
static const q_full_case_map_t* q_get_full_case_map(bool upper, unsigned cp) {
    return upper
        ? q_find_full_case_map(q_full_upper_map, QORE_ARRAY_SIZE(q_full_upper_map), cp)
        : q_find_full_case_map(q_full_lower_map, QORE_ARRAY_SIZE(q_full_lower_map), cp);
}

//! returns True if the codepoint has the Unicode \c Cased property
static bool q_unicode_is_cased(unsigned cp) {
    return q_cp_in_ranges(q_cased_ranges, QORE_ARRAY_SIZE(q_cased_ranges), cp);
}

//! returns True if the codepoint has the Unicode \c Case_Ignorable property
static bool q_unicode_is_case_ignorable(unsigned cp) {
    return q_cp_in_ranges(q_case_ignorable_ranges, QORE_ARRAY_SIZE(q_case_ignorable_ranges), cp);
}

//! updates the running "the last cased character seen was cased" state for the Final_Sigma condition
static void q_update_cased_state(unsigned cp, bool& prev_cased) {
    if (!q_unicode_is_case_ignorable(cp)) {
        prev_cased = q_unicode_is_cased(cp);
    }
}

unsigned q_unicode_tolower(unsigned cp) {
    unsigned rv = q_find_simple_case_map(q_simple_lower_map, QORE_ARRAY_SIZE(q_simple_lower_map), cp);
    return rv ? rv : cp;
}

unsigned q_unicode_toupper(unsigned cp) {
    unsigned rv = q_find_simple_case_map(q_simple_upper_map, QORE_ARRAY_SIZE(q_simple_upper_map), cp);
    return rv ? rv : cp;
}

//! implements the "after C" half of the Unicode Final_Sigma condition (Unicode 3.13)
/** @param src the source string
    @param pos the byte position immediately after the sigma
    @param xsink Qore-language exceptions are raised here

    @return True if the sigma is not followed by a cased character (ignoring any case-ignorable
    characters in between)
 */
static bool q_final_sigma_after(const QoreString& src, size_t pos, ExceptionSink* xsink) {
    size_t size = src.size();
    size_t scan_count = 0;
    while (pos < size) {
        if (++scan_count % 100 == 0 && qore_check_cancel(xsink, "string case conversion")) {
            return false;
        }
        unsigned len;
        unsigned cp = src.getUnicodePointFromBytePos(pos, len, xsink);
        if (*xsink) {
            return false;
        }
        assert(len);
        if (!len) {
            // cannot happen; guarantees loop progress in release builds
            len = 1;
        }
        if (!q_unicode_is_case_ignorable(cp)) {
            return !q_unicode_is_cased(cp);
        }
        pos += len;
    }
    return true;
}

//! concatenates the case mapping of a single source character to the target string
/** If the target encoding cannot represent the mapped character(s), the source character is
    copied verbatim instead, so that case conversion never fails on a valid string and never
    loses a character.

    @param str the target string
    @param src the source string
    @param pos the byte position of the source character in \a src
    @param len the length in bytes of the source character
    @param mapped the mapped codepoints
    @param mapped_len the number of mapped codepoints
    @param xsink Qore-language exceptions are raised here

    @return 0 for OK, -1 if a Qore-language exception was raised
 */
static int concat_case_mapped(QoreString& str, const QoreString& src, size_t pos, unsigned len,
        const unsigned* mapped, unsigned mapped_len, ExceptionSink* xsink) {
    // UTF-8 can represent every codepoint, so no fallback is possible or needed
    if (str.getEncoding() == QCS_UTF8) {
        for (unsigned i = 0; i < mapped_len; ++i) {
            if (str.concatUnicode(mapped[i], xsink)) {
                return -1;
            }
        }
        return 0;
    }

    size_t rollback = str.size();
    // encoding errors are handled here and must not reach the caller
    ExceptionSink enc_xsink;
    for (unsigned i = 0; i < mapped_len; ++i) {
        if (str.concatUnicode(mapped[i], &enc_xsink)) {
            enc_xsink.clear();
            str.terminate(rollback);
            str.concat(src.getBuffer() + pos, len);
            return 0;
        }
    }
    return 0;
}

//! applies the Unicode default full case mapping in the given direction
/** @param upper if True then the uppercase mapping is applied, otherwise the lowercase mapping
    @param str the target string; must be empty and have the same encoding as \a src
    @param src the source string
    @param xsink Qore-language exceptions are raised here

    @return 0 for OK, -1 if a Qore-language exception was raised
 */
static int apply_case_map(bool upper, QoreString& str, const QoreString& src, ExceptionSink* xsink) {
    assert(str.empty());
    assert(str.getEncoding() == src.getEncoding());

    const char* buf = src.getBuffer();
    size_t size = src.size();
    // True if the last non-case-ignorable character seen has the Cased property; this gives the
    // "before C" half of the Final_Sigma condition without having to scan backwards
    bool prev_cased = false;
    size_t scan_count = 0;

    for (size_t pos = 0; pos < size; ) {
        if (++scan_count % 100 == 0 && qore_check_cancel(xsink, "string case conversion")) {
            return -1;
        }
        // ASCII characters need no decoding in ASCII-compatible encodings
        if (!(buf[pos] & 0x80)) {
            unsigned char c = static_cast<unsigned char>(buf[pos]);
            str.concat(static_cast<char>(upper ? q_ascii_toupper(c) : q_ascii_tolower(c)));
            q_update_cased_state(c, prev_cased);
            ++pos;
            continue;
        }

        unsigned len;
        unsigned cp = src.getUnicodePointFromBytePos(pos, len, xsink);
        if (*xsink) {
            return -1;
        }
        assert(len);
        if (!len) {
            // cannot happen; guarantees loop progress in release builds
            len = 1;
        }

        // full (1-to-many) mappings take priority over the simple mappings
        const q_full_case_map_t* full = q_get_full_case_map(upper, cp);
        if (full) {
            if (concat_case_mapped(str, src, pos, len, full->mapped, full->len, xsink)) {
                return -1;
            }
        } else {
            unsigned mapped;
            // the only language-independent conditional mapping: a Greek capital sigma at the end
            // of a word lowercases to the final sigma
            if (!upper && cp == QORE_FINAL_SIGMA_CP && prev_cased
                && q_final_sigma_after(src, pos + len, xsink)) {
                mapped = QORE_FINAL_SIGMA_LOWER;
            } else {
                if (*xsink) {
                    return -1;
                }
                mapped = upper ? q_unicode_toupper(cp) : q_unicode_tolower(cp);
            }
            if (mapped == cp) {
                // no mapping: copy the source bytes verbatim
                str.concat(buf + pos, len);
            } else if (concat_case_mapped(str, src, pos, len, &mapped, 1, xsink)) {
                return -1;
            }
        }

        q_update_cased_state(cp, prev_cased);
        pos += len;
    }

    return 0;
}

//! the size in bytes of the UTF-8 encoding of the given codepoint
static size_t qore_utf8_unicode_size(unsigned code) {
    return code > 0xffff ? 4 : code > 0x7ff ? 3 : code > 0x7f ? 2 : 1;
}

//! returns the size of the case mapping of \a src without materializing the result
/** @param upper if True then the uppercase mapping is measured, otherwise the lowercase mapping
    @param src the source string
    @param characters if True then the result is in characters, otherwise in bytes
    @param result the size of the transformed string is returned here
    @param xsink Qore-language exceptions are raised here

    @return 0 for OK, -1 if a Qore-language exception was raised
 */
static int apply_case_map_measure(bool upper, const QoreString& src, bool characters,
        int64_t& result, ExceptionSink* xsink) {
    const char* buf = src.getBuffer();
    size_t size = src.size();
    size_t scan_count = 0;

    // ASCII case mapping never changes the size, so a pure ASCII string can be answered immediately
    size_t pos = 0;
    while (pos < size && !(buf[pos] & 0x80)) {
        if (++scan_count % 100 == 0 && qore_check_cancel(xsink, "string case measurement")) {
            return -1;
        }
        ++pos;
    }
    if (pos == size) {
        result = static_cast<int64_t>(size);
        return 0;
    }

    // the sizes below assume UTF-8 output; with any other encoding the only reliable way to get
    // the size is to perform the transformation
    if (src.getEncoding() != QCS_UTF8) {
        QoreString transformed(src.getEncoding());
        if (apply_case_map(upper, transformed, src, xsink)) {
            return -1;
        }
        result = static_cast<int64_t>(characters ? transformed.length() : transformed.size());
        return 0;
    }

    size_t count = pos;
    while (pos < size) {
        if (++scan_count % 100 == 0 && qore_check_cancel(xsink, "string case measurement")) {
            return -1;
        }
        if (!(buf[pos] & 0x80)) {
            ++count;
            ++pos;
            continue;
        }
        unsigned len;
        unsigned cp = src.getUnicodePointFromBytePos(pos, len, xsink);
        if (*xsink) {
            return -1;
        }
        assert(len);
        if (!len) {
            // cannot happen; guarantees loop progress in release builds
            len = 1;
        }
        const q_full_case_map_t* full = q_get_full_case_map(upper, cp);
        if (full) {
            if (characters) {
                count += full->len;
            } else {
                for (unsigned i = 0; i < full->len; ++i) {
                    count += qore_utf8_unicode_size(full->mapped[i]);
                }
            }
        } else {
            // the Final_Sigma condition maps U+03A3 to U+03C2, which has the same encoded size as
            // the simple mapping U+03C3, so the condition does not have to be evaluated here
            unsigned mapped = upper ? q_unicode_toupper(cp) : q_unicode_tolower(cp);
            count += characters ? 1 : mapped == cp ? len : qore_utf8_unicode_size(mapped);
        }
        pos += len;
    }
    result = static_cast<int64_t>(count);
    return 0;
}

int do_tolower(QoreString& str, const QoreString& src, ExceptionSink* xsink) {
    return apply_case_map(false, str, src, xsink);
}

int do_toupper(QoreString& str, const QoreString& src, ExceptionSink* xsink) {
    return apply_case_map(true, str, src, xsink);
}

int do_tolower_measure(const QoreString& src, bool characters, int64_t& result,
        ExceptionSink* xsink) {
    return apply_case_map_measure(false, src, characters, result, xsink);
}

int do_toupper_measure(const QoreString& src, bool characters, int64_t& result,
        ExceptionSink* xsink) {
    return apply_case_map_measure(true, src, characters, result, xsink);
}
