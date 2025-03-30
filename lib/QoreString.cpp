/*
    QoreString.cpp

    QoreString Class Definition

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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
#include "qore/intern/qore_string_private.h"
#include "qore/intern/IconvHelper.h"
#include "qore/intern/StringReaderHelper.h"
#include "qore/intern/QoreRegexSubst.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iconv.h>
#include <map>
#include <memory>
#include <set>
#include <string>

// to be used for trim
static intvec_t default_whitespace = {
    ' ', '\t', '\n', '\r', '\v',
};

struct code_table {
    char symbol;
    const char* code;
    unsigned len;
};

// complete set of characters to percent-encode (RFC 3986 http://tools.ietf.org/html/rfc3986)
static int_set_t url_reserved;

// maps from entity strings to unicode code points
typedef std::map<std::string, uint32_t> emap_t;
// entity map from strings to unicode code points
static emap_t emap;

// maps from unicode code points to known entity references
typedef std::map<uint32_t, std::string> smap_t;
// entity map from unicode code points to strings
static smap_t smap;

struct unicode_entity {
    // unicode code point for symbol
    uint32_t symbol;
    // entity string
    std::string entity;
};

// contains the 252 character entities in HTML + &apos; for XML
static const struct unicode_entity xhtml_entity_list[] = {
    {34, "quot"},         // HTML 2.0: double quotation mark
    {38, "amp"},          // HTML 2.0: ampersand
    {39, "apos"},         // XHTML 1.0: apostrophe
    {60, "lt"},           // HTML 2.0: less-than sign
    {62, "gt"},           // HTML 2.0: greater-than sign
    {160, "nbsp"},        // HTML 3.2: no-break space (non-breaking space)[d]
    {161, "iexcl"},       // HTML 3.2: inverted exclamation mark
    {162, "cent"},        // HTML 3.2: cent sign
    {163, "pound"},       // HTML 3.2: pound sign
    {164, "curren"},      // HTML 3.2: currency sign
    {165, "yen"},         // HTML 3.2: yen sign (yuan sign)
    {166, "brvbar"},      // HTML 3.2: broken bar (broken vertical bar)
    {167, "sect"},        // HTML 3.2: section sign
    {168, "uml"},         // HTML 3.2: diaeresis (spacing diaeresis); see Germanic umlaut
    {169, "copy"},        // HTML 3.2: copyright symbol
    {170, "ordf"},        // HTML 3.2: feminine ordinal indicator
    {171, "laquo"},       // HTML 3.2: left-pointing double angle quotation mark (left pointing guillemet)
    {172, "not"},         // HTML 3.2: not sign
    {173, "shy"},         // HTML 3.2: soft hyphen (discretionary hyphen)
    {174, "reg"},         // HTML 3.2: registered sign (registered trademark symbol)
    {175, "macr"},        // HTML 3.2: macron (spacing macron, overline, APL overbar)
    {176, "deg"},         // HTML 3.2: degree symbol
    {177, "plusmn"},      // HTML 3.2: plus-minus sign (plus-or-minus sign)
    {178, "sup2"},        // HTML 3.2: superscript two (superscript digit two, squared)
    {179, "sup3"},        // HTML 3.2: superscript three (superscript digit three, cubed)
    {180, "acute"},       // HTML 3.2: acute accent (spacing acute)
    {181, "micro"},       // HTML 3.2: micro sign
    {182, "para"},        // HTML 3.2: pilcrow sign (paragraph sign)
    {183, "middot"},      // HTML 3.2: middle dot (Georgian comma, Greek middle dot)
    {184, "cedil"},       // HTML 3.2: cedilla (spacing cedilla)
    {185, "sup1"},        // HTML 3.2: superscript one (superscript digit one)
    {186, "ordm"},        // HTML 3.2: masculine ordinal indicator
    {187, "raquo"},       // HTML 3.2: right-pointing double angle quotation mark (right pointing guillemet)
    {188, "frac14"},      // HTML 3.2: vulgar fraction one quarter (fraction one quarter)
    {189, "frac12"},      // HTML 3.2: vulgar fraction one half (fraction one half)
    {190, "frac34"},      // HTML 3.2: vulgar fraction three quarters (fraction three quarters)
    {191, "iquest"},      // HTML 3.2: inverted question mark (turned question mark)
    {192, "Agrave"},      // HTML 2.0: Latin capital letter A with grave accent (Latin capital letter A grave)
    {193, "Aacute"},      // HTML 2.0: Latin capital letter A with acute accent
    {194, "Acirc"},       // HTML 2.0: Latin capital letter A with circumflex
    {195, "Atilde"},      // HTML 2.0: Latin capital letter A with tilde
    {196, "Auml"},        // HTML 2.0: Latin capital letter A with diaeresis
    {197, "Aring"},       // HTML 2.0: Latin capital letter A with ring above (Latin capital letter A ring)
    {198, "AElig"},       // HTML 2.0: Latin capital letter AE (Latin capital ligature AE)
    {199, "Ccedil"},      // HTML 2.0: Latin capital letter C with cedilla
    {200, "Egrave"},      // HTML 2.0: Latin capital letter E with grave accent
    {201, "Eacute"},      // HTML 2.0: Latin capital letter E with acute accent
    {202, "Ecirc"},       // HTML 2.0: Latin capital letter E with circumflex
    {203, "Euml"},        // HTML 2.0: Latin capital letter E with diaeresis
    {204, "Igrave"},      // HTML 2.0: Latin capital letter I with grave accent
    {205, "Iacute"},      // HTML 2.0: Latin capital letter I with acute accent
    {206, "Icirc"},       // HTML 2.0: Latin capital letter I with circumflex
    {207, "Iuml"},        // HTML 2.0: Latin capital letter I with diaeresis
    {208, "ETH"},         // HTML 2.0: Latin capital letter Eth
    {209, "Ntilde"},      // HTML 2.0: Latin capital letter N with tilde
    {210, "Ograve"},      // HTML 2.0: Latin capital letter O with grave accent
    {211, "Oacute"},      // HTML 2.0: Latin capital letter O with acute accent
    {212, "Ocirc"},       // HTML 2.0: Latin capital letter O with circumflex
    {213, "Otilde"},      // HTML 2.0: Latin capital letter O with tilde
    {214, "Ouml"},        // HTML 2.0: Latin capital letter O with diaeresis
    {215, "times"},       // HTML 3.2: multiplication sign
    {216, "Oslash"},      // HTML 2.0: Latin capital letter O with stroke (Latin capital letter O slash)
    {217, "Ugrave"},      // HTML 2.0: Latin capital letter U with grave accent
    {218, "Uacute"},      // HTML 2.0: Latin capital letter U with acute accent
    {219, "Ucirc"},       // HTML 2.0: Latin capital letter U with circumflex
    {220, "Uuml"},        // HTML 2.0: Latin capital letter U with diaeresis
    {221, "Yacute"},      // HTML 2.0: Latin capital letter Y with acute accent
    {222, "THORN"},       // HTML 2.0: Latin capital letter THORN
    {223, "szlig"},       // HTML 2.0: Latin small letter sharp s (ess-zed); see German Eszett
    {224, "agrave"},      // HTML 2.0: Latin small letter a with grave accent
    {225, "aacute"},      // HTML 2.0: Latin small letter a with acute accent
    {226, "acirc"},       // HTML 2.0: Latin small letter a with circumflex
    {227, "atilde"},      // HTML 2.0: Latin small letter a with tilde
    {228, "auml"},        // HTML 2.0: Latin small letter a with diaeresis
    {229, "aring"},       // HTML 2.0: Latin small letter a with ring above
    {230, "aelig"},       // HTML 2.0: Latin small letter ae (Latin small ligature ae)
    {231, "ccedil"},      // HTML 2.0: Latin small letter c with cedilla
    {232, "egrave"},      // HTML 2.0: Latin small letter e with grave accent
    {233, "eacute"},      // HTML 2.0: Latin small letter e with acute accent
    {234, "ecirc"},       // HTML 2.0: Latin small letter e with circumflex
    {235, "euml"},        // HTML 2.0: Latin small letter e with diaeresis
    {236, "igrave"},      // HTML 2.0: Latin small letter i with grave accent
    {237, "iacute"},      // HTML 2.0: Latin small letter i with acute accent
    {238, "icirc"},       // HTML 2.0: Latin small letter i with circumflex
    {239, "iuml"},        // HTML 2.0: Latin small letter i with diaeresis
    {240, "eth"},         // HTML 2.0: Latin small letter eth
    {241, "ntilde"},      // HTML 2.0: Latin small letter n with tilde
    {242, "ograve"},      // HTML 2.0: Latin small letter o with grave accent
    {243, "oacute"},      // HTML 2.0: Latin small letter o with acute accent
    {244, "ocirc"},       // HTML 2.0: Latin small letter o with circumflex
    {245, "otilde"},      // HTML 2.0: Latin small letter o with tilde
    {246, "ouml"},        // HTML 2.0: Latin small letter o with diaeresis
    {247, "divide"},      // HTML 3.2: division sign (obelus)
    {248, "oslash"},      // HTML 2.0: Latin small letter o with stroke (Latin small letter o slash)
    {249, "ugrave"},      // HTML 2.0: Latin small letter u with grave accent
    {250, "uacute"},      // HTML 2.0: Latin small letter u with acute accent
    {251, "ucirc"},       // HTML 2.0: Latin small letter u with circumflex
    {252, "uuml"},        // HTML 2.0: Latin small letter u with diaeresis
    {253, "yacute"},      // HTML 2.0: Latin small letter y with acute accent
    {254, "thorn"},       // HTML 2.0: Latin small letter thorn
    {255, "yuml"},        // HTML 2.0: Latin small letter y with diaeresis
    {338, "OElig"},       // HTML 4.0: Latin capital ligature oe[e]
    {339, "oelig"},       // HTML 4.0: Latin small ligature oe[e]
    {352, "Scaron"},      // HTML 4.0: Latin capital letter s with caron
    {353, "scaron"},      // HTML 4.0: Latin small letter s with caron
    {376, "Yuml"},        // HTML 4.0: Latin capital letter y with diaeresis
    {402, "fnof"},        // HTML 4.0: Latin small letter f with hook (function, florin)
    {710, "circ"},        // HTML 4.0: modifier letter circumflex accent
    {732, "tilde"},       // HTML 4.0: small tilde
    {913, "Alpha"},       // HTML 4.0: Greek capital letter Alpha
    {914, "Beta"},        // HTML 4.0: Greek capital letter Beta
    {915, "Gamma"},       // HTML 4.0: Greek capital letter Gamma
    {916, "Delta"},       // HTML 4.0: Greek capital letter Delta
    {917, "Epsilon"},     // HTML 4.0: Greek capital letter Epsilon
    {918, "Zeta"},        // HTML 4.0: Greek capital letter Zeta
    {919, "Eta"},         // HTML 4.0: Greek capital letter Eta
    {920, "Theta"},       // HTML 4.0: Greek capital letter Theta
    {921, "Iota"},        // HTML 4.0: Greek capital letter Iota
    {922, "Kappa"},       // HTML 4.0: Greek capital letter Kappa
    {923, "Lambda"},      // HTML 4.0: Greek capital letter Lambda
    {924, "Mu"},          // HTML 4.0: Greek capital letter Mu
    {925, "Nu"},          // HTML 4.0: Greek capital letter Nu
    {926, "Xi"},          // HTML 4.0: Greek capital letter Xi
    {927, "Omicron"},     // HTML 4.0: Greek capital letter Omicron
    {928, "Pi"},          // HTML 4.0: Greek capital letter Pi
    {929, "Rho"},         // HTML 4.0: Greek capital letter Rho
    {931, "Sigma"},       // HTML 4.0: Greek capital letter Sigma
    {932, "Tau"},         // HTML 4.0: Greek capital letter Tau
    {933, "Upsilon"},     // HTML 4.0: Greek capital letter Upsilon
    {934, "Phi"},         // HTML 4.0: Greek capital letter Phi
    {935, "Chi"},         // HTML 4.0: Greek capital letter Chi
    {936, "Psi"},         // HTML 4.0: Greek capital letter Psi
    {937, "Omega"},       // HTML 4.0: Greek capital letter Omega
    {945, "alpha"},       // HTML 4.0: Greek small letter alpha
    {946, "beta"},        // HTML 4.0: Greek small letter beta
    {947, "gamma"},       // HTML 4.0: Greek small letter gamma
    {948, "delta"},       // HTML 4.0: Greek small letter delta
    {949, "epsilon"},     // HTML 4.0: Greek small letter epsilon
    {950, "zeta"},        // HTML 4.0: Greek small letter zeta
    {951, "eta"},         // HTML 4.0: Greek small letter eta
    {952, "theta"},       // HTML 4.0: Greek small letter theta
    {953, "iota"},        // HTML 4.0: Greek small letter iota
    {954, "kappa"},       // HTML 4.0: Greek small letter kappa
    {955, "lambda"},      // HTML 4.0: Greek small letter lambda
    {956, "mu"},          // HTML 4.0: Greek small letter mu
    {957, "nu"},          // HTML 4.0: Greek small letter nu
    {958, "xi"},          // HTML 4.0: Greek small letter xi
    {959, "omicron"},     // HTML 4.0: Greek small letter omicron
    {960, "pi"},          // HTML 4.0: Greek small letter pi
    {961, "rho"},         // HTML 4.0: Greek small letter rho
    {962, "sigmaf"},      // HTML 4.0: Greek small letter final sigma
    {963, "sigma"},       // HTML 4.0: Greek small letter sigma
    {964, "tau"},         // HTML 4.0: Greek small letter tau
    {965, "upsilon"},     // HTML 4.0: Greek small letter upsilon
    {966, "phi"},         // HTML 4.0: Greek small letter phi
    {967, "chi"},         // HTML 4.0: Greek small letter chi
    {968, "psi"},         // HTML 4.0: Greek small letter psi
    {969, "omega"},       // HTML 4.0: Greek small letter omega
    {977, "thetasym"},    // HTML 4.0: Greek theta symbol
    {978, "upsih"},       // HTML 4.0: Greek Upsilon with hook symbol
    {982, "piv"},         // HTML 4.0: Greek pi symbol
    {8194, "ensp"},       // HTML 4.0: en space[d]
    {8195, "emsp"},       // HTML 4.0: em space[d]
    {8201, "thinsp"},     // HTML 4.0: thin space[d]
    {8204, "zwnj"},       // HTML 4.0: zero-width non-joiner
    {8205, "zwj"},        // HTML 4.0: zero-width joiner
    {8206, "lrm"},        // HTML 4.0: left-to-right mark
    {8207, "rlm"},        // HTML 4.0: right-to-left mark
    {8211, "ndash"},      // HTML 4.0: en dash
    {8212, "mdash"},      // HTML 4.0: em dash
    {8216, "lsquo"},      // HTML 4.0: left single quotation mark
    {8217, "rsquo"},      // HTML 4.0: right single quotation mark
    {8218, "sbquo"},      // HTML 4.0: single low-9 quotation mark
    {8220, "ldquo"},      // HTML 4.0: left double quotation mark
    {8221, "rdquo"},      // HTML 4.0: right double quotation mark
    {8222, "bdquo"},      // HTML 4.0: double low-9 quotation mark
    {8224, "dagger"},     // HTML 4.0: dagger, obelisk
    {8225, "Dagger"},     // HTML 4.0: double dagger, double obelisk
    {8226, "bull"},       // HTML 4.0: bullet (black small circle)[f]
    {8230, "hellip"},     // HTML 4.0: horizontal ellipsis (three dot leader)
    {8240, "permil"},     // HTML 4.0: per mille sign
    {8242, "prime"},      // HTML 4.0: prime (minutes, feet)
    {8243, "Prime"},      // HTML 4.0: double prime (seconds, inches)
    {8249, "lsaquo"},     // HTML 4.0: single left-pointing angle quotation mark[g]
    {8250, "rsaquo"},     // HTML 4.0: single right-pointing angle quotation mark[g]
    {8254, "oline"},      // HTML 4.0: overline (spacing overscore)
    {8260, "frasl"},      // HTML 4.0: fraction slash (solidus)
    {8364, "euro"},       // HTML 4.0: euro sign
    {8465, "image"},      // HTML 4.0: black-letter capital I (imaginary part)
    {8472, "weierp"},     // HTML 4.0: script capital P (power set, Weierstrass p)
    {8476, "real"},       // HTML 4.0: black-letter capital R (real part symbol)
    {8482, "trade"},      // HTML 4.0: trademark symbol
    {8501, "alefsym"},    // HTML 4.0: alef symbol (first transfinite cardinal)[h]
    {8592, "larr"},       // HTML 4.0: leftwards arrow
    {8593, "uarr"},       // HTML 4.0: upwards arrow
    {8594, "rarr"},       // HTML 4.0: rightwards arrow
    {8595, "darr"},       // HTML 4.0: downwards arrow
    {8596, "harr"},       // HTML 4.0: left right arrow
    {8629, "crarr"},      // HTML 4.0: downwards arrow with corner leftwards (carriage return)
    {8656, "lArr"},       // HTML 4.0: leftwards double arrow[i]
    {8657, "uArr"},       // HTML 4.0: upwards double arrow
    {8658, "rArr"},       // HTML 4.0: rightwards double arrow[j]
    {8659, "dArr"},       // HTML 4.0: downwards double arrow
    {8660, "hArr"},       // HTML 4.0: left right double arrow
    {8704, "forall"},     // HTML 4.0: for all
    {8706, "part"},       // HTML 4.0: partial differential
    {8707, "exist"},      // HTML 4.0: there exists
    {8709, "empty"},      // HTML 4.0: empty set (null set); see also U+8960, ⌀
    {8711, "nabla"},      // HTML 4.0: del or nabla (vector differential operator)
    {8712, "isin"},       // HTML 4.0: element of
    {8713, "notin"},      // HTML 4.0: not an element of
    {8715, "ni"},         // HTML 4.0: contains as member
    {8719, "prod"},       // HTML 4.0: n-ary product (product sign)[k]
    {8721, "sum"},        // HTML 4.0: n-ary summation[l]
    {8722, "minus"},      // HTML 4.0: minus sign
    {8727, "lowast"},     // HTML 4.0: asterisk operator
    {8730, "radic"},      // HTML 4.0: square root (radical sign)
    {8733, "prop"},       // HTML 4.0: proportional to
    {8734, "infin"},      // HTML 4.0: infinity
    {8736, "ang"},        // HTML 4.0: angle
    {8743, "and"},        // HTML 4.0: logical and (wedge)
    {8744, "or"},         // HTML 4.0: logical or (vee)
    {8745, "cap"},        // HTML 4.0: intersection (cap)
    {8746, "cup"},        // HTML 4.0: union (cup)
    {8747, "int"},        // HTML 4.0: integral
    {8756, "there4"},     // HTML 4.0: therefore sign
    {8764, "sim"},        // HTML 4.0: tilde operator (varies with, similar to)[m]
    {8773, "cong"},       // HTML 4.0: congruent to
    {8776, "asymp"},      // HTML 4.0: almost equal to (asymptotic to)
    {8800, "ne"},         // HTML 4.0: not equal to
    {8801, "equiv"},      // HTML 4.0: identical to; sometimes used for 'equivalent to'
    {8804, "le"},         // HTML 4.0: less-than or equal to
    {8805, "ge"},         // HTML 4.0: greater-than or equal to
    {8834, "sub"},        // HTML 4.0: subset of
    {8835, "sup"},        // HTML 4.0: superset of[n]
    {8836, "nsub"},       // HTML 4.0: not a subset of
    {8838, "sube"},       // HTML 4.0: subset of or equal to
    {8839, "supe"},       // HTML 4.0: superset of or equal to
    {8853, "oplus"},      // HTML 4.0: circled plus (direct sum)
    {8855, "otimes"},     // HTML 4.0: circled times (vector product)
    {8869, "perp"},       // HTML 4.0: up tack (orthogonal to, perpendicular)[o]
    {8901, "sdot"},       // HTML 4.0: dot operator[p]
    {8968, "lceil"},      // HTML 4.0: left ceiling (APL upstile)
    {8969, "rceil"},      // HTML 4.0: right ceiling
    {8970, "lfloor"},     // HTML 4.0: left floor (APL downstile)
    {8971, "rfloor"},     // HTML 4.0: right floor
    {9001, "lang"},       // HTML 4.0: left-pointing angle bracket (bra)[q]
    {9002, "rang"},       // HTML 4.0: right-pointing angle bracket (ket)[r]
    {9674, "loz"},        // HTML 4.0: lozenge
    {9824, "spades"},     // HTML 4.0: black spade suit[f]
    {9827, "clubs"},      // HTML 4.0: black club suit (shamrock)[f]
    {9829, "hearts"},     // HTML 4.0: black heart suit (valentine)[f]
    {9830, "diams"},      // HTML 4.0: black diamond suit[f]
};

#define HTML_ASCII(c) (c==34||c==38||c==60||c==62)
#define XML_ASCII(c) (HTML_ASCII(c)||c==39)

#define NUM_ENTITIES (sizeof(xhtml_entity_list) / sizeof (struct unicode_entity))

static const struct code_table html_codes[] = {
    { '&', "&amp;", 5 },
    { '<', "&lt;", 4 },
    { '>', "&gt;", 4 },
    { '"', "&quot;", 6 },
};

#define NUM_HTML_CODES (sizeof(html_codes) / sizeof (struct code_table))

// for binary searches of non-spacing unicode characters
typedef std::map<int, int> intmap_t;

// non-spacing ranges - {end, start} so std::map::lower_bound() can be used
static intmap_t non_spacing_map = {
    {0x0036f, 0x00300}, {0x00486, 0x00483}, {0x00489, 0x00488}, {0x005bd, 0x00591},
    {0x005bf, 0x005bf}, {0x005c2, 0x005c1}, {0x005c5, 0x005c4}, {0x005c7, 0x005c7},
    {0x00603, 0x00600}, {0x00615, 0x00610}, {0x0065e, 0x0064b}, {0x00670, 0x00670},
    {0x006e4, 0x006d6}, {0x006e8, 0x006e7}, {0x006ed, 0x006ea}, {0x0070f, 0x0070f},
    {0x00711, 0x00711}, {0x0074a, 0x00730}, {0x007b0, 0x007a6}, {0x007f3, 0x007eb},
    {0x00902, 0x00901}, {0x0093c, 0x0093c}, {0x00948, 0x00941}, {0x0094d, 0x0094d},
    {0x00954, 0x00951}, {0x00963, 0x00962}, {0x00981, 0x00981}, {0x009bc, 0x009bc},
    {0x009c4, 0x009c1}, {0x009cd, 0x009cd}, {0x009e3, 0x009e2}, {0x00a02, 0x00a01},
    {0x00a3c, 0x00a3c}, {0x00a42, 0x00a41}, {0x00a48, 0x00a47}, {0x00a4d, 0x00a4b},
    {0x00a71, 0x00a70}, {0x00a82, 0x00a81}, {0x00abc, 0x00abc}, {0x00ac5, 0x00ac1},
    {0x00ac8, 0x00ac7}, {0x00acd, 0x00acd}, {0x00ae3, 0x00ae2}, {0x00b01, 0x00b01},
    {0x00b3c, 0x00b3c}, {0x00b3f, 0x00b3f}, {0x00b43, 0x00b41}, {0x00b4d, 0x00b4d},
    {0x00b56, 0x00b56}, {0x00b82, 0x00b82}, {0x00bc0, 0x00bc0}, {0x00bcd, 0x00bcd},
    {0x00c40, 0x00c3e}, {0x00c48, 0x00c46}, {0x00c4d, 0x00c4a}, {0x00c56, 0x00c55},
    {0x00cbc, 0x00cbc}, {0x00cbf, 0x00cbf}, {0x00cc6, 0x00cc6}, {0x00ccd, 0x00ccc},
    {0x00ce3, 0x00ce2}, {0x00d43, 0x00d41}, {0x00d4d, 0x00d4d}, {0x00dca, 0x00dca},
    {0x00dd4, 0x00dd2}, {0x00dd6, 0x00dd6}, {0x00e31, 0x00e31}, {0x00e3a, 0x00e34},
    {0x00e4e, 0x00e47}, {0x00eb1, 0x00eb1}, {0x00eb9, 0x00eb4}, {0x00ebc, 0x00ebb},
    {0x00ecd, 0x00ec8}, {0x00f19, 0x00f18}, {0x00f35, 0x00f35}, {0x00f37, 0x00f37},
    {0x00f39, 0x00f39}, {0x00f7e, 0x00f71}, {0x00f84, 0x00f80}, {0x00f87, 0x00f86},
    {0x00f97, 0x00f90}, {0x00fbc, 0x00f99}, {0x00fc6, 0x00fc6}, {0x01030, 0x0102d},
    {0x01032, 0x01032}, {0x01037, 0x01036}, {0x01039, 0x01039}, {0x01059, 0x01058},
    {0x011ff, 0x01160}, {0x0135f, 0x0135f}, {0x01714, 0x01712}, {0x01734, 0x01732},
    {0x01753, 0x01752}, {0x01773, 0x01772}, {0x017b5, 0x017b4}, {0x017bd, 0x017b7},
    {0x017c6, 0x017c6}, {0x017d3, 0x017c9}, {0x017dd, 0x017dd}, {0x0180d, 0x0180b},
    {0x018a9, 0x018a9}, {0x01922, 0x01920}, {0x01928, 0x01927}, {0x01932, 0x01932},
    {0x0193b, 0x01939}, {0x01a18, 0x01a17}, {0x01b03, 0x01b00}, {0x01b34, 0x01b34},
    {0x01b3a, 0x01b36}, {0x01b3c, 0x01b3c}, {0x01b42, 0x01b42}, {0x01b73, 0x01b6b},
    {0x01dca, 0x01dc0}, {0x01dff, 0x01dfe}, {0x0200f, 0x0200b}, {0x0202e, 0x0202a},
    {0x02063, 0x02060}, {0x0206f, 0x0206a}, {0x020ef, 0x020d0}, {0x0302f, 0x0302a},
    {0x0309a, 0x03099}, {0x0a806, 0x0a806}, {0x0a80b, 0x0a80b}, {0x0a826, 0x0a825},
    {0x0fb1e, 0x0fb1e}, {0x0fe0f, 0x0fe00}, {0x0fe23, 0x0fe20}, {0x0feff, 0x0feff},
    {0x0fffb, 0x0fff9}, {0x10a03, 0x10a01}, {0x10a06, 0x10a05}, {0x10a0f, 0x10a0c},
    {0x10a3a, 0x10a38}, {0x10a3f, 0x10a3f}, {0x1d169, 0x1d167}, {0x1d182, 0x1d173},
    {0x1d18b, 0x1d185}, {0x1d1ad, 0x1d1aa}, {0x1d244, 0x1d242}, {0xe0001, 0xe0001},
    {0xe007f, 0xe0020}, {0xe01ef, 0xe0100},
};

void qore_string_init() {
    static int url_reserved_list[] = {
        '!', '*', '\'', '(',
        ')', ';', ':', '@',
        '&', '=', '+', '$',
        ',', '/', '?', '#',
        '[', ']', '%', '\'',
    };
#define URLIST_SIZE (sizeof(url_reserved_list) / sizeof(int))

    for (unsigned i = 0; i < URLIST_SIZE; ++i)
        url_reserved.insert(url_reserved_list[i]);

    for (unsigned i = 0; i < NUM_ENTITIES; ++i) {
        assert(emap.find(xhtml_entity_list[i].entity) == emap.end());
        emap[xhtml_entity_list[i].entity] = xhtml_entity_list[i].symbol;
        assert(smap.find(xhtml_entity_list[i].symbol) == smap.end());
        smap[xhtml_entity_list[i].symbol] = xhtml_entity_list[i].entity;
    }
}

static unsigned get_unicode_from_utf8(const char* buf, unsigned bl) {
    if (bl == 1)
        return buf[0];

    if (bl == 2)
        return ((buf[0] & 0x1f) << 6)
            | (buf[1] & 0x3f);

    if (bl == 3)
        return ((buf[0] & 0x0f) << 12)
            | ((buf[1] & 0x3f) << 6)
            | (buf[2] & 0x3f);

    return (((unsigned)(buf[0] & 0x07)) << 18)
        | (((unsigned)(buf[1] & 0x3f)) << 12)
        | ((((unsigned)buf[2] & 0x3f)) << 6)
        | (((unsigned)buf[3] & 0x3f));
}

// based on: https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c by Markus Kuhn -- 2007-05-26 (Unicode 5.0)
size_t qore_get_unicode_character_width(int ucs) {
    // see if character is a non-spacing character or control character
    if (!ucs || (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))) {
        return 0;
    }

    intmap_t::const_iterator i = non_spacing_map.lower_bound(ucs);
    if (i != non_spacing_map.end()
        && (i->first == ucs || i->second <= ucs)) {
        return 0;
    }

    return 1 +
        (ucs >= 0x1100 &&
            (ucs <= 0x115f ||                    /* Hangul Jamo init. consonants */
                ucs == 0x2329 || ucs == 0x232a ||
                (ucs >= 0x2e80 && ucs <= 0xa4cf && ucs != 0x303f)
                ||                  /* CJK ... Yi */
                (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
                (ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
                (ucs >= 0xfe10 && ucs <= 0xfe19) || /* Vertical forms */
                (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
                (ucs >= 0xff00 && ucs <= 0xff60) || /* Fullwidth Forms */
                (ucs >= 0xffe0 && ucs <= 0xffe6) ||
                (ucs >= 0x20000 && ucs <= 0x2fffd) ||
                (ucs >= 0x30000 && ucs <= 0x3fffd)));
}

