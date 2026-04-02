/* -*- indent-tabs-mode: nil -*- */
/*
    Function.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/QoreListNodeEvalOptionalRefHolder.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreIRBuilder.h"
#include <qore/intern/VarRefNode.h>
#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRInterpreter.h"
#include "qore/intern/QoreIRVerifier.h"
#include "qore/intern/QoreJIT.h"
#include "qore/intern/IfStatement.h"
#include "qore/intern/ForStatement.h"
#include "qore/intern/ForEachStatement.h"
#include "qore/intern/WhileStatement.h"
#include "qore/intern/TryStatement.h"
#include "qore/intern/SwitchStatement.h"
#include "qore/intern/DebugStatement.h"
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/QoreAOT.h"
#include "qore/intern/FunctionCallNode.h"

#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <pthread.h>

static void duplicateSignatureException(const char* cname, const char* name, const UserSignature* sig) {
    parseException(*sig->getParseLocation(), "DUPLICATE-SIGNATURE", "%s%s%s(%s) has already been declared",
        cname ? cname : "", cname ? "::" : "", name, sig->getSignatureText());
}

static void ambiguousDuplicateSignatureException(const char* cname, const char* name,
        const AbstractFunctionSignature* sig1, const UserSignature* sig2) {
    parseException(*sig2->getParseLocation(), "DUPLICATE-SIGNATURE", "%s%s%s(%s) matches already declared variant " \
        "%s(%s)", cname ? cname : "", cname ? "::" : "", name, sig2->getSignatureText(), name,
        sig1->getSignatureText());
}

QoreFunction* IList::getFunction(const qore_class_private* class_ctx, const qore_class_private*& last_class,
    const_iterator aqfi, bool& internal_access, bool& stop) const {
    stop = internal_access && (*aqfi).access == Internal;

    QoreFunction* rv = (!last_class || ((*aqfi).access == Public) || stop
                        || (class_ctx && (*aqfi).access == Private)) ? (*aqfi).func : nullptr;

    if (rv) {
        const QoreClass* fc = rv->getClass();
        if (fc) {
            // get the function's class
            last_class = qore_class_private::get(*fc);
            if (last_class && class_ctx) {
                // set the internal access flag
                internal_access = last_class->equal(*class_ctx);
            }
        }
    }

    return rv;
}

bool AbstractFunctionSignature::compare(const AbstractFunctionSignature& sig, bool relaxed_match) const {
    // check varargs flags first
    if (varargs != sig.varargs) {
        //printd(5, "AbstractFunctionSignature::compare() varargs: %d sig.varargs: %d\n", varargs, sig.varargs);
        return false;
    }

    if (num_param_types != sig.num_param_types || min_param_types != sig.min_param_types) {
        //printd(5, "AbstractFunctionSignature::compare() pt: %d != %d || mpt %d != %d\n", num_param_types,
        //    sig.num_param_types, min_param_types, sig.min_param_types);
        return false;
    }

    // return types for abstract methods must be compatible if present
    if (sig.returnTypeInfo != nothingTypeInfo) {
        bool may_not_match = false;
        qore_type_result_e res = QoreTypeInfo::parseAccepts(sig.returnTypeInfo, returnTypeInfo, may_not_match);
        if (!res || (may_not_match && !relaxed_match
                // auto/any return types always accept any concrete return type
                && sig.returnTypeInfo != autoTypeInfo
                && sig.returnTypeInfo != anyTypeInfo)) {
            //printd(5, "AbstractFunctionSignature::compare() rt: %s is not compatible with %s (%p %p)\n",
            //    QoreTypeInfo::getName(returnTypeInfo), QoreTypeInfo::getName(sig.returnTypeInfo), returnTypeInfo,
            //    sig.returnTypeInfo);
            return false;
        }
    }

    for (unsigned i = 0; i < typeList.size(); ++i) {
        const QoreTypeInfo* ti = sig.typeList.size() <= i
            ? nullptr
            : sig.typeList[i];
        bool match;
        if (relaxed_match) {
            //printd(5, "AbstractFunctionSignature::compare() param %d (%s =~ %s) %d\n", i,
            //    QoreTypeInfo::getName(typeList[i]), QoreTypeInfo::getName(ti),
            //    QoreTypeInfo::parseAccepts(typeList[i], ti));
            match = QoreTypeInfo::parseAccepts(typeList[i], ti) >= QTI_WILDCARD;
        } else {
            //printd(5, "AbstractFunctionSignature::compare() param %d (%s =~ %s) %d\n", i,
            //    QoreTypeInfo::getName(typeList[i]), QoreTypeInfo::getName(ti),
            //    QoreTypeInfo::runtimeTypeMatch(typeList[i], ti));
            match = QoreTypeInfo::runtimeTypeMatch(typeList[i], ti) >= QTI_NEAR;
        }

        if (!match) {
            //printd(5, "AbstractFunctionSignature::compare() param %d %s != %s\n", i,
            //    QoreTypeInfo::getName(typeList[i]), QoreTypeInfo::getName(ti));
            return false;
        }
    }

    //printd(5, "AbstractFunctionSignature::compare() rv: '%s' '%s' == rv: '%s' '%s' TRUE\n",
    //    QoreTypeInfo::getName(returnTypeInfo), str.c_str(), QoreTypeInfo::getName(sig.returnTypeInfo),
    //    sig.str.c_str());
    return true;
}

QoreParseOptions AbstractQoreFunctionVariant::getParseOptions(const QoreParseOptions& po) const {
    return is_user ? getUserVariantBase()->getParseOptions(po) : po;
}

void AbstractQoreFunctionVariant::parseResolveUserSignature() {
    UserVariantBase* uvb = getUserVariantBase();
    if (uvb)
        uvb->getUserSignature()->resolve();
}

bool AbstractQoreFunctionVariant::hasBody() const {
    return is_user ? getUserVariantBase()->hasBody() : true;
}

LocalVar* AbstractQoreFunctionVariant::getSelfId() const {
    const UserVariantBase* uvb = getUserVariantBase();
    if (!uvb) {
        return nullptr;
    }
    return uvb->getUserSignature()->getSelfId();
}

static void do_call_name(QoreString &desc, const QoreFunction* func) {
    const char* class_name = func->className();
    if (class_name)
        desc.sprintf("%s::", class_name);
    desc.sprintf("%s(", func->getName());
}

static void add_args(QoreStringNode &desc, const QoreListNode* args) {
    if (!args || !args->size())
        return;

    for (unsigned i = 0; i < args->size(); ++i) {
        const QoreValue n = args->retrieveEntry(i);
        QoreString scratch;
        const char* tname = n.getFullTypeName(true, scratch);
        desc.concat(tname);
        if (i != (args->size() - 1))
            desc.concat(", ");
    }
}

CodeEvaluationHelper::CodeEvaluationHelper(ExceptionSink* n_xsink, RuntimeConfig& n_rc, const QoreFunction* func,
        const AbstractQoreFunctionVariant*& variant, const char* n_name, const QoreListNode* args, QoreObject* self,
        const qore_class_private* n_qc, qore_call_t n_ct, bool is_copy, const qore_class_private* cctx,
        QoreProgram* pgm_ctx)
    : ct(n_ct), name(n_name), xsink(n_xsink), rc(n_rc), qc(n_qc),
        loc(get_runtime_location()),
        tmp(n_xsink), returnTypeInfo((const QoreTypeInfo*)-1) {
    if (self && !self->isValid()) {
        assert(n_qc);
        xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has already been " \
            "deleted", qc->name.c_str(), func->getName());
        return;
    }

    setCallName(func);
    tmp.assignEval(args);
    if (*xsink) {
        return;
    }

    init(func, variant, is_copy, cctx, self, pgm_ctx);
}

CodeEvaluationHelper::CodeEvaluationHelper(ExceptionSink* n_xsink, RuntimeConfig& n_rc, const QoreFunction* func,
        const AbstractQoreFunctionVariant*& variant, const char* n_name, QoreListNode* args, QoreObject* self,
        const qore_class_private* n_qc, qore_call_t n_ct, bool is_copy, const qore_class_private* cctx,
        QoreProgram* pgm_ctx)
    : ct(n_ct), name(n_name), xsink(n_xsink), rc(n_rc), qc(n_qc),
        loc(get_runtime_location()),
        tmp(n_xsink), returnTypeInfo((const QoreTypeInfo*)-1) {
    if (self && !self->isValid()) {
        assert(n_qc);
        xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has already been " \
            "deleted", qc->name.c_str(), func->getName());
        return;
    }

    setCallName(func);
    tmp.assignEval(args);
    if (*xsink) {
        return;
    }

    init(func, variant, is_copy, cctx, self, pgm_ctx);
}

CodeEvaluationHelper::~CodeEvaluationHelper() {
    if (restore_stack) {
        if (ct == CT_BUILTIN) {
            update_runtime_stack_location(stack_loc, old_runtime_loc);
        } else {
            update_runtime_stack_location(stack_loc);
        }
    }
    if (restore_rtflags) {
        rc.setRuntimeFlags(old_rtflags);
    }
    if (returnTypeInfo != (const QoreTypeInfo*)-1) {
        saveReturnTypeInfo(returnTypeInfo);
    }
}

void CodeEvaluationHelper::setCallName(const QoreFunction* func) {
    if (qc) {
        callName = qc->name.c_str();
        callName += "::";
    }
    callName += func->getName();
}

void CodeEvaluationHelper::init(const QoreFunction* func, const AbstractQoreFunctionVariant*& variant, bool is_copy,
        const qore_class_private* cctx, QoreObject* self, QoreProgram* pgm_ctx) {
    //printd(5, "CodeEvaluationHelper::init() this: %p '%s()' file: %s line: %d variant: %p cctx: %p (%s)\n", this,
    //    func->getName(), loc->getFile(), loc->start_line, variant, cctx, cctx ? cctx->name.c_str() : "n/a");

#ifdef QORE_MANAGE_STACK
    if (check_stack(xsink)) {
        return;
    }
#endif

    // set the program context if necessary
    if (pgm_ctx) {
        set(xsink, pgm_ctx, true);
        if (*xsink) {
            return;
        }
    }

    if (variant) {
        // get default argument list of variant
        AbstractFunctionSignature* sig = variant->getSignature();

        const AbstractQoreFunctionVariant* v = variant;
        ExceptionSink xsink2;
        // first process all non-default args; no evaluation, so we can try to find another variant
        if (prepareDefaultArgs(&xsink2, variant, sig, is_copy, self, ARG_OTHER)) {
            // if no match can be found, return with the exception raised
            if (findVariant(func, variant, cctx)) {
                xsink2.clear();
                return;
            }
            if (v == variant) {
                xsink->assimilate(xsink2);
                return;
            }
            xsink2.clear();
            tmp.discard();
            sig = variant->getSignature();
            // prepare all args
            if (prepareDefaultArgs(xsink, variant, sig, is_copy, self, ARG_DEF | ARG_OTHER)) {
                return;
            }
        } else if (prepareDefaultArgs(&xsink2, variant, sig, is_copy, self, ARG_DEF)) {
            return;
        }
        if (processDefaultArgs(xsink, func, variant, sig, is_copy, self)) {
            return;
        }
    } else {
        if (findVariant(func, variant, cctx)) {
            return;
        }
        // get default argument list of variant
        AbstractFunctionSignature* sig = variant->getSignature();
        // prepare all args
        if (prepareDefaultArgs(xsink, variant, sig, is_copy, self, ARG_DEF | ARG_OTHER)) {
            return;
        }
        if (processDefaultArgs(xsink, func, variant, sig, is_copy, self)) {
            return;
        }
    }

    setCallType(variant->getCallType());
    setReturnTypeInfo(variant->getReturnTypeInfo());
    old_rtflags = rc.getRuntimeFlags();
    rc.setRuntimeFlags(static_cast<q_rt_flags_t>(variant->getFlags()));
    restore_rtflags = true;

    // add call to call stack; push builtin location on the stack if executing builtin c++ code
    if (ct == CT_BUILTIN) {
        stack_loc = update_get_runtime_stack_builtin_location(this, stmt, pgm, old_runtime_loc);
    } else {
        stack_loc = update_get_runtime_stack_location(this, stmt, pgm);
    }
    restore_stack = true;
}

int CodeEvaluationHelper::findVariant(const QoreFunction* func, const AbstractQoreFunctionVariant*& variant,
        const qore_class_private* cctx) {
    const qore_class_private* class_ctx = qc ? (cctx ? cctx : runtime_get_class()) : nullptr;
    if (class_ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*qc->cls, class_ctx)) {
        class_ctx = nullptr;
    }

    variant = func->runtimeFindVariant(xsink, getArgs(), false, class_ctx);
    if (!variant) {
        assert(*xsink);
        return -1;
    }

    // check for accessible variants
    if (qc) {
        const MethodVariant* mv = reinterpret_cast<const MethodVariant*>(variant);
        ClassAccess va = mv->getAccess();
        if ((va > Public && !class_ctx) || (va == Internal
            && !qore_class_private::get(*mv->getClass())->equal(*qc))) {
            xsink->raiseException("METHOD-IS-PRIVATE", "%s::%s(%s) is not accessible in this context",
                mv->className(), func->getName(), mv->getSignature()->getSignatureText());
            return -1;
        }
    }
    return 0;
}

int CodeEvaluationHelper::prepareDefaultArgs(ExceptionSink* xsink, const AbstractQoreFunctionVariant* variant,
        AbstractFunctionSignature* sig, bool is_copy, QoreObject* self, int arg_type) {
    const arg_vec_t& defaultArgList = sig->getDefaultArgList();
    const type_vec_t& typeList = sig->getTypeList();

    unsigned max = QORE_MAX(defaultArgList.size(), typeList.size());
    if (!max) {
        return 0;
    }
    OptionalObjectOnlySubstitutionHelper self_helper;
    bool self_set = false;
    for (unsigned i = 0; i < max; ++i) {
        if (i < defaultArgList.size() && defaultArgList[i] && (!tmp || tmp->retrieveEntry(i).isNothing())) {
            if (arg_type & ARG_DEF) {
                QoreValue& p = tmp.getEntryReference(i);

                // issue #3240: set self in case the default arg expression references a member of the current object
                // must be set only for evaluation, cannot be set when verifying types below in
                // QoreTypeInfo::acceptInputParam() as it will cause errors handling references related to the current
                // object - "self" is the object for the call but not necessarily the current "self"
                if (self && !self_helper) {
                    self_helper.set(self);
                }

                p = defaultArgList[i].eval(xsink);
                if (*xsink) {
                    return -1;
                }

                // process default argument with accepting type's filter if necessary
                const QoreTypeInfo* paramTypeInfo = sig->getParamTypeInfo(i);
                if (QoreTypeInfo::mayRequireFilter(paramTypeInfo, p)) {
                    QoreTypeInfo::acceptInputParam(paramTypeInfo, i, sig->getName(i), p, xsink);
                    if (*xsink) {
                        return -1;
                    }
                }
            }
        } else if (i < typeList.size()) {
            if (arg_type & ARG_OTHER) {
                QoreValue n{};  // value-initialized to NOTHING (bits=0)
                if (tmp) {
                    n = tmp->retrieveEntry(i);
                }

                if (is_copy && !i && n.isNothing()) {
                    continue;
                }

                const QoreTypeInfo* paramTypeInfo = sig->getParamTypeInfo(i);
                if (!paramTypeInfo) {
                    continue;
                }

                // issue #3184: do not create a NOTHING argument if none is needed
                if (!QoreTypeInfo::hasType(paramTypeInfo)
                    || (tmp.size() < i && QoreTypeInfo::parseAcceptsReturns(paramTypeInfo, NT_NOTHING))) {
                    continue;
                }
                // test for change or incompatibility
                QoreValue& p = tmp.getEntryReference(i);
                QoreTypeInfo::acceptInputParam(paramTypeInfo, i, sig->getName(i), p, xsink);
                if (*xsink) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

int CodeEvaluationHelper::processDefaultArgs(ExceptionSink* xsink, const QoreFunction* func,
        const AbstractQoreFunctionVariant* variant, AbstractFunctionSignature* sig, bool is_copy,
        QoreObject* self) {
    // check for excess args exception
    unsigned nargs = tmp.size();
    if (!nargs)
        return 0;
    unsigned nparams = sig->numParams();

    //printd(5, "processDefaultArgs() %s nargs: %d nparams: %d flags: %lld po: %d\n", func->getName(), nargs, nparams,
    //  variant->getFlags(), (bool)(getProgram()->getParseOptions() & (PO_REQUIRE_TYPES | PO_STRICT_ARGS)));
    //if (nargs > nparams && (getProgram()->getParseOptions() & (PO_REQUIRE_TYPES | PO_STRICT_ARGS))) {
    if (nargs > nparams) {
        // use the target program (if different than the current pgm) to check for argument errors
        const UserVariantBase* uvb = variant->getUserVariantBase();
        QoreParseOptions po;
        if (uvb)
            po = uvb->pgm->getParseOptions();
        else
            po = runtime_get_parse_options();

        if (po & (PO_REQUIRE_TYPES | PO_STRICT_ARGS)) {
            int64 flags = variant->getFlags();

            if (!(flags & QCF_USES_EXTRA_ARGS)) {
                for (unsigned i = nparams; i < nargs; ++i) {
                    //printd(5, "processDefaultArgs() %s arg %d nothing: %d\n", func->getName(), i,
                    //  tmp->retrieveEntry(i).isNothing());
                    if (!tmp->retrieveEntry(i).isNothing()) {
                        QoreStringNode* desc = new QoreStringNode("call to ");
                        do_call_name(*desc, func);
                        if (nparams)
                            desc->concat(sig->getSignatureText());
                        desc->concat(") made as ");
                        do_call_name(*desc, func);
                        add_args(*desc, *tmp);
                        unsigned diff = nargs - nparams;
                        desc->sprintf(") with %d excess argument%s, which is an error when PO_REQUIRE_TYPES or " \
                            "PO_STRICT_ARGS is set", diff, diff == 1 ? "" : "s");
                        xsink->raiseException("CALL-WITH-TYPE-ERRORS", desc);
                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}

void AbstractFunctionSignature::addDefaultArgument(std::string& str, QoreValue arg) {
    assert(arg);
    str.append(" = ");
    qore_type_t t = arg.getType();
    if (t == NT_BAREWORD) {
        str.append(arg.get<const BarewordNode>()->str);
        return;
    }
    if (t == NT_CONSTANT) {
        str.append(arg.get<const ScopedRefNode>()->scoped_ref->getIdentifier());
        return;
    }
    if (t == NT_OBJECT) {
        str.append("<");
        str.append(get_full_type_name(arg.getInternalNode(), true));
        str.append(" object>");
        return;
    }
    if (!arg.needsEval()) {
        QoreNodeAsStringHelper sh(arg, FMT_NONE, 0);
        str.append(sh->c_str());
        return;
    }
    str.append("<exp>");
}

UserSignature::UserSignature(int first_line, int last_line, QoreValue params, RetTypeInfo* retTypeInfo, const QoreParseOptions& po) :
        AbstractFunctionSignature(retTypeInfo ? retTypeInfo->getTypeInfo() : nullptr),
        parseReturnTypeInfo(retTypeInfo ? retTypeInfo->takeParseTypeInfo() : nullptr),
        loc(qore_program_private::get(*getProgram())->getLocation(first_line, last_line)),
        lv(0), argvid(0), selfid(0), resolved(false) {
    bool needs_types = (bool)(po & (PO_REQUIRE_TYPES | PO_REQUIRE_PROTOTYPES));
    bool bare_refs = (bool)(po & PO_ALLOW_BARE_REFS);

    // assign no return type if return type declaration is missing and PO_REQUIRE_TYPES or PO_REQUIRE_PROTOTYPES is set
    if (!retTypeInfo && needs_types)
        returnTypeInfo = nothingTypeInfo;
    delete retTypeInfo;

    if (!params) {
        return;
    }

    ValueHolder param_holder(params, nullptr);

    if (params.getType() == NT_VARREF) {
        err = pushParam(params.get<VarRefNode>(), QoreValue(), needs_types);
        return;
    }

    if (params.getType() == NT_BAREWORD) {
        err = pushParam(params.get<BarewordNode>(), needs_types, bare_refs);
        return;
    }

    if (params.getType() == NT_OPERATOR) {
        err = pushParam(params.get<QoreOperatorNode>(), needs_types);
        return;
    }

    if (params.getType() == NT_ELLIPSES) {
        assert(!varargs);
        varargs = true;
        return;
    }

    if (params.getType() != NT_PARSE_LIST) {
        param_error();
        err = -1;
        return;
    }

    QoreParseListNode* l = params.get<QoreParseListNode>();

    // first check for ellipses
    if (l->size() && l->get(l->size() - 1).getType() == NT_ELLIPSES) {
        assert(!varargs);
        varargs = true;

        l->pop().discard(nullptr);
        if (l->empty()) {
            return;
        }
    }

    parseTypeList.reserve(l->size());
    typeList.reserve(l->size());
    defaultArgList.reserve(l->size());

    for (unsigned i = 0; i < l->size(); ++i) {
        QoreValue n = l->get(i);
        qore_type_t t = n.getType();
        if (t == NT_OPERATOR) {
            if (pushParam(n.get<QoreOperatorNode>(), needs_types) && !err) {
                err = -1;
            }
        } else if (t == NT_BAREWORD) {
            if (pushParam(n.get<BarewordNode>(), needs_types, bare_refs) && !err) {
                err = -1;
            }
        } else if (t == NT_VARREF) {
            if (pushParam(n.get<VarRefNode>(), QoreValue(), needs_types) && !err) {
                err = -1;
            }
        } else {
            if (!n.isNothing()) {
                param_error();
                if (!err) {
                    err = -1;
                }
            }
            break;
        }

        // add a comma to the signature string if it's not the last parameter
        if (i != (l->size() - 1)) {
            str.append(", ");
        }
    }
}

int UserSignature::pushParam(QoreOperatorNode* t, bool needs_types) {
    QoreAssignmentOperatorNode* op = dynamic_cast<QoreAssignmentOperatorNode*>(t);
    if (!op) {
        parse_error(*loc, "invalid expression with the '%s' operator in parameter list; only simple assignments to " \
            "default values are allowed", t->getTypeName());
        return -1;
    }

    QoreValue l = op->getLeft();
    if (l.getType() != NT_VARREF) {
        param_error();
        return -1;
    }
    VarRefNode* v = l.get<VarRefNode>();
    QoreValue defArg = op->swapRight(0);
    pushParam(v, defArg, needs_types);
    return 0;
}

int UserSignature::pushParam(BarewordNode* b, bool needs_types, bool bare_refs) {
    names.push_back(b->str);
    parseTypeList.push_back(0);
    typeList.push_back(0);
    str.append(NO_TYPE_INFO);
    str.append(" ");
    str.append(b->str);
    defaultArgList.push_back(QoreValue());

    int err = 0;
    if (needs_types) {
        parse_error(*loc, "parameter '%s' declared without type information, but parse options require all " \
            "declarations to have type information", b->str);
        if (!err) {
            err = -1;
        }
    }

    if (!bare_refs) {
        parse_error(*loc, "parameter '%s' declared without '$' prefix, but parse option 'allow-bare-defs' is not " \
            "set", b->str);
        if (!err) {
            err = -1;
        }
    }
    return err;
}

int UserSignature::pushParam(VarRefNode* v, QoreValue defArg, bool needs_types) {
    int err = 0;
    // check for duplicate name
    for (name_vec_t::iterator i = names.begin(), e = names.end(); i != e; ++i) {
        if (*i == v->getName()) {
            parse_error(*loc, "duplicate variable '%s' declared in parameter list", (*i).c_str());
            if (!err) {
                err = -1;
            }
        }
    }

    names.push_back(v->getName());

    bool is_decl = v->isDecl();
    if (needs_types && !is_decl) {
        parse_error(*loc, "parameter '%s' declared without type information, but parse options require all " \
            "declarations to have type information", v->getName());
        if (!err) {
            err = -1;
        }
    }

    // see if this is a new object call
    if (v->has_effect()) {
        // here we make 4 virtual function calls when 2 would be enough, but no need to optimize for speed for an exception
        parse_error(*loc, "parameter '%s' may not be declared with implicit constructor syntax; instead use: " \
            "'%s %s = new %s()'", v->getName(), v->parseGetTypeName(), v->getName(), v->parseGetTypeName());
        if (!err) {
            err = -1;
        }
    }

    if (is_decl) {
        VarRefDeclNode* vd = reinterpret_cast<VarRefDeclNode*>(v);
        QoreParseTypeInfo* pti = vd->takeParseTypeInfo();
        parseTypeList.push_back(pti);
        const QoreTypeInfo* ti = vd->getTypeInfo();
        typeList.push_back(ti);

        assert(!(pti && ti));

        if (pti || QoreTypeInfo::hasType(ti)) {
            ++num_param_types;
            // only increment min_param_types if there is no default argument
            if (!defArg)
                ++min_param_types;
        }

        // add type name to signature
        if (pti) {
            QoreParseTypeInfo::concatName(pti, str);
        } else {
            str.append(QoreTypeInfo::getPath(ti));
        }
    } else {
        parseTypeList.push_back(nullptr);
        typeList.push_back(nullptr);
        str.append(NO_TYPE_INFO);
    }

    str.append(" ");
    str.append(v->getName());

    defaultArgList.push_back(defArg);
    if (defArg) {
        addDefaultArgument(str, defArg);
    }

    if (v->explicitScope()) {
        if (v->getType() == VT_LOCAL) {
            parse_error(*loc, "invalid local variable declaration in argument list; by default all variables " \
                "declared in argument lists are local");
            if (!err) {
                err = -1;
            }
        } else if (v->getType() == VT_GLOBAL) {
            parse_error(*loc, "invalid global variable declaration in argument list; by default all variables " \
                "declared in argument lists are local");
            if (!err) {
                err = -1;
            }
        }
    }

    //printd(5, "UserSignature::UserSignature() %p '%s'\n", this, str.c_str());
    return err;
}

void UserSignature::setupFromAOTMetadata(
        QoreProgram* pgm,
        const QoreTypeInfo* retType,
        const std::vector<std::string>& paramNames,
        const std::vector<const QoreTypeInfo*>& paramTypes,
        const std::vector<QoreValue>& defaults,
        bool hasVarargs,
        const QoreClass* classTypeInfo) {
    // Set return type
    returnTypeInfo = retType;

    // Set parameter types and names
    typeList = paramTypes;
    names = paramNames;

    // Set default arguments (ref values for storage)
    defaultArgList.resize(paramTypes.size());
    for (size_t i = 0; i < defaults.size() && i < paramTypes.size(); ++i) {
        defaultArgList[i] = defaults[i].refSelf();
    }

    // Create LocalVar* for each parameter via the program's local var allocator
    qore_program_private* pp = qore_program_private::get(*pgm);
    lv.resize(paramTypes.size());
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        const char* pname = i < paramNames.size() ? paramNames[i].c_str() : "";
        lv[i] = pp->createLocalVar(pname, paramTypes[i]);
    }

    // Create selfid local var for methods (matches parseInitPushLocalVars behavior)
    // Each method variant needs its own selfid that will be instantiated on the stack
    if (classTypeInfo) {
        selfid = pp->createLocalVar("self", classTypeInfo->getTypeInfo());
    }

    // Create argv local var - always created (matches parseInitPushLocalVars behavior)
    // evalTiered() unconditionally instantiates argvid
    argvid = pp->createLocalVar("argv", autoListOrNothingTypeInfo);

    // Set flags
    varargs = hasVarargs;
    // NOTE: Don't set resolved = true here. The signature will be marked as resolved
    // when parseCommit() is called on the function (which calls resolve() on each variant).
    // Setting it to true here would cause parseCheckDuplicateSignature() to fail when
    // adding multiple variants, as it expects all pending variants to be unresolved.

    // Count param types
    num_param_types = 0;
    min_param_types = 0;
    for (size_t i = 0; i < typeList.size(); ++i) {
        if (typeList[i]) {
            ++num_param_types;
            if (i >= defaultArgList.size() || !defaultArgList[i]) {
                ++min_param_types;
            }
        }
    }

    // Build signature string
    str.clear();
    addAbstractParameterSignature(str);
}

void UserSignature::parseInitPushLocalVars(const QoreTypeInfo* classTypeInfo) {
    lv.reserve(parseTypeList.size());

    int err = 0;
    if (selfid) {
        push_local_var(selfid, loc);
    } else if (classTypeInfo) {
        selfid = push_local_var("self", loc, classTypeInfo, err, true, 1);
        selfid->setSelf();
    }

    // push argv var on stack and save id
    argvid = push_local_var("argv", loc, autoListOrNothingTypeInfo, err, true, 1);
    printd(5, "UserSignature::parseInitPushLocalVars() this: %p (%s) argvid: %p selfid: %p\n", this,
        getSignatureText(), argvid, selfid);

    resolve();

    // init param ids and push local parameter vars on stack
    for (unsigned i = 0; i < typeList.size(); ++i) {
        // check for dups but do not check if the variables are referenced in the block
        // push args declared as type "*reference" as "any"; if no value passed, then they have no type restrictions
        // NOTE that when complex types are supported, the type restriction should be that of the reference's subtype
        lv.push_back(push_local_var(names[i].c_str(), loc,
            typeList[i] == referenceOrNothingTypeInfo ? anyTypeInfo : typeList[i], err, true, 1));
        printd(5, "UserSignature::parseInitPushLocalVars() registered local var %s (id: %p)\n", names[i].c_str(),
            lv[i]);
    }

    assert(!err);
}

void UserSignature::parseInitPopLocalVars() {
    // remove local variables from stack and unset the parse_assigned flag
    for (unsigned i = 0; i < typeList.size(); ++i) {
        pop_local_var(true);
    }

    // pop argv param off stack
    pop_local_var();

    // pop $self off stack if present
    if (selfid) {
        pop_local_var();
    }
}

int UserSignature::resolve() {
    if (resolved) {
        return 0;
    }

    resolved = true;

    int err = 0;

    if (!returnTypeInfo) {
        returnTypeInfo = QoreParseTypeInfo::resolveAndDelete(parseReturnTypeInfo, loc, err);
        parseReturnTypeInfo = nullptr;
    }
#ifdef DEBUG
    else assert(!parseReturnTypeInfo);
#endif

    // issue #3644: to fix recursive errors in signature resolution, first resolve types, then args
    bool has_def_args = true;
    for (unsigned i = 0; i < parseTypeList.size(); ++i) {
        if (parseTypeList[i]) {
            assert(!typeList[i]);
            typeList[i] = QoreParseTypeInfo::resolveAndDelete(parseTypeList[i], loc, err);
        }
        if (!has_def_args && defaultArgList[i]) {
            has_def_args = true;
        }
    }

    // now resolve default args
    if (has_def_args) {
        for (unsigned i = 0; i < parseTypeList.size(); ++i) {
            // initialize default arguments
            if (defaultArgList[i]) {
                QoreParseContext parse_context(selfid);
                if (parse_init_value(defaultArgList[i], parse_context) && !err) {
                    err = -1;
                }
                const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;
                if (parse_context.lvids) {
                    parse_error(*loc, "illegal local variable declaration in default value expression in parameter " \
                        "'%s'", names[i].c_str());
                    while (parse_context.lvids--) {
                        pop_local_var();
                    }
                }
                // check type compatibility
                if (!QoreTypeInfo::parseAccepts(typeList[i], argTypeInfo)) {
                    QoreStringNode* desc = new QoreStringNode;
                    desc->sprintf("parameter '%s' expects ", names[i].c_str());
                    QoreTypeInfo::getThisType(typeList[i], *desc);
                    desc->concat(", but the default value is ");
                    QoreTypeInfo::getThisType(argTypeInfo, *desc);
                    desc->concat(" instead");
                    qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
                    if (!err) {
                        err = -1;
                    }
                }
            }
        }
    }
    parseTypeList.clear();

    // redo signature
    str.clear();
    AbstractFunctionSignature::addAbstractParameterSignature(str);
    return err;
}

bool QoreFunction::existsVariant(const type_vec_t& paramTypeInfo) const {
    for (vlist_t::const_iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        AbstractFunctionSignature* sig = (*i)->getSignature();
        assert(sig);
        unsigned np = sig->numParams();
        if (np != paramTypeInfo.size())
            continue;
        if (!np)
            return true;
        bool ok = true;
        for (unsigned pi = 0; pi < np; ++pi) {
            if (!QoreTypeInfo::isInputIdentical(paramTypeInfo[pi], sig->getParamTypeInfo(pi))) {
                ok = false;
                break;
            }
        }
        if (ok)
            return true;
    }
    return false;
}

static QoreStringNode* getNoopError(const QoreFunction* func, const QoreFunction* aqf,
        const AbstractQoreFunctionVariant* variant) {
    QoreStringNode* desc = new QoreStringNode;
    do_call_name(*desc, aqf);
    desc->sprintf("%s) is a variant that returns a constant value when incorrect data types are passed to the " \
        "function", variant->getSignature()->getSignatureText());
    const QoreTypeInfo* rti = variant->getReturnTypeInfo();
    if (QoreTypeInfo::hasType(rti) && !variant->numParams()) {
        desc->concat(" and always returns ");
        if (QoreTypeInfo::getUniqueReturnClass(rti) || func->className()) {
            QoreTypeInfo::getThisType(rti, *desc);
        } else {
            // get actual value and include in warning
            ExceptionSink xsink;
            RuntimeConfig& rc = rc_get_current_ref();
            CodeEvaluationHelper ceh(&xsink, rc, func, variant, "noop-dummy");
            ValueHolder v(variant->evalFunction(nullptr, ceh), nullptr);
            //ReferenceHolder<AbstractQoreNode> v(variant->evalFunction(func->getName(), ceh, 0), 0);
            if (v->isNothing())
                desc->concat("NOTHING");
            else {
                QoreNodeAsStringHelper vs(*v, FMT_NONE, 0);
                desc->sprintf("the following value: %s (", vs->c_str());
                QoreTypeInfo::getThisType(rti, *desc);
                desc->concat(')');
            }
        }
    }
    return desc;
}

static bool skip_method_variant(const AbstractQoreFunctionVariant* v, const qore_class_private* class_ctx,
        bool internal_access) {
    assert(dynamic_cast<const MethodVariantBase*>(v));
    const MethodVariantBase* mvb = reinterpret_cast<const MethodVariantBase*>(v);
    ClassAccess va = mvb->getAccess();
    // skip if the variant is not accessible
    return ((!class_ctx && va > Public) || (va == Internal && !internal_access));
}

static AbstractQoreFunctionVariant* doSingleVariantTypeException(const QoreProgramLocation* loc, int pi,
        const char* class_name, const char* name, const AbstractFunctionSignature* sig, const QoreTypeInfo* proto,
        const QoreTypeInfo* arg) {
    QoreStringNode* desc = new QoreStringNode("argument ");
    const name_vec_t& nv = sig->getParamNames();
    if (nv.size() > pi) {
        desc->sprintf("'%s' to ", nv[pi].c_str());
    } else {
        desc->sprintf("%d to '", pi + 1);
    }
    if (class_name) {
        desc->sprintf("%s::", class_name);
    }
    desc->sprintf("%s(%s)' expects %s, but call supplies %s", name, sig->getSignatureText(),
        QoreTypeInfo::getPath(proto), QoreTypeInfo::getPath(arg));
    qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
    return nullptr;
}

static void do_call_str(QoreString &desc, const QoreFunction* func, const type_vec_t& argTypeInfo) {
    unsigned num_args = argTypeInfo.size();
    do_call_name(desc, func);
    if (num_args) {
        for (unsigned i = 0; i < num_args; ++i) {
            desc.concat(QoreTypeInfo::getPath(argTypeInfo[i]));
            if (i != (num_args - 1))
                desc.concat(", ");
        }
    }
    desc.concat(')');
}

static int warn_excess_args(const QoreProgramLocation* loc, const QoreFunction* func, const type_vec_t& argTypeInfo,
        AbstractFunctionSignature* sig) {
    unsigned nargs = argTypeInfo.size();
    unsigned nparams = sig->numParams();

    QoreStringNode* desc = new QoreStringNode("call to ");
    desc->concat(func->className() ? "method " : "function ");
    do_call_name(*desc, func);
    if (nparams)
        desc->concat(sig->getSignatureText());
    desc->concat(") made as ");
    do_call_str(*desc, func, argTypeInfo);
    unsigned diff = nargs - nparams;
    desc->sprintf(" (with %d excess argument%s)", diff, diff == 1 ? "" : "s");
    // raise warning if require-types is not set
    //if (getProgram()->getParseOptions() & (PO_REQUIRE_TYPES | PO_STRICT_ARGS)) {
    if (parse_get_parse_options() & (PO_REQUIRE_TYPES | PO_STRICT_ARGS)) {
        desc->concat("; this is an error when PO_REQUIRE_TYPES or PO_STRICT_ARGS is set");
        qore_program_private::makeParseException(getProgram(), *loc, "CALL-WITH-TYPE-ERRORS", desc);
        return -1;
    }

    // raise warning
    desc->concat("; excess arguments will be ignored; to disable this warning, use " \
        "'%%disable-warning excess-args' in your code");
    qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_EXCESS_ARGS, "EXCESS-ARGS", desc);
    return 0;
}

static int check_extra_args(AbstractFunctionSignature* sig, const type_vec_t& argTypeInfo) {
    // either extra arguments are ignored (strict_args is false) or preclude the match
    // see if any of the extra arguments have a type
    for (unsigned pi = sig->numParams(); pi < argTypeInfo.size(); ++pi) {
        const QoreTypeInfo* a = argTypeInfo[pi];
        if (!QoreTypeInfo::parseAcceptsReturns(a, NT_NOTHING))
            return -1;
    }
    return 0;
}

QoreListNode* QoreFunction::runtimeGetCallVariants() const {
   ReferenceHolder<QoreListNode> rv(new QoreListNode(autoHashTypeInfo), nullptr);

    const char* class_name = className();
    QoreParseOptions ppo = runtime_get_parse_options();

    printd(5, "QoreFunction::runtimeGetCallVariants() this: %p, class_name: %s\n", this, class_name);
    for (vlist_t::const_iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        // get code flags for the variant
        int64 vflags = (*i)->getFlags();

        // get parse options
        QoreParseOptions po = (*i)->getParseOptions(ppo);
        // if we should ignore "noop" variants
        bool strict_args = static_cast<bool>(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

        // ignore "runtime noop" variants if necessary
        if (strict_args && (vflags & QCF_RUNTIME_NOOP)) {
            printd(5, "QoreFunction::runtimeGetCallVariants() this: %p, skip runtime noop, vflags: %p\n", this, vflags);
            continue;
        }

        // check functionality flags to see if the variant is accessible
        int64 vfflags = (*i)->getFunctionality();
        if ((vfflags & po & ~PO_POSITIVE_OPTIONS)
            || ((vfflags & PO_POSITIVE_OPTIONS)
                && (((vfflags & PO_POSITIVE_OPTIONS) & po) != (vfflags & PO_POSITIVE_OPTIONS)))) {
            printd(5, "QoreFunction::runtimeGetCallVariants() this: %p, skip functionality, vfflags: %p\n", this, vfflags);
            continue;
        }

        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);

        SimpleRefHolder<QoreStringNode> desc(new QoreStringNode);

        AbstractFunctionSignature* sig = (*i)->getSignature();
        assert(sig);

        // add "desc" key
        QoreStringNodeMaker* sm = new QoreStringNodeMaker("%s%s%s(%s)",
                class_name ? class_name : "", class_name ? "::" : "", getName(),
                sig->getSignatureText());
        printd(5, "QoreFunction::runtimeGetCallVariants() this: %p, desc: %s, numparams: %d\n", this, sm->c_str(), sig->numParams());
        h->setKeyValue("desc", sm, nullptr);

        // add "params" key
        ReferenceHolder<QoreListNode> params(new QoreListNode(autoTypeInfo), nullptr);
        for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
            QoreStringNode* s = new QoreStringNode(QoreTypeInfo::getPath(sig->getParamTypeInfo(pi)));
            printd(5, "QoreFunction::runtimeGetCallVariants() this: %p, param %d: %s\n", this, pi, s->c_str());
            params->push(s, nullptr);
        }
        h->setKeyValue("params", params.release(), nullptr);

        rv->push(h.release(), nullptr);
    }

    return rv->empty() ? nullptr : rv.release();
}

// finds a variant at runtime
const AbstractQoreFunctionVariant* QoreFunction::runtimeFindVariant(ExceptionSink* xsink, const QoreListNode* args,
        bool only_user, const qore_class_private* class_ctx) const {
    unsigned nargs = args ? args->size() : 0;
    QoreParseOptions ppo = runtime_get_parse_options();

    // the lowest score length with the highest score wins
    int score_len = -1;
    int score = -1;
    const AbstractQoreFunctionVariant* variant = nullptr;
    //const AbstractQoreFunctionVariant* saved_variant = nullptr;

    if (className() && !strcmp(getName(), "constructor") && nargs == 0) {
        printd(5, "runtimeFindVariant() %s::constructor() nargs=0 vlist=%d ilist=%d\n",
            className(), (int)vlist.size(), (int)ilist.size());
    }

    const QoreFunction* aqf = nullptr;
    AbstractFunctionSignature* sig = nullptr;

    // parent class while iterating
    const qore_class_private* last_class = nullptr;
    bool internal_access = false;

    int cnt = 0;

    // iterate through inheritance list
    for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
        bool stop;
        aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
        if (!aqf) {
            break;
        }

        // issue #3070: skip abstract methods
        if (last_class && static_cast<const MethodFunctionBase*>(aqf)->isAbstract()) {
            continue;
        }

        //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s%s%s(...) size: %d last_class: %p ctx: %p: %s\n",
        //  this, aqf->className() ? aqf->className() : "", className() ? "::" : "", getName(), ilist.size(),
        //  last_class, class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a");

        for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
            // skip checking variant if we are only looking for user variants and this variant is builtin
            if (only_user && !(*i)->isUser()) {
                continue;
            }

            // skip if the variant is not accessible or abstract
            if (last_class
                && (skip_method_variant(*i, class_ctx, internal_access)
                    || static_cast<const MethodVariantBase*>(*i)->isAbstract())) {
                    continue;
            }

            // get runtime parse options
            QoreParseOptions po = (*i)->getParseOptions(ppo);
            // if we should ignore "noop" variants
            bool strict_args = static_cast<bool>(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

            int64 vflags = (*i)->getFlags();

            // ignore "runtime noop" variants if necessary
            if (strict_args && (vflags & QCF_RUNTIME_NOOP)) {
                continue;
            }

            // does the variant accept extra arguments?
            bool uses_extra_args = vflags & QCF_USES_EXTRA_ARGS;

            ++cnt;

            sig = (*i)->getSignature();
            assert(sig);

            // if the signature has ellipses, then QCF_USES_EXTRA_ARGS must be set in vflags
            assert(uses_extra_args || !sig->hasVarargs());

            //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s(%s) args: %p (%d) class: %s class_ctx: %p '%s' "
            //    "nargs: %d nparams: %d\n", this, getName(), sig->getSignatureText(), args, args ? args->size() : 0,
            //    aqf->className() ? aqf->className() : "n/a", class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a",
            //    nargs, sig->numParams());

            // issue 1507: ensure that calls with no arguments and no params are considered a perfect match
            if (!nargs && !sig->numParams()) {
                variant = *i;
                break;
            }

            // skip variants with signatures with fewer possible elements than the best match already
            if ((int)(sig->getParamTypes() * QTI_IDENT) < score) {
                continue;
            }

            int pscore = 0;
            bool ok = true;
            for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                const QoreTypeInfo* t = sig->getParamTypeInfo(pi);
                QoreValue n{};  // value-initialized to NOTHING (bits=0)
                if (args) {
                    n = args->retrieveEntry(pi);
                }

                int rc;
                if (n.isNothing() && sig->hasDefaultArg(pi)) {
                    rc = QTI_IGNORE;
                } else {
                    rc = QoreTypeInfo::runtimeAcceptsValue(t, n);
                    if (className() && !strcmp(getName(), "constructor") && nargs == 0) {
                        printd(5, "  param[%d] type='%s' hasDefault=%d acceptsNothing=%d rc=%d\n",
                            pi, QoreTypeInfo::getName(t), sig->hasDefaultArg(pi), (int)rc, (int)(rc == QTI_NOT_EQUAL));
                    }
                    if (rc == QTI_NOT_EQUAL) {
                        ok = false;
                        break;
                    }
                    // do not count default matches with non-existent arguments
                    if (pi >= nargs) {
                        rc = QTI_IGNORE;
                    }
                }

                // only increment for actual type matches (t may be NULL)
                if (t && rc != QTI_IGNORE) {
                    pscore += rc;
                }
            }
            if (!ok) {
                continue;
            }

            // check for extra args
            if ((sig->numParams() < nargs) && strict_args && !uses_extra_args) {
                bool has_arg = false;
                for (unsigned pi = sig->numParams(); pi < nargs; ++pi) {
                    if (!args->retrieveEntry(pi).isNothing()) {
                        has_arg = true;
                        break;
                    }
                }
                if (has_arg) {
                    continue;
                }
            }

            //printd(5, "QoreFunction::runtimeFindVariant() pscore: %d score: %d score_len: %d np: %d v: %p\n", pscore,
            //    score, score_len, sig->numParams(), variant);

            if (pscore > score || (pscore == score && (score_len == -1 || (sig->numParams() < (unsigned)score_len)))) {
                score = pscore;
                variant = *i;

                score_len = sig->numParams();
            }
        }
        // issue 1229: stop searching the class hierarchy if a match found
        if (stop || variant) {
            break;
        }
    }
    /*
    if (saved_variant) {
        assert(!variant);
        variant = saved_variant;
    }
    */

    if (!variant && !only_user) {
        QoreStringNode* desc = new QoreStringNode("no variant matching '");
        std::string class_path = classPath();
        if (!class_path.empty())
            desc->sprintf("%s::", class_path.c_str());
        desc->sprintf("%s(", getName());
        add_args(*desc, args);
        desc->concat(")' can be found; ");

        if (!cnt) {
            desc->concat("no variants were accessible in this execution context");
        } else {
            desc->concat("the following variants were tested:");

            last_class = 0;
            internal_access = false;

            // add variants tested
            // iterate through inheritance list
            for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
                bool stop;
                aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
                if (!aqf)
                    break;
                class_path = aqf->classPath();

                for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
                    // skip if the variant is not accessible or abstract
                    if (last_class
                        && (skip_method_variant(*i, class_ctx, internal_access)
                            || static_cast<const MethodVariantBase*>(*i)->isAbstract())) {
                            continue;
                    }
                    desc->concat("\n   ");
                    if (!class_path.empty()) {
                        desc->sprintf("%s::", class_path.c_str());
                    }
                    desc->sprintf("%s(%s)", getName(), (*i)->getSignature()->getSignatureText());
                }
                if (stop)
                    break;
            }
        }
        xsink->raiseException("RUNTIME-OVERLOAD-ERROR", desc);
    } else if (variant) {
        QoreProgram* pgm = getProgram();

        // pgm could be zero if called from a foreign thread with no current Program
        if (pgm) {
            // get runtime parse options
            QoreParseOptions po = variant->getParseOptions(ppo);

            // check parse options
            int64 vflags = variant->getFunctionality();
            // check restrictive flags
            //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s() returning %p %s(%s) vflags: " QLLD " po: " QLLD
            //    " neg: " QLLD "\n", this, getName(), variant, getName(),
            //    variant ? variant->getSignature()->getSignatureText() : "n/a", (vflags & po & ~PO_POSITIVE_OPTIONS));
            if ((vflags & po & ~PO_POSITIVE_OPTIONS) || ((vflags & PO_POSITIVE_OPTIONS) && (((vflags & PO_POSITIVE_OPTIONS) & po) != (vflags & PO_POSITIVE_OPTIONS)))) {
                //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s(%s) getProgram(): %p getProgram()->getParseOptions(): %x variant->getFunctionality(): %x\n", this, getName(), variant->getSignature()->getSignatureText(), getProgram(), getProgram()->getParseOptions(), variant->getFunctionality());
                if (!only_user) {
                    std::string class_path = classPath();
                    xsink->raiseException("INVALID-FUNCTION-ACCESS", "parse options do not allow access to builtin " \
                        "%s '%s%s%s(%s)'", !class_path.empty() ? "method" : "function",
                        !class_path.empty() ? class_path.c_str() : "", !class_path.empty() ? "::" : "", getName(),
                        variant->getSignature()->getSignatureText());
                }
                return 0;
            }

            assert(!(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS)) || !(variant->getFlags() & QCF_RUNTIME_NOOP));
        }
    }

    //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s() returning %p %s(%s) class: %s\n", this, getName(), variant, getName(), variant ? variant->getSignature()->getSignatureText() : "n/a", variant && aqf && aqf->className() ? aqf->className() : "n/a");

    return variant;
}

