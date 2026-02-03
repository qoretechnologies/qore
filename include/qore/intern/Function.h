/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Function.h

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

#ifndef _QORE_FUNCTION_H

#define _QORE_FUNCTION_H

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "qore/intern/QoreListNodeEvalOptionalRefHolder.h"

class qore_class_private;
class QoreIRFunction;
struct QoreAOTContext;
using JitFunctionPtr = uint64_t (*)(ExceptionSink*);
using AotFunctionPtr = uint64_t (*)(QoreAOTContext*, ExceptionSink*);

// these data structures are all private to the library

class LocalVar;
class VarRefNode;
class BCAList;
class QoreOperatorNode;
class BarewordNode;
class QoreFunction;
class qore_class_private;
class qore_ns_private;

typedef std::vector<QoreParseTypeInfo*> ptype_vec_t;
typedef std::vector<LocalVar*> lvar_vec_t;

class AbstractFunctionSignature {
public:
    DLLLOCAL AbstractFunctionSignature(const QoreTypeInfo* n_returnTypeInfo = nullptr)
            : returnTypeInfo(n_returnTypeInfo) {
    }

    DLLLOCAL AbstractFunctionSignature(const QoreTypeInfo* n_returnTypeInfo, const type_vec_t& n_typeList,
            const arg_vec_t& n_defaultArgList, const name_vec_t& n_names, bool varargs)
            : returnTypeInfo(n_returnTypeInfo), typeList(n_typeList), defaultArgList(n_defaultArgList),
                names(n_names), varargs(varargs) {
    }

    DLLLOCAL virtual ~AbstractFunctionSignature() {
        // delete all default argument expressions
        for (arg_vec_t::iterator i = defaultArgList.begin(), e = defaultArgList.end(); i != e; ++i) {
            (*i).discard(nullptr);
        }
    }

    // called at parse time to include optional type resolution
    DLLLOCAL const QoreTypeInfo* parseGetReturnTypeInfo() const {
        int err = 0;
        return parseGetReturnTypeInfo(err);
    }

    // called at parse time to include optional type resolution
    DLLLOCAL virtual const QoreTypeInfo* parseGetReturnTypeInfo(int& err) const = 0;

    DLLLOCAL virtual const QoreParseTypeInfo* getParseParamTypeInfo(unsigned num) const = 0;

    DLLLOCAL const QoreTypeInfo* getReturnTypeInfo() const {
        return returnTypeInfo;
    }

    DLLLOCAL const arg_vec_t& getDefaultArgList() const {
        return defaultArgList;
    }

    DLLLOCAL const type_vec_t& getTypeList() const {
        return typeList;
    }

    DLLLOCAL const name_vec_t& getParamNames() const {
        return names;
    }

    DLLLOCAL QoreValue evalDefaultArg(unsigned i, ExceptionSink* xsink) const {
        assert(i < defaultArgList.size());
        return defaultArgList[i].eval(xsink);
    }

    DLLLOCAL const char* getSignatureText() const {
        return str.c_str();
    }

    DLLLOCAL virtual void addAbstractParameterSignature(std::string& str) const {
        for (unsigned i = 0; i < typeList.size(); ++i) {
            str.append(QoreTypeInfo::getPath(typeList[i]));
            const char* vname = getName(i);
            if (vname) {
                str.append(" ");
                str.append(vname);
            }
            if (hasDefaultArg(i)) {
                addDefaultArgument(str, defaultArgList[i]);
            }
            if (i != typeList.size() - 1) {
                str.append(", ");
            }
        }
        if (varargs) {
            if (!typeList.empty()) {
                str.append(", ");
            }
            str.append("...");
        }
    }

    DLLLOCAL unsigned numParams() const {
        return (unsigned)typeList.size();
    }

    // number of params with type information
    DLLLOCAL unsigned getParamTypes() const {
        return num_param_types;
    }

    DLLLOCAL unsigned getMinParamTypes() const {
        return min_param_types;
    }

    DLLLOCAL const QoreTypeInfo* getParamTypeInfo(unsigned num) const {
        return num >= typeList.size() ? 0 : typeList[num];
    }

    DLLLOCAL bool hasDefaultArg(unsigned i) const {
        return i >= defaultArgList.size() || !defaultArgList[i] ? false : true;
    }

    // adds a description of the given default argument to the signature string
    DLLLOCAL static void addDefaultArgument(std::string& str, QoreValue arg);

    DLLLOCAL const char* getName(unsigned i) const {
        return i < names.size() ? names[i].c_str() : 0;
    }

    DLLLOCAL bool compare(const AbstractFunctionSignature& sig, bool relaxed_match = false) const;

    DLLLOCAL bool hasVarargs() const {
        return varargs;
    }

protected:
    unsigned short num_param_types = 0,    // number of parameters that have type information
        min_param_types = 0;                // minimum number of parameters with type info (without default args)

    const QoreTypeInfo* returnTypeInfo;
    type_vec_t typeList;
    arg_vec_t defaultArgList;
    name_vec_t names;

    // parameter signature string
    std::string str;

    // varargs flag
    bool varargs = false;
};

// used to store return type info during parsing for user code
class RetTypeInfo {
    QoreParseTypeInfo* parseTypeInfo;
    const QoreTypeInfo* typeInfo;

public:
    DLLLOCAL RetTypeInfo(QoreParseTypeInfo* n_parseTypeInfo, const QoreTypeInfo* n_typeInfo)
            : parseTypeInfo(n_parseTypeInfo), typeInfo(n_typeInfo) {
    }
    DLLLOCAL ~RetTypeInfo() {
        delete parseTypeInfo;
    }
    DLLLOCAL const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }
    DLLLOCAL QoreParseTypeInfo* takeParseTypeInfo() {
        QoreParseTypeInfo* rv = parseTypeInfo;
        parseTypeInfo = 0;
        return rv;
    }
};

class UserSignature : public AbstractFunctionSignature {
protected:
    ptype_vec_t parseTypeList;
    QoreParseTypeInfo* parseReturnTypeInfo;

    const QoreProgramLocation* loc;

    DLLLOCAL int pushParam(BarewordNode* b, bool needs_types, bool bare_refs);
    DLLLOCAL int pushParam(QoreOperatorNode* t, bool needs_types);
    DLLLOCAL int pushParam(VarRefNode* v, QoreValue defArg, bool needs_types);

    DLLLOCAL void param_error() {
        parse_error(*loc, "parameter list contains non-variable reference expressions");
    }

public:
    lvar_vec_t lv;
    LocalVar* argvid;
    LocalVar* selfid;
    bool resolved;
    int err = 0;

    DLLLOCAL UserSignature(int n_first_line, int n_last_line, QoreValue params, RetTypeInfo* retTypeInfo, int64 po);

