/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    CallReferenceNode.h

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

#ifndef _QORE_INTERN_FUNCTIONREFERENCENODE_H

#define _QORE_INTERN_FUNCTIONREFERENCENODE_H

#include <memory>
#include <string>
#include <vector>

class QoreParseTypeInfo;
struct QoreTypeParamInstantiation;

class LocalFunctionCallReferenceNode;
class FunctionCallReferenceNode;
class FunctionEntry;
class RuntimeConfig;

struct CallReferenceNodeLocation {
    const QoreProgramLocation* loc;

    DLLLOCAL CallReferenceNodeLocation(const QoreProgramLocation* loc) : loc(loc) {
    }
};

class AbstractCallReferenceNodeIntern : public AbstractCallReferenceNode, public CallReferenceNodeLocation {
public:
    DLLLOCAL AbstractCallReferenceNodeIntern(const QoreProgramLocation* loc, bool n_needs_eval) :
        AbstractCallReferenceNode(n_needs_eval), CallReferenceNodeLocation(loc) {
    }
};

//! calls a call reference or closure value with named arguments bound by parameter name at run time (M4 phase 3)
/** a value that cannot bind names (a call reference implemented outside of the library) raises
    NAMED-CALL-NOT-SUPPORTED

    @param args the arguments in source order, not evaluated
    @param arg_names the name of each argument ("" for a positional argument)
    @param policy the semantic policy of a legacy AST call (default for any other call)
*/
DLLLOCAL QoreValue qore_call_ref_invoke_named(const ResolvedCallReferenceNode* r, const QoreListNode* args,
        const name_vec_t& arg_names, LexicalSemanticPolicy policy, ExceptionSink* xsink);

class QoreListNodeEvalOptionalRefHolder;

class ResolvedCallReferenceNodeIntern : public ResolvedCallReferenceNode, public CallReferenceNodeLocation {
public:
    QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const override = 0;

    QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const override {
        return dispatchValue(args, xsink);
    }

    QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
            const LexicalSemanticPolicy& policy) const override {
        return dispatchValue(args, xsink);
    }

    //! calls the code with arguments in source order with the names in \a arg_names ("" for a positional
    //! argument), which are bound by parameter name when the call is made (M4 phase 3)
    /** a call through the reference binds the names as a named call of the referenced function or method does; the
        base implementation raises NAMED-CALL-NOT-SUPPORTED

        @param policy the semantic policy of a legacy AST call (default for any other call)
    */
    DLLLOCAL virtual QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const;

    DLLLOCAL ResolvedCallReferenceNodeIntern(const QoreProgramLocation* loc, bool n_needs_eval = false) :
        ResolvedCallReferenceNode(n_needs_eval), CallReferenceNodeLocation(loc) {
    }

    //! calls the code with a temporary argument list that the caller owns
    /** A reference whose call can take the values of the list over for its parameters, as the AST interpreter's call
        moves its evaluated arguments into the parameters (issue #5882), does so; the others call execValue()
    */
    DLLLOCAL virtual QoreValue execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const {
        return execValue(args, xsink);
    }

    //! returns the effective parse options of the context that created the reference value, or nullptr for a
    //! reference that its own code calls in place (a reference in the parse tree)
    /** a call through the reference runs with the permissions of the context that created it: the functional domains
        declared on a user function or method in Qore source are checked against these options
    */
    DLLLOCAL const QoreParseOptions* getCreatorParseOptions() const {
        return has_creator_po ? &creator_po : nullptr;
    }

    //! returns the effective parse options of the code that created the reference if it is the code of a Program that
    //! is not a module's, or nullptr (qore#7056)
    DLLLOCAL const QoreParseOptions* getNonModuleCreatorParseOptions() const {
        return has_creator_po && creator_non_module ? &creator_po : nullptr;
    }

    //! returns the Program whose code a call of the reference runs, or nullptr for builtin code (qore#6520)
    /** Called by captureCreatorParseOptions() in the constructor of the run-time reference classes, which give their
        target without the generic lookup (qore_call_reference_code_program())
    */
    DLLLOCAL virtual const QoreProgram* getOriginCodeProgram() const;

protected:
    //! records the effective parse options of the context creating the reference value
    DLLLOCAL void captureCreatorParseOptions();

private:
    QoreParseOptions creator_po;
    bool has_creator_po = false;
    //! true if the code that created the reference is the code of a Program that is not a module's, whose options
    //! are \c creator_po (see qore_callable_origin_restrictions_po(), qore#7056)
    bool creator_non_module = false;
};

