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
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/QoreTypeInfo.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/typed_hash_decl_private.h"
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
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/QoreExistsOperatorNode.h"
#include "qore/intern/QoreImplicitArgumentNode.h"
#include "qore/intern/QoreMinusOperatorNode.h"
#include "qore/intern/QoreKeysOperatorNode.h"
#include "qore/intern/QoreMultiplicationOperatorNode.h"
#include "qore/intern/QoreDivisionOperatorNode.h"
#include "qore/intern/QoreModuloOperatorNode.h"
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
#include "qore/intern/QoreLogicalEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotOperatorNode.h"
#include "qore/intern/QoreNullCoalescingOperatorNode.h"
#include "qore/intern/QoreValueCoalescingOperatorNode.h"
#include "qore/intern/QoreQuestionMarkOperatorNode.h"
#include "qore/intern/QoreFoldlOperatorNode.h"
#include "qore/intern/QoreMapOperatorNode.h"
#include "qore/intern/QoreMapSelectOperatorNode.h"
#include "qore/intern/QoreHashMapOperatorNode.h"
#include "qore/intern/QoreHashMapSelectOperatorNode.h"
#include "qore/intern/QoreSelectOperatorNode.h"
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
#include "qore/intern/ContextrefNode.h"
#include "qore/intern/ContextRowNode.h"
#include "qore/intern/ComplexContextrefNode.h"
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
#include "qore/intern/QoreAOTExprNodeRegistry.h"

#include <qore/QoreObject.h>

#include <cassert>
#include <cstring>
#include <deque>
#include <numeric>
#include <unordered_set>
#include <zlib.h>

static thread_local std::string qore_aot_expr_serialization_error;

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
    msg += "; add a native AOTExprKind serializer/reader or lower this operation to native IR";
    return msg;
}

// Defined in Function.cpp - collects all local variables from a StatementBlock and nested blocks
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);

// Forward-reference class lookup map used by readValue VT_NEW_OBJECT during
// AOT class deserialization. The deserializer populates this as each class is
// added to its namespace so that instance-member init expressions of the form
// `OtherClass m()` can resolve the target class even before it has been
// committed into the root namespace's clmap (which happens after the full
// classes pass). The pointer is installed in an RAII scope from
// deserializeClasses() and cleared on exit.
static thread_local const std::unordered_map<std::string, QoreClass*>*
    g_aot_pending_class_map = nullptr;

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

    // Try to resolve class: first in committed program, then in the
    // in-progress class map populated during deserializeClasses.
    const QoreClass* qc = nullptr;
    {
        ExceptionSink xs;
        qc = getProgram()->findClass(class_path, &xs);
        if (xs.isException()) {
            xs.clear();
        }
    }
    if (!qc && g_aot_pending_class_map) {
        auto it = g_aot_pending_class_map->find(class_path);
        if (it != g_aot_pending_class_map->end()) {
            qc = it->second;
        }
    }
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

static QoreValue aot_resolve_constant_path_tail(QoreValue cur, const std::string& encoded, size_t pos) {
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
        QoreValue rv = aot_resolve_constant_path_tail(base, encoded_path, tail_pos);
        if (!rv && xsink) {
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
        bool defer_if_pending, bool wrap_top_level_if_ready) {
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
            if (pos == encoded.size()) {
                return QoreValue(new RuntimeConstantRefNode(&loc_builtin, ce, true));
            }
            return QoreValue(new RuntimeConstantPathRefNode(&loc_builtin, ce, encoded, pos));
        }
        return QoreValue();
    }

    QoreValue cur = ce->getReferencedValue();
    QoreValue rv = aot_resolve_constant_path_tail(cur, encoded, pos);
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
    // For otherwise-unserializable pointer-backed values (e.g. a QoreObject
    // produced by parse-time constant folding of `{"int": IntType}`), check
    // whether the node pointer is registered in the program reverse map.
    // If so, write as VT_CONST_REF so the reference can be resolved at load
    // time. This is skipped for container types (list/hash) and scope refs
    // which have their own dedicated encodings below; checking only the
    // types that would otherwise fall into the default-NOTHING case avoids
    // accidentally rewriting a constant's own top-level value as a
    // self-reference (only leaf objects make it here).
    if (const_reverse_map && v.hasNode()) {
        qore_type_t nt = v.getType();
        if (nt == NT_OBJECT) {
            const AbstractQoreNode* node = v.getInternalNode();
            if (const std::string* path = aotFindConstantReverseMapPath(const_reverse_map, node)) {
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_CONST_REF));
                writeU32(static_cast<uint32_t>(path->size()));
                writeStringRef(path->c_str(), path->size());
                return true;
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
        case NT_FLOAT: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_FLOAT64));
            writeF64(v.getAsFloat());
            return true;
        }
        case NT_STRING: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_STRING));
            const QoreStringNode* str = v.get<const QoreStringNode>();
            if (str) {
                writeU32(static_cast<uint32_t>(str->size()));
                writeStringRef(str->c_str(), str->size());
            } else {
                writeU32(0);
                writeStringRef("", 0);
            }
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
        case NT_LIST: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_LIST));
            const QoreListNode* list = v.get<const QoreListNode>();
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
            if (socn && socn->oc) {
                std::string class_path = socn->oc->getNamespacePath();
                const QoreListNode* args = socn->getArgs();
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
                const char* type_path = QoreTypeInfo::getPath(ncl->typeInfo);
                if (!type_path) {
                    type_path = "";
                }
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
                uint32_t tlen = static_cast<uint32_t>(strlen(type_path));
                writeU32(tlen);
                writeStringRef(type_path, tlen);
                writeU32(nargs);
                for (uint32_t i = 0; i < nargs; ++i) {
                    writeValue(arg_list->retrieveEntry(i));
                }
                return true;
            }
            // NewComplexHashNode: `hash<K, V> m();` default-constructed complex hash
            if (auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
                const char* type_path = QoreTypeInfo::getPath(nch->typeInfo);
                if (!type_path) {
                    type_path = "";
                }
                uint32_t nargs = nch->args ? static_cast<uint32_t>(nch->args->size()) : 0;
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT));
                writeU8(1); // kind: 1 = complex hash
                uint32_t tlen = static_cast<uint32_t>(strlen(type_path));
                writeU32(tlen);
                writeStringRef(type_path, tlen);
                writeU32(nargs);
                for (uint32_t i = 0; i < nargs; ++i) {
                    writeValue(nch->args->get(i));
                }
                return true;
            }
            // NewHashDeclNode: `<MyHashdecl> m();` default-constructed hashdecl
            if (auto* nhd = dynamic_cast<const NewHashDeclNode*>(node)) {
                const TypedHashDecl* hd = nhd->hd;
                std::string ns_path = hd ? hd->getNamespacePath() : std::string();
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
            return QoreValue(new QoreStringNode(str, len, QCS_UTF8));
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
            return QoreValue(DateTimeNode::makeRelative(year, month, day, hour, minute, second, us));
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

        case QoreAOTValueTag::VT_LIST: {
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
                list->push(elem, nullptr);
            }
            return QoreValue(list.release());
        }

        case QoreAOTValueTag::VT_HASH: {
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
                hash->setKeyValue(key, val, nullptr);
            }
            return QoreValue(hash.release());
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

            // Resolve the class. Try the program's committed-class map
            // first; fall back to the deserializer's in-progress class map
            // for forward references (e.g. `OtherClass m();` in a class that
            // is deserialized before `OtherClass`). The in-progress map is
            // installed by deserializeClasses() via g_aot_pending_class_map.
            ExceptionSink xsink;
            const QoreClass* qc = getProgram()->findClass(class_path, &xsink);
            if (xsink.isException()) {
                xsink.clear();
            }
            if (!qc && g_aot_pending_class_map) {
                auto it = g_aot_pending_class_map->find(class_path);
                if (it != g_aot_pending_class_map->end()) {
                    qc = it->second;
                }
            }
            if (!qc) {
                printd(0, "AOT readValue VT_NEW_OBJECT: cannot resolve class '%s'\n",
                    class_path ? class_path : "(null)");
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
            // kind 0 or 1: resolve complex list/hash type
            // qore_get_type_from_string_intern handles `list<T>`, `hash<T>`,
            // etc. by looking up any class/hashdecl refs via the current
            // program. We don't install ProgramRuntimeParseAccessHelper here:
            // by the time readValue runs for a class member default, the
            // current program is already the active program and taking parse
            // access during deserialization can corrupt runtime state.
            const QoreTypeInfo* ti = qore_get_type_from_string_intern(type_path);
            if (!ti) {
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
            QoreValue rv = qore_aot_resolve_constant_path_value(pgm, fqn, false, wrap_const_ref_in_rcr);
            if (!rv) {
                printd(0, "AOT readValue: cannot resolve const_ref '%s'\n", fqn);
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
    {"bool",            &boolTypeInfo},
    {"date",            &dateTypeInfo},
    {"binary",          &binaryTypeInfo},
    {"hash",            &hashTypeInfo},
    {"list",            &listTypeInfo},
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
    {"softbool",        &softBoolTypeInfo},
    {"softdate",        &softDateTypeInfo},
    {"softlist",        &softListTypeInfo},
    {"*int",            &bigIntOrNothingTypeInfo},
    {"*float",          &floatOrNothingTypeInfo},
    {"*number",         &numberOrNothingTypeInfo},
    {"*string",         &stringOrNothingTypeInfo},
    {"*bool",           &boolOrNothingTypeInfo},
    {"*date",           &dateOrNothingTypeInfo},
    {"*binary",         &binaryOrNothingTypeInfo},
    {"*hash",           &hashOrNothingTypeInfo},
    {"*list",           &listOrNothingTypeInfo},
    {"*object",         &objectOrNothingTypeInfo},
    {"*data",           &dataOrNothingTypeInfo},
    {"*code",           &codeOrNothingTypeInfo},
    {"*reference",      &referenceOrNothingTypeInfo},
    {"*timeout",        &timeoutOrNothingTypeInfo},
    {"*softint",        &softBigIntOrNothingTypeInfo},
    {"*softfloat",      &softFloatOrNothingTypeInfo},
    {"*softnumber",     &softNumberOrNothingTypeInfo},
    {"*softstring",     &softStringOrNothingTypeInfo},
    {"*softbool",       &softBoolOrNothingTypeInfo},
    {"*softdate",       &softDateOrNothingTypeInfo},
    {"*softlist",       &softListOrNothingTypeInfo},
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
    ExceptionSink xsink;
    const QoreClass* qc = pgm->findClass(path, &xsink);
    if (xsink.isException()) {
        xsink.clear();
    }
    if (qc) {
        return qc->getTypeInfo();
    }
    return nullptr;
}

const QoreTypeInfo* QoreAOTTypeResolver::resolveHashDeclType(const char* path) {
    if (!pgm) {
        return nullptr;
    }
    // path format: "hash<DeclName>" or "*hash<DeclName>" — extract the hashdecl name and resolve it
    // directly to the registered TypedHashDecl.  Do not route this through the generic string parser:
    // anchored and unanchored paths such as hash<::SqlUtil::QueryInfo> and hash<SqlUtil::QueryInfo>
    // can otherwise produce distinct QoreTypeInfo objects for the same hashdecl.
    bool or_nothing = false;
    const char* hash_path = path;
    if (!strncmp(path, "*hash<", 6)) {
        or_nothing = true;
        hash_path = path + 1;
    }
    if (strncmp(hash_path, "hash<", 5)) {
        return nullptr;
    }
    const char* start = hash_path + 5;
    const char* end = strchr(start, '>');
    if (!end) {
        return nullptr;
    }
    std::string decl_name(start, end - start);
    if (decl_name.find(',') != std::string::npos) {
        return nullptr;
    }
    while (decl_name.rfind("::", 0) == 0) {
        decl_name.erase(0, 2);
    }

    const QoreNamespace* pns = nullptr;
    const TypedHashDecl* thd = pgm->findHashDecl(decl_name.c_str(), pns);
    if (thd) {
        return thd->getTypeInfo(or_nothing);
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

const QoreTypeInfo* QoreAOTTypeResolver::resolveComplexType(const char* path) {
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
            // Look up the class in the program's namespace tree
            qore_program_private* pp = qore_program_private::get(*pgm);
            qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
            // runtimeFindClass searches the namespace tree directly
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path.c_str());
            if (qc) {
                return qc->getOrNothingTypeInfo();
            }
        }
    }

    // Handle *object<ClassName> patterns (or-nothing class types)
    if (strncmp(path, "*object<", 8) == 0) {
        const char* start = path + 8;
        const char* end = strrchr(start, '>');
        if (end && end > start) {
            std::string class_path(start, end - start);
            qore_program_private* pp = qore_program_private::get(*pgm);
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path.c_str());
            if (qc) {
                return qc->getOrNothingTypeInfo();
            }
        }
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

//! Get type path string from QoreTypeInfo, handling null
static const char* getTypePath(const QoreTypeInfo* ti, bool no_narrow = false) {
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
    return ti ? QoreTypeInfo::getPath(ti) : "";
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
};

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
static inline bool shouldSkipByCompileFile(const char* item_file, const char* compile_file) {
    if (!compile_file) {
        return false;
    }
    if (!item_file) {
        return false;  // conservative: include unattributed items
    }
    return strcmp(item_file, compile_file) != 0;
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
*/
static void collectItems(AOTSerializeState& state, qore_ns_private* ns, uint32_t parent_idx,
        const char* current_module, const std::unordered_set<std::string>* keep_modules = nullptr,
        const char* compile_file = nullptr) {
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
                if (compile_file && priv->loc
                        && shouldSkipByCompileFile(priv->loc->getFile(), compile_file)) {
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
                if (compile_file) {
                    const QoreProgramLocation* hd_loc =
                        typed_hash_decl_private::get(*hd)->getParseLocation();
                    if (hd_loc && shouldSkipByCompileFile(hd_loc->getFile(), compile_file)) {
                        continue;
                    }
                }
                state.hashdecls.push_back({hd, ns_idx});
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
                if (compile_file) {
                    const QoreProgramLocation* ed_loc =
                        qore_enum_decl_private::get(*ed)->getParseLocation();
                    if (ed_loc && shouldSkipByCompileFile(ed_loc->getFile(), compile_file)) {
                        continue;
                    }
                }
                state.enums.push_back({ed, ns_idx});
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
            if (compile_file && ti.second->loc
                    && shouldSkipByCompileFile(ti.second->loc->getFile(), compile_file)) {
                continue;
            }
            state.typedefs.push_back({ti.first, ti.second->typeInfo, ti.second->pub, ns_idx});
        }
    }

    // Collect user constants
    {
        ConstantListIterator cli(ns->constant);
        while (cli.next()) {
            ConstantEntry* ce = cli.getEntry();
            if (!ce->isSystem()) {
                // Filter out constants from reexported dependencies
                const char* const_module = ce->getModuleName();
                if (shouldSkipReexportedItem(const_module, current_module, keep_modules)) {
                    continue;
                }
                if (compile_file && ce->loc
                        && shouldSkipByCompileFile(ce->loc->getFile(), compile_file)) {
                    continue;
                }
                state.constants.push_back({ce, ns_idx});
            }
        }
    }

    // Collect user global variables
    for (auto& vi : ns->var_list.vmap) {
        Var* var = vi.second;
        if (!var->isBuiltin()) {
            // Filter out globals from reexported dependencies
            const char* var_module = var->getModuleName();
            if (shouldSkipReexportedItem(var_module, current_module, keep_modules)) {
                continue;
            }
            if (compile_file) {
                const QoreProgramLocation* v_loc = var->getParseLocation();
                if (v_loc && shouldSkipByCompileFile(v_loc->getFile(), compile_file)) {
                    continue;
                }
            }
            state.globals.push_back({var, ns_idx});
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
            if (compile_file) {
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
                    if (!vloc || !shouldSkipByCompileFile(vloc->getFile(), compile_file)) {
                        any_in_file = true;
                        break;
                    }
                }
                if (!any_in_file) {
                    continue;
                }
            }
            state.functions.push_back({entry, func, ns_idx});
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
            collectItems(state, child_priv, ns_idx, current_module, keep_modules, compile_file);
        }
    }
}

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
    writer.writeU32(writer.internTypePath(getTypePath(sig->getReturnTypeInfo())));

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
    int16_t sig_first = 0, sig_last = 0;
    if (UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(v)->getUserVariantBase()) {
        if (UserSignature* usig = uvb->getUserSignature()) {
            if (const QoreProgramLocation* vloc = usig->getParseLocation()) {
                sig_first = vloc->start_line;
                sig_last  = vloc->end_line;
            }
        }
    }
    writer.writeU16(static_cast<uint16_t>(sig_first));
    writer.writeU16(static_cast<uint16_t>(sig_last));

    // params
    const arg_vec_t& defaults = sig->getDefaultArgList();
    for (uint32_t i = 0; i < np; ++i) {
        // param name
        const char* pname = sig->getName(i);
        writer.writeStringRef(pname ? pname : "");

        // param type path — u32 index into per-blob TYPE_TABLE (see
        // return-type comment above).
        writer.writeU32(writer.internTypePath(getTypePath(sig->getParamTypeInfo(i))));

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
                    if (socn->oc && (!args || args->empty())) {
                        writer.writeU8(1);
                        writer.writeValue(dv);
                        continue;
                    }
                }
            }

            if (dv.isNothing() || dv.isNull() || dt == NT_BOOLEAN || dt == NT_INT
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
                        ConstantEntry* ce = rcr->getConstantEntry();
                        if (ce) {
                            if (ce->val.hasNode()) {
                                auto it2 = writer.const_reverse_map->find(ce->val.getInternalNode());
                                if (it2 != writer.const_reverse_map->end()) {
                                    return it2->second;
                                }
                            }
                            QoreValue sv = ce->getReferencedValue();
                            std::string rv;
                            if (sv.hasNode()) {
                                auto it2 = writer.const_reverse_map->find(sv.getInternalNode());
                                if (it2 != writer.const_reverse_map->end()) {
                                    rv = it2->second;
                                }
                            }
                            sv.discard(nullptr);
                            return rv;
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
                        // getNamespacePath(): see QoreAOT.cpp NewObjectCallNode.
                        writer.writeStringRef(qc->getNamespacePath().c_str());
                        writer.writeStringRef(mname);
                        continue;
                    }
                }

                // Fallback: unclassifiable complex expression — write opaque
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

    switch (v.getType()) {
        case NT_BOOLEAN:
        case NT_INT:
        case NT_FLOAT:
        case NT_STRING:
        case NT_DATE:
        case NT_NUMBER:
        case NT_BINARY:
        case NT_LIST:
        case NT_HASH:
        case NT_OBJECT:
        case NT_SCOPE_REF:
            return true;
        default:
            return v.isEnum();
    }
}

