/*
    FunctionCallNode.cpp

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
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreParseTypeInfo.h"
#include "qore/intern/NewComplexTypeNode.h"
#include "qore/intern/QoreAOT.h"
#include "qore/intern/qore_aot_deps.h"

#include <string>
#include <vector>

static bool static_scope_has_parameterized_receiver(const NamedScope& scope) {
    if (scope.size() < 2) {
        return false;
    }
    for (unsigned i = 0, e = scope.size() - 1; i < e; ++i) {
        if (strchr(scope[i], '<')) {
            return true;
        }
    }
    return false;
}

static std::string get_static_scope_receiver_type_path(const NamedScope& scope) {
    std::string rv;
    for (unsigned i = 0, e = scope.size() - 1; i < e; ++i) {
        if (i) {
            rv += "::";
        }
        rv += scope[i];
    }
    return rv;
}

static void record_source_parse_reflection_class_for_name_import(QoreProgram* pgm, const QoreProgramLocation* loc,
        const QoreMethod* method, const QoreParseListNode* parse_args) {
    if (!qore_aot_source_parse_active() || !pgm || !method || strcmp(method->getName(), "forName") || !parse_args
            || parse_args->size() != 1) {
        return;
    }

    const QoreClass* qc = method->getClass();
    if (!qc || qc->getNamespacePath(false) != "Qore::Reflection::Class") {
        return;
    }

    QoreValue arg = parse_args->get(0);
    if (arg.getType() != NT_STRING) {
        return;
    }

    const QoreStringNode* class_name = arg.get<const QoreStringNode>();
    if (!class_name || !class_name->size()) {
        return;
    }

    std::string type_path = "object<";
    type_path += class_name->c_str();
    type_path += '>';
    const QoreProgramLocation* arg_loc = parse_args->getLocation(0);
    qore_program_private::recordSourceParseTypeImport(pgm, arg_loc ? arg_loc : loc, class_name->c_str(),
        type_path.c_str(), false, false);
}

static const QoreTypeInfo* resolve_static_scope_receiver_type(const QoreProgramLocation* loc, const NamedScope& scope,
        int& err) {
    std::string type_path = get_static_scope_receiver_type_path(scope);
    QoreParseTypeInfo* pti = qore_parse_type_string_to_pti(type_path.c_str());
    if (!pti) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve static method receiver type '%s'",
            type_path.c_str());
        err = -1;
        return autoTypeInfo;
    }
    const QoreTypeInfo* rv = QoreParseTypeInfo::resolveAny(pti, loc, err);
    delete pti;
    if (err) {
        return autoTypeInfo;
    }
    if (!QoreTypeInfo::getParameterizedClassType(rv) && !QoreTypeInfo::getComplexBufferType(rv)) {
        parseException(*loc, "PARSE-TYPE-ERROR", "static generic method call target '%s' must resolve to a "
            "parameterized class type or a built-in buffer<T> factory type; use 'Class<...>::method()' for generic "
            "static methods, 'Class::method()' for non-generic static methods, or "
            "'buffer<T>::sized()/filled()' for dense buffer factories", type_path.c_str());
        err = -1;
        return autoTypeInfo;
    }
    return rv;
}

static std::string get_static_scope_class_path(const NamedScope& scope) {
    std::string rv;
    unsigned count = scope.size();
    assert(count >= 2);
    for (unsigned i = 0; i + 1 < count; ++i) {
        if (i) {
            rv += "::";
        }
        rv += scope[i];
    }
    return rv;
}

static size_t qore_buffer_factory_arg_count(const QoreValue& args) {
    switch (args.getType()) {
        case NT_PARSE_LIST:
            return args.get<const QoreParseListNode>()->size();
        case NT_LIST:
            return args.get<const QoreListNode>()->size();
        case NT_NOTHING:
            return 0;
        default:
            return 1;
    }
}

static const QoreTypeInfo* qore_buffer_factory_arg_type(const QoreValue& args, size_t index,
        const type_vec_t* arg_types = nullptr) {
    if (arg_types) {
        assert(index < arg_types->size());
        return (*arg_types)[index];
    }

    switch (args.getType()) {
        case NT_PARSE_LIST: {
            const QoreParseListNode* pln = args.get<const QoreParseListNode>();
            assert(index < pln->getValueTypes().size());
            return pln->getValueTypes()[index];
        }
        case NT_LIST: {
            const QoreListNode* list = args.get<const QoreListNode>();
            assert(index < list->size());
            return list->retrieveEntry(index).getFullTypeInfo();
        }
        default:
            return args.getFullTypeInfo();
    }
}

static const char* qore_buffer_factory_find_expected_arg(const char* name, const char* const* expected_names,
        size_t expected_args, size_t& target) {
    assert(name && *name);
    for (size_t i = 0; i < expected_args; ++i) {
        if (!strcmp(name, expected_names[i])) {
            target = i;
            return nullptr;
        }
    }
    return expected_args == 1 ? expected_names[0] : "size, value";
}

struct QoreBufferFactoryArgBinding {
    bool named = false;
    std::vector<size_t> source_to_param;
    size_t result_size = 0;
};

static int qore_buffer_factory_bind_named_args(const QoreProgramLocation* loc, const QoreTypeInfo* buffer_type_info,
        const char* factory, const QoreParseListNode* parse_args, const char* const* expected_names,
        size_t expected_args, QoreBufferFactoryArgBinding& binding) {
    if (!parse_args || !parse_args->hasNamedArgs()) {
        return 0;
    }

    binding.named = true;
    binding.source_to_param.assign(parse_args->size(), 0);
    binding.result_size = expected_args;
    std::vector<bool> supplied(expected_args, false);
    bool seen_named = false;
    size_t positional = 0;

    for (size_t i = 0, e = parse_args->size(); i < e; ++i) {
        const char* name = parse_args->getArgName(i);
        size_t target;
        if (!name) {
            if (seen_named) {
                parseException(*loc, "PARSE-TYPE-ERROR", "cannot call '%s::%s()' with a positional argument after "
                    "a named argument", QoreTypeInfo::getName(buffer_type_info), factory);
                return -1;
            }
            target = positional++;
        } else {
            seen_named = true;
            const char* expected = qore_buffer_factory_find_expected_arg(name, expected_names, expected_args, target);
            if (expected) {
                parseException(*loc, "PARSE-TYPE-ERROR", "unknown named argument '%s' in call to '%s::%s()'; "
                    "expected %s", name, QoreTypeInfo::getName(buffer_type_info), factory, expected);
                return -1;
            }
            if (target < positional) {
                parseException(*loc, "PARSE-TYPE-ERROR", "named argument '%s' in call to '%s::%s()' would overwrite "
                    "a positional argument", name, QoreTypeInfo::getName(buffer_type_info), factory);
                return -1;
            }
            if (supplied[target]) {
                parseException(*loc, "PARSE-TYPE-ERROR", "duplicate named argument '%s' in call to '%s::%s()'",
                    name, QoreTypeInfo::getName(buffer_type_info), factory);
                return -1;
            }
        }

        if (target >= expected_args) {
            parseException(*loc, "PARSE-TYPE-ERROR", "'%s::%s()' expects %d argument%s; got %d",
                QoreTypeInfo::getName(buffer_type_info), factory, (int)expected_args, expected_args == 1 ? "" : "s",
                (int)parse_args->size());
            return -1;
        }

        binding.source_to_param[i] = target;
        supplied[target] = true;
    }

    for (size_t i = 0; i < expected_args; ++i) {
        if (!supplied[i]) {
            parseException(*loc, "PARSE-TYPE-ERROR", "missing required argument '%s' in call to '%s::%s()'",
                expected_names[i], QoreTypeInfo::getName(buffer_type_info), factory);
            return -1;
        }
    }
    return 0;
}

static int parse_init_complex_buffer_factory(const QoreProgramLocation* loc, const QoreTypeInfo* buffer_type_info,
        const char* factory, QoreParseListNode* parse_args, QoreValue& val, QoreParseContext& parse_context) {
    QoreComplexBufferInitKind init_kind;
    size_t expected_args;
    const char* sized_args[] = { "size" };
    const char* filled_args[] = { "size", "value" };
    const char* const* expected_names;
    if (!strcmp(factory, "sized")) {
        init_kind = QoreComplexBufferInitKind::Sized;
        expected_args = 1;
        expected_names = sized_args;
    } else if (!strcmp(factory, "filled")) {
        init_kind = QoreComplexBufferInitKind::Filled;
        expected_args = 2;
        expected_names = filled_args;
    } else {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve buffer factory '%s::%s()'; supported factories are "
            "'%s::sized(int size)' and '%s::filled(int size, %s value)'",
            QoreTypeInfo::getName(buffer_type_info), factory, QoreTypeInfo::getName(buffer_type_info),
            QoreTypeInfo::getName(buffer_type_info),
            QoreTypeInfo::getName(QoreTypeInfo::getComplexBufferType(buffer_type_info)->getElementTypeInfo()));
        delete parse_args;
        return -1;
    }

    QoreBufferFactoryArgBinding named_binding;
    if (qore_buffer_factory_bind_named_args(loc, buffer_type_info, factory, parse_args, expected_names,
            expected_args, named_binding)) {
        delete parse_args;
        return -1;
    }

    QoreValue new_args{};
    type_vec_t ordered_arg_types;
    int err = 0;
    const QoreTypeInfo* return_type_info = parse_context.typeInfo;
    parse_context.typeInfo = nullptr;
    if (named_binding.named) {
        type_vec_t source_arg_types;
        QoreListNode* arg_list = nullptr;
        err = parse_args->initArgs(parse_context, source_arg_types, arg_list);
        parse_args = nullptr;
        if (!err) {
            assert(source_arg_types.size() == named_binding.source_to_param.size());
            ordered_arg_types.resize(expected_args, nullptr);
            for (size_t i = 0, e = source_arg_types.size(); i < e; ++i) {
                size_t target = named_binding.source_to_param[i];
                assert(target < expected_args);
                ordered_arg_types[target] = source_arg_types[i];
            }
            qore_list_private::get(arg_list)->setCallArgEvalMap(std::move(named_binding.source_to_param),
                named_binding.result_size);
            new_args = arg_list;
        } else if (arg_list) {
            arg_list->deref(nullptr);
        }
    } else {
        QoreListNode* arg_list = nullptr;
        err = parse_args->initArgs(parse_context, ordered_arg_types, arg_list);
        parse_args = nullptr;
        if (!err) {
            new_args = arg_list;
        } else if (arg_list) {
            arg_list->deref(nullptr);
        }
    }
    parse_context.typeInfo = return_type_info;
    if (err) {
        new_args.discard(nullptr);
        return -1;
    }

    size_t arg_count = qore_buffer_factory_arg_count(new_args);
    if (arg_count != expected_args) {
        parseException(*loc, "PARSE-TYPE-ERROR", "'%s::%s()' expects %d argument%s; got %d",
            QoreTypeInfo::getName(buffer_type_info), factory, (int)expected_args, expected_args == 1 ? "" : "s",
            (int)arg_count);
        new_args.discard(nullptr);
        return -1;
    }

    const type_vec_t* named_arg_types = ordered_arg_types.empty() ? nullptr : &ordered_arg_types;
    const QoreTypeInfo* size_arg_type = qore_buffer_factory_arg_type(new_args, 0, named_arg_types);
    if (QoreTypeInfo::parseReturns(size_arg_type, NT_INT) == QTI_NOT_EQUAL) {
        parseException(*loc, "PARSE-TYPE-ERROR", "'%s::%s()' argument 'size' expects int; got '%s'",
            QoreTypeInfo::getName(buffer_type_info), factory, QoreTypeInfo::getName(size_arg_type));
        new_args.discard(nullptr);
        return -1;
    }

    if (init_kind == QoreComplexBufferInitKind::Filled) {
        const QoreComplexBufferTypeInfo* bti = QoreTypeInfo::getComplexBufferType(buffer_type_info);
        assert(bti);
        const QoreTypeInfo* value_type_info = qore_buffer_factory_arg_type(new_args, 1, named_arg_types);
        bool may_not_match = false;
        qore_type_result_e res = QoreTypeInfo::parseAccepts(bti->getElementTypeInfo(), value_type_info,
            may_not_match);
        if (!res || (res != QTI_IDENT && may_not_match)) {
            parseException(*loc, "PARSE-TYPE-ERROR", "'%s::filled()' argument 'value' expects '%s'; got '%s'",
                QoreTypeInfo::getName(buffer_type_info), QoreTypeInfo::getName(bti->getElementTypeInfo()),
                QoreTypeInfo::getName(value_type_info));
            new_args.discard(nullptr);
            return -1;
        }
    }

    val = new NewComplexBufferNode(loc, buffer_type_info, new_args, init_kind);
    return parse_init_value(val, parse_context);
}

// eval method against an object where the assumed qoreclass and method were saved at parse time
QoreValue AbstractMethodCallNode::exec(QoreObject* o, const char* c_str, const qore_class_private* ctx,
        ExceptionSink* xsink) const {
    // Assert: if parse_args is set but args is null, resolveParseArgs() was not called.
    // This catches AOT deserialization bugs where parse_init was skipped.
    assert(!parse_args || args || tmp_args
        || !"AbstractMethodCallNode::exec(): parse_args set but args is null; "
           "call resolveParseArgs() after AOT deserialization");
    //QORE_TRACE("AbstractMethodCallNode::exec()");

    /* the class and method saved at parse time are used here for this run-time
        optimization: the method pointer saved at parse time is used to execute the
        method directly if the object used at run-time is of the same class as
        either the method or the parse-time class.  Actually any class between the
        parse-time class and the method's class could be used, however I'd have to
        check and make sure that search would be quicker than the quick check
        implemented below on average
    */
    if (qc && method && (o->getClass() == qc || o->getClass() == method->getClass())) {
        //printd(5, "AbstractMethodCallNode::exec() using parse info for %s::%s() qc: %s (o: %s) v: %p\n",
        //    method->getClassName(), method->getName(), qc->getName(), o->getClass()->getName(), variant);
        assert(method);
        if (!o->isValid()) {
            if (variant) {
                xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s(%s) on an object that has " \
                    "already been deleted", qc->getName(), method->getName(),
                    variant->getSignature()->getSignatureText());
            } else {
                xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has " \
                    "already been deleted", qc->getName(), method->getName());
            }
            return QoreValue();
        }

        RuntimeConfig& rc = rc_get_current_ref();
        const QoreTypeParamInstantiation* explicit_inst = getExplicitTypeParamInstantiation();
        // When tmp_args is true (clone from IR interpreter), use evalTmpArgs to preserve
        // ReferenceNode values in the arg list. The eval() path goes through
        // CodeEvaluationHelper with const args, which calls evalList() and dereferences
        // ReferenceNodes.
        if (tmp_args) {
            return qore_method_private::evalTmpArgs(*method, xsink, rc, o, args, ctx, variant, nullptr,
                explicit_inst);
        }
        return variant
            ? qore_method_private::evalNormalVariant(*method, xsink, rc, o,
                reinterpret_cast<const QoreExternalMethodVariant*>(variant), args, explicit_inst)
            : qore_method_private::eval(*method, xsink, rc, o, args, ctx, nullptr, nullptr, explicit_inst);
    }
    //printd(5, "AbstractMethodCallNode::exec() calling QoreObject::evalMethod() for %s::%s()\n", o->getClassName(),
    //    c_str);
    RuntimeConfig& rc = rc_get_current_ref();
    const QoreTypeParamInstantiation* explicit_inst = getExplicitTypeParamInstantiation();
    if (tmp_args) {
        // Dynamic dispatch with pre-evaluated args: look up the method on the actual class
        // and use evalTmpArgs to preserve ReferenceNode values
        const qore_class_private* priv = qore_class_private::get(*o->getClass());
        const QoreMethod* w = priv->getMethodForEval(c_str, o->getProgram(), ctx, xsink);
        if (*xsink) {
            return QoreValue();
        }
        if (w) {
            return qore_method_private::evalTmpArgs(*w, xsink, rc, o, args, ctx, nullptr, nullptr, explicit_inst);
        }
    }
    return qore_class_private::get(*o->getClass())->evalMethod(o, c_str, args, ctx, rc, xsink, explicit_inst);
}

