// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: MIT

// Include the editor to exercise its private buffer conversion helpers.
#include "../src/linenoise/linenoise.cpp"
#include <algorithm>
#include <cstring>

static void require(bool condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "linenoise Unicode conversion: %s\n", message);
        abort();
    }
}

int main() {
    const char text[] = "Příliš žluťoučký 日本語 😀";
    const char32_t expected[] = U"Příliš žluťoučký 日本語 😀";
    char32_t decoded[64] = {};
    size_t count = 99;
    require(copyString8to32(decoded, 64, count, text) == conversionOK, "mixed UTF-8 input");
    require(count == std::size(expected) - 1, "Unicode character count");
    require(std::equal(std::begin(expected), std::end(expected), decoded), "decoded code points and terminator");

    char encoded[256] = {};
    size_t bytes = 99;
    copyString32to8(encoded, sizeof(encoded), &bytes, decoded, count);
    require(bytes == strlen(text), "encoded byte count");
    require(strcmp(encoded, text) == 0, "UTF-8 round trip including supplementary characters");

    require(copyString8to32(decoded, 64, count, "") == conversionOK, "empty input");
    require(count == 0 && decoded[0] == 0, "empty output terminator");
    require(copyString8to32(decoded, 2, count, "abc") == llvm::targetExhausted, "short output buffer");
    require(copyString8to32(decoded, 64, count, "\xc0\xaf") == llvm::sourceIllegal, "overlong UTF-8");
    require(copyString8to32(decoded, 64, count, "\xf0\x9f") == llvm::sourceExhausted, "incomplete UTF-8");
    require(copyString8to32(decoded, 64, count, "\xed\xa0\x80") == llvm::sourceIllegal, "UTF-8 surrogate");
    require(copyString8to32(decoded, 64, count, "\xf4\x90\x80\x80") == llvm::sourceIllegal,
        "code point above the Unicode limit");
    return 0;
}
