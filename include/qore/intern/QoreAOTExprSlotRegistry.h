/* -*- mode: c++ -*- */
/*
  QoreAOTExprSlotRegistry.h

  Qore expression slot metadata registry for AOT binary serialization

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

  Note that the Qore library is dual-licensed under a choice of two
  licenses.
*/

#ifndef _QORE_AOTEXPRSLOTREGISTRY_H

#define _QORE_AOTEXPRSLOTREGISTRY_H

#include <cstdint>
#include <string>
#include <vector>
#include "qore/intern/QoreAOTBinary.h"

// Forward declarations
class QoreAOTBinaryWriter;
struct AOTLocalSlotId;
struct AOTGlobalSlotId;
struct AOTExprSlotId;

//! Context for expression slot write operations
struct AOTExprSlotWriteCtx {
    QoreAOTBinaryWriter& writer;                                //!< Binary writer
    const AOTExprSlotId& expr;                                  //!< Expression slot being serialized
    const std::vector<AOTLocalSlotId>& parent_locals;           //!< Parent function's locals
    const std::vector<AOTGlobalSlotId>& parent_globals;         //!< Parent function's globals
    const AOTConstantReverseMap* const_reverse_map;             //!< Reverse map for constants
};

//! Expression slot handler function type
typedef bool (*AOTExprSlotWriteFn)(AOTExprSlotWriteCtx& ctx);

//! Registry entry for an expression slot kind
struct QoreAOTExprSlotKindInfo {
    const char* name;                   //!< Human-readable name ("NEW_OBJECT", etc.)
    uint8_t kind_value;                 //!< AOTExprKind value
    bool is_supported;                  //!< Whether this kind has serialization support
    AOTExprSlotWriteFn write_fn;        //!< Write handler (nullptr if not supported)
    const char* description;            //!< Brief description
};

//! Get registry entry for an expression slot kind
//! @param kind_byte Raw AOTExprKind value (0-255)
//! @return Pointer to QoreAOTExprSlotKindInfo, or nullptr if out of range
const QoreAOTExprSlotKindInfo* getAOTExprSlotKindInfo(uint8_t kind_byte);

//! Full registry table (256 entries to cover all uint8_t values)
extern const QoreAOTExprSlotKindInfo AOT_EXPR_SLOT_KIND_REGISTRY[256];

#endif // _QORE_AOTEXPRSLOTREGISTRY_H