const QoreTypeInfo* AbstractMethodCallNode::getTypeInfo() const {
    const QoreTypeInfo* rv = variant
        ? variant->parseGetReturnTypeInfo()
        : (method
            ? qore_method_private::get(*method)->getFunction()->parseGetUniqueReturnTypeInfo()
            : nullptr);
    return qore_substitute_type_params_if_needed(rv, receiver_type_info, getTypeParamInstantiation());
}

static void invalid_access(const QoreProgramLocation* loc, QoreFunction* func) {
   // func will always be non-zero with builtin functions
   const char* class_name = func->className();
   parse_error(*loc, "parse options do not allow access to builtin %s '%s%s%s()'", class_name ? "method" : "function",
        class_name ? class_name : "", class_name ? "::" : "", func->getName());
}

static void warn_retval_ignored(const QoreProgramLocation* loc, QoreFunction* func, bool is_bg_call = false) {
    const char* class_name = func->className();
    qore_program_private::makeParseWarning(
        getProgram(),
        *loc,
        QP_WARN_RETURN_VALUE_IGNORED,
        "RETURN-VALUE-IGNORED",
        "%s %s %s%s%s() does not have any side effects and the return value is ignored; to disable this warning, " \
            "use '%%disable-warning return-value-ignored' in your code",
            is_bg_call ? "background call to" : "call to",
            class_name ? "method" : "function",
            class_name ? class_name : "",
            class_name ? "::" : "",
            func->getName());
}