static void base64_concat(qore_string_private& str, unsigned char c, size_t& linelen, size_t maxlinelen,
        bool url_encode = false) {
    c = table64[c];
    if (url_encode) {
        if (c == '+') {
            str.concat('-');
        } else if (c == '/') {
            str.concat('_');
        } else {
            str.concat(c);
        }
        // ignore maxlinelen when url_encode == true
        return;
    }
    str.concat(c);
    ++linelen;
    if (maxlinelen > 0 && linelen == maxlinelen) {
        str.concat("\r\n");
        linelen = 0;
    }
}

void qore_string_private::concatBase64(const char* bbuf, size_t size, size_t maxlinelen, bool url_encode) {
    //printd(0, "bbuf=%p, size=" QSD "\n", bbuf, size);
    if (!size) {
        return;
    }

    size_t linelen = 0;

    unsigned char* p = (unsigned char*)bbuf;
    unsigned char* endbuf = p + size;
    while (p < endbuf) {
        // get 6 bits of data
        unsigned char c = p[0] >> 2;

        // byte 1: concat 1st 6-bit value
        base64_concat(*this, c, linelen, maxlinelen, url_encode);

        // byte 1: use remaining 2 bits in low order position
        c = (p[0] & 3) << 4;

        // check end
        if ((endbuf - p) == 1) {
            base64_concat(*this, c, linelen, maxlinelen, url_encode);
            if (!url_encode) {
                concat("==");
            }
            break;
        }

        // byte 2: get 4 bits to make next 6-bit unit
        c |= p[1] >> 4;

        // concat 2nd 6-bit value
        base64_concat(*this, c, linelen, maxlinelen, url_encode);

        // byte 2: get 4 low bits
        c = (p[1] & 15) << 2;

        if ((endbuf - p) == 2) {
            base64_concat(*this, c, linelen, maxlinelen, url_encode);
            if (!url_encode) {
                concat('=');
            }
            break;
        }

        // byte 3: get high 2 bits to make next 6-bit unit
        c |= p[2] >> 6;

        // concat 3rd 6-bit value
        base64_concat(*this, c, linelen, maxlinelen, url_encode);

        // byte 3: concat final 6 bits
        base64_concat(*this, p[2] & 63, linelen, maxlinelen, url_encode);
        p += 3;
    }
}