static bool writeMemberDefaultValue(QoreAOTBinaryWriter& writer, const QoreValue& v) {
    if (!aotValueTagPreservesMemberDefault(v)) {
        AOTSlotMap slots;
        std::vector<uint8_t> blob;
        if (serializeExprTreeToBlob(v, slots, blob, false, writer.const_reverse_map)
                && !blob.empty()) {
            writer.writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_EXPR_TREE));
            writer.writeU32(static_cast<uint32_t>(blob.size()));
            writer.writeBytes(blob.data(), static_cast<uint32_t>(blob.size()));
            return true;
        }
    }

    writer.writeValue(v);
    return false;
}

//! Write CLASSES section
static void writeClassesSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::CLASSES);

    uint32_t count = static_cast<uint32_t>(state.classes.size());
    writer.writeU32(count);

    for (auto& ci : state.classes) {
        const qore_class_private* priv = ci.priv;

        // name and path
        writer.writeStringRef(priv->name.c_str());
        writer.writeStringRef(priv->path.c_str());
        writer.writeU32(ci.ns_idx);

        // flags: bit 0 = pub, bit 1 = final
        uint16_t flags = 0;
        if (priv->pub) {
            flags |= 0x0001;
        }
        if (priv->final) {
            flags |= 0x0002;
        }
        writer.writeU16(flags);

        // domain
        writer.writeI64(priv->domain);

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
        for (auto& mi : priv->members.member_list) {
            if (!mi.second->local()) {
                continue;
            }
            writer.writeStringRef(mi.first);
            writer.writeStringRef(getTypePath(mi.second->getTypeInfo()));
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
                writeMemberDefaultValue(writer, mi.second->exp);
            } else {
                writer.writeU8(0);
            }
        }

        // static members
        uint32_t num_static = static_cast<uint32_t>(priv->vars.size());
        writer.writeU32(num_static);
        for (auto& vi : priv->vars.member_list) {
            writer.writeStringRef(vi.first);
            writer.writeStringRef(getTypePath(vi.second->getTypeInfo()));
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
                writeMemberDefaultValue(writer, vi.second->exp);
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
                if (!ccli.getEntry()->isSystem()) {
                    ++num_consts;
                }
            }
        }
        writer.writeU32(num_consts);
        {
            ConstConstantListIterator ccli(priv->constlist);
            while (ccli.next()) {
                const ConstantEntry* ce = ccli.getEntry();
                if (!ce->isSystem()) {
                    writer.writeStringRef(ce->getName());
                    writer.writeStringRef(getTypePath(ce->typeInfo));
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
                        writer.writeValue(actual_val);
                        actual_val.discard(nullptr);
                    }
                }
            }
        }
    }

    writer.endSection(sec_idx);
}

//! Write HASHDECLS section
static void writeHashDeclsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
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
                writer.writeStringRef(getTypePath(mi.second->getTypeInfo()));
                if (mi.second->exp) {
                    const QoreValue v = mi.second->exp;
                    qore_type_t t = v.getType();
                    writer.writeU8(1);
                    bool wrote_expr_tree = writeMemberDefaultValue(writer, mi.second->exp);
                    if (!wrote_expr_tree && !aotValueTagPreservesMemberDefault(v)) {
                        fprintf(stderr,
                            "warning: AOT serialisation of hashdecl '%s' member '%s' "
                            "default expression may lose information (type %d = %s); "
                            "loaded value will default to the type's zero-value "
                            "(0 for int, empty for string, etc.).  Consider using a "
                            "simple literal default and moving the computation into "
                            "an init() method.\n",
                            hd->getName(), mi.first, t, get_type_name(v.getInternalNode()));
                    }
                } else {
                    writer.writeU8(0);
                }
            }
        }
    }

    writer.endSection(sec_idx);
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
        writer.writeStringRef(getTypePath(ed->getBaseTypeInfo()));

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
        writer.writeStringRef(getTypePath(ti.typeInfo));
        writer.writeU32(ti.ns_idx);
        writer.writeU8(ti.pub ? 1 : 0);
    }

    writer.endSection(sec_idx);
}

//! Write CONSTANTS section (namespace-level constants only; class constants are in CLASSES)
static void writeConstantsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::CONSTANTS);

    uint32_t count = static_cast<uint32_t>(state.constants.size());
    writer.writeU32(count);

    for (auto& ci : state.constants) {
        const ConstantEntry* ce = ci.entry;
        writer.writeStringRef(ce->getName());
        writer.writeStringRef(getTypePath(ce->typeInfo));
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
    }

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
        writer.writeStringRef(getTypePath(var->getTypeInfo(), var->isNoNarrowing()));
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
        QoreValue v = nsi.getValue();
        if (!v.hasNode()) {
            continue;
        }
        std::string ns_path = ns->path;
        if (ns_path.size() >= 2) {
            ns_path = ns_path.substr(2);  // strip leading "::"
        }
        std::string fqn = ns_path.empty() ? nsi.getName() : ns_path + "::" + nsi.getName();
        qore_aot_add_constant_value_reverse_mappings(crm, v, fqn);
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
            QoreValue v = cci.getValue();
            if (!v.hasNode()) {
                continue;
            }
            std::string fqn = class_prefix + cci.getName();
            qore_aot_add_constant_value_reverse_mappings(crm, v, fqn);
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
        QoreValue v = cci.getValue();
        if (!v.hasNode()) {
            continue;
        }
        std::string fqn = class_prefix + cci.getName();
        qore_aot_add_constant_value_reverse_mappings(crm, v, fqn);
    }

    // Add namespace constants from the class's enclosing namespace hierarchy
    if (cls_priv->ns) {
        const qore_ns_private* ns_priv = cls_priv->ns;
        while (ns_priv) {
            // Iterate namespace constants using the ConstantList directly
            ConstConstantListIterator nsi(ns_priv->constant);
            while (nsi.next()) {
                QoreValue v = nsi.getValue();
                if (!v.hasNode()) {
                    continue;
                }
                std::string ns_path = ns_priv->path;
                if (ns_path.size() >= 2) {
                    ns_path = ns_path.substr(2);  // strip leading "::"
                }
                std::string fqn = ns_path.empty() ? nsi.getName() : ns_path + "::" + nsi.getName();
                qore_aot_add_constant_value_reverse_mappings(crm, v, fqn);
            }
            // Walk up to parent namespace
            ns_priv = ns_priv->parent;
        }
    }

    return crm;
}