    DLLLOCAL virtual ~UserSignature() {
        for (ptype_vec_t::iterator i = parseTypeList.begin(), e = parseTypeList.end(); i != e; ++i)
            delete* i;
        delete parseReturnTypeInfo;
    }

    DLLLOCAL void setFirstParamType(const QoreTypeInfo* typeInfo) {
        assert(!typeList.empty());
        typeList[0] = typeInfo;
    }

    DLLLOCAL void setSelfId(LocalVar* n_selfid) {
        assert(!selfid);
        selfid = n_selfid;
    }

    DLLLOCAL LocalVar* getSelfId() const {
        return selfid;
    }

    DLLLOCAL virtual const QoreParseTypeInfo* getParseParamTypeInfo(unsigned num) const {
        return num < parseTypeList.size() ? parseTypeList[num] : nullptr;
    }

    // resolves all parse types to the final types
    DLLLOCAL virtual int resolve();

    DLLLOCAL virtual void addAbstractParameterSignature(std::string& str) const {
        if (resolved) {
            AbstractFunctionSignature::addAbstractParameterSignature(str);
            return;
        }

        for (unsigned i = 0; i < parseTypeList.size(); ++i) {
            if (!parseTypeList[i] && typeList.size() > i && typeList[i])
                str.append(QoreTypeInfo::getPath(typeList[i]));
            else
                str.append(QoreParseTypeInfo::getName(parseTypeList[i]));
            if (i != parseTypeList.size() - 1)
                str.append(",");
        }
    }

    // called at parse time to ensure types are resolved
    DLLLOCAL virtual const QoreTypeInfo* parseGetReturnTypeInfo(int& err) const {
        if (const_cast<UserSignature*>(this)->resolve() && !err) {
            err = -1;
        }
        return returnTypeInfo;
    }

    DLLLOCAL const QoreProgramLocation* getParseLocation() const {
        return loc;
    }

    DLLLOCAL bool hasReturnTypeInfo() const {
        return parseReturnTypeInfo || returnTypeInfo;
    }

    DLLLOCAL void parseInitPushLocalVars(const QoreTypeInfo* classTypeInfo);

    // returns the $argv reference count
    DLLLOCAL void parseInitPopLocalVars();
};

class AbstractQoreFunctionVariant;
class RuntimeConfig;

#define ARG_DEF   (1 << 0)
#define ARG_OTHER (1 << 1)

class CodeEvaluationHelper : public QoreStackLocation, public ProgramThreadCountContextHelper {
public:
    //! Creates the object for evaluating the given code (function, method, closure) with the given arguments
    /**
        @param func the code being called
        @param variant the variant to be called, if known, may be nullptr, in which case it will be resolved in the
        call

        @param is_copy set to true if this is a call to a copy method
        @param self the object of the call target; not (necessarily) the current contextual object where the call is
        made.  "self" is needed to handle executing default argument expressions for normal (non-static) methods in
        case they reference class members or methods

        saves current program location in case there's an exception
    */
    DLLLOCAL CodeEvaluationHelper(ExceptionSink* n_xsink, RuntimeConfig& n_rc, const QoreFunction* func,
            const AbstractQoreFunctionVariant*& variant, const char* n_name, const QoreListNode* args = nullptr,
            QoreObject* self = nullptr, const qore_class_private* n_qc = nullptr, qore_call_t n_ct = CT_UNUSED,
            bool is_copy = false, const qore_class_private* cctx = nullptr, QoreProgram* pgm_ctx = nullptr);

    //! Creates the object for evaluating the given code (function, method, closure) with the given arguments
    /**
        @param func the code being called
        @param variant the variant to be called, if known, may be nullptr, in which case it will be resolved in the
        call

        @param is_copy set to true if this is a call to a copy method
        @param self the object of the call target; not (necessarily) the current contextual object where the call is
        made.  "self" is needed to handle executing default argument expressions for normal (non-static) methods in
        case they reference class members or methods

        saves current program location in case there's an exception;
        performs destructive evaluation of "args"
    */
    DLLLOCAL CodeEvaluationHelper(ExceptionSink* n_xsink, RuntimeConfig& n_rc, const QoreFunction* func,
            const AbstractQoreFunctionVariant*& variant, const char* n_name, QoreListNode* args,
            QoreObject* self = nullptr, const qore_class_private* n_qc = nullptr, qore_call_t n_ct = CT_UNUSED,
            bool is_copy = false, const qore_class_private* cctx = nullptr, QoreProgram* pgm_ctx = nullptr);

    DLLLOCAL ~CodeEvaluationHelper();

    DLLLOCAL void setReturnTypeInfo(const QoreTypeInfo* n_returnTypeInfo) {
        returnTypeInfo = saveReturnTypeInfo(n_returnTypeInfo);
    }

    // once this is set, exception information will be raised in the destructor if an exception has been raised
    DLLLOCAL void setCallType(qore_call_t n_ct) {
        ct = n_ct;
    }

    DLLLOCAL int prepareDefaultArgs(ExceptionSink* xsink, const AbstractQoreFunctionVariant* variant,
        AbstractFunctionSignature* sig, bool is_copy, QoreObject* self, int arg_type);

    //! process default arguments for a function, method, or closure call
    /** @param xsink the exception sink to use
        @param func the code being called
        @param variant the variant to be called, if known, may be nullptr, in which case it will be resolved in the
        call
        @param is_copy set to true if this is a call to a copy method
        @param self the object of the call target; not (necessarily) the current contextual object where the call is
        made.  "self" is needed to handle executing default argument expressions for normal (non-static) methods in
        case they reference class members or methods

        @return 0 = OK, -1 Qore-language exception raised
    */
    DLLLOCAL int processDefaultArgs(ExceptionSink* xsink, const QoreFunction* func,
            const AbstractQoreFunctionVariant* variant, AbstractFunctionSignature* sig, bool is_copy,
            QoreObject* self);

    DLLLOCAL void setArgs(QoreListNode* n_args) {
        assert(!*tmp);
        tmp.assign(true, n_args);
    }

    DLLLOCAL QoreListNodeEvalOptionalRefHolder& getArgHolder() {
        return tmp;
    }

    DLLLOCAL const QoreListNode* getArgs() const {
        return *tmp;
    }

    // returns the QoreProgram object where the call originated
    DLLLOCAL QoreProgram* getSourceProgram() const {
        return pgm;
    }

    DLLLOCAL q_rt_flags_t getRuntimeFlags() const {
        return rc.getRuntimeFlags();
    }

    DLLLOCAL RuntimeConfig& getRuntimeConfig() const {
        return rc;
    }