static void warn_only_may_throw_and_retval_ignored(const QoreProgramLocation* loc, QoreFunction* func,
        bool is_bg_call = false) {
    const char* class_name = func->className();
    qore_program_private::makeParseWarning(
        getProgram(),
        *loc,
        QP_WARN_RETURN_VALUE_IGNORED,
        "RETURN-VALUE-IGNORED",
        "%s %s %s%s%s() does not have any side effects except that it may throw an exception and the return value " \
            "is ignored; to disable this warning, use '%%disable-warning return-value-ignored' in your code",
            is_bg_call ? "background call to" : "call to",
            class_name ? "method" : "function",
            class_name ? class_name : "",
            class_name ? "::" : "",
            func->getName());
}

static void warn_deprecated(const QoreProgramLocation* loc, QoreFunction* func) {
    const char* class_name = func->className();
    qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_DEPRECATED, "DEPRECATED", "call to " \
        "deprecated %s %s%s%s(); to disable this warning, use '%%disable-warning deprecated' in your code",
        class_name ? "method" : "function", class_name ? class_name : "", class_name ? "::" : "", func->getName());
}

static void check_flags(const QoreProgramLocation* loc, QoreFunction* func, int64 flags, int64 pflag) {
    if (pflag & (PF_RETURN_VALUE_IGNORED | PF_BACKGROUND)) {
        bool is_bg_call = (pflag & PF_BACKGROUND);
        if ((flags & QCF_CONSTANT) == QCF_CONSTANT) {
            warn_retval_ignored(loc, func, is_bg_call);
        } else if (flags & QCF_RET_VALUE_ONLY && (pflag & PF_RETURN_VALUE_IGNORED)) {
            warn_only_may_throw_and_retval_ignored(loc, func, is_bg_call);
        }
    }
    if (flags & QCF_DEPRECATED) {
        warn_deprecated(loc, func);
    }
}

void FunctionCallBase::resolveParseArgs() {
    if (!parse_args || args) {
        return;
    }
    // Check if any parse_args entry contains an AST node that needs evaluation.
    // AOT EXPR_TREE deserialization may produce sub-expression nodes (e.g.,
    // StaticMethodCallNode for TypedHash::forName("StatInfo")) as argument values.
    // When such nodes exist, create the args list with needs_eval_flag=true so that
    // CodeEvaluationHelper::evalList() properly evaluates each entry at runtime.
    // This matches the pattern used by read_node_EN_SELF_CALL.
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

int FunctionCallBase::resolveExplicitTypeArgs(const QoreProgramLocation* loc) {
    if (!has_explicit_type_args || explicit_parse_type_args.empty()) {
        return 0;
    }

    int err = 0;
    explicit_type_args.clear();
    explicit_type_args.reserve(explicit_parse_type_args.size());
    for (QoreParseTypeInfo* pti : explicit_parse_type_args) {
        explicit_type_args.push_back(QoreParseTypeInfo::resolveAny(pti, loc, err));
        delete pti;
    }
    explicit_parse_type_args.clear();
    explicit_runtime_type_param_instantiation.owner = nullptr;
    explicit_runtime_type_param_instantiation.type_args = explicit_type_args;
    return err;
}

int FunctionCallBase::parseArgsVariant(const QoreProgramLocation* loc, QoreParseContext& parse_context,
        QoreFunction* func, qore_ns_private* ns, bool infer_class_receiver_from_args,
        const char* receiver_inference_call_desc) {
    int err = 0;
    QoreParseAnalysis arg_analysis;

    // number of arguments in call
    unsigned num_args = parse_args ? parse_args->size() : 0;
    bool named_args = parse_args && parse_args->hasNamedArgs();
    name_vec_t arg_names;
    if (named_args) {
        const QoreParseListNode::arg_name_vec_t& names = parse_args->getArgNamesVector();
        arg_names.assign(names.begin(), names.end());
    }

    // argument type list
    type_vec_t argTypeInfo;

    // initialize arguments and setup argument type list (argTypeInfo)
    if (num_args) {
        // issue #2993: do not initialize args with the "return value ignored" parse flag set
        QoreParseContextFlagHelper fh(parse_context);
        fh.unsetFlags(PF_RETURN_VALUE_IGNORED | PF_BACKGROUND);
        {
            QoreParseContextAnalysisHelper ah(parse_context);
            err = parse_args->initArgs(parse_context, argTypeInfo, args);
            arg_analysis = parse_context.analysis;
        }
        parsed_arg_type_info = argTypeInfo;
        parse_args = nullptr;

    }
    parse_context.typeInfo = nullptr;

    if (resolveExplicitTypeArgs(loc) && !err) {
        err = -1;
    }

    //printd(5, "FunctionCallBase::parseArgsVariant() this: %p args: %p '%s' func: %p\n", this, args,
    //    args ? get_full_type_name(args) : "n/a", func);

    // resolves pending signatures unconditionally
    if (func) {
        func->resolvePendingSignatures();

        // initialize function or class immediately for possible error messages later (also in case of constant
        // expressions for immediate evaluation)
        const QoreClass* qc = func->getClass();
        if (qc) {
            if (qore_class_private::get(*const_cast<QoreClass*>(qc))->parseInitPartial() && !err) {
                err = -1;
            }
            qore_ns_private* clsns = qore_class_private::get(*qc)->ns;
            NamespaceParseContextHelper nspch(clsns);
            QoreParseClassHelper qpch(const_cast<QoreClass*>(qc));
            if (func->parseInit(clsns) && !err) {
                err = -1;
            }
        } else {
            func->parseInit(ns);
        }

        const qore_class_private* class_ctx = qc ? parse_get_class_priv() : nullptr;
        if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*qc, class_ctx)) {
            class_ctx = nullptr;
        }

        if (!receiver_type_info && qc && qc->hasTypeParameters()) {
            bool constructor_call = !strcmp(func->getName(), "constructor");
            const QoreTypeInfo* inferred_receiver = func->parseInferClassReceiverTypeInfo(loc, argTypeInfo,
                named_args ? &arg_names : nullptr, class_ctx, err, parse_context.expected_type_info,
                constructor_call || infer_class_receiver_from_args,
                has_explicit_type_args ? &explicit_type_args : nullptr,
                receiver_inference_call_desc
                    ? receiver_inference_call_desc
                    : (constructor_call ? "constructor call" : "generic class call"));
            if (inferred_receiver) {
                receiver_type_info = inferred_receiver;
            }
        }

        // find variant
        QoreNamedArgBinding named_binding;
        type_param_instantiation.clear();
        variant = named_args
            ? func->parseFindVariantNamed(loc, argTypeInfo, arg_names, class_ctx, err, named_binding,
                receiver_type_info, &type_param_instantiation, has_explicit_type_args ? &explicit_type_args : nullptr)
            : func->parseFindVariant(loc, argTypeInfo, class_ctx, err, receiver_type_info,
                &type_param_instantiation, has_explicit_type_args ? &explicit_type_args : nullptr);

        if (named_args) {
            if (!variant) {
                if (!err) {
                    qore_program_private::makeParseException(parse_context.pgm, *loc, "NAMED-CALL-NOT-SUPPORTED",
                        new QoreStringNode("named arguments require a parse-time-resolved signature in this version; "
                            "this call is ambiguous or depends on runtime argument types, so use positional arguments "
                            "or make the target type explicit"));
                    err = -1;
                }
            } else if (args) {
                qore_list_private::setNeedsEval(*args);
                qore_list_private::get(args)->setCallArgEvalMap(std::move(named_binding.source_to_param),
                    named_binding.result_size);
            }
        }

        /*
        printd(5, "FunctionCallBase::parseArgsVariant() this: %p (%s::)%s ign: %d func: %p variant: %p rt: %s\n",
            this, func->className() ? func->className() : "", func->getName(),
            parse_context.pflag & PF_RETURN_VALUE_IGNORED, func, variant,
            QoreTypeInfo::getName(func->parseGetUniqueReturnTypeInfo()));
        */

        if (variant) {
            printd(5, "FunctionCallBase::parseArgsVariant() this: %p (%s::)%s variant: %p f: %lld (%lld) (%lld) " \
                "rt: %s\n", this, func->className() ? func->className() : "", func->getName(), variant,
                variant->getFunctionality(), variant->getFlags(), variant->getFlags() & QCF_RET_VALUE_ONLY,
                QoreTypeInfo::getName(variant->parseGetReturnTypeInfo()));
            if (qc) {
                assert(dynamic_cast<const MethodVariantBase*>(variant));
                const MethodVariantBase* mv = reinterpret_cast<const MethodVariantBase*>(variant);
                if (mv->isAbstract()) {
                    //printd(5, "FunctionCallBase::parseArgsVariant() found abstract %s::%s\n", qc->getName(),
                    //    func->getName());
                    // issue #3387: set return type before clearing variant
                    parse_context.typeInfo = qore_substitute_type_params_if_needed(mv->parseGetReturnTypeInfo(),
                        receiver_type_info, &type_param_instantiation);
                    variant = nullptr;
                    func = nullptr;
                    return err;
                } else if (mv->isPrivate() && !qore_class_private::parseCheckPrivateClassAccess(*qc)) {
                    parse_error(*loc, "illegal call to private method variant %s::%s(%s)", qc->getName(),
                        func->getName(), variant->getSignature()->getSignatureText());
                    if (!err) {
                        err = -1;
                    }
                }
            }
            if (variant) {
                int64 dflags = variant->getFunctionality();
                //printd(5, "FunctionCallBase::parseArgsVariant() this: %p (%s::)%s variant: %p dflags: " QLLD
                //    " fdflags: " QLLD "\n", this, func->className() ? func->className() : "", func->getName(),
                //    variant, dflags, func->parseGetUniqueFunctionality());
                if (dflags && qore_program_private::parseAddDomain(parse_context.pgm, dflags)) {
                    invalid_access(loc, func);
                    if (!err) {
                        err = -1;
                    }
                }
                int64 flags = variant->getFlags();
                check_flags(loc, func, flags, parse_context.pflag);
            }
        } else {
            //printd(5, "FunctionCallBase::parseArgsVariant() this: %p func: %p f: %lld (%lld) c: %lld (%lld)\n",
            //    this, func, func->parseGetUniqueFunctionality(),
            //    func->parseGetUniqueFunctionality() & parse_get_parse_options(), func->parseGetUniqueFlags(),
            //    func->parseGetUniqueFlags() & QCF_RET_VALUE_ONLY);

            int64 dflags = func->parseGetUniqueFunctionality();
            if (dflags && qore_program_private::parseAddDomain(parse_context.pgm, dflags)) {
                invalid_access(loc, func);
                if (!err) {
                    err = -1;
                }
            }
            check_flags(loc, func, func->parseGetUniqueFlags(), parse_context.pflag);
        }

        parse_context.typeInfo = qore_substitute_type_params_if_needed(
            variant ? variant->parseGetReturnTypeInfo() : func->parseGetUniqueReturnTypeInfo(),
            receiver_type_info, &type_param_instantiation);

        //printd(5, "FunctionCallBase::parseArgsVariant() this: %p func: %s variant: %p pflag: %d pe: %d\n", this,
        //    func ? func->getName() : "n/a", variant, pflag, func ? func->empty() : -1);
    } else {
        parse_context.typeInfo = nullptr;
    }

    //printd(5, "FunctionCallBase::parseArgsVariant() this: %p func: %s variant: %p args: %p (%zd)\n", this,
    //    func ? func->getName() : "n/a", variant, args, args ? args->size() : 0);
    parse_context.analysis.clear();
    if (parse_context.typeInfo) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = parse_context.typeInfo;
        if (arg_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
            && QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
            parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
        }
    }
    if (arg_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}

