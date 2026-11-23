/* -*- indent-tabs-mode: nil -*- */
/*
    CallReferenceNode.cpp

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

#include "qore/Qore.h"
#include "qore/intern/QoreRuntimeDiagnostic.h"
#include "qore/intern/QoreListNodeEvalOptionalRefHolder.h"
#include "qore/intern/QoreStatementTemps.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreAOT.h"
#include "qore/intern/qore_aot_deps.h"
#include "qore/intern/QoreSandboxProvenance.h"
#include "qore/intern/QorePseudoMethods.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/WeakHashReferenceNode.h"
#include "qore/intern/QoreMiscMembersOps.h"

#include <shared_mutex>
#include <optional>
#include <unordered_map>

namespace {
struct CallReferenceProvenanceRegistry {
    std::shared_mutex mutex;
    std::unordered_map<const ResolvedCallReferenceNode*, QoreSandboxContext*> contexts;
};

// Process lifetime: call references can be released by other static destructors. Entries own only contexts,
// never call references, and are removed when their call reference loses its last strong reference.
CallReferenceProvenanceRegistry& call_reference_provenance_registry() {
    static auto* registry = new CallReferenceProvenanceRegistry;
    return *registry;
}

std::atomic<size_t> call_reference_provenance_count{0};
}

void qore_capture_call_reference_provenance(const ResolvedCallReferenceNode* ref) {
    std::unique_ptr<QoreSandboxContext> context(qore_capture_sandbox_provenance());
    if (!context) {
        return;
    }
    auto& registry = call_reference_provenance_registry();
    std::unique_lock<std::shared_mutex> lock(registry.mutex);
    registry.contexts.emplace(ref, context.get());
    context.release();
    call_reference_provenance_count.fetch_add(1, std::memory_order_relaxed);
}

void qore_capture_call_reference_origin(const ResolvedCallReferenceNode* ref, const QoreProgram* code_pgm) {
    // only a reference whose code runs where the creating code is not on the stack records anything, and then only
    // for restricted creating code
    QoreParseOptions origin = qore_creation_origin_for(code_pgm);
    if (!origin) {
        return;
    }
    auto& registry = call_reference_provenance_registry();
    std::unique_lock<std::shared_mutex> lock(registry.mutex);
    auto i = registry.contexts.find(ref);
    if (i != registry.contexts.end()) {
        // the sandbox provenance that the reference's construction captured; not yet applied by any call
        i->second->origin_restrictions |= origin;
        return;
    }
    std::unique_ptr<QoreSandboxContext> context(new QoreSandboxContext(false));
    context->origin_restrictions = origin;
    registry.contexts.emplace(ref, context.get());
    context.release();
    call_reference_provenance_count.fetch_add(1, std::memory_order_relaxed);
}

void qore_release_call_reference_provenance(const ResolvedCallReferenceNode* ref) {
    if (!call_reference_provenance_count.load(std::memory_order_relaxed)) {
        return;
    }
    QoreSandboxContext* context = nullptr;
    auto& registry = call_reference_provenance_registry();
    {
        std::unique_lock<std::shared_mutex> lock(registry.mutex);
        auto i = registry.contexts.find(ref);
        if (i != registry.contexts.end()) {
            context = i->second;
            registry.contexts.erase(i);
            call_reference_provenance_count.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    if (context) {
        context->deref();
    }
}

//! returns the restricting options of a Program that is not a module's, or none for a module's Program or nullptr
/** The code of a Program that is not a module's runs with that Program on the stack, which decides for it and records
    nothing; the Program that creates a value runs the creating code
*/
static QoreParseOptions qore_non_module_program_restrictions(const QoreProgram* pgm) {
    return pgm && !qore_program_is_module_inline(pgm) ? qore_program_private::getParseOptions(*pgm)
        : QoreParseOptions();
}

static const QoreMethod* qore_call_reference_code_method(const ResolvedCallReferenceNode* rcr);

// returns the restrictions of a callable or object: with \a recorded_origin, also those that module or builtin code
// recorded when it created the value for the code that called it (qore_callable_origin_restrictions_po()), otherwise
// only those of the code that authored it (qore_callable_authored_restrictions_po(), qore#7056)
static QoreParseOptions qore_callable_restrictions_po(const QoreValue v, bool recorded_origin) {
    // only the restricting options are domains: not the positive options (PO_ALLOW_*) or PO_IN_MODULE
    static const QoreParseOptions restricting = QoreParseOptions(QDOM_ALL) & ~QoreParseOptions::POSITIVE_OPTIONS
        & ~QoreParseOptions(PO_IN_MODULE);
    QoreParseOptions rv;
    switch (v.getType()) {
        case NT_FUNCREF:
        case NT_RUNTIME_CLOSURE: {
            const ResolvedCallReferenceNode* rcr = v.get<const ResolvedCallReferenceNode>();
            // the restrictions recorded when it was created, for module or builtin code
            // (qore_capture_call_reference_origin())
            if (recorded_origin) {
                QoreSandboxContext* context = qore_get_call_reference_provenance(rcr);
                if (context) {
                    rv = context->origin_restrictions;
                    context->deref();
                }
            }
            // the Program whose code it runs, and the Program that created it, which the reference keeps alive: the
            // code of a restricted Program, or a reference that it created to the code of another Program (a method of
            // an object of a class that the host imported into it, for example)
            // the Program whose code it runs: for a method, the source Program of its class, referenced, as it can be
            // deleted while the reference exists; otherwise a Program that the reference keeps alive
            const QoreMethod* m = qore_call_reference_code_method(rcr);
            if (m) {
                const QoreClass* cls = m->getClass();
                QoreProgram* code_pgm = qore_class_private::get(*cls)->getReferencedSourceProgram();
                if (code_pgm) {
                    rv |= qore_non_module_program_restrictions(code_pgm);
                    code_pgm->deref();
                } else {
                    rv |= qore_non_module_program_restrictions(cls->getProgram());
                }
            } else {
                rv |= qore_non_module_program_restrictions(qore_call_reference_code_program(rcr));
            }
            // the Program of the reference is not its author: for a method reference it is the Program of the object,
            // for a closure the Program that was current when it was created (that of a restricted subclass for code of
            // a module that runs a method of its object); the author is the code that the reference runs (above: the
            // method that a call dispatches to on the object's class, the Program that defines a closure or a
            // function) and the code that created it (below) (qore#7056, qore#7087)
            if (recorded_origin) {
                rv |= qore_non_module_program_restrictions(rcr->getProgram());
            }
            // the code that created a run-time reference, if it is the code of a Program that is not a module's: a
            // reference that restricted code created to a method of an object of another Program (one lent to it)
            const ResolvedCallReferenceNodeIntern* intern = dynamic_cast<const ResolvedCallReferenceNodeIntern*>(rcr);
            const QoreParseOptions* creator_po = intern ? intern->getNonModuleCreatorParseOptions() : nullptr;
            if (creator_po) {
                rv |= *creator_po;
            }
            break;
        }
        case NT_OBJECT: {
            const QoreObject* obj = v.get<const QoreObject>();
            const qore_object_private* priv = qore_object_private::get(*obj);
            // set when the object is constructed and not changed after
            if (recorded_origin && priv->sandbox_provenance) {
                rv = priv->sandbox_provenance->origin_restrictions;
            }
            // the Program that created it, which the object keeps alive: also for an object of a builtin class or of
            // a class that the host imported into it, which records nothing; an object is authored by the code of its
            // class, whichever code creates it: the Program that is current when it is created can be that of a
            // restricted caller for which module code creates it (in a method of an object of a restricted subclass,
            // for example), and an object of a module or builtin class runs only that code (qore#7056)
            if (recorded_origin) {
                rv |= qore_non_module_program_restrictions(obj->getProgram());
            }
            // the source Program of its class, referenced, as it can be deleted while the object exists
            const qore_class_private* qc = qore_class_private::get(*obj->getClass());
            QoreProgram* code_pgm = qc->getReferencedSourceProgram();
            if (code_pgm) {
                rv |= qore_non_module_program_restrictions(code_pgm);
                code_pgm->deref();
            }
            break;
        }
        default:
            return QoreParseOptions();
    }
    return rv & restricting;
}

QoreParseOptions qore_callable_origin_restrictions_po(const QoreValue v) {
    return qore_callable_restrictions_po(v, true);
}

QoreParseOptions qore_callable_authored_restrictions_po(const QoreValue v) {
    return qore_callable_restrictions_po(v, false);
}

int64 qore_callable_origin_restrictions(const QoreValue v) {
    return qore_callable_origin_restrictions_po(v).getLo();
}

QoreSandboxContext* qore_get_call_reference_provenance(const ResolvedCallReferenceNode* ref) {
    if (!call_reference_provenance_count.load(std::memory_order_relaxed)) {
        return nullptr;
    }
    auto& registry = call_reference_provenance_registry();
    std::shared_lock<std::shared_mutex> lock(registry.mutex);
    auto i = registry.contexts.find(ref);
    if (i == registry.contexts.end()) {
        return nullptr;
    }
    i->second->ref();
    return i->second;
}

#include <cstdlib>
#include <cstring>

OptionalCallReferenceAccessHelper::OptionalCallReferenceAccessHelper(ExceptionSink* xsink,
        ResolvedCallReferenceNode* ref) : xsink(xsink), ref(ref) {
    if (!ref->optRef()) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "Cannot access a call reference that has already been deleted");
        this->ref = nullptr;
    }
}

CallReferenceCallNode::CallReferenceCallNode(const QoreProgramLocation* loc, QoreValue n_exp,
        QoreParseListNode* n_args) : ParseNode(loc, NT_FUNCREFCALL), exp(n_exp), parse_args(n_args) {
}

CallReferenceCallNode::CallReferenceCallNode(const QoreProgramLocation* loc, QoreValue n_exp, QoreListNode* n_args)
        : ParseNode(loc, NT_FUNCREFCALL), exp(n_exp), args(n_args) {
}

CallReferenceCallNode::~CallReferenceCallNode() {
    //printd(5, "CallReferenceCallNode::~CallReferenceCallNode() this: %p exp: %p '%s' type: %d refs: %d\n", this,
    //  exp, get_type_name(exp), get_node_type(exp), exp->reference_count());
    // only a node deleted without being released holds values here; see derefImpl()
    exp.discard(nullptr);
    if (parse_args) {
        parse_args->deref(nullptr);
    }
    if (args) {
        args->deref(nullptr);
    }
}

bool CallReferenceCallNode::derefImpl(ExceptionSink* xsink) {
    // a background thread releases the copy of its call when the call returns: the last reference to an object that
    // the closure captured or that an argument holds runs the object's destructor here, and an exception that it
    // raises is the thread's; with no sink it aborted a debug build
    exp.discard(xsink);
    exp = QoreValue();
    if (parse_args) {
        parse_args->deref(xsink);
        parse_args = nullptr;
    }
    if (args) {
        args->deref(xsink);
        args = nullptr;
    }
    return true;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int CallReferenceCallNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("call reference call (%p)", this);
    return 0;
}

// if del is true, then the returned QoreString*  should be deleted, if false, then it must not be
QoreString* CallReferenceCallNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// returns the type name as a c string
const char* CallReferenceCallNode::getTypeName() const {
    return "call reference call";
}

void CallReferenceCallNode::resolveParseArgs() {
    if (!parse_args || args) {
        return;
    }
    bool has_eval_entries = false;
    for (size_t i = 0; i < parse_args->size(); ++i) {
        if (parse_args->get(i).needsEval()) {
            has_eval_entries = true;
            break;
        }
    }
    args = has_eval_entries
        ? qore_list_private::newList(true)
        : new QoreListNode(autoTypeInfo);
    for (size_t i = 0; i < parse_args->size(); ++i) {
        QoreValue v = parse_args->get(i);
        v.refSelf();
        args->push(v, nullptr);
    }
}

// evalImpl(): return value requires a deref(xsink) if not 0
QoreValue CallReferenceCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

template <bool LexicalInvocation>
QORE_INTERN_ALWAYS_INLINE QoreValue CallReferenceCallNode::evalImplInternal(RuntimeConfig& rc, bool& needs_deref,
        ExceptionSink* xsink) const {
    assert(needs_deref);
    assert(!parse_args || args
        || !"CallReferenceCallNode::evalImpl(): parse_args set but args is null; "
           "call resolveParseArgs() after AOT deserialization");
    ValueEvalRefHolder lv(rc, exp, xsink);
    if (*xsink) {
        return QoreValue();
    }
    // a call reference or closure read from a variable is borrowed; the arguments are evaluated after it, and an
    // argument can assign the variable another value (c(c = NOTHING), c(c = other)), which releases the only reference
    // to the closure before it runs, so the call holds its own reference to a borrowed callable while its arguments
    // need evaluation: the call runs the closure that the expression gave, as in every other execution mode
    ValueHolder borrowed_callable(xsink);
    if (!lv.isTemp() && lv->hasNode() && args && args->needs_eval()) {
        borrowed_callable = lv->refSelf();
    }
    // a temporary call reference or closure that is called lives until the end of the statement, as a temporary
    // receiver does
    QoreStatementTempScope::holdReceiver(exp, lv, xsink);

    qore_type_t type = lv->getType();
    if (type != NT_FUNCREF && type != NT_RUNTIME_CLOSURE) {
        qore_raise_not_call_reference(xsink, loc, *lv);
        return QoreValue();
    }
    // An evaluated call reference is resolved, as in qore_check_call_target(); parse nodes are never values.
    assert(dynamic_cast<ResolvedCallReferenceNode*>(lv->getInternalNode()));
    auto* r = static_cast<ResolvedCallReferenceNode*>(lv->getInternalNode());
    // the location of this call is the location of the call stack frame of the called code
    QoreRcCallSiteHelper call_site(rc, loc);
    if (runtime_arg_names) {
        LexicalSemanticPolicy policy;
        if constexpr (LexicalInvocation) {
            policy = getSemanticPolicy();
        }
        return qore_call_ref_invoke_named(r, args, *runtime_arg_names, policy, xsink);
    }
    if constexpr (LexicalInvocation) {
        return qore_resolved_call_reference_node_ops(r).invokeLexical(args, xsink, getSemanticPolicy());
    } else {
        return r->invoke(args, xsink);
    }
}

QoreValue CallReferenceCallNode::evalRuntimeNamedWithArgs(QoreListNode* args, ExceptionSink* xsink) const {
    assert(runtime_arg_names);
    ReferenceHolder<QoreListNode> args_holder(args, xsink);
    ValueHolder callee(args ? args->shift() : QoreValue(), xsink);
    qore_type_t type = callee->getType();
    if (type != NT_FUNCREF && type != NT_RUNTIME_CLOSURE) {
        qore_raise_not_call_reference(xsink, loc, *callee);
        return QoreValue();
    }
    // An evaluated call reference is resolved, as in qore_check_call_target(); parse nodes are never values.
    assert(dynamic_cast<const ResolvedCallReferenceNode*>(callee->getInternalNode()));
    auto* r = static_cast<const ResolvedCallReferenceNode*>(callee->getInternalNode());
    RuntimeConfig& rc = rc_get_current_ref();
    // the location of this call is the location of the call stack frame of the called code
    QoreRcCallSiteHelper call_site(rc, loc);
    return qore_call_ref_invoke_named(r, *args_holder, *runtime_arg_names, {}, xsink);
}

QoreValue CallReferenceCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    return evalImplInternal<false>(rc, needs_deref, xsink);
}

QoreValue CallReferenceCallNode::evalLexical(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    return evalImplInternal<true>(rc, needs_deref, xsink);
}

QoreValue CallReferenceCallNode::evalLexical(bool& needs_deref, ExceptionSink* xsink) const {
    return evalImplInternal<true>(rc_get_current_ref(), needs_deref, xsink);
}

int CallReferenceCallNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    parse_context.typeInfo = nullptr;
    parse_context.analysis.clear();
    QoreParseAnalysis arg_analysis;
    int err = parse_init_value(exp, parse_context);
    const QoreTypeInfo* expTypeInfo = parse_context.typeInfo;
    parse_context.analysis.clear();

    if (!err && expTypeInfo && codeTypeInfo && QoreTypeInfo::hasType(expTypeInfo)
        && !QoreTypeInfo::parseAccepts(codeTypeInfo, expTypeInfo)) {
        // raise parse exception
        QoreStringNode* desc = new QoreStringNode("invalid call; expression gives ");
        QoreTypeInfo::getThisType(expTypeInfo, *desc);
        desc->concat(", but a call reference or closure is required to make a call");
        qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
        err = -1;
    }

    if (parse_args) {
        // a named call binds its arguments by parameter name when it is made: the code that the expression gives is
        // only known then (M4 phase 3)
        if (parse_args->hasNamedArgs()) {
            const QoreParseListNode::arg_name_vec_t& names = parse_args->getArgNamesVector();
            runtime_arg_names = qore_intern_call_arg_names(name_vec_t(names.begin(), names.end()));
        }
        type_vec_t argTypeInfo;
        {
            QoreParseContextAnalysisHelper ah(parse_context);
            if (parse_args->initArgs(parse_context, argTypeInfo, args) && !err) {
                err = -1;
            }
            arg_analysis = parse_context.analysis;
        }
        parse_args = nullptr;
    }

    // For typed code references (code<RetType(ArgTypes...)>), extract and
    // propagate the return type. This enables map/select and other functional
    // operators to preserve element type information through closure calls.
    {
        const QoreComplexCodeTypeInfo* cct = QoreTypeInfo::getComplexCodeType(expTypeInfo);
        parse_context.typeInfo = cct ? cct->getReturnType() : nullptr;
    }
    parse_context.analysis.clear();
    if (arg_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}

// refuses a call of a reference that code that may not control threads created, to code that does not check it when
// it is entered (the code of another Program, builtin code), if the expression of the background operator makes it
// itself (qore#7087)
static int check_background_creator(const ResolvedCallReferenceNodeIntern* r, ExceptionSink* xsink) {
    const QoreParseOptions* po = r->getNonModuleCreatorParseOptions();
    return po && (*po & QoreParseOptions(QDOM_THREAD_CONTROL)) ? qore_check_background_direct_call(xsink) : 0;
}

void ResolvedCallReferenceNodeIntern::captureCreatorParseOptions() {
    creator_po = runtime_get_effective_program_parse_options();
    // a reference that module code creates while it runs with the restrictions of a sandboxed caller runs with the
    // permissions of that code, which uses the domains that its module declares (see
    // qore_relieve_module_declared_domains()); only module code runs with the module's override, so a reference that
    // the caller's code creates to the module's code is not relieved (the caller's code clears the module's
    // override); the Program running the code is tested, not the Program of the reference, which is the object's for
    // a method reference
    const QoreProgram* pgm = ::getProgram();
    if (pgm && runtime_get_module_call_program() == pgm) {
        creator_po = qore_relieve_module_declared_domains(creator_po, pgm);
    }
    has_creator_po = true;
    creator_non_module = pgm && !qore_program_is_module_inline(pgm);
    // the run-time call reference classes call this from their constructors, when the reference is complete: a
    // reference to library module code records the restrictions of the code that creates it (qore#6520)
    qore_capture_call_reference_origin(this, getOriginCodeProgram());
}

//! returns the Program whose code a user function runs, or nullptr for a builtin function
static const QoreProgram* qore_function_code_program(const QoreFunction* f) {
    const AbstractQoreFunctionVariant* v = f ? f->first() : nullptr;
    const UserVariantBase* uvb = v ? v->getUserVariantBase() : nullptr;
    return uvb ? uvb->pgm : nullptr;
}

//! returns the Program whose code a method runs: the Program that defines its class, nullptr for a builtin class
static const QoreProgram* qore_method_code_program(const QoreMethod* m) {
    const QoreClass* cls = m ? m->getClass() : nullptr;
    if (!cls) {
        return nullptr;
    }
    const QoreProgram* pgm = cls->getSourceProgram();
    return pgm ? pgm : cls->getProgram();
}

const QoreProgram* ResolvedCallReferenceNodeIntern::getOriginCodeProgram() const {
    return qore_call_reference_code_program(this);
}

const QoreProgram* LocalStaticMethodCallReferenceNode::getOriginCodeProgram() const {
    return qore_method_code_program(method);
}

const QoreProgram* LocalFunctionCallReferenceNode::getOriginCodeProgram() const {
    return qore_function_code_program(uf);
}

const QoreProgram* RunTimeResolvedMethodReferenceNode::getOriginCodeProgram() const {
    // the method that a call dispatches to on the object's class (see qore_call_reference_code_program())
    return qore_call_reference_code_program(this);
}

const QoreProgram* qore_closure_code_program(const ResolvedCallReferenceNode* closure) {
    // a closure's code is that of the Program that defines it (see qore_call_reference_code_program())
    const QoreProgram* pgm = qore_function_code_program(closure->getFunction());
    return pgm ? pgm : closure->getProgram();
}

AbstractCallReferenceNode::AbstractCallReferenceNode(bool n_needs_eval,
        qore_type_t n_type) : AbstractQoreNode(n_type, false, n_needs_eval) {
}

AbstractCallReferenceNode::AbstractCallReferenceNode(bool n_needs_eval, bool n_there_can_be_only_one,
        qore_type_t n_type) : AbstractQoreNode(n_type, false, n_needs_eval, n_there_can_be_only_one) {
}

AbstractCallReferenceNode::~AbstractCallReferenceNode() {
}

const ResolvedCallReferenceNode* AbstractCallReferenceNode::getSandboxBoundCallback() const {
    return nullptr;
}

// parse types should never be copied
AbstractQoreNode* AbstractCallReferenceNode::realCopy() const {
    assert(false);
    return nullptr;
}

bool AbstractCallReferenceNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    return is_equal_hard(v, xsink);
}

bool AbstractCallReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    assert(false);
    return false;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int AbstractCallReferenceNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("function reference (%p)", this);
    return 0;
}

// if del is true, then the returned QoreString*  should be deleted, if false, then it must not be
QoreString* AbstractCallReferenceNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

QoreValue AbstractCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue AbstractCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    assert(false);
    return QoreValue();
}

// returns the type name as a c string
const char* AbstractCallReferenceNode::getTypeName() const {
    return getStaticTypeName();
}

bool AbstractCallReferenceNode::getAsBoolImpl() const {
    // check if we should do perl-style boolean evaluation
    if (qore_get_native_semantic_policy().strictBoolean())
        return false;
    return true;
}

bool ResolvedCallReferenceNode::derefImpl(ExceptionSink* xsink) {
    qore_release_call_reference_provenance(this);
    weakDeref();
    return false;
}

ParseObjectMethodReferenceNode::ParseObjectMethodReferenceNode(const QoreProgramLocation* loc, QoreValue n_exp,
        char* n_method) : AbstractParseObjectMethodReferenceNode(loc), exp(n_exp), method(n_method), qc(0), m(0) {
    free(n_method);
}

ParseObjectMethodReferenceNode::~ParseObjectMethodReferenceNode() {
    exp.discard(nullptr);
}

// returns a RunTimeObjectMethodReference or NULL if there's an exception
QoreValue ParseObjectMethodReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ParseObjectMethodReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    // evaluate lvalue expression
    ValueEvalRefHolder lv(rc, exp, xsink);
    if (*xsink) {
        return QoreValue();
    }
    // the reference holds its object weakly; a temporary object lives until the end of the statement, so the
    // statement can call the reference
    QoreStatementTempScope::holdReceiver(exp, lv, xsink);

    return makeReference(*lv, xsink);
}

QoreValue ParseObjectMethodReferenceNode::makeReference(QoreValue val, ExceptionSink* xsink) const {
    QoreObject* o = val.getType() == NT_OBJECT ? val.get<QoreObject>() : nullptr;
    if (!o) {
        // issue #995: a reference to a pseudo-method of the value, as a call of the method is a call of it
        return qore_make_pseudo_method_reference(loc, val, method.c_str(), xsink);
    }

    const QoreClass* oc = o->getClass();

    // the method of the declared class, resolved when parsing, for an object of that class, as for a call of the method
    // (AbstractMethodCallNode::exec()), also when it is inherited past a method of the name that is not accessible
    // here (qore#6680); a static method of it is bound to that class, as a call through the object is (qore#5990).
    // The class of the object can be another QoreClass of the declared class, with the same class ID: the copy of a
    // module's class imported into a Program, to which a compiled image resolves the declared class, while the object
    // has the module's own class; native code calls the method resolved for the class in this case as well
    // (qore_rt_same_class())
    if (m && (oc == qc || oc == m->getClass() || m->isStatic() || oc->getID() == qc->getID())) {
        return new RunTimeResolvedMethodReferenceNode(loc, o, m);
    }

    // otherwise the method is resolved for the class of each object, also an object of a subclass of the declared
    // class, which can override the method with one that is not accessible here (qore#6062)
    uint64_t class_id = qore_class_private::getAllocationId(*oc);
    if (rt_class_id.load(std::memory_order_acquire) == class_id) {
        if (rt_resolved) {
            return new RunTimeResolvedMethodReferenceNode(loc, o, rt_m);
        }
        return new RunTimeObjectMethodReferenceNode(loc, o, method.c_str());
    }

    return makeReferenceForClass(o, oc, class_id, xsink);
}

QoreValue ParseObjectMethodReferenceNode::makeReferenceForClass(QoreObject* o, const QoreClass* oc,
        uint64_t class_id, ExceptionSink* xsink) const {
    ClassAccess access = Public;
    const QoreMethod* om = oc->findMethod(method.c_str(), access);
    if (!om) {
        om = oc->findStaticMethod(method.c_str(), access);
        if (!om) {
            // issue #995: a pseudo-method of objects, such as className(), or a member with code: the call looks the
            // method up by name as a direct call does (a code member, then a pseudo-method)
            if (qore_object_has_pseudo_or_member_method(qore_class_private::get(*oc), method.c_str(),
                    runtime_get_class())) {
                return new RunTimeObjectMethodReferenceNode(loc, o, method.c_str(), true);
            }
            xsink->raiseException(*loc, "OBJECT-METHOD-REFERENCE-ERROR", QoreValue(),
                "cannot resolve reference to %s::%s(): unknown method", o->getClassName(), method.c_str());
            return QoreValue();
        }
    }
    // the constructor, destructor and copy method of the object's class or of a base class (qore#6783)
    if (qore_class_private::checkReferenceable(*om, xsink)) {
        return QoreValue();
    }

    if (access > Public) {
        // the access check depends on the class context of the caller, so the result is not cached
        if (!qore_class_private::runtimeCheckPrivateClassAccess(*oc)) {
            // the method that a call of the method makes from here, also when the method found first is not
            // accessible here (qore#6704); not cached, as it depends on the class context of the caller
            if (const QoreMethod* pm = qore_class_private::get(*oc)->runtimeFindPublicCommittedMethod(
                    method.c_str())) {
                return new RunTimeResolvedMethodReferenceNode(loc, o, pm);
            }
            if (om->isPrivate()) {
                xsink->raiseException(*loc, "ILLEGAL-CALL-REFERENCE", QoreValue(),
                    "cannot create a call reference to %s %s::%s() from outside the class",
                    privpub(access), o->getClassName(), method.c_str());
            } else {
                xsink->raiseException(*loc, "ILLEGAL-CALL-REFERENCE", QoreValue(),
                    "cannot create a call reference to %s::%s() because the parent class that implements the method "
                    "(%s::%s()) is privately inherited", o->getClassName(), method.c_str(),
                    om->getClass()->getName(), method.c_str());
            }
            return QoreValue();
        }
    }

    bool resolved = oc == om->getClass();
    if (access == Public && !rt_class_id.load(std::memory_order_acquire)) {
        AutoLocker al(lck);
        if (!rt_class_id.load(std::memory_order_relaxed)) {
            rt_m = om;
            rt_resolved = resolved;
            rt_class_id.store(class_id, std::memory_order_release);
        }
    }

    if (resolved) {
        return new RunTimeResolvedMethodReferenceNode(loc, o, om);
    }
    return new RunTimeObjectMethodReferenceNode(loc, o, method.c_str());
}

//! checks the functional domains of a method that a call reference is taken to against the parsing Program
/** as for a call, the domains of all variants are checked if every variant has them; otherwise the variant that a
    call through the reference resolves is checked when it is called, against the context that created the reference
*/
static int check_call_reference_domain(const QoreProgramLocation* loc, QoreParseContext& parse_context,
        const QoreMethod* m) {
    const QoreFunction* f = qore_method_private::get(*m)->getFunction();
    int64 dom = f->parseGetUniqueFunctionality();
    if (!dom) {
        return 0;
    }
    QoreParseOptions denied = qore_program_private::parseDeniedDomains(parse_context.pgm, dom,
        f->parseGetUniqueDeclaredDomain());
    if (!denied) {
        return 0;
    }
    parse_error(*loc, "parse options do not allow a call reference to method '%s::%s()'; functional domain: %s",
        m->getClass()->getName(), m->getName(), qore_describe_source_domains(denied.getLo()).c_str());
    return -1;
}

int ParseObjectMethodReferenceNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = nullptr;
    int err = parse_init_value(exp, parse_context);
    const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;

    if (QoreTypeInfo::hasType(argTypeInfo)) {
        if (objectTypeInfo && argTypeInfo && !QoreTypeInfo::parseAccepts(objectTypeInfo, argTypeInfo)) {
            // issue #995: a value that is not an object gives a reference to a pseudo-method of its type, as a call
            // of the method calls it; the type must have the pseudo-method, as for a call
            QoreClass* pqc;
            bool possible_match = false;
            // a hash can hold code with the name, which a direct call calls, as at parse time for a direct call
            if (!pseudo_classes_find_method(argTypeInfo, method.c_str(), pqc, possible_match) && !possible_match
                    && !QoreTypeInfo::parseAccepts(hashTypeInfo, argTypeInfo) && !err) {
                QoreStringNode* desc = new QoreStringNode("cannot take a call reference to pseudo-method '");
                desc->concat(method);
                desc->concat("()'; the expression gives ");
                QoreTypeInfo::getThisType(argTypeInfo, *desc);
                desc->concat(", which has no such method");
                qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
                err = -1;
            }
        } else {
            const QoreClass* n_qc = QoreTypeInfo::getUniqueReturnClass(argTypeInfo);
            if (n_qc) {
                qore_class_private* class_ctx = parse_context.oflag
                    ? qore_class_private::get(*const_cast<QoreClass*>(
                        QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()))
                    )
                    : parse_get_class_priv();
                if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*n_qc, class_ctx))
                    class_ctx = nullptr;

                // class access is checked internally
                m = qore_class_private::get(*const_cast<QoreClass*>(n_qc))->parseFindAnyMethod(method.c_str(),
                    class_ctx);
                if (m) {
                    if (!m->isStatic() && !strcmp(m->getName(), "copy")) {
                        parseException(*loc, "PARSE-ERROR", "cannot take a call reference to copy method %s::%s()",
                            n_qc->getName(), method.c_str());
                        if (!err) {
                            err = -1;
                        }
                    }
                    // check the functional domains of the method: a reference allows the call that naming the
                    // method would
                    if (check_call_reference_domain(loc, parse_context, m) && !err) {
                        err = -1;
                    }
                    // a method with abstract variants that its class does not implement is resolved for the class of
                    // the object when the reference is made, as a call of it is (QoreDotEvalOperatorNode): another
                    // parent class of the object's class implements them, and the abstract methods of the classes
                    // are resolved after this code is parsed, so that the method found here may not be the one that
                    // is called (qore#7206)
                    if (!m->isStatic() && (qore_method_private::get(*m)->isAbstract()
                            || qore_class_private::get(*m->getClass())->hasUnimplementedAbstractVariants(
                                method.c_str()))) {
                        m = nullptr;
                    } else {
                        qc = n_qc;
                    }
                } else {
                    // issue #995: a pseudo-method of objects, such as className(), or a member, which a direct call
                    // calls if it holds code; the reference is made for the object's class when the expression is
                    // evaluated
                    QoreClass* pqc;
                    const qore_class_private* mqc = nullptr;
                    ClassAccess maccess;
                    if (!pseudo_classes_find_method(NT_OBJECT, method.c_str(), pqc)
                            && !qore_class_private::get(*const_cast<QoreClass*>(n_qc))->parseFindMember(
                                method.c_str(), mqc, maccess)) {
                        parseException(*loc, "PARSE-ERROR", "no method %s::%s() is accessible in this context",
                            n_qc->getName(), method.c_str());
                        if (!err) {
                            err = -1;
                        }
                    }
                }
            }
        }
    }

    parse_context.typeInfo = callReferenceTypeInfo;
    return err;
}