    DLLLOCAL const qore_class_private* getClass() const {
        return qc;
    }

    //! returns the source location of the element
    DLLLOCAL virtual const QoreProgramLocation& getLocation() const {
        // return loc_builtin for Qore builtin calls
        return *loc;
    }

    //! returns the name of the function or method call (ex: "[Class::]method")
    DLLLOCAL virtual const std::string& getCallName() const {
        return callName;
    }

    DLLLOCAL virtual qore_call_t getCallType() const {
        return ct;
    }

    //! returns the QoreProgram container
    DLLLOCAL virtual QoreProgram* getProgram() const {
        return pgm;
    }

    DLLLOCAL virtual const AbstractStatement* getStatement() const {
        return stmt;
    }

    //! Returns the method / function name only without any class
    DLLLOCAL const char* getName() const {
        return name;
    }

protected:
    qore_call_t ct;
    const char* name;
    ExceptionSink* xsink;
    RuntimeConfig& rc;
    // method class
    const qore_class_private* qc;
    const QoreProgramLocation* loc;
    QoreListNodeEvalOptionalRefHolder tmp;
    const QoreTypeInfo* returnTypeInfo; // saved return type info
    QoreProgram* pgm = nullptr; // program used when evaluated (to find stacks for references)
    const AbstractStatement* stmt = nullptr; // the current statement for the call stack entry
    std::string callName;
    const QoreStackLocation* stack_loc = nullptr;
    const QoreProgramLocation* old_runtime_loc = nullptr;
    q_rt_flags_t old_rtflags = 0;
    bool restore_stack = false;
    bool restore_rtflags = false;

    DLLLOCAL void init(const QoreFunction* func, const AbstractQoreFunctionVariant*& variant, bool is_copy,
        const qore_class_private* cctx, QoreObject* self, QoreProgram* pgm_ctx);

    DLLLOCAL int findVariant(const QoreFunction* func, const AbstractQoreFunctionVariant*& variant,
        const qore_class_private* cctx);

    DLLLOCAL void setCallName(const QoreFunction* func);
};

class UserVariantBase;

// describes the details of the function variant
class AbstractQoreFunctionVariant : protected QoreReferenceCounter {
public:
    DLLLOCAL AbstractQoreFunctionVariant(int64 n_flags, bool n_is_user = false) : flags(n_flags), is_user(n_is_user) {
    }

    DLLLOCAL const QoreTypeInfo* parseGetReturnTypeInfo() const {
        int err = 0;
        return parseGetReturnTypeInfo(err);
    }

    DLLLOCAL virtual AbstractFunctionSignature* getSignature() const = 0;
    DLLLOCAL virtual const QoreTypeInfo* parseGetReturnTypeInfo(int& err) const = 0;

    DLLLOCAL const QoreTypeInfo* getReturnTypeInfo() const {
        return getSignature()->getReturnTypeInfo();
    }

    DLLLOCAL unsigned numParams() const {
        AbstractFunctionSignature* sig = getSignature();
        return sig ? sig->numParams() : 0;
    }

    DLLLOCAL qore_call_t getCallType() const {
        return is_user ? CT_USER : CT_BUILTIN;
    }

    DLLLOCAL int64 getParseOptions(int64 po) const;

    DLLLOCAL int64 getFlags() const {
        return flags;
    }

    DLLLOCAL virtual int64 getFunctionality() const = 0;

    // set flag to recheck params against committed variants in stage 2 parsing after type resolution (only for user
    // variants); should never be called with builtin variants
    DLLLOCAL virtual void setRecheck() {
        assert(false);
    }

    DLLLOCAL void parseResolveUserSignature();

    DLLLOCAL virtual UserVariantBase* getUserVariantBase() {
        return nullptr;
    }

    DLLLOCAL const UserVariantBase* getUserVariantBase() const {
        // avoid the virtual function call if possible
        return is_user ? const_cast<AbstractQoreFunctionVariant*>(this)->getUserVariantBase() : nullptr;
    }

    DLLLOCAL virtual QoreValue evalFunction(ExceptionSink* xsink, CodeEvaluationHelper& ceh) const {
        assert(false);
        return QoreValue();
    }

    DLLLOCAL virtual const QoreClass* getClass() const {
        return nullptr;
    }

    DLLLOCAL const char* className() const {
        const QoreClass* qc = getClass();
        return qc ? qc->getName() : nullptr;
    }

    DLLLOCAL std::string classPath() const {
        const QoreClass* qc = getClass();
        if (!qc) {
            return std::string();
        }
        return qc->getNamespacePath(true);
    }

    DLLLOCAL bool isSignatureIdentical(const AbstractFunctionSignature& sig, bool relaxed_match = false) const {
        //printd(5, "AbstractQoreFunctionVariant::isSignatureIdentical() this: %p '%s' == '%s': %d\n", this,
        //  getSignature()->getSignatureText(), sig.getSignatureText(), *(getSignature()) == sig);
        return getSignature()->compare(sig, relaxed_match);
    }

    // only returns a non-nullptr value for normal user method variants
    DLLLOCAL LocalVar* getSelfId() const;

    DLLLOCAL AbstractQoreFunctionVariant* ref() {
        ROreference();
        return this;
    }

    DLLLOCAL void deref() {
        if (ROdereference()) {
            delete this;
        }
    }

    DLLLOCAL bool isUser() const {
        return is_user;
    }

    DLLLOCAL bool hasBody() const;

    DLLLOCAL virtual bool isModulePublic() const {
        return false;
    }

    // the default implementation of this function does nothing
    DLLLOCAL virtual int parseInit(QoreFunction* f) {
        return 0;
    }

    // the default implementation of this function does nothing
    DLLLOCAL virtual void parseCommit() {
    }

    DLLLOCAL virtual bool hasVarargs() const {
        return flags & QCF_USES_EXTRA_ARGS;
    }

protected:
    // code flags
    int64 flags;
    bool is_user;

    DLLLOCAL virtual ~AbstractQoreFunctionVariant() {}

private:
    // not implemented
    DLLLOCAL AbstractQoreFunctionVariant(const AbstractQoreFunctionVariant& old) = delete;
    DLLLOCAL AbstractQoreFunctionVariant& operator=(AbstractQoreFunctionVariant& orig) = delete;
};

class VRMutex;
class UserVariantExecHelper;

// base implementation shared between all user variants
class UserVariantBase {
    friend class UserVariantExecHelper;

public:
    //! Tiered compilation execution tier
    enum ExecutionTier : uint8_t { TIER_AST = 0, TIER_IR = 1, TIER_JIT = 2 };

protected:
    UserSignature signature;
    StatementBlock* statements;
    // for "synchronized" functions
    VRMutex* gate;

public:
    QoreProgram* pgm;

protected:
    // flag to recheck params against committed after type resolution
    bool recheck;
    // flag to tell if variant has been initialized or not (still in pending list)
    bool init;