// finds a variant at runtime
const AbstractQoreFunctionVariant* QoreFunction::runtimeFindVariant(ExceptionSink* xsink, const type_vec_t& args,
        const qore_class_private* class_ctx) const {
    int match = -1;

    const AbstractQoreFunctionVariant* variant = nullptr;

    //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s%s%s() vlist: %d ilist: %d args: %p (%d)\n", this,
    //    className() ? className() : "", className() ? "::" : "", getName(), vlist.size(), ilist.size(),
    //    args.size());

    const QoreFunction* aqf = nullptr;
    AbstractFunctionSignature* sig = nullptr;

    // parent class while iterating
    const qore_class_private* last_class = nullptr;
    bool internal_access = false;

    QoreParseOptions ppo = runtime_get_parse_options();

    // iterate through inheritance list
    for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
        bool stop;
        aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
        if (!aqf) {
            break;
        }
        // issue #3070: skip abstract methods
        if (last_class && static_cast<const MethodFunctionBase*>(aqf)->isAbstract()) {
            continue;
        }

        //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s%s%s(...) size: %d last_class: %p ctx: %p: %s\n", this, aqf->className() ? aqf->className() : "", className() ? "::" : "", getName(), ilist.size(), last_class, class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a");

        for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
            // skip if the variant is not accessible or abstract
            if (last_class
                && (skip_method_variant(*i, class_ctx, internal_access)
                    || static_cast<const MethodVariantBase*>(*i)->isAbstract())) {
                    continue;
            }

            // get runtime parse options
            QoreParseOptions po = (*i)->getParseOptions(ppo);
            // if we should ignore "noop" variants
            bool strict_args = static_cast<bool>(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

            int64 vflags = (*i)->getFlags();

            // ignore "runtime noop" variants if necessary
            if (strict_args && (vflags & QCF_RUNTIME_NOOP))
                continue;

            sig = (*i)->getSignature();
            assert(sig);

            //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s(%s) args: %d class: %s class_ctx: %p '%s' nparams: %d\n", this, getName(), sig->getSignatureText(), args.size(), aqf->className() ? aqf->className() : "n/a", class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a", sig->numParams());

            // issue 1507: ensure that calls with no arguments and no params are considered a perfect match
            if (args.empty() && !sig->numParams()) {
                variant = *i;
                break;
            }

            // skip variants with signatures a different number of arguments than provided
            if (sig->numParams() != args.size()) {
                continue;
            }

            int count = 0;
            bool ok = true;
            for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                const QoreTypeInfo* t = sig->getParamTypeInfo(pi);
                const QoreTypeInfo* a = args[pi];

                int rc = QoreTypeInfo::runtimeTypeMatch(t, a);
                //printd(5, "QoreFunction::runtimeFindVariant() '%s' %d ('%s' <=> %s'): rc: %d\n", sig->getSignatureText(), pi, QoreTypeInfo::getName(t), QoreTypeInfo::getName(a), rc);
                if (rc < QTI_WILDCARD) {
                    ok = false;
                    break;
                }
                count += rc;
            }
            if (!ok) {
                continue;
            }

            //printd(5, "QoreFunction::runtimeFindVariant() variant: %p count: %d match: %d (%s)\n", variant, count, match, sig->getSignatureText());
            if (count > match) {
                variant = *i;
                match = count;
            }
        }
        // issue 1229: stop searching the class hierarchy if a match found
        if (stop || variant)
            break;
    }
    return checkVariant(xsink, args, class_ctx, aqf, last_class, internal_access, ppo, variant);
}

