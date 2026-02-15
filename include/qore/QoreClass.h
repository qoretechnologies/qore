/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreClass.h

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

#ifndef _QORE_QORECLASS_H

#define _QORE_QORECLASS_H

#include <cstdarg>
#include <memory>
#include <string>

// all qore class IDs
DLLEXPORT extern qore_classid_t CID_AUTOGATE;
DLLEXPORT extern qore_classid_t CID_AUTOLOCK;
DLLEXPORT extern qore_classid_t CID_AUTOREADLOCK;
DLLEXPORT extern qore_classid_t CID_AUTOWRITELOCK;
DLLEXPORT extern qore_classid_t CID_CONDITION;
DLLEXPORT extern qore_classid_t CID_COUNTER;
DLLEXPORT extern qore_classid_t CID_DATASOURCE;
DLLEXPORT extern qore_classid_t CID_DATASOURCEPOOL;
DLLEXPORT extern qore_classid_t CID_FILE;
DLLEXPORT extern qore_classid_t CID_FTPCLIENT;
DLLEXPORT extern qore_classid_t CID_GATE;
DLLEXPORT extern qore_classid_t CID_GETOPT;
DLLEXPORT extern qore_classid_t CID_HTTPCLIENT;
DLLEXPORT extern qore_classid_t CID_MUTEX;
DLLEXPORT extern qore_classid_t CID_PROGRAM;
DLLEXPORT extern qore_classid_t CID_PROGRAMDEBUG;
DLLEXPORT extern qore_classid_t CID_QUEUE;
DLLEXPORT extern qore_classid_t CID_RWLOCK;
DLLEXPORT extern qore_classid_t CID_SSLCERTIFICATE;
DLLEXPORT extern qore_classid_t CID_SSLPRIVATEKEY;
DLLEXPORT extern qore_classid_t CID_SEQUENCE;
DLLEXPORT extern qore_classid_t CID_SOCKET;
DLLEXPORT extern qore_classid_t CID_TERMIOS;
DLLEXPORT extern qore_classid_t CID_INPUTSTREAM;
DLLEXPORT extern qore_classid_t CID_OUTPUTSTREAM;
DLLEXPORT extern qore_classid_t CID_INPUTSTREAMBASE;
DLLEXPORT extern qore_classid_t CID_OUTPUTSTREAMBASE;
DLLEXPORT extern qore_classid_t CID_PROGRAM;
DLLEXPORT extern qore_classid_t CID_SERIALIZABLE;
DLLEXPORT extern qore_classid_t CID_ABSTRACTPOLLABLEIOOBJECT;
DLLEXPORT extern qore_classid_t CID_ABSTRACTPOLLABLEIOOBJECTBASE;
DLLEXPORT extern qore_classid_t CID_ABSTRACTPOLLOPERATION;
DLLEXPORT extern qore_classid_t CID_SOCKETPOLLOPERATIONBASE;
DLLEXPORT extern qore_classid_t CID_SOCKETPOLLOPERATION;
DLLEXPORT extern qore_classid_t CID_FILEPOLLOPERATIONBASE;
DLLEXPORT extern qore_classid_t CID_FILEPOLLOPERATION;
DLLEXPORT extern qore_classid_t CID_EVENTLOOP;
DLLEXPORT extern qore_classid_t CID_EVENTNOTIFIER;
DLLEXPORT extern qore_classid_t CID_SANDBOXMANAGER;

DLLEXPORT extern QoreClass* QC_ABSTRACTPOLLABLEIOOBJECT;
DLLEXPORT extern QoreClass* QC_ABSTRACTPOLLABLEIOOBJECTBASE;
DLLEXPORT extern QoreClass* QC_ABSTRACTPOLLOPERATION;
DLLEXPORT extern QoreClass* QC_CONDITION;
DLLEXPORT extern QoreClass* QC_COUNTER;
DLLEXPORT extern QoreClass* QC_FILEPOLLOPERATIONBASE;
DLLEXPORT extern QoreClass* QC_FILEPOLLOPERATION;
DLLEXPORT extern QoreClass* QC_EVENTLOOP;
DLLEXPORT extern QoreClass* QC_HTTPCLIENT;
DLLEXPORT extern QoreClass* QC_MUTEX;
DLLEXPORT extern QoreClass* QC_QUEUE;
DLLEXPORT extern QoreClass* QC_PROGRAM;
DLLEXPORT extern QoreClass* QC_RWLOCK;
DLLEXPORT extern QoreClass* QC_SERIALIZABLE;
DLLEXPORT extern QoreClass* QC_SOCKETPOLLOPERATIONBASE;
DLLEXPORT extern QoreClass* QC_SOCKETPOLLOPERATION;
DLLEXPORT extern QoreClass* QC_SSLCERTIFICATE;
DLLEXPORT extern QoreClass* QC_SSLPRIVATEKEY;
DLLEXPORT extern QoreClass* QC_SANDBOXMANAGER;

class BCList;
class BCSMList;
class BCAList;
class QoreObject;
class QoreClass;
class BCEAList;
class ParamList;
class QoreMemberInfo;
class BuiltinMethod;
class AbstractQoreFunctionVariant;
class AbstractFunctionSignature;
class UserMethod;
class BCANode;
class qore_method_private;
class MethodFunctionBase;
class NamedScope;
class ConstantList;
class MethodVariantBase;

class QoreExternalVariant;
class QoreExternalMethodVariant;
class QoreExternalNormalMember;
class QoreExternalStaticMember;
class QoreExternalProgramLocation;
class QoreExternalMethodFunction;
class QoreExternalMemberVarBase;
class QoreExternalStaticMember;
class QoreExternalNormalMember;
class QoreExternalConstant;

//! method type enum
/** @since %Qore 0.9
*/
enum method_type_e {
    MT_None = 0, // not a method function/variant
    MT_Normal = 1,
    MT_Static = 2,
    MT_Constructor = 3,
    MT_Destructor = 4,
    MT_Copy = 5,
    MT_Pseudo = 6,
};

//! a method in a QoreClass
/** methods can be implemented in the Qore language (user methods) or in C++ (builtin methods)
    @see QoreClass
 */