    // Tiered compilation state
    mutable std::atomic<uint64_t> exec_count{0};
    mutable std::atomic<ExecutionTier> current_tier{TIER_AST};
    mutable QoreIRFunction* cached_ir = nullptr;
    mutable JitFunctionPtr cached_jit_fn = nullptr;
    mutable AotFunctionPtr cached_aot_fn = nullptr;
    mutable QoreAOTContext* cached_aot_ctx = nullptr;
    mutable std::once_flag ir_lower_once;
    mutable std::once_flag jit_compile_once;
    mutable bool ir_lower_failed = false;
    mutable bool jit_compile_failed = false;
    bool is_closure = false;  //!< true for closure variants
    //! Deopt counter: incremented on JIT guard failure, triggers recompilation
    mutable std::atomic<uint32_t> deopt_count{0};
    //! Flag to allow a single recompilation attempt after deopt
    mutable std::once_flag jit_recompile_once;

    DLLLOCAL QoreValue evalIntern(const char* name, ReferenceHolder<QoreListNode>& argv, QoreObject* self,
            ExceptionSink* xsink) const;
    DLLLOCAL QoreValue eval(const char* name, CodeEvaluationHelper* ceh, QoreObject* self, ExceptionSink* xsink,
            const qore_class_private* qc = nullptr) const;
    DLLLOCAL int setupCall(CodeEvaluationHelper* ceh, ReferenceHolder<QoreListNode>& argv, ExceptionSink* xsink)
            const;

    //! Tiered compilation dispatch path
    DLLLOCAL QoreValue evalTiered(const char* name, ReferenceHolder<QoreListNode>& argv, QoreObject* self,
            ExceptionSink* xsink) const;
    //! Attempt to lower to IR; called via std::call_once
    DLLLOCAL void attemptIRLowering(const char* name) const;
    //! Attempt JIT compilation; called via std::call_once
    DLLLOCAL void attemptJITCompilation() const;
    //! Attempt JIT recompilation with updated type profiles after deopt
    DLLLOCAL void attemptJITRecompilation() const;

public:
    DLLLOCAL UserVariantBase(StatementBlock* b, int n_sig_first_line, int n_sig_last_line, QoreValue params,
            RetTypeInfo* rv, bool synced);
    DLLLOCAL virtual ~UserVariantBase();
    DLLLOCAL UserSignature* getUserSignature() const {
        return const_cast<UserSignature*>(&signature);
    }

    DLLLOCAL bool isSynchronized() const {
        return (bool)gate;
    }

    DLLLOCAL void setInit() {
        init = true;
    }

    DLLLOCAL bool getInit() const {
        return init;
    }

    DLLLOCAL bool hasBody() const {
        return (bool)statements;
    }
    DLLLOCAL StatementBlock* getStatementBlock() const {
        return statements;
    }

    DLLLOCAL int64 getParseOptions(int64 po) const;

    DLLLOCAL void parseInitPushLocalVars(const QoreTypeInfo* classTypeInfo);

    DLLLOCAL void parseInitPopLocalVars();

    DLLLOCAL void setSelfId(LocalVar* selfid) {
        signature.setSelfId(selfid);
    }

    DLLLOCAL void parseCommit();

    //! Register a pre-compiled AOT function pointer, promoting directly to JIT tier
    DLLLOCAL void registerPrecompiledFunction(JitFunctionPtr fn) {
        cached_jit_fn = fn;
        current_tier.store(TIER_JIT, std::memory_order_release);
    }

    //! Register a pre-compiled AOT function pointer with context, promoting directly to JIT tier
    DLLLOCAL void registerPrecompiledAOTFunction(AotFunctionPtr fn, QoreAOTContext* ctx) {
        cached_aot_fn = fn;
        cached_aot_ctx = ctx;
        current_tier.store(TIER_JIT, std::memory_order_release);
    }
};

// the following defines the pure virtual functions that are common to all user variants
#define COMMON_USER_VARIANT_FUNCTIONS DLLLOCAL virtual int64 getFunctionality() const {\
        const QoreClass* cls = getClass(); return QDOM_DEFAULT | (cls ? cls->getDomain() : 0);\
    }\
    using AbstractQoreFunctionVariant::getUserVariantBase; \
    DLLLOCAL virtual UserVariantBase* getUserVariantBase() { return static_cast<UserVariantBase*>(this); } \
    DLLLOCAL virtual AbstractFunctionSignature* getSignature() const { return const_cast<UserSignature*>(&signature); } \
    DLLLOCAL virtual const QoreTypeInfo* parseGetReturnTypeInfo(int& err) const { return signature.parseGetReturnTypeInfo(err); } \
    DLLLOCAL virtual void setRecheck() { recheck = true; } \
    DLLLOCAL virtual void parseCommit() { UserVariantBase::parseCommit(); } \
    DLLLOCAL virtual bool hasVarargs() const { if (signature.hasVarargs()) return true; return AbstractQoreFunctionVariant::hasVarargs(); }

// this class ensures that instantiated variables in user code are uninstantiated, even if an exception occurs
class UserVariantExecHelper : ProgramThreadCountContextHelper, ThreadFrameBoundaryHelper {
protected:
    const UserVariantBase* uvb;
    ReferenceHolder<QoreListNode> argv;
    ExceptionSink* xsink;

public:
    DLLLOCAL UserVariantExecHelper(const UserVariantBase* n_uvb, CodeEvaluationHelper* ceh, ExceptionSink* n_xsink) :
            ProgramThreadCountContextHelper(n_xsink, n_uvb->pgm, true),
            ThreadFrameBoundaryHelper(!*n_xsink),
            uvb(n_uvb), argv(n_xsink), xsink(n_xsink) {
        assert(xsink);
        if (*xsink || uvb->setupCall(ceh, argv, xsink))
            uvb = nullptr;
    }

    DLLLOCAL ~UserVariantExecHelper();

    DLLLOCAL operator bool() const {
        return uvb;
    }

    DLLLOCAL ReferenceHolder<QoreListNode>& getArgv() {
        return argv;
    }
};

class UserFunctionVariant : public AbstractQoreFunctionVariant, public UserVariantBase {
protected:
    bool mod_pub; // is public in module

    DLLLOCAL virtual ~UserFunctionVariant() {
    }

public:
    DLLLOCAL UserFunctionVariant(StatementBlock* b, int n_sig_first_line, int n_sig_last_line, QoreValue params,
            RetTypeInfo* rv, bool synced, int64 n_flags = QCF_NO_FLAGS) :
            AbstractQoreFunctionVariant(n_flags, true),
            UserVariantBase(b, n_sig_first_line, n_sig_last_line, params, rv, synced), mod_pub(false) {
        if (signature.hasVarargs()) {
            flags |= QCF_USES_EXTRA_ARGS;
        }
    }