class AbstractUnresolvedCallReferenceNode : public AbstractCallReferenceNodeIntern {
public:
    DLLLOCAL AbstractUnresolvedCallReferenceNode(const QoreProgramLocation* loc, bool n_needs_eval) : AbstractCallReferenceNodeIntern(loc, n_needs_eval) {
    }
};

//! resolves the parsed explicit type arguments of a call reference (\\f<T>()) to types, taking over the parse types
/** @return 0 on success, -1 if a type cannot be resolved (a parse error was raised)
*/
DLLLOCAL int qore_resolve_call_reference_type_args(std::vector<QoreParseTypeInfo*>& parse_type_args,
    const QoreProgramLocation* loc, std::vector<const QoreTypeInfo*>& type_args);

//! an unresolved call reference, only present temporarily in the parse tree
class UnresolvedProgramCallReferenceNode : public AbstractUnresolvedCallReferenceNode {
public:
    char* str;
    //! the explicit type arguments of \f<T>(), as parsed; they belong to the resolved reference (see
    //! LocalFunctionCallReferenceNode::setExplicitTypeArgs())
    std::vector<QoreParseTypeInfo*> explicit_parse_type_args;

    DLLLOCAL UnresolvedProgramCallReferenceNode(const QoreProgramLocation* loc, char* n_str);

    DLLLOCAL virtual ~UnresolvedProgramCallReferenceNode();

    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);
};

class UnresolvedCallReferenceNode : public UnresolvedProgramCallReferenceNode {
public:
    DLLLOCAL UnresolvedCallReferenceNode(const QoreProgramLocation* loc, char* n_str) : UnresolvedProgramCallReferenceNode(loc, n_str) {
    }

    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);
};

//! a call reference to a static user method
class LocalStaticMethodCallReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    DLLLOCAL const QoreProgram* getOriginCodeProgram() const override;

    /** @param n_scope_cls the class that the reference names (\\H::m()), if not the class of the method; a
        reference to a static method of a generic class through a class that inherits a specialization of it calls
        the method with the type arguments of the specialization (qore#6285)
    */
    DLLLOCAL LocalStaticMethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method,
            const QoreClass* n_scope_cls = nullptr);

    DLLLOCAL virtual ~LocalStaticMethodCallReferenceNode() {
    }

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return LocalStaticMethodCallReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;
    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const override;

    DLLLOCAL virtual QoreValue execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreFunction* getFunction() const;

    DLLLOCAL const QoreMethod* getMethod() const {
        return method;
    }

    //! Returns the class that the reference names, which a compiled reference records in place of the method's
    DLLLOCAL const QoreClass* getScopeClass() const {
        return scope_cls ? scope_cls : (method ? method->getClass() : nullptr);
    }

    //! Returns the receiver of the calls through a reference to the static method \a m created at run time
    /** For a method of a generic class, it is the specialization that \a scope_cls (the class that the reference
        names) inherits, if any, otherwise, in the code of a class that is or inherits the method's class
        (\a code_class), the receiver of its running instantiation if it binds the type parameters of the method's
        class (\\G::m() in G<T>); if there is none, a call through the reference binds them to the types of its
        arguments, as a call G::m(...) does (see getCallReceiverTypeInfo(); qore#6285); for a method of a class
        without type parameters that has variants of a generic class, the class (see
        qore_class_private::getInheritedStaticCallReceiverTypeInfo(); qore#6448)
    */
    DLLLOCAL static const QoreTypeInfo* getRuntimeReceiverTypeInfo(const QoreMethod& m, const QoreClass* scope_cls,
            const qore_class_private* code_class);

    //! Returns the receiver of a call of the static method \a m with the arguments \a args
    /** \a receiver_type_info, or else for a method of a generic class the receiver that binds the type parameters of
        the class to the types of the arguments, if any (qore#6285), and for a method of a class without type
        parameters that has variants of a generic class, the class (qore#6448)
    */
    DLLLOCAL static const QoreTypeInfo* getCallReceiverTypeInfo(const QoreMethod& m,
            const QoreTypeInfo* receiver_type_info, const QoreListNode* args);

    //! as getCallReceiverTypeInfo(), for arguments that can need evaluation
    /** If the receiver is inferred from the arguments, unevaluated arguments are evaluated into \a eval_args first,
        and \a args is set to the evaluated list, which the call takes then: the inference needs their values
    */
    DLLLOCAL static const QoreTypeInfo* getCallReceiverTypeInfo(const QoreMethod& m,
            const QoreTypeInfo* receiver_type_info, const QoreListNode*& args,
            QoreListNodeEvalOptionalRefHolder& eval_args, ExceptionSink* xsink);