class QoreMethod {
    friend class StaticMethodCallNode;
    friend class QoreObject;
    friend class qore_class_private;
    friend class qore_method_private;
    friend class BCList;

private:
    //! private implementation of the method
    class qore_method_private* priv;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreMethod(const QoreMethod&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreMethod& operator=(const QoreMethod&);

public:
    //! returns true if all variants of the method are user variants
    /** @return true if all variants of the method are user variants
    */
    DLLEXPORT bool isUser() const;

    //! returns true if all variants of the method are builtin variants
    /** @return true if all variants of the method are builtin variants
    */
    DLLEXPORT bool isBuiltin() const;

    //! returns true if all overloaded variants of a methods are private or class internal, false if at least one variant is public
    /** @return true if all overloaded variants of a methods are private or class internal, false if at least one variant is public
    */
    DLLEXPORT bool isPrivate() const;

    //! returns the lowest access code of all variants in the method
    DLLEXPORT ClassAccess getAccess() const;

    //! returns true if the method is static
    /**
         @return true if the method is static
    */
    DLLEXPORT bool isStatic() const;

    //! returns the method's name
    /**
         @return the method's name
    */
    DLLEXPORT const char* getName() const;

    //! returns the method's name
    /**
         @return the method's name
    */
    DLLEXPORT const std::string& getNameStr() const;

    //! returns a pointer to the parent class
    DLLEXPORT const QoreClass* getClass() const;

    //! returns the class name for the method
    DLLEXPORT const char* getClassName() const;

    //! returns true if a variant with the given parameter signature already exists in the method
    DLLEXPORT bool existsVariant(const type_vec_t& paramTypeInfo) const;

    /* returns the return type information for the method if it is available and if
        there is only one return type (there can be more return types if the method is
        overloaded)
    */
    DLLEXPORT const QoreTypeInfo* getUniqueReturnTypeInfo() const;

    //! evaluates the method and returns the value, does not reference the object for the call
    /** @note this method should only be used when the caller can guarantee that the object will not go out of scope during the call

        @since %Qore 0.8.12
    */
    DLLEXPORT QoreValue execManaged(QoreObject* self, const QoreListNode* args, ExceptionSink* xsink) const;

    //! returns the type of method
    /** @since %Qore 0.9
    */
    DLLEXPORT method_type_e getMethodType() const;

    //! returns the function for the method
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalMethodFunction* getFunction() const;

    DLLLOCAL QoreMethod(const QoreClass* p_class, MethodFunctionBase* n_func, bool n_static = false);

    DLLLOCAL bool inMethod(const QoreObject* self) const;
    DLLLOCAL QoreMethod* copy(const QoreClass* p_class) const;
    DLLLOCAL void assign_class(const QoreClass* p_class);
    // non-exported destructor
    DLLLOCAL ~QoreMethod();
};

//! an abstract class for class-specific external user data
/** @since %Qore 0.8.13
 */
class AbstractQoreClassUserData {
public:
    DLLEXPORT virtual ~AbstractQoreClassUserData();

    //! for reference-counted classes, returns the same object with the reference count incremented
    virtual AbstractQoreClassUserData* copy() const = 0;

    //! for non-reference counted classes, deletes the object immediately
    virtual void doDeref() = 0;
};

//! A holder class for AbstractQoreClassUserData objects
/** @since %Qore 2.0
*/
class QoreClassUserDataHolder {
public:
    //! Creates the object
    DLLLOCAL QoreClassUserDataHolder(AbstractQoreClassUserData* d) : d(d) {
    }

    //! Destroys the object by calling doDeref() on the user data object if it's still managed
    DLLLOCAL ~QoreClassUserDataHolder() {
        if (d) {
            d->doDeref();
        }
    }

    //! Returns the user data object if still managed
    DLLLOCAL AbstractQoreClassUserData* release() {
        AbstractQoreClassUserData* rv = d;
        d = nullptr;
        return rv;
    }

    //! Returns true if the user data object is still managed
    DLLLOCAL operator bool() const {
        return d ? true : false;
    }

    //! Returns the user data object, if any
    DLLLOCAL const AbstractQoreClassUserData* operator*() const {
        return d;
    }

    //! Returns the user data object, if any
    DLLLOCAL AbstractQoreClassUserData* operator*() {
        return d;
    }

    //! Returns the user data object, if any
    DLLLOCAL const AbstractQoreClassUserData* operator->() const {
        return d;
    }

    //! Returns the user data object, if any
    DLLLOCAL AbstractQoreClassUserData* operator->() {
        return d;
    }

private:
    //! The user data object being managed
    AbstractQoreClassUserData* d;
};

//! defines a Qore-language class
/** Qore's classes can be either implemented by Qore language code (user classes)
    or in C++ (builtin classes), or both, as in the case of a builtin class that
    also has user methods.
*/
class QoreClass {
    friend class BCList;
    friend class BCNode;
    friend class BCSMList;
    friend class qore_object_private;
    friend class qore_class_private;
    friend class QoreObject;
    friend class BCANode;
    friend class qore_method_private;
    friend class QoreMethodIterator;
    friend class QoreStaticMethodIterator;
    friend class ConstructorMethodFunction;
    friend class QoreBuiltinClass;
    friend class QoreParseClass;

public:
    //! creates the QoreClass object and assigns the name and the functional domain
    /** @note class names and subnamespaces names must be unique in a namespace; i.e. no class may have the same name as a subnamespace within a namespace and vice-versa
        @param n_name the name of the class
        @param ns_path the full pathname of the class with namespaces, including the root "::" namespace as the
        leading element
        @param n_domain the functional domain of the class to be used to enforce functional restrictions within a Program object

        @see QoreProgram

        @since %Qore 1.0
    */
    DLLEXPORT QoreClass(std::string&& n_name, std::string&& ns_path, int64 n_domain = QDOM_DEFAULT);

    //! creates the QoreClass object and assigns the name and the functional domain
    /** @note class names and subnamespaces names must be unique in a namespace; i.e. no class may have the same name as a subnamespace within a namespace and vice-versa
        @param n_name the name of the class
        @param ns_path the full pathname of the class with namespaces, including the root "::" namespace as the
        leading element
        @param n_domain the functional domain of the class to be used to enforce functional restrictions within a Program object

        @see QoreProgram

        @since %Qore 1.0
    */
    DLLEXPORT QoreClass(const char* n_name, const char* ns_path, int64 n_domain = QDOM_DEFAULT);

    //! creates the QoreClass object and assigns the name, the functional domain, and a custom QoreTypeInfo object created with AbstractQoreClassTypeInfoHelper
    /** @note class names and subnamespaces names must be unique in a namespace; i.e. no class may have the same name
        as a subnamespace within a namespace and vice-versa

        @param n_name the name of the class
        @param ns_path the full pathname of the class with namespaces, including the root "::" namespace as the
        leading element
        @param n_domain the functional domain of the class to be used to enforce functional restrictions within a
        Program object
        @param n_typeInfo the custom QoreTypeInfo object created with AbstractQoreClassTypeInfoHelper

        @see QoreProgram
        @see AbstractQoreClassTypeInfoHelper

        @since %Qore 1.0
    */
    DLLEXPORT QoreClass(const char* n_name, const char* ns_path, int64 n_domain, const QoreTypeInfo* n_typeInfo);

    //! copy constructor
    /** should be only called under the appropriate lock (ex: program parse lock while parsing)
    */
    DLLEXPORT QoreClass(const QoreClass& old);