DLLLOCAL const AbstractQoreFunctionVariant* QoreFunction::checkVariantDomain(ExceptionSink* xsink,
        const QoreParseOptions& ppo, const AbstractQoreFunctionVariant* variant) const {
    // get runtime parse options
    QoreParseOptions po = variant->getParseOptions(ppo);

    // check parse options
    int64 vflags = variant->getFunctionality();
    // check restrictive flags
    //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s() returning %p %s(%s) vflags: " QLLD " po: "
    //    QLLD " neg: " QLLD "\n", this, getName(), variant, getName(),
    //    variant ? variant->getSignature()->getSignatureText() : "n/a", (vflags & po & ~PO_POSITIVE_OPTIONS));
    if ((vflags & po & ~PO_POSITIVE_OPTIONS) || ((vflags & PO_POSITIVE_OPTIONS)
        && (((vflags & PO_POSITIVE_OPTIONS) & po) != (vflags & PO_POSITIVE_OPTIONS)))) {
        //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s(%s) getProgram(): %p "
        //    "getProgram()->getParseOptions(): %x variant->getFunctionality(): %x\n", this, getName(),
        //    variant->getSignature()->getSignatureText(), getProgram(), getProgram()->getParseOptions(),
        //    variant->getFunctionality());
        std::string class_path = classPath();
        xsink->raiseException("INVALID-FUNCTION-ACCESS", "parse options do not allow access to %s " \
            "'%s%s%s(%s)'", !class_path.empty() ? "method" : "function",
            !class_path.empty() ? class_path.c_str() : "", !class_path.empty() ? "::" : "", getName(),
            variant->getSignature()->getSignatureText());
        return nullptr;
    }

    assert(!(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS)) || !(variant->getFlags() & QCF_RUNTIME_NOOP));
    return variant;
}