QoreValue SelfFunctionCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue SelfFunctionCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    QoreObject* self = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    assert(self);

    const qore_class_private* runtime_cls = rc.getClass() ? rc.getClass() : runtime_get_class();

    if (is_copy) {
        return self->getClass()->execCopy(self, xsink);
    }

    if (ns.size() == 1) {
        // When the method pointer is resolved to a non-abstract multi-variant method AND the
        // runtime class differs from the declaring class (a derived class overrides some
        // variants), name-based dispatch via exec() finds only the overriding class's
        // variants, hiding inherited overloads.  In that case fall back to the parse-time
        // method pointer so cross-hierarchy overload resolution still works.
        // Only applies when the declaring method has multiple variants (no hiding possible
        // with a single variant) and is not abstract (abstract methods must use virtual
        // dispatch to reach the concrete implementation).
        if (method && !is_abstract && self->getClass() != method->getClass()
                && qore_method_private::get(*method)->getFunction()->numVariants() > 1) {
            // First try virtual dispatch: if self's class (or any ancestor between it and
            // method's class) overrides this method, prefer the override.  Without this,
            // calls to a base-class method from another base-class method statically bind to
            // the base implementation even when the derived class has overridden it.
            //
            // Use parse-time variant signature when available to verify the override has
            // a matching variant (covers the case where the derived class only overrides
            // some variants of a multi-variant method).  When no parse-time variant was
            // resolved (e.g. the call-site argument types couldn't disambiguate at parse),
            // fall back to runtime name-based dispatch via exec(), which finds and calls
            // the most-derived QoreMethod by name.
            const qore_class_private* obj_priv = qore_class_private::get(*self->getClass());
            const QoreMethod* derived = obj_priv->getMethodForEval(ns.ostr, self->getProgram(),
                class_ctx ? class_ctx : runtime_cls, xsink);
            if (*xsink) {
                return QoreValue();
            }
            if (derived && derived != method) {
                if (variant) {
                    const AbstractFunctionSignature* sig = variant->getSignature();
                    if (sig) {
                        unsigned np = sig->numParams();
                        const QoreFunction* dfunc = qore_method_private::get(*derived)->getFunction();
                        QoreFunctionIterator it(*dfunc);
                        while (it.next()) {
                            const AbstractQoreFunctionVariant* dv = it.getVariant();
                            const AbstractFunctionSignature* dsig = dv->getSignature();
                            if (!dsig || dsig->numParams() != np) {
                                continue;
                            }
                            bool match = true;
                            for (unsigned i = 0; i < np; ++i) {
                                if (!QoreTypeInfo::isInputIdentical(sig->getParamTypeInfo(i),
                                        dsig->getParamTypeInfo(i))) {
                                    match = false;
                                    break;
                                }
                            }
                            if (match) {
                                return tmp_args
                                    ? qore_method_private::evalTmpArgs(*derived, xsink, rc, self, args)
                                    : qore_method_private::eval(*derived, xsink, rc, self, args);
                            }
                        }
                    }
                } else {
                    // No parse-time variant — let runtime virtual dispatch resolve via the
                    // derived QoreMethod.  If the derived QoreMethod has no variant matching
                    // the runtime args, an exception will be raised — that's the same as
                    // calling the method by name from outside the class.
                    return exec(self, ns.ostr, class_ctx ? class_ctx : runtime_cls, xsink);
                }
            }
            // No matching override on self's class — use the parse-time method pointer
            // so the caller can reach inherited overloads.  Do NOT pass an explicit class
            // context — let CodeEvaluationHelper use runtime_get_class() for correct access
            // control (same as ns.size() > 1 path).
            return tmp_args
                ? qore_method_private::evalTmpArgs(*method, xsink, rc, self, args)
                : qore_method_private::eval(*method, xsink, rc, self, args);
        }
        // must have a class context here
        assert(class_ctx || runtime_cls);
        return exec(self, ns.ostr, class_ctx ? class_ctx : runtime_cls, xsink);
    }

    if (is_abstract) {
        return qore_class_private::get(*self->getClass())->evalMethod(self, ns.ostr, args,
            class_ctx ? class_ctx : runtime_cls, rc, xsink);
    }

    assert(method);

    return tmp_args
        ? qore_method_private::evalTmpArgs(*method, xsink, rc, self, args)
        : qore_method_private::eval(*method, xsink, rc, self, args);
}