    // the following defines the virtual functions that are common to all user variants
    COMMON_USER_VARIANT_FUNCTIONS

    DLLLOCAL virtual QoreValue evalFunction(ExceptionSink* xsink, CodeEvaluationHelper& ceh) const {
        return eval(ceh.getName(), &ceh, nullptr, xsink);
    }

    DLLLOCAL virtual int parseInit(QoreFunction* f);

    DLLLOCAL virtual bool isModulePublic() const {
        return mod_pub;
    }

    DLLLOCAL void setModulePublic() {
        assert(!mod_pub);
        mod_pub = true;
    }
};

#define UFV(f) (reinterpret_cast<UserFunctionVariant*>(f))
#define UFV_const(f) (reinterpret_cast<const UserFunctionVariant*>(f))

// type for lists of function variants
// this type will be read at runtime and could be appended to simultaneously at parse time (under a lock)
typedef safe_dslist<AbstractQoreFunctionVariant*> vlist_t;

class VList : public vlist_t {
public:
    DLLLOCAL ~VList() {
        del();
    }
    DLLLOCAL void del() {
        // dereference all variants
        for (vlist_t::iterator i = begin(), e = end(); i != e; ++i)
            (*i)->deref();
        clear();
    }
};

// inheritance noce
struct INode {
    QoreFunction* func;
    ClassAccess access;

    DLLLOCAL INode(QoreFunction* f, ClassAccess a) : func(f), access(a) {
    }
};

// inheritance list type
typedef std::vector<INode> ilist_t;

struct IList : public ilist_t {
    DLLLOCAL QoreFunction* getFunction(const qore_class_private* class_ctx, const qore_class_private*& last_class,
        const_iterator aqfi, bool& internal_access, bool& stop) const;
};

class QoreFunction : protected QoreReferenceCounter {
friend class QoreFunctionIterator;
friend class qore_external_function_iterator_private;
public:
    DLLLOCAL QoreFunction(const char* n_name) : name(n_name),
        same_return_type(true),
        nn_same_return_type(true),
        parse_rt_done(true),
        parse_init_done(true),
        parse_init_in_progress(false),
        has_user(false),
        has_builtin(false),
        has_pub(false),
        inject(false),
        check_parse(false),
        has_priv(false),
        all_priv(true) {
        ilist.push_back(INode(this, Public));
        //printd(5, "QoreFunction::QoreFunction() this: %p %s\n", this, name.c_str());
        const char* mod_name = get_module_context_name();
        if (mod_name) {
            from_module = mod_name;
        }
    }

    // copy constructor (used by method functions when copied)
    DLLLOCAL QoreFunction(const QoreFunction& old, int64 po = 0, bool copy_all = false, bool n_inject = false)
        : name(old.name),
            unique_functionality(old.unique_functionality),
            unique_flags(old.unique_flags),
            nn_unique_functionality(old.nn_unique_functionality),
            nn_unique_flags(old.nn_unique_flags),
            nn_count(old.nn_count),
            same_return_type(old.same_return_type),
            nn_same_return_type(old.nn_same_return_type),
            parse_rt_done(true),
            parse_init_done(true),
            parse_init_in_progress(false),
            has_user(old.has_user),
            has_builtin(old.has_builtin),
            has_pub(false),
            inject(n_inject),
            check_parse(false),
            has_priv(old.has_priv),
            all_priv(old.all_priv),
            nn_uniqueReturnType(old.nn_uniqueReturnType),
            from_module(old.from_module) {
        bool no_user = po & PO_NO_INHERIT_USER_FUNC_VARIANTS;
        bool no_builtin = po & PO_NO_SYSTEM_FUNC_VARIANTS;

        // copy variants by reference
        for (vlist_t::const_iterator i = old.vlist.begin(), e = old.vlist.end(); i != e; ++i) {
            if (!copy_all) {
                if ((*i)->isUser()) {
                    if (no_user || !(*i)->isModulePublic()) {
                        continue;
                    }
                } else {
                    if (no_builtin) {
                        continue;
                    }
                }
            }

            vlist.push_back((*i)->ref());
        }

        if (no_user && has_user) {
            has_user = false;
        }
        if (no_builtin && has_builtin) {
            has_builtin = false;
        }

        // make sure the new variant list is not empty if the parent also wasn't
        assert(old.vlist.empty() || !vlist.empty());

        assert(!old.ilist.empty());
        assert(old.ilist.front().func == &old);

        // resolve initial ilist entry to this function
        ilist.push_back(INode(this, Public));

        // the rest of ilist is copied in method base class
        // do not copy pending variants
        //printd(5, "QoreFunction::QoreFunction() this: %p %s\n", this, name.c_str());
    }

#if 0
    // copy constructor when importing public user variants from user modules into Program objects
    DLLLOCAL QoreFunction(bool ignore, const QoreFunction& old, qore_ns_private* nns)
        : name(old.name), ns(nns), same_return_type(old.same_return_type),
            unique_functionality(old.unique_functionality),
            unique_flags(old.unique_flags),
            nn_same_return_type(old.nn_same_return_type),
            nn_unique_functionality(old.nn_unique_functionality),
            nn_unique_flags(old.nn_unique_flags),
            nn_count(old.nn_count),
            parse_rt_done(true), parse_init_done(true),
            has_user(true), has_builtin(false), has_mod_pub(false /*old.has_mod_pub*/), inject(false),
            nn_uniqueReturnType(old.nn_uniqueReturnType) {
        assert(!ignore);
        assert(old.has_mod_pub);

        // copy variants by reference
        for (vlist_t::const_iterator i = old.vlist.begin(), e = old.vlist.end(); i != e; ++i) {
            if (!(*i)->isModulePublic())
                continue;
            vlist.push_back((*i)->ref());
        }

        // make sure the new variant list is not empty if the parent also wasn't
        assert(old.vlist.empty() || !vlist.empty());

        assert(!old.ilist.empty());
        assert(old.ilist.front().func == &old);

        // resolve initial ilist entry to this function
        ilist.push_back(INode(this, Public));

        // the rest of ilist is copied in method base class
        // do not copy pending variants
        //printd(5, "QoreFunction::QoreFunction() this: %p %s\n", this, name.c_str());
    }
#endif

    // convenience function for returning the first variant in the list
    DLLLOCAL const AbstractQoreFunctionVariant* first() const {
        assert(!vlist.empty());
        return *(vlist.begin());
    }

