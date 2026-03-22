/* -*- mode: c++ -*- */
/*
  QoreAOTExprNodeRegistry.h

  Qore expression tree node kind registry for AOT binary serialization

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

#ifndef _QORE_AOTEXPRNODEREGISTRY_H

#define _QORE_AOTEXPRNODEREGISTRY_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
class QoreValue;
class QoreProgram;
class QoreAOTContext;

//! Context for expression tree node read operations
struct AOTExprNodeReadCtx {
    const uint8_t*& ptr;           //!< Pointer to deserialization stream (advanced by handlers)
    const uint8_t* end;            //!< End of stream boundary
    QoreProgram* pgm;              //!< Program for class/function/enum resolution
    QoreAOTContext* aot_ctx;       //!< Context with locals, globals, exprs, stmts, regex_cases arrays
    bool& failed;                  //!< Shared failure flag (set by handlers on error, checked after recurse)
    std::function<QoreValue(AOTExprNodeReadCtx&)> recurse;  //!< Callback for recursive deserialization
};

//! Expression tree node handler function type
typedef QoreValue (*AOTExprNodeReadFn)(AOTExprNodeReadCtx& ctx);

//! Registry entry for an expression tree node kind
struct QoreAOTExprNodeKindInfo {
    const char* name;               //!< Human-readable name ("EN_FUNC_CALL", etc.)
    uint8_t kind_value;             //!< AOTExprNodeKind value (0-165)
    bool is_supported;              //!< Whether this kind has deserialization support
    AOTExprNodeReadFn read_fn;       //!< Read handler (nullptr if not supported)
    const char* description;        //!< Brief description of what this kind represents
};

//! Full registry table (256 entries to cover all uint8_t values)
extern const QoreAOTExprNodeKindInfo AOT_EXPR_NODE_KIND_REGISTRY[256];

//! Get registry entry for an expression tree node kind
//! @param kind_byte Raw AOTExprNodeKind value (0-255)
//! @return Pointer to QoreAOTExprNodeKindInfo, or nullptr if undefined
inline const QoreAOTExprNodeKindInfo* getAOTExprNodeKindInfo(uint8_t kind_byte) {
    const auto& info = AOT_EXPR_NODE_KIND_REGISTRY[kind_byte];
    return info.name ? &info : nullptr;
}

#endif // _QORE_AOTEXPRNODEREGISTRY_H