const AbstractQoreFunctionVariant* QoreFunction::checkVariant(ExceptionSink* xsink, const type_vec_t& args,
        const qore_class_private* class_ctx, const QoreFunction* aqf, const qore_class_private* last_class,
        bool internal_access, const QoreParseOptions& ppo, const AbstractQoreFunctionVariant* variant) const {
    if (!variant) {
        QoreStringNode* desc = new QoreStringNode("no variant matching '");
        do_call_str(*desc, this, args);
        desc->concat(")' can be found; ");
        desc->concat("the following variants are defined:");

        last_class = nullptr;
        internal_access = false;

        // add variants tested
        // iterate through inheritance list
        for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
            bool stop;
            aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
            if (!aqf)
                break;
            std::string class_path = aqf->classPath();

            for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
                // skip if the variant is not accessible or abstract
                if (last_class
                    && (skip_method_variant(*i, class_ctx, internal_access)
                        || static_cast<const MethodVariantBase*>(*i)->isAbstract())) {
                        continue;
                }
                desc->concat("\n   ");
                if (!class_path.empty()) {
                    desc->sprintf("%s::", class_path.c_str());
                }
                desc->sprintf("%s(%s)", getName(), (*i)->getSignature()->getSignatureText());
            }
            if (stop) {
                break;
            }
        }
        xsink->raiseException("VARIANT-MATCH-ERROR", desc);
    } else {
        variant = checkVariantDomain(xsink, ppo, variant);
    }

    //printd(5, "QoreFunction::checkVariant() this: %p %s() returning %p %s(%s) class: %s\n", this, getName(),
    //    variant, getName(), variant ? variant->getSignature()->getSignatureText() : "n/a",
    //    variant && aqf && aqf->className() ? aqf->className() : "n/a");

    return variant;
}

// finds a variant at runtime
const AbstractQoreFunctionVariant* QoreFunction::runtimeFindExactVariant(ExceptionSink* xsink, const type_vec_t& args,
        const qore_class_private* class_ctx) const {
    const AbstractQoreFunctionVariant* variant = nullptr;

    //printd(5, "QoreFunction::runtimeFindExactVariant() this: %p %s%s%s() vlist: %d ilist: %d args: %p (%d)\n", this,
    //    className() ? className() : "", className() ? "::" : "", getName(), vlist.size(), ilist.size(), args.size());

    const QoreFunction* aqf = nullptr;
    AbstractFunctionSignature* sig = nullptr;

    // parent class while iterating
    const qore_class_private* last_class = nullptr;
    bool internal_access = false;

    QoreParseOptions ppo = runtime_get_parse_options();

    // iterate through inheritance list
    for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
        bool stop;
        aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
        if (!aqf) {
            break;
        }
        // issue #3070: skip abstract methods
        if (last_class && static_cast<const MethodFunctionBase*>(aqf)->isAbstract()) {
            continue;
        }

        //printd(5, "QoreFunction::runtimeFindExactVariant() this: %p %s%s%s(...) size: %d last_class: %p ctx: %p: "
        //    "%s\n", this, aqf->className() ? aqf->className() : "", className() ? "::" : "", getName(),
        //    ilist.size(), last_class, class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a");

        for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
            // skip if the variant is not accessible or abstract
            if (last_class
                && (skip_method_variant(*i, class_ctx, internal_access)
                    || static_cast<const MethodVariantBase*>(*i)->isAbstract())) {
                    continue;
            }

            // get runtime parse options
            QoreParseOptions po = (*i)->getParseOptions(ppo);
            // if we should ignore "noop" variants
            bool strict_args = static_cast<bool>(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

            int64 vflags = (*i)->getFlags();

            // ignore "runtime noop" variants if necessary
            if (strict_args && (vflags & QCF_RUNTIME_NOOP))
                continue;

            sig = (*i)->getSignature();
            assert(sig);

            //printd(5, "QoreFunction::runtimeFindExactVariant() this: %p %s(%s) args: %d class: %s class_ctx: "
            //    "%p '%s' nparams: %d\n", this, getName(), sig->getSignatureText(), args.size(),
            //    aqf->className() ? aqf->className() : "n/a", class_ctx,
            //    class_ctx ? class_ctx->name.c_str() : "n/a", sig->numParams());

            // issue 1507: ensure that calls with no arguments and no params are considered a perfect match
            if (args.empty() && !sig->numParams()) {
                variant = *i;
                break;
            }

            // skip variants with signatures a different number of arguments than provided
            if (sig->numParams() != args.size())
                continue;

            bool ok = true;
            for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                const QoreTypeInfo* t = sig->getParamTypeInfo(pi);
                const QoreTypeInfo* a = args[pi];
                if (t == a || (!t && a == anyTypeInfo))
                    continue;
                ok = false;
                break;
            }
            if (!ok)
                continue;

            variant = *i;
        }
        // issue 1229: stop searching the class hierarchy if a match found
        if (stop || variant)
            break;
    }
    return checkVariant(xsink, args, class_ctx, aqf, last_class, internal_access, ppo, variant);
}

// finds a variant at parse time
const AbstractQoreFunctionVariant* QoreFunction::parseFindVariant(const QoreProgramLocation* loc,
        const type_vec_t& argTypeInfo, const qore_class_private* class_ctx, int& err) const {
    // the lowest match length with the highest score wins
    int score_len = -1;
    // the score for the match of the variant
    int score = -1;
    // the maximum score for the match of the variant
    int max_score = -1;
    // the number of possible matches at runtime (due to missing types at parse time); number of parameters
    int pmatch = -1;
    // the number of arguments matched perfectly in case of a tie score
    int nperfect = -1;
    // number of possible variants
    unsigned npv = 0;

    // pointer to the variant matched
    const AbstractQoreFunctionVariant* variant = nullptr;
    // pointer to the last possible variant matched
    const AbstractQoreFunctionVariant* pvariant = nullptr;
    unsigned num_args = argTypeInfo.size();

    printd(5, "QoreFunction::parseFindVariant() this: %p %s() vlist: %d ilist: %d num_args: %d\n", this, getName(),
        vlist.size(), ilist.size(), num_args);

    QoreFunction* aqf = nullptr;

    // parent class while iterating
    const qore_class_private* last_class = nullptr;
    bool internal_access = false;

    QoreParseOptions po = parse_get_parse_options();

    int cnt = 0;

    // do we need to match at runtime
    bool runtime_match = false;
    bool has_possible_match = false;

    QoreProgram* pgm = getProgram();

    // iterate through inheritance list
    for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
        bool stop;
        aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
        if (!aqf) {
            break;
        }
        printd(5, "QoreFunction::parseFindVariant() %p %s testing function %p\n", this, getName(), aqf);
        assert(!aqf->vlist.empty());

        // check committed list
        for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
            // skip if the variant is not accessible
            if (last_class && skip_method_variant(*i, class_ctx, internal_access)) {
                continue;
            }
            AbstractFunctionSignature* sig = (*i)->getSignature();

            // get variant parse flags
            int64 vflags = (*i)->getFlags();

            // if we should ignore "noop" variants
            bool strict_args = static_cast<bool>((*i)->getParseOptions(po) & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

            // ignore "noop" variants if necessary
            if (strict_args && (vflags & (QCF_NOOP | QCF_RUNTIME_NOOP))) {
                continue;
            }

            // does the variant accept extra arguments?
            bool uses_extra_args = (*i)->hasVarargs();

            ++cnt;

            printd(5, "QoreFunction::parseFindVariant() this: %p checking committed %s(%s) variant: %p sig->pt: %d " \
                "sig->mpt: %d score: %d, args: %d\n", this, getName(), sig->getSignatureText(), variant,
                sig->getParamTypes(), sig->getMinParamTypes(), score, num_args);

            // issue 1507: ensure that calls with no arguments and no params are considered a perfect match
            if (!num_args && !sig->numParams()) {
                variant = *i;
                break;
            }

            // skip variants with signatures with fewer possible elements than the best match already
            if ((int)(sig->numParams() * QTI_IDENT) >= score) {
                int variant_pmatch = 0;
                int pscore = 0;
                int max_pscore = 0;
                int variant_nperfect = 0;
                bool variant_runtime_match = false;
                bool variant_hard_match = false;  // true if missing type info (must do runtime dispatch)
                bool variant_soft_match = false;  // true if union type partial match (may do runtime dispatch)
                bool ok = true;

                for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                    const QoreTypeInfo* t = sig->getParamTypeInfo(pi);
                    bool pos_has_arg = num_args && num_args > pi;
                    const QoreTypeInfo* a = pos_has_arg ? argTypeInfo[pi] : nullptr;
                    if (pos_has_arg) {
                        pos_has_arg = QoreTypeInfo::hasType(a);
                    }

                    //printd(5, "QoreFunction::parseFindVariant() %s(%s) committed pi: %d num_args: %d t: %s "
                    //    "(has type: %d) a: %s (%p) t->parseAccepts(a): %d\n", getName(), sig->getSignatureText(), pi,
                    //    num_args, QoreTypeInfo::getName(t), QoreTypeInfo::hasType(t), QoreTypeInfo::getName(a), a,
                    //    QoreTypeInfo::parseAccepts(t, a));

                    qore_type_result_e rc = QTI_UNASSIGNED;
                    qore_type_result_e max_rc = QTI_UNASSIGNED;
                    if (QoreTypeInfo::hasType(t)) {
                        if (!QoreTypeInfo::hasType(a)) {
                            if (pi < num_args) {
                                // we are missing parse-time type information, we need to match at runtime (HARD)
                                variant_runtime_match = true;
                                variant_hard_match = true;
                                break;
                            } else if (sig->hasDefaultArg(pi)) {
                                rc = max_rc = QTI_IGNORE;
                            } else {
                                a = nothingTypeInfo;
                            }
                        } else if (QoreTypeInfo::isType(a, NT_NOTHING) && sig->hasDefaultArg(pi)) {
                            rc = max_rc = QTI_IDENT;
                        }
                    }

                    if (rc == QTI_UNASSIGNED) {
                        bool may_not_match = false;
                        bool may_need_filter = false;
                        rc = QoreTypeInfo::parseAccepts(t, a, may_not_match, may_need_filter, max_rc, true);
                        //printd(5, "QoreFunction::parseFindVariant() %s(%s) pi: %d (%s <= %s) rc: %d max_rc: %d "
                        //    "may_not_match: %d\n", getName(), sig->getSignatureText(), pi, QoreTypeInfo::getName(t),
                        //    QoreTypeInfo::getName(a), rc, max_rc, may_not_match);
                        // if we might not match, we have a soft match (union type partial match)
                        if (may_not_match) {
                            variant_soft_match = true;
                            variant_runtime_match = true;  // soft match also requires runtime dispatch flag
                            // For soft matches, treat as a match but mark that runtime dispatch may be needed
                            // Continue to next parameter instead of trying other accept specs
                            if (rc == QTI_IDENT) {
                                ++variant_nperfect;
                            }
                            // Note: has_possible_match will be set after score comparison if this variant is selected
                            // Don't break or return - continue to next parameter
                        } else {
                            if (rc == QTI_IDENT) {
                                ++variant_nperfect;
                            }
                        }
                    }

                    if (rc == QTI_NOT_EQUAL) {
                        ok = false;
                        // raise a detailed parse exception immediately if there is only one variant
                        if (ilist.size() == 1 && aqf->vlist.singular() && pgm->getParseExceptionSink()) {
                            return doSingleVariantTypeException(loc, pi, aqf->className(), getName(), sig, t, a);
                        }
                        break;
                    }
                    ++variant_pmatch;
                    if (rc != QTI_IGNORE && pos_has_arg) {
                        pscore += rc;
                        if (max_rc == QTI_UNASSIGNED) {
                            max_rc = rc;
                        }
                        max_pscore += max_rc;
                    }
                }

                // Handle runtime matching based on type of mismatch
                if (variant_runtime_match) {
                    runtime_match = true;
                    if (variant) {
                        variant = nullptr;
                    }
                    break;
                }

                //printd(5, "QoreFunction::parseFindVariant() this: %p tested %s(%s) ok: %d pscore: %d max_pscore: %d "
                //    "score: %d max_score: %d variant_pmatch: %d variant_nperfect: %d nperfect: %d "
                //    "variant_runtime_match: %d\n", this, getName(), sig->getSignatureText(), ok, pscore, max_pscore,
                //    score, max_score, variant_pmatch, variant_nperfect, nperfect, variant_runtime_match);
                if (!ok) {
                    printd(5, "QoreFunction::parseFindVariant() %s(%s) variant not ok, skipping\n", getName(), sig->getSignatureText());
                    continue;
                }

                // now check if additional args are present
                if ((sig->numParams() < num_args) && !uses_extra_args && strict_args &&
                    check_extra_args(sig, argTypeInfo)) {
                    continue;
                }

                if (!npv) {
                    pvariant = variant;
                } else {
                    pvariant = nullptr;
                }

                ++npv;

                if ((pscore > score && max_pscore >= max_score)
                    || (pscore == score
                        && (variant_nperfect > nperfect
                            || (variant_nperfect == nperfect
                                && (score_len == -1 || sig->numParams() < (unsigned)score_len))))) {
                    // if we could possibly match less than another variant
                    // then we have to match at runtime
                    printd(5, "QoreFunction::parseFindVariant() %s(%s) score better: pscore=%d score=%d max_pscore=%d "
                        "max_score=%d nperfect=%d variant_nperfect=%d variant_runtime_match=%d\n", getName(),
                        sig->getSignatureText(), pscore, score, max_pscore, max_score, nperfect, variant_nperfect,
                        variant_runtime_match);
                    if (variant_pmatch < pmatch) {
                        printd(5, "QoreFunction::parseFindVariant() %s(%s) variant_pmatch < pmatch, setting runtime\n",
                            getName(), sig->getSignatureText());
                        variant = nullptr;
                        runtime_match = true;
                        break;
                    } else {
                        // only set variant if it's the longest absolute match and the
                        // longest potential match
                        pmatch = variant_pmatch;
                        score = pscore;
                        nperfect = variant_nperfect;
                        score_len = sig->numParams();
                        printd(5, "QoreFunction::parseFindVariant() assigning variant %p %s(%s)\n", *i, getName(),
                            sig->getSignatureText());
                        variant = *i;
                    }
                } else if (variant_pmatch && (variant_pmatch >= pmatch || max_pscore >= max_score)) {
                    if (variant_soft_match && variant) {
                        // soft-match variant (union type partial match) cannot override a
                        // definitively-selected variant — the definitive variant handles ALL
                        // union components safely, while the soft-match one handles only SOME
                        // mark as possible to allow post-loop fallback if no definitive match found
                        has_possible_match = true;
                    } else {
                        // if we could possibly match less than another variant
                        // then we have to match at runtime
                        variant = nullptr;
                        pmatch = variant_pmatch;
                        score_len = -1;
                    }
                }
            }
        }

        // stop searching if we have to match at runtime
        if (runtime_match) {
            assert(!variant);
            break;
        }

        if (runtime_match) {
            if (variant) {
                variant = nullptr;
            }
            break;
        }
        // issue 1229: stop searching the class hierarchy if a match found
        if (stop || variant) {
            break;
        }
    }

    assert(!(runtime_match && variant));

    // if no definitive match was found but there are possible (soft-match)
    // variants that could match at runtime, use runtime dispatch
    if (!variant && has_possible_match && !runtime_match) {
        runtime_match = true;
    }

    // if we only have one possible variant, then assign it, even it it's not a guaranteed match
    if (!variant && pvariant) {
        variant = pvariant;
    } else if (!variant && !runtime_match && pmatch == -1 && pgm->getParseExceptionSink()) {
        QoreStringNode* desc = new QoreStringNode("no variant matching '");
        do_call_str(*desc, this, argTypeInfo);
        desc->concat("' can be found; ");
        if (!cnt) {
            desc->concat("no variants were accessible in this context");
        } else {
            desc->concat("the following variants were tested:");

            last_class = 0;
            internal_access = false;

            // add variants tested
            // iterate through inheritance list
            for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
                bool stop;
                aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
                if (!aqf) {
                    break;
                }
                const char* class_name = aqf->className();

                for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
                    // skip if the variant is not accessible
                    if (last_class && skip_method_variant(*i, class_ctx, internal_access)) {
                        continue;
                    }

                    // if we should ignore "noop" variants
                    bool strict_args = static_cast<bool>((*i)->getParseOptions(po) & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

                    // ignore "noop" variants if necessary
                    if (strict_args && ((*i)->getFlags() & (QCF_NOOP | QCF_RUNTIME_NOOP))) {
                        continue;
                    }

                    desc->concat("\n   ");
                    if (class_name) {
                        desc->sprintf("%s::", class_name);
                    }
                    desc->sprintf("%s(%s)", getName(), (*i)->getSignature()->getSignatureText());
                }
                if (stop) {
                    break;
                }
            }
        }
        qore_program_private::makeParseException(pgm, *loc, "PARSE-TYPE-ERROR", desc);
        if (!err) {
            err = -1;
        }
    } else if (variant) {
        int64 flags = variant->getFlags();
        if (flags & (QCF_NOOP | QCF_RUNTIME_NOOP)) {
            QoreStringNode* desc = getNoopError(this, aqf, variant);
            desc->concat("; to disable this warning, use '%disable-warning invalid-operation' in your code");
            qore_program_private::makeParseWarning(pgm, *loc, QP_WARN_CALL_WITH_TYPE_ERRORS,
                "CALL-WITH-TYPE-ERRORS", desc);
        }

        AbstractFunctionSignature* sig = variant->getSignature();
        if (!variant->hasVarargs() && num_args > sig->numParams()) {
            if (warn_excess_args(loc, this, argTypeInfo, sig) && !err) {
                err = -1;
            }
        }
    }

    /*
    {
        QoreString desc("(");
        for (int i = 0; i < argTypeInfo.size(); ++i)
            desc.sprintf("%s, ", QoreTypeInfo::getName(argTypeInfo[i]));
        if (desc.size() > 1)
            desc.terminate(desc.size() - 2);
        desc.concat(")");
        printd(0, "QoreFunction::parseFindVariant() this: %p %s%s%s() pmatch: %d call args: '%s' returning %p "
            "(class %s) %s(%s) flags: %lld runtime_match: %d\n", this, className() ? className() : "",
            className() ? "::" : "", getName(), pmatch, desc.c_str(), variant,
            variant && className() ? reinterpret_cast<const MethodVariant*>(variant)->getClass()->getName() : "n/a",
            getName(), variant ? variant->getSignature()->getSignatureText() : "n/a",
            variant ? variant->getFlags() : 0ll, runtime_match);
    }
    */

    /*
    printd(5, "QoreFunction::parseFindVariant() this: %p %s%s%s() returning %p %s(%s) flags: %lld (varargs: %s) "
        "num_args: %d (line: %d)\n",
        this, className() ? className() : "", className() ? "::" : "", getName(), variant, getName(),
        variant ? variant->getSignature()->getSignatureText() : "n/a", variant ? variant->getFlags() : 0ll,
        (variant ? variant->getFlags() : 0ll) & QCF_USES_EXTRA_ARGS ? "true" : "false",
        num_args, loc ? loc->start_line : -1);
    */

    return variant;
}