// returns a RunTimeObjectMethodReferenceNode or NULL if there's an exception
QoreValue ParseSelfMethodReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ParseSelfMethodReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    QoreObject* o = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    if (!o) {
        xsink->raiseException("OBJECT-ERROR", "cannot evaluate method reference '\\%s()' when not in an object "
            "context; if using 'background', evaluate the method reference before the 'background' statement",
            meth->getName());
        return QoreValue();
    }

    // meth may be null when this node is deserialized in an AOT binary (parse context not available);
    // in that case fall back to runtime method lookup by name
    if (!meth) {
        // a method that the class does not have is a pseudo-method or a member (issue #995)
        return new RunTimeObjectMethodReferenceNode(loc, o, method.c_str(),
            !o->getClass()->findMethod(method.c_str()));
    }

    // return class with method already found at parse time if known
    if (o->getClass() == meth->getClass()) {
        return new RunTimeResolvedMethodReferenceNode(loc, o, meth,
            rc.getClass() ? rc.getClass() : runtime_get_class());
    }

    // a method that the class does not have is a pseudo-method (issue #995)
    return new RunTimeObjectMethodReferenceNode(loc, o, meth->getName(), !o->getClass()->findMethod(meth->getName()));
}

int ParseSelfMethodReferenceNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = callReferenceTypeInfo;
    if (!parse_context.oflag) {
        parse_error(*loc, "reference to object member '%s' when not in an object context", method.c_str());
        return -1;
    }

    assert(!method.empty());
    qore_class_private* class_ctx = qore_class_private::get(
        *const_cast<QoreClass*>(QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()))
    );
    meth = class_ctx->parseResolveSelfMethod(loc, method.c_str(), class_ctx);
    if (!meth) {
        return -1;
    }
    return check_call_reference_domain(loc, parse_context, meth);
}

ParseScopedSelfMethodReferenceNode::ParseScopedSelfMethodReferenceNode(const QoreProgramLocation* loc,
        NamedScope* n_nscope) : AbstractParseObjectMethodReferenceNode(loc), nscope(n_nscope), method(0) {
}

ParseScopedSelfMethodReferenceNode::~ParseScopedSelfMethodReferenceNode() {
    delete nscope;
}

// returns a RunTimeObjectMethodReference or nullptr if there's an exception
QoreValue ParseScopedSelfMethodReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ParseScopedSelfMethodReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    QoreObject* o = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    return new RunTimeResolvedMethodReferenceNode(loc, o, method,
        rc.getClass() ? rc.getClass() : runtime_get_class());
}

int ParseScopedSelfMethodReferenceNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = callReferenceTypeInfo;
    if (!parse_context.oflag) {
        parse_error(*loc, "reference to object member '%s' when not in an object context", method->getName());
        return -1;
    }

    qore_class_private* class_ctx = qore_class_private::get(
        *const_cast<QoreClass*>(QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()))
    );
    method = class_ctx->parseResolveSelfMethod(loc, nscope);
    delete nscope;
    nscope = nullptr;
    if (!method) {
        return -1;
    }
    return check_call_reference_domain(loc, parse_context, method);
}

// the Program of a method call reference is a tagged pointer, whose low bit is a flag
static_assert(alignof(QoreProgram) > 1, "a QoreProgram pointer must have its low bit free");

RunTimeResolvedMethodReferenceNode::RunTimeResolvedMethodReferenceNode(const QoreProgramLocation* loc,
        QoreObject* n_obj, const QoreMethod* n_method)
        : ResolvedCallReferenceNodeIntern(loc), obj(n_obj), method(n_method) {
    QoreProgram* creator;
    // the Program and the class context of the creating code with one thread-local lookup
    runtime_get_program_and_class(creator, qc);
    init(creator);
}

RunTimeResolvedMethodReferenceNode::RunTimeResolvedMethodReferenceNode(const QoreProgramLocation* loc,
        QoreObject* n_obj, const QoreMethod* n_method, const qore_class_private* ctx)
        : ResolvedCallReferenceNodeIntern(loc), obj(n_obj), method(n_method), qc(ctx) {
    init(::getProgram());
}

void RunTimeResolvedMethodReferenceNode::init(QoreProgram* creator) {
    // the restrictions that a call of the reference is checked against: those of the creating context (#5803)
    captureCreatorParseOptions();
    printd(5, "RunTimeResolvedMethodReferenceNode::RunTimeResolvedMethodReferenceNode() this: %p obj: %p\n", this, obj);
    // the class context of the creator selects the variants of a call wherever it is made: with none, only the
    // public variants, not those that the class context of the caller could select.  The class is referenced, as it
    // can belong to another Program than the object, which releases it when it is deleted, while the reference can
    // be called afterwards; as for a static method call reference (qore#6622), the class only, not its Program
    if (qc) {
        qc->ref();
    } else {
        qc = qore_no_class_ctx();
    }
    obj->tRef();
    QoreProgram* opgm = qore_object_private::get(*obj)->pgm;
    // created in the object's own Program, the usual case, or a user method: the reference runs there, as before,
    // and needs no reference of its own to the Program (the object holds one)
    if (creator == opgm || qore_method_private::get(*method)->isUniquelyUser()) {
        pgm_ref = reinterpret_cast<uintptr_t>(opgm);
        return;
    }
    // a builtin method of a reference created with no Program runs with none
    if (!creator) {
        pgm_ref = 0;
        return;
    }
    // a builtin method runs in the Program of the code that creates the reference, not in the object's: a builtin
    // object belongs to the Program it was created for, which need not be the Program of the code that uses it (a
    // reflection handle belongs to the Program it describes), and a call of the reference must not run in that
    // Program with its parse options and so as code of that Program; a weak reference, as a strong one could cause
    // a recursive reference
    creator->depRef();
    pgm_ref = reinterpret_cast<uintptr_t>(creator) | PGM_CREATOR;
}

RunTimeResolvedMethodReferenceNode::~RunTimeResolvedMethodReferenceNode() {
    printd(5, "RunTimeResolvedMethodReferenceNode::~RunTimeResolvedMethodReferenceNode() this: %p obj: %p\n", this, obj);
    // released here and not in derefImpl(), which keeps the dereference of the usual reference, which holds no
    // Program, free of an extra call; the weak reference only keeps the Program's memory
    if (pgm_ref & PGM_CREATOR) {
        reinterpret_cast<QoreProgram*>(pgm_ref & ~PGM_CREATOR)->depDeref();
    }
    if (qc != qore_no_class_ctx()) {
        const_cast<qore_class_private*>(qc)->deref(false, false, false);
    }
    obj->tDeref();
}

QoreValue RunTimeResolvedMethodReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    //printd(5, "RunTimeResolvedMethodReferenceNode::execValue() cpgm: %p opgm: %p\n", getProgram(), obj->getProgram());
    // issue #2145: do not set the call reference class context before arguments are evaluted
    // NOTE: check this for race conditions
    if (checkObject(xsink)) {
        return QoreValue();
    }
    // a user method runs in the Program that defines its class; the object's Program is used only when that
    // Program has released the class (it raises PROGRAM-ERROR once the object's Program has been deleted).  A
    // builtin method runs as a direct call in the Program that created the reference: called there, it is not given
    // a Program, so it runs as a direct call there does, with the runtime parse options in effect; called from
    // another Program, it runs in the creating Program with its parse options, as a static method call reference
    // does (StaticMethodCallReferenceNode)
    QoreProgram* pgm_ctx = reinterpret_cast<QoreProgram*>(pgm_ref);
    if (pgm_ref & PGM_CREATOR) {
        pgm_ctx = reinterpret_cast<QoreProgram*>(pgm_ref & ~PGM_CREATOR);
        if (pgm_ctx == rc_get_current_program(rc)) {
            pgm_ctx = nullptr;
        }
    }
    return qore_method_private::eval(*method, xsink, rc, obj, args, qc, pgm_ctx);
}

QoreValue RunTimeResolvedMethodReferenceNode::execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // as dispatchValue(): the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    if (checkObject(xsink)) {
        return QoreValue();
    }
    QoreProgram* pgm_ctx = reinterpret_cast<QoreProgram*>(pgm_ref);
    if (pgm_ref & PGM_CREATOR) {
        pgm_ctx = reinterpret_cast<QoreProgram*>(pgm_ref & ~PGM_CREATOR);
        if (pgm_ctx == rc_get_current_program(rc)) {
            pgm_ctx = nullptr;
        }
    }
    return qore_method_private::evalGivenArgs(*method, xsink, rc, obj, args, qc, pgm_ctx);
}

QoreValue RunTimeResolvedMethodReferenceNode::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    if (checkObject(xsink)) {
        return QoreValue();
    }
    // the Program of the call as for a call with positional arguments (see dispatchValue())
    QoreProgram* pgm_ctx = reinterpret_cast<QoreProgram*>(pgm_ref);
    if (pgm_ref & PGM_CREATOR) {
        pgm_ctx = reinterpret_cast<QoreProgram*>(pgm_ref & ~PGM_CREATOR);
        if (pgm_ctx == rc_get_current_program(rc)) {
            pgm_ctx = nullptr;
        }
    }
    return qore_method_private::evalNamed(*method, xsink, rc, obj, args, false, qc, nullptr, nullptr, arg_names,
        policy, pgm_ctx);
}

QoreValue RunTimeResolvedMethodReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // Compatibility semantics do not change the permissions of the reference's creator.
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    //printd(5, "RunTimeResolvedMethodReferenceNode::dispatchValue() cpgm: %p opgm: %p\n", getProgram(), obj->getProgram());
    // issue #2145: do not set the call reference class context before arguments are evaluted
    // NOTE: check this for race conditions
    if (checkObject(xsink)) {
        return QoreValue();
    }
    // a user method runs in the Program that defines its class; the object's Program is used only when that
    // Program has released the class (it raises PROGRAM-ERROR once the object's Program has been deleted).  A
    // builtin method runs as a direct call in the Program that created the reference: called there, it is not given
    // a Program, so it runs as a direct call there does, with the runtime parse options in effect; called from
    // another Program, it runs in the creating Program with its parse options, as a static method call reference
    // does (StaticMethodCallReferenceNode)
    QoreProgram* pgm_ctx = reinterpret_cast<QoreProgram*>(pgm_ref);
    if (pgm_ref & PGM_CREATOR) {
        pgm_ctx = reinterpret_cast<QoreProgram*>(pgm_ref & ~PGM_CREATOR);
        if (pgm_ctx == rc_get_current_program(rc)) {
            pgm_ctx = nullptr;
        }
    }
    return qore_method_private::eval(policy, *method, xsink, rc, obj, args, qc, pgm_ctx);
}

QoreProgram* RunTimeResolvedMethodReferenceNode::getProgram() const {
    return obj->getProgram();
}

bool RunTimeResolvedMethodReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const RunTimeResolvedMethodReferenceNode* vc = dynamic_cast<const RunTimeResolvedMethodReferenceNode*>(v);
    return vc && vc->obj == obj && vc->method == method;
}

QoreFunction* RunTimeResolvedMethodReferenceNode::getFunction() const {
    return method ? qore_method_private::get(*method)->getFunction() : 0;
}

bool qore_object_has_pseudo_or_member_method(const qore_class_private* cls, const char* method,
        const qore_class_private* class_ctx) {
    QoreClass* pqc;
    return pseudo_classes_find_method(NT_OBJECT, method, pqc) || cls->runtimeGetCallableMemberInfo(method, class_ctx);
}

// out of line: needs_scan() calls it only for call references, and inlined there it makes needs_scan() too large to be
// inlined in needs_scan(const QoreValue&), which the storing of every value calls (issue #995)
__attribute__((noinline)) const AbstractQoreNode* qore_get_call_reference_scan_node(const AbstractQoreNode* n) {
    if (const ResolvedCallReferenceNode* callback = qore_get_sandbox_bound_callback(n)) {
        return callback;
    }
    if (const RunTimePseudoMethodReferenceNode* pr = dynamic_cast<const RunTimePseudoMethodReferenceNode*>(n)) {
        const QoreValue& v = pr->getValue();
        return v.isOpaque() ? nullptr : v.getInternalNode();
    }
    return nullptr;
}

QoreValue qore_make_pseudo_method_reference(const QoreProgramLocation* loc, QoreValue val, const char* method,
        ExceptionSink* xsink) {
    assert(val.getType() != NT_OBJECT);
    // the exception of a call of a pseudo-method that the value's type does not have is raised when the reference is
    // made, as for a method that an object's class does not have; a hash can hold code with the name, which a direct
    // call calls
    bool hash_member = false;
    if (val.getType() == NT_HASH) {
        hash_member = check_call_ref(val.getInternalNode(), method) != nullptr;
    } else if (val.getType() == NT_WEAKREF_HASH) {
        ReferenceHolder<QoreHashNode> h(val.get<WeakHashReferenceNode>()->lock(), xsink);
        hash_member = check_call_ref(*h, method) != nullptr;
    }
    if (!hash_member && pseudo_classes_check_method(val, method, xsink)) {
        return QoreValue();
    }
    return new RunTimePseudoMethodReferenceNode(loc, val, method);
}

RunTimePseudoMethodReferenceNode::RunTimePseudoMethodReferenceNode(const QoreProgramLocation* loc, QoreValue n_val,
        const char* n_method) : ResolvedCallReferenceNodeIntern(loc), val(n_val.refSelf()), method(n_method) {
    assert(val.getType() != NT_OBJECT);
    // the restrictions that a call of the reference is checked against: those of the creating context
    captureCreatorParseOptions();
}

RunTimePseudoMethodReferenceNode::~RunTimePseudoMethodReferenceNode() {
    // released in derefImpl()
    assert(val.isNothing());
}

bool RunTimePseudoMethodReferenceNode::derefImpl(ExceptionSink* xsink) {
    val.discard(xsink);
    val.clear();
    return ResolvedCallReferenceNodeIntern::derefImpl(xsink);
}

const ResolvedCallReferenceNode* RunTimePseudoMethodReferenceNode::getHashMember(
        ReferenceHolder<QoreHashNode>& hash_holder) const {
    const AbstractQoreNode* ref = nullptr;
    if (val.getType() == NT_HASH) {
        ref = check_call_ref(val.getInternalNode(), method.c_str());
    } else if (val.getType() == NT_WEAKREF_HASH) {
        hash_holder = val.get<WeakHashReferenceNode>()->lock();
        ref = check_call_ref(*hash_holder, method.c_str());
    }
    return reinterpret_cast<const ResolvedCallReferenceNode*>(ref);
}

QoreValue RunTimePseudoMethodReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    // a code member of a hash with the name of the method, as a direct call calls (QoreDotEvalOperatorNode)
    ReferenceHolder<QoreHashNode> member_hash(xsink);
    if (const ResolvedCallReferenceNode* ref = getHashMember(member_hash)) {
        return ref->execValue(args, xsink);
    }
    return pseudo_classes_eval(val, method.c_str(), args, xsink);
}