    //! Called when a class is copied for import
    /** @since %Qore 0.9.5
    */
    DLLEXPORT virtual QoreClass* copyImport();

    //! Called when a class is copied
    /** @since %Qore 0.9.5
    */
    DLLEXPORT virtual QoreClass* copy();

    //! Returns the owning QoreProgram object (if not the static system namespace)
    /** @since Qore 0.9.5
    */
    DLLEXPORT QoreProgram* getProgram() const;

    //! adds a builtin method variant to a class
    /** @par Example:
        @code
        // the actual function can be declared with the class to be expected as the private data as follows:
        static QoreValue AL_lock(QoreObject* self, QoreAutoLock* m, const QoreListNode* args, RuntimeConfig& rc,
            ExceptionSink* xsink);
        ...
        // and then casted to (q_method_t) in the addMethod call:
        QC_AutoLock->addMethod("lock", (q_method_t)AL_lock, Public, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);
        @endcode

        in debuggging mode, the call will abort if the name of the method is
        "constructor", "destructor", or "copy", or if the method already exists
        in the class.
        To set the constructor method, call QoreClass::setConstructor().
        To set the destructor method, call QoreClass::setDestructor().
        To set the copy method, call QoreClass::setCopy().

        @param n_name the name of the method, must be unique in the class
        @param meth the method to be added
        @param access the access modifier for the method variant
        @param n_flags code flags
        @param n_domain functional domain
        @param returnTypeInfo the return type of the method
        @param num_params the number of parameters

        @see Qoreclass::addStaticMethod()
        @see QoreClass::setConstructor()
        @see QoreClass::setDestructor()
        @see QoreClass::setCopy()
    */
    DLLEXPORT void addMethod(const char* n_name, q_method_t meth, ClassAccess access = Public, int64 n_flags = QCF_NO_FLAGS, int64 n_domain = QDOM_DEFAULT, const QoreTypeInfo* returnTypeInfo = 0, unsigned num_params = 0, ...);

    //! adds a builtin method variant to a class with the calling convention for external modules
    /** @par Example:
        @code
        // the actual function can be declared with the class to be expected as the private data as follows:
        static QoreValue AL_lock(const QoreMethod& method, const void* ptr, QoreObject* self, QoreAutoLock* m,
            const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink)
        ...
        // and then casted to (q_method_t) in the addMethod call:
        QC_AutoLock->addMethod(nullptr, "lock", (q_external_method_t)AL_lock, Public, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);
        @endcode

        in debuggging mode, the call will abort if the name of the method is
        "constructor", "destructor", or "copy", or if the method already exists
        in the class.
        To set the constructor method, call QoreClass::addConstructor().
        To set the destructor method, call QoreClass::setDestructor().
        To set the copy method, call QoreClass::setCopy().

        @param ptr user-defined data that will be included in the call to \a meth
        @param n_name the name of the method, must be unique in the class
        @param meth the method to be added
        @param access the access modifier for the method variant
        @param n_flags code flags
        @param n_domain functional domain
        @param returnTypeInfo the return type of the method
        @param n_typeList a list of type information for parameters to the variant
        @param defaultArgList a list of default arguments to each parameter
        @param n_names a list of parameter names

        @see Qoreclass::addStaticMethod()
        @see QoreClass::addConstructor()
        @see QoreClass::setDestructor()
        @see QoreClass::setCopy()

        @since %Qore 0.8.13
    */
    DLLEXPORT void addMethod(const void* ptr, const char* n_name, q_external_method_t meth, ClassAccess access = Public, int64 n_flags = QCF_NO_FLAGS, int64 n_domain = QDOM_DEFAULT, const QoreTypeInfo* returnTypeInfo = 0, const type_vec_t& n_typeList = type_vec_t(), const arg_vec_t& defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t());

    //! adds a builtin static method with extended information; additional functional domain info, return and parameter type info
    DLLEXPORT void addStaticMethod(const char* n_name, q_func_t meth, ClassAccess access = Public, int64 n_flags = QCF_NO_FLAGS, int64 n_domain = QDOM_DEFAULT, const QoreTypeInfo* returnTypeInfo = 0, unsigned num_params = 0, ...);

    //! adds a builtin static method with extended information; additional functional domain info, return and parameter type info
    /** @since %Qore 0.9
    */
    DLLEXPORT void addStaticMethod(const void* ptr, const char* n_name, q_external_static_method_t meth, ClassAccess access = Public, int64 n_flags = QCF_NO_FLAGS, int64 n_domain = QDOM_DEFAULT, const QoreTypeInfo* returnTypeInfo = 0, const type_vec_t& n_typeList = type_vec_t(), const arg_vec_t& defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t());

    //! adds an unimplemented abstract method variant to the class with extended information; with return and parameter type info
    DLLEXPORT void addAbstractMethod(const char* n_name, ClassAccess access = Public, int64 n_flags = QCF_NO_FLAGS, const QoreTypeInfo* returnTypeInfo = 0, unsigned num_params = 0, ...);

    //! adds an unimplemented abstract method variant to the class with return and parameter type info
    DLLEXPORT void addAbstractMethod(const char* n_name, ClassAccess access, int64 n_flags, const QoreTypeInfo* returnTypeInfo, const type_vec_t& n_typeList, const arg_vec_t& defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t());

    //! sets the builtin destructor method for the class
    /** you only need to implement destructor methods if the destructor should destroy the object
        before the reference count reaches zero.
        @param m the destructor method to run
        @code
        // the actual function can be declared with the class to be expected as the private data as follows:
        static void AL_destructor(QoreObject* self, QoreAutoLock* al, RuntimeConfig& rc, ExceptionSink* xsink);
        ...
        // and then casted to (q_destructor_t) in the setDestructor call:
        QC_AutoLock->setDestructor((q_destructor_t)AL_destructor);
        @endcode
    */
    DLLEXPORT void setDestructor(q_destructor_t m);

    //! sets the builtin destructor method for the class with the external calling convention
    /** you only need to implement destructor methods if the destructor should destroy the object
        before the reference count reaches zero.
        @param ptr user-defined data that will be passed to the destructor when it's called
        @param m the destructor method to run
        @code
        // the actual function can be declared with the class to be expected as the private data as follows:
        static void AL_destructor(const QoreClass& thisclass, const void* ptr, QoreObject* self, QoreAutoLock* al, ExceptionSink* xsink);
        ...
        // and then casted to (q_external_destructor_t) in the setDestructor call:
        QC_AutoLock->setDestructor((q_external_destructor_t)AL_destructor);
        @endcode
    */
    DLLEXPORT void setDestructor(const void* ptr, q_external_destructor_t m);