int SelfFunctionCallNode::parseInitCall(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    // issue #3637: qc might be non-null while method is null in case of calls to implicit copy() methods, for example
    int err = parseArgs(parse_context, method ? qore_method_private::get(*method)->getFunction() : nullptr, nullptr);
    if (parse_context.pflag & PF_CONST_METHOD) {
        if (is_copy) {
            parseException(*loc, "READONLY-RECEIVER-ERROR", "cannot call non-const copy() on read-only receiver");
            if (!err) {
                err = -1;
            }
        } else if (variant
            && check_readonly_receiver_method_call(loc, getName(), static_cast<const MethodVariantBase*>(variant))
            && !err) {
            err = -1;
        } else if (!variant && method
            && check_readonly_receiver_method_call(loc, getName(), nullptr,
                qore_method_private::get(*method)->getFunction())
            && !err) {
            err = -1;
        }
    }
    // issue #2380 make sure to set the method correctly if resolved from a hierarchy
    if (variant) {
        method = qore_method_private::resolveVariantMethod(*method, variant);
    }
    if (method) {
        printd(5, "SelfFunctionCallNode::parseInitCall() this: %p resolved '%s' to %p (abstract: %d)\n", this,
            method->getName(), method, qore_method_private::get(*method)->isAbstract());
        // issue #3070: make sure that abstract method calls are resolved at runtime
        if (qore_method_private::get(*method)->isAbstract()) {
            assert(!variant);
            is_abstract = true;
        }
    }
    return err;
}

// called at parse time
int SelfFunctionCallNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    if (!parse_context.oflag) {
        parse_error(*loc, "cannot call method '%s' outside of class code", getName());
        return -1;
    }

    class_ctx = qore_class_private::get(*QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()));

    int err = 0;

    if (!method) {
        printd(5, "SelfFunctionCallNode::parseInitImpl() this: %p resolving base class call '%s'\n", this, ns.ostr);

        // copy method calls will be recognized by name = 0
        if (ns.size() == 1) {
            if (!strcmp(ns.ostr, "copy")) {
                printd(5, "SelfFunctionCallNode::parseInitImpl() this: %p resolved to copy constructor\n", this);
                is_copy = true;
                if (args) {
                    parse_error(*loc, "no arguments may be passed to copy methods (%lu argument%s given in " \
                        "call to %s::copy())", args->size(), args->size() == 1 ? "" : "s", class_ctx->name.c_str());
                    err = -1;
                }
            } else {
                assert(!qc || qore_class_private::get(*qc) == class_ctx);
                // raises a parse exception if it fails
                method = const_cast<qore_class_private*>(class_ctx)->parseResolveSelfMethod(loc, ns.ostr, class_ctx);
                if (!method) {
                    // parse exception raised already
                    return -1;
                }
            }
        } else {
            assert(!qc);
            // possible only if old-style is in effect
            qc = qore_root_ns_private::parseFindScopedClassWithMethod(loc, ns, true);
            // parse exception raised if !qc
            if (!qc) {
                // parse exception raised already
                return -1;
            }
            // raises a parse exception if it fails
            method = const_cast<qore_class_private*>(
                qore_class_private::get(*qc))->parseResolveSelfMethod(loc, ns.getIdentifier(), class_ctx
            );
            if (!method) {
                // parse exception raised already
                return -1;
            }
        }
    }

    // by here, if there are no errors, the class has been initialized
    if (parseInitCall(val, parse_context) && !err) {
        err = -1;
    }
    return err;
}

int SelfFunctionCallNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("in-object method call (%p) to %s::%s()", this, method->getClass()->getName(), method->getName());
    return 0;
}

// if del is true, then the returned QoreString*  should be deleted, if false, then it must not be
QoreString* SelfFunctionCallNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

AbstractQoreNode* SelfFunctionCallNode::makeReferenceNodeAndDeref() {
    AbstractQoreNode* rv;
    if (ns.size() == 1)
        rv = new ParseSelfMethodReferenceNode(loc, ns.takeName());
    else
        rv = new ParseScopedSelfMethodReferenceNode(loc, ns.copy());
    deref();
    return rv;
}

SetSelfFunctionCallNode::SetSelfFunctionCallNode(const SelfFunctionCallNode& old, QoreListNode* args)
        : SelfFunctionCallNode(old, args) {
    runtime_get_object_and_class(self, cls);
    self = runtime_get_stack_object();
    assert(self);
    self->ref();
}

QoreValue SetSelfFunctionCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue SetSelfFunctionCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfigObjectHelper rc_obj_helper(rc, self, cls);
    ObjectSubstitutionHelper osh(self, cls);
    QoreValue rv = SelfFunctionCallNode::evalImpl(rc, needs_deref, xsink);
    self->deref(xsink);
    deref_self = false;
    return rv;
}

/* get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
   the ExceptionSink is only needed for QoreObject where a method may be executed
   use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
   returns -1 for exception raised, 0 = OK
*/
int FunctionCallNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("function call to '%s()' (%p)", getName(), this);
    return 0;
}

// if del is true, then the returned QoreString*  should be deleted, if false, then it must not be
QoreString* FunctionCallNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// eval(): return value requires a deref(xsink)
QoreValue FunctionCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue FunctionCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(!parse_args || args || tmp_args
        || !"FunctionCallNode::evalImpl(): parse_args set but args is null; "
           "call resolveParseArgs() after AOT deserialization");
    QoreFunction* func = fe->getFunction();
    QoreProgram* call_pgm = pgm ? pgm : rc.getProgram();
    if (!call_pgm) {
        call_pgm = getProgram();
    }
    return tmp_args
        ? func->evalFunctionTmpArgs(variant, args, call_pgm, rc, xsink, getExplicitTypeParamInstantiation())
        : func->evalFunction(variant, args, call_pgm, rc, xsink, getExplicitTypeParamInstantiation());
}

int FunctionCallNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    if (fe) {
        return 0;
    }
    //assert(!func);
    assert(c_str);

    bool abr = parse_check_parse_option(PO_ALLOW_BARE_REFS);

    // try to resolve bare reference if allowed
    if (abr) {
        // check for a local variable with the same name
        bool in_closure;
        LocalVar* id = find_local_var(c_str, in_closure);
        if (id) {
            VarRefNode* vrn = new VarRefNode(loc, takeName(), id, in_closure);
            val = new CallReferenceCallNode(loc, vrn, takeParseArgs());
            deref();
            return parse_init_value(val, parse_context);
        }
    }

    // try to resolve a method call if we are parsing in an object context
    if (parse_context.oflag) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo());

        QoreValue n{};  // value-initialized to NOTHING (bits=0)
        if (abr && !qore_class_private::parseResolveInternalMemberAccess(qc, c_str, parse_context.typeInfo)) {
            n = new SelfVarrefNode(loc, takeName());
        } else {
            bool found;
            n = qore_class_private::parseFindConstantValue(const_cast<QoreClass*>(qc), c_str,
                parse_context.typeInfo, found, qore_class_private::get(*qc));
            if (found) {
                n.ref();
                //printd(5, "FunctionCallNode::parseInitImpl() this: %p n: %p (%d -> %d)\n", this, n,
                //  n->reference_count(), n->reference_count() + 1);
            } else {
                // check for class static var reference
                const QoreClass* oqc = nullptr;
                ClassAccess access;
                QoreVarInfo *vi = qore_class_private::parseFindStaticVar(qc, c_str, oqc, access);
                if (vi) {
                    assert(qc);
                    parse_context.typeInfo = vi->getTypeInfo();
                    n = new StaticClassVarRefNode(loc, c_str, *oqc, *vi);
                }
            }
        }

        if (!n.isNothing()) {
            val = new CallReferenceCallNode(loc, n, takeParseArgs());
            deref();
            return parse_init_value(val, parse_context);
        }

        if (abr) {
            SelfFunctionCallNode* sfcn = nullptr;
            if (!strcmp(c_str, "copy")) {
                if (args) {
                    parse_error(*loc, "no arguments may be passed to copy methods (%lu argument%s given in "
                        "call to %s::copy())", args->size(), args->size() == 1 ? "" : "s", qc->getName());
                    return -1;
                }
                sfcn = new SelfFunctionCallNode(loc, takeName(), 0);
            } else {
                const QoreMethod* m = qore_class_private::parseFindSelfMethod(const_cast<QoreClass*>(qc), c_str);
                if (m) {
                    if (!m->isStatic()) {
                        sfcn = new SelfFunctionCallNode(loc, takeName(), takeParseArgs(), m, qc,
                            qore_class_private::get(*qc));
                    } else {
                        val = new StaticMethodCallNode(loc, m, takeParseArgs());
                        deref();
                        return parse_init_value(val, parse_context);
                    }
                }
            }
            if (sfcn) {
                val = sfcn;
                deref();
                return sfcn->parseInitCall(val, parse_context);
            }
        }
    } else {
        qore_class_private* class_ctx = parse_get_class_priv();
        // look for a static method
        if (class_ctx) {
            const QoreMethod* m = class_ctx->parseFindStaticMethod(c_str, class_ctx);
            if (m) {
                val = new StaticMethodCallNode(loc, m, takeParseArgs());
                deref();
                return parse_init_value(val, parse_context);
            }
        }
    }

    return parseInitCall(val, parse_context);
}

