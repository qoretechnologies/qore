/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreReflection.h external reflection API definitions */
/*
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

#ifndef _QORE_QOREREFLECTION_H

#define _QORE_QOREREFLECTION_H

//! external wrapper class for function and call variants
/** @since %Qore 0.9
*/
class QoreExternalVariant {
public:
    //! returns the class for a method variant or nullptr for a normal variant
    DLLEXPORT const QoreClass* getClass() const;

    //! returns the signature for the variant
    DLLEXPORT const char* getSignatureText() const;

    //! returns binary-or-combined code flags
    DLLEXPORT int64 getCodeFlags() const;

    //! returns true if the variant has the module public flag set
    DLLEXPORT bool isModulePublic() const;

    //! returns true if the variant has the synchronized flag set
    DLLEXPORT bool isSynchronized() const;

    //! returns true if the variant is builtin
    DLLEXPORT bool isBuiltin() const;

    //! returns true if the variant has a function body
    DLLEXPORT bool hasBody() const;

    //! returns the functional domain of the variant
    DLLEXPORT int64 getDomain() const;

    //! returns the number of parameters
    DLLEXPORT unsigned numParams() const;

    //! returns the return type
    DLLEXPORT const QoreTypeInfo* getReturnTypeInfo() const;

    //! returns the parameter types for the variant
    DLLEXPORT const type_vec_t& getParamTypeList() const;

    //! returns a list of default arguments for the variant
    DLLEXPORT const arg_vec_t& getDefaultArgList() const;

    //! returns a list of parameter names for the variant
    DLLEXPORT const name_vec_t& getParamNames() const;

    //! returns the source location of the variant
    DLLEXPORT const QoreExternalProgramLocation* getSourceLocation() const;

private:
    //! not implemented
    DLLLOCAL QoreExternalVariant();
};

//! external wrapper class for method variants
/** @since %Qore 0.9
*/
class QoreExternalMethodVariant : public QoreExternalVariant {
public:
    //! returns the method for a method variant
    DLLEXPORT const QoreMethod* getMethod() const;

    //! returns true if the method variant is abstract
    DLLEXPORT bool isAbstract() const;

    //! returns true if the method variant is final
    DLLEXPORT bool isFinal() const;

    //! returns true if the method variant is static
    DLLEXPORT bool isStatic() const;

    //! returns the access info for the member
    DLLEXPORT ClassAccess getAccess() const;

    //! returns a string for the access info for the member
    DLLEXPORT const char* getAccessString() const;

private:
    //! not implemented
    DLLLOCAL QoreExternalMethodVariant();
};

//! external wrapper base class for class and hashdecl members
/** @since %Qore 0.9
*/
class QoreExternalMemberBase {
public:
    //! returns the type info for the member
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! evaluates the initialization expression for the member and returns the referenced value
    DLLEXPORT QoreValue getDefaultValue(ExceptionSink* xsink) const;

    //! returns the source location of the member's declaration
    DLLEXPORT const QoreExternalProgramLocation* getSourceLocation() const;

private:
    //! not implemented
    DLLLOCAL QoreExternalMemberBase();
};

//! external wrapper base class for class members
/** @since %Qore 0.9
*/
class QoreExternalMemberVarBase : public QoreExternalMemberBase {
public:
    //! returns the access info for the member
    DLLEXPORT ClassAccess getAccess() const;

    //! returns a string for the access info for the member
    DLLEXPORT const char* getAccessString() const;

private:
    //! not implemented
    DLLLOCAL QoreExternalMemberVarBase();
};

//! external wrapper base class for class static members
/** @since %Qore 0.9
*/
class QoreExternalStaticMember : public QoreExternalMemberVarBase {
public:
    //! returns the current value of the member; caller owns an reference returned
    DLLEXPORT QoreValue getValue() const;