void qore_string_private::concatUTF8FromUnicode(unsigned code) {
    if (code > 0xffff) { // 4-byte code
        concat(0xf0 | ((code & (0x7 << 18)) >> 18));
        concat(0x80 | ((code & (0x3f << 12)) >> 12));
        concat(0x80 | ((code & (0x3f << 6)) >> 6));
        concat(0x80 | (code & 0x3f));
    } else if (code > 0x7ff) { // 3-byte code
        concat(0xe0 | ((code & (0xf << 12)) >> 12));
        concat(0x80 | ((code & (0x3f << 6)) >> 6));
        concat(0x80 | (code & 0x3f));
    } else if (code > 0x7f) { // 2-byte code
        concat(0xc0 | ((code & (0x2f << 6)) >> 6));
        concat(0x80 | (code & 0x3f));
    } else
        concat((char)code);
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
int qore_string_private::concatDecodeUriIntern(ExceptionSink* xsink, const qore_string_private& str,
        bool detect_query) {
    if (!getEncoding()->isAsciiCompat()) {
        xsink->raiseException("UNSUPPORTED-ENCODING", "cannot decode a URI to non-ASCII-compatible encoding \"%s\"",
            getEncoding()->getCode());
        return -1;
    }

    bool in_query = false;

    const char* url = str.buf;
    while (*url) {
        int x1 = getHex(url);
        if (x1 >= 0) {
            // see if a multi-byte char is starting
            if ((x1 & 0xc0) == 0xc0) {
                int x2 = getHex(url);
                if (x2 == -1) {
                    xsink->raiseException("URL-ENCODING-ERROR", "percent encoding indicated a multi-byte UTF-8 "
                        "character but only one byte was provided");
                    return -1;
                }
                // check for a 3-byte sequence
                if (x1 & 0x20) {
                    int x3 = getHex(url);
                    if (x3 == -1) {
                        xsink->raiseException("URL-ENCODING-ERROR", "percent encoding indicated a %s-byte UTF-8 "
                            "character but only two bytes were provided", (x1 & 0x10) ? "four" : "three");
                        return -1;
                    }

                    // check for a 4-byte sequence
                    if (x1 & 0x10) {
                        int x4 = getHex(url);
                        if (x4 == -1) {
                            xsink->raiseException("URL-ENCODING-ERROR", "percent encoding indicated a four-byte "
                                "UTF-8 character but only three bytes were provided");
                            return -1;
                        }
                        if (!(x2 & 0x80 && x3 & 0x80 && x4 & 0x80)) {
                            xsink->raiseException("URL-ENCODING-ERROR", "percent encoding gave an invalid four-byte "
                                "UTF-8 character");
                            return -1;
                        }

                        // if utf8 then concat directly
                        if (encoding == QCS_UTF8) {
                            concat((char)x1);
                            concat((char)x2);
                            concat((char)x3);
                            concat((char)x4);
                        } else {
                            unsigned cp = (((unsigned)(x1 & 0x07)) << 18)
                                | (((unsigned)(x2 & 0x3f)) << 12)
                                | ((((unsigned)x3 & 0x3f)) << 6)
                                | (((unsigned)x4 & 0x3f));
                            if (concatUnicode(cp, xsink))
                                return -1;
                        }
                        continue;
                    }
                    // 3-byte sequence
                    if (!(x2 & 0x80 && x3 & 0x80)) {
                        xsink->raiseException("URL-ENCODING-ERROR", "percent encoding gave an invalid three-byte "
                            "UTF-8 character");
                        return -1;
                    }

                    // if utf8 then concat directly
                    if (encoding == QCS_UTF8) {
                        concat((char)x1);
                        concat((char)x2);
                        concat((char)x3);
                    } else {
                        unsigned cp = (((unsigned)(x1 & 0x0f)) << 12)
                            | (((unsigned)(x2 & 0x3f)) << 6)
                            | (((unsigned)x3 & 0x3f));

                        if (concatUnicode(cp, xsink))
                            return -1;
                    }
                    continue;
                }
                // 2-byte sequence
                if (!(x2 & 0x80)) {
                    xsink->raiseException("URL-ENCODING-ERROR", "percent encoding gave an invalid two-byte UTF-8 "
                        "character");
                    return -1;
                }

                // if utf8 then concat directly
                if (encoding == QCS_UTF8) {
                    concat((char)x1);
                    concat((char)x2);
                } else {
                    unsigned cp = (((unsigned)(x1 & 0x1f)) << 6)
                        | ((unsigned)(x2 & 0x3f));
                    if (concatUnicode(cp, xsink))
                        return -1;
                }
                continue;
            }
            // single byte
            concat(x1);
            continue;
        }

        if (detect_query) {
            if (!in_query) {
                if ((*url) == '?')
                    in_query = true;
            } else {
                if ((*url) == '+') {
                    ++url;
                    concat(' ');
                    continue;
                } else if ((*url) == '#') {
                    in_query = false;
                }
            }
        }

        concat(*(url++));
    }
    return 0;
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
int qore_string_private::concatEncodeUriRequest(ExceptionSink* xsink, const qore_string_private& str) {
    assert(str.len);

    if (!getEncoding()->isAsciiCompat()) {
        xsink->raiseException("UNSUPPORTED-ENCODING", "cannot encode a URI to non-ASCII-compatible encoding \"%s\"",
            getEncoding()->getCode());
        return -1;
    }

    int state = QUS_PATH;

    const unsigned char* p = (const unsigned char*)str.buf;
    while (*p) {
        if ((*p) == '%')
            concat("%25");
        else if (*p > 127) {
            size_t len = q_UTF8_get_char_len((const char*)p, str.len - ((const char*)p - str.buf));
            if (len <= 0) {
                xsink->raiseException("INVALID-ENCODING", "invalid UTF-8 getEncoding() found in string");
                return -1;
            }
            // add UTF-8 percent-encoded characters
            for (size_t i = 0; i < len; ++i)
                sprintf("%%%X", (unsigned)p[i]);
            p += len;
            continue;
        } else if (state == QUS_PATH) {
            if ((*p) == '?') {
                state = QUS_QUERY;
                concat(*p);
            } else if ((*p) == '#') {
                state = QUS_FRAGMENT;
                concat(*p);
            } else if ((*p) == ' ')
                concat("%20");
            else
                concat(*p);
        } else if (state == QUS_QUERY) {
            if ((*p) == ' ')
                concat('+');
            else if ((*p) == '+')
                concat("%2b");
            else
                concat(*p);
        } else {
            assert(state == QUS_PATH);
            if ((*p) == ' ')
                concat("%20");
            else
                concat(*p);
        }

        ++p;
    }

    return 0;
}

// static function
int qore_string_private::convert_encoding_intern(const char* src, size_t src_len, const QoreEncoding* from,
        QoreString& targ, const QoreEncoding* nccs, ExceptionSink* xsink) {
    assert(targ.priv->getEncoding() == nccs);
    assert(targ.empty());

    //printd(5, "qore_string_private::convert_getEncoding()_intern() %s -> %s len: " QSD " src='%s'\n", from->getCode(), nccs->getCode(), src_len, src);

    IconvHelper c(nccs, from, xsink);
    if (xsink && *xsink)
        return -1;

    // now convert value
    size_t al = src_len + STR_CLASS_BLOCK;
    targ.allocate(al + 1);
    while (true) {
        size_t ilen = src_len;
        size_t olen = al;
        char* ib = (char*)src;
        char* ob = targ.priv->buf;
        size_t rc = c.iconv(&ib, &ilen, &ob, &olen);
        if (rc == (size_t)-1) {
            switch (errno) {
                case EINVAL:
                case EILSEQ:
                    c.reportIllegalSequence(ib - src, xsink);
                    targ.clear();
                    return -1;
                case E2BIG:
                    al += STR_CLASS_BLOCK;
                    targ.allocate(al + 1);
                    break;
                default: {
                    c.reportUnknownError(xsink);
                    targ.clear();
                    return -1;
                }
            }
        } else if (rc > 0) {
            // issue #2500 non-reversible encoding executed (i.e. source char does not exist in target encoding)
            c.reportIllegalSequence(ib - src, xsink);
            targ.clear();
            return -1;
        } else {
            // terminate string
            targ.priv->buf[al - olen] = '\0';
            targ.priv->len = al - olen;
            break;
        }
    }
    /*
    // remove byte order markers at the beginning of UTF16 strings
    if (nccs == QCS_UTF16 && targ.priv->len >= 2 && (signed char)targ.priv->buf[0] == -2 && (signed char)targ.priv->buf[1] == -1)
        targ.priv->splice_simple(0, 2);
    */
    return 0;
}

unsigned int qore_string_private::getUnicodePointFromBytePos(size_t offset, unsigned& clen, ExceptionSink* xsink) const {
    // gets the unicode code point
    return getEncoding()->getUnicode(buf + offset, buf + len, clen, xsink);
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
int qore_string_private::concatEncode(ExceptionSink* xsink, const QoreString& str, unsigned code) {
    //printd(5, "qore_string_private::concatEncode() '%s' code: 0x%x\n", str.c_str(), code);

    // if it's not a null string
    if (!str.priv->len)
        return 0;

    // if no encoding should be done, then just concat normally
    if (!code) {
        concat(&str, xsink);
        return *xsink ? -1 : 0;
    }

    if (!getEncoding()->isAsciiCompat()) {
        xsink->raiseException("UNSUPPORTED-ENCODING", "cannot encode to non-ASCII-compatible encoding \"%s\"",
            getEncoding()->getCode());
        return -1;
    }

    qore_string_private* p = 0;
    TempEncodingHelper cstr;

    // convert getEncoding() if we are not going to encode all non-ascii characters anyway
    if (!(code & CE_NONASCII)) {
        if (!cstr.set(&str, getEncoding(), xsink))
            return -1;
        p = cstr->priv;
    } else {
        if (!str.priv->getEncoding()->isAsciiCompat()) {
            xsink->raiseException("UNSUPPORTED-ENCODING", "cannot encode from non-ASCII-compatible encoding \"%s\"",
                str.priv->getEncoding()->getCode());
            return -1;
        }
        p = str.priv;
    }

    //printd(5, "qore_string_private::concatEncode() p: %p '%s' len: %d\n", p, p->buf, p->len);

    allocate(len + p->len + p->len / 10 + 10); // avoid reallocations inside the loop, value guesstimated
    for (size_t i = 0; i < p->len; ++i) {
        // see if we are dealing with a non-ascii character
        const unsigned char c = p->buf[i];
        if ((c & 0x80)) {
            unsigned len = 0;
            unsigned cp = p->getUnicodePointFromBytePos(i, len, xsink);
            if (*xsink)
                return -1;
            if ((code & CE_HTML)) {
                assert(len);
                assert(cp);
                // see if it's an known special character
                smap_t::iterator si = smap.find(cp);
                //printd(5, "qore_string_private::concatEncode() i: %u c: %u cp: %u len: %u found: %s (%s)\n", i, c,
                //    cp, len, si != smap.end() ? "true" : "false", getEncoding()->getCode());
                if (si != smap.end()) {
                    sprintf("&%s;", si->second.c_str());
                    i += len - 1;
                    continue;
                }
            }

            if (code & CE_NONASCII) {
                // otherwise just output the character entity
                sprintf("&#%u;", cp);
                i += len - 1;
                continue;
            }
        } else if (((code & CE_HTML) && HTML_ASCII(c))
                || ((code & CE_XML) && XML_ASCII(c))) {
            smap_t::iterator it = smap.find(c);
            assert(it != smap.end());
            concat('&');
            concat(it->second.c_str());
            concat(';');
            continue;
        }

        concat(p->buf[i]);
    }
    return 0;
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
int qore_string_private::concatDecode(ExceptionSink* xsink, const QoreString& str, unsigned code) {
   // if it's not a null string
   if (!str.priv->len)
      return 0;

   // if no decoding should be done, then just concat normally
   if (!code) {
      concat(&str, xsink);
      return *xsink ? -1 : 0;
   }

   const QoreEncoding* enc = getEncoding();
   if (!enc->isAsciiCompat()) {
       xsink->raiseException("UNSUPPORTED-ENCODING", "cannot decode to non-ASCII-compatible encoding \"%s\"", enc->getCode());
       return -1;
   }

   TempEncodingHelper cstr(str, enc, xsink);
   if (!cstr)
      return -1;

   qore_string_private* p = cstr->priv;
   //printd(5, "qore_string_private::concatDecode() p: %p '%s' len: %d\n", p, p->buf, p->len);

   // decode all entity references
   bool dall = (code & CD_XHTML) == CD_XHTML;

   // try to avoid reallocations inside the loop
   allocate(len + p->len + 1);
   for (size_t i = 0; i < p->len; ++i) {
      // see if we are dealing with a non-ascii character
      const char* s = p->buf + i;
      if (*s != '&') {
         concat(*s);
         continue;
      }
      // concatenate translated character
      // check for unicode character references
      if ((*(s + 1) == '#')) {
         if (code & CD_NUM_REF) {
            s += 2;
            // find end of character sequence
            const char* e = strchr(s, ';');
            // if not found or the number is too big, then don't try to decode it
            if (e && (e - s) < 8) {
               unsigned code;
               if (*s == 'x')
                  code = strtoul(s + 1, 0, 16);
               else
                  code = strtoul(s, 0, 10);

               if (!concatUnicode(code)) {
                  i = e - p->buf;
                  continue;
               }
               // error occurred, so back out
               s -= 2;
            }
         }
      } else if (isascii(*(s + 1)) && (code & CD_XHTML)) {
         s += 1;
         // find end of character sequence
         const char* e = strchr(s, ';');
         // if not found or the number is too big, then don't try to decode it
         if (e && (e - s) < 16) {
            // find unicode code point
            std::string entity(s, e - s);
            //printd(0, "entity: %s\n", entity.c_str());
            emap_t::iterator it = emap.find(entity);
            if (it != emap.end()) {
               bool ok = dall;
               // if XML only
               if (!ok && !(code & CD_HTML))
                  ok = it->second < 0x80;
               // if HTML only
               else if (!ok && !(code & CD_XML))
                  ok = it->second != 0x27;
               else
                  ok = true;
               if (ok && !concatUnicode(it->second)) {
                  i = e - p->buf;
                  continue;
               }
            }
         }
         // not found or error occurred concatenating char, so back out
         s -= 1;
      }

      // concatentate ASCII characters directly
      if ((unsigned char)(*s) < 0x80) {
         concat(*s);
         continue;
      }
      // concatenate character as a single unit
      size_t cl = enc->getByteLen(s, p->buf + p->len, 1, xsink);
      if (*xsink)
         return -1;
      concat_intern(s, cl);
      i += cl - 1;
   }

   return 0;
}

int qore_string_private::concat(const QoreString* str, ExceptionSink* xsink) {
    //printd(5, "concat() buf='%s' str='%s'\n", buf, str->priv->buf);
    // if it's not a null string
    if (!str || !str->priv->len)
        return 0;

    TempEncodingHelper cstr(str, getEncoding(), xsink);
    if (*xsink)
        return -1;

    // if priv->buffer needs to be resized
    check_char(cstr->priv->len + len + STR_CLASS_EXTRA);
    // concatenate new string
    memcpy(buf + len, cstr->priv->buf, cstr->priv->len);
    len += cstr->priv->len;
    buf[len] = '\0';
    return 0;
}

int qore_string_private::concatUnicode(unsigned code) {
    if (getEncoding() == QCS_UTF8) {
        concatUTF8FromUnicode(code);
        return 0;
    }
    QoreString tmp(QCS_UTF8);
    tmp.priv->concatUTF8FromUnicode(code);

    ExceptionSink xsink;

    TempString ns(tmp.convertEncoding(getEncoding(), &xsink));
    if (xsink) {
        // ignore exceptions
        xsink.clear();
        return -1;
    }

    concat(ns);
    return 0;
}

int qore_string_private::trimLeading(ExceptionSink* xsink, const intvec_t& cvec) {
    size_t i = 0;

    // trim default whitespace
    while (i < len) {
        // get unicode code point of first character in string
        unsigned clen;
        int c = getUnicodePointFromBytePos(i, clen, xsink);
        if (*xsink)
            return -1;
        // see if the character is in the character vector
        if (c && !inVector(c, cvec))
            break;
        i += clen;
    }

    memmove(buf, buf + i, len + 1 - i);
    len -= i;
    return 0;
}

int qore_string_private::trimLeading(ExceptionSink* xsink, const qore_string_private* chars) {
    if (!len)
        return 0;

    if (!chars)
        return trimLeading(xsink, default_whitespace);

    // get a list of unicode points for the characters in chars
    intvec_t cvec;
    if (chars->getUnicodeCharArray(cvec, xsink))
        return -1;

    return trimLeading(xsink, cvec);
}

int qore_string_private::trimTrailing(ExceptionSink* xsink, const intvec_t& cvec) {
    // get length of string in characters
    size_t i = getEncoding()->getLength(buf, buf + len, xsink);
    if (*xsink)
        return -1;
    assert(i);

    while (i) {
        --i;
        // get byte offset for the last character
        size_t bpos = getEncoding()->getByteLen(buf, buf + len, i, xsink);
        if (*xsink)
            return -1;
        unsigned clen;
        // get the unicode point for the last character in the string
        int c = getUnicodePointFromBytePos(bpos, clen, xsink);
        if (*xsink)
            return -1;
        // see if the character is in the character vector
        if (c && !inVector(c, cvec))
            break;
        terminate(bpos);
    }

    return 0;
}

int qore_string_private::trimTrailing(ExceptionSink* xsink, const qore_string_private* chars) {
    if (!len)
        return 0;

    if (!chars)
        return trimTrailing(xsink, default_whitespace);

    // get a list of unicode points for the characters in chars
    intvec_t cvec;
    if (chars->getUnicodeCharArray(cvec, xsink))
        return -1;

    return trimTrailing(xsink, cvec);
}

void qore_string_private::terminate(size_t size) {
    if (size > len)
        check_char(size);
    len = size;
    buf[size] = '\0';
}

int qore_string_private::substr_simple(QoreString* ns, qore_offset_t offset, qore_offset_t length) const {
    printd(5, "qore_string_private::substr_simple(offset=" QSD ", length=" QSD ") string=\"%s\" (this=%p len="
        QSD ")\n", offset, length, buf, this, len);

    size_t n_offset;
    if (offset < 0)
        n_offset = len + offset;
    else
        n_offset = offset;
    if (n_offset >= len)  // if offset outside of string, return nothing
        return -1;

    size_t n_length;
    if (length < 0) {
        length = len - n_offset + length;
        if (length < 0)
            n_length = 0;
        else
            n_length = length;
    } else if ((size_t)length > (len - n_offset))
        n_length = len - n_offset;
    else
        n_length = length;

    ns->concat(buf + n_offset, n_length);
    return 0;
}

int qore_string_private::substr_simple(QoreString* ns, qore_offset_t offset) const {
    printd(5, "qore_string_private::substr_simple(offset=" QSD ") string=\"%s\" (this=%p len=" QSD ")\n",
            offset, buf, this, len);

    size_t n_offset;
    if (offset < 0)
        n_offset = len + offset;
    else
        n_offset = offset;
    if (n_offset >= len)  // if offset outside of string, return nothing
        return -1;

    // add length to ensure that the entire string is copied even if it has embedded nulls
    ns->concat(buf + n_offset, len - n_offset);
    return 0;
}

int qore_string_private::substr_complex(QoreString* ns, qore_offset_t offset, qore_offset_t length,
        ExceptionSink* xsink) const {
    assert(xsink);
    QORE_TRACE("qore_string_private::substr_complex(offset, length)");
    printd(5, "qore_string_private::substr_complex(offset=" QSD ", length=" QSD ") string=\"%s\" (this=%p len="
        QSD ")\n", offset, length, buf, this, len);

    char* pend = buf + len;
    if (offset < 0) {
        int clength = getEncoding()->getLength(buf, pend, xsink);
        if (*xsink)
            return -1;

        offset = clength + offset;

        if ((offset < 0) || (offset >= clength))  // if offset outside of string, return nothing
            return -1;
    }

    size_t start = getEncoding()->getByteLen(buf, pend, offset, xsink);
    if (*xsink)
        return -1;

    if (start == len)
        return -1;

    if (length < 0) {
        length = getEncoding()->getLength(buf + start, pend, xsink) + length;
        if (*xsink)
            return -1;

        if (length < 0)
            length = 0;
    }
    size_t end = getEncoding()->getByteLen(buf + start, pend, length, xsink);
    if (*xsink)
        return -1;

    ns->concat(buf + start, end);
    return 0;
}

int qore_string_private::substr_complex(QoreString* ns, qore_offset_t offset, ExceptionSink* xsink) const {
    assert(xsink);
    //printd(5, "qore_string_private::substr_complex(offset=" QSD ") string=\"%s\" (this=%p len=" QSD ")\n", offset, buf, this, len);
    char* pend = buf + len;
    if (offset < 0) {
        size_t clength = getEncoding()->getLength(buf, pend, xsink);
        if (*xsink)
            return -1;

        offset = clength + offset;

        if ((offset < 0) || ((size_t)offset >= clength)) {  // if offset outside of string, return nothing
            //printd(5, "this=%p, len=" QSD ", offset=" QSD ", clength=" QSD ", buf=%s\n", this, len, offset, clength, buf);
            return -1;
        }
    }

    size_t start = getEncoding()->getByteLen(buf, pend, offset, xsink);
    if (*xsink)
        return -1;

    //printd(5, "offset=" QSD ", start=" QSD "\n", offset, start);
    if (start == len) {
        //printd(5, "this=%p, len=" QSD ", offset=" QSD ", buf=%p, start=" QSD ", %s\n", this, len, offset, buf, start, buf);
        return -1;
    }

    // calculate byte offset
    ns->concat(buf + start, len - start);
    return 0;
}

void qore_string_private::splice_simple(size_t offset, size_t num, QoreString* extract) {
    //printd(5, "splice_intern(offset=" QSD ", num=" QSD ", len=" QSD ")\n", offset, num, len);
    size_t end;
    if (num > (len - offset)) {
        end = len;
        num = len - offset;
    } else {
        end = offset + num;
    }

    // add to extract string if any
    if (extract && num) {
        extract->concat(buf + offset, num);
    }

    // move down entries if necessary
    if (end != len) {
        memmove(buf + offset, buf + end, sizeof(char) * (len - end));
    }

    // calculate new length
    len -= num;
    // set last entry to NULL
    buf[len] = '\0';
}

void qore_string_private::splice_simple(size_t offset, size_t num, const char* str, size_t str_len,
        QoreString* extract) {
    //printd(5, "splice_intern(offset=" QSD ", num=" QSD ", len=" QSD ")\n", offset, num, len);

    size_t end;
    if (num > (len - offset)) {
        end = len;
        num = len - offset;
    } else {
        end = offset + num;
    }

    // add to extract string if any
    if (extract && num) {
        extract->concat(buf + offset, num);
    }

    // get number of entries to insert
    if (str_len > num) { // make bigger
        size_t ol = len;
        check_char(len + str_len - num);
        // move trailing entries forward if necessary
        if (end != ol) {
            assert(ol == len);
            memmove(buf + offset + str_len, buf + offset + num, ol - end);
        }
    } else if (num > str_len) { // make list smaller
        memmove(buf + offset + str_len, buf + offset + num, sizeof(char) * (len - offset - str_len));
    }

    memcpy(buf + offset, str, str_len);

    // calculate new length
    len = len - num + str_len;
    // set last entry to NULL
    buf[len] = '\0';
}

void qore_string_private::splice_complex(qore_offset_t offset, ExceptionSink* xsink, QoreString* extract) {
    assert(xsink);
    // get length in chars
    size_t clen = getEncoding()->getLength(buf, buf + len, xsink);
    if (*xsink)
        return;

    //printd(0, "splice_complex(offset=" QSD ") clen=" QSD "\n", offset, clen);
    if (offset < 0) {
        offset = clen + offset;
        if (offset < 0)
            offset = 0;
    } else if ((size_t)offset >= clen)
        return;

    // calculate byte offset
    size_t n_offset = offset ? getEncoding()->getByteLen(buf, buf + len, offset, xsink) : 0;
    if (*xsink)
        return;

    // add to extract string if any
    if (extract && n_offset < len)
        extract->concat(buf + n_offset);

    // truncate string at offset
    len = n_offset;
    buf[len] = '\0';
}

void qore_string_private::splice_complex(qore_offset_t offset, qore_offset_t num, ExceptionSink* xsink,
        QoreString* extract) {
    assert(xsink);
    //printd(5, "splice_complex(offset=" QSD ", num=" QSD ", len=" QSD ")\n", offset, num, len);

    // get length in chars
    size_t clen = getEncoding()->getLength(buf, buf + len, xsink);
    if (*xsink)
        return;

    if (offset < 0) {
        offset = clen + offset;
        if (offset < 0)
            offset = 0;
    } else if ((size_t)offset >= clen)
        return;

    if (num < 0) {
        num = clen + num - offset;
        if (num < 0)
            num = 0;
    }

    size_t end;
    if ((size_t)num > (clen - offset)) {
        end = clen;
        num = clen - offset;
    } else
        end = offset + num;

    // get character positions
    offset = getEncoding()->getByteLen(buf, buf + len, offset, xsink);
    if (*xsink)
        return;

    end = getEncoding()->getByteLen(buf, buf + len, end, xsink);
    if (*xsink)
        return;

    num = getEncoding()->getByteLen(buf + offset, buf + len, num, xsink);
    if (*xsink)
        return;

    // add to extract string if any
    if (extract && num)
        extract->concat(buf + offset, num);

    // move down entries if necessary
    if (end != len)
        memmove(buf + offset, buf + end, sizeof(char) * (len - end));

    // calculate new length
    len -= num;

    // set last entry to NULL
    buf[len] = '\0';
}

void qore_string_private::splice_complex(qore_offset_t offset, qore_offset_t num, const QoreString* str,
        ExceptionSink* xsink, QoreString* extract) {
    assert(xsink);
    // get length in chars
    size_t clen = getEncoding()->getLength(buf, buf + len, xsink);
    if (*xsink)
        return;

    //printd(5, "splice_complex(offset=" QSD ", num=" QSD ", str='%s', len=" QSD ") clen=" QSD
    //    " buf='%s'\n", offset, num, str->c_str(), len, clen, buf);

    if (offset >= (qore_offset_t)clen)
        offset = clen;
    else if (offset < 0) {
        offset = clen + offset;
        if (offset < 0)
            offset = 0;
    }

    if (num < 0) {
        num = clen + num - offset;
        if (num < 0)
            num = 0;
    }

    size_t end;
    if ((size_t)num > (clen - offset)) {
        end = clen;
        num = clen - offset;
    } else
        end = offset + num;

    // get character positions
    char* endp = buf + len;
    offset = getEncoding()->getByteLen(buf, endp, offset, xsink);
    if (*xsink)
        return;

    end = getEncoding()->getByteLen(buf, endp, end, xsink);
    if (*xsink)
        return;

    num = getEncoding()->getByteLen(buf + offset, endp, num, xsink);
    if (*xsink)
        return;

    // add to extract string if any
    if (extract && num)
        extract->concat(buf + offset, num);

    //printd(5, "offset=" QSD ", end=" QSD ", num=" QSD "\n", offset, end, num);
    // get number of entries to insert
    if (str->priv->len > (size_t)num) { // make bigger
        size_t ol = len;
        check_char(len - num + str->priv->len);
        // move trailing entries forward if necessary
        //printd(5, "priv->buf='%s'(" QSD "), str='%s'(" QSD "), end=" QSD ", num=" QSD ", newlen=" QSD "\n", buf, ol,
        //    str->priv->buf, str->priv->len, end, num, len);
        if (end != ol)
            memmove(buf + (end - num + str->priv->len), buf + end, ol - end);
    } else if ((size_t)num > str->priv->len) // make string smaller
        memmove(buf + offset + str->priv->len, buf + offset + num, sizeof(char) * (len - offset - str->priv->len));

    memcpy(buf + offset, str->priv->buf, str->priv->len);

    // calculate new length
    len = len - num + str->priv->len;

    // set last entry to NULL
    buf[len] = '\0';
}

void qore_string_private::setRegexBaseOpts(QoreRegexBase& re, int opts) {
    if (opts & QS_RE_CASELESS) {
        re.setCaseInsensitive();
    }
    if (opts & QS_RE_DOTALL) {
        re.setDotAll();
    }
    if (opts & QS_RE_EXTENDED) {
        re.setExtended();
    }
    if (opts & QS_RE_MULTILINE) {
        re.setMultiline();
    }
}

void qore_string_private::setRegexOpts(QoreRegexSubst& re, int opts) {
    if (opts & QS_RE_GLOBAL) {
        re.setGlobal();
    }
    setRegexBaseOpts(re, opts);
}

QoreStringMaker::QoreStringMaker(const char* fmt, ...) {
    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = vsprintf(fmt, args);
        va_end(args);
        if (!rc)
            break;
    }
}

QoreStringMaker::QoreStringMaker(const QoreEncoding* enc, const char* fmt, ...) : QoreString(enc) {
    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = vsprintf(fmt, args);
        va_end(args);
        if (!rc)
            break;
    }
}

QoreString::QoreString() : priv(new qore_string_private) {
    priv->len = 0;
    priv->allocated = STR_CLASS_BLOCK;
    priv->buf = (char*)malloc(priv->allocated * sizeof(char));
    priv->buf[0] = '\0';
    priv->encoding = QCS_DEFAULT;
}

// FIXME: this is not very efficient with the array offsets...
QoreString::QoreString(const char* str) : priv(new qore_string_private) {
    priv->len = 0;
    priv->allocated = STR_CLASS_BLOCK;
    priv->buf = (char*)malloc(priv->allocated * sizeof(char));
    if (str) {
        while (str[priv->len]) {
            priv->check_char(priv->len);
            priv->buf[priv->len] = str[priv->len];
            priv->len++;
        }
        priv->check_char(priv->len);
        priv->buf[priv->len] = '\0';
    }
    else
        priv->buf[0] = '\0';
    priv->encoding = QCS_DEFAULT;
}

// FIXME: this is not very efficient with the array offsets...
QoreString::QoreString(const char* str, const QoreEncoding* new_qore_encoding) : priv(new qore_string_private) {
    priv->len = 0;
    priv->allocated = STR_CLASS_BLOCK;
    priv->buf = (char*)malloc(priv->allocated * sizeof(char));
    if (str) {
        while (str[priv->len]) {
            priv->check_char(priv->len);
            priv->buf[priv->len] = str[priv->len];
            priv->len++;
        }
        priv->check_char(priv->len);
        priv->buf[priv->len] = '\0';
    } else {
        priv->buf[0] = '\0';
    }
    priv->encoding = new_qore_encoding;
}

QoreString::QoreString(const std::string& str, const QoreEncoding* new_encoding) : priv(new qore_string_private) {
    priv->allocated = str.size() + 1 + STR_CLASS_BLOCK;
    priv->buf = (char*)malloc(priv->allocated * sizeof(char));
    memcpy(priv->buf, str.c_str(), str.size() + 1);
    priv->len = str.size();
    priv->encoding = new_encoding;
}

QoreString::QoreString(const QoreEncoding* new_qore_encoding) : priv(new qore_string_private) {
    priv->len = 0;
    priv->allocated = STR_CLASS_BLOCK;
    priv->buf = (char*)malloc(priv->allocated * sizeof(char));
    priv->buf[0] = '\0';
    priv->encoding = new_qore_encoding;
}

QoreString::QoreString(const char* str, size_t size, const QoreEncoding* new_qore_encoding) : priv(new qore_string_private) {
    priv->len = size;
    priv->allocated = size + STR_CLASS_EXTRA;
    priv->buf = (char*)malloc(sizeof(char) * priv->allocated);
    memcpy(priv->buf, str, size);
    priv->buf[size] = '\0';
    priv->encoding = new_qore_encoding;
}

QoreString::QoreString(const QoreString* str) : priv(new qore_string_private(*(str->priv))) {
}

QoreString::QoreString(const QoreString& str) : priv(new qore_string_private(*(str.priv))) {
}

QoreString::QoreString(const QoreString* str, size_t size) : priv(new qore_string_private) {
    if (size >= str->priv->len)
        size = str->priv->len;
    priv->len = size;
    priv->allocated = size + STR_CLASS_EXTRA;
    priv->buf = (char*)malloc(sizeof(char) * priv->allocated);
    if (size)
        memcpy(priv->buf, str->priv->buf, size);
    priv->buf[size] = '\0';
    priv->encoding = str->priv->encoding;
}

QoreString::QoreString(char c) : priv(new qore_string_private) {
    priv->len = 1;
    priv->allocated = STR_CLASS_BLOCK;
    priv->buf = (char*)malloc(sizeof(char) * priv->allocated);
    priv->buf[0] = c;
    priv->buf[1] = '\0';
    priv->encoding = QCS_DEFAULT;
}

QoreString::QoreString(int64 i) : priv(new qore_string_private) {
    priv->allocated = MAX_BIGINT_STRING_LEN + 1;
    priv->buf = (char*)malloc(sizeof(char) * priv->allocated);
    priv->len = ::snprintf(priv->buf, MAX_BIGINT_STRING_LEN, QLLD, i);
    // terminate string just in case
    priv->buf[MAX_BIGINT_STRING_LEN] = '\0';
    priv->encoding = QCS_DEFAULT;
}

QoreString::QoreString(bool b) : priv(new qore_string_private) {
    priv->allocated = 2;
    priv->buf = (char*)malloc(sizeof(char) * priv->allocated);
    priv->buf[0] = b ? '1' : '0';
    priv->buf[1] = 0;
    priv->len = 1;
    priv->encoding = QCS_DEFAULT;
}

QoreString::QoreString(double f) : priv(new qore_string_private) {
    priv->allocated = MAX_FLOAT_STRING_LEN + 1;
    priv->buf = (char*)malloc(sizeof(char) * priv->allocated);
    priv->len = ::snprintf(priv->buf, MAX_FLOAT_STRING_LEN, "%.9g", f);
    // snprintf() always terminates the string
    priv->encoding = QCS_DEFAULT;
    // issue 1556: external modules that call setlocale() can change
    // the decimal point character used here from '.' to ','
    q_fix_decimal(this, 0);
}

QoreString::QoreString(const DateTime *d) : priv(new qore_string_private) {
    priv->allocated = 15;
    priv->buf = (char*)malloc(sizeof(char) * priv->allocated);

    qore_tm info;
    d->getInfo(info);
    priv->len = ::sprintf(priv->buf, "%04d%02d%02d%02d%02d%02d", info.year, info.month,
                            info.day, info.hour, info.minute, info.second);
    priv->encoding = QCS_DEFAULT;
}

QoreString::QoreString(const BinaryNode *b) : priv(new qore_string_private) {
    priv->allocated = b->size() + (b->size() * 4) / 10 + 10; // estimate for base64 encoding
    priv->buf = (char*)malloc(sizeof(char) * priv->allocated);
    priv->len = 0;
    priv->encoding = QCS_DEFAULT;
    concatBase64(b, -1);
}

QoreString::QoreString(const BinaryNode *b, size_t maxlinelen) : priv(new qore_string_private) {
    priv->allocated = b->size() + (b->size() * 4) / 10 + 10; // estimate for base64 encoding
    priv->buf = (char*)malloc(sizeof(char) * priv->allocated);
    priv->len = 0;
    priv->encoding = QCS_DEFAULT;
    concatBase64(b, maxlinelen);
}

QoreString::QoreString(char* nbuf, size_t nlen, size_t nallocated, const QoreEncoding* enc) : priv(new qore_string_private) {
    assert(nallocated >= nlen);
    priv->buf = nbuf;
    priv->len = nlen;
    priv->allocated = nallocated;
    if (nallocated == nlen) {
        priv->check_char(nlen);
        priv->buf[nlen] = '\0';
    }
    priv->encoding = enc;
}

// FIXME: remove this function
// private constructor
QoreString::QoreString(struct qore_string_private *p) : priv(p) {
}

QoreString::~QoreString() {
    delete priv;
}

QoreListNode* QoreString::split(const char* sep, bool with_separator) {
    return split_intern(sep, ::strlen(sep), priv->buf, priv->len, priv->encoding, with_separator);
}

QoreListNode* QoreString::split(ExceptionSink* xsink, const char* sep, const char* quote,
        bool trim_unquoted) {
    return split_with_quote(xsink, sep, ::strlen(sep), this, quote, ::strlen(quote), trim_unquoted);
}

// NULL values sorted at end
int QoreString::compare(const QoreString* str) const {
    // empty strings are always equal even if the character encoding is different
    if (!priv->len) {
        if (!str->priv->len)
            return 0;
        return 1;
    }

    if (str->priv->getEncoding() != priv->getEncoding())
        return 1;

    int rc = memcmp(priv->buf, str->priv->buf, QORE_MIN(priv->len, str->size()));
    if (rc == 0) {
        if (priv->len < str->size()) {
            return -1;
        }
        if (priv->len > str->size()) {
            return 1;
        }
        return 0;
    }
    return (rc < 0) ? -1 : 1;
}

int QoreString::compare(const char* str) const {
   // empty strings are always equal even if the character encoding is different
   if (!priv->len) {
      if (!str)
         return 0;
      else
         return 1;
   }

   return strcmp(priv->buf, str);
}

bool QoreString::equal(const QoreString& str) const {
    if (priv->len != str.priv->len)
        return false;

    // empty strings are always equal even if the character encoding is different
    if (!priv->len)
        return true;

    if (priv->getEncoding() != str.priv->getEncoding())
        return false;

    return !memcmp(priv->buf, str.priv->buf, priv->len);
}

bool QoreString::equalPartial(const QoreString& str) const {
    // empty strings are always equal even if the character encoding is different
    if (!priv->len) {
        if (!str.priv->len)
            return true;
        return false;
    }
    if (!str.priv->len)
        return false;

    if (priv->getEncoding() != str.priv->getEncoding())
        return false;

    if (priv->len < str.priv->len)
        return false;

    return !memcmp(priv->buf, str.priv->buf, str.priv->len);
}

bool QoreString::equal(const char* str) const {
    // empty strings are always equal even if the character encoding is different
    if (!str || !str[0]) {
        if (!priv->len)
            return true;
        return false;
    }
    if (!priv->len)
        return false;

    return !strcmp(priv->buf, str);
}

bool QoreString::equalPartial(const char* str) const {
    // empty strings are always equal even if the character encoding is different
    if (!str || !str[0]) {
        if (!priv->len)
            return true;
        return false;
    }
    if (!priv->len)
        return false;

    return !strncmp(priv->buf, str, ::strlen(str));
}

bool QoreString::equalSoft(const QoreString& str, ExceptionSink* xsink) const {
    // empty strings are always equal even if the character encoding is different
    if (!priv->len) {
        if (!str.priv->len)
            return true;
        return false;
    }
    if (!str.priv->len)
        return false;

    // if the encodings are equal or equivalent and the lenghts are different then the strings are not equal
    if ((priv->getEncoding() == str.priv->getEncoding() || (!priv->getEncoding()->isMultiByte() && !str.priv->getEncoding()->isMultiByte())) && priv->len != str.priv->len)
        return false;

    // optionally convert both strings to the same encoding
    const QoreEncoding* enc = priv->getEncoding();
    if (enc == QCS_USASCII)
        enc = str.priv->getEncoding();
    TempEncodingHelper a(this, enc, xsink);
    if (xsink && *xsink)
        return false;
    TempEncodingHelper b(str, enc, xsink);
    if (xsink && *xsink)
        return false;

    if (a->size() != b->size())
        return false;

    return !memcmp(a->c_str(), b->c_str(), a->size());
}

bool QoreString::equalPartialSoft(const QoreString& str, ExceptionSink* xsink) const {
    // empty strings are always equal even if the character encoding is different
    if (!priv->len) {
        if (!str.priv->len)
            return true;
        return false;
    }
    if (!str.priv->len)
        return false;

    // if the encodings are equal or equivalent and the lengths are different then the strings are not equal
    if ((priv->getEncoding() == str.priv->getEncoding() || (!priv->getEncoding()->isMultiByte()
        && !str.priv->getEncoding()->isMultiByte())) && priv->len < str.priv->len)
        return false;

    // optionally convert both strings to the same encoding
    const QoreEncoding* enc = priv->getEncoding();
    if (enc == QCS_USASCII)
        enc = str.priv->getEncoding();
    TempEncodingHelper a(this, enc, xsink);
    if (xsink && *xsink)
        return false;
    TempEncodingHelper b(str, enc, xsink);
    if (xsink && *xsink)
        return false;

    if (a->size() < b->size())
        return false;

    return !memcmp(a->c_str(), b->c_str(), b->size());
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
bool QoreString::equalPartialPath(const QoreString& str, ExceptionSink* xsink) const {
    // empty strings are always equal even if the character encoding is different
    if (!priv->len) {
        if (!str.priv->len)
            return true;
        return false;
    }
    if (!str.priv->len)
        return false;

    // if the encodings are equal or equivalent and the lenghts are different then the strings are not equal
    if ((priv->getEncoding() == str.priv->getEncoding() || (!priv->getEncoding()->isMultiByte() && !str.priv->getEncoding()->isMultiByte())) && priv->len < str.priv->len)
        return false;

    // optionally convert both strings to the same encoding
    const QoreEncoding* enc = priv->getEncoding();
    if (enc == QCS_USASCII || !enc->isAsciiCompat())
        enc = str.priv->getEncoding();

    if (!enc->isAsciiCompat()) {
        xsink->raiseException("UNSUPPORTED-ENCODING", "cannot perform path matching on non-ASCII-compatible " \
            "encoding \"%s\"", enc->getCode());
        return false;
    }

    TempEncodingHelper a(this, enc, xsink);
    if (xsink && *xsink)
        return false;
    TempEncodingHelper b(str, enc, xsink);
    if (xsink && *xsink)
        return false;

    if (a->size() < b->size())
        return false;

    int rc = !memcmp(a->c_str(), b->c_str(), b->size());
    if (!rc)
        return false;

    if (a->priv->len == b->priv->len)
        return true;

    // NOTE: does not work with UTF-16 or other non-ASCII-compatible multi-byte encodings
    if (a->priv->buf[b->priv->len] == '/' || a->priv->buf[b->priv->len] == '?')
        return true;
    return false;
}

void QoreString::terminate(size_t size) {
    priv->terminate(size);
}

void QoreString::reserve(size_t size) {
    // leave room for the terminator char '\0'
    ++size;
    if (size > priv->len)
        priv->check_char(size);
}

void QoreString::take(char* str) {
    if (priv->buf)
        free(priv->buf);
    priv->buf = str;
    if (str) {
        priv->len = ::strlen(str);
        priv->allocated = priv->len + 1;
    } else {
        priv->allocated = 0;
        priv->len = 0;
    }
}

void QoreString::take(char* str, const QoreEncoding* new_qore_encoding) {
    take(str);
    priv->encoding = new_qore_encoding;
}

void QoreString::take(char* str, size_t size) {
    if (priv->buf)
        free(priv->buf);
    priv->buf = str;
    priv->len = size;
    priv->allocated = size + 1;
}

void QoreString::take(char* str, size_t size, const QoreEncoding* enc) {
    if (priv->buf)
        free(priv->buf);
    priv->buf = str;
    priv->len = size;
    priv->allocated = size + 1;

    priv->encoding = enc;
}

void QoreString::takeAndTerminate(char* str, size_t size) {
    if (priv->buf)
        free(priv->buf);
    priv->buf = str;
    priv->len = size;
    priv->allocated = size + 1;
    priv->check_char(size);
    priv->buf[size] = '\0';
}

void QoreString::takeAndTerminate(char* str, size_t size, const QoreEncoding* enc) {
    takeAndTerminate(str, size);
    priv->encoding = enc;
}

// NOTE: could be dangerous if we refer to the priv->buffer after this
// call and it's NULL (the only way the priv->buffer can become NULL)
char* QoreString::giveBuffer() {
    char* rv = priv->buf;
    priv->buf = 0;
    priv->len = 0;
    priv->allocated = 0;
    // reset character set, just in case the string will be reused
    // (normally not after this call)
    priv->encoding = QCS_DEFAULT;
    return rv;
}

void QoreString::clear() {
    priv->clear();
}

void QoreString::reset() {
    char* b = giveBuffer();
    if (b)
        free(b);
    priv->check_char(0);
    priv->buf[0] = '\0';
}

void QoreString::set(const char* str, const QoreEncoding* new_qore_encoding) {
    priv->len = 0;
    priv->encoding = new_qore_encoding;
    if (!str) {
        if (priv->buf)
            priv->buf[0] = '\0';
    } else
        concat(str);
}

void QoreString::set(const char* str, size_t len) {
    priv->len = 0;
    if (!str) {
        if (priv->buf) {
            priv->buf[0] = '\0';
        }
    } else {
        concat(str, len);
    }
}

void QoreString::set(const QoreString* str) {
    priv->len = str->priv->len;
    priv->encoding = str->priv->getEncoding();
    allocate(str->priv->len + 1);
    // copy string and trailing null
    memcpy(priv->buf, str->priv->buf, str->priv->len + 1);
}

void QoreString::set(const QoreString& str) {
    set(&str);
}

void QoreString::set(const std::string& str, const QoreEncoding* ne) {
    priv->len = str.size();
    priv->encoding = ne;
    allocate(priv->len + 1);
    // copy string and trailing null
    memcpy(priv->buf, str.c_str(), priv->len + 1);
}

void QoreString::set(char* nbuf, size_t nlen, size_t nallocated, const QoreEncoding* enc) {
    if (priv->buf)
        free(priv->buf);

    assert(nallocated >= nlen);
    priv->buf = nbuf;
    priv->len = nlen;
    priv->allocated = nallocated;
    if (nallocated == nlen) {
        priv->check_char(nlen);
        priv->buf[nlen] = '\0';
    }
    priv->encoding = enc;
}

void QoreString::setEncoding(const QoreEncoding* new_encoding) {
    priv->encoding = new_encoding;
}

void QoreString::replaceAll(const char* old_str, const char* new_str) {
    assert(old_str);
    assert(new_str);

    int old_len = ::strlen(old_str);
    int new_len = ::strlen(new_str);

    qore_offset_t start = 0;
    while (true) {
        qore_offset_t i = bindex(old_str, start);
        if (i < 0)
            break;

        replace(i, old_len, new_str);
        start = i + new_len;
    }
}

void QoreString::replace(size_t offset, size_t dlen, const char* str) {
    if (str && str[0])
        priv->splice_simple(offset, dlen, str, ::strlen(str));
    else
        priv->splice_simple(offset, dlen);
}

void QoreString::replace(size_t offset, size_t dlen, const QoreString* str, ExceptionSink* xsink) {
    if (str && str->strlen()) {
        TempEncodingHelper tmp(str, priv->getEncoding(), xsink);
        if (!tmp)
            return;
        priv->splice_simple(offset, dlen, tmp->c_str(), tmp->strlen());
        return;
    }

    priv->splice_simple(offset, dlen);
}

void QoreString::replaceChar(size_t offset, char c) {
    if (priv->len <= offset)
        return;

    priv->buf[offset] = c;
}

void QoreString::splice(qore_offset_t offset, ExceptionSink* xsink) {
    if (!priv->getEncoding()->isMultiByte()) {
        size_t n_offset = priv->check_offset(offset);
        if (n_offset == priv->len)
            return;

        priv->splice_simple(n_offset, priv->len - n_offset, 0);
        return;
    }
    priv->splice_complex(offset, xsink, 0);
}

void QoreString::splice(qore_offset_t offset, qore_offset_t num, ExceptionSink* xsink) {
    if (!priv->getEncoding()->isMultiByte()) {
        size_t n_offset, n_num;
        priv->check_offset(offset, num, n_offset, n_num);
        if (n_offset == priv->len || !n_num)
            return;

        priv->splice_simple(n_offset, n_num, 0);
        return;
    }
    priv->splice_complex(offset, num, xsink, 0);
}

void QoreString::splice(qore_offset_t offset, qore_offset_t num, const QoreString& str, ExceptionSink* xsink) {
    TempEncodingHelper tmp(&str, priv->getEncoding(), xsink);
    if (!tmp)
        return;

    if (priv->getEncoding()->isMultiByte()) {
        priv->splice_complex(offset, num, *tmp, xsink, 0);
        return;
    }

    size_t n_offset, n_num;
    priv->check_offset(offset, num, n_offset, n_num);
    if (n_offset == priv->len) {
        if (!tmp->priv->len)
            return;
        n_num = 0;
    }
    priv->splice_simple(n_offset, n_num, tmp->c_str(), tmp->strlen(), 0);
}

void QoreString::splice(qore_offset_t offset, qore_offset_t num, QoreValue strn, ExceptionSink* xsink) {
    QoreStringNodeValueHelper sv(strn);

    if (!sv->strlen()) {
        splice(offset, num, xsink);
        return;
    }

    splice(offset, num, **sv, xsink);
}

QoreString* QoreString::extract(qore_offset_t offset, ExceptionSink* xsink) {
    QoreString* str = new QoreString(priv->getEncoding());
    if (!priv->getEncoding()->isMultiByte()) {
        size_t n_offset = priv->check_offset(offset);
        if (n_offset != priv->len)
            priv->splice_simple(n_offset, priv->len - n_offset, str);
    } else
        priv->splice_complex(offset, xsink, str);
    return str;
}

QoreString* QoreString::extract(qore_offset_t offset, qore_offset_t num, ExceptionSink* xsink) {
    QoreString* str = new QoreString(priv->getEncoding());
    if (!priv->getEncoding()->isMultiByte()) {
        size_t n_offset, n_num;
        priv->check_offset(offset, num, n_offset, n_num);
        if (n_offset != priv->len && n_num)
            priv->splice_simple(n_offset, n_num, str);
    } else
        priv->splice_complex(offset, num, xsink, str);
    return str;
}

QoreString* QoreString::extract(qore_offset_t offset, qore_offset_t num, QoreValue strn, ExceptionSink* xsink) {
    QoreStringValueHelper tmp(strn, priv->encoding, xsink);
    if (*xsink) {
        return nullptr;
    }

    if (!tmp->strlen())
        return extract(offset, num, xsink);

    QoreString* rv = new QoreString(priv->getEncoding());
    if (!priv->getEncoding()->isMultiByte()) {
        size_t n_offset, n_num;
        priv->check_offset(offset, num, n_offset, n_num);
        if (n_offset == priv->len) {
            if (!tmp->priv->len)
                return rv;
            n_num = 0;
        }
        priv->splice_simple(n_offset, n_num, tmp->c_str(), tmp->strlen(), rv);
    } else
        priv->splice_complex(offset, num, *tmp, xsink, rv);
    return rv;
}

QoreString* QoreString::regexSubst(QoreString& match, QoreString& subst, int opts, ExceptionSink* xsink) const {
    QoreRegexSubst regex;
    priv->setRegexOpts(regex, opts);
    if (regex.parseRT(&match, xsink)) {
        assert(*xsink);
        return nullptr;
    }
    QoreStringNodeHolder rv(regex.exec(this, &subst, xsink));
    if (*xsink) {
        return nullptr;
    }
    size_t len = rv->priv->len;
    size_t allocated = rv->priv->allocated;
    const QoreEncoding* enc = rv->priv->encoding;
    return new QoreString(rv->giveBuffer(), len, allocated, enc);
}

int QoreString::regexSubst(QoreString& output, QoreString& match, QoreString& subst, int opts,
        ExceptionSink* xsink) const {
    QoreRegexSubst regex;
    priv->setRegexOpts(regex, opts);
    if (regex.parseRT(&match, xsink)) {
        assert(*xsink);
        return -1;
    }
    QoreStringNodeHolder rv(regex.exec(this, &subst, xsink));
    if (*xsink) {
        return -1;
    }
    output.concat(*rv, xsink);
    if (*xsink) {
        return -1;
    }
    return 0;
}

int QoreString::regexSubstInPlace(QoreString& match, QoreString& subst, int opts, ExceptionSink* xsink) const {
    QoreRegexSubst regex;
    priv->setRegexOpts(regex, opts);
    if (regex.parseRT(&match, xsink)) {
        assert(*xsink);
        return -1;
    }
    QoreStringNodeHolder rv(regex.exec(this, &subst, xsink));
    if (*xsink) {
        return -1;
    }

    size_t len = rv->priv->len;
    size_t allocated = rv->priv->allocated;
    const QoreEncoding* enc = rv->priv->encoding;

    priv->buf = rv->giveBuffer();
    priv->len = len;
    priv->allocated = allocated;
    priv->encoding = enc;
    return 0;
}

// removes a single trailing newline
size_t QoreString::chomp() {
    if (priv->len && priv->buf[priv->len - 1] == '\n') {
        terminate(priv->len - 1);
        if (priv->len && priv->buf[priv->len - 1] == '\r') {
            terminate(priv->len - 1);
            return 2;
        }
        return 1;
    }
    return 0;
}

QoreString* QoreString::convertEncoding(const QoreEncoding* nccs, ExceptionSink* xsink) const {
    printd(5, "QoreString::convertEncoding() from \"%s\" to \"%s\"\n", priv->getEncoding()->getCode(), nccs->getCode());

    if (nccs == priv->getEncoding())
        return copy();

    std::unique_ptr<QoreString> targ(new QoreString(nccs));

    if (priv->len) {
        if (qore_string_private::convert_encoding_intern(priv->buf, priv->len, priv->getEncoding(), *targ, nccs, xsink))
            return 0;

        // remove BOM bytes (invisible non-breaking space) at the beginning of a string when converting to UTF-8
        if (nccs == QCS_UTF8 && targ->priv->len >= 3 && (unsigned char)targ->priv->buf[0] == 0xef && (unsigned char)targ->priv->buf[1] == 0xbb && (unsigned char)targ->priv->buf[2] == 0xbf) {
            printd(5, "QoreString::convertEncoding() found BOM, removing\n");
            targ->priv->splice_simple(0, 3);
        }
    }

    return targ.release();
}

// endian-agnostic binary object -> base64 string function
// NOTE: not very high-performance - high-performance versions
//       would likely be endian-aware and operate directly on 32-bit words
// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concatBase64(const char* bbuf, size_t size, size_t maxlinelen) {
    priv->concatBase64(bbuf, size, maxlinelen);
}

void QoreString::concatBase64(const BinaryNode *b, size_t maxlinelen) {
    priv->concatBase64((char*)b->getPtr(), b->size(), maxlinelen);
}

void QoreString::concatBase64(const QoreString* str, size_t maxlinelen) {
    priv->concatBase64(str->priv->buf, str->priv->len, maxlinelen);
}

void QoreString::concatBase64(const BinaryNode *b) {
    priv->concatBase64((char*)b->getPtr(), b->size(), -1);
}

void QoreString::concatBase64(const QoreString* str) {
    priv->concatBase64(str->priv->buf, str->priv->len, -1);
}

void QoreString::concatBase64(const char* bbuf, size_t size) {
    priv->concatBase64(bbuf, size, -1);
}

void QoreString::concatBase64Url(const BinaryNode& b) {
    priv->concatBase64((char*)b.getPtr(), b.size(), -1, true);
}

void QoreString::concatBase64Url(const QoreString& str) {
    priv->concatBase64(str.priv->buf, str.priv->len, -1, true);
}

#define DO_HEX_CHAR(b) ((b) + (((b) > 9) ? 87 : 48))

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concatHex(const char* binbuf, size_t size) {
    //printf("priv->buf=%p, size=" QSD "\n", binbuf, size);
    if (!size)
        return;

    unsigned char* p = (unsigned char*)binbuf;
    unsigned char* endbuf = p + size;
    while (p < endbuf) {
        char c = (*p & 0xf0) >> 4;
        concat(DO_HEX_CHAR(c));
        c = *p & 0x0f;
        concat(DO_HEX_CHAR(c));
        p++;
    }
}

int QoreString::concatEncode(ExceptionSink* xsink, const QoreString& str, unsigned code) {
    return priv->concatEncode(xsink, str, code);
}

int QoreString::concatDecode(ExceptionSink* xsink, const QoreString& str, unsigned code) {
    return priv->concatDecode(xsink, str, code);
}

void QoreString::concatAndHTMLEncode(const QoreString* str, ExceptionSink* xsink) {
    priv->concatEncode(xsink, str, CE_HTML);
}

// FIXME: this is slow, each concatenated character gets terminated as well
// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concatAndHTMLEncode(const char* str) {
    // if it's not a null string
    if (str) {
        size_t i = 0;
        // iterate through new string
        while (str[i]) {
            // concatenate translated character
            size_t j;
            for (j = 0; j < NUM_HTML_CODES; j++)
                if (str[i] == html_codes[j].symbol) {
                    concat(html_codes[j].code);
                    break;
                }
            // otherwise concatenate untranslated symbol
            if (j == NUM_HTML_CODES)
                concat(str[i]);
            ++i;
        }
    }
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concatAndHTMLDecode(const QoreString* str) {
    if (!str || !str->priv->len)
        return;

    concatAndHTMLDecode(str->c_str(), str->priv->len);
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concatAndHTMLDecode(const char* str) {
    if (str)
        concatAndHTMLDecode(str, ::strlen(str));
}

// FIXME: this is slow, each concatenated character gets terminated as well
// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concatAndHTMLDecode(const char* str, size_t slen) {
    if (!slen)
        return;

    allocate(priv->len + slen); // avoid reallocations within the loop

    size_t i = 0;
    while (str[i]) {
        if (str[i] != '&') {
            concat(str[i++]);
            continue;
        }

        // concatenate translated character
        const char* s = str + i;
        // check for unicode character references
        if (*(s + 1) == '#') {
            s += 2;
            // find end of character sequence
            const char* e = strchr(s, ';');
            // if not found or the number is too big, then don't try to decode it
            if (e && (e - s) < 8) {
                unsigned code;
                if (*s == 'x')
                    code = strtoul(s + 1, 0, 16);
                else
                    code = strtoul(s, 0, 10);

                if (!concatUnicode(code)) {
                    i = e - str + 1;
                    continue;
                }
                // error occurred, so back out
                s -= 2;
            }
        } else if (isascii(*(s + 1))) {
            s += 1;
            // find end of character sequence
            const char* e = strchr(s, ';');
            // if not found or the number is too big, then don't try to decode it
            if (e && (e - s) < 16) {
                // find unicode code point
                std::string entity(s, e - s);
                //printd(0, "entity: %s\n", entity.c_str());
                emap_t::iterator it = emap.find(entity);
                if (it != emap.end() && !concatUnicode(it->second)) {
                    i = e - str + 1;
                    continue;
                }
            }
            // not found or error occurred concatenating char, so back out
            s -= 1;
        }
        concat(str[i++]);
    }
}

// deprecated, does not support RFC-3986
// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concatDecodeUrl(const char* url) {
    if (!url)
        return;

    while (*url) {
        if (*url == '%' && isxdigit(*(url + 1)) && isxdigit(*(url + 2))) {
            char x[3] = { *(url + 1), *(url + 2), '\0' };
            char code = strtol(x, 0, 16);
            concat(code);
            url += 3;
            continue;
        }
        concat(*url);
        ++url;
    }
}

// assume encoding according to http://tools.ietf.org/html/rfc3986#section-2.1
int QoreString::concatDecodeUrl(const QoreString& url_str, ExceptionSink* xsink) {
   assert(xsink);

   TempEncodingHelper str(url_str, priv->getEncoding(), xsink);
   if (*xsink)
      return -1;

   return priv->concatDecodeUriIntern(xsink, *str->priv);
}

// assume encoding according to http://tools.ietf.org/html/rfc3986#section-2.1
int QoreString::concatEncodeUrl(ExceptionSink* xsink, const QoreString& url, bool encode_all) {
    assert(xsink);
    if (!url.size())
        return 0;

    if (!priv->getEncoding()->isAsciiCompat()) {
        xsink->raiseException("UNSUPPORTED-ENCODING", "cannot encode a URI to non-ASCII-compatible encoding \"%s\"", priv->getEncoding()->getCode());
        return -1;
    }

    TempEncodingHelper str(url, QCS_UTF8, xsink);
    if (*xsink)
        return -1;

    const unsigned char* p = (const unsigned char*)str->c_str();
    while (*p) {
        if ((*p) == '%') {
            concat("%25");
        } else if ((*p) == ' ') {
            concat("%20");
        } else if (*p > 127) {
            size_t len = q_UTF8_get_char_len((const char*)p, str->size() - ((const char*)p - str->c_str()));
            if (len <= 0) {
                xsink->raiseException("INVALID-ENCODING", "invalid UTF-8 encoding found in string");
                return -1;
            }
            // add UTF-8 percent-encoded characters
            for (size_t i = 0; i < len; ++i) {
                sprintf("%%%X", (unsigned)p[i]);
            }
            p += len;
            continue;
        // issue #4917 always encode control characters
        } else if ((*p) < 32) {
            sprintf("%%%02X", (unsigned)*p);
        } else if (encode_all && url_reserved.find(*p) != url_reserved.end()) {
            sprintf("%%%02X", (unsigned)*p);
        } else {
            concat(*p);
        }

        ++p;
    }

    return 0;
}

int QoreString::concatEncodeUriRequest(ExceptionSink* xsink, const QoreString& url) {
    assert(xsink);
    if (!url.size())
        return 0;

    TempEncodingHelper str(url, QCS_UTF8, xsink);
    if (*xsink)
        return -1;

    return priv->concatEncodeUriRequest(xsink, *url.priv);
}

int QoreString::concatDecodeUriRequest(const QoreString& url_str, ExceptionSink* xsink) {
    assert(xsink);
    TempEncodingHelper str(url_str, priv->getEncoding(), xsink);
    if (*xsink)
        return -1;

    return priv->concatDecodeUriIntern(xsink, *str->priv, true);
}

// return 0 for success
int QoreString::vsprintf(const char* fmt, va_list args) {
    return priv->vsprintf(fmt, args);
}

void QoreString::concat(const char* str) {
    priv->concat(str);
}

void QoreString::concat(const std::string& str) {
    priv->check_char(priv->len + str.size());
    memcpy(priv->buf + priv->len, str.c_str(), str.size());
    priv->len += str.size();
    priv->buf[priv->len] = '\0';
}

void QoreString::concat(const char* str, size_t size) {
    priv->check_char(priv->len + size);
    memcpy(priv->buf + priv->len, str, size);
    priv->len += size;
    priv->buf[priv->len] = '\0';
}

void QoreString::concat(const QoreString* str) {
    if (str)
        priv->concat(str->priv);
}

/*
void QoreString::concat(const QoreString* str, size_t size) {
    // if it's not a null string
    if (str && str->priv->len) {
        // if priv->buffer needs to be resized
        priv->check_char(str->priv->len + size);
        // concatenate new string
        memcpy(priv->buf + priv->len, str->priv->buf, size);
        priv->len += size;
        priv->buf[priv->len] = '\0';
    }
}
*/

void QoreString::concat(const QoreString* str, ExceptionSink* xsink) {
    priv->concat(str, xsink);
}

void QoreString::concat(const QoreString* str, size_t size, ExceptionSink* xsink) {
    assert(xsink);
    // if it's not a null string
    if (str && str->priv->len) {
        TempEncodingHelper cstr(str, priv->getEncoding(), xsink);
        if (*xsink)
            return;

        // adjust size for number of characters if this is a multi-byte character set
        if (priv->getEncoding()->isMultiByte()) {
            size = priv->getEncoding()->getByteLen(cstr->priv->buf, cstr->priv->buf + cstr->priv->len, size, xsink);
            if (*xsink)
                return;
        }

        // if priv->buffer needs to be resized
        priv->check_char(cstr->priv->len + size + STR_CLASS_EXTRA);
        // concatenate new string
        memcpy(priv->buf + priv->len, cstr->priv->buf, size);
        priv->len += size;
        priv->buf[priv->len] = '\0';
    }
}

int QoreString::concat(const QoreString& str, qore_offset_t pos, ExceptionSink* xsink) {
    assert(xsink);
    if (str.empty())
        return 0;

    TempEncodingHelper cstr(str, priv->getEncoding(), xsink);
    if (*xsink)
        return -1;

    return priv->concat(*(cstr->priv), pos, xsink);
}

int QoreString::concat(const QoreString& str, qore_offset_t pos, qore_offset_t len, ExceptionSink* xsink) {
    assert(xsink);
    if (str.empty() || !len)
        return 0;

    TempEncodingHelper cstr(str, priv->getEncoding(), xsink);
    if (*xsink)
        return -1;

    return priv->concat(*(cstr->priv), pos, len, xsink);
}

void QoreString::concat(char c) {
    priv->concat(c);
}

int QoreString::vsnprintf(size_t size, const char* fmt, va_list args) {
    // ensure minimum space is free
    if ((priv->allocated - priv->len) < (unsigned)size) {
        priv->allocated += (size + STR_CLASS_EXTRA);
        // resize priv->buffer
        priv->buf = (char*)realloc(priv->buf, priv->allocated * sizeof(char));
    }
    // copy formatted string to priv->buffer
    int i = ::vsnprintf(priv->buf + priv->len, size, fmt, args);
    priv->len += i;
    return i;
}

// returns 0 for success
int QoreString::sprintf(const char* fmt, ...) {
    va_list args;
    while (true) {
        va_start(args, fmt);
        int rc = vsprintf(fmt, args);
        va_end(args);
        if (!rc)
            break;
    }
    return 0;
}

int QoreString::snprintf(size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int i = vsnprintf(size, fmt, args);
    va_end(args);
    return i;
}

// NULL values sorted at end
int QoreString::compareSoft(const QoreString* str, ExceptionSink* xsink) const {
    // empty strings are always equal even if the character encoding is different
    if (!priv->len) {
        if (!str->priv->len)
            return 0;
        else
            return 1;
    }

    TempEncodingHelper t(str, priv->getEncoding(), xsink);
    if (xsink && *xsink)
        return 1;

    int rc = memcmp(priv->buf, t->priv->buf, QORE_MIN(priv->len, t->size()));
    if (rc < 0)
        return -1;
    return !rc ? 0 : 1;
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concatEscape(const char* str, char c, char esc_char) {
    // if it's not a null string
    if (str) {
        size_t i = 0;
        // iterate through new string
        while (str[i]) {
            if (str[i] == c || str[i] == esc_char) {
                // check for space in priv->buffer
                priv->check_char(priv->len + 1);
                priv->buf[priv->len++] = esc_char;
            } else
                priv->check_char(priv->len);
            // concatenate one character at a time
            priv->buf[priv->len++] = str[i++];
        }
        // see if priv->buffer needs to be resized for '\0'
        priv->check_char(priv->len);
        // terminate string
        priv->buf[priv->len] = '\0';
    }
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concatEscape(const QoreString* str, char c, char esc_char, ExceptionSink* xsink) {
    if (!priv->getEncoding()->isAsciiCompat()) {
        xsink->raiseException("UNSUPPORTED-ENCODING", "cannot process escapes for non-ASCII-compatible encoding "
            "\"%s\"", priv->getEncoding()->getCode());
        return;
    }

    // if it's not a null string
    if (str && str->priv->len) {
        TempEncodingHelper cstr(str, priv->getEncoding(), xsink);
        if (xsink && *xsink)
            return;

        // if priv->buffer needs to be resized
        priv->check_char(cstr->priv->len + priv->len);

        concatEscape(cstr->priv->buf, c, esc_char);
    }
}

QoreString* QoreString::substr(qore_offset_t offset, ExceptionSink* xsink) const {
    TempString str(new QoreString(priv->getEncoding()));

    int rc;
    if (!priv->getEncoding()->isMultiByte())
        rc = priv->substr_simple(*str, offset);
    else
        rc = priv->substr_complex(*str, offset, xsink);

    return rc ? 0 : str.release();
}

QoreString* QoreString::substr(qore_offset_t offset, qore_offset_t length, ExceptionSink* xsink) const {
    TempString str(new QoreString(priv->getEncoding()));

    int rc;
    if (!priv->getEncoding()->isMultiByte())
        rc = priv->substr_simple(*str, offset, length);
    else
        rc = priv->substr_complex(*str, offset, length, xsink);

    return rc ? 0 : str.release();
}

size_t QoreString::length() const {
    if (priv->getEncoding()->isMultiByte() && priv->buf) {
        bool invalid;
        return priv->getEncoding()->getLength(priv->buf, priv->buf + priv->len, invalid);
    }
    return priv->len;
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concat(const DateTime *d) {
    qore_tm info;
    d->getInfo(info);
    sprintf("%04d%02d%02d%02d%02d%02d", info.year, info.month, info.day, info.hour, info.minute, info.second);
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::concatISO8601DateTime(const DateTime *d) {
    qore_tm info;
    d->getInfo(currentTZ(), info);
    sprintf("%04d%02d%02dT%02d:%02d:%02d", info.year, info.month, info.day, info.hour, info.minute, info.second);
}

void QoreString::concatHex(const BinaryNode *b) {
    concatHex((char*)b->getPtr(), b->size());
}

void QoreString::concatHex(const QoreString* str) {
    concatHex(str->priv->buf, str->priv->len);
}

// endian-agnostic base64 string -> binary object function
BinaryNode *QoreString::parseBase64(ExceptionSink* xsink) const {
    return ::parseBase64(priv->buf, priv->len, xsink);
}

QoreString* QoreString::parseBase64ToString(const QoreEncoding* qe, ExceptionSink* xsink) const {
    SimpleRefHolder<BinaryNode> b(::parseBase64(priv->buf, priv->len, xsink));
    return binary_to_string<QoreString>(b.release(), qe);
}

QoreString* QoreString::parseBase64ToString(ExceptionSink* xsink) const {
    return parseBase64ToString(QCS_DEFAULT, xsink);
}

// endian-agnostic base64 URL-encoded string -> binary object function
BinaryNode *QoreString::parseBase64Url(ExceptionSink* xsink) const {
    return ::parseBase64Url(priv->buf, priv->len, xsink);
}

QoreString* QoreString::parseBase64UrlToString(const QoreEncoding* qe, ExceptionSink* xsink) const {
    SimpleRefHolder<BinaryNode> b(::parseBase64Url(priv->buf, priv->len, xsink));
    return binary_to_string<QoreString>(b.release(), qe);
}

QoreString* QoreString::parseBase64UrlToString(ExceptionSink* xsink) const {
    return parseBase64ToString(QCS_DEFAULT, xsink);
}

BinaryNode *QoreString::parseHex(ExceptionSink* xsink) const {
    return ::parseHex(priv->buf, priv->len, xsink);
}

void QoreString::allocate(unsigned requested_size) {
    priv->allocate(requested_size);
}

const QoreEncoding* QoreString::getEncoding() const {
    return priv->getEncoding();
}

QoreString* QoreString::copy() const {
    return new QoreString(*this);
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::tolwr() {
    char* c = priv->buf;
    while (*c) {
        *c = ::tolower(*c);
        c++;
    }
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::toupr() {
    char* c = priv->buf;
    while (*c) {
        *c = ::toupper(*c);
        c++;
    }
}

// returns number of bytes
size_t QoreString::strlen() const {
    return priv->len;
}

size_t QoreString::size() const {
    return priv->len;
}

size_t QoreString::capacity() const {
    return priv->allocated;
}

const char* QoreString::getBuffer() const {
    return priv->buf;
}

const char* QoreString::c_str() const {
    return priv->buf;
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
void QoreString::addch(char c, unsigned times) {
    priv->check_char(priv->len + times); // more data will follow the padding
    memset(priv->buf + priv->len, c, times);
    priv->len += times;
    priv->buf[priv->len] = 0;
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
int QoreString::insertch(char c, size_t pos, unsigned times) {
    //printd(5, "QoreString::insertch(c: %c pos: " QLLD " times: %d) this: %p\n", c, pos, times, this);
    if (pos > priv->len || !times)
        return -1;

    priv->check_char(priv->len + times); // more data will follow the padding
    if (pos < priv->len)
        memmove(priv->buf + pos + times, priv->buf + pos, priv->len - pos);
    memset(priv->buf + pos, c, times);
    priv->len += times;
    priv->buf[priv->len] = 0;
    return 0;
}

// FIXME: does not work with non-ASCII-compatible encodings such as UTF-16*
int QoreString::insert(const char* str, size_t pos) {
    if (pos > priv->len)
        return -1;

    size_t sl = ::strlen(str);

    priv->check_char(priv->len + sl); // more data will follow the padding
    if (pos < priv->len)
        memmove(priv->buf + pos + sl, priv->buf + pos, priv->len - pos);
    strncpy(priv->buf + pos, str, sl);
    priv->len += sl;
    priv->buf[priv->len] = 0;
    return 0;
}

int QoreString::concatUnicode(unsigned code, ExceptionSink* xsink) {
    return priv->concatUnicode(code, xsink);
}

int QoreString::concatUnicode(unsigned code) {
    return priv->concatUnicode(code);
}

void QoreString::concatUTF8FromUnicode(unsigned code) {
    priv->concatUTF8FromUnicode(code);
}

unsigned int QoreString::getUnicodePointFromUTF8(qore_offset_t offset) const {
    // get length in chars
    bool invalid;
    char* endp = priv->buf + priv->len;
    size_t clen = priv->getEncoding()->getLength(priv->buf, endp, invalid);
    if (invalid)
        return -1;

    //printd(0, "splice_complex(offset=" QSD ") clen=" QSD "\n", offset, clen);
    if (offset < 0) {
        offset = clen + offset;
        if (offset < 0)
            offset = 0;
    } else if ((size_t)offset >= clen)
        return 0;

    // calculate byte offset
    if (offset) {
        offset = priv->getEncoding()->getByteLen(priv->buf, endp, offset, invalid);
        if (invalid)
            return -1;
    }

    size_t bl = priv->getEncoding()->getByteLen(priv->buf + offset, endp, 1, invalid);
    if (invalid)
        return -1;

    return get_unicode_from_utf8(priv->buf + offset, bl);
}

unsigned int QoreString::getUnicodePoint(qore_offset_t offset, ExceptionSink* xsink) const {
    if (offset < 0) {
        // get string length in characters
        qore_offset_t clen = (qore_offset_t)priv->getEncoding()->getLength(priv->buf, priv->buf + priv->len, xsink);
        if (*xsink)
            return -1;
        offset = clen + offset;
        if (offset < 0)
            offset = 0;
    }
    size_t bl = priv->getEncoding()->getByteLen(priv->buf, priv->buf + priv->len, offset, xsink);
    if (*xsink)
        return -1;

    unsigned len;
    return getUnicodePointFromBytePos(bl, len, xsink);
}

unsigned int QoreString::getUnicodePointFromBytePos(size_t offset, unsigned& len, ExceptionSink* xsink) const {
    return priv->getUnicodePointFromBytePos(offset, len, xsink);
}

QoreString* QoreString::reverse() const {
    QoreString* str = new QoreString(priv->getEncoding());
    concat_reverse(str);
    return str;
}

// remove trailing char
void QoreString::trim_trailing(char c) {
    if (!priv->len)
        return;

    char* p = priv->buf + priv->len - 1;
    while (p >= priv->buf && (*p) == c)
        --p;

    terminate(p + 1 - priv->buf);
}

// remove single trailing char
void QoreString::trim_single_trailing(char c) {
    if (priv->len && priv->buf[priv->len - 1] == c)
        terminate(priv->len - 1);
}

// remove leading char
void QoreString::trim_leading(char c) {
    if (!priv->len)
        return;

    size_t i = 0;
    while (i < priv->len && priv->buf[i] == c)
        ++i;
    if (!i)
        return;

    memmove(priv->buf, priv->buf + i, priv->len + 1 - i);
    priv->len -= i;
}

// remove single leading char
void QoreString::trim_single_leading(char c) {
    if (priv->len && priv->buf[0] == c) {
        memmove(priv->buf, priv->buf + 1, priv->len);
        priv->len -= 1;
    }
}

// remove leading and trailing char
void QoreString::trim(char c) {
    trim_trailing(c);
    trim_leading(c);
}

int QoreString::trim(ExceptionSink* xsink, const QoreString* chars) {
    qore_string_private* pchars = chars ? chars->priv : nullptr;
    return priv->trimLeading(xsink, pchars) || priv->trimTrailing(xsink, pchars);
}

int QoreString::trimLeading(ExceptionSink* xsink, const QoreString* chars) {
    return priv->trimLeading(xsink, chars ? chars->priv : nullptr);
}

int QoreString::trimTrailing(ExceptionSink* xsink, const QoreString* chars) {
    return priv->trimTrailing(xsink, chars ? chars->priv : nullptr);
}

// remove trailing chars
void QoreString::trim_trailing(const char* chars) {
    if (!priv->len)
        return;

    char* p = priv->buf + priv->len - 1;
    if (!chars) // use an alternate path here so we can check for embedded nulls as well
        while (p >= priv->buf && qore_string_private::inVector(*p, default_whitespace))
            --p;
    else
        while (p >= priv->buf && strchr(chars, *p))
            --p;

    terminate(p + 1 - priv->buf);
}

// remove leading char
void QoreString::trim_leading(const char* chars) {
    if (!priv->len)
        return;

    size_t i = 0;
    if (!chars)
        while (i < priv->len && qore_string_private::inVector(priv->buf[i], default_whitespace))
            ++i;
    else
        while (i < priv->len && strchr(chars, priv->buf[i]))
            ++i;
    if (!i)
        return;

    memmove(priv->buf, priv->buf + i, priv->len + 1 - i);
    priv->len -= i;
}

// remove leading and trailing blanks
void QoreString::trim(const char* chars) {
    trim_trailing(chars);
    trim_leading(chars);
}

// writes a new QoreString with the characters reversed of the "this" QoreString
// assumes the encoding is the same and the length is 0
void QoreString::concat_reverse(QoreString* str) const {
    assert(str->priv->getEncoding() == priv->getEncoding());
    assert(!str->priv->len);

    str->priv->check_char(priv->len);
    if (priv->getEncoding()->isMultiByte()) {
        char* p = priv->buf;
        char* targ_end = str->priv->buf + priv->len;
        char* end = priv->buf + priv->len;
        while (p < end) {
            bool invalid;
            int bl = priv->getEncoding()->getByteLen(p, end, 1, invalid);
            if (invalid) // if we hit an invalid encoding, then we just copy bytes
                bl = 1;
            targ_end -= bl;
            // in case of corrupt data, make sure we don't go off the beginning of the string
            if (targ_end < str->priv->buf) {
                break;
            }
            strncpy(targ_end, p, bl);
            p += bl;
        }
    } else {
        for (size_t i = 0; i < priv->len; ++i) {
            str->priv->buf[i] = priv->buf[priv->len - i - 1];
        }
    }

    str->priv->buf[priv->len] = '\0';
    str->priv->len = priv->len;
}

QoreString& QoreString::operator=(const QoreString& other) {
   set(other);
   return *this;
}

QoreString& QoreString::operator=(const char* other) {
   set(other);
   return *this;
}

QoreString& QoreString::operator=(const std::string& other) {
   set(other);
   return *this;
}

bool QoreString::operator==(const QoreString& other) const {
   if (other.priv->getEncoding() != priv->getEncoding() || other.priv->len != priv->len)
      return false;
   return !memcmp(other.priv->buf, priv->buf, priv->len);
}

bool QoreString::operator==(const std::string& other) const {
   if (other.size() != priv->len)
      return false;
   return !memcmp(other.c_str(), priv->buf, priv->len);
}

bool QoreString::operator==(const char* other) const {
   // NOTE: does not work with UTF-16 or other non-ASCII-compatible multi-byte encodings
   return !strcmp(other, priv->buf);
}

QoreString& QoreString::operator+=(const char* str) {
   concat(str);
   return *this;
}

QoreString& QoreString::operator+=(const std::string& str) {
   concat(str);
   return *this;
}

int QoreString::operator[](qore_offset_t pos) const {
    if (pos < 0) {
        pos = priv->len + pos;
        if (pos < 0) {
            return -1;
        }
    } else if ((size_t)pos >= priv->len) {
        return -1;
    }

    return priv->buf[pos];
}

bool QoreString::empty() const {
   return !priv->len;
}

void QoreString::prepend(const char* str) {
   prepend(str, ::strlen(str));
}

void QoreString::prepend(const char* str, size_t size) {
   priv->check_char(priv->len + size + 1);
   // move memory forward
   memmove((char*)priv->buf + size, priv->buf, priv->len + 1);
   // copy new memory to beginning
   memcpy((char*)priv->buf, str, size);
   priv->len += size;
}

qore_offset_t QoreString::index(const QoreString& needle, qore_offset_t pos, ExceptionSink* xsink) const {
   return priv->index(needle, pos, xsink);
}

qore_offset_t QoreString::bindex(const QoreString& needle, qore_offset_t pos) const {
   return priv->bindex(needle, pos);
}

qore_offset_t QoreString::bindex(const char* needle, qore_offset_t pos) const {
    return priv->bindex(needle, pos);
}

qore_offset_t QoreString::bindex(const std::string& needle, qore_offset_t pos) const {
    return priv->bindex(needle, pos);
}

qore_offset_t QoreString::rindex(const QoreString& needle, qore_offset_t pos, ExceptionSink* xsink) const {
    return priv->rindex(needle, pos, xsink);
}

qore_offset_t QoreString::brindex(const QoreString& needle, qore_offset_t pos) const {
    return priv->brindex(needle, pos);
}

qore_offset_t QoreString::brindex(const char* needle, qore_offset_t pos) const {
    return priv->brindex(needle, ::strlen(needle), pos);
}

qore_offset_t QoreString::brindex(const std::string& needle, qore_offset_t pos) const {
    return priv->brindex(needle, pos);
}

qore_offset_t QoreString::find(char c, qore_offset_t pos) const {
    return priv->find(c, pos);
}

qore_offset_t QoreString::rfind(char c, qore_offset_t pos) const {
    return priv->rfind(c, pos);
}

qore_offset_t QoreString::find(const char* str, qore_offset_t pos) const {
    return priv->bindex(str, pos);
}

qore_offset_t QoreString::rfind(const char* str, qore_offset_t pos) const {
    return priv->brindex(str, ::strlen(str), pos);
}

qore_offset_t QoreString::find(const std::string& str, qore_offset_t pos) const {
    return priv->bindex(str, pos);
}

qore_offset_t QoreString::rfind(const std::string& str, qore_offset_t pos) const {
    return priv->brindex(str, pos);
}

qore_offset_t QoreString::findAny(const char* str, qore_offset_t pos) const {
   return priv->findAny(str, pos);
}

qore_offset_t QoreString::rfindAny(const char* str, qore_offset_t pos) const {
   return priv->rfindAny(str, pos);
}

bool QoreString::startsWith(const char* str) const {
    return priv->startsWith(str, ::strlen(str));
}

bool QoreString::startsWith(const std::string& str) const {
    return priv->startsWith(str.c_str(), str.size());
}

bool QoreString::endsWith(const char* str) const {
    return priv->endsWith(str, ::strlen(str));
}

bool QoreString::endsWith(const std::string& str) const {
    return priv->endsWith(str.c_str(), str.size());
}

bool QoreString::isDataPrintableAscii() const {
   return priv->isDataPrintableAscii();
}

bool QoreString::isDataAscii() const {
   return priv->isDataAscii();
}

int64 QoreString::toBigInt() const {
   return strtoll(priv->buf, 0, 10);
}

qore_offset_t QoreString::getByteOffset(size_t i, ExceptionSink* xsink) const {
   return priv->getByteOffset(i, xsink);
}

size_t QoreString::removeBytes(size_t len) {
    return priv->removeBytes(len);
}

size_t QoreString::getCharWidth(ExceptionSink* xsink) const {
    if (!priv->getEncoding()->isMultiByte()) {
        return priv->len;
    }
    size_t rc = 0;
    UnicodeCharacterIterator i(*this);
    while (i.next(xsink)) {
        rc += qore_get_unicode_character_width(i.getValue());
    }
    return *xsink ? 0 : rc;
}

void TempEncodingHelper::removeBom() {
    if (!str || str->getEncoding()->isAsciiCompat())
        return;
    if (!temp) {
        str = new QoreString(*str);
        temp = true;
    }
    qore_string_private* pstr = qore_string_private::get(*str);
    assert(pstr->encoding);
    q_remove_bom_utf16(str, pstr->encoding);
}