// if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL, then it is
// identified at run time
QoreValue QoreFunction::evalFunction(const AbstractQoreFunctionVariant* variant, const QoreListNode* args,
        QoreProgram *pgm, RuntimeConfig& rc, ExceptionSink* xsink) const {
    const char* fname = getName();

    // issue #3027: catch recursive references during parse initialization
    if (!parse_init_done) {
        SimpleRefHolder<QoreStringNode> desc(new QoreStringNode("recursive reference to "));
        const char* class_name = className();
        if (class_name) {
            desc->sprintf("method %s::", class_name);
        } else {
            desc->concat("function ");
        }
        desc->sprintf("%s(", fname);
        if (variant) {
            desc->concat(variant->getSignature()->getSignatureText());
        }
        desc->concat(") during parse initialization");
        xsink->raiseException("PARSE-EXCEPTION", desc.release());
        return QoreValue();
    }

    CodeEvaluationHelper ceh(xsink, rc, this, variant, fname, args);
    if (*xsink) {
        return QoreValue();
    }
    // issue #3024: make the caller's call context available
    ProgramCallContextHelper pcch(pgm);
    return variant->evalFunction(xsink, ceh);
}

// if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL, then it is
// identified at run time
QoreValue QoreFunction::evalFunctionTmpArgs(const AbstractQoreFunctionVariant* variant, QoreListNode* args,
        QoreProgram *pgm, RuntimeConfig& rc, ExceptionSink* xsink) const {
    const char* fname = getName();
    CodeEvaluationHelper ceh(xsink, rc, this, variant, fname, args);
    if (*xsink) {
        return QoreValue();
    }
    // issue #3024: make the caller's call context available
    ProgramCallContextHelper pcch(pgm);
    return variant->evalFunction(xsink, ceh);
}

// finds a variant and checks variant capabilities against current
// program parse options
QoreValue QoreFunction::evalDynamic(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) const {
    const char* fname = getName();
    const AbstractQoreFunctionVariant* variant = 0;
    CodeEvaluationHelper ceh(xsink, rc, this, variant, fname, args);
    if (*xsink) {
        return QoreValue();
    }
    return variant->evalFunction(xsink, ceh);
}

void QoreFunction::addBuiltinVariant(AbstractQoreFunctionVariant* variant) {
    assert(variant->getCallType() == CT_BUILTIN);
#ifdef DEBUG
    // FIXME: this algorithm is no longer valid due to default arguments
    // does not detect ambiguous signatures
    AbstractFunctionSignature* sig = variant->getSignature();
    // check for duplicate parameter signatures
    for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        AbstractFunctionSignature* vs = (*i)->getSignature();
        unsigned tp = vs->numParams();
        if (tp != sig->numParams())
            continue;
        if (!tp) {
            printd(0, "BuiltinFunctionBase::addBuiltinVariant() this: %p %s(%s) added twice: %p, %p\n", this, getName(), sig->getSignatureText(), *i, variant);
            assert(false);
        }
        bool ok = false;
        for (unsigned pi = 0; pi < tp; ++pi) {
            if (vs->getParamTypeInfo(pi) != sig->getParamTypeInfo(pi)) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            printd(0, "BuiltinFunctionBase::addBuiltinVariant() this: %p %s(%s) added twice: %p, %p\n", this, getName(), sig->getSignatureText(), *i, variant);
            assert(false);
        }
    }
#endif
    if (!has_builtin) {
        has_builtin = true;
    }
    if (all_priv) {
        all_priv = false;
    }
    if (!has_pub) {
        has_pub = true;
    }
    addVariant(variant);
}

UserVariantExecHelper::~UserVariantExecHelper() {
    if (!uvb) {
        return;
    }
    UserSignature* sig = uvb->getUserSignature();
    // uninstantiate local vars from param list
    for (unsigned i = 0; i < sig->numParams(); ++i) {
        //printd(5, "UserVariantExecHelper::~UserVariantExecHelper() this: %p %s %d/%d %p lv: %s (%s)\n", this,
        //    sig->getSignatureText(), i, sig->numParams(), sig->lv[i], sig->lv[i]->getName(),
        //    sig->lv[i]->getValueTypeName());
        sig->lv[i]->uninstantiate(xsink);
    }
}

UserVariantBase::UserVariantBase(StatementBlock *b, int n_sig_first_line, int n_sig_last_line, QoreValue params,
        RetTypeInfo* rv, bool synced)
        : signature(n_sig_first_line, n_sig_last_line, params, rv,
            b ? b->pwo.parse_options : parse_get_parse_options()), statements(b),
        gate(synced ? new VRMutex : nullptr), pgm(getProgram()), recheck(false), init(false) {
    //printd(5, "UserVariantBase::UserVariantBase() this: %p params: %p rv: %p b: %p synced: %d\n", params, rv, b,
    //    synced);
}

UserVariantBase::~UserVariantBase() {
    delete cached_aot_ctx;
    delete cached_ir;
    delete gate;
    delete statements;
}

QoreParseOptions UserVariantBase::getParseOptions(const QoreParseOptions& po) const {
    return statements ? statements->pwo.parse_options : po;
}

const std::vector<LocalVar*>& UserVariantBase::getBodyLocals() const {
    if (cached_aot_ctx) {
        return cached_aot_ctx->all_body_locals;
    }
    assert(cached_ir);
    return cached_ir->all_body_locals;
}

bool UserVariantBase::areAllBodyLocalsIROnly() const {
    if (cached_aot_ctx) {
        return cached_aot_ctx->all_body_locals_ir_only;
    }
    return all_body_locals_ir_only;
}

const std::vector<LocalVar*>& UserVariantBase::getASTVisibleBodyLocals() const {
    if (cached_aot_ctx) {
        return cached_aot_ctx->all_body_locals;
    }
    assert(cached_ir);
    return cached_ir->ast_visible_body_locals;
}

void UserVariantBase::parseInitPushLocalVars(const QoreTypeInfo* classTypeInfo) {
    signature.parseInitPushLocalVars(classTypeInfo);
}

void UserVariantBase::parseInitPopLocalVars() {
    signature.parseInitPopLocalVars();
}

// instantiates arguments and sets up the argv variable
int UserVariantBase::setupCall(CodeEvaluationHelper *ceh, ReferenceHolder<QoreListNode>& argv, ExceptionSink* xsink)
        const {
    QoreListNodeEvalOptionalRefHolder* args = ceh ? &ceh->getArgHolder() : nullptr;

    unsigned num_args = args ? args->size() : 0;
    // instantiate local vars from param list
    unsigned num_params = signature.numParams();

    for (unsigned i = 0; i < num_params; ++i) {
        QoreValue val;
        if (args && *args) {
            if (args->canEdit()) {
                assert(**args);
                val = qore_list_private::get(***args)->takeExists(i);
            } else {
                val = (*args)->retrieveEntry(i).refSelf();
            }

            // Apply type filtering for complex hash parameters
            const QoreTypeInfo* paramTypeInfo = signature.getParamTypeInfo(i);
            if (paramTypeInfo && val.getType() == NT_HASH) {
                QoreTypeInfo::acceptInputParam(paramTypeInfo, i, signature.getName(i), val, xsink);
                if (*xsink) {
                    return -1;
                }
            }

            signature.lv[i]->instantiate(val);
            continue;
        }

        //printd(5, "UserVariantBase::setupCall() eval %d: instantiating param lvar %p ('%s') (exp nt: %d '%s')\n",
        //    i, signature.lv[i], signature.lv[i]->getName(), np.getType(), np.getTypeName());

        signature.lv[i]->instantiate(QoreValue());
    }

    // if there are more arguments than parameters
    printd(5, "UserVariantBase::setupCall() params: %d args: %d\n", num_params, num_args);

    if (num_params < num_args) {
        argv = new QoreListNode(autoTypeInfo);

        for (unsigned i = 0; i < (num_args - num_params); i++) {
            // here we try to take the reference from args if possible
            if (args->canEdit()) {
                argv->push(qore_list_private::get(***args)->takeExists(i + num_params), nullptr);
            } else {
                QoreValue n{};  // value-initialized to NOTHING (bits=0)
                if (args)
                    n = (*args)->retrieveEntry(i + num_params);
                //AbstractQoreNode* n = args ? const_cast<AbstractQoreNode*>(args->get_referenced_entry(i + num_params)) : 0;
                argv->push(n.refSelf(), nullptr);
            }
        }
    }

    return 0;
}

// helper: collect LVList locals into a vector
static void collectBlockLocals(const LVList* lvars, std::vector<LocalVar*>& locals) {
    if (lvars) {
        for (unsigned i = 0; i < lvars->size(); ++i) {
            locals.push_back(lvars->lv[i]);
        }
    }
}

// forward declaration — non-static for non-SCU visibility
void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);

// helper: collect all locals from a single statement and recurse into nested blocks.
// Recurses into statement types that are fully lowered to IR (if/for/while/try/switch/block/on_block_exit).
// With Phase 1 inline handler lowering, on_block_exit handler code is lowered directly into the
// parent IR context, so handler-internal locals must be collected.
// Statements delegated to AST via special IR opcodes (context, summarize, assert)
// are skipped because the AST's LVListInstantiator handles their locals.
static void collectStatementLocals(const AbstractStatement* stmt, std::vector<LocalVar*>& locals) {
    if (!stmt) {
        return;
    }
    if (auto* block = dynamic_cast<const StatementBlock*>(stmt)) {
        collectAllStatementLocals(block, locals);
    } else if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
        collectBlockLocals(if_stmt->getLVList(), locals);
        collectAllStatementLocals(if_stmt->getIfCode(), locals);
        collectAllStatementLocals(if_stmt->getElseCode(), locals);
    } else if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
        collectBlockLocals(for_stmt->getLVList(), locals);
        collectAllStatementLocals(for_stmt->getCode(), locals);
    } else if (auto* while_stmt = dynamic_cast<const WhileStatement*>(stmt)) {
        // Also covers DoWhileStatement (inherits from WhileStatement)
        collectBlockLocals(while_stmt->getLVList(), locals);
        collectAllStatementLocals(while_stmt->getCode(), locals);
    } else if (auto* try_stmt = dynamic_cast<const TryStatement*>(stmt)) {
        collectAllStatementLocals(try_stmt->getTryBlock(), locals);
        // Collect the catch variable (it's a separate LocalVar, not part of catch_block's LVList)
        if (LocalVar* catch_var = try_stmt->getCatchVar()) {
            locals.push_back(catch_var);
        }
        collectAllStatementLocals(try_stmt->getCatchBlock(), locals);
    } else if (auto* sw_stmt = dynamic_cast<const SwitchStatement*>(stmt)) {
        collectBlockLocals(sw_stmt->lvars, locals);
        const CaseNode* cn = sw_stmt->getCases();
        while (cn) {
            collectAllStatementLocals(cn->code, locals);
            cn = cn->next;
        }
    } else if (auto* foreach_stmt = dynamic_cast<const ForEachStatement*>(stmt)) {
        // Collect foreach locals for both reference and non-reference iteration.
        // Both are now fully lowered to IR.
        collectBlockLocals(foreach_stmt->getLVList(), locals);
        collectAllStatementLocals(foreach_stmt->getCode(), locals);
    } else if (auto* debug_stmt = dynamic_cast<const DebugStatement*>(stmt)) {
        // Debug is now fully lowered to IR: expression form has no locals,
        // block form recurses into the statement block.
        if (StatementBlock* block = debug_stmt->getBlock()) {
            collectAllStatementLocals(block, locals);
        }
    } else if (auto* obe_stmt = dynamic_cast<const OnBlockExitStatement*>(stmt)) {
        // Phase 1 inline lowering: on_exit handler code is directly lowered into the
        // parent IR context at normal exit points (fall-through, break, continue, return).
        // Handler-internal locals must be collected into pre_instantiated_locals so that
        // evalTiered() pre-instantiates them on the TLS stack before IR execution.
        // Without this, the IR interpreter cannot find handler locals (ThreadLocalVariableData::find asserts).
        collectAllStatementLocals(obe_stmt->getCode(), locals);
    }
    // ContextStatement, SummarizeStatement, AssertStatement: these generate special
    // IR opcodes that call into the AST, which handles their locals via LVListInstantiator. Skip them.
}

// Recursively collect all local variables from a StatementBlock and all nested blocks
// that are fully lowered to IR.  This collects the block's own LVList plus all nested
// block locals from if/for/while/try/switch statements.
// Note: this function is non-static because it's also used by StatementBlock.cpp for
// top-level code IR lowering.
void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals) {
    if (!block) {
        return;
    }
    // Collect this block's own locals
    collectBlockLocals(block->getLVList(), locals);
    // Recurse into child statements
    for (auto it = block->getStatements().begin(); it != block->getStatements().end(); ++it) {
        collectStatementLocals(*it, locals);
    }
}

// Forward declaration for mutual recursion with collectStmtSlotFromStatement
void collectStmtSlotStatements(const StatementBlock* block,
        std::vector<const AbstractStatement*>& stmts);

// helper: collect stmt_slot statements (OnBlockExit handler code + reference Foreach)
// from a single statement, in depth-first order matching IR lowering
static void collectStmtSlotFromStatement(const AbstractStatement* stmt,
        std::vector<const AbstractStatement*>& stmts) {
    if (!stmt) {
        return;
    }
    if (auto* obe = dynamic_cast<const OnBlockExitStatement*>(stmt)) {
        stmts.push_back(obe->getCode());
    } else if (auto* block = dynamic_cast<const StatementBlock*>(stmt)) {
        collectStmtSlotStatements(block, stmts);
    } else if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
        collectStmtSlotStatements(if_stmt->getIfCode(), stmts);
        collectStmtSlotStatements(if_stmt->getElseCode(), stmts);
    } else if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
        collectStmtSlotStatements(for_stmt->getCode(), stmts);
    } else if (auto* while_stmt = dynamic_cast<const WhileStatement*>(stmt)) {
        collectStmtSlotStatements(while_stmt->getCode(), stmts);
    } else if (auto* try_stmt = dynamic_cast<const TryStatement*>(stmt)) {
        collectStmtSlotStatements(try_stmt->getTryBlock(), stmts);
        collectStmtSlotStatements(try_stmt->getCatchBlock(), stmts);
    } else if (auto* sw_stmt = dynamic_cast<const SwitchStatement*>(stmt)) {
        const CaseNode* cn = sw_stmt->getCases();
        while (cn) {
            collectStmtSlotStatements(cn->code, stmts);
            cn = cn->next;
        }
    } else if (auto* foreach_stmt = dynamic_cast<const ForEachStatement*>(stmt)) {
        if (foreach_stmt->isRef()) {
            // Reference foreach: the whole ForEachStatement is a stmt_slot
            stmts.push_back(foreach_stmt);
        } else {
            // Non-reference foreach: recurse into body for nested OBE handlers
            collectStmtSlotStatements(foreach_stmt->getCode(), stmts);
        }
    } else if (auto* debug_stmt = dynamic_cast<const DebugStatement*>(stmt)) {
        if (StatementBlock* block = debug_stmt->getBlock()) {
            collectStmtSlotStatements(block, stmts);
        }
    }
}

void collectStmtSlotStatements(const StatementBlock* block,
        std::vector<const AbstractStatement*>& stmts) {
    if (!block) {
        return;
    }
    for (auto it = block->getStatements().begin(); it != block->getStatements().end(); ++it) {
        collectStmtSlotFromStatement(*it, stmts);
    }
}