protected:
    const QoreMethod* method;
    //! the class that the reference names, if not the class of the method
    const QoreClass* scope_cls = nullptr;
    //! the receiver of the calls of a static method of a generic class that the reference has from its class
    const QoreTypeInfo* receiver_type_info = nullptr;

    // constructor for subclasses
    DLLLOCAL LocalStaticMethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method,
            bool n_needs_eval) : ResolvedCallReferenceNodeIntern(loc, n_needs_eval), method(n_method) {
    }

    DLLLOCAL virtual QoreValue evalImpl(bool &needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool &needs_deref, ExceptionSink* xsink) const;
};

class StaticMethodCallReferenceNode : public LocalStaticMethodCallReferenceNode {
protected:
    QoreProgram* pgm;
    const qore_class_private* class_ctx;
    //! the classes that a call of the reference reads, referenced as long as the reference exists (qore#6622)
    /** the class of the method, the class context of the code that created the reference, and the class that the
        receiver of the calls belongs to: the Program is held with a weak reference only, and the classes that it
        imported (its copies of them) are released when the Program is deleted, while a call of the reference reads
        the method and the classes before it finds that the Program has been deleted
    */
    const qore_class_private* held_classes[3] = {};

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink);

public:
    /** @param n_receiver_type_info the receiver of the calls of a static method of a generic class (see
        LocalStaticMethodCallReferenceNode::getRuntimeReceiverTypeInfo())
    */
    DLLLOCAL StaticMethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method,
            QoreProgram* n_pgm, const qore_class_private* n_class_ctx,
            const QoreTypeInfo* n_receiver_type_info = nullptr);

    DLLLOCAL ~StaticMethodCallReferenceNode() {
        assert(!pgm);
    }

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode *args, ExceptionSink *xsink) const;
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;
    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const override;

    DLLLOCAL virtual QoreValue execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreProgram* getProgram() const {
        return pgm;
    }
};

//! a call reference to a static user method resolved after AOT source linking
class DeferredStaticMethodCallReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    DLLLOCAL DeferredStaticMethodCallReferenceNode(const QoreProgramLocation* loc, const char* n_class_path,
            const char* n_method_name);

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return DeferredStaticMethodCallReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;
    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const override;

    DLLLOCAL virtual QoreFunction* getFunction() const {
        return nullptr;
    }

    DLLLOCAL const std::string& getClassPath() const {
        return class_path;
    }

    DLLLOCAL const std::string& getMethodName() const {
        return method_name;
    }

protected:
    std::string class_path;
    std::string method_name;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

private:
    DLLLOCAL ResolvedCallReferenceNode* resolve(RuntimeConfig& rc, ExceptionSink* xsink) const;
};

//! a call reference to a user function resolved after AOT source linking
class DeferredFunctionCallReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    DLLLOCAL DeferredFunctionCallReferenceNode(const QoreProgramLocation* loc, const char* n_function_name);

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return DeferredFunctionCallReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;

    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
            const LexicalSemanticPolicy& policy) const override;

    DLLLOCAL virtual QoreFunction* getFunction() const {
        return nullptr;
    }

    DLLLOCAL const std::string& getFunctionName() const {
        return function_name;
    }

protected:
    std::string function_name;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

private:
    DLLLOCAL FunctionCallReferenceNode* resolve(RuntimeConfig& rc, ExceptionSink* xsink) const;
};