int FunctionCallNode::parseInitCall(QoreValue& val, QoreParseContext& parse_context) {
    assert(!fe);
    assert(c_str);
    assert(!parse_context.typeInfo);

    bool abr = parse_check_parse_option(PO_ALLOW_BARE_REFS);

    QoreValue n{};  // value-initialized to NOTHING (bits=0)
    std::string deferred_source_function_path = qore_aot_get_deferred_source_symbol_path(loc, c_str,
        QoreAOTSourceSymbolKind::Function);
    const bool defer_source_function = !deferred_source_function_path.empty();

    // try to resolve a global var
    if (!defer_source_function && abr) {
        Var* v = qore_root_ns_private::parseFindGlobalVar(c_str);
        if (v) {
            n = new GlobalVarRefNode(loc, takeName(), v);
        }
    }

    bool found = !n.isNothing();

    // see if a constant can be resolved
    if (!found && !defer_source_function) {
        n = qore_root_ns_private::parseFindConstantValue(loc, c_str, parse_context.typeInfo, found, false);
        if (found) {
            n.ref();
        }
    }

    if (found) {
        val = new CallReferenceCallNode(loc, n, takeParseArgs());
        deref();
        return parse_init_value(val, parse_context);
    }

    // resolves the function
    if (!defer_source_function) {
        fe = qore_root_ns_private::parseFindFunctionEntry(c_str);
        if (fe && parse_check_parse_option(PO_REQUIRE_TYPES)) {
            qore_root_ns_private::parseMaybeWarnAmbiguousFunctionCall(loc, c_str, fe);
        }
    }
    if (!fe && qore_aot_source_parse_active()) {
        if (has_explicit_type_args) {
            parse_error(*loc, "cannot defer unresolved function call '%s()' with explicit type arguments", c_str);
            return -1;
        }

        if (QoreProgram* pgm = parse_context.pgm ? parse_context.pgm : getProgram()) {
            qore_program_private::recordSourceParseFunctionImport(pgm, loc,
                defer_source_function ? deferred_source_function_path.c_str() : c_str);
        }

        const FunctionEntry* call_function_fe = qore_root_ns_private::parseResolveFunctionEntry(loc,
            "call_function");
        if (!call_function_fe) {
            return -1;
        }

        QoreParseListNode* dynamic_args = new QoreParseListNode(loc);
        dynamic_args->add(new QoreStringNode(defer_source_function ? deferred_source_function_path.c_str() : c_str),
            loc);
        QoreParseListNode* old_args = takeParseArgs();
        if (old_args) {
            dynamic_args->appendFrom(old_args);
            old_args->deref();
        }

        FunctionCallNode* dynamic_call = new FunctionCallNode(loc, call_function_fe, dynamic_args);
        val = dynamic_call;
        free(c_str);
        c_str = nullptr;
        deref();
        return dynamic_call->parseInitFinalizedCall(val, parse_context);
    }

    if (!fe && !defer_source_function) {
        fe = qore_root_ns_private::parseResolveFunctionEntry(loc, c_str);
    }
    free(c_str);
    c_str = nullptr;

    if (!fe) {
        // parse exception raised
        return -1;
    }

    return parseInitFinalizedCall(val, parse_context);
}

int FunctionCallNode::parseInitFinalizedCall(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    assert(fe);
    QoreFunction* func = fe->getFunction();
    return parseArgs(parse_context, func, fe->getNamespace());
}

AbstractQoreNode* FunctionCallNode::makeReferenceNodeAndDerefImpl() {
    return new UnresolvedCallReferenceNode(loc, takeName());
}

AbstractQoreNode* ProgramFunctionCallNode::makeReferenceNodeAndDerefImpl() {
    return new UnresolvedProgramCallReferenceNode(loc, takeName());
}

//DLLEXPORT AbstractQoreNode(qore_type_t t, bool n_value, bool n_needs_eval, bool n_there_can_be_only_one = false, bool n_custom_reference_handlers = false);

NewObjectCallNode::NewObjectCallNode(const QoreClass* qc, QoreListNode* args,
        const QoreTypeInfo* object_type_info)
        : AbstractQoreNode(NT_NEW_OBJECT, false, true),
        FunctionCallBase(nullptr, args), qc(qc), object_type_info(object_type_info) {
    if (!qc) {
        return;
    }
    // Skip variant resolution when there is no program context (e.g. during AOT registration)
    QoreProgram* pgm = getProgram();
    if (!pgm) {
        return;
    }
    const QoreMethod* constructor = qc->getConstructor();
    if (!constructor) {
        if (args && !args->empty()) {
            parse_error(QoreProgramLocation(), "Cannot call class '%s' with no constructor with arguments", qc->getName());
        }
        return;
    }
    // Skip variant resolution when args contain unevaluated expressions (AOT deserialization).
    // runtimeFindVariant inspects arg types, but unevaluated AST nodes (e.g., cast expressions)
    // have node types, not result types. Variant resolution happens at eval time via CodeEvaluationHelper.
    if (args && args->needs_eval()) {
        return;
    }
    ExceptionSink xsink;
    variant = qore_method_private::get(*constructor)->getFunction()->runtimeFindVariant(&xsink, args, false, nullptr,
        object_type_info);

    ExceptionSink* pxs = pgm->getParseExceptionSink();
    if (pxs) {
        pxs->assimilate(xsink);
    } else {
        xsink.clear();
    }
}

QoreValue NewObjectCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue NewObjectCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    const QoreTypeInfo* oti = qore_substitute_type_params_if_needed(object_type_info);
    return qore_class_private::execConstructor(*qc, rc, variant, args, xsink, oti);
}

int ScopedObjectCallNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    int err = 0;
    if (name) {
        assert(!oc);
        // find object class
        std::string deferred_source_class_path = qore_aot_get_deferred_source_symbol_path(loc, name->ostr,
            QoreAOTSourceSymbolKind::Class);
        bool defer_source_class = !deferred_source_class_path.empty();
        if (!defer_source_class && (oc = qore_root_ns_private::parseFindScopedClass(loc, *name, false))) {
            // check if parse options allow access to this class
            int64 cflags = oc->getDomain();
            if (cflags && qore_program_private::parseAddDomain(parse_context.pgm, cflags)) {
                parseException(*loc, "ILLEGAL-CLASS-INSTANTIATION", "parse options do not allow access to the '%s' " \
                    "class", oc->getName());
                err = -1;
            }
        } else if (qore_aot_source_parse_active()) {
            const char* class_path = defer_source_class ? deferred_source_class_path.c_str() : name->ostr;
            if (QoreProgram* pgm = parse_context.pgm ? parse_context.pgm : getProgram()) {
                std::string type_path = "object<";
                type_path += class_path;
                type_path += '>';
                qore_program_private::recordSourceParseTypeImport(pgm, loc, class_path, type_path.c_str(), false,
                    false);
            }

            dynamic_class_name = class_path;
            delete name;
            name = nullptr;
        } else {
            qore_root_ns_private::parseFindScopedClass(loc, *name, true);
            err = -1;
        }
        delete name;
        name = nullptr;
    }
#ifdef DEBUG
    else assert(oc);