    //! sets the value of the member
    /** @param val the value to set
        @param xsink Qore-language exception info is stored here

        @return 0 = no error, -1 Qore-language exception raised
    */
    DLLEXPORT int setValue(const QoreValue val, ExceptionSink* xsink) const;

private:
    //! not implemented
    DLLLOCAL QoreExternalStaticMember();
};

//! external wrapper base class for class normal members
/** @since %Qore 0.9
*/
class QoreExternalNormalMember : public QoreExternalMemberVarBase {
public:
    //! returns true if the member is transient (i.e. will not be serialized)
    DLLEXPORT bool isTransient() const;

private:
    //! not implemented
    DLLLOCAL QoreExternalNormalMember();
};

//! external wrapper class for source code location information
/** @since %Qore 0.9
*/
class QoreExternalProgramLocation {
public:
   //! returns a hash of the given source location
    DLLEXPORT QoreHashNode* getHash() const;

private:
    //! not implemented
    DLLLOCAL QoreExternalProgramLocation();
};

//! external wrapper class for constants
/** @since %Qore 0.9
*/
class QoreExternalConstant {
public:
    //! returns the constant name
    DLLEXPORT const char* getName() const;

    //! returns the module name, if any, otherwise returns nullptr
    /** @since %Qore 0.9.5
    */
    DLLEXPORT const char* getModuleName() const;

    //! returns true if the constant has the module public flag set
    DLLEXPORT bool isModulePublic() const;

    //! returns true if the constant is builtin
    DLLEXPORT bool isBuiltin() const;

    //! returns the type info for the constant
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! returns the value of the constant; caller owns any reference returned
    DLLEXPORT QoreValue getReferencedValue() const;

    //! returns the value of the constant
    DLLEXPORT const QoreValue getValue() const;

    //! returns the source code location of the constant's definition
    DLLEXPORT const QoreExternalProgramLocation* getSourceLocation() const;

    //! returns the access info for the constant
    DLLEXPORT ClassAccess getAccess() const;

private:
    //! not implemented
    DLLLOCAL QoreExternalConstant();
};

class QoreExternalFunction {
public:
    //! returns the function name
    DLLEXPORT const char* getName() const;

    //! returns the module name, if any, otherwise returns nullptr
    /** @since %Qore 0.9.5
    */
    DLLEXPORT const char* getModuleName() const;

    //! returns the class for the function if the function belongs to a class method or nullptr if not
    DLLEXPORT const QoreClass* getClass() const;

    //! returns a variant matching the arguments exactly or throws a Qore-language exception and returns nullptr
    DLLEXPORT const QoreExternalVariant* findVariant(const type_vec_t& type_vec, ExceptionSink* xsink) const;

    //! returns true if the function is builtin
    DLLEXPORT bool isBuiltin() const;

    //! returns true if the function has been injected as a dependency injection
    DLLEXPORT bool isInjected() const;

    //! returns the number of variants for this function
    DLLEXPORT unsigned numVariants() const;

    //! evaluates the function with the given variant (which must belong to the function) and program (where the function must be defined) and returns the result
    /** @note Do not use this function with class methods
    */
    DLLEXPORT QoreValue evalFunction(const QoreExternalVariant* variant, const QoreListNode* args, QoreProgram* pgm,
            ExceptionSink* xsink) const;

    //! returns the first declared variant in the variant list
    DLLEXPORT const QoreExternalVariant* getFirstVariant() const;

    //! returns capabilities common to all variants
    /** Check variants for specific capabilities

        @since %Qore 0.9.5
    */
    DLLEXPORT int64 getDomain() const;

    //! returns flags common to all variants
    /** Check variants for specific flags

        @since %Qore 0.9.5
    */
    DLLEXPORT int64 getCodeFlags() const;

private:
    //! not implemented
    DLLLOCAL QoreExternalFunction();
};

class QoreExternalFunctionIterator {
public:
    DLLEXPORT QoreExternalFunctionIterator(const QoreExternalFunction& f);

    DLLEXPORT ~QoreExternalFunctionIterator();