//! Write METHODS section
static void writeMethodsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
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
                if (v->isUser()) {
                    ++num_variants;
                }
            }
        }
        writer.writeU32(num_variants);

        // write user variant signatures
        {
            bool is_constructor = strcmp(method->getName(), "constructor") == 0;
            bool debug = getenv("QORE_AOT_DEBUG") != nullptr;
            QoreFunctionIterator qfi(*static_cast<const QoreFunction*>(mfb));
            while (qfi.next()) {
                const AbstractQoreFunctionVariant* v = qfi.getVariant();
                if (v->isUser()) {
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
                    writer.writeU8(mflags);
                    writeVariantSignature(writer, v);

                    // Serialize BCA (Base Class Constructor Arguments) for constructors
                    if (is_constructor) {
                        const ConstructorMethodVariant* cmv = CONMV_const(mvb);
                        const BCAList* bcal = cmv->getBaseClassArgumentList();
                        if (bcal && !bcal->empty()) {
                            writer.writeU8(1);  // has_bca = true
                            writer.writeU16(static_cast<uint16_t>(bcal->size()));

                            // Build slot map from constructor's signature params
                            // Must use dynamic_cast due to multiple inheritance:
                            // UserConstructorVariant inherits both ConstructorMethodVariant
                            // (via MethodVariantBase -> AbstractQoreFunctionVariant) and
                            // UserVariantBase. reinterpret_cast gives wrong pointer offset.
                            const UserConstructorVariant* ucv =
                                dynamic_cast<const UserConstructorVariant*>(cmv);
                            const UserSignature* sig = ucv
                                ? const_cast<UserConstructorVariant*>(ucv)->getUserSignature()
                                : nullptr;
                            AOTSlotMap bca_slots;
                            if (sig) {
                                for (unsigned i = 0; i < sig->numParams(); ++i) {
                                    bca_slots.local_slots[reinterpret_cast<const void*>(sig->lv[i])] = i;
                                }
                                // Also map selfid if present
                                if (sig->selfid) {
                                    bca_slots.local_slots[reinterpret_cast<const void*>(sig->selfid)] =
                                        static_cast<int32_t>(sig->numParams());
                                }
                                // Also map argvid if present
                                if (sig->argvid) {
                                    bca_slots.local_slots[reinterpret_cast<const void*>(sig->argvid)] =
                                        static_cast<int32_t>(sig->numParams() + (sig->selfid ? 1 : 0));
                                }
                            }

                            for (const BCANode* bca : *bcal) {
                                // Write base class path for runtime resolution
                                const QoreClass* base_cls = nullptr;
                                if (bca->classid) {
                                    const qore_class_private* cls_priv =
                                        qore_class_private::get(*mi.method->getClass());
                                    if (cls_priv->scl) {
                                        ClassAccess access;
                                        base_cls = cls_priv->scl->getClass(
                                            bca->classid, access, true);
                                    }
                                }
                                // getNamespacePath() walks the live namespace tree; see QoreAOT.cpp
                                // NewObjectCallNode note.
                                writer.writeStringRef(base_cls
                                    ? base_cls->getNamespacePath().c_str() : "");

                                // Serialize args as individual EXPR_TREE blobs
                                const QoreListNode* args = bca->getArgs();
                                uint16_t num_args = args ? static_cast<uint16_t>(args->size()) : 0;
                                writer.writeU16(num_args);

                                for (uint16_t ai = 0; ai < num_args; ++ai) {
                                    QoreValue arg_val = args->retrieveEntry(ai);
                                    std::vector<uint8_t> blob;
                                    if (serializeExprTreeToBlob(arg_val, bca_slots, blob, debug, &program_crm)) {
                                        writer.writeU32(static_cast<uint32_t>(blob.size()));
                                        if (!blob.empty()) {
                                            writer.writeBytes(blob.data(),
                                                static_cast<uint32_t>(blob.size()));
                                        }
                                    } else {
                                        printd(0, "AOT: failed to serialize BCA arg %d for %s::%s\n",
                                            ai, mi.method->getClass()->getName(), method->getName());
                                        // Write empty blob to keep format consistent
                                        writer.writeU32(0);
                                    }
                                }
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
}

} // anonymous namespace

//! Lower a closure variant to IR for serialization
/** Follows the same pattern as buildContextForVariant() in QoreAOTRuntime.cpp.
    @param variant the closure variant to lower
    @return heap-allocated IR function, or nullptr on failure (caller owns)
*/
QoreIRFunction* lowerClosureForSerialization(const UserClosureVariant* variant) {
    StatementBlock* sb = const_cast<UserClosureVariant*>(variant)->getStatementBlock();
    if (!sb) {
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
        delete ir;
        return nullptr;
    }

    // Compile handler IRs for try/catch blocks inside the closure.
    // Failure aborts closure IR lowering (the runtime asserts handler_ir
    // is populated — see executeHandlerBody).
    std::string handler_error;
    if (lowering.compileAllHandlerIRs(handler_error) < 0) {
        printd(2, "AOT: closure handler IR compilation failed: %s\n", handler_error.c_str());
        delete ir;
        return nullptr;
    }

    // Collect body locals
    collectAllStatementLocals(sb, ir->all_body_locals);

    ir->computeSlotIdsAndEmbed();
    return ir;
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
    auto trace_generic_eval = [&expr](const char* reason, const AbstractQoreNode* node) {
        if (!getenv("QORE_AOT_TRACE_GENERIC_EVAL")) {
            return;
        }
        const char* object_class = "";
        if (auto* obj = dynamic_cast<const QoreObject*>(node)) {
            object_class = obj->getClassName();
        }
        fprintf(stderr, "[aot-generic-eval] %s qtype=%d node=%p node_type=%s needs_eval=%d\n",
            reason, expr.getType(), static_cast<const void*>(node), node ? node->getTypeName() : "<none>",
            node ? (node->needs_eval() ? 1 : 0) : (expr.needsEval() ? 1 : 0));
        if (*object_class) {
            fprintf(stderr, "[aot-generic-eval] object_class=%s\n", object_class);
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
        // Handle inline primitive values
        switch (expr.getType()) {
            case NT_INT: {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_INT));
                writer.writeI64(expr.getAsBigInt());
                return true;
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
        trace_generic_eval("unsupported inline non-node expression", nullptr);
        qoreAOTSetExprSerializationError("unsupported inline native AOT expression for "
            + qoreAOTDescribeExpr(expr)
            + "; add a native AOTExprKind serializer/reader or lower this operation to native IR");
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL));
        return true;
    }

    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        trace_generic_eval("missing inline expression node", nullptr);
        qoreAOTSetExprSerializationError("unsupported inline native AOT expression for "
            + qoreAOTDescribeExpr(expr)
            + "; add a native AOTExprKind serializer/reader or lower this operation to native IR");
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL));
        return true;
    }

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
        const FunctionEntry* fe = call->getFunctionEntry();
        if (fe && fe->getNamespace()) {
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
        const QoreListNode* args = call->getArgs();
        const QoreParseListNode* pargs = call->getParseArgs();
        size_t nargs = args ? args->size() : (pargs ? pargs->size() : 0);
        if (nargs > 255) {
            qoreAOTSetExprSerializationError("FUNC_CALL inline payload has more than 255 arguments");
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

    // SelfFunctionCallNode: method call on self
    if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_METHOD_CALL));
        const QoreMethod* method = call->getMethod();
        if (method) {
            const QoreClass* qc = method->getClass();
            // getNamespacePath(): see QoreAOT.cpp NewObjectCallNode.
            writer.writeStringRef(qc ? qc->getNamespacePath().c_str() : "");
        } else {
            writer.writeStringRef("");
        }
        // Strip class prefix from method name if present (e.g., "LoggerWrapper::debug" → "debug")
        const char* mname = call->getName();
        const char* last_sep = strrchr(mname, ':');
        writer.writeStringRef((last_sep && last_sep > mname && *(last_sep - 1) == ':') ? last_sep + 1 : mname);
        return true;
    }

    // StaticMethodCallNode: static method call
    if (auto* call = dynamic_cast<const StaticMethodCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_METHOD_CALL));
        const QoreMethod* method = call->getMethod();
        if (method) {
            const QoreClass* qc = method->getClass();
            // getNamespacePath(): see QoreAOT.cpp NewObjectCallNode.
            writer.writeStringRef(qc ? qc->getNamespacePath().c_str() : "");
        } else {
            writer.writeStringRef("");
        }
        writer.writeStringRef(call->getName());
        // Serialize method args (must match read_expr_static_method_call format)
        const QoreListNode* args = call->getArgs();
        if (args && args->size() > 0) {
            writer.writeU8(static_cast<uint8_t>(args->size()));
            for (size_t j = 0; j < args->size(); ++j) {
                classifyAndWriteExpr(writer, args->retrieveEntry(j),
                    parent_locals, parent_globals, const_reverse_map);
            }
        } else {
            writer.writeU8(0);
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
            // getNamespacePath(): see QoreAOT.cpp NewObjectCallNode.
            writer.writeStringRef(qc->getNamespacePath().c_str());
            const QoreListNode* args = vrn->getArgs();
            if (args && args->size() > 0) {
                writer.writeU8(static_cast<uint8_t>(args->size()));
                for (size_t j = 0; j < args->size(); ++j) {
                    classifyAndWriteExpr(writer, args->retrieveEntry(j),
                        parent_locals, parent_globals, const_reverse_map);
                }
            } else {
                writer.writeU8(0);
            }
            return true;
        }
        // Non-class VarRefNewObjectNode (hashdecl, complex hash/list) — fall through
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
        writer.writeStringRef(sv->qc.getNamespacePath().c_str());
        writer.writeStringRef(sv->str.c_str());
        return true;
    }

    // QoreStringNode: string literal constant (e.g., "" as constructor arg)
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_STRING));
        writer.writeStringRef(str->c_str());
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
        if (socn->oc) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::SCOPED_NEW_OBJECT));
            // getNamespacePath(): see QoreAOT.cpp NewObjectCallNode.
            writer.writeStringRef(socn->oc->getNamespacePath().c_str());
            // Try evaluated args first, fall back to parse args
            const QoreListNode* args = socn->getArgs();
            if (args && args->size() > 0 && args->size() <= 255) {
                writer.writeU8(static_cast<uint8_t>(args->size()));
                for (size_t j = 0; j < args->size(); ++j) {
                    classifyAndWriteExpr(writer, args->retrieveEntry(j), parent_locals, parent_globals, const_reverse_map);
                }
            } else if (const QoreParseListNode* pargs = socn->getParseArgs()) {
                // Args not yet evaluated; serialize from parse-time expressions
                uint8_t num_args = pargs->size() <= 255 ? static_cast<uint8_t>(pargs->size()) : 0;
                writer.writeU8(num_args);
                for (uint8_t j = 0; j < num_args; ++j) {
                    classifyAndWriteExpr(writer, pargs->get(j), parent_locals, parent_globals, const_reverse_map);
                }
            } else {
                writer.writeU8(0);
            }
            return true;
        }
    }

    // NewObjectCallNode: bare constructor call (e.g., "new Foo()")
    if (auto* no = dynamic_cast<const NewObjectCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::NEW_OBJECT));
        const QoreClass* qc = no->getClass();
        // getNamespacePath(): see QoreAOT.cpp NewObjectCallNode.
        writer.writeStringRef(qc ? qc->getNamespacePath().c_str() : "");
        // Serialize constructor args if available
        const QoreListNode* args = no->getArgs();
        if (args && args->size() > 0 && args->size() <= 255) {
            writer.writeU8(static_cast<uint8_t>(args->size()));
            for (size_t j = 0; j < args->size(); ++j) {
                classifyAndWriteExpr(writer, args->retrieveEntry(j), parent_locals, parent_globals, const_reverse_map);
            }
        } else if (const QoreParseListNode* pargs = no->getParseArgs()) {
            uint8_t num_args = pargs->size() <= 255 ? static_cast<uint8_t>(pargs->size()) : 0;
            writer.writeU8(num_args);
            for (uint8_t j = 0; j < num_args; ++j) {
                classifyAndWriteExpr(writer, pargs->get(j), parent_locals, parent_globals, const_reverse_map);
            }
        } else {
            writer.writeU8(0);
        }
        return true;
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
                    classifyAndWriteExpr(writer, it.get(), parent_locals, parent_globals, const_reverse_map);
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
                classifyAndWriteExpr(writer, it.get(), parent_locals, parent_globals, const_reverse_map);
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
                classifyAndWriteExpr(writer, qln->retrieveEntry(i), parent_locals, parent_globals, const_reverse_map);
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
                classifyAndWriteExpr(writer, pln->get(i), parent_locals, parent_globals, const_reverse_map);
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
                    classifyAndWriteExpr(writer, keys[i], parent_locals, parent_globals, const_reverse_map);
                    classifyAndWriteExpr(writer, vals[i], parent_locals, parent_globals, const_reverse_map);
                }
                return true;
            } else {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
                writer.writeU8(static_cast<uint8_t>(keys.size()));
                for (size_t i = 0; i < keys.size(); ++i) {
                    // Keys are typically string constants
                    QoreStringValueHelper key(keys[i]);
                    writer.writeStringRef(key->c_str());
                    classifyAndWriteExpr(writer, vals[i], parent_locals, parent_globals, const_reverse_map);
                }
                return true;
            }
        }
        // Hash too large for u8 count — fall through to GENERIC_EVAL
    }

    // QoreHashObjectDereferenceOperatorNode: hash.key or hash{key} dereference chains
    // (e.g., oh.paths."/create".post — left=base, right=key, both recursively classified)
    if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_DEREF));
        classifyAndWriteExpr(writer, hd->getLeft(), parent_locals, parent_globals, const_reverse_map);
        classifyAndWriteExpr(writer, hd->getRight(), parent_locals, parent_globals, const_reverse_map);
        return true;
    }

    // Plus operator: used inside hash/list/constructor argument literals.
    // Encoding it directly avoids falling back to EXPR_TREE for common
    // expressions such as `hdr + ("Content-Type": content_type)`.
    if (auto* plus = dynamic_cast<const QorePlusOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::PLUS));
        classifyAndWriteExpr(writer, plus->getLeft(), parent_locals, parent_globals,
            const_reverse_map);
        classifyAndWriteExpr(writer, plus->getRight(), parent_locals, parent_globals,
            const_reverse_map);
        return true;
    }

    // Square-bracket operator: required for nested lvalues such as
    // `\hash[key]` carried inside ParseReferenceNode metadata.
    if (auto* sq = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SQUARE_BRACKET));
        classifyAndWriteExpr(writer, sq->getLeft(), parent_locals, parent_globals,
            const_reverse_map);
        classifyAndWriteExpr(writer, sq->getRight(), parent_locals, parent_globals,
            const_reverse_map);
        return true;
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
        classifyAndWriteExpr(writer, minus->getLeft(), parent_locals, parent_globals,
            const_reverse_map);
        classifyAndWriteExpr(writer, minus->getRight(), parent_locals, parent_globals,
            const_reverse_map);
        return true;
    }
    if (auto* multiply = dynamic_cast<const QoreMultiplicationOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::MULTIPLY));
        classifyAndWriteExpr(writer, multiply->getLeft(), parent_locals, parent_globals,
            const_reverse_map);
        classifyAndWriteExpr(writer, multiply->getRight(), parent_locals, parent_globals,
            const_reverse_map);
        return true;
    }
    if (auto* divide = dynamic_cast<const QoreDivisionOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::DIVIDE));
        classifyAndWriteExpr(writer, divide->getLeft(), parent_locals, parent_globals,
            const_reverse_map);
        classifyAndWriteExpr(writer, divide->getRight(), parent_locals, parent_globals,
            const_reverse_map);
        return true;
    }
    if (auto* modulo = dynamic_cast<const QoreModuloOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::MODULO));
        classifyAndWriteExpr(writer, modulo->getLeft(), parent_locals, parent_globals,
            const_reverse_map);
        classifyAndWriteExpr(writer, modulo->getRight(), parent_locals, parent_globals,
            const_reverse_map);
        return true;
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
        const char* type_path = ti ? QoreTypeInfo::getPath(ti) : "";
        if (type_path && *type_path) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::INSTANCEOF));
            writer.writeStringRef(type_path);
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
            writer.writeI64(re->getOptions());
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
        bool ok = classifyAndWriteExpr(writer, prn->getLVExp(), parent_locals, parent_globals, const_reverse_map);
        return ok;
    }

    // NewHashDeclNode: hashdecl construction (e.g., <StatInfo>{"size": 1})
    if (auto* nhd = dynamic_cast<const NewHashDeclNode*>(node)) {
        if (nhd->hd) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASHDECL_NEW));
            writer.writeStringRef(nhd->hd->getNamespacePath().c_str());
            // Serialize constructor args (typically a single hash initializer)
            if (nhd->args && nhd->args->size() > 0) {
                writer.writeU8(static_cast<uint8_t>(nhd->args->size()));
                for (size_t j = 0; j < nhd->args->size(); ++j) {
                    classifyAndWriteExpr(writer, nhd->args->get(j), parent_locals, parent_globals, const_reverse_map);
                }
            } else {
                writer.writeU8(0);
            }
            return true;
        }
    }

    // NewComplexHashNode: complex typed hash construction
    if (auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
        if (nch->typeInfo) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_HASH_NEW));
            writer.writeStringRef(QoreTypeInfo::getPath(nch->typeInfo));
            // Serialize constructor args
            if (nch->args && nch->args->size() > 0) {
                writer.writeU8(static_cast<uint8_t>(nch->args->size()));
                for (size_t j = 0; j < nch->args->size(); ++j) {
                    classifyAndWriteExpr(writer, nch->args->get(j), parent_locals, parent_globals, const_reverse_map);
                }
            } else {
                writer.writeU8(0);
            }
            return true;
        }
    }

    // NewComplexListNode: complex typed list construction
    if (auto* ncl = dynamic_cast<const NewComplexListNode*>(node)) {
        if (ncl->typeInfo) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_LIST_NEW));
            writer.writeStringRef(QoreTypeInfo::getPath(ncl->typeInfo));
            // Serialize constructor arg (single QoreValue)
            if (ncl->args.hasNode()) {
                writer.writeU8(1);
                classifyAndWriteExpr(writer, ncl->args, parent_locals, parent_globals, const_reverse_map);
            } else {
                writer.writeU8(0);
            }
            return true;
        }
    }

    // QoreHashDeclCastOperatorNode: cast<StatInfo>(hash)
    if (auto* hdc = dynamic_cast<const QoreHashDeclCastOperatorNode*>(node)) {
        const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(hdc->getCastTypeInfo());
        if (hd || hdc->getCastTypeInfo() == hashTypeInfo) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_HASHDECL));
            writer.writeStringRef(hd ? hd->getNamespacePath().c_str() : "hash");
            writer.writeU8(hdc->isOrNothing() ? 1 : 0);
            // Serialize the inner expression being cast
            QoreValue inner = hdc->getExp();
            if (inner.hasNode()) {
                writer.writeU8(1);  // has inner expression
                classifyAndWriteExpr(writer, inner, parent_locals, parent_globals, const_reverse_map);
            } else {
                writer.writeU8(0);  // no inner expression (cast from nothing)
            }
            return true;
        }
    }

    // QoreComplexHashCastOperatorNode: cast<hash<string, int>>(hash)
    if (auto* chc = dynamic_cast<const QoreComplexHashCastOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_COMPLEX_HASH));
        writer.writeStringRef(QoreTypeInfo::getPath(chc->getCastTypeInfo()));
        writer.writeU8(chc->isOrNothing() ? 1 : 0);
        return true;
    }

    // QoreComplexListCastOperatorNode: cast<list<int>>(list)
    if (auto* clc = dynamic_cast<const QoreComplexListCastOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_COMPLEX_LIST));
        const QoreTypeInfo* ti = clc->getCastTypeInfo();
        writer.writeStringRef(ti ? QoreTypeInfo::getPath(ti) : "list");
        writer.writeU8(clc->isOrNothing() ? 1 : 0);
        return true;
    }

    // QoreClassCastOperatorNode: cast<ClassName>(obj)
    if (auto* cc = dynamic_cast<const QoreClassCastOperatorNode*>(node)) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(cc->getCastTypeInfo());
        if (qc) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_CLASS));
            // getNamespacePath(): see QoreAOT.cpp NewObjectCallNode.
            writer.writeStringRef(qc->getNamespacePath().c_str());
            writer.writeU8(cc->isOrNothing() ? 1 : 0);
            return true;
        }
    }

    // QoreEnumCastOperatorNode: cast<EnumType>(val)
    if (auto* ec = dynamic_cast<const QoreEnumCastOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_ENUM));
        writer.writeStringRef(QoreTypeInfo::getPath(ec->getCastTypeInfo()));
        writer.writeU8(ec->isOrNothing() ? 1 : 0);
        return true;
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
                writer.writeStringRef(cti ? QoreTypeInfo::getPath(cti) : "");

                // Write return type
                const char* ret_path = sig->getReturnTypeInfo()
                    ? QoreTypeInfo::getPath(sig->getReturnTypeInfo()) : "";
                writer.writeStringRef(ret_path);

                // Write params: count, then (name, type_path, default) per param
                unsigned num_params = sig->numParams();
                writer.writeU16(static_cast<uint16_t>(num_params));
                for (unsigned p = 0; p < num_params; ++p) {
                    const char* pname = sig->getName(p);
                    writer.writeStringRef(pname ? pname : "");
                    const char* ptype = sig->getParamTypeInfo(p)
                        ? QoreTypeInfo::getPath(sig->getParamTypeInfo(p)) : "";
                    writer.writeStringRef(ptype);
                    bool has_default = sig->hasDefaultArg(p);
                    writer.writeU8(has_default ? 1 : 0);
                    if (has_default) {
                        writer.writeValue(sig->getDefaultArgList()[p]);
                    }
                }
                writer.writeU8(sig->hasVarargs() ? 1 : 0);

                // Write captured variable names and parent slot indices
                const LVarSet* vlist = const_cast<UserClosureFunction*>(ucf)->getVList();
                writer.writeU16(vlist ? static_cast<uint16_t>(vlist->size()) : 0);
                if (vlist) {
                    for (LocalVar* lv : *vlist) {
                        writer.writeStringRef(lv->getName());
                        // Write parent slot index for disambiguation
                        int32_t parent_slot = -1;
                        for (size_t i = 0; i < parent_locals.size(); ++i) {
                            if (parent_locals[i].local_var_ptr == lv) {
                                parent_slot = static_cast<int32_t>(i);
                                break;
                            }
                        }
                        writer.writeU32(static_cast<uint32_t>(parent_slot));
                    }
                }

                // Lower closure to IR and serialize
                const QoreIRFunction* closure_ir = const_cast<UserClosureVariant*>(variant)->getCachedIR();
                QoreIRFunction* owned_ir = nullptr;
                if (!closure_ir) {
                    owned_ir = ::lowerClosureForSerialization(variant);
                    closure_ir = owned_ir;
                }

                if (closure_ir) {
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
                } else {
                    writer.writeU8(0);  // no IR
                }
                delete owned_ir;
                return true;
            }
        }
    }

    // Call/method references inside container literals must serialize as
    // reference metadata, not as unevaluated AST constants.  The reader rebuilds
    // equivalent reference nodes that CreateCallRef/CreateMethodRef can evaluate
    // to runtime code values.
    if (auto* mcr = dynamic_cast<const LocalMethodCallReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::BOUND_METHOD_REF));
        const QoreMethod* method = mcr->getMethod();
        const QoreClass* qc = method ? method->getClass() : nullptr;
        std::string class_path = qc ? qc->getNamespacePath() : std::string();
        writer.writeStringRef(class_path.c_str());
        writer.writeStringRef(method ? method->getName() : "");
        return true;
    }
    if (auto* scr = dynamic_cast<const LocalStaticMethodCallReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_METHOD_REF));
        const QoreMethod* method = scr->getMethod();
        const QoreClass* qc = method ? method->getClass() : nullptr;
        std::string class_path = qc ? qc->getNamespacePath() : std::string();
        writer.writeStringRef(class_path.c_str());
        writer.writeStringRef(method ? method->getName() : "");
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
        ConstantEntry* ce = rcr->getConstantEntry();
        if (ce && const_reverse_map) {
            // ce->val may be:
            // 1. The actual value (QoreHashNode etc.) — check if it's in the reverse map
            // 2. Another RuntimeConstantRefNode — follow the chain to saved_val
            const AbstractQoreNode* val_node = nullptr;
            if (ce->val.hasNode()) {
                val_node = ce->val.getInternalNode();
                auto it = const_reverse_map->find(val_node);
                if (it != const_reverse_map->end()) {
                    writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
                    writer.writeStringRef(it->second.c_str());
                    return true;
                }
            }
            // Try saved_val (the resolved constant value after all indirection)
            QoreValue sv = ce->getReferencedValue();
            if (sv.hasNode() && sv.getInternalNode() != val_node) {
                auto it = const_reverse_map->find(sv.getInternalNode());
                if (it != const_reverse_map->end()) {
                    writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
                    writer.writeStringRef(it->second.c_str());
                    sv.discard(nullptr);
                    return true;
                }
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
                // Pseudo-classes are not in the namespace tree; getPath() is
                // the path matched by the AOT pseudo-class resolver.
                class_path = mc->isPseudo() ? qc->getPath() : qc->getNamespacePath();
            }
            writer.writeStringRef(class_path.c_str());
            writer.writeStringRef(mc->getName() ? mc->getName() : "");
            writer.writeU8(mc->isPseudo() ? 1 : 0);
            // Target expression (left-hand side of the dot)
            classifyAndWriteExpr(writer, de->getExpression(),
                parent_locals, parent_globals, const_reverse_map);
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
                num_args = 0;  // too many to encode — fall through to arg-less form
            }
            writer.writeU8(static_cast<uint8_t>(num_args));
            for (size_t j = 0; j < num_args; ++j) {
                QoreValue arg = call_args
                    ? call_args->retrieveEntry(j)
                    : parse_args->get(j);
                classifyAndWriteExpr(writer, arg,
                    parent_locals, parent_globals, const_reverse_map);
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
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL));
            return true;
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

    // Unsupported — write GENERIC_EVAL placeholder
    trace_generic_eval("unsupported inline expression node", node);
    qoreAOTSetExprSerializationError("unsupported inline native AOT expression for "
        + qoreAOTDescribeExpr(expr)
        + "; add a native AOTExprKind serializer/reader or lower this operation to native IR");
    printd(3, "AOT: handler IR unsupported expr type '%s' for serialization\n",
        node->getTypeName());
    writer.writeU8(static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL));
    return true;
}

