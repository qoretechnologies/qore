/*
    VarRefNode.cpp

    Qore programming language

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

#include <qore/Qore.h>
#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/ParserSupport.h"
#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/LocalVar.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/QoreStatementTemps.h"

static std::string qore_aot_deferred_object_type_class_path(const QoreTypeInfo* typeInfo) {
    if (!qore_type_info_is_aot_deferred(typeInfo)
            || QoreTypeInfo::parseReturns(typeInfo, NT_OBJECT) == QTI_NOT_EQUAL) {
        return std::string();
    }

    std::string path = qore_get_aot_serializable_type_path(typeInfo);
    const char* start = nullptr;
    if (path.rfind("object<", 0) == 0) {
        start = path.c_str() + 7;
    } else if (path.rfind("*object<", 0) == 0) {
        start = path.c_str() + 8;
    }
    if (!start) {
        return std::string();
    }
    const char* end = strrchr(start, '>');
    return end && end > start ? std::string(start, end - start) : std::string();
}

static std::string qore_var_ref_aot_deferred_hashdecl_type_path(const QoreTypeInfo* typeInfo) {
    if (!qore_type_info_is_aot_deferred(typeInfo)
            || QoreTypeInfo::parseReturns(typeInfo, NT_HASH) == QTI_NOT_EQUAL) {
        return std::string();
    }

    std::string path = qore_get_aot_serializable_type_path(typeInfo);
    const char* start = nullptr;
    if (path.rfind("hash<", 0) == 0) {
        start = path.c_str() + 5;
    } else if (path.rfind("*hash<", 0) == 0) {
        start = path.c_str() + 6;
    }
    if (!start) {
        return std::string();
    }
    const char* end = strrchr(start, '>');
    return end && end > start ? std::string(start, end - start) : std::string();
}

bool VarRefNode::parseEqualTo(const VarRefNode& other) const {
    if (type != other.type) {
        return false;
    }
    switch (type) {
        case VT_LOCAL:
        case VT_CLOSURE:
        case VT_LOCAL_TS:
            return ref.id == other.ref.id;
        case VT_GLOBAL:
        case VT_THREAD_LOCAL:
            return ref.var->parseGetVar() == other.ref.var->parseGetVar();
        case VT_IMMEDIATE:
            return ref.cvv == other.ref.cvv;
        default:
            assert(false);
            break;
    }

    return false;
}

bool VarRefNode::parseIsReadOnly() const {
    if (read_only_decl) {
        return true;
    }

    switch (type) {
        case VT_LOCAL:
        case VT_CLOSURE:
        case VT_LOCAL_TS:
            return ref.id && ref.id->isReadOnly();
        case VT_IMMEDIATE:
            return ref.cvv && ref.cvv->isReadOnly();
        default:
            return false;
    }
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int VarRefNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    const char* typestr;
    switch (type) {
        case VT_GLOBAL:
            typestr = "global";
            break;
        case VT_THREAD_LOCAL:
            typestr = "thread_local";
            break;
        case VT_LOCAL:
            typestr = "local";
            break;
        default:
            typestr = "unresolved";
            break;
    }

    str.sprintf("variable reference '%s' %s (%p)", name.ostr, typestr, this);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *VarRefNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
   del = true;
   QoreString *rv = new QoreString;
   getAsString(*rv, foff, xsink);
   return rv;
}

// returns the type name as a c string
const char *VarRefNode::getTypeName() const {
   return "variable reference";
}

void VarRefNode::resolve(const QoreTypeInfo* typeInfo) {
    LocalVar* id;

    printd(5, "VarRefNode::resolve() name: '%s' size: %d\n", name.ostr, name.size());

    bool in_closure;
    if (name.size() == 1 && (id = find_local_var(name.ostr, in_closure))) {
        if (typeInfo) {
            parse_error(*loc, "type definition given for existing local variable '%s'", id->getName());
        }

        ref.id = id;
        if (in_closure) {
            setClosureIntern();
        } else {
            type = VT_LOCAL;
        }

        printd(5, "VarRefNode::resolve(): local var %s resolved (id: %p, in_closure: %d)\n", name.ostr, ref.id,
            in_closure);
    } else {
        ref.var = qore_root_ns_private::parseCheckImplicitGlobalVar(loc, name, typeInfo);
        type = ref.var->isThreadLocal() ? VT_THREAD_LOCAL : VT_GLOBAL;
        printd(5, "VarRefNode::resolve(): implicit global var %s resolved (var: %p type: %d)\n", name.ostr, ref.var,
            type);
    }
}

QoreValue VarRefNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue VarRefNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    QoreValue v = evalValue(rc, needs_deref, xsink);
    // a read that gives the code a reference of its own - through a reference, or from a closure-bound, global or
    // thread-local variable - gives a temporary of the statement: the statement holds the value until it ends, so
    // that a later part of the statement that assigns the variable, or the variable that a reference refers to,
    // another value cannot release the value that an earlier part of the statement read and is still using, as in
    // the other execution modes, which keep the read's reference in the temp scope of the statement until
    // DiscardTemps.  A value borrowed from the variable (a local that holds the value itself, or a weak reference
    // to it) is not held: it is not a temporary, and the other modes do not keep it either
    if (needs_deref && !*xsink) {
        QoreStatementTempScope::holdRead(this, v, xsink);
    }
    return v;
}

QoreValue VarRefNode::evalCallArg(ExceptionSink* xsink) const {
    bool needs_deref = true;
    QoreValue v = evalRaw(needs_deref, xsink);
    // the runtime configuration is needed only to follow a reference: the read of a variable that holds a value
    // itself, which is nearly every argument, does not look the thread's configuration up
    AbstractQoreNode* n = v.getInternalNode();
    if (n && n->getType() == NT_REFERENCE) {
        v = followReference(v, rc_get_current_ref(), needs_deref, xsink);
    }
    if (*xsink) {
        if (needs_deref) {
            v.discard(xsink);
        }
        return QoreValue();
    }
    // a value borrowed from the variable is referenced for the argument list
    return needs_deref ? v : v.refSelf();
}

QoreValue VarRefNode::evalValue(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    QoreValue v = evalRaw(needs_deref, xsink);
    AbstractQoreNode* n = v.getInternalNode();
    if (n && n->getType() == NT_REFERENCE) {
        return followReference(v, rc, needs_deref, xsink);
    }
    return v;
}

QoreValue VarRefNode::followReference(QoreValue v, RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) {
    ReferenceNode* r = reinterpret_cast<ReferenceNode*>(v.getInternalNode());
    bool nd = true;
    QoreValue nv = r->eval(rc, nd, xsink);
    if (needs_deref) {
        discard(v.getInternalNode(), xsink);
    }
    needs_deref = nd;
    return nv;
}

QoreValue VarRefNode::evalRaw(bool& needs_deref, ExceptionSink* xsink) const {
    QoreValue v{};
    if (type == VT_LOCAL) {
        //printd(5, "VarRefNode::evalImpl() this: %p lvar %p (%s) v: '%s' pgm: %p\n", this, ref.id, ref.id->getName(),
        //    v.getTypeName(), getProgram());
        v = ref.id->eval(needs_deref, xsink);
    } else if (type == VT_CLOSURE) {
        ClosureVarValue* val = thread_get_runtime_closure_var(ref.id);
        v = val->eval(needs_deref, xsink);
    } else if (type == VT_LOCAL_TS) {
        ClosureVarValue* val = thread_find_closure_var(ref.id->getName());
        v = val->eval(needs_deref, xsink);
    } else if (type == VT_IMMEDIATE) {
        v = ref.cvv->eval(needs_deref, xsink);
    } else {
        assert(needs_deref);
        printd(5, "VarRefNode::evalImpl() this: %p %s var: %p (%s)\n", this,
            type == VT_THREAD_LOCAL ? "thread_local" : "global", ref.var, ref.var->getName());
        v = ref.var->eval();
    }
    return v;
}

int VarRefNode::parseInitIntern(QoreParseContext& parse_context, bool is_new) {
    if (parse_context.pflag & PF_CONST_EXPRESSION) {
        parseException(*loc, "ILLEGAL-VARIABLE-REFERENCE", "variable reference '%s' used illegally in an " \
            "expression executed at parse time to initialize a constant value", name.ostr);
        return -1;
    }

    int err = 0;

    printd(5, "VarRefNode::parseInitIntern() this: %p '%s' type: %d %p '%s'\n", this, name.ostr, type,
        parse_context.typeInfo, QoreTypeInfo::getName(parse_context.typeInfo));
    // if it is a new variable being declared
    if (type == VT_LOCAL || type == VT_CLOSURE || type == VT_LOCAL_TS) {
        if (!ref.id) {
            ref.id = push_local_var(name.ostr, loc, parse_context.typeInfo, err, false, is_new ? 1 : 0,
                parse_context.pflag);
            if (read_only_decl) {
                ref.id->setReadOnly();
            }
            ++parse_context.lvids;
        }
        //printd(5, "VarRefNode::parseInitIntern() this: %p local var '%s' declared (id: %p)\n", this, name.ostr, ref.id);
    } else if (type != VT_GLOBAL && type != VT_THREAD_LOCAL) {
        assert(type == VT_UNRESOLVED);
        // otherwise reference must be resolved
        resolve(parse_context.typeInfo);
    }

    name.optimize();
    return err;
}

int VarRefNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = nullptr;
    parse_context.analysis.clear();
    int err = parseInitIntern(parse_context);

    bool is_assignment = parse_context.pflag & PF_FOR_ASSIGNMENT;

    // this expression returns nothing if it's a new local variable declaration
    // so if we're not assigning we return nothingTypeInfo as the return type
    if (!is_assignment && new_decl) {
        parse_context.typeInfo = nothingTypeInfo;
    } else if (is_assignment && new_decl) {
        parse_context.typeInfo = parseGetTypeInfoForInitialAssignment();
    } else {
        parse_context.typeInfo = parseGetTypeInfo();
    }

    // Set PF_NARROWED_TYPE flag if this variable has a narrowed type
    if (type == VT_LOCAL || type == VT_CLOSURE || type == VT_LOCAL_TS) {
        if (ref.id && ref.id->isAutoType() && ref.id->parseGetNarrowedType()) {
            parse_context.pflag |= PF_NARROWED_TYPE;
        }
    } else if (type == VT_GLOBAL || type == VT_THREAD_LOCAL) {
        if (ref.var && ref.var->isAutoType() && ref.var->parseGetNarrowedType()) {
            parse_context.pflag |= PF_NARROWED_TYPE;
        }
    }

    if (parse_context.typeInfo) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = parse_context.typeInfo;
    }

    if (type == VT_LOCAL || type == VT_CLOSURE || type == VT_LOCAL_TS) {
        if (ref.id && ref.id->isAssigned()) {
            parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
        }
        // NOTE: Don't set analysis.narrowed_type here - use parse_context.typeInfo from parseGetTypeInfo() instead
        // Setting it here creates stale cached information that doesn't reflect post-merge narrowing state
        //if (ref.id && ref.id->parseGetNarrowedType()) {
        //    parse_context.analysis.narrowed_type = ref.id->parseGetNarrowedType();
        //}
    } else if (type == VT_GLOBAL || type == VT_THREAD_LOCAL) {
        if (ref.var && ref.var->parseGetNarrowedType()) {
            parse_context.analysis.narrowed_type = ref.var->parseGetNarrowedType();
        }
    }

    if (parse_context.analysis.narrowed_type) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
    }

    // the assignment state of the variable at this read (qore#5937); the parse is one forward pass, so a read is
    // proven unassigned only when no assignment parsed before it can have run, and no assignment parsed after it can
    // run before it: the variable is not changed through a reference or by a closure, and no loop body, iterated
    // expression or on_exit block lies between its declaration and the read
    if (!new_decl && !is_assignment && (type == VT_LOCAL || type == VT_CLOSURE || type == VT_LOCAL_TS) && ref.id) {
        if (parse_context.pflag & PF_LVALUE_NOT_READ) {
            // the reference, delete and remove operators do not read the variable, and a reference can assign it
            read_state = qore_local_read_state_e::Unknown;
        } else if (ref.id->isAssigned() || ref.id->isSelf()) {
            // the self variable of a method has the object whenever the method runs; it is not assigned in the code
            read_state = qore_local_read_state_e::Assigned;
        } else if (type == VT_LOCAL && !ref.id->mayBeAssigned() && !ref.id->closureUse() && !ref.id->isTopLevel()
                && parse_get_deferred_depth() <= ref.id->getDeclDeferredDepth()) {
            // a top-level local of a program is also a variable of its functions and methods, which can assign or
            // remove it whenever they are called, and whose code the parse does not see in the order it runs
            read_state = qore_local_read_state_e::Unassigned;
        } else {
            read_state = qore_local_read_state_e::Unknown;
        }
    }

    // A declaration constrains a value but does not initialize it.  The flow-sensitive LocalVar
    // state is authoritative here: it is merged across branches by AssignedStateHelper, so this
    // diagnostic is raised only when the read is not definitely preceded by an assignment.  `exists`
    // is the intentional way to inspect that state and suppresses it through its parse flag.  A read that is proven
    // unassigned where its value is used as a value of a type that does not accept NOTHING is a parse error with
    // %strict-parse-errors; see qore_parse_check_unassigned_read()
    if (!err && !new_decl && !is_assignment && !(parse_context.pflag & (PF_EXISTENCE_CHECK | PF_LVALUE_NOT_READ))
            && read_state != qore_local_read_state_e::Assigned) {
        assert(ref.id);
        const QoreTypeInfo* declared_type = ref.id->getTypeInfo();
        if (QoreTypeInfo::hasType(declared_type)
                && QoreTypeInfo::parseReturns(declared_type, NT_NOTHING) == QTI_NOT_EQUAL) {
            QoreStringMaker hint("initialize '%s' before this read, or declare it with an optional type if "
                "the absent value is intentional", ref.id->getName());
            QoreDiagnosticMetadata metadata("UNINITIALIZED-LOCAL-READ", hint.c_str());
            metadata.addFact("variable", ref.id->getName());
            metadata.addFact("declaredType", QoreTypeInfo::getName(declared_type));
            metadata.addFact("definitelyAssigned", "false");
            metadata.addFact("assignmentState", read_state == qore_local_read_state_e::Unassigned
                ? "unassigned" : "unknown");
            metadata.addSuggestion(QoreStringMaker("initialize %s before this read", ref.id->getName()).c_str());
            QoreStringNode* desc = new QoreStringNodeMaker("local variable '%s' with non-optional type '%s' "
                "is read before it is definitely assigned; a declaration does not initialize the variable",
                ref.id->getName(), QoreTypeInfo::getName(declared_type));
            qore_program_private::makeParseWarning(parse_context.pgm, *loc, QP_WARN_LANGUAGE_TRAPS,
                "LANGUAGE-TRAP", desc, metadata);
        }
    }

    return err;
}

template <typename... Args>
static VarRefNewObjectNode* qore_make_var_ref_new(Args&&... args) {
    LexicalSemanticPolicy policy;
    LexicalParsePolicyHelper::capture(policy);
    return !policy.isDefault()
        ? new LexicalVarRefNewObjectNode(std::forward<Args>(args)...)
        : new VarRefNewObjectNode(std::forward<Args>(args)...);
}

VarRefNewObjectNode* VarRefNode::globalMakeNewCall(QoreValue args) {
    assert(type == VT_GLOBAL || type == VT_THREAD_LOCAL);
    if (ref.var->hasTypeInfo()) {
        QoreParseTypeInfo* pti = ref.var->copyParseTypeInfo();
        VarRefNewObjectNode* rv = qore_make_var_ref_new(loc, takeName(), ref.var,
            make_args(loc, args), pti ? nullptr : ref.var->getTypeInfo(), pti);
        deref();
        return rv;
    }

    return nullptr;
}

AbstractQoreNode* VarRefNode::makeNewCall(QoreValue args) {
    return (type == VT_GLOBAL || type == VT_THREAD_LOCAL) && new_decl
        ? globalMakeNewCall(args)
        : nullptr;
}

void VarRefNode::setPublic() {
    // a "public thread_local" declaration in an expression, such as a top-level statement of a Program, is a
    // thread_local variable reference
    assert(type == VT_GLOBAL || type == VT_THREAD_LOCAL);
    ref.var->setPublic();
    if (type == VT_THREAD_LOCAL) {
        qore_ns_private::checkPublicThreadLocalDecl(*ref.var);
    }
}

void VarRefNode::makeGlobal(qore_var_t type) {
    assert(this->type != VT_GLOBAL);
    assert(this->type == VT_UNRESOLVED || !ref.id);

    ref.var = qore_root_ns_private::parseAddGlobalVarDef(loc, name, nullptr, type);
    this->type = type;
    new_decl = true;
}

int VarRefNode::getLValue(LValueHelper& lvh, bool for_remove) const {
    if (type == VT_LOCAL)
        return ref.id->getLValue(lvh, for_remove, new_decl);
    if (type == VT_CLOSURE)
        return thread_get_runtime_closure_var(ref.id)->getLValue(lvh, for_remove);
    if (type == VT_LOCAL_TS)
        return thread_find_closure_var(ref.id->getName())->getLValue(lvh, for_remove);
    if (type == VT_IMMEDIATE)
        return ref.cvv->getLValue(lvh, for_remove);
    assert(type == VT_GLOBAL || type == VT_THREAD_LOCAL);
    return ref.var->getLValue(lvh, for_remove);
}

void VarRefNode::remove(LValueRemoveHelper& lvrh) {
    if (type == VT_LOCAL)
        return ref.id->remove(lvrh);
    if (type == VT_CLOSURE)
        return thread_get_runtime_closure_var(ref.id)->remove(lvrh);
    if (type == VT_LOCAL_TS)
        return thread_find_closure_var(ref.id->getName())->remove(lvrh);
    if (type == VT_IMMEDIATE)
        return ref.cvv->remove(lvrh);
    assert(type == VT_GLOBAL || type == VT_THREAD_LOCAL);
    return ref.var->remove(lvrh);
}

bool VarRefNode::scanMembers(RSetHelper& rsh) {
    if (type == VT_CLOSURE)
        return rsh.checkNode(*thread_get_runtime_closure_var(ref.id));
    if (type == VT_LOCAL_TS)
        return rsh.checkNode(*thread_find_closure_var(ref.id->getName()));
    if (type == VT_IMMEDIATE)
        return rsh.checkNode(*ref.cvv);
    // never called with type == VT_LOCAL
    // we don't scan global vars; they are deleted explicitly when the program goes out of scope
    // or thread_local vars; they are deleted explicitly when threads terminate as well
    assert(type == VT_GLOBAL || type == VT_THREAD_LOCAL);
    return false;
}

GlobalVarRefNode::GlobalVarRefNode(const QoreProgramLocation* loc, char* n, const QoreTypeInfo* typeInfo,
        qore_var_t type) : VarRefNode(loc, n, nullptr, false, true, type) {
    explicit_scope = true;
    ref.var = qore_root_ns_private::parseAddResolvedGlobalVarDef(loc, name, typeInfo, type);
}

GlobalVarRefNode::GlobalVarRefNode(const QoreProgramLocation* loc, char* n, QoreParseTypeInfo* parseTypeInfo,
        qore_var_t type) : VarRefNode(loc, n, nullptr, false, true, type) {
    explicit_scope = true;
    ref.var = qore_root_ns_private::parseAddGlobalVarDef(loc, name, parseTypeInfo, type);
}

int VarRefDeclNode::parseInitCommon(QoreParseContext& parse_context, bool is_new) {
    int err = 0;
    if (!typeInfo) {
        typeInfo = QoreParseTypeInfo::resolveAndDelete(parseTypeInfo, loc, err);
        parseTypeInfo = nullptr;
    }
#ifdef DEBUG
    else assert(!parseTypeInfo);
#endif

    parse_context.typeInfo = typeInfo;
    return parseInitIntern(parse_context, is_new) || err ? -1 : 0;
}

int VarRefDeclNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    if (parseInitCommon(parse_context)) {
        return -1;
    }
    bool is_assignment = parse_context.pflag & PF_FOR_ASSIGNMENT;
    if (read_only_decl && new_decl && !is_assignment) {
        parseException(*loc, "READONLY-VARIABLE-ASSIGNMENT-ERROR",
            "read-only local variable '%s' requires an initializer", getName());
        return -1;
    }

    // this expression returns nothing if it's a new local variable
    // so if we're not assigning we return nothingTypeInfo as the
    // return type
    if (!is_assignment && new_decl) {
        parse_context.typeInfo = nothingTypeInfo;
    } else {
        if (is_assignment && new_decl) {
            parse_context.typeInfo = parseGetTypeInfoForInitialAssignment();
        } else {
            parse_context.typeInfo = parseGetTypeInfo();
        }
    }

    // Set PF_NARROWED_TYPE flag if this variable has a narrowed type
    if (type == VT_LOCAL || type == VT_CLOSURE || type == VT_LOCAL_TS) {
        if (ref.id && ref.id->isAutoType() && ref.id->parseGetNarrowedType()) {
            parse_context.pflag |= PF_NARROWED_TYPE;
        }
    } else if (type == VT_GLOBAL || type == VT_THREAD_LOCAL) {
        if (ref.var && ref.var->isAutoType() && ref.var->parseGetNarrowedType()) {
            parse_context.pflag |= PF_NARROWED_TYPE;
        }
    }

    return 0;
}

// for checking for new object calls
AbstractQoreNode* VarRefDeclNode::makeNewCall(QoreValue args) {
    VarRefNewObjectNode* rv = qore_make_var_ref_new(loc, takeName(), typeInfo, takeParseTypeInfo(),
        make_args(loc, args), static_cast<qore_var_t>(type));
    if (isReadOnlyDecl()) {
        rv->setReadOnlyDecl();
    }
    deref();
    return rv;
}

void VarRefDeclNode::makeGlobal(qore_var_t type) {
    // could be tagged as local if allow-bare-refs is enabled
    assert(this->type == VT_UNRESOLVED || (this->type == VT_LOCAL && parse_check_parse_option(PO_ALLOW_BARE_REFS)));

    if (parseTypeInfo) {
        ref.var = qore_root_ns_private::parseAddGlobalVarDef(loc, name, takeParseTypeInfo(), type);
    } else {
        ref.var = qore_root_ns_private::parseAddResolvedGlobalVarDef(loc, name, typeInfo, type);
    }
    this->type = type;
    new_decl = true;
}

int VarRefNewObjectNode::parseInitConstructorCall(const QoreProgramLocation* loc, QoreParseContext& parse_context,
        const QoreClass* qc) {
    assert(qc);
    int err = 0;
    // throw an exception if trying to instantiate a class with abstract method variants
    qore_class_private::get(*const_cast<QoreClass*>(qc))->parseCheckAbstractNew(loc);

    if (qore_program_private::parseAddDomain(parse_context.pgm, qc->getDomain())) {
        parseException(*loc, "ILLEGAL-CLASS-INSTANTIATION", "parse options do not allow access to the '%s' class",
            qc->getName());
        err = -1;
    }

    // FIXME: make common code with ScopedObjectCallNode
    const QoreMethod* constructor = qc ? qore_class_private::get(*qc)->parseGetConstructor() : nullptr;
    setReceiverTypeInfo(typeInfo);
    int e = parseArgsVariant(loc, parse_context, constructor
        ? qore_method_private::get(*constructor)->getFunction()
        : nullptr, nullptr);
    if (e && !err) {
        err = -1;
    }

    //printd(5, "VarRefFunctionCallBase::parseInitConstructorCall() this: %p constructor: %p variant: %p\n", this,
    //  constructor, variant);

    if (((constructor && (qore_method_private::getAccess(*constructor) > Public))
        || (variant && CONMV_const(variant)->isPrivate()))
            && !qore_class_private::parseCheckPrivateClassAccess(*qc)) {
        if (variant) {
            parse_error(*loc, "illegal external access to private constructor %s::constructor(%s)", qc->getName(),
                variant->getSignature()->getSignatureText());
        } else {
            parse_error(*loc, "illegal external access to private constructor of class %s", qc->getName());
        }
        if (!err) {
            err = -1;
        }
    }

    printd(5, "VarRefFunctionCallBase::parseInitConstructorCall() this: %p class: %s (%p) constructor: %p " \
        "function: %p variant: %p\n", this, qc->getName(), qc, constructor,
        constructor ? qore_method_private::get(*constructor)->getFunction() : nullptr, variant);
    return err;
}

int VarRefNewObjectNode::parseInitHashDeclInitialization(const QoreProgramLocation* loc,
        QoreParseContext& parse_context, const TypedHashDecl* hd) {
    assert(hd);
    return typed_hash_decl_private::get(*hd)->parseInitHashDeclInitialization(loc, parse_context, parse_args,
        runtime_check);
}

int VarRefNewObjectNode::parseInitComplexHashInitialization(const QoreProgramLocation* loc,
        QoreParseContext& parse_context) {
    assert(parse_context.typeInfo);
    return qore_hash_private::parseInitComplexHashInitialization(loc, parse_context, parse_args);
}

int VarRefNewObjectNode::parseInitComplexListInitialization(const QoreProgramLocation* loc,
        QoreParseContext& parse_context) {
    assert(parse_context.typeInfo);
    return qore_list_private::parseInitComplexListInitialization(loc, parse_context, takeParseArgs(), new_args);
}

int VarRefNewObjectNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    if (parseInitCommon(parse_context, true)) {
        return -1;
    }

    int err = 0;
    // parseAssigned is only valid for local variables - ref.id/ref.var share a union,
    // so calling parseAssigned on a global Var* (cast to LocalVar*) would write into
    // Var::name's std::string pointer field at the LocalVar::parse_assigned offset
    const bool is_local_type = (type == VT_LOCAL || type == VT_CLOSURE || type == VT_LOCAL_TS);
    const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(typeInfo);
    if (qc) {
        err = parseInitConstructorCall(loc, parse_context, qc);
        if (!err && is_local_type && ref.id) {
            // Mark the variable as assigned after successful constructor call
            ref.id->parseAssigned();
        }
        vrn_type = VRN_OBJECT;
    } else {
        // Check complex hash/list BEFORE hashdecl, since hash<HashdeclType> has both
        const QoreTypeInfo* ti = typeInfo == autoHashTypeInfo
            ? autoTypeInfo
            : QoreTypeInfo::getUniqueReturnComplexHash(typeInfo);
        //printd(5, "VarRefNewObjectNode::parseInitImpl() ti: %p type: '%s' ti: %p '%s'\n", typeInfo,
        //  QoreTypeInfo::getName(typeInfo), ti, QoreTypeInfo::getName(ti));
        if (ti) {
            parse_context.typeInfo = ti;
            err = parseInitComplexHashInitialization(loc, parse_context);
            vrn_type = VRN_COMPLEXHASH;
            if (!err && is_local_type && ref.id) {
                ref.id->parseAssigned();
            }
        } else {
            ti = typeInfo == autoListTypeInfo ? autoTypeInfo : QoreTypeInfo::getUniqueReturnComplexList(typeInfo);
            if (ti) {
                parse_context.typeInfo = ti;
                err = parseInitComplexListInitialization(loc, parse_context);
                vrn_type = VRN_COMPLEXLIST;
                if (!err && is_local_type && ref.id) {
                    ref.id->parseAssigned();
                }
            } else {
                const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(typeInfo);
                if (hd) {
                    err = parseInitHashDeclInitialization(loc, parse_context, hd);
                    vrn_type = VRN_HASHDECL;
                    if (!err && is_local_type && ref.id) {
                        ref.id->parseAssigned();
                    }
                } else if (qore_aot_source_parse_active()
                        && !(dynamic_hashdecl_name = qore_var_ref_aot_deferred_hashdecl_type_path(typeInfo))
                            .empty()) {
                    // the initializer is parsed as for a hashdecl bound at parse time, so it stays in parse_args
                    // for every consumer; its keys cannot be checked against the deferred hashdecl here, so they
                    // are checked when the hash is constructed
                    parse_context.typeInfo = nullptr;
                    QoreValue arg{};
                    qore_hash_private::parseInitHashInitialization(loc, parse_context, parse_args, arg, err);
                    runtime_check = true;
                    vrn_type = VRN_DYNAMIC_HASHDECL;
                    if (!err && is_local_type && ref.id) {
                        ref.id->parseAssigned();
                    }
                } else if (qore_aot_source_parse_active()
                        && !(dynamic_class_name = qore_aot_deferred_object_type_class_path(typeInfo)).empty()) {
                    setReceiverTypeInfo(typeInfo);
                    err = parseArgsVariant(loc, parse_context, nullptr, nullptr);
                    vrn_type = VRN_DYNAMIC_OBJECT;
                    if (!err && is_local_type && ref.id) {
                        ref.id->parseAssigned();
                    }
                } else {
                    parse_error(*loc, "type '%s' does not support implied constructor instantiation",
                        QoreTypeInfo::getName(typeInfo));
                    err = -1;
                }
            }
        }
    }

    if (parse_context.pflag & PF_FOR_ASSIGNMENT) {
        parse_error(*loc, "variable instantiation with the implied contructor syntax implies an assignment; it is " \
            "an error to make an additional assignment");
        if (!err) {
            err = -1;
        }
    }

    if (!err && (type == VT_GLOBAL || type == VT_THREAD_LOCAL) && ref.var) {
        ref.var->preserveAOTInitExpr(val, true);
    }

    parse_context.typeInfo = typeInfo;
    return err;
}

//! Reports a class deferred at parse time that cannot be resolved when the object is constructed
/** During an AOT source parse the class is a build-group declaration that is linked later: the compile resolves
    against preloaded objects that need not declare it, and code run at parse commit (a static variable initializer)
    can reach the construction before it exists.  That is reported as pending linking, as a scoped `new` or a static
    call reports it, so that the initializer is deferred rather than failing the compile.  Anywhere else the class
    is simply missing.
*/
static void raiseUnresolvedDynamicClass(const std::string& class_name, ExceptionSink* xsink) {
    if (*xsink) {
        return;
    }
    if (qore_aot_source_parse_active()) {
        xsink->raiseException("AOT-PENDING-CLASS", "class '%s' is pending AOT source linking for instantiation",
            class_name.c_str());
        return;
    }
    xsink->raiseException("CREATE-OBJECT-ERROR", "cannot resolve class '%s' for instantiation", class_name.c_str());
}