//! a call reference to a static user method
class LocalMethodCallReferenceNode : public LocalStaticMethodCallReferenceNode {
protected:
    DLLLOCAL virtual QoreValue evalImpl(bool &needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool &needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL LocalMethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method, bool n_needs_eval) : LocalStaticMethodCallReferenceNode(loc, n_method, n_needs_eval) {
        //printd(5, "LocalMethodCallReferenceNode::LocalStaticMethodCallReferenceNode() this: %p %s::%s() pgm: %p\n", this, method->getClass()->getName(), method->getName(), pgm);
    }

public:
    DLLLOCAL LocalMethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method) : LocalStaticMethodCallReferenceNode(loc, n_method) {
        //printd(5, "LocalMethodCallReferenceNode::LocalStaticMethodCallReferenceNode() this: %p %s::%s() pgm: %p\n", this, method->getClass()->getName(), method->getName(), pgm);
    }

    DLLLOCAL virtual ~LocalMethodCallReferenceNode() {
    }

    //! binds the method of a reference to the method \a mname of the class \a qc deferred when it was parsed once the
    //! abstract methods are resolved (qore#7411)
    DLLLOCAL int parseDeferredMethodBind(QoreParseContext& parse_context, const QoreClass* qc, const char* mname);

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return LocalMethodCallReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;
    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const override;

    DLLLOCAL virtual QoreValue execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const;
};

class MethodCallReferenceNode : public LocalMethodCallReferenceNode {
protected:
    QoreObject* obj;

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink);

public:
    DLLLOCAL MethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method, QoreProgram* n_pgm);

    //! returns the object that the method is called on, if any
    DLLLOCAL const QoreObject* getObject() const {
        return obj;
    }

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;
    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const override;

    DLLLOCAL virtual QoreValue execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const;
};

//! a call reference to a user function from within the same QoreProgram object
class LocalFunctionCallReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    DLLLOCAL const QoreProgram* getOriginCodeProgram() const override;

    DLLLOCAL LocalFunctionCallReferenceNode(const QoreProgramLocation* loc, const QoreFunction* n_uf);

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;

    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
            const LexicalSemanticPolicy& policy) const override;

    DLLLOCAL virtual QoreValue execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return LocalFunctionCallReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreFunction* getFunction() const {
        return const_cast<QoreFunction*>(uf);
    }

    //! sets the explicit type arguments of the reference (\f<T>()), which instantiate the function when it is called
    DLLLOCAL void setExplicitTypeArgs(std::vector<const QoreTypeInfo*>&& type_args);

    //! binds a type parameter of the code that creates the reference, in the explicit type arguments, to the instantiation that is running
    DLLLOCAL void bindExplicitTypeArgs();

    //! returns the explicit type-parameter instantiation of the reference, or nullptr
    DLLLOCAL const QoreTypeParamInstantiation* getExplicitTypeParamInstantiation() const {
        return explicit_inst.get();
    }

    //! sets the function entry that the parser resolved the reference to
    /** The entry says which namespace the function is in, which the function does not: an AOT image names the
        function by the path of its namespace, so that a function of the root namespace is not taken for the builtin
        of the same name, nor a function of another namespace for the function of the same name in the root namespace
    */
    DLLLOCAL void setFunctionEntry(const FunctionEntry* n_fe) {
        fe = n_fe;
    }

    //! returns the function entry that the parser resolved the reference to, or nullptr
    DLLLOCAL const FunctionEntry* getFunctionEntry() const {
        return fe;
    }

protected:
    const QoreFunction* uf;
    //! the explicit type arguments of the reference, or null; shared with the copies that the evaluation makes
    std::shared_ptr<QoreTypeParamInstantiation> explicit_inst;
    const FunctionEntry* fe = nullptr;

    // constructor for subclasses
    DLLLOCAL LocalFunctionCallReferenceNode(const QoreProgramLocation* loc, const QoreFunction* n_uf,
            bool n_needs_eval);

    DLLLOCAL virtual QoreValue evalImpl(bool &needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool &needs_deref, ExceptionSink* xsink) const;
};

//! a call reference to a user function
/** The reference holds the function: a reference can outlive the Program that defines the function, which frees
    its functions when it is deleted, and a call of the reference must then fail as a call into a deleted Program
    does (see qore_get_function_pgm_context()), not use a freed function
*/
class FunctionCallReferenceNode : public LocalFunctionCallReferenceNode {
public:
    DLLLOCAL FunctionCallReferenceNode(const QoreProgramLocation* loc, const QoreFunction* n_uf,
            QoreProgram* n_pgm) : LocalFunctionCallReferenceNode(loc, n_uf, false), pgm(n_pgm) {
        assert(pgm);
        assert(uf);
        captureCreatorParseOptions();
        const_cast<QoreFunction*>(uf)->ref();
        // make a weak reference to the Program - a strong reference (QoreProgram::ref()) could cause a recursive
        // reference
        pgm->depRef();
    }

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;

    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
            const LexicalSemanticPolicy& policy) const override;

    DLLLOCAL virtual QoreValue execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreProgram* getProgram() const {
        return pgm;
    }

    //! keeps the declarations of the explicit type arguments; called once they are final
    /** The reference can outlive the Program that declared a type argument (a reference made by generic code of
        one Program for a class of another), whose declarations are freed with it (issue #6489)
    */
    DLLLOCAL void holdExplicitTypeArgDecls() {
        if (explicit_inst) {
            explicit_type_arg_decls.add(explicit_inst->type_args);
        }
    }

