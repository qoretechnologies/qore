/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

// qore#6451: the checksum of AOT metadata is found after the encoded metadata also when the data is longer than the
// metadata and its checksum: the data of a symbol of a Mach-O object file, whose size is taken up to the next symbol
// and includes the padding after it.  Sealed metadata opens as written and with padding after its checksum; a changed
// byte is refused with and without padding, and so is metadata cut before its checksum.

#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/intern/QoreAOTBinary.h>

#include <cstdio>
#include <string>
#include <vector>

static unsigned count = 0;

//! opens the data; returns true if it opens, or false with the checksum mismatch flag
static bool openData(const std::vector<uint8_t>& data, bool& mismatch, std::string& error) {
    QoreAOTBinaryReader reader;
    bool ok = reader.open(data.data(), static_cast<uint32_t>(data.size()), error);
    mismatch = reader.checksumMismatch();
    if (ok) {
        // the sections are read from the metadata, not from the padding
        const QoreAOTSectionHeader* sh = reader.findSection(QoreAOTSectionType::TOPLEVEL);
        if (!sh || !reader.getSectionData(*sh)) {
            error = "no section data";
            return false;
        }
    }
    return ok;
}

static bool check(const char* label, const std::vector<uint8_t>& data, bool expect_open) {
    ++count;
    bool mismatch;
    std::string error;
    bool ok = openData(data, mismatch, error);
    if (ok != expect_open || (!expect_open && !mismatch)) {
        fprintf(stderr, "%s: opened=%d mismatch=%d error='%s', expected %s\n", label, ok, mismatch, error.c_str(),
            expect_open ? "to open" : "a checksum mismatch");
        return false;
    }
    return true;
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    bool ok = true;

    QoreAOTBinaryWriter writer;
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    writer.writeStringRef("checksum");
    for (unsigned i = 0; i < 64; ++i) {
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

    ok = check("as written", sealed, true) && ok;

    // the padding of a symbol of a Mach-O object file after the checksum
    for (size_t pad : {1u, 8u, 15u, 64u}) {
        std::vector<uint8_t> padded = sealed;
        padded.insert(padded.end(), pad, 0);
        ok = check(("padded by " + std::to_string(pad)).c_str(), padded, true) && ok;
        // padding that is not zero, as the data of the next symbol
        std::fill(padded.end() - pad, padded.end(), 0xa5);
        ok = check(("followed by " + std::to_string(pad) + " other bytes").c_str(), padded, true) && ok;
    }

    // a changed byte of the metadata, with and without padding
    std::vector<uint8_t> changed = sealed;
    changed[changed.size() / 2] ^= 0x01;
    ok = check("changed byte", changed, false) && ok;
    changed.insert(changed.end(), 16, 0);
    ok = check("changed byte, padded", changed, false) && ok;

    // a changed checksum, with padding
    std::vector<uint8_t> bad_checksum = sealed;
    bad_checksum[bad_checksum.size() - 1] ^= 0x80;
    bad_checksum.insert(bad_checksum.end(), 16, 0);
    ok = check("changed checksum, padded", bad_checksum, false) && ok;

    // metadata cut before its checksum
    std::vector<uint8_t> cut(sealed.begin(), sealed.end() - 4);
    ok = check("cut before its checksum", cut, false) && ok;

    qore_cleanup();
    if (ok) {
        printf("PASS: %u AOT metadata checksum checks\n", count);
    }
    return ok ? 0 : 1;
}