    //! adds a constructor method variant with the access specifier, additional functional domain info, and parameter type info
    DLLEXPORT void addConstructor(q_constructor_t meth, ClassAccess access = Public, int64 n_flags = QCF_NO_FLAGS, int64 n_domain = QDOM_DEFAULT, unsigned num_params = 0, ...);

    //! adds a constructor method variant with the external calling convention and includes the access specifier, additional functional domain info, and parameter type info
    /** @since %Qore 0.9
    */
    DLLEXPORT void addConstructor(const void* ptr, q_external_constructor_t meth, ClassAccess access = Public, int64 n_flags = QCF_NO_FLAGS, int64 n_domain = QDOM_DEFAULT, const type_vec_t& n_typeList = type_vec_t(), const arg_vec_t& defaultArgList = arg_vec_t(), const name_vec_t& n_names = name_vec_t());

    //! sets the builtin constructor for system objects (ex: used as constant values)
    /** @note system constructors in a class hierarchy must call the base class constructors manually
        @param m the constructor method
    */
    DLLEXPORT void setSystemConstructor(q_system_constructor_t m);

    //! sets the builtin copy method for the class
    /** copy methods should either call QoreObject::setPrivate() or call xsink->raiseException()
        (but should not do both)
        @param m the copy method to set
        @code
        // the actual function can be declared with the class to be expected as the private data as follows:
        static void AL_copy(QoreObject* self, QoreObject* old, QoreAutoLock *m, RuntimeConfig& rc, ExceptionSink* xsink)
        ...
        // and then casted to (q_copy_t) in the addMethod call:
        QC_AutoLock->setCopy((q_copy_t)AL_copy);
        @endcode
    */
    DLLEXPORT void setCopy(q_copy_t m);

    //! sets the builtin copy method for the class using the new generic calling convention
    /** copy methods should either call QoreObject::setPrivate() or call xsink->raiseException()
        (but should not do both)
        @param ptr user-defined data that will be passed to the destructor when it's called
        @param m the copy method to set
        @code
        // the actual function can be declared with the class to be expected as the private data as follows:
        static void AL_copy(const QoreClass &thisclass, const void* ptr, QoreObject* self, QoreObject* old, QoreAutoLock *m, ExceptionSink* xsink)
        ...
        // and then casted to (q_external_copy_t) in the addMethod call:
        QC_AutoLock->setCopy((q_external_copy_t)AL_copy);
        @endcode
    */
    DLLEXPORT void setCopy(const void* ptr, q_external_copy_t m);

    //! sets the serializer method for builtin classes
    /** @param m the serializer method

        @since %Qore 0.9
    */
    DLLEXPORT void setSerializer(q_serializer_t m);

    //! sets the deserializer method for builtin classes
    /** @param m the deserializer method

        @since %Qore 0.9
    */
    DLLEXPORT void setDeserializer(q_deserializer_t m);

    //! returns the serializer method or nullptr if not present
    /** @since %Qore 0.9
    */
    DLLEXPORT q_serializer_t getSerializer() const;

    //! returns the deserializer method or nullptr if not present
    /** @since %Qore 0.9
    */
    DLLEXPORT q_deserializer_t getDeserializer() const;

    //! sets the final flag of the class
    DLLEXPORT void setFinal();

    //! returns true if the member is private
    /**
        @param str the member name to check
        @return true if the member is private
    */
    DLLEXPORT bool isPrivateMember(const char* str) const;

    //! returns true if the member is private or public
    /**
        @param str the member name to check
        @param priv true if the member is private, false if public
        @return true if the member is private
    */
    DLLEXPORT bool isPublicOrPrivateMember(const char* str, bool& priv) const;

    //! creates a new object and executes the constructor on it and returns the new object
    /** if a Qore-language exception occurs, 0 is returned.
        @param args the arguments for the method
        @param xsink Qore-language exception information is added here

        @return the object created
    */
    DLLEXPORT QoreObject* execConstructor(const QoreListNode* args, ExceptionSink* xsink) const;

    //! Creates a new object and executes the constructor and returns the new object
    /** The object created will be an instance of the first argument, which may be a different class than the current
        class.

        @param obj_cls the class for the object to be returned
        @param args the arguments for the method
        @param allow_abstract if construction of abstract classes is allowed; this should only be true when called
        from an external language module where the class has been inherited and all abstract methods have been
        overridden in the subclass in the other language, otherwise allowing an abstract class to be constructed
        will result in runtime errors / core dumps
        @param xsink Qore-language exception information is added here

        @since %Qore 0.9.5
    */
    DLLEXPORT QoreObject* execConstructor(const QoreClass& obj_cls, const QoreListNode* args, bool allow_abstract,
        ExceptionSink* xsink) const;

    //! creates a new object and executes the constructor on it and returns the new object
    /** if a Qore-language exception occurs, 0 is returned.
        @param mv the constructor variant to use; must belong to a constructor method of the current class
        @param args the arguments for the method
        @param xsink Qore-language exception information is added here

        @return the object created
    */
    DLLEXPORT QoreObject* execConstructorVariant(const QoreExternalMethodVariant* mv, const QoreListNode *args, ExceptionSink* xsink) const;

    //! creates a new "system" object for use as the value of a constant, executes the system constructor on it and returns the new object
    /** if a Qore-language exception occurs, 0 is returned
        @param code an optional code for the constructor; this parameter is here because passing a variable number of arguments requires at least one fixed parameter before the (possibly empty) list
        @return the object created
    */
    DLLEXPORT QoreObject* execSystemConstructor(int code = 0, ...) const;

    //! executes a class's "copy" method on an object and returns the new object (or 0 in the case of an exception)
    /** @param old the original object to copy
        @param xsink Qore-language exception information is added here
        @return the object created
    */
    DLLEXPORT QoreObject* execCopy(QoreObject* old, ExceptionSink* xsink) const;

    //! returns a list strings of all non-static methods in the class, the caller owns the reference count returned
    /** always returns a list; if there are no non-static methods then an empty list is returned
        @return a list strings of all non-static methods in the class, the caller owns the reference count returned
    */
    DLLEXPORT QoreListNode* getMethodList() const;

    //! returns a list strings of all static methods in the class, the caller owns the reference count returned
    /** always returns a list; if there are no static methods then an empty list is returned
        @return a list strings of all static methods in the class, the caller owns the reference count returned
    */
    DLLEXPORT QoreListNode* getStaticMethodList() const;

    //! returns a pointer to the QoreClass object representing the class ID passed if it exists in the class hierarchy
    /** if the class ID is equal to the current class or is a base class
        of the current class, the appropriate QoreClass pointer will be
        returned.  Do not delete or change the QoreClass* returned if
        non-null.

        @param cid the class ID of the QoreClass to find

        @return a pointer to the QoreClass object representing the class ID passed if it exists in the class hierarchy
    */
    DLLEXPORT const QoreClass* getClass(qore_classid_t cid) const;