bool serializeSlotMaps(QoreAOTBinaryWriter& writer, const std::vector<AOTCompiledFuncWithSlots>& funcs,
        const AOTConstantReverseMap* const_reverse_map, std::string& error) {
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

        // Local slot entries (in slot order)
        for (auto& local : func.slot_ids.locals) {
            writer.writeStringRef(local.name.c_str());
            writer.writeStringRef(local.type_path.c_str());
            writer.writeU8(local.flags);
            writer.writeU16(local.param_index);
        }

        // Global slot entries (in slot order)
        for (auto& global : func.slot_ids.globals) {
            writer.writeStringRef(global.name.c_str());
            writer.writeStringRef(global.type_path.c_str());
            writer.writeU8(global.is_thread_local ? 1 : 0);
        }

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
                if (match) {
                    fprintf(stderr, "[aot-slot] func=%s kind=%u ref1=%s ref2=%s\n",
                        func.name.c_str(), static_cast<unsigned>(expr.kind),
                        expr.ref1.c_str(), expr.ref2.c_str());
                }
            }
            if (expr.kind == AOTExprKind::EXPR_TREE || expr.kind == AOTExprKind::GENERIC_EVAL) {
                std::string detail;
                std::string prefix = "slot " + std::to_string(expr_idx);
                for (const std::string& d : func.slot_ids.unsupported_expr_details) {
                    if (d.rfind(prefix, 0) == 0) {
                        detail = d;
                        break;
                    }
                }
                error = "AOT cannot serialize function '" + func.name
                    + "' expression " + prefix + " without source fallback";
                if (!detail.empty()) {
                    error += ": ";
                    error += detail;
                } else if (!expr.ref1.empty()) {
                    error += ": ";
                    error += expr.ref1;
                }
                error += "; EXPR_TREE and GENERIC_EVAL are fatal for new AOT output";
                return false;
            }
            writer.writeU8(static_cast<uint8_t>(expr.kind));

            // Use registry dispatch for expression slot metadata serialization
            const auto* kinfo = getAOTExprSlotKindInfo(static_cast<uint8_t>(expr.kind));
            if (!kinfo || !kinfo->is_supported || !kinfo->write_fn) {
                error = "unsupported expression slot kind " + std::to_string(static_cast<uint8_t>(expr.kind))
                    + " in function '" + func.name + "' slot " + std::to_string(expr_idx);
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
                    } else if (!expr.ref1.empty()) {
                        error += " ref1='";
                        error += expr.ref1;
                        error += "'";
                    }
                }
                return false;
            }
        }

        // Body local entries (in order)
        for (auto& bl : func.slot_ids.body_locals) {
            writer.writeStringRef(bl.name.c_str());
            writer.writeStringRef(bl.type_path.c_str());
            writer.writeU8(bl.is_closure ? 1 : 0);
        }

        // Regex case entries (in slot-index order)
        // Format per case: pattern_ref(u32) options(i64) is_negated(u8)
        for (auto& rc : func.slot_ids.regex_cases) {
            writer.writeStringRef(rc.pattern.c_str());
            writer.writeI64(rc.options);
            writer.writeU8(rc.is_negated ? 1 : 0);
        }

        // LValuePath instruction entries (in slot-index order)
        for (auto& lvid : func.slot_ids.lv_path_insts) {
            writer.writeU16(lvid.opcode);
            writer.writeU8(lvid.weak);
            writer.writeU8(lvid.compound_op);
            writer.writeU8(lvid.unary_op);
            writer.writeU8(lvid.binary_mut_op);
            writer.writeU8(lvid.ternary_op);
            writer.writeU8(lvid.ref_rv);
            // QORE_AOT_FEAT_LVPATH_DELETE_EXPR: preserve the original AST
            // lvalue expression so runtime Delete/Remove can use
            // LValueRemoveHelper's detach-then-destroy semantics when needed.
            writer.writeU8(lvid.delete_lvalue_expr.hasNode() ? 1 : 0);
            if (lvid.delete_lvalue_expr.hasNode()) {
                qoreAOTClearExprSerializationError();
                bool delete_expr_ok = classifyAndWriteExpr(writer, lvid.delete_lvalue_expr,
                    func.slot_ids.locals, func.slot_ids.globals, func_const_reverse_map);
                std::string expr_error;
                bool expr_error_set = qoreAOTTakeExprSerializationError(expr_error);
                if (!delete_expr_ok || expr_error_set) {
                    error = "failed to serialize LValuePath delete expression in function '"
                        + func.name + "'";
                    if (expr_error_set) {
                        error += ": ";
                        error += expr_error;
                    }
                    return false;
                }
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
                        || step.kind == static_cast<uint8_t>(LVPathStepKind::ListIndexSlice)) {
                    writer.writeU32(static_cast<uint32_t>(step.slice_operand_ids.size()));
                    for (uint32_t sid : step.slice_operand_ids) {
                        writer.writeU32(sid);
                    }
                }
            }
        }

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

        // Location table entries (AOT runtime_loc tracking)
        writer.writeU16(static_cast<uint16_t>(func.aot_locs.size()));
        for (auto& loc : func.aot_locs) {
            writer.writeU16(static_cast<uint16_t>(loc.start_line));
            writer.writeU16(static_cast<uint16_t>(loc.end_line));
            writer.writeStringRef(loc.file.c_str());
        }

        // Patch the entry size field
        uint32_t entry_end_pos = writer.position();
        writer.patchU32(entry_size_pos, entry_end_pos - entry_size_pos - 4);
    }

    writer.endSection(sec_idx);
    return true;
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
static QoreIRInstGroup classifyInstruction(const QoreIRInstruction* inst) {
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
    // Default: base instruction (no extra fields)
    return QoreIRInstGroup::Base;
}