QoreValue RunTimePseudoMethodReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    ReferenceHolder<QoreHashNode> member_hash(xsink);
    if (const ResolvedCallReferenceNode* ref = getHashMember(member_hash)) {
        // the code member runs with its own declaration policy
        return ref->execValue(args, xsink);
    }
    return pseudo_classes_eval(policy, val, method.c_str(), args, xsink);
}

QoreValue RunTimePseudoMethodReferenceNode::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    ReferenceHolder<QoreHashNode> member_hash(xsink);
    if (const ResolvedCallReferenceNode* ref = getHashMember(member_hash)) {
        // the code member binds the names, as a named call through it does
        if (const ResolvedCallReferenceNodeIntern* iref = dynamic_cast<const ResolvedCallReferenceNodeIntern*>(ref)) {
            return iref->dispatchNamed(args, arg_names, policy, xsink);
        }
        return ResolvedCallReferenceNodeIntern::dispatchNamed(args, arg_names, policy, xsink);
    }
    // the pseudo-method with its variants bound to the arguments by name, as a direct named call does
    // (MethodCallNode::execPseudoNamed())
    const qore_class_private* pseudo_cls = qore_class_private::get(*qore_pseudo_get_class(val.getType()));
    const QoreMethod* w = pseudo_cls->runtimeFindPseudoMethod(val, method.c_str(), xsink);
    if (!w) {
        assert(*xsink);
        return QoreValue();
    }
    // with no class context: a pseudo-method has no private variants, and the reference keeps no class, which could
    // belong to a Program that is deleted before the reference is called
    return NMETHF(qore_method_private::get(*w)->func)->evalPseudoMethodNamed(xsink, rc, val, args, false,
        qore_no_class_ctx(), arg_names, policy);
}

bool RunTimePseudoMethodReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const RunTimePseudoMethodReferenceNode* vc = dynamic_cast<const RunTimePseudoMethodReferenceNode*>(v);
    return vc && vc->method == method && vc->val.isEqualHard(val, xsink);
}

QoreFunction* RunTimePseudoMethodReferenceNode::getFunction() const {
    QoreClass* qc;
    const QoreMethod* m = pseudo_classes_find_method(val.getType(), method.c_str(), qc);
    return m ? qore_method_private::get(*m)->getFunction() : nullptr;
}

//! returns true if the object has a member of the name, accessible in the class context, that holds code, which a call
//! of the name calls when the class has no method of the name (see qore_class_private::checkMethodForEval())
static bool qore_object_member_holds_code(const qore_class_private* cls, QoreObject* obj, const char* name,
        const qore_class_private* class_ctx) {
    const QoreMemberInfo* mi = cls->runtimeGetCallableMemberInfo(name, class_ctx);
    if (!mi) {
        return false;
    }
    const qore_class_private* member_class_ctx = mi->getClassContext(class_ctx);
    qore_object_private* obj_priv = qore_object_private::get(*obj);
    QoreAutoVarRWReadLocker al(obj_priv->rml);
    if (obj_priv->status == OS_DELETED) {
        return false;
    }
    const QoreHashNode* odata = member_class_ctx ? obj_priv->getInternalData(member_class_ctx) : obj_priv->data;
    if (!odata) {
        return false;
    }
    bool exists = false;
    QoreValue mv = qore_hash_private::get(*odata)->getKeyValueExistenceIntern(name, exists);
    return exists && (mv.getType() == NT_FUNCREF || mv.getType() == NT_RUNTIME_CLOSURE);
}

template <typename F>
QoreValue RunTimeObjectMethodReferenceNode::evalInContext(F f) const {
    // a direct call of a pseudo-method runs in the class context of its code: a call through the reference runs in
    // that of the code that made the reference, which can differ from the context it is called in; so does a method
    // looked up without a class context, whose variants are selected in the class context of the code, as for a
    // direct call (qore#7220)
    if ((missing || !qc || lookup_without_ctx) && qc != runtime_get_class()) {
        ClassOnlySubstitutionHelper csh(qc);
        return f();
    }
    return f();
}

RunTimeObjectMethodReferenceNode::RunTimeObjectMethodReferenceNode(const QoreProgramLocation* loc, QoreObject* n_obj,
        const char* n_method, bool n_missing) : ResolvedCallReferenceNodeIntern(loc), obj(n_obj), method(n_method),
        missing(n_missing) {
    QoreProgram* creator;
    runtime_get_program_and_class(creator, qc);
    // a method call in the code that creates the reference looks the method up without a class context if the
    // context has no private access to the object's class (MethodCallNode::execImpl()), such as the code of a class
    // that the object's class reaches only through the private:internal inheritance of one of its parent classes,
    // whose calls are made as from outside the class hierarchy (qore#7220); a pseudo-method or a member with code
    // (n_missing) is looked up as before
    lookup_without_ctx = qc && !n_missing
        && !qore_class_private::runtimeCheckPrivateClassAccess(*n_obj->getClass(), qc);
    // the restrictions that a call of the reference is checked against: those of the creating context (#5803)
    captureCreatorParseOptions();
    printd(5, "RunTimeObjectMethodReferenceNode::RunTimeObjectMethodReferenceNode() this: %p obj: %p (method: %s " \
        "qc: %p)\n", this, obj, n_method, qc);
    // the class context can belong to another Program than the object, which releases it when it is deleted; as for a
    // static method call reference (qore#6622), the class is referenced, not its Program
    if (qc) {
        qc->ref();
    }
    // the method is called as a direct call in the Program that creates the reference would be; the object holds a
    // weak reference to its own Program, which is where a reference is usually created; any other Program is held
    // with a weak reference of the reference's own, as a strong one could cause a recursive reference
    pgm_ref = reinterpret_cast<uintptr_t>(creator);
    if (creator != qore_object_private::get(*n_obj)->pgm && creator) {
        creator->depRef();
        pgm_ref |= PGM_WEAK_REF;
    }
    obj->tRef();
}

RunTimeObjectMethodReferenceNode::~RunTimeObjectMethodReferenceNode() {
    printd(5, "RunTimeObjectMethodReferenceNode::~RunTimeObjectMethodReferenceNode() this: %p obj: %p (method: %s " \
        "qc: %p)\n", this, obj, method.c_str(), qc);
    // released here and not in derefImpl(), which keeps the dereference of the usual reference, which holds no
    // Program, free of an extra call; the weak reference only keeps the Program's memory
    if (pgm_ref & PGM_WEAK_REF) {
        reinterpret_cast<QoreProgram*>(pgm_ref & ~PGM_WEAK_REF)->depDeref();
    }
    if (qc) {
        const_cast<qore_class_private*>(qc)->deref(false, false, false);
    }
    obj->tDeref();
}

QoreValue RunTimeObjectMethodReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    if (checkObject(xsink)) {
        return QoreValue();
    }
    // issue #2145: set class context after evaluating arguments
    // the method runs as a direct call in the Program that created the reference does: called there, it is not
    // given a Program; called from another Program, it runs in the creating Program with its parse options, as a
    // reference to a builtin method of the object's own class does (RunTimeResolvedMethodReferenceNode), and not
    // with the privileges or restrictions of the Program that calls it.  A user method runs in the Program that
    // defines its class in any case; rc_get_current_ref() has set the current Program
    // (a reference created with no Program has none to run in: nullptr differs from the current Program)
    QoreProgram* pgm = reinterpret_cast<QoreProgram*>(pgm_ref & ~PGM_WEAK_REF);
    QoreProgram* pgm_ctx = pgm != rc_get_current_program(rc) ? pgm : nullptr;
    return evalInContext([&] () {
        return qore_class_private::get(*qore_object_private::get(*obj)->theclass)->evalMethod(obj, method.c_str(),
            args, getLookupContext(), rc, xsink, nullptr, pgm_ctx);
    });
}

QoreValue RunTimeObjectMethodReferenceNode::execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // as dispatchValue(): the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    if (checkObject(xsink)) {
        return QoreValue();
    }
    QoreProgram* pgm = reinterpret_cast<QoreProgram*>(pgm_ref & ~PGM_WEAK_REF);
    QoreProgram* pgm_ctx = pgm != rc_get_current_program(rc) ? pgm : nullptr;
    return evalInContext([&] () {
        return qore_class_private::get(*qore_object_private::get(*obj)->theclass)->evalMethodGivenArgs(obj,
            method.c_str(), args, getLookupContext(), rc, xsink, pgm_ctx);
    });
}

QoreValue RunTimeObjectMethodReferenceNode::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    if (checkObject(xsink)) {
        return QoreValue();
    }
    // the Program of the call as for a call with positional arguments (see dispatchValue())
    QoreProgram* pgm = reinterpret_cast<QoreProgram*>(pgm_ref & ~PGM_WEAK_REF);
    QoreProgram* pgm_ctx = pgm != rc_get_current_program(rc) ? pgm : nullptr;
    // the method of the class of the object (which inherits or overrides the method that the reference was created
    // with), whose variants the names are bound to; a methodGate() method takes
    // its arguments by position
    const qore_class_private* cls = qore_class_private::get(*qore_object_private::get(*obj)->theclass);
    const QoreMethod* m = cls->getMethodForEval(method.c_str(), obj->getProgram(), getLookupContext(), xsink);
    if (*xsink) {
        return QoreValue();
    }
    if (!m) {
        // a member of the name holding code is called before a pseudo-method by a positional call
        // (qore_eval_missing_method()); a named call of it is refused, as a direct named call of a member declared
        // with a code type is (a direct named call of an auto member binds the names to the pseudo-method and calls
        // the code by position: qore#6361); a member holding no code leaves the call to the pseudo-method (issue
        // #995)
        if (qore_object_member_holds_code(cls, obj, method.c_str(), qc)) {
            xsink->raiseException("NAMED-CALL-NOT-SUPPORTED", "cannot call %s::%s() with named arguments: it is a "
                "member of the class, whose code takes the arguments of the call by position", obj->getClassName(),
                method.c_str());
            return QoreValue();
        }
        // a pseudo-method of objects with its variants bound to the arguments by name, as a direct named call binds
        // them (issue #995)
        QoreClass* pqc;
        if (const QoreMethod* w = pseudo_classes_find_method(NT_OBJECT, method.c_str(), pqc)) {
            return evalInContext([&] () {
                return NMETHF(qore_method_private::get(*w)->func)->evalPseudoMethodNamed(xsink, rc, QoreValue(obj),
                    args, false, qc, arg_names, policy);
            });
        }
        // as for a named call of the method (AbstractMethodCallNode::execNamed()): a method of the name is handled by
        // the class's methodGate() method, which takes its arguments by position, so a named call of it is refused,
        // as at parse time for a direct call
        if (cls->methodGate) {
            xsink->raiseException("NAMED-CALL-NOT-SUPPORTED", "cannot call %s::%s() with named arguments: the class "
                "has no method of the name, and its methodGate() method takes the arguments of the call by position",
                obj->getClassName(), method.c_str());
            return QoreValue();
        }
        xsink->raiseException("METHOD-DOES-NOT-EXIST", "no method %s::%s() has been defined", obj->getClassName(),
            method.c_str());
        return QoreValue();
    }
    // with no class context, only the public variants of the method, as for a call with positional arguments
    return qore_method_private::evalNamed(*m, xsink, rc, obj, args, false, qc ? qc : qore_no_class_ctx(), nullptr,
        nullptr, arg_names, policy, pgm_ctx);
}

QoreValue RunTimeObjectMethodReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // Compatibility semantics do not change the permissions of the reference's creator.
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    if (checkObject(xsink)) {
        return QoreValue();
    }
    // issue #2145: set class context after evaluating arguments
    // the method runs as a direct call in the Program that created the reference does: called there, it is not
    // given a Program; called from another Program, it runs in the creating Program with its parse options, as a
    // reference to a builtin method of the object's own class does (RunTimeResolvedMethodReferenceNode), and not
    // with the privileges or restrictions of the Program that calls it.  A user method runs in the Program that
    // defines its class in any case; rc_get_current_ref() has set the current Program
    // (a reference created with no Program has none to run in: nullptr differs from the current Program)
    QoreProgram* pgm = reinterpret_cast<QoreProgram*>(pgm_ref & ~PGM_WEAK_REF);
    QoreProgram* pgm_ctx = pgm != rc_get_current_program(rc) ? pgm : nullptr;
    return evalInContext([&] () {
        return qore_class_private::get(*qore_object_private::get(*obj)->theclass)->evalMethod(policy, obj,
            method.c_str(), args, getLookupContext(), rc, xsink, nullptr, pgm_ctx);
    });
}

QoreProgram* RunTimeObjectMethodReferenceNode::getProgram() const {
    return obj->getProgram();
}

const QoreMethod* RunTimeObjectMethodReferenceNode::findMethod() const {
    return obj ? obj->getClass()->findMethod(method.c_str()) : nullptr;
}

int RunTimeResolvedMethodReferenceNode::checkObject(ExceptionSink* xsink) const {
    // the status is read inline, without the exported call of QoreObject::isValid() (#5692)
    if (obj && qore_object_private::get(*obj)->status == OS_DELETED) {
        // the method of the reference, as the call names it (qore_object_private::startCall())
        qore_object_private::get(*obj)->raiseDeletedCall(qore_class_private::get(*method->getClass()),
            method->getName(), xsink);
        return -1;
    }
    return 0;
}

int RunTimeObjectMethodReferenceNode::checkObject(ExceptionSink* xsink) const {
    // the status is read inline, without the exported call of QoreObject::isValid() (#5692)
    if (obj && qore_object_private::get(*obj)->status == OS_DELETED) {
        // the method that the call finds by name, as the call names it (qore_object_private::startCall()): looked up
        // in the class context of the reference, as the call looks it up (evalMethod()), not in that of the caller;
        // the object's class if there is none
        const qore_class_private* class_ctx = qc;
        if (class_ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*obj->getClass(), class_ctx)) {
            class_ctx = nullptr;
        }
        ExceptionSink lookup_xsink;
        const QoreMethod* m = qore_class_private::get(*obj->getClass())->getMethodForEval(method.c_str(),
            obj->getProgram(), class_ctx, &lookup_xsink);
        // an error of the lookup is the call's own, which is not made
        lookup_xsink.clear();
        qore_object_private::get(*obj)->raiseDeletedCall(m ? qore_class_private::get(*m->getClass()) : nullptr,
            method.c_str(), xsink);
        return -1;
    }
    return 0;
}

void qore_raise_not_call_reference(ExceptionSink* xsink, const QoreProgramLocation* loc, const QoreValue& v) {
    if (loc) {
        xsink->raiseException(*loc, "REFERENCE-CALL-ERROR", QoreValue(), "expression does not evaluate to a call "
            "reference (evaluated to type '%s')", v.getTypeName());
    } else {
        xsink->raiseException("REFERENCE-CALL-ERROR", "expression does not evaluate to a call reference (evaluated "
            "to type '%s')", v.getTypeName());
    }
}

int qore_call_reference_check_target(const ResolvedCallReferenceNode* rcr, ExceptionSink* xsink) {
    if (const RunTimeResolvedMethodReferenceNode* r = dynamic_cast<const RunTimeResolvedMethodReferenceNode*>(rcr)) {
        return r->checkObject(xsink);
    }
    if (const RunTimeObjectMethodReferenceNode* r = dynamic_cast<const RunTimeObjectMethodReferenceNode*>(rcr)) {
        return r->checkObject(xsink);
    }
    return 0;
}