    // convenience function for returning the first variant in the list
    DLLLOCAL AbstractQoreFunctionVariant* first() {
        assert(!vlist.empty());
        return *(vlist.begin());
    }

    DLLLOCAL unsigned numVariants() const {
        return vlist.size();
    }

    DLLLOCAL QoreListNode* runtimeGetCallVariants() const;

    // returns 0 for OK, -1 for error
    DLLLOCAL int parseCheckDuplicateSignatureCommitted(UserSignature* sig);

    DLLLOCAL const char* getName() const {
        return name.c_str();
    }

    DLLLOCAL virtual const QoreClass* getClass() const {
        return nullptr;
    }

    DLLLOCAL void ref() {
        ROreference();
    }

    DLLLOCAL void deref() {
        if (ROdereference())
            delete this;
    }

    DLLLOCAL const char* className() const {
        const QoreClass* qc = getClass();
        return qc ? qc->getName() : nullptr;
    }

    DLLLOCAL std::string classPath() const {
        const QoreClass* qc = getClass();
        if (!qc) {
            return std::string();
        }
        return qc->getNamespacePath(true);
    }

    DLLLOCAL void addAncestor(QoreFunction* ancestor, ClassAccess access) {
        ilist.push_back(INode(ancestor, access));
    }

    DLLLOCAL void addNewAncestor(QoreFunction* ancestor, ClassAccess access) {
        for (ilist_t::iterator i = ilist.begin(), e = ilist.end(); i != e; ++i)
            if ((*i).func == ancestor)
                return;
        ilist.push_back(INode(ancestor, access));
    }

    // resolves all types in signatures and return types in pending variants; called during the "parseInit" phase
    DLLLOCAL void resolvePendingSignatures();

    DLLLOCAL AbstractFunctionSignature* getUniqueSignature() const {
        return vlist.singular() ? first()->getSignature() : 0;
    }

    DLLLOCAL AbstractFunctionSignature* parseGetUniqueSignature() const;

    DLLLOCAL int64 parseGetUniqueFunctionality() const {
        if (parse_get_parse_options() & PO_REQUIRE_TYPES)
            return nn_unique_functionality;
        return unique_functionality;
    }

    DLLLOCAL int64 parseGetUniqueFlags() const {
        if (parse_get_parse_options() & PO_REQUIRE_TYPES)
            return nn_unique_flags;
        return unique_flags;
    }

    // object takes ownership of variant or deletes it if it can't be added
    DLLLOCAL int addPendingVariant(AbstractQoreFunctionVariant* variant);

    DLLLOCAL void addBuiltinVariant(AbstractQoreFunctionVariant* variant);

    DLLLOCAL int parseInit(qore_ns_private* ns);
    DLLLOCAL void parseCommit();
    DLLLOCAL void parseRollback();

    DLLLOCAL const QoreTypeInfo* getUniqueReturnTypeInfo() const {
        if (runtime_get_parse_options() & PO_REQUIRE_TYPES)
            return nn_uniqueReturnType;

        return same_return_type && !vlist.empty() ? first()->getReturnTypeInfo() : 0;
    }

    DLLLOCAL const QoreTypeInfo* parseGetUniqueReturnTypeInfo() {
        parseCheckReturnType();

        //printd(5, "QoreFunction::parseGetUniqueReturnTypeInfo() this: %p '%s' rt: %d srt: %d vs: %d\n", this, name.c_str(), parse_get_parse_options() & PO_REQUIRE_TYPES, same_return_type, vlist.size());

        if (!same_return_type)
            return nullptr;

        if (parse_get_parse_options() & PO_REQUIRE_TYPES) {
            if (!nn_same_return_type)
                return nullptr;

            return nn_count ? nn_uniqueReturnType : (!vlist.empty() ? first()->getReturnTypeInfo() : nullptr);
        }

        if (!vlist.empty())
            return first()->getReturnTypeInfo();

        return nullptr;
    }

    // if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified at run time
    DLLLOCAL virtual QoreValue evalFunction(const AbstractQoreFunctionVariant* variant, const QoreListNode* args,
            QoreProgram* pgm, RuntimeConfig& rc, ExceptionSink* xsink) const;

    // if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL then it is identified at run time
    // this function will use destructive evaluation of "args"
    DLLLOCAL virtual QoreValue evalFunctionTmpArgs(const AbstractQoreFunctionVariant* variant, QoreListNode* args,
            QoreProgram* pgm, RuntimeConfig& rc, ExceptionSink* xsink) const;

    // finds a variant and checks variant capabilities against current program parse options and executes the variant
    DLLLOCAL QoreValue evalDynamic(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) const;

    // find variant at parse time, throw parse exception if no variant can be matched
    // class_ctx is only for use in a class hierarchy and is only set if there is a current class context and it's
    // reachable
    DLLLOCAL const AbstractQoreFunctionVariant* parseFindVariant(const QoreProgramLocation* loc,
            const type_vec_t& argTypeInfo, const qore_class_private* class_ctx, int& err) const;

    // returns true if there are no uncommitted parse variants in the function
    DLLLOCAL bool pendingEmpty() const {
        return vlist.empty() || !check_parse;
    }

    // returns true if there are no committed parse variants in the function
    DLLLOCAL bool committedEmpty() const {
        return vlist.empty() || check_parse;
    }

    DLLLOCAL bool existsVariant(const type_vec_t& paramTypeInfo) const;

    // find variant at runtime
    // class_ctx is only for use in a class hierarchy and is only set if there is a current class context and it's reachable from the object being executed
    DLLLOCAL const AbstractQoreFunctionVariant* runtimeFindVariant(ExceptionSink* xsink, const QoreListNode* args, bool only_user, const qore_class_private* class_ctx) const;

    // finds the best match with the given arg types
    DLLLOCAL const AbstractQoreFunctionVariant* runtimeFindVariant(ExceptionSink* xsink, const type_vec_t& args, const qore_class_private* class_ctx) const;
    // finds only an exact match with the given arg types
    DLLLOCAL const AbstractQoreFunctionVariant* runtimeFindExactVariant(ExceptionSink* xsink, const type_vec_t& args, const qore_class_private* class_ctx) const;

    DLLLOCAL void parseAssimilate(QoreFunction& other) {
        while (!other.vlist.empty()) {
            addPendingVariant(*(other.vlist.begin()));
            other.vlist.pop_front();
        }
    }

    DLLLOCAL bool hasUser() const {
        return has_user;
    }

    DLLLOCAL bool hasBuiltin() const {
        return has_builtin;
    }

    DLLLOCAL bool hasPublic() const {
        return has_pub;
    }

    DLLLOCAL bool hasUserPublic() const {
        return has_pub && has_user;
    }