QoreValue VarRefNewObjectNode::constructValue(ExceptionSink* xsink) const {
    // NOTE: VRN_OBJECT is handled separately by the NewObject IR opcode, which calls
    // qore_class_private::execConstructor() directly. This method only handles the
    // non-object typed container construction cases.
    const QoreTypeInfo* runtime_type_info = qore_substitute_type_params_if_needed(typeInfo);
    switch (vrn_type) {
        case VRN_HASHDECL:
            return typed_hash_decl_private::get(*QoreTypeInfo::getUniqueReturnHashDecl(runtime_type_info))
                ->newHash(parse_args, runtime_check, xsink);

        case VRN_COMPLEXHASH:
            return qore_hash_private::newComplexHash(runtime_type_info, parse_args, xsink);

        case VRN_COMPLEXLIST:
            return qore_list_private::newComplexList(runtime_type_info, new_args, xsink);

        case VRN_DYNAMIC_OBJECT: {
            const QoreClass* qc = qore_aot_resolve_class_ref(getProgram(), dynamic_class_name.c_str(), false);
            if (!qc) {
                raiseUnresolvedDynamicClass(dynamic_class_name, xsink);
                return QoreValue();
            }
            if (qore_class_private::runtimeCheckInstantiateClassByName(*qc, getProgram(), xsink)) {
                return QoreValue();
            }
            return qc->execConstructor(args, xsink);
        }

        case VRN_DYNAMIC_HASHDECL: {
            QoreProgram* pgm = getProgram();
            if (!pgm) {
                xsink->raiseException("HASHDECL-ERROR",
                    "cannot resolve hashdecl '%s' for instantiation: no program context",
                    dynamic_hashdecl_name.c_str());
                return QoreValue();
            }
            qore_program_private* pp = qore_program_private::get(*pgm);
            const qore_ns_private* found_ns = nullptr;
            const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(*pp->RootNS,
                dynamic_hashdecl_name.c_str(), found_ns);
            if (!hd) {
                xsink->raiseException("HASHDECL-ERROR", "cannot resolve hashdecl '%s' for instantiation",
                    dynamic_hashdecl_name.c_str());
                return QoreValue();
            }
            return typed_hash_decl_private::get(*hd)->newHash(parse_args, runtime_check, xsink);
        }

        default:
            assert(false);
            return QoreValue();
    }
}