    DLLEXPORT bool next();

    DLLEXPORT const QoreExternalVariant* getVariant();

private:
    class qore_external_function_iterator_private* priv;
};

class QoreExternalMethodFunction : public QoreExternalFunction {
public:
    //! returns the method for the function if the function belongs to a class method or nullptr if not
    DLLEXPORT const QoreMethod* getMethod() const;

    //! evaluates the function with the given variant (which must belong to the method) and program (where the method must be defined) and returns the result
    DLLEXPORT QoreValue evalNormalMethod(QoreObject* obj, const QoreExternalMethodVariant* variant,
            const QoreListNode* args, QoreProgram* pgm, ExceptionSink* xsink) const;

    //! evaluates the method as a static method with the given variant (which must belong to the method) and program (where the method must be defined) and returns the result
    DLLEXPORT QoreValue evalStaticMethod(const QoreExternalMethodVariant* variant, const QoreListNode* args,
            QoreProgram* pgm, ExceptionSink* xsink) const;

    //! returns true if the method is static
    DLLEXPORT bool isStatic() const;

private:
    //! not implemented
    DLLLOCAL QoreExternalMethodFunction();
};

class QoreExternalGlobalVar {
public:
    //! returns the global variable's name
    DLLEXPORT const char* getName() const;

    //! returns true if the global variable has the module public flag set
    DLLEXPORT bool isModulePublic() const;

    //! returns true if the global variable is builtin
    DLLEXPORT bool isBuiltin() const;

    //! returns the type info for the global variable
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! returns the value of the global variable; caller owns any reference returned
    DLLEXPORT QoreValue getReferencedValue() const;

    //! returns the source code location of the global variable's definition
    DLLEXPORT const QoreExternalProgramLocation* getSourceLocation() const;

    //! sets the value of the global variable
    /** @param the value to set
        @param xsink Qore-language exception info is stored here

        @return 0 = no error, -1 Qore-language exception raised
    */
    DLLEXPORT int setValue(const QoreValue val, ExceptionSink* xsink) const;

private:
    //! not implemented
    DLLLOCAL QoreExternalGlobalVar();
};

//! returns the name of the type; the argument may be nullptr meaning no type restrictions
DLLEXPORT const char* qore_type_get_name(const QoreTypeInfo* ti);

//! returns the full namespace path of the type; the argument may be nullptr meaning no type restrictions
/** @since %Qore 1.0
*/
DLLEXPORT const char* qore_type_get_path(const QoreTypeInfo* ti);

//! returns true if the types are equal; either argument may be nullptr meaning no type restrictions
DLLEXPORT bool qore_type_equal(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2);

//! returns true if ti2's return type is compatible with ti1's; either argument may be nullptr meaning no type restrictions
DLLEXPORT bool qore_type_is_output_compatible(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2);

//! returns true if ti2's return type is compatible with ti1 input (i.e. ti1 is assignable from ti2)
/** @param ti1 the type to check assignability to
    @param ti2 the type to see if it can be assigned to \a ti1

    @returns true if ti2's return type is compatible with ti1 input (i.e. ti1 is assignable from ti2)

    either argument may be nullptr meaning no type restrictions

    @since %Qore 0.9.4
*/
DLLEXPORT bool qore_type_is_assignable_from(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2);

//! returns true if ti2's return type is compatible with ti1 input (i.e. ti1 is assignable from ti2)
/** @param ti1 the type to check assignability to
    @param ti2 the type to see if it can be assigned to \a ti1
    @param may_not_match an output variable, if true then the assignment is not guaranteed to succeed and may fail at
    runtime

    @returns true if ti2's return type is compatible with ti1 input (i.e. ti1 is assignable from ti2)

    either argument may be nullptr meaning no type restrictions

    @since %Qore 0.9.4
*/
DLLEXPORT bool qore_type_is_assignable_from(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2, bool& may_not_match);