    DLLLOCAL bool injected() const {
        return inject;
    }

    DLLLOCAL bool hasPrivate() const {
        return has_priv;
    }

    DLLLOCAL bool allPrivate() const {
        return all_priv;
    }

    DLLLOCAL const std::string& getNameStr() const {
        return name;
    }

    DLLLOCAL const char* getModuleName() const {
        return from_module.empty() ? nullptr : from_module.c_str();
    }

protected:
    std::string name;

    // list of function variants
    VList vlist;

    // list of inherited methods for variant matching; the first pointer is always a pointer to "this"
    IList ilist;

    int64 unique_functionality = QDOM_DEFAULT;
    int64 unique_flags = QCF_NO_FLAGS;

    // same as above but for variants without QCF_RUNTIME_NOOP
    int64 nn_unique_functionality = QDOM_DEFAULT;
    int64 nn_unique_flags = QCF_NO_FLAGS;
    int nn_count = 0;

    // if true means all variants have the same return value
    bool same_return_type : 1;
    bool nn_same_return_type : 1;
    bool parse_rt_done : 1;
    bool parse_init_done : 1;
    bool parse_init_in_progress : 1;
    bool has_user : 1;                   // has at least 1 committed user variant
    bool has_builtin : 1;                // has at least 1 committed builtin variant
    bool has_pub : 1;                    // has at least 1 committed user variant with public visibility
    bool inject : 1;
    bool check_parse : 1;
    bool has_priv : 1;                   // has at least 1 private variant
    bool all_priv : 1;                   // all variants are private

    const QoreTypeInfo* nn_uniqueReturnType = nullptr;

    std::string from_module;

    DLLLOCAL int parseCheckReturnType() {
        if (parse_rt_done)
            return 0;

        parse_rt_done = true;

        if (!same_return_type)
            return 0;


        int err = 0;
        for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
            if (reinterpret_cast<UserSignature*>((*i)->getUserVariantBase()->getUserSignature())->resolve() && !err) {
                err = -1;
            }
            const QoreTypeInfo* rti = (*i)->getReturnTypeInfo();

            if (i == vlist.begin()) {
                continue;
            }

            if (!QoreTypeInfo::isOutputIdentical(rti, first()->getReturnTypeInfo())) {
                same_return_type = false;
                break;
            }
        }
        //printd(5, "QoreFunction::parseCheckReturnType() '%s' srt: %d\n", name.c_str(), same_return_type);
        return err;
    }

    // returns QTI_NOT_EQUAL, QTI_AMBIGUOUS, or QTI_IDENT
    DLLLOCAL static int parseCompareResolvedSignature(const VList& vlist, const AbstractFunctionSignature* sig,
            const AbstractFunctionSignature*& vs);

    // returns 0 for OK (not a duplicate), -1 for error (duplicate) - parse exceptions are raised if a duplicate is found
    DLLLOCAL int parseCheckDuplicateSignature(AbstractQoreFunctionVariant* variant);

    // FIXME: does not check unparsed types properly
    DLLLOCAL void addVariant(AbstractQoreFunctionVariant* variant) {
        const QoreTypeInfo* rti = variant->getReturnTypeInfo();
        if (same_return_type && !vlist.empty() && !QoreTypeInfo::isOutputIdentical(rti, first()->getReturnTypeInfo()))
            same_return_type = false;

        int64 vf = variant->getFunctionality();
        int64 vflags = variant->getFlags();

        bool rtn = (bool)(vflags & QCF_RUNTIME_NOOP);

        if (vlist.empty()) {
            unique_functionality = vf;
            unique_flags = vflags;
        } else {
            unique_functionality &= vf;
            unique_flags &= vflags;
        }

        if (!rtn) {
            if (!nn_count) {
                nn_unique_functionality = vf;
                nn_unique_flags = vflags;
                nn_uniqueReturnType = rti;
                ++nn_count;
            } else {
                nn_unique_functionality &= vf;
                nn_unique_flags &= vflags;
                if (nn_uniqueReturnType && !QoreTypeInfo::isOutputIdentical(rti, nn_uniqueReturnType))
                nn_uniqueReturnType = 0;
                ++nn_count;
            }
        }

        vlist.push_back(variant);
    }

    DLLLOCAL virtual ~QoreFunction() {
        //printd(5, "QoreFunction::~QoreFunction() this: %p %s\n", this, name.c_str());
    }

    DLLLOCAL const AbstractQoreFunctionVariant* checkVariant(ExceptionSink* xsink, const type_vec_t& args,
        const qore_class_private* class_ctx, const QoreFunction* aqf, const qore_class_private* last_class,
        bool internal_access, int64 ppo, const AbstractQoreFunctionVariant* variant) const;

    DLLLOCAL const AbstractQoreFunctionVariant* checkVariantDomain(ExceptionSink* xsink, int64 ppo,
        const AbstractQoreFunctionVariant* variant) const;
};

class QoreFunctionIterator {
public:
    DLLLOCAL QoreFunctionIterator(const QoreFunction& f) : f(f) {
        i = f.vlist.end();
    }

    DLLLOCAL bool next() {
        if (i == f.vlist.end()) {
            i = f.vlist.begin();
        }
        else {
            ++i;
        }

        return i != f.vlist.end();
    }

    DLLLOCAL const AbstractQoreFunctionVariant* getVariant() const {
        return *i;
    }

private:
    const QoreFunction& f;
    VList::const_iterator i;
};

class MethodVariantBase;
class MethodFunctionBase;
#define METHFB(f) (reinterpret_cast<MethodFunctionBase*>(f))
#define METHFB_const(f) (reinterpret_cast<const MethodFunctionBase*>(f))

class MethodFunctionBase : public QoreFunction {
friend struct AbstractMethod;
protected:
    const QoreClass* qc;

    // for concrete variants for local abstract variants inherited from base classes
    VList pending_save;

    // pointer to copy, only valid during copy
    mutable MethodFunctionBase* new_copy = nullptr;

    bool is_static,
        has_final = false,
        is_abstract = true,
        has_private_internal_variants = false;

    ClassAccess access;

    DLLLOCAL int checkFinalVariant(const MethodFunctionBase* m, const MethodVariantBase* v) const;

    DLLLOCAL void replaceAbstractVariantIntern(MethodVariantBase* variant);

public:
    DLLLOCAL MethodFunctionBase(const char* nme, const QoreClass* n_qc, bool n_is_static) : QoreFunction(nme),
        qc(n_qc), is_static(n_is_static), has_final(false), access(Internal) {
    }