QoreValue VarRefNewObjectNode::evalCallArg(ExceptionSink* xsink) const {
    bool needs_deref = true;
    QoreValue v = evalImpl(needs_deref, xsink);
    if (*xsink) {
        if (needs_deref) {
            v.discard(xsink);
        }
        return QoreValue();
    }
    return needs_deref ? v : v.refSelf();
}

QoreValue LexicalVarRefNewObjectNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return evalImplInternal<true>(rc_get_current_ref(), needs_deref, xsink);
}

QoreValue VarRefNewObjectNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue VarRefNewObjectNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    return evalImplInternal<false>(rc, needs_deref, xsink);
}

template <bool Lexical>
QORE_INTERN_ALWAYS_INLINE QoreValue VarRefNewObjectNode::evalImplInternal(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    ReferenceHolder<> value(xsink);

    const QoreTypeInfo* runtime_type_info = qore_substitute_type_params_if_needed(typeInfo);
    switch (vrn_type) {
        case VRN_OBJECT: {
            assert(QoreTypeInfo::getUniqueReturnClass(runtime_type_info));
            // the location of this call is the location of the call stack frame of the constructor
            QoreRcCallSiteHelper call_site(rc, loc);
            value = qore_class_private::execConstructorWithPolicy<Lexical>(getSemanticPolicy(),
                *QoreTypeInfo::getUniqueReturnClass(runtime_type_info), rc, variant, args, xsink, runtime_type_info
            );
            break;
        }

        case VRN_HASHDECL:
            if constexpr (Lexical) {
                value = typed_hash_decl_private::get(*QoreTypeInfo::getUniqueReturnHashDecl(runtime_type_info))
                    ->newHash(parse_args, runtime_check, xsink, getSemanticPolicy());
            } else {
                value = typed_hash_decl_private::get(*QoreTypeInfo::getUniqueReturnHashDecl(runtime_type_info))
                    ->newHash(parse_args, runtime_check, xsink);
            }
            break;

        case VRN_COMPLEXHASH:
            if constexpr (Lexical) {
                value = qore_hash_private::newComplexHash(runtime_type_info, parse_args, xsink, getSemanticPolicy());
            } else {
                value = qore_hash_private::newComplexHash(runtime_type_info, parse_args, xsink);
            }
            break;

        case VRN_COMPLEXLIST:
            if constexpr (Lexical) {
                value = qore_list_private::newComplexList(runtime_type_info, new_args, xsink, getSemanticPolicy());
            } else {
                value = qore_list_private::newComplexList(runtime_type_info, new_args, xsink);
            }
            break;

        case VRN_DYNAMIC_OBJECT: {
            const QoreClass* qc = qore_aot_resolve_class_ref(getProgram(), dynamic_class_name.c_str(), false);
            if (!qc) {
                raiseUnresolvedDynamicClass(dynamic_class_name, xsink);
                return QoreValue();
            }
            if (qore_class_private::runtimeCheckInstantiateClassByName(*qc, getProgram(), xsink)) {
                return QoreValue();
            }
            QoreRcCallSiteHelper call_site(rc, loc);
            value = qore_class_private::execConstructorWithPolicy<Lexical>(getSemanticPolicy(), *qc, rc, nullptr, args, xsink);
            break;
        }

        case VRN_DYNAMIC_HASHDECL: {
            QoreProgram* pgm = getProgram();
            if (!pgm) {
                xsink->raiseException("HASHDECL-ERROR",
                    "cannot resolve hashdecl '%s' for instantiation: no program context",
                    dynamic_hashdecl_name.c_str());
                break;
            }
            qore_program_private* pp = qore_program_private::get(*pgm);
            const qore_ns_private* found_ns = nullptr;
            const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(*pp->RootNS,
                dynamic_hashdecl_name.c_str(), found_ns);
            if (!hd) {
                xsink->raiseException("HASHDECL-ERROR", "cannot resolve hashdecl '%s' for instantiation",
                    dynamic_hashdecl_name.c_str());
                break;
            }
            if constexpr (Lexical) {
                value = typed_hash_decl_private::get(*hd)->newHash(parse_args, runtime_check, xsink, getSemanticPolicy());
            } else {
                value = typed_hash_decl_private::get(*hd)->newHash(parse_args, runtime_check, xsink);
            }
            break;
        }

        default:
            assert(false);
            break;
    }

    if (*xsink)
        return QoreValue();

    LValueHelper lv(QoreValue(const_cast<VarRefNewObjectNode*>(this)), rc, xsink);
    if (!lv)
        return QoreValue();
    AbstractQoreNode* rv;
    if constexpr (Lexical) {
        lv.assign(rv = value.release(), "<lvalue>", getSemanticPolicy());
    } else {
        lv.assign(rv = value.release());
    }
    if (*xsink)
        return QoreValue();
    needs_deref = false;
    return QoreValue(rv);
}