//! returns true if the given type can be assigned from the given value
/** @param t the type to check assignability to
    @param value the value to check if it can be assigned to \a t

    @returns a type match code for the assignability

    The type argument \a t may be nullptr meaning no type restrictions

    @since %Qore 0.9.4
*/
DLLEXPORT int qore_type_is_assignable_from(const QoreTypeInfo* t, QoreValue value);

//! processes the given value by the given type and returns the result; a Qore-language exception is thrown if the value is not assignable to the type
/**
    @since %Qore 0.9.4
*/
DLLEXPORT QoreValue qore_type_assign_value(const QoreTypeInfo* t, const QoreValue value, ExceptionSink* xsink);

//! returns the base type code for the type or NT_ALL for those that don't have types
/**
    @since %Qore 0.9.4
*/
DLLEXPORT qore_type_t qore_type_get_base_type(const QoreTypeInfo* t);

//! returns a hash of base types accepted by the type
/** @note equivalent to qore_type_get_accept_types(t, false)

    @since %Qore 0.9.4
*/
DLLEXPORT QoreHashNode* qore_type_get_accept_types(const QoreTypeInfo* t);

//! returns a hash of base types accepted by the type
/** @param simple if true then only simple types are returned by this call

    @since %Qore 2.0
*/
DLLEXPORT QoreHashNode* qore_type_get_accept_types(const QoreTypeInfo* t, bool simple);

//! returns a hash of base types returned by the type
/** @note equivalent to qore_type_get_return_types(t, false)

    @since %Qore 0.9.4
*/
DLLEXPORT QoreHashNode* qore_type_get_return_types(const QoreTypeInfo* t);

//! returns a hash of base types returned by the type
/** @param simple if true then only simple types are returned by this call

    @since %Qore 2.0
*/
DLLEXPORT QoreHashNode* qore_type_get_return_types(const QoreTypeInfo* t, bool simple);

//! returns true if the type's value can be converted to a scalar; the argument may be nullptr meaning no type restrictions
DLLEXPORT bool qore_type_can_convert_to_scalar(const QoreTypeInfo* ti);

//! returns true if the type has a default value; the argument may be nullptr meaning no type restrictions
DLLEXPORT bool qore_type_has_default_value(const QoreTypeInfo* ti);

//! returns the default value of the type (if any); the caller owns any reference returned; the argument may be nullptr meaning no type restrictions
DLLEXPORT QoreValue qore_type_get_default_value(const QoreTypeInfo* ti);

//! returns true if the types are compatible with inputs and outputs
DLLEXPORT bool qore_type_is_input_output_compatible(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2);

//! returns true if the type is a typed code type (code<ReturnType(ParamTypes...)>)
/** @since %Qore 2.1
*/
DLLEXPORT bool qore_type_is_typed_code(const QoreTypeInfo* ti);

//! returns the return type of a typed code type, or nullptr if not a typed code type
/** @since %Qore 2.1
*/
DLLEXPORT const QoreTypeInfo* qore_type_get_code_return_type(const QoreTypeInfo* ti);

//! returns a pointer to the parameter types vector for a typed code type, or nullptr if not a typed code type
/** @note this returns a pointer to internal storage; do not delete
    @since %Qore 2.1
*/
DLLEXPORT const type_vec_t* qore_type_get_code_param_types(const QoreTypeInfo* ti);

//! returns true if a typed code type accepts variable arguments
/** @since %Qore 2.1
*/
DLLEXPORT bool qore_type_code_has_varargs(const QoreTypeInfo* ti);

//! returns true if the type is a union type (union<T1, T2, ...>)
/** @since %Qore 2.1
*/
DLLEXPORT bool qore_type_is_union(const QoreTypeInfo* ti);

//! returns a new vector of member types for a union type, or nullptr if not a union type
/** @note the caller owns the vector returned and must delete it
    @since %Qore 2.1
*/
DLLEXPORT type_vec_t* qore_type_get_union_member_types(const QoreTypeInfo* ti);

#endif
