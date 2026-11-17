/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

// qore-aot-reseal <file> [<offset> <length>]: sets the checksum of the AOT metadata in the file again (qore#6451)
/* The AOT metadata of format QORE_AOT_METADATA_CHECKSUM_VERSION and later is followed by its checksum, which the
   reader checks before it reads anything else; the tests that patch the metadata of a module to check the readers
   behind the checksum (aot-section-bounds.qtest, aot-ir-value-ids.qtest, AOTModuleMetadataSection.qtest) set it
   again with this tool after patching.  The metadata is the <length> bytes at <offset> of the file, which a test
   takes from the module before it patches it (a patch of the section directory changes the length that the
   directory gives, but not that of the metadata that the module passes to the loader); without them, it is the first
   "QORD" header of an uncompressed metadata blob whose section directory and string pool fit in the file.  The
   checksum is written after the metadata.  Exits with 0 if the checksum was set, 1 otherwise.
*/

#define XXH_NO_XXH3
#define XXH_STATIC_LINKING_ONLY
#include <qore/intern/xxhash/xxhash.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

static constexpr size_t HeaderSize = 60;
static constexpr size_t SectionEntrySize = 12;
static constexpr unsigned ChecksumVersion = 43;
static constexpr size_t ChecksumSize = 8;
static constexpr uint16_t ChecksumFlag = 0x0004;

static uint32_t u32(const std::vector<uint8_t>& b, size_t p) {
    return b[p] | (b[p + 1] << 8) | (b[p + 2] << 16) | (static_cast<uint32_t>(b[p + 3]) << 24);
}

//! returns the length of the uncompressed metadata at pos without its checksum, or 0 if there is none
static size_t metadataLength(const std::vector<uint8_t>& b, size_t pos) {
    if (pos + HeaderSize > b.size() || memcmp(b.data() + pos, "QORD", 4)) {
        return 0;
    }
    unsigned version = b[pos + 4] | (b[pos + 5] << 8);
    uint32_t count = u32(b, pos + 16);
    // uncompressed metadata and string pool only
    if (version < ChecksumVersion || b[pos + 34] || b[pos + 35] || !count || count > 1000) {
        return 0;
    }
    size_t pool_size_pos = pos + HeaderSize + count * SectionEntrySize;
    if (pool_size_pos + 4 > b.size()) {
        return 0;
    }
    size_t data_pos = pool_size_pos + 4 + u32(b, pool_size_pos);
    if (data_pos > b.size()) {
        return 0;
    }
    uint64_t data_size = 0;
    for (uint32_t i = 0; i < count; ++i) {
        size_t e = pos + HeaderSize + i * SectionEntrySize;
        uint64_t end = static_cast<uint64_t>(u32(b, e + 4)) + u32(b, e + 8);
        if (end > data_size) {
            data_size = end;
        }
    }
    size_t len = data_pos - pos + data_size;
    return pos + len + ChecksumSize <= b.size() ? len : 0;
}

int main(int argc, char** argv) {
    if (argc != 2 && argc != 4) {
        fprintf(stderr, "usage: %s <file> [<offset> <length>]\n", argv[0]);
        return 1;
    }
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    size_t pos = 0;
    size_t len = 0;
    if (argc == 4) {
        pos = strtoull(argv[2], nullptr, 10);
        len = strtoull(argv[3], nullptr, 10);
        if (len < HeaderSize || pos + len + ChecksumSize > b.size() || memcmp(b.data() + pos, "QORD", 4)) {
            fprintf(stderr, "%s: no AOT metadata of %zu bytes at offset %zu\n", argv[1], len, pos);
            return 1;
        }
    } else {
        for (; pos + HeaderSize <= b.size(); ++pos) {
            if ((len = metadataLength(b, pos))) {
                break;
            }
        }
        if (!len) {
            fprintf(stderr, "%s: no uncompressed AOT metadata with a checksum\n", argv[1]);
            return 1;
        }
    }
    b[pos + 6] |= ChecksumFlag & 0xff;
    uint64_t checksum = XXH64(b.data() + pos, len, 0);
    for (size_t j = 0; j < ChecksumSize; ++j) {
        b[pos + len + j] = static_cast<uint8_t>((checksum >> (j * 8)) & 0xff);
    }
    std::ofstream out(argv[1], std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    if (!out) {
        fprintf(stderr, "%s: cannot write the file\n", argv[1]);
        return 1;
    }
    return 0;
}