// helper: check if an IR basic block has a terminator instruction
static bool irBlockHasTerminatorFunc(const QoreIRBasicBlock* block) {
    if (!block || block->instructions.empty()) {
        return false;
    }
    auto op = block->instructions.back()->opcode;
    switch (op) {
        case QoreIROpcode::Return:
        case QoreIROpcode::ReturnNothing:
        case QoreIROpcode::Br:
        case QoreIROpcode::BrIf:
        case QoreIROpcode::Rethrow:
            return true;
        default:
            return false;
    }
}

void UserVariantBase::attemptIRLowering(const char* name) const {
    assert(pgm);
    assert(statements);

    // Make the IR function name unique per variant to avoid name collisions in the
    // JIT's compiled_functions map.  Multiple closures (or overloaded functions) can
    // share the same display name (e.g. "<anonymous closure>"), but each variant has
    // its own LocalVar pointers baked into IR/JIT code, so they must compile to
    // distinct JIT entries.  A monotonic counter is needed in addition to the address
    // because when a variant is destroyed and a new one allocated at the same address,
    // the old JIT cache entry would be returned with stale LocalVar pointers.
    static std::atomic<uint64_t> variant_counter{0};
    std::string unique_name = std::string(name) + "@" + std::to_string((uintptr_t)this)
        + "_" + std::to_string(variant_counter.fetch_add(1));
    QoreIRFunction* func = new QoreIRFunction(unique_name.c_str());
    // Store the return type info for type coercion in Return opcode lowering
    func->return_type_info = getReturnTypeInfo();
    // Record which locals are pre-instantiated by the calling convention so the JIT
    // skips instantiation/uninstantiation for them.
    for (unsigned i = 0; i < signature.numParams(); ++i) {
        LocalVar* lv = signature.lv[i];
        func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
        func->pre_instantiated_cache.insert(lv);
    }
    if (signature.argvid) {
        func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(signature.argvid));
        func->pre_instantiated_cache.insert(signature.argvid);
    }
    if (signature.selfid) {
        func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(signature.selfid));
        func->pre_instantiated_cache.insert(signature.selfid);
    }
    // Collect ALL body locals from the statement tree (top-level + nested blocks from
    // fully-lowered statements).  These are instantiated by evalTiered() before IR/JIT
    // execution so AST Invoke callbacks can find them on the thread-local stack.
    collectAllStatementLocals(statements, func->all_body_locals);
    for (LocalVar* lv : func->all_body_locals) {
        func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
        func->pre_instantiated_cache.insert(lv);
    }
    QoreIRBuilder builder(func);
    auto* entry = func->createBlock("entry");
    builder.setBlock(entry);

    QoreParseContext parse_context(pgm);
    QoreIRLowering lowering(builder, &parse_context);
    std::string error;
    if (!lowering.lowerStatementBlock(statements, error)) {
        ir_lower_failed = true;
        delete func;
        printd(2, "UserVariantBase::attemptIRLowering() '%s' failed: %s\n", name, error.c_str());
        if (pgm) {
            pgm->recordIRFallback((std::string("lowering: ") + error).c_str());
        }
        return;
    }
    if (!irBlockHasTerminatorFunc(builder.getBlock())) {
        builder.createReturnNothing();
    }
    if (!QoreIRVerifier::verify(*func, error)) {
        ir_lower_failed = true;
        delete func;
        printd(2, "UserVariantBase::attemptIRLowering() '%s' verification failed: %s\n", name, error.c_str());
        if (pgm) {
            pgm->recordIRFallback((std::string("verification: ") + error).c_str());
        }
        return;
    }

    // Compute slot IDs, max_value_id, and embed pre-computed fields in instructions
    // This must happen BEFORE compileAllHandlerIRs() to ensure parent slots are populated
    func->computeSlotIdsAndEmbed();

    // Phase A4: Compile all handler bodies to separate IR functions and attach to OnBlockExit instructions
    // This must happen AFTER computeSlotIdsAndEmbed() so handlers can be compiled with correct parent context
    std::string handler_compile_error;
    int handlers_compiled = lowering.compileAllHandlerIRs(handler_compile_error);
    if (!handler_compile_error.empty()) {
        printd(2, "UserVariantBase::attemptIRLowering() '%s' handler compilation: %s\n", name, handler_compile_error.c_str());
    }

    // Classify locals as IR-only vs AST-visible for optimization
    func->computeIROnlyLocals();
    // Check if all body locals are IR-only (enables skipping instantiation in fast call path)
    all_body_locals_ir_only = func->areAllBodyLocalsIROnly();

    // When debugger is enabled, all locals must be on the TLS stack so
    // get_local_vars() and set_local_var_value() can access them
    if (pgm && (pgm->getParseOptions() & PO_ALLOW_DEBUGGER)) {
        if (!func->ir_only_locals.empty()) {
            func->ir_only_locals.clear();
            func->ast_visible_body_locals = func->all_body_locals;
            all_body_locals_ir_only = false;
        }
    }

    // Third pass: set ir_only flags on fused instructions using computed ir_only_locals
    if (!func->ir_only_locals.empty()) {
        for (const auto& block : func->blocks) {
            for (const auto& inst : block->instructions) {
                if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                    auto* fused = static_cast<QoreIRAddAssignLocalIntInstruction*>(inst.get());
                    fused->target_ir_only = fused->target
                        && func->ir_only_locals.count(reinterpret_cast<const void*>(fused->target));
                } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                    auto* fused = static_cast<QoreIRIncrementLocalIntInstruction*>(inst.get());
                    fused->ir_only = fused->local
                        && func->ir_only_locals.count(reinterpret_cast<const void*>(fused->local));
                }
            }
        }
    }

    // Conservative approach: assume argv and self are used if they exist
    // This allows the framework to skip ArgvContextHelper and SelfFunctionCallHelper
    // instantiation when both flags are false, but for now both default to the presence
    // of argv/self in the function signature. More precise analysis can be added later
    // to detect when they're actually unused in the IR body.
    uses_argv = signature.argvid != nullptr;
    uses_self = signature.selfid != nullptr;
    // Initialize type profiling for guards
    func->initGuardProfiles();

    // Build cached pre-instantiated set to avoid per-call allocation in evalTiered()
    // This includes: parameters + argvid + selfid + ast_visible_body_locals
    // (NOT all_body_locals - only the AST-visible subset)
    auto* cached_pre_inst = new std::unordered_set<const LocalVar*>();
    // Add parameter locals
    for (unsigned i = 0; i < signature.numParams(); ++i) {
        cached_pre_inst->insert(signature.lv[i]);
    }
    // Add argv
    if (signature.argvid) {
        cached_pre_inst->insert(signature.argvid);
    }
    // Add selfid (if it exists - will be conditional at runtime)
    if (signature.selfid) {
        cached_pre_inst->insert(signature.selfid);
    }
    // Add ast_visible_body_locals (the filtered subset, not all_body_locals)
    // Skip closure-use vars: they must NOT be pre-instantiated because the cvstack
    // is a LIFO stack and block-scope cleanup pops from the top.  Pre-instantiating
    // all closure-use vars at once creates wrong stack ordering, causing
    // UninstantiateLocal to pop a different variable than intended.
    // The IR interpreter handles them on-demand via ensureLocalInstantiated().
    for (LocalVar* lv : func->ast_visible_body_locals) {
        if (!lv->closureUse()) {
            cached_pre_inst->insert(lv);
        }
    }
    func->cached_pre_instantiated = cached_pre_inst;

    // Build param_slot_ids: maps param index → slot_id for direct param passing.
    // Also build param_local_vars for type information access during direct param processing.
    // This allows IR-to-IR calls to bypass TLS entirely by pre-populating
    // the slot cache from caller-provided values.
    bool all_params_ir_only = true;
    bool all_params_have_slots = true;
    for (unsigned i = 0; i < signature.numParams(); ++i) {
        auto it = func->local_var_slots.find(signature.lv[i]);
        if (it != func->local_var_slots.end()) {
            func->param_slot_ids[static_cast<int>(i)] = it->second;
            func->param_local_vars[static_cast<int>(i)] = signature.lv[i];
        } else {
            // Param only used in fused instructions — no slot_id, can't pre-populate cache
            all_params_have_slots = false;
        }
        const void* key = reinterpret_cast<const void*>(signature.lv[i]);
        if (!func->ir_only_locals.count(key)) {
            all_params_ir_only = false;
        }
    }
    // If function uses argv (variadic), inline direct path would always pass NOTHING for argv.
    // Ineligible: must go through qore_rt_call_fast which builds argv from excess args.
    // Direct params eligible: all params ir_only AND all have slot IDs AND not variadic
    func->direct_params_eligible = all_params_ir_only && all_params_have_slots && !signature.argvid;

    cached_ir = func;
    current_tier.store(TIER_IR, std::memory_order_release);
    printd(3, "UserVariantBase::attemptIRLowering() '%s' promoted to IR tier (%d guards)\n",
        name, func->num_guards);
}

// Check if a callee is eligible for Approach B (direct LLVM arg passing).
// Requirements: all params and body locals are IR-only, no varargs,
// no reference-type params, no closure-captured params, same QoreProgram.
static bool isApproachBEligible(const UserVariantBase* uvb, const QoreIRFunction* callee_ir,
        QoreProgram* root_pgm) {
    bool debug = getenv("QORE_BATCH_DEBUG") != nullptr;

    // Must be same program (no ProgramThreadCountContextHelper needed)
    if (uvb->pgm != root_pgm) {
        if (debug) {
            printd(5, "  APPROACH_B: '%s' ineligible: different program\n",
                callee_ir->name.c_str());
        }
        return false;
    }

    // All body locals must be IR-only (or no body locals at all).
    // areAllBodyLocalsIROnly() returns false when all_body_locals is empty,
    // but for Approach B, no body locals is trivially fine.
    if (!callee_ir->all_body_locals.empty() && !callee_ir->areAllBodyLocalsIROnly()) {
        if (debug) {
            printd(5, "  APPROACH_B: '%s' ineligible: body locals not all IR-only"
                " (body_locals=%d, ir_only=%d)\n",
                callee_ir->name.c_str(), (int)callee_ir->all_body_locals.size(),
                (int)callee_ir->ir_only_locals.size());
        }
        return false;
    }

    const UserSignature* sig = uvb->getUserSignature();

    // If the callee uses argvid in its IR (it's in ir_only_locals), it would get
    // NOTHING in the fast entry which is wrong.  Check that argvid is not referenced.
    if (sig->argvid) {
        const void* argv_key = reinterpret_cast<const void*>(sig->argvid);
        if (callee_ir->ir_only_locals.count(argv_key)) {
            if (debug) {
                printd(5, "  APPROACH_B: '%s' ineligible: argvid referenced in IR\n",
                    callee_ir->name.c_str());
            }
            return false;
        }
    }

    // Check all params
    unsigned num_params = sig->numParams();
    for (unsigned i = 0; i < num_params; ++i) {
        const LocalVar* lv = sig->lv[i];
        const void* key = reinterpret_cast<const void*>(lv);

        // Param must be IR-only
        if (!callee_ir->ir_only_locals.count(key)) {
            if (debug) {
                printd(5, "  APPROACH_B: '%s' ineligible: param '%s' not IR-only\n",
                    callee_ir->name.c_str(), lv->getName());
            }
            return false;
        }

        // No closure-captured params
        if (lv->closureUse()) {
            if (debug) {
                printd(5, "  APPROACH_B: '%s' ineligible: param '%s' closure-captured\n",
                    callee_ir->name.c_str(), lv->getName());
            }
            return false;
        }

        // No reference-type params
        if (QoreTypeInfo::isReference(lv->getTypeInfo())) {
            if (debug) {
                printd(5, "  APPROACH_B: '%s' ineligible: param '%s' is reference type\n",
                    callee_ir->name.c_str(), lv->getName());
            }
            return false;
        }
    }

    if (debug) {
        printd(5, "  APPROACH_B: '%s' eligible (%d params)\n",
            callee_ir->name.c_str(), num_params);
    }
    return true;
}

// Collect direct callees from an IR function's CallDirect instructions.
// Returns a vector of BatchCallee entries for callees that have cached IR.
static std::vector<QoreJIT::BatchCallee> collectDirectCallees(const QoreIRFunction& func,
        QoreProgram* root_pgm) {
    std::vector<QoreJIT::BatchCallee> callees;
    std::unordered_set<const AbstractQoreFunctionVariant*> seen;

    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            const AbstractQoreFunctionVariant* variant = nullptr;
            const char* callee_name = nullptr;

            if (inst->opcode == QoreIROpcode::CallDirect) {
                const auto* direct = static_cast<const QoreIRCallDirectInstruction*>(inst.get());
                variant = direct->variant;
                if (direct->func) {
                    callee_name = direct->func->getName();
                }
            } else if (inst->opcode == QoreIROpcode::Invoke) {
                const auto* inv = static_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (inv->invoke_opcode == QoreIROpcode::CallDirect) {
                    const auto* call = dynamic_cast<const FunctionCallNode*>(
                            inv->expr.getInternalNode());
                    if (call) {
                        variant = call->getVariant();
                        if (call->getFunction()) {
                            callee_name = call->getFunction()->getName();
                        }
                    }
                }
            }

            if (!variant || seen.count(variant)) {
                continue;
            }
            seen.insert(variant);

            // Check if the variant is a user function eligible for fast calls
            const UserVariantBase* uvb = variant->getUserVariantBase();
            if (!uvb || !uvb->isStaticallyFastCallEligible()) {
                continue;
            }

            // If the callee doesn't have cached IR yet, attempt IR lowering now.
            // This enables batch compilation even when the callee hasn't been called yet
            // (common in --exec-mode=jit where the caller is compiled on first call).
            const QoreIRFunction* callee_ir = uvb->getCachedIR();
            if (!callee_ir && callee_name) {
                // Force IR lowering for the callee — must go through forceIRLowering
                // which handles the call_once flag properly
                uvb->forceIRLowering(callee_name);
                callee_ir = uvb->getCachedIR();
                if (!callee_ir) {
                    continue;
                }
            }
            if (!callee_ir) {
                continue;
            }

            // Skip self-recursion (the root function is already being compiled)
            if (callee_ir->name == func.name) {
                continue;
            }

            // Include callees even if already JIT-compiled — the batch module
            // can emit direct LLVM calls to the callee's in-module function,
            // bypassing the qore_rt_call_fast() runtime dispatch overhead.

            // Check Approach B eligibility (direct LLVM arg passing)
            bool approach_b = isApproachBEligible(uvb, callee_ir, root_pgm);
            unsigned num_params = uvb->getUserSignature()->numParams();

            callees.push_back(QoreJIT::BatchCallee{
                callee_ir,
                uvb->getDeoptCounterPtr(),
                variant,
                approach_b,
                num_params
            });
        }
    }

    return callees;
}

void UserVariantBase::attemptJITCompilation() const {
    assert(cached_ir);

    // Atomically claim JIT compilation (CAS 0→1).
    // This is the single point of guard — callers should NOT do their own CAS.
    int expected = 0;
    if (!jit_compile_state.compare_exchange_strong(expected, 1)) {
        return;  // Already enqueued or completed
    }

    // Enqueue for background JIT compilation instead of compiling synchronously.
    // This allows I/O threads and other critical threads to continue executing IR code
    // while the background thread performs expensive LLVM compilation.
    //
    // The function stays at IR tier until background compilation finishes and promotes it.
    // To avoid deadlocks when JIT is triggered from synchronized functions, we use
    // background compilation which doesn't hold any Qore mutexes during the lengthy
    // LLVM phases.

    void* deopt_ptr = getDeoptCounterPtr();

    // Collect direct callees that have cached IR for batch compilation
    auto callees = collectDirectCallees(*cached_ir, pgm);

    if (!callees.empty()) {
        if (getenv("QORE_BATCH_DEBUG")) {
            printd(5, "BATCH: '%s' enqueued with %d callees:",
                cached_ir->name.c_str(), (int)callees.size());
            for (const auto& c : callees) {
                printd(5, " %s%s", c.ir_func->name.c_str(),
                    c.approach_b_eligible ? "(B)" : "");
            }
            printd(5, "\n");
        }
        printd(3, "UserVariantBase::attemptJITCompilation() '%s' batch enqueued with %d callees\n",
            cached_ir->name.c_str(), (int)callees.size());
        QoreJIT::instance().enqueueBgCompile(this, cached_ir, deopt_ptr, &callees);
    } else {
        // No eligible callees: single-function background compilation
        QoreJIT::instance().enqueueBgCompile(this, cached_ir, deopt_ptr);
    }

    printd(3, "UserVariantBase::attemptJITCompilation() '%s' enqueued for background compilation\n",
        cached_ir->name.c_str());
}

void UserVariantBase::eagerlyCompileForExecMode(const char* name, qore_exec_mode_t exec_mode) const {
    // Eagerly compile to IR when --exec-mode=ir, --exec-mode=jit, or --exec-mode=tiered
    // This respects the user's explicit request to execute in a specific mode
    // rather than using the threshold-based promotion mechanism

    if (exec_mode != QEM_IR && exec_mode != QEM_JIT && exec_mode != QEM_TIERED) {
        return;
    }

    // Attempt IR lowering (bypasses threshold check via call_once)
    std::call_once(ir_lower_once, [this, name]() {
        attemptIRLowering(name);
    });

    // For JIT mode, set IR tier so function executes immediately, then enqueue
    // for background JIT compilation. This avoids blocking startup while LLVM
    // compiles each function synchronously (~23ms each).
    if (exec_mode == QEM_JIT && cached_ir) {
        current_tier.store(TIER_IR, std::memory_order_release);
        attemptJITCompilation();
        printd(3, "UserVariantBase::eagerlyCompileForExecMode() '%s' enqueued for background JIT\n", name);
    } else if (exec_mode == QEM_TIERED && cached_ir) {
        // For tiered mode, start with IR tier and enqueue for background JIT
        // This provides 2x speedup immediately while hot code still gets JIT benefit
        current_tier.store(TIER_IR, std::memory_order_release);
        attemptJITCompilation();
        printd(3, "UserVariantBase::eagerlyCompileForExecMode() '%s' (tiered) starting with IR, enqueued for background JIT\n", name);
    } else if (exec_mode == QEM_IR && cached_ir) {
        // For IR mode, mark tier as TIER_IR (skip threshold-based promotion)
        current_tier.store(TIER_IR, std::memory_order_release);
        printd(3, "UserVariantBase::eagerlyCompileForExecMode() '%s' eager IR compilation complete\n", name);
    }
}

