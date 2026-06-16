/*
    ConstantList.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreException.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_aot_deps.h"

#include <cstdlib>
#include <cstring>

static QoreValue resolveRtConstRefDeep(const QoreValue& start, ExceptionSink* xsink, bool& changed);

static void reapplyConstantType(const QoreTypeInfo* typeInfo, QoreValue& value) {
    if (!typeInfo || value.isNothing()) {
        return;
    }

    ExceptionSink type_xsink;
    QoreTypeInfo::retypeValue(value, typeInfo, &type_xsink);
    if (type_xsink) {
        type_xsink.clear();
    }
    QoreTypeInfo::acceptAssignment(typeInfo, "<constant>", value, &type_xsink);
    if (type_xsink) {
        type_xsink.clear();
    }
}

class AOTPreloadedSourceSymbolResolutionHelper {
public:
    DLLLOCAL AOTPreloadedSourceSymbolResolutionHelper()
            : old(qore_aot_set_allow_preloaded_source_symbols(true)) {
    }

    DLLLOCAL ~AOTPreloadedSourceSymbolResolutionHelper() {
        qore_aot_set_allow_preloaded_source_symbols(old);
    }

private:
    bool old;
};

static bool qore_is_deferred_runtime_init_err(const QoreValue& err_val) {
    if (err_val.getType() != NT_STRING) {
        return false;
    }

    QoreStringValueHelper err_str(err_val);
    const char* err = err_str->c_str();
    return !strcmp(err, "EXTERNAL-STUB-CONSTANT")
        || !strcmp(err, "AOT-PENDING-CONSTANT")
        || !strcmp(err, "AOT-PENDING-CLASS")
        || !strcmp(err, "AOT-PENDING-FUNCTION");
}

bool qore_is_deferred_runtime_init_exception(ExceptionSink* xsink) {
    if (!xsink || !xsink->isException()) {
        return false;
    }

    for (QoreException* ex = xsink->getException(); ex; ex = ex->next) {
        if (qore_is_deferred_runtime_init_err(ex->err)) {
            return true;
        }
    }
    return false;
}

#ifdef DEBUG
const char* ClassNs::getName() const {
   return isNs() ? getNs()->name.c_str() : getClass()->name.c_str();
}
#endif

ConstantEntry::ConstantEntry(const QoreProgramLocation* loc, const char* n, QoreValue val, const QoreTypeInfo* ti,
        bool n_pub, bool n_init, bool n_builtin, ClassAccess n_access, QoreParseTypeInfo* pti)
        : loc(loc), name(n), typeInfo(ti), parseTypeInfo(pti), val(val), in_init(false), pub(n_pub),
        init(n_init), builtin(n_builtin), delayed_eval(false), explicit_type(ti || pti), has_init_expr(false),
        saved_val_set(false), aot_shell_pending(false), external_stub(false), external_stub_dependent(false),
        access(n_access) {
    QoreProgram* pgm = getProgram();
    if (pgm)
        pwo = qore_program_private::getParseWarnOptions(pgm);

    const char* mod_name = get_module_context_name();
    if (mod_name) {
        from_module = mod_name;
    }

    //printd(5, "ConstantEntry::ConstantEntry() this: %p '%s' ti: '%s' nti: '%s'\n", this, n,
    //  QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(val.getTypeInfo()));
}

ConstantEntry::ConstantEntry(const ConstantEntry& old)
        : loc(old.loc), pwo(old.pwo), name(old.name),
        typeInfo(old.typeInfo), val(old.val.refSelf()),
        in_init(false), pub(old.pub), init(true), builtin(old.builtin), delayed_eval(old.delayed_eval),
        explicit_type(old.explicit_type),
        has_init_expr(old.has_init_expr),
        saved_val_set(old.saved_val_set),
        aot_shell_pending(old.aot_shell_pending),
        external_stub(old.external_stub),
        external_stub_dependent(old.external_stub_dependent),
        saved_val(old.saved_val.refSelf()),
        access(old.access), from_module(old.from_module) {
    assert(!old.in_init);
    assert(old.init);
    //printd(5, "ConstantEntry::ConstantEntry() this: %p copy '%s' ti: '%s' nti: '%s'\n", this, name.c_str(),
    //  QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(val.getTypeInfo()));
}

void ConstantEntry::del(QoreListNode& l) {
    //printd(5, "ConstantEntry::del(l) this: %p '%s' node: %p (%d) %s %d (saved_val: %s)\n", this, name.c_str(),
    //  node, get_node_type(node), get_type_name(node), node->reference_count(), saved_val.getTypeName());
    aot_init_expr.discard(nullptr);
#ifdef DEBUG
    aot_init_expr.clear();
#endif
    if (saved_val_set) {
        val.discard(nullptr);
        if (saved_val.hasNode()) {
            l.push(saved_val, nullptr);
        } else {
            saved_val.clear();
        }
#ifdef DEBUG
        val.clear();
        saved_val.clear();
#endif
    } else {
        if (val.hasNode()) {
            l.push(val.takeNode(), nullptr);
        }
#ifdef DEBUG
        val.clear();
#endif
    }
}

void ConstantEntry::del(ExceptionSink* xsink) {
    aot_init_expr.discard(xsink);
#ifdef DEBUG
    aot_init_expr.clear();
#endif
    if (saved_val_set) {
        val.discard(xsink);
        saved_val.discard(xsink);
#ifdef DEBUG
        val.clear();
        saved_val.clear();
#endif
    } else {
        // note that objects may be present here when discarding with xsink == nullptr if there is a builtin object in
        // a class constant; in this case the destructor cannot throw an exception
        val.discard(xsink);
#ifdef DEBUG
        val.clear();
#endif
    }
}

void ConstantEntry::setRuntimeValue(QoreValue result, ExceptionSink* xsink) {
    // AOT init functions can lose container metadata while computing values.
    // Re-apply the declared constant type before storing so runtime overload
    // dispatch sees the same value type as source mode.
    reapplyConstantType(typeInfo, result);
    saved_val.discard(xsink);
    if (val.getType() == NT_RTCONSTREF) {
        saved_val = result;
    } else {
        val.discard(xsink);
        val = result;
        saved_val = result.refSelf();
    }
    saved_val_set = true;
    init = true;
    aot_shell_pending = false;
}

void ConstantEntry::materializeRuntimeRefs(ExceptionSink* xsink) {
    QoreValue& target = saved_val_set ? saved_val : val;
    bool changed = false;
    QoreValue resolved = resolveRtConstRefDeep(target, xsink, changed);
    if (xsink && *xsink) {
        resolved.discard(nullptr);
        return;
    }
    if (!changed) {
        resolved.discard(nullptr);
        return;
    }

    reapplyConstantType(typeInfo, resolved);

    target.discard(xsink);
    target = resolved;
    if (!saved_val_set) {
        saved_val.discard(xsink);
        saved_val = target.refSelf();
        saved_val_set = true;
    }
}

void ConstantEntry::makeExternalStubDeclaration() {
    assert(!builtin);
    delayed_eval = false;
    has_init_expr = false;
    aot_shell_pending = false;
    external_stub = true;
    external_stub_dependent = false;
    init = true;

    saved_val.discard(nullptr);
    saved_val_set = false;
    aot_init_expr.discard(nullptr);

    QoreValue placeholder = val;
    val.clear();
    val = new RuntimeConstantRefNode(loc, this, true);
    placeholder.discard(nullptr);
}

int ConstantEntry::parseInit(ClassNs ptr) {
    //printd(5, "ConstantEntry::parseInit() this: %p '%s' pub: %d init: %d in_init: %d node: %p '%s' "
    //  "class context: %p '%s' ns: %p ('%s') pub: %d\n", this, name.c_str(), pub, init, in_init, node,
    //  get_type_name(node), ptr.getClass(), ptr.getClass() ? ptr.getClass()->name.c_str() : "<none>", ptr.getNs(),
    //  ptr.getNs() ? ptr.getNs()->name.c_str() : "<none>", ptr.getNs() ? ptr.getNs()->pub : 0);
    if (init) {
        return 0;
    }

    if (in_init) {
        parse_error(*loc, "recursive constant reference found to constant '%s'", name.c_str());
        return -1;
    }

    ConstantEntryInitHelper ceih(*this);

    if (!val.hasNode()) {
        return 0;
    }

    int err = 0;
    bool external_stub_constant_ref = false;

    QoreProgram* pgm = getProgram();
    if (!builtin) {
        QoreParseContext parse_context;
        parse_context.setFlags(PF_CONST_EXPRESSION);

        // push parse class context
        qore_class_private* p = ptr.getClass();
        QoreParseClassHelper qpch(p ? p->cls : nullptr, ptr.getNs());

        // ensure that there is no accessible local variable state
        VariableBlockHelper vbh;

        // set parse options and warning mask for this statement
        ParseWarnHelper pwh(pwo);

        //printd(5, "ConstantEntry::parseInit() this: %p '%s' about to init val: '%s' class: %p '%s'\n", this,
        //    name.c_str(), val.getFullTypeName(), p, p ? p->name.c_str() : "n/a");

        AOTPreloadedSourceSymbolResolutionHelper apssrh;
        // resolve a deferred explicit declared type here (rather than eagerly in the parser) so that forward
        // references to hashdecls/classes declared later in the same module resolve correctly; the class/namespace
        // parse context pushed above is in scope, matching the behavior of type inference
        if (parseTypeInfo) {
            int resolve_err = 0;
            typeInfo = QoreParseTypeInfo::resolveAndDelete(parseTypeInfo, loc, resolve_err);
            parseTypeInfo = nullptr;
            if (resolve_err) {
                err = resolve_err;
            }
        }

        const QoreTypeInfo* declaredTypeInfo = explicit_type ? typeInfo : nullptr;
        parse_context.expected_type_info = declaredTypeInfo;
        err = parse_init_value(val, parse_context);
        if (declaredTypeInfo) {
            const QoreTypeInfo* exprTypeInfo = parse_context.typeInfo;
            qore_type_result_e res = QoreTypeInfo::parseAccepts(declaredTypeInfo, exprTypeInfo);
            if (res == QTI_NOT_EQUAL && pgm->getParseExceptionSink()) {
                QoreStringNode* edesc = new QoreStringNodeMaker("constant '%s' declared as ", name.c_str());
                QoreTypeInfo::getThisType(declaredTypeInfo, *edesc);
                edesc->concat(", but initializer expression is ");
                QoreTypeInfo::getThisType(exprTypeInfo, *edesc);
                qore_program_private::makeParseException(pgm, *loc, "PARSE-TYPE-ERROR", edesc);
                err = -1;
            }
            typeInfo = declaredTypeInfo;
        } else {
            typeInfo = parse_context.typeInfo;
        }
        external_stub_constant_ref = parse_context.external_stub_constant_ref;

        // Enrich exception with constant name for better debugging
        if (err) {
            ExceptionSink* xsink = getProgram()->getParseExceptionSink();
            if (xsink) {
                xsink->appendLastDescription(" (while initializing constant '%s')", name.c_str());
            }
        }
        assert(!parse_context.lvids);
        pgm = parse_context.pgm;
        assert(pgm == getProgram());
    }

    //printd(5, "ConstantEntry::parseInit() this: %p %s initialized to node: %p (%s) value: %d type: '%s'\n", this,
    //  name.c_str(), node, get_type_name(node), node->is_value(), QoreTypeInfo::getName(typeInfo));

    if (external_stub_constant_ref) {
        external_stub_dependent = true;
    }

    // do not evaluate expression if any parse exceptions have been thrown
    if (!val.hasNode() || !val.getInternalNode()->needs_eval() || pgm->parseExceptionRaised()) {
        if (!QoreTypeInfo::hasType(typeInfo)) {
            typeInfo = val.getTypeInfo();
        } else if (explicit_type && val && !val.needsEval()) {
            ExceptionSink type_xsink;
            QoreTypeInfo::retypeValue(val, typeInfo, &type_xsink);
            if (!type_xsink) {
                QoreTypeInfo::acceptAssignment(typeInfo, "<constant>", val, &type_xsink);
            }
            if (type_xsink) {
                qore_program_private::addParseException(getProgram(), type_xsink, loc);
                err = -1;
            }
        }
        return err;
    }

    delayed_eval = true;
    saved_val = val.takeIfNode();
    saved_val_set = true;
    val = new RuntimeConstantRefNode(loc, this);
    return err;
}

int ConstantEntry::parseCommitRuntimeInit() {
    if (!delayed_eval) {
        return 0;
    }
    delayed_eval = false;
    has_init_expr = true;
    assert(saved_val_set);
    assert(saved_val.needsEval());

    // Preserve the init expression for AOT lowering before evaluation consumes it.
    // The expression AST is ref-counted; this keeps it alive after saved_val is replaced.
    aot_init_expr = saved_val.refSelf();

    if (external_stub_dependent) {
        saved_val.discard(nullptr);
        saved_val_set = false;
        return 0;
    }

    int err = 0;
    bool defer_runtime_init = false;

    // evaluate expression
    ExceptionSink xsink;
    qore_program_private* pgm_priv = qore_program_private::get(*getProgram());
    bool old_parse_commit_in_progress = pgm_priv->parse_commit_in_progress;
    pgm_priv->parse_commit_in_progress = true;
    {
        try {
            ValueEvalOptimizedRefHolder v(saved_val, &xsink);

            //printd(5, "ConstantEntry::parseInit() this: %p %s evaluated to node: %p (%s)\n", this, name.c_str(), *v,
            //  get_type_name(*v));

            if (!xsink) {
                QoreValue nv = v.takeReferencedValue();
                saved_val.discard(&xsink);
                saved_val = nv;
                saved_val_set = true;
                if (explicit_type) {
                    QoreTypeInfo::retypeValue(saved_val, typeInfo, &xsink);
                    if (!xsink) {
                        QoreTypeInfo::acceptAssignment(typeInfo, "<constant>", saved_val, &xsink);
                    }
                } else {
                    typeInfo = saved_val.getTypeInfo();
                }
                assert(!saved_val.getInternalNode() || !saved_val.needsEval());
            } else {
                // The init expression references a value that is not available at
                // compile time: either a qcc --stub constant supplied by the runtime
                // host, an AOT-deserialized shell whose __const_init has not run
                // yet, a sibling function/class that is linked later, or a reflected
                // class/hashdecl lookup committed later in the same parse. Type and
                // cast checks can wrap the deferred dependency as a chained
                // exception, so examine the whole exception chain before deciding
                // this is a hard initialization failure.
                defer_runtime_init = qore_is_deferred_runtime_init_exception(&xsink);
                if (!defer_runtime_init) {
                    typeInfo = nothingTypeInfo;
                }
            }
        } catch (...) {
            pgm_priv->parse_commit_in_progress = old_parse_commit_in_progress;
            throw;
        }
    }
    pgm_priv->parse_commit_in_progress = old_parse_commit_in_progress;

    if (defer_runtime_init) {
        xsink.clear();
        external_stub_dependent = true;
        saved_val.discard(nullptr);
        saved_val_set = false;
        return 0;
    }

    if (xsink.isEvent()) {
        // Enrich exception with constant name for better debugging
        xsink.appendLastDescription(" (while initializing constant '%s')", name.c_str());
        qore_program_private::addParseException(getProgram(), xsink, loc);
        if (!err) {
            err = -1;
        }
    }

    return err;
}

// Collapse a chain of plain RuntimeConstantRefNode indirections to the concrete
// stored value.  This helper returns a borrowed value and is therefore only
// suitable for paths that cannot materialize a computed reference, such as
// ConstantEntry::getValue().
const QoreValue& ConstantEntry::resolveRtConstRef(const QoreValue& start) {
    const QoreValue* v = &start;
    for (unsigned i = 0; v->getType() == NT_RTCONSTREF && i < 65536; ++i) {
        ConstantEntry* rce = v->get<const RuntimeConstantRefNode>()->getConstantEntry();
        if (!rce->saved_val_set) {
            return rce->aot_shell_pending || rce->external_stub ? *v : rce->saved_val;
        }
        v = &rce->saved_val;
    }
    return *v;
}

// Collapse and evaluate RuntimeConstantRefNode values to an owned value.  AOT
// uses RuntimeConstantRefNode subclasses for references to nested constant
// paths; those must be evaluated polymorphically instead of manually unwrapping
// only the base ConstantEntry.
static QoreValue materializeRtConstRefValue(const QoreValue& start, ExceptionSink* xsink, bool& changed) {
    changed = false;
    QoreValue cur = start.refSelf();
    for (unsigned i = 0; cur.getType() == NT_RTCONSTREF && i < 65536; ++i) {
        QoreValue next = cur.eval(xsink);
        cur.discard(nullptr);
        if (*xsink) {
            next.discard(nullptr);
            return QoreValue();
        }
        cur = next;
        changed = true;
    }
    return cur;
}

static QoreValue resolveRtConstRefDeep(const QoreValue& start, ExceptionSink* xsink, bool& changed) {
    QoreValue resolved = materializeRtConstRefValue(start, xsink, changed);
    if (*xsink) {
        return QoreValue();
    }

    // AOT constant containers can hold nested RuntimeConstantRefNode values.
    // Materialize only the containers that actually contain such references.
    if (resolved.getType() == NT_HASH) {
        const QoreHashNode* h = resolved.get<const QoreHashNode>();
        if (!h) {
            return resolved;
        }

        ReferenceHolder<QoreHashNode> rv(xsink);
        ConstHashIterator hi(*h);
        while (hi.next()) {
            bool child_changed;
            QoreValue v = resolveRtConstRefDeep(hi.get(), xsink, child_changed);
            if (*xsink) {
                resolved.discard(nullptr);
                return QoreValue();
            }
            if (child_changed) {
                if (!rv) {
                    rv = h->realCopy();
                }
                // This is not a source-level assignment; it is materializing a
                // serialized constant-reference graph.  Preserve the resolved
                // value exactly, including complex container metadata, instead
                // of applying hash assignment's plain-hash type stripping.
                qore_hash_private::get(**rv)->setKeyValueIntern(hi.getKey(), v);
            } else {
                v.discard(nullptr);
            }
        }

        if (rv) {
            changed = true;
            resolved.discard(nullptr);
            return rv.release();
        }

        return resolved;
    }

    if (resolved.getType() == NT_LIST) {
        const QoreListNode* l = resolved.get<const QoreListNode>();
        if (!l) {
            return resolved;
        }

        ReferenceHolder<QoreListNode> rv(xsink);
        for (size_t i = 0, e = l->size(); i < e; ++i) {
            bool child_changed;
            QoreValue v = resolveRtConstRefDeep(l->retrieveEntry(i), xsink, child_changed);
            if (*xsink) {
                resolved.discard(nullptr);
                return QoreValue();
            }
            if (child_changed) {
                if (!rv) {
                    rv = static_cast<QoreListNode*>(l->realCopy());
                }
                rv->setEntry(i, v, xsink);
                if (*xsink) {
                    resolved.discard(nullptr);
                    return QoreValue();
                }
            } else {
                v.discard(nullptr);
            }
        }

        if (rv) {
            changed = true;
            resolved.discard(nullptr);
            return rv.release();
        }

        return resolved;
    }

    return resolved;
}

QoreValue ConstantEntry::getReferencedValue() const {
    ExceptionSink xsink;
    bool changed;
    QoreValue rv = resolveRtConstRefDeep(val, &xsink, changed);
    if (!xsink) {
        return rv;
    }
    rv.discard(nullptr);

    // This accessor cannot report exceptions. Preserve the historical raw
    // return path for genuinely unresolved AOT/external constants.
    xsink.clear();
    return resolveRtConstRef(val).refSelf();
}

const QoreValue ConstantEntry::getValue() const {
    return resolveRtConstRef(val);
}

ConstantList::ConstantList(const ConstantList& old, const QoreParseOptions& po, ClassNs p) : ptr(p), runtime_init_hwm(old.runtime_init_hwm) {
    //printd(5, "ConstantList::ConstantList(old: %p, p: %s %s) this: %p cls: %p ns: %p\n", &old, p.getType(),
    //  p.getName(), this, ptr.getClass(), ptr.getNs());
    cnemap_t::iterator last = cnemap.begin();
    for (cnemap_t::const_iterator i = old.cnemap.begin(), e = old.cnemap.end(); i != e; ++i) {
        assert(i->second->init);
        // only check copying criteria when copying a constant list in a namespace
        if (p.isNs()) {
            // check the public flag
            if (!i->second->pub)
                continue;
            if (po & PO_NO_INHERIT_USER_CONSTANTS && i->second->isUser())
                continue;
            if (po & PO_NO_INHERIT_SYSTEM_CONSTANTS && i->second->isSystem())
                continue;
        }

        ConstantEntry* ce = i->second;

        if (ce->getModuleName() || !get_module_context_name()) {
            ce->ref();
        } else {
            ce = new ConstantEntry(*ce);
        }

        last = cnemap.insert(last, cnemap_t::value_type(ce->getName(), ce));
        //printd(5, "ConstantList::ConstantList(old=%p) this=%p copying %s (%p)\n", &old, this, i->first,
        //  i->second->node);
    }
}

ConstantList::~ConstantList() {
    //QORE_TRACE("ConstantList::~ConstantList()");
    // for non-debug mode with old modules: clear constants here
    //fprintf(stderr, "XXX ConstantList::~ConstantList() this=%p size=%d\n", this, cnemap.size());

    reset();
}

void ConstantList::reset() {
   if (!cnemap.empty())
      clearIntern(0);
}

void ConstantList::clearIntern(ExceptionSink* xsink) {
    for (auto& i : cnemap) {
        if (i.second) {
            i.second->deref(xsink);
        }
    }

    cnemap.clear();
}

// called at runtime
void ConstantList::clear(QoreListNode& l) {
    for (auto& i : cnemap) {
        if (i.second) {
            i.second->deref(l);
        }
    }

    cnemap.clear();
}

// called at runtime
void ConstantList::deleteAll(ExceptionSink* xsink) {
    clearIntern(xsink);
}

void ConstantList::parseDeleteAll() {
    ExceptionSink xsink;
    clearIntern(&xsink);

    if (xsink.isEvent())
        qore_program_private::addParseException(getProgram(), xsink);
}

static bool parsingExternalStubDeclarations() {
    QoreProgram* pgm = getProgram();
    return pgm && qore_program_private::get(*pgm)->isParsingStubDeclarations();
}

cnemap_t::iterator ConstantList::parseAdd(const QoreProgramLocation* loc, const char* name, QoreValue value,
        const QoreTypeInfo* typeInfo, bool pub, ClassAccess access, QoreParseTypeInfo* parseTypeInfo) {
    // first check if the constant has already been defined
    if (cnemap.find(name) != cnemap.end()) {
        parse_error(*loc, "constant \"%s\" has already been defined", name);
        value.discard(nullptr);
        delete parseTypeInfo;
        return cnemap.end();
    }

    // when an explicit declared type is deferred (parseTypeInfo set), do not infer the type from the value here;
    // keep typeInfo null and let ConstantEntry::parseInit() resolve parseTypeInfo when hashdecls/classes are committed
    const QoreTypeInfo* ti = parseTypeInfo
        ? typeInfo
        : (typeInfo || (value.hasNode() && value.getInternalNode()->needs_eval()) ? typeInfo : value.getTypeInfo());
    ConstantEntry* ce = new ConstantEntry(loc, name, value, ti, pub, false, false, access, parseTypeInfo);
    if (parsingExternalStubDeclarations()) {
        ce->makeExternalStubDeclaration();
    }
    return cnemap.insert(cnemap_t::value_type(ce->getName(), ce)).first;
}

cnemap_t::iterator ConstantList::add(const char* name, QoreValue value, const QoreTypeInfo* typeInfo,
        ClassAccess access) {
#ifdef DEBUG
    if (cnemap.find(name) != cnemap.end()) {
        printd(0, "ConstantList::add() %s added twice!", name);
        assert(false);
    }
#endif
    ConstantEntry* ce = new ConstantEntry(&loc_builtin, name, value,
        typeInfo || (value.hasNode() && value.getInternalNode()->needs_eval()) ? typeInfo : value.getTypeInfo(),
        true, true, true, access);
    return cnemap.insert(cnemap_t::value_type(ce->getName(), ce)).first;
}

ConstantEntry* ConstantList::findEntry(const char* name) {
    cnemap_t::iterator i = cnemap.find(name);
    return i == cnemap.end() ? 0 : i->second;
}

const ConstantEntry* ConstantList::findEntry(const char* name) const {
    cnemap_t::const_iterator i = cnemap.find(name);
    return i == cnemap.end() ? 0 : i->second;
}

QoreValue ConstantList::find(const char* name, const QoreTypeInfo*& constantTypeInfo, ClassAccess& access,
        bool& found) {
    cnemap_t::iterator i = cnemap.find(name);
    if (i != cnemap.end()) {
        if (!i->second->parseInit(ptr)) {
            constantTypeInfo = i->second->getParseTypeInfo();
            access = i->second->getAccess();
            found = true;
            // AOT incremental dependency: see ConstantEntry::get().  Records
            // the defining source file so a folded cross-unit constant/enum
            // reference still triggers a rebuild when that file changes.
            qore_aot_note_referenced_decl(i->second->loc);
            return i->second->val;
        }
        constantTypeInfo = nothingTypeInfo;
        found = true;
        return QoreValue();
    }

    constantTypeInfo = nullptr;
    found = false;
    return QoreValue();
}

bool ConstantList::inList(const char* name) const {
    cnemap_t::const_iterator i = cnemap.find(name);
    return i != cnemap.end() ? true : false;
}

bool ConstantList::inList(const std::string& name) const {
    cnemap_t::const_iterator i = cnemap.find(name.c_str());
    return i != cnemap.end() ? true : false;
}

void ConstantList::mergeUserPublic(const ConstantList& src) {
    for (cnemap_t::const_iterator i = src.cnemap.begin(), e = src.cnemap.end(); i != e; ++i) {
        if (!i->second->isUserPublic()) {
            continue;
        }

        // skip constants that already exist (same module re-imported via different dependency paths,
        // e.g. QUnit -> Util and FsUtil -> Util); scanMergeCommittedNamespace already validated
        // that any existing constant has the same identity
        if (inList(i->first)) {
            continue;
        }

        ConstantEntry* n = new ConstantEntry(*i->second);
        cnemap[n->getName()] = n;
    }
}

int ConstantList::importSystemConstants(const ConstantList& src, ExceptionSink* xsink) {
    for (cnemap_t::const_iterator i = src.cnemap.begin(), e = src.cnemap.end(); i != e; ++i) {
        if (!i->second->isSystem())
            continue;

        if (inList(i->first)) {
            xsink->raiseException("IMPORT-SYSTEM-API-ERROR", "cannot import system constant %s due to an existing " \
                "constant with the same name in the target namespace", i->first);
            return -1;
        }

        ConstantEntry* n = new ConstantEntry(*i->second);
        cnemap[n->getName()] = n;
    }
    return 0;
}

// no duplicate checking is done here
void ConstantList::assimilate(ConstantList& n) {
    for (cnemap_t::iterator i = n.cnemap.begin(), e = n.cnemap.end(); i != e; ++i) {
        assert(!inList(i->first));
        // "move" data to new list
        cnemap[i->first] = i->second;
        i->second = nullptr;
    }

    n.parseDeleteAll();
}

// duplicate checking is done here
void ConstantList::assimilate(ConstantList& n, const char* type, const char* name,
        std::vector<std::string>* pending_names) {
    qore_ns_private* ns = ptr.getNs();
    const bool imported_ns = ns && ns->imported;

    // assimilate target list
    for (cnemap_t::iterator i = n.cnemap.begin(), e = n.cnemap.end(); i != e; ++i) {
        ConstantEntry* existing = findEntry(i->first);
        if (existing) {
            if (imported_ns && existing->isPublic() && i->second->isPublic()) {
                // A child Program can parse a user module that redeclares public API constants inherited from the
                // parent Program. Keep the inherited value and drop the redundant parsed declaration.
                continue;
            }
            parse_error(*i->second->loc, "constant \"%s\" has already been defined in %s \"%s\"", i->first, type,
                name);
            continue;
        }

        cnemap[i->first] = i->second;
        // track the new constant name for rollback support
        if (pending_names) {
            pending_names->push_back(i->first);
        }
        i->second = nullptr;
    }

    n.parseDeleteAll();
}

void ConstantList::parseAdd(const QoreProgramLocation* loc, const std::string& name, QoreValue val,
        ClassAccess access, const char* cname, const QoreTypeInfo* typeInfo, QoreParseTypeInfo* parseTypeInfo) {
    if (inList(name)) {
        parse_error(*loc, "constant \"%s\" has already been defined in class \"%s\"", name.c_str(), cname);
        val.discard(nullptr);
        delete parseTypeInfo;
        return;
    }

    // when an explicit declared type is deferred (parseTypeInfo set), do not infer the type from the value here;
    // keep typeInfo null and let ConstantEntry::parseInit() resolve parseTypeInfo when hashdecls/classes are committed
    const QoreTypeInfo* ti = parseTypeInfo
        ? typeInfo
        : (typeInfo || (val.hasNode() && val.getInternalNode()->needs_eval()) ? typeInfo : val.getTypeInfo());
    ConstantEntry* ce = new ConstantEntry(loc, name.c_str(), val, ti, false, false, false, access, parseTypeInfo);
    if (parsingExternalStubDeclarations()) {
        ce->makeExternalStubDeclaration();
    }
    cnemap[ce->getName()] = ce;
}

int ConstantList::parseInit() {
    int err = 0;
    for (cnemap_t::iterator i = cnemap.begin(), e = cnemap.end(); i != e; ++i) {
        //printd(5, "ConstantList::parseInit() this: %p '%s' %p (class: %p '%s' ns: %p '%s')\n", this, i->first,
        //  i->second->node, ptr.getClass(), ptr.getClass() ? ptr.getClass()->name.c_str() : "n/a", ptr.getNs(),
        //  ptr.getNs() ? ptr.getNs()->name.c_str() : "n/a");
        if (i->second->parseInit(ptr) && !err) {
            err = -1;
        }
    }
    return err;
}

int ConstantList::parseCommitRuntimeInit() {
    int err = 0;
    // Initialize only constants beyond the high water mark to avoid double evaluation
    // while allowing new constants added in REPL sessions (AOT mode) to be initialized
    size_t idx = 0;
    for (auto& i : cnemap) {
        if (idx >= runtime_init_hwm) {
            //printd(5, "ConstantList::parseInit() this: %p '%s' (class: %p '%s' ns: %p '%s')\n", this, i.first,
            //  ptr.getClass(), ptr.getClass() ? ptr.getClass()->name.c_str() : "n/a", ptr.getNs(),
            //  ptr.getNs() ? ptr.getNs()->name.c_str() : "n/a");
            if (i.second->parseCommitRuntimeInit() && !err) {
                err = -1;
            }
        }
        ++idx;
    }
    runtime_init_hwm = cnemap.size();
    return err;
}

QoreHashNode* ConstantList::getInfo() {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);

    qore_hash_private* hp = qore_hash_private::get(*h);
    for (cnemap_t::iterator i = cnemap.begin(), e = cnemap.end(); i != e; ++i)
        hp->setKeyValueIntern(i->first, i->second->val.refSelf());

    return h;
}

void ConstantList::parseRemove(const char* name, ExceptionSink* xsink) {
    cnemap_t::iterator i = cnemap.find(name);
    if (i != cnemap.end()) {
        i->second->deref(xsink);
        cnemap.erase(i);
    }
}