    //! returns a pointer to the QoreClass object representing the class ID passed if it exists in the class hierarchy and sets a flag indicating if it's privately inherited or not
    /** if the class ID is equal to the current class or is a base class
        of the current class, the appropriate QoreClass pointer will be
        returned.

        @param cid the class ID of the QoreClass to find
        @param priv a flag indicating if the class is privately inherited or not

        @return a pointer to the QoreClass object representing the class ID passed if it exists in the class hierarchy
    */
    DLLEXPORT const QoreClass* getClass(qore_classid_t cid, bool& priv) const;

    //! returns a pointer to the QoreClass object representing the class ID passed if it exists in the class hierarchy and sets a flag indicating if it's privately inherited or not
    /** if the class is equal to the current class or is a base class
        of the current class, the appropriate QoreClass pointer will be
        returned.

        @param qc the class to check the hierarchy for
        @param priv a flag indicating if the class is privately inherited or not

        @return a pointer to the QoreClass object corresponding to the class passed if it exists in the class hierarchy; in the case that the passed class is from a different QoreProgram object, the value returned could be a different pointer to the qc parameter passed
    */
    DLLEXPORT const QoreClass* getClass(const QoreClass& qc, bool& priv) const;

    //! returns the number of non-static methods in this class (user and builtin)
    DLLEXPORT int numMethods() const;

    //! returns the number of static methods in this class (user and builtin)
    DLLEXPORT int numStaticMethods() const;

    //! returns the number of non-static user methods in this class
    DLLEXPORT int numUserMethods() const;

    //! returns the number of static user methods in this class
    DLLEXPORT int numStaticUserMethods() const;

    //! returns true if the class implements a copy method
    DLLEXPORT bool hasCopy() const;

    //! returns the class ID of this class
    DLLEXPORT qore_classid_t getID() const;

    //! returns true if the class is a builtin class
    DLLEXPORT bool isSystem() const;

    //! returns true if the class has its module public flag set
    /** @since %Qore 0.9
    */
    DLLEXPORT bool isModulePublic() const;

    //! returns true if the class has at least one unimplemented abstract method variant
    /** @since %Qore 0.9
    */
    DLLEXPORT bool isAbstract() const;

    //! returns true if the class is final
    /** @since %Qore 0.9
    */
    DLLEXPORT bool isFinal() const;

    //! returns true if the class has been injected as a dependency injection
    /** @since %Qore 0.9
    */
    DLLEXPORT bool isInjected() const;

    //! returns true if the class is a pseudo class
    /** @since %Qore 0.9
    */
    DLLEXPORT bool isPseudoClass() const;

    //! returns the class pointer for any injection target class if this class was injected, otherwise nullptr
    /** @return the class pointer for any injection target class if this class was injected, otherwise nullptr

        @since %Qore 1.0.3
    */
    DLLEXPORT QoreClass* getInjectedAsClass();

    //! returns the class pointer for any injection target class if this class was injected, otherwise nullptr
    /** @return the class pointer for any injection target class if this class was injected, otherwise nullptr

        @since %Qore 1.0.3
    */
    DLLEXPORT const QoreClass* getInjectedAsClass() const;

    //! returns a pseudo-classes base type
    /** if the class is not a pseudo-class, or is the default pseudo-class taking any value, then -1 is returned

        @since %Qore 0.9
    */
    DLLEXPORT qore_type_t getPseudoClassType() const;

    //! evaluates a pseudo-method on a pseudo-class
    /** The current class must be a pseudo-class or the call with abort in debug builds and crash in non-debug builds

        @since %Qore 0.9
    */
    DLLEXPORT QoreValue evalPseudoMethod(const QoreValue n, const char* nme, const QoreListNode* args, ExceptionSink* xsink) const;

    //! evaluates a pseudo-method on a pseudo-class
    /** The current class must be a pseudo-class or the call with abort in debug builds and crash in non-debug builds

        The method and variant arguments must belong to the class

        @param m the method to call; must be non-nullptr and must belong to the class
        @param variant may be nullptr meanin that the variant is matched in the call
        @param n the value to use for the call
        @param args call arguments, if any (may be nullptr)
        @param xsink Qore-language exception info is stored here

        @return the return value of the call

        @since %Qore 0.9
    */
    DLLEXPORT QoreValue evalPseudoMethod(const QoreMethod* m, const QoreExternalMethodVariant* variant, const QoreValue n, const QoreListNode* args, ExceptionSink* xsink) const;

    //! marks the class as a builtin class
    DLLEXPORT void setSystem();

    //! returns true if the class implements a "memberGate" method
    DLLEXPORT bool hasMemberGate() const;

    //! returns true if the class implements a "methodGate" method
    DLLEXPORT bool hasMethodGate() const;

    //! returns true if the class implements a "memberNotification" method
    DLLEXPORT bool hasMemberNotification() const;

    //! returns the functional domain of the class
    /** @since %Qore 0.9
    */
    DLLEXPORT int64 getDomain() const;

    //! returns the class name
    DLLEXPORT const char* getName() const;

    //! finds a normal (non-static) method in the class hierarchy
    /** @note used at run-time: will not return inaccessible methods

        @see findLocalMethod()
    */
    DLLEXPORT const QoreMethod* findMethod(const char* nme) const;

    //! finds a static method in the class hierarchy
    /** @note used at run-time: will not return inaccessible methods

        @see findLocalStaticMethod()
    */
    DLLEXPORT const QoreMethod* findStaticMethod(const char* nme) const;

    //! finds a normal (non-static) method in the class hierarchy at runtime and sets the access code
    /** @note used at run-time: will not return inaccessible methods

        @see findLocalMethod()
    */
    DLLEXPORT const QoreMethod* findMethod(const char* nme, ClassAccess& access) const;

    //! finds a static method in the class hierarchy and sets the priv flag if it's a private method or not
    /** @note used at run-time: will not return inaccessible methods

        @see findLocalStaticMethod()
    */
    DLLEXPORT const QoreMethod* findStaticMethod(const char* nme, ClassAccess& access) const;

    //! finds a normal (non-static) method in the class hierarchy
    /** @param name the name of the method

        @returns a pointer to the method found, or nullptr if no such method exists in the class
    */
    DLLEXPORT const QoreMethod* findLocalMethod(const char* name) const;

    //! finds a static method in the class hierarchy
    /** @param name the name of the method

        @returns a pointer to the method found, or nullptr if no such method exists in the class
    */
    DLLEXPORT const QoreMethod* findLocalStaticMethod(const char* name) const;

    //! make a builtin class a child of another builtin class
    /** the xargs argument must not be used; before qore supported function overloading, base class arguments could be given here
        @param qc the base class to add
    */
    DLLEXPORT void addBuiltinBaseClass(QoreClass* qc);