#endif

    const QoreMethod* constructor = oc ? qore_class_private::get(*oc)->parseGetConstructor() : nullptr;
    setReceiverTypeInfo(object_type_info);
    if (parseArgs(parse_context,
        constructor
            ? qore_method_private::get(*constructor)->getFunction()
            : nullptr,
        nullptr) && !err) {
        err = -1;
    }

    if (oc) {
        if (!object_type_info && receiver_type_info) {
            const QoreParameterizedClassTypeInfo* pcti = QoreTypeInfo::getParameterizedClassType(receiver_type_info);
            if (pcti && pcti->getBaseClass() == oc) {
                object_type_info = receiver_type_info;
            }
        }

        // parse init the class and check if we're trying to instantiate an abstract class
        qore_class_private::get(*const_cast<QoreClass*>(oc))->parseCheckAbstractNew(loc);

        // initialize class immediately, in case the class will be instantiated immediately after during parsing
        // to be assigned to a constant
        //qore_class_private::parseInit(*const_cast<QoreClass*>(oc));

        parse_context.typeInfo = object_type_info ? object_type_info : oc->getTypeInfo();
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = parse_context.typeInfo;
        desc.sprintf("new %s", oc->getName());
    } else if (!dynamic_class_name.empty()) {
        parse_context.typeInfo = objectTypeInfo;
        parse_context.analysis.clear();
        desc.sprintf("new %s", dynamic_class_name.c_str());
    } else {
        parse_context.typeInfo = nullptr;
        parse_context.analysis.clear();
    }

    //printd(5, "ScopedObjectCallNode::parseInitImpl() this: %p constructor: %p variant: %p\n", this, constructor,
    //  variant);

    if (((constructor && (qore_method_private::getAccess(*constructor) > Public))
        || (variant && CONMV_const(variant)->isPrivate()))
            && !qore_class_private::parseCheckPrivateClassAccess(*oc)) {
        if (variant) {
            parse_error(*loc, "illegal external access to private constructor %s::constructor(%s)", oc->getName(),
                variant->getSignature()->getSignatureText());
            if (!err) {
                err = -1;
            }
        } else {
            parse_error(*loc, "illegal external access to private constructor of class %s", oc->getName());
            if (!err) {
                err = -1;
            }
        }
    }

    //printd(5, "ScopedObjectCallNode::parseInitImpl() this: %p class: %s (%p) constructor: %p function: %p "
    //    "variant: %p\n", this, oc->getName(), oc, constructor,
    //    constructor ? qore_method_private::get(*constructor)->getFunction() : 0, variant);
    return err;
}

QoreValue ScopedObjectCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ScopedObjectCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(!parse_args || args || tmp_args
        || !"ScopedObjectCallNode::evalImpl(): parse_args set but args is null; "
           "call resolveParseArgs() after AOT deserialization");
    if (!oc) {
        if (dynamic_class_name.empty()) {
            xsink->raiseException("CREATE-OBJECT-ERROR", "cannot resolve class for instantiation");
            return QoreValue();
        }
        const QoreClass* qc = qore_aot_resolve_class_ref(getProgram(), dynamic_class_name.c_str(), false);
        if (!qc) {
            if (!*xsink) {
                xsink->raiseException("AOT-PENDING-CLASS",
                    "class '%s' is pending AOT source linking for instantiation", dynamic_class_name.c_str());
            }
            return QoreValue();
        }
        if (getProgram()->getParseOptions() & qc->getDomain()) {
            xsink->raiseException("CREATE-OBJECT-ERROR", "current Program sandboxing restrictions do not allow "
                "access to the '%s' class", qc->getName());
            return QoreValue();
        }
        if (qore_class_private::runtimeCheckInstantiateClass(*qc, xsink)) {
            return QoreValue();
        }
        const QoreTypeInfo* oti = qore_substitute_type_params_if_needed(object_type_info);
        return qore_class_private::execConstructor(*qc, rc, variant, args, xsink, oti);
    }
    const QoreTypeInfo* oti = qore_substitute_type_params_if_needed(object_type_info);
    return qore_class_private::execConstructor(*oc, rc, variant, args, xsink, oti);
}

QoreValue MethodCallNode::exec(QoreObject* o, ExceptionSink* xsink) const {
    // issue #3596: do not use the context class if it's not compatible with "o"
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*o->getClass(), class_ctx)) {
        class_ctx = nullptr;
    }
    return AbstractMethodCallNode::exec(o, c_str, class_ctx, xsink);
}

QoreValue MethodCallNode::execPseudo(const QoreValue n, ExceptionSink* xsink) const {
   //printd(5, "MethodCallNode::execPseudo() %s::%s() variant: %p\n", qc->getName(), method->getName(), variant);
   // if n is nothing make sure and use the "<nothing>" class with a dynamic method lookup
   RuntimeConfig& rc = rc_get_current_ref();
   if (n.isNothing() && qc != QC_PSEUDONOTHING)
      return qore_class_private::evalPseudoMethod(QC_PSEUDONOTHING, n, method->getName(), args, rc, xsink);
   else
      return qore_class_private::evalPseudoMethod(qc, method, variant, n, args, rc, xsink);
}

AbstractQoreNode* StaticMethodCallNode::makeReferenceNodeAndDeref() {
   if (args) {
      parse_error(*loc, "argument given to static method call reference");
      return this;
   }

   UnresolvedStaticMethodCallReferenceNode* rv = new UnresolvedStaticMethodCallReferenceNode(loc, takeScope());
   deref();
   return rv;
}

std::string StaticMethodCallNode::getClassPath() const {
    if (method) {
        return method->getClass()->getNamespacePath();
    }
    return scope && scope->size() >= 2 ? get_static_scope_class_path(*scope) : std::string();
}

int StaticMethodCallNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    int err = 0;
    if (!method) {
        assert(!parse_context.typeInfo);
        bool abr = parse_check_parse_option(PO_ALLOW_BARE_REFS);

        bool parameterized_receiver = static_scope_has_parameterized_receiver(*scope);
        bool defer_source_static_receiver = false;
        bool defer_source_function = false;
        std::string source_receiver_path;
        std::string deferred_source_receiver_path;
        std::string deferred_source_function_path;
        QoreClass* qc = nullptr;
        if (parameterized_receiver) {
            receiver_type_info = resolve_static_scope_receiver_type(loc, *scope, err);
            if (err) {
                return -1;
            }
            if (QoreTypeInfo::getComplexBufferType(receiver_type_info)) {
                AbstractQoreNode* original = val.getInternalNode();
                QoreParseListNode* factory_args = takeParseArgs();
                int factory_err = parse_init_complex_buffer_factory(loc, receiver_type_info, scope->getIdentifier(),
                    factory_args, val, parse_context);
                delete scope;
                scope = nullptr;
                if (val.hasNode() && val.getInternalNode() != original) {
                    deref();
                }
                return factory_err;
            }
            const QoreParameterizedClassTypeInfo* pcti = QoreTypeInfo::getParameterizedClassType(receiver_type_info);
            assert(pcti);
            qc = const_cast<QoreClass*>(pcti->getBaseClass());
        } else {
            source_receiver_path = get_static_scope_class_path(*scope);
            deferred_source_receiver_path = qore_aot_get_deferred_source_symbol_path(loc,
                source_receiver_path.c_str(),
                QoreAOTSourceSymbolKind::Class);
            defer_source_static_receiver = !deferred_source_receiver_path.empty();
            if (scope->size() >= 2) {
                deferred_source_function_path = qore_aot_get_deferred_source_symbol_path(loc, scope->ostr,
                    QoreAOTSourceSymbolKind::Function);
                defer_source_function = !deferred_source_function_path.empty();
                if (!defer_source_function && !defer_source_static_receiver) {
                    deferred_source_function_path = qore_aot_get_deferred_source_symbol_path(loc,
                        scope->getIdentifier(),
                        QoreAOTSourceSymbolKind::Function);
                    defer_source_function = !deferred_source_function_path.empty();
                }
            }
            if (!defer_source_static_receiver) {
                qc = qore_root_ns_private::parseFindScopedClassWithMethod(loc, *scope, false);
            }
        }

        const QoreClass* pc = parse_context.oflag
            && abr ? QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()) : nullptr;

        qore_class_private* class_ctx = parse_context.oflag
            ? qore_class_private::get(
                *const_cast<QoreClass*>(pc
                    ? pc
                    : QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()))
                )
            : parse_get_class_priv();

        // see if this is a call to a base class method if bare refs are allowed
        // and we're parsing in a class context and the class found is in the
        // current class parse context
        if (qc) {
            if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*qc, class_ctx))
                class_ctx = nullptr;
            if (parameterized_receiver) {
                method = qore_class_private::get(*qc)->parseFindStaticMethod(scope->getIdentifier(), class_ctx);
                if (!method) {
                    const QoreMethod* any = qore_class_private::get(*qc)->parseFindAnyMethodStaticFirst(
                        scope->getIdentifier(), class_ctx);
                    if (any && !any->isStatic()) {
                        parseException(*loc, "INVALID-METHOD", "cannot call instance method %s::%s() through "
                            "parameterized static target '%s'; call the method on an object instance instead",
                            qc->getName(), scope->getIdentifier(),
                            get_static_scope_receiver_type_path(*scope).c_str());
                        return -1;
                    }
                }
            } else if (pc && class_ctx) {
                // checks access already
                method = qore_class_private::get(*qc)->parseFindAnyMethodStaticFirst(scope->getIdentifier(),
                    class_ctx);
                if (method && !method->isStatic() && !strcmp(method->getName(), "copy")) {
                    parseException(*loc, "INVALID-METHOD", "cannot explicitly call base class %s::%s() copy method",
                        qc->getName(), scope->getIdentifier());
                    return -1;
                }
                //printd(5, "StaticMethodCallNode::parseInitImpl() '%s' pc: %s qc: %smethod: %p\n", scope->ostr,
                //  pc->getName(), qc->getName(), method);
            } else
                method = qore_class_private::get(*qc)->parseFindStaticMethod(scope->getIdentifier(), class_ctx);
        }

        //printd(5, "StaticMethodCallNode::parseInitImpl() %s qc: %p '%s' method: %p '%s()'\n", scope->ostr, qc,
        //  qc ? qc->getName() : "n/a", method, scope->getIdentifier());

        // see if a constant can be resolved
        if (!method) {
            if (parameterized_receiver) {
                parse_error(*loc, "cannot resolve static method call '%s()'; class '%s' has no reachable static "
                    "method named '%s'", scope->ostr, qc ? qc->getName() : "(unknown)", scope->getIdentifier());
                return -1;
            }
            if (!defer_source_static_receiver) {
                // see if this is a function call to a function defined in a namespace
                const FunctionEntry* f = qore_root_ns_private::parseResolveFunctionEntry(*scope);
                if (f) {
                    FunctionCallNode* fcn = new FunctionCallNode(loc, f, takeParseArgs());
                    val = fcn;
                    deref();
                    return fcn->parseInitFinalizedCall(val, parse_context);
                }
            }

            /*
            ValueHolder n(nullptr);

            if (abr) {
                Var* v = qore_root_ns_private::parseFindGlobalVar(*scope);
                if (v)
                    n = new GlobalVarRefNode(loc, strdup(scope->getIdentifier()), v);
            }

            bool found = false;
            if (n->isNothing()) {
                n = qore_root_ns_private::parseFindReferencedConstantValue(loc, *scope, typeInfo, found, false);
            }
            */

            bool found = false;
            QoreValue n = defer_source_static_receiver
                ? QoreValue()
                : qore_root_ns_private::parseFindReferencedConstantValue(loc, *scope, parse_context.typeInfo,
                    found, false);

            if (found) {
                val = new CallReferenceCallNode(loc, n, takeParseArgs());
                deref();
                return parse_init_value(val, parse_context);
            } else {
                assert(!n);
            }

            if (qore_aot_source_parse_active() && scope->size() >= 2) {
                if (defer_source_function) {
                    const char* function_path = deferred_source_function_path.empty()
                        ? scope->ostr : deferred_source_function_path.c_str();
                    if (QoreProgram* pgm = parse_context.pgm ? parse_context.pgm : getProgram()) {
                        qore_program_private::recordSourceParseFunctionImport(pgm, loc, function_path);
                    }

                    const FunctionEntry* call_function_fe = qore_root_ns_private::parseResolveFunctionEntry(loc,
                        "call_function");
                    if (!call_function_fe) {
                        return -1;
                    }

                    QoreParseListNode* dynamic_args = new QoreParseListNode(loc);
                    dynamic_args->add(new QoreStringNode(function_path), loc);
                    QoreParseListNode* old_args = takeParseArgs();
                    if (old_args) {
                        dynamic_args->appendFrom(old_args);
                        old_args->deref();
                    }

                    FunctionCallNode* dynamic_call = new FunctionCallNode(loc, call_function_fe, dynamic_args);
                    val = dynamic_call;
                    delete scope;
                    scope = nullptr;
                    deref();
                    return dynamic_call->parseInitFinalizedCall(val, parse_context);
                }

                {
                    if (defer_source_static_receiver && deferred_source_receiver_path != source_receiver_path) {
                        std::string method_path = deferred_source_receiver_path;
                        method_path += "::";
                        method_path += scope->getIdentifier();
                        delete scope;
                        scope = new NamedScope(strdup(method_path.c_str()));
                    }
                    if (QoreProgram* pgm = parse_context.pgm ? parse_context.pgm : getProgram()) {
                        qore_program_private::recordSourceParseMethodImport(pgm, loc, scope->ostr);
                    }

                    return parseArgs(parse_context, nullptr, nullptr);
                }
            }

            {
                // suggest a near-match function name
                QoreSuggestionList sl(scope->ostr);
                qore_root_ns_private::addFunctionSuggestions(sl);
                std::string hint = sl.getHint();
                if (!hint.empty()) {
                    parse_error(*loc, "cannot resolve call '%s()' to any reachable and callable object; %s",
                        scope->ostr, hint.c_str());
                } else {
                    parse_error(*loc, "cannot resolve call '%s()' to any reachable and callable object",
                        scope->ostr);
                }
            }
            return -1;
        }

        // check class capabilities against parse options
        if (qore_program_private::parseAddDomain(parse_context.pgm, qc->getDomain())) {
            parseException(*loc, "INVALID-METHOD", "class '%s' implements capabilities that are not allowed by " \
                "current parse options", qc->getName());
            return -1;
        }

        if (!method->isStatic()) {
            assert(!parameterized_receiver);
            SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(loc, scope->takeName(), takeParseArgs(), method, qc,
                class_ctx);
            val = sfcn;
            deref();
            return parse_init_value(val, parse_context);
        }

        delete scope;
        scope = nullptr;
    } else {
        assert(!scope);
        // check class capabilities against parse options
        if (qore_program_private::parseAddDomain(parse_context.pgm, method->getClass()->getDomain())) {
            parseException(*loc, "INVALID-METHOD", "class '%s' implements capabilities that are not allowed by " \
                "current parse options", method->getClass()->getName());
            err = -1;
        }
    }

    assert(method->isStatic());

    std::string receiver_inference_call_desc = "static method call '";
    receiver_inference_call_desc += method->getClass()->getNamespacePath(false);
    receiver_inference_call_desc += "::";
    receiver_inference_call_desc += method->getName();
    receiver_inference_call_desc += "()'";
    record_source_parse_reflection_class_for_name_import(parse_context.pgm, loc, method, parse_args);
    if (parseArgs(parse_context, qore_method_private::get(*method)->getFunction(), nullptr, true,
            receiver_inference_call_desc.c_str()) && !err) {
        err = -1;
    }
    // issue #2380 make sure to set the method correctly if resolved from a hierarchy
    if (variant) {
        method = qore_method_private::resolveVariantMethod(*method, variant);
    }
    assert(err || method);
    return err;
}

QoreValue StaticMethodCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue StaticMethodCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(!parse_args || args || tmp_args
        || !"StaticMethodCallNode::evalImpl(): parse_args set but args is null; "
           "call resolveParseArgs() after AOT deserialization");
    if (!method) {
        if (qore_aot_source_parse_active() && scope && scope->size() >= 2) {
            xsink->raiseException("AOT-PENDING-FUNCTION",
                "static method call '%s::%s()' is pending AOT source linking",
                getClassPath().c_str(), getName());
            return QoreValue();
        }
        xsink->raiseException("METHOD-CALL-ERROR", "cannot evaluate unresolved static method call '%s::%s()'",
            getClassPath().c_str(), getName());
        return QoreValue();
    }
    // Pass the class context so that private static methods called from within the same class are visible
    const qore_class_private* cctx = qore_class_private::get(*qore_method_private::get(*method)->parent_class);
    const QoreTypeParamInstantiation* explicit_inst = getExplicitTypeParamInstantiation();
    if (tmp_args) {
        return qore_method_private::evalTmpArgs(*method, xsink, rc, nullptr, args, cctx, variant, receiver_type_info,
            explicit_inst);
    }
    return variant
        ? qore_method_private::evalNormalVariant(*method, xsink, rc, nullptr,
            reinterpret_cast<const QoreExternalMethodVariant*>(variant), args, explicit_inst, receiver_type_info)
        : qore_method_private::eval(*method, xsink, rc, nullptr, args, cctx, nullptr, receiver_type_info,
            explicit_inst);
}

const QoreTypeInfo* StaticMethodCallNode::getTypeInfo() const {
    const QoreTypeInfo* rv = variant
        ? variant->parseGetReturnTypeInfo()
        : (method
            ? qore_method_private::get(*method)->getFunction()->parseGetUniqueReturnTypeInfo()
            : 0);
    return qore_substitute_type_params_if_needed(rv, receiver_type_info, getTypeParamInstantiation());
}