protected:
    QoreProgram* pgm;
    //! the declarations of the explicit type arguments (see holdExplicitTypeArgDecls())
    QoreTypeDeclRefs explicit_type_arg_decls;

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink);
};

//! an unresolved static method call reference, only present temporarily in the parse tree
class UnresolvedStaticMethodCallReferenceNode : public AbstractUnresolvedCallReferenceNode {
public:
    //! the explicit type arguments of \\NS::f<T>(), as parsed; they belong to the resolved reference of a function
    std::vector<QoreParseTypeInfo*> explicit_parse_type_args;

    DLLLOCAL UnresolvedStaticMethodCallReferenceNode(const QoreProgramLocation* loc, NamedScope* n_scope);

    DLLLOCAL virtual ~UnresolvedStaticMethodCallReferenceNode();

    using AbstractQoreNode::deref;
    DLLLOCAL void deref() {
        if (ROdereference())
            delete this;
    }

    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);

protected:
    NamedScope* scope;
};

class ImportedFunctionEntry;

//! a run-time call reference to a method of a particular object
class RunTimeObjectMethodReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    //! Raises the exception of a call of the method for an object that has been deleted, before the arguments are
    //! evaluated (issue #5784) and when the call is made: naming the class of the method and the method, as the call
    //! of the method raises it (issue #6083)
    /** @return -1 if the exception was raised, 0 if not
    */
    DLLLOCAL int checkObject(ExceptionSink* xsink) const;

    //! creates the reference
    /** @param n_missing true if the class of the object has no method of the name, so that a call runs a member
        holding code or a pseudo-method of objects (issue #995): such a call runs in the class context of the code
        that made the reference
    */
    DLLLOCAL RunTimeObjectMethodReferenceNode(const QoreProgramLocation* loc, QoreObject* n_obj,
            const char* n_method, bool n_missing = false);

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;
    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const override;

    DLLLOCAL virtual QoreValue execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreProgram* getProgram() const;

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return RunTimeObjectMethodReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreFunction* getFunction() const {
        // FIXME: implement type checking and method matching in ParseObjectMethodReferenceNode::parseInit()
        return nullptr;
    }

    //! returns the method that a call of the reference would run now, or nullptr if there is none
    DLLLOCAL const QoreMethod* findMethod() const;

private:
    QoreObject* obj;
    std::string method;
    //! the class context of the code that created the reference, if any, which a call looks the method up and selects
    //! its variants with
    /** Referenced as long as the reference exists: it can be a class of another Program than the object's, which
        releases it when it is deleted, while the reference can be called afterwards.  A call of a reference created
        with no class context is made with none (see evalInContext())
    */
    const qore_class_private* qc;
    //! true if the reference was made for a method that the object's class does not have (issue #995)
    bool missing;
    //! true if the method is looked up without a class context: \a qc has no private access to the object's class
    /** A method call in the code that created the reference looks the method up without a class context in this case
        (MethodCallNode::execImpl()) and selects its variants in the class context of that code (\a qc), as a call of
        the reference does (qore#7220)
    */
    bool lookup_without_ctx = false;

    //! returns the class context that a call looks the method up with (see lookup_without_ctx)
    DLLLOCAL const qore_class_private* getLookupContext() const {
        return lookup_without_ctx ? nullptr : qc;
    }
    //! the Program of the code that created the reference, or nullptr if there was none, with PGM_WEAK_REF set if
    //! the reference holds a weak reference to it
    /** A call runs the method as a direct call in this Program would: called from another Program, the method runs
        in this one with its parse options; see execValue().  Held with a weak reference unless it is the object's
        Program, which the object holds while the reference holds the object.  The flag is in the pointer, so that
        the destructor tests one value that it loads anyway
    */
    uintptr_t pgm_ref;
    //! set in \c pgm_ref if the reference holds a weak reference to the Program
    static constexpr uintptr_t PGM_WEAK_REF = 1;

    DLLLOCAL virtual ~RunTimeObjectMethodReferenceNode();

    //! evaluates \a f in the class context of the code that made the reference if it was made for a method that the
    //! object's class does not have, where a pseudo-method that depends on the class context runs (issue #995), or
    //! if that code had no class context, so that the call does not select the variants of the method with the class
    //! context of the code that calls the reference
    template <typename F>
    DLLLOCAL QoreValue evalInContext(F f) const;
};

