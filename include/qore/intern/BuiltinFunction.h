/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    BuiltinFunction.h

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

#ifndef _QORE_BUILTINFUNCTION_H

#define _QORE_BUILTINFUNCTION_H

#include <vector>
#include <string>

class BCList;
class BCEAList;

class BuiltinSignature : public AbstractFunctionSignature {
public:
    DLLLOCAL BuiltinSignature(bool extra_args, const QoreTypeInfo* n_returnTypeInfo, const type_vec_t& n_typeList,
            const arg_vec_t& n_defaultArgList, const name_vec_t& n_names)
            : AbstractFunctionSignature(n_returnTypeInfo, n_typeList, n_defaultArgList, n_names, extra_args) {
        for (unsigned i = 0; i < typeList.size(); ++i) {
            bool hasDefaultArg = i < defaultArgList.size() && defaultArgList[i];
            if (QoreTypeInfo::hasType(typeList[i])) {
                ++num_param_types;
                if (!hasDefaultArg) {
                    ++min_param_types;
                }
            }

            str.append(QoreTypeInfo::getPath(typeList[i]));
            if (names.size() > i && !names[i].empty()) {
                str.append(" ");
                str.append(names[i]);
            }

            if (hasDefaultArg) {
                addDefaultArgument(str, defaultArgList[i]);
            }

            // add a comma to the signature string if it's not the last parameter
            if (i != (typeList.size() - 1)) {
                str.append(", ");
            }
        }
        if (extra_args) {
            if (!typeList.empty()) {
                str.append(", ");
            }
            str.append("...");
        }
    }

    DLLLOCAL virtual ~BuiltinSignature() {
    }

    DLLLOCAL virtual const QoreTypeInfo* parseGetReturnTypeInfo(int& err) const {
        return returnTypeInfo;
    }

    DLLLOCAL virtual const QoreParseTypeInfo* getParseParamTypeInfo(unsigned num) const {
        return nullptr;
    }
};

// the following defines the virtual functions that are common to all builtin variants
#define COMMON_BUILTIN_VARIANT_FUNCTIONS DLLLOCAL virtual int64 getFunctionality() const { return functionality; } \
   DLLLOCAL virtual AbstractFunctionSignature* getSignature() const { return const_cast<BuiltinSignature*>(&signature); } \
   DLLLOCAL virtual const QoreTypeInfo* parseGetReturnTypeInfo(int& err) const { return signature.getReturnTypeInfo(); }

class BuiltinFunctionVariantBase {
public:
    BuiltinSignature signature;
    // functionality bitmap for parse restrictions
    int64 functionality;

    DLLLOCAL BuiltinFunctionVariantBase(int64 flags = 0, int64 functionality = QDOM_DEFAULT,
            const QoreTypeInfo* returnTypeInfo = nullptr, const type_vec_t& typeList = type_vec_t(),
            const arg_vec_t& defaultArgList = arg_vec_t(), const name_vec_t& names = name_vec_t()) :
            signature(flags & QCF_USES_EXTRA_ARGS, returnTypeInfo, typeList, defaultArgList, names),
            functionality(functionality) {
        //printd(5, "BuiltinFunctionVariantBase::BuiltinFunctionVariantBase() this=%p flags=%lld (%lld) functionality=%lld\n", this, flags, n_flags, functionality);
    }
};

class BuiltinFunctionValueVariant : public AbstractQoreFunctionVariant, public BuiltinFunctionVariantBase {
protected:
    q_func_n_t func;

public:
    DLLLOCAL BuiltinFunctionValueVariant(q_func_n_t m, int64 n_flags, int64 n_functionality,
            const QoreTypeInfo* n_returnTypeInfo = 0, const type_vec_t& n_typeList = type_vec_t(),
            const arg_vec_t& n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t()) :
            AbstractQoreFunctionVariant(n_flags), BuiltinFunctionVariantBase(n_flags, n_functionality,
                    n_returnTypeInfo, n_typeList, n_defaultArgList, n_names), func(m) {
    }

    // the following defines the pure virtual functions that are common to all builtin variants
    COMMON_BUILTIN_VARIANT_FUNCTIONS

    DLLLOCAL virtual QoreValue evalFunction(ExceptionSink* xsink, CodeEvaluationHelper& ceh) const {
        CodeContextHelper cch(xsink, CT_BUILTIN, ceh.getName());
        return func(ceh.getArgs(), ceh.getRuntimeFlags(), xsink);
    }
};

//! External function variable
/** @since %Qore 0.9.5
*/
class BuiltinFunctionExternalVariant : public AbstractQoreFunctionVariant, public BuiltinFunctionVariantBase {
protected:
    q_external_func_t func;
    void* ptr;

public:
    DLLLOCAL BuiltinFunctionExternalVariant(void* ptr, q_external_func_t m, int64 n_flags, int64 n_functionality,
        const QoreTypeInfo* n_returnTypeInfo = nullptr, const type_vec_t& n_typeList = type_vec_t(),
        const arg_vec_t& n_defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t()) :
        AbstractQoreFunctionVariant(n_flags), BuiltinFunctionVariantBase(n_flags, n_functionality, n_returnTypeInfo,
            n_typeList, n_defaultArgList, n_names), func(m), ptr(ptr) {
    }

    // the following defines the pure virtual functions that are common to all builtin variants
    COMMON_BUILTIN_VARIANT_FUNCTIONS

    DLLLOCAL virtual QoreValue evalFunction(ExceptionSink* xsink, CodeEvaluationHelper& ceh) const {
        CodeContextHelper cch(xsink, CT_BUILTIN, ceh.getName());
        return func(ptr, ceh.getArgs(), ceh.getRuntimeFlags(), xsink);
    }
};

#endif // _QORE_BUILTIN_FUNCTION