int qore_parse_check_unassigned_read(QoreProgram* pgm, const QoreProgramLocation* loc, const QoreValue& expr,
        const QoreTypeInfo* target_type, const char* target_desc) {
    if (!expr.hasNode() || expr.getType() != NT_VARREF) {
        return 0;
    }
    const VarRefNode* vrn = expr.get<const VarRefNode>();
    // once the parse has raised an exception, the state of a variable can be a consequence of it: a declaration or an
    // assignment whose type or value could not be parsed does not mark the variable assigned, so the error would not
    // be proven (the code does not parse in any case)
    if (vrn->getReadState() != qore_local_read_state_e::Unassigned
            || !QoreTypeInfo::hasType(target_type)
            || QoreTypeInfo::parseAcceptsReturns(target_type, NT_NOTHING)
            || !parse_check_parse_option(QoreParseOptions::STRICT_PARSE_ERRORS)
            || pgm->parseExceptionRaised()) {
        return 0;
    }
    QoreStringMaker hint("assign '%s' before this use, or give %s an optional type if the absent value is "
        "intentional", vrn->getName(), target_desc);
    QoreDiagnosticMetadata metadata("UNINITIALIZED-LOCAL-READ", hint.c_str());
    metadata.addFact("variable", vrn->getName());
    metadata.addFact("targetType", QoreTypeInfo::getName(target_type));
    metadata.addFact("assignmentState", "unassigned");
    QoreStringNode* desc = new QoreStringNodeMaker("local variable '%s' cannot have been assigned where it is read, "
        "so it gives NOTHING to %s, whose type '%s' does not accept NOTHING (%%strict-parse-errors)", vrn->getName(),
        target_desc, QoreTypeInfo::getName(target_type));
    qore_program_private::makeParseException(pgm, *loc, "PARSE-TYPE-ERROR", desc, metadata);
    return -1;
}
