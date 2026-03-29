/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTInstRegistry.h

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

#ifndef _QORE_INTERN_QOREAOTINSTREGISTRY_H
#define _QORE_INTERN_QOREAOTINSTREGISTRY_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "qore/intern/QoreAOTBinary.h"

class QoreAOTBinaryWriter;
class QoreAOTBinaryReader;
class QoreIRInstruction;
class QoreIRBasicBlock;
class QoreProgram;
class LocalVar;
struct QoreIRValue;
enum class QoreIROpcode : uint16_t;
enum class QoreIRInstGroup : uint8_t;

//! Context for instruction write operations
struct AOTInstWriteCtx {
    QoreAOTBinaryWriter& writer;
    const QoreIRInstruction* inst;
    const std::unordered_map<const QoreIRBasicBlock*, uint16_t>& block_idx;
    const AOTExprWriteFunc& writeExpr;
};

//! Context for instruction read operations
struct AOTInstReadCtx {
    const QoreAOTBinaryReader& reader;
    const uint8_t*& ptr;
    const uint8_t* end;
    const std::vector<std::unique_ptr<QoreIRBasicBlock>>& blocks;
    const std::unordered_map<std::string, class LocalVar*>& local_map;
    const AOTExprReadFunc& readExpr;
    QoreProgram* pgm;
    std::string& error;
    //! Slot-indexed local variable map for disambiguation of same-named variables
    const std::unordered_map<uint32_t, LocalVar*>* slot_to_local = nullptr;

    //! Resolve local variable by slot_id (preferred, avoids name collisions)
    LocalVar* resolveLocalBySlot(uint32_t slot_id) const {
        if (!slot_to_local) {
            return nullptr;
        }
        auto it = slot_to_local->find(slot_id);
        return (it != slot_to_local->end()) ? it->second : nullptr;
    }

    //! Resolve block index to block pointer (0xFFFF = nullptr)
    QoreIRBasicBlock* resolveBlock(uint16_t idx) const {
        if (idx == 0xFFFF) {
            return nullptr;
        }
        if (idx >= blocks.size()) {
            return nullptr;
        }
        return blocks[idx].get();
    }

    //! Resolve local variable name to LocalVar pointer
    LocalVar* resolveLocal(const char* name) const {
        if (!name || !*name) {
            return nullptr;
        }
        auto it = local_map.find(name);
        if (it == local_map.end()) {
            return nullptr;
        }
        return it->second;
    }
};

//! Function pointer type for writing an instruction group
typedef bool (*AOTInstWriteFn)(AOTInstWriteCtx& ctx);

//! Function pointer type for reading an instruction group
typedef std::unique_ptr<QoreIRInstruction> (*AOTInstReadFn)(
    uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
    const std::vector<QoreIRValue>& operands, uint32_t result_id,
    AOTInstReadCtx& ctx);

//! Registry entry for one QoreIRInstGroup
struct QoreIRInstGroupInfo {
    const char* name;           //!< Human-readable name (e.g. "OnBlockExit"), nullptr if undefined
    uint8_t raw_value;          //!< The QoreIRInstGroup enum value
    bool is_serializable;       //!< false for removed groups (44, 50, 51) and unsupported (Context, Summarize)
    bool has_nested_ir;         //!< true for OnBlockExit which nests a full IR function
    AOTInstWriteFn write_fn;    //!< Write group-specific fields (nullptr if not serializable)
    AOTInstReadFn  read_fn;     //!< Read group-specific fields (nullptr if not serializable)
    const char* description;    //!< What this instruction group does
};

//! Size of the instruction group registry (256 to cover all uint8_t values)
constexpr size_t AOT_INST_GROUP_TABLE_SIZE = 256;

//! Registry table indexed by the raw uint8_t group value
//! Entries for undefined groups have nullptr for name, is_serializable=false
//! Defined in QoreAOTInstRegistry.cpp
extern const QoreIRInstGroupInfo AOT_INST_GROUP_REGISTRY[AOT_INST_GROUP_TABLE_SIZE];

//! Look up instruction group info by raw byte value (bounds-safe)
//! @return pointer to info, or nullptr if raw_group is undefined
inline const QoreIRInstGroupInfo* getAOTInstGroupInfo(uint8_t raw_group) {
    const auto& info = AOT_INST_GROUP_REGISTRY[raw_group];
    if (!info.name) {
        return nullptr;
    }
    return &info;
}

#endif // _QORE_INTERN_QOREAOTINSTREGISTRY_H