//! a run-time call reference to a pseudo-method of a value that is not an object (issue #995): \c \\l.size() for a
//! list \c l
/** The reference holds the value; a call calls the method as a direct call with the value does: a code member of a
    hash with the name of the method, or the pseudo-method of the value's type (pseudo_classes_eval()).  A reference
    to a pseudo-method of an object is a RunTimeObjectMethodReferenceNode, whose call looks the method up by name as a
    direct call does.  The value is scanned for recursive references (qore_get_call_reference_scan_node())
*/
class RunTimePseudoMethodReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    //! a pseudo-method is builtin code
    DLLLOCAL const QoreProgram* getOriginCodeProgram() const override {
        return nullptr;
    }

    //! creates the reference; takes a new reference to \a n_val, which is not an object
    DLLLOCAL RunTimePseudoMethodReferenceNode(const QoreProgramLocation* loc, QoreValue n_val, const char* n_method);

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const override;
    //! a call with named arguments, bound to the parameters of the pseudo-method as a direct call does
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return RunTimePseudoMethodReferenceNode::is_equal_hard(v, xsink);
    }

    //! returns the value that the reference holds
    DLLLOCAL const QoreValue& getValue() const {
        return val;
    }

    //! two references are equal if they refer to the same pseudo-method of the same value (hard comparison)
    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! returns the function of the pseudo-method of the value's type, or nullptr if there is none
    DLLLOCAL virtual QoreFunction* getFunction() const;

protected:
    //! releases the value, which can hold objects, with the ExceptionSink of the dereference
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink);

private:
    //! the value
    QoreValue val;
    std::string method;

    //! returns the code member of a hash value with the name of the method, which a direct call calls, or nullptr
    /** @param hash_holder holds the hash of a weak reference, read with a reference of its own (qore#7198), while the
        member returned is used
    */
    DLLLOCAL const ResolvedCallReferenceNode* getHashMember(ReferenceHolder<QoreHashNode>& hash_holder) const;

    DLLLOCAL virtual ~RunTimePseudoMethodReferenceNode();
};

//! makes a call reference to the method \a method of \a val, which is not an object (issue #995)
/** raises an exception as a direct call of the pseudo-method does if the value's type has no such pseudo-method and
    the value is not a hash with a code member of the name

    @return the reference, or no value if an exception was raised
*/
DLLLOCAL QoreValue qore_make_pseudo_method_reference(const QoreProgramLocation* loc, QoreValue val,
        const char* method, ExceptionSink* xsink);

//! returns true if an object of the class has a method of the name for a call reference that a class method does not
//! provide: a pseudo-method of objects, or a member, which a direct call calls if it holds code (issue #995)
DLLLOCAL bool qore_object_has_pseudo_or_member_method(const qore_class_private* cls, const char* method,
        const qore_class_private* class_ctx);