//! returns the method whose code a call of a method or static method reference runs, or nullptr for another reference
/** For a method of an object, the method that the call is dispatched to on the object's class
*/
static const QoreMethod* qore_call_reference_code_method(const ResolvedCallReferenceNode* rcr) {
    const QoreMethod* m = nullptr;
    // the object a method is called on, if any: the call is dispatched on its class
    const QoreObject* obj = nullptr;
    if (const RunTimeResolvedMethodReferenceNode* r = dynamic_cast<const RunTimeResolvedMethodReferenceNode*>(rcr)) {
        m = r->getMethod();
        obj = r->getObject();
    } else if (const RunTimeObjectMethodReferenceNode* r
            = dynamic_cast<const RunTimeObjectMethodReferenceNode*>(rcr)) {
        m = r->findMethod();
    } else if (const MethodCallReferenceNode* r = dynamic_cast<const MethodCallReferenceNode*>(rcr)) {
        m = r->getMethod();
        obj = r->getObject();
    } else if (const LocalStaticMethodCallReferenceNode* r
            = dynamic_cast<const LocalStaticMethodCallReferenceNode*>(rcr)) {
        m = r->getMethod();
    }
    if (m && obj && !m->isStatic()) {
        // the method that the call is dispatched to on the object's class
        const QoreMethod* dm = obj->getClass()->findMethod(m->getName());
        if (dm) {
            m = dm;
        }
    }
    return m;
}

const QoreProgram* qore_call_reference_code_program(const ResolvedCallReferenceNode* rcr) {
    assert(rcr);
    const QoreMethod* m = qore_call_reference_code_method(rcr);
    if (!m) {
        // a closure's code is that of the Program that defines it, which is not necessarily the Program it runs
        // in: a closure that module code creates while running a method for an object of a class of its caller
        // runs in the caller's Program, but its code is the module's
        if (rcr->getType() == NT_RUNTIME_CLOSURE) {
            const QoreFunction* f = rcr->getFunction();
            const AbstractQoreFunctionVariant* v = f ? f->first() : nullptr;
            const UserVariantBase* uvb = v ? v->getUserVariantBase() : nullptr;
            if (uvb && uvb->pgm) {
                return uvb->pgm;
            }
        }
        return rcr->getProgram();
    }
    // the code of a method runs in the Program that defines its class; a builtin class has none
    const QoreClass* cls = m->getClass();
    const QoreProgram* pgm = cls->getSourceProgram();
    return pgm ? pgm : cls->getProgram();
}

//! returns the user variant of a call reference to a user function (not a closure or a method), or nullptr
static const UserVariantBase* qore_call_reference_function_variant(const ResolvedCallReferenceNode* rcr) {
    if (rcr->getType() == NT_RUNTIME_CLOSURE) {
        return nullptr;
    }
    const QoreFunction* f = rcr->getFunction();
    if (!f || f->getClass() || !f->hasUser()) {
        return nullptr;
    }
    QoreFunctionIterator i(*f);
    while (i.next()) {
        if (const UserVariantBase* uvb = i.getVariant()->getUserVariantBase()) {
            return uvb;
        }
    }
    return nullptr;
}

int qore_call_reference_code_parse_options(const ResolvedCallReferenceNode* rcr, QoreParseOptions& po,
        ExceptionSink* xsink, bool* sandboxed) {
    if (sandboxed) {
        *sandboxed = false;
    }
    // the code of a user function runs in the Program that defines it (see qore_get_function_pgm_context()), which
    // can have been deleted: it is taken with a reference while its options are read
    if (const UserVariantBase* uvb = qore_call_reference_function_variant(rcr)) {
        QoreSourceProgramHolder holder(xsink);
        holder.setVariant(uvb);
        if (QoreProgram* pgm = holder.get()) {
            // a Program that another thread has deleted, or is deleting, stays registered while any thread holds a
            // reference to it (such as the thread that runs a call of the function scheduled earlier, while the call
            // leaves the Program), but its code cannot run anymore: it is deleted whatever other threads hold
            if (qore_program_private::get(*pgm)->deletedByOtherThread()) {
                return -1;
            }
            po = pgm->getParseOptions();
            if (sandboxed) {
                *sandboxed = qore_program_is_sandboxed(pgm);
            }
            return 1;
        }
        if (holder.sourceDeleted()) {
            return -1;
        }
    }
    const QoreProgram* pgm = qore_call_reference_code_program(rcr);
    if (!pgm) {
        return 0;
    }
    po = pgm->getParseOptions();
    if (sandboxed) {
        *sandboxed = qore_program_is_sandboxed(pgm);
    }
    return 1;
}

bool RunTimeObjectMethodReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const RunTimeObjectMethodReferenceNode* vc = dynamic_cast<const RunTimeObjectMethodReferenceNode*>(v);
    return vc && obj == vc->obj && method == vc->method;
}

int qore_resolve_call_reference_type_args(std::vector<QoreParseTypeInfo*>& parse_type_args,
        const QoreProgramLocation* loc, std::vector<const QoreTypeInfo*>& type_args) {
    int err = 0;
    type_args.clear();
    type_args.reserve(parse_type_args.size());
    for (QoreParseTypeInfo* pti : parse_type_args) {
        type_args.push_back(QoreParseTypeInfo::resolveAny(pti, loc, err));
        delete pti;
    }
    parse_type_args.clear();
    return err ? -1 : 0;
}

UnresolvedProgramCallReferenceNode::UnresolvedProgramCallReferenceNode(const QoreProgramLocation* loc, char* n_str)
        : AbstractUnresolvedCallReferenceNode(loc, false), str(n_str) {
}

UnresolvedProgramCallReferenceNode::~UnresolvedProgramCallReferenceNode() {
    free(str);
    for (QoreParseTypeInfo* pti : explicit_parse_type_args) {
        delete pti;
    }
}

int UnresolvedProgramCallReferenceNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = callReferenceTypeInfo;
    // the following function returns the argument if there's an error
    val = qore_root_ns_private::parseResolveCallReference(this);
    return val.getInternalNode() == this ? -1 : 0;
}

int UnresolvedCallReferenceNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = callReferenceTypeInfo;

    // try to resolve a method call if bare references are allowed
    // and we are parsing in an object context
    if (parse_check_parse_option(PO_ALLOW_BARE_REFS) && parse_context.oflag) {
        // resolve the unqualified self-method reference against the ambient lexical class rather than
        // parse_context.class_ctx: they are not always the same object, and class_ctx is not guaranteed to be
        // set whenever oflag is (e.g. in a base-constructor argument list oflag is set but class_ctx is null,
        // while the lexical class is the derived class being constructed). Using parse_get_class() keeps
        // self-method reference resolution consistent with bare self-member resolution (see
        // parseResolveBarewordIntern()) and avoids dereferencing a null class context
        if (QoreClass* qc = parse_get_class()) {
            const QoreMethod* m = qore_class_private::parseFindSelfMethod(qc, str);
            if (m) {
                if (!explicit_parse_type_args.empty()) {
                    parse_error(*loc, "explicit type arguments are not supported in the call reference '\\%s()' to a "
                        "method; call the method, or reference a function", str);
                    return -1;
                }
                // a reference allows the call that naming the method would
                int err = check_call_reference_domain(loc, parse_context, m);
                val = new ParseSelfMethodReferenceNode(loc, m);
                delete this;
                return err;
            }
        }
    } else if (parse_context.class_ctx) {
        qore_class_private* priv = qore_class_private::get(*const_cast<QoreClass*>(parse_context.class_ctx));
        const QoreMethod* m = priv->parseFindStaticMethod(str, priv);
        if (m) {
            assert(m->isStatic());
            if (!explicit_parse_type_args.empty()) {
                parse_error(*loc, "explicit type arguments are not supported in the call reference '\\%s()' to a "
                    "method; call the method, or reference a function", str);
                return -1;
            }
            // resolve to the same node as the qualified form (\Class::method()) so that both spellings
            // evaluate and lower identically
            // a reference allows the call that naming the method would
            int err = check_call_reference_domain(loc, parse_context, m);
            val = new LocalStaticMethodCallReferenceNode(loc, m);
            delete this;
            return err;
        }
    }

    // the following function returns the argument if there's an error
    val = qore_root_ns_private::parseResolveCallReference(this);
    return val.getInternalNode() == this ? -1 : 0;
}

LocalStaticMethodCallReferenceNode::LocalStaticMethodCallReferenceNode(const QoreProgramLocation* loc,
        const QoreMethod* n_method, const QoreClass* n_scope_cls) : ResolvedCallReferenceNodeIntern(loc, true),
        method(n_method),
        scope_cls(n_scope_cls && n_method && n_scope_cls != n_method->getClass() ? n_scope_cls : nullptr) {
    if (scope_cls) {
        receiver_type_info = qore_class_private::get(*scope_cls)->getStaticCallReceiverTypeInfo(*method);
    }
}

const QoreTypeInfo* LocalStaticMethodCallReferenceNode::getRuntimeReceiverTypeInfo(const QoreMethod& m,
        const QoreClass* scope_cls, const qore_class_private* code_class) {
    const QoreClass* mcls = m.getClass();
    if (!mcls) {
        return nullptr;
    }
    // the method of a class without type parameters that has variants of a generic class binds the type parameters
    // of the generic class wherever it is called from (qore#6448)
    if (!mcls->hasTypeParameters()) {
        return qore_class_private::getInheritedStaticCallReceiverTypeInfo(m);
    }
    if (scope_cls) {
        if (const QoreTypeInfo* rv = qore_class_private::get(*scope_cls)->getStaticCallReceiverTypeInfo(m)) {
            return rv;
        }
    }
    // in the code of a class that is or inherits the method's class, the receiver of the running instantiation of
    // the code: the specialization of the method's class that it is or inherits (as a call in the class's code is
    // bound at parse time; see qore_class_private::parseGetStaticCallReceiverTypeInfo())
    ClassAccess access;
    if (!code_class || !code_class->getClass(*qore_class_private::get(*mcls), access)) {
        return nullptr;
    }
    // the code of a class without type parameters binds them to the specialization that it inherits, also when it
    // declares a method of the same name, which the reference does not name then
    if (!code_class->hasTypeParams()) {
        return code_class->getStaticCallReceiverTypeInfo(m);
    }
    const QoreTypeInfo* current = runtime_get_receiver_type_info();
    if (!current) {
        return nullptr;
    }
    if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(current)) {
        return qore_class_private::get(*pti->getBaseClass())->getParameterizedBaseTypeInfo(pti, mcls);
    }
    const QoreClass* current_cls = QoreTypeInfo::getUniqueReturnClass(current);
    return current_cls ? qore_class_private::get(*current_cls)->getStaticCallReceiverTypeInfo(m) : nullptr;
}

const QoreTypeInfo* LocalStaticMethodCallReferenceNode::getCallReceiverTypeInfo(const QoreMethod& m,
        const QoreTypeInfo* receiver_type_info, const QoreListNode* args) {
    if (receiver_type_info) {
        return receiver_type_info;
    }
    const QoreClass* mcls = m.getClass();
    if (!mcls) {
        return nullptr;
    }
    if (!mcls->hasTypeParameters()) {
        return qore_class_private::getInheritedStaticCallReceiverTypeInfo(m);
    }
    return qore_method_private::get(m)->getFunction()->runtimeInferClassReceiverTypeInfo(args);
}

const QoreTypeInfo* LocalStaticMethodCallReferenceNode::getCallReceiverTypeInfo(const QoreMethod& m,
        const QoreTypeInfo* receiver_type_info, const QoreListNode*& args,
        QoreListNodeEvalOptionalRefHolder& eval_args, ExceptionSink* xsink) {
    if (receiver_type_info) {
        return receiver_type_info;
    }
    const QoreClass* mcls = m.getClass();
    if (!mcls) {
        return nullptr;
    }
    if (!mcls->hasTypeParameters()) {
        return qore_class_private::getInheritedStaticCallReceiverTypeInfo(m);
    }
    // the type arguments are inferred from the values of the arguments
    if (args && args->needs_eval()) {
        eval_args.assignEval(args);
        if (*xsink) {
            return nullptr;
        }
        args = *eval_args;
    }
    return qore_method_private::get(m)->getFunction()->runtimeInferClassReceiverTypeInfo(args);
}

// evalImpl(): return value requires a deref(xsink) if not 0
QoreValue LocalStaticMethodCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue LocalStaticMethodCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    QoreProgram* call_pgm = rc.getProgram() ? rc.getProgram() : ::getProgram();
    const qore_class_private* cls = rc.getClass() ? rc.getClass() : runtime_get_class();
    return new StaticMethodCallReferenceNode(loc, method, call_pgm, cls,
        receiver_type_info ? receiver_type_info : getRuntimeReceiverTypeInfo(*method, nullptr, cls));
}

QoreValue LocalStaticMethodCallReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    QoreListNodeEvalOptionalRefHolder eval_args(xsink);
    const QoreTypeInfo* rti = getCallReceiverTypeInfo(*method, receiver_type_info, args, eval_args, xsink);
    if (*xsink) {
        return QoreValue();
    }
    return qore_method_private::eval(*method, xsink, rc, nullptr, args, nullptr, nullptr, rti);
}

QoreValue LocalStaticMethodCallReferenceNode::execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // as dispatchValue(): the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return qore_method_private::evalStaticGivenArgs(*method, xsink, rc, args, nullptr, nullptr,
        getCallReceiverTypeInfo(*method, receiver_type_info, args));
}

QoreValue LocalStaticMethodCallReferenceNode::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    QoreListNodeEvalOptionalRefHolder eval_args(xsink);
    const QoreTypeInfo* rti = getCallReceiverTypeInfo(*method, receiver_type_info, args, eval_args, xsink);
    if (*xsink) {
        return QoreValue();
    }
    return qore_method_private::evalNamed(*method, xsink, rc, nullptr, args, false, nullptr, rti, nullptr,
        arg_names, policy);
}

QoreValue LocalStaticMethodCallReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // Compatibility semantics do not change the permissions of the reference's creator.
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    QoreListNodeEvalOptionalRefHolder eval_args(xsink);
    const QoreTypeInfo* rti = getCallReceiverTypeInfo(*method, receiver_type_info, args, eval_args, xsink);
    if (*xsink) {
        return QoreValue();
    }
    return qore_method_private::eval(policy, *method, xsink, rc, nullptr, args, nullptr, nullptr, rti);
}

bool LocalStaticMethodCallReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const LocalStaticMethodCallReferenceNode* vc = dynamic_cast<const LocalStaticMethodCallReferenceNode*>(v);
    //printd(5, "LocalStaticMethodCallReferenceNode::is_equal_hard() %p == %p (%p %s)\n", uf, vc ? vc->uf : 0, v,
    //  v ? v->getTypeName() : "n/a");
    return vc && method == vc->method && receiver_type_info == vc->receiver_type_info;
}

QoreFunction* LocalStaticMethodCallReferenceNode::getFunction() const {
    return method ? qore_method_private::get(*method)->getFunction() : nullptr;
}

// evalImpl(): return value requires a deref(xsink) if not 0
QoreValue LocalMethodCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue LocalMethodCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    QoreObject* o = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    return new RunTimeResolvedMethodReferenceNode(loc, o, method,
        rc.getClass() ? rc.getClass() : runtime_get_class());
}

QoreValue LocalMethodCallReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return qore_method_private::eval(*method, xsink, rc, runtime_get_stack_object(), args, nullptr);
}

QoreValue LocalMethodCallReferenceNode::execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // as dispatchValue(): the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return qore_method_private::evalGivenArgs(*method, xsink, rc, runtime_get_stack_object(), args);
}

QoreValue LocalMethodCallReferenceNode::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return qore_method_private::evalNamed(*method, xsink, rc, runtime_get_stack_object(), args, false, nullptr,
        nullptr, nullptr, arg_names, policy);
}

QoreValue LocalMethodCallReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // Compatibility semantics do not change the permissions of the reference's creator.
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return qore_method_private::eval(policy, *method, xsink, rc, runtime_get_stack_object(), args, nullptr);
}

bool LocalMethodCallReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const LocalMethodCallReferenceNode* vc = dynamic_cast<const LocalMethodCallReferenceNode*>(v);
    //printd(5, "LocalMethodCallReferenceNode::is_equal_hard() %p == %p (%p %s)\n", uf, vc ? vc->uf : 0, v,
    //  v ? v->getTypeName() : "n/a");
    return vc && method == vc->method;
}