    //! make a builtin class a child of another builtin class and ensures that the given class's private data will be used in all class methods
    /** In the case this function is used, objects of this class cannot have
        private data saved against the class ID.
        @param qc the base class to add
    */
    DLLEXPORT void addDefaultBuiltinBaseClass(QoreClass* qc);

    //! sets "virtual" base class for a class, meaning that the base class data is appropriate for use in the subclass builtin methods
    /** this method adds a base class placeholder for a subclass - where the subclass's private data
        object is actually a subclass of the parent class and all methods are virtual, so the
        base class's constructor, destructor, and copy constructor will never be run and the base
        class methods will be passed a pointer to the subclass's data
        @param qc the base class to add
    */
    DLLEXPORT void addBuiltinVirtualBaseClass(QoreClass* qc);

    //! Adds a base class to the current class
    /** @param qc the class to add
        @param virt if the base class is "virtual", meaning that the current class's binary object is also compatible
        with this base class's data (meaning that the base class's private data object is also a base class of this
        class's private data object)
    */
    DLLEXPORT void addBaseClass(QoreClass* qc, bool virt = false);

    //! call this function if your builtin class requires *all* methods (except the constructor) to be run in an RMutex lock
    /** use this for classes that require exclusive access to the private data in all functions
    */
    DLLEXPORT void setSynchronousClass();

    //! returns a const pointer to the QoreMethod object of the constuctor method, if any is set
    /** executes in constant time
        @return a const pointer to the QoreMethod object of the constuctor method, if any is set
    */
    DLLEXPORT const QoreMethod* getConstructor() const;

    //! returns a const pointer to the QoreMethod object of the constuctor method, if any is set
    /** executes in constant time
        @return a const pointer to the QoreMethod object of the constuctor method, if any is set
    */
    DLLEXPORT const QoreMethod* getSystemConstructor() const;

    //! returns a const pointer to the QoreMethod object of the constructor method, if any is set
    /** executes in constant time
        @return a const pointer to the QoreMethod object of the constructor method, if any is set
    */
    DLLEXPORT const QoreMethod* getDestructor() const;

    //! returns a const pointer to the QoreMethod object of the destructor method, if any is set
    /** executes in constant time
        @return a const pointer to the QoreMethod object of the destructor method, if any is set
    */
    DLLEXPORT const QoreMethod* getCopyMethod() const;

    //! returns a const pointer to the QoreMethod object of the memberGate method, if any is set
    /** executes in constant time
        @return a const pointer to the QoreMethod object of the memberGate method, if any is set
    */
    DLLEXPORT const QoreMethod* getMemberGateMethod() const;

    //! returns a const pointer to the QoreMethod object of the methodGate method, if any is set
    /** executes in constant time
        @return a const pointer to the QoreMethod object of the methodGate method, if any is set
    */
    DLLEXPORT const QoreMethod* getMethodGate() const;

    //! returns a const pointer to the QoreMethod object of the memberNotification method, if any is set
    /** executes in constant time
        @return a const pointer to the QoreMethod object of the memberNotification method, if any is set
    */
    DLLEXPORT const QoreMethod* getMemberNotificationMethod() const;

    //! returns the type information structure for this class
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! returns the "or nothing" type information structure for this class
    DLLEXPORT const QoreTypeInfo* getOrNothingTypeInfo() const;

    //! adds a member
    DLLEXPORT void addMember(const char* mem, ClassAccess access, const QoreTypeInfo* n_typeInfo, QoreValue initial_value = QoreValue());

    //! sets a pointer to user-specific data in the class
    /** @deprecated use setManagedUserData(AbstractQoreClassUserData*) instead
    */
    DLLEXPORT void setUserData(const void* ptr);

    //! retrieves the user-specific data pointer
    /** @deprecated use getManagedUserData() instead
    */
    DLLEXPORT const void* getUserData() const;

    //! sets a pointer to user-specific data in the class
    /** @since %Qore 0.8.13
    */
    DLLEXPORT void setManagedUserData(AbstractQoreClassUserData* cud);

    //! retrieves the user-specific data pointer
    /** @since %Qore 0.8.13
    */
    DLLEXPORT AbstractQoreClassUserData* getManagedUserData() const;

    //! retrieves the user-specific data pointer
    /** @since %Qore 0.8.13
    */
    template <typename T>
    DLLLOCAL T* getManagedUserData() const {
        return static_cast<T*>(getManagedUserData());
    }

    //! rechecks for inherited methods in base classes when adding builtin classes
    DLLEXPORT void recheckBuiltinMethodHierarchy();

    //! returns the user variant for the given non-static method and argument types
    /** argTypeList must have a non-null value for each type in the list
    */
    DLLEXPORT const QoreExternalMethodVariant* findUserMethodVariant(const char* name, const QoreMethod*& method, const type_vec_t& argTypeList) const;

    //! adds a class constant to the class
    /** @param name the name of the constant; cannot be the same as the name of a static variable
        @param value the value of the constant
        @param access the access protection of the constant
        @param typeInfo only need to set if the value assigned to the constant is not an internally-defined type and therefore the type info cannot be automatically determined, otherwise this parameter may be passed as NULL, in which case the type info will be automatically determined
    */
    DLLEXPORT void addBuiltinConstant(const char* name, QoreValue value, ClassAccess access = Public, const QoreTypeInfo* typeInfo = nullptr);

    //! adds a static variable to the class
    /** @param name the name of the static variable; cannot be the same as the name of a class constant
        @param value the initial of the static variable
        @param access the access protection of the static variable
        @param typeInfo only need to set if the initial value assigned to the static variable (as given by the 'value' parameter) is not an internally-defined type and therefore the type info cannot be automatically determined, otherwise this parameter may be passed as NULL, in which case the type info will be automatically determined
    */
    DLLEXPORT void addBuiltinStaticVar(const char* name, QoreValue value, ClassAccess access = Public, const QoreTypeInfo* typeInfo = nullptr);

    //! rescan builtin parent classes in a class hierarchy; to be used with out-of-order class hierarchy construction
    /** For example, when Qore classes are generated externally such as with the jni module, parent class
        information may need to be rescanned after adding to the class hierarchy to ensure that all
        virtual parents are correctly marked in child classes

        @since %Qore 0.8.13
    */
    DLLEXPORT void rescanParents();

    //! sets the class's public member flag so that undeclared member references will fail
    /** @since %Qore 0.8.13
    */
    DLLEXPORT void setPublicMemberFlag();

    //! sets the class's gate access flag so that memberGate() and methodGate() methods will be called with an extra boolean argument giving the current class access before the call
    /** @since %Qore 0.8.13
    */
    DLLEXPORT void setGateAccessFlag();

    //! Sets the language for classes imported from another programming language
    /** @since %Qore 1.0.1
     */
    DLLEXPORT void setLanguage(const char* lang);

