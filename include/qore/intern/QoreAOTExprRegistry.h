/* -*- mode: c++ -*- */
/*
  QoreAOTExprRegistry.h

  Qore expression kind registry for AOT binary serialization

  Copyright (C) 2025 Qore Technologies, s.r.o.

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

  Note that the Qore library is dual-licensed under a choice of two
  licenses.
*/

#ifndef _QORE_AOTEXPRREGISTRY_H

#define _QORE_AOTEXPRREGISTRY_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Include QoreAOTBinary.h for type definitions and the typedef for AOTConstantReverseMap
#include "qore/intern/QoreAOTBinary.h"

// Forward declarations
class QoreAOTBinaryWriter;
class QoreAOTBinaryReader;
class QoreValue;
class QoreProgram;
class LocalVar;
class Var;
struct AOTLocalSlotId;
struct AOTGlobalSlotId;
// Note: AOTConstantReverseMap is a typedef in QoreAOTBinary.h; included below

//! Context for expression write operations
struct AOTExprWriteCtx {
    QoreAOTBinaryWriter& writer;
    const QoreValue& expr;
    const std::vector<AOTLocalSlotId>& parent_locals;
    const std::vector<AOTGlobalSlotId>& parent_globals;
    const AOTConstantReverseMap* const_reverse_map;
};

//! Context for expression read operations
struct AOTExprReadCtx {
    const QoreAOTBinaryReader& reader;
    const uint8_t*& ptr;
    const uint8_t* end;
    QoreProgram* pgm;
    std::string& error;
    LocalVar** locals;              //!< Local variable array for LOCAL_VARREF resolution
    int num_locals;                 //!< Number of local variables
    Var** globals;                  //!< Global variable array for GLOBAL_VARREF resolution
    int num_globals;                //!< Number of global variables
};

//! Expression handler function types
typedef bool (*AOTExprWriteFn)(AOTExprWriteCtx& ctx);
typedef QoreValue (*AOTExprReadFn)(AOTExprReadCtx& ctx);

//! Registry entry for an expression kind
struct QoreAOTExprKindInfo {
    const char* name;                   //!< Human-readable name ("FUNC_CALL", etc.)
    uint8_t kind_value;                 //!< AOTExprKind value (0-33, 0xFE, 0xFF)
    bool is_supported;                  //!< Whether this kind has serialization support
    AOTExprWriteFn write_fn;            //!< Write handler (nullptr if not supported)
    AOTExprReadFn read_fn;              //!< Read handler (nullptr if not supported)
    const char* description;            //!< Brief description of what this kind represents
};

//! Get registry entry for an expression kind
//! @param kind_byte Raw AOTExprKind value (0-255)
//! @return Pointer to QoreAOTExprKindInfo, or nullptr if out of range
const QoreAOTExprKindInfo* getAOTExprKindInfo(uint8_t kind_byte);

//! Full registry table (256 entries to cover all uint8_t values)
extern const QoreAOTExprKindInfo AOT_EXPR_KIND_REGISTRY[256];

#endif // _QORE_AOEXPRREGISTRY_H
