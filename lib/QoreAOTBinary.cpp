/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTBinary.cpp

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

#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/QoreIR.h"

#include "qore/intern/QoreJITIncludes.h"
#include <qore/QoreRegexInterface.h>
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/QoreTypeInfo.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/QoreParseTypeInfo.h"
#include "qore/intern/qore_enum_decl_private.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/StaticClassVarRefNode.h"
#include "qore/intern/SelfVarrefNode.h"
#include "qore/intern/CaseNodeRegex.h"
#include "qore/intern/QoreRegex.h"
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/Function.h"
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/QorePlusOperatorNode.h"
#include "qore/intern/QoreRangeOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/QoreSquareBracketsRangeOperatorNode.h"
#include "qore/intern/QoreExistsOperatorNode.h"
#include "qore/intern/QoreImplicitArgumentNode.h"
#include "qore/intern/QoreMinusOperatorNode.h"
#include "qore/intern/QoreKeysOperatorNode.h"
#include "qore/intern/QoreMultiplicationOperatorNode.h"
#include "qore/intern/QoreDivisionOperatorNode.h"
#include "qore/intern/QoreModuloOperatorNode.h"
#include "qore/intern/QoreBinaryAndOperatorNode.h"
#include "qore/intern/QoreBinaryOrOperatorNode.h"
#include "qore/intern/QoreBinaryXorOperatorNode.h"
#include "qore/intern/QoreShiftLeftOperatorNode.h"
#include "qore/intern/QoreShiftRightOperatorNode.h"
#include "qore/intern/QoreImplicitElementNode.h"
#include "qore/intern/QoreInstanceOfOperatorNode.h"
#include "qore/intern/QoreRegexNMatchOperatorNode.h"
#include "qore/intern/QoreRegexExtractOperatorNode.h"
#include "qore/intern/QorePreIncrementOperatorNode.h"
#include "qore/intern/QorePreDecrementOperatorNode.h"
#include "qore/intern/QorePostIncrementOperatorNode.h"
#include "qore/intern/QorePostDecrementOperatorNode.h"
#include "qore/intern/QoreIntPostIncrementOperatorNode.h"
#include "qore/intern/QoreIntPostDecrementOperatorNode.h"
#include "qore/intern/QoreLogicalAndOperatorNode.h"
#include "qore/intern/QoreLogicalOrOperatorNode.h"
#include "qore/intern/QoreLogicalEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalAbsoluteEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalAbsoluteNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotOperatorNode.h"
#include "qore/intern/QoreNullCoalescingOperatorNode.h"
#include "qore/intern/QoreValueCoalescingOperatorNode.h"
#include "qore/intern/QoreQuestionMarkOperatorNode.h"
#include "qore/intern/QoreFoldlOperatorNode.h"
#include "qore/intern/QoreIterateOperatorNode.h"
#include "qore/intern/QoreMapOperatorNode.h"
#include "qore/intern/QoreMapSelectOperatorNode.h"
#include "qore/intern/QoreHashMapOperatorNode.h"
#include "qore/intern/QoreHashMapSelectOperatorNode.h"
#include "qore/intern/QoreSelectOperatorNode.h"
#include "qore/intern/QoreStreamingOperatorNode.h"
#include "qore/intern/QoreElementsOperatorNode.h"
#include "qore/intern/QoreDeleteOperatorNode.h"
#include "qore/intern/QoreRemoveOperatorNode.h"
#include "qore/intern/QoreBackgroundOperatorNode.h"
#include "qore/intern/QoreTrimOperatorNode.h"
#include "qore/intern/QoreChompOperatorNode.h"
#include "qore/intern/QorePopOperatorNode.h"
#include "qore/intern/QoreShiftOperatorNode.h"
#include "qore/intern/QorePushOperatorNode.h"
#include "qore/intern/QoreUnshiftOperatorNode.h"
#include "qore/intern/QoreUnaryMinusOperatorNode.h"
#include "qore/intern/QoreAssignmentOperatorNode.h"
#include "qore/intern/QorePlusEqualsOperatorNode.h"
#include "qore/intern/QoreMinusEqualsOperatorNode.h"
#include "qore/intern/QoreMultiplyEqualsOperatorNode.h"
#include "qore/intern/QoreDivideEqualsOperatorNode.h"
#include "qore/intern/QoreModuloEqualsOperatorNode.h"
#include "qore/intern/QoreAndEqualsOperatorNode.h"
#include "qore/intern/QoreOrEqualsOperatorNode.h"
#include "qore/intern/QoreXorEqualsOperatorNode.h"
#include "qore/intern/QoreShiftLeftEqualsOperatorNode.h"
#include "qore/intern/QoreShiftRightEqualsOperatorNode.h"
#include "qore/intern/ContextrefNode.h"
#include "qore/intern/ContextRowNode.h"
#include "qore/intern/ComplexContextrefNode.h"
#include "qore/intern/CallReferenceCallNode.h"
#include "qore/intern/ParseNode.h"
#include "qore/intern/ScopedObjectCallNode.h"
#include <qore/intern/ParseReferenceNode.h>
#include "qore/intern/NewComplexTypeNode.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/QoreCastOperatorNode.h"
#include <qore/QoreEnumDecl.h>

#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreIRBuilder.h"
#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRVerifier.h"
#include "qore/intern/QoreAOTInstRegistry.h"
#include "qore/intern/QoreAOTExprSlotRegistry.h"
#include "qore/intern/QoreAOTExprRegistry.h"
#include "qore/intern/QoreAOTExprNodeRegistry.h"
#include "qore/intern/QorePluginRegistry.h"

#include <qore/QoreBigFloatNode.h>
#include <qore/QoreBigIntNode.h>
#include <qore/QoreBufferNode.h>
#include <qore/QoreNothingNode.h>
#include <qore/QoreObject.h>
#include <qore/QoreStringNode.h>

#include <cassert>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>
#include <iomanip>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>
#include <cstdlib>
#include <typeinfo>
#include <unordered_set>
#include <zlib.h>

static thread_local std::string qore_aot_expr_serialization_error;

extern std::string getVariantKey(const char* name, const AbstractQoreFunctionVariant* variant);

// ---------------------------------------------------------------------------
// AOT PC->loc trailer (lazy on-throw source-location maps). See QoreAOTBinary.h
// for the on-disk layout.
// ---------------------------------------------------------------------------
namespace {
inline void pcmapPut32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}
inline bool pcmapGet32(const uint8_t* data, size_t len, size_t& p, uint32_t& v) {
    if (p + 4 > len) {
        return false;
    }
    v = static_cast<uint32_t>(data[p])
        | (static_cast<uint32_t>(data[p + 1]) << 8)
        | (static_cast<uint32_t>(data[p + 2]) << 16)
        | (static_cast<uint32_t>(data[p + 3]) << 24);
    p += 4;
    return true;
}
} // anonymous namespace

size_t qoreAOTSerializePcLocPayload(const std::vector<AOTCompiledFuncWithSlots>& func_slots,
        std::vector<uint8_t>& out) {
    const size_t count_pos = out.size();
    pcmapPut32(out, 0);  // placeholder for num_funcs
    uint32_t n = 0;
    for (const auto& fws : func_slots) {
        if (fws.pc_loc_map.empty() || fws.llvm_symbol.empty()) {
            continue;
        }
        ++n;
        pcmapPut32(out, static_cast<uint32_t>(fws.llvm_symbol.size()));
        out.insert(out.end(), fws.llvm_symbol.begin(), fws.llvm_symbol.end());
        pcmapPut32(out, static_cast<uint32_t>(fws.pc_loc_map.size()));
        for (const auto& e : fws.pc_loc_map) {
            pcmapPut32(out, e.first);
            pcmapPut32(out, e.second);
        }
    }
    // Backfill num_funcs now that it is known.
    out[count_pos]     = static_cast<uint8_t>(n & 0xff);
    out[count_pos + 1] = static_cast<uint8_t>((n >> 8) & 0xff);
    out[count_pos + 2] = static_cast<uint8_t>((n >> 16) & 0xff);
    out[count_pos + 3] = static_cast<uint8_t>((n >> 24) & 0xff);
    return n;
}

bool qoreAOTParsePcLocPayload(const uint8_t* data, size_t len,
        std::vector<AOTPcLocFuncEntry>& out) {
    size_t p = 0;
    uint32_t n = 0;
    if (!pcmapGet32(data, len, p, n)) {
        return false;
    }
    out.clear();
    out.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t slen = 0;
        if (!pcmapGet32(data, len, p, slen) || p + slen > len) {
            return false;
        }
        AOTPcLocFuncEntry m;
        m.symbol.assign(reinterpret_cast<const char*>(data + p), slen);
        p += slen;
        uint32_t ne = 0;
        if (!pcmapGet32(data, len, p, ne)) {
            return false;
        }
        m.entries.reserve(ne);
        for (uint32_t j = 0; j < ne; ++j) {
            uint32_t off = 0, loc = 0;
            if (!pcmapGet32(data, len, p, off) || !pcmapGet32(data, len, p, loc)) {
                return false;
            }
            m.entries.emplace_back(off, loc);
        }
        out.push_back(std::move(m));
    }
    return true;
}

bool qoreAOTAppendPcLocTrailer(const std::string& path, const std::vector<uint8_t>& payload,
        std::string& error) {
    if (payload.empty()) {
        return true;
    }
    FILE* f = fopen(path.c_str(), "ab");
    if (!f) {
        error = "failed to open '" + path + "' to append PC->loc trailer: " + strerror(errno);
        return false;
    }
    bool ok = fwrite(payload.data(), 1, payload.size(), f) == payload.size();
    if (ok) {
        uint8_t footer[QORE_AOT_PCMAP_FOOTER_SIZE];
        uint64_t plen = payload.size();
        uint32_t magic = QORE_AOT_PCMAP_MAGIC;
        uint32_t ver = QORE_AOT_PCMAP_VERSION;
        memcpy(footer, &plen, 8);
        memcpy(footer + 8, &magic, 4);
        memcpy(footer + 12, &ver, 4);
        ok = fwrite(footer, 1, QORE_AOT_PCMAP_FOOTER_SIZE, f) == QORE_AOT_PCMAP_FOOTER_SIZE;
    }
    if (fclose(f) != 0) {
        ok = false;
    }
    if (!ok) {
        error = "failed to write PC->loc trailer to '" + path + "': " + strerror(errno);
        return false;
    }
    return true;
}

bool qoreAOTReadPcLocTrailer(const std::string& path, std::vector<AOTPcLocFuncEntry>& out) {
    out.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    bool ok = false;
    do {
        if (fseek(f, 0, SEEK_END) != 0) {
            break;
        }
        long fsize = ftell(f);
        if (fsize < static_cast<long>(QORE_AOT_PCMAP_FOOTER_SIZE)) {
            break;
        }
        if (fseek(f, fsize - static_cast<long>(QORE_AOT_PCMAP_FOOTER_SIZE), SEEK_SET) != 0) {
            break;
        }
        uint8_t footer[QORE_AOT_PCMAP_FOOTER_SIZE];
        if (fread(footer, 1, QORE_AOT_PCMAP_FOOTER_SIZE, f) != QORE_AOT_PCMAP_FOOTER_SIZE) {
            break;
        }
        uint64_t plen = 0;
        uint32_t magic = 0, ver = 0;
        memcpy(&plen, footer, 8);
        memcpy(&magic, footer + 8, 4);
        memcpy(&ver, footer + 12, 4);
        if (magic != QORE_AOT_PCMAP_MAGIC || ver != QORE_AOT_PCMAP_VERSION) {
            break;
        }
        if (plen == 0 || plen > static_cast<uint64_t>(fsize) - QORE_AOT_PCMAP_FOOTER_SIZE) {
            break;
        }
        std::vector<uint8_t> payload(plen);
        if (fseek(f, fsize - static_cast<long>(QORE_AOT_PCMAP_FOOTER_SIZE)
                - static_cast<long>(plen), SEEK_SET) != 0) {
            break;
        }
        if (fread(payload.data(), 1, plen, f) != plen) {
            break;
        }
        ok = qoreAOTParsePcLocPayload(payload.data(), payload.size(), out);
    } while (false);
    fclose(f);
    if (!ok) {
        out.clear();
    }
    return ok;
}

void qoreAOTFramePcLocSectionRecord(const std::vector<uint8_t>& payload, std::vector<uint8_t>& out) {
    if (payload.empty()) {
        return;
    }
    pcmapPut32(out, QORE_AOT_PCMAP_MAGIC);
    pcmapPut32(out, static_cast<uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
}

// Locate the `qore_aot_pcloc` section bytes in a 64-bit ELF file via a self-contained
// section-header walk (no libLLVM). Returns true and fills `sec` with the section
// contents (copied) when found.
static bool readElfSectionBytes(const std::string& path, const char* sec_name,
        std::vector<uint8_t>& sec) {
    sec.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    bool ok = false;
    do {
        unsigned char e_ident[16];
        if (fread(e_ident, 1, 16, f) != 16) {
            break;
        }
        // ELF magic + 64-bit class + little-endian (the only AOT target layout).
        if (e_ident[0] != 0x7f || e_ident[1] != 'E' || e_ident[2] != 'L' || e_ident[3] != 'F') {
            break;
        }
        if (e_ident[4] != 2 /*ELFCLASS64*/ || e_ident[5] != 1 /*ELFDATA2LSB*/) {
            break;
        }
        // Read the rest of the ELF64 header fields we need.
        // Offsets within Elf64_Ehdr: e_shoff@40 (8), e_shentsize@58 (2), e_shnum@60 (2),
        // e_shstrndx@62 (2).
        unsigned char hdr[64 - 16];
        if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
            break;
        }
        auto rd16 = [](const unsigned char* p) -> uint16_t {
            return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
        };
        auto rd64 = [](const unsigned char* p) -> uint64_t {
            uint64_t v = 0;
            for (int i = 0; i < 8; ++i) {
                v |= static_cast<uint64_t>(p[i]) << (8 * i);
            }
            return v;
        };
        uint64_t e_shoff = rd64(hdr + (40 - 16));
        uint16_t e_shentsize = rd16(hdr + (58 - 16));
        uint16_t e_shnum = rd16(hdr + (60 - 16));
        uint16_t e_shstrndx = rd16(hdr + (62 - 16));
        if (!e_shoff || !e_shnum || e_shentsize < 64 || e_shstrndx >= e_shnum) {
            break;
        }
        // Read the section header table.
        std::vector<unsigned char> sh(static_cast<size_t>(e_shentsize) * e_shnum);
        if (fseek(f, static_cast<long>(e_shoff), SEEK_SET) != 0
                || fread(sh.data(), 1, sh.size(), f) != sh.size()) {
            break;
        }
        // Within Elf64_Shdr: sh_name@0 (4), sh_offset@24 (8), sh_size@32 (8).
        auto shdr = [&](uint16_t i) -> const unsigned char* {
            return sh.data() + static_cast<size_t>(i) * e_shentsize;
        };
        auto rd32 = [](const unsigned char* p) -> uint32_t {
            return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
                | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
        };
        // Read .shstrtab to resolve section names.
        const unsigned char* str_sh = shdr(e_shstrndx);
        uint64_t str_off = rd64(str_sh + 24);
        uint64_t str_size = rd64(str_sh + 32);
        if (!str_size) {
            break;
        }
        std::vector<char> strtab(str_size);
        if (fseek(f, static_cast<long>(str_off), SEEK_SET) != 0
                || fread(strtab.data(), 1, str_size, f) != str_size) {
            break;
        }
        size_t name_len = strlen(sec_name);
        for (uint16_t i = 0; i < e_shnum; ++i) {
            const unsigned char* s = shdr(i);
            uint32_t nm = rd32(s + 0);
            if (nm >= str_size) {
                continue;
            }
            if (strncmp(strtab.data() + nm, sec_name, name_len) != 0
                    || strtab[nm + name_len] != '\0') {
                continue;
            }
            uint64_t off = rd64(s + 24);
            uint64_t size = rd64(s + 32);
            if (!size) {
                break;
            }
            sec.resize(size);
            if (fseek(f, static_cast<long>(off), SEEK_SET) != 0
                    || fread(sec.data(), 1, size, f) != size) {
                sec.clear();
                break;
            }
            ok = true;
            break;
        }
    } while (false);
    fclose(f);
    return ok;
}

// Little-endian fixed-width readers shared by the Mach-O walk below.
static inline uint32_t machoRd32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
static inline uint64_t machoRd64(const unsigned char* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(p[i]) << (8 * i);
    }
    return v;
}

// Walk one 64-bit Mach-O image starting at file offset `slice_off`, accumulating the
// bytes of every (seg,sect) section that matches into `sec`. Self-contained (no libLLVM).
static void readMachoSliceSections(FILE* f, uint64_t slice_off, const char* seg,
        const char* sect, std::vector<uint8_t>& sec) {
    unsigned char mh[32];
    if (fseek(f, static_cast<long>(slice_off), SEEK_SET) != 0
            || fread(mh, 1, sizeof(mh), f) != sizeof(mh)) {
        return;
    }
    // MH_MAGIC_64 (0xFEEDFACF) only — AOT targets are little-endian arm64 / x86_64.
    if (machoRd32(mh) != 0xFEEDFACFu) {
        return;
    }
    uint32_t ncmds = machoRd32(mh + 16);
    uint32_t sizeofcmds = machoRd32(mh + 20);
    if (!ncmds || !sizeofcmds || sizeofcmds > (64u << 20)) {
        return;
    }
    std::vector<unsigned char> cmds(sizeofcmds);
    if (fread(cmds.data(), 1, sizeofcmds, f) != sizeofcmds) {
        return;
    }
    const size_t seg_len = strlen(seg);
    const size_t sect_len = strlen(sect);
    size_t p = 0;
    for (uint32_t i = 0; i < ncmds && p + 8 <= sizeofcmds; ++i) {
        uint32_t cmd = machoRd32(cmds.data() + p);
        uint32_t cmdsize = machoRd32(cmds.data() + p + 4);
        if (cmdsize < 8 || p + cmdsize > sizeofcmds) {
            break;
        }
        if (cmd == 0x19u /*LC_SEGMENT_64*/ && cmdsize >= 72) {
            uint32_t nsects = machoRd32(cmds.data() + p + 64);
            // section_64 records follow the 72-byte segment_command_64 header.
            if (static_cast<uint64_t>(nsects) * 80 + 72 <= cmdsize) {
                for (uint32_t s = 0; s < nsects; ++s) {
                    const unsigned char* sc = cmds.data() + p + 72 + static_cast<size_t>(s) * 80;
                    // sectname@0 (16), segname@16 (16), addr@32, size@40, offset@48 (u32).
                    if (strncmp(reinterpret_cast<const char*>(sc), sect, sect_len) == 0
                            && (sect_len == 16 || sc[sect_len] == '\0')
                            && strncmp(reinterpret_cast<const char*>(sc + 16), seg, seg_len) == 0
                            && (seg_len == 16 || sc[16 + seg_len] == '\0')) {
                        uint64_t size = machoRd64(sc + 40);
                        uint32_t off = machoRd32(sc + 48);
                        if (size && size < (256u << 20)) {
                            std::vector<uint8_t> buf(size);
                            if (fseek(f, static_cast<long>(slice_off + off), SEEK_SET) == 0
                                    && fread(buf.data(), 1, size, f) == size) {
                                sec.insert(sec.end(), buf.begin(), buf.end());
                            }
                        }
                    }
                }
            }
        }
        p += cmdsize;
    }
}

// Locate the (seg,sect) Mach-O section bytes in a thin or FAT 64-bit Mach-O file,
// accumulating every matching section (the linker concatenates same-named sections, and
// the framed-record reader walks the result). Returns true when any bytes were found.
static bool readMachoSectionBytes(const std::string& path, const char* seg, const char* sect,
        std::vector<uint8_t>& sec) {
    sec.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    unsigned char magic[4];
    if (fread(magic, 1, 4, f) != 4) {
        fclose(f);
        return false;
    }
    // FAT headers store fields big-endian. Select the slice matching this process's arch.
    auto rd32be = [](const unsigned char* p) -> uint32_t {
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
            | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    };
    uint32_t m = rd32be(magic);
    if (m == 0xCAFEBABEu || m == 0xCAFEBABFu) {  // FAT_MAGIC / FAT_MAGIC_64
        bool fat64 = (m == 0xCAFEBABFu);
#if defined(__aarch64__) || defined(__arm64__)
        const uint32_t want_cpu = 0x0100000Cu;  // CPU_TYPE_ARM64
#elif defined(__x86_64__)
        const uint32_t want_cpu = 0x01000007u;  // CPU_TYPE_X86_64
#else
        const uint32_t want_cpu = 0;
#endif
        unsigned char nbuf[4];
        if (fread(nbuf, 1, 4, f) == 4) {
            uint32_t narch = rd32be(nbuf);
            for (uint32_t i = 0; i < narch && i < 64; ++i) {
                unsigned char a[32];
                size_t asz = fat64 ? 32 : 20;
                if (fread(a, 1, asz, f) != asz) {
                    break;
                }
                uint32_t cputype = rd32be(a);
                uint64_t off = fat64
                    ? ((static_cast<uint64_t>(rd32be(a + 8)) << 32) | rd32be(a + 12))
                    : rd32be(a + 8);
                if (want_cpu && cputype == want_cpu) {
                    readMachoSliceSections(f, off, seg, sect, sec);
                    break;
                }
            }
        }
    } else if (m == 0xCFFAEDFEu /*MH_MAGIC_64 on disk, LE*/ || machoRd32(magic) == 0xFEEDFACFu) {
        readMachoSliceSections(f, 0, seg, sect, sec);
    }
    fclose(f);
    return !sec.empty();
}

bool qoreAOTReadPcLocSection(const std::string& path, std::vector<AOTPcLocFuncEntry>& out) {
    out.clear();
    std::vector<uint8_t> sec;
    if (!readElfSectionBytes(path, QORE_AOT_PCLOC_SECTION_NAME, sec)
            && !readMachoSectionBytes(path, QORE_AOT_PCLOC_MACHO_SEG,
                QORE_AOT_PCLOC_MACHO_SECT, sec)) {
        return false;
    }
    // Walk concatenated [magic][len][payload] records.
    size_t p = 0;
    const size_t n = sec.size();
    while (p + 8 <= n) {
        uint32_t magic = 0, plen = 0;
        size_t q = p;
        if (!pcmapGet32(sec.data(), n, q, magic) || !pcmapGet32(sec.data(), n, q, plen)) {
            break;
        }
        if (magic != QORE_AOT_PCMAP_MAGIC || plen == 0 || q + plen > n) {
            break;
        }
        std::vector<AOTPcLocFuncEntry> recs;
        if (qoreAOTParsePcLocPayload(sec.data() + q, plen, recs)) {
            for (auto& r : recs) {
                out.push_back(std::move(r));
            }
        }
        p = q + plen;
    }
    if (out.empty()) {
        return false;
    }
    return true;
}

bool qoreAOTReadElfFuncSymbols(const std::string& path, std::vector<AOTElfFuncSym>& out,
        bool& out_is_et_dyn) {
    out.clear();
    out_is_et_dyn = false;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    bool ok = false;
    do {
        unsigned char e_ident[16];
        if (fread(e_ident, 1, 16, f) != 16) {
            break;
        }
        if (e_ident[0] != 0x7f || e_ident[1] != 'E' || e_ident[2] != 'L' || e_ident[3] != 'F'
                || e_ident[4] != 2 /*ELFCLASS64*/ || e_ident[5] != 1 /*ELFDATA2LSB*/) {
            break;
        }
        unsigned char hdr[64 - 16];
        if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
            break;
        }
        auto rd16 = [](const unsigned char* p) -> uint16_t {
            return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
        };
        // e_type is the first ELF64 header field after e_ident (offset 16 => hdr[0]).
        out_is_et_dyn = (rd16(hdr + 0) == 3 /*ET_DYN*/);
        auto rd32 = [](const unsigned char* p) -> uint32_t {
            return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
                | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
        };
        auto rd64 = [](const unsigned char* p) -> uint64_t {
            uint64_t v = 0;
            for (int i = 0; i < 8; ++i) {
                v |= static_cast<uint64_t>(p[i]) << (8 * i);
            }
            return v;
        };
        uint64_t e_shoff = rd64(hdr + (40 - 16));
        uint16_t e_shentsize = rd16(hdr + (58 - 16));
        uint16_t e_shnum = rd16(hdr + (60 - 16));
        if (!e_shoff || !e_shnum || e_shentsize < 64) {
            break;
        }
        std::vector<unsigned char> sh(static_cast<size_t>(e_shentsize) * e_shnum);
        if (fseek(f, static_cast<long>(e_shoff), SEEK_SET) != 0
                || fread(sh.data(), 1, sh.size(), f) != sh.size()) {
            break;
        }
        auto shdr = [&](uint32_t i) -> const unsigned char* {
            return sh.data() + static_cast<size_t>(i) * e_shentsize;
        };
        // Section header fields: sh_type@4(4), sh_link@40(4), sh_offset@24(8),
        // sh_size@32(8), sh_entsize@56(8).
        for (uint16_t i = 0; i < e_shnum; ++i) {
            const unsigned char* s = shdr(i);
            if (rd32(s + 4) != 2 /*SHT_SYMTAB*/) {
                continue;
            }
            uint64_t sym_off = rd64(s + 24);
            uint64_t sym_size = rd64(s + 32);
            uint64_t sym_entsz = rd64(s + 56);
            uint32_t strndx = rd32(s + 40);
            if (!sym_size || sym_entsz < 24 || strndx >= e_shnum) {
                continue;
            }
            const unsigned char* ss = shdr(strndx);
            uint64_t str_off = rd64(ss + 24);
            uint64_t str_size = rd64(ss + 32);
            if (!str_size) {
                continue;
            }
            std::vector<unsigned char> syms(sym_size);
            std::vector<char> strtab(str_size);
            if (fseek(f, static_cast<long>(sym_off), SEEK_SET) != 0
                    || fread(syms.data(), 1, sym_size, f) != sym_size) {
                break;
            }
            if (fseek(f, static_cast<long>(str_off), SEEK_SET) != 0
                    || fread(strtab.data(), 1, str_size, f) != str_size) {
                break;
            }
            uint64_t count = sym_size / sym_entsz;
            for (uint64_t k = 0; k < count; ++k) {
                const unsigned char* sym = syms.data() + k * sym_entsz;
                // Elf64_Sym: st_name@0(4), st_info@4(1), st_value@8(8), st_size@16(8).
                if ((sym[4] & 0xf) != 2 /*STT_FUNC*/) {
                    continue;
                }
                uint64_t st_size = rd64(sym + 16);
                if (!st_size) {
                    continue;
                }
                uint32_t nm = rd32(sym + 0);
                if (nm >= str_size) {
                    continue;
                }
                AOTElfFuncSym e;
                e.value = rd64(sym + 8);
                e.size = st_size;
                e.name.assign(strtab.data() + nm);
                out.push_back(std::move(e));
            }
            // typically a single SYMTAB; stop after the first non-empty one
            if (!out.empty()) {
                ok = true;
                break;
            }
        }
    } while (false);
    fclose(f);
    if (!ok) {
        out.clear();
    }
    return ok;
}

// Resolve the file offset of the 64-bit Mach-O image to parse (0 for thin; the matching
// arch slice for FAT). Returns false when `f` is not a 64-bit Mach-O.
static bool machoSliceOffset(FILE* f, uint64_t& slice_off) {
    slice_off = 0;
    if (fseek(f, 0, SEEK_SET) != 0) {
        return false;
    }
    unsigned char magic[4];
    if (fread(magic, 1, 4, f) != 4) {
        return false;
    }
    auto rd32be = [](const unsigned char* p) -> uint32_t {
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
            | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    };
    uint32_t m = rd32be(magic);
    if (m == 0xCAFEBABEu || m == 0xCAFEBABFu) {  // FAT_MAGIC / FAT_MAGIC_64
        bool fat64 = (m == 0xCAFEBABFu);
#if defined(__aarch64__) || defined(__arm64__)
        const uint32_t want_cpu = 0x0100000Cu;  // CPU_TYPE_ARM64
#elif defined(__x86_64__)
        const uint32_t want_cpu = 0x01000007u;  // CPU_TYPE_X86_64
#else
        const uint32_t want_cpu = 0;
#endif
        unsigned char nbuf[4];
        if (fread(nbuf, 1, 4, f) != 4) {
            return false;
        }
        uint32_t narch = rd32be(nbuf);
        for (uint32_t i = 0; i < narch && i < 64; ++i) {
            unsigned char a[32];
            size_t asz = fat64 ? 32 : 20;
            if (fread(a, 1, asz, f) != asz) {
                return false;
            }
            uint32_t cputype = rd32be(a);
            uint64_t off = fat64
                ? ((static_cast<uint64_t>(rd32be(a + 8)) << 32) | rd32be(a + 12))
                : rd32be(a + 8);
            if (want_cpu && cputype == want_cpu) {
                slice_off = off;
                return true;
            }
        }
        return false;
    }
    // Thin little-endian MH_MAGIC_64 (on disk: CF FA ED FE -> 0xCFFAEDFE big-endian).
    return m == 0xCFFAEDFEu;
}

// Read function symbols (value as offset from the image base, computed size, name with the
// Mach-O leading '_' stripped) from a thin/FAT 64-bit Mach-O LC_SYMTAB. `out_is_pie` is
// always true: Mach-O images are position-independent, so the runtime bias is dli_fbase
// and `value` is the offset from it (matching qoreAOTReadElfFuncSymbols' ET_DYN contract).
bool qoreAOTReadMachoFuncSymbols(const std::string& path, std::vector<AOTElfFuncSym>& out,
        bool& out_is_pie) {
    out.clear();
    out_is_pie = true;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    // Capture the file size so all allocations below can be bounded against it: a corrupted
    // or crafted nsyms/strsize must not trigger a huge allocation (this can run on the throw
    // path during exception-backtrace resolution, where a bad_alloc would be catastrophic).
    uint64_t fsize = 0;
    if (fseek(f, 0, SEEK_END) == 0) {
        long end = ftell(f);
        if (end > 0) {
            fsize = static_cast<uint64_t>(end);
        }
    }
    bool ok = false;
    do {
        uint64_t slice = 0;
        if (!machoSliceOffset(f, slice)) {
            break;
        }
        unsigned char mh[32];
        if (fseek(f, static_cast<long>(slice), SEEK_SET) != 0
                || fread(mh, 1, sizeof(mh), f) != sizeof(mh)
                || machoRd32(mh) != 0xFEEDFACFu) {
            break;
        }
        uint32_t ncmds = machoRd32(mh + 16);
        uint32_t sizeofcmds = machoRd32(mh + 20);
        if (!ncmds || !sizeofcmds || sizeofcmds > (64u << 20)
                || !fsize || slice + 32 + sizeofcmds > fsize) {
            break;
        }
        std::vector<unsigned char> cmds(sizeofcmds);
        if (fread(cmds.data(), 1, sizeofcmds, f) != sizeofcmds) {
            break;
        }
        // Locate __TEXT vmaddr/vmsize (image preferred base + text extent) and LC_SYMTAB.
        uint64_t text_vmaddr = 0, text_end = 0;
        uint32_t symoff = 0, nsyms = 0, stroff = 0, strsize = 0;
        bool have_text = false, have_symtab = false;
        size_t p = 0;
        for (uint32_t i = 0; i < ncmds && p + 8 <= sizeofcmds; ++i) {
            uint32_t cmd = machoRd32(cmds.data() + p);
            uint32_t cmdsize = machoRd32(cmds.data() + p + 4);
            if (cmdsize < 8 || p + cmdsize > sizeofcmds) {
                break;
            }
            if (cmd == 0x19u /*LC_SEGMENT_64*/ && cmdsize >= 72
                    && !memcmp(cmds.data() + p + 8, "__TEXT\0\0\0\0\0\0\0\0\0\0", 16)) {
                text_vmaddr = machoRd64(cmds.data() + p + 24);
                text_end = text_vmaddr + machoRd64(cmds.data() + p + 32);
                have_text = true;
            } else if (cmd == 0x2u /*LC_SYMTAB*/ && cmdsize >= 24) {
                symoff = machoRd32(cmds.data() + p + 8);
                nsyms = machoRd32(cmds.data() + p + 12);
                stroff = machoRd32(cmds.data() + p + 16);
                strsize = machoRd32(cmds.data() + p + 20);
                have_symtab = true;
            }
            p += cmdsize;
        }
        if (!have_text || !have_symtab || !nsyms || !strsize) {
            break;
        }
        // Bound the symbol- and string-table allocations: both must lie wholly within the
        // file (offsets are slice-relative). Rejects corrupted/oversized header fields.
        uint64_t sym_bytes = static_cast<uint64_t>(nsyms) * 16;
        if (!fsize || slice + symoff + sym_bytes > fsize || slice + stroff + strsize > fsize) {
            break;
        }
        std::vector<unsigned char> syms(static_cast<size_t>(sym_bytes));
        std::vector<char> strtab(strsize);
        if (fseek(f, static_cast<long>(slice + symoff), SEEK_SET) != 0
                || fread(syms.data(), 1, syms.size(), f) != syms.size()
                || fseek(f, static_cast<long>(slice + stroff), SEEK_SET) != 0
                || fread(strtab.data(), 1, strsize, f) != strsize) {
            break;
        }
        for (uint32_t k = 0; k < nsyms; ++k) {
            const unsigned char* s = syms.data() + static_cast<size_t>(k) * 16;
            // nlist_64: n_strx@0(4), n_type@4(1), n_sect@5(1), n_desc@6(2), n_value@8(8).
            uint8_t n_type = s[4];
            if (n_type & 0xe0) {  // N_STAB (debug) entry
                continue;
            }
            if ((n_type & 0x0e) != 0x0e) {  // N_TYPE != N_SECT
                continue;
            }
            uint64_t n_value = machoRd64(s + 8);
            if (n_value < text_vmaddr || n_value >= text_end) {  // only __TEXT functions
                continue;
            }
            uint32_t strx = machoRd32(s);
            if (strx == 0 || strx >= strsize) {
                continue;
            }
            const char* nm = strtab.data() + strx;
            if (!*nm) {
                continue;
            }
            AOTElfFuncSym e;
            e.value = n_value - text_vmaddr;  // offset from image base
            e.size = 0;                       // filled by next-symbol delta below
            e.name.assign(nm[0] == '_' ? nm + 1 : nm);
            out.push_back(std::move(e));
        }
        if (out.empty()) {
            break;
        }
        // Mach-O symbols carry no size; derive it from the gap to the next function symbol,
        // bounding the last by the __TEXT segment end.
        std::sort(out.begin(), out.end(),
            [](const AOTElfFuncSym& a, const AOTElfFuncSym& b) { return a.value < b.value; });
        uint64_t text_size = text_end - text_vmaddr;
        for (size_t i = 0; i < out.size(); ++i) {
            uint64_t next = (i + 1 < out.size()) ? out[i + 1].value : text_size;
            out[i].size = (next > out[i].value) ? (next - out[i].value) : 0;
        }
        ok = true;
    } while (false);
    fclose(f);
    if (!ok) {
        out.clear();
    }
    return ok;
}

static bool qorePluginQordTrace() {
    return std::getenv("QORE_PLUGIN_QORD_TRACE") != nullptr;
}

static void tracePluginQord(const std::string& msg) {
    if (qorePluginQordTrace()) {
        std::fprintf(stderr, "QORE_PLUGIN_QORD_TRACE: %s\n", msg.c_str());
    }
}

static void qoreAOTClearExprSerializationError() {
    qore_aot_expr_serialization_error.clear();
}

static void qoreAOTSetExprSerializationError(std::string msg) {
    if (qore_aot_expr_serialization_error.empty()) {
        qore_aot_expr_serialization_error = std::move(msg);
    }
}

static bool qoreAOTTakeExprSerializationError(std::string& error) {
    if (qore_aot_expr_serialization_error.empty()) {
        return false;
    }
    error = std::move(qore_aot_expr_serialization_error);
    qore_aot_expr_serialization_error.clear();
    return true;
}

static std::string qoreAOTDescribeExpr(const QoreValue& v) {
    if (!v.hasNode()) {
        std::string rv = "non-node QoreValue type ";
        rv += std::to_string(static_cast<int>(v.getType()));
        return rv;
    }
    const AbstractQoreNode* n = v.getInternalNode();
    if (!n) {
        return "null expression node";
    }
    std::string rv = "node '";
    rv += n->getTypeName();
    rv += "' (node type ";
    rv += std::to_string(n->getType());
    rv += ")";
    if (auto* pn = dynamic_cast<const ParseNode*>(n)) {
        if (pn->loc && (pn->loc->getFile() || pn->loc->getSource() || pn->loc->start_line >= 0)) {
            rv += ", location=";
            const char* file = pn->loc->getFileValue();
            rv += *file ? file : "<unknown>";
            if (pn->loc->start_line >= 0) {
                rv += ":";
                rv += std::to_string(pn->loc->start_line);
                if (pn->loc->end_line >= 0 && pn->loc->end_line != pn->loc->start_line) {
                    rv += "-";
                    rv += std::to_string(pn->loc->end_line);
                }
            }
            if (pn->loc->getSource() && *pn->loc->getSource()) {
                rv += ", source=";
                rv += pn->loc->getSource();
            }
            if (pn->loc->offset) {
                rv += ", offset=";
                rv += std::to_string(pn->loc->offset);
            }
        }
    }
    return rv;
}

static std::string qoreAOTBuildExprTreeFallbackDiagnostic(const QoreValue& expr,
        const std::vector<AOTLocalSlotId>& parent_locals,
        const AOTConstantReverseMap* const_reverse_map) {
    std::string msg = "unsupported inline native AOT expression; old fallback would require EXPR_TREE for ";
    msg += qoreAOTDescribeExpr(expr);

    AOTSlotMap temp_slots;
    for (size_t j = 0; j < parent_locals.size(); ++j) {
        if (parent_locals[j].local_var_ptr) {
            temp_slots.local_slots[parent_locals[j].local_var_ptr] = j;
        }
    }

    std::vector<uint8_t> blob;
    if (serializeExprTreeToBlob(expr, temp_slots, blob, false, const_reverse_map) && !blob.empty()) {
        uint8_t root_kind = blob[0];
        const auto* root_info = getAOTExprNodeKindInfo(root_kind);
        msg += ", EXPR_TREE root=";
        msg += root_info && root_info->name ? root_info->name : "UNKNOWN";
        msg += " (";
        msg += std::to_string(root_kind);
        msg += "), blob-size=";
        msg += std::to_string(blob.size());
    }
    msg += "; no fallback marker was emitted; add a native AOTExprKind serializer/reader or lower this "
        "operation to native IR";
    return msg;
}

static std::string qoreAOTQuoteDetail(const std::string& s) {
    constexpr size_t max_len = 240;
    if (s.size() <= max_len) {
        return s;
    }
    return s.substr(0, max_len) + "...";
}

static std::string qoreAOTDescribeLocation(const QoreProgramLocation* loc) {
    if (!loc) {
        return "<unknown>";
    }

    std::string rv;
    const char* file = loc->getFileValue();
    rv += (file && *file) ? file : "<unknown>";
    if (loc->start_line >= 0) {
        rv += ":";
        rv += std::to_string(loc->start_line);
        if (loc->end_line >= 0 && loc->end_line != loc->start_line) {
            rv += "-";
            rv += std::to_string(loc->end_line);
        }
    }
    if (loc->getSource() && *loc->getSource()) {
        rv += ", source=";
        rv += loc->getSource();
    }
    if (loc->offset) {
        rv += ", offset=";
        rv += std::to_string(loc->offset);
    }
    return rv;
}

static void qoreAOTAppendExprSlotContext(std::string& error, const AOTExprSlotId& expr,
        size_t expr_idx) {
    error += "; slot-index=";
    error += std::to_string(expr_idx);
    if (!expr.ref1.empty()) {
        error += "; ref1='";
        error += qoreAOTQuoteDetail(expr.ref1);
        error += "'";
    }
    if (!expr.ref2.empty()) {
        error += "; ref2='";
        error += qoreAOTQuoteDetail(expr.ref2);
        error += "'";
    }
    if (!expr.ref3.empty()) {
        error += "; ref3='";
        error += qoreAOTQuoteDetail(expr.ref3);
        error += "'";
    }
    if (expr.flags) {
        error += "; flags=0x";
        char buf[8];
        snprintf(buf, sizeof(buf), "%02x", expr.flags);
        error += buf;
    }
    if (expr.child_expr) {
        error += "; child=";
        error += qoreAOTDescribeExpr(expr.child_expr);
    }
    if (expr.call_args) {
        error += "; call-args=";
        error += std::to_string(expr.call_args->size());
    }
    if (expr.parse_args) {
        error += "; parse-args=";
        error += std::to_string(expr.parse_args->size());
    }
    error += "; no fallback marker was emitted";
}

// Defined in Function.cpp - collects all local variables from a StatementBlock and nested blocks
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);
extern void removeSignatureLocalsFromBodyLocals(std::vector<LocalVar*>& locals, const UserSignature* sig);

// Forward-reference class lookup map used by readValue VT_NEW_OBJECT during
// AOT class deserialization. The deserializer populates this as each class is
// added to its namespace so that instance-member init expressions of the form
// `OtherClass m()` can resolve the target class even before it has been
// committed into the root namespace's clmap (which happens after the full
// classes pass). The pointer is installed in an RAII scope from
// deserializeClasses() and cleared on exit.
static thread_local const std::unordered_map<std::string, QoreClass*>*
    g_aot_pending_class_map = nullptr;

static constexpr const char* AOT_BINARY_CLASS_REF_MODULE_PREFIX = "@qore-module:";
static constexpr size_t AOT_BINARY_CLASS_REF_MODULE_PREFIX_LEN = 13;

static const char* qoreAOTClassRefPath(const char* class_ref) {
    if (!class_ref) {
        return nullptr;
    }
    if (!strncmp(class_ref, AOT_BINARY_CLASS_REF_MODULE_PREFIX,
            AOT_BINARY_CLASS_REF_MODULE_PREFIX_LEN)) {
        const char* module_start = class_ref + AOT_BINARY_CLASS_REF_MODULE_PREFIX_LEN;
        const char* sep = strchr(module_start, '\n');
        if (sep) {
            return sep + 1;
        }
    }
    return class_ref;
}

static std::string qoreAOTDescribeClassRef(const char* class_ref) {
    const char* path = qoreAOTClassRefPath(class_ref);
    if (!class_ref || path == class_ref) {
        return path ? path : "(null)";
    }

    const char* module_start = class_ref + AOT_BINARY_CLASS_REF_MODULE_PREFIX_LEN;
    const char* sep = strchr(module_start, '\n');
    std::string rv(path && *path ? path : "(null)");
    rv += " [module ";
    rv.append(module_start, sep ? sep - module_start : 0);
    rv += "]";
    return rv;
}

static const QoreClass* qoreAOTFindClassInMap(
        const std::unordered_map<std::string, QoreClass*>* class_map,
        const char* class_ref) {
    if (!class_map || !class_ref || !*class_ref) {
        return nullptr;
    }

    auto lookup = [class_map](const char* path) -> const QoreClass* {
        if (!path || !*path) {
            return nullptr;
        }
        auto it = class_map->find(path);
        return it != class_map->end() ? it->second : nullptr;
    };

    if (const QoreClass* qc = lookup(class_ref)) {
        return qc;
    }

    const char* path = qoreAOTClassRefPath(class_ref);
    if (path != class_ref) {
        if (const QoreClass* qc = lookup(path)) {
            return qc;
        }
    }

    if (path && *path) {
        if (!strncmp(path, "::", 2)) {
            if (const QoreClass* qc = lookup(path + 2)) {
                return qc;
            }
        } else {
            std::string absolute_path("::");
            absolute_path += path;
            auto it = class_map->find(absolute_path);
            if (it != class_map->end()) {
                return it->second;
            }
        }
    }

    return nullptr;
}

static void qoreAOTAddClassLookupAliases(
        std::unordered_map<std::string, QoreClass*>& class_map,
        QoreClass* qc) {
    if (!qc) {
        return;
    }
    const char* cpath = qc->getPath();
    if (!cpath || !*cpath) {
        return;
    }
    class_map[cpath] = qc;
    if (!strncmp(cpath, "::", 2)) {
        class_map[std::string(cpath + 2)] = qc;
    } else {
        class_map[std::string("::") + cpath] = qc;
    }
}

static const QoreClass* qoreAOTResolveClassRefForDeserialization(
        QoreProgram* pgm,
        const char* class_ref,
        const std::unordered_map<std::string, QoreClass*>* extra_class_map = nullptr,
        bool pseudo = false) {
    if (const QoreClass* qc = qoreAOTFindClassInMap(extra_class_map, class_ref)) {
        return qc;
    }
    if (const QoreClass* qc = qoreAOTFindClassInMap(g_aot_pending_class_map, class_ref)) {
        return qc;
    }
    return qore_aot_resolve_class_ref(pgm, class_ref, pseudo);
}

void QoreAOTBinaryDeserializer::appendClassesToLookupMap(
        std::unordered_map<std::string, QoreClass*>& map) const {
    for (QoreClass* qc : class_list) {
        qoreAOTAddClassLookupAliases(map, qc);
    }
}

const QoreClass* QoreAOTBinaryDeserializer::resolveClassRefForSession(
        const char* class_ref,
        const std::unordered_map<std::string, QoreClass*>* local_class_map,
        bool pseudo) const {
    if (const QoreClass* qc = qoreAOTFindClassInMap(local_class_map, class_ref)) {
        return qc;
    }
    if (const QoreClass* qc = qoreAOTFindClassInMap(g_aot_pending_class_map, class_ref)) {
        return qc;
    }
    if (const QoreClass* qc = qoreAOTFindClassInMap(batch_class_lookup_map, class_ref)) {
        return qc;
    }
    return qore_aot_resolve_class_ref(pgm, class_ref, pseudo);
}

// Pending fixup for param defaults that reference a static method
// whose class is still pending commit at the time the variant signature is
// deserialized (e.g. `constructor(string b = MultiPartMessage::getBoundary())`
// inside MultiPartMessage itself — the static method entry is added to the
// class but not committed into the vlist yet). A post-pass after
// commitDeserializedClasses resolves the QoreMethod* and patches the default
// arg slot in place.
//
// The struct is defined in QoreAOTBinary.h as a nested type on
// QoreAOTBinaryDeserializer so its storage can persist across the
// phase-split boundaries in batch mode (between
// deserializeFunctionsAndMethods and finalize, potentially with
// other sessions' phases running in between).
using PendingStaticMethodDefault = QoreAOTBinaryDeserializer::PendingStaticMethodDefault;
static thread_local std::vector<PendingStaticMethodDefault>*
    g_aot_pending_static_method_defaults = nullptr;

// Read an instance-member / static-member default value.
//
// If the value is a VT_NEW_OBJECT whose target class is not yet registered in
// the program or in the in-progress class map, defer the class lookup: read
// the class path + arg values into `pending_class_path` + `pending_args` so
// the second pass (after all classes are committed) can resolve the class
// and construct the ScopedObjectCallNode. Otherwise, read normally and
// return the QoreValue via `default_val`.
//
// Returns true on success, false on malformed data.
template <class Pending>
static bool readDeferredMemberDefault(
        const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end,
        std::string& error,
        QoreValue& default_val,
        Pending& pim) {
    if (ptr >= end) {
        error = "unexpected end of data reading member default tag";
        return false;
    }
    const uint8_t* save = ptr;
    uint8_t tag_byte = *ptr;
    if (tag_byte == static_cast<uint8_t>(QoreAOTValueTag::VT_ENUM)) {
        // Class/static member defaults are read during deserializeClasses,
        // which runs BEFORE deserializeEnums. The enum the member references
        // doesn't exist yet — defer resolution to resolveInstanceMembers /
        // resolveStaticMembers where all enums are registered.
        ++ptr;  // consume tag
        if (ptr + 8 > end) {
            error = "unexpected end of data reading enum path";
            return false;
        }
        (void)QoreAOTBinaryReader::readU32(ptr);  // path_len (unused)
        uint32_t path_offset = QoreAOTBinaryReader::readU32(ptr);
        const char* path = reader.getString(path_offset);
        if (!path) {
            error = "invalid string offset for enum path";
            return false;
        }
        if (ptr + 8 > end) {
            error = "unexpected end of data reading enum member name";
            return false;
        }
        (void)QoreAOTBinaryReader::readU32(ptr);  // name_len (unused)
        uint32_t name_offset = QoreAOTBinaryReader::readU32(ptr);
        const char* member_name = reader.getString(name_offset);
        if (!member_name) {
            error = "invalid string offset for enum member name";
            return false;
        }
        pim.pending_enum_path = path;
        pim.pending_enum_member = member_name;
        default_val = QoreValue();
        (void)save;
        return true;
    }
    if (tag_byte == static_cast<uint8_t>(QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT)) {
        // Class/static member defaults are read during deserializeClasses,
        // which runs BEFORE deserializeHashDecls AND before all classes in
        // the same module are committed. Complex-type defaults like
        // `hash<ComponentInfo>()` (hashdecl), `hash<string, MyClass>()`
        // (complex hash referencing a class), or `list<MyClass>()` may
        // reference types that don't exist yet. Defer ALL complex defaults
        // to resolveInstanceMembers / resolveStaticMembers.
        if (ptr + 2 > end) {
            error = "unexpected end of data reading complex_default kind";
            return false;
        }
        ptr += 1;  // consume tag
        uint8_t kind = QoreAOTBinaryReader::readU8(ptr);
        QoreComplexBufferInitKind buffer_init_kind = QoreComplexBufferInitKind::Constructor;
        if (kind == 3 && (reader.getHeader().feature_flags & QORE_AOT_FEAT_COMPLEX_BUFFER_INIT_KIND) != 0) {
            if (ptr + 1 > end) {
                error = "unexpected end of data reading complex buffer init kind";
                return false;
            }
            buffer_init_kind = static_cast<QoreComplexBufferInitKind>(QoreAOTBinaryReader::readU8(ptr));
        }
        if (ptr + 8 > end) {
            error = "unexpected end of data reading complex_default type path";
            return false;
        }
        (void)QoreAOTBinaryReader::readU32(ptr);  // path_len (unused)
        uint32_t path_offset = QoreAOTBinaryReader::readU32(ptr);
        const char* type_path = reader.getString(path_offset);
        if (!type_path) {
            error = "invalid string offset for complex_default type path in deferred member default";
            return false;
        }
        if (ptr + 4 > end) {
            error = "unexpected end of data reading complex_default arg count";
            return false;
        }
        uint32_t nargs = QoreAOTBinaryReader::readU32(ptr);
        std::vector<QoreValue> args;
        args.reserve(nargs);
        for (uint32_t i = 0; i < nargs; ++i) {
            QoreValue arg = reader.readValue(ptr, end, error);
            if (!error.empty()) {
                for (auto& v : args) {
                    v.discard(nullptr);
                }
                return false;
            }
            args.push_back(arg);
        }
        pim.pending_complex_default_kind = static_cast<int8_t>(kind);
        pim.pending_complex_buffer_init_kind = static_cast<int8_t>(buffer_init_kind);
        pim.pending_complex_default_path = type_path;
        pim.pending_complex_default_args = std::move(args);
        default_val = QoreValue();
        return true;
    }
    if (tag_byte == static_cast<uint8_t>(QoreAOTValueTag::VT_EXPR_TREE)) {
        // Expression-tree defaults may reference class/namespace constants
        // from the same AOT blob.  Those constants are not registered while
        // class shells are being read, so defer materializing the AST until
        // the member-resolution pass.
        ++ptr;  // consume tag
        if (ptr + 4 > end) {
            error = "unexpected end of data reading expr_tree size";
            return false;
        }
        uint32_t blob_size = QoreAOTBinaryReader::readU32(ptr);
        if (ptr + blob_size > end) {
            error = "expr_tree blob exceeds section bounds";
            return false;
        }
        pim.pending_expr_tree_blob.assign(ptr, ptr + blob_size);
        ptr += blob_size;
        default_val = QoreValue();
        return true;
    }
    if (tag_byte == static_cast<uint8_t>(QoreAOTValueTag::VT_EXPR_NATIVE)) {
        // Native default expressions use the binary string pool for symbol
        // references, so defer materializing them until the member-resolution
        // pass where the owning reader is still available.
        ++ptr;  // consume tag
        if (ptr + 4 > end) {
            error = "unexpected end of data reading native expr size";
            return false;
        }
        uint32_t blob_size = QoreAOTBinaryReader::readU32(ptr);
        if (ptr + blob_size > end) {
            error = "native expr blob exceeds section bounds";
            return false;
        }
        pim.pending_expr_native_blob.assign(ptr, ptr + blob_size);
        ptr += blob_size;
        default_val = QoreValue();
        return true;
    }
    if (tag_byte == static_cast<uint8_t>(QoreAOTValueTag::VT_CONST_REF)
            && !reader.wrap_const_ref_in_rcr) {
        // Class/static member records are read before class constants from
        // the same CLASSES record are registered. Resolve constant refs after
        // resolveClassConstants() instead of silently converting an unresolved
        // same-class constant to NOTHING.
        ++ptr;  // consume tag
        if (ptr + 8 > end) {
            error = "unexpected end of data reading const_ref name";
            return false;
        }
        (void)QoreAOTBinaryReader::readU32(ptr);  // name_len (unused)
        uint32_t name_offset = QoreAOTBinaryReader::readU32(ptr);
        const char* fqn = reader.getString(name_offset);
        if (!fqn) {
            error = "invalid string offset for const_ref name";
            return false;
        }
        pim.pending_const_ref_path = fqn;
        default_val = QoreValue();
        return true;
    }
    if (tag_byte != static_cast<uint8_t>(QoreAOTValueTag::VT_NEW_OBJECT)) {
        // Normal case: use the standard reader
        default_val = reader.readValue(ptr, end, error);
        return error.empty();
    }
    // VT_NEW_OBJECT: try resolving the class now, deferring to a pending
    // entry on the PendingInstanceMember / PendingStaticMember if not yet
    // registered.
    ++ptr;  // consume tag
    if (ptr + 8 > end) {
        error = "unexpected end of data reading new_object class path";
        return false;
    }
    (void)QoreAOTBinaryReader::readU32(ptr);  // path_len (unused — using string pool)
    uint32_t path_offset = QoreAOTBinaryReader::readU32(ptr);
    const char* class_path = reader.getString(path_offset);
    if (!class_path) {
        error = "invalid string offset for new_object class path";
        return false;
    }
    if (ptr + 4 > end) {
        error = "unexpected end of data reading new_object arg count";
        return false;
    }
    uint32_t nargs = QoreAOTBinaryReader::readU32(ptr);

    // Read constructor arguments regardless — args don't forward-reference
    // classes in practice (they're usually simple constant values).
    std::vector<QoreValue> args;
    args.reserve(nargs);
    for (uint32_t i = 0; i < nargs; ++i) {
        QoreValue arg = reader.readValue(ptr, end, error);
        if (!error.empty()) {
            for (auto& v : args) v.discard(nullptr);
            return false;
        }
        args.push_back(arg);
    }

    // Try to resolve class in the in-progress class maps first, then via the
    // AOT class-ref resolver for committed program/module classes.
    const QoreClass* qc = qoreAOTResolveClassRefForDeserialization(getProgram(), class_path);
    if (qc) {
        // Resolvable now: construct ScopedObjectCallNode immediately.
        QoreParseListNode* parse_args = nullptr;
        if (nargs > 0) {
            parse_args = new QoreParseListNode(&loc_builtin);
            for (auto& v : args) {
                parse_args->add(v, &loc_builtin);
            }
            args.clear();
        }
        ScopedObjectCallNode* socn = new ScopedObjectCallNode(
            &loc_builtin, qc, parse_args);
        if (parse_args) {
            socn->resolveParseArgs();
        }
        default_val = QoreValue(socn);
    } else {
        // Defer: store class path + args; second pass resolves.
        pim.pending_new_class_path = class_path;
        pim.pending_new_args = std::move(args);
        default_val = QoreValue();
    }
    (void)save;  // save no longer needed — ptr correctly advanced
    return true;
}

static bool resolveDeferredConstRefDefault(std::string& path, QoreValue& default_val,
        QoreProgram* pgm, const char* kind, const char* owner_name,
        const char* member_name, std::string& error) {
    if (path.empty()) {
        return true;
    }
    if (default_val.hasNode()) {
        default_val.discard(nullptr);
        default_val = QoreValue();
    }
    bool resolved = false;
    QoreValue v = qore_aot_resolve_constant_path_value(pgm, path.c_str(),
        true, true, &resolved);
    if (!resolved) {
        error = "AOT cannot resolve deferred constant-ref default '";
        error += path;
        error += "' for ";
        error += kind ? kind : "member";
        error += " '";
        error += owner_name ? owner_name : "<unknown>";
        error += "::";
        error += member_name ? member_name : "<unknown>";
        error += "'";
        return false;
    }
    default_val = v;
    path.clear();
    return true;
}

template <class Pending>
static bool materializeDeferredMemberDefault(const QoreAOTBinaryReader& reader,
        Pending& pm, QoreValue& default_val, std::string& error,
        const char* kind, const char* owner_name, const char* member_name) {
    if (pm.value_blob.empty()) {
        return true;
    }
    const uint8_t* vptr = pm.value_blob.data();
    const uint8_t* vend = vptr + pm.value_blob.size();
    std::string value_error;
    struct ConstRefDeferGuard {
        const QoreAOTBinaryReader& r;
        bool prev;
        ConstRefDeferGuard(const QoreAOTBinaryReader& r_, bool newv)
            : r(r_), prev(r_.defer_unresolved_const_refs) {
            r_.defer_unresolved_const_refs = newv;
        }
        ~ConstRefDeferGuard() { r.defer_unresolved_const_refs = prev; }
    } defer_guard(reader, true);

    if (!readDeferredMemberDefault(reader, vptr, vend, value_error,
            default_val, pm)) {
        error = "AOT cannot deserialize default for ";
        error += kind ? kind : "member";
        error += " '";
        error += owner_name ? owner_name : "<unknown>";
        error += "::";
        error += member_name ? member_name : "<unknown>";
        error += "': ";
        error += value_error;
        return false;
    }
    if (vptr != vend) {
        error = "AOT member default did not consume serialized payload for ";
        error += kind ? kind : "member";
        error += " '";
        error += owner_name ? owner_name : "<unknown>";
        error += "::";
        error += member_name ? member_name : "<unknown>";
        error += "'";
        return false;
    }
    pm.value_blob.clear();
    return true;
}

static bool setAOTDeferredMemberResolutionError(std::string& error,
        const char* target_kind, const char* target_name, const char* owner_kind,
        const char* owner_name, const char* member_name,
        const char* details = nullptr) {
    error = "AOT cannot resolve ";
    error += target_kind ? target_kind : "deferred member default";
    error += " '";
    error += target_name ? target_name : "<unknown>";
    error += "' for ";
    error += owner_kind ? owner_kind : "member";
    error += " '";
    error += owner_name ? owner_name : "<unknown>";
    error += "::";
    error += member_name ? member_name : "<unknown>";
    error += "'";
    if (details && *details) {
        error += ": ";
        error += details;
    }
    return false;
}

static bool skipAOTValueContainerType(const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end, std::string& error) {
    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPED_VALUE_CONTAINERS) == 0) {
        return true;
    }
    if (ptr + 1 > end) {
        error = "unexpected end of data skipping typed container value kind";
        return false;
    }
    QoreAOTContainerValueType kind = static_cast<QoreAOTContainerValueType>(
        QoreAOTBinaryReader::readU8(ptr));
    if (kind == QoreAOTContainerValueType::Plain) {
        return true;
    }
    if (kind != QoreAOTContainerValueType::Complex && kind != QoreAOTContainerValueType::HashDecl) {
        error = "invalid typed container value kind while skipping value";
        return false;
    }
    if (ptr + 8 > end) {
        error = "unexpected end of data skipping typed container value type path";
        return false;
    }
    ptr += 8;
    return true;
}

static bool checkAOTSerializedValueSkipCancel(uint32_t i, std::string& error,
        const char* context) {
    if (i && !(i % 100) && qore_check_cancel(nullptr, context)) {
        error = std::string(context) + " cancelled";
        return false;
    }
    return true;
}

static bool skipAOTSerializedValue(const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end, std::string& error) {
    if (ptr >= end) {
        error = "unexpected end of data skipping value tag";
        return false;
    }
    QoreAOTValueTag tag = static_cast<QoreAOTValueTag>(
        QoreAOTBinaryReader::readU8(ptr));

    auto skip_fixed = [&](size_t n, const char* what) -> bool {
        if (static_cast<size_t>(end - ptr) < n) {
            error = "unexpected end of data skipping ";
            error += what;
            return false;
        }
        ptr += n;
        return true;
    };

    auto read_count = [&](uint32_t& count, const char* what) -> bool {
        if (ptr + 4 > end) {
            error = "unexpected end of data skipping ";
            error += what;
            return false;
        }
        count = QoreAOTBinaryReader::readU32(ptr);
        return true;
    };

    switch (tag) {
        case QoreAOTValueTag::VT_NOTHING:
        case QoreAOTValueTag::VT_NULL:
        case QoreAOTValueTag::VT_OPAQUE_DEFAULT:
            return true;

        case QoreAOTValueTag::VT_BOOL:
            return skip_fixed(1, "bool value");

        case QoreAOTValueTag::VT_INT64:
        case QoreAOTValueTag::VT_FLOAT64:
        case QoreAOTValueTag::VT_STRING:
        case QoreAOTValueTag::VT_NUMBER:
        case QoreAOTValueTag::VT_CONST_REF:
            return skip_fixed(8, "scalar value payload");

        case QoreAOTValueTag::VT_CHAR:
            return skip_fixed(4, "char value");

        case QoreAOTValueTag::VT_ABS_DATE:
            return skip_fixed(16, "absolute date value");

        case QoreAOTValueTag::VT_ABS_DATE_REGION:
            return skip_fixed(16, "absolute date region value");

        case QoreAOTValueTag::VT_REL_DATE:
            return skip_fixed(56, "relative date value");

        case QoreAOTValueTag::VT_BINARY: {
            uint32_t len = 0;
            if (!read_count(len, "binary length")) {
                return false;
            }
            return skip_fixed(len, "binary payload");
        }

        case QoreAOTValueTag::VT_PLUGIN_INSTANCE: {
            if (!skip_fixed(8, "plugin value instance header")) {
                return false;
            }
            uint32_t payload_len = 0;
            if (!read_count(payload_len, "plugin value payload length")) {
                return false;
            }
            return skip_fixed(payload_len, "plugin value payload");
        }

        case QoreAOTValueTag::VT_LIST: {
            if (!skipAOTValueContainerType(reader, ptr, end, error)) {
                return false;
            }
            uint32_t count = 0;
            if (!read_count(count, "list count")) {
                return false;
            }
            for (uint32_t i = 0; i < count; ++i) {
                if (!checkAOTSerializedValueSkipCancel(i, error,
                        "AOT serialized list value skip")) {
                    return false;
                }
                if (!skipAOTSerializedValue(reader, ptr, end, error)) {
                    error += " in list element ";
                    error += std::to_string(i);
                    return false;
                }
            }
            return true;
        }

        case QoreAOTValueTag::VT_HASH: {
            if (!skipAOTValueContainerType(reader, ptr, end, error)) {
                return false;
            }
            uint32_t count = 0;
            if (!read_count(count, "hash count")) {
                return false;
            }
            for (uint32_t i = 0; i < count; ++i) {
                if (!checkAOTSerializedValueSkipCancel(i, error,
                        "AOT serialized hash value skip")) {
                    return false;
                }
                if (!skip_fixed(4, "hash key")) {
                    return false;
                }
                if (!skipAOTSerializedValue(reader, ptr, end, error)) {
                    error += " in hash value ";
                    error += std::to_string(i);
                    return false;
                }
            }
            return true;
        }

        case QoreAOTValueTag::VT_ENUM:
            return skip_fixed(16, "enum value");

        case QoreAOTValueTag::VT_NEW_OBJECT: {
            if (!skip_fixed(8, "new_object class path")) {
                return false;
            }
            uint32_t nargs = 0;
            if (!read_count(nargs, "new_object arg count")) {
                return false;
            }
            for (uint32_t i = 0; i < nargs; ++i) {
                if (!checkAOTSerializedValueSkipCancel(i, error,
                        "AOT serialized new_object arg skip")) {
                    return false;
                }
                if (!skipAOTSerializedValue(reader, ptr, end, error)) {
                    error += " in new_object arg ";
                    error += std::to_string(i);
                    return false;
                }
            }
            return true;
        }

        case QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT: {
            if (ptr >= end) {
                error = "unexpected end of data skipping complex default kind";
                return false;
            }
            uint8_t kind = QoreAOTBinaryReader::readU8(ptr);
            if (kind == 3
                    && (reader.getHeader().feature_flags & QORE_AOT_FEAT_COMPLEX_BUFFER_INIT_KIND) != 0
                    && !skip_fixed(1, "complex buffer init kind")) {
                return false;
            }
            if (!skip_fixed(8, "complex default type path")) {
                return false;
            }
            uint32_t nargs = 0;
            if (!read_count(nargs, "complex default arg count")) {
                return false;
            }
            for (uint32_t i = 0; i < nargs; ++i) {
                if (!checkAOTSerializedValueSkipCancel(i, error,
                        "AOT serialized complex default arg skip")) {
                    return false;
                }
                if (!skipAOTSerializedValue(reader, ptr, end, error)) {
                    error += " in complex default arg ";
                    error += std::to_string(i);
                    return false;
                }
            }
            return true;
        }

        case QoreAOTValueTag::VT_EXPR_TREE:
        case QoreAOTValueTag::VT_EXPR_NATIVE: {
            uint32_t blob_size = 0;
            if (!read_count(blob_size, "expression blob size")) {
                return false;
            }
            return skip_fixed(blob_size, "expression blob");
        }

        default:
            error = "unknown value tag while skipping class constant value: "
                + std::to_string(static_cast<int>(tag));
            return false;
    }
}

static bool readDeferredClassConstantValue(const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end, std::string& error,
        std::vector<uint8_t>& value_blob) {
    const uint8_t* start = ptr;
    if (!skipAOTSerializedValue(reader, ptr, end, error)) {
        return false;
    }
    value_blob.assign(start, ptr);
    return true;
}

static void resolveDeferredExprTreeDefault(std::vector<uint8_t>& blob,
        QoreValue& default_val, QoreProgram* pgm, const char* kind,
        const char* owner_name, const char* member_name) {
    if (blob.empty()) {
        return;
    }
    if (default_val.hasNode()) {
        default_val.discard(nullptr);
        default_val = QoreValue();
    }
    QoreValue v = deserializeExprTreeFromBlob(blob.data(),
        static_cast<uint32_t>(blob.size()), pgm, nullptr, 0);
    default_val = v;
    blob.clear();
    (void)kind;
    (void)owner_name;
    (void)member_name;
}

static bool resolveDeferredNativeExprDefault(const QoreAOTBinaryReader& reader,
        std::vector<uint8_t>& blob, QoreValue& default_val, QoreProgram* pgm,
        const char* kind, const char* owner_name, const char* member_name,
        std::string& error, bool wrap_const_ref_in_rcr = false,
        LocalVar** locals = nullptr, int num_locals = 0) {
    if (blob.empty()) {
        return true;
    }
    if (default_val.hasNode()) {
        default_val.discard(nullptr);
        default_val = QoreValue();
    }

    bool old_wrap = reader.wrap_const_ref_in_rcr;
    reader.wrap_const_ref_in_rcr = wrap_const_ref_in_rcr;
    const uint8_t* p = blob.data();
    const uint8_t* end = p + blob.size();
    std::string expr_error;
    QoreValue v = readOneExpr(reader, p, end, expr_error, pgm, locals, num_locals, nullptr, 0);
    reader.wrap_const_ref_in_rcr = old_wrap;

    if (!expr_error.empty() || p != end) {
        error = "AOT cannot deserialize native default expression for ";
        error += kind ? kind : "member";
        error += " '";
        error += owner_name ? owner_name : "<unknown>";
        error += "::";
        error += member_name ? member_name : "<unknown>";
        error += "'";
        if (!expr_error.empty()) {
            error += ": ";
            error += expr_error;
        }
        if (p != end) {
            error += expr_error.empty() ? ": " : "; ";
            error += "trailing payload bytes";
        }
        printd(0, "AOT deser: %s\n", error.c_str());
        v.discard(nullptr);
        blob.clear();
        return false;
    }
    default_val = v;
    blob.clear();
    return true;
}

static constexpr const char* AOT_CONST_PATH_PREFIX = "@qore-aot-const-path:";

static bool aot_is_encoded_constant_path(const char* path) {
    return path && !strncmp(path, AOT_CONST_PATH_PREFIX, strlen(AOT_CONST_PATH_PREFIX));
}

static bool aot_constant_reverse_path_is_better(const std::string& current,
        const std::string& candidate) {
    bool current_encoded = aot_is_encoded_constant_path(current.c_str());
    bool candidate_encoded = aot_is_encoded_constant_path(candidate.c_str());

    if (current_encoded != candidate_encoded) {
        return !candidate_encoded;
    }
    if (candidate.size() != current.size()) {
        return candidate.size() < current.size();
    }
    return candidate < current;
}

static std::string aot_encode_constant_path_root(const std::string& base) {
    return std::string(AOT_CONST_PATH_PREFIX) + std::to_string(base.size()) + ":" + base;
}

static std::string aot_append_constant_hash_key_path(const std::string& base, const char* key) {
    std::string rv = aot_is_encoded_constant_path(base.c_str()) ? base : aot_encode_constant_path_root(base);
    size_t key_len = key ? strlen(key) : 0;
    rv += "H";
    rv += std::to_string(key_len);
    rv += ":";
    if (key_len) {
        rv.append(key, key_len);
    }
    return rv;
}

static std::string aot_append_constant_list_index_path(const std::string& base, size_t index) {
    std::string rv = aot_is_encoded_constant_path(base.c_str()) ? base : aot_encode_constant_path_root(base);
    rv += "L";
    rv += std::to_string(index);
    rv += ":";
    return rv;
}

static bool aot_parse_size_component(const std::string& path, size_t& pos, size_t& value) {
    if (pos >= path.size() || path[pos] < '0' || path[pos] > '9') {
        return false;
    }
    size_t rv = 0;
    while (pos < path.size() && path[pos] >= '0' && path[pos] <= '9') {
        rv = (rv * 10) + static_cast<size_t>(path[pos] - '0');
        ++pos;
    }
    if (pos >= path.size() || path[pos] != ':') {
        return false;
    }
    ++pos;
    value = rv;
    return true;
}

static QoreValue aot_resolve_constant_path_tail(QoreValue cur, const std::string& encoded, size_t pos,
        bool* resolved = nullptr) {
    if (resolved) {
        *resolved = false;
    }
    while (pos < encoded.size()) {
        char seg = encoded[pos++];
        size_t len_or_index = 0;
        if (!aot_parse_size_component(encoded, pos, len_or_index)) {
            cur.discard(nullptr);
            return QoreValue();
        }

        QoreValue next;
        if (seg == 'H') {
            if (pos + len_or_index > encoded.size() || cur.getType() != NT_HASH) {
                cur.discard(nullptr);
                return QoreValue();
            }
            std::string key = encoded.substr(pos, len_or_index);
            pos += len_or_index;
            const QoreHashNode* h = cur.get<const QoreHashNode>();
            bool exists = false;
            next = h ? h->getKeyValueExistence(key.c_str(), exists) : QoreValue();
            if (!exists) {
                cur.discard(nullptr);
                return QoreValue();
            }
            next = next.refSelf();
        } else if (seg == 'L') {
            if (cur.getType() != NT_LIST) {
                cur.discard(nullptr);
                return QoreValue();
            }
            const QoreListNode* l = cur.get<const QoreListNode>();
            if (!l || len_or_index >= l->size()) {
                cur.discard(nullptr);
                return QoreValue();
            }
            QoreValue lv = l->retrieveEntry(len_or_index);
            next = lv.refSelf();
        } else {
            cur.discard(nullptr);
            return QoreValue();
        }

        cur.discard(nullptr);
        cur = next;
    }

    if (resolved) {
        *resolved = true;
    }
    return cur;
}

class RuntimeConstantPathRefNode : public RuntimeConstantRefNode {
private:
    std::string encoded_path;
    size_t tail_pos;

    DLLLOCAL QoreValue resolvePath(ExceptionSink* xsink) const {
        ConstantEntry* ce = getConstantEntry();
        if (!ce->hasValue()) {
            if (xsink) {
                xsink->raiseException("AOT-PENDING-CONSTANT",
                    "cannot evaluate AOT-deserialized constant path '%s' before base constant '%s' "
                    "__const_init function has populated the value",
                    encoded_path.c_str(), ce->getName());
            }
            return QoreValue();
        }

        QoreValue base = ce->getReferencedValue();
        bool resolved = false;
        QoreValue rv = aot_resolve_constant_path_tail(base, encoded_path, tail_pos, &resolved);
        if (!resolved && xsink) {
            xsink->raiseException("AOT-CONSTANT-PATH-ERROR",
                "cannot resolve AOT-deserialized constant path '%s' from base constant '%s'",
                encoded_path.c_str(), ce->getName());
        }
        return rv;
    }

protected:
    DLLLOCAL QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        needs_deref = true;
        return resolvePath(xsink);
    }

    DLLLOCAL int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) override {
        parse_context.typeInfo = autoTypeInfo;
        return 0;
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo() const override {
        return autoTypeInfo;
    }

public:
    DLLLOCAL RuntimeConstantPathRefNode(const QoreProgramLocation* loc, ConstantEntry* ce,
            std::string n_encoded_path, size_t n_tail_pos)
            : RuntimeConstantRefNode(loc, ce, true), encoded_path(std::move(n_encoded_path)), tail_pos(n_tail_pos) {
    }

    DLLLOCAL int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const override {
        QoreValue rv = resolvePath(xsink);
        if (xsink && *xsink) {
            return -1;
        }
        int rc = rv.getAsString(str, foff, xsink);
        rv.discard(xsink);
        return rc;
    }

    DLLLOCAL QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const override {
        QoreValue rv = resolvePath(xsink);
        if (xsink && *xsink) {
            del = false;
            return nullptr;
        }
        QoreString* str = rv.getAsString(del, foff, xsink);
        rv.discard(xsink);
        return str;
    }

    DLLLOCAL const char* getTypeName() const override {
        return "AOT constant path reference";
    }
};

static void aot_add_constant_value_reverse_mappings_impl(AOTConstantReverseMap& crm,
        const QoreValue& v, const std::string& path,
        std::unordered_set<const AbstractQoreNode*>& seen, bool root_value) {
    if (!v.hasNode()) {
        return;
    }

    const AbstractQoreNode* node = v.getInternalNode();
    if (!node || !seen.insert(node).second) {
        return;
    }

    auto it = crm.find(node);
    if (it == crm.end() || aot_constant_reverse_path_is_better(it->second, path)) {
        crm[node] = path;
        if (getenv("QORE_AOT_DEBUG_CONST_MAP")) {
            if (auto* obj = dynamic_cast<const QoreObject*>(node)) {
                fprintf(stderr, "AOT const map object: class=%s ptr=%p path=%s\n",
                    obj->getClassName(), static_cast<const void*>(node), path.c_str());
            }
        }
    }

    if (v.getType() == NT_HASH) {
        const QoreHashNode* h = v.get<const QoreHashNode>();
        if (!h) {
            return;
        }
        ConstHashIterator hi(*h);
        while (hi.next()) {
            QoreValue hv = hi.get();
            if (hv.hasNode()) {
                aot_add_constant_value_reverse_mappings_impl(crm, hv,
                    aot_append_constant_hash_key_path(path, hi.getKey()), seen, false);
            }
        }
        return;
    }

    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (!l) {
            return;
        }
        for (size_t i = 0, e = l->size(); i < e; ++i) {
            QoreValue lv = l->retrieveEntry(i);
            if (lv.hasNode()) {
                aot_add_constant_value_reverse_mappings_impl(crm, lv,
                    aot_append_constant_list_index_path(path, i), seen, false);
            }
        }
    }
}

static const std::string* aotFindConstantReverseMapPath(
        const AOTConstantReverseMap* crm, const AbstractQoreNode* node) {
    if (!crm || !node) {
        return nullptr;
    }
    auto it = crm->find(node);
    return it == crm->end() ? nullptr : &it->second;
}

void qore_aot_add_constant_value_reverse_mappings(AOTConstantReverseMap& crm,
        const QoreValue& v, const std::string& path) {
    std::unordered_set<const AbstractQoreNode*> seen;
    aot_add_constant_value_reverse_mappings_impl(crm, v, path, seen, true);
}

static bool qore_aot_resolve_runtime_constant_path_impl(const RuntimeConstantRefNode* node,
        const AOTConstantReverseMap* const_reverse_map, std::string& path,
        std::unordered_set<const AbstractQoreNode*>& seen_nodes,
        std::unordered_set<const ConstantEntry*>& seen_entries) {
    if (!node) {
        return false;
    }

    const AbstractQoreNode* abstract_node = node;
    if (!seen_nodes.insert(abstract_node).second) {
        return false;
    }

    if (const_reverse_map) {
        if (const std::string* mapped_path = aotFindConstantReverseMapPath(const_reverse_map, abstract_node)) {
            path = *mapped_path;
            return true;
        }
    }

    ConstantEntry* ce = node->getConstantEntry();
    if (!ce) {
        return false;
    }

    if (!seen_entries.insert(ce).second) {
        return false;
    }

    auto resolve_value_node = [&](const QoreValue& v) -> bool {
        if (!v.hasNode()) {
            return false;
        }

        const AbstractQoreNode* value_node = v.getInternalNode();
        if (auto* nested_rcr = dynamic_cast<const RuntimeConstantRefNode*>(value_node)) {
            // Follow alias chains before accepting a root mapping for the stored
            // node; otherwise `const A = B; const B = C;` can resolve only one
            // level deep.
            if (qore_aot_resolve_runtime_constant_path_impl(nested_rcr, const_reverse_map, path,
                    seen_nodes, seen_entries)) {
                return true;
            }
        }

        if (const_reverse_map) {
            if (const std::string* mapped_path = aotFindConstantReverseMapPath(const_reverse_map, value_node)) {
                path = *mapped_path;
                return true;
            }
        }

        return false;
    };

    if (resolve_value_node(ce->val)) {
        return true;
    }

    QoreValue referenced_value = ce->getReferencedValue();
    bool resolved = resolve_value_node(referenced_value);
    referenced_value.discard(nullptr);
    if (resolved) {
        return true;
    }

    if (!const_reverse_map) {
        path = ce->getName();
        return !path.empty();
    }

    return false;
}

bool qore_aot_resolve_runtime_constant_path(const RuntimeConstantRefNode* node,
        const AOTConstantReverseMap* const_reverse_map, std::string& path) {
    path.clear();
    std::unordered_set<const AbstractQoreNode*> seen_nodes;
    std::unordered_set<const ConstantEntry*> seen_entries;
    return qore_aot_resolve_runtime_constant_path_impl(node, const_reverse_map, path,
        seen_nodes, seen_entries);
}

static void aot_add_constant_root_reverse_mapping(AOTConstantReverseMap& crm,
        const QoreValue& v, const std::string& path) {
    if (!v.hasNode()) {
        return;
    }
    const AbstractQoreNode* node = v.getInternalNode();
    if (node && crm.find(node) == crm.end()) {
        crm.emplace(node, path);
    }
}

static bool aot_constant_path_belongs_to_fqns(const std::string& path,
        const std::unordered_set<std::string>& excluded_fqns,
        const std::string& excluded_direct_fqn) {
    if (!aot_is_encoded_constant_path(path.c_str())) {
        return !excluded_direct_fqn.empty() && path == excluded_direct_fqn;
    }

    size_t pos = strlen(AOT_CONST_PATH_PREFIX);
    size_t base_len = 0;
    if (!aot_parse_size_component(path, pos, base_len) || pos + base_len > path.size()) {
        return false;
    }
    return excluded_fqns.find(path.substr(pos, base_len)) != excluded_fqns.end();
}

static bool aot_constant_path_belongs_to_fqn(const std::string& path,
        const std::string& fqn) {
    if (fqn.empty()) {
        return false;
    }
    if (!aot_is_encoded_constant_path(path.c_str())) {
        return path == fqn;
    }

    size_t pos = strlen(AOT_CONST_PATH_PREFIX);
    size_t base_len = 0;
    if (!aot_parse_size_component(path, pos, base_len) || pos + base_len > path.size()) {
        return false;
    }
    return path.compare(pos, base_len, fqn) == 0;
}

static bool aot_get_constant_path_base(const std::string& path, std::string& base) {
    if (!aot_is_encoded_constant_path(path.c_str())) {
        base = path;
        return true;
    }

    size_t pos = strlen(AOT_CONST_PATH_PREFIX);
    size_t base_len = 0;
    if (!aot_parse_size_component(path, pos, base_len) || pos + base_len > path.size()) {
        return false;
    }
    base.assign(path, pos, base_len);
    return true;
}

static bool aot_constant_path_available_for_writer(const QoreAOTBinaryWriter& writer,
        const std::string& path) {
    if (!writer.current_blob_const_fqns) {
        return true;
    }

    std::string base;
    if (!aot_get_constant_path_base(path, base)) {
        return false;
    }

    return writer.available_const_ref_fqns
        && writer.available_const_ref_fqns->find(base) != writer.available_const_ref_fqns->end();
}

static AOTConstantReverseMap aot_filter_constant_reverse_map(
        const AOTConstantReverseMap& crm, const std::vector<std::string>& excluded_fqns,
        const std::string& excluded_direct_fqn) {
    std::unordered_set<std::string> excluded_set(excluded_fqns.begin(), excluded_fqns.end());
    AOTConstantReverseMap rv;
    rv.reserve(crm.size());
    for (const auto& it : crm) {
        if (!aot_constant_path_belongs_to_fqns(it.second, excluded_set, excluded_direct_fqn)) {
            rv.emplace(it.first, it.second);
        }
    }
    return rv;
}

// Resolve a constant FQN (e.g. "Reflection::AutoHashType" or
// "Some::Class::MEMBER") to its ConstantEntry via the given program, falling
// back to a class-constant lookup when the name isn't a namespace constant.
// Returns nullptr on failure. Used by AOT load paths that rebuild AST nodes
// referencing program/class constants.
static ConstantEntry* aot_resolve_constant_by_fqn(QoreProgram* pgm, const char* fqn) {
    if (!pgm || !fqn || !*fqn) {
        return nullptr;
    }
    qore_program_private* pp = qore_program_private::get(*pgm);
    const qore_ns_private* cns = nullptr;
    ConstantEntry* ce = const_cast<ConstantEntry*>(
        qore_root_ns_private::runtimeFindNamespaceConstant(*pp->RootNS, fqn, cns));
    if (ce) {
        return ce;
    }
    // Try class constant lookup: path format "ClassName::ConstName"
    std::string path(fqn);
    size_t sep = path.rfind("::");
    if (sep == std::string::npos || sep == 0) {
        return nullptr;
    }
    std::string class_path = path.substr(0, sep);
    std::string const_name = path.substr(sep + 2);
    const qore_ns_private* found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
        *pp->RootNS, class_path.c_str(), found_ns);
    if (!qc) {
        return nullptr;
    }
    return const_cast<ConstantEntry*>(
        qore_class_private::get(*qc)->constlist.findEntry(const_name.c_str()));
}

QoreValue qore_aot_resolve_constant_path_value(QoreProgram* pgm, const char* path,
        bool defer_if_pending, bool wrap_top_level_if_ready, bool* resolved) {
    if (resolved) {
        *resolved = false;
    }
    if (!path || !*path) {
        return QoreValue();
    }
    const char* const_trace = getenv("QORE_AOT_CONST_TRACE");
    bool trace_const = const_trace && (!*const_trace || strstr(path, const_trace));
    if (trace_const) {
        fprintf(stderr, "[aot-init] resolve constant path pgm=%p path=%s defer=%d wrap=%d\n",
            (void*)pgm, path, (int)defer_if_pending, (int)wrap_top_level_if_ready);
    }

    if (!aot_is_encoded_constant_path(path)) {
        ConstantEntry* ce = aot_resolve_constant_by_fqn(pgm, path);
        if (trace_const) {
            fprintf(stderr, "[aot-init] resolve constant simple path=%s ce=%p has=%d pending=%d\n",
                path, (void*)ce, ce ? (int)ce->hasValue() : -1,
                ce ? (int)ce->aot_shell_pending : -1);
        }
        if (!ce) {
            return QoreValue();
        }
        if (resolved) {
            *resolved = true;
        }
        if (wrap_top_level_if_ready && ce->hasValue()) {
            return QoreValue(new RuntimeConstantRefNode(&loc_builtin, ce));
        }
        if (ce->hasValue()) {
            return ce->getReferencedValue();
        }
        if (defer_if_pending) {
            return QoreValue(new RuntimeConstantRefNode(&loc_builtin, ce, true));
        }
        return ce->getReferencedValue();
    }

    std::string encoded(path);
    size_t pos = strlen(AOT_CONST_PATH_PREFIX);
    size_t base_len = 0;
    if (!aot_parse_size_component(encoded, pos, base_len) || pos + base_len > encoded.size()) {
        return QoreValue();
    }
    std::string base = encoded.substr(pos, base_len);
    pos += base_len;

    ConstantEntry* ce = aot_resolve_constant_by_fqn(pgm, base.c_str());
    if (trace_const) {
        fprintf(stderr, "[aot-init] resolve constant encoded path=%s base=%s ce=%p has=%d pending=%d\n",
            path, base.c_str(), (void*)ce, ce ? (int)ce->hasValue() : -1,
            ce ? (int)ce->aot_shell_pending : -1);
    }
    if (!ce) {
        return QoreValue();
    }
    if (!ce->hasValue()) {
        if (defer_if_pending) {
            if (resolved) {
                *resolved = true;
            }
            if (pos == encoded.size()) {
                return QoreValue(new RuntimeConstantRefNode(&loc_builtin, ce, true));
            }
            return QoreValue(new RuntimeConstantPathRefNode(&loc_builtin, ce, encoded, pos));
        }
        return QoreValue();
    }

    QoreValue cur = ce->getReferencedValue();
    bool tail_resolved = false;
    QoreValue rv = aot_resolve_constant_path_tail(cur, encoded, pos, &tail_resolved);
    if (resolved) {
        *resolved = tail_resolved;
    }
    if (trace_const) {
        fprintf(stderr, "[aot-init] resolve constant result path=%s type=%s has_node=%d\n",
            path, rv.getTypeName(), (int)rv.hasNode());
        if (rv.getType() == NT_HASH) {
            const QoreHashNode* h = rv.get<const QoreHashNode>();
            bool exists = false;
            QoreValue rt = h ? h->getKeyValueExistence("return_type", exists) : QoreValue();
            fprintf(stderr, "[aot-init] resolve constant result return_type exists=%d type=%s has_node=%d\n",
                (int)exists, rt.getTypeName(), (int)rt.hasNode());
        }
    }
    return rv;
}

static std::string qoreAOTExceptionText(ExceptionSink& xsink) {
    QoreValue e = xsink.getExceptionErr();
    QoreValue d = xsink.getExceptionDesc();
    QoreStringValueHelper err(e);
    QoreStringValueHelper desc(d);
    const char* es = e.getType() == NT_STRING ? err->c_str() : "(?err)";
    const char* ds = d.getType() == NT_STRING ? desc->c_str() : "(?desc)";
    std::string rv(es);
    rv += ": ";
    rv += ds;
    xsink.clear();
    return rv;
}

static bool aotSerializableTypePathIsOrNothing(const QoreTypeInfo* ti) {
    return ti && ti->return_vec.size() == 2
        && ti->return_vec[1].spec.match(NT_NOTHING) == QTI_IDENT;
}

static bool aotSerializableTypePathStartsWith(const char* s, const char* prefix) {
    return s && !strncmp(s, prefix, strlen(prefix));
}

static std::string getAOTSerializableTypePath(const QoreTypeInfo* ti, bool no_narrow = false) {
    if (!ti) {
        return {};
    }

    if (const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(ti)) {
        const QoreClass* owner_class = tpi->getOwnerClass();
        const TypedHashDecl* owner_hashdecl = tpi->getOwnerHashDecl();
        std::string rv = tpi->isOrNothing() ? "*" : "";
        rv += owner_hashdecl ? "hashdecl_typeparam<" : "typeparam<";
        if (owner_hashdecl) {
            rv += owner_hashdecl->getNamespacePath();
        } else if (owner_class) {
            rv += owner_class->getPath();
        }
        rv += ", ";
        rv += std::to_string(tpi->getIndex());
        rv += ", ";
        rv += tpi->getParameterName();
        rv += ">";
        return rv;
    }

    if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(ti)) {
        bool or_nothing = aotSerializableTypePathIsOrNothing(ti);
        std::string rv = or_nothing ? "*object<" : "object<";
        rv += pti->getBaseClass() ? qore_aot_encode_class_ref(pti->getBaseClass()) : "";
        rv += "<";
        const std::vector<const QoreTypeInfo*>& args = pti->getTypeArgs();
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) {
                rv += ", ";
            }
            rv += getAOTSerializableTypePath(args[i]);
        }
        rv += ">>";
        return rv;
    }

    if (const QoreComplexCodeTypeInfo* cti = QoreTypeInfo::getComplexCodeType(ti)) {
        std::string rv = cti->isOrNothing() ? "*code<" : "code<";
        rv += cti->getReturnType() ? getAOTSerializableTypePath(cti->getReturnType()) : "nothing";
        rv += "(";
        const type_vec_t& params = cti->getParamTypes();
        for (size_t i = 0; i < params.size(); ++i) {
            if (i) {
                rv += ", ";
            }
            rv += getAOTSerializableTypePath(params[i]);
        }
        if (cti->hasVarArgs()) {
            if (!params.empty()) {
                rv += ", ";
            }
            rv += "...";
        }
        rv += ")>";
        return rv;
    }

    if (no_narrow) {
        if (ti == autoTypeInfo) {
            return "auto!";
        }
        if (ti == autoHashTypeInfo) {
            return "hash<auto!>";
        }
        if (ti == autoHashOrNothingTypeInfo) {
            return "*hash<auto!>";
        }
        if (ti == autoListTypeInfo) {
            return "list<auto!>";
        }
        if (ti == autoListOrNothingTypeInfo) {
            return "*list<auto!>";
        }
    }
    if (ti == autoNoNarrowTypeInfo) {
        return "auto!";
    }
    if (ti == autoNoNarrowHashTypeInfo) {
        return "hash<auto!>";
    }
    if (ti == autoNoNarrowHashOrNothingTypeInfo) {
        return "*hash<auto!>";
    }
    if (ti == autoNoNarrowListTypeInfo) {
        return "list<auto!>";
    }
    if (ti == autoNoNarrowListOrNothingTypeInfo) {
        return "*list<auto!>";
    }

    if (const TypedHashDecl* hd = QoreTypeInfo::getTypedHash(ti)) {
        bool or_nothing = aotSerializableTypePathIsOrNothing(ti);
        const typed_hash_decl_private* hp = typed_hash_decl_private::get(*hd);
        std::string rv = or_nothing ? "*hash<" : "hash<";
        if (hp->isParameterizedHashDecl()) {
            const TypedHashDecl* base = hp->getParameterizedBase();
            rv += base ? base->getNamespacePath() : hd->getNamespacePath();
            rv += "<";
            const std::vector<const QoreTypeInfo*>& args = hp->getTypeArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT hashdecl type path serialization")) {
                    return {};
                }
                if (i) {
                    rv += ", ";
                }
                rv += getAOTSerializableTypePath(args[i]);
            }
            rv += ">";
        } else {
            rv += hd->getNamespacePath();
        }
        rv += ">";
        return rv;
    }

    const char* raw_path = QoreTypeInfo::getPath(ti);
    bool or_nothing = aotSerializableTypePathIsOrNothing(ti);

    const QoreClass* qc = QoreTypeInfo::returnsSingle(ti)
        ? QoreTypeInfo::getUniqueReturnClass(ti)
        : (or_nothing ? QoreTypeInfo::getReturnClass(ti) : nullptr);
    if (qc) {
        std::string class_path = qore_aot_encode_class_ref(qc);
        if (!class_path.empty()) {
            return std::string(or_nothing ? "*object<" : "object<") + class_path + ">";
        }
    }

    if (QoreTypeInfo::isReference(ti) && raw_path && strchr(raw_path, '<')) {
        const QoreTypeInfo* ref_ti = QoreTypeInfo::getReferenceTarget(ti);
        if (ref_ti) {
            return std::string(or_nothing ? "*reference<" : "reference<")
                + getAOTSerializableTypePath(ref_ti) + ">";
        }
    }

    const QoreTypeInfo* hash_ti = QoreTypeInfo::getReturnComplexHashOrNothing(ti);
    if (hash_ti) {
        const char* prefix = or_nothing ? "*hash<" : "hash<";
        if (hash_ti == autoTypeInfo) {
            return std::string(prefix) + "auto>";
        }
        if (hash_ti == autoNoNarrowTypeInfo) {
            return std::string(prefix) + "auto!>";
        }
        return std::string(prefix) + "string, " + getAOTSerializableTypePath(hash_ti) + ">";
    }

    const QoreTypeInfo* list_ti = QoreTypeInfo::getReturnComplexListOrNothing(ti);
    if (list_ti) {
        bool softlist = aotSerializableTypePathStartsWith(raw_path, "softlist<")
            || aotSerializableTypePathStartsWith(raw_path, "*softlist<");
        const char* base = softlist ? "softlist" : "list";
        return std::string(or_nothing ? "*" : "") + base + "<"
            + getAOTSerializableTypePath(list_ti) + ">";
    }

    const QoreTypeInfo* buffer_ti = QoreTypeInfo::getReturnComplexBufferOrNothing(ti);
    if (buffer_ti) {
        const QoreComplexBufferTypeInfo* bti = QoreTypeInfo::getComplexBufferType(buffer_ti);
        if (bti) {
            return std::string(or_nothing ? "*" : "") + "buffer<"
                + (bti->hasNullableElements() ? "*" : "")
                + qore_buffer_element_type_name(bti->getBufferElementType()) + ">";
        }
    }

    return raw_path ? raw_path : "";
}

std::string qore_get_aot_serializable_type_path(const QoreTypeInfo* ti, bool no_narrow) {
    return getAOTSerializableTypePath(ti, no_narrow);
}

static std::string qore_aot_trim_signature_param(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static bool qore_aot_split_type_signature(const char* sig, std::vector<std::string>& out, std::string& error) {
    if (!sig || strlen(sig) < 2 || sig[0] != '(' || sig[strlen(sig) - 1] != ')') {
        error = "invalid static-call argument type signature";
        return false;
    }

    size_t len = strlen(sig);
    if (len == 2) {
        return true;
    }

    std::string cur;
    int angle_depth = 0;
    int paren_depth = 0;
    for (size_t i = 1; i + 1 < len; ++i) {
        char c = sig[i];
        if (c == '<' && paren_depth == 0) {
            ++angle_depth;
        } else if (c == '>' && paren_depth == 0 && angle_depth > 0) {
            --angle_depth;
        } else if (c == '(') {
            ++paren_depth;
        } else if (c == ')' && paren_depth > 0) {
            --paren_depth;
        }

        if (c == ',' && angle_depth == 0 && paren_depth == 0) {
            out.push_back(qore_aot_trim_signature_param(cur));
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }

    out.push_back(qore_aot_trim_signature_param(cur));
    return true;
}

QoreAOTStaticMethodRef::QoreAOTStaticMethodRef(const char* encoded) : method_name(encoded) {
    if (!encoded) {
        return;
    }

    const char* first_sep = strchr(encoded, '\n');
    if (!first_sep) {
        return;
    }

    method_name_storage.assign(encoded, first_sep - encoded);
    method_name = method_name_storage.c_str();

    const char* payload = first_sep + 1;
    size_t marker_len = strlen(QORE_AOT_STATIC_CALL_ARG_TYPES_MARKER);
    if (!strncmp(payload, QORE_AOT_STATIC_CALL_ARG_TYPES_MARKER, marker_len)
            && payload[marker_len] == '\n') {
        arg_type_sig = payload + marker_len + 1;
        return;
    }

    const char* second_sep = strrchr(payload, '\n');
    if (!second_sep) {
        sig_text = payload;
        return;
    }

    variant_class_storage.assign(payload, second_sep - payload);
    if (!variant_class_storage.empty()) {
        variant_class_path = variant_class_storage.c_str();
    }
    sig_text = second_sep + 1;
}

static std::string qore_aot_encode_call_arg_type_signature(const type_vec_t& arg_types) {
    std::string rv("(");
    for (size_t i = 0, e = arg_types.size(); i < e; ++i) {
        if (i) {
            rv += ",";
        }
        std::string type_path = qore_get_aot_serializable_type_path(arg_types[i]);
        rv += type_path.empty() ? "auto" : type_path;
    }
    rv += ")";
    return rv;
}

std::string qore_aot_encode_static_method_ref(const char* method_name,
        const AbstractQoreFunctionVariant* variant, const type_vec_t* arg_types) {
    std::string rv(method_name ? method_name : "");
    if (variant) {
        if (AbstractFunctionSignature* sig = const_cast<AbstractQoreFunctionVariant*>(variant)->getSignature()) {
            rv += "\n";
            const QoreClass* variant_class = variant->getClass();
            if (variant_class) {
                rv += qore_aot_encode_class_ref(variant_class);
            }
            rv += "\n";
            rv += sig->getSignatureText();
        }
        return rv;
    }

    if (arg_types && !arg_types->empty()) {
        rv += "\n";
        rv += QORE_AOT_STATIC_CALL_ARG_TYPES_MARKER;
        rv += "\n";
        rv += qore_aot_encode_call_arg_type_signature(*arg_types);
    }
    return rv;
}

static bool qore_aot_type_signature_has_weak_arg_type(const type_vec_t& arg_types) {
    for (const QoreTypeInfo* ti : arg_types) {
        if (!QoreTypeInfo::hasType(ti) || ti == autoTypeInfo
                || QoreTypeInfo::parseReturns(ti, NT_NOTHING) != QTI_NOT_EQUAL) {
            return true;
        }
    }
    return false;
}

const AbstractQoreFunctionVariant* qore_aot_resolve_variant_from_arg_type_signature(QoreProgram* pgm,
        QoreFunction* func, const char* arg_type_sig, const qore_class_private* class_ctx,
        const QoreTypeInfo* receiver_type_info, QoreTypeParamInstantiation* type_param_instantiation,
        std::string& error) {
    if (!func || !arg_type_sig || !*arg_type_sig) {
        return nullptr;
    }

    std::vector<std::string> type_paths;
    if (!qore_aot_split_type_signature(arg_type_sig, type_paths, error)) {
        return nullptr;
    }

    QoreAOTTypeResolver resolver(pgm);
    type_vec_t arg_types;
    arg_types.reserve(type_paths.size());
    for (const std::string& type_path : type_paths) {
        std::string type_error;
        const QoreTypeInfo* ti = resolver.resolve(type_path.empty() ? "auto" : type_path.c_str(), type_error);
        if (!ti || !type_error.empty()) {
            error = "cannot resolve static-call argument type '";
            error += type_path;
            error += "'";
            if (!type_error.empty()) {
                error += ": ";
                error += type_error;
            }
            return nullptr;
        }
        arg_types.push_back(ti);
    }

    if (qore_aot_type_signature_has_weak_arg_type(arg_types)) {
        error = "static-call argument type signature contains weak or nullable types; runtime dispatch is required";
        return nullptr;
    }

    // Only argument types — not a resolved variant — were serialized because the
    // parser deferred variant selection: the method is overloaded and selection
    // depends on runtime argument types.  Runtime type-vector matching here is
    // stricter than value-based dispatch and, for an overloaded method where a
    // more-specific variant matches only via a defaulted trailing parameter (e.g.
    // a (string, hash<auto>, X, *bool) overload alongside a less-specific
    // (string, auto, X) overload, called with 3 args), it can select the
    // less-specific exact-arg-count variant that the parser and interpreter would
    // never pick.  For an overloaded method, defer variant selection to
    // value-based runtime dispatch (matching the interpreter and develop's AOT
    // path) rather than risk pre-binding the wrong variant.  Returning nullptr
    // leaves the call node without a pre-bound variant, so it dispatches by value
    // at call time.  This also avoids taking the program parse lock at AOT-load
    // time (the single-variant fallback below would otherwise require it), which
    // can invert lock order against a concurrent parse waiting on AOT init.
    if (func->numVariants() > 1) {
        return nullptr;
    }

    ExceptionSink xsink;
    const AbstractQoreFunctionVariant* variant = func->runtimeFindVariant(&xsink, arg_types, class_ctx,
        receiver_type_info, type_param_instantiation);
    if (xsink) {
        xsink.clear();
    }
    if (variant) {
        return variant;
    }

    // Single variant only: runtime type-vector matching is stricter than the
    // parser and can reject valid parse-time calls such as a hashdecl value
    // passed to an optional hashdecl parameter.  Fall back to parse-equivalent
    // matching (which can only resolve to that single variant here).
    ExceptionSink parse_xsink;
    ProgramRuntimeParseContextHelper pch(&parse_xsink, pgm);
    if (parse_xsink) {
        parse_xsink.clear();
        error = "failed to set parse context while resolving AOT argument type signature";
        return nullptr;
    }
    QoreTypeParamInstantiation parse_type_param_instantiation;
    variant = func->parseFindVariantNoDiagnostics(arg_types, class_ctx, receiver_type_info,
        &parse_type_param_instantiation);
    if (variant && type_param_instantiation) {
        *type_param_instantiation = std::move(parse_type_param_instantiation);
    }
    return variant;
}

static bool extract_aot_type_args(const char* path, const char* type_name, bool& or_nothing,
        std::vector<std::string>& args);

const TypedHashDecl* qore_aot_resolve_hashdecl_path(QoreProgram* pgm, const char* path) {
    if (!pgm || !path || !*path) {
        return nullptr;
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    const qore_ns_private* found_ns = nullptr;
    if (const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(*pp->RootNS, path, found_ns)) {
        return hd;
    }

    if (!strchr(path, '<')) {
        return nullptr;
    }

    std::string type_path;
    if (!strncmp(path, "hash<", 5) || !strncmp(path, "*hash<", 6)) {
        type_path = path;
    } else {
        type_path = "hash<";
        type_path += path;
        type_path += ">";
    }

    ExceptionSink xsink;
    ProgramRuntimeParseAccessHelper pah(&xsink, pgm);
    if (xsink) {
        xsink.clear();
        return nullptr;
    }

    const QoreTypeInfo* ti = qore_get_type_from_string_intern(type_path.c_str());
    if (xsink) {
        xsink.clear();
        return nullptr;
    }
    return QoreTypeInfo::getTypedHash(ti);
}

static std::string qoreAOTHashDeclDefaultDynamicName(const std::string& path, bool& or_nothing) {
    or_nothing = false;

    std::vector<std::string> args;
    if (extract_aot_type_args(path.c_str(), "hash", or_nothing, args) && args.size() == 1) {
        return args[0];
    }

    std::string rv = path;
    if (!rv.empty() && rv[0] == '*') {
        or_nothing = true;
        rv.erase(0, 1);
    }
    while (rv.rfind("::", 0) == 0) {
        rv.erase(0, 2);
    }
    return rv;
}

static QoreValue qoreAOTMakeHashDeclDefaultNode(QoreProgram* pgm, const std::string& path,
        QoreParseListNode* parse_args) {
    if (const TypedHashDecl* hd = qore_aot_resolve_hashdecl_path(pgm, path.c_str())) {
        return QoreValue(new NewHashDeclNode(&loc_builtin, hd, parse_args, false));
    }

    bool or_nothing = false;
    std::string dynamic_name = qoreAOTHashDeclDefaultDynamicName(path, or_nothing);
    const QoreTypeInfo* ti = qore_get_aot_deferred_type_info(&loc_builtin, dynamic_name.c_str(), or_nothing, true);
    return QoreValue(new NewHashDeclNode(&loc_builtin, dynamic_name.c_str(), ti, parse_args));
}

static QoreParseListNode* qoreAOTTakeParseArgs(std::vector<QoreValue>& args) {
    if (args.empty()) {
        return nullptr;
    }

    QoreParseListNode* parse_args = new QoreParseListNode(&loc_builtin);
    for (auto& v : args) {
        parse_args->add(v, &loc_builtin);
    }
    args.clear();
    return parse_args;
}

static QoreValue qoreAOTMakeObjectDefaultNode(QoreProgram* pgm, const QoreClass* qc,
        const std::string& class_path, std::vector<QoreValue>& args) {
    QoreParseListNode* parse_args = qoreAOTTakeParseArgs(args);
    ScopedObjectCallNode* socn = nullptr;
    if (qc) {
        socn = new ScopedObjectCallNode(&loc_builtin, qc, parse_args, qc->getTypeInfo());
    } else {
        const QoreTypeInfo* object_type_info = qore_get_aot_deferred_type_info(
            &loc_builtin, class_path.c_str(), false, false);
        socn = new ScopedObjectCallNode(&loc_builtin, class_path.c_str(), parse_args, object_type_info, pgm);
    }
    if (parse_args) {
        socn->resolveParseArgs();
    }
    return QoreValue(socn);
}

static void qoreAOTWriteContainerValueType(QoreAOTBinaryWriter& writer,
        QoreAOTContainerValueType kind, const char* type_path) {
    writer.writeU8(static_cast<uint8_t>(kind));
    if (kind == QoreAOTContainerValueType::Plain) {
        return;
    }
    size_t len = type_path ? strlen(type_path) : 0;
    writer.writeU32(static_cast<uint32_t>(len));
    writer.writeStringRef(type_path ? type_path : "", len);
}

static void qoreAOTWriteListValueType(QoreAOTBinaryWriter& writer,
        const QoreListNode* list) {
    if ((writer.feature_flags & QORE_AOT_FEAT_TYPED_VALUE_CONTAINERS) == 0) {
        return;
    }
    const QoreTypeInfo* ti = list ? list->getTypeInfo() : nullptr;
    const QoreTypeInfo* vt = QoreTypeInfo::getUniqueReturnComplexList(ti);
    if (vt && vt != anyTypeInfo) {
        std::string type_path = getAOTSerializableTypePath(ti);
        qoreAOTWriteContainerValueType(writer, QoreAOTContainerValueType::Complex,
            type_path.c_str());
        return;
    }
    qoreAOTWriteContainerValueType(writer, QoreAOTContainerValueType::Plain, nullptr);
}

static void qoreAOTWriteHashValueType(QoreAOTBinaryWriter& writer,
        const QoreHashNode* hash) {
    if ((writer.feature_flags & QORE_AOT_FEAT_TYPED_VALUE_CONTAINERS) == 0) {
        return;
    }
    if (hash) {
        if (const TypedHashDecl* hd = hash->getHashDecl()) {
            std::string path = hd->getNamespacePath();
            qoreAOTWriteContainerValueType(writer, QoreAOTContainerValueType::HashDecl,
                path.c_str());
            return;
        }
        const QoreTypeInfo* ti = hash->getTypeInfo();
        const QoreTypeInfo* vt = QoreTypeInfo::getUniqueReturnComplexHash(ti);
        if (vt && vt != anyTypeInfo) {
            std::string type_path = getAOTSerializableTypePath(ti);
            qoreAOTWriteContainerValueType(writer, QoreAOTContainerValueType::Complex,
                type_path.c_str());
            return;
        }
    }
    qoreAOTWriteContainerValueType(writer, QoreAOTContainerValueType::Plain, nullptr);
}

static bool qoreAOTReadContainerValueType(const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end, QoreAOTContainerValueType& kind,
        const char*& type_path, std::string& error) {
    kind = QoreAOTContainerValueType::Plain;
    type_path = nullptr;
    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPED_VALUE_CONTAINERS) == 0) {
        return true;
    }
    if (ptr + 1 > end) {
        error = "unexpected end of data reading typed container value kind";
        return false;
    }
    kind = static_cast<QoreAOTContainerValueType>(QoreAOTBinaryReader::readU8(ptr));
    if (kind == QoreAOTContainerValueType::Plain) {
        return true;
    }
    if (kind != QoreAOTContainerValueType::Complex && kind != QoreAOTContainerValueType::HashDecl) {
        error = "invalid typed container value kind";
        return false;
    }
    if (ptr + 8 > end) {
        error = "unexpected end of data reading typed container value type path";
        return false;
    }
    uint32_t len = QoreAOTBinaryReader::readU32(ptr);
    uint32_t offset = QoreAOTBinaryReader::readU32(ptr);
    type_path = reader.getString(offset);
    if (!type_path || !*type_path) {
        error = "invalid or empty typed container value type path";
        return false;
    }
    if (strlen(type_path) < len) {
        error = "truncated typed container value type path";
        return false;
    }
    return true;
}

static bool qoreAOTApplyContainerValueType(QoreValue& v,
        QoreAOTContainerValueType kind, const char* type_path, QoreProgram* pgm,
        std::string& error) {
    if (kind == QoreAOTContainerValueType::Plain) {
        return true;
    }

    const QoreTypeInfo* ti = nullptr;
    if (kind == QoreAOTContainerValueType::HashDecl) {
        const QoreNamespace* pns = nullptr;
        const TypedHashDecl* hd = pgm ? pgm->findHashDecl(type_path, pns) : nullptr;
        if (!hd) {
            error = "cannot resolve hashdecl '" + std::string(type_path)
                + "' while reading typed AOT container value";
            return false;
        }
        ti = hd->getTypeInfo();
    } else if (kind == QoreAOTContainerValueType::Complex) {
        QoreAOTTypeResolver resolver(pgm);
        std::string type_error;
        ti = resolver.resolve(type_path, type_error);
        if (!ti) {
            error = "cannot resolve complex type '" + std::string(type_path)
                + "' while reading typed AOT container value";
            if (!type_error.empty()) {
                error += ": " + type_error;
            }
            return false;
        }
    } else {
        error = "invalid typed AOT container value kind";
        return false;
    }

    ExceptionSink xsink;
    bool ok = QoreTypeInfo::retypeValue(v, ti, &xsink);
    if (!ok || xsink.isException()) {
        error = "cannot restore typed AOT container value as '"
            + std::string(type_path) + "'";
        if (xsink.isException()) {
            error += ": " + qoreAOTExceptionText(xsink);
        }
        return false;
    }
    return true;
}

// ---- QoreAOTBinaryWriter ----

bool QoreAOTBinaryWriter::writeValue(const QoreValue& v) {
    if (v.isNothing()) {
        writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
        return true;
    }
    if (v.isNull()) {
        writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NULL));
        return true;
    }
    // Preserve source constant-reference semantics inside folded container
    // values when the node pointer matches a known program constant. Avoid
    // only paths belonging to the top-level constant currently being written,
    // which would create a self-reference at load time.
    if (const_reverse_map && v.hasNode()) {
        qore_type_t nt = v.getType();
        if (nt == NT_OBJECT || nt == NT_HASH || nt == NT_LIST) {
            const AbstractQoreNode* node = v.getInternalNode();
            if (const std::string* path = aotFindConstantReverseMapPath(const_reverse_map, node)) {
                if (!aot_constant_path_belongs_to_fqn(*path, current_const_path)
                        && aot_constant_path_available_for_writer(*this, *path)) {
                    writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_CONST_REF));
                    writeU32(static_cast<uint32_t>(path->size()));
                    writeStringRef(path->c_str(), path->size());
                    return true;
                }
            }
        }
    }
    // Must check isEnum() before getType() because getType() on TAG_ENUM
    // returns the base type (e.g., NT_INT), which would serialize the wrong thing
    if (v.isEnum()) {
        writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_ENUM));
        const QoreEnumMember* member = v.getEnumMember();
        std::string path = member->getEnumDecl()->getNamespacePath();
        writeU32(static_cast<uint32_t>(path.size()));
        writeStringRef(path.c_str(), path.size());
        const char* name = member->getName();
        uint32_t name_len = static_cast<uint32_t>(strlen(name));
        writeU32(name_len);
        writeStringRef(name, name_len);
        return true;
    }

    qore_type_t t = v.getType();
    switch (t) {
        case NT_BOOLEAN: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_BOOL));
            writeU8(v.getAsBool() ? 1 : 0);
            return true;
        }
        case NT_INT: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_INT64));
            writeI64(v.getAsBigInt());
            return true;
        }
        case NT_CHAR: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_CHAR));
            writeU32(v.getChar());
            return true;
        }
        case NT_FLOAT: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_FLOAT64));
            writeF64(v.getAsFloat());
            return true;
        }
        case NT_STRING: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_STRING));
            QoreStringValueHelper str(v);
            writeU32(static_cast<uint32_t>(str->size()));
            writeStringRef(str->c_str(), str->size());
            return true;
        }
        case NT_DATE: {
            const DateTimeNode* dt = v.get<const DateTimeNode>();
            if (!dt) {
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
                return true;
            }
            if (dt->isRelative()) {
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_REL_DATE));
                // For relative dates, store individual components
                writeI64(static_cast<int64_t>(dt->getYear()));
                writeI64(static_cast<int64_t>(dt->getMonth()));
                writeI64(static_cast<int64_t>(dt->getDay()));
                writeI64(static_cast<int64_t>(dt->getHour()));
                writeI64(static_cast<int64_t>(dt->getMinute()));
                writeI64(static_cast<int64_t>(dt->getSecond()));
                writeI64(static_cast<int64_t>(dt->getMicrosecond()));
            } else {
                const AbstractQoreZoneInfo* zone = dt->getZone();
                const char* region = zone ? zone->getRegionName() : nullptr;
                // Use region name for DST-aware zones (e.g., "Europe/Paris")
                // Offset zones have names like "+01:00", "-06:00" — use fixed offset for those
                if (region && region[0] != '+' && region[0] != '-' && strcmp(region, "UTC") != 0) {
                    writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_ABS_DATE_REGION));
                    writeI64(dt->getEpochMicrosecondsUTC());
                    // Write region name as length-prefixed string in string pool
                    uint32_t len = static_cast<uint32_t>(strlen(region));
                    writeU32(len);
                    writeStringRef(region, len);
                } else {
                    writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_ABS_DATE));
                    // For absolute dates, store epoch microseconds UTC + zone offset
                    writeI64(dt->getEpochMicrosecondsUTC());
                    // Store UTC offset in seconds for zone reconstruction
                    int utc_offset = 0;
                    if (zone) {
                        utc_offset = AbstractQoreZoneInfo::getUTCOffset(zone);
                    }
                    writeI64(static_cast<int64_t>(utc_offset));
                }
            }
            return true;
        }
        case NT_NUMBER: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NUMBER));
            const QoreNumberNode* num = v.get<const QoreNumberNode>();
            if (num) {
                // Serialize as string representation for portability
                QoreString str;
                num->toString(str, QORE_NF_RAW);
                writeU32(static_cast<uint32_t>(str.size()));
                writeStringRef(str.c_str(), str.size());
            } else {
                writeU32(0);
                writeStringRef("0", 1);
            }
            return true;
        }
        case NT_BINARY: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_BINARY));
            const BinaryNode* bin = v.get<const BinaryNode>();
            if (bin && bin->size() > 0) {
                writeU32(static_cast<uint32_t>(bin->size()));
                writeBytes(bin->getPtr(), static_cast<uint32_t>(bin->size()));
            } else {
                writeU32(0);
            }
            return true;
        }
        case NT_PLUGIN_VALUE: {
            QorePluginSerializedValueInfo plugin_value;
            ExceptionSink xsink;
            if (qore_plugin_serialize_value_node(v.getInternalNode(), plugin_value, &xsink) || xsink) {
                if (xsink) {
                    tracePluginQord("write failed: plugin value serialization failed: "
                        + qoreAOTExceptionText(xsink));
                }
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
                return false;
            }
            uint16_t import_idx = 0;
            if (!addPluginTypeRef(plugin_value.module_name.c_str(), plugin_value.local_type_id, &import_idx)) {
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
                return false;
            }
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_PLUGIN_INSTANCE));
            writeU16(import_idx);
            writeU16(plugin_value.local_type_id);
            writeU16(plugin_value.serializer_format_version);
            writeU16(0);
            writeU32(static_cast<uint32_t>(plugin_value.payload.size()));
            if (!plugin_value.payload.empty()) {
                writeBytes(plugin_value.payload.data(), static_cast<uint32_t>(plugin_value.payload.size()));
            }
            return true;
        }
        case NT_LIST: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_LIST));
            const QoreListNode* list = v.get<const QoreListNode>();
            qoreAOTWriteListValueType(*this, list);
            if (list) {
                uint32_t count = static_cast<uint32_t>(list->size());
                writeU32(count);
                for (uint32_t i = 0; i < count; ++i) {
                    // Must not return false here - would leave partial data
                    // Unsupported element types become NOTHING
                    writeValue(list->retrieveEntry(i));
                }
            } else {
                writeU32(0);
            }
            return true;
        }
        case NT_HASH: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_HASH));
            const QoreHashNode* hash = v.get<const QoreHashNode>();
            qoreAOTWriteHashValueType(*this, hash);
            if (hash) {
                uint32_t count = static_cast<uint32_t>(hash->size());
                writeU32(count);
                ConstHashIterator hi(*hash);
                while (hi.next()) {
                    const char* key = hi.getKey();
                    writeStringRef(key);
                    // Must not return false here - would leave partial data
                    // Unsupported value types become NOTHING
                    writeValue(hi.get());
                }
            } else {
                writeU32(0);
            }
            return true;
        }
        case NT_SCOPE_REF: {
            // NT_SCOPE_REF is shared by ScopedObjectCallNode, NewHashDeclNode,
            // NewComplexListNode, NewComplexHashNode — must use dynamic_cast
            const AbstractQoreNode* node = v.getInternalNode();
            const ScopedObjectCallNode* socn = dynamic_cast<const ScopedObjectCallNode*>(node);
            if (socn && (socn->oc || socn->isDynamicObjectConstruct())) {
                std::string class_path = socn->oc
                    ? qore_aot_encode_class_ref(socn->oc)
                    : socn->getDynamicClassName();
                const QoreListNode* args = socn->getArgs();
                const QoreParseListNode* parse_args = socn->getParseArgs();
                if (parse_args && !parse_args->empty() && (!args || args->empty())) {
                    writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
                    return true;
                }
                uint32_t nargs = args ? static_cast<uint32_t>(args->size()) : 0;

                // Only serialize if all args are serializable concrete values
                bool args_ok = true;
                for (uint32_t i = 0; i < nargs; ++i) {
                    QoreValue arg = args->retrieveEntry(i);
                    // Check that args are concrete value types we can serialize
                    if (arg.getType() == NT_SCOPE_REF || arg.getType() == NT_FUNCTION_CALL
                            || arg.getType() == NT_SELF_VARREF || arg.getType() == NT_VARREF) {
                        args_ok = false;
                        break;
                    }
                }

                if (args_ok) {
                    writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NEW_OBJECT));
                    writeU32(static_cast<uint32_t>(class_path.size()));
                    writeStringRef(class_path.c_str(), class_path.size());
                    writeU32(nargs);
                    for (uint32_t i = 0; i < nargs; ++i) {
                        writeValue(args->retrieveEntry(i));
                    }
                    return true;
                }
            }
            // NewComplexListNode: `list<T> m();` default-constructed complex list
            if (auto* ncl = dynamic_cast<const NewComplexListNode*>(node)) {
                std::string type_path = getAOTSerializableTypePath(ncl->typeInfo);
                // NewComplexListNode stores `args` as a single QoreValue that
                // may be a list node or NOTHING. For empty-arg ctor calls
                // (the common case, `list<T> m();`), args is NOTHING.
                uint32_t nargs = 0;
                const QoreListNode* arg_list = nullptr;
                if (!ncl->args.isNothing()) {
                    arg_list = ncl->args.getType() == NT_LIST
                        ? ncl->args.get<const QoreListNode>() : nullptr;
                    if (arg_list) {
                        nargs = static_cast<uint32_t>(arg_list->size());
                    }
                }
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT));
                writeU8(0); // kind: 0 = complex list
                uint32_t tlen = static_cast<uint32_t>(type_path.size());
                writeU32(tlen);
                writeStringRef(type_path.c_str(), tlen);
                writeU32(nargs);
                for (uint32_t i = 0; i < nargs; ++i) {
                    writeValue(arg_list->retrieveEntry(i));
                }
                return true;
            }
            // NewComplexHashNode: `hash<K, V> m();` default-constructed complex hash
            if (auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
                std::string type_path = getAOTSerializableTypePath(nch->typeInfo);
                uint32_t nargs = nch->args ? static_cast<uint32_t>(nch->args->size()) : 0;
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT));
                writeU8(1); // kind: 1 = complex hash
                uint32_t tlen = static_cast<uint32_t>(type_path.size());
                writeU32(tlen);
                writeStringRef(type_path.c_str(), tlen);
                writeU32(nargs);
                for (uint32_t i = 0; i < nargs; ++i) {
                    writeValue(nch->args->get(i));
                }
                return true;
            }
            // NewComplexBufferNode: `buffer<T> m();` default-constructed complex buffer
            if (auto* ncb = dynamic_cast<const NewComplexBufferNode*>(node)) {
                std::string type_path = getAOTSerializableTypePath(ncb->typeInfo);
                uint32_t nargs = 0;
                const QoreListNode* arg_list = nullptr;
                if (!ncb->args.isNothing()) {
                    arg_list = ncb->args.getType() == NT_LIST
                        ? ncb->args.get<const QoreListNode>() : nullptr;
                    if (arg_list) {
                        nargs = static_cast<uint32_t>(arg_list->size());
                    }
                }
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT));
                writeU8(3); // kind: 3 = complex buffer
                if ((feature_flags & QORE_AOT_FEAT_COMPLEX_BUFFER_INIT_KIND) != 0) {
                    writeU8(static_cast<uint8_t>(ncb->initKind));
                }
                uint32_t tlen = static_cast<uint32_t>(type_path.size());
                writeU32(tlen);
                writeStringRef(type_path.c_str(), tlen);
                writeU32(nargs);
                for (uint32_t i = 0; i < nargs; ++i) {
                    writeValue(arg_list->retrieveEntry(i));
                }
                return true;
            }
            // NewHashDeclNode: `<MyHashdecl> m();` default-constructed hashdecl
            if (auto* nhd = dynamic_cast<const NewHashDeclNode*>(node)) {
                const TypedHashDecl* hd = nhd->hd;
                std::string ns_path = hd ? hd->getNamespacePath()
                    : (nhd->isDynamicHashDeclConstruct() ? nhd->getDynamicHashDeclName() : std::string());
                uint32_t nargs = nhd->args ? static_cast<uint32_t>(nhd->args->size()) : 0;
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT));
                writeU8(2); // kind: 2 = hashdecl
                uint32_t tlen = static_cast<uint32_t>(ns_path.size());
                writeU32(tlen);
                writeStringRef(ns_path.c_str(), tlen);
                writeU32(nargs);
                for (uint32_t i = 0; i < nargs; ++i) {
                    writeValue(nhd->args->get(i));
                }
                return true;
            }
            // Fall through to default if not serializable
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
            return true;
        }

        default:
            // Unsupported value type - write NOTHING instead of failing
            // This preserves binary structure integrity for container types
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
            return true;
    }
}

bool QoreAOTBinaryWriter::finalize(const QoreAOTBinaryHeader& in_header, std::vector<uint8_t>& output) {
    // Make a mutable copy so we can fill in section count
    QoreAOTBinaryHeader header = in_header;
    header.section_count = static_cast<uint32_t>(sections.size());

    // Fixed header size (60 bytes)
    uint32_t header_size = QORE_AOT_HEADER_SIZE;
    uint32_t section_dir_size = static_cast<uint32_t>(sections.size() * sizeof(QoreAOTSectionHeader));
    uint32_t string_pool_size = strings.size();
    uint32_t data_size = static_cast<uint32_t>(buffer.size());
    uint32_t total = header_size + section_dir_size + 4 /* string pool size */ + string_pool_size + data_size;

    output.clear();
    output.reserve(total);

    // Write header (60 bytes, single flat format)
    auto writeU8LE = [&](uint8_t v) {
        output.push_back(v);
    };
    auto writeU16LE = [&](uint16_t v) {
        output.push_back(static_cast<uint8_t>(v & 0xFF));
        output.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    };
    auto writeU32LE = [&](uint32_t v) {
        output.push_back(static_cast<uint8_t>(v & 0xFF));
        output.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        output.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        output.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    auto writeI64LE = [&](int64_t v) {
        uint64_t uv;
        memcpy(&uv, &v, sizeof(uv));
        for (int i = 0; i < 8; ++i) {
            output.push_back(static_cast<uint8_t>((uv >> (i * 8)) & 0xFF));
        }
    };
    auto writeU64LE = [&](uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            output.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
        }
    };

    // Bytes 0-3: magic
    writeU32LE(header.magic);
    // Bytes 4-5: version
    writeU16LE(header.version);
    // Bytes 6-7: flags
    writeU16LE(header.flags);
    // Bytes 8-15: parse_options_lo
    writeI64LE(header.parse_options_lo);
    // Bytes 16-19: section_count
    writeU32LE(header.section_count);
    // Bytes 20-23: label_offset
    writeU32LE(header.label_offset);
    // Bytes 24-27: label_length
    writeU32LE(header.label_length);
    // Bytes 28-29: max_opcode_id
    writeU16LE(header.max_opcode_id);
    // Bytes 30: qore_version_major
    writeU8LE(header.qore_version_major);
    // Bytes 31: qore_version_minor
    writeU8LE(header.qore_version_minor);
    // Bytes 32-33: qore_version_patch
    writeU16LE(header.qore_version_patch);
    // Bytes 34: compression, byte 35: reserved (separate writes to preserve layout)
    writeU8LE(header.compression);
    writeU8LE(header.reserved);
    // Bytes 36-43: parse_options_hi
    writeI64LE(header.parse_options_hi);
    // Bytes 44-51: source_hash
    writeU64LE(header.source_hash);
    // Bytes 52-59: feature_flags
    writeU64LE(header.feature_flags);

    // Write section directory
    for (auto& sec : sections) {
        writeU16LE(sec.type);
        writeU16LE(sec.reserved);
        writeU32LE(sec.offset);
        writeU32LE(sec.size);
    }

    // Write string pool (preceded by its size)
    writeU32LE(string_pool_size);
    const auto& pool_data = strings.getData();
    output.insert(output.end(), pool_data.begin(), pool_data.end());

    // Write data area
    output.insert(output.end(), buffer.begin(), buffer.end());

    return true;
}

// ---- QoreAOTBinaryReader ----

bool QoreAOTBinaryReader::open(const uint8_t* in_data, uint32_t in_size, std::string& error) {
    data = in_data;
    total_size = in_size;

    // Fixed header size (60 bytes)
    const uint32_t header_size = QORE_AOT_HEADER_SIZE;

    // Check full header fits
    if (in_size < header_size) {
        error = "binary too small for header (" + std::to_string(in_size) + " < " + std::to_string(header_size) + ")";
        return false;
    }

    // Read header (60 bytes, single flat format)
    const uint8_t* ptr = data;
    header.magic = readU32(ptr);
    header.version = readU16(ptr);
    header.flags = readU16(ptr);
    header.parse_options_lo = readI64(ptr);
    header.section_count = readU32(ptr);
    header.label_offset = readU32(ptr);
    header.label_length = readU32(ptr);
    header.max_opcode_id = readU16(ptr);
    header.qore_version_major = readU8(ptr);
    header.qore_version_minor = readU8(ptr);
    header.qore_version_patch = readU16(ptr);
    header.compression = readU8(ptr);
    header.reserved = readU8(ptr);
    header.parse_options_hi = readI64(ptr);

    // Read new v1 fields: source_hash (8) and feature_flags (8)
    uint64_t source_hash = 0;
    uint64_t feature_flags = 0;
    {
        uint64_t temp = 0;
        for (int i = 0; i < 8; ++i) {
            temp |= static_cast<uint64_t>(ptr[i]) << (i * 8);
        }
        source_hash = temp;
        ptr += 8;
        temp = 0;
        for (int i = 0; i < 8; ++i) {
            temp |= static_cast<uint64_t>(ptr[i]) << (i * 8);
        }
        feature_flags = temp;
        ptr += 8;
    }
    header.source_hash = source_hash;
    header.feature_flags = feature_flags;

    // Validate magic
    if (header.magic != QORE_AOT_BINARY_MAGIC) {
        error = "invalid magic number (expected QORD)";
        return false;
    }

    // Validate version (must be exactly 1)
    if (header.version != QORE_AOT_BINARY_VERSION) {
        error = "unsupported binary format version " + std::to_string(header.version)
              + " (expected " + std::to_string(QORE_AOT_BINARY_VERSION) + ")"
              + "; please update your Qore installation";
        return false;
    }

    // Validate opcode compatibility
    if (header.max_opcode_id > QORE_IR_MAX_OPCODE) {
        error = "binary was compiled with Qore "
              + std::to_string(header.qore_version_major) + "."
              + std::to_string(header.qore_version_minor) + "."
              + std::to_string(header.qore_version_patch)
              + " (max opcode ID " + std::to_string(header.max_opcode_id) + ")"
              + " but this runtime only supports up to opcode ID "
              + std::to_string(QORE_IR_MAX_OPCODE)
              + "; please update your Qore installation";
        return false;
    }

    // Handle decompression if needed (before reading section directory and string pool)
    // When compressed, the entire post-header region is compressed
    if (header.compression == 1) {
        const uint8_t* compressed_start = data + header_size;
        size_t compressed_len = in_size - header_size;
        std::string decomp_error;
        if (!decompressMetadata(compressed_start, compressed_len, decompressed_body, decomp_error)) {
            error = "failed to decompress metadata: " + decomp_error;
            return false;
        }
        // Use decompressed data as our working buffer
        // IMPORTANT: After decompression, ptr and data must be consistent!
        // ptr was pointing into section directory, so reset it relative to new buffer
        data = decompressed_body.data();
        total_size = static_cast<uint32_t>(decompressed_body.size());
        ptr = data;  // Reset ptr to start of decompressed data (start of section directory)
    }

    // Read section directory
    uint32_t section_dir_size = header.section_count * sizeof(QoreAOTSectionHeader);
    uint32_t needed = header_size + section_dir_size;
    if (total_size < needed) {
        error = "binary too small for section directory";
        return false;
    }

    sections.resize(header.section_count);
    for (uint32_t i = 0; i < header.section_count; ++i) {
        sections[i].type = readU16(ptr);
        sections[i].reserved = readU16(ptr);
        sections[i].offset = readU32(ptr);
        sections[i].size = readU32(ptr);
    }

    // Read string pool size
    if (ptr + 4 > data + total_size) {
        error = "binary too small for string pool size";
        return false;
    }
    string_pool_size = readU32(ptr);

    // Validate string pool
    if (ptr + string_pool_size > data + total_size) {
        error = "binary too small for string pool data";
        return false;
    }
    string_pool = reinterpret_cast<const char*>(ptr);
    ptr += string_pool_size;

    // Remaining data is the data area
    data_area = ptr;
    data_area_size = static_cast<uint32_t>((data + total_size) - ptr);

    // Validate section offsets
    for (auto& sec : sections) {
        if (sec.offset + sec.size > data_area_size) {
            error = "section offset/size exceeds data area";
            return false;
        }
    }

    return true;
}

static bool qoreAOTGetPluginImportModuleName(const QoreAOTBinaryReader& reader,
        uint16_t import_idx, const char*& module_name, std::string& error) {
    module_name = nullptr;
    const QoreAOTSectionHeader* import_sec = reader.findSection(QoreAOTSectionType::PLUGIN_IMPORTS);
    if (!import_sec) {
        error = "QORD-PLUGIN-IMPORT-MISSING: plugin value instance requires PLUGIN_IMPORTS";
        return false;
    }
    const uint8_t* ptr = reader.getSectionData(*import_sec);
    if (!ptr) {
        error = "invalid PLUGIN_IMPORTS section data";
        return false;
    }
    const uint8_t* end = ptr + import_sec->size;
    auto ensure = [&](size_t needed, const char* what) -> bool {
        if (needed > static_cast<size_t>(end - ptr)) {
            error = "unexpected end of PLUGIN_IMPORTS while reading ";
            error += what;
            return false;
        }
        return true;
    };

    if (!ensure(4, "import count")) {
        return false;
    }
    uint32_t import_count = QoreAOTBinaryReader::readU32(ptr);
    for (uint32_t i = 0; i < import_count; ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT plugin import module lookup")) {
            error = "operation cancelled during AOT plugin import module lookup";
            return false;
        }
        if (!ensure(12, "plugin import strings")) {
            return false;
        }
        const char* candidate_module = reader.readStringRef(ptr);
        (void)reader.readStringRef(ptr);
        (void)reader.readStringRef(ptr);
        if (!candidate_module || !*candidate_module) {
            error = "QORD-PLUGIN-IMPORT-MISSING: invalid plugin import string reference";
            return false;
        }
        if (!ensure(4, "plugin import type count")) {
            return false;
        }
        uint32_t type_count = QoreAOTBinaryReader::readU32(ptr);
        if (type_count > std::numeric_limits<size_t>::max() / sizeof(uint16_t)) {
            error = "PLUGIN_IMPORTS type id byte count overflow";
            return false;
        }
        size_t type_bytes = static_cast<size_t>(type_count) * sizeof(uint16_t);
        if (!ensure(type_bytes, "plugin import type ids")) {
            return false;
        }
        ptr += type_bytes;
        if (!ensure(4, "plugin import operation count")) {
            return false;
        }
        uint32_t operation_count = QoreAOTBinaryReader::readU32(ptr);
        if (operation_count > std::numeric_limits<size_t>::max() / sizeof(uint16_t)) {
            error = "PLUGIN_IMPORTS operation id byte count overflow";
            return false;
        }
        size_t operation_bytes = static_cast<size_t>(operation_count) * sizeof(uint16_t);
        if (!ensure(operation_bytes, "plugin import operation ids")) {
            return false;
        }
        ptr += operation_bytes;
        if (i == import_idx) {
            module_name = candidate_module;
            return true;
        }
    }

    error = "QORD-PLUGIN-IMPORT-MISSING: plugin value import index ";
    error += std::to_string(import_idx);
    error += " is out of range";
    return false;
}

QoreValue QoreAOTBinaryReader::readValue(const uint8_t*& ptr, const uint8_t* end,
        std::string& error) const {
    if (ptr >= end) {
        error = "unexpected end of data reading value tag";
        return QoreValue();
    }

    QoreAOTValueTag tag = static_cast<QoreAOTValueTag>(readU8(ptr));
    switch (tag) {
        case QoreAOTValueTag::VT_NOTHING:
            return QoreValue();

        case QoreAOTValueTag::VT_NULL:
            return QoreValue(null());

        case QoreAOTValueTag::VT_BOOL: {
            if (ptr >= end) {
                error = "unexpected end of data reading bool value";
                return QoreValue();
            }
            uint8_t b = readU8(ptr);
            return QoreValue(b != 0);
        }

        case QoreAOTValueTag::VT_INT64: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading int64 value";
                return QoreValue();
            }
            int64_t val = readI64(ptr);
            return QoreValue(val);
        }

        case QoreAOTValueTag::VT_CHAR: {
            if (ptr + 4 > end) {
                error = "unexpected end of data reading char value";
                return QoreValue();
            }
            unsigned cp = readU32(ptr);
            if (!QoreValue::isValidCharCodepoint(cp)) {
                error = "invalid char codepoint in value: U+" + std::to_string(cp);
                return QoreValue();
            }
            return QoreValue::makeChar(cp);
        }

        case QoreAOTValueTag::VT_FLOAT64: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading float64 value";
                return QoreValue();
            }
            double val = readF64(ptr);
            return QoreValue(val);
        }

        case QoreAOTValueTag::VT_STRING: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading string value";
                return QoreValue();
            }
            uint32_t len = readU32(ptr);
            uint32_t str_offset = readU32(ptr);
            const char* str = getString(str_offset);
            if (!str) {
                error = "invalid string offset in value";
                return QoreValue();
            }
            return QoreValue::makeStringValue(str, len, QCS_UTF8);
        }

        case QoreAOTValueTag::VT_ABS_DATE: {
            if (ptr + 16 > end) {
                error = "unexpected end of data reading abs_date value";
                return QoreValue();
            }
            int64_t epoch_us = readI64(ptr);
            int64_t utc_offset = readI64(ptr);
            // Reconstruct zone from UTC offset
            const AbstractQoreZoneInfo* zone = nullptr;
            if (utc_offset != 0) {
                zone = findCreateOffsetZone(static_cast<int>(utc_offset));
            }
            // Convert epoch_us to seconds + microseconds
            int64_t epoch_s = epoch_us / 1000000;
            int us = static_cast<int>(epoch_us % 1000000);
            if (us < 0) {
                // Handle negative microseconds (dates before epoch)
                epoch_s -= 1;
                us += 1000000;
            }
            return QoreValue(DateTimeNode::makeAbsolute(zone, epoch_s, us));
        }

        case QoreAOTValueTag::VT_ABS_DATE_REGION: {
            if (ptr + 12 > end) {
                error = "unexpected end of data reading abs_date_region value";
                return QoreValue();
            }
            int64_t epoch_us = readI64(ptr);
            uint32_t name_len = readU32(ptr);
            if (ptr + 4 > end) {
                error = "unexpected end of data reading region name offset";
                return QoreValue();
            }
            // Read string pool offset (writeStringRef writes a pool offset)
            uint32_t str_offset = readU32(ptr);
            const char* region_name = getString(str_offset);
            if (!region_name) {
                error = "invalid string offset for region name";
                return QoreValue();
            }
            // Look up region zone
            ExceptionSink xsink;
            const AbstractQoreZoneInfo* zone = QTZM.findLoadRegion(region_name, &xsink);
            if (!zone || xsink) {
                // Fallback to UTC if region not found
                xsink.clear();
                zone = nullptr;
            }
            // Convert epoch_us to seconds + microseconds
            int64_t epoch_s = epoch_us / 1000000;
            int us = static_cast<int>(epoch_us % 1000000);
            if (us < 0) {
                epoch_s -= 1;
                us += 1000000;
            }
            return QoreValue(DateTimeNode::makeAbsolute(zone, epoch_s, us));
        }

        case QoreAOTValueTag::VT_REL_DATE: {
            if (ptr + 56 > end) {
                error = "unexpected end of data reading rel_date value";
                return QoreValue();
            }
            int year = static_cast<int>(readI64(ptr));
            int month = static_cast<int>(readI64(ptr));
            int day = static_cast<int>(readI64(ptr));
            int hour = static_cast<int>(readI64(ptr));
            int minute = static_cast<int>(readI64(ptr));
            int second = static_cast<int>(readI64(ptr));
            int us = static_cast<int>(readI64(ptr));
            return QoreValue(DateTimeNode::makeRelativeUnnormalized(year, month, day, hour, minute, second, us));
        }

        case QoreAOTValueTag::VT_NUMBER: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading number value";
                return QoreValue();
            }
            uint32_t len = readU32(ptr);
            uint32_t str_offset = readU32(ptr);
            const char* str = getString(str_offset);
            if (!str) {
                error = "invalid string offset in number value";
                return QoreValue();
            }
            return QoreValue(new QoreNumberNode(str));
        }

        case QoreAOTValueTag::VT_BINARY: {
            if (ptr + 4 > end) {
                error = "unexpected end of data reading binary value";
                return QoreValue();
            }
            uint32_t len = readU32(ptr);
            if (len == 0) {
                return QoreValue(new BinaryNode());
            }
            if (ptr + len > end) {
                error = "unexpected end of data reading binary payload";
                return QoreValue();
            }
            void* buf = malloc(len);
            if (!buf) {
                error = "out of memory allocating binary value";
                return QoreValue();
            }
            memcpy(buf, ptr, len);
            ptr += len;
            return QoreValue(new BinaryNode(buf, len));
        }

        case QoreAOTValueTag::VT_PLUGIN_INSTANCE: {
            if (static_cast<size_t>(end - ptr) < 12) {
                error = "unexpected end of data reading plugin value instance header";
                return QoreValue();
            }
            uint16_t import_idx = readU16(ptr);
            uint16_t local_type_id = readU16(ptr);
            uint16_t serializer_version = readU16(ptr);
            uint16_t reserved = readU16(ptr);
            uint32_t payload_len = readU32(ptr);
            if (reserved) {
                error = "QORD-PLUGIN-RESERVED-NONZERO: plugin value instance reserved field is non-zero";
                return QoreValue();
            }
            if (payload_len > static_cast<size_t>(end - ptr)) {
                error = "unexpected end of data reading plugin value payload";
                return QoreValue();
            }
            const char* module_name = nullptr;
            if (!qoreAOTGetPluginImportModuleName(*this, import_idx, module_name, error)) {
                return QoreValue();
            }
            ExceptionSink xsink;
            QoreValue rv = qore_plugin_deserialize_value(module_name, local_type_id, serializer_version, ptr,
                payload_len, &xsink);
            ptr += payload_len;
            if (xsink) {
                error = qoreAOTExceptionText(xsink);
                rv.discard(nullptr);
                return QoreValue();
            }
            return rv;
        }

        case QoreAOTValueTag::VT_LIST: {
            QoreAOTContainerValueType kind = QoreAOTContainerValueType::Plain;
            const char* type_path = nullptr;
            if (!qoreAOTReadContainerValueType(*this, ptr, end, kind, type_path, error)) {
                return QoreValue();
            }
            if (ptr + 4 > end) {
                error = "unexpected end of data reading list count";
                return QoreValue();
            }
            uint32_t count = readU32(ptr);
            ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
            for (uint32_t i = 0; i < count; ++i) {
                QoreValue elem = readValue(ptr, end, error);
                if (!error.empty()) {
                    return QoreValue();
                }
                qore_list_private::get(**list)->pushIntern(elem);
            }
            QoreValue rv(list.release());
            if (!qoreAOTApplyContainerValueType(rv, kind, type_path, getProgram(), error)) {
                rv.discard(nullptr);
                return QoreValue();
            }
            return rv;
        }

        case QoreAOTValueTag::VT_HASH: {
            QoreAOTContainerValueType kind = QoreAOTContainerValueType::Plain;
            const char* type_path = nullptr;
            if (!qoreAOTReadContainerValueType(*this, ptr, end, kind, type_path, error)) {
                return QoreValue();
            }
            if (ptr + 4 > end) {
                error = "unexpected end of data reading hash count";
                return QoreValue();
            }
            uint32_t count = readU32(ptr);
            ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), nullptr);
            for (uint32_t i = 0; i < count; ++i) {
                if (ptr + 4 > end) {
                    error = "unexpected end of data reading hash key";
                    return QoreValue();
                }
                uint32_t key_offset = readU32(ptr);
                const char* key = getString(key_offset);
                if (!key) {
                    error = "invalid string offset for hash key";
                    return QoreValue();
                }
                QoreValue val = readValue(ptr, end, error);
                if (!error.empty()) {
                    return QoreValue();
                }
                qore_hash_private::get(**hash)->setKeyValueIntern(key, val);
            }
            QoreValue rv(hash.release());
            if (!qoreAOTApplyContainerValueType(rv, kind, type_path, getProgram(), error)) {
                rv.discard(nullptr);
                return QoreValue();
            }
            return rv;
        }

        case QoreAOTValueTag::VT_OPAQUE_DEFAULT:
            // Complex expression default (e.g. function call) that couldn't be
            // serialized. Return boolean True as a placeholder to mark the parameter
            // as optional in the function signature. The actual default is evaluated
            // by the compiled function code at runtime.
            return QoreValue(true);

        case QoreAOTValueTag::VT_ENUM: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading enum path";
                return QoreValue();
            }
            uint32_t path_len = readU32(ptr);
            uint32_t path_offset = readU32(ptr);
            const char* path = getString(path_offset);
            if (!path) {
                error = "invalid string offset for enum path";
                return QoreValue();
            }
            if (ptr + 8 > end) {
                error = "unexpected end of data reading enum member name";
                return QoreValue();
            }
            uint32_t name_len = readU32(ptr);
            uint32_t name_offset = readU32(ptr);
            const char* member_name = getString(name_offset);
            if (!member_name) {
                error = "invalid string offset for enum member name";
                return QoreValue();
            }
            const QoreNamespace* pns = nullptr;
            const QoreEnumDecl* ed = getProgram()->findEnum(path, pns);
            if (!ed) {
                error = std::string("enum not found: ") + path;
                return QoreValue();
            }
            const QoreEnumMember* member = ed->findMember(member_name);
            if (!member) {
                error = std::string("enum member not found: ") + std::string(path) + "::" + member_name;
                return QoreValue();
            }
            return QoreValue::makeEnum(member);
        }

        case QoreAOTValueTag::VT_NEW_OBJECT: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading new_object class path";
                return QoreValue();
            }
            uint32_t path_len = readU32(ptr);
            uint32_t path_offset = readU32(ptr);
            const char* class_path = getString(path_offset);
            if (!class_path) {
                error = "invalid string offset for new_object class path";
                return QoreValue();
            }
            if (ptr + 4 > end) {
                error = "unexpected end of data reading new_object arg count";
                return QoreValue();
            }
            uint32_t nargs = readU32(ptr);

            // Read constructor arguments
            QoreParseListNode* parse_args = nullptr;
            if (nargs > 0) {
                parse_args = new QoreParseListNode(&loc_builtin);
                for (uint32_t i = 0; i < nargs; ++i) {
                    QoreValue arg = readValue(ptr, end, error);
                    if (!error.empty()) {
                        delete parse_args;
                        return QoreValue();
                    }
                    parse_args->add(arg, &loc_builtin);
                }
            }

            // Resolve the class through the same encoded class-ref path used
            // by call sites and slots. This preserves module ownership for
            // private classes referenced by defaults.
            const QoreClass* qc = qoreAOTResolveClassRefForDeserialization(
                getProgram(), class_path);
            if (!qc) {
                error = "cannot resolve new_object default class '";
                error += qoreAOTDescribeClassRef(class_path);
                error += "'";
                delete parse_args;
                return QoreValue();
            }
            ScopedObjectCallNode* socn = new ScopedObjectCallNode(&loc_builtin, qc, parse_args);
            // Convert parse_args to args so evalImpl() doesn't hit the assertion
            if (parse_args) {
                socn->resolveParseArgs();
            }
            return QoreValue(socn);
        }

        case QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT: {
            // Complex-type default construction: kind + type path + args.
            // Kind 0 = complex list, 1 = complex hash, 2 = hashdecl.
            if (ptr + 1 > end) {
                error = "unexpected end of data reading complex_default kind";
                return QoreValue();
            }
            uint8_t kind = readU8(ptr);
            QoreComplexBufferInitKind buffer_init_kind = QoreComplexBufferInitKind::Constructor;
            if (kind == 3 && (getHeader().feature_flags & QORE_AOT_FEAT_COMPLEX_BUFFER_INIT_KIND) != 0) {
                if (ptr + 1 > end) {
                    error = "unexpected end of data reading complex buffer init kind";
                    return QoreValue();
                }
                buffer_init_kind = static_cast<QoreComplexBufferInitKind>(readU8(ptr));
            }
            if (ptr + 8 > end) {
                error = "unexpected end of data reading complex_default type path";
                return QoreValue();
            }
            (void)readU32(ptr);  // path_len (unused — using string pool offset)
            uint32_t path_offset = readU32(ptr);
            const char* type_path = getString(path_offset);
            if (!type_path) {
                error = "invalid string offset for complex_default type path";
                return QoreValue();
            }
            if (ptr + 4 > end) {
                error = "unexpected end of data reading complex_default arg count";
                return QoreValue();
            }
            uint32_t nargs = readU32(ptr);
            // Read args (always, even if type resolution fails — must advance ptr)
            std::vector<QoreValue> args;
            args.reserve(nargs);
            for (uint32_t i = 0; i < nargs; ++i) {
                QoreValue arg = readValue(ptr, end, error);
                if (!error.empty()) {
                    for (auto& v : args) v.discard(nullptr);
                    return QoreValue();
                }
                args.push_back(arg);
            }
            // Build a QoreParseListNode from the args (empty when nargs == 0).
            QoreParseListNode* parse_args = nullptr;
            if (nargs > 0) {
                parse_args = new QoreParseListNode(&loc_builtin);
                for (auto& v : args) {
                    parse_args->add(v, &loc_builtin);
                }
                args.clear();
            }
            if (kind == 2) {
                // Hashdecl: type_path is a namespace path to a hashdecl
                QoreProgram* pgm = getProgram();
                const QoreNamespace* pns = nullptr;
                const TypedHashDecl* hd = pgm ? pgm->findHashDecl(type_path, pns) : nullptr;
                if (!hd) {
                    printd(0, "AOT readValue VT_NEW_COMPLEX_DEFAULT: cannot resolve hashdecl '%s'\n",
                        type_path);
                    if (parse_args) {
                        parse_args->deref(nullptr);
                    }
                    return QoreValue();
                }
                NewHashDeclNode* nhd = new NewHashDeclNode(&loc_builtin, hd, parse_args, false);
                return QoreValue(nhd);
            }
            // kind 0 or 1: resolve complex list/hash type. Use the AOT
            // resolver so nested sibling class/hashdecl refs can remain
            // deferred until the linked program has all metadata loaded.
            QoreAOTTypeResolver type_resolver(getProgram());
            std::string type_error;
            const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
            if (!ti || !type_error.empty()) {
                printd(0, "AOT readValue VT_NEW_COMPLEX_DEFAULT: cannot resolve type '%s' (kind=%d)\n",
                    type_path, (int)kind);
                if (parse_args) {
                    parse_args->deref(nullptr);
                }
                return QoreValue();
            }
            if (kind == 0) {
                // NewComplexListNode stores args as a single QoreValue that's
                // either NOTHING or a list of args. Mirror what the parser
                // does in parseInitComplexListInitialization() for the empty
                // case: just pass NOTHING, which evaluates to an empty list.
                QoreValue list_args;
                if (parse_args) {
                    list_args = QoreValue(parse_args);
                }
                NewComplexListNode* ncl = new NewComplexListNode(&loc_builtin, ti, list_args);
                return QoreValue(ncl);
            }
            if (kind == 3) {
                QoreValue buffer_args;
                if (parse_args) {
                    buffer_args = QoreValue(parse_args);
                }
                NewComplexBufferNode* ncb = new NewComplexBufferNode(&loc_builtin, ti, buffer_args,
                    buffer_init_kind);
                return QoreValue(ncb);
            }
            // kind == 1: complex hash
            NewComplexHashNode* nch = new NewComplexHashNode(&loc_builtin, ti, parse_args);
            return QoreValue(nch);
        }

        case QoreAOTValueTag::VT_CONST_REF: {
            // Written as: FQN string (length + string pool offset).
            // At load time, resolve the referenced constant from the current
            // program's namespace tree.  Two return modes:
            //
            //   1. `wrap_const_ref_in_rcr == true`: return a fresh
            //      RuntimeConstantRefNode wrapping the ConstantEntry.  Used
            //      when the caller needs the *lazy-eval* semantics of the
            //      original AST — crucially, for hashdecl member defaults
            //      like `hash<string, hash<MapperRuntimeKeyInfo>> mapper_keys =
            //      Mapper::MapperKeyInfo;` where `Mapper::MapperKeyInfo`'s
            //      type is `hash<auto>` and naïvely folding its value into
            //      the typed member at parse-time fails the narrowing.
            //      Wrapping in RCR defers the evaluation to runtime, matching
            //      what source-parse does for the same declaration.
            //
            //   2. otherwise: return the referenced value directly.  Used
            //      for objects and other unserializable values that live
            //      inside parse-time-folded hash/list literals — the parser
            //      inlines the constant's value into the literal, and the
            //      writer detects the shared node pointer via the program
            //      reverse map.
            if (ptr + 8 > end) {
                error = "unexpected end of data reading const_ref name";
                return QoreValue();
            }
            uint32_t name_len = readU32(ptr);
            uint32_t name_offset = readU32(ptr);
            const char* fqn = getString(name_offset);
            if (!fqn) {
                error = "invalid string offset for const_ref name";
                return QoreValue();
            }
            QoreProgram* pgm = getProgram();
            if (!pgm) {
                error = "no current program for const_ref resolution";
                return QoreValue();
            }
            bool resolved = false;
            QoreValue rv = qore_aot_resolve_constant_path_value(pgm, fqn,
                defer_unresolved_const_refs, wrap_const_ref_in_rcr, &resolved);
            if (!resolved) {
                error = std::string("cannot resolve const_ref '") + fqn
                    + "' in the current program; if this reference came from qcc --stub, "
                    "the runtime host must inject the external constant before loading the AOT binary";
                return QoreValue();
            }
            return rv;
        }

        case QoreAOTValueTag::VT_EXPR_TREE: {
            if (ptr + 4 > end) {
                error = "unexpected end of data reading expr_tree size";
                return QoreValue();
            }
            uint32_t blob_size = QoreAOTBinaryReader::readU32(ptr);
            if (ptr + blob_size > end) {
                error = "expr_tree blob exceeds section bounds";
                return QoreValue();
            }
            const uint8_t* blob = ptr;
            ptr += blob_size;
            QoreValue rv = deserializeExprTreeFromBlob(
                blob, blob_size, getProgram(), nullptr, 0);
            return rv;
        }

        case QoreAOTValueTag::VT_EXPR_NATIVE: {
            if (ptr + 4 > end) {
                error = "unexpected end of data reading native expr size";
                return QoreValue();
            }
            uint32_t blob_size = QoreAOTBinaryReader::readU32(ptr);
            if (ptr + blob_size > end) {
                error = "native expr blob exceeds section bounds";
                return QoreValue();
            }
            const uint8_t* blob = ptr;
            const uint8_t* blob_end = ptr + blob_size;
            ptr = blob_end;
            const uint8_t* ep = blob;
            QoreValue rv = readOneExpr(*this, ep, blob_end, error, getProgram(),
                nullptr, 0, nullptr, 0);
            if (error.empty() && ep != blob_end) {
                error = "native expr default did not consume its payload";
                rv.discard(nullptr);
                return QoreValue();
            }
            return rv;
        }

        default:
            error = "unknown value tag: " + std::to_string(static_cast<int>(tag))
                + " at offset " + std::to_string(ptr - 1 - end);
            return QoreValue();
    }
}

// ---- QoreAOTTypeResolver ----

//! Static lookup table for builtin type path strings → QoreTypeInfo*
/** This provides a fast path for the most common type resolutions.
    The map keys are the strings returned by QoreTypeInfo::getPath() for builtin types.
*/
struct BuiltinTypeEntry {
    const char* name;
    const QoreTypeInfo** type_ptr;
};

// NOTE: this table must stay in sync with QoreTypeInfo type path strings
static const BuiltinTypeEntry builtin_types[] = {
    {"int",             &bigIntTypeInfo},
    {"float",           &floatTypeInfo},
    {"number",          &numberTypeInfo},
    {"string",          &stringTypeInfo},
    {"char",            &charTypeInfo},
    {"bool",            &boolTypeInfo},
    {"date",            &dateTypeInfo},
    {"binary",          &binaryTypeInfo},
    {"hash",            &hashTypeInfo},
    {"list",            &listTypeInfo},
    {"buffer",          &bufferTypeInfo},
    {"object",          &objectTypeInfo},
    {"nothing",         &nothingTypeInfo},
    {"null",            &nullTypeInfo},
    {"auto",            &autoTypeInfo},
    {"auto!",           &autoNoNarrowTypeInfo},
    {"any",             &anyTypeInfo},
    {"data",            &dataTypeInfo},
    {"code",            &codeTypeInfo},
    {"reference",       &referenceTypeInfo},
    {"timeout",         &timeoutTypeInfo},
    {"softint",         &softBigIntTypeInfo},
    {"softfloat",       &softFloatTypeInfo},
    {"softnumber",      &softNumberTypeInfo},
    {"softstring",      &softStringTypeInfo},
    {"softchar",        &softCharTypeInfo},
    {"softbool",        &softBoolTypeInfo},
    {"softdate",        &softDateTypeInfo},
    {"softlist",        &softListTypeInfo},
    {"*int",            &bigIntOrNothingTypeInfo},
    {"*float",          &floatOrNothingTypeInfo},
    {"*number",         &numberOrNothingTypeInfo},
    {"*string",         &stringOrNothingTypeInfo},
    {"*char",           &charOrNothingTypeInfo},
    {"*bool",           &boolOrNothingTypeInfo},
    {"*date",           &dateOrNothingTypeInfo},
    {"*binary",         &binaryOrNothingTypeInfo},
    {"*hash",           &hashOrNothingTypeInfo},
    {"*list",           &listOrNothingTypeInfo},
    {"*buffer",         &bufferOrNothingTypeInfo},
    {"*object",         &objectOrNothingTypeInfo},
    {"*data",           &dataOrNothingTypeInfo},
    {"*code",           &codeOrNothingTypeInfo},
    {"*reference",      &referenceOrNothingTypeInfo},
    {"*timeout",        &timeoutOrNothingTypeInfo},
    {"*softint",        &softBigIntOrNothingTypeInfo},
    {"*softfloat",      &softFloatOrNothingTypeInfo},
    {"*softnumber",     &softNumberOrNothingTypeInfo},
    {"*softstring",     &softStringOrNothingTypeInfo},
    {"*softchar",       &softCharOrNothingTypeInfo},
    {"*softbool",       &softBoolOrNothingTypeInfo},
    {"*softdate",       &softDateOrNothingTypeInfo},
    {"*softlist",       &softListOrNothingTypeInfo},
    {"int|float",       &bigIntOrFloatTypeInfo},
    {"int|float|number", &bigIntFloatOrNumberTypeInfo},
    {"float|number",    &floatOrNumberTypeInfo},
    {"auto list",       &autoListTypeInfo},
    {"auto hash",       &autoHashTypeInfo},
    {"*auto list",      &autoListOrNothingTypeInfo},
    {"*auto hash",      &autoHashOrNothingTypeInfo},
    {"hash<auto!>",     &autoNoNarrowHashTypeInfo},
    {"*hash<auto!>",    &autoNoNarrowHashOrNothingTypeInfo},
    {"list<auto!>",     &autoNoNarrowListTypeInfo},
    {"*list<auto!>",    &autoNoNarrowListOrNothingTypeInfo},
    {"softauto list",   &softAutoListTypeInfo},
    {"*softauto list",  &softAutoListOrNothingTypeInfo},
    {nullptr, nullptr}
};

const QoreTypeInfo* QoreAOTTypeResolver::resolveBuiltin(const char* path) {
    for (const BuiltinTypeEntry* entry = builtin_types; entry->name; ++entry) {
        if (strcmp(path, entry->name) == 0) {
            return *entry->type_ptr;
        }
    }
    return nullptr;
}

const QoreTypeInfo* QoreAOTTypeResolver::resolveClassType(const char* path) {
    if (!pgm) {
        return nullptr;
    }
    const QoreClass* qc = qoreAOTResolveClassRefForDeserialization(pgm, path);
    if (qc) {
        return qc->getTypeInfo();
    }
    return nullptr;
}

static bool extract_aot_type_args(const char* path, const char* type_name, bool& or_nothing,
        std::vector<std::string>& args);

static bool split_aot_parameterized_type_use(const std::string& path, std::string& base_path,
        std::vector<std::string>& arg_paths);

const QoreTypeInfo* QoreAOTTypeResolver::resolveHashDeclType(const char* path) {
    if (!pgm) {
        return nullptr;
    }
    // path format: "hash<DeclName>" / "hash<DeclName<T>>" and or-nothing forms.
    // Resolve directly to the registered TypedHashDecl so anchored and unanchored
    // paths such as hash<::SqlUtil::QueryInfo> and hash<SqlUtil::QueryInfo> keep
    // one canonical QoreTypeInfo object.
    bool or_nothing = false;
    std::vector<std::string> hash_args;
    if (!extract_aot_type_args(path, "hash", or_nothing, hash_args) || hash_args.size() != 1) {
        return nullptr;
    }

    std::string decl_name = hash_args[0];
    if (decl_name == "auto" || decl_name == "auto!") {
        return nullptr;
    }

    std::string base_path;
    std::vector<std::string> type_arg_paths;
    if (split_aot_parameterized_type_use(decl_name, base_path, type_arg_paths)) {
        while (base_path.rfind("::", 0) == 0) {
            base_path.erase(0, 2);
        }

        const QoreNamespace* pns = nullptr;
        const TypedHashDecl* base_hd = pgm->findHashDecl(base_path.c_str(), pns);
        if (!base_hd) {
            return nullptr;
        }

        const typed_hash_decl_private* hp = typed_hash_decl_private::get(*base_hd);
        if (!hp->hasTypeParams()) {
            return nullptr;
        }
        size_t expected = hp->getTypeParamCount();
        size_t actual = type_arg_paths.size();
        if (actual < hp->getTypeParamRequiredCount() || actual > expected) {
            return nullptr;
        }

        type_vec_t type_args;
        type_args.reserve(expected);
        for (size_t i = 0; i < type_arg_paths.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT hashdecl type argument resolution")) {
                return nullptr;
            }
            std::string arg_error;
            const QoreTypeInfo* arg = resolve(type_arg_paths[i].c_str(), arg_error);
            if (!arg || !arg_error.empty()) {
                return nullptr;
            }
            type_args.push_back(arg);
        }
        for (size_t i = actual; i < expected; ++i) {
            std::string arg_error;
            const QoreTypeInfo* arg = resolve(hp->getTypeParamDefaultType(i), arg_error);
            if (!arg || !arg_error.empty()) {
                return nullptr;
            }
            type_args.push_back(arg);
        }
        return hp->getParameterizedTypeInfo(type_args, or_nothing);
    }

    while (decl_name.rfind("::", 0) == 0) {
        decl_name.erase(0, 2);
    }

    const QoreNamespace* pns = nullptr;
    const TypedHashDecl* thd = pgm->findHashDecl(decl_name.c_str(), pns);
    if (thd) {
        if (typed_hash_decl_private::get(*thd)->hasTypeParams()) {
            return nullptr;
        }
        return thd->getTypeInfo(or_nothing);
    }
    return nullptr;
}

const QoreTypeInfo* QoreAOTTypeResolver::resolveEnumType(const char* path) {
    if (!pgm) {
        return nullptr;
    }

    // path format: "enum<EnumName>" or "*enum<EnumName>". Resolve directly
    // through QoreProgram like hashdecls above so anchored paths emitted by
    // QoreTypeInfo::getPath() ("enum<::Ns::Enum>") do not go through the
    // generic parser resolver, which treats the leading "::" as an empty
    // namespace component during runtime lookup.
    bool or_nothing = false;
    const char* enum_path = path;
    if (!strncmp(path, "*enum<", 6)) {
        or_nothing = true;
        enum_path = path + 1;
    }
    if (strncmp(enum_path, "enum<", 5)) {
        return nullptr;
    }
    const char* start = enum_path + 5;
    const char* end = strchr(start, '>');
    if (!end || end[1]) {
        return nullptr;
    }
    std::string enum_name(start, end - start);
    while (enum_name.rfind("::", 0) == 0) {
        enum_name.erase(0, 2);
    }

    const QoreNamespace* pns = nullptr;
    const QoreEnumDecl* ed = pgm->findEnum(enum_name.c_str(), pns);
    if (ed) {
        return ed->getTypeInfo(or_nothing);
    }
    return nullptr;
}

//! Normalise a complex type path string for runtime parsing.
//!
//! QoreTypeInfo::getPath() for user hashdecls returns fully-qualified names
//! with a leading "::", e.g. "hash<::DataProvider::DataProviderExpressionInfo>".
//! When the parser-based runtime resolver walks this into a NamedScope, the
//! leading "::" becomes an empty first component, and
//! qore_root_ns_private::runtimeFindHashDeclIntern(NamedScope) iterates
//! nsmap looking for a namespace whose name is "" — which never matches, so
//! the hashdecl is not found and the outer cast fails with IR-CAST-ERROR.
//!
//! This normalizer strips any "::" prefix from identifier components inside
//! complex type paths (after "<" or ", "), which makes the nested hashdecl
//! lookup succeed. It is a string-level fix contained to the AOT resolver
//! path so shared runtime name-lookup code remains unchanged.
static std::string normalize_aot_type_path(const char* path) {
    std::string out;
    if (!path) {
        return out;
    }
    out.reserve(strlen(path));
    size_t i = 0;
    size_t n = strlen(path);
    // Handle a possible leading "*::" / "::" on the outer type path.
    if (n >= 2 && path[0] == ':' && path[1] == ':') {
        i += 2;
    } else if (n >= 3 && path[0] == '*' && path[1] == ':' && path[2] == ':') {
        out.push_back('*');
        i += 3;
    }
    while (i < n) {
        char c = path[i];
        out.push_back(c);
        ++i;
        // After "<" or "," (possibly followed by space), strip a "::" prefix.
        bool at_arg_start = (c == '<');
        if (c == ',' && i < n && path[i] == ' ') {
            out.push_back(' ');
            ++i;
            at_arg_start = true;
        } else if (c == ',') {
            at_arg_start = true;
        }
        if (at_arg_start && i + 1 < n && path[i] == ':' && path[i + 1] == ':') {
            i += 2;
        }
    }
    return out;
}

static bool extract_aot_single_type_arg(const char* path, const char* type_name, bool& or_nothing,
        std::string& arg) {
    or_nothing = false;
    const char* p = path;
    if (*p == '*') {
        or_nothing = true;
        ++p;
    }

    size_t type_len = strlen(type_name);
    if (strncmp(p, type_name, type_len) || p[type_len] != '<') {
        return false;
    }

    const char* start = p + type_len + 1;
    int depth = 1;
    for (const char* q = start; *q; ++q) {
        if (*q == '<') {
            ++depth;
        } else if (*q == '>' && --depth == 0) {
            if (q[1]) {
                return false;
            }
            arg.assign(start, q - start);
            return !arg.empty();
        }
    }
    return false;
}

static std::string trim_aot_type_component(const std::string& str);

static bool split_aot_type_args(const std::string& args, std::vector<std::string>& out) {
    out.clear();
    if (trim_aot_type_component(args).empty()) {
        return true;
    }
    std::string current;
    int angle_depth = 0;
    int paren_depth = 0;
    for (char c : args) {
        if (c == '<' && paren_depth == 0) {
            ++angle_depth;
        } else if (c == '>' && paren_depth == 0 && angle_depth > 0) {
            --angle_depth;
        } else if (c == '(') {
            ++paren_depth;
        } else if (c == ')' && paren_depth > 0) {
            --paren_depth;
        }

        if (c == ',' && angle_depth == 0 && paren_depth == 0) {
            out.push_back(trim_aot_type_component(current));
            current.clear();
            continue;
        }
        current.push_back(c);
    }

    out.push_back(trim_aot_type_component(current));
    for (const std::string& arg : out) {
        if (arg.empty()) {
            out.clear();
            return false;
        }
    }
    return true;
}

static bool extract_aot_type_args(const char* path, const char* type_name, bool& or_nothing,
        std::vector<std::string>& args) {
    or_nothing = false;
    args.clear();
    const char* p = path;
    if (*p == '*') {
        or_nothing = true;
        ++p;
    }

    size_t type_len = strlen(type_name);
    if (strncmp(p, type_name, type_len) || p[type_len] != '<') {
        return false;
    }

    const char* start = p + type_len + 1;
    int depth = 1;
    for (const char* q = start; *q; ++q) {
        if (*q == '<') {
            ++depth;
        } else if (*q == '>' && --depth == 0) {
            if (q[1]) {
                return false;
            }
            return split_aot_type_args(std::string(start, q - start), args);
        }
    }
    return false;
}

static std::string trim_aot_type_component(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return std::string();
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static bool split_aot_union_shorthand(const char* path, bool& or_nothing, std::vector<std::string>& members) {
    or_nothing = false;
    members.clear();

    const char* p = path;
    if (*p == '*') {
        or_nothing = true;
        ++p;
    }

    std::string current;
    int angle_depth = 0;
    int paren_depth = 0;
    bool found_union_separator = false;
    for (; *p; ++p) {
        char c = *p;
        if (c == '<' && paren_depth == 0) {
            ++angle_depth;
        } else if (c == '>' && paren_depth == 0 && angle_depth > 0) {
            --angle_depth;
        } else if (c == '(') {
            ++paren_depth;
        } else if (c == ')' && paren_depth > 0) {
            --paren_depth;
        }

        if (c == '|' && angle_depth == 0 && paren_depth == 0) {
            members.push_back(trim_aot_type_component(current));
            current.clear();
            found_union_separator = true;
            continue;
        }
        current.push_back(c);
    }

    if (!found_union_separator) {
        members.clear();
        return false;
    }

    members.push_back(trim_aot_type_component(current));
    for (const std::string& member : members) {
        if (member.empty()) {
            return false;
        }
    }
    return true;
}

static bool split_aot_parameterized_type_use(const std::string& path, std::string& base_path,
        std::vector<std::string>& arg_paths) {
    base_path.clear();
    arg_paths.clear();

    std::string trimmed = trim_aot_type_component(path);
    if (trimmed.empty()) {
        return false;
    }

    size_t open = std::string::npos;
    int paren_depth = 0;
    for (size_t i = 0; i < trimmed.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT parameterized type path parsing")) {
            return false;
        }
        char c = trimmed[i];
        if (c == '(') {
            ++paren_depth;
        } else if (c == ')' && paren_depth > 0) {
            --paren_depth;
        } else if (c == '<' && paren_depth == 0) {
            open = i;
            break;
        }
    }
    if (open == std::string::npos) {
        return false;
    }

    int depth = 1;
    size_t close = std::string::npos;
    for (size_t i = open + 1; i < trimmed.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT parameterized type argument parsing")) {
            return false;
        }
        char c = trimmed[i];
        if (c == '<') {
            ++depth;
        } else if (c == '>' && --depth == 0) {
            close = i;
            break;
        }
    }
    if (close == std::string::npos || close + 1 != trimmed.size()) {
        return false;
    }

    base_path = trim_aot_type_component(trimmed.substr(0, open));
    if (base_path.empty()) {
        return false;
    }
    return split_aot_type_args(trimmed.substr(open + 1, close - open - 1), arg_paths);
}

static bool split_aot_code_signature(const std::string& sig, std::string& return_path,
        std::vector<std::string>& param_paths, bool& varargs) {
    return_path.clear();
    param_paths.clear();
    varargs = false;

    std::string trimmed = trim_aot_type_component(sig);
    int angle_depth = 0;
    size_t open = std::string::npos;
    for (size_t i = 0; i < trimmed.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT code type signature parsing")) {
            return false;
        }
        char c = trimmed[i];
        if (c == '<') {
            ++angle_depth;
        } else if (c == '>' && angle_depth > 0) {
            --angle_depth;
        } else if (c == '(' && angle_depth == 0) {
            open = i;
            break;
        }
    }
    if (open == std::string::npos) {
        return false;
    }

    int paren_depth = 1;
    size_t close = std::string::npos;
    for (size_t i = open + 1; i < trimmed.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT code type parameter parsing")) {
            return false;
        }
        char c = trimmed[i];
        if (c == '(') {
            ++paren_depth;
        } else if (c == ')' && --paren_depth == 0) {
            close = i;
            break;
        }
    }
    if (close == std::string::npos || close + 1 != trimmed.size()) {
        return false;
    }

    return_path = trim_aot_type_component(trimmed.substr(0, open));
    if (return_path.empty()) {
        return false;
    }

    std::string params = trim_aot_type_component(trimmed.substr(open + 1, close - open - 1));
    if (params.empty()) {
        return true;
    }

    std::vector<std::string> parts;
    if (!split_aot_type_args(params, parts)) {
        return false;
    }
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT code type parameter list parsing")) {
            return false;
        }
        if (parts[i] == "...") {
            if (i + 1 != parts.size()) {
                return false;
            }
            varargs = true;
            continue;
        }
        param_paths.push_back(parts[i]);
    }
    return true;
}

const QoreTypeInfo* QoreAOTTypeResolver::resolveTypeParameterType(const char* path) {
    if (!pgm) {
        return nullptr;
    }

    bool or_nothing = false;
    std::vector<std::string> args;
    if (extract_aot_type_args(path, "hashdecl_typeparam", or_nothing, args)) {
        if (args.size() != 3) {
            return nullptr;
        }

        std::string hashdecl_path = args[0];
        while (hashdecl_path.rfind("::", 0) == 0) {
            hashdecl_path.erase(0, 2);
        }

        const QoreNamespace* pns = nullptr;
        const TypedHashDecl* hd = pgm->findHashDecl(hashdecl_path.c_str(), pns);
        if (!hd) {
            return nullptr;
        }

        const typed_hash_decl_private* hp = typed_hash_decl_private::get(*hd);
        if (!hp->hasTypeParams()) {
            return nullptr;
        }

        errno = 0;
        char* end = nullptr;
        unsigned long long raw_index = strtoull(args[1].c_str(), &end, 10);
        if (errno || !end || *end || raw_index >= hp->getTypeParamCount()) {
            return nullptr;
        }

        size_t index = static_cast<size_t>(raw_index);
        const char* param_name = hp->getTypeParamName(index);
        if (!param_name || args[2] != param_name) {
            return nullptr;
        }

        return qore_get_hashdecl_type_parameter_type(hd, index, param_name, or_nothing);
    }

    if (!extract_aot_type_args(path, "typeparam", or_nothing, args) || args.size() != 3) {
        return nullptr;
    }

    const QoreClass* qc = qoreAOTResolveClassRefForDeserialization(pgm, args[0].c_str());
    if (!qc || !qc->hasTypeParameters()) {
        return nullptr;
    }

    errno = 0;
    char* end = nullptr;
    unsigned long long raw_index = strtoull(args[1].c_str(), &end, 10);
    if (errno || !end || *end || raw_index >= qc->getTypeParameterCount()) {
        return nullptr;
    }

    size_t index = static_cast<size_t>(raw_index);
    const char* param_name = qc->getTypeParameterName(index);
    if (!param_name || args[2] != param_name) {
        return nullptr;
    }

    return qore_get_type_parameter_type(qc, index, param_name, or_nothing);
}

const QoreTypeInfo* QoreAOTTypeResolver::resolveUnionShorthandType(const char* path) {
    bool or_nothing = false;
    std::vector<std::string> member_paths;
    if (!split_aot_union_shorthand(path, or_nothing, member_paths)) {
        return nullptr;
    }

    type_vec_t member_types;
    member_types.reserve(member_paths.size());
    for (const std::string& member_path : member_paths) {
        std::string member_error;
        const QoreTypeInfo* member = resolve(member_path.c_str(), member_error);
        if (!member) {
            return nullptr;
        }
        if (member == autoTypeInfo) {
            return autoTypeInfo;
        }
        member_types.push_back(member);
    }

    return qore_get_union_type(member_types, or_nothing);
}

const QoreTypeInfo* QoreAOTTypeResolver::resolveStructuredComplexType(const char* path) {
    bool or_nothing = false;
    std::vector<std::string> args;

    if (extract_aot_type_args(path, "object", or_nothing, args)) {
        if (args.size() != 1) {
            return nullptr;
        }
        if (args[0] == "auto") {
            return or_nothing ? objectOrNothingTypeInfo : objectTypeInfo;
        }
        std::string param_class_path;
        std::vector<std::string> type_arg_paths;
        if (split_aot_parameterized_type_use(args[0], param_class_path, type_arg_paths)) {
            param_class_path = normalize_aot_type_path(param_class_path.c_str());
            const QoreClass* qc = qoreAOTResolveClassRefForDeserialization(pgm, param_class_path.c_str());
            if (!qc || !qc->hasTypeParameters()) {
                return nullptr;
            }
            size_t expected = qc->getTypeParameterCount();
            size_t actual = type_arg_paths.size();
            if (actual < qc->getTypeParameterRequiredCount() || actual > expected) {
                return nullptr;
            }
            type_vec_t type_args;
            type_args.reserve(expected);
            for (size_t i = 0; i < type_arg_paths.size(); ++i) {
                if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT parameterized type argument resolution")) {
                    return nullptr;
                }
                const std::string& type_arg_path = type_arg_paths[i];
                std::string arg_error;
                const QoreTypeInfo* type_arg = resolve(type_arg_path.c_str(), arg_error);
                if (!type_arg || !arg_error.empty()) {
                    return nullptr;
                }
                type_args.push_back(type_arg);
            }
            for (size_t i = actual; i < expected; ++i) {
                std::string arg_error;
                const QoreTypeInfo* type_arg = resolve(qc->getTypeParameterDefaultType(i), arg_error);
                if (!type_arg || !arg_error.empty()) {
                    return nullptr;
                }
                type_args.push_back(type_arg);
            }
            return qc->getTypeInfo(type_args, or_nothing);
        }
        std::string class_path = normalize_aot_type_path(args[0].c_str());
        const QoreClass* qc = qoreAOTResolveClassRefForDeserialization(pgm, class_path.c_str());
        if (!qc) {
            return nullptr;
        }
        return or_nothing ? qc->getOrNothingTypeInfo() : qc->getTypeInfo();
    }

    if (extract_aot_type_args(path, "hash", or_nothing, args)) {
        if (args.size() == 1) {
            if (args[0] == "auto") {
                return or_nothing ? autoHashOrNothingTypeInfo : autoHashTypeInfo;
            }
            if (args[0] == "auto!") {
                return or_nothing ? autoNoNarrowHashOrNothingTypeInfo : autoNoNarrowHashTypeInfo;
            }
            std::string hashdecl_path = or_nothing ? "*hash<" : "hash<";
            hashdecl_path += args[0];
            hashdecl_path += '>';
            return resolveHashDeclType(hashdecl_path.c_str());
        }
        if (args.size() != 2 || args[0] != "string") {
            return nullptr;
        }
        if (args[1] == "auto") {
            return or_nothing ? autoHashOrNothingTypeInfo : autoHashTypeInfo;
        }
        if (args[1] == "auto!") {
            return or_nothing ? autoNoNarrowHashOrNothingTypeInfo : autoNoNarrowHashTypeInfo;
        }
        std::string error;
        const QoreTypeInfo* value_type = resolve(args[1].c_str(), error);
        if (!QoreTypeInfo::hasType(value_type)) {
            return nullptr;
        }
        return or_nothing
            ? qore_get_complex_hash_or_nothing_type(value_type)
            : qore_get_complex_hash_type(value_type);
    }

    if (extract_aot_type_args(path, "list", or_nothing, args)) {
        if (args.size() != 1) {
            return nullptr;
        }
        if (args[0] == "auto") {
            return or_nothing ? autoListOrNothingTypeInfo : autoListTypeInfo;
        }
        if (args[0] == "auto!") {
            return or_nothing ? autoNoNarrowListOrNothingTypeInfo : autoNoNarrowListTypeInfo;
        }
        std::string error;
        const QoreTypeInfo* value_type = resolve(args[0].c_str(), error);
        if (!QoreTypeInfo::hasType(value_type)) {
            return nullptr;
        }
        return or_nothing
            ? qore_get_complex_list_or_nothing_type(value_type)
            : qore_get_complex_list_type(value_type);
    }

    if (extract_aot_type_args(path, "buffer", or_nothing, args)) {
        if (args.size() != 1) {
            return nullptr;
        }
        bool nullable_elements = false;
        std::string element_name = args[0];
        if (!element_name.empty() && element_name[0] == '*') {
            nullable_elements = true;
            element_name.erase(0, 1);
        }
        QoreBufferElementType element_type = QoreBufferElementType::Invalid;
        if (!qore_buffer_element_type_from_name(element_name.c_str(), element_type)) {
            return nullptr;
        }
        return or_nothing
            ? qore_get_complex_buffer_or_nothing_type(element_type, nullable_elements)
            : qore_get_complex_buffer_type(element_type, nullable_elements);
    }

    if (extract_aot_type_args(path, "softlist", or_nothing, args)) {
        if (args.size() != 1) {
            return nullptr;
        }
        if (args[0] == "auto" || args[0] == "auto!") {
            return or_nothing ? softAutoListOrNothingTypeInfo : softAutoListTypeInfo;
        }
        std::string error;
        const QoreTypeInfo* value_type = resolve(args[0].c_str(), error);
        if (!QoreTypeInfo::hasType(value_type)) {
            return nullptr;
        }
        return or_nothing
            ? qore_get_complex_softlist_or_nothing_type(value_type)
            : qore_get_complex_softlist_type(value_type);
    }

    if (extract_aot_type_args(path, "reference", or_nothing, args)) {
        if (args.size() != 1) {
            return nullptr;
        }
        if (args[0] == "auto" || args[0] == "auto!") {
            return or_nothing ? referenceOrNothingTypeInfo : referenceTypeInfo;
        }
        std::string error;
        const QoreTypeInfo* value_type = resolve(args[0].c_str(), error);
        if (!QoreTypeInfo::hasType(value_type)) {
            return nullptr;
        }
        return or_nothing
            ? qore_get_complex_reference_or_nothing_type(value_type)
            : qore_get_complex_reference_type(value_type);
    }

    if (extract_aot_type_args(path, "code", or_nothing, args)) {
        if (args.size() != 1) {
            return nullptr;
        }
        std::string return_path;
        std::vector<std::string> param_paths;
        bool varargs = false;
        if (!split_aot_code_signature(args[0], return_path, param_paths, varargs)) {
            return nullptr;
        }

        const QoreTypeInfo* return_type = nullptr;
        if (return_path != "nothing") {
            std::string error;
            return_type = resolve(return_path.c_str(), error);
            if (!return_type || !error.empty()) {
                return nullptr;
            }
        }

        type_vec_t param_types;
        param_types.reserve(param_paths.size());
        for (size_t i = 0; i < param_paths.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT code type parameter resolution")) {
                return nullptr;
            }
            const std::string& param_path = param_paths[i];
            std::string error;
            const QoreTypeInfo* param_type = resolve(param_path.c_str(), error);
            if (!param_type || !error.empty()) {
                return nullptr;
            }
            param_types.push_back(param_type);
        }
        return qore_get_complex_code_type(return_type, param_types, varargs, or_nothing);
    }

    if (extract_aot_type_args(path, "union", or_nothing, args)) {
        if (args.empty() || args.size() > QORE_MAX_UNION_MEMBERS) {
            return nullptr;
        }
        type_vec_t member_types;
        member_types.reserve(args.size());
        for (const std::string& arg : args) {
            if (arg == "auto") {
                return autoTypeInfo;
            }
            std::string error;
            const QoreTypeInfo* member = resolve(arg.c_str(), error);
            if (!QoreTypeInfo::hasType(member)) {
                return nullptr;
            }
            member_types.push_back(member);
        }
        return qore_get_union_type(member_types, or_nothing);
    }

    return nullptr;
}

const QoreTypeInfo* QoreAOTTypeResolver::resolveComplexType(const char* path) {
    if (const QoreTypeInfo* ti = resolveStructuredComplexType(path)) {
        return ti;
    }

    bool or_nothing = false;
    std::string inner_type;
    if (extract_aot_single_type_arg(path, "reference", or_nothing, inner_type)) {
        std::string error;
        const QoreTypeInfo* value_type = resolve(inner_type.c_str(), error);
        if (QoreTypeInfo::hasType(value_type)) {
            return or_nothing
                ? qore_get_complex_reference_or_nothing_type(value_type)
                : qore_get_complex_reference_type(value_type);
        }
    }

    // Handle object<ClassName> patterns directly by looking up the class
    // in the namespace tree. This works even before rebuildAllIndexes() is called.
    if (strncmp(path, "object<", 7) == 0) {
        const char* start = path + 7;
        const char* end = strrchr(start, '>');
        if (end && end > start) {
            std::string class_path(start, end - start);
            const QoreClass* qc = qoreAOTResolveClassRefForDeserialization(pgm, class_path.c_str());
            if (qc) {
                return qc->getTypeInfo();
            }
            return qore_get_aot_deferred_type_info(nullptr, class_path.c_str(), false, false);
        }
    }

    // Handle *object<ClassName> patterns (or-nothing class types)
    if (strncmp(path, "*object<", 8) == 0) {
        const char* start = path + 8;
        const char* end = strrchr(start, '>');
        if (end && end > start) {
            std::string class_path(start, end - start);
            const QoreClass* qc = qoreAOTResolveClassRefForDeserialization(pgm, class_path.c_str());
            if (qc) {
                return qc->getOrNothingTypeInfo();
            }
            return qore_get_aot_deferred_type_info(nullptr, class_path.c_str(), true, false);
        }
    }

    std::vector<std::string> hash_args;
    bool hash_or_nothing = false;
    if (extract_aot_type_args(path, "hash", hash_or_nothing, hash_args)
            && hash_args.size() == 1 && hash_args[0] != "auto" && hash_args[0] != "auto!") {
        return qore_get_aot_deferred_type_info(nullptr, hash_args[0].c_str(), hash_or_nothing, true);
    }

    // Use the existing parser infrastructure to resolve complex type strings
    // qore_get_type_from_string_intern() handles: list<T>, hash<T>, *T, reference<T>, etc.
    // We need to set up the program context so that class lookups like object<ClassName>
    // can find classes defined in the program's namespace tree.
    // Normalize the path to strip leading "::" on nested hashdecl/enum refs
    // (see the normaliser comment for the rationale).
    std::string norm_path = normalize_aot_type_path(path);
    const char* use_path = norm_path.c_str();
    if (pgm) {
        ExceptionSink xsink;
        ProgramRuntimeParseAccessHelper pah(&xsink, pgm);
        if (!xsink) {
            return qore_get_type_from_string_intern(use_path);
        }
    }
    return qore_get_type_from_string_intern(use_path);
}

const QoreTypeInfo* QoreAOTTypeResolver::resolve(const char* path, std::string& error) {
    if (!path || !*path) {
        return nullptr;  // null/empty = no type constraint (auto)
    }

    // Check cache first (may be owned or shared across sibling sessions)
    auto it = cache_ptr->find(path);
    if (it != cache_ptr->end()) {
        return it->second;
    }

    // Try builtin types (fast path)
    const QoreTypeInfo* result = resolveBuiltin(path);

    // Hashdecl type paths must resolve to the canonical TypedHashDecl type object.
    if (!result) {
        result = resolveHashDeclType(path);
    }

    if (!result) {
        result = resolveEnumType(path);
    }

    if (!result) {
        result = resolveTypeParameterType(path);
    }

    // QoreTypeInfo::getPath() can emit compact top-level union spellings such
    // as "int|float|number"; keep that canonical spelling round-trippable.
    if (!result) {
        result = resolveUnionShorthandType(path);
    }

    // Try the parser-based resolver for complex types (handles everything)
    if (!result) {
        result = resolveComplexType(path);
    }

    if (result) {
        (*cache_ptr)[path] = result;
        return result;
    }

    error = "cannot resolve type path: " + std::string(path);
    return nullptr;
}

// ---- Namespace Serialization (Phase 3) ----

namespace {

//! Get a serializable type path string from QoreTypeInfo, preserving no-narrow markers recursively.
static std::string getTypePath(const QoreTypeInfo* ti, bool no_narrow = false) {
    return getAOTSerializableTypePath(ti, no_narrow);
}

static uint32_t internTypePath(QoreAOTBinaryWriter& writer, const QoreTypeInfo* ti, bool no_narrow = false) {
    std::string path = getTypePath(ti, no_narrow);
    return writer.internTypePath(path.c_str());
}

static void writeTypePathRef(QoreAOTBinaryWriter& writer, const QoreTypeInfo* ti, bool no_narrow = false) {
    std::string path = getTypePath(ti, no_narrow);
    writer.writeStringRef(path.c_str());
}

static std::string getNamespaceConstantPath(const qore_ns_private* ns, const char* name) {
    std::string ns_path = ns ? ns->path : "";
    if (ns_path.size() >= 2) {
        ns_path = ns_path.substr(2);
    }
    return ns_path.empty() ? std::string(name ? name : "") : ns_path + "::" + (name ? name : "");
}

static std::string getClassConstantPath(const qore_class_private* priv, const char* name) {
    std::string class_path = priv ? priv->path : "";
    if (class_path.size() >= 2) {
        class_path = class_path.substr(2);
    }
    return class_path.empty() ? std::string(name ? name : "") : class_path + "::" + (name ? name : "");
}

//! Internal state for collecting namespace items during serialization
struct AOTSerializeState {
    qore_ns_private* root_ns = nullptr;  // program root namespace (for program-wide CRM)

    struct NSInfo {
        qore_ns_private* ns;
        uint32_t parent_idx;
    };
    std::vector<NSInfo> namespaces;

    struct ClassInfo {
        QoreClass* cls;
        qore_class_private* priv;
        uint32_t ns_idx;
    };
    std::vector<ClassInfo> classes;

    struct HashDeclInfo {
        const TypedHashDecl* hd;
        uint32_t ns_idx;
    };
    std::vector<HashDeclInfo> hashdecls;

    struct EnumInfo {
        const QoreEnumDecl* ed;
        uint32_t ns_idx;
    };
    std::vector<EnumInfo> enums;

    struct TypedefInfo {
        std::string name;
        const QoreTypeInfo* typeInfo;
        bool pub;
        uint32_t ns_idx;
        const QoreProgramLocation* loc;
    };
    std::vector<TypedefInfo> typedefs;

    struct ConstInfo {
        const ConstantEntry* entry;
        uint32_t ns_idx;
    };
    std::vector<ConstInfo> constants;

    struct GlobalInfo {
        Var* var;
        uint32_t ns_idx;
    };
    std::vector<GlobalInfo> globals;

    struct FuncInfo {
        FunctionEntry* entry;
        QoreFunction* func;
        uint32_t ns_idx;
    };
    std::vector<FuncInfo> functions;

    struct MethodInfo {
        const QoreMethod* method;
        uint32_t class_idx;
        bool is_static;
    };
    std::vector<MethodInfo> methods;

    std::unordered_set<std::string> class_keys;
    std::unordered_set<std::string> hashdecl_keys;
    std::unordered_set<std::string> enum_keys;
    std::unordered_set<std::string> typedef_keys;
    std::unordered_set<std::string> constant_keys;
    std::unordered_set<std::string> global_keys;
    std::unordered_set<std::string> function_keys;
};

static std::string makeNamespaceItemKey(const qore_ns_private* ns, const char* name) {
    std::string key = ns && !ns->path.empty() ? ns->path : std::string();
    if (!key.empty() && name && *name) {
        key += "::";
    }
    if (name) {
        key += name;
    }
    return key;
}

//! Helper to check if an item should be skipped (from a different module than the one being compiled)
/** @param item_module the module name of the item (from getModuleName())
    @param current_module the module being compiled (nullptr means include all items)
    @return true if the item should be skipped, false otherwise
*/
static inline bool shouldSkipReexportedItem(const char* item_module, const char* current_module,
        const std::unordered_set<std::string>* keep_modules = nullptr) {
    // If no current module specified, include all items (non-strip-source mode)
    if (!current_module) {
        return false;
    }
    // If item has no module name, it matches the current module (or is script-local)
    if (!item_module) {
        return false;
    }
    // If item's module is in the keep set, don't skip it (e.g., local modules that
    // can't be loaded by name at runtime)
    if (keep_modules && keep_modules->count(item_module)) {
        return false;
    }
    // If item has a module and it differs from current module, skip it
    return strcmp(item_module, current_module) != 0;
}

//! Phase 4 slice 4: per-file filter helper — skip items whose AST
//! declaration location does not match the target source file.
static inline bool hasAOTBinaryCompileFileFilter(const char* compile_file,
        const std::unordered_set<std::string>* compile_files = nullptr) {
    return compile_file || (compile_files && !compile_files->empty());
}

static inline bool shouldSkipByCompileFile(const char* item_file, const char* compile_file,
        const std::unordered_set<std::string>* compile_files = nullptr) {
    if (!hasAOTBinaryCompileFileFilter(compile_file, compile_files)) {
        return false;
    }
    if (!item_file) {
        return false;  // conservative: include unattributed items
    }
    if (compile_file && !strcmp(item_file, compile_file)) {
        return false;
    }
    if (compile_files && compile_files->find(item_file) != compile_files->end()) {
        return false;
    }
    return true;
}

//! Returns true for user variants that should be emitted for this method.
/**
    Abstract-method resolution can install a concrete parent variant into a
    child class's local method map when a sibling base satisfies the abstract
    contract.  That variant remains owned by the parent QoreMethod; serializing
    it as a child method creates a source-stripped AOT variant with signature
    metadata but no executable body.  The child relationship is derived from
    the base hierarchy, so AOT must let deserialization rebuild it instead.
*/
static bool isAOTSerializableMethodVariant(const QoreMethod* method, const AbstractQoreFunctionVariant* variant) {
    if (!method || !variant || !variant->isUser()) {
        return false;
    }
    const MethodVariantBase* mvb = reinterpret_cast<const MethodVariantBase*>(variant);
    const QoreMethod* owner = mvb->method();
    if (owner == method) {
        return true;
    }
    const QoreClass* method_class = method->getClass();
    const QoreClass* owner_class = owner->getClass();
    return !(method_class && owner_class && method_class->getClass(owner_class->getID()));
}

//! Returns true for method variants that can be named in the symbol index.
static bool isAOTLinkableMethodVariant(const QoreMethod* method, const AbstractQoreFunctionVariant* variant) {
    return method && variant && (!variant->isUser() || isAOTSerializableMethodVariant(method, variant));
}

//! Collect function names that have native AOT slot maps in this binary.
static bool collectAOTSlotMapFunctionNames(const QoreAOTBinaryReader& reader,
        std::unordered_set<std::string>& slot_map_names, bool& found_section, std::string& error) {
    found_section = false;

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::SLOT_MAPS);
    if (!sec) {
        return true;
    }
    found_section = true;

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        return true;
    }
    const uint8_t* end = ptr + sec->size;
    if (ptr + sizeof(uint32_t) > end) {
        return true;
    }

    uint32_t num_funcs = QoreAOTBinaryReader::readU32(ptr);
    for (uint32_t i = 0; i < num_funcs && ptr + sizeof(uint32_t) <= end; ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT slot map name collection")) {
            error = "operation cancelled during AOT slot map name collection";
            return false;
        }
        const uint8_t* entry_start = ptr;
        uint32_t entry_size = QoreAOTBinaryReader::readU32(ptr);
        const uint8_t* entry_end = ptr + entry_size;
        if (entry_end < ptr || entry_end > end) {
            break;
        }
        if (const char* name = reader.readStringRef(ptr)) {
            if (*name) {
                slot_map_names.insert(name);
            }
        }
        ptr = entry_end;
        if (ptr <= entry_start) {
            break;
        }
    }
    return true;
}

static std::string getAOTMethodVariantKey(const QoreClass* qc, const char* method_name, bool is_static,
        const AbstractQoreFunctionVariant* variant) {
    std::string key;
    if (const char* class_path = qc ? qc->getPath() : nullptr) {
        if (class_path[0] == ':' && class_path[1] == ':') {
            class_path += 2;
        }
        key = class_path;
    }
    key += "::";
    if (is_static) {
        key += "_static_";
    }
    key += method_name ? method_name : "";
    return getVariantKey(key.c_str(), variant);
}

static MethodVariantBase* findInheritedConcreteMethodVariant(const BCList* scl, const QoreClass* origin_class,
        const char* method_name, MethodVariantBase* variant, std::unordered_set<const QoreClass*>& visited) {
    for (auto& i : *scl) {
        const QoreClass* nqc = (*i).sclass;
        if (!nqc || nqc == origin_class || !visited.insert(nqc).second) {
            continue;
        }

        qore_class_private* npriv = qore_class_private::get(*const_cast<QoreClass*>(nqc));
        QoreMethod* m = npriv->parseFindLocalMethod(method_name);
        if (m) {
            MethodFunctionBase* f = qore_method_private::get(*m)->getFunction();
            MethodVariantBase* ov = f->parseHasVariantWithSignature(variant, npriv->ahm.relaxed_match);
            if (ov && !ov->isAbstract()) {
                return ov;
            }
        }
        if (npriv->scl) {
            MethodVariantBase* ov = findInheritedConcreteMethodVariant(npriv->scl, origin_class, method_name,
                variant, visited);
            if (ov) {
                return ov;
            }
        }
    }
    return nullptr;
}

static bool hasInheritedConcreteMethodVariant(QoreClass* qc, const char* method_name, MethodVariantBase* variant) {
    if (!qc || !method_name || !*method_name || !variant) {
        return false;
    }
    qore_class_private* priv = qore_class_private::get(*qc);
    if (!priv->scl) {
        return false;
    }
    std::unordered_set<const QoreClass*> visited;
    MethodVariantBase* inherited = findInheritedConcreteMethodVariant(priv->scl, qc, method_name, variant, visited);
    return inherited && inherited != variant;
}

//! Recursively collect all user-defined items from the namespace tree
/** @param state the state object to collect items into
    @param ns the namespace to collect from
    @param parent_idx the parent namespace index
    @param current_module optional module name to filter items; when provided, only items from this
           module are collected (items from reexported dependencies are filtered out)
    @param keep_modules optional allow-list of module names
    @param compile_file optional per-file filter (Phase 4 slice 4); when
           provided, items whose AST declaration file doesn't match are
           skipped (used for per-file `.qo` metadata emission)
    @param compile_files optional multi-file filter used for aggregate
           script metadata
*/
static void collectItems(AOTSerializeState& state, qore_ns_private* ns, uint32_t parent_idx,
        const char* current_module, const std::unordered_set<std::string>* keep_modules = nullptr,
        const char* compile_file = nullptr,
        const std::unordered_set<std::string>* compile_files = nullptr) {
    uint32_t ns_idx = static_cast<uint32_t>(state.namespaces.size());
    state.namespaces.push_back({ns, parent_idx});

    // Collect user classes
    {
        ClassListIterator cli(ns->classList);
        while (cli.next()) {
            QoreClass* cls = cli.get();
            qore_class_private* priv = qore_class_private::get(*cls);
            if (!priv->sys) {
                // Filter out classes from reexported dependencies
                const char* class_module = priv->getModuleName();
                printd(5, "AOT serialize class '%s': module='%s' current_module='%s' skip=%d\n",
                    cls->getName(), class_module ? class_module : "n/a",
                    current_module ? current_module : "n/a",
                    shouldSkipReexportedItem(class_module, current_module, keep_modules));
                if (shouldSkipReexportedItem(class_module, current_module, keep_modules)) {
                    continue;
                }
                if (shouldSkipByCompileFile(priv->loc ? priv->loc->getFile() : nullptr,
                        compile_file, compile_files)) {
                    continue;
                }

                std::string class_key = priv->path.empty()
                    ? makeNamespaceItemKey(ns, cls->getName()) : priv->path;
                if (!state.class_keys.insert(class_key).second) {
                    continue;
                }

                uint32_t class_idx = static_cast<uint32_t>(state.classes.size());
                state.classes.push_back({cls, priv, ns_idx});

                // Collect user methods for this class
                for (auto& mi : priv->hm) {
                    if (mi.second->isUser()) {
                        state.methods.push_back({mi.second, class_idx, false});
                    }
                }
                for (auto& mi : priv->shm) {
                    if (mi.second->isUser()) {
                        state.methods.push_back({mi.second, class_idx, true});
                    }
                }
            }
        }
    }

    // Collect user hashdecls
    {
        HashDeclListIterator hdi(ns->hashDeclList);
        while (hdi.next()) {
            TypedHashDecl* hd = hdi.get();
            if (!hd->isSystem()) {
                // Filter out hashdecls from reexported dependencies
                const char* hd_module = typed_hash_decl_private::get(*hd)->getModuleName();
                if (shouldSkipReexportedItem(hd_module, current_module, keep_modules)) {
                    continue;
                }
                if (hasAOTBinaryCompileFileFilter(compile_file, compile_files)) {
                    const QoreProgramLocation* hd_loc =
                        typed_hash_decl_private::get(*hd)->getParseLocation();
                    if (shouldSkipByCompileFile(hd_loc ? hd_loc->getFile() : nullptr,
                            compile_file, compile_files)) {
                        continue;
                    }
                }
                std::string key = hd->getNamespacePath();
                if (key.empty()) {
                    key = makeNamespaceItemKey(ns, hdi.getName());
                }
                if (state.hashdecl_keys.insert(key).second) {
                    state.hashdecls.push_back({hd, ns_idx});
                }
            }
        }
    }

    // Collect user enums
    {
        EnumListIterator eli(ns->enumList);
        while (eli.next()) {
            QoreEnumDecl* ed = eli.get();
            if (!ed->isSystem()) {
                // Filter out enums from reexported dependencies
                const char* ed_module = qore_enum_decl_private::get(*ed)->getModuleName();
                if (shouldSkipReexportedItem(ed_module, current_module, keep_modules)) {
                    continue;
                }
                if (hasAOTBinaryCompileFileFilter(compile_file, compile_files)) {
                    const QoreProgramLocation* ed_loc =
                        qore_enum_decl_private::get(*ed)->getParseLocation();
                    if (shouldSkipByCompileFile(ed_loc ? ed_loc->getFile() : nullptr,
                            compile_file, compile_files)) {
                        continue;
                    }
                }
                std::string key = ed->getNamespacePath();
                if (key.empty()) {
                    key = makeNamespaceItemKey(ns, eli.getName());
                }
                if (state.enum_keys.insert(key).second) {
                    state.enums.push_back({ed, ns_idx});
                }
            }
        }
    }

    // Collect user typedefs (only resolved ones)
    for (auto& ti : ns->typedefMap) {
        if (ti.second->typeInfo) {
            // Filter out typedefs from reexported dependencies
            const char* td_module = ti.second->getModuleName();
            if (shouldSkipReexportedItem(td_module, current_module, keep_modules)) {
                continue;
            }
            if (shouldSkipByCompileFile(ti.second->loc ? ti.second->loc->getFile() : nullptr,
                    compile_file, compile_files)) {
                continue;
            }
            std::string key = makeNamespaceItemKey(ns, ti.first.c_str());
            if (state.typedef_keys.insert(key).second) {
                state.typedefs.push_back({ti.first, ti.second->typeInfo, ti.second->pub, ns_idx,
                    ti.second->loc});
            }
        }
    }

    // Collect user constants
    {
        ConstantListIterator cli(ns->constant);
        while (cli.next()) {
            ConstantEntry* ce = cli.getEntry();
            if (!ce->isSystem() && !ce->isExternalStub()) {
                // Filter out constants from reexported dependencies
                const char* const_module = ce->getModuleName();
                if (shouldSkipReexportedItem(const_module, current_module, keep_modules)) {
                    continue;
                }
                if (shouldSkipByCompileFile(ce->loc ? ce->loc->getFile() : nullptr,
                        compile_file, compile_files)) {
                    continue;
                }
                std::string key = makeNamespaceItemKey(ns, ce->getName());
                if (state.constant_keys.insert(key).second) {
                    state.constants.push_back({ce, ns_idx});
                }
            }
        }
    }

    // Collect user global variables
    for (auto& vi : ns->var_list.vmap) {
        Var* var = vi.second;
        if (var->isImported() || var->isAOTImport()) {
            // Imported globals belong to dependency modules.  Serializing them
            // here would make the downstream AOT module deserialize a private
            // local slot instead of binding to the dependency's live storage.
            continue;
        }
        if (!var->isBuiltin()) {
            // Filter out globals from reexported dependencies
            const char* var_module = var->getModuleName();
            if (shouldSkipReexportedItem(var_module, current_module, keep_modules)) {
                continue;
            }
            if (hasAOTBinaryCompileFileFilter(compile_file, compile_files)) {
                const QoreProgramLocation* v_loc = var->getParseLocation();
                if (shouldSkipByCompileFile(v_loc ? v_loc->getFile() : nullptr,
                        compile_file, compile_files)) {
                    continue;
                }
            }
            std::string key = makeNamespaceItemKey(ns, vi.first);
            if (state.global_keys.insert(key).second) {
                state.globals.push_back({var, ns_idx});
            }
        }
    }

    // Collect user functions
    for (auto fi = ns->func_list.begin(), fe = ns->func_list.end(); fi != fe; ++fi) {
        FunctionEntry* entry = fi->second;
        QoreFunction* func = entry->getFunction();
        if (func && !entry->hasBuiltin()) {
            // Filter out functions from reexported dependencies
            const char* func_module = func->getModuleName();
            if (shouldSkipReexportedItem(func_module, current_module, keep_modules)) {
                continue;
            }
            // Phase 4 slice 4: per-file filter — keep the function only
            // if at least one user variant's declaration file matches the
            // target. Overloaded variants can live in different files
            // (unusual but legal); per-variant filtering at codegen time
            // ensures only matching variants produce native code.
            if (hasAOTBinaryCompileFileFilter(compile_file, compile_files)) {
                bool any_in_file = false;
                QoreFunctionIterator vit(*func);
                while (vit.next()) {
                    const AbstractQoreFunctionVariant* v = vit.getVariant();
                    UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(v)
                        ->getUserVariantBase();
                    if (!uvb) {
                        continue;
                    }
                    const UserSignature* sig = uvb->getUserSignature();
                    const QoreProgramLocation* vloc = sig ? sig->getParseLocation() : nullptr;
                    if (!shouldSkipByCompileFile(vloc ? vloc->getFile() : nullptr,
                            compile_file, compile_files)) {
                        any_in_file = true;
                        break;
                    }
                }
                if (!any_in_file) {
                    continue;
                }
            }
            std::string key = makeNamespaceItemKey(ns, func->getName());
            if (state.function_keys.insert(key).second) {
                state.functions.push_back({entry, func, ns_idx});
            }
        }
    }

    // Recurse into child namespaces (filter out namespaces from reexported dependencies)
    for (auto ni = ns->nsl.nsmap.begin(), ne = ns->nsl.nsmap.end(); ni != ne; ++ni) {
        QoreNamespace* child_ns = ni->second;
        if (child_ns) {
            qore_ns_private* child_priv = qore_ns_private::get(*child_ns);
            // Cross-module namespace shells (e.g. ::OMQ, ::Qore, ::Priv)
            // carry the module name of whichever module FIRST created
            // the shell — not of every item subsequently added to it.
            // A user module declaring new classes under an existing
            // namespace (e.g. `public namespace OMQ { class ThreadLocalData { ... } }`
            // in QorusClientBase.qm) legitimately owns those classes
            // even though ns->getModuleName() reflects the shell's
            // original creator.
            //
            // Filtering at the namespace level silently drops all such
            // items; the per-item filters below
            // (`shouldSkipReexportedItem(class_module, ...)` etc) already
            // catch real cross-module items using each item's own
            // `getModuleName()`, which IS accurate.  Matching fix on
            // the compile-walker side in QoreAOT.cpp.
            collectItems(state, child_priv, ns_idx, current_module, keep_modules, compile_file, compile_files);
        }
    }
}

} // namespace

const char* qoreAOTSymbolKindName(QoreAOTSymbolKind kind) {
    switch (kind) {
        case QoreAOTSymbolKind::NAMESPACE: return "namespace";
        case QoreAOTSymbolKind::CLASS: return "class";
        case QoreAOTSymbolKind::HASHDECL: return "hashdecl";
        case QoreAOTSymbolKind::ENUM: return "enum";
        case QoreAOTSymbolKind::ENUM_MEMBER: return "enum_member";
        case QoreAOTSymbolKind::TYPEDEF: return "typedef";
        case QoreAOTSymbolKind::CONSTANT: return "constant";
        case QoreAOTSymbolKind::GLOBAL: return "global";
        case QoreAOTSymbolKind::FUNCTION: return "function";
        case QoreAOTSymbolKind::METHOD: return "method";
        case QoreAOTSymbolKind::STATIC_METHOD: return "static_method";
        case QoreAOTSymbolKind::CONSTRUCTOR: return "constructor";
        case QoreAOTSymbolKind::STATIC_VAR: return "static_var";
        case QoreAOTSymbolKind::NATIVE: return "native";
    }
    return "unknown";
}

const char* qoreAOTDependencyClassName(QoreAOTDependencyClass dependency_class) {
    switch (dependency_class) {
        case QoreAOTDependencyClass::UNKNOWN: return "unknown";
        case QoreAOTDependencyClass::SOURCE_TEXT: return "source_text";
        case QoreAOTDependencyClass::QORE_API: return "qore_api";
        case QoreAOTDependencyClass::QORE_VALUE: return "qore_value";
        case QoreAOTDependencyClass::NATIVE_BODY: return "native_body";
        case QoreAOTDependencyClass::MODULE_API: return "module_api";
        case QoreAOTDependencyClass::MODULE_RUNTIME: return "module_runtime";
        case QoreAOTDependencyClass::DYNAMIC: return "dynamic";
    }
    return "unknown";
}

const char* qoreAOTCallRelocationTargetKindName(QoreAOTCallRelocationTargetKind kind) {
    switch (kind) {
        case QoreAOTCallRelocationTargetKind::NONE: return "none";
        case QoreAOTCallRelocationTargetKind::FUNCTION: return "function";
        case QoreAOTCallRelocationTargetKind::METHOD: return "method";
        case QoreAOTCallRelocationTargetKind::STATIC_METHOD: return "static_method";
        case QoreAOTCallRelocationTargetKind::CONSTRUCTOR: return "constructor";
    }
    return "unknown";
}

static std::string aotStripLeadingColons(std::string path) {
    while (path.size() >= 2 && path[0] == ':' && path[1] == ':') {
        path.erase(0, 2);
    }
    return path;
}

static std::string aotNamespacePath(const AOTSerializeState& state, uint32_t ns_idx) {
    if (ns_idx >= state.namespaces.size() || !state.namespaces[ns_idx].ns) {
        return std::string();
    }
    return aotStripLeadingColons(state.namespaces[ns_idx].ns->path);
}

static std::string aotJoinPath(const std::string& scope, const char* name) {
    if (scope.empty()) {
        return name ? std::string(name) : std::string();
    }
    return scope + "::" + (name ? name : "");
}

static const char* aotLocationFile(const QoreProgramLocation* loc) {
    return loc ? loc->getFile() : "";
}

static std::string aotVisibility(bool pub) {
    return pub ? "public" : "private";
}

static std::string aotVisibility(ClassAccess access) {
    switch (access) {
        case Public: return "public";
        case Private: return "private";
        case Internal: return "private:internal";
        case Inaccessible: return "inaccessible";
    }
    return "unknown";
}

static uint64_t aotFnv1a64Update(uint64_t h, const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static void aotHashAppend(uint64_t& h, const std::string& value) {
    h = aotFnv1a64Update(h, value.data(), value.size());
    const uint8_t sep = 0xff;
    h = aotFnv1a64Update(h, &sep, 1);
}

static std::string aotHashHex(uint64_t h) {
    std::ostringstream os;
    os << std::hex << std::setfill('0') << std::setw(16) << h;
    return os.str();
}

static std::string aotHashParts(const std::vector<std::string>& parts) {
    uint64_t h = 1469598103934665603ULL;
    for (const std::string& part : parts) {
        aotHashAppend(h, part);
    }
    return aotHashHex(h);
}

static std::string aotHexBytes(const uint8_t* data, size_t size) {
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
        os << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return os.str();
}

static std::string aotTypePathString(const QoreTypeInfo* ti, bool no_narrow = false) {
    return getTypePath(ti, no_narrow);
}

static std::string aotValueTypeName(const QoreValue& value) {
    QoreString type_scratch;
    const char* full_type = value.getFullTypeName(true, type_scratch);
    return full_type ? full_type : "";
}

static std::string aotNonFoldableValueHash(const QoreValue& value, const char* reason) {
    return "not-foldable:" + aotValueTypeName(value) + ":" + (reason ? reason : "unsupported");
}

static bool aotCheckValueHashCancel(size_t ordinal) {
    return ordinal && !(ordinal % 100) && qore_check_cancel(nullptr, "AOT symbol-index value hashing");
}

static bool aotAppendValueHashParts(const QoreValue& value, std::vector<std::string>& parts,
        std::unordered_set<const void*>& seen, size_t depth) {
    if (depth > 64) {
        parts.push_back(aotNonFoldableValueHash(value, "depth"));
        return false;
    }

    parts.push_back("type");
    parts.push_back(aotValueTypeName(value));
    parts.push_back(std::to_string(value.getType()));

    if (value.isNothing()) {
        parts.push_back("nothing");
        return true;
    }
    if (value.isNull()) {
        parts.push_back("null");
        return true;
    }
    if (value.isBool()) {
        parts.push_back(value.getBool() ? "true" : "false");
        return true;
    }
    if (value.isInt()) {
        parts.push_back(std::to_string(value.getInt()));
        return true;
    }
    if (value.isFloat()) {
        std::ostringstream os;
        os << std::setprecision(std::numeric_limits<double>::max_digits10) << value.getDouble();
        parts.push_back(os.str());
        return true;
    }
    if (value.isChar()) {
        parts.push_back("char");
        parts.push_back(std::to_string(value.getChar()));
        return true;
    }
    if (value.isShortString()) {
        char buf[8] = {};
        value.getShortString(buf);
        parts.push_back("string");
        parts.emplace_back(buf, value.shortStringLen());
        return true;
    }
    if (value.isEnum()) {
        const QoreEnumMember* member = value.getEnumMember();
        const QoreEnumDecl* decl = member ? member->getEnumDecl() : nullptr;
        parts.push_back("enum");
        parts.push_back(decl ? aotStripLeadingColons(decl->getNamespacePath()) : "");
        parts.push_back(member && member->getName() ? member->getName() : "");
        if (member) {
            QoreValue base = member->getValue();
            return aotAppendValueHashParts(base, parts, seen, depth + 1);
        }
        return true;
    }

    switch (value.getType()) {
        case NT_STRING:
        case NT_NUMBER:
        case NT_DATE: {
            ExceptionSink xsink;
            QoreNodeAsStringHelper str(value, FMT_NONE, &xsink);
            if (xsink) {
                xsink.handleExceptions();
                parts.push_back(aotNonFoldableValueHash(value, "string-conversion"));
                return false;
            }
            const QoreString* qs = *str;
            parts.push_back("scalar");
            parts.emplace_back(qs ? qs->c_str() : "", qs ? qs->strlen() : 0);
            return true;
        }
        case NT_LIST: {
            const QoreListNode* list = value.get<const QoreListNode>();
            if (!list) {
                parts.push_back(aotNonFoldableValueHash(value, "list"));
                return false;
            }
            if (!seen.insert(list).second) {
                parts.push_back(aotNonFoldableValueHash(value, "recursive-list"));
                return false;
            }
            parts.push_back("list");
            parts.push_back(std::to_string(list->size()));
            bool precise = true;
            ConstListIterator li(list);
            size_t i = 0;
            while (li.next()) {
                if (aotCheckValueHashCancel(i)) {
                    parts.push_back(aotNonFoldableValueHash(value, "cancelled"));
                    precise = false;
                    break;
                }
                parts.push_back(std::to_string(i++));
                precise = aotAppendValueHashParts(li.getValue(), parts, seen, depth + 1) && precise;
            }
            seen.erase(list);
            return precise;
        }
        case NT_HASH: {
            const QoreHashNode* hash = value.get<const QoreHashNode>();
            if (!hash) {
                parts.push_back(aotNonFoldableValueHash(value, "hash"));
                return false;
            }
            if (!seen.insert(hash).second) {
                parts.push_back(aotNonFoldableValueHash(value, "recursive-hash"));
                return false;
            }
            parts.push_back("hash");
            parts.push_back(std::to_string(hash->size()));
            std::vector<std::pair<std::string, QoreValue>> entries;
            entries.reserve(hash->size());
            ConstHashIterator hi(hash);
            size_t i = 0;
            while (hi.next()) {
                if (aotCheckValueHashCancel(i++)) {
                    parts.push_back(aotNonFoldableValueHash(value, "cancelled"));
                    seen.erase(hash);
                    return false;
                }
                entries.emplace_back(hi.getKey() ? hi.getKey() : "", hi.get());
            }
            std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
            bool precise = true;
            for (size_t i = 0; i < entries.size(); ++i) {
                if (aotCheckValueHashCancel(i)) {
                    parts.push_back(aotNonFoldableValueHash(value, "cancelled"));
                    precise = false;
                    break;
                }
                const auto& entry = entries[i];
                parts.push_back(entry.first);
                precise = aotAppendValueHashParts(entry.second, parts, seen, depth + 1) && precise;
            }
            seen.erase(hash);
            return precise;
        }
        default:
            parts.push_back(aotNonFoldableValueHash(value, "unsupported"));
            return false;
    }
}

static std::string aotValueHash(const QoreValue& value) {
    std::vector<std::string> parts = {"value"};
    std::unordered_set<const void*> seen;
    bool precise = aotAppendValueHashParts(value, parts, seen, 0);
    if (!precise) {
        return aotNonFoldableValueHash(value, "unsupported");
    }
    return aotHashParts({
        "value",
        aotValueTypeName(value),
        std::to_string(value.getType()),
        aotHashParts(parts),
    });
}

static std::string aotValueHashForConstant(const ConstantEntry* ce) {
    if (!ce) {
        return std::string();
    }
    if (ce->hasInitExpr()) {
        return "pending";
    }
    QoreValue value = ce->getReferencedValue();
    std::string rv = aotValueHash(value);
    value.discard(nullptr);
    return rv;
}

static std::string aotCallableDisplayKey(const char* name, const AbstractQoreFunctionVariant* variant) {
    return getVariantKey(name ? name : "", variant);
}

static std::string aotMethodDisplayKey(const QoreClass* qc, const char* method_name,
        const AbstractQoreFunctionVariant* variant) {
    std::string key;
    if (const char* class_path = qc ? qc->getPath() : nullptr) {
        key = aotStripLeadingColons(class_path);
    }
    key += "::";
    key += method_name ? method_name : "";
    return getVariantKey(key.c_str(), variant);
}

static std::string aotSignatureSurface(const std::string& key,
        const AbstractQoreFunctionVariant* variant) {
    std::vector<std::string> parts = {"signature", key};
    const AbstractFunctionSignature* sig =
        const_cast<AbstractQoreFunctionVariant*>(variant)->getSignature();
    if (!sig) {
        return aotHashParts(parts);
    }

    parts.push_back(aotTypePathString(sig->getReturnTypeInfo()));
    parts.push_back(sig->hasVarargs() ? "sig-varargs" : "sig-fixed");
    parts.push_back(variant && variant->hasVarargs() ? "effective-varargs" : "effective-fixed");
    parts.push_back(std::to_string(sig->numParams()));
    for (unsigned i = 0; i < sig->numParams(); ++i) {
        parts.push_back(sig->getName(i) ? sig->getName(i) : "");
        parts.push_back(aotTypePathString(sig->getParamTypeInfo(i)));
    }
    return aotHashParts(parts);
}

static std::string aotFunctionVariantSourceFile(const AbstractQoreFunctionVariant* variant) {
    const UserVariantBase* uvb = variant
        ? const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase() : nullptr;
    const UserSignature* sig = uvb ? uvb->getUserSignature() : nullptr;
    return aotLocationFile(sig ? sig->getParseLocation() : nullptr);
}

static std::string aotFunctionVariantDeclHash(const char* kind, const std::string& key,
        const std::string& visibility, const AbstractQoreFunctionVariant* variant) {
    std::vector<std::string> parts = {
        kind ? kind : "",
        key,
        visibility,
        aotSignatureSurface(key, variant),
    };
    const UserVariantBase* uvb = variant
        ? const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase() : nullptr;
    parts.push_back(uvb && uvb->isSynchronized() ? "synchronized" : "not-synchronized");
    return aotHashParts(parts);
}

static std::string aotMethodVariantDeclHash(const std::string& key, const std::string& visibility,
        const AbstractQoreFunctionVariant* variant, bool is_static) {
    std::vector<std::string> parts = {
        is_static ? "static_method" : "method",
        key,
        visibility,
        aotSignatureSurface(key, variant),
    };
    const MethodVariantBase* mvb = reinterpret_cast<const MethodVariantBase*>(variant);
    const UserVariantBase* uvb = variant
        ? const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase() : nullptr;
    parts.push_back(mvb && mvb->isFinal() ? "final" : "not-final");
    parts.push_back(mvb && mvb->isAbstract() ? "abstract" : "concrete");
    parts.push_back((mvb && mvb->isMethodSynchronized()) || (uvb && uvb->isSynchronized())
        ? "synchronized" : "not-synchronized");
    parts.push_back(mvb && mvb->isConstMethod() ? "const" : "mutable");
    return aotHashParts(parts);
}

static std::string aotClassDeclHash(const AOTSerializeState::ClassInfo& ci) {
    const qore_class_private* priv = ci.priv;
    if (priv && priv->hash) {
        return "sha1:" + aotHexBytes(reinterpret_cast<const uint8_t*>(priv->hash.getHash()), SH_SIZE);
    }

    std::vector<std::string> parts = {
        "class",
        priv ? aotStripLeadingColons(priv->path) : "",
        priv && priv->pub ? "public" : "private",
        priv && priv->final ? "final" : "not-final",
    };
    if (priv && priv->scl) {
        for (auto* base : *priv->scl) {
            parts.push_back(base && base->sclass
                ? aotStripLeadingColons(qore_class_private::get(*base->sclass)->path) : "");
            parts.push_back(base ? aotVisibility(base->access) : "");
            parts.push_back(base && base->is_virtual ? "virtual" : "non-virtual");
        }
    }
    return aotHashParts(parts);
}

static bool aotCheckSymbolIndexCancel(size_t ordinal, std::string* error, const char* operation) {
    if (ordinal && !(ordinal % 100) && qore_check_cancel(nullptr, operation)) {
        if (error) {
            *error = "operation cancelled during ";
            *error += operation ? operation : "AOT symbol-index serialization";
        }
        return false;
    }
    return true;
}

static bool aotBCANeedsParseResolution(const BCANode* bca) {
    return bca && ((!bca->classid && (bca->ns || bca->name)) || bca->getParseArgs());
}

static bool aotResolveBCAListForSerialization(const QoreClass* qc, const UserConstructorVariant* ucv,
        const BCAList* bcal, std::string* error) {
    if (!qc || !ucv || !bcal) {
        return true;
    }

    bool needs_resolution = false;
    for (const BCANode* bca : *bcal) {
        if (aotBCANeedsParseResolution(bca)) {
            needs_resolution = true;
            break;
        }
    }
    if (!needs_resolution) {
        return true;
    }

    qore_class_private* priv = qore_class_private::get(*const_cast<QoreClass*>(qc));
    if (!priv || !priv->scl) {
        if (error) {
            *error = "cannot resolve base-constructor arguments for class '";
            *error += qc->getName();
            *error += "': class has no base-class list";
        }
        return false;
    }

    UserConstructorVariant* mut_ucv = const_cast<UserConstructorVariant*>(ucv);
    UserParamListLocalVarHelper ph(mut_ucv, qc->getTypeInfo());
    for (BCANode* bca : *const_cast<BCAList*>(bcal)) {
        if (!aotBCANeedsParseResolution(bca)) {
            continue;
        }
        if (bca->parseInit(priv->scl, qc->getName())) {
            if (error) {
                *error = "cannot resolve base-constructor arguments for class '";
                *error += qc->getName();
                *error += "'";
            }
            return false;
        }
    }
    return true;
}

static bool aotClassRefMatches(const QoreClass* qc, const char* ref) {
    if (!qc || !ref || !*ref) {
        return false;
    }
    auto match_path = [ref](const char* path) -> bool {
        if (!path || !*path) {
            return false;
        }
        if (!strcmp(path, ref)) {
            return true;
        }
        return path[0] == ':' && path[1] == ':' && !strcmp(path + 2, ref);
    };
    std::string ns_path = qc->getNamespacePath();
    return match_path(qc->getName()) || match_path(qc->getPath()) || match_path(ns_path.c_str());
}

static const QoreClass* aotFindBCAClass(const QoreMethod* method, const BCANode* bca) {
    if (!method || !bca) {
        return nullptr;
    }

    const QoreClass* method_class = method->getClass();
    const qore_class_private* cls_priv = method_class ? qore_class_private::get(*method_class) : nullptr;
    if (!cls_priv || !cls_priv->scl) {
        return nullptr;
    }

    if (bca->classid) {
        ClassAccess access = Public;
        if (const QoreClass* base_cls = cls_priv->scl->getClass(bca->classid, access, true)) {
            return base_cls;
        }
    }

    const char* ref = bca->ns ? bca->ns->ostr : bca->name;
    if (!ref || !*ref) {
        return nullptr;
    }
    for (auto bi = cls_priv->scl->begin(), be = cls_priv->scl->end(); bi != be; ++bi) {
        const QoreClass* base = (*bi)->sclass;
        if (aotClassRefMatches(base, ref)) {
            return base;
        }
    }
    return nullptr;
}

static std::string aotBCAClassRef(const QoreMethod* method, const BCANode* bca) {
    if (const QoreClass* base_cls = aotFindBCAClass(method, bca)) {
        return qore_aot_encode_class_ref(base_cls);
    }
    if (!bca) {
        return std::string();
    }
    const char* ref = bca->ns ? bca->ns->ostr : bca->name;
    return ref ? std::string(ref) : std::string();
}

static uint16_t aotBCAArgCount(const BCANode* bca) {
    if (!bca) {
        return 0;
    }
    if (const QoreListNode* args = bca->getArgs()) {
        return static_cast<uint16_t>(args->size());
    }
    if (const QoreParseListNode* parse_args = bca->getParseArgs()) {
        return static_cast<uint16_t>(parse_args->size());
    }
    return 0;
}

static QoreValue aotBCAArgValue(const BCANode* bca, uint16_t arg_index) {
    assert(bca);
    if (const QoreListNode* args = bca->getArgs()) {
        return args->retrieveEntry(arg_index);
    }
    const QoreParseListNode* parse_args = bca->getParseArgs();
    assert(parse_args);
    return parse_args->get(arg_index);
}

static bool aotSymbolRecordLess(const QoreAOTSymbolIndexRecord& a,
        const QoreAOTSymbolIndexRecord& b) {
    if (a.qore_path != b.qore_path) {
        return a.qore_path < b.qore_path;
    }
    if (a.kind != b.kind) {
        return static_cast<uint8_t>(a.kind) < static_cast<uint8_t>(b.kind);
    }
    if (a.source_file != b.source_file) {
        return a.source_file < b.source_file;
    }
    return a.native_symbol < b.native_symbol;
}

static void writeSymbolIndexRecord(QoreAOTBinaryWriter& writer,
        const QoreAOTSymbolIndexRecord& rec) {
    writer.writeU8(static_cast<uint8_t>(rec.kind));
    writer.writeU8(static_cast<uint8_t>(rec.dependency_class));
    writer.writeU16(rec.flags);
    writer.writeU32(rec.metadata_slot);
    writer.writeStringRef(rec.qore_path.c_str());
    writer.writeStringRef(rec.source_file.c_str());
    writer.writeStringRef(rec.visibility.c_str());
    writer.writeStringRef(rec.signature_hash.c_str());
    writer.writeStringRef(rec.declaration_hash.c_str());
    writer.writeStringRef(rec.value_hash.c_str());
    writer.writeStringRef(rec.native_symbol.c_str());
    writer.writeStringRef(rec.abi_kind.c_str());
    writer.writeStringRef(rec.consumer_source_file.c_str());
    writer.writeStringRef(rec.provider_source_file.c_str());
    writer.writeU32(rec.fast_entry_flags);
    writer.writeU32(rec.fast_entry_num_params);
    writer.writeU8(rec.fast_return_kind);
    for (const auto* values : {&rec.fast_param_kinds,
            &rec.fast_param_rejects_nothing, &rec.fast_param_noescape}) {
        writer.writeU32(static_cast<uint32_t>(values->size()));
        if (!values->empty()) {
            writer.writeBytes(values->data(), static_cast<uint32_t>(values->size()));
        }
    }
    writer.writeU8(rec.scalar_leaf_kind);
    writer.writeU16(rec.scalar_leaf_opcode);
    writer.writeU8(static_cast<uint8_t>(rec.scalar_leaf_lhs_param));
    writer.writeU8(static_cast<uint8_t>(rec.scalar_leaf_rhs_param));
    writer.writeI64(rec.scalar_leaf_lhs_int);
    writer.writeI64(rec.scalar_leaf_rhs_int);
    writer.writeF64(rec.scalar_leaf_lhs_float);
    writer.writeF64(rec.scalar_leaf_rhs_float);
    writer.writeI64(rec.scalar_leaf_true_scale);
    writer.writeI64(rec.scalar_leaf_true_offset);
    writer.writeI64(rec.scalar_leaf_false_scale);
    writer.writeI64(rec.scalar_leaf_false_offset);
    writer.writeStringRef(rec.object_getter_member.c_str());
}

static bool writeSymbolIndexRecordVector(QoreAOTBinaryWriter& writer,
        std::vector<QoreAOTSymbolIndexRecord>& records, std::string* error,
        const char* operation) {
    if (records.size() > std::numeric_limits<uint32_t>::max()) {
        if (error) {
            *error = "too many AOT symbol-index records for u32 wire format";
        }
        return false;
    }
    std::sort(records.begin(), records.end(), aotSymbolRecordLess);
    writer.writeU32(static_cast<uint32_t>(records.size()));
    for (size_t i = 0; i < records.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, operation)) {
            return false;
        }
        writeSymbolIndexRecord(writer, records[i]);
    }
    return true;
}

static void aotAddNativeRecord(std::vector<QoreAOTSymbolIndexRecord>& native,
        const std::string& qore_path, const std::string& native_symbol, const char* abi_kind) {
    if (native_symbol.empty()) {
        return;
    }
    QoreAOTSymbolIndexRecord rec;
    rec.kind = QoreAOTSymbolKind::NATIVE;
    rec.dependency_class = QoreAOTDependencyClass::NATIVE_BODY;
    rec.flags = QORE_AOT_SYMBOL_FLAG_NATIVE_DEFINED;
    rec.qore_path = qore_path;
    rec.native_symbol = native_symbol;
    rec.abi_kind = abi_kind ? abi_kind : "qore_body";
    native.push_back(std::move(rec));
}

static void aotAddFastEntryRecord(std::vector<QoreAOTSymbolIndexRecord>& native,
        const std::string& qore_path, const QoreAOTFastEntryIndexInfo& info) {
    if (info.native_symbol.empty() || !(info.flags & QORE_AOT_FAST_ENTRY_PRESENT)) {
        return;
    }
    QoreAOTSymbolIndexRecord rec;
    rec.kind = QoreAOTSymbolKind::NATIVE;
    rec.dependency_class = QoreAOTDependencyClass::NATIVE_BODY;
    rec.flags = QORE_AOT_SYMBOL_FLAG_NATIVE_DEFINED;
    rec.qore_path = qore_path;
    rec.native_symbol = info.native_symbol;
    rec.abi_kind = "qore_fast_v1";
    rec.fast_entry_flags = info.flags;
    rec.fast_entry_num_params = info.num_params;
    rec.fast_return_kind = info.return_kind;
    rec.fast_param_kinds = info.param_kinds;
    rec.fast_param_rejects_nothing = info.param_rejects_nothing;
    rec.fast_param_noescape = info.param_noescape;
    rec.scalar_leaf_kind = info.scalar_leaf_kind;
    rec.scalar_leaf_opcode = info.scalar_leaf_opcode;
    rec.scalar_leaf_lhs_param = info.scalar_leaf_lhs_param;
    rec.scalar_leaf_rhs_param = info.scalar_leaf_rhs_param;
    rec.scalar_leaf_lhs_int = info.scalar_leaf_lhs_int;
    rec.scalar_leaf_rhs_int = info.scalar_leaf_rhs_int;
    rec.scalar_leaf_lhs_float = info.scalar_leaf_lhs_float;
    rec.scalar_leaf_rhs_float = info.scalar_leaf_rhs_float;
    rec.scalar_leaf_true_scale = info.scalar_leaf_true_scale;
    rec.scalar_leaf_true_offset = info.scalar_leaf_true_offset;
    rec.scalar_leaf_false_scale = info.scalar_leaf_false_scale;
    rec.scalar_leaf_false_offset = info.scalar_leaf_false_offset;
    rec.object_getter_member = info.object_getter_member;
    native.push_back(std::move(rec));
}

static QoreAOTSymbolKind aotSymbolKindForCallRelocation(QoreAOTCallRelocationTargetKind kind) {
    switch (kind) {
        case QoreAOTCallRelocationTargetKind::FUNCTION:
            return QoreAOTSymbolKind::FUNCTION;
        case QoreAOTCallRelocationTargetKind::METHOD:
            return QoreAOTSymbolKind::METHOD;
        case QoreAOTCallRelocationTargetKind::STATIC_METHOD:
            return QoreAOTSymbolKind::STATIC_METHOD;
        case QoreAOTCallRelocationTargetKind::CONSTRUCTOR:
            return QoreAOTSymbolKind::CONSTRUCTOR;
        case QoreAOTCallRelocationTargetKind::NONE:
            break;
    }
    return QoreAOTSymbolKind::FUNCTION;
}

static const char* aotFuncSlotSourceFile(const AOTCompiledFuncWithSlots& func, size_t expr_slot) {
    if (expr_slot < func.aot_locs.size() && !func.aot_locs[expr_slot].file.empty()) {
        return func.aot_locs[expr_slot].file.c_str();
    }
    if (!func.aot_locs.empty() && !func.aot_locs.front().file.empty()) {
        return func.aot_locs.front().file.c_str();
    }
    return nullptr;
}

static bool aotAppendCallImportRecords(const std::vector<AOTCompiledFuncWithSlots>* funcs,
        std::vector<QoreAOTSymbolIndexRecord>& imported, const char* compile_file,
        const std::unordered_set<std::string>* compile_files, std::string* error) {
    if (!funcs) {
        return true;
    }

    std::set<std::string> seen;
    for (size_t i = 0; i < funcs->size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index call-import collection")) {
            return false;
        }
        const AOTCompiledFuncWithSlots& func = (*funcs)[i];
        for (size_t j = 0; j < func.slot_ids.exprs.size(); ++j) {
            if (!aotCheckSymbolIndexCancel(j, error, "AOT symbol-index call-import collection")) {
                return false;
            }
            const AOTExprSlotId& expr = func.slot_ids.exprs[j];
            if (expr.call_relocation_kind == QoreAOTCallRelocationTargetKind::NONE
                    || expr.reloc_qore_path.empty()) {
                continue;
            }
            const char* consumer_file = aotFuncSlotSourceFile(func, j);
            if (shouldSkipByCompileFile(consumer_file, compile_file, compile_files)) {
                continue;
            }

            QoreAOTSymbolKind kind = aotSymbolKindForCallRelocation(expr.call_relocation_kind);
            std::string seen_key = std::to_string(static_cast<unsigned>(kind));
            seen_key += '\n';
            seen_key += expr.reloc_qore_path;
            seen_key += '\n';
            seen_key += consumer_file ? consumer_file : "";
            if (!seen.insert(seen_key).second) {
                continue;
            }

            QoreAOTSymbolIndexRecord rec;
            rec.kind = kind;
            rec.dependency_class = QoreAOTDependencyClass::QORE_API;
            rec.flags = QORE_AOT_SYMBOL_FLAG_OPTIONAL_IMPORT;
            rec.metadata_slot = static_cast<uint32_t>(std::min<size_t>(j, UINT32_MAX));
            rec.qore_path = expr.reloc_qore_path;
            rec.consumer_source_file = consumer_file ? consumer_file : "";
            imported.push_back(std::move(rec));
        }
    }
    return true;
}

static bool aotAppendGlobalImportRecords(const std::vector<AOTCompiledFuncWithSlots>* funcs,
        std::vector<QoreAOTSymbolIndexRecord>& imported, const char* compile_file,
        const std::unordered_set<std::string>* compile_files, std::string* error) {
    if (!funcs) {
        return true;
    }

    std::set<std::string> seen;
    for (size_t i = 0; i < funcs->size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index global import collection")) {
            return false;
        }
        const AOTCompiledFuncWithSlots& func = (*funcs)[i];
        for (size_t j = 0; j < func.slot_ids.globals.size(); ++j) {
            if (!aotCheckSymbolIndexCancel(j, error, "AOT symbol-index global import slot collection")) {
                return false;
            }
            const AOTGlobalSlotId& global = func.slot_ids.globals[j];
            if (!global.is_aot_import || global.name.empty()) {
                continue;
            }
            const char* consumer_file = aotFuncSlotSourceFile(func, j);
            if (shouldSkipByCompileFile(consumer_file, compile_file, compile_files)) {
                continue;
            }

            std::string seen_key = global.name;
            seen_key += '\n';
            seen_key += consumer_file ? consumer_file : "";
            if (!seen.insert(seen_key).second) {
                continue;
            }

            QoreAOTSymbolIndexRecord rec;
            rec.kind = QoreAOTSymbolKind::GLOBAL;
            rec.dependency_class = QoreAOTDependencyClass::QORE_API;
            rec.metadata_slot = static_cast<uint32_t>(std::min<size_t>(j, UINT32_MAX));
            rec.qore_path = global.name;
            rec.consumer_source_file = consumer_file ? consumer_file : "";
            rec.declaration_hash = aotHashParts({
                "global-import",
                rec.qore_path,
                global.type_path,
                global.is_thread_local ? "thread_local" : "global",
            });
            imported.push_back(std::move(rec));
        }
    }
    return true;
}

static std::string aotStaticMemberImportPath(const std::string& class_ref, const std::string& member) {
    if (member.empty()) {
        return std::string();
    }
    if (class_ref.empty()) {
        return member;
    }
    // Module-private encoded class references are not linkable by a sibling
    // source path. They are resolved through the owning module program.
    if (class_ref.find('\n') != std::string::npos || class_ref.find("@qore-module:") == 0) {
        return std::string();
    }
    return aotJoinPath(aotStripLeadingColons(class_ref), member.c_str());
}

static bool aotAppendStaticMemberImportRecords(const std::vector<AOTCompiledFuncWithSlots>* funcs,
        std::vector<QoreAOTSymbolIndexRecord>& imported, const char* compile_file,
        const std::unordered_set<std::string>* compile_files, std::string* error) {
    if (!funcs) {
        return true;
    }

    std::set<std::string> seen;
    for (size_t i = 0; i < funcs->size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index static-member import collection")) {
            return false;
        }
        const AOTCompiledFuncWithSlots& func = (*funcs)[i];
        for (size_t j = 0; j < func.slot_ids.exprs.size(); ++j) {
            if (!aotCheckSymbolIndexCancel(j, error, "AOT symbol-index static-member import slot collection")) {
                return false;
            }
            const AOTExprSlotId& expr = func.slot_ids.exprs[j];
            if (expr.kind != AOTExprKind::STATIC_VARREF) {
                continue;
            }
            std::string path = aotStaticMemberImportPath(expr.ref1, expr.ref2);
            if (path.empty()) {
                continue;
            }
            const char* consumer_file = aotFuncSlotSourceFile(func, j);
            if (shouldSkipByCompileFile(consumer_file, compile_file, compile_files)) {
                continue;
            }

            std::string seen_key = path;
            seen_key += '\n';
            seen_key += consumer_file ? consumer_file : "";
            if (!seen.insert(seen_key).second) {
                continue;
            }

            QoreAOTSymbolIndexRecord rec;
            rec.kind = expr.ref1.empty() ? QoreAOTSymbolKind::GLOBAL : QoreAOTSymbolKind::STATIC_VAR;
            rec.dependency_class = QoreAOTDependencyClass::QORE_API;
            rec.metadata_slot = static_cast<uint32_t>(std::min<size_t>(j, UINT32_MAX));
            rec.qore_path = std::move(path);
            rec.consumer_source_file = consumer_file ? consumer_file : "";
            imported.push_back(std::move(rec));
        }
    }
    return true;
}

static bool aotAppendTypeImportRecords(QoreProgram* pgm, std::vector<QoreAOTSymbolIndexRecord>& imported,
        const char* compile_file, const std::unordered_set<std::string>* compile_files, std::string* error) {
    if (!pgm) {
        return true;
    }

    const std::vector<qore_program_private::source_parse_type_import_t>& type_imports =
        qore_program_private::getSourceParseTypeImportRecords(pgm);
    std::set<std::string> seen;
    for (size_t i = 0; i < type_imports.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index source type-import collection")) {
            return false;
        }
        const qore_program_private::source_parse_type_import_t& rec = type_imports[i];
        if (shouldSkipByCompileFile(rec.source_file.c_str(), compile_file, compile_files)) {
            continue;
        }

        std::string seen_key = rec.hashdecl ? "hashdecl" : "class";
        seen_key += '\n';
        seen_key += rec.qore_path;
        seen_key += '\n';
        seen_key += rec.source_file;
        seen_key += '\n';
        seen_key += rec.type_path;
        if (!seen.insert(seen_key).second) {
            continue;
        }

        QoreAOTSymbolIndexRecord ir;
        ir.kind = rec.hashdecl ? QoreAOTSymbolKind::HASHDECL : QoreAOTSymbolKind::CLASS;
        ir.dependency_class = QoreAOTDependencyClass::QORE_API;
        ir.qore_path = rec.qore_path;
        ir.consumer_source_file = rec.source_file;
        imported.push_back(std::move(ir));
    }
    return true;
}

static bool aotAppendFunctionImportRecords(QoreProgram* pgm, std::vector<QoreAOTSymbolIndexRecord>& imported,
        const char* compile_file, const std::unordered_set<std::string>* compile_files, std::string* error) {
    if (!pgm) {
        return true;
    }

    const std::vector<qore_program_private::source_parse_function_import_t>& function_imports =
        qore_program_private::getSourceParseFunctionImportRecords(pgm);
    std::set<std::string> seen;
    for (size_t i = 0; i < function_imports.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index source function-import collection")) {
            return false;
        }
        const qore_program_private::source_parse_function_import_t& rec = function_imports[i];
        if (shouldSkipByCompileFile(rec.source_file.c_str(), compile_file, compile_files)) {
            continue;
        }

        QoreAOTSymbolKind kind =
            rec.kind == qore_program_private::source_parse_call_import_kind_t::Method
                ? QoreAOTSymbolKind::METHOD
                : QoreAOTSymbolKind::FUNCTION;

        std::string seen_key = std::to_string(static_cast<unsigned>(kind));
        seen_key += '\n';
        seen_key += rec.qore_path;
        seen_key += '\n';
        seen_key += rec.source_file;
        if (!seen.insert(seen_key).second) {
            continue;
        }

        QoreAOTSymbolIndexRecord ir;
        ir.kind = kind;
        ir.dependency_class = QoreAOTDependencyClass::QORE_API;
        ir.qore_path = rec.qore_path;
        ir.consumer_source_file = rec.source_file;
        imported.push_back(std::move(ir));
    }
    return true;
}

static bool aotAppendBCAImportRecords(const AOTSerializeState& state,
        std::vector<QoreAOTSymbolIndexRecord>& imported, const char* compile_file,
        const std::unordered_set<std::string>* compile_files, std::string* error) {
    std::set<std::string> seen;
    for (size_t i = 0; i < state.methods.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index BCA import collection")) {
            return false;
        }
        const auto& mi = state.methods[i];
        if (mi.class_idx >= state.classes.size() || !mi.method || mi.is_static
                || strcmp(mi.method->getName(), "constructor")) {
            continue;
        }

        const QoreClass* qc = state.classes[mi.class_idx].cls;
        const qore_method_private* mp = qore_method_private::get(*mi.method);
        const MethodFunctionBase* mfb = mp->func;
        QoreFunctionIterator qfi(*static_cast<const QoreFunction*>(mfb));
        size_t variant_i = 0;
        while (qfi.next()) {
            if (!aotCheckSymbolIndexCancel(variant_i++, error,
                    "AOT symbol-index BCA import variant collection")) {
                return false;
            }
            const AbstractQoreFunctionVariant* v = qfi.getVariant();
            if (!isAOTSerializableMethodVariant(mi.method, v)) {
                continue;
            }
            std::string source_file = aotFunctionVariantSourceFile(v);
            if (shouldSkipByCompileFile(source_file.empty() ? nullptr : source_file.c_str(),
                    compile_file, compile_files)) {
                continue;
            }

            const UserConstructorVariant* ucv = dynamic_cast<const UserConstructorVariant*>(
                reinterpret_cast<const MethodVariantBase*>(v));
            const BCAList* bcal = ucv ? ucv->getBaseClassArgumentList() : nullptr;
            if (!bcal || bcal->empty()) {
                continue;
            }
            if (!aotResolveBCAListForSerialization(qc, ucv, bcal, error)) {
                return false;
            }

            size_t bca_i = 0;
            for (const BCANode* bca : *bcal) {
                if (!aotCheckSymbolIndexCancel(bca_i++, error,
                        "AOT symbol-index BCA import entry collection")) {
                    return false;
                }
                const AbstractQoreFunctionVariant* base_variant = bca->getVariant();
                const QoreClass* base_cls = aotFindBCAClass(mi.method, bca);
                if (!base_variant || !base_cls) {
                    continue;
                }

                std::string key = aotMethodDisplayKey(base_cls, "constructor", base_variant);
                std::string seen_key = key;
                seen_key += '\n';
                seen_key += source_file;
                if (!seen.insert(seen_key).second) {
                    continue;
                }

                QoreAOTSymbolIndexRecord rec;
                rec.kind = QoreAOTSymbolKind::CONSTRUCTOR;
                rec.dependency_class = QoreAOTDependencyClass::QORE_API;
                rec.flags = QORE_AOT_SYMBOL_FLAG_OPTIONAL_IMPORT;
                rec.metadata_slot = static_cast<uint32_t>(std::min<size_t>(i, UINT32_MAX));
                rec.qore_path = std::move(key);
                rec.consumer_source_file = source_file;
                imported.push_back(std::move(rec));
            }
        }
    }
    return true;
}

static bool aotAppendClassMemberRecords(const AOTSerializeState::ClassInfo& ci,
        uint32_t class_slot, std::vector<QoreAOTSymbolIndexRecord>& defined,
        std::string* error) {
    const qore_class_private* priv = ci.priv;
    if (!priv) {
        return true;
    }
    std::string class_path = aotStripLeadingColons(priv->path);

    size_t var_i = 0;
    for (auto& vi : priv->vars.member_list) {
        if (!aotCheckSymbolIndexCancel(var_i++, error, "AOT symbol-index static-var collection")) {
            return false;
        }
        QoreAOTSymbolIndexRecord rec;
        rec.kind = QoreAOTSymbolKind::STATIC_VAR;
        rec.dependency_class = QoreAOTDependencyClass::QORE_API;
        rec.metadata_slot = class_slot;
        rec.qore_path = aotJoinPath(class_path, vi.first);
        rec.source_file = aotLocationFile(vi.second->loc);
        rec.visibility = aotVisibility(vi.second->getAccess());
        rec.declaration_hash = aotHashParts({
            "static_var",
            rec.qore_path,
            rec.visibility,
            aotTypePathString(vi.second->getTypeInfo()),
        });
        defined.push_back(std::move(rec));
    }

    ConstConstantListIterator ccli(priv->constlist);
    size_t const_i = 0;
    while (ccli.next()) {
        if (!aotCheckSymbolIndexCancel(const_i++, error, "AOT symbol-index class-constant collection")) {
            return false;
        }
        const ConstantEntry* ce = ccli.getEntry();
        if (ce->isSystem() || ce->isExternalStub()) {
            continue;
        }
        QoreAOTSymbolIndexRecord rec;
        rec.kind = QoreAOTSymbolKind::CONSTANT;
        rec.dependency_class = ce->hasInitExpr()
            ? QoreAOTDependencyClass::QORE_API : QoreAOTDependencyClass::QORE_VALUE;
        rec.metadata_slot = class_slot;
        rec.qore_path = getClassConstantPath(priv, ce->getName());
        rec.source_file = aotLocationFile(ce->loc);
        rec.visibility = aotVisibility(ce->getAccess());
        rec.value_hash = aotValueHashForConstant(ce);
        rec.declaration_hash = aotHashParts({
            "class_constant",
            rec.qore_path,
            rec.visibility,
            aotTypePathString(ce->typeInfo),
            ce->hasInitExpr() ? "pending" : "literal",
        });
        defined.push_back(std::move(rec));
    }
    return true;
}

struct AOTInheritedClassMemberAliasState {
    std::set<std::string> static_vars;
    std::set<std::string> constants;
    std::set<std::string> static_methods;
    std::set<const qore_class_private*> visited_bases;
    size_t record_count = 0;
};

static bool aotInheritedMemberAccessLinkable(ClassAccess access) {
    return access < Internal;
}

static bool aotAppendInheritedStaticVarAlias(const std::string& class_path,
        const std::string& class_source_file, const char* name, const QoreVarInfo& vi,
        uint32_t class_slot, std::vector<QoreAOTSymbolIndexRecord>& defined) {
    if (!name || !*name || !aotInheritedMemberAccessLinkable(vi.getAccess())) {
        return false;
    }

    QoreAOTSymbolIndexRecord rec;
    rec.kind = QoreAOTSymbolKind::STATIC_VAR;
    rec.dependency_class = QoreAOTDependencyClass::QORE_API;
    rec.metadata_slot = class_slot;
    rec.qore_path = aotJoinPath(class_path, name);
    rec.source_file = class_source_file;
    rec.visibility = aotVisibility(vi.getAccess());
    rec.provider_source_file = aotLocationFile(vi.loc);
    rec.abi_kind = "inherited_alias";
    rec.declaration_hash = aotHashParts({
        "inherited_static_var",
        rec.qore_path,
        rec.visibility,
        aotTypePathString(vi.getTypeInfo()),
    });
    defined.push_back(std::move(rec));
    return true;
}

static bool aotAppendInheritedClassConstantAlias(const std::string& class_path,
        const std::string& class_source_file, const ConstantEntry* ce,
        uint32_t class_slot, std::vector<QoreAOTSymbolIndexRecord>& defined) {
    if (!ce || ce->isSystem() || ce->isExternalStub()
            || !aotInheritedMemberAccessLinkable(ce->getAccess())) {
        return false;
    }

    QoreAOTSymbolIndexRecord rec;
    rec.kind = QoreAOTSymbolKind::CONSTANT;
    rec.dependency_class = ce->hasInitExpr()
        ? QoreAOTDependencyClass::QORE_API : QoreAOTDependencyClass::QORE_VALUE;
    rec.metadata_slot = class_slot;
    rec.qore_path = aotJoinPath(class_path, ce->getName());
    rec.source_file = class_source_file;
    rec.visibility = aotVisibility(ce->getAccess());
    rec.provider_source_file = aotLocationFile(ce->loc);
    rec.abi_kind = "inherited_alias";
    rec.value_hash = aotValueHashForConstant(ce);
    rec.declaration_hash = aotHashParts({
        "inherited_class_constant",
        rec.qore_path,
        rec.visibility,
        aotTypePathString(ce->typeInfo),
        ce->hasInitExpr() ? "pending" : "literal",
    });
    defined.push_back(std::move(rec));
    return true;
}

static bool aotAppendInheritedStaticMethodAliases(const std::string& class_path,
        const std::string& class_source_file, const QoreMethod* method,
        uint32_t class_slot, std::vector<QoreAOTSymbolIndexRecord>& defined,
        std::string* error, AOTInheritedClassMemberAliasState& state) {
    if (!method || !method->isStatic()) {
        return false;
    }

    const qore_method_private* mp = qore_method_private::get(*method);
    const MethodFunctionBase* mfb = mp->func;
    QoreFunctionIterator qfi(*static_cast<const QoreFunction*>(mfb));
    bool emitted = false;
    size_t variant_i = 0;
    while (qfi.next()) {
        if (!aotCheckSymbolIndexCancel(state.record_count++,
                error, "AOT symbol-index inherited static-method alias collection")) {
            return false;
        }
        const AbstractQoreFunctionVariant* v = qfi.getVariant();
        if (!isAOTLinkableMethodVariant(method, v)) {
            continue;
        }
        const MethodVariantBase* mvb = reinterpret_cast<const MethodVariantBase*>(v);
        if (!mvb || !aotInheritedMemberAccessLinkable(mvb->getAccess())) {
            continue;
        }

        std::string key = class_path;
        key += "::";
        key += method->getName();
        key = getVariantKey(key.c_str(), v);

        QoreAOTSymbolIndexRecord rec;
        rec.kind = QoreAOTSymbolKind::STATIC_METHOD;
        rec.dependency_class = QoreAOTDependencyClass::QORE_API;
        rec.metadata_slot = (class_slot << 16) | std::min<uint32_t>(variant_i, UINT16_MAX);
        rec.qore_path = key;
        rec.source_file = class_source_file;
        rec.visibility = aotVisibility(mvb->getAccess());
        rec.signature_hash = aotSignatureSurface(key, v);
        rec.declaration_hash = aotMethodVariantDeclHash(key, rec.visibility, v, true);
        rec.provider_source_file = aotFunctionVariantSourceFile(v);
        rec.abi_kind = "inherited_alias";
        defined.push_back(std::move(rec));
        emitted = true;
        ++variant_i;
    }
    return emitted;
}

static bool aotAppendInheritedClassMemberAliasesFromBase(const qore_class_private* base_priv,
        const std::string& class_path, const std::string& class_source_file, uint32_t class_slot,
        AOTInheritedClassMemberAliasState& state, std::vector<QoreAOTSymbolIndexRecord>& defined,
        std::string* error) {
    if (!base_priv || !state.visited_bases.insert(base_priv).second) {
        return true;
    }

    for (auto& vi : base_priv->vars.member_list) {
        if (!aotCheckSymbolIndexCancel(state.record_count++,
                error, "AOT symbol-index inherited static-var alias collection")) {
            return false;
        }
        if (!state.static_vars.insert(vi.first).second) {
            continue;
        }
        if (!aotAppendInheritedStaticVarAlias(class_path, class_source_file,
                vi.first, *vi.second, class_slot, defined)) {
            state.static_vars.erase(vi.first);
        }
    }

    ConstConstantListIterator ccli(base_priv->constlist);
    while (ccli.next()) {
        if (!aotCheckSymbolIndexCancel(state.record_count++,
                error, "AOT symbol-index inherited class-constant alias collection")) {
            return false;
        }
        const ConstantEntry* ce = ccli.getEntry();
        const std::string& name = ce->getName();
        if (!state.constants.insert(name).second) {
            continue;
        }
        if (!aotAppendInheritedClassConstantAlias(class_path, class_source_file,
                ce, class_slot, defined)) {
            state.constants.erase(name);
        }
    }

    for (auto& mi : base_priv->shm) {
        if (!state.static_methods.insert(mi.first).second) {
            continue;
        }
        if (!aotAppendInheritedStaticMethodAliases(class_path, class_source_file,
                mi.second, class_slot, defined, error, state)) {
            state.static_methods.erase(mi.first);
        }
    }

    if (!base_priv->scl) {
        return true;
    }

    for (auto* bcn : *base_priv->scl) {
        if (!aotCheckSymbolIndexCancel(state.record_count++,
                error, "AOT symbol-index inherited class-member alias base walk")) {
            return false;
        }
        if (!bcn || !bcn->sclass) {
            continue;
        }
        if (!aotAppendInheritedClassMemberAliasesFromBase(qore_class_private::get(*bcn->sclass),
                class_path, class_source_file, class_slot, state, defined, error)) {
            return false;
        }
    }
    return true;
}

static bool aotAppendInheritedClassMemberAliases(const AOTSerializeState::ClassInfo& ci,
        uint32_t class_slot, std::vector<QoreAOTSymbolIndexRecord>& defined,
        std::string* error) {
    const qore_class_private* priv = ci.priv;
    if (!priv || !priv->scl) {
        return true;
    }

    AOTInheritedClassMemberAliasState state;
    for (auto& vi : priv->vars.member_list) {
        state.static_vars.insert(vi.first);
    }
    ConstConstantListIterator ccli(priv->constlist);
    while (ccli.next()) {
        state.constants.insert(ccli.getEntry()->getName());
    }
    for (auto& mi : priv->shm) {
        state.static_methods.insert(mi.first);
    }

    std::string class_path = aotStripLeadingColons(priv->path);
    std::string class_source_file = aotLocationFile(priv->loc);
    for (auto* bcn : *priv->scl) {
        if (!aotCheckSymbolIndexCancel(state.record_count++,
                error, "AOT symbol-index inherited class-member alias base walk")) {
            return false;
        }
        if (!bcn || !bcn->sclass) {
            continue;
        }
        if (!aotAppendInheritedClassMemberAliasesFromBase(qore_class_private::get(*bcn->sclass),
                class_path, class_source_file, class_slot, state, defined, error)) {
            return false;
        }
    }
    return true;
}

bool serializeSymbolIndex(QoreAOTBinaryWriter& writer, qore_ns_private* root_ns,
        const char* module_name, const std::unordered_set<std::string>* keep_modules,
        const char* compile_file,
        const std::unordered_map<std::string, std::string>* native_symbol_map,
        const std::unordered_map<std::string, std::string>* init_native_symbol_map,
        const std::vector<AOTCompiledFuncWithSlots>* func_slots, std::string* error,
        const std::unordered_set<std::string>* compile_files,
        const std::unordered_map<const AbstractQoreFunctionVariant*, QoreAOTFastEntryIndexInfo>*
            fast_entry_map) {
    if (!root_ns) {
        if (error) {
            *error = "missing root namespace for AOT symbol index";
        }
        return false;
    }

    AOTSerializeState state;
    state.root_ns = root_ns;
    collectItems(state, root_ns, UINT32_MAX, module_name, keep_modules, compile_file, compile_files);

    if (module_name && !*module_name && keep_modules
            && !hasAOTBinaryCompileFileFilter(compile_file, compile_files)) {
        size_t keep_i = 0;
        for (const std::string& mod : *keep_modules) {
            if (!aotCheckSymbolIndexCancel(keep_i++, error, "AOT symbol-index local-module collection")) {
                return false;
            }
            QoreProgram* module_pgm = MM.findUserModuleProgram(mod.c_str());
            if (!module_pgm) {
                continue;
            }
            RootQoreNamespace* module_root = module_pgm->getRootNS();
            if (!module_root) {
                continue;
            }
            qore_ns_private* module_root_priv = qore_ns_private::get(*module_root);
            if (module_root_priv != root_ns) {
                collectItems(state, module_root_priv, UINT32_MAX, mod.c_str(), nullptr, nullptr, nullptr);
            }
        }
    }

    std::vector<QoreAOTSymbolIndexRecord> defined;
    std::vector<QoreAOTSymbolIndexRecord> imported;
    std::vector<QoreAOTSymbolIndexRecord> native;
    std::vector<std::pair<std::string, std::string>> context;

    context.emplace_back("qore_version", std::to_string(QORE_VERSION_MAJOR) + "."
        + std::to_string(QORE_VERSION_MINOR) + "." + std::to_string(QORE_VERSION_PATCH));
    context.emplace_back("aot_binary_version", std::to_string(QORE_AOT_BINARY_VERSION));
    context.emplace_back("symbol_index_version", std::to_string(QORE_AOT_SYMBOL_INDEX_VERSION));
    context.emplace_back("feature_flags", aotHashHex(writer.feature_flags));
    context.emplace_back("max_opcode_id", std::to_string(QORE_IR_MAX_OPCODE));
    context.emplace_back("module_filter", module_name ? module_name : "");
    context.emplace_back("compile_file", compile_file ? compile_file : "");
    context.emplace_back("compile_file_count", std::to_string(compile_files ? compile_files->size() : 0));
    if (QoreProgram* pgm = root_ns->getProgram()) {
        const std::vector<qore_program_private::source_parse_define_t>& source_parse_defines =
            qore_program_private::getSourceParseDefineRecords(pgm);
        for (size_t i = 0; i < source_parse_defines.size(); ++i) {
            if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index source parse-define collection")) {
                return false;
            }
            const qore_program_private::source_parse_define_t& rec = source_parse_defines[i];
            if (compile_file && rec.source_file != compile_file) {
                continue;
            }
            if (!compile_file && compile_files && !compile_files->empty()
                    && compile_files->find(rec.source_file) == compile_files->end()) {
                continue;
            }
            context.emplace_back("source_parse_define", rec.define);
        }
        if (!aotAppendTypeImportRecords(pgm, imported, compile_file, compile_files, error)) {
            return false;
        }
        if (!aotAppendFunctionImportRecords(pgm, imported, compile_file, compile_files, error)) {
            return false;
        }
    }

    for (size_t i = 0; i < state.namespaces.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index namespace collection")) {
            return false;
        }
        qore_ns_private* ns = state.namespaces[i].ns;
        std::string path = aotNamespacePath(state, static_cast<uint32_t>(i));
        if (!ns || path.empty()) {
            continue;
        }
        QoreAOTSymbolIndexRecord rec;
        rec.kind = QoreAOTSymbolKind::NAMESPACE;
        rec.dependency_class = QoreAOTDependencyClass::QORE_API;
        rec.metadata_slot = static_cast<uint32_t>(i);
        rec.qore_path = std::move(path);
        rec.visibility = aotVisibility(ns->pub);
        rec.declaration_hash = aotHashParts({"namespace", rec.qore_path, rec.visibility});
        defined.push_back(std::move(rec));
    }

    for (size_t i = 0; i < state.classes.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index class collection")) {
            return false;
        }
        const auto& ci = state.classes[i];
        QoreAOTSymbolIndexRecord rec;
        rec.kind = QoreAOTSymbolKind::CLASS;
        rec.dependency_class = QoreAOTDependencyClass::QORE_API;
        rec.metadata_slot = static_cast<uint32_t>(i);
        rec.qore_path = aotStripLeadingColons(ci.priv->path);
        rec.source_file = aotLocationFile(ci.priv->loc);
        rec.visibility = aotVisibility(ci.priv->pub);
        rec.declaration_hash = aotClassDeclHash(ci);
        defined.push_back(std::move(rec));
        if (!aotAppendClassMemberRecords(ci, static_cast<uint32_t>(i), defined, error)) {
            return false;
        }
        if (!aotAppendInheritedClassMemberAliases(ci, static_cast<uint32_t>(i), defined, error)) {
            return false;
        }
    }

    for (size_t i = 0; i < state.hashdecls.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index hashdecl collection")) {
            return false;
        }
        const TypedHashDecl* hd = state.hashdecls[i].hd;
        const typed_hash_decl_private* hdp = typed_hash_decl_private::get(*hd);
        QoreAOTSymbolIndexRecord rec;
        rec.kind = QoreAOTSymbolKind::HASHDECL;
        rec.dependency_class = QoreAOTDependencyClass::QORE_API;
        rec.metadata_slot = static_cast<uint32_t>(i);
        rec.qore_path = aotStripLeadingColons(hd->getNamespacePath());
        rec.source_file = aotLocationFile(hdp->getParseLocation());
        rec.visibility = aotVisibility(hd->isPublic());
        std::vector<std::string> parts = {"hashdecl", rec.qore_path, rec.visibility};
        const HashDeclMemberMap& members = hdp->getMembers();
        size_t member_i = 0;
        for (auto& mi : members.member_list) {
            if (!aotCheckSymbolIndexCancel(member_i++, error, "AOT symbol-index hashdecl-member collection")) {
                return false;
            }
            parts.push_back(mi.first);
            parts.push_back(aotTypePathString(mi.second->getTypeInfo()));
        }
        rec.declaration_hash = aotHashParts(parts);
        defined.push_back(std::move(rec));
    }

    for (size_t i = 0; i < state.enums.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index enum collection")) {
            return false;
        }
        const QoreEnumDecl* ed = state.enums[i].ed;
        const qore_enum_decl_private* edp = qore_enum_decl_private::get(*ed);
        std::string enum_path = aotStripLeadingColons(ed->getNamespacePath());
        std::vector<std::string> parts = {
            "enum",
            enum_path,
            aotVisibility(ed->isPublic()),
            aotTypePathString(ed->getBaseTypeInfo()),
        };
        QoreEnumMemberIterator emi(*ed);
        size_t member_i = 0;
        while (emi.next()) {
            if (!aotCheckSymbolIndexCancel(member_i++, error, "AOT symbol-index enum-member collection")) {
                return false;
            }
            QoreValue value = emi.getValue();
            parts.push_back(emi.getName() ? emi.getName() : "");
            parts.push_back(aotValueHash(value));

            QoreAOTSymbolIndexRecord member_rec;
            member_rec.kind = QoreAOTSymbolKind::ENUM_MEMBER;
            member_rec.dependency_class = QoreAOTDependencyClass::QORE_VALUE;
            member_rec.metadata_slot = static_cast<uint32_t>(i);
            member_rec.qore_path = aotJoinPath(enum_path, emi.getName());
            member_rec.source_file = aotLocationFile(edp->getParseLocation());
            member_rec.visibility = aotVisibility(ed->isPublic());
            member_rec.value_hash = aotValueHash(value);
            member_rec.declaration_hash = aotHashParts({
                "enum_member",
                member_rec.qore_path,
                member_rec.visibility,
                member_rec.value_hash,
            });
            defined.push_back(std::move(member_rec));
        }

        QoreAOTSymbolIndexRecord rec;
        rec.kind = QoreAOTSymbolKind::ENUM;
        rec.dependency_class = QoreAOTDependencyClass::QORE_API;
        rec.metadata_slot = static_cast<uint32_t>(i);
        rec.qore_path = std::move(enum_path);
        rec.source_file = aotLocationFile(edp->getParseLocation());
        rec.visibility = aotVisibility(ed->isPublic());
        rec.declaration_hash = aotHashParts(parts);
        rec.value_hash = rec.declaration_hash;
        defined.push_back(std::move(rec));
    }

    for (size_t i = 0; i < state.typedefs.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index typedef collection")) {
            return false;
        }
        const auto& ti = state.typedefs[i];
        QoreAOTSymbolIndexRecord rec;
        rec.kind = QoreAOTSymbolKind::TYPEDEF;
        rec.dependency_class = QoreAOTDependencyClass::QORE_API;
        rec.metadata_slot = static_cast<uint32_t>(i);
        rec.qore_path = aotJoinPath(aotNamespacePath(state, ti.ns_idx), ti.name.c_str());
        rec.source_file = aotLocationFile(ti.loc);
        rec.visibility = aotVisibility(ti.pub);
        rec.declaration_hash = aotHashParts({
            "typedef",
            rec.qore_path,
            rec.visibility,
            aotTypePathString(ti.typeInfo),
        });
        defined.push_back(std::move(rec));
    }

    for (size_t i = 0; i < state.constants.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index constant collection")) {
            return false;
        }
        const ConstantEntry* ce = state.constants[i].entry;
        qore_ns_private* ns = state.constants[i].ns_idx < state.namespaces.size()
            ? state.namespaces[state.constants[i].ns_idx].ns : nullptr;
        QoreAOTSymbolIndexRecord rec;
        rec.kind = QoreAOTSymbolKind::CONSTANT;
        rec.dependency_class = ce->hasInitExpr()
            ? QoreAOTDependencyClass::QORE_API : QoreAOTDependencyClass::QORE_VALUE;
        rec.metadata_slot = static_cast<uint32_t>(i);
        rec.qore_path = getNamespaceConstantPath(ns, ce->getName());
        rec.source_file = aotLocationFile(ce->loc);
        rec.visibility = aotVisibility(ce->getAccess());
        rec.value_hash = aotValueHashForConstant(ce);
        rec.declaration_hash = aotHashParts({
            "constant",
            rec.qore_path,
            rec.visibility,
            aotTypePathString(ce->typeInfo),
            ce->hasInitExpr() ? "pending" : "literal",
        });
        defined.push_back(std::move(rec));
    }

    for (size_t i = 0; i < state.globals.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index global collection")) {
            return false;
        }
        Var* var = state.globals[i].var;
        QoreAOTSymbolIndexRecord rec;
        rec.kind = QoreAOTSymbolKind::GLOBAL;
        rec.dependency_class = QoreAOTDependencyClass::QORE_API;
        rec.metadata_slot = static_cast<uint32_t>(i);
        rec.qore_path = aotJoinPath(aotNamespacePath(state, state.globals[i].ns_idx), var->getName());
        rec.source_file = aotLocationFile(var->getParseLocation());
        rec.visibility = aotVisibility(var->isPublic());
        rec.declaration_hash = aotHashParts({
            "global",
            rec.qore_path,
            rec.visibility,
            aotTypePathString(var->getTypeInfo(), var->isNoNarrowing()),
            var->isThreadLocal() ? "thread_local" : "global",
        });
        defined.push_back(std::move(rec));
    }

    for (size_t i = 0; i < state.functions.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index function collection")) {
            return false;
        }
        const auto& fi = state.functions[i];
        std::string base = aotJoinPath(aotNamespacePath(state, fi.ns_idx), fi.entry->getName());
        QoreFunctionIterator qfi(*fi.func);
        uint32_t variant_slot = 0;
        size_t variant_i = 0;
        while (qfi.next()) {
            if (!aotCheckSymbolIndexCancel(variant_i++, error, "AOT symbol-index function-variant collection")) {
                return false;
            }
            const AbstractQoreFunctionVariant* v = qfi.getVariant();
            if (!v->isUser()) {
                continue;
            }
            std::string source_file = aotFunctionVariantSourceFile(v);
            if (shouldSkipByCompileFile(source_file.empty() ? nullptr : source_file.c_str(),
                    compile_file, compile_files)) {
                continue;
            }
            std::string key = aotCallableDisplayKey(base.c_str(), v);
            QoreAOTSymbolIndexRecord rec;
            rec.kind = QoreAOTSymbolKind::FUNCTION;
            rec.dependency_class = QoreAOTDependencyClass::QORE_API;
            rec.metadata_slot = (static_cast<uint32_t>(i) << 16)
                | std::min<uint32_t>(variant_slot, UINT16_MAX);
            rec.qore_path = key;
            rec.source_file = std::move(source_file);
            rec.visibility = aotVisibility(fi.entry->isPublic());
            rec.signature_hash = aotSignatureSurface(key, v);
            rec.declaration_hash = aotFunctionVariantDeclHash("function", key, rec.visibility, v);
            if (native_symbol_map) {
                auto it = native_symbol_map->find(key);
                if (it != native_symbol_map->end()) {
                    rec.native_symbol = it->second;
                    aotAddNativeRecord(native, key, it->second, "qore_body");
                }
            }
            if (fast_entry_map) {
                auto fast_it = fast_entry_map->find(v);
                if (fast_it != fast_entry_map->end()) {
                    aotAddFastEntryRecord(native, key, fast_it->second);
                }
            }
            defined.push_back(std::move(rec));
            ++variant_slot;
        }
    }

    for (size_t i = 0; i < state.methods.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index method collection")) {
            return false;
        }
        const auto& mi = state.methods[i];
        if (mi.class_idx >= state.classes.size()) {
            continue;
        }
        const QoreClass* qc = state.classes[mi.class_idx].cls;
        const QoreMethod* method = mi.method;
        const qore_method_private* mp = qore_method_private::get(*method);
        const MethodFunctionBase* mfb = mp->func;
        QoreFunctionIterator qfi(*static_cast<const QoreFunction*>(mfb));
        uint32_t variant_slot = 0;
        size_t variant_i = 0;
        while (qfi.next()) {
            if (!aotCheckSymbolIndexCancel(variant_i++, error, "AOT symbol-index method-variant collection")) {
                return false;
            }
            const AbstractQoreFunctionVariant* v = qfi.getVariant();
            if (!isAOTSerializableMethodVariant(method, v)) {
                continue;
            }
            std::string source_file = aotFunctionVariantSourceFile(v);
            if (shouldSkipByCompileFile(source_file.empty() ? nullptr : source_file.c_str(),
                    compile_file, compile_files)) {
                continue;
            }
            const MethodVariantBase* mvb = reinterpret_cast<const MethodVariantBase*>(v);
            bool is_constructor = strcmp(method->getName(), "constructor") == 0;
            std::string key = aotMethodDisplayKey(qc, method->getName(), v);
            std::string native_key = getAOTMethodVariantKey(qc, method->getName(), mi.is_static, v);
            QoreAOTSymbolIndexRecord rec;
            rec.kind = is_constructor ? QoreAOTSymbolKind::CONSTRUCTOR
                : (mi.is_static ? QoreAOTSymbolKind::STATIC_METHOD : QoreAOTSymbolKind::METHOD);
            rec.dependency_class = QoreAOTDependencyClass::QORE_API;
            rec.metadata_slot = (static_cast<uint32_t>(i) << 16)
                | std::min<uint32_t>(variant_slot, UINT16_MAX);
            rec.qore_path = key;
            rec.source_file = std::move(source_file);
            rec.visibility = aotVisibility(mvb->getAccess());
            rec.signature_hash = aotSignatureSurface(key, v);
            rec.declaration_hash = aotMethodVariantDeclHash(key, rec.visibility, v, mi.is_static);
            if (native_symbol_map) {
                auto it = native_symbol_map->find(native_key);
                if (it != native_symbol_map->end()) {
                    rec.native_symbol = it->second;
                    aotAddNativeRecord(native, key, it->second, "qore_body");
                }
            }
            if (fast_entry_map) {
                auto fast_it = fast_entry_map->find(v);
                if (fast_it != fast_entry_map->end()) {
                    aotAddFastEntryRecord(native, key, fast_it->second);
                }
            }
            defined.push_back(std::move(rec));
            ++variant_slot;
        }
    }

    if (init_native_symbol_map) {
        size_t i = 0;
        for (const auto& entry : *init_native_symbol_map) {
            if (!aotCheckSymbolIndexCancel(i++, error, "AOT symbol-index init-native collection")) {
                return false;
            }
            aotAddNativeRecord(native, entry.first, entry.second, "init_func");
        }
    }
    if (!aotAppendGlobalImportRecords(func_slots, imported, compile_file, compile_files, error)) {
        return false;
    }
    if (!aotAppendStaticMemberImportRecords(func_slots, imported, compile_file, compile_files, error)) {
        return false;
    }
    if (!aotAppendCallImportRecords(func_slots, imported, compile_file, compile_files, error)) {
        return false;
    }
    if (!aotAppendBCAImportRecords(state, imported, compile_file, compile_files, error)) {
        return false;
    }

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::SYMBOL_INDEX);
    writer.writeU16(QORE_AOT_SYMBOL_INDEX_VERSION);
    writer.writeU16(0);

    writer.writeU32(static_cast<uint32_t>(context.size()));
    for (size_t i = 0; i < context.size(); ++i) {
        if (!aotCheckSymbolIndexCancel(i, error, "AOT symbol-index context serialization")) {
            return false;
        }
        writer.writeStringRef(context[i].first.c_str());
        writer.writeStringRef(context[i].second.c_str());
    }

    if (!writeSymbolIndexRecordVector(writer, defined, error,
            "AOT symbol-index definition serialization")
            || !writeSymbolIndexRecordVector(writer, imported, error,
                "AOT symbol-index import serialization")
            || !writeSymbolIndexRecordVector(writer, native, error,
                "AOT symbol-index native serialization")) {
        return false;
    }

    writer.endSection(sec_idx);
    return true;
}

static bool readSymbolIndexString(const QoreAOTBinaryReader& reader, const uint8_t*& ptr,
        const uint8_t* end, std::string& out, std::string& error, const char* field) {
    if (ptr + sizeof(uint32_t) > end) {
        error = "truncated SYMBOL_INDEX string field '";
        error += field ? field : "<unknown>";
        error += "'";
        return false;
    }
    const char* s = reader.readStringRef(ptr);
    if (!s) {
        error = "invalid SYMBOL_INDEX string reference in field '";
        error += field ? field : "<unknown>";
        error += "'";
        return false;
    }
    out = s;
    return true;
}

static bool readSymbolIndexByteVector(const uint8_t*& ptr, const uint8_t* end,
        std::vector<uint8_t>& values, std::string& error, const char* field) {
    if (ptr + sizeof(uint32_t) > end) {
        error = "truncated SYMBOL_INDEX byte-vector field '";
        error += field;
        error += "'";
        return false;
    }
    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    if (count > static_cast<uint32_t>(end - ptr)) {
        error = "invalid SYMBOL_INDEX byte-vector field '";
        error += field;
        error += "'";
        return false;
    }
    values.assign(ptr, ptr + count);
    ptr += count;
    return true;
}

static bool readSymbolIndexRecord(const QoreAOTBinaryReader& reader, const uint8_t*& ptr,
        const uint8_t* end, uint16_t version, QoreAOTSymbolIndexRecord& rec,
        std::string& error) {
    if (ptr + 8 > end) {
        error = "truncated SYMBOL_INDEX record header";
        return false;
    }
    rec.kind = static_cast<QoreAOTSymbolKind>(QoreAOTBinaryReader::readU8(ptr));
    rec.dependency_class = static_cast<QoreAOTDependencyClass>(QoreAOTBinaryReader::readU8(ptr));
    rec.flags = QoreAOTBinaryReader::readU16(ptr);
    rec.metadata_slot = QoreAOTBinaryReader::readU32(ptr);

    if (!(readSymbolIndexString(reader, ptr, end, rec.qore_path, error, "qore_path")
        && readSymbolIndexString(reader, ptr, end, rec.source_file, error, "source_file")
        && readSymbolIndexString(reader, ptr, end, rec.visibility, error, "visibility")
        && readSymbolIndexString(reader, ptr, end, rec.signature_hash, error, "signature_hash")
        && readSymbolIndexString(reader, ptr, end, rec.declaration_hash, error, "declaration_hash")
        && readSymbolIndexString(reader, ptr, end, rec.value_hash, error, "value_hash")
        && readSymbolIndexString(reader, ptr, end, rec.native_symbol, error, "native_symbol")
        && readSymbolIndexString(reader, ptr, end, rec.abi_kind, error, "abi_kind")
        && readSymbolIndexString(reader, ptr, end, rec.consumer_source_file, error, "consumer_source_file")
        && readSymbolIndexString(reader, ptr, end, rec.provider_source_file, error, "provider_source_file"))) {
        return false;
    }
    if (version < 2) {
        return true;
    }
    if (ptr + 2 * sizeof(uint32_t) > end) {
        error = "truncated SYMBOL_INDEX fast-entry metadata";
        return false;
    }
    rec.fast_entry_flags = QoreAOTBinaryReader::readU32(ptr);
    rec.fast_entry_num_params = QoreAOTBinaryReader::readU32(ptr);
    if (version >= 3) {
        if (ptr >= end) {
            error = "truncated SYMBOL_INDEX fast-entry return metadata";
            return false;
        }
        rec.fast_return_kind = QoreAOTBinaryReader::readU8(ptr);
    }
    if (!(readSymbolIndexByteVector(ptr, end, rec.fast_param_kinds, error,
            "fast_param_kinds")
        && readSymbolIndexByteVector(ptr, end, rec.fast_param_rejects_nothing,
            error, "fast_param_rejects_nothing")
        && readSymbolIndexByteVector(ptr, end, rec.fast_param_noescape, error,
            "fast_param_noescape"))) {
        return false;
    }
    if (version < 4) {
        return true;
    }
    constexpr size_t scalar_leaf_size = 1 + 2 + 2 + 4 * sizeof(uint64_t);
    if (static_cast<size_t>(end - ptr) < scalar_leaf_size) {
        error = "truncated SYMBOL_INDEX scalar leaf metadata";
        return false;
    }
    rec.scalar_leaf_kind = QoreAOTBinaryReader::readU8(ptr);
    rec.scalar_leaf_opcode = QoreAOTBinaryReader::readU16(ptr);
    rec.scalar_leaf_lhs_param = static_cast<int8_t>(QoreAOTBinaryReader::readU8(ptr));
    rec.scalar_leaf_rhs_param = static_cast<int8_t>(QoreAOTBinaryReader::readU8(ptr));
    rec.scalar_leaf_lhs_int = QoreAOTBinaryReader::readI64(ptr);
    rec.scalar_leaf_rhs_int = QoreAOTBinaryReader::readI64(ptr);
    rec.scalar_leaf_lhs_float = QoreAOTBinaryReader::readF64(ptr);
    rec.scalar_leaf_rhs_float = QoreAOTBinaryReader::readF64(ptr);
    if (version < 5) {
        return true;
    }
    constexpr size_t affine_select_size = 4 * sizeof(uint64_t);
    if (static_cast<size_t>(end - ptr) < affine_select_size) {
        error = "truncated SYMBOL_INDEX affine select metadata";
        return false;
    }
    rec.scalar_leaf_true_scale = QoreAOTBinaryReader::readI64(ptr);
    rec.scalar_leaf_true_offset = QoreAOTBinaryReader::readI64(ptr);
    rec.scalar_leaf_false_scale = QoreAOTBinaryReader::readI64(ptr);
    rec.scalar_leaf_false_offset = QoreAOTBinaryReader::readI64(ptr);
    if (version < 6) {
        return true;
    }
    if (!readSymbolIndexString(reader, ptr, end, rec.object_getter_member,
            error, "object_getter_member")) {
        return false;
    }
    return true;
}

static bool readSymbolIndexRecordVector(const QoreAOTBinaryReader& reader, const uint8_t*& ptr,
        const uint8_t* end, std::vector<QoreAOTSymbolIndexRecord>& records,
        uint16_t version, std::string& error, const char* label) {
    if (ptr + sizeof(uint32_t) > end) {
        error = "truncated SYMBOL_INDEX ";
        error += label ? label : "record";
        error += " count";
        return false;
    }
    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    if (count > static_cast<uint32_t>((end - ptr) / 48)) {
        error = "invalid SYMBOL_INDEX ";
        error += label ? label : "record";
        error += " count";
        return false;
    }
    records.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT symbol-index read")) {
            error = "operation cancelled during AOT symbol-index read";
            return false;
        }
        QoreAOTSymbolIndexRecord rec;
        if (!readSymbolIndexRecord(reader, ptr, end, version, rec, error)) {
            return false;
        }
        records.push_back(std::move(rec));
    }
    return true;
}

bool readSymbolIndex(const QoreAOTBinaryReader& reader, QoreAOTSymbolIndex& index,
        std::string& error) {
    index = QoreAOTSymbolIndex();

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::SYMBOL_INDEX);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid SYMBOL_INDEX section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;
    if (ptr + 4 > end) {
        error = "truncated SYMBOL_INDEX header";
        return false;
    }
    index.version = QoreAOTBinaryReader::readU16(ptr);
    uint16_t reserved = QoreAOTBinaryReader::readU16(ptr);
    if (reserved != 0) {
        error = "invalid SYMBOL_INDEX reserved field";
        return false;
    }
    if (index.version < 1 || index.version > QORE_AOT_SYMBOL_INDEX_VERSION) {
        error = "unsupported SYMBOL_INDEX version ";
        error += std::to_string(index.version);
        return false;
    }

    if (ptr + sizeof(uint32_t) > end) {
        error = "truncated SYMBOL_INDEX context count";
        return false;
    }
    uint32_t context_count = QoreAOTBinaryReader::readU32(ptr);
    if (context_count > static_cast<uint32_t>((end - ptr) / 8)) {
        error = "invalid SYMBOL_INDEX context count";
        return false;
    }
    index.context.reserve(context_count);
    for (uint32_t i = 0; i < context_count; ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT symbol-index context read")) {
            error = "operation cancelled during AOT symbol-index context read";
            return false;
        }
        std::string key;
        std::string value;
        if (!readSymbolIndexString(reader, ptr, end, key, error, "context.key")
                || !readSymbolIndexString(reader, ptr, end, value, error, "context.value")) {
            return false;
        }
        index.context.emplace_back(std::move(key), std::move(value));
    }

    if (!readSymbolIndexRecordVector(reader, ptr, end, index.defined, index.version,
                error, "defined")
            || !readSymbolIndexRecordVector(reader, ptr, end, index.imported,
                index.version, error, "imported")
            || !readSymbolIndexRecordVector(reader, ptr, end, index.native,
                index.version, error, "native")) {
        return false;
    }
    if (ptr != end) {
        error = "trailing bytes in SYMBOL_INDEX section";
        return false;
    }
    return true;
}

static bool readCallRelocationString(const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end, std::string& out,
        std::string& error, const char* field) {
    if (ptr + 4 > end) {
        error = "truncated CALL_RELOCATIONS string field '";
        error += field ? field : "?";
        error += "'";
        return false;
    }
    const char* s = reader.readStringRef(ptr);
    if (!s) {
        error = "invalid CALL_RELOCATIONS string reference in field '";
        error += field ? field : "?";
        error += "'";
        return false;
    }
    out = s;
    return true;
}

static bool readCallRelocationRecord(const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end,
        QoreAOTCallRelocationRecord& rec, std::string& error) {
    if (ptr + 12 > end) {
        error = "truncated CALL_RELOCATIONS record header";
        return false;
    }
    if (!readCallRelocationString(reader, ptr, end, rec.function_name, error, "function_name")) {
        return false;
    }
    rec.expr_slot = QoreAOTBinaryReader::readU32(ptr);
    uint8_t target_kind = QoreAOTBinaryReader::readU8(ptr);
    uint8_t strictness = QoreAOTBinaryReader::readU8(ptr);
    uint16_t reserved = QoreAOTBinaryReader::readU16(ptr);
    if (reserved) {
        error = "invalid CALL_RELOCATIONS record reserved field";
        return false;
    }
    if (target_kind > static_cast<uint8_t>(QoreAOTCallRelocationTargetKind::CONSTRUCTOR)) {
        error = "invalid CALL_RELOCATIONS target kind ";
        error += std::to_string(target_kind);
        return false;
    }
    if (strictness > static_cast<uint8_t>(QoreAOTCallRelocationStrictness::REQUIRED)) {
        error = "invalid CALL_RELOCATIONS strictness ";
        error += std::to_string(strictness);
        return false;
    }
    rec.target_kind = static_cast<QoreAOTCallRelocationTargetKind>(target_kind);
    rec.strictness = static_cast<QoreAOTCallRelocationStrictness>(strictness);
    return readCallRelocationString(reader, ptr, end, rec.qore_path, error, "qore_path")
        && readCallRelocationString(reader, ptr, end, rec.signature_hash, error, "signature_hash")
        && readCallRelocationString(reader, ptr, end, rec.declaration_hash, error, "declaration_hash")
        && readCallRelocationString(reader, ptr, end, rec.native_symbol, error, "native_symbol")
        && readCallRelocationString(reader, ptr, end, rec.fallback_descriptor, error, "fallback_descriptor");
}

bool readCallRelocations(const QoreAOTBinaryReader& reader, QoreAOTCallRelocations& relocs,
        std::string& error) {
    relocs = QoreAOTCallRelocations();
    error.clear();

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::CALL_RELOCATIONS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid CALL_RELOCATIONS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;
    if (ptr + 8 > end) {
        error = "truncated CALL_RELOCATIONS header";
        return false;
    }
    relocs.version = QoreAOTBinaryReader::readU16(ptr);
    uint16_t reserved = QoreAOTBinaryReader::readU16(ptr);
    if (reserved) {
        error = "invalid CALL_RELOCATIONS reserved field";
        return false;
    }
    if (relocs.version != QORE_AOT_CALL_RELOCATIONS_VERSION) {
        error = "unsupported CALL_RELOCATIONS version ";
        error += std::to_string(relocs.version);
        return false;
    }
    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    if (count > 1000000) {
        error = "invalid CALL_RELOCATIONS record count";
        return false;
    }
    relocs.records.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT call-relocation read")) {
            error = "operation cancelled during AOT call-relocation read";
            return false;
        }
        QoreAOTCallRelocationRecord rec;
        if (!readCallRelocationRecord(reader, ptr, end, rec, error)) {
            return false;
        }
        relocs.records.push_back(std::move(rec));
    }
    if (ptr != end) {
        error = "trailing bytes in CALL_RELOCATIONS section";
        return false;
    }
    return true;
}

namespace {

//! Write a function/method variant signature
static void writeVariantSignature(QoreAOTBinaryWriter& writer, const AbstractQoreFunctionVariant* v) {
    const AbstractFunctionSignature* sig = const_cast<AbstractQoreFunctionVariant*>(v)->getSignature();
    assert(sig);

    // return type path — emitted as a u32 index into the per-blob
    // TYPE_TABLE (see QoreAOTBinaryWriter::internTypePath).  At read
    // time the deserializer uses this index for an O(1) array lookup
    // against a pre-resolved `const QoreTypeInfo*` table instead of a
    // hash lookup on the path string per param (cf. the original
    // `writer.writeStringRef(getTypePath(...))` path).
    writer.writeU32(internTypePath(writer, sig->getReturnTypeInfo()));

    // num params
    uint32_t np = sig->numParams();
    writer.writeU32(np);

    // flags:
    //   bit  0 = effective varargs  (v->hasVarargs(), OR of sig-ellipsis and QCF_USES_EXTRA_ARGS)
    //   bit  1 = is_user
    //   bit  2 = signature literally has the `...` ellipsis (sig->hasVarargs())
    //   bit 15 = format marker: "bits 2+ are meaningful" (new-format qmod)
    //
    // Bits 0 and 2 together let the reader separate two distinct
    // concepts that the pre-bit-2 format conflated:
    //
    //   * `sub zip() { ...argv... }` — body uses `$argv`/`$N` so the
    //     parser sets QCF_USES_EXTRA_ARGS on the VARIANT, but the
    //     SIGNATURE has no ellipsis.  Flag must round-trip so overload
    //     resolution finds the variant for callers that pass more args
    //     than the declared signature (Function.cpp:1208 assertion,
    //     fixed in c6f92f071).
    //
    //   * `f(...)` — signature literally declares an ellipsis.  The
    //     SIGNATURE must carry the ellipsis flag so that
    //     isSignatureIdentical() comparisons (abstract/concrete method
    //     matching in qore_class_private::parseCommit / AOT abstract-
    //     resolution) correctly report the signature shape.
    //
    // The pre-format-marker writer set bit 0 to the OR of the two
    // concepts and the reader set BOTH sig->varargs and
    // QCF_USES_EXTRA_ARGS from that single bit.  That spuriously
    // promoted "body uses argv" into "signature has ellipsis", breaking
    // abstract-override matching whenever a concrete override's body
    // references $argv/$N — e.g. `RestPingPollOperation::continuePoll()`
    // with `on_error rethrow $1.err, ...` AOT-serialized with its
    // signature sprouting a spurious `(...)` so the class stayed
    // abstract on .qmod load, which in turn broke
    // `MewsRestClient`/`SalesforceRestClient`/etc. init.
    //
    // Bit 15 is a format marker so new readers can distinguish new
    // qmods (interpret bits 0 and 2 independently) from old qmods
    // (fall back to the old conflated-bit-0 semantics, preserving
    // bug-compatible behavior for unrebuilt artifacts).
    uint16_t flags = 0x8000;   // bit 15: new-format marker
    if (v->hasVarargs()) {
        flags |= 0x0001;
    }
    if (v->isUser()) {
        flags |= 0x0002;
    }
    if (sig->hasVarargs()) {
        flags |= 0x0004;
    }
    writer.writeU16(flags);

    // Per-variant signature start/end lines — plumbed into the reader's
    // setupFromAOTMetadata call so `sig->getParseLocation()` reports real
    // line numbers instead of 0.  Gated on QORE_AOT_FEAT_SIG_LINES so older
    // readers skip these bytes and newer readers expect them.  Stored as
    // int16_t to match QoreProgramLineLocation's on-heap representation.
    UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(v)->getUserVariantBase();
    int16_t sig_first = 0, sig_last = 0;
    if (uvb) {
        if (UserSignature* usig = uvb->getUserSignature()) {
            if (const QoreProgramLocation* vloc = usig->getParseLocation()) {
                sig_first = vloc->start_line;
                sig_last  = vloc->end_line;
            }
        }
    }
    writer.writeU16(static_cast<uint16_t>(sig_first));
    writer.writeU16(static_cast<uint16_t>(sig_last));

    // Function-entry StatementBlock line range.  This is distinct from the
    // signature location: source-stripped AOT keeps no executable body, but
    // ProgramControl::findFunctionStatementId() must still resolve to a stable
    // statement id with source-like entry location metadata.
    int16_t entry_first = 0, entry_last = 0;
    if (uvb) {
        if (StatementBlock* sb = uvb->getStatementBlock()) {
            if (const QoreProgramLocation* sloc = sb->loc) {
                entry_first = sloc->start_line;
                entry_last = sloc->end_line;
            }
        }
    }
    writer.writeU16(static_cast<uint16_t>(entry_first));
    writer.writeU16(static_cast<uint16_t>(entry_last));

    // Source-stripped variants do not keep executable AST statement bodies.
    // Preserve the body block parse options so runtime dispatch still applies
    // the same domain gates as source execution.
    QoreParseOptions variant_po;
    if (uvb) {
        if (StatementBlock* sb = uvb->getStatementBlock()) {
            variant_po = sb->pwo.parse_options;
        } else {
            variant_po = v->getParseOptions(QoreParseOptions());
        }
    }
    writer.writeI64(variant_po.getLo());
    writer.writeI64(variant_po.getHi());

    // params
    const arg_vec_t& defaults = sig->getDefaultArgList();
    auto write_native_expr_default = [&writer](const QoreValue& dv) -> bool {
        uint32_t start_pos = writer.position();
        writer.writeU8(1);
        writer.writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_EXPR_NATIVE));
        uint32_t size_pos = writer.position();
        writer.writeU32(0);
        uint32_t payload_pos = writer.position();

        static const std::vector<AOTLocalSlotId> no_locals;
        static const std::vector<AOTGlobalSlotId> no_globals;
        qoreAOTClearExprSerializationError();
        bool ok = classifyAndWriteExpr(writer, dv, no_locals, no_globals,
            writer.const_reverse_map);
        std::string expr_error;
        bool expr_error_set = qoreAOTTakeExprSerializationError(expr_error);
        if (!ok || expr_error_set || writer.position() == payload_pos) {
            writer.truncate(start_pos);
            return false;
        }

        writer.patchU32(size_pos, writer.position() - payload_pos);
        return true;
    };

    for (uint32_t i = 0; i < np; ++i) {
        // param name
        const char* pname = sig->getName(i);
        writer.writeStringRef(pname ? pname : "");

        // param type path — u32 index into per-blob TYPE_TABLE (see
        // return-type comment above).
        writer.writeU32(internTypePath(writer, sig->getParamTypeInfo(i)));
        if ((writer.feature_flags & QORE_AOT_FEAT_READONLY_LOCALS) != 0) {
            writer.writeU8(sig->isParamReadOnly(i) ? 0x01 : 0x00);
        }

        // default argument
        bool has_default = sig->hasDefaultArg(i);
        if (has_default && i < static_cast<uint32_t>(defaults.size())) {
            QoreValue dv = defaults[i];
            // Check if the default value is a serializable constant type.
            // AST expression nodes (function calls, variable refs, etc.) have types
            // not in the switch list and would be serialized as VT_NOTHING, which
            // would make the parameter appear required. Use VT_OPAQUE_DEFAULT instead
            // to preserve the "has default" semantics.
            qore_type_t dt = dv.getType();
            if (dt == NT_SCOPE_REF) {
                const AbstractQoreNode* node = dv.getInternalNode();
                if (auto* socn = dynamic_cast<const ScopedObjectCallNode*>(node)) {
                    const QoreListNode* args = socn->getArgs();
                    const QoreParseListNode* parse_args = socn->getParseArgs();
                    if (socn->oc && (!args || args->empty()) && (!parse_args || parse_args->empty())) {
                        writer.writeU8(1);
                        writer.writeValue(dv);
                        continue;
                    }
                }
            }

            if (dv.isNothing() || dv.isNull() || dt == NT_BOOLEAN || dt == NT_INT || dt == NT_CHAR
                    || dt == NT_FLOAT || dt == NT_STRING || dt == NT_DATE
                    || dt == NT_NUMBER || dt == NT_BINARY || dt == NT_LIST
                    || dt == NT_HASH || dt == NT_OBJECT) {
                // Note: NT_OBJECT is routed through writeValue so the writer's
                // CRM lookup can emit VT_CONST_REF for parse-folded Type-class
                // constants (e.g. `*Type t = IntType` default args).
                writer.writeU8(1);
                writer.writeValue(dv);
            } else if (dv.hasNode()) {
                const AbstractQoreNode* node = dv.getInternalNode();

                // Helper: resolve the FQN of a constant referenced by node
                // chain (either a directly-registered CRM pointer, or a
                // RuntimeConstantRefNode wrapping a ConstantEntry whose
                // val/saved_val node is in the CRM).
                auto resolveConstFqn = [&writer](const AbstractQoreNode* n) -> std::string {
                    if (!writer.const_reverse_map || !n) {
                        return std::string();
                    }
                    auto it = writer.const_reverse_map->find(n);
                    if (it != writer.const_reverse_map->end()) {
                        return it->second;
                    }
                    if (auto* rcr = dynamic_cast<const RuntimeConstantRefNode*>(n)) {
                        std::string path;
                        if (qore_aot_resolve_runtime_constant_path(rcr, writer.const_reverse_map, path)) {
                            return path;
                        }
                    }
                    return std::string();
                };

                // Case 1: no-arg function call default (e.g., getcwd(), now())
                auto* fcn = dynamic_cast<const FunctionCallNode*>(node);
                if (fcn && fcn->getName() && (!fcn->getArgs() || fcn->getArgs()->empty())) {
                    writer.writeU8(2);  // expression default: function call
                    writer.writeStringRef(fcn->getName());
                    continue;
                }

                // Case 2: RuntimeConstantRefNode — a plain constant reference.
                //   e.g. `hash<auto> options = DefaultsMap`. At call time we want
                //   to return the constant's current value, so serialize the
                //   constant's FQN and rebuild a RuntimeConstantRefNode at load.
                if (auto* rcr = dynamic_cast<const RuntimeConstantRefNode*>(node)) {
                    (void)rcr;  // silence unused-if-no-CRM warning
                    std::string fqn = resolveConstFqn(node);
                    if (!fqn.empty()) {
                        writer.writeU8(3);  // expression default: constant ref
                        writer.writeStringRef(fqn.c_str());
                        continue;
                    }
                }

                // Case 3: QoreDotEvalOperatorNode — method call on a constant,
                //   e.g. `AutoHashType.getName()`. We only support the no-arg
                //   form with a constant-ref left-hand side. The reader builds
                //   a fresh QoreDotEvalOperatorNode whose `left` is a new
                //   RuntimeConstantRefNode — the method is then resolved by
                //   dynamic dispatch at each call.
                if (auto* de = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
                    MethodCallNode* mc = de->getMethodCall();
                    if (mc && mc->getName() && (!mc->getArgs() || mc->getArgs()->empty())) {
                        QoreValue left = de->getExpression();
                        std::string fqn;
                        if (left.hasNode()) {
                            fqn = resolveConstFqn(left.getInternalNode());
                        }
                        if (!fqn.empty()) {
                            writer.writeU8(4);  // expression default: const.method()
                            writer.writeStringRef(fqn.c_str());
                            writer.writeStringRef(mc->getName());
                            continue;
                        }
                    }
                }

                // Case 4: QoreHashDeclCastOperatorNode — `<Hashdecl>{...}` typed
                //   hash literal, commonly used as a param default like
                //   `hash<AuthCodeInfo> info = <AuthCodeInfo>{}`. The inner
                //   expression is typically a parse-time-folded QoreHashNode
                //   (empty or constant). Serialize the hashdecl path plus the
                //   inner hash so the reader can rebuild a
                //   QoreHashDeclCastOperatorNode whose eval produces a properly
                //   initialized hashdecl instance.
                if (auto* hdc = dynamic_cast<const QoreHashDeclCastOperatorNode*>(node)) {
                    const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(
                        hdc->getCastTypeInfo());
                    QoreValue inner = hdc->getExp();
                    qore_type_t itype = inner.getType();
                    bool inner_ok = inner.isNothing() || itype == NT_HASH;
                    if (hd && inner_ok) {
                        writer.writeU8(5);  // expression default: hashdecl cast
                        writer.writeStringRef(hd->getNamespacePath().c_str());
                        writer.writeU8(hdc->isOrNothing() ? 1 : 0);
                        writer.writeU8(inner.isNothing() ? 0 : 1);
                        if (!inner.isNothing()) {
                            writer.writeValue(inner);
                        }
                        continue;
                    }
                }

                // Case 5: StaticMethodCallNode — no-arg static method call, e.g.
                //   `string boundary = MultiPartMessage::getBoundary()`. Serialize
                //   the class path and method name; the reader rebuilds a
                //   StaticMethodCallNode bound to the resolved QoreMethod.
                if (auto* smcn = dynamic_cast<const StaticMethodCallNode*>(node)) {
                    const QoreListNode* args = smcn->getArgs();
                    bool no_args = !args || args->size() == 0;
                    const QoreMethod* m = smcn->getMethod();
                    const QoreClass* qc = m ? m->getClass() : nullptr;
                    const char* mname = smcn->getName();
                    if (no_args && qc && mname && *mname) {
                        writer.writeU8(6);  // expression default: static method call
                        std::string class_ref = qore_aot_encode_class_ref(qc);
                        writer.writeStringRef(class_ref.c_str());
                        writer.writeStringRef(mname);
                        continue;
                    }
                }

                // Prefer native expression metadata; otherwise keep the legacy
                // opaque default marker instead of fabricating an executable fallback.
                if (write_native_expr_default(dv)) {
                    continue;
                }

                writer.writeU8(1);
                writer.writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_OPAQUE_DEFAULT));
            } else {
                // Unknown type — write opaque placeholder
                writer.writeU8(1);
                writer.writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_OPAQUE_DEFAULT));
            }
        } else {
            writer.writeU8(0);
        }
    }
}

//! Write NAMESPACES section
static void writeNamespacesSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::NAMESPACES);

    uint32_t count = static_cast<uint32_t>(state.namespaces.size());
    writer.writeU32(count);

    for (auto& nsi : state.namespaces) {
        const qore_ns_private* ns = nsi.ns;
        writer.writeStringRef(ns->name.c_str());
        writer.writeStringRef(ns->path.c_str());
        writer.writeU32(nsi.parent_idx);
        writer.writeU32(ns->depth);
        uint16_t flags = 0;
        if (ns->pub) {
            flags |= 0x0001;
        }
        if (ns->builtin) {
            flags |= 0x0002;
        }
        if (ns->root) {
            flags |= 0x0004;
        }
        writer.writeU16(flags);
    }

    writer.endSection(sec_idx);
}

static bool aotValueTagPreservesMemberDefault(const QoreValue& v) {
    if (!v.hasNode()) {
        return true;
    }
    if (v.isEnum()) {
        return true;
    }

    switch (v.getType()) {
        case NT_BOOLEAN:
        case NT_INT:
        case NT_CHAR:
        case NT_FLOAT:
        case NT_STRING:
        case NT_DATE:
        case NT_NUMBER:
        case NT_BINARY:
            return true;
        case NT_LIST: {
            const QoreListNode* list = v.get<const QoreListNode>();
            if (!list) {
                return true;
            }
            for (size_t i = 0; i < list->size(); ++i) {
                if (!aotValueTagPreservesMemberDefault(list->retrieveEntry(i))) {
                    return false;
                }
            }
            return true;
        }
        case NT_HASH: {
            const QoreHashNode* hash = v.get<const QoreHashNode>();
            if (!hash) {
                return true;
            }
            ConstHashIterator hi(*hash);
            while (hi.next()) {
                if (!aotValueTagPreservesMemberDefault(hi.get())) {
                    return false;
                }
            }
            return true;
        }
        case NT_SCOPE_REF: {
            const AbstractQoreNode* node = v.getInternalNode();
            if (const auto* socn = dynamic_cast<const ScopedObjectCallNode*>(node)) {
                const QoreListNode* args = socn->getArgs();
                const QoreParseListNode* parse_args = socn->getParseArgs();
                if (parse_args && !parse_args->empty() && (!args || args->empty())) {
                    return false;
                }
                if (!args) {
                    return true;
                }
                for (size_t i = 0; i < args->size(); ++i) {
                    if (!aotValueTagPreservesMemberDefault(args->retrieveEntry(i))) {
                        return false;
                    }
                }
                return true;
            }
            if (const auto* ncl = dynamic_cast<const NewComplexListNode*>(node)) {
                if (ncl->args.isNothing()) {
                    return true;
                }
                const QoreListNode* args = ncl->args.getType() == NT_LIST
                    ? ncl->args.get<const QoreListNode>() : nullptr;
                if (!args) {
                    return false;
                }
                for (size_t i = 0; i < args->size(); ++i) {
                    if (!aotValueTagPreservesMemberDefault(args->retrieveEntry(i))) {
                        return false;
                    }
                }
                return true;
            }
            if (const auto* ncb = dynamic_cast<const NewComplexBufferNode*>(node)) {
                if (ncb->args.isNothing()) {
                    return true;
                }
                const QoreListNode* args = ncb->args.getType() == NT_LIST
                    ? ncb->args.get<const QoreListNode>() : nullptr;
                if (!args) {
                    return false;
                }
                for (size_t i = 0; i < args->size(); ++i) {
                    if (!aotValueTagPreservesMemberDefault(args->retrieveEntry(i))) {
                        return false;
                    }
                }
                return true;
            }
            if (const auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
                if (!nch->args) {
                    return true;
                }
                for (size_t i = 0; i < nch->args->size(); ++i) {
                    if (!aotValueTagPreservesMemberDefault(nch->args->get(i))) {
                        return false;
                    }
                }
                return true;
            }
            if (const auto* nhd = dynamic_cast<const NewHashDeclNode*>(node)) {
                if (!nhd->args) {
                    return true;
                }
                for (size_t i = 0; i < nhd->args->size(); ++i) {
                    if (!aotValueTagPreservesMemberDefault(nhd->args->get(i))) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

enum class AOTMemberDefaultEncoding {
    Value,
    NativeExpr,
};

static bool writeNativeMemberDefaultExpr(QoreAOTBinaryWriter& writer, const QoreValue& v,
        const std::vector<AOTLocalSlotId>* parent_locals, std::string* error) {
    uint32_t start_pos = writer.position();
    writer.writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_EXPR_NATIVE));
    uint32_t size_pos = writer.position();
    writer.writeU32(0);
    uint32_t payload_pos = writer.position();

    static const std::vector<AOTLocalSlotId> no_locals;
    static const std::vector<AOTGlobalSlotId> no_globals;
    const std::vector<AOTLocalSlotId>& locals = parent_locals ? *parent_locals : no_locals;
    qoreAOTClearExprSerializationError();
    bool ok = classifyAndWriteExpr(writer, v, locals, no_globals,
        writer.const_reverse_map);
    std::string expr_error;
    bool expr_error_set = qoreAOTTakeExprSerializationError(expr_error);

    // classifyAndWriteExpr reports unsupported expressions by setting the
    // thread-local diagnostic and returning false before writing fallback
    // markers.  Roll back to keep the value stream well-formed.
    if (!ok || expr_error_set || writer.position() == payload_pos) {
        writer.truncate(start_pos);
        if (error) {
            if (expr_error_set) {
                *error = expr_error;
            } else if (!ok) {
                *error = "classifyAndWriteExpr returned false without a nested expression diagnostic";
            } else {
                *error = "native expression serializer produced an empty payload";
            }
        }
        return false;
    }

    writer.patchU32(size_pos, writer.position() - payload_pos);
    return true;
}

static bool writeMemberDefaultValue(QoreAOTBinaryWriter& writer, const QoreValue& v,
        const char* owner_kind, const char* owner_name, const char* member_name,
        std::string& error, AOTMemberDefaultEncoding* encoding = nullptr,
        const std::vector<AOTLocalSlotId>* parent_locals = nullptr) {
    if (!aotValueTagPreservesMemberDefault(v)) {
        std::string native_error;
        if (writeNativeMemberDefaultExpr(writer, v, parent_locals, &native_error)) {
            if (encoding) {
                *encoding = AOTMemberDefaultEncoding::NativeExpr;
            }
            return true;
        }

        error = "AOT cannot serialize ";
        error += owner_kind ? owner_kind : "member-owner";
        error += " '";
        error += owner_name ? owner_name : "<unknown>";
        error += "' member '";
        error += member_name ? member_name : "<unknown>";
        error += "' default without fallback";
        if (!native_error.empty()) {
            error += ": ";
            error += native_error;
        }
        error += "; default=";
        error += qoreAOTDescribeExpr(v);
        error += "; no fallback marker was emitted; add a native AOTExprKind serializer/reader or lower this "
            "member default to native IR";
        return false;
    }

    writer.writeValue(v);
    if (encoding) {
        *encoding = AOTMemberDefaultEncoding::Value;
    }
    return true;
}

static bool qoreAOTWriteDefaultArgValuePayloadImpl(QoreAOTBinaryWriter& writer, const QoreValue& v,
        const char* owner_kind, const char* owner_name, const char* param_name,
        std::string* error, const std::vector<AOTLocalSlotId>* parent_locals) {
    if (v.hasNode() && v.needsEval()) {
        std::string native_error;
        if (writeNativeMemberDefaultExpr(writer, v, parent_locals, &native_error)) {
            return true;
        }
        if (!aotValueTagPreservesMemberDefault(v)) {
            std::string diag = "AOT cannot serialize ";
            diag += owner_kind ? owner_kind : "callable";
            diag += " '";
            diag += owner_name ? owner_name : "<unknown>";
            diag += "' parameter '";
            diag += param_name ? param_name : "<unknown>";
            diag += "' default without fallback";
            if (!native_error.empty()) {
                diag += ": ";
                diag += native_error;
            }
            diag += "; default=";
            diag += qoreAOTDescribeExpr(v);
            diag += "; no fallback marker was emitted; add a native AOTExprKind serializer/reader or lower this "
                "default argument to native IR";

            if (error) {
                *error = diag;
            }
            qoreAOTSetExprSerializationError(std::move(diag));
            return false;
        }
    }

    if (aotValueTagPreservesMemberDefault(v)) {
        writer.writeValue(v);
        return true;
    }

    std::string native_error;
    if (writeNativeMemberDefaultExpr(writer, v, parent_locals, &native_error)) {
        return true;
    }

    std::string diag = "AOT cannot serialize ";
    diag += owner_kind ? owner_kind : "callable";
    diag += " '";
    diag += owner_name ? owner_name : "<unknown>";
    diag += "' parameter '";
    diag += param_name ? param_name : "<unknown>";
    diag += "' default without fallback";
    if (!native_error.empty()) {
        diag += ": ";
        diag += native_error;
    }
    diag += "; default=";
    diag += qoreAOTDescribeExpr(v);
    diag += "; no fallback marker was emitted; add a native AOTExprKind serializer/reader or lower this "
        "default argument to native IR";

    if (error) {
        *error = diag;
    }
    qoreAOTSetExprSerializationError(std::move(diag));
    return false;
}

//! Write CLASSES section
static bool writeClassesSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state,
        std::string& error) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::CLASSES);

    uint32_t count = static_cast<uint32_t>(state.classes.size());
    writer.writeU32(count);

    for (auto& ci : state.classes) {
        const qore_class_private* priv = ci.priv;

        // name and path
        writer.writeStringRef(priv->name.c_str());
        std::string class_path = priv->cls ? priv->cls->getNamespacePath(true) : std::string();
        writer.writeStringRef(class_path.empty() ? priv->path.c_str() : class_path.c_str());
        writer.writeU32(ci.ns_idx);

        // flags: bit 0 = pub, bit 1 = final, bit 2 = injected import,
        // bit 3 = reexported import, bit 4 = raw generic accepts,
        // bit 5 = raw generic construction
        uint16_t flags = 0;
        if (priv->pub) {
            flags |= 0x0001;
        }
        if (priv->final) {
            flags |= 0x0002;
        }
        if (priv->inject) {
            flags |= 0x0004;
        }
        if (priv->reexport) {
            flags |= 0x0008;
        }
        if ((writer.feature_flags & QORE_AOT_FEAT_CLASS_RAW_GENERIC) != 0) {
            if (priv->rawAcceptsParameterized()) {
                flags |= 0x0010;
            }
            if (priv->rawConstructionDefaultsToAuto()) {
                flags |= 0x0020;
            }
        }
        writer.writeU16(flags);

        // domain
        writer.writeI64(priv->domain);

        // Parser-produced class signature hash. Runtime class compatibility
        // uses this hash to compare same-named classes across Program objects;
        // preserve it exactly instead of recomputing from deserialized maps.
        writer.writeU8(priv->hash ? 1 : 0);
        if (priv->hash) {
            writer.writeBytes(priv->hash.getHash(), SH_SIZE);
        } else {
            static const uint8_t empty_hash[SH_SIZE] = {};
            writer.writeBytes(empty_hash, SH_SIZE);
        }
        if ((writer.feature_flags & QORE_AOT_FEAT_CLASS_INJECTION) != 0) {
            writer.writeStringRef(priv->injectedClass ? priv->injectedClass->path.c_str() : "");
        }
        if ((writer.feature_flags & QORE_AOT_FEAT_CLASS_TYPE_PARAMS) != 0) {
            uint32_t type_param_count = static_cast<uint32_t>(priv->getTypeParamCount());
            writer.writeU32(type_param_count);
            for (uint32_t i = 0; i < type_param_count; ++i) {
                if (i && !(i % 100)
                        && qore_check_cancel(nullptr, "AOT class type parameter serialization")) {
                    error = "operation cancelled during AOT class type parameter serialization";
                    return false;
                }
                writer.writeStringRef(priv->getTypeParamName(i));
                if ((writer.feature_flags & QORE_AOT_FEAT_TYPE_PARAM_DEFAULTS) != 0) {
                    const char* default_type = priv->getTypeParamDefaultType(i);
                    writer.writeU8(default_type ? 1 : 0);
                    if (default_type) {
                        writer.writeStringRef(default_type);
                    }
                }
                if ((writer.feature_flags & QORE_AOT_FEAT_TYPE_PARAM_BOUNDS) != 0) {
                    const char* bound_type = priv->getTypeParamBoundType(i);
                    writer.writeU8(bound_type ? 1 : 0);
                    if (bound_type) {
                        writer.writeStringRef(bound_type);
                    }
                }
            }
        }

        // base classes
        if (priv->scl) {
            uint32_t num_bases = static_cast<uint32_t>(priv->scl->size());
            writer.writeU32(num_bases);
            for (auto* bcn : *priv->scl) {
                // base class path
                if (bcn->sclass) {
                    const qore_class_private* bp = qore_class_private::get(*bcn->sclass);
                    writer.writeStringRef(bp->path.c_str());
                } else {
                    writer.writeStringRef("");
                }
                writer.writeU8(static_cast<uint8_t>(bcn->access));
                writer.writeU8(bcn->is_virtual ? 1 : 0);
                if ((writer.feature_flags & QORE_AOT_FEAT_CLASS_PARAM_BASES) != 0) {
                    writeTypePathRef(writer, bcn->type_info);
                }
            }
        } else {
            writer.writeU32(0);
        }

        // members - only serialize local (non-inherited) members
        uint32_t num_members = 0;
        for (auto& mi : priv->members.member_list) {
            if (mi.second->local()) {
                ++num_members;
            }
        }
        writer.writeU32(num_members);
        std::vector<AOTLocalSlotId> member_default_locals;
        member_default_locals.push_back(AOTLocalSlotId{});
        member_default_locals.back().name = "self";
        member_default_locals.back().type_path = getTypePath(priv->typeInfo);
        member_default_locals.back().flags = 0x04;
        member_default_locals.back().local_var_ptr = &priv->selfid;
        for (auto& mi : priv->members.member_list) {
            if (!mi.second->local()) {
                continue;
            }
            writer.writeStringRef(mi.first);
            writeTypePathRef(writer, mi.second->getTypeInfo());
            writer.writeU8(static_cast<uint8_t>(mi.second->access));
            // flags byte — bit 0 = transient
            uint8_t mflags = 0;
            if (mi.second->getTransient()) {
                mflags |= 0x01;
            }
            writer.writeU8(mflags);
            // default initialization value
            if (mi.second->exp) {
                writer.writeU8(1);
                if (!writeMemberDefaultValue(writer, mi.second->exp, "class",
                        priv->path.c_str(), mi.first, error, nullptr,
                        &member_default_locals)) {
                    return false;
                }
            } else {
                writer.writeU8(0);
            }
        }

        // static members
        uint32_t num_static = static_cast<uint32_t>(priv->vars.size());
        writer.writeU32(num_static);
        for (auto& vi : priv->vars.member_list) {
            writer.writeStringRef(vi.first);
            writeTypePathRef(writer, vi.second->getTypeInfo());
            writer.writeU8(static_cast<uint8_t>(vi.second->access));
            // Serialize the initial value. If the parser folded the init
            // expression to a concrete value (e.g. `static Type t = IntType`
            // where IntType is a reflection constant), we need to persist
            // that value so it survives AOT load. For unserializable values
            // (objects, closures), writeValue falls back to VT_CONST_REF via
            // the program reverse map when possible; otherwise NOTHING is
            // written and the static var will need an init function (which
            // is generated separately if the expression `needs_eval()`).
            if (vi.second->exp) {
                writer.writeU8(1);
                if (!writeMemberDefaultValue(writer, vi.second->exp, "class",
                        priv->path.c_str(), vi.first, error)) {
                    return false;
                }
            } else {
                writer.writeU8(0);
            }
        }

        // class constants
        uint32_t num_consts = 0;
        {
            // count user constants
            ConstConstantListIterator ccli(priv->constlist);
            while (ccli.next()) {
                const ConstantEntry* ce = ccli.getEntry();
                if (!ce->isSystem() && !ce->isExternalStub()) {
                    ++num_consts;
                }
            }
        }
        writer.writeU32(num_consts);
        {
            ConstConstantListIterator ccli(priv->constlist);
            while (ccli.next()) {
                const ConstantEntry* ce = ccli.getEntry();
                if (!ce->isSystem() && !ce->isExternalStub()) {
                    writer.writeStringRef(ce->getName());
                    writeTypePathRef(writer, ce->typeInfo);
                    writer.writeU8(static_cast<uint8_t>(ce->getAccess()));
                    // QORE_AOT_FEAT_CONST_PENDING: 1 if the constant had a
                    // non-literal init expression (value is not foldable
                    // until the __const_init::<path>::<name> init-func runs
                    // at register time).
                    writer.writeU8(ce->hasInitExpr() ? 1 : 0);
                    if (ce->hasInitExpr()) {
                        // Class constants with init expressions get NOTHING placeholder
                        writer.writeValue(QoreValue());
                    } else {
                        // Use getReferencedValue() for the actual evaluated value
                        QoreValue actual_val = ce->getReferencedValue();
                        std::string old_const_path = std::move(writer.current_const_path);
                        writer.current_const_path = getClassConstantPath(priv, ce->getName());
                        writer.writeValue(actual_val);
                        writer.current_const_path = std::move(old_const_path);
                        actual_val.discard(nullptr);
                    }
                }
            }
        }
    }

    writer.endSection(sec_idx);
    return true;
}

//! Write HASHDECLS section
static bool writeHashDeclsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state,
        std::string& error) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::HASHDECLS);

    uint32_t count = static_cast<uint32_t>(state.hashdecls.size());
    writer.writeU32(count);

    for (auto& hdi : state.hashdecls) {
        const TypedHashDecl* hd = hdi.hd;

        writer.writeStringRef(hd->getName());
        std::string nspath = hd->getNamespacePath();
        writer.writeStringRef(nspath.c_str());
        writer.writeU32(hdi.ns_idx);

        // flags: bit 0 = pub
        uint16_t flags = 0;
        if (hd->isPublic()) {
            flags |= 0x0001;
        }
        writer.writeU16(flags);

        // parent hashdecl path (empty if no parent)
        const TypedHashDecl* parent = hd->getParentHashDecl();
        if (parent) {
            std::string parent_path = parent->getNamespacePath();
            writer.writeStringRef(parent_path.c_str());
        } else {
            writer.writeStringRef("");
        }

        if ((writer.feature_flags & QORE_AOT_FEAT_HASHDECL_TYPE_PARAMS) != 0) {
            const typed_hash_decl_private* hdp = typed_hash_decl_private::get(*hd);
            size_t type_param_count = hdp->getTypeParamCount();
            if (type_param_count > UINT16_MAX) {
                error = "hashdecl '";
                error += hd->getNamespacePath();
                error += "' has too many type parameters for AOT serialization";
                return false;
            }
            writer.writeU16(static_cast<uint16_t>(type_param_count));
            for (size_t i = 0; i < type_param_count; ++i) {
                if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT hashdecl type parameter serialization")) {
                    error = "operation cancelled during AOT hashdecl type parameter serialization";
                    return false;
                }
                writer.writeStringRef(hdp->getTypeParamName(i));
                if ((writer.feature_flags & QORE_AOT_FEAT_TYPE_PARAM_DEFAULTS) != 0) {
                    const char* default_type = hdp->getTypeParamDefaultType(i);
                    writer.writeU8(default_type ? 1 : 0);
                    if (default_type) {
                        writer.writeStringRef(default_type);
                    }
                }
                if ((writer.feature_flags & QORE_AOT_FEAT_TYPE_PARAM_BOUNDS) != 0) {
                    const char* bound_type = hdp->getTypeParamBoundType(i);
                    writer.writeU8(bound_type ? 1 : 0);
                    if (bound_type) {
                        writer.writeStringRef(bound_type);
                    }
                }
            }
        }

        // members - count by iterating first
        uint32_t num_members = 0;
        {
            TypedHashDeclMemberIterator tmi(*hd);
            while (tmi.next()) {
                ++num_members;
            }
        }
        writer.writeU32(num_members);
        {
            // Access the hashdecl's private map directly so we can reach each
            // member's init expression (not just the public reflection API,
            // which only exposes the evaluated default value). Serializing the
            // expression lets writeValue encode e.g. `list<auto>()` via
            // VT_NEW_COMPLEX_DEFAULT and reconstruct the exact initializer on
            // load. Missing hashdecl member defaults caused hashdecl-typed
            // values (e.g. DataProviderPipelineFactory::PipelineQueueInfo::elems)
            // to deserialize as NOTHING, breaking downstream `.last()` and
            // similar container operations.
            const typed_hash_decl_private* hdp = typed_hash_decl_private::get(*hd);
            const HashDeclMemberMap& mm = hdp->getMembers();
            for (auto& mi : mm.member_list) {
                writer.writeStringRef(mi.first);
                writeTypePathRef(writer, mi.second->getTypeInfo());
                if (mi.second->exp) {
                    writer.writeU8(1);
                    if (!writeMemberDefaultValue(writer, mi.second->exp, "hashdecl",
                            nspath.c_str(), mi.first, error)) {
                        return false;
                    }
                } else {
                    writer.writeU8(0);
                }
            }
        }
    }

    writer.endSection(sec_idx);
    return true;
}

//! Write ENUMS section
static void writeEnumsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::ENUMS);

    uint32_t count = static_cast<uint32_t>(state.enums.size());
    writer.writeU32(count);

    for (auto& ei : state.enums) {
        const QoreEnumDecl* ed = ei.ed;

        writer.writeStringRef(ed->getName());
        std::string nspath = ed->getNamespacePath();
        writer.writeStringRef(nspath.c_str());
        writer.writeU32(ei.ns_idx);

        // flags: bit 0 = pub
        uint16_t flags = 0;
        if (ed->isPublic()) {
            flags |= 0x0001;
        }
        writer.writeU16(flags);

        // base type path
        writeTypePathRef(writer, ed->getBaseTypeInfo());

        // members
        uint32_t num_members = static_cast<uint32_t>(ed->getMemberCount());
        writer.writeU32(num_members);
        {
            QoreEnumMemberIterator emi(*ed);
            while (emi.next()) {
                writer.writeStringRef(emi.getName());
                writer.writeValue(emi.getValue());
            }
        }
    }

    writer.endSection(sec_idx);
}

//! Write TYPEDEFS section
static void writeTypedefsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::TYPEDEFS);

    uint32_t count = static_cast<uint32_t>(state.typedefs.size());
    writer.writeU32(count);

    for (auto& ti : state.typedefs) {
        writer.writeStringRef(ti.name.c_str());
        writeTypePathRef(writer, ti.typeInfo);
        writer.writeU32(ti.ns_idx);
        writer.writeU8(ti.pub ? 1 : 0);
    }

    writer.endSection(sec_idx);
}

//! Write CONSTANTS section (namespace-level constants only; class constants are in CLASSES)
static void writeConstantsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::CONSTANTS);

    std::unordered_set<std::string> current_blob_consts;
    std::unordered_set<std::string> available_consts;
    for (auto& ci : state.classes) {
        ConstConstantListIterator ccli(ci.priv->constlist);
        while (ccli.next()) {
            const ConstantEntry* ce = ccli.getEntry();
            if (!ce->isSystem() && !ce->isExternalStub()) {
                std::string fqn = getClassConstantPath(ci.priv, ce->getName());
                current_blob_consts.insert(fqn);
                // Class constants are resolved before namespace constants.
                available_consts.insert(std::move(fqn));
            }
        }
    }
    for (auto& ci : state.constants) {
        const ConstantEntry* ce = ci.entry;
        qore_ns_private* ns = ci.ns_idx < state.namespaces.size()
            ? state.namespaces[ci.ns_idx].ns : nullptr;
        current_blob_consts.insert(getNamespaceConstantPath(ns, ce->getName()));
    }
    const std::unordered_set<std::string>* old_blob_consts = writer.current_blob_const_fqns;
    const std::unordered_set<std::string>* old_available_consts = writer.available_const_ref_fqns;
    writer.current_blob_const_fqns = &current_blob_consts;
    writer.available_const_ref_fqns = &available_consts;

    uint32_t count = static_cast<uint32_t>(state.constants.size());
    writer.writeU32(count);

    for (auto& ci : state.constants) {
        const ConstantEntry* ce = ci.entry;
        qore_ns_private* ns = ci.ns_idx < state.namespaces.size()
            ? state.namespaces[ci.ns_idx].ns : nullptr;
        std::string const_path = getNamespaceConstantPath(ns, ce->getName());
        std::string old_const_path = std::move(writer.current_const_path);
        writer.current_const_path = const_path;

        writer.writeStringRef(ce->getName());
        writeTypePathRef(writer, ce->typeInfo);
        writer.writeU32(ci.ns_idx);
        writer.writeU8(static_cast<uint8_t>(ce->getAccess()));
        writer.writeU8(ce->isPublic() ? 1 : 0);
        // QORE_AOT_FEAT_CONST_PENDING: 1 if the constant had a
        // non-literal init expression (value is not foldable until the
        // __const_init::<ns>::<name> init-func runs at register time).
        // Readers wrap pending constants in a RuntimeConstantRefNode so
        // references from sibling `.qo`s defer evaluation.
        writer.writeU8(ce->hasInitExpr() ? 1 : 0);
        if (ce->hasInitExpr()) {
            // Constants with init expressions will be initialized at runtime
            // by their lowered init function — serialize NOTHING as placeholder
            writer.writeValue(QoreValue());
        } else {
            // Use getReferencedValue() to get the actual evaluated value.
            // ce->val may hold a RuntimeConstantRefNode (NT_RTCONSTREF) which is
            // just a reference to the constant's evaluated saved_val.
            QoreValue actual_val = ce->getReferencedValue();
            writer.writeValue(actual_val);
            actual_val.discard(nullptr);
        }
        available_consts.insert(std::move(const_path));
        writer.current_const_path = std::move(old_const_path);
    }

    writer.current_blob_const_fqns = old_blob_consts;
    writer.available_const_ref_fqns = old_available_consts;
    writer.endSection(sec_idx);
}

//! Write GLOBALS section
static void writeGlobalsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::GLOBALS);

    uint32_t count = static_cast<uint32_t>(state.globals.size());
    writer.writeU32(count);

    for (auto& gi : state.globals) {
        Var* var = gi.var;
        writer.writeStringRef(var->getName());
        writeTypePathRef(writer, var->getTypeInfo(), var->isNoNarrowing());
        writer.writeU32(gi.ns_idx);
        writer.writeU8(var->isThreadLocal() ? 1 : 0);
        writer.writeU8(var->isPublic() ? 1 : 0);
    }

    writer.endSection(sec_idx);
}

//! Write FUNCTIONS section
static void writeFunctionsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::FUNCTIONS);

    uint32_t count = static_cast<uint32_t>(state.functions.size());
    writer.writeU32(count);

    for (auto& fi : state.functions) {
        writer.writeStringRef(fi.entry->getName());
        writer.writeU32(fi.ns_idx);

        // flags: bit 0 = pub
        uint16_t flags = 0;
        if (fi.entry->isPublic()) {
            flags |= 0x0001;
        }
        writer.writeU16(flags);

        // count user variants
        uint32_t num_variants = 0;
        {
            QoreFunctionIterator qfi(*fi.func);
            while (qfi.next()) {
                if (qfi.getVariant()->isUser()) {
                    ++num_variants;
                }
            }
        }
        writer.writeU32(num_variants);

        // write user variant signatures
        {
            QoreFunctionIterator qfi(*fi.func);
            while (qfi.next()) {
                const AbstractQoreFunctionVariant* v = qfi.getVariant();
                if (v->isUser()) {
                    uint8_t vflags = 0;
                    const UserVariantBase* uvb = v->getUserVariantBase();
                    if (uvb && uvb->isSynchronized()) {
                        vflags |= 0x01;
                    }
                    writer.writeU8(vflags);
                    writeVariantSignature(writer, v);
                }
            }
        }
    }

    writer.endSection(sec_idx);
}

//! Recursively build a reverse map from constant value node pointers to FQNs for all namespaces
//! Used for BCA serialization to resolve constants from the entire program, not just ancestor namespaces
static void buildProgramConstantReverseMapImpl(qore_ns_private* ns,
        AOTConstantReverseMap& crm) {
    if (!ns) {
        return;
    }

    // Add namespace constants
    ConstConstantListIterator nsi(ns->constant);
    while (nsi.next()) {
        const ConstantEntry* ce = nsi.getEntry();
        std::string ns_path = ns->path;
        if (ns_path.size() >= 2) {
            ns_path = ns_path.substr(2);  // strip leading "::"
        }
        std::string fqn = ns_path.empty() ? nsi.getName() : ns_path + "::" + nsi.getName();
        if (ce->hasInitExpr()) {
            aot_add_constant_root_reverse_mapping(crm, ce->val, fqn);
            continue;
        }
        QoreValue v = ce->getReferencedValue();
        if (!v.hasNode()) {
            v.discard(nullptr);
            continue;
        }
        qore_aot_add_constant_value_reverse_mappings(crm, v, fqn);
        v.discard(nullptr);
    }

    // Add class constants within this namespace
    ClassListIterator cli(ns->classList);
    while (cli.next()) {
        QoreClass* qc = cli.get();
        if (!qc) {
            continue;
        }
        std::string class_prefix = std::string(qc->getPath() + 2) + "::";  // strip leading "::"
        ConstConstantListIterator cci(qore_class_private::get(*qc)->constlist);
        while (cci.next()) {
            const ConstantEntry* ce = cci.getEntry();
            std::string fqn = class_prefix + cci.getName();
            if (ce->hasInitExpr()) {
                aot_add_constant_root_reverse_mapping(crm, ce->val, fqn);
                continue;
            }
            QoreValue v = ce->getReferencedValue();
            if (!v.hasNode()) {
                v.discard(nullptr);
                continue;
            }
            qore_aot_add_constant_value_reverse_mappings(crm, v, fqn);
            v.discard(nullptr);
        }
    }

    // Recurse into child namespaces
    for (auto& ni : ns->nsl.nsmap) {
        if (ni.second) {
            buildProgramConstantReverseMapImpl(qore_ns_private::get(*ni.second), crm);
        }
    }
}

//! Build a reverse map from constant value node pointers to names for a specific class
//! Includes the class's own constants and parent namespace constants
static AOTConstantReverseMap buildClassConstantReverseMap(const QoreClass* qc) {
    AOTConstantReverseMap crm;
    if (!qc) {
        return crm;
    }

    const qore_class_private* cls_priv = qore_class_private::get(*qc);
    std::string class_prefix = std::string(qc->getPath() + 2) + "::";  // strip leading "::"

    // Add class constants
    ConstConstantListIterator cci(cls_priv->constlist);
    while (cci.next()) {
        const ConstantEntry* ce = cci.getEntry();
        std::string fqn = class_prefix + cci.getName();
        if (ce->hasInitExpr()) {
            aot_add_constant_root_reverse_mapping(crm, ce->val, fqn);
            continue;
        }
        QoreValue v = ce->getReferencedValue();
        if (!v.hasNode()) {
            v.discard(nullptr);
            continue;
        }
        qore_aot_add_constant_value_reverse_mappings(crm, v, fqn);
        v.discard(nullptr);
    }

    // Add namespace constants from the class's enclosing namespace hierarchy
    if (cls_priv->ns) {
        const qore_ns_private* ns_priv = cls_priv->ns;
        while (ns_priv) {
            // Iterate namespace constants using the ConstantList directly
            ConstConstantListIterator nsi(ns_priv->constant);
            while (nsi.next()) {
                const ConstantEntry* ce = nsi.getEntry();
                std::string ns_path = ns_priv->path;
                if (ns_path.size() >= 2) {
                    ns_path = ns_path.substr(2);  // strip leading "::"
                }
                std::string fqn = ns_path.empty() ? nsi.getName() : ns_path + "::" + nsi.getName();
                if (ce->hasInitExpr()) {
                    aot_add_constant_root_reverse_mapping(crm, ce->val, fqn);
                    continue;
                }
                QoreValue v = ce->getReferencedValue();
                if (!v.hasNode()) {
                    v.discard(nullptr);
                    continue;
                }
                qore_aot_add_constant_value_reverse_mappings(crm, v, fqn);
                v.discard(nullptr);
            }
            // Walk up to parent namespace
            ns_priv = ns_priv->parent;
        }
    }

    return crm;
}

static std::vector<AOTLocalSlotId> buildAOTLocalSlotsForUserSignature(const UserSignature* sig) {
    std::vector<AOTLocalSlotId> rv;
    if (!sig) {
        return rv;
    }

    rv.reserve(sig->numParams() + (sig->selfid ? 1 : 0) + (sig->argvid ? 1 : 0));
    for (unsigned i = 0; i < sig->numParams(); ++i) {
        AOTLocalSlotId slot;
        slot.local_var_ptr = reinterpret_cast<const void*>(sig->lv[i]);
        slot.flags = 0x01;
        if (sig->isParamReadOnly(i)) {
            slot.flags |= 0x10;
        }
        slot.param_index = static_cast<uint16_t>(i);
        if (sig->lv[i]) {
            slot.name = sig->lv[i]->getName();
        }
        rv.push_back(std::move(slot));
    }
    if (sig->selfid) {
        AOTLocalSlotId slot;
        slot.local_var_ptr = reinterpret_cast<const void*>(sig->selfid);
        slot.flags = 0x04;
        slot.name = "self";
        rv.push_back(std::move(slot));
    }
    if (sig->argvid) {
        AOTLocalSlotId slot;
        slot.local_var_ptr = reinterpret_cast<const void*>(sig->argvid);
        slot.flags = 0x08;
        slot.name = "$argv";
        rv.push_back(std::move(slot));
    }
    return rv;
}

static bool writeNativeBCAArgBlob(QoreAOTBinaryWriter& writer, const QoreValue& arg_val,
        const std::vector<AOTLocalSlotId>& bca_locals, const AOTConstantReverseMap* const_reverse_map,
        const std::string& class_path, const char* method_name, const std::string& base_path,
        uint16_t bca_index, uint16_t arg_index, const QoreProgramLocation* loc, std::string& error) {
    uint32_t size_pos = writer.position();
    writer.writeU32(0);
    uint32_t payload_pos = writer.position();
    static const std::vector<AOTGlobalSlotId> no_globals;

    qoreAOTClearExprSerializationError();
    bool ok = classifyAndWriteExpr(writer, arg_val, bca_locals, no_globals, const_reverse_map);
    std::string expr_error;
    bool expr_error_set = qoreAOTTakeExprSerializationError(expr_error);
    if (!ok || expr_error_set || writer.position() == payload_pos) {
        writer.truncate(size_pos);
        error = "AOT cannot serialize base-class constructor argument without fallback";
        error += "; class='";
        error += class_path.empty() ? "<unknown>" : class_path;
        error += "' method='";
        error += method_name ? method_name : "<unknown>";
        error += "' base='";
        error += base_path.empty() ? "<unknown>" : base_path;
        error += "' bca-index=";
        error += std::to_string(bca_index);
        error += " arg-index=";
        error += std::to_string(arg_index);
        error += " location=";
        error += qoreAOTDescribeLocation(loc);
        if (expr_error_set) {
            error += ": ";
            error += expr_error;
        } else if (!ok) {
            error += ": classifyAndWriteExpr returned false without a nested expression diagnostic";
        } else {
            error += ": native expression serializer produced an empty payload";
        }
        error += "; arg=";
        error += qoreAOTDescribeExpr(arg_val);
        error += "; no fallback marker was emitted; add a native AOTExprKind serializer/reader or lower this "
            "base-constructor argument to native IR";
        return false;
    }

    writer.patchU32(size_pos, writer.position() - payload_pos);
    return true;
}

//! Write METHODS section
static bool writeMethodsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state,
        std::string& error) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::METHODS);

    // Build program-wide constant reverse map for BCA arg serialization
    // This includes constants from ALL namespaces, not just the class's ancestors
    AOTConstantReverseMap program_crm;
    if (state.root_ns) {
        buildProgramConstantReverseMapImpl(state.root_ns, program_crm);
    }

    uint32_t count = static_cast<uint32_t>(state.methods.size());
    writer.writeU32(count);

    for (auto& mi : state.methods) {
        const QoreMethod* method = mi.method;
        writer.writeU32(mi.class_idx);
        writer.writeStringRef(method->getName());
        writer.writeU8(mi.is_static ? 1 : 0);

        // get the method's underlying function
        const qore_method_private* mp = qore_method_private::get(*method);
        const MethodFunctionBase* mfb = mp->func;

        // count user variants
        uint32_t num_variants = 0;
        {
            QoreFunctionIterator qfi(*static_cast<const QoreFunction*>(mfb));
            while (qfi.next()) {
                const AbstractQoreFunctionVariant* v = qfi.getVariant();
                if (isAOTSerializableMethodVariant(method, v)) {
                    ++num_variants;
                }
            }
        }
        writer.writeU32(num_variants);

        // write user variant signatures
        {
            bool is_constructor = strcmp(method->getName(), "constructor") == 0;
            bool is_destructor = strcmp(method->getName(), "destructor") == 0;
            bool is_copy = strcmp(method->getName(), "copy") == 0;
            QoreFunctionIterator qfi(*static_cast<const QoreFunction*>(mfb));
            while (qfi.next()) {
                const AbstractQoreFunctionVariant* v = qfi.getVariant();
                if (isAOTSerializableMethodVariant(method, v)) {
                    const MethodVariantBase* mvb = reinterpret_cast<const MethodVariantBase*>(v);
                    // write access + flags before the signature
                    writer.writeU8(static_cast<uint8_t>(mvb->getAccess()));
                    uint8_t mflags = 0;
                    if (mvb->isFinal()) {
                        mflags |= 0x01;
                    }
                    if (mvb->isAbstract()) {
                        mflags |= 0x02;
                    }
                    const UserVariantBase* uvb = v->getUserVariantBase();
                    if (mvb->isMethodSynchronized()
                            || (uvb && uvb->isSynchronized())) {
                        mflags |= 0x04;
                    }
                    if (mvb->isConstMethod()) {
                        if ((writer.feature_flags & QORE_AOT_FEAT_CONST_METHODS) == 0) {
                            error = "const method '";
                            error += method->getName() ? method->getName() : "<unknown>";
                            error += "' cannot be serialized without QORE_AOT_FEAT_CONST_METHODS";
                            return false;
                        }
                        if (mi.is_static || is_constructor || is_destructor || is_copy) {
                            error = "invalid const metadata on ";
                            error += mi.is_static ? "static" : "special";
                            error += " method '";
                            error += method->getName() ? method->getName() : "<unknown>";
                            error += "'";
                            return false;
                        }
                        mflags |= 0x08;
                    }
                    writer.writeU8(mflags);
                    writeVariantSignature(writer, v);

                    // Serialize BCA (Base Class Constructor Arguments) for constructors
                    if (is_constructor) {
                        const ConstructorMethodVariant* cmv = CONMV_const(mvb);
                        const BCAList* bcal = cmv->getBaseClassArgumentList();
                        if (bcal && !bcal->empty()) {
                            // Must use dynamic_cast due to multiple inheritance:
                            // UserConstructorVariant inherits both ConstructorMethodVariant
                            // (via MethodVariantBase -> AbstractQoreFunctionVariant) and
                            // UserVariantBase. reinterpret_cast gives wrong pointer offset.
                            const UserConstructorVariant* ucv =
                                dynamic_cast<const UserConstructorVariant*>(cmv);
                            if (ucv && !aotResolveBCAListForSerialization(
                                    method->getClass(), ucv, bcal, &error)) {
                                return false;
                            }

                            writer.writeU8(1);  // has_bca = true
                            writer.writeU16(static_cast<uint16_t>(bcal->size()));

                            // Build slot map from constructor's signature params
                            const UserSignature* sig = ucv
                                ? const_cast<UserConstructorVariant*>(ucv)->getUserSignature()
                                : nullptr;
                            std::vector<AOTLocalSlotId> bca_locals = buildAOTLocalSlotsForUserSignature(sig);

                            uint16_t bca_index = 0;
                            for (const BCANode* bca : *bcal) {
                                // Write base class path for runtime resolution
                                std::string base_path = aotBCAClassRef(mi.method, bca);
                                writer.writeStringRef(base_path.c_str());
                                writer.writeU16(static_cast<uint16_t>(
                                    bca->loc ? bca->loc->start_line : 0));
                                writer.writeU16(static_cast<uint16_t>(
                                    bca->loc ? bca->loc->end_line : 0));

                                // Serialize args as individual native expression blobs.
                                // Legacy readers still understand EXPR_TREE blobs when the
                                // QORE_AOT_FEAT_BCA_NATIVE_ARGS bit is absent; new writers do
                                // not emit EXPR_TREE fallback.
                                const QoreListNode* args = bca->getArgs();
                                uint16_t num_args = aotBCAArgCount(bca);
                                const qore_list_private* args_priv = qore_list_private::get(args);
                                const std::vector<size_t>* eval_map = args_priv
                                    ? args_priv->getCallArgEvalMap()
                                    : nullptr;
                                uint16_t eval_result_size = 0;
                                uint16_t eval_map_size = 0;
                                if (eval_map) {
                                    if (args_priv->getCallArgEvalResultSize() > UINT16_MAX
                                            || eval_map->size() > UINT16_MAX) {
                                        error = "BCA named-argument map is too large to serialize";
                                        return false;
                                    }
                                    eval_result_size = static_cast<uint16_t>(
                                        args_priv->getCallArgEvalResultSize());
                                    eval_map_size = static_cast<uint16_t>(eval_map->size());
                                }
                                writer.writeU16(eval_result_size);
                                writer.writeU16(eval_map_size);
                                for (uint16_t mi = 0; mi < eval_map_size; ++mi) {
                                    if (mi && !(mi % 100)
                                            && qore_check_cancel(nullptr, "AOT BCA named argument map serialization")) {
                                        error = "operation cancelled during AOT BCA named argument map serialization";
                                        return false;
                                    }
                                    size_t target = (*eval_map)[mi];
                                    if (target > UINT16_MAX) {
                                        error = "BCA named-argument map target is too large to serialize";
                                        return false;
                                    }
                                    writer.writeU16(static_cast<uint16_t>(target));
                                }
                                writer.writeU16(num_args);

                                const QoreClass* method_class = mi.method->getClass();
                                std::string method_class_path = qore_aot_encode_class_ref(method_class);
                                for (uint16_t ai = 0; ai < num_args; ++ai) {
                                    QoreValue arg_val = aotBCAArgValue(bca, ai);
                                    if (!writeNativeBCAArgBlob(writer, arg_val, bca_locals, &program_crm,
                                            method_class_path, method->getName(), base_path, bca_index, ai,
                                            bca->loc, error)) {
                                        return false;
                                    }
                                }
                                ++bca_index;
                            }
                        } else {
                            writer.writeU8(0);  // has_bca = false
                        }
                    }
                }
            }
        }
    }

    writer.endSection(sec_idx);
    return true;
}

} // anonymous namespace

//! Phase 4 slice 10c (single-file `-L` preload): collect the set of source
//! files that contributed user declarations to the program rooted at @p ns.
/** The single-file incremental compiler (`qcc -c -L <dir>`) parses the target
    source — which textually pulls in its `%include`d files — BEFORE preloading
    sibling `.qo`s.  Any sibling whose source file is in this set has already
    been declared by that parse, so preloading its `.qo` shells would
    re-register the same declarations and raise a duplicate-definition parse
    error (e.g. "hashdecl 'AppenderParams' is already defined").  The caller
    skips those siblings.

    The per-item location accessors mirror collectItems() so the file
    attribution matches exactly what each per-file `.qo` was emitted for (each
    batch `.qo` carries only its own source file's declarations — see the
    `compile_file` filter in collectItems). */
void collectDeclaredSourceFiles(qore_ns_private* ns, std::unordered_set<std::string>& files) {
    auto add_file = [&files](const QoreProgramLocation* loc) {
        if (loc) {
            const char* f = loc->getFile();
            if (f && *f) {
                files.insert(f);
            }
        }
    };

    // classes
    {
        ClassListIterator cli(ns->classList);
        while (cli.next()) {
            qore_class_private* priv = qore_class_private::get(*cli.get());
            if (!priv->sys) {
                add_file(priv->loc);
            }
        }
    }
    // hashdecls
    {
        HashDeclListIterator hdi(ns->hashDeclList);
        while (hdi.next()) {
            TypedHashDecl* hd = hdi.get();
            if (!hd->isSystem()) {
                add_file(typed_hash_decl_private::get(*hd)->getParseLocation());
            }
        }
    }
    // enums
    {
        EnumListIterator eli(ns->enumList);
        while (eli.next()) {
            QoreEnumDecl* ed = eli.get();
            if (!ed->isSystem()) {
                add_file(qore_enum_decl_private::get(*ed)->getParseLocation());
            }
        }
    }
    // typedefs
    for (auto& ti : ns->typedefMap) {
        if (ti.second->typeInfo) {
            add_file(ti.second->loc);
        }
    }
    // constants
    {
        ConstantListIterator cli(ns->constant);
        while (cli.next()) {
            ConstantEntry* ce = cli.getEntry();
            if (!ce->isSystem() && !ce->isExternalStub()) {
                add_file(ce->loc);
            }
        }
    }
    // global variables
    for (auto& vi : ns->var_list.vmap) {
        Var* var = vi.second;
        if (!var->isImported() && !var->isAOTImport() && !var->isBuiltin()) {
            add_file(var->getParseLocation());
        }
    }
    // functions (per user variant — overloads can legally live in different files)
    for (auto fi = ns->func_list.begin(), fe = ns->func_list.end(); fi != fe; ++fi) {
        FunctionEntry* entry = fi->second;
        QoreFunction* func = entry->getFunction();
        if (func && !entry->hasBuiltin()) {
            QoreFunctionIterator vit(*func);
            while (vit.next()) {
                const AbstractQoreFunctionVariant* v = vit.getVariant();
                UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(v)->getUserVariantBase();
                if (!uvb) {
                    continue;
                }
                const UserSignature* sig = uvb->getUserSignature();
                if (sig) {
                    add_file(sig->getParseLocation());
                }
            }
        }
    }
    // recurse into child namespaces
    for (auto ni = ns->nsl.nsmap.begin(), ne = ns->nsl.nsmap.end(); ni != ne; ++ni) {
        QoreNamespace* child_ns = ni->second;
        if (child_ns) {
            collectDeclaredSourceFiles(qore_ns_private::get(*child_ns), files);
        }
    }
}

bool qoreAOTWriteDefaultArgValuePayload(QoreAOTBinaryWriter& writer, const QoreValue& v,
        const char* owner_kind, const char* owner_name, const char* param_name,
        std::string* error, const std::vector<AOTLocalSlotId>* parent_locals) {
    return qoreAOTWriteDefaultArgValuePayloadImpl(writer, v, owner_kind, owner_name,
        param_name, error, parent_locals);
}

//! Lower a closure variant to IR for serialization
/** Follows the same pattern as buildContextForVariant() in QoreAOTRuntime.cpp.
    @param variant the closure variant to lower
    @param error_out optional detailed error destination
    @return heap-allocated IR function, or nullptr on failure (caller owns)
*/
QoreIRFunction* lowerClosureForSerialization(const UserClosureVariant* variant, std::string* error_out) {
    StatementBlock* sb = const_cast<UserClosureVariant*>(variant)->getStatementBlock();
    if (!sb) {
        if (error_out) {
            *error_out = "closure has no statement block";
        }
        return nullptr;
    }

    auto* ir = new QoreIRFunction("<closure>");

    // Record pre-instantiated locals from signature
    const UserSignature* sig = const_cast<UserClosureVariant*>(variant)->getUserSignature();
    if (sig) {
        for (unsigned i = 0; i < sig->numParams(); ++i) {
            ir->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->lv[i]));
        }
        if (sig->argvid) {
            ir->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->argvid));
        }
        if (sig->selfid) {
            ir->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->selfid));
        }
    }

    QoreIRBuilder builder(ir);
    auto* entry = ir->createBlock("entry");
    builder.setBlock(entry);

    QoreProgram* pgm = const_cast<UserClosureVariant*>(variant)->pgm;
    QoreParseContext parse_context(pgm);
    QoreIRLowering lowering(builder, &parse_context);
    std::string error;
    if (!lowering.lowerStatementBlock(sb, error)) {
        printd(2, "AOT: closure IR lowering failed: %s\n", error.c_str());
        if (error_out) {
            *error_out = "closure IR lowering failed: " + error;
        }
        delete ir;
        return nullptr;
    }

    // Ensure all blocks have terminators. Complex closures (switch statements
    // with break/fall-through) can leave merge blocks empty or unterminated.
    // Add ReturnNothing to any block that needs it.
    for (auto& block : ir->blocks) {
        if (block->instructions.empty()) {
            builder.setBlock(block.get());
            builder.createReturnNothing();
        } else if (!isTerminator(block->instructions.back()->opcode)) {
            builder.setBlock(block.get());
            builder.createReturnNothing();
        }
    }

    std::string verify_error;
    if (!QoreIRVerifier::verify(*ir, verify_error)) {
        printd(2, "AOT: closure IR verification failed: %s\n", verify_error.c_str());
        if (error_out) {
            *error_out = "closure IR verification failed: " + verify_error;
        }
        delete ir;
        return nullptr;
    }

    // Compile handler IRs for try/catch blocks inside the closure.
    // Failure aborts closure IR lowering (the runtime asserts handler_ir
    // is populated — see executeHandlerBody).
    std::string handler_error;
    if (lowering.compileAllHandlerIRs(handler_error) < 0) {
        printd(2, "AOT: closure handler IR compilation failed: %s\n", handler_error.c_str());
        if (error_out) {
            *error_out = "closure handler IR compilation failed: " + handler_error;
        }
        delete ir;
        return nullptr;
    }

    // Collect body locals
    collectAllStatementLocals(sb, ir->all_body_locals);

    ir->computeSlotIdsAndEmbed();
    return ir;
}

namespace {

struct QoreAOTClosureCaptureInfo {
    LocalVar* lv;
    int32_t parent_slot;
};

int32_t qoreAOTFindParentLocalSlot(const std::vector<AOTLocalSlotId>& parent_locals,
        const LocalVar* lv) {
    const void* var_ptr = reinterpret_cast<const void*>(lv);
    for (size_t i = 0; i < parent_locals.size(); ++i) {
        if (parent_locals[i].local_var_ptr == var_ptr) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

void qoreAOTAppendClosureCapture(std::vector<QoreAOTClosureCaptureInfo>& captures,
        LocalVar* lv, int32_t parent_slot) {
    if (!lv) {
        return;
    }
    for (const QoreAOTClosureCaptureInfo& capture : captures) {
        if (capture.lv == lv) {
            return;
        }
    }
    captures.push_back({lv, parent_slot});
}

bool qoreAOTClosureOwnsLocal(const QoreIRFunction* closure_ir, const LocalVar* lv) {
    const void* var_ptr = reinterpret_cast<const void*>(lv);
    if (closure_ir->pre_instantiated_locals.count(var_ptr)) {
        return true;
    }
    for (const LocalVar* body_lv : closure_ir->all_body_locals) {
        if (body_lv == lv) {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

bool qoreAOTWriteClosureCaptures(QoreAOTBinaryWriter& writer, const LVarSet* vlist,
        const QoreIRFunction* closure_ir, const std::vector<AOTLocalSlotId>& parent_locals) {
    std::vector<QoreAOTClosureCaptureInfo> captures;
    if (vlist) {
        for (LocalVar* lv : *vlist) {
            qoreAOTAppendClosureCapture(captures, lv,
                qoreAOTFindParentLocalSlot(parent_locals, lv));
        }
    }

    if (closure_ir) {
        std::vector<std::pair<const LocalVar*, uint32_t>> sorted_slots(
            closure_ir->local_var_slots.begin(), closure_ir->local_var_slots.end());
        std::sort(sorted_slots.begin(), sorted_slots.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        for (auto& [lv, slot_id] : sorted_slots) {
            (void)slot_id;
            int32_t parent_slot = qoreAOTFindParentLocalSlot(parent_locals, lv);
            bool owned = qoreAOTClosureOwnsLocal(closure_ir, lv);
            // A closure created in a method has lexical `self` semantics.  Do
            // not let the closure signature's synthetic selfid make the parent
            // object look like an owned local; it must be captured so delayed
            // invocation, for example Program::setThreadInit(), still sees it.
            bool lexical_self = lv->isSelf() || (lv->getName() && strcmp(lv->getName(), "self") == 0);
            if (parent_slot >= 0 || !owned || lexical_self) {
                qoreAOTAppendClosureCapture(captures, const_cast<LocalVar*>(lv), parent_slot);
            }
        }
    }

    if (captures.size() > UINT16_MAX) {
        return false;
    }
    writer.writeU16(static_cast<uint16_t>(captures.size()));
    for (const QoreAOTClosureCaptureInfo& capture : captures) {
        writer.writeStringRef(capture.lv->getName() ? capture.lv->getName() : "");
        writer.writeU32(static_cast<uint32_t>(capture.parent_slot));
    }
    return true;
}

void qoreAOTPruneClosureIRBodyLocals(QoreIRFunction* closure_ir, const UserSignature* sig,
        const LVarSet* vlist) {
    if (!closure_ir) {
        return;
    }
    removeSignatureLocalsFromBodyLocals(closure_ir->all_body_locals, sig);
    if (!vlist || vlist->empty() || closure_ir->all_body_locals.empty()) {
        return;
    }
    closure_ir->all_body_locals.erase(
        std::remove_if(closure_ir->all_body_locals.begin(), closure_ir->all_body_locals.end(),
            [vlist](LocalVar* lv) {
                return vlist->find(lv) != vlist->end();
            }),
        closure_ir->all_body_locals.end());
}


//! Classify and write a QoreValue expression in AOTExprKind format
/** Used by handler IR serialization to classify expression nodes inline.
    Handles function calls, method calls, variable refs, constants, and enums.
    Returns true on success, false if the expression cannot be classified.
*/
bool classifyAndWriteExpr(QoreAOTBinaryWriter& writer, const QoreValue& expr,
        const std::vector<AOTLocalSlotId>& parent_locals,
        const std::vector<AOTGlobalSlotId>& parent_globals,
        const AOTConstantReverseMap* const_reverse_map) {
    auto trace_unsupported_expr = [&expr](const char* reason, const AbstractQoreNode* node) {
        if (!getenv("QORE_AOT_TRACE_UNSUPPORTED_EXPR") && !getenv("QORE_AOT_TRACE_GENERIC_EVAL")) {
            return;
        }
        const char* object_class = "";
        if (auto* obj = dynamic_cast<const QoreObject*>(node)) {
            object_class = obj->getClassName();
        }
        fprintf(stderr, "[aot-unsupported-expr] %s qtype=%d node=%p node_type=%s needs_eval=%d\n",
            reason, expr.getType(), static_cast<const void*>(node), node ? node->getTypeName() : "<none>",
            node ? (node->needs_eval() ? 1 : 0) : (expr.needsEval() ? 1 : 0));
        if (*object_class) {
            fprintf(stderr, "[aot-unsupported-expr] object_class=%s\n", object_class);
        }
    };
    if (!expr.hasNode()) {
        if (expr.isEnum()) {
            const QoreEnumMember* member = expr.getEnumMember();
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_ENUM));
            std::string ns_path = member->getEnumDecl()->getNamespacePath();
            writer.writeStringRef(ns_path.c_str());
            writer.writeStringRef(member->getName());
            return true;
        }
        if (expr.isShortString()) {
            char buf[8];
            expr.getShortString(buf);
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_STRING));
            writer.writeStringRef(buf, expr.shortStringLen());
            return true;
        }
        // Handle inline primitive values
        switch (expr.getType()) {
            case NT_INT: {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_INT));
                writer.writeI64(expr.getAsBigInt());
                return true;
            }
            case NT_CHAR: {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_VALUE));
                return writer.writeValue(expr);
            }
            case NT_FLOAT: {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_FLOAT));
                writer.writeF64(expr.getAsFloat());
                return true;
            }
            case NT_BOOLEAN: {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_BOOL));
                writer.writeU8(expr.getAsBool() ? 1 : 0);
                return true;
            }
            case NT_NOTHING:
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
                return true;
            case NT_NULL:
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NULL));
                return true;
            default:
                break;
        }
        trace_unsupported_expr("unsupported inline non-node expression", nullptr);
        qoreAOTSetExprSerializationError("unsupported inline native AOT expression for "
            + qoreAOTDescribeExpr(expr)
            + "; no fallback marker was emitted; add a native AOTExprKind serializer/reader or lower this "
                "operation to native IR");
        return false;
    }

    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        trace_unsupported_expr("missing inline expression node", nullptr);
        qoreAOTSetExprSerializationError("unsupported inline native AOT expression for "
            + qoreAOTDescribeExpr(expr)
            + "; no fallback marker was emitted; add a native AOTExprKind serializer/reader or lower this "
                "operation to native IR");
        return false;
    }

    if (node->getType() == NT_DATE) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_VALUE));
        return writer.writeValue(expr);
    }

    auto write_inline_expr = [&](const QoreValue& v) -> bool {
        return classifyAndWriteExpr(writer, v, parent_locals, parent_globals,
            const_reverse_map);
    };
    auto write_cast_inner = [&](const QoreValue& v) -> bool {
        writer.writeU8(1);
        return write_inline_expr(v);
    };
    auto write_qore_arg_list = [&](const QoreListNode* args) -> bool {
        size_t nargs = args ? args->size() : 0;
        if (nargs > 255) {
            qoreAOTSetExprSerializationError("inline AOT expression payload has more than 255 arguments in "
                + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
            return false;
        }
        writer.writeU8(static_cast<uint8_t>(nargs));
        for (size_t j = 0; j < nargs; ++j) {
            if (!write_inline_expr(args->retrieveEntry(j))) {
                return false;
            }
        }
        return true;
    };
    auto write_parse_arg_list = [&](const QoreParseListNode* args) -> bool {
        size_t nargs = args ? args->size() : 0;
        if (nargs > 255) {
            qoreAOTSetExprSerializationError("inline AOT expression payload has more than 255 arguments in "
                + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
            return false;
        }
        writer.writeU8(static_cast<uint8_t>(nargs));
        for (size_t j = 0; j < nargs; ++j) {
            if (!write_inline_expr(args->get(j))) {
                return false;
            }
        }
        return true;
    };
    auto write_args_prefer_qore = [&](const QoreListNode* args,
            const QoreParseListNode* parse_args) -> bool {
        if (args && args->size() > 0) {
            return write_qore_arg_list(args);
        }
        return write_parse_arg_list(parse_args);
    };

    // Preserve named constant identity when the expression node pointer is
    // already registered in the constant reverse map.
    if (const_reverse_map) {
        qore_type_t qt = node->getType();
        if (qt == NT_HASH || qt == NT_LIST || qt == NT_OBJECT) {
            if (const std::string* path = aotFindConstantReverseMapPath(const_reverse_map, node)) {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
                writer.writeStringRef(path->c_str());
                return true;
            }
        }
    }

    // FunctionCallNode: regular function call
    if (auto* call = dynamic_cast<const FunctionCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::FUNC_CALL));
        // Emit namespace-qualified name so runtime lookup lands on
        // the exact function the parser resolved, not a same-named
        // wrapper in the caller's scope (see write_expr_func_call in
        // QoreAOTExprHandlers.cpp for the full rationale).
        const char* deferred_source_function = call->getAOTDeferredSourceFunction();
        const FunctionEntry* fe = deferred_source_function ? nullptr : call->getFunctionEntry();
        if (deferred_source_function) {
            writer.writeStringRef(deferred_source_function);
        } else if (fe && fe->getNamespace()) {
            std::string qualified;
            fe->getNamespace()->getPath(qualified);
            if (!qualified.empty()) {
                qualified += "::";
            }
            qualified += fe->getName();
            writer.writeStringRef(qualified.c_str());
        } else {
            writer.writeStringRef(call->getName());
        }
        if (!deferred_source_function) {
            if (const AbstractQoreFunctionVariant* v = call->getVariant()) {
                if (AbstractFunctionSignature* sig = const_cast<AbstractQoreFunctionVariant*>(v)->getSignature()) {
                    std::string sig_ref = "sig:";
                    sig_ref += sig->getSignatureText();
                    writer.writeStringRef(sig_ref.c_str());
                } else {
                    writer.writeStringRef("");
                }
            } else {
                writer.writeStringRef("");
            }
        } else {
            writer.writeStringRef("");
        }
        const QoreListNode* args = call->getArgs();
        const QoreParseListNode* pargs = call->getParseArgs();
        size_t arg_start = deferred_source_function ? 1 : 0;
        size_t total_args = args ? args->size() : (pargs ? pargs->size() : 0);
        size_t nargs = total_args > arg_start ? total_args - arg_start : 0;
        if (nargs > 255) {
            qoreAOTSetExprSerializationError("FUNC_CALL inline payload has more than 255 arguments in "
                + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
            return false;
        }
        writer.writeU8(static_cast<uint8_t>(nargs));
        for (size_t j = 0; j < nargs; ++j) {
            const size_t arg_index = arg_start + j;
            const QoreValue arg = args ? args->retrieveEntry(arg_index) : pargs->get(arg_index);
            if (!classifyAndWriteExpr(writer, arg, parent_locals, parent_globals, const_reverse_map)) {
                return false;
            }
        }
        return true;
    }

    // SelfFunctionCallNode: method call on self
    if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_METHOD_CALL));
        const QoreMethod* method = call->getMethod();
        const QoreClass* qc = call->getClass() ? call->getClass() : (method ? method->getClass() : nullptr);
        std::string class_ref = qore_aot_encode_class_ref(qc);
        writer.writeStringRef(class_ref.c_str());
        const AbstractQoreFunctionVariant* variant = call->getVariant();
        std::string method_ref = qore_aot_encode_static_method_ref(call->getName(), variant,
            variant ? nullptr : &call->getParsedArgTypeInfo());
        writer.writeStringRef(method_ref.c_str());
        const QoreListNode* args = call->getArgs();
        const QoreParseListNode* pargs = call->getParseArgs();
        size_t nargs = args ? args->size() : (pargs ? pargs->size() : 0);
        if (nargs > 255) {
            qoreAOTSetExprSerializationError("SELF_METHOD_CALL inline payload has more than 255 arguments in "
                + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
            return false;
        }
        writer.writeU8(static_cast<uint8_t>(nargs));
        for (size_t j = 0; j < nargs; ++j) {
            const QoreValue arg = args ? args->retrieveEntry(j) : pargs->get(j);
            if (!classifyAndWriteExpr(writer, arg, parent_locals, parent_globals, const_reverse_map)) {
                return false;
            }
        }
        return true;
    }

    // StaticMethodCallNode: static method call
    if (auto* call = dynamic_cast<const StaticMethodCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_METHOD_CALL));
        const QoreMethod* method = call->getMethod();
        if (method) {
            const QoreClass* qc = method->getClass();
            std::string class_ref = qore_aot_encode_class_ref(qc);
            writer.writeStringRef(class_ref.c_str());
        } else {
            std::string class_ref = call->getClassPath();
            writer.writeStringRef(class_ref.c_str());
        }
        const AbstractQoreFunctionVariant* variant = call->getVariant();
        std::string method_ref = qore_aot_encode_static_method_ref(call->getName(), variant,
            variant ? nullptr : &call->getParsedArgTypeInfo());
        writer.writeStringRef(method_ref.c_str());
        if ((writer.feature_flags & QORE_AOT_FEAT_STATIC_CALL_RECEIVER_TYPE) != 0) {
            writer.writeStringRef(qore_get_aot_serializable_type_path(call->getReceiverTypeInfo()).c_str());
        }
        // Serialize method args (must match read_expr_static_method_call format)
        const QoreListNode* args = call->getArgs();
        const QoreParseListNode* pargs = call->getParseArgs();
        if (!write_args_prefer_qore(args, pargs)) {
            return false;
        }
        return true;
    }

    // VarRefNewObjectNode: variable declaration with object constructor call
    // (e.g., "Foo f("arg1", var)") — MUST check before VarRefNode since it
    // inherits from VarRefNode and would be matched by the VarRefNode handler
    if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
        if (qc) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::NEW_OBJECT));
            std::string class_ref = qore_aot_encode_class_ref(qc);
            writer.writeStringRef(class_ref.c_str());
            if ((writer.feature_flags & QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO) != 0) {
                writeTypePathRef(writer, vrn->getTypeInfo());
            }
            return write_args_prefer_qore(vrn->getArgs(), vrn->getParseArgs());
        }

        const QoreTypeInfo* vti = vrn->getTypeInfo();
        if (vrn->isHashDeclConstruct()) {
            const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(vti);
            if (!hd) {
                qoreAOTSetExprSerializationError("HASHDECL VarRefNewObjectNode has no hashdecl type in "
                    + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
                return false;
            }
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASHDECL_NEW));
            writer.writeStringRef(hd->getNamespacePath().c_str());
            return write_parse_arg_list(vrn->getParseArgs());
        }
        if (vrn->isComplexHashConstruct()) {
            if (!vti) {
                qoreAOTSetExprSerializationError("complex-hash VarRefNewObjectNode has no type info in "
                    + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
                return false;
            }
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_HASH_NEW));
            writeTypePathRef(writer, vti);
            return write_parse_arg_list(vrn->getParseArgs());
        }
        if (vrn->isComplexListConstruct()) {
            if (!vti) {
                qoreAOTSetExprSerializationError("complex-list VarRefNewObjectNode has no type info in "
                    + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
                return false;
            }
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_LIST_NEW));
            writeTypePathRef(writer, vti);
            const QoreValue& new_args = vrn->getNewArgs();
            if (new_args.hasNode()) {
                writer.writeU8(1);
                if (!classifyAndWriteExpr(writer, new_args, parent_locals, parent_globals, const_reverse_map)) {
                    return false;
                }
            } else {
                writer.writeU8(0);
            }
            return true;
        }
        if (QoreTypeInfo::getComplexBufferType(vti)) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_BUFFER_NEW));
            writeTypePathRef(writer, vti);
            writer.writeU8(static_cast<uint8_t>(QoreComplexBufferInitKind::Constructor));
            const QoreValue& new_args = vrn->getNewArgs();
            if (new_args.hasNode()) {
                writer.writeU8(1);
                return classifyAndWriteExpr(writer, new_args, parent_locals, parent_globals, const_reverse_map);
            }
            writer.writeU8(0);
            return true;
        }
        if (vrn->isDynamicObjectConstruct()) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::NEW_OBJECT));
            writer.writeStringRef(vrn->getDynamicClassName().c_str());
            if ((writer.feature_flags & QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO) != 0) {
                writeTypePathRef(writer, vrn->getTypeInfo());
            }
            return write_args_prefer_qore(vrn->getArgs(), vrn->getParseArgs());
        }
        if (vrn->isDynamicHashDeclConstruct()) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASHDECL_NEW));
            writer.writeStringRef(vrn->getDynamicHashDeclName().c_str());
            return write_parse_arg_list(vrn->getParseArgs());
        }
        qoreAOTSetExprSerializationError("unsupported VarRefNewObjectNode constructor in "
            + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
        return false;
    }

    // VarRefNode: local and global variable references
    // Note: VarRefNewObjectNode inherits from VarRefNode, so must come AFTER above check
    if (auto* varref = dynamic_cast<const VarRefNode*>(node)) {
        if (varref->getType() == VT_LOCAL || varref->getType() == VT_LOCAL_TS ||
                varref->getType() == VT_CLOSURE) {
            // Look up the local slot index using pointer identity first (handles
            // same-named variables in different scopes), then fall back to name
            const void* var_ptr = varref->ref.id;
            bool found = false;
            // First pass: match by pointer identity (exact match)
            if (var_ptr) {
                for (size_t i = 0; i < parent_locals.size(); ++i) {
                    if (parent_locals[i].local_var_ptr == var_ptr) {
                        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOCAL_VARREF));
                        writer.writeStringRef(std::to_string(i).c_str());
                        found = true;
                        break;
                    }
                }
            }
            // Second pass: fall back to name match (for cases where pointer isn't available)
            if (!found) {
                for (size_t i = 0; i < parent_locals.size(); ++i) {
                    if (varref->getName() && parent_locals[i].name == varref->getName()) {
                        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOCAL_VARREF));
                        writer.writeStringRef(std::to_string(i).c_str());
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                return true;
            }
        } else if (varref->getType() == VT_GLOBAL || varref->getType() == VT_THREAD_LOCAL) {
            Var* global_var = varref->ref.var;
            if (global_var) {
                // Find the global slot index by name
                for (size_t i = 0; i < parent_globals.size(); ++i) {
                    if (parent_globals[i].name == global_var->getName()) {
                        writer.writeU8(static_cast<uint8_t>(AOTExprKind::GLOBAL_VARREF));
                        writer.writeStringRef(std::to_string(i).c_str());
                        return true;
                    }
                }
                // Member defaults and other non-function payloads do not have
                // a SLOT_MAP global table.  Preserve the global variable
                // identity by name so the loader can resolve it in the target
                // program namespace instead of falling back to EXPR_TREE.
                const char* name = global_var->getName();
                if (name && *name) {
                    std::string ref = "name:";
                    ref += name;
                    writer.writeU8(static_cast<uint8_t>(AOTExprKind::GLOBAL_VARREF));
                    writer.writeStringRef(ref.c_str());
                    return true;
                }
            }
        }
    }

    // SelfVarrefNode: self variable reference
    if (auto* svn = dynamic_cast<const SelfVarrefNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_VARREF));
        writer.writeStringRef(svn->str ? svn->str : "");
        return true;
    }

    // StaticClassVarRefNode: static class variable reference
    if (auto* sv = dynamic_cast<const StaticClassVarRefNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_VARREF));
        std::string class_ref = qore_aot_encode_class_ref(&sv->qc);
        writer.writeStringRef(class_ref.c_str());
        writer.writeStringRef(sv->str.c_str());
        return true;
    }
    if (auto* dsv = dynamic_cast<const DeferredStaticClassMemberRefNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_VARREF));
        writer.writeStringRef(dsv->class_path.c_str());
        writer.writeStringRef(dsv->member_name.c_str());
        return true;
    }

    // Heap-allocated scalar literal nodes: large integers and exceptional floats
    // cannot be represented as immediate QoreValue scalars.
    if (auto* i = dynamic_cast<const QoreBigIntNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_INT));
        writer.writeI64(i->getValue());
        return true;
    }
    if (auto* f = dynamic_cast<const QoreBigFloatNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_FLOAT));
        writer.writeF64(f->getValue());
        return true;
    }
    if (dynamic_cast<const QoreNothingNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
        return true;
    }

    // QoreStringNode: string literal constant (e.g., "" as constructor arg)
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_STRING));
        writer.writeStringRef(str->c_str(), str->size());
        return true;
    }

    // QoreNullNode: NULL constant
    if (dynamic_cast<const QoreNullNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NULL));
        return true;
    }

    // ScopedObjectCallNode: namespace-scoped constructor call (e.g., "new Ns::Foo(args)")
    // Used in inline IR context where the QoreIRNewObjectInstruction::expr holds this node.
    // Must come BEFORE NewObjectCallNode since they have different class hierarchies.
    if (auto* socn = dynamic_cast<const ScopedObjectCallNode*>(node)) {
        if (socn->oc || socn->isDynamicObjectConstruct()) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::SCOPED_NEW_OBJECT));
            std::string class_ref = socn->oc
                ? qore_aot_encode_class_ref(socn->oc)
                : socn->getDynamicClassName();
            writer.writeStringRef(class_ref.c_str());
            if ((writer.feature_flags & QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO) != 0) {
                writeTypePathRef(writer, socn->getObjectTypeInfo());
            }
            // Try evaluated args first, fall back to parse args
            return write_args_prefer_qore(socn->getArgs(), socn->getParseArgs());
        }
    }

    // NewObjectCallNode: bare constructor call (e.g., "new Foo()")
    if (auto* no = dynamic_cast<const NewObjectCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::NEW_OBJECT));
        const QoreClass* qc = no->getClass();
        std::string class_ref = qore_aot_encode_class_ref(qc);
        writer.writeStringRef(class_ref.c_str());
        if ((writer.feature_flags & QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO) != 0) {
            writeTypePathRef(writer, no->getObjectTypeInfo());
        }
        // Serialize constructor args if available
        return write_args_prefer_qore(no->getArgs(), no->getParseArgs());
    }

    // QoreNumberNode: number literal constant
    if (auto* num = dynamic_cast<const QoreNumberNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NUMBER));
        QoreString str;
        num->toString(str);
        writer.writeStringRef(str.c_str());
        return true;
    }

    // BinaryNode: binary literal constant
    if (auto* bin = dynamic_cast<const BinaryNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_BINARY));
        std::string hex;
        const unsigned char* data = static_cast<const unsigned char*>(bin->getPtr());
        size_t len = bin->size();
        hex.reserve(len * 2);
        for (size_t i = 0; i < len; ++i) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", data[i]);
            hex.append(buf);
        }
        writer.writeStringRef(hex.c_str());
        return true;
    }

    // QoreHashNode: already-evaluated hash (e.g., {}, {"key": "val"}) — serialize as HASH_LITERAL
    // Only handles string-keyed hashes with simple values for safety; empty hash is the common case.
    if (auto* qhn = dynamic_cast<const QoreHashNode*>(node)) {
        // If this is a hashdecl-typed hash, serialize as HASHDECL_NEW to preserve the type.
        // The reader creates a NewHashDeclNode which evaluates to a properly typed hash.
        const TypedHashDecl* qhd = qhn->getHashDecl();
        if (qhd) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASHDECL_NEW));
            writer.writeStringRef(qhd->getNamespacePath().c_str());
            if (qhn->empty()) {
                writer.writeU8(0);  // no args
            } else {
                // Non-empty hashdecl hash: serialize the hash contents as a single HASH_LITERAL arg
                writer.writeU8(1);  // 1 arg (the hash contents)
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
                writer.writeU8(static_cast<uint8_t>(qhn->size()));
                ConstHashIterator it(qhn);
                while (it.next()) {
                    writer.writeStringRef(it.getKey());
                    if (!write_inline_expr(it.get())) {
                        return false;
                    }
                }
            }
            return true;
        }
        if (qhn->size() <= 255) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
            writer.writeU8(static_cast<uint8_t>(qhn->size()));
            ConstHashIterator it(qhn);
            while (it.next()) {
                writer.writeStringRef(it.getKey());
                if (!write_inline_expr(it.get())) {
                    return false;
                }
            }
            return true;
        }
    }

    // QoreListNode: already-evaluated list constant — serialize as LIST_LITERAL
    if (auto* qln = dynamic_cast<const QoreListNode*>(node)) {
        if (qln->size() <= 255) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::LIST_LITERAL));
            writer.writeU8(static_cast<uint8_t>(qln->size()));
            for (size_t i = 0; i < qln->size(); ++i) {
                if (!write_inline_expr(qln->retrieveEntry(i))) {
                    return false;
                }
            }
            return true;
        }
    }

    // QoreParseListNode: parse-time list literal with unevaluated elements
    // (e.g., list of hashdecl init expressions in constructor args)
    if (auto* pln = dynamic_cast<const QoreParseListNode*>(node)) {
        if (pln->size() <= 255) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::LIST_LITERAL));
            writer.writeU8(static_cast<uint8_t>(pln->size()));
            for (size_t i = 0; i < pln->size(); ++i) {
                if (!write_inline_expr(pln->get(i))) {
                    return false;
                }
            }
            return true;
        }
    }

    // QoreParseHashNode: hash literal with runtime values (e.g., ("key": local_var))
    if (auto* phn = dynamic_cast<const QoreParseHashNode*>(node)) {
        const QoreParseHashNode::nvec_t& keys = phn->getKeys();
        const QoreParseHashNode::nvec_t& vals = phn->getValues();
        assert(keys.size() == vals.size());
        if (keys.size() <= 255) {
            bool const_keys = true;
            for (const QoreValue& key : keys) {
                if (key.needsEval()) {
                    const_keys = false;
                    break;
                }
            }
            if (!const_keys) {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::PARSE_HASH));
                writer.writeU8(static_cast<uint8_t>(keys.size()));
                for (size_t i = 0; i < keys.size(); ++i) {
                    if (!write_inline_expr(keys[i]) || !write_inline_expr(vals[i])) {
                        return false;
                    }
                }
                return true;
            } else {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
                writer.writeU8(static_cast<uint8_t>(keys.size()));
                for (size_t i = 0; i < keys.size(); ++i) {
                    // Keys are typically string constants
                    QoreStringValueHelper key(keys[i]);
                    writer.writeStringRef(key->c_str());
                    if (!write_inline_expr(vals[i])) {
                        return false;
                    }
                }
                return true;
            }
        }
        // Hash too large for u8 count — fail below with full unsupported-expression context.
    }

    // QoreHashObjectDereferenceOperatorNode: hash.key or hash{key} dereference chains
    // (e.g., oh.paths."/create".post — left=base, right=key, both recursively classified)
    if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_DEREF));
        if ((writer.feature_flags & QORE_AOT_FEAT_HASH_DEREF_TYPEINFO) != 0) {
            writeTypePathRef(writer, hd->getTypeInfo());
        }
        return write_inline_expr(hd->getLeft()) && write_inline_expr(hd->getRight());
    }

    if (dynamic_cast<const QoreWeakAssignmentOperatorNode*>(node)) {
        qoreAOTSetExprSerializationError("unsupported inline native AOT weak assignment for "
            + qoreAOTDescribeExpr(expr)
            + "; no fallback marker was emitted; add a native AOTExprKind serializer/reader for weak assignment "
                "or lower this operation to native IR");
        return false;
    }

    if (auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::ASSIGN));
        return write_inline_expr(assign->getLeft()) && write_inline_expr(assign->getRight());
    }
    // Compound-assignment operators: native encodings so they round-trip without the legacy
    // EXPR_TREE fallback when they appear as call args / sub-expressions (e.g. SqlUtil dropColumn's
    // self-call arg `i += 1`).  NOTE: QoreDivideEquals inherits QoreMultiplyEquals — check Divide first.
    if (auto* op = dynamic_cast<const QorePlusEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::PLUS_EQ));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreMinusEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::MINUS_EQ));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreDivideEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::DIVIDE_EQ));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreMultiplyEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::MULTIPLY_EQ));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreModuloEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::MODULO_EQ));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreAndEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::AND_EQ));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreOrEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::OR_EQ));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreXorEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::XOR_EQ));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreShiftLeftEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SHL_EQ));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreShiftRightEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SHR_EQ));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }

    // Plus operator: used inside hash/list/constructor argument literals.
    // Encoding it directly avoids falling back to EXPR_TREE for common
    // expressions such as `hdr + ("Content-Type": content_type)`.
    if (auto* plus = dynamic_cast<const QorePlusOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::PLUS));
        return write_inline_expr(plus->getLeft()) && write_inline_expr(plus->getRight());
    }

    if (auto* range = dynamic_cast<const QoreRangeOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::RANGE));
        return classifyAndWriteExpr(writer, range->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, range->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }

    // Square-bracket operator: required for nested lvalues such as
    // `\hash[key]` carried inside ParseReferenceNode metadata.
    if (auto* sq = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SQUARE_BRACKET));
        return write_inline_expr(sq->getLeft()) && write_inline_expr(sq->getRight());
    }
    if (auto* sbr = dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SQUARE_BRACKET_RANGE));
        return write_inline_expr(sbr->get(0)) && write_inline_expr(sbr->get(1))
            && write_inline_expr(sbr->get(2));
    }
    if (auto* exists = dynamic_cast<const QoreExistsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::EXISTS));
        return classifyAndWriteExpr(writer, exists->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* ia = dynamic_cast<const QoreImplicitArgumentNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::IMPLICIT_ARG));
        writer.writeI64(static_cast<int64_t>(ia->getOffset()));
        return true;
    }
    if (auto* minus = dynamic_cast<const QoreMinusOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::MINUS));
        return write_inline_expr(minus->getLeft()) && write_inline_expr(minus->getRight());
    }
    if (auto* unary_minus = dynamic_cast<const QoreUnaryMinusOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::UNARY_MINUS));
        return write_inline_expr(unary_minus->getExp());
    }
    if (auto* multiply = dynamic_cast<const QoreMultiplicationOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::MULTIPLY));
        return write_inline_expr(multiply->getLeft()) && write_inline_expr(multiply->getRight());
    }
    if (auto* divide = dynamic_cast<const QoreDivisionOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::DIVIDE));
        return write_inline_expr(divide->getLeft()) && write_inline_expr(divide->getRight());
    }
    if (auto* modulo = dynamic_cast<const QoreModuloOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::MODULO));
        return write_inline_expr(modulo->getLeft()) && write_inline_expr(modulo->getRight());
    }
    if (auto* op = dynamic_cast<const QoreBinaryAndOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::BIT_AND));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreBinaryOrOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::BIT_OR));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreBinaryXorOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::BIT_XOR));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreShiftLeftOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SHIFT_LEFT));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* op = dynamic_cast<const QoreShiftRightOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SHIFT_RIGHT));
        return write_inline_expr(op->getLeft()) && write_inline_expr(op->getRight());
    }
    if (auto* keys = dynamic_cast<const QoreKeysOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::KEYS));
        return classifyAndWriteExpr(writer, keys->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (dynamic_cast<const QoreImplicitElementNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::IMPLICIT_ELEM));
        return true;
    }
    if (auto* inst = dynamic_cast<const QoreInstanceOfOperatorNode*>(node)) {
        const QoreTypeInfo* ti = inst->getInstanceTypeInfo();
        std::string type_path = getTypePath(ti);
        if (!type_path.empty()) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::INSTANCEOF));
            writer.writeStringRef(type_path.c_str());
            return classifyAndWriteExpr(writer, inst->getExp(), parent_locals,
                parent_globals, const_reverse_map);
        }
    }
    if (auto* regex = dynamic_cast<const QoreRegexMatchOperatorNode*>(node)) {
        AOTExprKind regex_kind = AOTExprKind::REGEX_MATCH;
        if (dynamic_cast<const QoreRegexExtractOperatorNode*>(node)) {
            regex_kind = AOTExprKind::REGEX_EXTRACT;
        } else if (dynamic_cast<const QoreRegexNMatchOperatorNode*>(node)) {
            regex_kind = AOTExprKind::REGEX_NMATCH;
        }
        QoreRegex* re = regex->getRegex();
        const char* pattern = re ? re->getPatternCStr() : nullptr;
        if (pattern) {
            writer.writeU8(static_cast<uint8_t>(regex_kind));
            writer.writeStringRef(pattern);
            writer.writeI64(re->getOptions() | (re->isGlobal() ? QRE_GLOBAL : 0));
            return classifyAndWriteExpr(writer, regex->getExp(), parent_locals,
                parent_globals, const_reverse_map);
        }
    }
    if (auto* op = dynamic_cast<const QorePreDecrementOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::PRE_DEC));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QorePreIncrementOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::PRE_INC));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreIntPostDecrementOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::POST_DEC));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreIntPostIncrementOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::POST_INC));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QorePostDecrementOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::POST_DEC));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QorePostIncrementOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::POST_INC));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalOrOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_OR));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalAndOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_AND));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalAbsoluteNotEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_ANE));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalAbsoluteEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_AEQ));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalNotEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_NE));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_EQ));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalLessThanOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_LT));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalGreaterThanOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_GT));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalLessThanOrEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_LE));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalGreaterThanOrEqualsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_GE));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreLogicalNotOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_NOT));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreNullCoalescingOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::NULL_COAL));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreValueCoalescingOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::VALUE_COAL));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreQuestionMarkOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::QUESTION));
        return classifyAndWriteExpr(writer, op->get(0), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->get(1), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->get(2), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreFoldrOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::FOLDR));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreFoldlOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::FOLDL));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreMapOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::MAP));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreMapSelectOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::MAP_SELECT));
        return classifyAndWriteExpr(writer, op->get(0), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->get(1), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->get(2), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreHashMapSelectOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_MAP_SELECT_OP));
        return classifyAndWriteExpr(writer, op->get(0), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->get(1), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->get(2), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->get(3), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreHashMapOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_MAP_OP));
        return classifyAndWriteExpr(writer, op->get(0), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->get(1), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->get(2), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreSelectOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELECT));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreIterateOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::ITERATE));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreStreamingOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::STREAMING));
        writer.writeU8(static_cast<uint8_t>(op->getKind()));
        return classifyAndWriteExpr(writer, op->getPredicate(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getSource(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreTrimOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::TRIM));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreChompOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CHOMP));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QorePopOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::POP));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreShiftOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SHIFT));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QorePushOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::PUSH));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreUnshiftOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::UNSHIFT));
        return classifyAndWriteExpr(writer, op->getLeft(), parent_locals,
                parent_globals, const_reverse_map)
            && classifyAndWriteExpr(writer, op->getRight(), parent_locals,
                parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreElementsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::ELEMENTS));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreDeleteOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::DELETE));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreRemoveOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::REMOVE));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* op = dynamic_cast<const QoreBackgroundOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::BACKGROUND));
        return classifyAndWriteExpr(writer, op->getExp(), parent_locals,
            parent_globals, const_reverse_map);
    }
    if (auto* cr = dynamic_cast<const ContextrefNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONTEXT_REF));
        writer.writeStringRef(cr->str ? cr->str : "");
        return true;
    }
    if (dynamic_cast<const ContextRowNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONTEXT_ROW));
        return true;
    }
    if (auto* ccr = dynamic_cast<const ComplexContextrefNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_CONTEXT_REF));
        writer.writeStringRef(ccr->name ? ccr->name : "");
        writer.writeStringRef(ccr->member ? ccr->member : "");
        writer.writeI64(static_cast<int64_t>(ccr->stack_offset));
        return true;
    }

    // ParseReferenceNode: \var lvalue reference — serialize inner lvalue expression
    if (auto* prn = dynamic_cast<const ParseReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::PARSE_REF));
        writeTypePathRef(writer, prn->getTypeInfo());
        return write_inline_expr(prn->getLVExp());
    }

    // NewHashDeclNode: hashdecl construction (e.g., <StatInfo>{"size": 1})
    if (auto* nhd = dynamic_cast<const NewHashDeclNode*>(node)) {
        if (nhd->hd) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASHDECL_NEW));
            writer.writeStringRef(nhd->hd->getNamespacePath().c_str());
        } else if (nhd->isDynamicHashDeclConstruct()) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASHDECL_NEW));
            writer.writeStringRef(nhd->getDynamicHashDeclName().c_str());
        } else {
            return false;
        }
        // Serialize constructor args (typically a single hash initializer)
        return write_parse_arg_list(nhd->args);
    }

    // NewComplexHashNode: complex typed hash construction
    if (auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
        if (nch->typeInfo) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_HASH_NEW));
            writeTypePathRef(writer, nch->typeInfo);
            // Serialize constructor args
            return write_parse_arg_list(nch->args);
        }
    }

    // NewComplexListNode: complex typed list construction
    if (auto* ncl = dynamic_cast<const NewComplexListNode*>(node)) {
        if (ncl->typeInfo) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_LIST_NEW));
            writeTypePathRef(writer, ncl->typeInfo);
            // Serialize constructor arg (single QoreValue)
            if (ncl->args.hasNode()) {
                writer.writeU8(1);
                if (!write_inline_expr(ncl->args)) {
                    return false;
                }
            } else {
                writer.writeU8(0);
            }
            return true;
        }
    }

    // NewComplexBufferNode: complex typed buffer construction
    if (auto* ncb = dynamic_cast<const NewComplexBufferNode*>(node)) {
        if (ncb->typeInfo) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_BUFFER_NEW));
            writeTypePathRef(writer, ncb->typeInfo);
            writer.writeU8(static_cast<uint8_t>(ncb->initKind));
            if (ncb->args.hasNode()) {
                writer.writeU8(1);
                return write_inline_expr(ncb->args);
            }
            writer.writeU8(0);
            return true;
        }
    }

    // QoreHashDeclCastOperatorNode: cast<StatInfo>(hash)
    if (auto* hdc = dynamic_cast<const QoreHashDeclCastOperatorNode*>(node)) {
        const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(hdc->getCastTypeInfo());
        if (hd || hdc->getCastTypeInfo() == hashTypeInfo || hdc->isDynamicHashDeclCast()) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_HASHDECL));
            writer.writeStringRef(hd ? hd->getNamespacePath().c_str()
                : (hdc->isDynamicHashDeclCast() ? hdc->getDynamicHashDeclName().c_str() : "hash"));
            writer.writeU8(hdc->isOrNothing() ? 1 : 0);
            // Serialize the inner expression being cast
            return write_cast_inner(hdc->getExp());
        }
    }

    // QoreComplexHashCastOperatorNode: cast<hash<string, int>>(hash)
    if (auto* chc = dynamic_cast<const QoreComplexHashCastOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_COMPLEX_HASH));
        writeTypePathRef(writer, chc->getCastTypeInfo());
        writer.writeU8(chc->isOrNothing() ? 1 : 0);
        return write_cast_inner(chc->getExp());
    }

    // QoreComplexListCastOperatorNode: cast<list<int>>(list)
    if (auto* clc = dynamic_cast<const QoreComplexListCastOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_COMPLEX_LIST));
        const QoreTypeInfo* ti = clc->getCastTypeInfo();
        if (ti) {
            writeTypePathRef(writer, ti);
        } else {
            writer.writeStringRef("list");
        }
        writer.writeU8(clc->isOrNothing() ? 1 : 0);
        return write_cast_inner(clc->getExp());
    }

    // QoreClassCastOperatorNode: cast<ClassName>(obj)
    if (auto* cc = dynamic_cast<const QoreClassCastOperatorNode*>(node)) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(cc->getCastTypeInfo());
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_CLASS));
        std::string class_ref = qc ? qore_aot_encode_class_ref(qc) : "object";
        writer.writeStringRef(class_ref.c_str());
        writer.writeU8(cc->isOrNothing() ? 1 : 0);
        return write_cast_inner(cc->getExp());
    }

    // QoreEnumCastOperatorNode: cast<EnumType>(val)
    if (auto* ec = dynamic_cast<const QoreEnumCastOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_ENUM));
        writeTypePathRef(writer, ec->getCastTypeInfo());
        writer.writeU8(ec->isOrNothing() ? 1 : 0);
        return write_cast_inner(ec->getExp());
    }

    // QoreScalarCastOperatorNode: cast<int>(val), cast<auto!>(val), etc.
    if (auto* sc = dynamic_cast<const QoreScalarCastOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_SCALAR));
        std::string type_path = getAOTSerializableTypePath(sc->getCastTypeInfo());
        writer.writeStringRef(type_path.c_str());
        writer.writeU8(sc->isOrNothing() ? 1 : 0);
        return write_cast_inner(sc->getExp());
    }

    // QoreClosureParseNode: closure/lambda in expression context (e.g., hash literal values)
    if (auto* closure = dynamic_cast<const QoreClosureParseNode*>(node)) {
        UserClosureFunction* ucf = closure->getFunction();
        if (ucf) {
            auto* variant = static_cast<const UserClosureVariant*>(ucf->first());
            if (variant) {
                const UserSignature* sig = const_cast<UserClosureVariant*>(variant)->getUserSignature();
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CLOSURE_CREATE));

                // Write flags: "lambda,in_method" (matches write_slot_CLOSURE_CREATE ref1 format)
                std::string flags = std::string(closure->isLambda() ? "1" : "0") + ","
                    + (closure->isInMethod() ? "1" : "0");
                writer.writeStringRef(flags.c_str());

                // Write class type path (ref2)
                const QoreTypeInfo* cti = ucf->getClassType();
                writeTypePathRef(writer, cti);

                if ((writer.feature_flags & QORE_AOT_FEAT_NATIVE_CLOSURE_BODY) != 0) {
                    // Inline closure records embedded in fallback IR do not own a
                    // native slot-map entry.  The owning compiled body's expression
                    // slot carries the native key when one is available.
                    writer.writeStringRef("");
                }

                // Write return type
                writeTypePathRef(writer, sig->getReturnTypeInfo());

                // Write params: count, then (name, type_path, default) per param
                unsigned num_params = sig->numParams();
                writer.writeU16(static_cast<uint16_t>(num_params));
                for (unsigned p = 0; p < num_params; ++p) {
                    const char* pname = sig->getName(p);
                    writer.writeStringRef(pname ? pname : "");
                    writeTypePathRef(writer, sig->getParamTypeInfo(p));
                    bool has_default = sig->hasDefaultArg(p);
                    writer.writeU8(has_default ? 1 : 0);
                    if (has_default) {
                        std::string default_error;
                        if (!qoreAOTWriteDefaultArgValuePayload(writer, sig->getDefaultArgList()[p],
                                "closure", ucf->getName(), pname, &default_error)) {
                            qoreAOTSetExprSerializationError(std::move(default_error));
                            return false;
                        }
                    }
                }
                uint16_t closure_flags = 0;
                if (variant->hasVarargs()) {
                    closure_flags |= 0x0001;
                }
                if (sig->hasVarargs()) {
                    closure_flags |= 0x0004;
                }
                writer.writeU16(closure_flags);

                // Lower closure before writing captures so embedded expression
                // payloads (for example \local reference expressions) can add
                // parent locals that are absent from the parser's closure vlist.
                const QoreIRFunction* closure_ir = const_cast<UserClosureVariant*>(variant)->getCachedIR();
                QoreIRFunction* owned_ir = nullptr;
                if (!closure_ir) {
                    std::string closure_error;
                    owned_ir = ::lowerClosureForSerialization(variant, &closure_error);
                    closure_ir = owned_ir;
                    if (!closure_ir) {
                        qoreAOTSetExprSerializationError("failed to lower closure for AOT serialization in "
                            + qoreAOTDescribeExpr(expr) + ": "
                            + (closure_error.empty() ? std::string("unknown error") : closure_error));
                        return false;
                    }
                }

                const LVarSet* vlist = const_cast<UserClosureFunction*>(ucf)->getVList();
                qoreAOTPruneClosureIRBodyLocals(const_cast<QoreIRFunction*>(closure_ir), sig, vlist);
                if (!qoreAOTWriteClosureCaptures(writer, vlist, closure_ir, parent_locals)) {
                    qoreAOTSetExprSerializationError("closure captures exceed AOT format limit in "
                        + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
                    delete owned_ir;
                    return false;
                }

                writer.writeU8(1);  // has_ir
                uint32_t size_pos = writer.position();
                writer.writeU32(0);  // placeholder

                // Expression trees inside serialized closure IR use the
                // same slot domain as the closure IR local slot table.
                // This keeps ParseReferenceNode lvalues and other embedded
                // VarRefNodes aligned with the LocalVar* objects resolved by
                // deserializeIRFunction().
                std::vector<AOTLocalSlotId> closure_locals;
                uint32_t max_slot = 0;
                bool has_slots = false;
                for (const auto& [lv, slot_id] : closure_ir->local_var_slots) {
                    if (lv) {
                        if (!has_slots || slot_id > max_slot) {
                            max_slot = slot_id;
                        }
                        has_slots = true;
                    }
                }
                if (has_slots) {
                    closure_locals.resize(static_cast<size_t>(max_slot) + 1);
                    for (const auto& [lv, slot_id] : closure_ir->local_var_slots) {
                        if (lv) {
                            AOTLocalSlotId& slot = closure_locals[slot_id];
                            slot.local_var_ptr = reinterpret_cast<const void*>(lv);
                            slot.name = lv->getName() ? lv->getName() : "";
                        }
                    }
                }

                auto writeExpr = [&closure_locals, &parent_globals, const_reverse_map](
                        QoreAOTBinaryWriter& w, const QoreValue& e) -> bool {
                    return classifyAndWriteExpr(w, e, closure_locals, parent_globals,
                        const_reverse_map);
                };

                if (!::serializeIRFunction(writer, *closure_ir, writeExpr)) {
                    qoreAOTSetExprSerializationError("failed to serialize closure IR for "
                        + qoreAOTDescribeExpr(expr));
                    delete owned_ir;
                    return false;
                }
                uint32_t end_pos = writer.position();
                writer.patchU32(size_pos, end_pos - size_pos - 4);
                delete owned_ir;
                return true;
            }
        }
    }

    if (auto* crc = dynamic_cast<const CallReferenceCallNode*>(node)) {
        const QoreListNode* args = crc->getArgs();
        const QoreParseListNode* pargs = crc->getParseArgs();
        size_t nargs = args ? args->size() : (pargs ? pargs->size() : 0);
        if (nargs > 255) {
            qoreAOTSetExprSerializationError("CALLREF_CALL inline payload has more than 255 arguments in "
                + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
            return false;
        }
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CALLREF_CALL));
        if (!classifyAndWriteExpr(writer, crc->getExp(), parent_locals,
                parent_globals, const_reverse_map)) {
            return false;
        }
        writer.writeU8(static_cast<uint8_t>(nargs));
        for (size_t j = 0; j < nargs; ++j) {
            QoreValue arg = args ? args->retrieveEntry(j) : pargs->get(j);
            if (!classifyAndWriteExpr(writer, arg, parent_locals,
                    parent_globals, const_reverse_map)) {
                return false;
            }
        }
        return true;
    }

    // Call/method references inside container literals must serialize as
    // reference metadata, not as unevaluated AST constants.  The reader rebuilds
    // equivalent reference nodes that CreateCallRef/CreateMethodRef can evaluate
    // to runtime code values.
    if (auto* mcr = dynamic_cast<const LocalMethodCallReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::BOUND_METHOD_REF));
        const QoreMethod* method = mcr->getMethod();
        const QoreClass* qc = method ? method->getClass() : nullptr;
        std::string class_path = qore_aot_encode_class_ref(qc);
        writer.writeStringRef(class_path.c_str());
        writer.writeStringRef(method ? method->getName() : "");
        return true;
    }
    if (auto* scr = dynamic_cast<const LocalStaticMethodCallReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_METHOD_REF));
        const QoreMethod* method = scr->getMethod();
        const QoreClass* qc = method ? method->getClass() : nullptr;
        std::string class_path = qore_aot_encode_class_ref(qc);
        writer.writeStringRef(class_path.c_str());
        writer.writeStringRef(method ? method->getName() : "");
        return true;
    }
    if (auto* dscr = dynamic_cast<const DeferredStaticMethodCallReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::DEFERRED_STATIC_METHOD_REF));
        writer.writeStringRef(dscr->getClassPath().c_str());
        writer.writeStringRef(dscr->getMethodName().c_str());
        return true;
    }
    if (auto* dfcr = dynamic_cast<const DeferredFunctionCallReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::DEFERRED_FUNCTION_REF));
        writer.writeStringRef(dfcr->getFunctionName().c_str());
        return true;
    }
    if (auto* fcr = dynamic_cast<const LocalFunctionCallReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::FUNC_CALL_REF));
        QoreFunction* f = fcr->getFunction();
        writer.writeStringRef(f ? f->getName() : "");
        return true;
    }
    if (auto* smr = dynamic_cast<const ParseSelfMethodReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_METHOD_REF));
        writer.writeStringRef(smr->getMethodName().c_str());
        return true;
    }
    if (auto* omr = dynamic_cast<const ParseObjectMethodReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::OBJ_METHOD_REF_EXPR));
        writer.writeStringRef(omr->getMethodName().c_str());
        return classifyAndWriteExpr(writer, omr->getExp(), parent_locals, parent_globals,
            const_reverse_map);
    }

    // RuntimeConstantRefNode: reference to a compile-time constant
    // Look up the constant's evaluated value node in the reverse map
    if (auto* rcr = dynamic_cast<const RuntimeConstantRefNode*>(node)) {
        std::string path;
        if (qore_aot_resolve_runtime_constant_path(rcr, const_reverse_map, path)) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
            writer.writeStringRef(path.c_str());
            return true;
        }
        ConstantEntry* ce = rcr->getConstantEntry();
        if (ce && ce->isExternalStub()) {
            return false;
        }
        if (ce) {
            QoreValue sv = ce->getReferencedValue();
            qore_type_t st = sv.getType();
            bool can_inline = !sv.needsEval()
                && !(sv.hasNode() && sv.getInternalNode() == node)
                && (sv.isEnum() || st == NT_INT || st == NT_FLOAT || st == NT_BOOLEAN
                    || st == NT_CHAR || st == NT_STRING || st == NT_DATE || st == NT_NUMBER || st == NT_BINARY
                    || st == NT_NULL || st == NT_NOTHING);
            if (can_inline) {
                bool ok = classifyAndWriteExpr(writer, sv, parent_locals, parent_globals, const_reverse_map);
                sv.discard(nullptr);
                return ok;
            }
            sv.discard(nullptr);
        }
    }

    // QoreDotEvalOperatorNode: obj.method(args) — serialize as DOT_EVAL_TARGET.
    // The inline form (used when this node appears as an argument or sub-expression
    // of another expression — e.g. STATIC_METHOD_CALL arg) must carry the full
    // information needed to rebuild the AST at load time: class_path, method_name,
    // is_pseudo, the target expression (`left`), and the argument list.  The slot
    // map form (see write_slot_DOT_EVAL_TARGET) writes only the method identity
    // because there the target and args are separate slots.
    if (auto* de = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
        MethodCallNode* mc = de->getMethodCall();
        if (mc) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::DOT_EVAL_TARGET));
            const QoreClass* qc = mc->getClass();
            std::string class_path;
            if (qc) {
                class_path = qore_aot_encode_class_ref(qc, mc->isPseudo());
            }
            writer.writeStringRef(class_path.c_str());
            writer.writeStringRef(mc->getName() ? mc->getName() : "");
            writer.writeU8(mc->isPseudo() ? 1 : 0);
            // Target expression (left-hand side of the dot)
            if (!classifyAndWriteExpr(writer, de->getExpression(),
                    parent_locals, parent_globals, const_reverse_map)) {
                return false;
            }
            // Method args: prefer the evaluated args list, fall back to parse_args
            const QoreListNode* call_args = mc->getArgs();
            const QoreParseListNode* parse_args = mc->getParseArgs();
            size_t num_args = 0;
            if (call_args) {
                num_args = call_args->size();
            } else if (parse_args) {
                num_args = parse_args->size();
            }
            if (num_args > 255) {
                qoreAOTSetExprSerializationError("DOT_EVAL_TARGET inline payload has more than 255 arguments in "
                    + qoreAOTDescribeExpr(expr) + "; no fallback marker was emitted");
                return false;
            }
            writer.writeU8(static_cast<uint8_t>(num_args));
            for (size_t j = 0; j < num_args; ++j) {
                QoreValue arg = call_args
                    ? call_args->retrieveEntry(j)
                    : parse_args->get(j);
                if (!classifyAndWriteExpr(writer, arg,
                        parent_locals, parent_globals, const_reverse_map)) {
                    return false;
                }
            }
            return true;
        }
    }

    // Try reverse constant lookup for unsupported node types (e.g., QoreObject)
    if (const_reverse_map) {
        if (const std::string* path = aotFindConstantReverseMapPath(const_reverse_map, node)) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
            writer.writeStringRef(path->c_str());
            return true;
        }
    }

    // The old path serialized arbitrary AST as EXPR_TREE here.  That hides
    // native lowering/classification gaps, so make new AOT metadata fail
    // with a diagnostic instead of emitting EXPR_TREE.
    {
        std::string diag = qoreAOTBuildExprTreeFallbackDiagnostic(expr, parent_locals, const_reverse_map);
        if (diag.find("EXPR_TREE root=") != std::string::npos) {
            qoreAOTSetExprSerializationError(std::move(diag));
            return false;
        }
    }

    // QoreObject (or any other pointer-backed value) via program constant
    // reverse map — parse-time folding can leave a concrete QoreObject in
    // expression position (e.g. `Class::forName("...")` folds to a
    // Reflection::Class instance that lands inside a containing hash
    // literal).  If the CRM knows the node pointer, emit RUNTIME_CONST_REF
    // so the loader resolves it to the same named constant at load time.
    if (const_reverse_map) {
        if (const std::string* path = aotFindConstantReverseMapPath(const_reverse_map, node)) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
            writer.writeStringRef(path->c_str());
            return true;
        }
    }

    // Unsupported — fail before writing a fallback marker.
    trace_unsupported_expr("unsupported inline expression node", node);
    qoreAOTSetExprSerializationError("unsupported inline native AOT expression for "
        + qoreAOTDescribeExpr(expr)
        + "; no fallback marker was emitted; add a native AOTExprKind serializer/reader or lower this operation "
            "to native IR");
    printd(3, "AOT: handler IR unsupported expr type '%s' for serialization\n",
        node->getTypeName());
    return false;
}

static bool aotCallRelocationLess(const QoreAOTCallRelocationRecord& a,
        const QoreAOTCallRelocationRecord& b) {
    if (a.function_name != b.function_name) {
        return a.function_name < b.function_name;
    }
    if (a.expr_slot != b.expr_slot) {
        return a.expr_slot < b.expr_slot;
    }
    if (a.qore_path != b.qore_path) {
        return a.qore_path < b.qore_path;
    }
    return static_cast<uint8_t>(a.target_kind) < static_cast<uint8_t>(b.target_kind);
}

static void writeCallRelocationRecord(QoreAOTBinaryWriter& writer,
        const QoreAOTCallRelocationRecord& rec) {
    writer.writeStringRef(rec.function_name.c_str());
    writer.writeU32(rec.expr_slot);
    writer.writeU8(static_cast<uint8_t>(rec.target_kind));
    writer.writeU8(static_cast<uint8_t>(rec.strictness));
    writer.writeU16(0);
    writer.writeStringRef(rec.qore_path.c_str());
    writer.writeStringRef(rec.signature_hash.c_str());
    writer.writeStringRef(rec.declaration_hash.c_str());
    writer.writeStringRef(rec.native_symbol.c_str());
    writer.writeStringRef(rec.fallback_descriptor.c_str());
}

static std::string aotCallRelocationFallbackDescriptor(const AOTCompiledFuncWithSlots& func,
        size_t expr_slot, const AOTExprSlotId& expr) {
    std::string desc = func.name;
    desc += "#expr";
    desc += std::to_string(expr_slot);
    desc += " kind=";
    desc += qoreAOTCallRelocationTargetKindName(expr.call_relocation_kind);
    if (!expr.ref1.empty()) {
        desc += " ref1=";
        desc += expr.ref1;
    }
    if (!expr.ref2.empty()) {
        desc += " ref2=";
        desc += expr.ref2;
    }
    return desc;
}

static bool serializeCallRelocations(QoreAOTBinaryWriter& writer,
        const std::vector<AOTCompiledFuncWithSlots>& funcs, std::string& error) {
    if ((writer.feature_flags & QORE_AOT_FEAT_CALL_RELOCATIONS) == 0) {
        return true;
    }

    std::vector<QoreAOTCallRelocationRecord> records;
    for (size_t i = 0; i < funcs.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT call-relocation collection")) {
            error = "operation cancelled during AOT call-relocation collection";
            return false;
        }
        const AOTCompiledFuncWithSlots& func = funcs[i];
        for (size_t j = 0; j < func.slot_ids.exprs.size(); ++j) {
            if (!((j + 1) % 100) && qore_check_cancel(nullptr, "AOT call-relocation collection")) {
                error = "operation cancelled during AOT call-relocation collection";
                return false;
            }
            const AOTExprSlotId& expr = func.slot_ids.exprs[j];
            if (expr.call_relocation_kind == QoreAOTCallRelocationTargetKind::NONE
                    || expr.reloc_qore_path.empty()) {
                continue;
            }
            QoreAOTCallRelocationRecord rec;
            rec.function_name = func.name;
            rec.expr_slot = static_cast<uint32_t>(j);
            rec.target_kind = expr.call_relocation_kind;
            rec.strictness = QoreAOTCallRelocationStrictness::OPTIONAL;
            rec.qore_path = expr.reloc_qore_path;
            rec.fallback_descriptor = aotCallRelocationFallbackDescriptor(func, j, expr);
            records.push_back(std::move(rec));
        }
    }

    if (records.size() > std::numeric_limits<uint32_t>::max()) {
        error = "too many AOT call relocations for u32 wire format";
        return false;
    }
    std::sort(records.begin(), records.end(), aotCallRelocationLess);

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::CALL_RELOCATIONS);
    writer.writeU16(QORE_AOT_CALL_RELOCATIONS_VERSION);
    writer.writeU16(0);
    writer.writeU32(static_cast<uint32_t>(records.size()));
    for (size_t i = 0; i < records.size(); ++i) {
        if (!((i + 1) % 100) && qore_check_cancel(nullptr, "AOT call-relocation serialization")) {
            writer.endSection(sec_idx);
            error = "operation cancelled during AOT call-relocation serialization";
            return false;
        }
        writeCallRelocationRecord(writer, records[i]);
    }
    writer.endSection(sec_idx);
    return true;
}

bool serializeSlotMaps(QoreAOTBinaryWriter& writer, const std::vector<AOTCompiledFuncWithSlots>& funcs,
        const AOTConstantReverseMap* const_reverse_map, std::string& error) {
    std::string registry_error;
    if (!qore_aot_validate_expr_registries(registry_error)) {
        error = "AOT expression registry validation failed: " + registry_error;
        return false;
    }
    if (!qore_aot_validate_expr_node_registry(registry_error)) {
        error = "AOT expression node registry validation failed: " + registry_error;
        return false;
    }
    if (!qore_aot_validate_inst_group_registry(registry_error)) {
        error = "AOT instruction group registry validation failed: " + registry_error;
        return false;
    }

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::SLOT_MAPS);

    // Number of function entries
    writer.writeU32(static_cast<uint32_t>(funcs.size()));

    for (auto& func : funcs) {
        AOTConstantReverseMap filtered_crm;
        const AOTConstantReverseMap* func_const_reverse_map =
            func.const_reverse_map_override ? func.const_reverse_map_override.get() : const_reverse_map;
        if (!func.const_reverse_map_override && const_reverse_map
                && (!func.const_reverse_map_exclude_fqns.empty()
                || !func.const_reverse_map_exclude_direct_fqn.empty())) {
            filtered_crm = aot_filter_constant_reverse_map(*const_reverse_map,
                func.const_reverse_map_exclude_fqns, func.const_reverse_map_exclude_direct_fqn);
            func_const_reverse_map = &filtered_crm;
        }

        // Save entry start position and write size placeholder
        uint32_t entry_size_pos = writer.position();
        writer.writeU32(0);  // placeholder for entry size (patched below)
        const uint32_t entry_payload_start = entry_size_pos + 4;
        const char* trace_entry_env = getenv("QORE_AOT_SLOT_TRACE");
        const bool trace_entry = trace_entry_env
            && (!*trace_entry_env || func.name.find(trace_entry_env) != std::string::npos);
        auto traceEntryOffset = [&writer, entry_payload_start, &func, trace_entry](const char* label) {
            if (trace_entry) {
                fprintf(stderr, "[aot-slot] func=%s %s off=%u\n",
                    func.name.c_str(), label, writer.position() - entry_payload_start);
            }
        };
        const bool wide_loc_tables = (writer.feature_flags & QORE_AOT_FEAT_WIDE_LOC_TABLES) != 0;
        auto writeLocTableCount = [&writer, wide_loc_tables, &func, &error](size_t count,
                const char* table_name) -> bool {
            if (wide_loc_tables) {
                if (count > std::numeric_limits<uint32_t>::max()) {
                    error = "too many ";
                    error += table_name;
                    error += " entries in function '";
                    error += func.name;
                    error += "': ";
                    error += std::to_string(count);
                    error += " exceeds u32 wire format";
                    return false;
                }
                writer.writeU32(static_cast<uint32_t>(count));
                return true;
            }
            if (count > std::numeric_limits<uint16_t>::max()) {
                error = "too many ";
                error += table_name;
                error += " entries in function '";
                error += func.name;
                error += "': ";
                error += std::to_string(count);
                error += " exceeds legacy u16 wire format; QORE_AOT_FEAT_WIDE_LOC_TABLES is required";
                return false;
            }
            writer.writeU16(static_cast<uint16_t>(count));
            return true;
        };

        // Function header
        writer.writeStringRef(func.name.c_str());
        writer.writeU16(static_cast<uint16_t>(func.num_locals));
        writer.writeU16(static_cast<uint16_t>(func.num_globals));
        writer.writeU16(static_cast<uint16_t>(func.num_exprs));
        writer.writeU16(static_cast<uint16_t>(func.num_stmts));
        writer.writeU16(static_cast<uint16_t>(func.slot_ids.regex_cases.size()));
        writer.writeU16(static_cast<uint16_t>(func.slot_ids.body_locals.size()));
        writer.writeU8(func.slot_ids.has_unsupported_exprs ? 1 : 0);
        writer.writeU8(static_cast<uint8_t>(func.num_lv_path_insts)); // was: padding byte
        traceEntryOffset("after header");

        // Local slot entries (in slot order)
        for (auto& local : func.slot_ids.locals) {
            writer.writeStringRef(local.name.c_str());
            writer.writeStringRef(local.type_path.c_str());
            writer.writeU8(local.flags);
            writer.writeU16(local.param_index);
            if ((writer.feature_flags & QORE_AOT_FEAT_LOCAL_DECL_ORDINAL) != 0) {
                writer.writeU32(local.body_ordinal);
            }
        }
        traceEntryOffset("after locals");

        // Global slot entries (in slot order)
        for (auto& global : func.slot_ids.globals) {
            writer.writeStringRef(global.name.c_str());
            writer.writeStringRef(global.type_path.c_str());
            writer.writeU8(global.is_thread_local ? 1 : 0);
            if ((writer.feature_flags & QORE_AOT_FEAT_GLOBAL_SLOT_FLAGS) != 0) {
                writer.writeU8(global.is_aot_import ? 1 : 0);
            }
        }
        traceEntryOffset("after globals");

        // Expression slot entries (in slot order)
        for (size_t expr_idx = 0; expr_idx < func.slot_ids.exprs.size(); ++expr_idx) {
            auto& expr = func.slot_ids.exprs[expr_idx];
            if (const char* trace = getenv("QORE_AOT_SLOT_TRACE")) {
                bool match = !*trace;
                if (!match && func.name.find(trace) != std::string::npos) {
                    match = true;
                }
                if (!match && expr.ref1.find(trace) != std::string::npos) {
                    match = true;
                }
                if (!match && expr.ref2.find(trace) != std::string::npos) {
                    match = true;
                }
                if (!match && expr.ref3.find(trace) != std::string::npos) {
                    match = true;
                }
                if (match) {
                    fprintf(stderr, "[aot-slot] func=%s kind=%u ref1=%s ref2=%s ref3=%s\n",
                        func.name.c_str(), static_cast<unsigned>(expr.kind),
                        expr.ref1.c_str(), expr.ref2.c_str(), expr.ref3.c_str());
                }
            }
            if (expr.kind == AOTExprKind::UNSUPPORTED || expr.kind == AOTExprKind::EXPR_TREE
                    || expr.kind == AOTExprKind::GENERIC_EVAL) {
                std::string detail;
                std::string prefix = "slot " + std::to_string(expr_idx);
                for (const std::string& d : func.slot_ids.unsupported_expr_details) {
                    if (d.rfind(prefix, 0) == 0) {
                        detail = d;
                        break;
                    }
                }
                error = "AOT cannot serialize function '" + func.name
                    + "' expression " + prefix + ": fallback markers are forbidden";
                if (!detail.empty()) {
                    error += ": ";
                    error += detail;
                } else if (!expr.ref1.empty()) {
                    error += ": ";
                    error += expr.ref1;
                }
                error += "; unsupported expressions, EXPR_TREE, and GENERIC_EVAL are fatal for new AOT output";
                return false;
            }
            writer.writeU8(static_cast<uint8_t>(expr.kind));

            // Use registry dispatch for expression slot metadata serialization
            const auto* kinfo = getAOTExprSlotKindInfo(static_cast<uint8_t>(expr.kind));
            if (!kinfo || !kinfo->is_supported || !kinfo->write_fn) {
                error = "unsupported expression slot kind " + std::to_string(static_cast<uint8_t>(expr.kind))
                    + " in function '" + func.name + "'";
                qoreAOTAppendExprSlotContext(error, expr, expr_idx);
                return false;
            }
            AOTExprSlotWriteCtx wctx{writer, expr, func.slot_ids.locals, func.slot_ids.globals,
                func_const_reverse_map};
            qoreAOTClearExprSerializationError();
            bool slot_ok = kinfo->write_fn(wctx);
            std::string expr_error;
            bool expr_error_set = qoreAOTTakeExprSerializationError(expr_error);
            if (!slot_ok || expr_error_set) {
                if (error.empty()) {
                    error = "failed to serialize expression slot kind "
                        + std::to_string(static_cast<uint8_t>(expr.kind))
                        + " (" + (kinfo->name ? kinfo->name : "?")
                        + ") in function '" + func.name + "' slot "
                        + std::to_string(expr_idx);
                    if (expr_error_set) {
                        error += ": ";
                        error += expr_error;
                    } else {
                        error += ": writer returned false without a nested expression diagnostic";
                    }
                    qoreAOTAppendExprSlotContext(error, expr, expr_idx);
                }
                return false;
            }
        }
        traceEntryOffset("after exprs");

        // Body local entries (in order)
        for (auto& bl : func.slot_ids.body_locals) {
            writer.writeStringRef(bl.name.c_str());
            writer.writeStringRef(bl.type_path.c_str());
            writer.writeU8((bl.is_closure ? 0x01 : 0) | (bl.read_only ? 0x02 : 0));
            writer.writeU32(bl.slot_id);
        }
        traceEntryOffset("after body-locals");

        // Regex case entries (in slot-index order)
        // Format per case: pattern_ref(u32) options(i64) is_negated(u8)
        for (auto& rc : func.slot_ids.regex_cases) {
            writer.writeStringRef(rc.pattern.c_str());
            writer.writeI64(rc.options);
            writer.writeU8(rc.is_negated ? 1 : 0);
        }
        traceEntryOffset("after regex");

        // LValuePath instruction entries (in slot-index order)
        for (auto& lvid : func.slot_ids.lv_path_insts) {
            writer.writeU16(lvid.opcode);
            writer.writeU8(lvid.weak);
            writer.writeU8(lvid.compound_op);
            writer.writeU8(lvid.unary_op);
            writer.writeU8(lvid.binary_mut_op);
            writer.writeU8(lvid.ternary_op);
            writer.writeU8(lvid.ref_rv);
            if ((writer.feature_flags & QORE_AOT_FEAT_LVPATH_DELETE_EXPR) != 0) {
                // Legacy reserved byte. New writers never set this feature; if
                // an embedding path does, emit "not present" so no executable
                // AST lvalue fallback metadata is serialized.
                writer.writeU8(0);
            }
            // Pattern info for RegexSubst / Transliterate binary_mut ops.
            // Emitted unconditionally as (present_flag u8) so readers can skip.
            writer.writeU8(lvid.pattern_empty ? 0 : 1);
            if (!lvid.pattern_empty) {
                writer.writeStringRef(lvid.pattern.c_str());
                writer.writeStringRef(lvid.pattern_newstr.c_str());
                writer.writeI64(lvid.pattern_options);
                writer.writeU8(lvid.pattern_global);
            }
            writer.writeU8(static_cast<uint8_t>(lvid.steps.size()));
            for (auto& step : lvid.steps) {
                writer.writeU8(step.kind);
                writer.writeU32(step.slot_id);
                writer.writeStringRef(step.name.c_str());
                writer.writeU32(step.operand_idx);
                // Slice steps: serialize the SSA id vector (matches wire
                // format of writeLValuePath in QoreAOTInstRegistry.cpp).
                // Feature-gated via QORE_AOT_FEAT_LVPATH_SLICE.
                if (step.kind == static_cast<uint8_t>(LVPathStepKind::HashKeySlice)
                        || step.kind == static_cast<uint8_t>(LVPathStepKind::ListIndexSlice)
                        || step.kind == static_cast<uint8_t>(LVPathStepKind::ListRangeSlice)) {
                    writer.writeU32(static_cast<uint32_t>(step.slice_operand_ids.size()));
                    for (uint32_t sid : step.slice_operand_ids) {
                        writer.writeU32(sid);
                    }
                }
            }
        }
        traceEntryOffset("after lvpath");

        // Handler IR entries for statement slots
        // For each stmt slot, write u8 flag (1 = handler IR follows, 0 = no handler IR)
        // If handler IR is present, serialize the IR function inline
        for (int i = 0; i < func.num_stmts; ++i) {
            const QoreIRFunction* handler_ir = (i < static_cast<int>(func.handler_irs.size()))
                ? func.handler_irs[i] : nullptr;
            if (handler_ir) {
                writer.writeU8(1);
                // Write a size placeholder — we'll patch it after serializing the IR
                uint32_t size_pos = writer.position();
                writer.writeU32(0);  // placeholder

                // Use a writeExpr callback that classifies and writes expressions
                // using the parent function's slot info for variable resolution
                const auto& parent_locals = func.slot_ids.locals;
                const auto& parent_globals = func.slot_ids.globals;
                auto writeExpr = [&parent_locals, &parent_globals, func_const_reverse_map](
                        QoreAOTBinaryWriter& w, const QoreValue& expr) -> bool {
                    return classifyAndWriteExpr(w, expr, parent_locals, parent_globals,
                        func_const_reverse_map);
                };
                qoreAOTClearExprSerializationError();
                bool handler_ok = serializeIRFunction(writer, *handler_ir, writeExpr);
                std::string expr_error;
                bool expr_error_set = qoreAOTTakeExprSerializationError(expr_error);
                if (!handler_ok || expr_error_set) {
                    error = "failed to serialize handler IR for function '" + func.name
                        + "' stmt slot " + std::to_string(i);
                    if (expr_error_set) {
                        error += ": ";
                        error += expr_error;
                    }
                    return false;
                }
                // Patch the size field
                uint32_t end_pos = writer.position();
                writer.patchU32(size_pos, end_pos - size_pos - 4);
            } else {
                writer.writeU8(0);
            }
        }
        traceEntryOffset("after stmt-handlers");

        // Location table entries (AOT runtime_loc tracking)
        if (!writeLocTableCount(func.aot_locs.size(), "location table")) {
            return false;
        }
        traceEntryOffset("after loc count");
        for (auto& loc : func.aot_locs) {
            writer.writeU16(static_cast<uint16_t>(loc.start_line));
            writer.writeU16(static_cast<uint16_t>(loc.end_line));
            writer.writeStringRef(loc.file.c_str());
        }
        traceEntryOffset("after loc table");

        // Metadata-only statement location entries for source-stripped
        // ProgramControl::findStatementId() support.
        if (!writeLocTableCount(func.aot_stmt_locs.size(), "statement location table")) {
            return false;
        }
        traceEntryOffset("after stmt-loc count");
        for (auto& loc : func.aot_stmt_locs) {
            writer.writeU16(static_cast<uint16_t>(loc.start_line));
            writer.writeU16(static_cast<uint16_t>(loc.end_line));
            writer.writeI64(loc.offset);
            writer.writeStringRef(loc.file.c_str());
            writer.writeStringRef(loc.source.c_str());
        }
        traceEntryOffset("after stmt-loc table");

        // Full function IR for source-stripped debug execution.  This is not
        // source fallback: it is the same lowered IR used for AOT codegen,
        // interpreted only when DebugProgram attaches to an AOT-only variant.
        if (func.debug_ir) {
            writer.writeU8(1);
            traceEntryOffset("after debug-ir flag");
            uint32_t size_pos = writer.position();
            writer.writeU32(0);
            const auto& parent_locals = func.slot_ids.locals;
            const auto& parent_globals = func.slot_ids.globals;
            auto writeExpr = [&parent_locals, &parent_globals, func_const_reverse_map](
                    QoreAOTBinaryWriter& w, const QoreValue& expr) -> bool {
                return classifyAndWriteExpr(w, expr, parent_locals, parent_globals,
                    func_const_reverse_map);
            };
            qoreAOTClearExprSerializationError();
            bool debug_ir_ok = serializeIRFunction(writer, *func.debug_ir, writeExpr);
            std::string expr_error;
            bool expr_error_set = qoreAOTTakeExprSerializationError(expr_error);
            if (!debug_ir_ok || expr_error_set) {
                error = "failed to serialize debug IR for function '" + func.name + "'";
                if (expr_error_set) {
                    error += ": ";
                    error += expr_error;
                } else {
                    error += ": serializeIRFunction returned false without a nested expression diagnostic";
                }
                error += "; no fallback marker was emitted";
                return false;
            }
            uint32_t end_pos = writer.position();
            writer.patchU32(size_pos, end_pos - size_pos - 4);
            traceEntryOffset("after debug-ir payload");
        } else {
            writer.writeU8(0);
            traceEntryOffset("after debug-ir flag");
        }

        // Patch the entry size field
        uint32_t entry_end_pos = writer.position();
        writer.patchU32(entry_size_pos, entry_end_pos - entry_size_pos - 4);
        traceEntryOffset("entry end");
    }

    writer.endSection(sec_idx);
    return serializeCallRelocations(writer, funcs, error);
}

// ---- Init Functions Section ----

//! Topologically sort init functions by their dependency graph.
//! Returns indices in execution order. Detects and reports circular dependencies.
static std::vector<size_t> topologicalSortInitFuncs(
        const std::vector<AOTCompiledInitFunc>& init_funcs) {
    size_t n = init_funcs.size();

    // Build name → index map
    std::unordered_map<std::string, size_t> name_to_idx;
    for (size_t i = 0; i < n; ++i) {
        name_to_idx[init_funcs[i].name] = i;
    }

    // Build adjacency list and in-degree count
    std::vector<std::vector<size_t>> dependents(n);  // dependents[i] = list of funcs that depend on i
    std::vector<int> in_degree(n, 0);

    for (size_t i = 0; i < n; ++i) {
        for (auto& dep_name : init_funcs[i].deps) {
            auto it = name_to_idx.find(dep_name);
            if (it != name_to_idx.end()) {
                dependents[it->second].push_back(i);
                ++in_degree[i];
            }
        }
    }

    // Kahn's algorithm
    std::vector<size_t> order;
    order.reserve(n);
    std::deque<size_t> queue;
    for (size_t i = 0; i < n; ++i) {
        if (in_degree[i] == 0) {
            queue.push_back(i);
        }
    }

    while (!queue.empty()) {
        size_t idx = queue.front();
        queue.pop_front();
        order.push_back(idx);
        for (size_t dep_idx : dependents[idx]) {
            if (--in_degree[dep_idx] == 0) {
                queue.push_back(dep_idx);
            }
        }
    }

    if (order.size() < n) {
        // Circular dependency detected — report and include remaining in original order
        printd(0, "AOT WARNING: circular dependency detected among %d init functions\n",
            (int)(n - order.size()));
        std::unordered_set<size_t> added(order.begin(), order.end());
        for (size_t i = 0; i < n; ++i) {
            if (added.find(i) == added.end()) {
                printd(0, "AOT WARNING: circular dependency involves '%s'\n",
                    init_funcs[i].name.c_str());
                order.push_back(i);
            }
        }
    }

    return order;
}

void serializeInitFuncs(QoreAOTBinaryWriter& writer,
        const std::vector<AOTCompiledInitFunc>& init_funcs) {
    // Topologically sort to ensure dependencies are initialized first
    std::vector<size_t> order = topologicalSortInitFuncs(init_funcs);

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::INIT_FUNCS);

    writer.writeU32(static_cast<uint32_t>(init_funcs.size()));
    for (size_t idx : order) {
        auto& cif = init_funcs[idx];
        writer.writeStringRef(cif.name.c_str());
        writer.writeU8(static_cast<uint8_t>(cif.target_type));
        writer.writeStringRef(cif.ns_path.c_str());
        writer.writeStringRef(cif.item_name.c_str());
    }

    writer.endSection(sec_idx);
}

bool readInitFuncs(const uint8_t* data, uint32_t size,
        std::vector<AOTInitFuncDescriptor>& init_funcs, std::string& error) {
    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }
    return readInitFuncs(reader, init_funcs, error);
}

bool readInitFuncs(const QoreAOTBinaryReader& reader,
        std::vector<AOTInitFuncDescriptor>& init_funcs, std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::INIT_FUNCS);
    if (!sec) {
        // No init funcs section — this is OK
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid INIT_FUNCS section data";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    init_funcs.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        AOTInitFuncDescriptor desc;
        const char* name = reader.readStringRef(ptr);
        if (!name) {
            error = "invalid init func name at index " + std::to_string(i);
            return false;
        }
        desc.name = name;
        desc.target_type = static_cast<AOTCompiledInitFunc::TargetType>(
            QoreAOTBinaryReader::readU8(ptr));
        const char* ns_path = reader.readStringRef(ptr);
        if (!ns_path) {
            error = "invalid init func ns_path at index " + std::to_string(i);
            return false;
        }
        desc.ns_path = ns_path;
        const char* item_name = reader.readStringRef(ptr);
        if (!item_name) {
            error = "invalid init func item_name at index " + std::to_string(i);
            return false;
        }
        desc.item_name = item_name;
        init_funcs.push_back(std::move(desc));
    }

    return true;
}

// ---- Embedded Source Section (legacy fallback-function metadata) ----

void serializeFallbackSources(QoreAOTBinaryWriter& writer,
        const std::vector<AOTCompiledFuncWithSlots>& funcs,
        const char* source_text, int source_len) {
    // Collect legacy function names that would need source fallback. Current
    // compiler call sites reject such functions before this writer is called,
    // so this list should be empty for newly generated AOT objects.
    std::vector<const AOTCompiledFuncWithSlots*> fallback_funcs;
    for (auto& func : funcs) {
        if (func.slot_ids.has_unsupported_exprs) {
            fallback_funcs.push_back(&func);
            continue;
        }
        if (func.num_stmts > 0) {
            bool all_have_ir = static_cast<int>(func.handler_irs.size()) == func.num_stmts
                && std::all_of(func.handler_irs.begin(), func.handler_irs.end(),
                    [](const QoreIRFunction* hir) { return hir != nullptr; });
            if (!all_have_ir) {
                fallback_funcs.push_back(&func);
                continue;
            }
        }
        // NOTE: constructor/destructor/copy methods no longer need blanket source
        // fallback — BCA data is serialized in the METHODS section (v2 format)
    }

    // Always write the FUNC_SOURCES section when called; current compiler
    // call sites only do this for explicit --include-source.
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::FUNC_SOURCES);

    // Store the full source text for explicit metadata embedding.
    writer.writeStringRef(source_text, static_cast<size_t>(source_len));

    // Write the legacy fallback function list. Deserialization rejects
    // non-empty lists because source fallback is no longer supported.
    writer.writeU32(static_cast<uint32_t>(fallback_funcs.size()));
    for (auto* func : fallback_funcs) {
        writer.writeStringRef(func->name.c_str());
    }

    writer.endSection(sec_idx);
}

// ---- IR Function Serialization (Phase 5) ----

#include "qore/intern/Variable.h"

//! Determine the instruction group for serialization using dynamic_cast
static QoreIRInstGroup classifyInstruction(const QoreIRInstruction* inst, std::string* error = nullptr) {
    // Check specific subclasses from most to least derived to avoid false matches.
    // Order matters because some subclasses derive from others (e.g. InvokeSimError from Throw).
    if (dynamic_cast<const QoreIRConstInstruction*>(inst)) {
        return QoreIRInstGroup::Const;
    }
    if (dynamic_cast<const QoreIRBranchIfInstruction*>(inst)) {
        return QoreIRInstGroup::BranchIf;
    }
    if (dynamic_cast<const QoreIRBranchInstruction*>(inst)) {
        return QoreIRInstGroup::Branch;
    }
    if (dynamic_cast<const QoreIRSwitchIntInstruction*>(inst)) {
        return QoreIRInstGroup::SwitchInt;
    }
    if (dynamic_cast<const QoreIRSwitchStringInstruction*>(inst)) {
        return QoreIRInstGroup::SwitchString;
    }
    if (dynamic_cast<const QoreIRPhiInstruction*>(inst)) {
        return QoreIRInstGroup::Phi;
    }
    if (dynamic_cast<const QoreIRGuardInstruction*>(inst)) {
        return QoreIRInstGroup::Guard;
    }
    if (dynamic_cast<const QoreIRReturnInstruction*>(inst)) {
        return QoreIRInstGroup::Return;
    }
    if (dynamic_cast<const QoreIRThrowInstruction*>(inst)) {
        return QoreIRInstGroup::Throw;
    }
    if (dynamic_cast<const QoreIRAddAssignLocalIntInstruction*>(inst)) {
        return QoreIRInstGroup::FusedAddLocal;
    }
    if (dynamic_cast<const QoreIRIncrementLocalIntInstruction*>(inst)) {
        return QoreIRInstGroup::FusedIncLocal;
    }
    if (dynamic_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst)) {
        return QoreIRInstGroup::FusedBrLtLocal;
    }
    if (dynamic_cast<const QoreIRLocalInstruction*>(inst)) {
        return QoreIRInstGroup::Local;
    }
    if (dynamic_cast<const QoreIRVarInstruction*>(inst)) {
        return QoreIRInstGroup::Var;
    }
    if (dynamic_cast<const QoreIRImplicitArgInstruction*>(inst)) {
        return QoreIRInstGroup::ImplicitArg;
    }
    if (dynamic_cast<const QoreIRHashKeyStoreInstruction*>(inst)) {
        return QoreIRInstGroup::HashKeyStore;
    }
    if (dynamic_cast<const QoreIRHashKeyStoreDynamicInstruction*>(inst)) {
        return QoreIRInstGroup::HashKeyStoreDynamic;
    }
    if (dynamic_cast<const QoreIRLValuePathInstruction*>(inst)) {
        return QoreIRInstGroup::LValuePath;
    }
    if (dynamic_cast<const QoreIRHashKeyAccessInstruction*>(inst)) {
        return QoreIRInstGroup::HashKeyAccess;
    }
    if (dynamic_cast<const QoreIRListIndexStoreInstruction*>(inst)) {
        return QoreIRInstGroup::ListIndexStore;
    }
    if (dynamic_cast<const QoreIRMapHashKeyInstruction*>(inst)) {
        return QoreIRInstGroup::MapHashKey;
    }
    if (dynamic_cast<const QoreIRSelfMemberInstruction*>(inst)) {
        return QoreIRInstGroup::SelfMember;
    }
    if (dynamic_cast<const QoreIRStaticVarInstruction*>(inst)) {
        return QoreIRInstGroup::StaticVar;
    }
    if (dynamic_cast<const QoreIRNewObjectInstruction*>(inst)) {
        return QoreIRInstGroup::NewObject;
    }
    if (dynamic_cast<const QoreIRLoadConstantInstruction*>(inst)) {
        return QoreIRInstGroup::LoadConst;
    }
    if (dynamic_cast<const QoreIRCreateClosureInstruction*>(inst)) {
        return QoreIRInstGroup::CreateClosure;
    }
    if (dynamic_cast<const QoreIRCreateCallRefInstruction*>(inst)) {
        return QoreIRInstGroup::CreateCallRef;
    }
    if (dynamic_cast<const QoreIRCreateMethodRefInstruction*>(inst)) {
        return QoreIRInstGroup::CreateMethodRef;
    }
    if (dynamic_cast<const QoreIRCreateParseRefInstruction*>(inst)) {
        return QoreIRInstGroup::CreateParseRef;
    }
    if (dynamic_cast<const QoreIRNewHashDeclInstruction*>(inst)) {
        return QoreIRInstGroup::NewHashDecl;
    }
    if (dynamic_cast<const QoreIRNewComplexHashInstruction*>(inst)) {
        return QoreIRInstGroup::NewComplexHash;
    }
    if (dynamic_cast<const QoreIRNewComplexListInstruction*>(inst)) {
        return QoreIRInstGroup::NewComplexList;
    }
    if (dynamic_cast<const QoreIRNewComplexBufferInstruction*>(inst)) {
        return QoreIRInstGroup::NewComplexBuffer;
    }
    if (dynamic_cast<const QoreIRPluginInstruction*>(inst)) {
        return QoreIRInstGroup::Plugin;
    }
    if (dynamic_cast<const QoreIRVrnConstructInstruction*>(inst)) {
        return QoreIRInstGroup::VrnConstruct;
    }
    if (dynamic_cast<const QoreIRNewHashDeclFromHashInstruction*>(inst)) {
        return QoreIRInstGroup::NewHashDeclFromHash;
    }
    if (dynamic_cast<const QoreIRLValueInstruction*>(inst)) {
        return QoreIRInstGroup::LValue;
    }
    // Check specific call instruction types before generic expr
    if (dynamic_cast<const QoreIRCallDirectInstruction*>(inst)) {
        return QoreIRInstGroup::CallDirect;
    }
    if (dynamic_cast<const QoreIRInvokeMethodDirectInstruction*>(inst)) {
        return QoreIRInstGroup::InvokeMethodDirect;
    }
    if (dynamic_cast<const QoreIRCallMethodDirectInstruction*>(inst)) {
        return QoreIRInstGroup::CallMethodDirect;
    }
    if (dynamic_cast<const QoreIRCallStaticDirectInstruction*>(inst)) {
        return QoreIRInstGroup::CallStaticDirect;
    }
    if (dynamic_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst)) {
        return QoreIRInstGroup::InvokeDotEvalMethodDirect;
    }
    if (dynamic_cast<const QoreIRDotEvalMethodDirectInstruction*>(inst)) {
        return QoreIRInstGroup::DotEvalMethodDirect;
    }
    if (dynamic_cast<const QoreIRInvokeInstruction*>(inst)) {
        return QoreIRInstGroup::Invoke;
    }
    if (dynamic_cast<const QoreIRIteratorCreateInstruction*>(inst)) {
        return QoreIRInstGroup::IteratorCreate;
    }
    if (dynamic_cast<const QoreIRIteratorNextInstruction*>(inst)) {
        return QoreIRInstGroup::IteratorNext;
    }
    if (dynamic_cast<const QoreIROnBlockExitInstruction*>(inst)) {
        return QoreIRInstGroup::OnBlockExit;
    }
    if (dynamic_cast<const QoreIRScopeEnterInstruction*>(inst)) {
        return QoreIRInstGroup::ScopeEnter;
    }
    if (dynamic_cast<const QoreIRScopeExitInstruction*>(inst)) {
        return QoreIRInstGroup::ScopeExit;
    }
    if (dynamic_cast<const QoreIRLandingPadInstruction*>(inst)) {
        return QoreIRInstGroup::LandingPad;
    }
    if (dynamic_cast<const QoreIRSwitchRegexMatchInstruction*>(inst)) {
        return QoreIRInstGroup::SwitchRegexMatch;
    }
    if (dynamic_cast<const QoreIRRefForeachInitInstruction*>(inst)) {
        return QoreIRInstGroup::RefForeachInit;
    }
    if (dynamic_cast<const QoreIRMakeHashConstKeysInstruction*>(inst)) {
        return QoreIRInstGroup::MakeHashConstKeys;
    }
    if (dynamic_cast<const QoreIRMakeListInstruction*>(inst)) {
        return QoreIRInstGroup::MakeList;
    }
    if (dynamic_cast<const QoreIRMakeHashInstruction*>(inst)) {
        return QoreIRInstGroup::MakeHash;
    }
    if (dynamic_cast<const QoreIRSwitchCaseMatchInstruction*>(inst)) {
        return QoreIRInstGroup::SwitchCaseMatch;
    }
    if (dynamic_cast<const QoreIRContextInstruction*>(inst)) {
        return QoreIRInstGroup::Context;
    }
    if (dynamic_cast<const QoreIRBackquoteInstruction*>(inst)) {
        return QoreIRInstGroup::Backquote;
    }
    if (dynamic_cast<const QoreIRFindInstruction*>(inst)) {
        return QoreIRInstGroup::Find;
    }
    if (dynamic_cast<const QoreIRBackgroundInstruction*>(inst)) {
        return QoreIRInstGroup::Background;
    }
    if (dynamic_cast<const QoreIRContextRefInstruction*>(inst)) {
        return QoreIRInstGroup::ContextRef;
    }
    if (dynamic_cast<const QoreIRSummarizeInstruction*>(inst)) {
        return QoreIRInstGroup::Summarize;
    }
    if (dynamic_cast<const QoreIRListIndexAccessInstruction*>(inst)) {
        return QoreIRInstGroup::ListIndexAccess;
    }
    if (auto* expr_inst = dynamic_cast<const QoreIRExprInstruction*>(inst)) {
        if (expr_inst->opcode == QoreIROpcode::CallClosureDirect) {
            return QoreIRInstGroup::CallClosureDirect;
        }
    }
    if (dynamic_cast<const QoreIRExprInstruction*>(inst)) {
        return QoreIRInstGroup::Expr;
    }
    if (typeid(*inst) == typeid(QoreIRInstruction)) {
        return inst->element_type ? QoreIRInstGroup::TypedBase : QoreIRInstGroup::Base;
    }

    if (error) {
        const OpcodeInfo* oi = getOpcodeInfo(static_cast<uint16_t>(inst->opcode));
        *error = "cannot serialize IR instruction opcode ";
        *error += oi && oi->name ? oi->name : "<unknown>";
        *error += " (";
        *error += std::to_string(static_cast<uint16_t>(inst->opcode));
        *error += "): dynamic instruction type '";
        *error += typeid(*inst).name();
        *error += "' has no AOT instruction group mapping; add a classifyInstruction() mapping, "
            "AOT_INST_GROUP_REGISTRY entry, and read/write handlers before this instruction can be serialized";
    }
    return QoreIRInstGroup::Unsupported;
}

//! Get type path string for a LocalVar, handling nullptr typeInfo
static std::string getLocalTypePathString(const LocalVar* lv) {
    const QoreTypeInfo* ti = lv->getTypeInfo();
    return getTypePath(ti, lv->isNoNarrowing());
}

const char* getLocalTypePath(const LocalVar* lv) {
    thread_local std::string path;
    path = getLocalTypePathString(lv);
    return path.c_str();
}

//! Serialize a single IR instruction
static bool serializeIRInstruction(QoreAOTBinaryWriter& writer, const QoreIRInstruction* inst,
        const std::unordered_map<const QoreIRBasicBlock*, uint16_t>& block_idx,
        const AOTExprWriteFunc& writeExpr) {
    std::string classify_error;
    QoreIRInstGroup group = classifyInstruction(inst, &classify_error);
    if (!classify_error.empty()) {
        qoreAOTSetExprSerializationError(std::move(classify_error));
        return false;
    }

    // Write opcode
    writer.writeU16(static_cast<uint16_t>(inst->opcode));

    // Classify and write group tag
    writer.writeU8(static_cast<uint8_t>(group));

    // Write base fields: result, operands, exception_target
    writer.writeU32(inst->result.id);
    if ((writer.feature_flags & QORE_AOT_FEAT_WIDE_IR_OPERANDS) != 0) {
        if (inst->operands.size() > UINT16_MAX) {
            const OpcodeInfo* oi = getOpcodeInfo(static_cast<uint16_t>(inst->opcode));
            std::string diag = "cannot serialize IR instruction opcode ";
            diag += oi && oi->name ? oi->name : "<unknown>";
            diag += " (";
            diag += std::to_string(static_cast<uint16_t>(inst->opcode));
            diag += "): operand count ";
            diag += std::to_string(inst->operands.size());
            diag += " exceeds the u16 AOT debug IR wire limit";
            qoreAOTSetExprSerializationError(std::move(diag));
            return false;
        }
        writer.writeU16(static_cast<uint16_t>(inst->operands.size()));
    } else {
        if (inst->operands.size() > UINT8_MAX) {
            const OpcodeInfo* oi = getOpcodeInfo(static_cast<uint16_t>(inst->opcode));
            std::string diag = "cannot serialize IR instruction opcode ";
            diag += oi && oi->name ? oi->name : "<unknown>";
            diag += " (";
            diag += std::to_string(static_cast<uint16_t>(inst->opcode));
            diag += "): operand count ";
            diag += std::to_string(inst->operands.size());
            diag += " exceeds legacy u8 AOT debug IR wire limit; "
                "QORE_AOT_FEAT_WIDE_IR_OPERANDS is required";
            qoreAOTSetExprSerializationError(std::move(diag));
            return false;
        }
        writer.writeU8(static_cast<uint8_t>(inst->operands.size()));
    }
    for (auto& op : inst->operands) {
        writer.writeU32(op.id);
    }
    // Exception target block index (0xFFFF = none)
    // Write base exception target
    if (inst->exception_target) {
        auto it = block_idx.find(inst->exception_target);
        writer.writeU16(it != block_idx.end() ? it->second : 0xFFFF);
    } else {
        writer.writeU16(0xFFFF);
    }

    // Write group-specific fields via registry dispatch
    const auto* ginfo = getAOTInstGroupInfo(static_cast<uint8_t>(group));
    if (!ginfo || !ginfo->is_serializable) {
        const OpcodeInfo* oi = getOpcodeInfo(static_cast<uint16_t>(inst->opcode));
        std::string diag = "cannot serialize IR instruction opcode ";
        diag += oi && oi->name ? oi->name : "<unknown>";
        diag += " (";
        diag += std::to_string(static_cast<uint16_t>(inst->opcode));
        diag += "): classified AOT instruction group ";
        diag += std::to_string(static_cast<uint8_t>(group));
        if (!ginfo) {
            diag += " is not registered";
        } else {
            diag += " ('";
            diag += ginfo->name ? ginfo->name : "<unnamed>";
            diag += "') is not serializable";
        }
        diag += "; IR lowering/codegen must not emit non-roundtrippable AOT debug IR";
        qoreAOTSetExprSerializationError(std::move(diag));
        return false;
    }
    if (ginfo->write_fn) {
        AOTInstWriteCtx wctx{writer, inst, block_idx, writeExpr};
        if (!ginfo->write_fn(wctx)) {
            const OpcodeInfo* oi = getOpcodeInfo(static_cast<uint16_t>(inst->opcode));
            std::string diag = "failed to serialize IR instruction opcode ";
            diag += oi && oi->name ? oi->name : "<unknown>";
            diag += " (";
            diag += std::to_string(static_cast<uint16_t>(inst->opcode));
            diag += ") in AOT instruction group ";
            diag += ginfo->name ? ginfo->name : "<unnamed>";
            diag += " (";
            diag += std::to_string(static_cast<uint8_t>(group));
            diag += ")";
            qoreAOTSetExprSerializationError(std::move(diag));
            return false;
        }
    }

    // Write source location for runtime exception stack traces (AOT location table)
    if (inst->loc && inst->loc->start_line > 0) {
        writer.writeU16(static_cast<uint16_t>(inst->loc->start_line));
        writer.writeU16(static_cast<uint16_t>(inst->loc->end_line));
        writer.writeStringRef(inst->loc->getFile() ? inst->loc->getFile() : "");
    } else {
        writer.writeU16(0);  // start_line=0 signals "no location"
        writer.writeU16(0);
        writer.writeStringRef("");
    }

    return true;
}

bool serializeIRFunction(QoreAOTBinaryWriter& writer, const QoreIRFunction& func,
        const AOTExprWriteFunc& writeExpr) {
    std::string registry_error;
    if (!qore_aot_validate_inst_group_registry(registry_error)) {
        qoreAOTSetExprSerializationError("AOT instruction group registry validation failed: " + registry_error);
        return false;
    }

    // 1. Function header
    writer.writeStringRef(func.name.c_str());
    writer.writeU32(func.max_value_id);
    writer.writeU32(func.max_local_slot_id);
    writer.writeU32(func.num_guards);
    writeTypePathRef(writer, func.return_type_info);
    // Phase C: Serialize parent_slot_count for handler IR functions
    writer.writeU32(func.parent_slot_count);
    writer.writeU16(static_cast<uint16_t>(func.blocks.size()));
    writer.writeU16(static_cast<uint16_t>(func.local_var_slots.size()));
    writer.writeU16(static_cast<uint16_t>(func.all_body_locals.size()));

    // 2. Local variable slot table
    std::unordered_map<const LocalVar*, uint32_t> body_local_ordinals;
    for (uint32_t i = 0; i < func.all_body_locals.size(); ++i) {
        body_local_ordinals[func.all_body_locals[i]] = i;
    }

    // Sort by slot_id for deterministic serialization
    std::vector<std::pair<const LocalVar*, uint32_t>> sorted_slots(
        func.local_var_slots.begin(), func.local_var_slots.end());
    std::sort(sorted_slots.begin(), sorted_slots.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    for (auto& [lv, slot_id] : sorted_slots) {
        writer.writeStringRef(lv->getName());
        std::string type_path = getLocalTypePathString(lv);
        writer.writeStringRef(type_path.c_str());
        writer.writeU32(slot_id);
        if ((writer.feature_flags & QORE_AOT_FEAT_LOCAL_DECL_ORDINAL) != 0) {
            auto ordinal_it = body_local_ordinals.find(lv);
            writer.writeU32(ordinal_it != body_local_ordinals.end() ? ordinal_it->second : UINT32_MAX);
        }
    }

    // 3. Body locals
    for (auto* lv : func.all_body_locals) {
        writer.writeStringRef(lv->getName());
        std::string type_path = getLocalTypePathString(lv);
        writer.writeStringRef(type_path.c_str());
        auto slot_it = func.local_var_slots.find(lv);
        writer.writeU32(slot_it != func.local_var_slots.end() ? slot_it->second : UINT32_MAX);
    }

    // 4. Build block index map for block reference serialization
    std::unordered_map<const QoreIRBasicBlock*, uint16_t> block_idx;
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        block_idx[func.blocks[i].get()] = static_cast<uint16_t>(i);
    }

    // 5. Serialize blocks
    for (auto& block : func.blocks) {
        writer.writeStringRef(block->name.c_str());
        writer.writeU8(block->is_loop_header ? 1 : 0);
        writer.writeU16(static_cast<uint16_t>(block->instructions.size()));

        for (size_t inst_idx = 0; inst_idx < block->instructions.size(); ++inst_idx) {
            auto& inst_ptr = block->instructions[inst_idx];
            uint32_t inst_start = writer.position();
            QoreIRInstGroup group = classifyInstruction(inst_ptr.get());
            if (const char* trace = getenv("QORE_AOT_TRACE_IR_SERIALIZE")) {
                bool match = !*trace || (func.name.find(trace) != std::string::npos);
                if (match) {
                    const OpcodeInfo* oi = getOpcodeInfo(static_cast<uint16_t>(inst_ptr->opcode));
                    const auto* gi = getAOTInstGroupInfo(static_cast<uint8_t>(group));
                    fprintf(stderr,
                        "[aot-ir-ser] func=%s block=%s inst=%zu pos=%u opcode=%s(%u) group=%s(%u)\n",
                        func.name.c_str(), block->name.c_str(), inst_idx, inst_start,
                        oi && oi->name ? oi->name : "<unknown>",
                        static_cast<uint16_t>(inst_ptr->opcode),
                        gi && gi->name ? gi->name : "<unknown>",
                        static_cast<uint8_t>(group));
                }
            }
            if (!serializeIRInstruction(writer, inst_ptr.get(), block_idx, writeExpr)) {
                return false;
            }
            if (const char* trace = getenv("QORE_AOT_TRACE_IR_SERIALIZE")) {
                bool match = !*trace || (func.name.find(trace) != std::string::npos);
                if (match) {
                    fprintf(stderr,
                        "[aot-ir-ser] func=%s block=%s inst=%zu bytes=%u next_pos=%u\n",
                        func.name.c_str(), block->name.c_str(), inst_idx,
                        writer.position() - inst_start, writer.position());
                }
            }
        }
    }

    return true;
}

// ---- Namespace Deserialization (Phase 4) ----

#include "qore/intern/Function.h"
#include "qore/intern/FunctionList.h"
#include "qore/intern/Variable.h"

// Phase 4 slice 10: split the existing all-in-one deserializeIntoProgram
// into two phases so the new QoreAOTBinaryMultiDeserializer can run
// phase 1 (shell creation) for every blob before running phase 2
// (cross-blob resolution) once.  Single-blob callers keep the same
// entry point (deserializeIntoProgram) — it just chains both phases.

// Phase 1: shells only — namespaces, class declarations, hashdecl /
// enum / typedef stubs.  NO resolution passes run.  After this,
// pgm's namespace tree has every declared type present as a shell;
// cross-blob base-class / member-type lookups (via pgm->findClass)
// will succeed in subsequent phase-2 runs regardless of load order.
static bool checkAOTFeatureCompatibility(const QoreAOTBinaryReader& reader, std::string& error) {
    uint64_t unsupported = reader.getHeader().feature_flags & ~QORE_AOT_SUPPORTED_FEATURES;
    if (!unsupported) {
        return true;
    }

    char unsupported_buf[32];
    snprintf(unsupported_buf, sizeof(unsupported_buf), "0x%016llx",
        static_cast<unsigned long long>(unsupported));
    char supported_buf[32];
    snprintf(supported_buf, sizeof(supported_buf), "0x%016llx",
        static_cast<unsigned long long>(QORE_AOT_SUPPORTED_FEATURES));

    error = "AOT binary '";
    error += reader.getLabel() ? reader.getLabel() : "<unknown>";
    error += "' requires unsupported feature flags ";
    error += unsupported_buf;
    error += " (runtime supports ";
    error += supported_buf;
    error += "); update Qore or rebuild the AOT binary with a compatible qcc";
    return false;
}

static bool ensurePluginSectionBytes(const uint8_t* ptr, const uint8_t* end, size_t needed,
        const char* section, std::string& error) {
    if (ptr > end || static_cast<size_t>(end - ptr) < needed) {
        error = section;
        error += " section is truncated";
        return false;
    }
    return true;
}

static bool checkAOTPluginCancel(size_t i, ExceptionSink& xsink, const char* operation, std::string& error);

static uint64_t readPluginHash64(const uint8_t*& ptr) {
    uint64_t lo = QoreAOTBinaryReader::readU32(ptr);
    uint64_t hi = QoreAOTBinaryReader::readU32(ptr);
    return lo | (hi << 32);
}

static const QorePluginAOTOperationInfo* findPluginAOTOperation(
        const QorePluginAOTModuleInfo& info, uint16_t local_id, ExceptionSink& xsink, std::string& error) {
    for (size_t i = 0; i < info.operations.size(); ++i) {
        if (checkAOTPluginCancel(i, xsink, "QORD plugin operation metadata lookup", error)) {
            return nullptr;
        }
        const QorePluginAOTOperationInfo& op = info.operations[i];
        if (op.local_id == local_id) {
            return &op;
        }
    }
    return nullptr;
}

static const QorePluginAOTTypeInfo* findPluginAOTType(
        const QorePluginAOTModuleInfo& info, uint16_t local_id, ExceptionSink& xsink, std::string& error) {
    for (size_t i = 0; i < info.types.size(); ++i) {
        if (checkAOTPluginCancel(i, xsink, "QORD plugin type metadata lookup", error)) {
            return nullptr;
        }
        const QorePluginAOTTypeInfo& type = info.types[i];
        if (type.local_type_id == local_id) {
            return &type;
        }
    }
    return nullptr;
}

bool QoreAOTBinaryDeserializer::resolvePluginImports(std::string& error) {
    std::vector<QorePluginAOTModuleInfo> plugin_imports_resolved;
    const QoreAOTSectionHeader* import_sec = reader.findSection(QoreAOTSectionType::PLUGIN_IMPORTS);
    const QoreAOTSectionHeader* helper_sec = reader.findSection(QoreAOTSectionType::PLUGIN_HELPER_REFS);
    const QoreAOTSectionHeader* registry_sec = reader.findSection(QoreAOTSectionType::PLUGIN_TYPE_REGISTRY);
    if (!import_sec) {
        if (helper_sec || registry_sec) {
            error = "QORD-PLUGIN-HELPER-REF-INVALID: plugin helper/type sections require PLUGIN_IMPORTS";
            tracePluginQord("load failed: PLUGIN_HELPER_REFS/PLUGIN_TYPE_REGISTRY present without PLUGIN_IMPORTS");
            return false;
        }
        tracePluginQord("load: no PLUGIN_IMPORTS section present");
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*import_sec);
    if (!ptr) {
        error = "invalid PLUGIN_IMPORTS section data";
        return false;
    }
    const uint8_t* end = ptr + import_sec->size;
    if (!ensurePluginSectionBytes(ptr, end, 4, "PLUGIN_IMPORTS", error)) {
        return false;
    }
    uint32_t import_count = QoreAOTBinaryReader::readU32(ptr);
    tracePluginQord("load: resolving " + std::to_string(import_count) + " plugin import(s)");
    plugin_imports_resolved.reserve(import_count);
    std::vector<std::vector<uint16_t>> required_type_ids;
    std::vector<std::vector<uint16_t>> required_operation_ids;
    required_type_ids.reserve(import_count);
    required_operation_ids.reserve(import_count);

    ExceptionSink xsink;
    for (uint32_t i = 0; i < import_count; ++i) {
        if (checkAOTPluginCancel(i, xsink, "QORD plugin import resolution", error)) {
            return false;
        }
        if (!ensurePluginSectionBytes(ptr, end, 12, "PLUGIN_IMPORTS", error)) {
            return false;
        }
        const char* module_name = reader.readStringRef(ptr);
        const char* plugin_abi_version = reader.readStringRef(ptr);
        const char* operation_set_version = reader.readStringRef(ptr);
        if (!module_name || !*module_name || !plugin_abi_version || !operation_set_version) {
            error = "QORD-PLUGIN-IMPORT-MISSING: invalid plugin import string reference";
            tracePluginQord("load failed: invalid plugin import string reference at index " + std::to_string(i));
            return false;
        }
        tracePluginQord("load: import[" + std::to_string(i) + "] module='" + module_name
            + "' plugin_abi='" + plugin_abi_version + "' operation_set='" + operation_set_version + "'");

        QorePluginAOTModuleInfo info;
        if (qore_plugin_get_aot_module_info(module_name, info, &xsink) || xsink) {
            error = "QORD-PLUGIN-IMPORT-MISSING: plugin module '";
            error += module_name;
            error += "' is not loaded";
            tracePluginQord("load failed: plugin module '" + std::string(module_name) + "' is not loaded");
            return false;
        }
        if (info.plugin_abi_version != plugin_abi_version) {
            error = "QORD-PLUGIN-IMPORT-MISSING: plugin module '";
            error += module_name;
            error += "' ABI version mismatch";
            tracePluginQord("load failed: plugin module '" + std::string(module_name)
                + "' ABI version mismatch");
            return false;
        }
        if (info.operation_set_version != operation_set_version) {
            error = "QORD-PLUGIN-IMPORT-MISSING: plugin module '";
            error += module_name;
            error += "' operation-set version mismatch";
            tracePluginQord("load failed: plugin module '" + std::string(module_name)
                + "' operation-set version mismatch");
            return false;
        }

        if (!ensurePluginSectionBytes(ptr, end, 4, "PLUGIN_IMPORTS", error)) {
            return false;
        }
        uint32_t type_count = QoreAOTBinaryReader::readU32(ptr);
        tracePluginQord("load: import[" + std::to_string(i) + "] requires "
            + std::to_string(type_count) + " type id(s)");
        if (!ensurePluginSectionBytes(ptr, end, static_cast<size_t>(type_count) * 2, "PLUGIN_IMPORTS", error)) {
            return false;
        }
        std::vector<uint16_t> types;
        types.reserve(type_count);
        for (uint32_t n = 0; n < type_count; ++n) {
            if (checkAOTPluginCancel(n, xsink, "QORD plugin required type validation", error)) {
                return false;
            }
            uint16_t id = QoreAOTBinaryReader::readU16(ptr);
            if (!findPluginAOTType(info, id, xsink, error)) {
                if (!error.empty()) {
                    return false;
                }
                error = "QORD-PLUGIN-IMPORT-MISSING: plugin module '";
                error += module_name;
                error += "' does not register required local type id ";
                error += std::to_string(id);
                tracePluginQord("load failed: plugin module '" + std::string(module_name)
                    + "' missing required local type id " + std::to_string(id));
                return false;
            }
            types.push_back(id);
        }
        if (!ensurePluginSectionBytes(ptr, end, 4, "PLUGIN_IMPORTS", error)) {
            return false;
        }
        uint32_t op_count = QoreAOTBinaryReader::readU32(ptr);
        tracePluginQord("load: import[" + std::to_string(i) + "] requires "
            + std::to_string(op_count) + " operation id(s)");
        if (!ensurePluginSectionBytes(ptr, end, static_cast<size_t>(op_count) * 2, "PLUGIN_IMPORTS", error)) {
            return false;
        }
        std::vector<uint16_t> ops;
        ops.reserve(op_count);
        for (uint32_t n = 0; n < op_count; ++n) {
            if (checkAOTPluginCancel(n, xsink, "QORD plugin required operation validation", error)) {
                return false;
            }
            uint16_t id = QoreAOTBinaryReader::readU16(ptr);
            if (!findPluginAOTOperation(info, id, xsink, error)) {
                if (!error.empty()) {
                    return false;
                }
                error = "QORD-PLUGIN-IMPORT-MISSING: plugin module '";
                error += module_name;
                error += "' does not register required local operation id ";
                error += std::to_string(id);
                tracePluginQord("load failed: plugin module '" + std::string(module_name)
                    + "' missing required local operation id " + std::to_string(id));
                return false;
            }
            ops.push_back(id);
        }
        required_type_ids.push_back(std::move(types));
        required_operation_ids.push_back(std::move(ops));
        plugin_imports_resolved.push_back(std::move(info));
    }
    if (ptr != end) {
        error = "PLUGIN_IMPORTS section has trailing bytes";
        return false;
    }

    if (registry_sec) {
        ptr = reader.getSectionData(*registry_sec);
        if (!ptr) {
            error = "invalid PLUGIN_TYPE_REGISTRY section data";
            return false;
        }
        end = ptr + registry_sec->size;
        if (!ensurePluginSectionBytes(ptr, end, 4, "PLUGIN_TYPE_REGISTRY", error)) {
            return false;
        }
        uint32_t module_count = QoreAOTBinaryReader::readU32(ptr);
        tracePluginQord("load: validating PLUGIN_TYPE_REGISTRY for " + std::to_string(module_count)
            + " module(s)");
        for (uint32_t i = 0; i < module_count; ++i) {
            if (checkAOTPluginCancel(i, xsink, "QORD plugin type registry validation", error)) {
                return false;
            }
            if (!ensurePluginSectionBytes(ptr, end, 16, "PLUGIN_TYPE_REGISTRY", error)) {
                return false;
            }
            const char* module_name = reader.readStringRef(ptr);
            const char* plugin_abi_version = reader.readStringRef(ptr);
            const char* operation_set_version = reader.readStringRef(ptr);
            uint32_t type_count = QoreAOTBinaryReader::readU32(ptr);
            const QorePluginAOTModuleInfo* live = nullptr;
            for (size_t n = 0; n < plugin_imports_resolved.size(); ++n) {
                if (checkAOTPluginCancel(n, xsink, "QORD plugin registry import matching", error)) {
                    return false;
                }
                if (plugin_imports_resolved[n].module_name == (module_name ? module_name : "")) {
                    live = &plugin_imports_resolved[n];
                    break;
                }
            }
            if (!live || live->plugin_abi_version != (plugin_abi_version ? plugin_abi_version : "")
                    || live->operation_set_version != (operation_set_version ? operation_set_version : "")) {
                error = "QORD-PLUGIN-IMPORT-MISSING: PLUGIN_TYPE_REGISTRY module metadata does not match imports";
                tracePluginQord("load failed: PLUGIN_TYPE_REGISTRY module metadata mismatch");
                return false;
            }
            tracePluginQord("load: registry module[" + std::to_string(i) + "] module='"
                + (module_name ? module_name : "") + "' types=" + std::to_string(type_count));
            for (uint32_t n = 0; n < type_count; ++n) {
                if (checkAOTPluginCancel(n, xsink, "QORD plugin type metadata validation", error)) {
                    return false;
                }
                if (!ensurePluginSectionBytes(ptr, end, 12, "PLUGIN_TYPE_REGISTRY", error)) {
                    return false;
                }
                uint16_t local_id = QoreAOTBinaryReader::readU16(ptr);
                uint16_t serializer_version = QoreAOTBinaryReader::readU16(ptr);
                const char* type_name = reader.readStringRef(ptr);
                const char* type_path = reader.readStringRef(ptr);
                const QorePluginAOTTypeInfo* live_type = findPluginAOTType(*live, local_id, xsink, error);
                if (!live_type && !error.empty()) {
                    return false;
                }
                if (!live_type || live_type->serializer_format_version != serializer_version
                        || live_type->type_name != (type_name ? type_name : "")
                        || live_type->type_path != (type_path ? type_path : "")) {
                    error = "QORD-PLUGIN-IMPORT-MISSING: plugin type metadata mismatch";
                    tracePluginQord("load failed: plugin type metadata mismatch for module '"
                        + std::string(module_name ? module_name : "") + "' local_type_id="
                        + std::to_string(local_id));
                    return false;
                }
            }
            if (!ensurePluginSectionBytes(ptr, end, 4, "PLUGIN_TYPE_REGISTRY", error)) {
                return false;
            }
            uint32_t op_count = QoreAOTBinaryReader::readU32(ptr);
            tracePluginQord("load: registry module[" + std::to_string(i) + "] module='"
                + (module_name ? module_name : "") + "' operations=" + std::to_string(op_count));
            for (uint32_t n = 0; n < op_count; ++n) {
                if (checkAOTPluginCancel(n, xsink, "QORD plugin operation metadata validation", error)) {
                    return false;
                }
                if (!ensurePluginSectionBytes(ptr, end, 36, "PLUGIN_TYPE_REGISTRY", error)) {
                    return false;
                }
                uint16_t local_id = QoreAOTBinaryReader::readU16(ptr);
                uint8_t canonical_version = QoreAOTBinaryReader::readU8(ptr);
                uint8_t reserved = QoreAOTBinaryReader::readU8(ptr);
                uint64_t signature_hash = readPluginHash64(ptr);
                const char* operation_name = reader.readStringRef(ptr);
                uint8_t arity = QoreAOTBinaryReader::readU8(ptr);
                uint8_t helper_abi = QoreAOTBinaryReader::readU8(ptr);
                uint8_t access = QoreAOTBinaryReader::readU8(ptr);
                uint8_t result_alias = QoreAOTBinaryReader::readU8(ptr);
                ptr += 4; // nullability/reserved bytes; helper refs validate the hash.
                const char* primary_type = reader.readStringRef(ptr);
                const char* secondary_type = reader.readStringRef(ptr);
                const char* return_type = reader.readStringRef(ptr);
                (void)primary_type;
                (void)secondary_type;
                (void)return_type;
                const QorePluginAOTOperationInfo* live_op = findPluginAOTOperation(*live, local_id, xsink, error);
                if (!live_op && !error.empty()) {
                    return false;
                }
                if (reserved || !live_op || live_op->canonical_signature_version != canonical_version
                        || live_op->signature_hash != signature_hash
                        || live_op->operation_name != (operation_name ? operation_name : "")
                        || live_op->signature.arity != arity
                        || static_cast<uint8_t>(live_op->signature.helper_abi) != helper_abi
                        || static_cast<uint8_t>(live_op->signature.access) != access
                        || static_cast<uint8_t>(live_op->signature.result_alias) != result_alias) {
                    error = "QORD-PLUGIN-SIGNATURE-HASH-MISMATCH: plugin operation metadata mismatch";
                    tracePluginQord("load failed: plugin operation metadata mismatch for module '"
                        + std::string(module_name ? module_name : "") + "' local_operation_id="
                        + std::to_string(local_id));
                    return false;
                }
            }
        }
        if (ptr != end) {
            error = "PLUGIN_TYPE_REGISTRY section has trailing bytes";
            return false;
        }
    }

    if (!helper_sec) {
        tracePluginQord("load: no PLUGIN_HELPER_REFS section present");
        return true;
    }
    ptr = reader.getSectionData(*helper_sec);
    if (!ptr) {
        error = "invalid PLUGIN_HELPER_REFS section data";
        return false;
    }
    end = ptr + helper_sec->size;
    if (!ensurePluginSectionBytes(ptr, end, 4, "PLUGIN_HELPER_REFS", error)) {
        return false;
    }
    uint32_t helper_count = QoreAOTBinaryReader::readU32(ptr);
    tracePluginQord("load: validating " + std::to_string(helper_count) + " plugin helper ref(s)");
    for (uint32_t i = 0; i < helper_count; ++i) {
        if (checkAOTPluginCancel(i, xsink, "QORD plugin helper-ref validation", error)) {
            return false;
        }
        if (!ensurePluginSectionBytes(ptr, end, 16, "PLUGIN_HELPER_REFS", error)) {
            return false;
        }
        uint16_t slot_idx = QoreAOTBinaryReader::readU16(ptr);
        uint16_t import_idx = QoreAOTBinaryReader::readU16(ptr);
        uint16_t op_local_id = QoreAOTBinaryReader::readU16(ptr);
        uint8_t canonical_version = QoreAOTBinaryReader::readU8(ptr);
        uint8_t reserved = QoreAOTBinaryReader::readU8(ptr);
        uint64_t signature_hash = readPluginHash64(ptr);
        (void)slot_idx;
        tracePluginQord("load: helper_ref[" + std::to_string(i) + "] slot="
            + std::to_string(slot_idx) + " import_idx=" + std::to_string(import_idx)
            + " op_local_id=" + std::to_string(op_local_id) + " canonical_signature_version="
            + std::to_string(canonical_version));
        if (reserved) {
            error = "QORD-PLUGIN-RESERVED-NONZERO: PLUGIN_HELPER_REFS reserved byte is non-zero";
            tracePluginQord("load failed: PLUGIN_HELPER_REFS reserved byte is non-zero");
            return false;
        }
        if (import_idx >= plugin_imports_resolved.size()) {
            error = "QORD-PLUGIN-HELPER-REF-INVALID: import_idx is out of range";
            tracePluginQord("load failed: helper ref import_idx is out of range");
            return false;
        }
        if (canonical_version != QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1) {
            error = "QORD-PLUGIN-SIGNATURE-VERSION-UNSUPPORTED: unsupported canonical signature version";
            tracePluginQord("load failed: unsupported canonical signature version "
                + std::to_string(canonical_version));
            return false;
        }
        const QorePluginAOTOperationInfo* op = findPluginAOTOperation(plugin_imports_resolved[import_idx],
            op_local_id, xsink, error);
        if (!op && !error.empty()) {
            return false;
        }
        if (!op) {
            error = "QORD-PLUGIN-HELPER-REF-INVALID: op_local_id is not registered";
            tracePluginQord("load failed: helper ref operation local id is not registered");
            return false;
        }
        if (op->signature_hash != signature_hash) {
            error = "QORD-PLUGIN-SIGNATURE-HASH-MISMATCH: helper ref signature hash does not match live registry";
            tracePluginQord("load failed: helper ref signature hash mismatch for op_local_id="
                + std::to_string(op_local_id));
            return false;
        }
    }
    if (ptr != end) {
        error = "PLUGIN_HELPER_REFS section has trailing bytes";
        return false;
    }
    tracePluginQord("load: plugin QORD imports resolved successfully");
    return true;
}

bool QoreAOTBinaryDeserializer::openAndDeserializeShells(QoreProgram* in_pgm,
        const uint8_t* data, uint32_t size, std::string& error) {
    pgm = in_pgm;

    // Open and validate the binary blob
    if (!reader.open(data, size, error)) {
        return false;
    }
    return deserializeShellsFromOpenReader(error);
}

bool QoreAOTBinaryDeserializer::openAndDeserializeShells(QoreProgram* in_pgm,
        QoreAOTBinaryReader&& open_reader, std::string& error) {
    pgm = in_pgm;
    reader = std::move(open_reader);
    return deserializeShellsFromOpenReader(error);
}

bool QoreAOTBinaryDeserializer::deserializeShellsFromOpenReader(std::string& error) {
    if (!checkAOTFeatureCompatibility(reader, error)) {
        return false;
    }

    // Decide whether to use the per-blob TYPE_TABLE fast path.  The
    // writer advertises QORE_AOT_FEAT_TYPE_TABLE unconditionally for
    // binaries produced by the current compiler; older blobs don't
    // have the bit and fall back to inline-string type paths.
    uses_type_table = (reader.getHeader().feature_flags
        & QORE_AOT_FEAT_TYPE_TABLE) != 0;

    if (!resolvePluginImports(error)) {
        return false;
    }

    // Create type resolver for this program
    type_resolver = new QoreAOTTypeResolver(pgm);

    // Deserialize shells in dependency order (no resolution passes here).
    if (!deserializeNamespaces(error)) {
        return false;
    }
    // Enums first: enum member VALUES are primitive (int / string) so they
    // never reference classes or hashdecls.  By registering them before
    // classes and hashdecls we let NESTED VT_ENUM references inside class
    // instance-member defaults and hashdecl member defaults resolve
    // immediately via QoreProgram::findEnum during reader.readValue,
    // instead of failing with "enum not found" when the enum hasn't been
    // deserialized yet.
    //
    // The deferred-resolution hooks for enum defaults (see
    // readDeferredMemberDefault's VT_ENUM branch and the
    // pending_enum_* fields on PendingInstanceMember /
    // PendingHashdeclMember) only fire for the OUTERMOST value tag;
    // an enum ref buried inside a VT_LIST / VT_HASH literal default
    // falls through to readValue which calls findEnum directly.
    // Previous order was: classes -> hashdecls -> enums -> typedefs,
    // which broke exactly that shape — e.g.
    // `qlib/GeneratorDataProvider/GeneratorRecordIterator.qc:91`
    // has `list<auto> fields = DefaultFields;` where DefaultFields
    // is a const list of hashes each containing
    // `GeneratorFieldType::Int`/`::String`/`::Float`.
    if (!deserializeEnums(error)) {
        return false;
    }
    if (!deserializeClasses(error)) {
        return false;
    }
    if (!deserializeHashDecls(error)) {
        return false;
    }
    if (!deserializeTypedefs(error)) {
        return false;
    }
    return true;
}

// Phase 2: resolution — base classes, member types, constants,
// globals, functions, methods, class commit, embedded source metadata,
// index rebuild, deferred BCA resolution.  Expected to run AFTER
// openAndDeserializeShells has completed for every blob in the
// current batch.
// Phase-split 2a-1.  Resolves types and bases only.
bool QoreAOTBinaryDeserializer::resolveTypes(std::string& error) {
    // Resolve class base classes (looks up bases via pgm->findClass,
    // so blobs from a sibling session are reachable after all shells
    // exist).
    if (!resolveClassBases(error)) {
        return false;
    }
    // Resolve typedefs first (multi-pass for forward refs), then enum base types and hashdecl members
    // Order matters: enum base types and hashdecl members may reference typedefs
    if (!resolveTypedefs(error)) {
        return false;
    }
    if (!resolveEnumBaseTypes(error)) {
        return false;
    }
    if (!resolveHashdeclMembers(error)) {
        return false;
    }
    return true;
}

// Phase-split 2a-2.  Register constants before member defaults are
// deserialized, because expression-tree member defaults can refer to same-class
// or same-module constants (for example Class::Defaults.Key).
bool QoreAOTBinaryDeserializer::resolveConstants(std::string& error) {
    return resolveClassConstants(error);
}

// Phase-split 2a-3.  Resolve each session's OWN instance members.  Does not
// import inherited base-class members — that must wait until sibling sessions
// have finished their own resolveInstanceMembers (cross-session sync point).
bool QoreAOTBinaryDeserializer::resolveMembers(std::string& error) {
    if (!resolveInstanceMembers(error)) {
        return false;
    }
    return true;
}

// Phase-split 2a-2b.  Register static members before instance-member defaults
// are deserialized, because those defaults can call static methods with static
// var arguments (for example HashDataType::default_other_field_type references
// DataProvider::AbstractDataProviderType::anyType).
bool QoreAOTBinaryDeserializer::resolveStaticMembersPhase(std::string& error) {
    return resolveStaticMembers(error);
}

static void rebuildAOTRootIndexes(QoreProgram* pgm) {
    qore_program_private* pp_idx = qore_program_private::get(*pgm);
    qore_root_ns_private* rpriv = static_cast<qore_root_ns_private*>(
        qore_ns_private::get(*pp_idx->RootNS));
    rpriv->rebuildAllIndexes();
}

bool QoreAOTBinaryDeserializer::resolveTypesAndMembers(std::string& error) {
    if (!resolveTypes(error)
            || !resolveConstants(error)
            || !resolveStaticsAndConstants(error)) {
        return false;
    }
    rebuildAOTRootIndexes(pgm);
    if (!deserializeFunctionsAndMethods(error)) {
        return false;
    }
    // Function deserialization mutates namespace function lists. Rebuild before
    // materializing member default expression ASTs that resolve through the
    // root runtime indexes.
    rebuildAOTRootIndexes(pgm);
    return resolveStaticMembersPhase(error)
        && resolveMembers(error);
}

// Phase-split 2a-sml.  Re-propagate super-class map list entries
// across sibling sessions.
//
// `resolveClassBases` calls `qc->addBaseClass(base, ...)` which
// reaches into `base->priv->scl->sml` to copy the base's
// ancestors into `qc->priv->scl->sml`.  If `base` is owned by a
// session whose own `resolveClassBases` hasn't run yet, its sml
// is incomplete — grandparents of `qc` never arrive.
// `processMemberInitializationList` later iterates sml to build
// `member_init_list`, so missing sml entries mean missing
// inherited-member init.
//
// This phase re-walks every newly deserialized class's scl and
// re-invokes `BCSMList::addBaseClassesToSubclass` from each base.
// `BCSMList::add` is idempotent (line 3557-3558 of QoreClass.cpp
// returns 0 if the target class ID is already in the sml), so
// the pass safely completes the cross-session sml without
// duplicates.
bool QoreAOTBinaryDeserializer::rebuildBaseClassSmlPhase(std::string& error) {
    for (size_t i = 0; i < class_list.size(); ++i) {
        if (preexisting_classes.count(static_cast<uint32_t>(i))) {
            continue;
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        if (!priv->scl) {
            continue;
        }
        for (auto* bcn : *priv->scl) {
            QoreClass* base = bcn->sclass;
            if (!base) {
                continue;
            }
            qore_class_private* base_priv = qore_class_private::get(*base);
            if (!base_priv->scl || !base_priv->scl->valid) {
                continue;
            }
            // Re-propagate base's ancestors into qc's sml. Safe
            // to call repeatedly — BCSMList::add skips duplicates.
            base_priv->scl->addBaseClassesToSubclass(base, qc, bcn->is_virtual);
        }
    }
    return true;
}

// Phase-split 2a-b.  Import inherited members from base classes
// into derived classes.  Must run AFTER every session's
// resolveInstanceMembers — otherwise a derived class in session X
// may copy an empty member list from a base class owned by session
// Y whose members haven't been registered yet.
//
// This is the silent failure mode that caused `zctx` (member of
// AbstractQorusClientProcess, inherited all the way up to QWf) to
// be missing from QWf's `member_init_list` at construction time —
// `initMembers` had no entry for it, `zctx` was NOTHING, and
// AbstractQorusClientProcess::constructor's `zctx.setOption(...)`
// raised `<nothing>::setOption()`.
bool QoreAOTBinaryDeserializer::importInheritedMembersPhase(std::string& error) {
    return importInheritedMembers(error);
}

// Phase-split 2a-c.  Top-level globals.  Run before function/method signatures
// are deserialized so native default-argument expressions can resolve globals
// declared in sibling script fragments.
bool QoreAOTBinaryDeserializer::resolveStaticsAndConstants(std::string& error) {
    if (globals_deserialized) {
        return true;
    }
    if (!deserializeGlobals(error)) {
        return false;
    }
    globals_deserialized = true;
    return true;
}

// Phase-split 2b.  Deserializes functions and methods into this
// session's classes.  Methods land in each class's pending hm/shm
// maps; parseCommit is NOT called here.
//
// In batch mode, the MultiDeserializer runs this phase on ALL
// sessions before any session's commitClasses, so a derived class's
// recursive parseCommit walk can't finalize a base class before its
// methods have been added in a sibling session.
bool QoreAOTBinaryDeserializer::resolveTypeTable(std::string& error) {
    if (!uses_type_table) {
        return true;
    }
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::TYPE_TABLE);
    if (!sec) {
        // Feature flag set but no section present — writer advertised the
        // capability and happened to emit no variants (interner never got
        // called).  That's fine: leave type_table_resolved empty and the
        // read path will never consult it because np will be 0 for every
        // variant and the return-type index will be 0 (= empty/nullptr).
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid TYPE_TABLE section data";
        return false;
    }
    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    type_table_resolved.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        const char* path = reader.readStringRef(ptr);
        if (!path || !*path) {
            // Index 0 (or any other empty-string entry) → no type
            // constraint / auto.
            type_table_resolved[i] = nullptr;
            continue;
        }
        std::string resolve_error;
        const QoreTypeInfo* ti = type_resolver->resolve(path, resolve_error);
        if (!resolve_error.empty()) {
            // Match the per-param fallback in readAndSetupVariantSignature:
            // missing types degrade to `auto` rather than aborting the
            // entire binary load, since the compiled code has the actual
            // checks baked in and this only affects variant matching.
            printd(2, "AOT type-table: cannot resolve '%s': %s (falling back to auto)\n",
                path, resolve_error.c_str());
            ti = autoTypeInfo;
        }
        type_table_resolved[i] = ti;
    }
    return true;
}

const QoreProgramLocation* QoreAOTBinaryDeserializer::getBlobLocation(int16_t start_line,
        int16_t end_line) const {
    if (!pgm) {
        return &loc_builtin;
    }
    const char* label = reader.getLabel();
    if (!label || !*label) {
        return &loc_builtin;
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    const char* interned_label = pp->addString(label);
    QoreProgramLocation loc(interned_label, start_line, end_line);
    return pp->getLocation(loc, start_line, end_line);
}

bool QoreAOTBinaryDeserializer::deserializeFunctionsAndMethods(std::string& error) {
    // Install pending-static-method-default context for the function/method
    // deserialization phase. Param defaults like
    // `string b = MultiPartMessage::getBoundary()` inside MultiPartMessage's
    // own constructor need deferred resolution because the referenced static
    // method has not been committed to the class vlist yet.  The vector
    // is a session member (`pending_smd`) so it survives into finalize().
    struct StaticMethodDefaultsRAII {
        StaticMethodDefaultsRAII(std::vector<PendingStaticMethodDefault>* p) {
            g_aot_pending_static_method_defaults = p;
        }
        ~StaticMethodDefaultsRAII() {
            g_aot_pending_static_method_defaults = nullptr;
        }
    };
    StaticMethodDefaultsRAII smd_raii(&pending_smd);

    // Resolve the per-blob TYPE_TABLE once up front so
    // readAndSetupVariantSignature can look up return/param types by
    // index.  Safe at this point: all sibling sessions' shells are
    // populated (phase 1 is complete across the whole batch) and
    // phase 2a's type pass has linked base classes + typedefs, so complex
    // type paths like `*hash<X::Y>` resolve.
    if (!resolveTypeTable(error)) {
        return false;
    }

    bool time_on = getenv("QORE_AOT_PHASE_TIMING") != nullptr;
    auto now_us = [] () -> uint64_t {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
    };
    uint64_t t0 = time_on ? now_us() : 0;
    if (!deserializeFunctions(error)) {
        return false;
    }
    uint64_t t1 = time_on ? now_us() : 0;
    if (!deserializeMethods(error)) {
        return false;
    }
    uint64_t t2 = time_on ? now_us() : 0;
    if (time_on) {
        // Per-session sub-breakdown of the dominant deserializeFuncsMethods
        // bucket.  Across 132 sessions (qwf), methods-only vs functions-only
        // reveals which pass to attack first.
        extern uint64_t g_aot_sum_funcs_us;
        extern uint64_t g_aot_sum_methods_us;
        g_aot_sum_funcs_us += (t1 - t0);
        g_aot_sum_methods_us += (t2 - t1);
    }
    return true;
}

// Sub-timing accumulators across all sessions.  Printed (if
// QORE_AOT_PHASE_TIMING is on) at the end of the process via a
// one-shot atexit hook installed on first use.
uint64_t g_aot_sum_funcs_us = 0;
uint64_t g_aot_sum_methods_us = 0;

// Deeper breakdown inside deserializeMethods — three sub-phases
// per variant: (a) allocate MethodVariantBase + dynamic_cast,
// (b) readAndSetupVariantSignature (type resolve + signature
// setup + default value read), (c) BCA read + addUserMethod.
uint64_t g_aot_dm_alloc_us = 0;
uint64_t g_aot_dm_sig_us = 0;
uint64_t g_aot_dm_add_us = 0;
uint64_t g_aot_dm_variants = 0;

// Finer-grained split of readAndSetupVariantSignature — the
// per-param read loop vs the final setupFromAOTMetadata call.
uint64_t g_aot_dm_sig_paramread_us = 0;
uint64_t g_aot_dm_sig_setup_us = 0;

// Phase-split 2c.  Commits all newly deserialized classes in this
// session.  Requires every class's method map to already be
// populated — in batch mode the MultiDeserializer ensures 2b has
// run on ALL sessions before calling 2c on any of them.
//
// Single-session callers invoke the full 4-sub-phase sequence
// (prepare → commit → importAbstract → validate).  Multi-session
// callers bypass this and interleave the sub-phases across sessions
// via the MultiDeserializer — see `QoreAOTBinaryMultiDeserializer::resolveAll`.
bool QoreAOTBinaryDeserializer::commitClasses(std::string& error) {
    return commitDeserializedClasses(error);
}

// Phase-split 2d.  Resolves deferred static-method defaults,
// embedded source metadata, rebuilds root-namespace indexes, and
// resolves the BCA (base-class constructor argument) expression blobs.
bool QoreAOTBinaryDeserializer::finalizePreIndex(std::string& error) {
    {
        for (const auto& pd : pending_smd) {
            const QoreClass* qc = !pd.class_path.empty()
                ? resolveClassRefForSession(pd.class_path.c_str())
                : nullptr;
            const QoreMethod* m = nullptr;
            if (qc && !pd.method_name.empty()) {
                m = qc->findStaticMethod(pd.method_name.c_str());
                if (!m) {
                    qore_class_private* qcp = qore_class_private::get(
                        *const_cast<QoreClass*>(qc));
                    m = qcp->parseFindLocalStaticMethod(pd.method_name.c_str());
                }
            }
            if (!m) {
                error = "cannot resolve deferred static method default '";
                error += qoreAOTDescribeClassRef(pd.class_path.c_str());
                error += "::";
                error += pd.method_name;
                error += "()'";
                return false;
            }
            UserSignature* sig = pd.uvb->getUserSignature();
            arg_vec_t& defaults = const_cast<arg_vec_t&>(sig->getDefaultArgList());
            if (pd.param_index < defaults.size()) {
                defaults[pd.param_index].discard(nullptr);
                defaults[pd.param_index] = QoreValue(new StaticMethodCallNode(
                    &loc_builtin, m, (QoreParseListNode*)nullptr));
            }
        }
        pending_smd.clear();
    }
    if (!deserializeFallbackSources(error)) {
        return false;
    }
    return true;
}

bool QoreAOTBinaryDeserializer::finalizePostIndex(std::string& error) {
    // Resolve deferred BCA (base class constructor argument) blobs.
    // Must run after commitDeserializedClasses + rebuildAllIndexes so all
    // methods and classes are findable by the native/legacy expression handlers.
    if (!resolveBCAExpressions(error)) {
        return false;
    }

    printd(2, "AOT: deserialized namespace tree: %d namespaces, %d classes\n",
        static_cast<int>(ns_list.size()), static_cast<int>(class_list.size()));

    return true;
}

bool QoreAOTBinaryDeserializer::finalize(std::string& error) {
    if (!finalizePreIndex(error)) {
        return false;
    }
    // Rebuild root namespace indexes (fmap, varmap, clmap, etc.) so that
    // runtime lookups like runtimeFindFunctionEntry() can find the
    // deserialized functions, classes, etc.
    {
        rebuildAOTRootIndexes(pgm);
    }
    return finalizePostIndex(error);
}

void QoreAOTBinaryMultiDeserializer::rebuildRootIndexesOnce() {
    rebuildAOTRootIndexes(pgm);
}

bool QoreAOTBinaryDeserializer::resolveAll(std::string& error) {
    return resolveTypesAndMembers(error)
        && rebuildBaseClassSmlPhase(error)
        && importInheritedMembersPhase(error)
        && resolveStaticsAndConstants(error)
        && commitClasses(error)
        && finalize(error);
}

// Phase 4 slice 10: single-blob entry point preserved as a chain of
// phase-1 + phase-2 so existing callers (qore_aot_module_init_v3 and
// friends in QoreAOTRuntime.cpp) remain unchanged.
bool QoreAOTBinaryDeserializer::deserializeIntoProgram(QoreProgram* in_pgm,
        const uint8_t* data, uint32_t size, std::string& error) {
    if (!openAndDeserializeShells(in_pgm, data, size, error)) {
        return false;
    }
    return resolveAll(error);
}

bool QoreAOTBinaryDeserializer::deserializeIntoProgram(QoreProgram* in_pgm,
        QoreAOTBinaryReader&& open_reader, std::string& error) {
    if (!openAndDeserializeShells(in_pgm, std::move(open_reader), error)) {
        return false;
    }
    return resolveAll(error);
}

bool QoreAOTBinaryDeserializer::deserializeNamespaces(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::NAMESPACES);
    if (!sec) {
        return true;  // no namespaces section is OK
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid NAMESPACES section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    ns_list.resize(count);

    // Get program's root namespace
    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);

    auto restore_path = [](qore_ns_private* nsp, const char* serialized_path, const qore_ns_private* parent) {
        std::string path;
        if (serialized_path && *serialized_path) {
            path = serialized_path;
            if (path != "::" && path.rfind("::", 0) != 0) {
                path.insert(0, "::");
            }
        } else if (parent) {
            path = parent->path;
            if (path != "::") {
                path += "::";
            }
            path += nsp->name;
        } else {
            path = "::";
        }

        if (nsp->path != path) {
            nsp->path = std::move(path);
        }
    };

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* path = reader.readStringRef(ptr);
        uint32_t parent_idx = QoreAOTBinaryReader::readU32(ptr);
        uint32_t depth = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        (void)depth;

        if (parent_idx == UINT32_MAX) {
            // Root namespace - use existing
            restore_path(root_ns, path, nullptr);
            ns_list[i] = root_ns;
        } else {
            // Create child namespace and add to parent
            if (parent_idx >= ns_list.size() || !ns_list[parent_idx]) {
                error = "invalid parent namespace index " + std::to_string(parent_idx);
                return false;
            }

            // Check if this namespace already exists in the parent (e.g. "Qore" system NS)
            QoreNamespace* existing = nullptr;
            auto it = ns_list[parent_idx]->nsl.nsmap.find(name);
            if (it != ns_list[parent_idx]->nsl.nsmap.end()) {
                existing = it->second;
            }

            if (existing) {
                qore_ns_private* existing_priv = qore_ns_private::get(*existing);
                restore_path(existing_priv, path, ns_list[parent_idx]);
                if (flags & 0x0001) {
                    existing_priv->pub = true;
                }
                // Repair from_module attribution when this module is the authoritative owner.
                // The existing namespace may have been created earlier in this program by a
                // dependency that extends our namespace (e.g. ConnectionProvider.qmod depends on
                // DataProvider.qmod, and DP's deserialization runs first under mod_ctx="DataProvider"
                // — creating a ConnectionProvider namespace attributed to "DataProvider"). When CP's
                // own deserializer then finds its namespace already present, we must re-attribute it
                // to CP so reflection reports the correct module owner. Mirrors the parseAssimilate()
                // repair used on the source-loading path.
                const char* mod_ctx = get_module_context_name();
                if (mod_ctx && strcmp(mod_ctx, name) == 0) {
                    const char* existing_from = existing_priv->getModuleName();
                    if (!existing_from || strcmp(existing_from, mod_ctx) != 0) {
                        existing_priv->overrideFromModule(mod_ctx);
                    }
                }
                ns_list[i] = existing_priv;
            } else {
                QoreNamespace* ns = new QoreNamespace(name);
                qore_ns_private* nsp = qore_ns_private::get(*ns);
                nsp->pub = (flags & 0x0001) != 0;
                // Mark as non-builtin so it's treated as user-defined and can be merged
                nsp->builtin = false;
                restore_path(nsp, path, ns_list[parent_idx]);
                ns_list[parent_idx]->ns->addNamespace(ns);
                ns_list[i] = nsp;
            }
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeClasses(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::CLASSES);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid CLASSES section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    class_list.resize(count);
    class_signature_hashes.resize(count);
    class_injected_paths.resize(count);
    const bool has_class_hash = (reader.getHeader().feature_flags & QORE_AOT_FEAT_CLASS_HASH) != 0;
    const bool has_class_injection
        = (reader.getHeader().feature_flags & QORE_AOT_FEAT_CLASS_INJECTION) != 0;
    const bool has_class_type_params
        = (reader.getHeader().feature_flags & QORE_AOT_FEAT_CLASS_TYPE_PARAMS) != 0;
    const bool has_type_param_defaults
        = (reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPE_PARAM_DEFAULTS) != 0;
    const bool has_type_param_bounds
        = (reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPE_PARAM_BOUNDS) != 0;
    const bool has_class_param_bases
        = (reader.getHeader().feature_flags & QORE_AOT_FEAT_CLASS_PARAM_BASES) != 0;
    const bool has_class_raw_generic
        = (reader.getHeader().feature_flags & QORE_AOT_FEAT_CLASS_RAW_GENERIC) != 0;

    // Populate the root namespace's clmap incrementally as each class is
    // created, so standard lookup paths (runtimeFindClass, findClass,
    // en_resolveClass in EXPR_TREE handlers) work during deserialization.
    // The pending_class_map is kept as a secondary fallback for the
    // VT_NEW_OBJECT deferred path which may encounter forward references
    // (class A's member default references class B that hasn't been added
    // to a namespace yet due to ordering within this same loop).
    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_root_ns_private* root_priv = static_cast<qore_root_ns_private*>(
        qore_ns_private::get(*pp->RootNS));
    std::unordered_map<std::string, QoreClass*> pending_class_map;
    struct ClassMapRAII {
        ClassMapRAII(const std::unordered_map<std::string, QoreClass*>* p) {
            g_aot_pending_class_map = p;
        }
        ~ClassMapRAII() {
            g_aot_pending_class_map = nullptr;
        }
    };
    ClassMapRAII raii(&pending_class_map);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* path = reader.readStringRef(ptr);
        const std::string class_name(name ? name : "");
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        int64_t domain = QoreAOTBinaryReader::readI64(ptr);
        if (has_class_hash) {
            uint8_t hash_valid = QoreAOTBinaryReader::readU8(ptr);
            if (ptr + SH_SIZE > end) {
                error = "invalid class signature hash for class '" + std::string(name) + "'";
                return false;
            }
            if (hash_valid) {
                class_signature_hashes[i].assign(reinterpret_cast<const char*>(ptr), SH_SIZE);
            }
            ptr += SH_SIZE;
        }
        const char* injected_path = "";
        if (has_class_injection) {
            injected_path = reader.readStringRef(ptr);
        }
        std::vector<QoreGenericTypeParam> type_params;
        if (has_class_type_params) {
            uint32_t type_param_count = QoreAOTBinaryReader::readU32(ptr);
            type_params.reserve(type_param_count);
            std::unordered_set<std::string> seen_type_params;
            for (uint32_t j = 0; j < type_param_count; ++j) {
                if (j && !(j % 100)
                        && qore_check_cancel(nullptr, "AOT class type parameter deserialization")) {
                    error = "operation cancelled during AOT class type parameter deserialization";
                    return false;
                }
                const char* type_param = reader.readStringRef(ptr);
                std::string type_param_name(type_param ? type_param : "");
                if (type_param_name.empty()) {
                    error = "invalid empty type parameter for class '" + class_name + "'";
                    return false;
                }
                if (!seen_type_params.insert(type_param_name).second) {
                    error = "duplicate type parameter '" + type_param_name + "' for class '"
                        + class_name + "'";
                    return false;
                }
                std::string default_type;
                if (has_type_param_defaults) {
                    uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
                    if (has_default) {
                        const char* default_type_str = reader.readStringRef(ptr);
                        default_type = default_type_str ? default_type_str : "";
                        if (default_type.empty()) {
                            error = "invalid empty default type for type parameter '" + type_param_name
                                + "' in class '" + class_name + "'";
                            return false;
                        }
                    }
                }
                std::string bound_type;
                if (has_type_param_bounds) {
                    uint8_t has_bound = QoreAOTBinaryReader::readU8(ptr);
                    if (has_bound) {
                        const char* bound_type_str = reader.readStringRef(ptr);
                        bound_type = bound_type_str ? bound_type_str : "";
                        if (bound_type.empty()) {
                            error = "invalid empty bound type for type parameter '" + type_param_name
                                + "' in class '" + class_name + "'";
                            return false;
                        }
                    }
                }
                type_params.emplace_back(std::move(type_param_name), std::move(default_type),
                    std::move(bound_type));
            }
        }

        // Validate namespace index before creating the class
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            error = "invalid namespace index for class '" + std::string(name) + "'";
            return false;
        }

        // Create the class and add to namespace immediately so it's owned
        // by the namespace (QoreClass destructor is protected)
        QoreClass* qc = new QoreClass(name, path, domain);
        qore_class_private* priv = qore_class_private::get(*qc);
        priv->loc = getBlobLocation();
        priv->pub = (flags & 0x0001) != 0;
        if (flags & 0x0002) {
            priv->final = true;
        }
        priv->inject = (flags & 0x0004) != 0;
        if (flags & 0x0008) {
            priv->reexport = true;
        }
        if (!type_params.empty()) {
            for (const QoreGenericTypeParam& type_param : type_params) {
                qc->addTypeParameter(type_param.name.c_str(), type_param.getDefaultType(),
                    type_param.getBoundType());
            }
        }
        if (has_class_raw_generic) {
            priv->setLegacyRawGenericCompatibility((flags & 0x0010) != 0, (flags & 0x0020) != 0);
        }
        bool class_already_existed = false;
        int add_rv = ns_list[ns_idx]->classList.add(qc);
        if (add_rv != 0) {
            printd(2, "AOT deser: class '%s' already exists in namespace, using existing\n", name);
            // Class already exists - use the existing one and delete the new one
            QoreClass* existing = ns_list[ns_idx]->classList.find(name);
            qore_class_private::get(*qc)->deref(true, true);
            qc = existing;
            class_already_existed = true;
            preexisting_classes.insert(i);
        } else {
            // Link the class back to its owning namespace. `classList.add`
            // only puts the pointer in the map — it does NOT update the
            // class's own ns pointer. Without this, QoreClass::getNamespacePath
            // returns an empty string (priv->ns is null), breaking
            // Serializable::serialize (it writes "" as _class, then
            // deserialize fails with "Cannot find class ''").
            qore_class_private::get(*qc)->setNamespaceConditional(ns_list[ns_idx]);
        }
        class_list[i] = qc;

        // Update root namespace's clmap so all standard lookup paths work
        // immediately (runtimeFindClass, en_resolveClass, etc.).
        root_priv->clmap.update(qc->getName(), ns_list[ns_idx], qc);

        // Also register into the forward-ref pending map as a fallback for
        // VT_NEW_OBJECT member init expressions that may encounter ordering
        // issues (class A's member default references class B and vice versa).
        if (path && *path) {
            pending_class_map[path] = qc;
            if (strncmp(path, "::", 2) == 0) {
                pending_class_map[std::string(path + 2)] = qc;
            } else {
                pending_class_map[std::string("::") + path] = qc;
            }
        }

        // Read base classes (store paths for later resolution)
        uint32_t num_bases = QoreAOTBinaryReader::readU32(ptr);
        std::vector<PendingBaseClass> bases;
        bases.reserve(num_bases);
        for (uint32_t j = 0; j < num_bases; ++j) {
            const char* base_path = reader.readStringRef(ptr);
            uint8_t access = QoreAOTBinaryReader::readU8(ptr);
            uint8_t is_virtual = QoreAOTBinaryReader::readU8(ptr);
            const char* base_type_path = has_class_param_bases ? reader.readStringRef(ptr) : nullptr;
            if (base_path && *base_path) {
                PendingBaseClass pbc;
                pbc.base_path = base_path;
                pbc.type_path = base_type_path ? base_type_path : "";
                pbc.access = access;
                pbc.is_virtual = (is_virtual != 0);
                bases.push_back(std::move(pbc));
            }
        }
        // Skip pending data for classes that already existed (from loaded modules)
        // — they already have their bases, members, etc. set up
        if (class_already_existed) {
            bases.clear();
        } else if (injected_path && *injected_path) {
            class_injected_paths[i] = injected_path;
        }
        pending_bases.push_back(std::move(bases));

        // Read instance members (store for later resolution after hashdecls/enums)
        uint32_t num_members = QoreAOTBinaryReader::readU32(ptr);
        std::vector<PendingInstanceMember> instance_members;
        if (!class_already_existed) {
            instance_members.reserve(num_members);
        }
        for (uint32_t j = 0; j < num_members; ++j) {
            const char* mname = reader.readStringRef(ptr);
            const char* mtype_path = reader.readStringRef(ptr);
            uint8_t maccess = QoreAOTBinaryReader::readU8(ptr);
            uint8_t mflags = QoreAOTBinaryReader::readU8(ptr);
            uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
            QoreValue default_val;
            PendingInstanceMember pim;
            pim.name = mname ? mname : "";
            pim.type_path = mtype_path ? mtype_path : "";
            pim.access = maccess;
            pim.flags = mflags;
            if (has_default) {
                if (!readDeferredClassConstantValue(reader, ptr, end, error,
                        pim.value_blob)) {
                    error = "instance member '" + pim.name + "' default: " + error;
                    return false;
                }
            }
            pim.default_val = default_val;

            if (!class_already_existed && mname && *mname) {
                instance_members.push_back(std::move(pim));
            } else {
                if (default_val.hasNode()) {
                    default_val.discard(nullptr);
                }
                for (auto& v : pim.pending_new_args) {
                    v.discard(nullptr);
                }
            }
        }
        pending_instance_members.push_back(std::move(instance_members));

        // Read static members (store for later resolution)
        uint32_t num_static = QoreAOTBinaryReader::readU32(ptr);
        std::vector<PendingStaticMember> static_members;
        if (!class_already_existed) {
            static_members.reserve(num_static);
        }
        for (uint32_t j = 0; j < num_static; ++j) {
            const char* sm_name = reader.readStringRef(ptr);
            const char* sm_type_path = reader.readStringRef(ptr);
            uint8_t sm_access = QoreAOTBinaryReader::readU8(ptr);
            // Read the serialized initial value if present. Matches the
            // write-side layout: u8 has_value + optional value.
            QoreValue default_val;
            PendingStaticMember psm;
            psm.name = sm_name ? sm_name : "";
            psm.type_path = sm_type_path ? sm_type_path : "";
            psm.access = sm_access;
            uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
            if (has_default) {
                if (!readDeferredClassConstantValue(reader, ptr, end, error,
                        psm.value_blob)) {
                    error = "static member '" + psm.name + "': " + error;
                    return false;
                }
            }
            psm.default_val = default_val;
            if (!class_already_existed && sm_name && *sm_name) {
                static_members.push_back(std::move(psm));
            } else {
                if (default_val.hasNode()) {
                    default_val.discard(nullptr);
                }
                for (auto& v : psm.pending_new_args) {
                    v.discard(nullptr);
                }
            }
        }
        pending_static_members.push_back(std::move(static_members));

        // Read class constants (store for later resolution after hashdecls/enums)
        uint32_t num_consts = QoreAOTBinaryReader::readU32(ptr);
        std::vector<PendingClassConstant> class_constants;
        if (!class_already_existed) {
            class_constants.reserve(num_consts);
        }
        const bool has_const_pending_flag =
            (reader.getHeader().feature_flags & QORE_AOT_FEAT_CONST_PENDING) != 0;
        for (uint32_t j = 0; j < num_consts; ++j) {
            const char* cname = reader.readStringRef(ptr);
            const char* ctype_path = reader.readStringRef(ptr);
            uint8_t caccess = QoreAOTBinaryReader::readU8(ptr);
            uint8_t cpending = has_const_pending_flag ? QoreAOTBinaryReader::readU8(ptr) : 0;
            std::vector<uint8_t> value_blob;
            if (!readDeferredClassConstantValue(reader, ptr, end, error,
                    value_blob)) {
                error = "class constant '" + std::string(cname ? cname : "(null)") + "': " + error;
                return false;
            }

            if (!class_already_existed && cname && *cname) {
                PendingClassConstant pcc;
                pcc.name = cname;
                pcc.type_path = ctype_path ? ctype_path : "";
                pcc.access = caccess;
                pcc.pending_init = (cpending != 0);
                pcc.value_blob = std::move(value_blob);
                class_constants.push_back(std::move(pcc));
            }
        }
        pending_class_constants.push_back(std::move(class_constants));
    }

    if (has_class_injection) {
        std::unordered_map<std::string, QoreClass*> all_class_map;
        for (uint32_t i = 0; i < class_list.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT class injection map build")) {
                error = "AOT class injection map build cancelled";
                return false;
            }
            qoreAOTAddClassLookupAliases(all_class_map, class_list[i]);
        }

        for (uint32_t i = 0; i < class_injected_paths.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT class injection resolution")) {
                error = "AOT class injection resolution cancelled";
                return false;
            }
            if (preexisting_classes.count(i) || !class_list[i] || class_injected_paths[i].empty()) {
                continue;
            }
            const QoreClass* injected = resolveClassRefForSession(
                class_injected_paths[i].c_str(), &all_class_map);
            if (!injected) {
                error = "cannot resolve injected target class '" + class_injected_paths[i] + "' for class '"
                    + std::string(class_list[i]->getName()) + "'";
                return false;
            }
            qore_class_private::get(*class_list[i])->injectedClass = qore_class_private::get(*injected);
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::resolveClassBases(std::string& error) {
    uint32_t count = std::min(static_cast<uint32_t>(class_list.size()),
        static_cast<uint32_t>(pending_bases.size()));

    // Build a map from class path to class_list index for newly deserialized classes
    std::unordered_map<std::string, uint32_t> path_to_idx;
    for (uint32_t i = 0; i < count; ++i) {
        if (!class_list[i] || preexisting_classes.count(i)) {
            continue;
        }
        path_to_idx[class_list[i]->getPath()] = i;
    }

    // Compute topological order (bases before derived) using Kahn's algorithm.
    // This ensures addBaseClass() can propagate grandparent classes correctly,
    // since the base class's hierarchy is fully resolved before the derived class.
    {
        // Build adjacency graph: edge from base_idx -> derived_idx
        std::vector<std::vector<uint32_t>> dependents(count);
        std::vector<uint32_t> in_degree(count, 0);

        for (uint32_t i = 0; i < count; ++i) {
            if (!class_list[i] || preexisting_classes.count(i)) {
                continue;
            }
            for (auto& pbc : pending_bases[i]) {
                auto it = path_to_idx.find(pbc.base_path);
                if (it != path_to_idx.end() && it->second != i) {
                    // Base class is also newly deserialized — must be processed first
                    dependents[it->second].push_back(i);
                    ++in_degree[i];
                }
            }
        }

        // Kahn's algorithm: start with classes that have no in-module base dependencies
        std::deque<uint32_t> queue;
        for (uint32_t i = 0; i < count; ++i) {
            if (!class_list[i] || preexisting_classes.count(i)) {
                continue;
            }
            if (in_degree[i] == 0) {
                queue.push_back(i);
            }
        }

        topo_order.clear();
        topo_order.reserve(count);
        while (!queue.empty()) {
            uint32_t idx = queue.front();
            queue.pop_front();
            topo_order.push_back(idx);
            for (uint32_t dep : dependents[idx]) {
                if (--in_degree[dep] == 0) {
                    queue.push_back(dep);
                }
            }
        }

        // Add any classes not in the topological order (preexisting or null)
        // These are processed last but typically skipped anyway
        for (uint32_t i = 0; i < count; ++i) {
            if (!class_list[i] || preexisting_classes.count(i)) {
                topo_order.push_back(i);
            }
        }

        printd(5, "AOT deser: topological order for %d classes computed (%d in topo sort)\n",
            (int)count, (int)topo_order.size());
    }

    // Resolve base classes in topological order (bases before derived)
    for (uint32_t idx : topo_order) {
        if (idx >= count || !class_list[idx] || preexisting_classes.count(idx)) {
            continue;
        }
        QoreClass* qc = class_list[idx];

        for (auto& pbc : pending_bases[idx]) {
            const QoreClass* base = resolveClassRefForSession(pbc.base_path.c_str());
            if (base) {
                // Add base class to this class with proper access level
                qc->addBaseClass(const_cast<QoreClass*>(base),
                    static_cast<ClassAccess>(pbc.access), pbc.is_virtual);
                if (!pbc.type_path.empty()) {
                    std::string type_error;
                    const QoreTypeInfo* base_type = type_resolver->resolve(pbc.type_path.c_str(), type_error);
                    if (!base_type) {
                        error = "cannot resolve parameterized base type '" + pbc.type_path + "' for class '" +
                            std::string(qc->getName()) + "'";
                        if (!type_error.empty()) {
                            error += ": ";
                            error += type_error;
                        }
                        pending_bases.clear();
                        return false;
                    }
                    qore_class_private::get(*qc)->addParameterizedVirtualBase(base_type);
                } else {
                    const qore_class_private* base_priv = qore_class_private::get(*base);
                    if (base_priv->hasTypeParams() && base_priv->rawConstructionDefaultsToAuto()) {
                        type_vec_t raw_args(base->getTypeParameterCount(), autoTypeInfo);
                        qore_class_private::get(*qc)->addParameterizedVirtualBase(base->getTypeInfo(raw_args));
                    }
                }
                // Source parsing treats inherited classes as class-signature
                // input. AOT attaches bases manually, so mark the signature
                // dirty here; otherwise memberless derived classes commit with
                // an empty hash and fail cross-Program class compatibility.
                qore_class_private::get(*qc)->has_sig_changes = true;
                printd(5, "AOT deser: resolved base class '%s' (id: %d) for class '%s' (id: %d)\n",
                    pbc.base_path.c_str(), base->getID(),
                    qc->getName(), qc->getID());
            } else {
                error = "cannot resolve base class '" + pbc.base_path + "' for class '" +
                    std::string(qc->getName()) + "'";
                pending_bases.clear();
                return false;
            }
        }
    }

    // Clear pending data
    pending_bases.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::resolveInstanceMembers(std::string& error) {
    // Second pass: create instance members now that types are resolved
    // NOTE: hashdecls and enums must be deserialized before calling this method
    // so that type references to them can be resolved
    // Build a path→QoreClass* map across all deserialized classes so pending
    // forward-reference `NewObject` init expressions can be resolved here,
    // after every class has been registered.
    std::unordered_map<std::string, QoreClass*> all_class_map;
    for (uint32_t i = 0; i < class_list.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT instance member class lookup map build")) {
            error = "AOT instance member class lookup map build cancelled";
            return false;
        }
        qoreAOTAddClassLookupAliases(all_class_map, class_list[i]);
    }

    for (uint32_t i = 0; i < class_list.size() && i < pending_instance_members.size(); ++i) {
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        qore_class_private* priv = qore_class_private::get(*qc);
        for (auto& pim : pending_instance_members[i]) {
            if (!materializeDeferredMemberDefault(reader, pim, pim.default_val,
                    error, "class", qc->getName(), pim.name.c_str())) {
                return false;
            }

            const QoreTypeInfo* ti = nullptr;
            if (!pim.type_path.empty()) {
                std::string type_error;
                ti = type_resolver->resolve(pim.type_path.c_str(), type_error);
                if (!type_error.empty() || !ti) {
                    return setAOTDeferredMemberResolutionError(error, "member type",
                        pim.type_path.c_str(), "class", qc->getName(),
                        pim.name.c_str(), type_error.c_str());
                }
            }

            // Resolve a pending forward-referenced NewObject default if any.
            if (!pim.pending_new_class_path.empty()) {
                const QoreClass* target = resolveClassRefForSession(
                    pim.pending_new_class_path.c_str(), &all_class_map);
                pim.default_val = qoreAOTMakeObjectDefaultNode(getProgram(), target,
                    pim.pending_new_class_path, pim.pending_new_args);
                pim.pending_new_class_path.clear();
            }

            // Resolve a pending forward-referenced enum member default if any.
            // Enums are deserialized after classes, so member defaults that
            // reference enum values are deferred until here.
            if (!pim.pending_enum_path.empty()) {
                const QoreNamespace* pns = nullptr;
                const QoreEnumDecl* ed = getProgram()->findEnum(
                    pim.pending_enum_path.c_str(), pns);
                if (ed) {
                    const QoreEnumMember* member = ed->findMember(
                        pim.pending_enum_member.c_str());
                    if (member) {
                        pim.default_val = QoreValue::makeEnum(member);
                    } else {
                        std::string enum_member = pim.pending_enum_path + "::"
                            + pim.pending_enum_member;
                        return setAOTDeferredMemberResolutionError(error,
                            "enum member default", enum_member.c_str(), "class",
                            qc->getName(), pim.name.c_str());
                    }
                } else {
                    return setAOTDeferredMemberResolutionError(error,
                        "enum default", pim.pending_enum_path.c_str(), "class",
                        qc->getName(), pim.name.c_str());
                }
                pim.pending_enum_path.clear();
                pim.pending_enum_member.clear();
            }

            // Resolve a pending complex-type default (deferred because the
            // referenced type wasn't registered yet during deserializeClasses).
            if (pim.pending_complex_default_kind >= 0) {
                QoreParseListNode* parse_args = nullptr;
                if (!pim.pending_complex_default_args.empty()) {
                    parse_args = new QoreParseListNode(&loc_builtin);
                    for (auto& v : pim.pending_complex_default_args) {
                        parse_args->add(v, &loc_builtin);
                    }
                    pim.pending_complex_default_args.clear();
                }
                if (pim.pending_complex_default_kind == 2) {
                    // Hashdecl: resolve now when possible, otherwise keep a
                    // dynamic node so source-parse preloads do not force the
                    // provider .qo into the compile graph.
                    pim.default_val = qoreAOTMakeHashDeclDefaultNode(getProgram(),
                        pim.pending_complex_default_path, parse_args);
                } else {
                    // kind 0 (complex list) or kind 1 (complex hash)
                    std::string type_error;
                    const QoreTypeInfo* cti = type_resolver->resolve(
                        pim.pending_complex_default_path.c_str(), type_error);
                    if (cti && type_error.empty()) {
                        if (pim.pending_complex_default_kind == 0) {
                            QoreValue list_args;
                            if (parse_args) {
                                list_args = QoreValue(parse_args);
                            }
                            NewComplexListNode* ncl = new NewComplexListNode(
                                &loc_builtin, cti, list_args);
                            pim.default_val = QoreValue(ncl);
                        } else if (pim.pending_complex_default_kind == 3) {
                            QoreValue buffer_args;
                            if (parse_args) {
                                buffer_args = QoreValue(parse_args);
                            }
                            NewComplexBufferNode* ncb = new NewComplexBufferNode(
                                &loc_builtin, cti, buffer_args,
                                static_cast<QoreComplexBufferInitKind>(pim.pending_complex_buffer_init_kind));
                            pim.default_val = QoreValue(ncb);
                        } else {
                            NewComplexHashNode* nch = new NewComplexHashNode(
                                &loc_builtin, cti, parse_args);
                            pim.default_val = QoreValue(nch);
                        }
                    } else {
                        if (parse_args) {
                            parse_args->deref(nullptr);
                        }
                        std::string details = "complex default kind="
                            + std::to_string((int)pim.pending_complex_default_kind);
                        if (!type_error.empty()) {
                            details += "; ";
                            details += type_error;
                        }
                        return setAOTDeferredMemberResolutionError(error,
                            "complex default type",
                            pim.pending_complex_default_path.c_str(), "class",
                            qc->getName(), pim.name.c_str(), details.c_str());
                    }
                }
                pim.pending_complex_default_kind = -1;
                pim.pending_complex_buffer_init_kind = 0;
                pim.pending_complex_default_path.clear();
            }

            if (!resolveDeferredConstRefDefault(pim.pending_const_ref_path,
                    pim.default_val, getProgram(), "class", qc->getName(),
                    pim.name.c_str(), error)) {
                return false;
            }
            resolveDeferredExprTreeDefault(pim.pending_expr_tree_blob,
                pim.default_val, getProgram(), "class", qc->getName(),
                pim.name.c_str());
            LocalVar* self_local = qore_class_private::getSelfId(*qc);
            if (!resolveDeferredNativeExprDefault(reader, pim.pending_expr_native_blob,
                    pim.default_val, getProgram(), "class", qc->getName(),
                    pim.name.c_str(), error, false, &self_local, 1)) {
                return false;
            }
            // Transfer ownership of the default value to the class member
            QoreValue default_val = pim.default_val;
            pim.default_val = QoreValue();  // Clear to prevent double-deref
            priv->addMember(pim.name.c_str(), static_cast<ClassAccess>(pim.access), ti,
                default_val);
            // Apply member flags — specifically the transient flag, which
            // excludes the member from Serializable::serialize(). Without
            // this, `transient RWLock rwlock();` style members get serialized
            // (and fail) at runtime because the flag is lost.
            if (pim.flags & 0x01) {
                QoreMemberInfo* new_mi = priv->members.find(pim.name.c_str());
                if (new_mi) {
                    new_mi->setTransient();
                    if (!priv->has_transient_member) {
                        priv->has_transient_member = true;
                    }
                }
            }

            printd(5, "AOT deser: added instance member '%s' to class '%s'\n",
                pim.name.c_str(), qc->getName());
        }
    }

    // Clear pending data
    pending_instance_members.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::resolveStaticMembers(std::string& error) {
    // Second pass: create static members now that types are resolved
    // NOTE: hashdecls and enums must be deserialized before calling this method
    // so that type references to them can be resolved
    // Build an all-classes path map for pending forward-ref NewObject init
    // resolution (mirrors resolveInstanceMembers).
    std::unordered_map<std::string, QoreClass*> all_class_map;
    for (uint32_t i = 0; i < class_list.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT static member class lookup map build")) {
            error = "AOT static member class lookup map build cancelled";
            return false;
        }
        qoreAOTAddClassLookupAliases(all_class_map, class_list[i]);
    }

    for (uint32_t i = 0; i < class_list.size() && i < pending_static_members.size(); ++i) {
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        qore_class_private* priv = qore_class_private::get(*qc);
        for (auto& psm : pending_static_members[i]) {
            if (!materializeDeferredMemberDefault(reader, psm, psm.default_val,
                    error, "class static member", qc->getName(),
                    psm.name.c_str())) {
                return false;
            }

            const QoreTypeInfo* ti = nullptr;
            if (!psm.type_path.empty()) {
                std::string type_error;
                ti = type_resolver->resolve(psm.type_path.c_str(), type_error);
                if (!type_error.empty() || !ti) {
                    return setAOTDeferredMemberResolutionError(error, "static member type",
                        psm.type_path.c_str(), "class", qc->getName(),
                        psm.name.c_str(), type_error.c_str());
                }
            }

            // Resolve a pending forward-referenced NewObject default if any
            if (!psm.pending_new_class_path.empty()) {
                const QoreClass* target = resolveClassRefForSession(
                    psm.pending_new_class_path.c_str(), &all_class_map);
                psm.default_val = qoreAOTMakeObjectDefaultNode(getProgram(), target,
                    psm.pending_new_class_path, psm.pending_new_args);
                psm.pending_new_class_path.clear();
            }

            // Resolve a pending forward-referenced enum member default if any.
            // Enums are deserialized after classes, so static member defaults
            // that reference enum values are deferred until here.
            if (!psm.pending_enum_path.empty()) {
                const QoreNamespace* pns = nullptr;
                const QoreEnumDecl* ed = getProgram()->findEnum(
                    psm.pending_enum_path.c_str(), pns);
                if (ed) {
                    const QoreEnumMember* member = ed->findMember(
                        psm.pending_enum_member.c_str());
                    if (member) {
                        psm.default_val = QoreValue::makeEnum(member);
                    } else {
                        std::string enum_member = psm.pending_enum_path + "::"
                            + psm.pending_enum_member;
                        return setAOTDeferredMemberResolutionError(error,
                            "enum member default", enum_member.c_str(), "class",
                            qc->getName(), psm.name.c_str());
                    }
                } else {
                    return setAOTDeferredMemberResolutionError(error,
                        "enum default", psm.pending_enum_path.c_str(), "class",
                        qc->getName(), psm.name.c_str());
                }
                psm.pending_enum_path.clear();
                psm.pending_enum_member.clear();
            }

            // Resolve a pending complex-type default.
            if (psm.pending_complex_default_kind >= 0) {
                QoreParseListNode* parse_args = nullptr;
                if (!psm.pending_complex_default_args.empty()) {
                    parse_args = new QoreParseListNode(&loc_builtin);
                    for (auto& v : psm.pending_complex_default_args) {
                        parse_args->add(v, &loc_builtin);
                    }
                    psm.pending_complex_default_args.clear();
                }
                if (psm.pending_complex_default_kind == 2) {
                    psm.default_val = qoreAOTMakeHashDeclDefaultNode(getProgram(),
                        psm.pending_complex_default_path, parse_args);
                } else {
                    std::string type_error;
                    const QoreTypeInfo* cti = type_resolver->resolve(
                        psm.pending_complex_default_path.c_str(), type_error);
                    if (cti && type_error.empty()) {
                        if (psm.pending_complex_default_kind == 0) {
                            QoreValue list_args;
                            if (parse_args) {
                                list_args = QoreValue(parse_args);
                            }
                            NewComplexListNode* ncl = new NewComplexListNode(
                                &loc_builtin, cti, list_args);
                            psm.default_val = QoreValue(ncl);
                        } else if (psm.pending_complex_default_kind == 3) {
                            QoreValue buffer_args;
                            if (parse_args) {
                                buffer_args = QoreValue(parse_args);
                            }
                            NewComplexBufferNode* ncb = new NewComplexBufferNode(
                                &loc_builtin, cti, buffer_args,
                                static_cast<QoreComplexBufferInitKind>(psm.pending_complex_buffer_init_kind));
                            psm.default_val = QoreValue(ncb);
                        } else {
                            NewComplexHashNode* nch = new NewComplexHashNode(
                                &loc_builtin, cti, parse_args);
                            psm.default_val = QoreValue(nch);
                        }
                    } else {
                        if (parse_args) {
                            parse_args->deref(nullptr);
                        }
                        std::string details = "complex default kind="
                            + std::to_string((int)psm.pending_complex_default_kind);
                        if (!type_error.empty()) {
                            details += "; ";
                            details += type_error;
                        }
                        return setAOTDeferredMemberResolutionError(error,
                            "complex default type",
                            psm.pending_complex_default_path.c_str(), "class",
                            qc->getName(), psm.name.c_str(), details.c_str());
                    }
                }
                psm.pending_complex_default_kind = -1;
                psm.pending_complex_buffer_init_kind = 0;
                psm.pending_complex_default_path.clear();
            }

            if (!resolveDeferredConstRefDefault(psm.pending_const_ref_path,
                    psm.default_val, getProgram(), "class", qc->getName(),
                    psm.name.c_str(), error)) {
                return false;
            }
            resolveDeferredExprTreeDefault(psm.pending_expr_tree_blob,
                psm.default_val, getProgram(), "class", qc->getName(),
                psm.name.c_str());
            if (!resolveDeferredNativeExprDefault(reader, psm.pending_expr_native_blob,
                    psm.default_val, getProgram(), "class", qc->getName(),
                    psm.name.c_str(), error)) {
                return false;
            }

            // Create the static variable info. The default value is
            // installed via assignInit() below (after construction) so the
            // serialized initial value survives AOT load. For static vars
            // whose init expression `needs_eval()`, an svar init function
            // was generated at compile time and will overwrite this value
            // at load time (see compileInitExpr / executeInitFunctions).
            QoreVarInfo* vi = new QoreVarInfo(&loc_builtin, ti, nullptr, QoreValue(),
                static_cast<ClassAccess>(psm.access));

            // Run parseInit() first so the QoreMemberInfoBaseAccess::init
            // flag is set and future parseInit() calls (e.g. from
            // qore_class_private::copy() during class merge) become no-ops.
            // Without this guard, the copy path would re-enter parseInit()
            // and call val.set(typeInfo) → reset() on our already-installed
            // value, silently losing it.
            vi->parseInit(psm.name.c_str());

            // Install the serialized initial value (if any). Takes
            // ownership of the ref held in psm.default_val.
            if (psm.default_val.hasNode() || psm.default_val.getType() != NT_NOTHING) {
                QoreValue v = psm.default_val;
                psm.default_val = QoreValue();  // transfer ownership
                vi->assignInit(v);
                vi->eval_init = true;
            }

            // Add to class's vars list
            priv->vars.addNoCheck(strdup(psm.name.c_str()), vi);

            printd(5, "AOT deser: added static member '%s' to class '%s'\n",
                psm.name.c_str(), qc->getName());
        }
    }

    // Clear pending data
    pending_static_members.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::registerClassConstantShells(std::string& error) {
    // Register class-constant shells now that types are resolved.  Value
    // blobs are intentionally not materialized here: in batch mode a class
    // constant in this fragment can reference a class constant or namespace
    // constant from a sibling fragment whose value has not been installed yet.
    auto resolve_type = [this](const PendingClassConstant& pcc,
            const QoreClass* qc) -> const QoreTypeInfo* {
        if (pcc.type_path.empty()) {
            return nullptr;
        }
        std::string type_error;
        const QoreTypeInfo* ti = type_resolver->resolve(pcc.type_path.c_str(), type_error);
        if (!type_error.empty()) {
            printd(2, "AOT deser: cannot resolve type '%s' for constant '%s' "
                "in class '%s': %s (falling back to auto)\n",
                pcc.type_path.c_str(), pcc.name.c_str(), qc->getName(), type_error.c_str());
            return autoTypeInfo;
        }
        return ti;
    };

    for (uint32_t i = 0; i < class_list.size() && i < pending_class_constants.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT class constant registration")) {
            error = "AOT class constant registration cancelled";
            return false;
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        qore_class_private* priv = qore_class_private::get(*qc);
        for (size_t j = 0; j < pending_class_constants[i].size(); ++j) {
            if (j && !(j % 100) && qore_check_cancel(nullptr, "AOT class constant registration")) {
                error = "AOT class constant registration cancelled";
                return false;
            }
            auto& pcc = pending_class_constants[i][j];
            const QoreTypeInfo* ti = resolve_type(pcc, qc);

            // Use addUserConstant to avoid setting sys=true on user classes
            priv->addUserConstant(pcc.name.c_str(), QoreValue(),
                static_cast<ClassAccess>(pcc.access), ti);

            ConstantEntry* ce = priv->constlist.findEntry(pcc.name.c_str());
            if (ce) {
                ce->loc = getBlobLocation();
                // Every class constant shell starts without its serialized value.
                // Keep it pending so sibling constants that refer forward do not
                // resolve to the placeholder value installed by addUserConstant().
                ce->aot_shell_pending = true;
                if (pcc.pending_init) {
                    // Pending init-func: parser-time references must defer to
                    // runtime.  Swap val for a self-referential
                    // RuntimeConstantRefNode; the init-func populates saved_val
                    // when it runs at register time.
                    ce->val.discard(nullptr);
                    ce->val = new RuntimeConstantRefNode(&loc_builtin, ce,
                        /*aot_deferred=*/true);
                }
            }

            printd(5, "AOT deser: added constant '%s' to class '%s'\n",
                pcc.name.c_str(), qc->getName());
        }
    }
    return true;
}

bool QoreAOTBinaryDeserializer::resolveClassConstantValues(std::string& error) {
    auto resolve_type = [this](const PendingClassConstant& pcc,
            const QoreClass* qc) -> const QoreTypeInfo* {
        if (pcc.type_path.empty()) {
            return nullptr;
        }
        std::string type_error;
        const QoreTypeInfo* ti = type_resolver->resolve(pcc.type_path.c_str(), type_error);
        if (!type_error.empty()) {
            printd(2, "AOT deser: cannot resolve type '%s' for constant '%s' "
                "in class '%s': %s (falling back to auto)\n",
                pcc.type_path.c_str(), pcc.name.c_str(), qc->getName(), type_error.c_str());
            return autoTypeInfo;
        }
        return ti;
    };

    auto apply_constant_type = [](const PendingClassConstant& pcc, const QoreClass* qc,
            const QoreTypeInfo* ti, QoreValue& value) {
        if (pcc.pending_init || !ti || !value.hasNode()
                || (value.getType() != NT_HASH && value.getType() != NT_LIST)) {
            return;
        }
        ExceptionSink xs;
        QoreTypeInfo::retypeValue(value, ti, &xs);
        if (xs.isException()) {
            xs.clear();
        }
        QoreTypeInfo::acceptInputMember(ti, pcc.name.c_str(), value, &xs);
        if (xs.isException()) {
            QoreValue e = xs.getExceptionErr();
            QoreValue d = xs.getExceptionDesc();
            QoreStringValueHelper es_str(e);
            QoreStringValueHelper ds_str(d);
            const char* es = e.getType() == NT_STRING
                ? es_str->c_str() : "(?err)";
            const char* ds = d.getType() == NT_STRING
                ? ds_str->c_str() : "(?desc)";
            printd(0, "AOT deser: class '%s' constant '%s' narrowing "
                "to '%s' failed: %s: %s\n",
                qc->getName(), pcc.name.c_str(),
                pcc.type_path.c_str(), es, ds);
            xs.clear();
        }
    };

    // Resolve class-constant value blobs after every class constant in every
    // batch fragment has a ConstantEntry and namespace constants have been
    // registered.  Nested VT_CONST_REF values can then become real values or
    // RuntimeConstantRefNode shells instead of failing during fragment open.
    for (uint32_t i = 0; i < class_list.size() && i < pending_class_constants.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT class constant value resolution")) {
            error = "AOT class constant value resolution cancelled";
            return false;
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        qore_class_private* priv = qore_class_private::get(*qc);
        for (size_t j = 0; j < pending_class_constants[i].size(); ++j) {
            if (j && !(j % 100) && qore_check_cancel(nullptr,
                    "AOT class constant value resolution")) {
                error = "AOT class constant value resolution cancelled";
                return false;
            }
            auto& pcc = pending_class_constants[i][j];
            if (pcc.pending_init) {
                continue;
            }

            ConstantEntry* ce = priv->constlist.findEntry(pcc.name.c_str());
            if (!ce) {
                error = "AOT cannot resolve deferred value for class constant '";
                error += qc->getName();
                error += "::";
                error += pcc.name;
                error += "': constant entry was not registered";
                return false;
            }

            const uint8_t* vptr = pcc.value_blob.data();
            const uint8_t* vend = vptr + pcc.value_blob.size();
            std::string value_error;
            struct ConstRefDeferGuard {
                const QoreAOTBinaryReader& r;
                bool prev;
                ConstRefDeferGuard(const QoreAOTBinaryReader& r_, bool newv)
                    : r(r_), prev(r_.defer_unresolved_const_refs) {
                    r_.defer_unresolved_const_refs = newv;
                }
                ~ConstRefDeferGuard() { r.defer_unresolved_const_refs = prev; }
            } defer_guard(reader, true);
            QoreValue resolved = reader.readValue(vptr, vend, value_error);
            if (!value_error.empty()) {
                resolved.discard(nullptr);
                error = "AOT cannot deserialize value for class constant '";
                error += qc->getName();
                error += "::";
                error += pcc.name;
                error += "': ";
                error += value_error;
                return false;
            }
            if (vptr != vend) {
                resolved.discard(nullptr);
                error = "AOT class constant value did not consume serialized payload for '";
                error += qc->getName();
                error += "::";
                error += pcc.name;
                error += "'";
                return false;
            }

            const QoreTypeInfo* ti = resolve_type(pcc, qc);
            apply_constant_type(pcc, qc, ti, resolved);
            ce->val.discard(nullptr);
            ce->val = resolved;
            ce->typeInfo = ti ? ti : resolved.getTypeInfo();
            ce->init = true;
            ce->aot_shell_pending = false;
            pcc.value_blob.clear();
        }
    }

    // Now that every class constant in the batch has a value, collapse any
    // nested deferred sibling references that had to remain lazy during the
    // first pass.  This preserves source semantics for constants such as a
    // folded hash referring to a later sibling constant.
    ExceptionSink xsink;
    for (uint32_t i = 0; i < class_list.size() && i < pending_class_constants.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT class constant materialization")) {
            error = "AOT class constant materialization cancelled";
            return false;
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        for (size_t j = 0; j < pending_class_constants[i].size(); ++j) {
            if (j && !(j % 100) && qore_check_cancel(nullptr, "AOT class constant materialization")) {
                error = "AOT class constant materialization cancelled";
                return false;
            }
            auto& pcc = pending_class_constants[i][j];
            ConstantEntry* ce = priv->constlist.findEntry(pcc.name.c_str());
            if (!ce || ce->aot_shell_pending) {
                continue;
            }
            ce->materializeRuntimeRefs(&xsink);
            if (xsink) {
                xsink.clear();
            }
        }
    }

    // Clear pending data
    pending_class_constants.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::resolveNamespaceConstants(std::string& error) {
    return deserializeConstants(error);
}

bool QoreAOTBinaryDeserializer::resolveClassConstants(std::string& error) {
    return registerClassConstantShells(error)
        && resolveNamespaceConstants(error)
        && resolveClassConstantValues(error);
}

bool QoreAOTBinaryDeserializer::resolveHashdeclMembers(std::string& error) {
    // Second pass: add hashdecl members now that all types exist
    for (auto& entry : pending_hashdecl_members) {
        TypedHashDecl* hd = entry.first;
        typed_hash_decl_private* hdp = typed_hash_decl_private::get(*hd);

        for (auto& phm : entry.second) {
            // Resolve deferred enum member default.  At hashdecl-read time
            // enums haven't been deserialized yet; now that deserializeEnums
            // has run, look up the enum member and materialise the value.
            if (!phm.pending_enum_path.empty()) {
                const QoreNamespace* pns = nullptr;
                const QoreEnumDecl* ed = getProgram()->findEnum(
                    phm.pending_enum_path.c_str(), pns);
                if (ed) {
                    const QoreEnumMember* member = ed->findMember(
                        phm.pending_enum_member.c_str());
                    if (member) {
                        phm.default_val = QoreValue::makeEnum(member);
                    } else {
                        std::string enum_member = phm.pending_enum_path + "::"
                            + phm.pending_enum_member;
                        return setAOTDeferredMemberResolutionError(error,
                            "enum member default", enum_member.c_str(), "hashdecl",
                            hd->getName(), phm.name.c_str());
                    }
                } else {
                    return setAOTDeferredMemberResolutionError(error,
                        "enum default", phm.pending_enum_path.c_str(), "hashdecl",
                        hd->getName(), phm.name.c_str());
                }
                phm.pending_enum_path.clear();
                phm.pending_enum_member.clear();
            }

            // Resolve deferred VT_NEW_OBJECT: build the ScopedObjectCallNode
            // for `Class(args)` defaults now that the class is registered.
            if (!phm.pending_new_class_path.empty()) {
                const QoreClass* target = resolveClassRefForSession(
                    phm.pending_new_class_path.c_str());
                phm.default_val = qoreAOTMakeObjectDefaultNode(getProgram(), target,
                    phm.pending_new_class_path, phm.pending_new_args);
                phm.pending_new_class_path.clear();
            }

            // Resolve deferred VT_NEW_COMPLEX_DEFAULT: build the type-default
            // node (`hash<X>()`, `list<X>()`, `hash<string, X>()`) now that
            // the referenced element/value type is registered.
            if (phm.pending_complex_default_kind >= 0) {
                QoreParseListNode* parse_args = nullptr;
                if (!phm.pending_complex_default_args.empty()) {
                    parse_args = new QoreParseListNode(&loc_builtin);
                    for (auto& a : phm.pending_complex_default_args) {
                        parse_args->add(a, &loc_builtin);
                    }
                    phm.pending_complex_default_args.clear();
                }
                if (phm.pending_complex_default_kind == 2) {
                    phm.default_val = qoreAOTMakeHashDeclDefaultNode(getProgram(),
                        phm.pending_complex_default_path, parse_args);
                } else {
                    std::string type_error;
                    const QoreTypeInfo* cti = type_resolver->resolve(
                        phm.pending_complex_default_path.c_str(), type_error);
                    if (cti && type_error.empty()) {
                        if (phm.pending_complex_default_kind == 0) {
                            QoreValue list_args;
                            if (parse_args) {
                                list_args = QoreValue(parse_args);
                            }
                            phm.default_val = QoreValue(new NewComplexListNode(
                                &loc_builtin, cti, list_args));
                        } else if (phm.pending_complex_default_kind == 3) {
                            QoreValue buffer_args;
                            if (parse_args) {
                                buffer_args = QoreValue(parse_args);
                            }
                            phm.default_val = QoreValue(new NewComplexBufferNode(
                                &loc_builtin, cti, buffer_args,
                                static_cast<QoreComplexBufferInitKind>(phm.pending_complex_buffer_init_kind)));
                        } else {
                            phm.default_val = QoreValue(new NewComplexHashNode(
                                &loc_builtin, cti, parse_args));
                        }
                    } else {
                        if (parse_args) {
                            parse_args->deref(nullptr);
                        }
                        std::string details = "complex default kind="
                            + std::to_string((int)phm.pending_complex_default_kind);
                        if (!type_error.empty()) {
                            details += "; ";
                            details += type_error;
                        }
                        return setAOTDeferredMemberResolutionError(error,
                            "complex default type",
                            phm.pending_complex_default_path.c_str(), "hashdecl",
                            hd->getName(), phm.name.c_str(), details.c_str());
                    }
                }
                phm.pending_complex_default_kind = -1;
                phm.pending_complex_buffer_init_kind = 0;
                phm.pending_complex_default_path.clear();
            }

            resolveDeferredExprTreeDefault(phm.pending_expr_tree_blob,
                phm.default_val, getProgram(), "hashdecl", hd->getName(),
                phm.name.c_str());
            if (!resolveDeferredNativeExprDefault(reader, phm.pending_expr_native_blob,
                    phm.default_val, getProgram(), "hashdecl", hd->getName(),
                    phm.name.c_str(), error, true)) {
                return false;
            }

            const QoreTypeInfo* mti = type_resolver->resolve(phm.type_path.c_str(), error);
            if (!error.empty()) {
                std::string type_error = error;
                return setAOTDeferredMemberResolutionError(error, "member type",
                    phm.type_path.c_str(), "hashdecl", hd->getName(),
                    phm.name.c_str(), type_error.c_str());
            }
            // Narrow the deserialized default into the member's declared type.
            // writeValue's NT_HASH / NT_LIST paths serialize by key/value or
            // element without preserving the source container's declared
            // element type — a typed hash like
            // `hash<string, hash<MapperRuntimeKeyInfo>> mapper_keys =
            // Mapper::MapperKeyInfo;` comes back as a plain hash<auto>.
            // Source-parse retains the declared typing via the AST's
            // `exp.getFullTypeInfo()`; AOT-load doesn't have that, so parse-time
            // folding of downstream `<DataProviderInfo>{...}` constants trips
            // the hash<auto>→typed-hash narrowing check in
            // `acceptInputComplexHash`.  Pre-narrow the default here via
            // `acceptInputMember` so the stored exp already carries the
            // member's declared type info.  This matches the effective shape
            // of source-parse without requiring a wire-format extension.
            if (mti && phm.default_val.hasNode()
                    && (phm.default_val.getType() == NT_HASH
                        || phm.default_val.getType() == NT_LIST)) {
                ExceptionSink xs;
                // Pre-retype nested containers recursively so inner values
                // carry the declared hashdecl/complex-hash typing before
                // `acceptInputMember` runs its narrowing check.
                QoreTypeInfo::retypeValue(phm.default_val, mti, &xs);
                if (xs.isException()) {
                    xs.clear();  // fall through to acceptInputMember for the
                                 // canonical diagnostic path
                }
                QoreTypeInfo::acceptInputMember(mti, phm.name.c_str(),
                    phm.default_val, &xs);
                if (xs.isException()) {
                    // Narrowing failed — keep the original value; the hashdecl
                    // instance will hit the same error later with the same
                    // diagnostic.  Don't swallow silently.
                    QoreValue e = xs.getExceptionErr();
                    QoreValue d = xs.getExceptionDesc();
                    QoreStringValueHelper es_str(e);
                    QoreStringValueHelper ds_str(d);
                    const char* es = e.getType() == NT_STRING
                        ? es_str->c_str() : "(?err)";
                    const char* ds = d.getType() == NT_STRING
                        ? ds_str->c_str() : "(?desc)";
                    printd(0, "AOT deser: hashdecl '%s' member '%s' default "
                        "narrowing to '%s' failed: %s: %s\n",
                        hd->getName(), phm.name.c_str(), phm.type_path.c_str(),
                        es, ds);
                    xs.clear();
                }
            }
            hdp->addMember(phm.name.c_str(), mti, phm.default_val);

            printd(5, "AOT deser: added member '%s' to hashdecl '%s'\n",
                phm.name.c_str(), hd->getName());
        }
    }

    for (auto& entry : pending_hashdecl_members) {
        TypedHashDecl* hd = entry.first;
        if (typed_hash_decl_private::get(*hd)->resolveParseParent()) {
            error = "failed to resolve parent hashdecl for '";
            error += hd->getName();
            error += "'";
            return false;
        }
    }

    // Clear pending data
    pending_hashdecl_members.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::resolveTypedefs(std::string& error) {
    // Multi-pass resolution to handle forward references between typedefs
    // Keep iterating until all are resolved or no progress is made
    while (!pending_typedefs.empty()) {
        size_t resolved_count = 0;
        std::vector<PendingTypedef> unresolved;

        for (auto& pt : pending_typedefs) {
            std::string temp_error;
            const QoreTypeInfo* ti = type_resolver->resolve(pt.type_path.c_str(), temp_error);
            if (temp_error.empty() && ti) {
                ns_list[pt.ns_idx]->typedefMap[pt.name.c_str()] =
                    new TypedefEntry(nullptr, ti, nullptr, pt.is_pub);
                ++resolved_count;
                printd(5, "AOT deser: created typedef '%s'\n", pt.name.c_str());
            } else {
                unresolved.push_back(std::move(pt));
            }
        }

        if (resolved_count == 0 && !unresolved.empty()) {
            // No progress - circular reference or genuinely missing type
            error = "cannot resolve type '" + unresolved[0].type_path +
                "' for typedef '" + unresolved[0].name + "'";
            pending_typedefs.clear();
            return false;
        }

        pending_typedefs = std::move(unresolved);
    }

    return true;
}

bool QoreAOTBinaryDeserializer::resolveEnumBaseTypes(std::string& error) {
    // Resolve enum base types now that typedefs are available
    for (auto& pebt : pending_enum_base_types) {
        const QoreTypeInfo* base_ti = type_resolver->resolve(pebt.base_type_path.c_str(), error);
        if (!error.empty()) {
            error = "cannot resolve base type '" + pebt.base_type_path +
                "' for enum '" + std::string(pebt.ed->getName()) + "': " + error;
            pending_enum_base_types.clear();
            return false;
        }
        if (base_ti) {
            qore_enum_decl_private::get(*pebt.ed)->setBaseTypeInfo(base_ti);
            printd(5, "AOT deser: set base type for enum '%s'\n", pebt.ed->getName());
        }
    }

    // Clear pending data
    pending_enum_base_types.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::deserializeHashDecls(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::HASHDECLS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid HASHDECLS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    // Two-pass approach: first create all hashdecls, then resolve parent pointers
    struct HashdeclInfo {
        TypedHashDecl* hd;
        std::string parent_path;
    };
    std::vector<HashdeclInfo> hashdecl_list;
    hashdecl_list.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* nspath = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        const char* parent_path = reader.readStringRef(ptr);
        std::vector<QoreGenericTypeParam> type_params;
        if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_HASHDECL_TYPE_PARAMS) != 0) {
            const bool has_type_param_defaults
                = (reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPE_PARAM_DEFAULTS) != 0;
            const bool has_type_param_bounds
                = (reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPE_PARAM_BOUNDS) != 0;
            uint16_t type_param_count = QoreAOTBinaryReader::readU16(ptr);
            type_params.reserve(type_param_count);
            for (uint16_t ti = 0; ti < type_param_count; ++ti) {
                if (ti && !(ti % 100) && qore_check_cancel(nullptr, "AOT hashdecl type parameter deserialization")) {
                    error = "operation cancelled during AOT hashdecl type parameter deserialization";
                    return false;
                }
                const char* type_param = reader.readStringRef(ptr);
                std::string type_param_name(type_param ? type_param : "");
                std::string default_type;
                if (has_type_param_defaults) {
                    uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
                    if (has_default) {
                        const char* default_type_str = reader.readStringRef(ptr);
                        default_type = default_type_str ? default_type_str : "";
                        if (default_type.empty()) {
                            error = "invalid empty default type for type parameter '" + type_param_name
                                + "' in hashdecl '" + std::string(name ? name : "(null)") + "'";
                            return false;
                        }
                    }
                }
                std::string bound_type;
                if (has_type_param_bounds) {
                    uint8_t has_bound = QoreAOTBinaryReader::readU8(ptr);
                    if (has_bound) {
                        const char* bound_type_str = reader.readStringRef(ptr);
                        bound_type = bound_type_str ? bound_type_str : "";
                        if (bound_type.empty()) {
                            error = "invalid empty bound type for type parameter '" + type_param_name
                                + "' in hashdecl '" + std::string(name ? name : "(null)") + "'";
                            return false;
                        }
                    }
                }
                type_params.emplace_back(std::move(type_param_name), std::move(default_type),
                    std::move(bound_type));
            }
        }

        // Read members first to collect info.  The default-value path uses
        // readDeferredMemberDefault so that VT_ENUM references — which the
        // generic readValue would try to resolve immediately — are deferred
        // to resolveHashdeclMembers.  Hashdecls are deserialized BEFORE
        // enums in openAndDeserializeShells; without the deferral a member
        // like `string http_version = HttpVersionMode::Auto;` in
        // `hashdecl HttpListenerInfo` fails with "enum not found" at load
        // time.
        // Local MemberInfo mirrors PendingHashdeclMember's deferred-resolution
        // fields so readDeferredMemberDefault instantiates against it.
        struct MemberInfo {
            std::string name;
            std::string type_path;
            QoreValue default_val;
            std::string pending_enum_path;
            std::string pending_enum_member;
            std::string pending_new_class_path;
            std::vector<QoreValue> pending_new_args;
            int8_t pending_complex_default_kind = -1;
            int8_t pending_complex_buffer_init_kind = 0;
            std::string pending_complex_default_path;
            std::vector<QoreValue> pending_complex_default_args;
            std::vector<uint8_t> pending_expr_tree_blob;
            std::string pending_const_ref_path;
            std::vector<uint8_t> pending_expr_native_blob;
        };
        std::vector<MemberInfo> members;

        uint32_t num_members = QoreAOTBinaryReader::readU32(ptr);
        members.reserve(num_members);
        // Enable RCR wrapping for VT_CONST_REF defaults so member initializers
        // like `hash<string, hash<MapperRuntimeKeyInfo>> mapper_keys =
        // Mapper::MapperKeyInfo;` preserve lazy-eval semantics and match
        // source-parse behaviour during parse-time folding of downstream
        // `<DataProviderInfo>{...}` constants.  See the VT_CONST_REF reader
        // branch in readValue for the mechanism.
        struct RcrWrapGuard {
            QoreAOTBinaryReader& r;
            bool prev;
            RcrWrapGuard(QoreAOTBinaryReader& r_, bool newv) : r(r_), prev(r_.wrap_const_ref_in_rcr) {
                r_.wrap_const_ref_in_rcr = newv;
            }
            ~RcrWrapGuard() { r.wrap_const_ref_in_rcr = prev; }
        } rcr_guard(reader, true);

        for (uint32_t j = 0; j < num_members; ++j) {
            MemberInfo mi;
            mi.name = reader.readStringRef(ptr);
            mi.type_path = reader.readStringRef(ptr);
            uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
            if (has_default) {
                if (!readDeferredMemberDefault(reader, ptr, end, error,
                        mi.default_val, mi)) {
                    error = "hashdecl '" + std::string(name ? name : "(null)")
                        + "' member '" + mi.name + "' default: " + error;
                    return false;
                }
            }
            members.push_back(std::move(mi));
        }

        // Validate namespace index
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            printd(2, "AOT: skipping hashdecl '%s' - invalid namespace index %u\n", name, ns_idx);
            for (auto& mi : members) {
                mi.default_val.discard(nullptr);
            }
            continue;
        }

        // Create the TypedHashDecl
        TypedHashDecl* hd = new TypedHashDecl(name, nspath);
        typed_hash_decl_private* hdp = typed_hash_decl_private::get(*hd);

        // Set visibility
        if (flags & 0x0001) {
            hdp->setPublic();
        }

        // Set namespace
        hdp->setNamespace(ns_list[ns_idx]);

        for (size_t ti = 0; ti < type_params.size(); ++ti) {
            if (ti && !(ti % 100) && qore_check_cancel(nullptr, "AOT hashdecl type parameter registration")) {
                error = "operation cancelled during AOT hashdecl type parameter registration";
                hdp->deref();
                for (auto& mi : members) {
                    mi.default_val.discard(nullptr);
                }
                return false;
            }
            hdp->addTypeParameter(type_params[ti].name.c_str(), type_params[ti].getDefaultType(),
                type_params[ti].getBoundType());
        }

        // Add to namespace's hashDeclList FIRST (before storing in pending list)
        if (ns_list[ns_idx]->hashDeclList.add(hd) != 0) {
            printd(2, "AOT: hashdecl '%s' already exists in namespace\n", name);
            hdp->deref();
            for (auto& mi : members) {
                mi.default_val.discard(nullptr);
            }
            continue;
        }

        // Update root namespace's thdmap so findHashDecl() works immediately
        {
            qore_program_private* pp_hd = qore_program_private::get(*pgm);
            qore_root_ns_private* rpriv = static_cast<qore_root_ns_private*>(
                qore_ns_private::get(*pp_hd->RootNS));
            rpriv->thdmap.update(hd->getName(), ns_list[ns_idx], hd);
        }

        // Store members for later resolution (after all hashdecls/enums/typedefs exist).
        // Carry the deferred-resolution fields (pending_enum_*, pending_new_*,
        // pending_complex_default_*) through to resolveHashdeclMembers.
        std::vector<PendingHashdeclMember> pending_members;
        pending_members.reserve(members.size());
        for (auto& mi : members) {
            PendingHashdeclMember phm;
            phm.name = std::move(mi.name);
            phm.type_path = std::move(mi.type_path);
            phm.default_val = mi.default_val;
            phm.pending_enum_path = std::move(mi.pending_enum_path);
            phm.pending_enum_member = std::move(mi.pending_enum_member);
            phm.pending_new_class_path = std::move(mi.pending_new_class_path);
            phm.pending_new_args = std::move(mi.pending_new_args);
            phm.pending_complex_default_kind = mi.pending_complex_default_kind;
            phm.pending_complex_buffer_init_kind = mi.pending_complex_buffer_init_kind;
            phm.pending_complex_default_path = std::move(mi.pending_complex_default_path);
            phm.pending_complex_default_args = std::move(mi.pending_complex_default_args);
            phm.pending_expr_tree_blob = std::move(mi.pending_expr_tree_blob);
            phm.pending_const_ref_path = std::move(mi.pending_const_ref_path);
            phm.pending_expr_native_blob = std::move(mi.pending_expr_native_blob);
            pending_members.push_back(std::move(phm));
        }
        pending_hashdecl_members.push_back({hd, std::move(pending_members)});

        // Store for parent resolution pass
        hashdecl_list.push_back({hd, parent_path ? parent_path : ""});
    }

    // Second pass: resolve parent hashdecl pointers
    const bool has_parameterized_hashdecl_parents
        = (reader.getHeader().feature_flags & QORE_AOT_FEAT_HASHDECL_PARAM_PARENTS) != 0;
    for (auto& hdi : hashdecl_list) {
        if (!hdi.parent_path.empty()) {
            if (has_parameterized_hashdecl_parents && strchr(hdi.parent_path.c_str(), '<')) {
                QoreParseTypeInfo* parent_pti = qore_parse_type_string_to_pti(hdi.parent_path.c_str());
                if (!parent_pti) {
                    error = "cannot parse parameterized parent hashdecl path '";
                    error += hdi.parent_path;
                    error += "' for hashdecl '";
                    error += hdi.hd->getName();
                    error += "'";
                    return false;
                }
                typed_hash_decl_private::get(*hdi.hd)->setParseParent(parent_pti);
                continue;
            }

            // Look up parent by path in the program
            qore_program_private* pp = qore_program_private::get(*pgm);
            qore_root_ns_private* rpriv = static_cast<qore_root_ns_private*>(
                qore_ns_private::get(*pp->RootNS));
            const qore_ns_private* found_ns = nullptr;
            const TypedHashDecl* parent = qore_root_ns_private::runtimeFindHashDecl(
                *rpriv->rns, hdi.parent_path.c_str(), found_ns);
            if (parent) {
                typed_hash_decl_private::get(*hdi.hd)->setParentHashDecl(parent);
            }
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeEnums(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::ENUMS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid ENUMS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* nspath = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        const char* base_type_path = reader.readStringRef(ptr);

        // Read members first to collect info
        struct EnumMemberInfo {
            std::string name;
            QoreValue val;
        };
        std::vector<EnumMemberInfo> members;

        uint32_t num_members = QoreAOTBinaryReader::readU32(ptr);
        members.reserve(num_members);
        for (uint32_t j = 0; j < num_members; ++j) {
            EnumMemberInfo emi;
            emi.name = reader.readStringRef(ptr);
            emi.val = reader.readValue(ptr, end, error);
            if (!error.empty()) {
                error = "enum '" + std::string(name ? name : "(null)") + "' member '" + emi.name + "': " + error;
                return false;
            }
            members.push_back(std::move(emi));
        }

        // Validate namespace index
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            printd(2, "AOT: skipping enum '%s' - invalid namespace index %u\n", name, ns_idx);
            continue;
        }

        // Create the QoreEnumDecl with default base type (will be resolved later if needed)
        QoreEnumDecl* ed = new QoreEnumDecl(name, nspath, bigIntTypeInfo);

        // Store base type path for later resolution if it's not the default
        if (base_type_path && *base_type_path) {
            PendingEnumBaseType pebt;
            pebt.ed = ed;
            pebt.base_type_path = base_type_path;
            pending_enum_base_types.push_back(std::move(pebt));
        }
        qore_enum_decl_private* edp = qore_enum_decl_private::get(*ed);

        // Set visibility
        if (flags & 0x0001) {
            edp->setPublic();
        }

        // Set namespace
        edp->setNamespace(ns_list[ns_idx]);

        // Add members
        for (auto& emi : members) {
            edp->addMember(emi.name.c_str(), emi.val);
        }

        // Add to namespace's enumList
        if (ns_list[ns_idx]->enumList.add(ed) != 0) {
            printd(2, "AOT: enum '%s' already exists in namespace\n", name);
            // Remove any pending base type entry that points to the deleted enum
            if (base_type_path && *base_type_path) {
                pending_enum_base_types.pop_back();
            }
            edp->deref();
            continue;
        }

        // Update root namespace's edmap so findEnum() works immediately during
        // the type-resolution phase, before the later full rebuildAllIndexes().
        {
            qore_program_private* pp_ed = qore_program_private::get(*pgm);
            qore_root_ns_private* rpriv = static_cast<qore_root_ns_private*>(
                qore_ns_private::get(*pp_ed->RootNS));
            rpriv->edmap.update(ed->getName(), ns_list[ns_idx], ed);
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeTypedefs(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::TYPEDEFS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid TYPEDEFS section data";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    // Store typedefs for later resolution (after all hashdecls/enums exist)
    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* type_path = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint8_t is_pub = QoreAOTBinaryReader::readU8(ptr);

        // Validate namespace index
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            error = "invalid namespace index " + std::to_string(ns_idx) +
                " for typedef '" + std::string(name ? name : "(null)") + "'";
            return false;
        }

        if (name && *name) {
            PendingTypedef pt;
            pt.name = name;
            pt.type_path = type_path ? type_path : "";
            pt.ns_idx = ns_idx;
            pt.is_pub = (is_pub != 0);
            pending_typedefs.push_back(std::move(pt));
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeConstants(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::CONSTANTS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid CONSTANTS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    const bool has_pending_flag =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_CONST_PENDING) != 0;

    struct PendingNamespaceConstant {
        std::string name;
        std::string type_path;
        uint32_t ns_idx;
        uint8_t access;
        uint8_t is_pub;
        uint8_t pending;
        const QoreTypeInfo* type_info = nullptr;
        std::vector<uint8_t> value_blob;
    };

    std::vector<PendingNamespaceConstant> pending_constants;
    pending_constants.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT namespace constant registration")) {
            error = "AOT namespace constant registration cancelled";
            return false;
        }
        const char* name = reader.readStringRef(ptr);
        const char* type_path = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint8_t access = QoreAOTBinaryReader::readU8(ptr);
        uint8_t is_pub = QoreAOTBinaryReader::readU8(ptr);
        uint8_t pending = has_pending_flag ? QoreAOTBinaryReader::readU8(ptr) : 0;

        std::vector<uint8_t> value_blob;
        if (!readDeferredClassConstantValue(reader, ptr, end, error, value_blob)) {
            error = "namespace constant '" + std::string(name ? name : "(null)") + "': " + error;
            return false;
        }
        if (!error.empty()) {
            error = "namespace constant '" + std::string(name ? name : "(null)") + "': " + error;
            return false;
        }

        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            error = "invalid namespace index " + std::to_string(ns_idx) +
                " for constant '" + std::string(name ? name : "(null)") + "'";
            return false;
        }
        if (!name || !*name) {
            error = "invalid empty name for namespace constant";
            return false;
        }

        // Skip if constant already exists (from dependency module).
        {
            const QoreTypeInfo* existing_ti = nullptr;
            bool found = false;
            ns_list[ns_idx]->constant.find(name, existing_ti, found);
            if (found) {
                printd(2, "AOT: skipping constant '%s' - already exists (from dependency)\n", name);
                continue;
            }
        }

        const QoreTypeInfo* ti = type_resolver->resolve(type_path, error);
        if (!error.empty()) {
            printd(0, "AOT: failed to resolve type '%s' for const '%s': %s\n",
                type_path ? type_path : "(null)", name ? name : "(null)", error.c_str());
            error = "cannot resolve type '" + std::string(type_path ? type_path : "(null)") +
                "' for constant '" + std::string(name) + "': " + error;
            return false;
        }

        // Register a shell before deserializing any namespace constant value.
        // Folded container literals can contain VT_CONST_REF entries pointing
        // to sibling namespace constants from the same module, and the
        // ConstantList iteration order is not a dependency order.
        ConstantEntry* ce = new ConstantEntry(getBlobLocation(), name, QoreValue(),
            ti, is_pub != 0, true, false, static_cast<ClassAccess>(access));
        ns_list[ns_idx]->constant.addEntry(name, ce);

        if (pending) {
            // Pending init-func: parser-time references to this constant must
            // defer to runtime until the init-func populates saved_val.
            ce->aot_shell_pending = true;
            ce->val.discard(nullptr);
            ce->val = new RuntimeConstantRefNode(&loc_builtin, ce,
                /*aot_deferred=*/true);
        }

        pending_constants.push_back({name, type_path ? type_path : "", ns_idx,
            access, is_pub, pending, ti, std::move(value_blob)});
    }

    rebuildAOTRootIndexes(pgm);

    for (size_t i = 0; i < pending_constants.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT namespace constant value resolution")) {
            error = "AOT namespace constant value resolution cancelled";
            return false;
        }
        auto& pc = pending_constants[i];
        if (pc.pending) {
            continue;
        }

        ConstantEntry* ce = ns_list[pc.ns_idx]->constant.findEntry(pc.name.c_str());
        if (!ce) {
            error = "AOT cannot resolve deferred value for namespace constant '";
            error += pc.name;
            error += "': constant entry was not registered";
            return false;
        }

        const uint8_t* vptr = pc.value_blob.data();
        const uint8_t* vend = vptr + pc.value_blob.size();
        std::string value_error;
        struct ConstRefDeferGuard {
            const QoreAOTBinaryReader& r;
            bool prev;
            ConstRefDeferGuard(const QoreAOTBinaryReader& r_, bool newv)
                : r(r_), prev(r_.defer_unresolved_const_refs) {
                r_.defer_unresolved_const_refs = newv;
            }
            ~ConstRefDeferGuard() { r.defer_unresolved_const_refs = prev; }
        } defer_guard(reader, true);
        QoreValue val = reader.readValue(vptr, vend, value_error);
        if (!value_error.empty()) {
            val.discard(nullptr);
            error = "namespace constant '" + pc.name + "': " + value_error;
            return false;
        }
        if (vptr != vend) {
            val.discard(nullptr);
            error = "AOT namespace constant value did not consume serialized payload for '";
            error += pc.name;
            error += "'";
            return false;
        }

        const QoreTypeInfo* final_ti = pc.type_info ? pc.type_info : val.getTypeInfo();
        if (final_ti && val.hasNode()
                && (val.getType() == NT_HASH || val.getType() == NT_LIST)) {
            ExceptionSink xs;
            QoreTypeInfo::retypeValue(val, final_ti, &xs);
            if (xs.isException()) {
                xs.clear();
            }
            QoreTypeInfo::acceptInputMember(final_ti, pc.name.c_str(), val, &xs);
            if (xs.isException()) {
                QoreValue e = xs.getExceptionErr();
                QoreValue d = xs.getExceptionDesc();
                QoreStringValueHelper es_str(e);
                QoreStringValueHelper ds_str(d);
                const char* es = e.getType() == NT_STRING
                    ? es_str->c_str() : "(?err)";
                const char* ds = d.getType() == NT_STRING
                    ? ds_str->c_str() : "(?desc)";
                printd(0, "AOT deser: namespace constant '%s' narrowing "
                    "to '%s' failed: %s: %s\n",
                    pc.name.c_str(), pc.type_path.c_str(), es, ds);
                xs.clear();
            }
        }

        ce->val.discard(nullptr);
        ce->val = val;
        ce->typeInfo = final_ti;
        pc.value_blob.clear();
    }

    ExceptionSink xsink;
    for (size_t i = 0; i < pending_constants.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT namespace constant materialization")) {
            error = "AOT namespace constant materialization cancelled";
            return false;
        }
        auto& pc = pending_constants[i];
        if (pc.pending) {
            continue;
        }
        ConstantEntry* ce = ns_list[pc.ns_idx]->constant.findEntry(pc.name.c_str());
        if (!ce || ce->aot_shell_pending) {
            continue;
        }
        ce->materializeRuntimeRefs(&xsink);
        if (xsink) {
            xsink.clear();
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeGlobals(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::GLOBALS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid GLOBALS section data";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* type_path = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint8_t is_thread_local = QoreAOTBinaryReader::readU8(ptr);
        uint8_t is_pub = QoreAOTBinaryReader::readU8(ptr);

        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            error = "invalid namespace index " + std::to_string(ns_idx) +
                " for global variable '" + std::string(name ? name : "(null)") + "'";
            return false;
        }
        if (!name || !*name) {
            error = "invalid empty name for global variable";
            return false;
        }

        Var* existing_var = ns_list[ns_idx]->var_list.runtimeFindVar(name);
        if (existing_var) {
            if (existing_var->isImported()) {
                printd(5, "AOT deser: reusing imported global var '%s' instead of creating local copy\n",
                    name);
                continue;
            }
            error = "duplicate global variable '" + std::string(name) + "'";
            return false;
        }

        const QoreTypeInfo* ti = type_resolver->resolve(type_path, error);
        if (!error.empty()) {
            error = "cannot resolve type '" + std::string(type_path ? type_path : "(null)") +
                "' for global variable '" + std::string(name) + "': " + error;
            return false;
        }

        // Create the global variable directly
        Var* var = new Var(getBlobLocation(), name, ti, false,
            is_thread_local != 0);
        if (is_pub) {
            var->setPublic();
        }
        ns_list[ns_idx]->var_list.vmap[var->getName()] = var;

        printd(5, "AOT deser: created global var '%s' (type=%s, thread_local=%d)\n",
            name, type_path, is_thread_local);
    }

    return true;
}

static bool skipAOTVariantDefaultPayload(const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end, uint8_t has_default,
        std::string& error) {
    switch (has_default) {
        case 0:
            return true;

        case 1:
            return skipAOTSerializedValue(reader, ptr, end, error);

        case 2:
        case 3:
            reader.readStringRef(ptr);
            return true;

        case 4:
        case 6:
            reader.readStringRef(ptr);
            reader.readStringRef(ptr);
            return true;

        case 5: {
            reader.readStringRef(ptr);
            (void)QoreAOTBinaryReader::readU8(ptr);  // or_nothing
            uint8_t has_inner = QoreAOTBinaryReader::readU8(ptr);
            return !has_inner || skipAOTSerializedValue(reader, ptr, end, error);
        }
    }

    error = "invalid AOT variant default marker: " + std::to_string(has_default);
    return false;
}

static bool skipAOTVariantSignature(const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end, bool uses_type_table,
        std::string& error) {
    // Must match readAndSetupVariantSignature().
    if (uses_type_table) {
        (void)QoreAOTBinaryReader::readU32(ptr);
    } else {
        reader.readStringRef(ptr);
    }

    uint32_t num_params = QoreAOTBinaryReader::readU32(ptr);
    QoreAOTBinaryReader::readU16(ptr);

    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_SIG_LINES) != 0) {
        QoreAOTBinaryReader::readU16(ptr);
        QoreAOTBinaryReader::readU16(ptr);
    }
    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_ENTRY_STMT_LINES) != 0) {
        QoreAOTBinaryReader::readU16(ptr);
        QoreAOTBinaryReader::readU16(ptr);
    }
    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_VARIANT_PARSE_OPTIONS) != 0) {
        QoreAOTBinaryReader::readI64(ptr);
        QoreAOTBinaryReader::readI64(ptr);
    }

    for (uint32_t p = 0; p < num_params; ++p) {
        if (p && !(p % 100) && qore_check_cancel(nullptr, "AOT variant signature skip")) {
            error = "operation cancelled during AOT variant signature skip";
            return false;
        }
        reader.readStringRef(ptr);
        if (uses_type_table) {
            (void)QoreAOTBinaryReader::readU32(ptr);
        } else {
            reader.readStringRef(ptr);
        }
        if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_READONLY_LOCALS) != 0) {
            (void)QoreAOTBinaryReader::readU8(ptr);
        }
        uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
        if (!skipAOTVariantDefaultPayload(reader, ptr, end, has_default, error)) {
            return false;
        }
    }

    return true;
}

//! Helper: read a variant signature and set up the UserSignature from AOT metadata
static bool readAndSetupVariantSignature(
        const QoreAOTBinaryReader& reader,
        QoreAOTTypeResolver* type_resolver,
        QoreProgram* pgm,
        const uint8_t*& ptr, const uint8_t* end,
        UserVariantBase* uvb,
        bool& sig_has_ellipsis,
        bool& needs_extra_args_flag,
        int16_t& entry_first_line,
        int16_t& entry_last_line,
        QoreParseOptions& variant_parse_options,
        std::string& error,
        const QoreClass* classTypeInfo = nullptr,
        const std::vector<const QoreTypeInfo*>* type_table = nullptr) {
    // Return type — when a per-blob type table is provided, the
    // serialized form is a `u32` index into it; otherwise fall back
    // to the legacy inline string + per-lookup resolve path.
    const QoreTypeInfo* ret_ti_preresolved = nullptr;
    const char* ret_type_path = nullptr;
    if (type_table) {
        uint32_t idx = QoreAOTBinaryReader::readU32(ptr);
        if (idx < type_table->size()) {
            ret_ti_preresolved = (*type_table)[idx];
        }
    } else {
        ret_type_path = reader.readStringRef(ptr);
    }

    // num params
    uint32_t np = QoreAOTBinaryReader::readU32(ptr);

    // flags: see writeVariantSignature for the bit layout
    //   bit  0 = effective varargs (v->hasVarargs())
    //   bit  1 = is_user
    //   bit  2 = signature literally has `...` (sig->hasVarargs())
    //   bit 15 = new-format marker — bits 2+ are meaningful
    //
    // Pre-marker qmods used bit 0 as the OR of both concepts, and
    // setting both sig->varargs and QCF_USES_EXTRA_ARGS from it
    // spuriously inflated concrete variants' signatures with `...`
    // when the body referenced $argv/$N.  New format splits them.
    uint16_t sig_flags = QoreAOTBinaryReader::readU16(ptr);
    bool new_format = (sig_flags & 0x8000) != 0;
    bool bit0 = (sig_flags & 0x0001) != 0;
    bool bit2 = (sig_flags & 0x0004) != 0;
    if (new_format) {
        // bit 2 tells us precisely whether the signature had `...`;
        // bit 0 - bit 2 is the QCF_USES_EXTRA_ARGS flag alone.
        sig_has_ellipsis = bit2;
        needs_extra_args_flag = bit0;
    } else {
        // Old format: bit 0 conflates; preserve pre-fix behavior for
        // unrebuilt qmods so existing `sub zip(){...argv...}` callers
        // don't regress.  The concrete-abstract mismatch for modules
        // with $argv-in-concrete-override remains until they're rebuilt.
        sig_has_ellipsis = bit0;
        needs_extra_args_flag = bit0;
    }

    // Per-variant signature start/end lines — present iff the blob
    // advertises QORE_AOT_FEAT_SIG_LINES.  Older blobs (pre-feat) don't
    // have these 4 bytes and continue to report line 0.
    int16_t sig_first_line = 0;
    int16_t sig_last_line  = 0;
    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_SIG_LINES) != 0) {
        sig_first_line = static_cast<int16_t>(QoreAOTBinaryReader::readU16(ptr));
        sig_last_line  = static_cast<int16_t>(QoreAOTBinaryReader::readU16(ptr));
    }
    entry_first_line = 0;
    entry_last_line = 0;
    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_ENTRY_STMT_LINES) != 0) {
        entry_first_line = static_cast<int16_t>(QoreAOTBinaryReader::readU16(ptr));
        entry_last_line  = static_cast<int16_t>(QoreAOTBinaryReader::readU16(ptr));
    }
    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_VARIANT_PARSE_OPTIONS) != 0) {
        int64_t po_lo = QoreAOTBinaryReader::readI64(ptr);
        int64_t po_hi = QoreAOTBinaryReader::readI64(ptr);
        variant_parse_options = QoreParseOptions(po_lo, po_hi);
    } else {
        variant_parse_options = pgm ? pgm->getParseOptions() : QoreParseOptions();
    }

    // Read params — reserve+emplace rather than resize+assign so we
    // skip the up-front default-construction of `np` empty strings per
    // variant (~3.3 M skipped default-constructions in qwf batch).
    std::vector<std::string> param_names;
    std::vector<const QoreTypeInfo*> param_types;
    std::vector<QoreValue> param_defaults;
    std::vector<uint8_t> param_flags;
    param_names.reserve(np);
    param_types.reserve(np);
    param_defaults.resize(np);  // sparse by has_default — keep indexed
    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_READONLY_LOCALS) != 0) {
        param_flags.reserve(np);
    }

    for (uint32_t j = 0; j < np; ++j) {
        const char* pname = reader.readStringRef(ptr);
        const char* ptype_path = nullptr;  // only populated on the legacy path
        const QoreTypeInfo* pti = nullptr;
        if (type_table) {
            uint32_t idx = QoreAOTBinaryReader::readU32(ptr);
            if (idx < type_table->size()) {
                pti = (*type_table)[idx];
            }
        } else {
            ptype_path = reader.readStringRef(ptr);
        }
        uint8_t pflags = 0;
        if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_READONLY_LOCALS) != 0) {
            pflags = QoreAOTBinaryReader::readU8(ptr);
        }
        uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);

        param_names.emplace_back(pname ? pname : "");

        if (!type_table) {
            pti = type_resolver->resolve(ptype_path, error);
            if (!error.empty()) {
                // Fall back to auto type when the type can't be resolved
                // (e.g., module-private types filtered from metadata).
                printd(0, "AOT: cannot resolve type '%s' for parameter '%s': %s "
                    "(falling back to auto)\n",
                    ptype_path ? ptype_path : "(null)", param_names.back().c_str(), error.c_str());
                error.clear();
                pti = autoTypeInfo;
            }
        }
        param_types.push_back(pti);
        if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_READONLY_LOCALS) != 0) {
            param_flags.push_back(pflags);
        }

        if (has_default == 1) {
            // Constant default value
            param_defaults[j] = reader.readValue(ptr, end, error);
            if (!error.empty()) {
                // Clean up already-read defaults
                for (uint32_t k = 0; k < j; ++k) {
                    param_defaults[k].discard(nullptr);
                }
                return false;
            }
        } else if (has_default == 2) {
            // Expression default: no-arg function call (e.g., getcwd())
            const char* fname = reader.readStringRef(ptr);
            if (fname && *fname) {
                qore_program_private* pp = qore_program_private::get(*pgm);
                const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
                    *pp->RootNS, fname);
                if (fe) {
                    FunctionCallNode* fcn = new FunctionCallNode(
                        &loc_builtin, fe, static_cast<QoreParseListNode*>(nullptr));
                    param_defaults[j] = QoreValue(fcn);
                } else {
                    printd(0, "AOT deser: cannot resolve default expression function '%s'\n",
                        fname);
                    param_defaults[j] = QoreValue(true);
                }
            } else {
                param_defaults[j] = QoreValue(true);
            }
        } else if (has_default == 3) {
            // Expression default: plain constant reference.
            // Build a RuntimeConstantRefNode pointing at the resolved entry
            // so later evaluation returns the current value of the constant.
            const char* cfqn = reader.readStringRef(ptr);
            ConstantEntry* ce = aot_resolve_constant_by_fqn(pgm, cfqn);
            if (ce) {
                param_defaults[j] = QoreValue(new RuntimeConstantRefNode(&loc_builtin, ce,
                    /*aot_deferred=*/true));
            } else {
                printd(0, "AOT deser: cannot resolve default const ref '%s'\n",
                    cfqn ? cfqn : "(null)");
                param_defaults[j] = QoreValue(true);
            }
        } else if (has_default == 4) {
            // Expression default: no-arg method call on a constant
            //   e.g. `AutoHashType.getName()`.
            // Rebuild the AST as a QoreDotEvalOperatorNode whose `left` is a
            // freshly-constructed RuntimeConstantRefNode wrapping the
            // referenced ConstantEntry, and whose method call has a dynamic
            // lookup by name (qc/method left null) — the runtime dispatch
            // path in AbstractMethodCallNode::exec handles this case.
            const char* cfqn = reader.readStringRef(ptr);
            const char* mname = reader.readStringRef(ptr);
            ConstantEntry* ce = aot_resolve_constant_by_fqn(pgm, cfqn);
            if (ce && mname && *mname) {
                auto* rcr = new RuntimeConstantRefNode(&loc_builtin, ce,
                    /*aot_deferred=*/true);
                auto* mc = new MethodCallNode(&loc_builtin, strdup(mname),
                    (QoreParseListNode*)nullptr);
                auto* de = new QoreDotEvalOperatorNode(&loc_builtin, QoreValue(rcr), mc);
                param_defaults[j] = QoreValue(de);
            } else {
                printd(0, "AOT deser: cannot resolve default dot-eval const '%s'.%s()\n",
                    cfqn ? cfqn : "(null)", mname ? mname : "(null)");
                param_defaults[j] = QoreValue(true);
            }
        } else if (has_default == 6) {
            // Expression default: no-arg static method call,
            //   e.g. `string boundary = MultiPartMessage::getBoundary()`.
            // Resolve the class by path, then locate the static method
            // and wrap it in a StaticMethodCallNode. Evaluation goes
            // through the normal AbstractFunctionCallNode dispatch.
            const char* class_path = reader.readStringRef(ptr);
            const char* mname = reader.readStringRef(ptr);
            const QoreClass* qc = (class_path && *class_path)
                ? qoreAOTResolveClassRefForDeserialization(pgm, class_path)
                : nullptr;
            const QoreMethod* m = nullptr;
            if (qc && mname && *mname) {
                m = qc->findStaticMethod(mname);
                if (!m) {
                    qore_class_private* qcp = qore_class_private::get(
                        *const_cast<QoreClass*>(qc));
                    m = qcp->parseFindLocalStaticMethod(mname);
                }
            }
            if (m) {
                param_defaults[j] = QoreValue(new StaticMethodCallNode(
                    &loc_builtin, m, (QoreParseListNode*)nullptr));
            } else {
                // Method not yet committed (likely a default referencing a
                // static method of the same or a still-pending class).
                // Defer: store class_path + method_name on the signature
                // slot for post-commit fixup by resolveDeferredStaticMethodDefaults.
                if (g_aot_pending_static_method_defaults) {
                    PendingStaticMethodDefault pd;
                    pd.class_path = class_path ? class_path : "";
                    pd.method_name = mname ? mname : "";
                    pd.uvb = uvb;
                    pd.param_index = j;
                    g_aot_pending_static_method_defaults->push_back(pd);
                    // Use a non-NOTHING placeholder so hasDefaultArg(j)
                    // reports true and min_param_types counts this param as
                    // optional. The fixup pass after commitDeserializedClasses
                    // replaces this with the resolved StaticMethodCallNode
                    // before any call can execute.
                    param_defaults[j] = QoreValue(true);
                } else {
                    error = "cannot resolve default static method '";
                    error += qoreAOTDescribeClassRef(class_path);
                    error += "::";
                    error += mname ? mname : "(null)";
                    error += "()' and no deferred-defaults context is active";
                    for (uint32_t k = 0; k < j; ++k) {
                        param_defaults[k].discard(nullptr);
                    }
                    return false;
                }
            }
        } else if (has_default == 5) {
            // Expression default: hashdecl typed-hash literal, e.g.
            //   `hash<AuthCodeInfo> info = <AuthCodeInfo>{}`.
            // Reader resolves the hashdecl by namespace path and builds a
            // QoreHashDeclCastOperatorNode wrapping a (possibly empty) inner
            // hash. At call time prepareDefaultArgs evaluates this node via
            // typed_hash_decl_private::newHash, producing an all-defaults
            // hashdecl instance.
            const char* hd_path = reader.readStringRef(ptr);
            uint8_t or_nothing = QoreAOTBinaryReader::readU8(ptr);
            uint8_t has_inner = QoreAOTBinaryReader::readU8(ptr);
            QoreValue inner;
            if (has_inner) {
                inner = reader.readValue(ptr, end, error);
                if (!error.empty()) {
                    for (uint32_t k = 0; k < j; ++k) {
                        param_defaults[k].discard(nullptr);
                    }
                    return false;
                }
            }
            const TypedHashDecl* hd = qore_aot_resolve_hashdecl_path(pgm, hd_path);
            if (!hd && (!hd_path || !*hd_path || !strcmp(hd_path, "hash"))) {
                printd(0, "AOT deser: cannot resolve hashdecl '%s' for default value\n",
                    hd_path ? hd_path : "(null)");
                inner.discard(nullptr);
                param_defaults[j] = QoreValue(true);
            } else {
                // If inner was NOTHING, supply an empty hash so the cast has a
                // concrete value to operate on (matching the parser's behavior
                // for `<X>{}` which folds the empty body to QoreHashNode{}).
                if (!inner.hasNode()) {
                    inner = QoreValue(new QoreHashNode(autoTypeInfo));
                }
                auto* nd = hd
                    ? new QoreHashDeclCastOperatorNode(&loc_builtin, hd, inner, or_nothing != 0)
                    : new QoreHashDeclCastOperatorNode(&loc_builtin, hd_path, inner, or_nothing != 0);
                param_defaults[j] = QoreValue(nd);
            }
        }
    }

    // Resolve return type — type-table path already pre-resolved at
    // phase 2b entry (see resolveTypeTable); legacy path still does
    // per-variant hash lookup here.
    const QoreTypeInfo* ret_ti;
    if (type_table) {
        ret_ti = ret_ti_preresolved;
    } else {
        ret_ti = type_resolver->resolve(ret_type_path, error);
        if (!error.empty()) {
            printd(2, "AOT deser: cannot resolve return type '%s': %s (falling back to auto)\n",
                ret_type_path ? ret_type_path : "(null)", error.c_str());
            error.clear();
            ret_ti = autoTypeInfo;
        }
    }

    // Split timing: param-read loop above vs setup call below
    static auto now_us_fn = [] () -> uint64_t {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
    };
    static const bool time_on_sub = getenv("QORE_AOT_PHASE_TIMING") != nullptr;
    uint64_t t_setup0 = time_on_sub ? now_us_fn() : 0;

    // Set up the variant's signature from metadata.  Only signature-
    // level ellipsis (`...`) flows into signature.varargs; the
    // QCF_USES_EXTRA_ARGS flag alone does NOT inflate the signature.
    //
    // Plumb the binary's label (the module's .qm source path) as the
    // variant's parse-location file so runtime errors that override the
    // exception location via `sig->getParseLocation()` (e.g.
    // block-missing-return) report the declaring source instead of the
    // empty-file/line-0 default.
    UserSignature* sig = uvb->getUserSignature();
    sig->setupFromAOTMetadata(pgm, ret_ti,
        std::move(param_names), std::move(param_types), std::move(param_defaults),
        sig_has_ellipsis, classTypeInfo, reader.getLabel(),
        sig_first_line, sig_last_line, std::move(param_flags));

    if (time_on_sub) {
        g_aot_dm_sig_setup_us += now_us_fn() - t_setup0;
    }

    return true;
}

static void attachAOTEntryStatementBlock(QoreProgram* pgm, UserVariantBase* uvb,
        int16_t entry_first_line, int16_t entry_last_line,
        const QoreParseOptions& variant_parse_options) {
    assert(pgm);
    assert(uvb);

    qore_program_private* pp = qore_program_private::get(*pgm);
    StatementBlock* entry = nullptr;
    {
        AutoLocker al(&pp->plock);

        const QoreProgramLocation* loc = nullptr;
        if (UserSignature* sig = uvb->getUserSignature()) {
            loc = sig->getParseLocation();
            if (loc && (entry_first_line || entry_last_line)) {
                loc = pp->getLocation(*loc, entry_first_line, entry_last_line);
            }
        }
        if (!loc) {
            loc = pp->getLocation(entry_first_line, entry_last_line);
        }

        entry = new StatementBlock(pp, loc);
        entry->pwo.parse_options = variant_parse_options;
        // Function-entry metadata is resolved through findFunctionStatementId().
        // Do not add it to the file/line index, or findStatementId() can
        // resolve the first executable statement line to the function entry.
        qore_program_private::registerStatement(pgm, entry, false);
    }

    uvb->setAOTEntryStatementBlock(entry);
}

bool QoreAOTBinaryDeserializer::deserializeFunctions(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::FUNCTIONS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid FUNCTIONS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        uint32_t num_variants = QoreAOTBinaryReader::readU32(ptr);

        if (!name || !*name || ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            error = "invalid function entry";
            return false;
        }

        // Skip if function already exists (from dependency module)
        if (ns_list[ns_idx]->func_list.findNode(name)) {
            printd(2, "AOT: skipping function '%s' - already exists (from dependency)\n", name);
            // Skip reading variants (must match exact format of readAndSetupVariantSignature)
            for (uint32_t v = 0; v < num_variants; ++v) {
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_METHOD_SYNC) != 0) {
                    QoreAOTBinaryReader::readU8(ptr);  // variant flags
                }
                if (!skipAOTVariantSignature(reader, ptr, end, uses_type_table, error)) {
                    return false;
                }
            }
            continue;
        }

        // Create the QoreFunction
        QoreFunction* func = new QoreFunction(name);

        for (uint32_t v = 0; v < num_variants; ++v) {
            uint8_t vflags = 0;
            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_METHOD_SYNC) != 0) {
                vflags = QoreAOTBinaryReader::readU8(ptr);
            }
            // Create an empty UserFunctionVariant (no body, no params)
            UserFunctionVariant* ufv = new UserFunctionVariant(
                nullptr, 0, 0, QoreValue(), nullptr, (vflags & 0x01) != 0);

            bool sig_has_ellipsis = false;
            bool needs_extra_args_flag = false;
            int16_t entry_first_line = 0;
            int16_t entry_last_line = 0;
            QoreParseOptions variant_parse_options;
            const std::vector<const QoreTypeInfo*>* tt =
                uses_type_table ? &type_table_resolved : nullptr;
            if (!readAndSetupVariantSignature(reader, type_resolver, pgm, ptr, end,
                    ufv, sig_has_ellipsis, needs_extra_args_flag, entry_first_line,
                    entry_last_line, variant_parse_options, error, nullptr, tt)) {
                // variant ownership transfers to addPendingVariant or cleanup
                ufv->deref();
                // function can't be deleted directly; add it to namespace empty
                ns_list[ns_idx]->func_list.add(func, ns_list[ns_idx]);
                return false;
            }
            attachAOTEntryStatementBlock(pgm, ufv, entry_first_line, entry_last_line,
                variant_parse_options);

            // Set QCF_USES_EXTRA_ARGS on the variant.  This flag is
            // independent of signature.varargs (which was already set
            // from sig_has_ellipsis inside readAndSetupVariantSignature):
            // it marks bodies that reference `$argv`/`$N` even when the
            // declared signature has no ellipsis, so overload resolution
            // can still route callers passing extra args here.
            if (needs_extra_args_flag) {
                ufv->setFlag(QCF_USES_EXTRA_ARGS);
            }

            if (flags & 0x0001) {
                ufv->setModulePublic();
            }

            // Add variant to function via the parse-time API, then commit
            func->addPendingVariant(ufv);
        }

        // Commit all pending variants to the committed list
        func->parseCommit();

        // Add function to namespace
        ns_list[ns_idx]->func_list.add(func, ns_list[ns_idx]);

        printd(5, "AOT deser: created function '%s' with %d variant(s)\n", name, num_variants);
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeMethods(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::METHODS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid METHODS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    // Fine-grained sub-timing for the per-variant inner loop.
    // Gated by QORE_AOT_PHASE_TIMING; totals across all sessions
    // go into globals and are printed in the MultiDeserializer's
    // destructor.
    const bool time_on = getenv("QORE_AOT_PHASE_TIMING") != nullptr;
    auto now_us = [] () -> uint64_t {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
    };
    uint64_t local_alloc_us = 0, local_sig_us = 0, local_add_us = 0;
    uint64_t local_variants = 0;
    bool has_slot_map_section = false;
    std::unordered_set<std::string> slot_map_names;
    if (!collectAOTSlotMapFunctionNames(reader, slot_map_names, has_slot_map_section, error)) {
        return false;
    }
    const bool has_const_methods = (reader.getHeader().feature_flags & QORE_AOT_FEAT_CONST_METHODS) != 0;

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t class_idx = QoreAOTBinaryReader::readU32(ptr);
        const char* method_name = reader.readStringRef(ptr);
        uint8_t is_static = QoreAOTBinaryReader::readU8(ptr);
        uint32_t num_variants = QoreAOTBinaryReader::readU32(ptr);

        if (class_idx >= class_list.size() || !class_list[class_idx]) {
            error = "invalid class index for method '" + std::string(method_name ? method_name : "") + "'";
            return false;
        }

        QoreClass* qc = class_list[class_idx];
        bool skip_class = preexisting_classes.count(class_idx) > 0;

        for (uint32_t v = 0; v < num_variants; ++v) {
            // Read method-specific fields: access + flags
            uint8_t access = QoreAOTBinaryReader::readU8(ptr);
            uint8_t mflags = QoreAOTBinaryReader::readU8(ptr);
            bool is_final = (mflags & 0x01) != 0;
            bool is_abstract = (mflags & 0x02) != 0;
            bool is_synchronized = (mflags & 0x04) != 0;
            if ((mflags & 0x08) != 0 && !has_const_methods) {
                error = "AOT method '";
                error += qc->getName();
                error += "::";
                error += method_name ? method_name : "(null)";
                error += "' has const-method metadata without QORE_AOT_FEAT_CONST_METHODS";
                return false;
            }
            bool is_const_method = has_const_methods && (mflags & 0x08) != 0;

            // Create the correct variant type for special methods:
            // constructor → UserConstructorVariant, destructor → UserDestructorVariant,
            // copy → UserCopyVariant, everything else → UserMethodVariant.
            // This is critical because the runtime dispatches through type-specific
            // virtual methods (evalConstructor, evalDestructor, evalCopy) via
            // reinterpret_cast from the base MethodVariant pointer.
            bool is_constructor = method_name && strcmp(method_name, "constructor") == 0;
            bool is_destructor = method_name && strcmp(method_name, "destructor") == 0;
            bool is_copy = method_name && strcmp(method_name, "copy") == 0;
            if (is_const_method && (is_static || is_constructor || is_destructor || is_copy)) {
                error = "AOT method '";
                error += qc->getName();
                error += "::";
                error += method_name ? method_name : "(null)";
                error += "' has invalid const-method metadata on ";
                error += is_static ? "a static method" : "a special method";
                return false;
            }

            uint64_t t_alloc0 = time_on ? now_us() : 0;
            // Capture both the MethodVariantBase* and the UserVariantBase*
            // arms via implicit upcasts from the just-constructed concrete
            // type — UserConstructorVariant et al. inherit from BOTH
            // (multiple inheritance), so a later `dynamic_cast<UserVariantBase*>`
            // would incur the RTTI cross-cast walk on every variant
            // (~73ns/var × 652k variants = ~48ms in qwf).  The compiler
            // adjusts the ptr offset for the non-leftmost base at the
            // assignment site for free.
            MethodVariantBase* mvb;
            UserVariantBase* umv;
            if (is_constructor) {
                auto* v = new UserConstructorVariant(
                    static_cast<ClassAccess>(access),
                    nullptr, 0, 0, QoreValue(), nullptr, QCF_NO_FLAGS);
                mvb = v; umv = v;
            } else if (is_destructor) {
                auto* v = new UserDestructorVariant(nullptr, 0, 0);
                mvb = v; umv = v;
            } else if (is_copy) {
                auto* v = new UserCopyVariant(
                    static_cast<ClassAccess>(access),
                    nullptr, 0, 0, QoreValue(), nullptr, is_synchronized);
                mvb = v; umv = v;
            } else {
                auto* v = new UserMethodVariant(
                    static_cast<ClassAccess>(access), is_final,
                    nullptr, 0, 0, QoreValue(), nullptr, is_synchronized,
                    QCF_NO_FLAGS, is_abstract, is_const_method);
                mvb = v; umv = v;
            }

            bool sig_has_ellipsis = false;
            bool needs_extra_args_flag = false;
            int16_t entry_first_line = 0;
            int16_t entry_last_line = 0;
            QoreParseOptions variant_parse_options;
            uint64_t t_sig0 = time_on ? now_us() : 0;
            if (time_on) {
                local_alloc_us += t_sig0 - t_alloc0;
            }
            const std::vector<const QoreTypeInfo*>* tt =
                uses_type_table ? &type_table_resolved : nullptr;
            if (!readAndSetupVariantSignature(reader, type_resolver, pgm, ptr, end,
                    umv, sig_has_ellipsis, needs_extra_args_flag, entry_first_line,
                    entry_last_line, variant_parse_options, error, qc, tt)) {
                delete mvb;
                return false;
            }
            if (time_on) {
                local_sig_us += now_us() - t_sig0;
            }
            local_variants++;

            // See deserializeFunctions counterpart above: the flag is
            // independent of signature-level ellipsis.  Separating the
            // two is what makes abstract/concrete method matching work
            // for concrete overrides whose bodies reference $argv/$N
            // (e.g. RestPingPollOperation::continuePoll with `on_error
            // rethrow $1.err, ...`).
            if (needs_extra_args_flag) {
                mvb->setFlag(QCF_USES_EXTRA_ARGS);
            }

            // Collect BCA (Base Class Constructor Arguments) raw blob data
            // for deferred deserialization. EXPR_TREE blobs may reference
            // static methods of the same class that haven't been added yet.
            //
            // If `skip_class` is true, `mvb` will be deleted below without
            // being handed to `addUserMethod` — so even though we must
            // still advance `ptr` past the BCA bytes (stream format is
            // fixed), we must NOT record a pbca entry whose `ucv` points
            // at the about-to-be-freed variant.  We read-and-discard
            // instead to preserve the post-loop stream position.
            if (is_constructor && ptr < end) {
                uint8_t has_bca = QoreAOTBinaryReader::readU8(ptr);
                if (has_bca) {
                    uint16_t num_bca = QoreAOTBinaryReader::readU16(ptr);
                    if (num_bca > 0) {
                        UserConstructorVariant* ucv = dynamic_cast<UserConstructorVariant*>(mvb);

                        // Build local var array from constructor's signature params
                        UserSignature* sig = umv->getUserSignature();
                        std::vector<LocalVar*> local_vars;
                        if (sig) {
                            for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                                local_vars.push_back(sig->lv[pi]);
                            }
                            if (sig->selfid) {
                                local_vars.push_back(sig->selfid);
                            }
                            if (sig->argvid) {
                                local_vars.push_back(sig->argvid);
                            }
                        }

                        PendingBCA pbca;
                        pbca.qc = qc;
                        pbca.ucv = ucv;
                        pbca.local_vars = std::move(local_vars);

                        for (uint16_t bi = 0; bi < num_bca; ++bi) {
                            PendingBCAEntry entry;
                            const char* base_path = reader.readStringRef(ptr);
                            entry.base_path = base_path ? base_path : "";
                            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_BCA_LINES) != 0) {
                                entry.start_line = static_cast<int16_t>(QoreAOTBinaryReader::readU16(ptr));
                                entry.end_line = static_cast<int16_t>(QoreAOTBinaryReader::readU16(ptr));
                            }
                            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_BCA_NAMED_ARG_MAP) != 0) {
                                entry.eval_result_size = QoreAOTBinaryReader::readU16(ptr);
                                uint16_t eval_map_size = QoreAOTBinaryReader::readU16(ptr);
                                entry.source_to_param.reserve(eval_map_size);
                                for (uint16_t mi = 0; mi < eval_map_size; ++mi) {
                                    if (mi && !(mi % 100)
                                            && qore_check_cancel(nullptr, "AOT BCA named argument map deserialization")) {
                                        error = "operation cancelled during AOT BCA named argument map deserialization";
                                        return false;
                                    }
                                    entry.source_to_param.push_back(QoreAOTBinaryReader::readU16(ptr));
                                }
                            }

                            // Resolve base class by path
                            entry.classid = 0;
                            if (base_path && base_path[0]) {
                                const QoreClass* base_cls = qore_aot_resolve_class_ref(pgm, base_path, false);
                                if (base_cls) {
                                    entry.classid = base_cls->getID();
                                }
                            }

                            // Read raw arg blobs (advance ptr but don't deserialize)
                            uint16_t num_args = QoreAOTBinaryReader::readU16(ptr);
                            entry.arg_blobs.reserve(num_args);
                            for (uint16_t ai = 0; ai < num_args; ++ai) {
                                uint32_t blob_size = QoreAOTBinaryReader::readU32(ptr);
                                PendingBCAArgBlob ab;
                                ab.data = (blob_size > 0 && ptr + blob_size <= end) ? ptr : nullptr;
                                ab.size = blob_size;
                                entry.arg_blobs.push_back(ab);
                                ptr += blob_size;
                            }
                            pbca.entries.push_back(std::move(entry));
                        }
                        // Only record the pbca when mvb will survive the
                        // method-add step below.  If skip_class fires, mvb
                        // is freed and pbca.ucv would dangle.
                        if (!skip_class) {
                            pending_bcas.push_back(std::move(pbca));
                        }
                    }
                }
            }

            // Skip methods for classes that already existed from module loading
            // — they're already committed with all their methods
            if (skip_class) {
                delete mvb;
                continue;
            }

            if (has_slot_map_section && !is_static && !is_constructor && !is_destructor && !is_copy
                    && !is_abstract) {
                // Empty inherited shells emitted by older qmods carry no body
                // lines (entry_first_line == 0 && entry_last_line == 0).  A
                // genuine derived-class override has actual body lines even
                // when it isn't native-compiled (no slot map entry).  Without
                // the body-lines guard the old fix dropped real overrides like
                // QoreFilterRecordsProcessor::processRecordsShapeImpl,
                // silently falling back to the base class implementation.
                bool has_body_lines = (entry_first_line != 0) || (entry_last_line != 0);
                if (!has_body_lines) {
                    std::string variant_key = getAOTMethodVariantKey(qc, method_name, false, mvb);
                    if (slot_map_names.find(variant_key) == slot_map_names.end()
                            && hasInheritedConcreteMethodVariant(qc, method_name, mvb)) {
                        printd(2, "AOT deser: skipping stale inherited method shell '%s::%s' without native slot '%s'\n",
                            qc->getName(), method_name ? method_name : "", variant_key.c_str());
                        delete mvb;
                        continue;
                    }
                }
            }

            // Note: QCF_USES_EXTRA_ARGS flag is handled by the overridden hasVarargs()
            // method which checks signature.hasVarargs() directly
            attachAOTEntryStatementBlock(pgm, umv, entry_first_line, entry_last_line,
                variant_parse_options);

            // Add method to class
            uint64_t t_add0 = time_on ? now_us() : 0;
            if (qore_class_private::addUserMethod(*qc, method_name, mvb, is_static != 0)) {
                error = "cannot add AOT method '";
                error += qc->getName();
                error += "::";
                error += method_name ? method_name : "(null)";
                error += "()'";
                error += is_static ? " (static" : " (instance";
                error += is_abstract ? ", abstract" : ", concrete";
                error += ") from ";
                error += reader.getLabel() ? reader.getLabel() : "(unknown source)";
                if (entry_first_line || entry_last_line) {
                    error += ":";
                    error += std::to_string(entry_first_line);
                    if (entry_last_line && entry_last_line != entry_first_line) {
                        error += "-";
                        error += std::to_string(entry_last_line);
                    }
                }
                return false;
            }
            if (time_on) {
                local_add_us += now_us() - t_add0;
            }
        }

        printd(5, "AOT deser: %s method '%s::%s' (%s) with %d variant(s)\n",
            skip_class ? "skipped preexisting" : "created",
            qc->getName(), method_name, is_static ? "static" : "instance", num_variants);
    }

    if (time_on) {
        g_aot_dm_alloc_us += local_alloc_us;
        g_aot_dm_sig_us   += local_sig_us;
        g_aot_dm_add_us   += local_add_us;
        g_aot_dm_variants += local_variants;
    }
    return true;
}

bool QoreAOTBinaryDeserializer::importInheritedMembers(std::string& error) {
    // Import inherited members from base classes into newly deserialized classes.
    // During normal parsing, BCNode::initializeMembers() calls parseImportMembers()
    // to copy base class members into the derived class's member map. AOT deserialization
    // skips this step, so derived classes can't access inherited members at runtime.
    for (size_t i = 0; i < class_list.size(); ++i) {
        if (preexisting_classes.count(i)) {
            continue;  // already fully initialized from module loading
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        // initializeMembers() checks parse_resolve_class_members flag to avoid re-initialization,
        // iterates base class list, and calls parseImportMembers() for each base class
        priv->initializeMembers();
        printd(5, "AOT deser: imported inherited members for class '%s'\n", qc->getName());
    }
    return true;
}

bool QoreAOTBinaryDeserializer::commitDeserializedClasses(std::string& error) {
    // Single-session wrapper — runs all 5 sub-phases in order.
    // Multi-session callers bypass this and interleave sub-phases
    // across sessions via the MultiDeserializer.
    if (!commitClassesPrepare(error)) return false;
    if (!commitClassesDoCommit(error)) return false;
    if (!commitClassesImportAbstract(error)) return false;
    if (!commitClassesResolveAbstract(error)) return false;
    if (!commitClassesValidate(error)) return false;
    return true;
}

// Sub-phase 2c-1: set initialized + has_new_user_changes on each
// class and run parseAddAncestors.  No parseCommit fires here.
//
// In batch mode (multi-session), the MultiDeserializer runs this on
// ALL sessions before any session calls commitClassesDoCommit.
// This guarantees that when session X's parseCommit recurses into a
// base class owned by session Y, the base already has
// `has_new_user_changes=true` and its methods have been handed to
// parseAddAncestors — so the recursive parseCommit's method-commit
// loop actually runs instead of silently skipping (which was the
// root of the ADPwDPC/QWf empty-class cascade).
bool QoreAOTBinaryDeserializer::commitClassesPrepare(std::string& error) {
    // Use topological order (bases before derived) computed in resolveClassBases().
    // If topo_order is empty (no classes or resolveClassBases not called), fall back
    // to sequential order for backward compatibility.
    auto& order = topo_order;
    std::vector<uint32_t> fallback_order;
    if (order.empty() && !class_list.empty()) {
        fallback_order.resize(class_list.size());
        std::iota(fallback_order.begin(), fallback_order.end(), 0);
        order = fallback_order;
    }

    // Pre-commit pass: set flags + parseAddAncestors on every class.
    for (uint32_t i : order) {
        if (i >= class_list.size() || preexisting_classes.count(i)) {
            continue;  // already initialized and committed
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        // Mirror initializeIntern(): class signature hashing must use resolved
        // method signatures, not the empty pre-resolve signature text.
        for (auto& mi : priv->hm) {
            qore_method_private::get(*mi.second)->getFunction()->resolvePendingSignatures();
        }
        for (auto& mi : priv->shm) {
            qore_method_private::get(*mi.second)->getFunction()->resolvePendingSignatures();
        }
        priv->initialized = true;
        // Force has_new_user_changes so parseCommit() runs its full path:
        //   - addLocalMembersForInit() populates member_init_list (so
        //     `string name = "p"` defaults aren't silently dropped)
        //   - parseCommitMethod() moves pending variants to committed vlist
        //   - checkAssignSpecial() binds priv->constructor / destructor / copy
        //     / methodGate / memberGate / memberNotification pointers from the
        //     method map, so block-exit destructors (and the other special
        //     methods) actually run on AOT-deserialized class instances.
        // In source parse this flag is set by addUserMethod() for ANY newly
        // added method; mirror that here for every newly deserialized class.
        // addMember() only bumps has_sig_changes, which is not sufficient.
        priv->has_new_user_changes = true;
        // Populate each method's inheritance list (ilist) from base-class methods
        // with the same name.  In source parse this is done in initializeHierarchy()
        // via parseAddAncestors(); the AOT deserializer skips that path, so without
        // this step an overloaded method that the derived class overrides for only
        // SOME variants cannot dispatch to the inherited parent variants.
        // Concrete symptom: HashDeclDataType overrides
        //   isAssignableFrom(AbstractDataProviderType)
        // but inherits
        //   isAssignableFrom(Type)
        // from AbstractDataProviderType; calling the Type overload at runtime
        // raises RUNTIME-OVERLOAD-ERROR because the ilist only sees the derived
        // variant. Runs in topo order — parent hm is populated first.
        // Special methods (constructor/destructor/copy) are skipped by
        // initializeHierarchy; mirror that via checkSpecial().
        if (priv->scl) {
            for (auto& mi : priv->hm) {
                const char* mn = mi.second->getName();
                if (strcmp(mn, "constructor") && strcmp(mn, "destructor") && strcmp(mn, "copy")) {
                    priv->parseAddAncestors(mi.second);
                }
            }
            for (auto& mi : priv->shm) {
                priv->parseAddStaticAncestors(mi.second);
            }
        }
    }

    return true;
}

// Sub-phase 2c-2: call parseCommit on every newly deserialized
// class in topological order.  In multi-session mode this runs on
// all sessions AFTER every session has completed
// commitClassesPrepare — so recursive parseCommit walks through
// sibling sessions' classes find prepared method maps and bind
// priv->constructor / destructor / copy correctly.
bool QoreAOTBinaryDeserializer::commitClassesDoCommit(std::string& error) {
    auto& order = topo_order;
    std::vector<uint32_t> fallback_order;
    if (order.empty() && !class_list.empty()) {
        fallback_order.resize(class_list.size());
        std::iota(fallback_order.begin(), fallback_order.end(), 0);
        order = fallback_order;
    }
    for (uint32_t i : order) {
        if (i >= class_list.size() || preexisting_classes.count(i)) {
            continue;
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        // Commits all pending method variants (hm, shm maps); handles base-class recursion
        priv->parseCommit();
        if (i < class_signature_hashes.size() && class_signature_hashes[i].size() == SH_SIZE) {
            priv->hash.setRawHash(class_signature_hashes[i].data());
        }
        printd(5, "AOT deser: committed class '%s' constructor=%p hm.size=%d\n",
            qc->getName(), (void*)priv->constructor, (int)priv->hm.size());
    }
    return true;
}

// Sub-phase 2c-3: import abstract methods from parent classes into
// derived classes.  Must run after all sessions' parseCommit so
// `parseHasVariantWithSignature` sees the fully committed vlist.
bool QoreAOTBinaryDeserializer::commitClassesImportAbstract(std::string& error) {
    auto& order = topo_order;
    std::vector<uint32_t> fallback_order;
    if (order.empty() && !class_list.empty()) {
        fallback_order.resize(class_list.size());
        std::iota(fallback_order.begin(), fallback_order.end(), 0);
        order = fallback_order;
    }
    // Second pass: import abstract methods from parent classes and resolve them.
    // This must happen AFTER parseCommit() because concrete variants are in the
    // pending list until committed — parseHasVariantWithSignature() only searches
    // the committed vlist. This mirrors parseInitPartialIntern() (QoreClass.cpp:4477).
    for (uint32_t i : order) {
        if (i >= class_list.size() || preexisting_classes.count(i)) {
            continue;
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        if (!priv->scl) {
            continue;
        }
        for (auto bi = priv->scl->begin(), be = priv->scl->end(); bi != be; ++bi) {
            const QoreClass* parent = (*bi)->sclass;
            if (!parent) {
                continue;
            }
            const AbstractMethodMap& parent_ahm = qore_class_private::get(*parent)->ahm;
            for (auto ai = parent_ahm.begin(), ae = parent_ahm.end(); ai != ae; ++ai) {
                if (priv->ahm.find(ai->first) != priv->ahm.end()) {
                    continue;
                }
                // Check if we have a local concrete override (now in committed vlist)
                auto mi = priv->hm.find(ai->first);
                MethodFunctionBase* f = (mi != priv->hm.end())
                    ? qore_method_private::get(*mi->second)->getFunction() : nullptr;
                if (f && f->parseHasVariantWithSignature(
                        ai->second->vlist.begin()->second, priv->ahm.relaxed_match)) {
                    // Resolved — concrete override matches abstract signature
                    continue;
                }
                // Unresolved — import abstract method
                std::unique_ptr<AbstractMethod> m(new AbstractMethod(priv->ahm.relaxed_match));
                m->parseMergeBase(*(ai->second), f);
                if (!m->empty()) {
                    priv->ahm.insert(amap_t::value_type(ai->first, m.release()));
                }
            }
        }
    }
    return true;
}

// Sub-phase 2c-3b: resolve imported abstract methods by searching
// sibling parent classes for concrete overrides.
//
// `commitClassesImportAbstract` only checks the derived class's own
// committed methods for concrete overrides — if a sibling parent in
// a diamond provides the concrete, the abstract stays in `priv->ahm`
// and later trips the `ahm.empty()` assertion in `execConstructor`.
//
// Example from qlib/HttpClientIo/Http1ClientPollOperationImpl.qc:
//     public class Http1ClientPollOperation
//             inherits Http1ClientPollOperationBase, HttpClientPollOperation {
//         # cancelRequest is the only locally-concrete method here
//         cancelRequest(int stream_id) { ... }
//     }
// `HttpClientPollOperation` (Qore) inherits the abstract `goalReached`
// `getGoal` `getState` `continuePoll` slots from `AbstractPollOperation`.
// `Http1ClientPollOperationBase` (C++/qpp, via `SocketPollOperationBase`)
// provides concrete overrides for all four — but they live on the
// SIBLING parent chain, so `commitClassesImportAbstract`'s local-hm
// check misses them.
//
// Source-parse path runs `qore_class_private::parseResolveAbstract()`
// (lib/QoreClass.cpp:4951) which calls `ahm.parseInit(*this, scl)`
// (lib/QoreClass.cpp:335) — the latter walks `scl->matchNonAbstractVariant`
// over ALL siblings and moves resolved abstracts from `vlist` to
// `pending_save`.  Mirror that here for AOT-deserialized classes.
bool QoreAOTBinaryDeserializer::commitClassesResolveAbstract(std::string& error) {
    auto& order = topo_order;
    std::vector<uint32_t> fallback_order;
    if (order.empty() && !class_list.empty()) {
        fallback_order.resize(class_list.size());
        std::iota(fallback_order.begin(), fallback_order.end(), 0);
        order = fallback_order;
    }
    for (uint32_t i : order) {
        if (i >= class_list.size() || preexisting_classes.count(i)) {
            continue;
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        if (!priv->scl || priv->ahm.empty()) {
            continue;
        }
        std::unordered_set<std::string> existing_methods;
        existing_methods.reserve(priv->hm.size());
        size_t method_count = 0;
        for (auto& mi : priv->hm) {
            if (++method_count % 100 == 0
                    && qore_check_cancel(nullptr, "AOT abstract method inheritance restore")) {
                error = "operation cancelled during AOT abstract method inheritance restore";
                return false;
            }
            existing_methods.insert(mi.first);
        }

        priv->ahm.parseInit(*priv, priv->scl);

        // Abstract resolution can synthesize a local method on this class
        // after commitClassesPrepare() has already run parseAddAncestors().
        // Give those synthesized methods the same inherited overload list
        // source parsing would attach during initializeHierarchy().
        method_count = 0;
        for (auto& mi : priv->hm) {
            if (++method_count % 100 == 0
                    && qore_check_cancel(nullptr, "AOT abstract method inheritance restore")) {
                error = "operation cancelled during AOT abstract method inheritance restore";
                return false;
            }
            if (existing_methods.find(mi.first) == existing_methods.end()
                    && strcmp(mi.second->getName(), "constructor")
                    && strcmp(mi.second->getName(), "destructor")
                    && strcmp(mi.second->getName(), "copy")) {
                priv->parseAddAncestors(mi.second);
            }
        }
        // parseResolveAbstract() is a no-op once this flag is true; set it
        // so any later source-parse pass in the same Program doesn't
        // redundantly walk the same classes.
        priv->parse_resolve_abstract = true;
    }
    return true;
}

// Sub-phase 2c-4: verify base-class reachability.
bool QoreAOTBinaryDeserializer::commitClassesValidate(std::string& error) {
    auto& order = topo_order;
    std::vector<uint32_t> fallback_order;
    if (order.empty() && !class_list.empty()) {
        fallback_order.resize(class_list.size());
        std::iota(fallback_order.begin(), fallback_order.end(), 0);
        order = fallback_order;
    }
    // Validation pass: verify every base class is reachable via getClass().
    // Catches hierarchy bugs at load time instead of deep in object construction.
    for (uint32_t i : order) {
        if (i >= class_list.size() || preexisting_classes.count(i)) {
            continue;
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        if (!priv->scl) {
            continue;
        }
        for (auto bi = priv->scl->begin(), be = priv->scl->end(); bi != be; ++bi) {
            const QoreClass* base = (*bi)->sclass;
            if (!base) {
                continue;
            }
            if (!qc->getClass(base->getID())) {
                error = "class hierarchy broken: '" + std::string(qc->getName()) +
                    "' cannot reach base class '" + std::string(base->getName()) +
                    "' (id: " + std::to_string(base->getID()) + ")";
                return false;
            }
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::resolveBCAExpressions(std::string& error) {
    bool bca_native_args = (reader.getHeader().feature_flags & QORE_AOT_FEAT_BCA_NATIVE_ARGS) != 0;
    for (auto& pbca : pending_bcas) {
        if (!pbca.ucv) {
            continue;
        }

        BCAList* bcal = new BCAList();
        for (size_t bca_index = 0; bca_index < pbca.entries.size(); ++bca_index) {
            auto& entry = pbca.entries[bca_index];
            if (!entry.classid && !entry.base_path.empty()) {
                if (const QoreClass* base_cls = qore_aot_resolve_class_ref(pgm, entry.base_path.c_str(), false)) {
                    entry.classid = base_cls->getID();
                } else {
                    error = "cannot resolve base constructor class '";
                    error += entry.base_path;
                    error += "' for class '";
                    error += pbca.qc ? pbca.qc->getNamespacePath() : "<unknown>";
                    error += "'";
                    delete bcal;
                    return false;
                }
            }

            // Deserialize arg blobs now that all methods are committed.  New
            // objects use native inline AOT expression blobs; legacy objects
            // without QORE_AOT_FEAT_BCA_NATIVE_ARGS still use EXPR_TREE.
            uint16_t num_args = static_cast<uint16_t>(entry.arg_blobs.size());
            QoreListNode* arg_list = nullptr;
            if (num_args > 0) {
                arg_list = qore_list_private::newList(true);
                qore_list_private::get(*arg_list)->complexTypeInfo =
                    qore_get_complex_list_type(autoTypeInfo);
                for (uint16_t ai = 0; ai < num_args; ++ai) {
                    auto& ab = entry.arg_blobs[ai];
                    if (bca_native_args) {
                        if (!ab.data || !ab.size) {
                            error = "invalid empty native BCA argument blob";
                            error += "; class='";
                            error += pbca.qc ? pbca.qc->getNamespacePath() : "<unknown>";
                            error += "' method='constructor' base='";
                            error += entry.base_path.empty() ? "<unknown>" : entry.base_path;
                            error += "' bca-index=";
                            error += std::to_string(bca_index);
                            error += " arg-index=";
                            error += std::to_string(ai);
                            error += " bca-lines=";
                            error += std::to_string(entry.start_line);
                            error += "-";
                            error += std::to_string(entry.end_line);
                            arg_list->deref(nullptr);
                            delete bcal;
                            return false;
                        }

                        const uint8_t* p = ab.data;
                        const uint8_t* end = ab.data + ab.size;
                        std::string expr_error;
                        QoreValue arg_val = readOneExpr(reader, p, end, expr_error, pgm,
                            pbca.local_vars.empty() ? nullptr : pbca.local_vars.data(),
                            static_cast<int>(pbca.local_vars.size()), nullptr, 0);
                        if (!expr_error.empty() || p != end) {
                            arg_val.discard(nullptr);
                            error = "failed to deserialize native BCA argument";
                            error += "; class='";
                            error += pbca.qc ? pbca.qc->getNamespacePath() : "<unknown>";
                            error += "' method='constructor' base='";
                            error += entry.base_path.empty() ? "<unknown>" : entry.base_path;
                            error += "' bca-index=";
                            error += std::to_string(bca_index);
                            error += " arg-index=";
                            error += std::to_string(ai);
                            error += " blob-size=";
                            error += std::to_string(ab.size);
                            error += " bca-lines=";
                            error += std::to_string(entry.start_line);
                            error += "-";
                            error += std::to_string(entry.end_line);
                            if (!expr_error.empty()) {
                                error += ": ";
                                error += expr_error;
                            }
                            if (p != end) {
                                error += "; trailing-bytes=";
                                error += std::to_string(end - p);
                            }
                            arg_list->deref(nullptr);
                            delete bcal;
                            return false;
                        }
                        arg_list->push(arg_val, nullptr);
                    } else if (ab.data && ab.size > 0) {
                        QoreValue arg_val = deserializeExprTreeFromBlob(
                            ab.data, ab.size, pgm,
                            pbca.local_vars.empty() ? nullptr : pbca.local_vars.data(),
                            static_cast<int>(pbca.local_vars.size()));
                        arg_list->push(arg_val, nullptr);
                    } else {
                        arg_list->push(QoreValue(), nullptr);
                    }
                }
            }
            if (arg_list && !entry.source_to_param.empty()) {
                if (entry.source_to_param.size() != arg_list->size()) {
                    error = "invalid BCA named-argument map size";
                    error += "; class='";
                    error += pbca.qc ? pbca.qc->getNamespacePath() : "<unknown>";
                    error += "' method='constructor' base='";
                    error += entry.base_path.empty() ? "<unknown>" : entry.base_path;
                    error += "'";
                    arg_list->deref(nullptr);
                    delete bcal;
                    return false;
                }
                for (size_t mi = 0; mi < entry.source_to_param.size(); ++mi) {
                    if (mi && !(mi % 100)
                            && qore_check_cancel(nullptr, "AOT BCA named argument map validation")) {
                        error = "operation cancelled during AOT BCA named argument map validation";
                        arg_list->deref(nullptr);
                        delete bcal;
                        return false;
                    }
                    size_t target = entry.source_to_param[mi];
                    if (target >= entry.eval_result_size) {
                        error = "invalid BCA named-argument map target";
                        error += "; class='";
                        error += pbca.qc ? pbca.qc->getNamespacePath() : "<unknown>";
                        error += "' method='constructor' base='";
                        error += entry.base_path.empty() ? "<unknown>" : entry.base_path;
                        error += "'";
                        arg_list->deref(nullptr);
                        delete bcal;
                        return false;
                    }
                }
                qore_list_private::setNeedsEval(*arg_list);
                qore_list_private::get(arg_list)->setCallArgEvalMap(std::move(entry.source_to_param),
                    entry.eval_result_size);
            }

            const QoreProgramLocation* bca_loc = &loc_builtin;
            if (entry.start_line > 0 || entry.end_line > 0) {
                const QoreProgramLocation* ctor_loc = nullptr;
                if (UserSignature* sig = pbca.ucv->getUserSignature()) {
                    ctor_loc = sig->getParseLocation();
                }
                qore_program_private* pp = qore_program_private::get(*pgm);
                bca_loc = ctor_loc
                    ? pp->getLocation(*ctor_loc, entry.start_line, entry.end_line)
                    : pp->getLocation(entry.start_line, entry.end_line);
            }

            BCANode* bca_node = new BCANode(entry.classid, arg_list, bca_loc);
            bcal->push_back(bca_node);
        }

        pbca.ucv->setBCAList(bcal);
    }

    pending_bcas.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::deserializeFallbackSources(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::FUNC_SOURCES);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid FUNC_SOURCES section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    // Read the full source text reference
    const char* src = reader.readStringRef(ptr);
    if (src && *src) {
        fallback_source = src;
        fallback_source_len = strlen(src);
    }

    // Read fallback function names
    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    fallback_func_names.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT fallback metadata deserialization")) {
            error = "AOT fallback metadata deserialization cancelled";
            return false;
        }
        const char* name = reader.readStringRef(ptr);
        if (name) {
            fallback_func_names.emplace_back(name);
        }
    }

    if (count > 0) {
        error = "AOT source fallback is disabled; object contains ";
        error += std::to_string(count);
        error += count == 1 ? " fallback function" : " fallback functions";
        error += ": ";
        size_t printed = std::min<size_t>(fallback_func_names.size(), 8);
        for (size_t i = 0; i < printed; ++i) {
            if (i) {
                error += ", ";
            }
            error += fallback_func_names[i];
        }
        if (printed < std::min<uint32_t>(count, 8)) {
            if (printed) {
                error += ", ";
            }
            error += "<invalid>";
        }
        if (count > 8) {
            error += ", ...";
        }
        return false;
    }

    printd(2, "AOT: loaded embedded source (%d bytes, no fallback functions)\n",
        static_cast<int>(fallback_source_len));

    return true;
}

bool serializeNamespaceTree(QoreAOTBinaryWriter& writer, qore_ns_private* root_ns,
        const char* module_name, const std::unordered_set<std::string>* keep_modules,
        const char* compile_file, std::string* error,
        const std::unordered_set<std::string>* compile_files) {
    // Phase 1: Collect all user-defined items into indexed vectors
    // When module_name is provided, filter out items from reexported dependencies
    // When keep_modules is provided, items from those modules are always included
    // When compile_file is provided (Phase 4 slice 4), items whose AST
    // declaration file doesn't match are skipped so the emitted metadata
    // describes only the contributions of one source file.
    printd(5, "serializeNamespaceTree: module_name='%s' compile_file='%s' compile_files=%zu root_ns='%s'\n",
        module_name ? module_name : "n/a",
        compile_file ? compile_file : "n/a",
        compile_files ? compile_files->size() : 0,
        root_ns->ns->getName());
    AOTSerializeState state;
    state.root_ns = root_ns;  // Store root namespace for program-wide CRM building
    collectItems(state, root_ns, UINT32_MAX, module_name, keep_modules, compile_file, compile_files);

    std::vector<qore_ns_private*> extra_roots;
    bool debug_local_modules = getenv("QORE_AOT_DEBUG_LOCAL_MODULES") != nullptr;
    if (debug_local_modules && keep_modules) {
        fprintf(stderr, "AOT local module metadata: root collection classes=%zu methods=%zu keep_modules=%zu\n",
            state.classes.size(), state.methods.size(), keep_modules->size());
        for (const std::string& mod : *keep_modules) {
            fprintf(stderr, "AOT local module metadata: keep '%s'\n", mod.c_str());
        }
    }
    if (module_name && !*module_name && keep_modules
            && !hasAOTBinaryCompileFileFilter(compile_file, compile_files)) {
        for (const std::string& mod : *keep_modules) {
            QoreProgram* module_pgm = MM.findUserModuleProgram(mod.c_str());
            if (debug_local_modules) {
                fprintf(stderr, "AOT local module metadata: lookup '%s' -> %p\n",
                    mod.c_str(), module_pgm);
            }
            if (module_pgm) {
                RootQoreNamespace* module_root = module_pgm->getRootNS();
                if (!module_root) {
                    continue;
                }
                qore_ns_private* module_root_priv = qore_ns_private::get(*module_root);
                if (module_root_priv == root_ns) {
                    continue;
                }
                size_t before_classes = state.classes.size();
                size_t before_methods = state.methods.size();
                extra_roots.push_back(module_root_priv);
                collectItems(state, module_root_priv, UINT32_MAX, mod.c_str(), nullptr, nullptr, nullptr);
                if (debug_local_modules) {
                    fprintf(stderr,
                        "AOT local module metadata: collected '%s' root='%s' classes +%zu methods +%zu\n",
                        mod.c_str(), module_root_priv->name.c_str(),
                        state.classes.size() - before_classes, state.methods.size() - before_methods);
                }
            }
        }
    }

    // Build a program-wide constant reverse map once and make it available to
    // the writer so writeValue() can encode node-pointer references (e.g. an
    // object inside a parse-time-folded hash literal) as VT_CONST_REF entries
    // that resolve at load time. The writer takes a non-owning pointer; the
    // map is cleared after all sections that call writeValue are done.
    AOTConstantReverseMap program_crm;
    if (root_ns) {
        buildProgramConstantReverseMapImpl(root_ns, program_crm);
    }
    for (qore_ns_private* extra_root : extra_roots) {
        buildProgramConstantReverseMapImpl(extra_root, program_crm);
    }
    writer.const_reverse_map = &program_crm;

    // Phase 2: Write each section
    writeNamespacesSection(writer, state);
    std::string section_error;
    if (!writeClassesSection(writer, state, section_error)) {
        if (error) {
            *error = "CLASSES section: " + section_error;
        }
        return false;
    }
    section_error.clear();
    if (!writeHashDeclsSection(writer, state, section_error)) {
        if (error) {
            *error = "HASHDECLS section: " + section_error;
        }
        return false;
    }
    writeEnumsSection(writer, state);
    writeTypedefsSection(writer, state);
    writeConstantsSection(writer, state);
    writeGlobalsSection(writer, state);
    writeFunctionsSection(writer, state);
    section_error.clear();
    if (!writeMethodsSection(writer, state, section_error)) {
        if (error) {
            *error = "METHODS section: " + section_error;
        }
        return false;
    }

    // After every variant signature has been emitted, flush the per-blob
    // type-path table (TYPE_TABLE section).  Must come after
    // writeFunctions/Methods so the table contains every path the
    // variants referenced via writer.internTypePath().
    writer.writeTypeTableSection();
    section_error.clear();
    if (!writer.writePluginSections(section_error)) {
        if (error) {
            *error = "PLUGIN sections: " + section_error;
        }
        return false;
    }

    // Drop the non-owning CRM pointer — program_crm goes out of scope next.
    writer.const_reverse_map = nullptr;

    return true;
}

void QoreAOTBinaryWriter::writeTypeTableSection() {
    // Skip emission when the interner was never touched (no variant wrote
    // through the new path, or the module has no user variants) — absence
    // of the section signals to the reader that the old string-based
    // format is in effect.
    if (type_path_table.empty()) {
        return;
    }
    uint32_t idx = beginSection(QoreAOTSectionType::TYPE_TABLE);
    const uint32_t count = static_cast<uint32_t>(type_path_table.size());
    writeU32(count);
    for (uint32_t i = 0; i < count; ++i) {
        writeStringRef(type_path_table[i].c_str());
    }
    endSection(idx);
}

bool QoreAOTBinaryWriter::addPluginOperationRef(const char* module_name, uint16_t op_local_id,
        uint8_t canonical_signature_version, uint64_t signature_hash) {
    if (!module_name || !*module_name || plugin_helper_refs.size() > UINT16_MAX) {
        tracePluginQord("write failed: invalid plugin operation ref module name or helper ref count overflow");
        return false;
    }

    uint16_t import_idx;
    auto i = plugin_import_index.find(module_name);
    if (i == plugin_import_index.end()) {
        if (plugin_imports.size() > UINT16_MAX) {
            return false;
        }
        import_idx = static_cast<uint16_t>(plugin_imports.size());
        PluginImportRecord import;
        import.module_name = module_name;
        plugin_imports.push_back(std::move(import));
        plugin_import_index.emplace(plugin_imports.back().module_name, import_idx);
        tracePluginQord("write: added plugin import[" + std::to_string(import_idx) + "] module='"
            + module_name + "'");
    } else {
        import_idx = i->second;
    }

    std::vector<uint16_t>& ops = plugin_imports[import_idx].required_operation_ids;
    if (std::find(ops.begin(), ops.end(), op_local_id) == ops.end()) {
        ops.push_back(op_local_id);
    }

    PluginHelperRefRecord ref;
    ref.slot_idx = static_cast<uint16_t>(plugin_helper_refs.size());
    ref.import_idx = import_idx;
    ref.op_local_id = op_local_id;
    ref.canonical_signature_version = canonical_signature_version;
    ref.signature_hash = signature_hash;
    plugin_helper_refs.push_back(ref);
    tracePluginQord("write: added helper_ref[" + std::to_string(ref.slot_idx) + "] module='"
        + module_name + "' import_idx=" + std::to_string(import_idx) + " op_local_id="
        + std::to_string(op_local_id) + " canonical_signature_version="
        + std::to_string(canonical_signature_version));
    return true;
}

bool QoreAOTBinaryWriter::addPluginTypeRef(const char* module_name, uint16_t local_type_id, uint16_t* import_idx_out) {
    if (import_idx_out) {
        *import_idx_out = 0;
    }
    if (!module_name || !*module_name) {
        tracePluginQord("write failed: invalid plugin type ref module name");
        return false;
    }

    uint16_t import_idx;
    auto i = plugin_import_index.find(module_name);
    if (i == plugin_import_index.end()) {
        if (plugin_imports.size() > UINT16_MAX) {
            tracePluginQord("write failed: plugin import count overflow");
            return false;
        }
        import_idx = static_cast<uint16_t>(plugin_imports.size());
        PluginImportRecord import;
        import.module_name = module_name;
        plugin_imports.push_back(std::move(import));
        plugin_import_index.emplace(plugin_imports.back().module_name, import_idx);
        tracePluginQord("write: added plugin import[" + std::to_string(import_idx) + "] module='"
            + module_name + "'");
    } else {
        import_idx = i->second;
    }

    std::vector<uint16_t>& types = plugin_imports[import_idx].required_type_ids;
    if (std::find(types.begin(), types.end(), local_type_id) == types.end()) {
        types.push_back(local_type_id);
    }
    if (import_idx_out) {
        *import_idx_out = import_idx;
    }
    tracePluginQord("write: added type ref module='" + std::string(module_name) + "' import_idx="
        + std::to_string(import_idx) + " local_type_id=" + std::to_string(local_type_id));
    return true;
}

static bool checkAOTPluginCancel(size_t i, ExceptionSink& xsink, const char* operation, std::string& error) {
    if (i && !(i % 100) && qore_check_cancel(&xsink, operation)) {
        error = "operation cancelled during ";
        error += operation;
        return true;
    }
    return false;
}

static void writePluginSignatureMetadata(QoreAOTBinaryWriter& writer,
        const QorePluginOperationSignature& sig) {
    writer.writeU8(sig.arity);
    writer.writeU8(static_cast<uint8_t>(sig.helper_abi));
    writer.writeU8(static_cast<uint8_t>(sig.access));
    writer.writeU8(static_cast<uint8_t>(sig.result_alias));
    writer.writeU8(sig.primary_nullable ? 1 : 0);
    writer.writeU8(sig.secondary_nullable ? 1 : 0);
    writer.writeU8(sig.return_nullable ? 1 : 0);
    writer.writeU8(0);
    writer.writeStringRef(sig.primary_type ? qore_type_get_path(sig.primary_type) : "");
    writer.writeStringRef(sig.secondary_type ? qore_type_get_path(sig.secondary_type) : "");
    writer.writeStringRef(sig.return_type ? qore_type_get_path(sig.return_type) : "");
}

static void writePluginHash64(QoreAOTBinaryWriter& writer, uint64_t hash) {
    writer.writeU32(static_cast<uint32_t>(hash & 0xffffffffu));
    writer.writeU32(static_cast<uint32_t>(hash >> 32));
}

bool QoreAOTBinaryWriter::writePluginSections(std::string& error) {
    if (plugin_imports.empty()) {
        tracePluginQord("write: no plugin imports to serialize");
        return true;
    }

    tracePluginQord("write: serializing " + std::to_string(plugin_imports.size())
        + " plugin import(s) and " + std::to_string(plugin_helper_refs.size()) + " helper ref(s)");
    ExceptionSink xsink;
    std::vector<QorePluginAOTModuleInfo> infos;
    infos.reserve(plugin_imports.size());
    for (size_t i = 0; i < plugin_imports.size(); ++i) {
        if (checkAOTPluginCancel(i, xsink, "AOT plugin import metadata lookup", error)) {
            return false;
        }
        QorePluginAOTModuleInfo info;
        if (qore_plugin_get_aot_module_info(plugin_imports[i].module_name.c_str(), info, &xsink) || xsink) {
            error = "plugin import '";
            error += plugin_imports[i].module_name;
            error += "' is not registered in the process plugin registry";
            tracePluginQord("write failed: plugin import '" + plugin_imports[i].module_name
                + "' is not registered");
            return false;
        }
        plugin_imports[i].plugin_abi_version = info.plugin_abi_version;
        plugin_imports[i].operation_set_version = info.operation_set_version;
        std::sort(plugin_imports[i].required_type_ids.begin(), plugin_imports[i].required_type_ids.end());
        std::sort(plugin_imports[i].required_operation_ids.begin(), plugin_imports[i].required_operation_ids.end());
        tracePluginQord("write: import[" + std::to_string(i) + "] module='" + plugin_imports[i].module_name
            + "' types=" + std::to_string(plugin_imports[i].required_type_ids.size())
            + " operations=" + std::to_string(plugin_imports[i].required_operation_ids.size()));
        infos.push_back(std::move(info));
    }

    std::vector<std::unordered_map<uint16_t, size_t>> type_indexes(infos.size());
    std::vector<std::unordered_map<uint16_t, size_t>> op_indexes(infos.size());
    for (size_t i = 0; i < infos.size(); ++i) {
        type_indexes[i].reserve(infos[i].types.size());
        for (size_t n = 0; n < infos[i].types.size(); ++n) {
            if (checkAOTPluginCancel(n, xsink, "AOT plugin type metadata indexing", error)) {
                return false;
            }
            type_indexes[i].emplace(infos[i].types[n].local_type_id, n);
        }
        op_indexes[i].reserve(infos[i].operations.size());
        for (size_t n = 0; n < infos[i].operations.size(); ++n) {
            if (checkAOTPluginCancel(n, xsink, "AOT plugin operation metadata indexing", error)) {
                return false;
            }
            op_indexes[i].emplace(infos[i].operations[n].local_id, n);
        }
    }
    for (size_t i = 0; i < plugin_imports.size(); ++i) {
        for (size_t n = 0; n < plugin_imports[i].required_type_ids.size(); ++n) {
            if (checkAOTPluginCancel(n, xsink, "AOT plugin type ref validation", error)) {
                return false;
            }
            uint16_t local_type_id = plugin_imports[i].required_type_ids[n];
            if (type_indexes[i].find(local_type_id) == type_indexes[i].end()) {
                error = "plugin type ref for import '";
                error += plugin_imports[i].module_name;
                error += "' references unregistered local type id ";
                error += std::to_string(local_type_id);
                tracePluginQord("write failed: type ref references unregistered local type id "
                    + std::to_string(local_type_id));
                return false;
            }
        }
    }

    uint32_t sec_idx = beginSection(QoreAOTSectionType::PLUGIN_IMPORTS);
    tracePluginQord("write: emitting PLUGIN_IMPORTS");
    writeU32(static_cast<uint32_t>(plugin_imports.size()));
    for (size_t i = 0; i < plugin_imports.size(); ++i) {
        if (checkAOTPluginCancel(i, xsink, "AOT plugin import section serialization", error)) {
            return false;
        }
        const PluginImportRecord& import = plugin_imports[i];
        writeStringRef(import.module_name.c_str());
        writeStringRef(import.plugin_abi_version.c_str());
        writeStringRef(import.operation_set_version.c_str());
        writeU32(static_cast<uint32_t>(import.required_type_ids.size()));
        for (size_t n = 0; n < import.required_type_ids.size(); ++n) {
            if (checkAOTPluginCancel(n, xsink, "AOT plugin import type id serialization", error)) {
                return false;
            }
            writeU16(import.required_type_ids[n]);
        }
        writeU32(static_cast<uint32_t>(import.required_operation_ids.size()));
        for (size_t n = 0; n < import.required_operation_ids.size(); ++n) {
            if (checkAOTPluginCancel(n, xsink, "AOT plugin import operation id serialization", error)) {
                return false;
            }
            writeU16(import.required_operation_ids[n]);
        }
    }
    endSection(sec_idx);

    sec_idx = beginSection(QoreAOTSectionType::PLUGIN_TYPE_REGISTRY);
    tracePluginQord("write: emitting PLUGIN_TYPE_REGISTRY");
    writeU32(static_cast<uint32_t>(infos.size()));
    for (size_t i = 0; i < infos.size(); ++i) {
        if (checkAOTPluginCancel(i, xsink, "AOT plugin type registry section serialization", error)) {
            return false;
        }
        const QorePluginAOTModuleInfo& info = infos[i];
        writeStringRef(info.module_name.c_str());
        writeStringRef(info.plugin_abi_version.c_str());
        writeStringRef(info.operation_set_version.c_str());
        writeU32(static_cast<uint32_t>(info.types.size()));
        for (size_t n = 0; n < info.types.size(); ++n) {
            if (checkAOTPluginCancel(n, xsink, "AOT plugin type metadata serialization", error)) {
                return false;
            }
            const QorePluginAOTTypeInfo& type = info.types[n];
            writeU16(type.local_type_id);
            writeU16(type.serializer_format_version);
            writeStringRef(type.type_name.c_str());
            writeStringRef(type.type_path.c_str());
        }
        writeU32(static_cast<uint32_t>(info.operations.size()));
        for (size_t n = 0; n < info.operations.size(); ++n) {
            if (checkAOTPluginCancel(n, xsink, "AOT plugin operation metadata serialization", error)) {
                return false;
            }
            const QorePluginAOTOperationInfo& op = info.operations[n];
            writeU16(op.local_id);
            writeU8(op.canonical_signature_version);
            writeU8(0);
            writePluginHash64(*this, op.signature_hash);
            writeStringRef(op.operation_name.c_str());
            writePluginSignatureMetadata(*this, op.signature);
        }
    }
    endSection(sec_idx);

    sec_idx = beginSection(QoreAOTSectionType::PLUGIN_HELPER_REFS);
    tracePluginQord("write: emitting PLUGIN_HELPER_REFS");
    writeU32(static_cast<uint32_t>(plugin_helper_refs.size()));
    for (size_t i = 0; i < plugin_helper_refs.size(); ++i) {
        if (checkAOTPluginCancel(i, xsink, "AOT plugin helper-ref section serialization", error)) {
            return false;
        }
        PluginHelperRefRecord ref = plugin_helper_refs[i];
        if (ref.import_idx >= infos.size()) {
            error = "plugin helper ref has invalid import index ";
            error += std::to_string(ref.import_idx);
            tracePluginQord("write failed: helper ref import index is invalid");
            return false;
        }
        auto oi = op_indexes[ref.import_idx].find(ref.op_local_id);
        if (oi == op_indexes[ref.import_idx].end()) {
            error = "plugin helper ref for import '";
            error += plugin_imports[ref.import_idx].module_name;
            error += "' references unregistered local operation id ";
            error += std::to_string(ref.op_local_id);
            tracePluginQord("write failed: helper ref references unregistered local operation id "
                + std::to_string(ref.op_local_id));
            return false;
        }
        const QorePluginAOTOperationInfo& op = infos[ref.import_idx].operations[oi->second];
        if (!ref.canonical_signature_version) {
            ref.canonical_signature_version = op.canonical_signature_version;
        }
        if (!ref.signature_hash) {
            ref.signature_hash = op.signature_hash;
        }
        if (ref.canonical_signature_version != op.canonical_signature_version) {
            error = "plugin helper ref for import '";
            error += plugin_imports[ref.import_idx].module_name;
            error += "' local operation id ";
            error += std::to_string(ref.op_local_id);
            error += " has canonical signature version ";
            error += std::to_string(ref.canonical_signature_version);
            error += " but the registered operation has ";
            error += std::to_string(op.canonical_signature_version);
            tracePluginQord("write failed: helper ref canonical signature version mismatch");
            return false;
        }
        if (ref.signature_hash != op.signature_hash) {
            error = "plugin helper ref for import '";
            error += plugin_imports[ref.import_idx].module_name;
            error += "' local operation id ";
            error += std::to_string(ref.op_local_id);
            error += " has a signature hash mismatch";
            tracePluginQord("write failed: helper ref signature hash mismatch");
            return false;
        }
        tracePluginQord("write: helper_ref[" + std::to_string(i) + "] slot="
            + std::to_string(ref.slot_idx) + " import_idx=" + std::to_string(ref.import_idx)
            + " op_local_id=" + std::to_string(ref.op_local_id) + " canonical_signature_version="
            + std::to_string(ref.canonical_signature_version));
        writeU16(ref.slot_idx);
        writeU16(ref.import_idx);
        writeU16(ref.op_local_id);
        writeU8(ref.canonical_signature_version);
        writeU8(0);
        writePluginHash64(*this, ref.signature_hash);
    }
    endSection(sec_idx);
    tracePluginQord("write: plugin QORD sections serialized successfully");
    return true;
}

void serializeDependencies(QoreAOTBinaryWriter& writer, const std::vector<std::string>& dependencies) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::DEPENDENCIES);

    uint32_t count = static_cast<uint32_t>(dependencies.size());
    writer.writeU32(count);

    for (const auto& dep : dependencies) {
        writer.writeStringRef(dep.c_str());
    }

    writer.endSection(sec_idx);
}

bool readDependencies(const uint8_t* data, uint32_t size, std::vector<std::string>& dependencies, std::string& error) {
    // Open the binary to read just the dependencies section
    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }
    return readDependencies(reader, dependencies, error);
}

bool readDependencies(const QoreAOTBinaryReader& reader, std::vector<std::string>& dependencies,
        std::string& error) {
    // Find DEPENDENCIES section
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::DEPENDENCIES);
    if (!sec) {
        // No dependencies section - this is OK, just means no deps
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid DEPENDENCIES section data";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    dependencies.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        const char* dep_name = reader.readStringRef(ptr);
        if (!dep_name) {
            error = "invalid dependency name at index " + std::to_string(i);
            return false;
        }
        dependencies.push_back(dep_name);
    }

    return true;
}

void serializeReexportModules(QoreAOTBinaryWriter& writer, const std::vector<std::string>& reexport_modules) {
    if (reexport_modules.empty()) {
        return;
    }

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::REEXPORT_MODULES);

    uint32_t count = static_cast<uint32_t>(reexport_modules.size());
    writer.writeU32(count);

    for (const auto& mod : reexport_modules) {
        writer.writeStringRef(mod.c_str());
    }

    writer.endSection(sec_idx);
}

bool readReexportModules(const uint8_t* data, uint32_t size, std::vector<std::string>& reexport_modules,
        std::string& error) {
    // Open the binary to read the reexport modules section
    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }
    return readReexportModules(reader, reexport_modules, error);
}

bool readReexportModules(const QoreAOTBinaryReader& reader, std::vector<std::string>& reexport_modules,
        std::string& error) {
    // Find REEXPORT_MODULES section
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::REEXPORT_MODULES);
    if (!sec) {
        // No reexport modules section - this is OK, just means no reexports
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid REEXPORT_MODULES section data";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    // Sanity check: each entry needs at least 4 bytes (string ref), so count can't exceed section size
    uint32_t max_entries = sec->size / 4;
    if (count > max_entries) {
        error = "reexport module count " + std::to_string(count) + " exceeds section capacity";
        return false;
    }
    reexport_modules.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        const char* mod_name = reader.readStringRef(ptr);
        if (!mod_name) {
            error = "invalid reexport module name at index " + std::to_string(i);
            return false;
        }
        reexport_modules.push_back(mod_name);
    }

    return true;
}

void serializeModulePathLists(QoreAOTBinaryWriter& writer,
        const std::vector<std::string>& prepended,
        const std::vector<std::string>& appended,
        uint64_t& feature_flags) {
    if (prepended.empty() && appended.empty()) {
        return;
    }
    feature_flags |= QORE_AOT_FEAT_MODULE_PATH_LISTS;

    if (!prepended.empty()) {
        uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::MODULE_PATH_PREPEND);
        writer.writeU32(static_cast<uint32_t>(prepended.size()));
        for (const std::string& p : prepended) {
            writer.writeStringRef(p.c_str());
        }
        writer.endSection(sec_idx);
    }
    if (!appended.empty()) {
        uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::MODULE_PATH_APPEND);
        writer.writeU32(static_cast<uint32_t>(appended.size()));
        for (const std::string& p : appended) {
            writer.writeStringRef(p.c_str());
        }
        writer.endSection(sec_idx);
    }
}

void serializeModuleCommands(QoreAOTBinaryWriter& writer,
        const std::vector<AOTModuleCommand>& commands,
        uint64_t& feature_flags) {
    if (commands.empty()) {
        return;
    }
    feature_flags |= QORE_AOT_FEAT_MODULE_COMMANDS;

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::MODULE_COMMANDS);
    writer.writeU32(static_cast<uint32_t>(commands.size()));
    for (const AOTModuleCommand& cmd : commands) {
        writer.writeStringRef(cmd.module.c_str());
        writer.writeStringRef(cmd.command.c_str());
    }
    writer.endSection(sec_idx);
}

bool readModuleCommands(const QoreAOTBinaryReader& reader,
        std::vector<AOTModuleCommand>& commands,
        std::string& error) {
    commands.clear();

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::MODULE_COMMANDS);
    if (!sec) {
        return true;
    }
    const uint8_t* sec_data = reader.getSectionData(*sec);
    if (!sec_data) {
        error = "invalid module-command section data";
        return false;
    }
    if (sec->size < 4) {
        error = "module-command section too small for count";
        return false;
    }

    const uint8_t* ptr = sec_data;
    const uint8_t* end = sec_data + sec->size;
    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    uint32_t max_entries = (sec->size - 4) / 8;
    if (count > max_entries) {
        error = "module-command count " + std::to_string(count) + " exceeds section capacity";
        return false;
    }
    commands.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const char* module = reader.readStringRef(ptr);
        const char* command = reader.readStringRef(ptr);
        if (!module || !command) {
            error = "invalid module-command entry at index " + std::to_string(i);
            return false;
        }
        commands.push_back(AOTModuleCommand{module, command});
    }
    if (ptr != end) {
        error = "module-command section has " + std::to_string(end - ptr) + " trailing byte(s)";
        return false;
    }

    return true;
}

bool readModuleCommands(const uint8_t* data, uint32_t size,
        std::vector<AOTModuleCommand>& commands,
        std::string& error) {
    commands.clear();
    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }
    return readModuleCommands(reader, commands, error);
}

bool readModulePathLists(const uint8_t* data, uint32_t size,
        std::vector<std::string>& prepended,
        std::vector<std::string>& appended,
        std::string& error) {
    prepended.clear();
    appended.clear();
    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }
    return readModulePathLists(reader, prepended, appended, error);
}

void applyModulePathListsToProgram(QoreProgram* pgm,
        const std::vector<std::string>& prepended,
        const std::vector<std::string>& appended) {
    if (!pgm || (prepended.empty() && appended.empty())) {
        return;
    }
    qore_program_private* pp = qore_program_private::get(*pgm);
    if (!pp) {
        return;
    }
    // Silent dedup (same policy as applyModulePathDirective).  Prepended paths
    // are applied in input order so the front-most stays front-most.
    auto has = [](const std::vector<std::string>& v, const std::string& s) {
        for (const std::string& x : v) {
            if (x == s) {
                return true;
            }
        }
        return false;
    };
    for (const std::string& p : prepended) {
        if (!has(pp->prepended_module_paths, p)) {
            pp->prepended_module_paths.push_back(p);
        }
    }
    for (const std::string& p : appended) {
        if (!has(pp->appended_module_paths, p)) {
            pp->appended_module_paths.push_back(p);
        }
    }
}

bool readModulePathLists(const QoreAOTBinaryReader& reader,
        std::vector<std::string>& prepended,
        std::vector<std::string>& appended,
        std::string& error) {
    prepended.clear();
    appended.clear();

    auto readList = [&](QoreAOTSectionType type, std::vector<std::string>& out) -> bool {
        const QoreAOTSectionHeader* sec = reader.findSection(type);
        if (!sec) {
            return true;  // absent — fine, back-compat with pre-feature-flag blobs
        }
        const uint8_t* ptr = reader.getSectionData(*sec);
        if (!ptr) {
            error = "invalid module-path section data";
            return false;
        }
        uint32_t count = QoreAOTBinaryReader::readU32(ptr);
        out.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const char* s = reader.readStringRef(ptr);
            out.emplace_back(s ? s : "");
        }
        return true;
    };

    if (!readList(QoreAOTSectionType::MODULE_PATH_PREPEND, prepended)) {
        return false;
    }
    if (!readList(QoreAOTSectionType::MODULE_PATH_APPEND, appended)) {
        return false;
    }
    return true;
}

void serializeProgramMetadata(QoreAOTBinaryWriter& writer, const char* exec_class_name) {
    // Only create the section if there's metadata to write
    if (!exec_class_name || !*exec_class_name) {
        return;
    }

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::PROGRAM_METADATA);

    // Write exec-class flag (u8) and name (string ref)
    writer.writeU8(1);  // has exec-class
    writer.writeStringRef(exec_class_name);

    writer.endSection(sec_idx);
}

bool readProgramMetadata(const uint8_t* data, uint32_t size, std::string& exec_class_name,
        std::string& error) {
    exec_class_name.clear();

    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }
    return readProgramMetadata(reader, exec_class_name, error);
}

bool readProgramMetadata(const QoreAOTBinaryReader& reader, std::string& exec_class_name,
        std::string& error) {
    exec_class_name.clear();

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::PROGRAM_METADATA);
    if (!sec) {
        // No program metadata section — this is OK (older binaries won't have it)
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid PROGRAM_METADATA section data";
        return false;
    }

    uint8_t has_exec_class = QoreAOTBinaryReader::readU8(ptr);
    if (has_exec_class) {
        const char* name = reader.readStringRef(ptr);
        if (name && *name) {
            exec_class_name = name;
        }
    }

    return true;
}

void serializeBuildInfo(QoreAOTBinaryWriter& writer,
        const std::vector<std::pair<std::string, std::string>>& info) {
    if (info.empty()) {
        return;
    }

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::BUILD_INFO);
    writer.writeU32(static_cast<uint32_t>(info.size()));
    for (const auto& [key, value] : info) {
        writer.writeStringRef(key.c_str());
        writer.writeStringRef(value.c_str());
    }
    writer.endSection(sec_idx);
}

bool readBuildInfo(const QoreAOTBinaryReader& reader,
        std::vector<std::pair<std::string, std::string>>& info,
        std::string& error) {
    info.clear();

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::BUILD_INFO);
    if (!sec) {
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid BUILD_INFO section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;
    if (ptr + 4 > end) {
        error = "BUILD_INFO section too small for count";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    if (count > (sec->size - 4) / 8) {
        error = "BUILD_INFO count exceeds section capacity";
        return false;
    }
    info.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (ptr + 8 > end) {
            error = "unexpected end of BUILD_INFO section";
            return false;
        }
        const char* key = reader.readStringRef(ptr);
        const char* value = reader.readStringRef(ptr);
        if (!key) {
            error = "invalid BUILD_INFO key at index " + std::to_string(i);
            return false;
        }
        info.emplace_back(key, value ? value : "");
    }

    return true;
}

bool readBuildInfo(const uint8_t* data, uint32_t size,
        std::vector<std::pair<std::string, std::string>>& info,
        std::string& error) {
    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }
    return readBuildInfo(reader, info, error);
}

bool readFallbackSource(const uint8_t* data, uint32_t size, const char*& source, size_t& source_len,
        std::string& error) {
    source = nullptr;
    source_len = 0;

    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::FUNC_SOURCES);
    if (!sec) {
        // No embedded source section - this is OK
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid FUNC_SOURCES section data";
        return false;
    }

    // Read the full source text reference (first item in FUNC_SOURCES section)
    const char* src = reader.readStringRef(ptr);
    if (src && *src) {
        source = src;
        source_len = strlen(src);
    }

    return true;
}

bool compressMetadata(const std::vector<uint8_t>& input,
        std::vector<uint8_t>& output,
        std::string& error) {
    if (input.empty()) {
        // Empty input, still compress it
        output.resize(4);
        // Store original size (0) as 4-byte little-endian
        output[0] = 0;
        output[1] = 0;
        output[2] = 0;
        output[3] = 0;
        return true;
    }

    // Reserve space for original size (4 bytes) + compressed data
    uLongf compressed_size = compressBound(input.size());
    output.resize(4 + compressed_size);

    // Store original size as 4-byte little-endian prefix
    uint32_t orig_size = static_cast<uint32_t>(input.size());
    output[0] = static_cast<uint8_t>(orig_size & 0xFF);
    output[1] = static_cast<uint8_t>((orig_size >> 8) & 0xFF);
    output[2] = static_cast<uint8_t>((orig_size >> 16) & 0xFF);
    output[3] = static_cast<uint8_t>((orig_size >> 24) & 0xFF);

    // Compress into buffer after the size prefix
    int ret = compress2(output.data() + 4, &compressed_size,
                        input.data(), input.size(), 9);

    if (ret != Z_OK) {
        error = "zlib compression failed (error code " + std::to_string(ret) + ")";
        output.clear();
        return false;
    }

    // Trim output to actual compressed size + 4 byte prefix
    output.resize(4 + compressed_size);
    return true;
}

bool decompressMetadata(const uint8_t* input, size_t input_len,
        std::vector<uint8_t>& output,
        std::string& error) {
    if (input_len < 4) {
        error = "compressed metadata too short (need at least 4 bytes for size prefix)";
        return false;
    }

    // Read original size from first 4 bytes (little-endian)
    uint32_t orig_size = static_cast<uint32_t>(input[0]) |
                         (static_cast<uint32_t>(input[1]) << 8) |
                         (static_cast<uint32_t>(input[2]) << 16) |
                         (static_cast<uint32_t>(input[3]) << 24);

    if (orig_size == 0) {
        // Empty metadata
        output.clear();
        return true;
    }

    // Sanity check: decompressed size shouldn't be larger than a reasonable limit
    const size_t MAX_DECOMPRESSED = 100 * 1024 * 1024;  // 100 MB limit
    if (orig_size > MAX_DECOMPRESSED) {
        error = "decompressed metadata size " + std::to_string(orig_size) +
                " exceeds maximum allowed (" + std::to_string(MAX_DECOMPRESSED) + " bytes)";
        return false;
    }

    output.resize(orig_size);
    uLongf dest_len = orig_size;

    // Decompress
    int ret = uncompress(output.data(), &dest_len,
                         input + 4, input_len - 4);

    if (ret != Z_OK) {
        error = "zlib decompression failed (error code " + std::to_string(ret) + ")";
        output.clear();
        return false;
    }

    if (dest_len != orig_size) {
        error = "decompressed size " + std::to_string(dest_len) +
                " does not match expected size " + std::to_string(orig_size);
        output.clear();
        return false;
    }

    return true;
}