void UserVariantBase::attemptJITRecompilation() const {
    assert(cached_ir);

    // Try to acquire the compile lock non-blocking
    if (!QoreJIT::instance().tryAcquireCompileLock()) {
        // Reset state to allow retry later
        jit_recompile_state.store(0, std::memory_order_release);
        return;
    }

    // Demote to IR tier while recompiling
    cached_jit_fn.store(nullptr, std::memory_order_release);
    current_tier.store(TIER_IR, std::memory_order_release);

    // Reset deopt counter before recompilation
    deopt_count.store(0, std::memory_order_relaxed);

    // Use a versioned name so LLVM ORC creates a new symbol (old one stays in memory)
    std::string orig_name = cached_ir->name;
    cached_ir->name = orig_name + "_reopt";
    // Pre-copy the lookup name before compilation — LLVM 21 corrupts adjacent heap on Linux
    const std::string lookup_name = cached_ir->name;

    // Recompile with the accumulated type profiles and deopt tracking
    std::string error;
    if (!QoreJIT::instance().compileFunctionLocked(*cached_ir, error,
            const_cast<void*>(static_cast<const void*>(&deopt_count)))) {
        cached_ir->name = orig_name;
        QoreJIT::instance().releaseCompileLock();
        jit_recompile_state.store(2, std::memory_order_release);
        printd(2, "UserVariantBase::attemptJITRecompilation() '%s' failed: %s\n",
            orig_name.c_str(), error.c_str());
        return;
    }
    JitFunctionPtr fn = QoreJIT::instance().lookupFunction(lookup_name);
    cached_ir->name = orig_name;
    QoreJIT::instance().releaseCompileLock();
    if (!fn) {
        jit_recompile_state.store(2, std::memory_order_release);
        printd(2, "UserVariantBase::attemptJITRecompilation() '%s' lookup failed\n",
            orig_name.c_str());
        return;
    }
    cached_jit_fn.store(fn, std::memory_order_release);
    jit_recompile_state.store(2, std::memory_order_relaxed);
    current_tier.store(TIER_JIT, std::memory_order_release);
    printd(2, "UserVariantBase::attemptJITRecompilation() '%s' recompiled with profiled guards\n",
        orig_name.c_str());
}

QoreValue UserVariantBase::evalTiered(const char* name, ReferenceHolder<QoreListNode>& argv, QoreObject* self,
        ExceptionSink* xsink) const {
    assert(pgm);
    // Note: statements can be null for AOT-only functions (deserialized from binary metadata)
    // assert(statements);

    ExecutionTier tier = current_tier.load(std::memory_order_acquire);

    // When a debugger is attached at dispatch time, force AST execution so
    // that all debug events (dbgFunctionEnter/Exit, dbgStep with onBlock/onStep,
    // dbgException) are generated with full fidelity. JIT has no debug hooks;
    // IR interpreter has hooks but only generates onStep events (no onBlock),
    // so it can't match AST fidelity for dispatch-time debugging.
    // The IR interpreter's debug hooks handle the mid-execution attachment case
    // (debugger attaches while a function is already executing in IR).
    if (tier != TIER_AST && qore_program_private::get(*pgm)->hasDebuggerAttached()) {
        tier = TIER_AST;
    }

    // JIT/AOT tier: execute native function
    // Load cached_jit_fn atomically; if it was invalidated by recompilation, fall through to IR tier
    JitFunctionPtr jit_fn = cached_jit_fn.load(std::memory_order_acquire);
    if (tier == TIER_JIT && (jit_fn || cached_aot_fn)) {
        printd(3, "evalTiered JIT/AOT '%s' exec_count=%lu aot_ctx=%p\n",
            name, exec_count.load(), (void*)cached_aot_ctx);

        // self might be 0 if instantiated by a constructor call
        // Only instantiate if actually used in the function body
        if (uses_self && self && signature.selfid) {
            signature.selfid->instantiateSelf(self);
        }

        // Only instantiate argv if actually used in the function body
        if (uses_argv) {
            assert(signature.argvid);
            signature.argvid->instantiate(argv ? argv->refSelf() : nullptr);
        }

        QoreValue val{};
        {
            // Only create ArgvContextHelper if argv is used
            std::optional<ArgvContextHelper> argv_helper;
            if (uses_argv) {
                argv_helper.emplace(argv.release(), xsink);
            } else {
                // argv not used - just discard the reference without creating context
                if (argv) {
                    argv->deref(xsink);
                }
            }

            if (!gate || (gate->enter(xsink) >= 0)) {
                // Get AST-visible body locals: for AOT use all_body_locals (separate optimization),
                // for IR use filtered ast_visible_body_locals (excludes IR-only locals that
                // are never accessed by AST callbacks).
                const std::vector<LocalVar*>& body_locals = cached_aot_ctx
                    ? cached_aot_ctx->all_body_locals
                    : cached_ir->ast_visible_body_locals;

                // Instantiate AST-visible body locals so that
                // AST Invoke callbacks can find them on the thread-local stack.
                // Skip closure-use vars in AOT mode: the LLVM code handles their
                // instantiation/uninstantiation at block scope boundaries via
                // qore_rt_instantiate_local_aot / qore_rt_pop_closure_var_aot.
                const QoreParseOptions& po = pgm->getParseOptions();
                if (!body_locals.empty()) {
                    for (LocalVar* lv : body_locals) {
                        if (cached_aot_ctx && lv->closureUse()) {
                            continue;
                        }
                        lv->instantiate(po);
                    }
                }

                // Swap in the program's parse options and set runtime_loc to the function's
                // parse location so that nested function/method calls (via CodeEvaluationHelper)
                // report this function's source location as the caller.
                const AbstractStatement* old_stmt;
                const QoreProgramLocation* old_loc;
                QoreParseOptions old_po;
                swap_runtime_statement_location(xsink, nullptr, signature.getParseLocation(), po,
                    old_stmt, old_loc, old_po);

                // Note: We used to isolate from outer AST stack location chain by nulling it,
                // but this caused crashes when exceptions are thrown from builtins called during
                // JIT execution. The exception constructor tries to walk the stack while it's being
                // thrown, and a nullptr location breaks the walk. Instead, we rely on the fact that
                // the stack locations are properly set up by CodeEvaluationHelper when builtins
                // are called, so dangling pointers should not occur.

                uint64_t result_bits;
                if (cached_aot_ctx && cached_aot_fn) {
                    result_bits = cached_aot_fn(cached_aot_ctx, xsink);
                } else {
                    assert(jit_fn);
                    result_bits = jit_fn(xsink);
                }

                // Check for JIT guard failure requesting deopt to AST.
                // In the IR interpreter, guard failure returns false (no exception)
                // and evalTiered falls through to AST.  In JIT, the guard sets a
                // thread-local flag and returns NOTHING via error_return_block.
                // Body locals are still instantiated, so AST can re-execute.
                if (!*xsink && qore_jit_deopt_requested()) {
                    printd(2, "evalTiered JIT-DEOPT: '%s' — falling back to AST\n", name);
                    static bool debug_deopt = [] {
                        const char* debug_env = getenv("QORE_IR_DEBUG");
                        return debug_env && strstr(debug_env, "deopt");
                    }();
                    if (debug_deopt) {
                        fprintf(stderr, "[DEOPT-JIT->AST] Function '%s' deopting from JIT to AST\n",
                                name ? name : "<unknown>");
                        fflush(stderr);
                    }
                    if (pgm) {
                        pgm->recordIRFallback("JIT guard failure");
                    }
                    if (statements) {
                        val = statements->exec(xsink);
                    }
                } else {
                    QoreValue result;
                    std::memcpy(&result, &result_bits, sizeof(result));
                    val = result;
                }

                // Restore thread-local parse options
                swap_runtime_statement_location(xsink, old_stmt, old_loc, old_po, old_stmt, old_loc, old_po);

                // Uninstantiate in reverse order (LIFO)
                // Skip closure-use vars in AOT mode: the LLVM code already popped
                // them via qore_rt_pop_closure_var_aot at block scope boundaries.
                if (!body_locals.empty()) {
                    for (int i = (int)body_locals.size() - 1; i >= 0; --i) {
                        if (cached_aot_ctx && body_locals[i]->closureUse()) {
                            continue;
                        }
                        body_locals[i]->uninstantiate(xsink);
                    }
                }

                if (gate) {
                    gate->exit();
                }
            }
        }

        // Only uninstantiate if we instantiated them
        if (uses_argv) {
            signature.argvid->uninstantiate(xsink);
        }
        if (uses_self && self && signature.selfid) {
            signature.selfid->uninstantiateSelf();
        }

        // Check if profiled guards triggered deopts; if so, attempt recompilation
        // with updated type profiles (one-time, non-blocking)
        uint32_t deopts = deopt_count.load(std::memory_order_relaxed);
        if (deopts >= 10 && cached_ir) {
            int expected = 0;
            if (jit_recompile_state.compare_exchange_strong(expected, 1)) {
                attemptJITRecompilation();
            }
        }

        if (!*xsink) {
            const QoreTypeInfo* rt = signature.getReturnTypeInfo();
            if (rt && QoreTypeInfo::hasType(rt)) {
                if (val.isNothing()) {
                    QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
                    if (*xsink) {
                        xsink->overrideLocation(*signature.getParseLocation());
                        xsink->appendLastDescription(": block missing return statement");
                    }
                } else {
                    QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
                }
            }
        }
        return val;
    }

    // IR tier: execute via IR interpreter
    // Also handles JIT→IR fallback when cached_jit_fn was invalidated by recompilation
    if ((tier == TIER_IR || (tier == TIER_JIT && !jit_fn && !cached_aot_fn)) && cached_ir) {
        // Push frame boundary for debugger introspection (get_local_vars, etc.)
        // AST path does this via UserVariantExecHelper; IR path needs it explicitly.
        ThreadFrameBoundaryHelper tfbh(true);

        if (self && signature.selfid) {
            signature.selfid->instantiateSelf(self);
        }

        assert(signature.argvid);
        signature.argvid->instantiate(argv ? argv->refSelf() : nullptr);

        QoreValue val{};
        {
            ArgvContextHelper argv_helper(argv.release(), xsink);

            if (!gate || (gate->enter(xsink) >= 0)) {
                // Instantiate AST-visible body locals (excludes IR-only locals that
                // are never accessed by AST callbacks) so that AST Invoke callbacks
                // can find them on the thread-local variable stack.
                const QoreParseOptions& po = pgm->getParseOptions();
                for (LocalVar* lv : cached_ir->ast_visible_body_locals) {
                    // Skip closure-use vars: the cvstack is LIFO and pre-instantiating
                    // all closure-use vars at once breaks block-scope cleanup ordering.
                    // The IR interpreter handles them on-demand via ensureLocalInstantiated().
                    if (!lv->closureUse()) {
                        lv->instantiate(po);
                    }
                }

                // Swap in the program's parse options and set runtime_loc to the function's
                // parse location for the duration of IR execution. The IR interpreter updates
                // runtime_loc per-instruction, but this ensures correct location from the start.
                const AbstractStatement* old_stmt;
                const QoreProgramLocation* old_loc;
                QoreParseOptions old_po;
                swap_runtime_statement_location(xsink, nullptr, signature.getParseLocation(), po,
                    old_stmt, old_loc, old_po);

                // Note: We do NOT isolate from outer stack location chain here.
                // CodeEvaluationHelper RAII properly manages the stack location chain
                // for all function calls during IR execution, so dangling pointers
                // should not occur.  Clearing would break exception call stacks.

                // Use the cached set of pre-instantiated locals built during IR lowering.
                // Pass pointer directly to avoid per-call allocation (critical for recursive calls).
                // If self is null and signature.selfid is present, pass it as excluded_selfid
                // so the IR interpreter skips it even though it's in the cached set.
                const LocalVar* excluded_selfid = (!self && signature.selfid) ? signature.selfid : nullptr;

                QoreValue ir_return_value;
                bool fell_back_to_ast = false;
                bool ok = QoreIRInterpreter::execute(*cached_ir, ir_return_value, xsink, nullptr,
                    nullptr, nullptr, cached_ir->cached_pre_instantiated, excluded_selfid, statements, pgm);

                if (ok && !*xsink) {
                    val = ir_return_value;
                } else if (*xsink) {
                    // exception raised — propagate
                } else {
                    // IR execution failed without exception — clean up pre-instantiated
                    // AST-visible locals FIRST (destroys any values from partial IR execution),
                    // then fall back to AST which manages its own locals
                    for (int i = (int)cached_ir->ast_visible_body_locals.size() - 1; i >= 0; --i) {
                        if (!cached_ir->ast_visible_body_locals[i]->closureUse()) {
                            cached_ir->ast_visible_body_locals[i]->uninstantiate(xsink);
                        }
                    }
                    fell_back_to_ast = true;
                    printd(2, "UserVariantBase::evalTiered() IR execution failed for '%s', "
                        "falling back to AST\n", name);
                    static bool debug_deopt = [] {
                        const char* debug_env = getenv("QORE_IR_DEBUG");
                        return debug_env && strstr(debug_env, "deopt");
                    }();
                    if (debug_deopt) {
                        fprintf(stderr, "[DEOPT-IR->AST] Function '%s' deopting from IR to AST\n",
                                name ? name : "<unknown>");
                        fflush(stderr);
                    }
                    if (pgm) {
                        pgm->recordIRFallback("execution: runtime failure");
                    }
                    val = statements->exec(xsink);
                }

                // Restore thread-local parse options
                swap_runtime_statement_location(xsink, old_stmt, old_loc, old_po, old_stmt, old_loc, old_po);

                // Only uninstantiate pre-instantiated AST-visible locals if we didn't already
                // do it before the AST fallback
                if (!fell_back_to_ast) {
                    for (int i = (int)cached_ir->ast_visible_body_locals.size() - 1; i >= 0; --i) {
                        if (!cached_ir->ast_visible_body_locals[i]->closureUse()) {
                            cached_ir->ast_visible_body_locals[i]->uninstantiate(xsink);
                        }
                    }
                }

                if (gate) {
                    gate->exit();
                }
            }
        }

        signature.argvid->uninstantiate(xsink);
        if (self && signature.selfid) {
            signature.selfid->uninstantiateSelf();
        }

        // Check for JIT promotion while on IR tier
        uint64_t count = exec_count.fetch_add(1, std::memory_order_relaxed) + 1;
        printd(3, "evalTiered IR '%s' exec_count=%lu jit_threshold=%lu is_closure=%d jit_failed=%d\n",
            name, count, (unsigned long)QoreJIT::getJITThreshold(), (int)is_closure, (int)jit_compile_failed);
        // OSR: hot loop detected by IR interpreter — trigger JIT compilation early
        if (cached_ir->osr_jit_requested && !jit_compile_failed) {
            cached_ir->osr_jit_requested = false;  // Reset flag
            printd(2, "evalTiered OSR: promoting '%s' to JIT tier (hot loop detected)\n",
                cached_ir->name.c_str());
            attemptJITCompilation();
        } else if (count >= QoreJIT::getJITThreshold() && !jit_compile_failed) {
            attemptJITCompilation();
        }

        if (!*xsink) {
            const QoreTypeInfo* rt = signature.getReturnTypeInfo();
            if (rt && QoreTypeInfo::hasType(rt)) {
                if (val.isNothing()) {
                    QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
                    if (*xsink) {
                        xsink->overrideLocation(*signature.getParseLocation());
                        xsink->appendLastDescription(": block missing return statement");
                    }
                } else {
                    QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
                }
            }
        }
        return val;
    }

    // AST tier: check thresholds and possibly promote
    uint64_t count = exec_count.fetch_add(1, std::memory_order_relaxed) + 1;

    // Check for JIT promotion (IR already cached from a previous call)
    if (count >= QoreJIT::getJITThreshold() && cached_ir && !jit_compile_failed) {
        attemptJITCompilation();
        // If promotion succeeded, dispatch to JIT on next call; for now, continue with IR or AST
    }

    // Check for IR promotion
    if (count >= QoreJIT::getIRThreshold() && !ir_lower_failed) {
        std::call_once(ir_lower_once, [this, name]() {
            attemptIRLowering(name);
        });
        // If promotion succeeded, the next call will use IR tier
    }

    // Fall through to AST execution (bypass the tiered check)
    QoreValue val{};
    if (self && signature.selfid) {
        signature.selfid->instantiateSelf(self);
    }

    assert(signature.argvid);
    signature.argvid->instantiate(argv ? argv->refSelf() : nullptr);

    {
        ArgvContextHelper argv_helper(argv.release(), xsink);

        if (!gate || (gate->enter(xsink) >= 0)) {
            // Note: We do NOT isolate from outer stack location chain here.
            // CodeEvaluationHelper RAII properly manages the stack location chain
            // for all function calls during AST execution, so dangling pointers
            // should not occur.  Clearing would break exception call stacks.
            val = statements->exec(xsink);

            if (gate) {
                gate->exit();
            }
        }
    }

    signature.argvid->uninstantiate(xsink);
    if (self && signature.selfid) {
        signature.selfid->uninstantiateSelf();
    }

    if (!*xsink && val.isNothing()) {
        const QoreTypeInfo* rt = signature.getReturnTypeInfo();
        QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
        if (*xsink) {
            xsink->overrideLocation(*signature.getParseLocation());
            xsink->appendLastDescription(": block missing return statement");
        }
    }
    return val;
}

