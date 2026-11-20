/*
    multibyte_iconv_parity.cpp

    Copyright (C) 2026 Qore Technologies, s.r.o.

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
*/

/** The character functions of the multi-byte encodings that Qore recognizes by the byte ranges of their characters
    (charset.cpp) and the platform's iconv agree on which byte sequences are valid (qore#6796): Qore validates the
    input of a conversion from these encodings with its character functions before iconv, so a sequence that iconv
    decodes and the character functions refuse would make a valid conversion fail, and one that the character
    functions accept and iconv refuses would make it fail with an iconv error instead of INVALID-ENCODING.

    Every byte 0x80 - 0xff on its own and every two-byte sequence whose first byte is 0x80 - 0xff is checked; for each
    encoding the program prints one line:
        <encoding> <checked> <qore-only> <iconv-only> <iconv-only-80> [<first qore-only sequences>]
            [<first iconv-only sequences that do not start with 0x80>]
    where iconv-only-80 is the number of sequences that iconv decodes and the character functions refuse whose first
    byte is 0x80 (glibc decodes the byte 0x80 of Big5, Big5-HKSCS and CP950 as U+0080; it is no character of these
    encodings, as for the WHATWG Encoding Standard, and isEncodingValid() refuses it)
    or "<encoding> unavailable" if the platform's iconv does not know the encoding.  The test that runs it
    (multibyte-iconv-parity.qtest) decides which differences are allowed on the platform.
*/

#include <qore/Qore.h>

#include <cerrno>
#include <cstdio>
#include <iconv.h>
#include <string>
#include <vector>

// returns true if the bytes are complete, valid characters of the encoding by its character functions
static bool qore_valid(const QoreEncoding* enc, const unsigned char* p, size_t len) {
    size_t i = 0;
    while (i < len) {
        qore_offset_t cl = enc->getCharLen(reinterpret_cast<const char*>(p + i), len - i);
        if (cl <= 0) {
            return false;
        }
        i += static_cast<size_t>(cl);
    }
    return true;
}

// returns true if iconv decodes all of the bytes without an error
static bool iconv_valid(iconv_t cd, const unsigned char* p, size_t len) {
    // reset the conversion state
    iconv(cd, nullptr, nullptr, nullptr, nullptr);
    char out[64];
    char* ib = reinterpret_cast<char*>(const_cast<unsigned char*>(p));
    size_t il = len;
    char* ob = out;
    size_t ol = sizeof(out);
    size_t rc = iconv(cd, &ib, &il, &ob, &ol);
    if (rc == static_cast<size_t>(-1) || il) {
        return false;
    }
    // a character cut off by the end of the input is reported here by some implementations
    return iconv(cd, nullptr, nullptr, &ob, &ol) != static_cast<size_t>(-1);
}

static std::string hex(const unsigned char* p, size_t len) {
    std::string rv;
    char buf[4];
    for (size_t i = 0; i < len; ++i) {
        std::snprintf(buf, sizeof(buf), "%02x", p[i]);
        rv += buf;
    }
    return rv;
}

int main(int argc, char** argv) {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    // the iconv implementation: glibc's and Apple's libiconv refuse invalid sequences; musl's does not always
#if defined(__GLIBC__)
    std::printf("iconv glibc\n");
#elif defined(__APPLE__)
    std::printf("iconv apple\n");
#else
    std::printf("iconv other\n");
#endif
    std::vector<const char*> encs;
    for (int i = 1; i < argc; ++i) {
        encs.push_back(argv[i]);
    }
    if (encs.empty()) {
        encs = {"Shift_JIS", "CP932", "EUC-JP", "EUC-KR", "CP949", "JOHAB", "GBK", "GB2312", "GB18030", "Big5",
            "Big5-HKSCS", "CP950", "EUC-TW"};
    }
    for (const char* name : encs) {
        const QoreEncoding* enc = QoreEncodingManager::findCreate(name);
        iconv_t cd = iconv_open("UTF-8", enc->getCode());
        if (cd == reinterpret_cast<iconv_t>(static_cast<intptr_t>(-1))) {
            std::printf("%s unavailable\n", name);
            continue;
        }
        unsigned long checked = 0, qore_only = 0, iconv_only = 0, iconv_only_80 = 0;
        std::string qore_ex, iconv_ex;
        auto check = [&](const unsigned char* b, size_t len) {
            ++checked;
            bool q = qore_valid(enc, b, len);
            bool i = iconv_valid(cd, b, len);
            if (q && !i) {
                if (++qore_only <= 8) {
                    qore_ex += (qore_ex.empty() ? "" : ",") + hex(b, len);
                }
            } else if (i && !q) {
                if (b[0] == 0x80) {
                    ++iconv_only_80;
                } else if (++iconv_only <= 8) {
                    iconv_ex += (iconv_ex.empty() ? "" : ",") + hex(b, len);
                }
            }
        };
        for (unsigned b0 = 0x80; b0 < 0x100; ++b0) {
            unsigned char b[2] = {static_cast<unsigned char>(b0), 0};
            check(b, 1);
            for (unsigned b1 = 0; b1 < 0x100; ++b1) {
                b[1] = static_cast<unsigned char>(b1);
                check(b, 2);
            }
        }
        iconv_close(cd);
        std::printf("%s %lu %lu %lu %lu [%s] [%s]\n", enc->getCode(), checked, qore_only, iconv_only, iconv_only_80,
            qore_ex.c_str(), iconv_ex.c_str());
    }
    qore_cleanup();
    return 0;
}
