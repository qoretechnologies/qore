/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

// qore#6451: the checksum of AOT metadata is found after the encoded metadata also when the data is longer than the
// metadata and its checksum: the data of a symbol of a Mach-O object file, whose size is taken up to the next symbol
// and includes the padding after it.  Sealed metadata, uncompressed and compressed with zlib and Zstandard, opens as
// written and with padding after its checksum, and the input that the reader keeps is the metadata and its checksum
// without the padding, so that a copy of it opens again without verification; a changed byte is refused with and
// without padding, and so is metadata cut before its checksum.

#define XXH_NO_XXH3
#define XXH_STATIC_LINKING_ONLY
#include <qore/intern/xxhash/xxhash.h>

#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/intern/QoreAOTBinary.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

static unsigned count = 0;

//! the section data of an open reader is the metadata's, not the padding's
static bool readSection(QoreAOTBinaryReader& reader, std::string& error) {
    const QoreAOTSectionHeader* sh = reader.findSection(QoreAOTSectionType::TOPLEVEL);
    if (!sh || !reader.getSectionData(*sh)) {
        error = "no section data";
        return false;
    }
    return true;
}

//! checks that the data opens or is refused with a checksum mismatch
static bool check(const std::string& label, const std::vector<uint8_t>& data, bool expect_open) {
    ++count;
    QoreAOTBinaryReader reader;
    std::string error;
    bool ok = reader.open(data.data(), static_cast<uint32_t>(data.size()), error) && readSection(reader, error);
    bool mismatch = reader.checksumMismatch();
    if (ok != expect_open || (!expect_open && !mismatch)) {
        fprintf(stderr, "%s: opened=%d mismatch=%d error='%s', expected %s\n", label.c_str(), ok, mismatch,
            error.c_str(), expect_open ? "to open" : "a checksum mismatch");
        return false;
    }
    return true;
}

//! checks that the input that the reader keeps for padded data is the metadata and its checksum (\a sealed_size), and
//! that a copy of it opens again without verification, as the debug metadata of a fragment is opened
static bool checkReopen(const std::string& label, const std::vector<uint8_t>& padded, size_t sealed_size) {
    ++count;
    QoreAOTBinaryReader reader;
    std::string error;
    if (!reader.open(padded.data(), static_cast<uint32_t>(padded.size()), error)) {
        fprintf(stderr, "%s: cannot open: %s\n", label.c_str(), error.c_str());
        return false;
    }
    if (reader.getInputSize() != sealed_size) {
        fprintf(stderr, "%s: the input size is %u, expected %zu\n", label.c_str(), reader.getInputSize(),
            sealed_size);
        return false;
    }
    std::vector<uint8_t> copy(reader.getInputData(), reader.getInputData() + reader.getInputSize());
    QoreAOTBinaryReader copy_reader;
    if (!copy_reader.open(copy.data(), static_cast<uint32_t>(copy.size()), error, false)
            || !readSection(copy_reader, error)) {
        fprintf(stderr, "%s: the copy of the input does not open without verification: %s\n", label.c_str(),
            error.c_str());
        return false;
    }
    return true;
}

//! returns the sealed metadata with its body compressed with zlib or Zstandard, sealed again
static bool compress(const std::vector<uint8_t>& sealed, uint8_t compression, std::vector<uint8_t>& out) {
    std::vector<uint8_t> body(sealed.begin() + QORE_AOT_HEADER_SIZE, sealed.end() - QORE_AOT_METADATA_CHECKSUM_SIZE);
    std::vector<uint8_t> compressed;
    std::string error;
    bool ok = compression == QORE_AOT_COMPRESSION_ZSTD
        ? compressMetadataZstd(body, compressed, error)
        : compressMetadata(body, compressed, error);
    if (!ok) {
        fprintf(stderr, "cannot compress the metadata: %s\n", error.c_str());
        return false;
    }
    out.assign(sealed.begin(), sealed.begin() + QORE_AOT_HEADER_SIZE);
    out[34] = compression;
    out.insert(out.end(), compressed.begin(), compressed.end());
    uint64_t sum = XXH64(out.data(), out.size(), 0);
    for (unsigned i = 0; i < QORE_AOT_METADATA_CHECKSUM_SIZE; ++i) {
        out.push_back(static_cast<uint8_t>(sum >> (i * 8)));
    }
    return true;
}

//! the checks of one encoding of the metadata
static bool checkEncoding(const std::string& name, const std::vector<uint8_t>& sealed) {
    bool ok = check(name + " as written", sealed, true);

    // the padding of a symbol of a Mach-O object file after the checksum, and other data (the next symbol)
    for (size_t pad : {1u, 8u, 15u, 64u}) {
        std::vector<uint8_t> padded = sealed;
        padded.insert(padded.end(), pad, 0);
        ok = check(name + " padded by " + std::to_string(pad), padded, true) && ok;
        std::fill(padded.end() - pad, padded.end(), 0xa5);
        ok = check(name + " followed by " + std::to_string(pad) + " other bytes", padded, true) && ok;
        if (pad == 15) {
            ok = checkReopen(name + " input of padded data", padded, sealed.size()) && ok;
        }
    }

    // a changed byte of the metadata, with and without padding
    std::vector<uint8_t> changed = sealed;
    changed[QORE_AOT_HEADER_SIZE + (changed.size() - QORE_AOT_HEADER_SIZE) / 2] ^= 0x01;
    ok = check(name + " changed byte", changed, false) && ok;
    changed.insert(changed.end(), 16, 0);
    ok = check(name + " changed byte, padded", changed, false) && ok;

    // a changed checksum, with padding
    std::vector<uint8_t> bad_checksum = sealed;
    bad_checksum[bad_checksum.size() - 1] ^= 0x80;
    bad_checksum.insert(bad_checksum.end(), 16, 0);
    ok = check(name + " changed checksum, padded", bad_checksum, false) && ok;

    // metadata cut before its checksum
    std::vector<uint8_t> cut(sealed.begin(), sealed.end() - 4);
    ok = check(name + " cut before its checksum", cut, false) && ok;
    return ok;
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    bool ok = true;

    QoreAOTBinaryWriter writer;
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    writer.writeStringRef("checksum");
    for (unsigned i = 0; i < 256; ++i) {
        writer.writeU32(i * 7);
    }
    writer.endSection(section);
    QoreAOTBinaryHeader header{};
    header.magic = QORE_AOT_BINARY_MAGIC;
    header.version = QORE_AOT_BINARY_VERSION;
    std::vector<uint8_t> sealed;
    if (!writer.finalize(header, sealed)) {
        fprintf(stderr, "cannot finalize the metadata\n");
        return 1;
    }
    ok = checkEncoding("uncompressed", sealed) && ok;

    std::vector<uint8_t> zlib;
    std::vector<uint8_t> zstd;
    if (!compress(sealed, QORE_AOT_COMPRESSION_ZLIB, zlib) || !compress(sealed, QORE_AOT_COMPRESSION_ZSTD, zstd)) {
        return 1;
    }
    ok = checkEncoding("zlib", zlib) && ok;
    ok = checkEncoding("zstd", zstd) && ok;

    qore_cleanup();
    if (ok) {
        printf("PASS: %u AOT metadata checksum checks\n", count);
    }
    return ok ? 0 : 1;
}