StaticMethodCallReferenceNode::StaticMethodCallReferenceNode(const QoreProgramLocation* loc,
        const QoreMethod* n_method, QoreProgram* n_pgm, const qore_class_private* n_class_ctx,
        const QoreTypeInfo* n_receiver_type_info)
        : LocalStaticMethodCallReferenceNode(loc, n_method, false), pgm(n_pgm), class_ctx(n_class_ctx) {
    receiver_type_info = n_receiver_type_info;
    captureCreatorParseOptions();
    assert(pgm);
    //printd(5, "StaticMethodCallReferenceNode::StaticMethodCallReferenceNode() this: %p %s::%s()\n", this,
    //  n_method->getClass()->getName(), n_method->getName());
    //printd(5, "StaticMethodCallReferenceNode::StaticMethodCallReferenceNode() this: %p calling
    //  QoreProgram::depRef() pgm: %p\n", this, pgm);
    // make a weak reference to the Program - a strong reference (QoreProgram::ref()) could cause a recursive
    // reference
    pgm->depRef();
    // the classes are referenced, as the Program's copies of the classes that it imported are released with it
    // (qore#6622); a class can refer to the reference (from a static variable or a constant), but the deletion of
    // the Program releases those values with their own reference counts, so this does not make a recursive reference
    const qore_class_private* receiver_cls = nullptr;
    if (receiver_type_info) {
        if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(receiver_type_info)) {
            receiver_cls = qore_class_private::get(*pti->getBaseClass());
        } else if (const QoreClass* rcls = QoreTypeInfo::getUniqueReturnClass(receiver_type_info)) {
            receiver_cls = qore_class_private::get(*rcls);
        }
    }
    const qore_class_private* classes[3] = {
        method->getClass() ? qore_class_private::get(*method->getClass()) : nullptr,
        class_ctx,
        receiver_cls,
    };
    for (unsigned i = 0; i < 3; ++i) {
        if (classes[i]) {
            classes[i]->ref();
            held_classes[i] = classes[i];
        }
    }
    // a reference created with no class context is called with none, so that a call selects only the public variants
    // of the method, not those that the class context of the code calling it could select
    if (!class_ctx) {
        class_ctx = qore_no_class_ctx();
    }
}

bool StaticMethodCallReferenceNode::derefImpl(ExceptionSink* xsink) {
    //printd(5, "StaticMethodCallReferenceNode::deref() this: %p pgm: %p refs: %d -> %d\n", this, pgm,
    //  reference_count(), reference_count() - 1);
    for (const qore_class_private* cls : held_classes) {
        if (cls) {
            const_cast<qore_class_private*>(cls)->deref(false, false, false);
        }
    }
    pgm->depDeref();
#ifdef DEBUG
    pgm = nullptr;
#endif
    return ResolvedCallReferenceNode::derefImpl(xsink);
}

QoreValue StaticMethodCallReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    // issue #4964: do not set object or class context until arguments are evaluated
    // do not set pgm context here before evaluating args
    QoreListNodeEvalOptionalRefHolder eval_args(xsink);
    const QoreTypeInfo* rti = getCallReceiverTypeInfo(*method, receiver_type_info, args, eval_args, xsink);
    if (*xsink) {
        return QoreValue();
    }
    return qore_method_private::eval(*method, xsink, rc, nullptr, args, class_ctx, pgm, rti);
}

QoreValue StaticMethodCallReferenceNode::execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // as dispatchValue(): the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return qore_method_private::evalStaticGivenArgs(*method, xsink, rc, args, class_ctx, pgm,
        getCallReceiverTypeInfo(*method, receiver_type_info, args));
}

QoreValue StaticMethodCallReferenceNode::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    QoreListNodeEvalOptionalRefHolder eval_args(xsink);
    const QoreTypeInfo* rti = getCallReceiverTypeInfo(*method, receiver_type_info, args, eval_args, xsink);
    if (*xsink) {
        return QoreValue();
    }
    return qore_method_private::evalNamed(*method, xsink, rc, nullptr, args, false, class_ctx, rti, nullptr,
        arg_names, policy, pgm);
}

QoreValue StaticMethodCallReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // Compatibility semantics do not change the permissions of the reference's creator.
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    // issue #4964: do not set object or class context until arguments are evaluated
    // do not set pgm context here before evaluating args
    QoreListNodeEvalOptionalRefHolder eval_args(xsink);
    const QoreTypeInfo* rti = getCallReceiverTypeInfo(*method, receiver_type_info, args, eval_args, xsink);
    if (*xsink) {
        return QoreValue();
    }
    return qore_method_private::eval(policy, *method, xsink, rc, nullptr, args, class_ctx, pgm, rti);
}

DeferredStaticMethodCallReferenceNode::DeferredStaticMethodCallReferenceNode(const QoreProgramLocation* loc,
        const char* n_class_path, const char* n_method_name)
        : ResolvedCallReferenceNodeIntern(loc, true), class_path(n_class_path ? n_class_path : ""),
        method_name(n_method_name ? n_method_name : "") {
}

bool DeferredStaticMethodCallReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const DeferredStaticMethodCallReferenceNode* d = dynamic_cast<const DeferredStaticMethodCallReferenceNode*>(v);
    return d && class_path == d->class_path && method_name == d->method_name;
}

ResolvedCallReferenceNode* DeferredStaticMethodCallReferenceNode::resolve(RuntimeConfig& rc,
        ExceptionSink* xsink) const {
    if (class_path.empty() || method_name.empty()) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot resolve method call reference '%s::%s()'",
            class_path.c_str(), method_name.c_str());
        return nullptr;
    }

    QoreProgram* pgm = rc.getProgram() ? rc.getProgram() : getProgram();
    if (!pgm) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve method call reference '%s::%s()': no program context",
            class_path.c_str(), method_name.c_str());
        return nullptr;
    }

    const QoreClass* qc = qore_aot_resolve_class_ref(pgm, class_path.c_str(), false);
    if (!qc) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve class '%s' for method call reference '%s()'",
            class_path.c_str(), method_name.c_str());
        return nullptr;
    }

    ClassAccess access = Public;
    const QoreMethod* method = qc->findStaticMethod(method_name.c_str(), access);
    if (!method) {
        method = qc->findMethod(method_name.c_str(), access);
    }
    if (!method) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve method call reference '%s::%s()'",
            class_path.c_str(), method_name.c_str());
        return nullptr;
    }
    if (access > Public && !qore_class_private::runtimeCheckPrivateClassAccess(*qc)) {
        xsink->raiseException("ILLEGAL-CALL-REFERENCE",
            "cannot create a call reference to %s %s::%s() from outside the class",
            privpub(access), class_path.c_str(), method_name.c_str());
        return nullptr;
    }

    QoreProgram* call_pgm = rc.getProgram() ? rc.getProgram() : pgm;
    const qore_class_private* cls = rc.getClass() ? rc.getClass() : runtime_get_class();
    if (method->isStatic()) {
        return new StaticMethodCallReferenceNode(loc, method, call_pgm, cls,
            LocalStaticMethodCallReferenceNode::getRuntimeReceiverTypeInfo(*method, qc, cls));
    }

    QoreObject* obj = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    if (!obj) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot create instance method call reference '%s::%s()': no current object",
            class_path.c_str(), method_name.c_str());
        return nullptr;
    }
    return new RunTimeResolvedMethodReferenceNode(loc, obj, method, cls);
}

QoreValue DeferredStaticMethodCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue DeferredStaticMethodCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref,
        ExceptionSink* xsink) const {
    assert(needs_deref);
    return resolve(rc, xsink);
}

QoreValue DeferredStaticMethodCallReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    ResolvedCallReferenceNode* ref = resolve(rc, xsink);
    if (!ref) {
        return QoreValue();
    }
    QoreValue rv = ref->invoke(args, xsink);
    ref->deref(xsink);
    return rv;
}

QoreValue DeferredStaticMethodCallReferenceNode::dispatchNamed(const QoreListNode* args,
        const name_vec_t& arg_names, LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    ResolvedCallReferenceNode* ref = resolve(rc, xsink);
    if (!ref) {
        return QoreValue();
    }
    QoreValue rv = qore_call_ref_invoke_named(ref, args, arg_names, policy, xsink);
    ref->deref(xsink);
    return rv;
}

QoreValue DeferredStaticMethodCallReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // Compatibility semantics do not change the permissions of the reference's creator.
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    ResolvedCallReferenceNode* ref = resolve(rc, xsink);
    if (!ref) {
        return QoreValue();
    }
    QoreValue rv = qore_resolved_call_reference_node_ops(ref).invokeLexical(args, xsink, policy);
    ref->deref(xsink);
    return rv;
}

MethodCallReferenceNode::MethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method,
        QoreProgram* n_pgm) : LocalMethodCallReferenceNode(loc, n_method, false), obj(runtime_get_stack_object()) {
    captureCreatorParseOptions();
    assert(obj);
    //printd(5, "MethodCallReferenceNode::MethodCallReferenceNode() this: %p %s::%s()\n", this,
    //  n_method->getClass()->getName(), n_method->getName());
    //printd(5, "MethodCallReferenceNode::MethodCallReferenceNode() this: %p calling QoreProgram::depRef() pgm: %p\n",
    //  this, pgm);
    // make a weak reference to the object
    obj->tRef();
}

bool MethodCallReferenceNode::derefImpl(ExceptionSink* xsink) {
    //printd(5, "MethodCallReferenceNode::deref() this: %p pgm: %p refs: %d -> %d\n", this, pgm, reference_count(),
    //  reference_count() - 1);
    obj->tDeref();
    return ResolvedCallReferenceNode::derefImpl(xsink);
}

QoreValue MethodCallReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return qore_method_private::eval(*method, xsink, rc, obj, args);
}

QoreValue MethodCallReferenceNode::execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // as dispatchValue(): the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return qore_method_private::evalGivenArgs(*method, xsink, rc, obj, args);
}

QoreValue MethodCallReferenceNode::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return qore_method_private::evalNamed(*method, xsink, rc, obj, args, false, nullptr, nullptr, nullptr, arg_names,
        policy);
}

QoreValue MethodCallReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // Compatibility semantics do not change the permissions of the reference's creator.
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return qore_method_private::eval(policy, *method, xsink, rc, obj, args);
}

UnresolvedStaticMethodCallReferenceNode::UnresolvedStaticMethodCallReferenceNode(const QoreProgramLocation* loc,
        NamedScope* n_scope) : AbstractUnresolvedCallReferenceNode(loc, false), scope(n_scope) {
    //printd(5, "UnresolvedStaticMethodCallReferenceNode::UnresolvedStaticMethodCallReferenceNode(%s) this: %p\n",
    //  n_scope->ostr, this);
}

UnresolvedStaticMethodCallReferenceNode::~UnresolvedStaticMethodCallReferenceNode() {
    //printd(5, "UnresolvedStaticMethodCallReferenceNode::~UnresolvedStaticMethodCallReferenceNode() this: %p\n",
    //  this);
    delete scope;
    for (QoreParseTypeInfo* pti : explicit_parse_type_args) {
        delete pti;
    }
}

int UnresolvedStaticMethodCallReferenceNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = callReferenceTypeInfo;

    std::string source_receiver_path;
    if (scope->size() >= 2) {
        for (unsigned i = 0; i < (scope->size() - 1); ++i) {
            if (i) {
                source_receiver_path += "::";
            }
            source_receiver_path += (*scope)[i];
        }
    }
    std::string deferred_source_receiver_path;
    if (!source_receiver_path.empty()) {
        deferred_source_receiver_path = qore_aot_get_deferred_source_symbol_path(loc, source_receiver_path.c_str(),
            QoreAOTSourceSymbolKind::Class);
    }
    const bool defer_source_static_receiver = !deferred_source_receiver_path.empty();
    bool defer_source_function = false;
    std::string deferred_source_function_path;
    if (scope->size() >= 2) {
        deferred_source_function_path = qore_aot_get_deferred_source_symbol_path(loc, scope->ostr,
            QoreAOTSourceSymbolKind::Function);
        defer_source_function = !deferred_source_function_path.empty();
        if (!defer_source_function && !defer_source_static_receiver) {
            deferred_source_function_path = qore_aot_get_deferred_source_symbol_path(loc, scope->getIdentifier(),
                QoreAOTSourceSymbolKind::Function);
            defer_source_function = !deferred_source_function_path.empty();
        }
    }
    if (defer_source_static_receiver && !defer_source_function) {
        qore_class_private* class_ctx = parse_context.oflag
            ? qore_class_private::get(*const_cast<QoreClass*>(
                QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo())))
            : parse_get_class_priv();
        QoreClass* import_qc = qore_root_ns_private::parseFindScopedClassWithMethod(
            loc, *scope, false);
        if (import_qc && class_ctx
                && !qore_class_private::parseCheckPrivateClassAccess(*import_qc, class_ctx)) {
            class_ctx = nullptr;
        }
        const QoreMethod* import_method = nullptr;
        if (import_qc) {
            qore_class_private* import_priv = qore_class_private::get(*import_qc);
            import_method = parse_context.oflag && parse_check_parse_option(PO_ALLOW_BARE_REFS)
                ? import_priv->parseFindAnyMethodStaticFirst(scope->getIdentifier(), class_ctx)
                : import_priv->parseFindStaticMethod(scope->getIdentifier(), class_ctx);
        }
        std::string method_path = deferred_source_receiver_path;
        method_path += "::";
        method_path += scope->getIdentifier();
        if (QoreProgram* pgm = parse_context.pgm ? parse_context.pgm : getProgram()) {
            qore_aot_record_source_parse_call_import(pgm, loc, method_path.c_str(),
                true, import_method);
        }
        val = new DeferredStaticMethodCallReferenceNode(loc, deferred_source_receiver_path.c_str(),
            scope->getIdentifier());
        deref();
        return 0;
    }

    QoreClass* qc = qore_root_ns_private::parseFindScopedClassWithMethod(loc, *scope, false);
    if (qc && !explicit_parse_type_args.empty()) {
        // a call reference to a method does not take explicit type arguments (they would be ignored silently)
        parse_error(*loc, "explicit type arguments are not supported in the call reference '\\%s()' to a method; "
            "call the method, or reference a function", scope->ostr);
        return -1;
    }
    // a reference to a function defined in a namespace
    auto make_function_ref = [&](const FunctionEntry* fe) -> int {
        LocalFunctionCallReferenceNode* fr = new LocalFunctionCallReferenceNode(loc, fe->getFunction());
        fr->setFunctionEntry(fe);
        // the explicit type arguments of \NS::f<T>() instantiate the function when the reference is called
        if (!explicit_parse_type_args.empty()) {
            std::vector<const QoreTypeInfo*> type_args;
            if (qore_resolve_call_reference_type_args(explicit_parse_type_args, loc, type_args)) {
                fr->deref(nullptr);
                return -1;
            }
            fr->setExplicitTypeArgs(std::move(type_args));
        }
        deref();
        val = fr;
        return parse_init_same_level(fr, val, parse_context);
    };

    if (!qc) {
        // see if this is a function call to a function defined in a namespace
        const FunctionEntry* fe = qore_root_ns_private::parseResolveFunctionEntry(*scope);
        if (fe) {
            return make_function_ref(fe);
        }
        if (qore_aot_source_parse_active() && scope->size() >= 2) {
            if (QoreProgram* pgm = parse_context.pgm ? parse_context.pgm : getProgram()) {
                if (defer_source_function) {
                    qore_aot_record_source_parse_call_import(pgm, loc,
                        deferred_source_function_path.c_str(), false);
                } else {
                    if (defer_source_static_receiver) {
                        std::string method_path = deferred_source_receiver_path;
                        method_path += "::";
                        method_path += scope->getIdentifier();
                        qore_aot_record_source_parse_call_import(pgm, loc,
                            method_path.c_str(), true);
                    } else {
                        qore_aot_record_source_parse_call_import(pgm, loc, scope->ostr, true);
                    }
                }
            }
            if (defer_source_function) {
                val = new DeferredFunctionCallReferenceNode(loc, deferred_source_function_path.c_str());
                deref();
                return 0;
            }
            val = new DeferredStaticMethodCallReferenceNode(loc,
                defer_source_static_receiver ? deferred_source_receiver_path.c_str() : source_receiver_path.c_str(),
                scope->getIdentifier());
            deref();
            return 0;
        }
        parse_error(*loc, "reference to undefined class '%s' in '%s()'", scope->get(scope->size() - 2), scope->ostr);
        return -1;
    }

    qore_class_private* class_ctx = parse_context.oflag
        ? qore_class_private::get(*const_cast<QoreClass*>(
            QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()))
        )
        : parse_get_class_priv();
    if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*qc, class_ctx)) {
        class_ctx = nullptr;
    }

    const QoreMethod* qm = nullptr;
    // try to find a pointer to a non-static method if parsing in the class's context
    // and bare references are enabled
    bool bare_method = parse_context.oflag && parse_check_parse_option(PO_ALLOW_BARE_REFS);
    if (bare_method) {
        qm = qore_class_private::get(*qc)->parseFindAnyMethodStaticFirst(scope->getIdentifier(), class_ctx);
        //assert(!qm || !qm->isStatic());

        if (qm && !qm->isStatic() && !strcmp(qm->getName(), "copy")) {
            parseException(*loc, "INVALID-METHOD", "cannot take a reference to base class copy method %s::%s()",
                qc->getName(), scope->getIdentifier());
            return -1;
        }
    } else {
        qm = qore_class_private::get(*qc)->parseFindStaticMethod(scope->getIdentifier(), class_ctx);
    }

    if (!qm && scope->size() == 2) {
        // the first class with the name has no such method: as for calls, a function of a namespace with the name,
        // a method of the class being parsed or of a parent class with the name (with bare references), and a static
        // method of another class with the name (issue #1527)
        qore_class_private* ref_class_ctx = parse_context.oflag
            ? qore_class_private::get(*const_cast<QoreClass*>(
                QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo())))
            : parse_get_class_priv();
        if (const FunctionEntry* fe = qore_root_ns_private::parseResolveFunctionEntry(*scope)) {
            return make_function_ref(fe);
        }
        // a call with the name would call a closure or call reference in a constant or static variable of a class
        // with the name: the reference keeps the error rather than reach another target than the call
        const QoreTypeInfo* const_type = nullptr;
        bool const_found = false;
        ValueHolder const_val(defer_source_function ? QoreValue()
            : qore_root_ns_private::parseFindReferencedConstantValue(loc, *scope, const_type, const_found, false),
            nullptr);
        const QoreMethod* other_method = nullptr;
        QoreClass* other_qc = nullptr;
        // a function of another source of an AOT group is resolved at link time
        if (!defer_source_function && !const_found) {
            if (bare_method) {
                other_qc = qore_root_ns_private::parseFindParentClassWithMethod(scope->get(0), scope->getIdentifier(),
                    ref_class_ctx, other_method);
            }
            if (!other_qc) {
                other_qc = qore_root_ns_private::parseFindClassWithStaticMethod(scope->get(0),
                    scope->getIdentifier(), ref_class_ctx, &other_method);
            }
        }
        if (other_qc && other_method) {
            qc = other_qc;
            qm = other_method;
            class_ctx = ref_class_ctx && qore_class_private::parseCheckPrivateClassAccess(*qc, ref_class_ctx)
                ? ref_class_ctx : nullptr;
        }
    }

    if (!qm) {
        if (bare_method) {
            parseException(*loc, "INVALID-METHOD", "class '%s' has no accessible method '%s'", qc->getName(),
                scope->getIdentifier());
        } else {
            parseException(*loc, "INVALID-METHOD", "class '%s' has no accessible static method '%s'", qc->getName(),
                scope->getIdentifier());
        }
        return -1;
    }

    // check class capabilities against parse options
    if (qore_program_private::parseAddDomain(parse_context.pgm, qc->getDomain())) {
        parse_error(*loc, "class '%s' implements capabilities that are not allowed by current parse options",
            qc->getName());
        return -1;
    }

    // check the functional domains of the method: a reference allows the call that naming the method would
    if (int err = check_call_reference_domain(loc, parse_context, qm)) {
        return err;
    }

    //printd(5, "UnresolvedStaticMethodCallReferenceNode::parseInit() got %s::%s() static: %d\n",
    //  qm->getClass()->getName(), qm->getName(), qm->isStatic());
    if (qm->isStatic()) {
        // a reference to a static method of a generic class through a class that inherits a specialization of it
        // calls it with the type arguments of the specialization, also through the generic class in the code of a
        // class that inherits it, whose class the reference then names (qore#6285)
        // (only if the name resolves to the same method in it, as the class is all that compiled code records of
        // the reference)
        const qore_class_private* code_class = parse_get_class_priv();
        bool current_receiver;
        const QoreClass* ref_cls = qc;
        // (only in the code of a class, and for a method of a generic class: a method of a class that is not generic
        // has a receiver for the variants that it adds to a generic base class's method also in top-level code)
        if (code_class && qm->getClass()->hasTypeParameters()
                && qore_class_private::parseGetStaticCallReceiverTypeInfo(*qm, qc, code_class, current_receiver)
                && qore_class_private::get(*qc)->hasTypeParams()
                && const_cast<qore_class_private*>(code_class)->parseFindStaticMethod(scope->getIdentifier(),
                    code_class) == qm) {
            ref_cls = code_class->cls;
        }
        val = new LocalStaticMethodCallReferenceNode(loc, qm, ref_cls);
    } else {
        val = new LocalMethodCallReferenceNode(loc, qm);
    }
    deref();
    return 0;
}

LocalFunctionCallReferenceNode::LocalFunctionCallReferenceNode(const QoreProgramLocation* loc,
        const QoreFunction* n_uf, bool n_needs_eval) : ResolvedCallReferenceNodeIntern(loc, n_needs_eval), uf(n_uf) {
}

LocalFunctionCallReferenceNode::LocalFunctionCallReferenceNode(const QoreProgramLocation* loc,
        const QoreFunction* n_uf) : ResolvedCallReferenceNodeIntern(loc, true), uf(n_uf) {
}

void LocalFunctionCallReferenceNode::setExplicitTypeArgs(std::vector<const QoreTypeInfo*>&& type_args) {
    if (type_args.empty()) {
        explicit_inst.reset();
        return;
    }
    explicit_inst = std::make_shared<QoreTypeParamInstantiation>();
    explicit_inst->type_args = std::move(type_args);
}

QoreValue LocalFunctionCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue LocalFunctionCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    QoreProgram* call_pgm = rc.getProgram() ? rc.getProgram() : ::getProgram();
    FunctionCallReferenceNode* rv = new FunctionCallReferenceNode(loc, uf, call_pgm);
    // a reference without explicit type arguments (the common case) pays nothing for them
    if (explicit_inst) {
        rv->explicit_inst = explicit_inst;
        // an explicit type argument can be a type parameter of the generic code that creates the reference (\mk0<U>()
        // in a generic function or a method of a generic class): it is bound here, to the instantiation that is
        // running, as the reference can outlive that code and be called when no such instantiation exists (the same
        // as bindExplicitTypeArgs(), written out: a call of it here measured +44 instructions)
        QoreTypeParamInstantiation resolved;
        if (qore_resolve_explicit_type_param_instantiation(*explicit_inst, resolved)) {
            rv->explicit_inst = std::make_shared<QoreTypeParamInstantiation>(std::move(resolved));
        }
        // the reference can outlive the Program of a type argument (issue #6489)
        rv->holdExplicitTypeArgDecls();
    }
    return rv;
}

void LocalFunctionCallReferenceNode::bindExplicitTypeArgs() {
    if (explicit_inst) {
        QoreTypeParamInstantiation resolved;
        if (qore_resolve_explicit_type_param_instantiation(*explicit_inst, resolved)) {
            explicit_inst = std::make_shared<QoreTypeParamInstantiation>(std::move(resolved));
        }
    }
}

QoreValue LocalFunctionCallReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return uf->evalFunction(0, args, nullptr, rc, xsink, explicit_inst.get());
}

QoreValue LocalFunctionCallReferenceNode::execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // as dispatchValue(): the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return uf->evalFunctionTmpArgs(nullptr, args, nullptr, rc, xsink, explicit_inst.get());
}

QoreValue LocalFunctionCallReferenceNode::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return uf->evalFunctionNamed(args, nullptr, rc, xsink, explicit_inst.get(), 0, arg_names, policy);
}

QoreValue LocalFunctionCallReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // Compatibility semantics do not change the permissions of the reference's creator.
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return uf->evalFunction(0, args, nullptr, rc, xsink, explicit_inst.get(), false, policy);
}

bool LocalFunctionCallReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const LocalFunctionCallReferenceNode* vc = dynamic_cast<const LocalFunctionCallReferenceNode*>(v);
    //printd(5, "LocalFunctionCallReferenceNode::is_equal_hard() %p == %p (%p %s)\n", uf, vc ? vc->uf : 0, v,
    //  v ? v->getTypeName() : "n/a");
    return vc && uf == vc->uf;
}

bool FunctionCallReferenceNode::derefImpl(ExceptionSink* xsink) {
    //printd(5, "FunctionCallReferenceNode::deref() this: %p pgm: %p refs: %d -> %d\n", this, pgm, reference_count(),
    //  reference_count() - 1);
    // the function can outlive the Program that defined it (see the constructor)
    const_cast<QoreFunction*>(uf)->deref();
    pgm->depDeref();
    return ResolvedCallReferenceNode::derefImpl(xsink);
}

QoreValue FunctionCallReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return uf->evalFunction(0, args, pgm, rc, xsink, explicit_inst.get());
}

QoreValue FunctionCallReferenceNode::execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // as dispatchValue(): the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return uf->evalFunctionTmpArgs(nullptr, args, pgm, rc, xsink, explicit_inst.get());
}

QoreValue FunctionCallReferenceNode::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return uf->evalFunctionNamed(args, pgm, rc, xsink, explicit_inst.get(), 0, arg_names, policy);
}

QoreValue FunctionCallReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    if (check_background_creator(this, xsink)) {
        return QoreValue();
    }
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // Compatibility semantics do not change the permissions of the reference's creator.
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    return uf->evalFunction(0, args, pgm, rc, xsink, explicit_inst.get(), false, policy);
}

DeferredFunctionCallReferenceNode::DeferredFunctionCallReferenceNode(const QoreProgramLocation* loc,
        const char* n_function_name)
        : ResolvedCallReferenceNodeIntern(loc, true), function_name(n_function_name ? n_function_name : "") {
}

bool DeferredFunctionCallReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const DeferredFunctionCallReferenceNode* d = dynamic_cast<const DeferredFunctionCallReferenceNode*>(v);
    return d && function_name == d->function_name;
}

FunctionCallReferenceNode* DeferredFunctionCallReferenceNode::resolve(RuntimeConfig& rc, ExceptionSink* xsink) const {
    if (function_name.empty()) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot resolve function call reference '%s()'",
            function_name.c_str());
        return nullptr;
    }

    QoreProgram* pgm = rc.getProgram() ? rc.getProgram() : getProgram();
    if (!pgm) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve function call reference '%s()': no program context", function_name.c_str());
        return nullptr;
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(*pp->RootNS, function_name.c_str());
    QoreFunction* func = fe ? fe->getFunction(true) : nullptr;
    if (!func) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot resolve function call reference '%s()'",
            function_name.c_str());
        return nullptr;
    }

    QoreProgram* call_pgm = rc.getProgram() ? rc.getProgram() : pgm;
    return new FunctionCallReferenceNode(loc, func, call_pgm);
}

QoreValue DeferredFunctionCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue DeferredFunctionCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref,
        ExceptionSink* xsink) const {
    assert(needs_deref);
    return resolve(rc, xsink);
}

QoreValue DeferredFunctionCallReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    FunctionCallReferenceNode* ref = resolve(rc, xsink);
    if (!ref) {
        return QoreValue();
    }
    QoreValue rv = ref->invoke(args, xsink);
    ref->deref(xsink);
    return rv;
}

QoreValue DeferredFunctionCallReferenceNode::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call runs with the permissions of the context that created the reference
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    FunctionCallReferenceNode* ref = resolve(rc, xsink);
    if (!ref) {
        return QoreValue();
    }
    QoreValue rv = ref->dispatchNamed(args, arg_names, policy, xsink);
    ref->deref(xsink);
    return rv;
}

QoreValue DeferredFunctionCallReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    QoreSandboxProvenanceHelper provenance(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // Compatibility semantics do not change the permissions of the reference's creator.
    QoreCallReferenceCallHelper call_ref_call(rc, args, getCreatorParseOptions());
    FunctionCallReferenceNode* ref = resolve(rc, xsink);
    if (!ref) {
        return QoreValue();
    }
    QoreValue rv = qore_resolved_call_reference_node_ops(ref).invokeLexical(args, xsink, policy);
    ref->deref(xsink);
    return rv;
}

ResolvedCallReferenceNode::ResolvedCallReferenceNode() : AbstractCallReferenceNode(false, NT_FUNCREF) {
    qore_capture_call_reference_provenance(this);
}

ResolvedCallReferenceNode::ResolvedCallReferenceNode(bool n_needs_eval, qore_type_t n_type)
        : AbstractCallReferenceNode(n_needs_eval, n_type) {
    if (!n_needs_eval) {
        qore_capture_call_reference_provenance(this);
    }
}

ResolvedCallReferenceNode::~ResolvedCallReferenceNode() {
    qore_release_call_reference_provenance(this);
}

QoreValue ResolvedCallReferenceNode::invoke(const QoreListNode* args, ExceptionSink* xsink) const {
    QoreSandboxProvenanceHelper provenance(this);
    return dispatchValue(args, xsink);
}

QoreValue qore_resolved_call_reference_node_ops::invokeLexicalImpl(ResolvedCallReferenceNode* b, const QoreListNode* args, ExceptionSink* xsink, const LexicalSemanticPolicy& policy) {
    QoreSandboxProvenanceHelper provenance(b);
    return b->dispatchLexicalValue(args, xsink, policy);
}

namespace {
//! Only external/native callbacks use this scope; Qore entries clear policy in their existing CEH flags store.
class NativeCallbackPolicyScope {
public:
    explicit NativeCallbackPolicyScope(LexicalSemanticPolicy policy)
            : rc(rc_get_tls_ref()), saved(QoreRuntimeFlags::get(rc)) {
        QoreRuntimeFlags::set(rc, (saved & ~LexicalSemanticPolicy::NativeMask) | policy.nativeFlags());
    }

    ~NativeCallbackPolicyScope() {
        QoreRuntimeFlags::set(rc, saved);
    }

private:
    RuntimeConfig& rc;
    q_rt_flags_t saved;
};
}

QoreValue ResolvedCallReferenceNode::dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const {
    NativeCallbackPolicyScope policy({});
    return execValue(args, xsink);
}

QoreValue ResolvedCallReferenceNode::dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const {
    NativeCallbackPolicyScope invocation(policy);
    return execValue(args, xsink);
}

QoreValue ResolvedCallReferenceNodeIntern::dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
        LexicalSemanticPolicy policy, ExceptionSink* xsink) const {
    xsink->raiseException("NAMED-CALL-NOT-SUPPORTED", "a call through a %s cannot take named arguments; use "
        "positional arguments", getTypeName());
    return QoreValue();
}