    //! Returns the language this class is written in
    /** @return \c Qore for Qore classe, \c Java for Java classes, and \c Python for Python classes

        @since %Qore 1.0.1
     */
    DLLEXPORT const char* getLanguage() const;

    //! Sets relaxed abstract matching
    /** For use with languages that do not support exact type mappings from Qore

        @since %Qore 1.0.1
     */
    DLLEXPORT void setRelaxedAbstractMatch();

    //! Returns the relaxed abstract matching flag
    /** For use with languages that do not support exact type mappings from Qore

        @return the relaxed abstract matching flag

        @since %Qore 1.0.1
     */
    DLLEXPORT bool getRelaxedAbstractMatch() const;

    //! Resolves abstract method variants against parent class implementations at runtime
    /** For use with classes created dynamically at runtime (e.g., JNI classes imported through Python)
        where abstract method resolution needs to happen after class creation.

        This method checks all abstract method variants against parent class implementations
        and removes abstract variants that have matching concrete implementations in parent classes.

        @since %Qore 2.2
     */
    DLLEXPORT void runtimeResolveAbstractMethods();

    //! Finds the given local member or returns nullptr
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalNormalMember* findLocalMember(const char* name) const;

    //! Finds the given local static member or returns nullptr
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalStaticMember* findLocalStaticMember(const char* name) const;

    //! returns the full namespace path of the class
    /** @param anchored if true then the path will always be prefixed by "::" for the unnamed root namespace

        @since %Qore 0.9
    */
    DLLEXPORT std::string getNamespacePath(bool anchored = false) const;

    //! Returns the root-justified namespace path of the class including the class name
    /** @note equivalent to getNamespacePath(true)

        @since %Qore 1.0
    */
    DLLEXPORT const char* getPath() const;

    //! returns true if the classes are equal
    /** @since %Qore 0.9
    */
    DLLEXPORT bool isEqual(const QoreClass& cls) const;

    //! returns a binary hash for the class's API
    /** @since %Qore 0.9
    */
    DLLEXPORT BinaryNode* getBinaryHash() const;

    //! Throws a Qore-language exception if the class cannot be instantiated
    /** @since %Qore 0.9
    */
    DLLEXPORT int runtimeCheckInstantiateClass(ExceptionSink* xsink) const;

    //! Finds the given constant or returns nullptr if not found
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalConstant* findConstant(const char* name) const;

    //! Returns the namespace that owns this class
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreNamespace* getNamespace() const;

    //! Returns true if the class passed as an argument is present in the current class's hierachy, even if not accessible from the class due to private:internal inheritance
    /** @since %Qore 0.9
    */
    DLLEXPORT bool inHierarchy(const QoreClass& cls, ClassAccess& n_access) const;

    //! Returns true if the class passed as an argument is present in the current class's hierachy, even if not accessible from the class due to private:internal inheritance; does not check injected compatibility
    /** @since %Qore 1.0.3
    */
    DLLEXPORT bool inHierarchyStrict(const QoreClass& cls, ClassAccess& n_access) const;

    //! Returns true if the class has at least one locally-declared transient member
    /** @since %Qore 0.9
    */
    DLLEXPORT bool hasTransientMember() const;

    //! Returns the module name the class was loaded from or nullptr if it is a builtin class
    /** @since %Qore 0.9
    */
    DLLEXPORT const char* getModuleName() const;

    //! Sets a key value in the class's key-value store unconditionally
    /** @param key the key to store
        @param val the value to store; must be already referenced for storage

        @return any value previously stored in that key; must be dereferenced by the caller

        @note All class key-value operations are atomic

        @since %Qore 1.0
    */
    DLLEXPORT QoreValue setKeyValue(const std::string& key, QoreValue val);

    //! Sets a key value in the class's key-value store only if no value exists for the given key
    /** @param key the key to store
        @param val the value to store; must be already referenced for storage

        @return returns \a value if another value already exists for that key, otherwise returns no value

        @note
        - All class key-value operations are atomic
        - if \a value is returned, the caller must dereference it

        @since %Qore 1.0
    */
    DLLEXPORT QoreValue setKeyValueIfNotSet(const std::string& key, QoreValue val);

    //! Sets a key value in the class's key-value store only if no value exists for the given key
    /** @param key the key to store
        @param str the string to store; will be converted to a QoreStringNode if stored

        @return true if the value was set, false if not (a value is already in place)

        @note All class key-value operations are atomic

        @since %Qore 1.0
    */
    DLLEXPORT bool setKeyValueIfNotSet(const std::string& key, const char* str);

    //! Returns a referenced key value from the class's key-value store
    /** @param key the key to check

        @return the value corresponding to the key; the caller is responsible for dereferencing the value returned

        @note All class key-value operations are atomic

        @since %Qore 1.0
    */
    DLLEXPORT QoreValue getReferencedKeyValue(const std::string& key) const;

    //! Returns a referenced key value from the class's key-value store
    /** @param key the key to check

        @return the value corresponding to the key; the caller is responsible for dereferencing the value returned

        @note All class key-value operations are atomic

        @since %Qore 1.0.13
    */
    DLLEXPORT QoreValue getReferencedKeyValue(const char* key) const;

    //! returns true if the class has one or more parent classes
    DLLEXPORT bool hasParentClass() const;

    //! returns true if the class has any publicly-declared members
    DLLEXPORT bool hasPublicMembersInHierarchy() const;

    //! Returns an expression that can be used to create a new object when executed
    /** @param args the arguments to the call; may be nullptr; the call takes over ownership of the reference
    */
    DLLEXPORT AbstractQoreNode* getNewObjectExpression(QoreListNode* args = nullptr) const;

protected:
    //! Deletes the object and frees all memory
    DLLEXPORT virtual ~QoreClass();

    //! For use with QoreClass::copyImport()
    DLLEXPORT QoreClass();

private:
    QoreClass& operator=(const QoreClass&) = delete;

    //! private implementation of the class
    class qore_class_private* priv;
};

//! To be used to iterate through a class's normal (non-static) methods
class QoreMethodIterator {
private:
    void* priv;

public:
    DLLEXPORT QoreMethodIterator(const QoreClass& qc);
    DLLEXPORT ~QoreMethodIterator();
    DLLEXPORT bool next();
    DLLEXPORT const QoreMethod* getMethod() const;
};

//! To be used to iterate through a class's static methods
class QoreStaticMethodIterator {
private:
    void* priv;

public:
    DLLEXPORT QoreStaticMethodIterator(const QoreClass& qc);
    DLLEXPORT ~QoreStaticMethodIterator();
    DLLEXPORT bool next();
    DLLEXPORT const QoreMethod* getMethod() const;
};

//! allows for temporary storage of a QoreClass pointer
/** @since %Qore 0.8.13
 */
class QoreClassHolder {
public:
    //! creates the object
    DLLLOCAL QoreClassHolder(QoreClass* c) : c(c) {
    }