    // copy constructor, only copies committed variants
    DLLLOCAL MethodFunctionBase(const MethodFunctionBase& old, const QoreClass* n_qc)
        : QoreFunction(old, 0, true),
            qc(n_qc),
            is_static(old.is_static),
            has_final(old.has_final),
            is_abstract(old.is_abstract),
            has_private_internal_variants(old.has_private_internal_variants),
            access(old.access) {
        //printd(5, "MethodFunctionBase() copying old=%p -> new=%p %p %s::%s() %p %s::%s()\n",& old, this, old.qc, old.qc->getName(), old.getName(), qc, qc->getName(), old.getName());

        // set a pointer to the new function
        old.new_copy = this;

        // copy ilist, will be adjusted for new class pointers after all classes have been copied
        ilist.reserve(old.ilist.size());
        ilist_t::const_iterator i = old.ilist.begin(), e = old.ilist.end();
        ++i;
        for (; i != e; ++i) {
            ilist.push_back(*i);
        }
    }

    DLLLOCAL void resolveCopy() {
        ilist_t::iterator i = ilist.begin(), e = ilist.end();
        ++i;
        for (; i != e; ++i) {
            MethodFunctionBase* mfb = METHFB((*i).func);
#ifdef DEBUG
            if (!mfb->new_copy)
                printd(0, "error resolving %p %s::%s() base method %p %s::%s() nas no new method pointer\n", qc, qc->getName(), getName(), mfb->qc, mfb->qc->getName(), getName());
            assert(mfb->new_copy);
            //printd(5, "resolving %p %s::%s() base method %p %s::%s() from %p -> %p\n", qc, qc->getName(), getName(), mfb->qc, mfb->qc->getName(), getName(), mfb, mfb->new_copy);
#endif
            (*i).func = mfb->new_copy;
        }
    }

    DLLLOCAL bool parseHasAmbiguousSignature(const MethodFunctionBase& other, bool relaxed_match) const;

    DLLLOCAL int parseInit();
    DLLLOCAL void parseCommit();
    DLLLOCAL void parseRollback();

    // returns -1 for error, 0 = OK
    DLLLOCAL int parseAddUserMethodVariant(MethodVariantBase* variant);

    // maintains access flag and commits the builtin variant
    DLLLOCAL void addBuiltinMethodVariant(MethodVariantBase* variant);

    // maintains access flag and commits user variants
    DLLLOCAL void parseCommitMethod(QoreString& csig, const char* mod);

    DLLLOCAL void parseCommitMethod();
    // processes method signatures
    DLLLOCAL void parseSignatures(QoreString& csig, const char* mod) const;

    // if an identical signature is found to the passed variant, then it is removed from the abstract list
    DLLLOCAL MethodVariantBase* parseHasVariantWithSignature(MethodVariantBase* v, bool relaxed_match = false) const;

    // runtime version - assumes signatures are already resolved
    DLLLOCAL MethodVariantBase* runtimeHasVariantWithSignature(MethodVariantBase* v, bool relaxed_match = false) const;

    DLLLOCAL void replaceAbstractVariant(MethodVariantBase* variant);

    DLLLOCAL void parseRollbackMethod();

    DLLLOCAL bool isUniquelyPrivate() const {
        return access > Public;
    }

    DLLLOCAL bool isAbstract() const {
        return is_abstract;
    }

    DLLLOCAL ClassAccess getAccess() const {
        return access;
    }

    DLLLOCAL virtual const QoreClass* getClass() const {
        return qc;
    }

    DLLLOCAL const char* getClassName() const {
        return qc->getName();
    }

    DLLLOCAL std::string classPath() const {
        if (!qc) {
            return std::string();
        }
        return qc->getNamespacePath(true);
    }

    DLLLOCAL bool isStatic() const {
        return is_static;
    }

    DLLLOCAL bool hasPrivateInternalVariants() const {
        return has_private_internal_variants;
    }

    DLLLOCAL int checkFinal() const;

    // virtual copy constructor
    DLLLOCAL virtual MethodFunctionBase* copy(const QoreClass* n_qc) const = 0;
};

class UserParamListLocalVarHelper {
protected:
    UserVariantBase* uvb;

public:
    DLLLOCAL UserParamListLocalVarHelper(UserVariantBase* n_uvb, const QoreTypeInfo* classTypeInfo = nullptr)
            : uvb(n_uvb) {
        uvb->parseInitPushLocalVars(classTypeInfo);
    }

    DLLLOCAL ~UserParamListLocalVarHelper() {
        uvb->parseInitPopLocalVars();
    }
};

class UserClosureVariant : public UserFunctionVariant {
protected:
public:
    DLLLOCAL UserClosureVariant(StatementBlock* b, int n_sig_first_line, int n_sig_last_line, QoreValue params, RetTypeInfo* rv, bool synced = false, int64 n_flags = QCF_NO_FLAGS) : UserFunctionVariant(b, n_sig_first_line, n_sig_last_line, params, rv, synced, n_flags) {
        is_closure = true;
    }

    DLLLOCAL virtual int parseInit(QoreFunction* f);

    DLLLOCAL QoreValue evalClosure(CodeEvaluationHelper& ceh, QoreObject* self, ExceptionSink* xsink) const {
        return eval("<anonymous closure>", &ceh, self, xsink);
    }
};

#define UCLOV(f) (reinterpret_cast<UserClosureVariant*>(f))
#define UCLOV_const(f) (reinterpret_cast<const UserClosureVariant*>(f))

class UserClosureFunction : public QoreFunction {
protected:
    LVarSet varlist;  // closure local variable environment
    const QoreTypeInfo* classTypeInfo;

public:
    DLLLOCAL UserClosureFunction(StatementBlock* b, int n_sig_first_line, int n_sig_last_line, QoreValue params, RetTypeInfo* rv, bool synced = false, int64 n_flags = QCF_NO_FLAGS) : QoreFunction("<anonymous closure>"), classTypeInfo(0) {
        addPendingVariant(new UserClosureVariant(b, n_sig_first_line, n_sig_last_line, params, rv, synced, n_flags));
    }

    DLLLOCAL QoreValue evalClosure(const QoreClosureBase& closure_base, QoreProgram* pgm, const QoreListNode* args, QoreObject* self, const qore_class_private* class_ctx, ExceptionSink* xsink) const;

    DLLLOCAL void setClassType(const QoreTypeInfo* cti) {
        classTypeInfo = cti;
    }

    DLLLOCAL const QoreTypeInfo* getClassType() const {
        return classTypeInfo;
    }

    DLLLOCAL LVarSet* getVList() {
        return &varlist;
    }

    // returns true if at least one variable in the set of closure-bound local variables could contain an object or a closure (also through a container)
    DLLLOCAL bool needsScan() const {
        return varlist.needsScan();
    }
};

#endif // _QORE_FUNCTION_H