//! a run-time call reference to a method of a particular object where the method's class
// is known when the method reference node object is created
class RunTimeResolvedMethodReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    DLLLOCAL const QoreProgram* getOriginCodeProgram() const override;

    //! Raises the exception of a call of the method for an object that has been deleted, before the arguments are
    //! evaluated (issue #5784) and when the call is made: naming the class of the method and the method, as the call
    //! of the method raises it (issue #6083)
    /** @return -1 if the exception was raised, 0 if not
    */
    DLLLOCAL int checkObject(ExceptionSink* xsink) const;

    //! creates the reference with the class context of the creating code
    DLLLOCAL RunTimeResolvedMethodReferenceNode(const QoreProgramLocation* loc, QoreObject* n_obj,
            const QoreMethod* n_method);

    DLLLOCAL RunTimeResolvedMethodReferenceNode(const QoreProgramLocation* loc, QoreObject* n_obj,
            const QoreMethod* n_method, const qore_class_private* ctx);

    DLLLOCAL virtual QoreValue dispatchValue(const QoreListNode* args, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue dispatchNamed(const QoreListNode* args, const name_vec_t& arg_names,
            LexicalSemanticPolicy policy, ExceptionSink* xsink) const override;
    DLLLOCAL QoreValue dispatchLexicalValue(const QoreListNode* args, ExceptionSink* xsink,
        const LexicalSemanticPolicy& policy) const override;

    DLLLOCAL virtual QoreValue execValueTmpArgs(QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreProgram* getProgram() const;

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return RunTimeResolvedMethodReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreFunction* getFunction() const;

    //! returns the method that the reference was resolved to
    DLLLOCAL const QoreMethod* getMethod() const {
        return method;
    }

    //! returns the object that the method is called on
    DLLLOCAL const QoreObject* getObject() const {
        return obj;
    }

private:
    QoreObject* obj;
    const QoreMethod* method;
    //! the class context that a call of the reference selects the variants of the method with
    /** The class context of the code that created the reference, or qore_no_class_ctx() if it had none, so that a
        call selects only the public variants wherever it is made, never with the class context of the caller.  A
        class is referenced as long as the reference exists: it can be a class of another Program than the object's,
        such as a subclass of the object's class, which is released with its Program (see init())
    */
    const qore_class_private* qc;
    //! the Program of the reference, with PGM_CREATOR set if it is the Program of the code that created it
    /** Without PGM_CREATOR (a user method, or a builtin method of an object of the Program that creates the
        reference): the Program of the object when the reference was created, or nullptr; the method runs in the
        Program that defines its class, and this one is used only when that Program has released the class.  Also
        nullptr for a builtin method of a reference created with no Program, which runs with none.

        With PGM_CREATOR (a builtin method of an object of another Program): the Program of the code that created the
        reference, held with a weak reference; a call runs the method as a direct call in this Program would, not in
        the Program of the object, which for a builtin object is the Program it was created for (a reflection handle
        is created in the Program that it describes).

        See execValue().  One tagged pointer keeps the reference as small as a reference with a single Program, and
        a call and the destructor test the case in the value that they load anyway
    */
    uintptr_t pgm_ref;
    //! set in \c pgm_ref if it is the Program of the code that created the reference
    static constexpr uintptr_t PGM_CREATOR = 1;

    //! sets up the Program of the reference; \a creator is the Program of the code that creates it
    DLLLOCAL void init(QoreProgram* creator);

    DLLLOCAL virtual ~RunTimeResolvedMethodReferenceNode();
};

//! returns the Program whose code a call of the given call reference runs
/** For a method reference, this is the Program that defines the class of the method that a call would run now: calls
    of a method reference are dispatched on the class of the object, so a subclass that overrides the method runs
    its own code; for a closure, it is the Program that defines the closure's code; for any other reference, it is
    ResolvedCallReferenceNode::getProgram().  Returns nullptr for builtin code, which has no Program.
*/
DLLLOCAL const QoreProgram* qore_call_reference_code_program(const ResolvedCallReferenceNode* rcr);

//! Raises the exception for a call of a value that is not a call reference or a closure, as the AST interpreter
//! raises it (CallReferenceCallNode::evalImpl()); every execution mode uses it (issue #5784)
/** @param loc the location of the call, if known
*/
DLLLOCAL void qore_raise_not_call_reference(ExceptionSink* xsink, const QoreProgramLocation* loc, const QoreValue& v);

//! Raises the exception that a call through @a rcr raises before it evaluates the arguments, if any (issue #5784)
/** A reference to a method of an object that has been deleted.

    @return -1 if the exception was raised, 0 if not
*/
DLLLOCAL int qore_call_reference_check_target(const ResolvedCallReferenceNode* rcr, ExceptionSink* xsink);

//! Gets the parse options of the Program whose code a call reference runs (see qore_call_reference_code_program())
/** The Program of a user function is the Program that defined it, which can have been deleted while a Program that
    imported the function still holds it: it is taken with a reference while its options are read.  Such a Program
    is deleted once another thread has deleted it or is deleting it, also while threads still hold references to it.

    @param sandboxed if not nullptr, set to true if the same Program is sandboxed (see qore_program_is_sandboxed())

    @return 1 if the code has a Program (\a po is set to its options), 0 if it has none (builtin code), -1 if the
    Program of its code has been deleted
*/
DLLLOCAL int qore_call_reference_code_parse_options(const ResolvedCallReferenceNode* rcr, QoreParseOptions& po,
        ExceptionSink* xsink, bool* sandboxed = nullptr);

#endif