    //! deletes the QoreClass object if still managed
    DLLEXPORT ~QoreClassHolder();

    //! implicit conversion to QoreClass*
    DLLLOCAL operator QoreClass*() const {
        return c;
    }

    //! releases the QoreClass*
    DLLLOCAL QoreClass* release() {
        auto rv = c;
        c = 0;
        return rv;
    }

private:
    //! the object being managed
    QoreClass* c;
};

//! creates a builtin class
class QoreBuiltinClass : public QoreClass {
public:
    //! Creates the object and marks it as a builtin class
    /** Also marks the source program for the class, however the source program's reference count is not increased
        in this call, and in the destructor no dereference is made either

        @since %Qore 1.0
    */
    DLLEXPORT QoreBuiltinClass(QoreProgram* pgm, const char* name, const char* path, int64 n_domain = QDOM_DEFAULT);

    //! creates the object and marks it as a builtin class
    DLLEXPORT QoreBuiltinClass(const char* name, const char* path, int64 n_domain = QDOM_DEFAULT);

    //! copies the object
    DLLEXPORT QoreBuiltinClass(const QoreBuiltinClass& old);

    //! Sets the module public flag (default true)
    DLLEXPORT void setModulePublic(bool v);

protected:
    //! for use with QoreClass::copyImport()
    DLLEXPORT QoreBuiltinClass();
};

//! iterates parent classes for a class with inheritance access information
/** @since %Qore 0.9
*/
class QoreParentClassIterator final {
public:
    //! creates the iterator; call next() to start iterating
    DLLEXPORT QoreParentClassIterator(const QoreClass& cls);

    //! destroys the object
    DLLEXPORT ~QoreParentClassIterator();

    //! returns advances to the next element (or to the first element if starting to iterate) and returns true if there is an element to query or returns false if at the end of the list
    DLLEXPORT bool next();

    //! returns true if the iterator is pointing at a valid element
    DLLEXPORT bool valid() const;

    //! returns the parent class
    DLLEXPORT const QoreClass& getParentClass() const;

    //! returns the access of the parent class
    DLLEXPORT ClassAccess getAccess() const;

private:
    std::unique_ptr<class qore_parent_class_iterator_private> priv;
};

//! iterates normal (non-static) members of a class
/** @since %Qore 0.9
*/
class QoreClassMemberIterator final {
public:
    //! creates the iterator; call next() to start iterating
    DLLEXPORT QoreClassMemberIterator(const QoreClass& cls);

    //! destroys the object
    DLLEXPORT ~QoreClassMemberIterator();

    //! returns advances to the next element (or to the first element if starting to iterate) and returns true if there is an element to query or returns false if at the end of the list
    DLLEXPORT bool next();

    //! returns true if the iterator is pointing at a valid element
    DLLEXPORT bool valid() const;

    //! returns the member
    DLLEXPORT const QoreExternalNormalMember& getMember() const;

    //! returns the member's name
    DLLEXPORT const char* getName() const;

private:
    std::unique_ptr<class qore_class_member_iterator_private> priv;
};

//! iterates static members of a class
/** @since %Qore 0.9
*/
class QoreClassStaticMemberIterator final {
public:
    //! creates the iterator; call next() to start iterating
    DLLEXPORT QoreClassStaticMemberIterator(const QoreClass& cls);

    //! destroys the object
    DLLEXPORT ~QoreClassStaticMemberIterator();

    //! returns advances to the next element (or to the first element if starting to iterate) and returns true if there is an element to query or returns false if at the end of the list
    DLLEXPORT bool next();

    //! returns true if the iterator is pointing at a valid element
    DLLEXPORT bool valid() const;

    //! returns the member
    DLLEXPORT const QoreExternalStaticMember& getMember() const;

    //! returns the member's name
    DLLEXPORT const char* getName() const;

private:
    std::unique_ptr<class qore_class_static_member_iterator_private> priv;
};

//! iterates class constants
/** @since %Qore 0.9
*/
class QoreClassConstantIterator final {
public:
    //! creates the iterator; call next() to start iterating
    DLLEXPORT QoreClassConstantIterator(const QoreClass& cls);

    //! destroys the object
    DLLEXPORT ~QoreClassConstantIterator();

    //! returns advances to the next element (or to the first element if starting to iterate) and returns true if there is an element to query or returns false if at the end of the list
    DLLEXPORT bool next();

    //! returns true if the iterator is pointing at a valid element
    DLLEXPORT bool valid() const;

    //! returns the
    DLLEXPORT const QoreExternalConstant& get() const;

private:
    std::unique_ptr<class qore_class_constant_iterator> priv;
};

//! iterates the class hierarchy in the order of constructor execution
/** @see QoreClassDestructorHierarchyIterator

    @since %Qore 0.9
*/
class QoreClassHierarchyIterator final {
public:
    //! creates the iterator; call next() to start iterating
    DLLEXPORT QoreClassHierarchyIterator(const QoreClass& cls);

    //! destroys the object
    DLLEXPORT ~QoreClassHierarchyIterator();

    //! returns advances to the next element (or to the first element if starting to iterate) and returns true if there is an element to query or returns false if at the end of the list
    DLLEXPORT bool next();

    //! returns true if the iterator is pointing at a valid element
    DLLEXPORT bool valid() const;

    //! returns the parent class
    DLLEXPORT const QoreClass& get() const;

    //! returns true if the class has virtual inheritance, meaning that it is a builtin class without its own private data
    /** if true, compatible private data is supplied by a child class
    */
    DLLEXPORT bool isVirtual() const;

private:
    std::unique_ptr<class qore_class_hierarchy_iterator> priv;
};

//! iterates the class hierarchy in the order of destructor execution
/** @see QoreClassHierarchyIterator

    @since %Qore 0.9
*/
class QoreClassDestructorHierarchyIterator {
public:
    //! creates the iterator; call next() to start iterating
    DLLEXPORT QoreClassDestructorHierarchyIterator(const QoreClass* cls);

    //! destroys the object
    DLLEXPORT ~QoreClassDestructorHierarchyIterator();

    //! returns advances to the next element (or to the first element if starting to iterate) and returns true if there is an element to query or returns false if at the end of the list
    DLLEXPORT bool next();

    //! returns true if the iterator is pointing at a valid element
    DLLEXPORT bool valid() const;

    //! returns the parent class
    DLLEXPORT const QoreClass* get() const;

    //! returns true if the class has virtual inheritance, meaning that it is a builtin class without its own private data
    /** if true, compatible private data is supplied by a child class
    */
    DLLEXPORT bool isVirtual() const;

private:
    class qore_class_destructor_hierarchy_iterator* priv;
};

DLLEXPORT const char* get_access_string(ClassAccess access);

#endif // _QORE_QORECLASS_H