QoreValue UserVariantBase::evalIntern(const char* name, ReferenceHolder<QoreListNode>& argv, QoreObject* self,
        ExceptionSink* xsink) const {
    //QORE_TRACE("UserVariantBase::evalIntern()");
    // Tiered compilation dispatch
    // Also dispatch to evalTiered for AOT-only functions (no AST body) that are already at TIER_JIT
    if (pgm) {
        bool has_aot = current_tier.load(std::memory_order_acquire) == TIER_JIT && cached_aot_fn;
        if (statements || has_aot || cached_ir) {
            qore_exec_mode_t mode = pgm->getExecMode();
            printd(3, "evalIntern '%s': mode=%d pgm=%p statements=%p has_aot=%d cached_ir=%p\n",
                name, (int)mode, (void*)pgm, (void*)statements, (int)has_aot, (void*)cached_ir);
            // AOT dispatch: always use evalTiered when a cached AOT function is
            // available (tier==TIER_JIT && cached_aot_fn). This covers both
            // strip-source (no AST body) and normal AOT with AST body.
            // The AOT context is valid because registerPrecompiledAOTFunction()
            // set it up during program initialization.
            if (has_aot) {
                return evalTiered(name, argv, self, xsink);
            }
            // IR-only dispatch: closure variants reconstructed from AOT binary
            // with cached IR but no AST body and no native AOT function
            if (!statements && cached_ir) {
                return evalTiered(name, argv, self, xsink);
            }
            // Tiered promotion for JIT/IR/tiered modes with %modern code.
            // Skip if function already has an AOT fn registered — the AOT path
            // in evalTiered would dispatch to the AOT-compiled code, which may
            // have stale expression slots when called outside tiered mode.
            if (statements && !has_aot
                    && (mode == QEM_TIERED || mode == QEM_JIT || mode == QEM_IR)) {
                const QoreParseOptions& po = pgm->getParseOptions();
                if ((po & QoreParseOptions(PO_MODERN)) == QoreParseOptions(PO_MODERN)) {
                    return evalTiered(name, argv, self, xsink);
                }
            }
        }
    }

    QoreValue val{};  // value-initialized to NOTHING (bits=0)
    if (statements) {
        // self might be 0 if instantiated by a constructor call
        if (self && signature.selfid) {
            signature.selfid->instantiateSelf(self);
        }

        // instantiate argv and push id on stack
        assert(signature.argvid);
        signature.argvid->instantiate(argv ? argv->refSelf() : nullptr);

        {
            ArgvContextHelper argv_helper(argv.release(), xsink);

            // enter gate if necessary
            if (!gate || (gate->enter(xsink) >= 0)) {
                // execute function
                val = statements->exec(xsink);

                // exit gate if necessary
                if (gate) {
                    gate->exit();
                }
            }
        }

        // uninstantiate argv
        signature.argvid->uninstantiate(xsink);

        // if self then uninstantiate
        // self might be 0 if instantiated by a constructor call
        if (self && signature.selfid) {
            signature.selfid->uninstantiateSelf();
        }
    } else {
        argv = nullptr; // dereference argv now
    }

    // if return value is NOTHING; make sure it's valid; maybe there wasn't a return statement
    // only check if there isn't an active exception
    if (!*xsink && val.isNothing()) {
        const QoreTypeInfo* rt = signature.getReturnTypeInfo();

        // check return type
        QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
        // issue #3255: make sure any exception reflects the signature location
        if (*xsink) {
            xsink->overrideLocation(*signature.getParseLocation());
            xsink->appendLastDescription(": block missing return statement");
        }
    }

    return val;
}

// primary function for executing user code
QoreValue UserVariantBase::eval(const char* name, CodeEvaluationHelper* ceh, QoreObject* self, ExceptionSink* xsink,
        const qore_class_private* qc) const {
    QORE_TRACE("UserVariantBase::eval()");

    assert(!self || (ceh ? ceh->getClass() : qc));

    UserVariantExecHelper uveh(this, ceh, xsink);
    if (!uveh) {
        return QoreValue();
    }

    CodeContextHelper cch(xsink, CT_USER, name, self, qc ? qc : (ceh ? ceh->getClass() : nullptr));
    return evalIntern(name, uveh.getArgv(), self, xsink);
}

void UserVariantBase::parseCommit() {
    if (statements) {
        statements->parseCommit(getProgram());
    }
}

int QoreFunction::parseCheckDuplicateSignatureCommitted(UserSignature* sig) {
    const AbstractFunctionSignature* vs = 0;
    int rc = parseCompareResolvedSignature(vlist, sig, vs);
    if (rc == QTI_NOT_EQUAL) {
        return 0;
    }

    if (rc == QTI_AMBIGUOUS || rc == QTI_WILDCARD) {
        ambiguousDuplicateSignatureException(className(), getName(), vs, sig);
    } else {
        duplicateSignatureException(className(), getName(), sig);
    }
    return -1;
}

// returns 0 for OK, -1 for error
// this is called after types have been resolved and the types must be rechecked
int QoreFunction::parseCompareResolvedSignature(const VList& vlist, const AbstractFunctionSignature* sig, const AbstractFunctionSignature*& vs) {
    unsigned vp = sig->getParamTypes();

    // now check already-committed variants
    for (vlist_t::const_iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        vs = (*i)->getSignature();
        // get the minimum number of parameters with type information that need to match
        unsigned mp = vs->getMinParamTypes();
        // get number of parameters with type information
        unsigned tp = vs->getParamTypes();

        // shortcut: if the two variants have different numbers of parameters with type information, then they do not match
        if (vp < mp || vp > tp)
            continue;

        bool dup = true;
        bool ambiguous = false;
        unsigned max = QORE_MAX(tp, vp);
        for (unsigned pi = 0; pi < max; ++pi) {
            const QoreTypeInfo* variantTypeInfo = vs->getParamTypeInfo(pi);
            bool variantHasDefaultArg = vs->hasDefaultArg(pi);

            const QoreTypeInfo* typeInfo = sig->getParamTypeInfo(pi);
            assert(!sig->getParseParamTypeInfo(pi));
            bool thisHasDefaultArg = sig->hasDefaultArg(pi);

            // check for ambiguous matches
            if (typeInfo) {
                //printd(5, "QoreFunction::parseCompareResolvedSignature() this: sig: '%s' vti: %s ti: %s ident: %d\n", vs->getSignatureText(), QoreTypeInfo::getName(variantTypeInfo), QoreTypeInfo::getName(typeInfo), QoreTypeInfo::isInputIdentical(typeInfo, variantTypeInfo));

                if (!QoreTypeInfo::hasType(variantTypeInfo) && thisHasDefaultArg)
                    ambiguous = true;
                else if (!QoreTypeInfo::isInputIdentical(typeInfo, variantTypeInfo)) {
                    dup = false;
                    break;
                }
            } else {
                if (QoreTypeInfo::hasType(variantTypeInfo) && variantHasDefaultArg)
                    ambiguous = true;
                else if (!QoreTypeInfo::isInputIdentical(typeInfo, variantTypeInfo)) {
                    dup = false;
                    break;
                }
            }
        }
        if (dup)
            return ambiguous ? QTI_AMBIGUOUS : QTI_IDENT;
    }
    return QTI_NOT_EQUAL;
}

// Dispatch matrix relocated to QoreParseTypeInfo::paramTypesIdentical()
// See include/qore/intern/QoreParseTypeInfo.h for documentation
static bool paramTypesIdentical(
    const QoreTypeInfo* ti_a, const QoreParseTypeInfo* pti_a,
    const QoreTypeInfo* ti_b, const QoreParseTypeInfo* pti_b,
    bool& recheck) {
    return QoreParseTypeInfo::paramTypesIdentical(ti_a, pti_a, ti_b, pti_b, recheck);
}

// Stage 1 (parse-time) duplicate-signature checking with conservative type matching.
// At this stage, unresolved types are string-based (QoreParseTypeInfo), and we use
// paramTypesIdentical() to implement a 2x2 dispatch matrix over resolved/unresolved pairs.
// When ambiguous matches are detected (e.g. issue #3861 namespace-scoped names),
// setRecheck() is called to flag the variant for stage-2 rechecking.
// See UserFunctionVariant::parseInit() -> parseCheckDuplicateSignatureCommitted()
// for the stage-2 recheck logic after type resolution.
int QoreFunction::parseCheckDuplicateSignature(AbstractQoreFunctionVariant* variant) {
    UserSignature* sig = reinterpret_cast<UserSignature*>(variant->getSignature());

    // check for duplicate parameter signatures
    unsigned vnp = sig->numParams();
    unsigned vtp = sig->getParamTypes();
    unsigned vmp = sig->getMinParamTypes();

    // check all variants
    for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        UserSignature* vs = reinterpret_cast<UserSignature*>((*i)->getSignature());
        assert(!vs->resolved);
        // get the minimum number of parameters with type information that need to match
        unsigned mp = vs->getMinParamTypes();
        // get number of parameters with type information
        unsigned tp = vs->getParamTypes();

        //printd(5, "QoreFunction::parseCheckDuplicateSignature() adding %s(%s) checking %s(%s) vmp: %d vtp: %d mp: %d tp: %d\n", getName(), sig->getSignatureText(), getName(), vs->getSignatureText(), vmp, vtp, mp, tp);

        // shortcut: if the two variants have different numbers of parameters with type information, then they do not match
        if (vmp > tp || vtp < mp)
            continue;

        // the 2 signatures have the same number of parameters with type information
        if (!tp) {
            duplicateSignatureException(className(), getName(), vs);
            return -1;
        }

        unsigned np = vs->numParams();

        bool dup = true;
        bool ambiguous = false;
        bool recheck = false;
        unsigned max = QORE_MAX(np, vnp);
        for (unsigned pi = 0; pi < max; ++pi) {
            const QoreTypeInfo* variantTypeInfo = vs->getParamTypeInfo(pi);
            const QoreParseTypeInfo* variantParseTypeInfo = variantTypeInfo ? nullptr : vs->getParseParamTypeInfo(pi);
            bool variantHasDefaultArg = vs->hasDefaultArg(pi);

            const QoreTypeInfo* typeInfo = sig->getParamTypeInfo(pi);
            const QoreParseTypeInfo* parseTypeInfo = typeInfo ? nullptr : sig->getParseParamTypeInfo(pi);
            bool thisHasDefaultArg = sig->hasDefaultArg(pi);

            //printd(5, "QoreFunction::parseCheckDuplicateSignature() ti: '%s' pti: '%s' vti: '%s' vpti: '%s' ident: %d\n", QoreTypeInfo::getName(typeInfo), QoreParseTypeInfo::getName(parseTypeInfo), QoreTypeInfo::getName(variantTypeInfo), QoreParseTypeInfo::getName(variantParseTypeInfo), QoreTypeInfo::isInputIdentical(typeInfo, variantTypeInfo));

            // Check if types match using consolidated comparison logic
            if (!paramTypesIdentical(typeInfo, parseTypeInfo, variantTypeInfo, variantParseTypeInfo, recheck)) {
                dup = false;
                break;
            }

            // Detect ambiguous matches: one has type, other doesn't, but has default arg
            if ((typeInfo || parseTypeInfo) && !QoreTypeInfo::hasType(variantTypeInfo) && !variantParseTypeInfo && thisHasDefaultArg) {
                ambiguous = true;
            } else if (!(typeInfo || parseTypeInfo) && (QoreTypeInfo::hasType(variantTypeInfo) || variantParseTypeInfo) && variantHasDefaultArg) {
                ambiguous = true;
            }
            //printd(5, "QoreFunction::parseCheckDuplicateSignature() %s(%s) == %s(%s) i: %d: %s <=> %s dup: %d\n", getName(), sig->getSignatureText(), getName(), vs->getSignatureText(), pi, QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(variantTypeInfo), dup);
        }
        if (dup) {
            if (ambiguous)
                ambiguousDuplicateSignatureException(className(), getName(), vs, sig);
            else
                duplicateSignatureException(className(), getName(), sig);
            return -1;
        }
        if (recheck)
            variant->setRecheck();
    }

    return 0;
}

AbstractFunctionSignature* QoreFunction::parseGetUniqueSignature() const {
    if (vlist.singular()) {
        const UserVariantBase* uvb = first()->getUserVariantBase();
        if (uvb) {
            UserSignature* sig = uvb->getUserSignature();
            sig->resolve();
            return sig;
        }
        return first()->getSignature();
    }

    return nullptr;
}

void QoreFunction::resolvePendingSignatures() {
    if (!check_parse) {
        return;
    }

    const QoreTypeInfo* ti = nullptr;

    for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        UserVariantBase* uvb = (*i)->getUserVariantBase();
        if (!uvb) {
            continue;
        }

        UserSignature* sig = uvb->getUserSignature();
        sig->resolve();

        if (same_return_type) {
            const QoreTypeInfo* st = sig->getReturnTypeInfo();
            if (i != vlist.begin() && !QoreTypeInfo::isInputIdentical(st, ti))
                same_return_type = false;
            ti = st;
        }
    }
}

int QoreFunction::addPendingVariant(AbstractQoreFunctionVariant* variant) {
    if (!vlist.empty() && parse_init_done) {
        UserSignature* sig = reinterpret_cast<UserSignature*>(variant->getSignature());
        const char* cname = className();
        const char* name = getName();
        parse_error(*sig->getParseLocation(), "variant %s%s%s(%s) cannot be added to an existing function", cname ? cname : "", cname ? "::" : ""
, name, sig->getSignatureText());
        variant->deref();
        return -1;
    }

    parse_rt_done = false;
    parse_init_done = false;
    if (!check_parse) {
        check_parse = true;
    }

    // check for duplicate signature with existing variants
    if (parseCheckDuplicateSignature(variant)) {
        variant->deref();
        return -1;
    }

    vlist.push_back(variant);

    return 0;
}

void QoreFunction::parseCommit() {
    if (!check_parse) {
        return;
    }
    check_parse = false;

    parseCheckReturnType();

    for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        if ((*i)->isUser()) {
            if (!has_pub && (*i)->isModulePublic()) {
                has_pub = true;
                if (all_priv) {
                    all_priv = false;
                }
            }
            if (!has_user)
                has_user = true;
        }
        else if (!has_builtin) {
            has_builtin = true;
            if (all_priv) {
                all_priv = false;
            }
        }

        (*i)->parseCommit();
    }

    parse_rt_done = true;
    parse_init_done = true;
}

void QoreFunction::parseRollback() {
    // noop: object will be destroyed
}

int QoreFunction::parseInit(qore_ns_private* ns) {
    if (parse_init_done || parse_init_in_progress) {
        return 0;
    }
    parse_init_in_progress = true;

    int err = 0;
    if (check_parse) {
        OptionalNamespaceParseContextHelper pch(ns);

        for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
            if ((*i)->parseInit(this) && !err) {
                err = -1;
            }
        }
    }
    parse_init_done = true;
    return err;
}

QoreValue UserClosureFunction::evalClosure(const QoreClosureBase& closure_base, QoreProgram* pgm, const QoreListNode* args, QoreObject *self, const qore_class_private* class_ctx, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    // closures cannot be overloaded
    assert(vlist.singular());
    const AbstractQoreFunctionVariant* variant = first();

    // setup call, save runtime position
    // issue #1303: do not check for object validity here in the call, we already have a weak reference to the object,
    // so it will stay valid, if the closure code itself refers to the object, it will fail then if the object is invalid
    // Pass pgm to set up program context - this ensures tlpd is set up for thread pool threads
    CodeEvaluationHelper ceh(xsink, rc, this, variant, "<anonymous closure>", args, nullptr, class_ctx, CT_USER,
        false, nullptr, pgm);
    if (*xsink) {
        return QoreValue();
    }

    ThreadSafeLocalVarRuntimeEnvironmentHelper ch(&closure_base);

    //printd(5, "UserClosureFunction::evalClosure() this: %p (%s) variant: %p args: %p self: %p\n", this, getName(), variant, args, self);
    return UCLOV_const(variant)->evalClosure(ceh, self, xsink);
}

int UserFunctionVariant::parseInit(QoreFunction* f) {
    signature.resolve();

    // set the varargs flag on the variant if the signature has ellipses at the end
    if (!(flags & QCF_USES_EXTRA_ARGS) && signature.hasVarargs()) {
        flags ^= QCF_USES_EXTRA_ARGS;
    }

    // resolve and push current return type on stack
    ParseCodeInfoHelper rtih(f->getName(), signature.getReturnTypeInfo());

    // set implicit argv arg type as unknown
    ParseImplicitArgTypeHelper pia(nullptr);

    // For AOT-compiled functions, statements is null (pre-compiled code)
    int err = statements ? statements->parseInit(this) : 0;

    // recheck types against committed types if necessary
    if (recheck && f->parseCheckDuplicateSignatureCommitted(&signature) && !err) {
        err = -1;
    }
    return err;
}

int UserClosureVariant::parseInit(QoreFunction* f) {
    UserClosureFunction* cf = static_cast<UserClosureFunction*>(f);

    int err = signature.resolve();

    // resolve and push current return type on stack
    ParseCodeInfoHelper rtih(f->getName(), signature.getReturnTypeInfo());

    // For AOT-compiled closures, statements is null (pre-compiled code)
    if (statements) {
        if (statements->parseInitClosure(this, cf) && !err) {
            err = -1;
        }
    }

    // only one variant is possible, no need to recheck types
    return err;
}