//! Get type path string for a LocalVar, handling nullptr typeInfo
const char* getLocalTypePath(const LocalVar* lv) {
    const QoreTypeInfo* ti = lv->getTypeInfo();
    return getTypePath(ti, lv->isNoNarrowing());
}

//! Serialize a single IR instruction
static bool serializeIRInstruction(QoreAOTBinaryWriter& writer, const QoreIRInstruction* inst,
        const std::unordered_map<const QoreIRBasicBlock*, uint16_t>& block_idx,
        const AOTExprWriteFunc& writeExpr) {
    // Write opcode
    writer.writeU16(static_cast<uint16_t>(inst->opcode));

    // Classify and write group tag
    QoreIRInstGroup group = classifyInstruction(inst);
    writer.writeU8(static_cast<uint8_t>(group));

    // Write base fields: result, operands, exception_target
    writer.writeU32(inst->result.id);
    writer.writeU8(static_cast<uint8_t>(inst->operands.size()));
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
        return false;
    }
    if (ginfo->write_fn) {
        AOTInstWriteCtx wctx{writer, inst, block_idx, writeExpr};
        if (!ginfo->write_fn(wctx)) {
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
    // 1. Function header
    writer.writeStringRef(func.name.c_str());
    writer.writeU32(func.max_value_id);
    writer.writeU32(func.max_local_slot_id);
    writer.writeU32(func.num_guards);
    writer.writeStringRef(func.return_type_info ? QoreTypeInfo::getPath(func.return_type_info) : "");
    // Phase C: Serialize parent_slot_count for handler IR functions
    writer.writeU32(func.parent_slot_count);
    writer.writeU16(static_cast<uint16_t>(func.blocks.size()));
    writer.writeU16(static_cast<uint16_t>(func.local_var_slots.size()));
    writer.writeU16(static_cast<uint16_t>(func.all_body_locals.size()));

    // 2. Local variable slot table
    // Sort by slot_id for deterministic serialization
    std::vector<std::pair<const LocalVar*, uint32_t>> sorted_slots(
        func.local_var_slots.begin(), func.local_var_slots.end());
    std::sort(sorted_slots.begin(), sorted_slots.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    for (auto& [lv, slot_id] : sorted_slots) {
        writer.writeStringRef(lv->getName());
        writer.writeStringRef(getLocalTypePath(lv));
        writer.writeU32(slot_id);
    }

    // 3. Body locals
    for (auto* lv : func.all_body_locals) {
        writer.writeStringRef(lv->getName());
        writer.writeStringRef(getLocalTypePath(lv));
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

        for (auto& inst_ptr : block->instructions) {
            if (!serializeIRInstruction(writer, inst_ptr.get(), block_idx, writeExpr)) {
                return false;
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
bool QoreAOTBinaryDeserializer::openAndDeserializeShells(QoreProgram* in_pgm,
        const uint8_t* data, uint32_t size, std::string& error) {
    pgm = in_pgm;

    // Open and validate the binary blob
    if (!reader.open(data, size, error)) {
        return false;
    }

    // Decide whether to use the per-blob TYPE_TABLE fast path.  The
    // writer advertises QORE_AOT_FEAT_TYPE_TABLE unconditionally for
    // binaries produced by the current compiler; older blobs don't
    // have the bit and fall back to inline-string type paths.
    uses_type_table = (reader.getHeader().feature_flags
        & QORE_AOT_FEAT_TYPE_TABLE) != 0;

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
    if (!resolveClassConstants(error)) {
        return false;
    }
    if (!deserializeConstants(error)) {
        return false;
    }
    return true;
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

bool QoreAOTBinaryDeserializer::resolveTypesAndMembers(std::string& error) {
    return resolveTypes(error)
        && resolveConstants(error)
        && resolveStaticMembersPhase(error)
        && deserializeFunctionsAndMethods(error)
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

// Phase-split 2a-c.  Top-level globals.  Class static members are registered
// before instance members so member defaults can resolve static var references.
bool QoreAOTBinaryDeserializer::resolveStaticsAndConstants(std::string& error) {
    if (!deserializeGlobals(error)) {
        return false;
    }
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
        qore_program_private* pp = qore_program_private::get(*pgm);
        for (const auto& pd : pending_smd) {
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = !pd.class_path.empty()
                ? qore_root_ns_private::runtimeFindClass(*pp->RootNS,
                    pd.class_path.c_str(), found_ns)
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
                printd(0, "AOT deser: cannot resolve deferred static method "
                    "default '%s::%s()'\n",
                    pd.class_path.c_str(), pd.method_name.c_str());
                continue;
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
    // Resolve deferred BCA (base class constructor argument) EXPR_TREE blobs.
    // Must run after commitDeserializedClasses + rebuildAllIndexes so all
    // methods and classes are findable by the EXPR_TREE handlers.
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
        qore_program_private* pp_idx = qore_program_private::get(*pgm);
        qore_root_ns_private* rpriv = static_cast<qore_root_ns_private*>(
            qore_ns_private::get(*pp_idx->RootNS));
        rpriv->rebuildAllIndexes();
    }
    return finalizePostIndex(error);
}

void QoreAOTBinaryMultiDeserializer::rebuildRootIndexesOnce() {
    qore_program_private* pp_idx = qore_program_private::get(*pgm);
    qore_root_ns_private* rpriv = static_cast<qore_root_ns_private*>(
        qore_ns_private::get(*pp_idx->RootNS));
    rpriv->rebuildAllIndexes();
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

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* path = reader.readStringRef(ptr);
        uint32_t parent_idx = QoreAOTBinaryReader::readU32(ptr);
        uint32_t depth = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        (void)depth;

        if (parent_idx == UINT32_MAX) {
            // Root namespace - use existing
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
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        int64_t domain = QoreAOTBinaryReader::readI64(ptr);

        // Validate namespace index before creating the class
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            error = "invalid namespace index for class '" + std::string(name) + "'";
            return false;
        }

        // Create the class and add to namespace immediately so it's owned
        // by the namespace (QoreClass destructor is protected)
        QoreClass* qc = new QoreClass(name, path, domain);
        qore_class_private* priv = qore_class_private::get(*qc);
        priv->pub = (flags & 0x0001) != 0;
        if (flags & 0x0002) {
            priv->final = true;
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
            if (base_path && *base_path) {
                PendingBaseClass pbc;
                pbc.base_path = base_path;
                pbc.access = access;
                pbc.is_virtual = (is_virtual != 0);
                bases.push_back(std::move(pbc));
            }
        }
        // Skip pending data for classes that already existed (from loaded modules)
        // — they already have their bases, members, etc. set up
        if (class_already_existed) {
            bases.clear();
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
                if (!readDeferredMemberDefault(reader, ptr, end, error,
                        default_val, pim)) {
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
                if (!readDeferredMemberDefault(reader, ptr, end, error,
                        default_val, psm)) {
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
            QoreValue cval = reader.readValue(ptr, end, error);
            if (!error.empty()) {
                error = "class constant '" + std::string(cname ? cname : "(null)") + "': " + error;
                return false;
            }

            if (!class_already_existed && cname && *cname) {
                PendingClassConstant pcc;
                pcc.name = cname;
                pcc.type_path = ctype_path ? ctype_path : "";
                pcc.access = caccess;
                pcc.pending_init = (cpending != 0);
                pcc.value = cval;
                class_constants.push_back(std::move(pcc));
            } else if (cval.hasNode()) {
                cval.discard(nullptr);
            }
        }
        pending_class_constants.push_back(std::move(class_constants));
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
            // Look up base class by path in the program
            ExceptionSink xsink;
            const QoreClass* base = pgm->findClass(pbc.base_path.c_str(), &xsink);
            if (xsink.isException()) {
                xsink.clear();
            }
            if (base) {
                // Add base class to this class with proper access level
                qc->addBaseClass(const_cast<QoreClass*>(base),
                    static_cast<ClassAccess>(pbc.access), pbc.is_virtual);
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
        if (!class_list[i]) {
            continue;
        }
        const char* cpath = class_list[i]->getPath();
        if (cpath && *cpath) {
            all_class_map[cpath] = class_list[i];
            if (strncmp(cpath, "::", 2) == 0) {
                all_class_map[std::string(cpath + 2)] = class_list[i];
            } else {
                all_class_map[std::string("::") + cpath] = class_list[i];
            }
        }
    }

    for (uint32_t i = 0; i < class_list.size() && i < pending_instance_members.size(); ++i) {
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        qore_class_private* priv = qore_class_private::get(*qc);
        for (auto& pim : pending_instance_members[i]) {
            const QoreTypeInfo* ti = nullptr;
            if (!pim.type_path.empty()) {
                ti = type_resolver->resolve(pim.type_path.c_str(), error);
                if (!error.empty()) {
                    printd(2, "AOT deser: cannot resolve type '%s' for instance member '%s' "
                        "in class '%s': %s (falling back to auto)\n",
                        pim.type_path.c_str(), pim.name.c_str(), qc->getName(), error.c_str());
                    error.clear();
                    ti = autoTypeInfo;
                }
            }

            // Resolve a pending forward-referenced NewObject default if any.
            if (!pim.pending_new_class_path.empty()) {
                const QoreClass* target = nullptr;
                {
                    ExceptionSink xs;
                    target = getProgram()->findClass(pim.pending_new_class_path.c_str(), &xs);
                    if (xs.isException()) {
                        xs.clear();
                    }
                }
                if (!target) {
                    auto it = all_class_map.find(pim.pending_new_class_path);
                    if (it != all_class_map.end()) {
                        target = it->second;
                    }
                }
                if (target) {
                    QoreParseListNode* parse_args = nullptr;
                    if (!pim.pending_new_args.empty()) {
                        parse_args = new QoreParseListNode(&loc_builtin);
                        for (auto& v : pim.pending_new_args) {
                            parse_args->add(v, &loc_builtin);
                        }
                        pim.pending_new_args.clear();
                    }
                    ScopedObjectCallNode* socn = new ScopedObjectCallNode(
                        &loc_builtin, target, parse_args);
                    if (parse_args) {
                        socn->resolveParseArgs();
                    }
                    pim.default_val = QoreValue(socn);
                } else {
                    printd(0, "AOT deser: cannot resolve pending NewObject class '%s' for "
                        "instance member '%s' in class '%s'\n",
                        pim.pending_new_class_path.c_str(), pim.name.c_str(), qc->getName());
                    for (auto& v : pim.pending_new_args) {
                        v.discard(nullptr);
                    }
                    pim.pending_new_args.clear();
                }
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
                        printd(0, "AOT deser: enum member '%s::%s' not found for "
                            "instance member '%s' in class '%s'\n",
                            pim.pending_enum_path.c_str(),
                            pim.pending_enum_member.c_str(),
                            pim.name.c_str(), qc->getName());
                    }
                } else {
                    printd(0, "AOT deser: enum '%s' not found for instance member "
                        "'%s' in class '%s'\n",
                        pim.pending_enum_path.c_str(), pim.name.c_str(), qc->getName());
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
                    // Hashdecl: resolve by namespace path
                    const QoreNamespace* pns = nullptr;
                    const TypedHashDecl* hd = getProgram()->findHashDecl(
                        pim.pending_complex_default_path.c_str(), pns);
                    if (hd) {
                        NewHashDeclNode* nhd = new NewHashDeclNode(
                            &loc_builtin, hd, parse_args, false);
                        pim.default_val = QoreValue(nhd);
                    } else {
                        printd(0, "AOT deser: hashdecl '%s' not found for instance member "
                            "'%s' in class '%s'\n",
                            pim.pending_complex_default_path.c_str(), pim.name.c_str(),
                            qc->getName());
                        if (parse_args) {
                            parse_args->deref(nullptr);
                        }
                    }
                } else {
                    // kind 0 (complex list) or kind 1 (complex hash)
                    const QoreTypeInfo* cti = qore_get_type_from_string_intern(
                        pim.pending_complex_default_path.c_str());
                    if (cti) {
                        if (pim.pending_complex_default_kind == 0) {
                            QoreValue list_args;
                            if (parse_args) {
                                list_args = QoreValue(parse_args);
                            }
                            NewComplexListNode* ncl = new NewComplexListNode(
                                &loc_builtin, cti, list_args);
                            pim.default_val = QoreValue(ncl);
                        } else {
                            NewComplexHashNode* nch = new NewComplexHashNode(
                                &loc_builtin, cti, parse_args);
                            pim.default_val = QoreValue(nch);
                        }
                    } else {
                        printd(0, "AOT deser: type '%s' not found for instance member "
                            "'%s' in class '%s' (complex default kind=%d)\n",
                            pim.pending_complex_default_path.c_str(), pim.name.c_str(),
                            qc->getName(), (int)pim.pending_complex_default_kind);
                        if (parse_args) {
                            parse_args->deref(nullptr);
                        }
                    }
                }
                pim.pending_complex_default_kind = -1;
                pim.pending_complex_default_path.clear();
            }

            resolveDeferredExprTreeDefault(pim.pending_expr_tree_blob,
                pim.default_val, getProgram(), "class", qc->getName(),
                pim.name.c_str());

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
        if (!class_list[i]) {
            continue;
        }
        const char* cpath = class_list[i]->getPath();
        if (cpath && *cpath) {
            all_class_map[cpath] = class_list[i];
            if (strncmp(cpath, "::", 2) == 0) {
                all_class_map[std::string(cpath + 2)] = class_list[i];
            } else {
                all_class_map[std::string("::") + cpath] = class_list[i];
            }
        }
    }

    for (uint32_t i = 0; i < class_list.size() && i < pending_static_members.size(); ++i) {
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        qore_class_private* priv = qore_class_private::get(*qc);
        for (auto& psm : pending_static_members[i]) {
            const QoreTypeInfo* ti = nullptr;
            if (!psm.type_path.empty()) {
                ti = type_resolver->resolve(psm.type_path.c_str(), error);
                if (!error.empty()) {
                    printd(2, "AOT deser: cannot resolve type '%s' for static member '%s' "
                        "in class '%s': %s (falling back to auto)\n",
                        psm.type_path.c_str(), psm.name.c_str(), qc->getName(), error.c_str());
                    error.clear();
                    ti = autoTypeInfo;
                }
            }

            // Resolve a pending forward-referenced NewObject default if any
            if (!psm.pending_new_class_path.empty()) {
                const QoreClass* target = nullptr;
                {
                    ExceptionSink xs;
                    target = getProgram()->findClass(psm.pending_new_class_path.c_str(), &xs);
                    if (xs.isException()) {
                        xs.clear();
                    }
                }
                if (!target) {
                    auto it = all_class_map.find(psm.pending_new_class_path);
                    if (it != all_class_map.end()) {
                        target = it->second;
                    }
                }
                if (target) {
                    QoreParseListNode* parse_args = nullptr;
                    if (!psm.pending_new_args.empty()) {
                        parse_args = new QoreParseListNode(&loc_builtin);
                        for (auto& v : psm.pending_new_args) {
                            parse_args->add(v, &loc_builtin);
                        }
                        psm.pending_new_args.clear();
                    }
                    ScopedObjectCallNode* socn = new ScopedObjectCallNode(
                        &loc_builtin, target, parse_args);
                    if (parse_args) {
                        socn->resolveParseArgs();
                    }
                    psm.default_val = QoreValue(socn);
                } else {
                    printd(0, "AOT deser: cannot resolve pending NewObject class '%s' for "
                        "static member '%s' in class '%s'\n",
                        psm.pending_new_class_path.c_str(), psm.name.c_str(), qc->getName());
                    for (auto& v : psm.pending_new_args) {
                        v.discard(nullptr);
                    }
                    psm.pending_new_args.clear();
                }
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
                        printd(0, "AOT deser: enum member '%s::%s' not found for "
                            "static member '%s' in class '%s'\n",
                            psm.pending_enum_path.c_str(),
                            psm.pending_enum_member.c_str(),
                            psm.name.c_str(), qc->getName());
                    }
                } else {
                    printd(0, "AOT deser: enum '%s' not found for static member "
                        "'%s' in class '%s'\n",
                        psm.pending_enum_path.c_str(), psm.name.c_str(), qc->getName());
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
                    const QoreNamespace* pns = nullptr;
                    const TypedHashDecl* hd = getProgram()->findHashDecl(
                        psm.pending_complex_default_path.c_str(), pns);
                    if (hd) {
                        NewHashDeclNode* nhd = new NewHashDeclNode(
                            &loc_builtin, hd, parse_args, false);
                        psm.default_val = QoreValue(nhd);
                    } else {
                        printd(0, "AOT deser: hashdecl '%s' not found for static member "
                            "'%s' in class '%s'\n",
                            psm.pending_complex_default_path.c_str(), psm.name.c_str(),
                            qc->getName());
                        if (parse_args) {
                            parse_args->deref(nullptr);
                        }
                    }
                } else {
                    const QoreTypeInfo* cti = qore_get_type_from_string_intern(
                        psm.pending_complex_default_path.c_str());
                    if (cti) {
                        if (psm.pending_complex_default_kind == 0) {
                            QoreValue list_args;
                            if (parse_args) {
                                list_args = QoreValue(parse_args);
                            }
                            NewComplexListNode* ncl = new NewComplexListNode(
                                &loc_builtin, cti, list_args);
                            psm.default_val = QoreValue(ncl);
                        } else {
                            NewComplexHashNode* nch = new NewComplexHashNode(
                                &loc_builtin, cti, parse_args);
                            psm.default_val = QoreValue(nch);
                        }
                    } else {
                        printd(0, "AOT deser: type '%s' not found for static member "
                            "'%s' in class '%s' (complex default kind=%d)\n",
                            psm.pending_complex_default_path.c_str(), psm.name.c_str(),
                            qc->getName(), (int)psm.pending_complex_default_kind);
                        if (parse_args) {
                            parse_args->deref(nullptr);
                        }
                    }
                }
                psm.pending_complex_default_kind = -1;
                psm.pending_complex_default_path.clear();
            }

            resolveDeferredExprTreeDefault(psm.pending_expr_tree_blob,
                psm.default_val, getProgram(), "class", qc->getName(),
                psm.name.c_str());

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

bool QoreAOTBinaryDeserializer::resolveClassConstants(std::string& error) {
    // Second pass: add class constants now that types are resolved
    // NOTE: hashdecls and enums must be deserialized before calling this method
    // so that type references to them can be resolved
    for (uint32_t i = 0; i < class_list.size() && i < pending_class_constants.size(); ++i) {
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        qore_class_private* priv = qore_class_private::get(*qc);
        for (auto& pcc : pending_class_constants[i]) {
            const QoreTypeInfo* ti = nullptr;
            if (!pcc.type_path.empty()) {
                ti = type_resolver->resolve(pcc.type_path.c_str(), error);
                if (!error.empty()) {
                    printd(2, "AOT deser: cannot resolve type '%s' for constant '%s' "
                        "in class '%s': %s (falling back to auto)\n",
                        pcc.type_path.c_str(), pcc.name.c_str(), qc->getName(), error.c_str());
                    error.clear();
                    ti = autoTypeInfo;
                }
            }

            if (!pcc.pending_init && ti && pcc.value.hasNode()
                    && (pcc.value.getType() == NT_HASH
                        || pcc.value.getType() == NT_LIST)) {
                ExceptionSink xs;
                QoreTypeInfo::retypeValue(pcc.value, ti, &xs);
                if (xs.isException()) {
                    xs.clear();
                }
                QoreTypeInfo::acceptInputMember(ti, pcc.name.c_str(),
                    pcc.value, &xs);
                if (xs.isException()) {
                    QoreValue e = xs.getExceptionErr();
                    QoreValue d = xs.getExceptionDesc();
                    const char* es = e.getType() == NT_STRING
                        ? e.get<const QoreStringNode>()->c_str() : "(?err)";
                    const char* ds = d.getType() == NT_STRING
                        ? d.get<const QoreStringNode>()->c_str() : "(?desc)";
                    printd(0, "AOT deser: class '%s' constant '%s' narrowing "
                        "to '%s' failed: %s: %s\n",
                        qc->getName(), pcc.name.c_str(),
                        pcc.type_path.c_str(), es, ds);
                    xs.clear();
                }
            }

            // Use addUserConstant to avoid setting sys=true on user classes
            priv->addUserConstant(pcc.name.c_str(), pcc.value,
                static_cast<ClassAccess>(pcc.access), ti);

            if (pcc.pending_init) {
                // Pending init-func: parser-time references must defer to
                // runtime.  Look the new ConstantEntry back up and swap its
                // val for a self-referential RuntimeConstantRefNode; the
                // init-func populates saved_val when it runs at register time.
                ConstantEntry* ce = priv->constlist.findEntry(pcc.name.c_str());
                if (ce) {
                    ce->aot_shell_pending = true;
                    ce->val.discard(nullptr);
                    ce->val = new RuntimeConstantRefNode(&loc_builtin, ce,
                        /*aot_deferred=*/true);
                }
            }

            printd(5, "AOT deser: added constant '%s' to class '%s'\n",
                pcc.name.c_str(), qc->getName());
        }
    }

    // Clear pending data
    pending_class_constants.clear();
    return true;
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
                        printd(0, "AOT deser: enum member '%s::%s' not found "
                            "for hashdecl '%s' member '%s'\n",
                            phm.pending_enum_path.c_str(),
                            phm.pending_enum_member.c_str(),
                            hd->getName(), phm.name.c_str());
                    }
                } else {
                    printd(0, "AOT deser: enum '%s' not found for hashdecl "
                        "'%s' member '%s'\n",
                        phm.pending_enum_path.c_str(), hd->getName(),
                        phm.name.c_str());
                }
                phm.pending_enum_path.clear();
                phm.pending_enum_member.clear();
            }

            // Resolve deferred VT_NEW_OBJECT: build the ScopedObjectCallNode
            // for `Class(args)` defaults now that the class is registered.
            if (!phm.pending_new_class_path.empty()) {
                qore_program_private* pp = qore_program_private::get(*pgm);
                const qore_ns_private* found_ns = nullptr;
                const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                    *pp->RootNS, phm.pending_new_class_path.c_str(), found_ns);
                if (qc) {
                    QoreParseListNode* parse_args = nullptr;
                    if (!phm.pending_new_args.empty()) {
                        parse_args = new QoreParseListNode(&loc_builtin);
                        for (auto& a : phm.pending_new_args) {
                            parse_args->add(a, &loc_builtin);
                        }
                        phm.pending_new_args.clear();
                    }
                    phm.default_val = QoreValue(new ScopedObjectCallNode(
                        &loc_builtin, qc, parse_args));
                } else {
                    printd(0, "AOT deser: class '%s' not found for hashdecl "
                        "'%s' member '%s' default\n",
                        phm.pending_new_class_path.c_str(), hd->getName(),
                        phm.name.c_str());
                    for (auto& a : phm.pending_new_args) {
                        a.discard(nullptr);
                    }
                    phm.pending_new_args.clear();
                }
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
                    const QoreNamespace* pns = nullptr;
                    const TypedHashDecl* default_hd = getProgram()->findHashDecl(
                        phm.pending_complex_default_path.c_str(), pns);
                    if (default_hd) {
                        phm.default_val = QoreValue(new NewHashDeclNode(
                            &loc_builtin, default_hd, parse_args, false));
                    } else {
                        printd(0, "AOT deser: hashdecl '%s' not found for "
                            "hashdecl '%s' member '%s' default\n",
                            phm.pending_complex_default_path.c_str(),
                            hd->getName(), phm.name.c_str());
                        if (parse_args) {
                            parse_args->deref(nullptr);
                        }
                    }
                } else {
                    const QoreTypeInfo* cti = qore_get_type_from_string_intern(
                        phm.pending_complex_default_path.c_str());
                    if (cti) {
                        if (phm.pending_complex_default_kind == 0) {
                            QoreValue list_args;
                            if (parse_args) {
                                list_args = QoreValue(parse_args);
                            }
                            phm.default_val = QoreValue(new NewComplexListNode(
                                &loc_builtin, cti, list_args));
                        } else {
                            phm.default_val = QoreValue(new NewComplexHashNode(
                                &loc_builtin, cti, parse_args));
                        }
                    } else {
                        printd(0, "AOT deser: type '%s' not found for "
                            "hashdecl '%s' member '%s' default (complex kind=%d)\n",
                            phm.pending_complex_default_path.c_str(),
                            hd->getName(), phm.name.c_str(),
                            (int)phm.pending_complex_default_kind);
                        if (parse_args) {
                            parse_args->deref(nullptr);
                        }
                    }
                }
                phm.pending_complex_default_kind = -1;
                phm.pending_complex_default_path.clear();
            }

            resolveDeferredExprTreeDefault(phm.pending_expr_tree_blob,
                phm.default_val, getProgram(), "hashdecl", hd->getName(),
                phm.name.c_str());

            const QoreTypeInfo* mti = type_resolver->resolve(phm.type_path.c_str(), error);
            if (!error.empty()) {
                // Fall back to auto type when the type can't be resolved
                printd(0, "AOT: cannot resolve type '%s' for member '%s' "
                    "in hashdecl '%s': %s (falling back to auto)\n",
                    phm.type_path.c_str(), phm.name.c_str(), hd->getName(), error.c_str());
                error.clear();
                mti = autoTypeInfo;
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
                    const char* es = e.getType() == NT_STRING
                        ? e.get<const QoreStringNode>()->c_str() : "(?err)";
                    const char* ds = d.getType() == NT_STRING
                        ? d.get<const QoreStringNode>()->c_str() : "(?desc)";
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
            std::string pending_complex_default_path;
            std::vector<QoreValue> pending_complex_default_args;
            std::vector<uint8_t> pending_expr_tree_blob;
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
            phm.pending_complex_default_path = std::move(mi.pending_complex_default_path);
            phm.pending_complex_default_args = std::move(mi.pending_complex_default_args);
            phm.pending_expr_tree_blob = std::move(mi.pending_expr_tree_blob);
            pending_members.push_back(std::move(phm));
        }
        pending_hashdecl_members.push_back({hd, std::move(pending_members)});

        // Store for parent resolution pass
        hashdecl_list.push_back({hd, parent_path ? parent_path : ""});
    }

    // Second pass: resolve parent hashdecl pointers
    for (auto& hdi : hashdecl_list) {
        if (!hdi.parent_path.empty()) {
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

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* type_path = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint8_t access = QoreAOTBinaryReader::readU8(ptr);
        uint8_t is_pub = QoreAOTBinaryReader::readU8(ptr);
        uint8_t pending = has_pending_flag ? QoreAOTBinaryReader::readU8(ptr) : 0;
        QoreValue val = reader.readValue(ptr, end, error);
        if (!error.empty()) {
            error = "namespace constant '" + std::string(name ? name : "(null)") + "': " + error;
            return false;
        }

        // Add constant to namespace
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            val.discard(nullptr);
            error = "invalid namespace index " + std::to_string(ns_idx) +
                " for constant '" + std::string(name ? name : "(null)") + "'";
            return false;
        }
        if (!name || !*name) {
            val.discard(nullptr);
            error = "invalid empty name for namespace constant";
            return false;
        }

        // Skip if constant already exists (from dependency module)
        {
            const QoreTypeInfo* existing_ti = nullptr;
            bool found = false;
            ns_list[ns_idx]->constant.find(name, existing_ti, found);
            if (found) {
                printd(2, "AOT: skipping constant '%s' - already exists (from dependency)\n", name);
                val.discard(nullptr);
                continue;
            }
        }

        const QoreTypeInfo* ti = type_resolver->resolve(type_path, error);
        if (!error.empty()) {
            val.discard(nullptr);
            printd(0, "AOT: failed to resolve type '%s' for const '%s': %s\n",
                type_path ? type_path : "(null)", name ? name : "(null)", error.c_str());
            error = "cannot resolve type '" + std::string(type_path ? type_path : "(null)") +
                "' for constant '" + std::string(name) + "': " + error;
            return false;
        }
        const QoreTypeInfo* final_ti = ti ? ti : val.getTypeInfo();
        if (!pending && final_ti && val.hasNode()
                && (val.getType() == NT_HASH || val.getType() == NT_LIST)) {
            ExceptionSink xs;
            QoreTypeInfo::retypeValue(val, final_ti, &xs);
            if (xs.isException()) {
                xs.clear();
            }
            QoreTypeInfo::acceptInputMember(final_ti, name, val, &xs);
            if (xs.isException()) {
                QoreValue e = xs.getExceptionErr();
                QoreValue d = xs.getExceptionDesc();
                const char* es = e.getType() == NT_STRING
                    ? e.get<const QoreStringNode>()->c_str() : "(?err)";
                const char* ds = d.getType() == NT_STRING
                    ? d.get<const QoreStringNode>()->c_str() : "(?desc)";
                printd(0, "AOT deser: namespace constant '%s' narrowing "
                    "to '%s' failed: %s: %s\n",
                    name, type_path ? type_path : "(null)", es, ds);
                xs.clear();
            }
        }
        // Create as user constant (not builtin) with proper pub flag.
        // Using add() would mark the constant as builtin, which causes
        // scanMergeCommittedNamespace to skip it (isUserPublic() returns false).
        // Create the ConstantEntry directly with: pub = is_pub, init = true (value
        // already resolved), builtin = false (user constant from AOT module).
        ConstantEntry* ce = new ConstantEntry(&loc_builtin, name, val,
            final_ti, is_pub != 0, true, false,
            static_cast<ClassAccess>(access));
        if (pending) {
            // Pending init-func: parser-time references to this constant must
            // defer to runtime (when the init-func populates saved_val) instead
            // of folding the NOTHING placeholder.  Swap val for a self-
            // referential RuntimeConstantRefNode; setRuntimeValue() will keep
            // this shell and populate saved_val once the init-func runs.
            ce->aot_shell_pending = true;
            ce->val.discard(nullptr);
            ce->val = new RuntimeConstantRefNode(&loc_builtin, ce, /*aot_deferred=*/true);
        }
        ns_list[ns_idx]->constant.addEntry(name, ce);
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

        const QoreTypeInfo* ti = type_resolver->resolve(type_path, error);
        if (!error.empty()) {
            error = "cannot resolve type '" + std::string(type_path ? type_path : "(null)") +
                "' for global variable '" + std::string(name) + "': " + error;
            return false;
        }

        // Create the global variable directly
        Var* var = new Var(get_runtime_location(), name, ti, false,
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

//! Helper: read a variant signature and set up the UserSignature from AOT metadata
static bool readAndSetupVariantSignature(
        const QoreAOTBinaryReader& reader,
        QoreAOTTypeResolver* type_resolver,
        QoreProgram* pgm,
        const uint8_t*& ptr, const uint8_t* end,
        UserVariantBase* uvb,
        bool& sig_has_ellipsis,
        bool& needs_extra_args_flag,
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

    // Read params — reserve+emplace rather than resize+assign so we
    // skip the up-front default-construction of `np` empty strings per
    // variant (~3.3 M skipped default-constructions in qwf batch).
    std::vector<std::string> param_names;
    std::vector<const QoreTypeInfo*> param_types;
    std::vector<QoreValue> param_defaults;
    param_names.reserve(np);
    param_types.reserve(np);
    param_defaults.resize(np);  // sparse by has_default — keep indexed

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
            qore_program_private* pp = qore_program_private::get(*pgm);
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = (class_path && *class_path)
                ? qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path, found_ns)
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
                    printd(0, "AOT deser: cannot resolve default static method "
                        "'%s::%s()' (no deferred-defaults context)\n",
                        class_path ? class_path : "(null)",
                        mname ? mname : "(null)");
                    param_defaults[j] = QoreValue(true);
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
            qore_program_private* pp = qore_program_private::get(*pgm);
            const qore_ns_private* found_ns = nullptr;
            const TypedHashDecl* hd = hd_path
                ? qore_root_ns_private::runtimeFindHashDecl(*pp->RootNS, hd_path, found_ns)
                : nullptr;
            if (!hd) {
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
                auto* nd = new QoreHashDeclCastOperatorNode(&loc_builtin, hd, inner,
                    or_nothing != 0);
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
        sig_first_line, sig_last_line);

    if (time_on_sub) {
        g_aot_dm_sig_setup_us += now_us_fn() - t_setup0;
    }

    return true;
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
                // Read variant data matching the format in readAndSetupVariantSignature:
                // 1. ret_type (u32 index when type-table is in use, else StringRef)
                if (uses_type_table) {
                    (void)QoreAOTBinaryReader::readU32(ptr);  // ret type index
                } else {
                    reader.readStringRef(ptr);
                }
                // 2. num_params (U32)
                uint32_t num_params = QoreAOTBinaryReader::readU32(ptr);
                // 3. sig_flags (U16)
                QoreAOTBinaryReader::readU16(ptr);
                // 3a. sig start/end lines (2x U16) — only when feat advertised
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_SIG_LINES) != 0) {
                    QoreAOTBinaryReader::readU16(ptr);
                    QoreAOTBinaryReader::readU16(ptr);
                }
                // 4. For each param: name, type, has_default, and optionally value
                for (uint32_t p = 0; p < num_params; ++p) {
                    reader.readStringRef(ptr);  // param name
                    if (uses_type_table) {
                        (void)QoreAOTBinaryReader::readU32(ptr);  // param type index
                    } else {
                        reader.readStringRef(ptr);               // param type path
                    }
                    uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
                    if (has_default == 1) {
                        // Constant default: skip value
                        QoreValue default_val = reader.readValue(ptr, end, error);
                        if (!error.empty()) {
                            return false;
                        }
                        default_val.discard(nullptr);
                    } else if (has_default == 2) {
                        // Expression default (no-arg function call): skip function name ref
                        reader.readStringRef(ptr);
                    } else if (has_default == 3) {
                        // Expression default (const ref): skip FQN
                        reader.readStringRef(ptr);
                    } else if (has_default == 4) {
                        // Expression default (method call on const): skip FQN + method name
                        reader.readStringRef(ptr);
                        reader.readStringRef(ptr);
                    } else if (has_default == 5) {
                        // Expression default (hashdecl cast): skip path + or_nothing +
                        // optional inner value
                        reader.readStringRef(ptr);
                        (void)QoreAOTBinaryReader::readU8(ptr);  // or_nothing
                        uint8_t has_inner = QoreAOTBinaryReader::readU8(ptr);
                        if (has_inner) {
                            QoreValue v = reader.readValue(ptr, end, error);
                            if (!error.empty()) {
                                return false;
                            }
                            v.discard(nullptr);
                        }
                    } else if (has_default == 6) {
                        // Expression default (static method call): skip class path
                        // + method name
                        reader.readStringRef(ptr);
                        reader.readStringRef(ptr);
                    }
                }
            }
            continue;
        }

        // Create the QoreFunction
        QoreFunction* func = new QoreFunction(name);

        for (uint32_t v = 0; v < num_variants; ++v) {
            // Create an empty UserFunctionVariant (no body, no params)
            UserFunctionVariant* ufv = new UserFunctionVariant(
                nullptr, 0, 0, QoreValue(), nullptr, false);

            bool sig_has_ellipsis = false;
            bool needs_extra_args_flag = false;
            const std::vector<const QoreTypeInfo*>* tt =
                uses_type_table ? &type_table_resolved : nullptr;
            if (!readAndSetupVariantSignature(reader, type_resolver, pgm, ptr, end,
                    ufv, sig_has_ellipsis, needs_extra_args_flag, error, nullptr, tt)) {
                // variant ownership transfers to addPendingVariant or cleanup
                ufv->deref();
                // function can't be deleted directly; add it to namespace empty
                ns_list[ns_idx]->func_list.add(func, ns_list[ns_idx]);
                return false;
            }

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

            // Create the correct variant type for special methods:
            // constructor → UserConstructorVariant, destructor → UserDestructorVariant,
            // copy → UserCopyVariant, everything else → UserMethodVariant.
            // This is critical because the runtime dispatches through type-specific
            // virtual methods (evalConstructor, evalDestructor, evalCopy) via
            // reinterpret_cast from the base MethodVariant pointer.
            bool is_constructor = method_name && strcmp(method_name, "constructor") == 0;
            bool is_destructor = method_name && strcmp(method_name, "destructor") == 0;
            bool is_copy = method_name && strcmp(method_name, "copy") == 0;

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
                    nullptr, 0, 0, QoreValue(), nullptr, false);
                mvb = v; umv = v;
            } else {
                auto* v = new UserMethodVariant(
                    static_cast<ClassAccess>(access), is_final,
                    nullptr, 0, 0, QoreValue(), nullptr, false,
                    QCF_NO_FLAGS, is_abstract);
                mvb = v; umv = v;
            }

            bool sig_has_ellipsis = false;
            bool needs_extra_args_flag = false;
            uint64_t t_sig0 = time_on ? now_us() : 0;
            if (time_on) {
                local_alloc_us += t_sig0 - t_alloc0;
            }
            const std::vector<const QoreTypeInfo*>* tt =
                uses_type_table ? &type_table_resolved : nullptr;
            if (!readAndSetupVariantSignature(reader, type_resolver, pgm, ptr, end,
                    umv, sig_has_ellipsis, needs_extra_args_flag, error, qc, tt)) {
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

                            // Resolve base class by path
                            entry.classid = 0;
                            if (base_path && base_path[0]) {
                                ExceptionSink xsink;
                                const QoreClass* base_cls = pgm->findClass(base_path, &xsink);
                                if (xsink.isException()) {
                                    xsink.clear();
                                }
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

            // Note: QCF_USES_EXTRA_ARGS flag is handled by the overridden hasVarargs()
            // method which checks signature.hasVarargs() directly

            // Add method to class
            uint64_t t_add0 = time_on ? now_us() : 0;
            qore_class_private::addUserMethod(*qc, method_name, mvb, is_static != 0);
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
        priv->ahm.parseInit(*priv, priv->scl);
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
    for (auto& pbca : pending_bcas) {
        if (!pbca.ucv) {
            continue;
        }

        BCAList* bcal = new BCAList();
        for (auto& entry : pbca.entries) {
            // Deserialize arg EXPR_TREE blobs now that all methods are committed
            uint16_t num_args = static_cast<uint16_t>(entry.arg_blobs.size());
            QoreListNode* arg_list = nullptr;
            if (num_args > 0) {
                arg_list = qore_list_private::newList(true);
                qore_list_private::get(*arg_list)->complexTypeInfo =
                    qore_get_complex_list_type(autoTypeInfo);
                for (uint16_t ai = 0; ai < num_args; ++ai) {
                    auto& ab = entry.arg_blobs[ai];
                    if (ab.data && ab.size > 0) {
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

            BCANode* bca_node = new BCANode(entry.classid, arg_list);
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
        const char* compile_file) {
    // Phase 1: Collect all user-defined items into indexed vectors
    // When module_name is provided, filter out items from reexported dependencies
    // When keep_modules is provided, items from those modules are always included
    // When compile_file is provided (Phase 4 slice 4), items whose AST
    // declaration file doesn't match are skipped so the emitted metadata
    // describes only the contributions of one source file.
    printd(5, "serializeNamespaceTree: module_name='%s' compile_file='%s' root_ns='%s'\n",
        module_name ? module_name : "n/a",
        compile_file ? compile_file : "n/a",
        root_ns->ns->getName());
    AOTSerializeState state;
    state.root_ns = root_ns;  // Store root namespace for program-wide CRM building
    collectItems(state, root_ns, UINT32_MAX, module_name, keep_modules, compile_file);

    // Build a program-wide constant reverse map once and make it available to
    // the writer so writeValue() can encode node-pointer references (e.g. an
    // object inside a parse-time-folded hash literal) as VT_CONST_REF entries
    // that resolve at load time. The writer takes a non-owning pointer; the
    // map is cleared after all sections that call writeValue are done.
    AOTConstantReverseMap program_crm;
    if (root_ns) {
        buildProgramConstantReverseMapImpl(root_ns, program_crm);
    }
    writer.const_reverse_map = &program_crm;

    // Phase 2: Write each section
    writeNamespacesSection(writer, state);
    writeClassesSection(writer, state);
    writeHashDeclsSection(writer, state);
    writeEnumsSection(writer, state);
    writeTypedefsSection(writer, state);
    writeConstantsSection(writer, state);
    writeGlobalsSection(writer, state);
    writeFunctionsSection(writer, state);
    writeMethodsSection(writer, state);

    // After every variant signature has been emitted, flush the per-blob
    // type-path table (TYPE_TABLE section).  Must come after
    // writeFunctions/Methods so the table contains every path the
    // variants referenced via writer.internTypePath().
    writer.writeTypeTableSection();

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