namespace {
//! A fixed callback, bound at registration time; native subclasses still implement execValue().

class SandboxBoundCallReference : public ResolvedCallReferenceNode {
public:
    //! calls the callback with named arguments as dispatchValue() calls it with positional arguments
    QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names, LexicalSemanticPolicy policy,
            ExceptionSink* xsink) const {
        QoreSandboxProvenanceHelper provenance(this);
        if (check_stack(xsink)) {
            return QoreValue();
        }
        if (trusted) {
            // the arguments are evaluated in the caller's context (see dispatchValue())
            QoreListNodeEvalOptionalRefHolder evaluated(args, xsink);
            if (*xsink) {
                return QoreValue();
            }
            // a trusted build's barrier hides the caller; it never hides the current Program, which is the Program
            // that created the capability once it has been entered, so that Program's sandbox governs the build
            std::optional<QoreModuleInitSandboxBarrierHelper> build_barrier;
            std::optional<QoreTrustedBuildChargeHelper> build_charge;
            if (enterBuild(build_barrier, build_charge, xsink)) {
                return QoreValue();
            }
            ProgramThreadCountContextHelper registration_context(xsink, trusted_program, true);
            if (*xsink) {
                return QoreValue();
            }
            // the caller's policy is masked for a system-policy callback; a trusted build's barrier hides it already
            std::optional<QoreSandboxTrustedPolicyHelper> privilege;
            if (!build) {
                privilege.emplace();
            }
            return qore_call_ref_invoke_named(callback, *evaluated, arg_names, policy, xsink);
        }
        return qore_call_ref_invoke_named(callback, args, arg_names, policy, xsink);
    }

    QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const override {
        return dispatchValue(args, xsink);
    }

    //! a binding: \a trusted for a capability (SandboxManager::createSystemPolicyCall()), \a build for a trusted
    //! build capability as well (SandboxManager::createTrustedBuildCall())
    SandboxBoundCallReference(const ResolvedCallReferenceNode* callback, bool trusted, bool build = false)
            : callback(const_cast<ResolvedCallReferenceNode*>(callback)), trusted(trusted), build(build),
                trusted_program(trusted ? ::getProgram() : nullptr) {
        assert(!build || trusted);
        callback->ref();
        if (trusted_program) {
            trusted_program->depRef();
        }
    }

    QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const override {
        QoreSandboxProvenanceHelper provenance(this);
        if (check_stack(xsink)) {
            return QoreValue();
        }
        if (trusted) {
            // the arguments are expressions of the caller: they are evaluated in the caller's context, before the
            // Program that registered the capability is entered, as the arguments of any call into another Program
            // are; evaluated there, the caller's local variables are not found
            QoreListNodeEvalOptionalRefHolder evaluated(args, xsink);
            if (*xsink) {
                return QoreValue();
            }
            // a trusted build's barrier hides the caller; it never hides the current Program, which is the Program
            // that created the capability once it has been entered, so that Program's sandbox governs the build
            std::optional<QoreModuleInitSandboxBarrierHelper> build_barrier;
            std::optional<QoreTrustedBuildChargeHelper> build_charge;
            if (enterBuild(build_barrier, build_charge, xsink)) {
                return QoreValue();
            }
            ProgramThreadCountContextHelper registration_context(xsink, trusted_program, true);
            if (*xsink) {
                return QoreValue();
            }
            // the caller's policy is masked for a system-policy callback; a trusted build's barrier hides it already
            std::optional<QoreSandboxTrustedPolicyHelper> privilege;
            if (!build) {
                privilege.emplace();
            }
            return callback->invoke(*evaluated, xsink);
        }
        return callback->invoke(args, xsink);
    }

    QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
            const LexicalSemanticPolicy& policy) const override {
        QoreSandboxProvenanceHelper provenance(this);
        if (check_stack(xsink)) {
            return QoreValue();
        }
        if (trusted) {
            // the arguments are expressions of the caller: they are evaluated in the caller's context, before the
            // Program that registered the capability is entered, as the arguments of any call into another Program
            // are; evaluated there, the caller's local variables are not found
            QoreListNodeEvalOptionalRefHolder evaluated(args, xsink);
            if (*xsink) {
                return QoreValue();
            }
            // a trusted build's barrier hides the caller; it never hides the current Program, which is the Program
            // that created the capability once it has been entered, so that Program's sandbox governs the build
            std::optional<QoreModuleInitSandboxBarrierHelper> build_barrier;
            std::optional<QoreTrustedBuildChargeHelper> build_charge;
            if (enterBuild(build_barrier, build_charge, xsink)) {
                return QoreValue();
            }
            ProgramThreadCountContextHelper registration_context(xsink, trusted_program, true);
            if (*xsink) {
                return QoreValue();
            }
            // the caller's policy is masked for a system-policy callback; a trusted build's barrier hides it already
            std::optional<QoreSandboxTrustedPolicyHelper> privilege;
            if (!build) {
                privilege.emplace();
            }
            return qore_resolved_call_reference_node_ops(callback).invokeLexical(*evaluated, xsink, policy);
        }
        return qore_resolved_call_reference_node_ops(callback).invokeLexical(args, xsink, policy);
    }

    QoreFunction* getFunction() const override { return callback->getFunction(); }
    QoreProgram* getProgram() const override { return callback->getProgram(); }
    const ResolvedCallReferenceNode* getCallback() const { return callback; }
    const ResolvedCallReferenceNode* getSandboxBoundCallback() const override { return callback; }
    bool isTrusted() const { return trusted; }
    bool isBuild() const { return build; }

protected:
    bool derefImpl(ExceptionSink* xsink) override {
        QoreSandboxProvenanceHelper provenance(this);
        callback->deref(xsink);
        if (trusted_program) {
            trusted_program->depDeref();
        }
        return ResolvedCallReferenceNode::derefImpl(xsink);
    }

private:
    ResolvedCallReferenceNode* const callback;
    const bool trusted;
    //! a trusted build: the callback runs under a construction barrier (see SandboxManager::createTrustedBuildCall())
    const bool build;
    QoreProgram* const trusted_program;

    //! Establishes the construction barrier of a trusted build, if this is one
    /** Under it nothing of the caller below it is seen: what the callback creates or commits keeps no sandbox of the
        caller (see qore_program_private::creator_sandbox_manager), and the threads that it starts inherit none; the
        provenance of the objects and callbacks of the caller that it uses still applies to them

        @return 0 for OK, -1 if the barrier could not be recorded (an out-of-memory exception is raised)
    */
    int enterBuild(std::optional<QoreModuleInitSandboxBarrierHelper>& barrier,
            std::optional<QoreTrustedBuildChargeHelper>& charge, ExceptionSink* xsink) const {
        if (!build) {
            return 0;
        }
        // the sandboxes that govern the caller, resolved before the barrier hides them: the code that they govern is
        // the caller's code in the build (QoreSandboxBudgetStack::suspendCharge())
        QoreSandboxManager* callers = qore_find_thread_sandbox_manager_ref();
        barrier.emplace(QoreTrustedBuildTag{trusted_program});
        if (!barrier->recorded()) {
            if (callers) {
                callers->deref(nullptr);
            }
            xsink->outOfMemory();
            return -1;
        }
        // the memory that the build's own code allocates is shared state, charged to no caller but to the sandbox of
        // the Program that created the capability, if it has a memory limit; the caller's code that it calls stays
        // charged to the caller (QoreSandboxBudgetStack::suspendCharge()); the floor is set before the creator is
        // entered, so that the creator's budgets are above it
        charge.emplace(trusted_program, callers);
        if (!charge->established()) {
            xsink->outOfMemory();
            return -1;
        }
        return 0;
    }
};
}

const ResolvedCallReferenceNode* qore_get_sandbox_bound_callback(const AbstractQoreNode* callback) {
    if (!callback || callback->getType() != NT_FUNCREF) {
        return nullptr;
    }
    return static_cast<const AbstractCallReferenceNode*>(callback)->getSandboxBoundCallback();
}

ResolvedCallReferenceNode* qore_bind_sandbox_call(const ResolvedCallReferenceNode* callback, bool trusted,
        ExceptionSink* xsink) {
    if (trusted) {
        // any governing sandbox, whatever policy barrier is active: a capability is registered by trusted code only
        QoreSandboxManagerHelper governing(QoreSandboxManagerHelper::All);
        if (governing) {
            xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "a system-policy callback must be registered "
                "outside sandbox execution and must not have sandbox provenance");
            return nullptr;
        }
        // Do not accumulate forwarding chains when an already bound callback is registered again.
        // Every binding's provenance must be checked before unwrapping it.
        while (true) {
            auto* creation = qore_get_call_reference_provenance(callback);
            if (creation) {
                creation->deref();
                xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "a system-policy callback must not have "
                    "sandbox provenance");
                return nullptr;
            }
            const auto* bound = dynamic_cast<const SandboxBoundCallReference*>(callback);
            if (!bound) {
                break;
            }
            if (bound->isBuild()) {
                xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "a trusted build capability cannot be registered as "
                    "a system-policy callback");
                return nullptr;
            }
            if (bound->isTrusted()) {
                return bound->refRefSelf();
            }
            callback = bound->getCallback();
        }
    } else if (const auto* bound = dynamic_cast<const SandboxBoundCallReference*>(callback)) {
        if (!bound->isTrusted()) {
            // Bind repeatedly without building a recursive forwarding chain; retain all prior provenance.
            QoreSandboxProvenanceHelper previous(callback);
            return new SandboxBoundCallReference(bound->getCallback(), false);
        }
    }
    return new SandboxBoundCallReference(callback, trusted);
}

namespace {
//! A capability that starts a fixed callback in a new thread that no sandbox governs
/** See SandboxManager::createSystemThreadCall(); the callback and its Program are fixed when the capability is
    created, and a call of the capability takes no arguments
*/
class SystemThreadCallReference : public ResolvedCallReferenceNode {
public:
    QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const override {
        return dispatchValue(args, xsink);
    }

    SystemThreadCallReference(const ResolvedCallReferenceNode* callback, QoreProgram* pgm)
            : callback(callback->refRefSelf()), pgm(pgm) {
        pgm->depRef();
    }

    QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const override {
        // the capability starts only the fixed callback: nothing of the caller reaches the thread
        if (args && !args->empty()) {
            xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "a system thread capability takes no arguments");
            return QoreValue();
        }
        int tid = qore_start_system_thread(callback, pgm, xsink);
        return tid < 0 ? QoreValue() : QoreValue(static_cast<int64>(tid));
    }

    QoreFunction* getFunction() const override { return callback->getFunction(); }
    QoreProgram* getProgram() const override { return callback->getProgram(); }

protected:
    bool derefImpl(ExceptionSink* xsink) override {
        callback->deref(xsink);
        pgm->depDeref();
        return ResolvedCallReferenceNode::derefImpl(xsink);
    }

private:
    ResolvedCallReferenceNode* const callback;
    //! the Program that created the capability, in which the thread runs; with a dependency reference
    QoreProgram* const pgm;
};
}

ResolvedCallReferenceNode* qore_create_trusted_build_call(const ResolvedCallReferenceNode* callback,
        ExceptionSink* xsink) {
    // code that any sandbox governs, whatever policy barrier is active, and sandboxed code that no manager governs (a
    // Program sandboxed by its flag): the capability removes the caller's sandbox from what its callback builds
    if (qore_caller_is_sandboxed()) {
        xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "a trusted build capability cannot be created by code "
            "that a sandbox governs or that is sandboxed");
        return nullptr;
    }
    if (QoreSandboxContext* creation = qore_get_call_reference_provenance(callback)) {
        creation->deref();
        xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "the callback of a trusted build capability must not have "
            "sandbox provenance");
        return nullptr;
    }
    if (qore_get_sandbox_bound_callback(callback) || dynamic_cast<const SystemThreadCallReference*>(callback)) {
        xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "the callback of a trusted build capability must be a plain "
            "callback, not a bound callback or a capability");
        return nullptr;
    }
    // the code of the callback must not be sandboxed code either (a function of a Program that sandboxed code
    // created, imported into a trusted Program)
    QoreParseOptions code_po;
    bool code_sandboxed;
    int rc = qore_call_reference_code_parse_options(callback, code_po, xsink, &code_sandboxed);
    if (*xsink) {
        return nullptr;
    }
    if (rc < 0) {
        xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "the Program of the callback of a trusted build capability "
            "has been deleted");
        return nullptr;
    }
    if (rc > 0 && code_sandboxed) {
        xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "the callback of a trusted build capability belongs to a "
            "sandboxed Program");
        return nullptr;
    }
    return new SandboxBoundCallReference(callback, true, true);
}

ResolvedCallReferenceNode* qore_create_system_thread_call(const ResolvedCallReferenceNode* callback,
        ExceptionSink* xsink) {
    // any governing sandbox, whatever policy barrier is active: the capability escapes every sandbox
    {
        QoreSandboxManagerHelper governing(QoreSandboxManagerHelper::All);
        if (governing) {
            xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "a system thread capability cannot be created by code "
                "that a sandbox governs");
            return nullptr;
        }
    }
    if (QoreSandboxContext* creation = qore_get_call_reference_provenance(callback)) {
        creation->deref();
        xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "the callback of a system thread capability must not have "
            "sandbox provenance");
        return nullptr;
    }
    if (qore_get_sandbox_bound_callback(callback) || dynamic_cast<const SystemThreadCallReference*>(callback)) {
        xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "the callback of a system thread capability must be a plain "
            "callback, not a bound callback or a capability");
        return nullptr;
    }
    if (qore_exit_hooks_started()) {
        xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "a system thread capability cannot be created after the exit "
            "hooks of the process have started");
        return nullptr;
    }
    // trusted initialization: the module's init, before any code it serves runs
    if (!get_module_context_path()) {
        xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "a system thread capability can only be created while a "
            "module is being loaded");
        return nullptr;
    }
    QoreProgram* pgm = ::getProgram();
    assert(pgm);
    return new SystemThreadCallReference(callback, pgm);
}

QoreProgram* ResolvedCallReferenceNode::getProgram() const {
    return nullptr;
}

int ResolvedCallReferenceNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    return 0;
}

bool ResolvedCallReferenceNode::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const {
    return this == v;
}

bool ResolvedCallReferenceNode::is_equal_soft(const AbstractQoreNode *v, ExceptionSink *xsink) const {
    return this == v;
}

QoreValue ResolvedCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ResolvedCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    needs_deref = false;
    return this;
}

AbstractQoreNode* ResolvedCallReferenceNode::realCopy() const {
    return refRefSelf();
}

QoreValue qore_call_ref_invoke_named(const ResolvedCallReferenceNode* r, const QoreListNode* args,
        const name_vec_t& arg_names, LexicalSemanticPolicy policy, ExceptionSink* xsink) {
    if (const ResolvedCallReferenceNodeIntern* ri = dynamic_cast<const ResolvedCallReferenceNodeIntern*>(r)) {
        return ri->dispatchNamed(args, arg_names, policy, xsink);
    }
    if (const QoreClosureBase* cb = dynamic_cast<const QoreClosureBase*>(r)) {
        return cb->dispatchNamed(args, arg_names, xsink);
    }
    if (const SandboxBoundCallReference* sb = dynamic_cast<const SandboxBoundCallReference*>(r)) {
        return sb->dispatchNamed(args, arg_names, policy, xsink);
    }
    // a call reference implemented outside of the library takes its arguments by position
    xsink->raiseException("NAMED-CALL-NOT-SUPPORTED", "a call through a %s cannot take named arguments; use "
        "positional arguments", r->getTypeName());
    return QoreValue();
}

QoreParseOptions qore_program_authored_restrictions_po(const QoreProgram* pgm) {
    static const QoreParseOptions restricting = QoreParseOptions(QDOM_ALL) & ~QoreParseOptions::POSITIVE_OPTIONS
        & ~QoreParseOptions(PO_IN_MODULE);
    return qore_non_module_program_restrictions(pgm) & restricting;
}
