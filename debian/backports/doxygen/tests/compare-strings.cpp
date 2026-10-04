// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: GPL-2.0-only
// Test the actual upstream comparators against unsigned std::string ordering.
#include "qcstring.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

static std::string folded(std::string value) {
    for (char& byte : value) {
        if (byte >= 'A' && byte <= 'Z') {
            byte += 'a' - 'A';
        }
    }
    return value;
}

static int sign(int value) {
    return (value > 0) - (value < 0);
}

int main() {
    std::vector<std::string> values = {
        "", "A", "a", "AA", "Aa", "aA", "Z", "z", "Context Functions",
        "Context — Service Endpoint", "Context — Step Loop", "é", "É", "α", "😀",
    };
    // Include all non-NUL byte values, including incomplete UTF-8 sequences:
    // these comparators operate on bytes, not locale-dependent collation.
    for (int byte = 1; byte <= 255; ++byte) {
        values.emplace_back(1, static_cast<char>(byte));
    }
    for (const auto& first : values) {
        for (const auto& second : values) {
            const auto left = folded(first);
            const auto right = folded(second);
            if (sign(qstricmp(first.c_str(), second.c_str())) != sign(left.compare(right))) {
                std::fprintf(stderr, "qstricmp disagrees with unsigned byte ordering\n");
                return 1;
            }
            for (size_t length = 0; length <= std::max(first.size(), second.size()) + 1; ++length) {
                if (sign(qstrnicmp(first.c_str(), second.c_str(), length))
                        != sign(left.substr(0, length).compare(right.substr(0, length)))) {
                    std::fprintf(stderr, "qstrnicmp disagrees at length %zu\n", length);
                    return 1;
                }
            }
        }
    }
    std::printf("PASS: %zu string pairs, including every non-NUL byte\n", values.size() * values.size());
}
