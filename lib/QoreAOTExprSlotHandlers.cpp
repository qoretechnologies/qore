/* -*- mode: c++ -*- */
/*
  QoreAOTExprSlotHandlers.cpp

  Expression slot metadata write handlers for AOT binary serialization

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

namespace {

// Forward declarations for recursive functions (will use :: to disambiguate)
// classifyAndWriteExpr is declared in this namespace as extern,
// but the real definition is at global scope in QoreAOTBinary.cpp

// ============================================================================
// Handler Functions for Expression Slot Metadata Serialization
// ============================================================================

//! NEW_OBJECT and SCOPED_NEW_OBJECT
static bool write_slot_NEW_OBJECT(AOTExprSlotWriteCtx& ctx) {
    // ref1 = class path, followed by serialized constructor args.
    // Use classifyAndWriteExpr (not writeValue) so runtime-evaluated args
    // like VarRefNode and hash literals are serialized with slot indices
    // and reconstructed correctly at load time.
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    if (ctx.expr.call_args && ctx.expr.call_args->size() > 0) {
        ctx.writer.writeU8(static_cast<uint8_t>(ctx.expr.call_args->size()));
        for (size_t j = 0; j < ctx.expr.call_args->size(); ++j) {
            ::classifyAndWriteExpr(ctx.writer, ctx.expr.call_args->retrieveEntry(j),
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        }
    } else if (ctx.expr.parse_args && ctx.expr.parse_args->size() > 0) {
        // Fallback to parse_args (VarRefNewObjectNode stores args here, not in call_args)
        uint8_t num_args = ctx.expr.parse_args->size() <= 255
            ? static_cast<uint8_t>(ctx.expr.parse_args->size()) : 0;
        ctx.writer.writeU8(num_args);
        for (uint8_t j = 0; j < num_args; ++j) {
            ::classifyAndWriteExpr(ctx.writer, ctx.expr.parse_args->get(j),
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        }
    } else {
        ctx.writer.writeU8(0);
    }
    return true;
}

//! SCOPED_NEW_OBJECT (same handler as NEW_OBJECT)
static bool write_slot_SCOPED_NEW_OBJECT(AOTExprSlotWriteCtx& ctx) {
    return write_slot_NEW_OBJECT(ctx);
}

//! FUNC_CALL: ref1 = function name
static bool write_slot_FUNC_CALL(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! RUNTIME_CONST_REF: ref1 = constant name
static bool write_slot_RUNTIME_CONST_REF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! LOCAL_VARREF: ref1 = local slot index (as string)
static bool write_slot_LOCAL_VARREF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! GLOBAL_VARREF: ref1 = global slot index (as string)
static bool write_slot_GLOBAL_VARREF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_NUMBER: ref1 = string representation
static bool write_slot_CONST_NUMBER(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_BINARY: ref1 = hex-encoded bytes
static bool write_slot_CONST_BINARY(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_INT: ref1 = i64 value as string
static bool write_slot_CONST_INT(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_FLOAT: ref1 = f64 value as string
static bool write_slot_CONST_FLOAT(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_BOOL: ref1 = "0" or "1"
static bool write_slot_CONST_BOOL(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_STRING: ref1 = string content
static bool write_slot_CONST_STRING(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! HASHDECL_NEW: ref1 = hashdecl path + u8 num_args + N×classifyAndWriteExpr-encoded args
static bool write_slot_HASHDECL_NEW(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    // Serialize constructor args from parse_args (NewHashDeclNode) or call_args (VrnConstruct)
    if (ctx.expr.parse_args && ctx.expr.parse_args->size() > 0) {
        ctx.writer.writeU8(static_cast<uint8_t>(ctx.expr.parse_args->size()));
        for (size_t j = 0; j < ctx.expr.parse_args->size(); ++j) {
            ::classifyAndWriteExpr(ctx.writer, ctx.expr.parse_args->get(j),
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        }
    } else if (ctx.expr.call_args && ctx.expr.call_args->size() > 0) {
        ctx.writer.writeU8(static_cast<uint8_t>(ctx.expr.call_args->size()));
        for (size_t j = 0; j < ctx.expr.call_args->size(); ++j) {
            ::classifyAndWriteExpr(ctx.writer, ctx.expr.call_args->retrieveEntry(j),
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        }
    } else {
        ctx.writer.writeU8(0);
    }
    return true;
}

//! COMPLEX_HASH_NEW: ref1 = type path + u8 num_args + N×classifyAndWriteExpr-encoded args
static bool write_slot_COMPLEX_HASH_NEW(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    // Serialize constructor args from parse_args or call_args
    if (ctx.expr.parse_args && ctx.expr.parse_args->size() > 0) {
        ctx.writer.writeU8(static_cast<uint8_t>(ctx.expr.parse_args->size()));
        for (size_t j = 0; j < ctx.expr.parse_args->size(); ++j) {
            ::classifyAndWriteExpr(ctx.writer, ctx.expr.parse_args->get(j),
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        }
    } else if (ctx.expr.call_args && ctx.expr.call_args->size() > 0) {
        ctx.writer.writeU8(static_cast<uint8_t>(ctx.expr.call_args->size()));
        for (size_t j = 0; j < ctx.expr.call_args->size(); ++j) {
            ::classifyAndWriteExpr(ctx.writer, ctx.expr.call_args->retrieveEntry(j),
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        }
    } else {
        ctx.writer.writeU8(0);
    }
    return true;
}

//! COMPLEX_LIST_NEW: ref1 = type path + u8 num_args + N×classifyAndWriteExpr-encoded args
static bool write_slot_COMPLEX_LIST_NEW(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    // Serialize constructor args from parse_args or call_args
    if (ctx.expr.parse_args && ctx.expr.parse_args->size() > 0) {
        ctx.writer.writeU8(static_cast<uint8_t>(ctx.expr.parse_args->size()));
        for (size_t j = 0; j < ctx.expr.parse_args->size(); ++j) {
            ::classifyAndWriteExpr(ctx.writer, ctx.expr.parse_args->get(j),
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        }
    } else if (ctx.expr.call_args && ctx.expr.call_args->size() > 0) {
        ctx.writer.writeU8(static_cast<uint8_t>(ctx.expr.call_args->size()));
        for (size_t j = 0; j < ctx.expr.call_args->size(); ++j) {
            ::classifyAndWriteExpr(ctx.writer, ctx.expr.call_args->retrieveEntry(j),
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        }
    } else {
        ctx.writer.writeU8(0);
    }
    return true;
}

//! CONST_NOTHING: no additional data
static bool write_slot_CONST_NOTHING(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;  // Unused
    return true;
}

//! CONST_NULL: NULL constant — no payload
static bool write_slot_CONST_NULL(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;  // Unused
    return true;
}

//! SELF_METHOD_CALL: ref1 = class path, ref2 = method name
static bool write_slot_SELF_METHOD_CALL(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    return true;
}

//! STATIC_METHOD_CALL: ref1 = class path, ref2 = method name
static bool write_slot_STATIC_METHOD_CALL(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    // Serialize method args (may contain sub-expressions like string constants)
    if (ctx.expr.call_args && ctx.expr.call_args->size() > 0) {
        ctx.writer.writeU8(static_cast<uint8_t>(ctx.expr.call_args->size()));
        for (size_t j = 0; j < ctx.expr.call_args->size(); ++j) {
            ::classifyAndWriteExpr(ctx.writer, ctx.expr.call_args->retrieveEntry(j),
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        }
    } else if (ctx.expr.parse_args && ctx.expr.parse_args->size() > 0) {
        // Fallback to parse_args when call_args is not yet populated
        uint8_t num_args = ctx.expr.parse_args->size() <= 255
            ? static_cast<uint8_t>(ctx.expr.parse_args->size()) : 0;
        ctx.writer.writeU8(num_args);
        for (uint8_t j = 0; j < num_args; ++j) {
            ::classifyAndWriteExpr(ctx.writer, ctx.expr.parse_args->get(j),
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        }
    } else {
        ctx.writer.writeU8(0);
    }
    return true;
}

//! STATIC_VARREF: ref1 = class path, ref2 = var name
static bool write_slot_STATIC_VARREF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    return true;
}

//! CONST_ENUM: ref1 = enum path, ref2 = member name
static bool write_slot_CONST_ENUM(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    return true;
}

//! SELF_VARREF: ref1 = member name
static bool write_slot_SELF_VARREF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CLOSURE_CREATE: closure signature, params, captured vars, and IR code
static bool write_slot_CLOSURE_CREATE(AOTExprSlotWriteCtx& ctx) {
    // Write flags and class type path
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());  // "lambda,in_method"
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());  // class_type_path

    // Serialize closure signature metadata
    const UserClosureFunction* ucf = ctx.expr.closure_func;
    if (!ucf) {
        return false;
    }
    auto* variant = static_cast<const UserClosureVariant*>(ucf->first());
    const UserSignature* sig = const_cast<UserClosureVariant*>(variant)->getUserSignature();

    // Write return type
    const char* ret_path = sig->getReturnTypeInfo()
        ? QoreTypeInfo::getPath(sig->getReturnTypeInfo()) : "";
    ctx.writer.writeStringRef(ret_path);

    // Write params: count, then (name, type_path, default) per param
    unsigned num_params = sig->numParams();
    ctx.writer.writeU16(static_cast<uint16_t>(num_params));
    for (unsigned p = 0; p < num_params; ++p) {
        const char* pname = sig->getName(p);
        ctx.writer.writeStringRef(pname ? pname : "");
        const char* ptype = sig->getParamTypeInfo(p)
            ? QoreTypeInfo::getPath(sig->getParamTypeInfo(p)) : "";
        ctx.writer.writeStringRef(ptype);
        // Default value: write has_default flag + value if present
        bool has_default = sig->hasDefaultArg(p);
        ctx.writer.writeU8(has_default ? 1 : 0);
        if (has_default) {
            ctx.writer.writeValue(sig->getDefaultArgList()[p]);
        }
    }
    ctx.writer.writeU8(sig->hasVarargs() ? 1 : 0);

    // Write captured variable names and parent slot indices (from LVarSet)
    const LVarSet* vlist = const_cast<UserClosureFunction*>(ucf)->getVList();
    ctx.writer.writeU16(vlist ? static_cast<uint16_t>(vlist->size()) : 0);
    if (vlist) {
        for (LocalVar* lv : *vlist) {
            ctx.writer.writeStringRef(lv->getName());
            // Write parent slot index for disambiguation of same-named variables
            // in different scopes
            int32_t parent_slot = -1;
            for (size_t i = 0; i < ctx.parent_locals.size(); ++i) {
                if (ctx.parent_locals[i].local_var_ptr == lv) {
                    parent_slot = static_cast<int32_t>(i);
                    break;
                }
            }
            ctx.writer.writeU32(static_cast<uint32_t>(parent_slot));
        }
    }

    // Lower closure to IR and serialize
    const QoreIRFunction* closure_ir = const_cast<UserClosureVariant*>(variant)->getCachedIR();
    QoreIRFunction* owned_ir = nullptr;
    if (!closure_ir) {
        // Lower on the fly for serialization
        owned_ir = ::lowerClosureForSerialization(variant);
        closure_ir = owned_ir;
    }

    if (closure_ir) {
        ctx.writer.writeU8(1);  // has_ir
        uint32_t size_pos = ctx.writer.position();
        ctx.writer.writeU32(0);  // placeholder

        auto writeExpr = [&ctx](
                QoreAOTBinaryWriter& w, const QoreValue& e) -> bool {
            return classifyAndWriteExpr(w, e, ctx.parent_locals, ctx.parent_globals,
                ctx.const_reverse_map);
        };

        ::serializeIRFunction(ctx.writer, *closure_ir, writeExpr);
        uint32_t end_pos = ctx.writer.position();
        ctx.writer.patchU32(size_pos, end_pos - size_pos - 4);
    } else {
        delete owned_ir;
        return false;
    }
    delete owned_ir;
    return true;
}

//! CALL_REF: no additional data
static bool write_slot_CALL_REF(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;  // Unused
    return true;
}

//! OBJ_METHOD_REF: no additional data
static bool write_slot_OBJ_METHOD_REF(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;  // Unused
    return true;
}

//! EXPR_TREE: ref1 contains binary tree blob — write inline with length prefix
static bool write_slot_EXPR_TREE(AOTExprSlotWriteCtx& ctx) {
    uint32_t blob_size = static_cast<uint32_t>(ctx.expr.ref1.size());
    ctx.writer.writeU32(blob_size);
    ctx.writer.writeBytes(ctx.expr.ref1.data(), blob_size);
    return true;
}

//! GENERIC_EVAL: no additional data — function needs source fallback
static bool write_slot_GENERIC_EVAL(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;  // Unused
    return true;
}

}  // namespace
