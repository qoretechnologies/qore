/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreNamespace.h

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

/*
  namespaces are children of a program object.  there is a parse
  lock per program object to ensure that objects are added (or backed out)
  atomically per program object.  All the objects referenced here should
  be safe to read & copied at all times.  They will only be deleted when the
  program object is deleted (except the pending structures, which will be
  deleted any time there is a parse error, together with all other
  pending structures)
*/

#ifndef _QORE_QORENAMESPACE_H

#define _QORE_QORENAMESPACE_H

#include <cstdlib>
#include <cstring>
#include <string>

// forward declarations
class QoreExternalFunction;
class QoreExternalConstant;
class QoreExternalGlobalVar;
class QoreProgram;
class QoreEnumDecl;

//! namespace class handler function type
/** called when a class cannot be found in the namespace
    @param ns namespace pointer
    @param cname unqualified class name to load
    @return 0=class cannot be loaded and mapped, otherwise class pointer from new QoreClass added to ns
 */
typedef QoreClass* (*q_ns_class_handler_t)(QoreNamespace* ns, const char* cname);

//! contains constants, classes, and subnamespaces in QoreProgram objects
class QoreNamespace {
    friend class QoreNamespaceList;
    friend class RootQoreNamespace;
    friend class qore_ns_private;
    friend class qore_root_ns_private;
    friend struct NSOInfoBase;

public:
    //! creates a namespace with the given name
    /** the name of a subnamespace must be unique in the parent namespace and must not have the same name as a class in the parent namespace either
        @param n the name of the namespace
    */
    DLLEXPORT QoreNamespace(const char* n);

    //! copies the namespace and assigns new parse options to the new namespace
    /** @param old the old namespace to copy
        @param po new parse options for the new namespace

        @since %Qore 1.0
    */
    DLLEXPORT QoreNamespace(const QoreNamespace& old, int64 po);

    //! destroys the object and frees memory
    DLLEXPORT virtual ~QoreNamespace();

    //! clears the contents of the namespace before deleting
    /** use if the namespace could contain objects

        @since %Qore 0.8.13
    */
    DLLEXPORT void clear(ExceptionSink* xsink);

    //! adds a constant definition to the namespace
    /** use addConstant(const char* name, QoreValue value, const QoreTypeInfo* typeInfo) when adding
        constants of externally-defined base (non-class) types; all other types (and all objects) can have
        their type information automatically added
        @param name the name of the constant to add
        @param value the value of the constant
    */
    DLLEXPORT void addConstant(const char* name, QoreValue value);

    //! adds a constant definition to the namespace with type information
    /**
        @param name the name of the constant to add
        @param value the value of the constant
        @param typeInfo the type of the constant
        @see QoreTypeInfoHelper
    */
    DLLEXPORT void addConstant(const char* name, QoreValue value, const QoreTypeInfo* typeInfo);

    //! adds a class to a namespace
    /**
        @param oc the class to add to the namespace
    */
    DLLEXPORT void addSystemClass(QoreClass* oc);

    //! adds a hashdecl to a namespace
    /**
        @param hashdecl the hashdecl to add to the namespace

        @since %Qore 0.8.13
    */
    DLLEXPORT void addSystemHashDecl(TypedHashDecl* hashdecl);

    //! adds an enum declaration to a namespace
    /**
        @param enumdecl the enum declaration to add to the namespace
    */
    DLLEXPORT void addSystemEnum(QoreEnumDecl* enumdecl);

    //! returns a deep copy of the namespace
    /** @param po parse options to use when copying the namespace
        @return a deep copy of the namespace
    */
    DLLEXPORT QoreNamespace* copy(int64 po = PO_DEFAULT) const;

    //! gets a hash of all classes in the namespace, the hash keys are the class names and the values are lists of strings giving the method names
    /**
        @see QoreHashNode
        @see QoreListNode
        @return a hash of all classes in the namespace, the hash keys are the class names and the values are lists of strings giving the method names
    */
    DLLEXPORT QoreHashNode* getClassInfo() const;

    //! a hash of all constants in the namespace, the hash keys are the constant names and the values are the values of the constants
    /**
        @see QoreHashNode
        @see QoreListNode
        @return a hash of all constants in the namespace, the hash keys are the constant names and the values are the values of the constants
    */
    DLLEXPORT QoreHashNode* getConstantInfo() const;

    //! returns a hash giving information about the definitions in the namespace
    /** the return value has the following keys: "constants", "classes", and "subnamespaces"
        having as values the result of calling QoreNamespace::getConstantInfo(),
        QoreNamespace::getClassInfo(), and a hash of subnamespace names having as values
        the result of calling this function on each, respectively.
        @return a hash giving information about the definitions in the namespace
    */
    DLLEXPORT QoreHashNode* getInfo() const;

    //! returns the name of the namespace
    /**
        @return the name of the namespace
    */
    DLLEXPORT const char* getName() const;

    //! adds a namespace to the namespace tree
    /** the namespace must be unique, must also not clash with a class name in the same parent namespace
        @param ns the namespace to add, memory is now owned by parent namespace
    */
    DLLEXPORT void addNamespace(QoreNamespace* ns);

    //! adds a subnamespace to the namespace
    /** use this function when the QoreNamespace can be added directly to the tree
        (does not need to be merged with another namespace of the same name and does
        not contain user code)
        @param ns the subnamespace to add to the namespace
    */
    DLLEXPORT void addInitialNamespace(QoreNamespace* ns);

    //! finds a Namespace based on the argument; creates it (or the whole path) if necessary
    /** can only be called in the parse lock

        @param nspath must be a complete path ("ns1::ns2[::ns3...]" to a namespace, which will be found or created in
        this namespace; the last element of the path is ignored

        @return the namespace found or created according to the path

        @note
        - namespaces are created private by default
        - the last element of the path is assumed to be the new symbol to be added and is ignored when creating the
          namespaces; use @ref findCreateNamespacePathAll() to create namespaces for all elements of the path
    */
    DLLEXPORT QoreNamespace* findCreateNamespacePath(const char* nspath);

    //! finds a Namespace based on the argument; creates it (or the whole path) if necessary
    /** can only be called in the parse lock

        @param nspath must be a complete path ("ns1::ns2[::ns3...]" to a namespace, which will be found or created in
        this namespace; the last element of the path is not ignored and the final namespace is created with this name

        @return the namespace found or created according to the path

        @note
        - namespaces are created private by default

        @since %Qore 0.9.5
    */
    DLLEXPORT QoreNamespace* findCreateNamespacePathAll(const char* nspath);

    //! finds a class in this namespace, does not search child namespaces
    /** can only be called in the parse lock; does not call the class handler

        @param cname the class name to find in this namespace, must be unqualified (without a namespace path)

        @return the class found or 0 if not present
    */
    DLLEXPORT QoreClass* findLocalClass(const char* cname) const;

    //! finds a class in this namespace, does not search child namespaces
    /** can only be called in the parse lock; does call the class handler

        @param cname the class name to find in this namespace, must be unqualified (without a namespace path)

        @return the class found or 0 if not present

        @since %Qore 0.9.5
    */
    DLLEXPORT QoreClass* findLoadLocalClass(const char* cname);

    //! finds a subnamespace in this namespace, does not search child namespaces
    /** can only be called in the parse lock
        @param nsname the subnamespace name to find in this namespace, must be unqualified (without a namespace path)
        @return the namespace found or 0 if not present
    */
    DLLEXPORT QoreNamespace* findLocalNamespace(const char* nsname) const;

    //! sets the namespace class handler
    /** to be called when a class cannot be found in the namespace
        @param class_handler pointer to the class handler function
    */
    DLLEXPORT void setClassHandler(q_ns_class_handler_t class_handler);

    //! returns a pointer to the parent namespace or nullptr if there is no parent
    /** @return a pointer to the parent namespace or nullptr if there is no parent
    */
    DLLEXPORT const QoreNamespace* getParent() const;

    //! this function must be called before the QoreNamespace object is deleted or a crash could result due if constants and/or class static vars contain objects
    DLLEXPORT void deleteData(ExceptionSink* xsink);

    //! adds a function variant
    DLLEXPORT void addBuiltinVariant(const char* name, q_func_t f, int64 code_flags = QCF_NO_FLAGS, int64 functional_domain = QDOM_DEFAULT, const QoreTypeInfo* returnTypeInfo = 0, unsigned num_params = 0, ...);

    //! adds a function variant
    /** @since %Qore 0.9.5
    */
    DLLEXPORT void addBuiltinVariant(void* ptr, const char* name, q_external_func_t f,
        int64 code_flags = QCF_NO_FLAGS, int64 functional_domain = QDOM_DEFAULT,
        const QoreTypeInfo* returnTypeInfo = nullptr, unsigned num_params = 0, ...);

    //! find a function in the current namespace; returns nullptr if not found
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalFunction* findLocalFunction(const char* name) const;

    //! find a constant in the current namespace; returns nullptr if not found
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalConstant* findLocalConstant(const char* name) const;

    //! find a global variable in the current namespace; returns nullptr if not found
    /** @since %Qore 0.9
    */
    DLLEXPORT const QoreExternalGlobalVar* findLocalGlobalVar(const char* name) const;

    //! find a typed hash (hashdecl) in the current namespace; returns nullptr if not found
    /** @since %Qore 0.9
    */
    DLLEXPORT const TypedHashDecl* findLocalTypedHash(const char* name) const;

    //! find an enum in the current namespace; returns nullptr if not found
    /** @since %Qore 2.1
    */
    DLLEXPORT const QoreEnumDecl* findLocalEnum(const char* name) const;

    //! returns the path for the namespace
    /** @param anchored if true then the namespace will be prefixed with "::" for the unnamed root namespace

        @since %Qore 0.9
    */
    DLLEXPORT std::string getPath(bool anchored = false) const;

    //! returns true if the namespace has its module public flag set
    /** @since %Qore 0.9
    */
    DLLEXPORT bool isModulePublic() const;

    //! returns true if the namespace is builtin
    /** @since %Qore 0.9
    */
    DLLEXPORT bool isBuiltin() const;

    //! returns true if the namespace was imported from another program object
    /** @since %Qore 0.9
    */
    DLLEXPORT bool isImported() const;

    //! returns true if the namespace is the root namespace
    /** @since %Qore 0.9
    */
    DLLEXPORT bool isRoot() const;

    //! Returns the owning QoreProgram object (if not the static system namespace)
    /** @since Qore 0.9.5
    */
    DLLEXPORT QoreProgram* getProgram() const;

    //! Returns the module name the namespace was loaded from or nullptr if it is a builtin namespace
    /** @since %Qore 0.9.5
    */
    DLLEXPORT const char* getModuleName() const;

    //! Sets a key value in the namespace's key-value store unconditionally
    /** @param key the key to store
        @param val the value to store; must be already referenced for storage

        @return any value previously stored in that key; must be dereferenced by the caller

        @note All namespace key-value operations are atomic

        @since %Qore 1.0.13
    */
    DLLEXPORT QoreValue setKeyValue(const std::string& key, QoreValue val);

    //! Sets a key value in the namespace's key-value store only if no value exists for the given key
    /** @param key the key to store
        @param val the value to store; must be already referenced for storage

        @return returns \a value if another value already exists for that key, otherwise returns no value

        @note
        - All namespace key-value operations are atomic
        - if \a value is returned, the caller should dereference it if necessary

        @since %Qore 1.0.13
    */
    DLLEXPORT QoreValue setKeyValueIfNotSet(const std::string& key, QoreValue val);

    //! Sets a key value in the namespace's key-value store only if no value exists for the given key
    /** @param key the key to store
        @param str the string to store; will be converted to a QoreStringNode if stored

        @return true if the value was set, false if not (a value is already in place)

        @note All namespace key-value operations are atomic

        @since %Qore 1.0.13
    */
    DLLEXPORT bool setKeyValueIfNotSet(const std::string& key, const char* str);

    //! Returns a referenced key value from the namespace's key-value store
    /** @param key the key to check

        @return the value corersponding to the key; the caller is responsible for dereferencing the value returned

        @note All namespace key-value operations are atomic

        @since %Qore 1.0.13
    */
    DLLEXPORT QoreValue getReferencedKeyValue(const std::string& key) const;

    //! Returns a referenced key value from the namespace's key-value store
    /** @param key the key to check

        @return the value corersponding to the key; the caller is responsible for dereferencing the value returned

        @note All namespace key-value operations are atomic

        @since %Qore 1.0.13
    */
    DLLEXPORT QoreValue getReferencedKeyValue(const char* key) const;

private:
    //! this function is not implemented
    QoreNamespace(const QoreNamespace&) = delete;

    //! this function is not implemented
    QoreNamespace& operator=(const QoreNamespace&) = delete;

protected:
    class qore_ns_private* priv; // private implementation

    // protected, function not exported in the API
    DLLLOCAL QoreNamespace(qore_ns_private* p);
};

//! the root namespace of a QoreProgram object
/** is a specialization of QoreNamespace that provides functionality specific to the root namespace
    this class' constructor and destructors are private, so the class may change without affecting the library's ABI
    @see QoreNamespace
*/
class RootQoreNamespace : public QoreNamespace {
    friend class qore_ns_private;
    friend class qore_root_ns_private;
    friend class StaticSystemNamespace;

public:
    //! returns a pointer to the QoreNamespace for the "Qore" namespace
    /**
        @return a pointer to the QoreNamespace for the "Qore" namespace; do not delete the object returned
    */
    DLLEXPORT QoreNamespace* rootGetQoreNamespace() const;

    //! destructor is not exported in the library's public API
    DLLLOCAL virtual ~RootQoreNamespace();

    //! Returns the owning QoreProgram object (if not the static system namespace)
    /** @since Qore 0.9.5
    */
    DLLEXPORT QoreProgram* getProgram() const;

protected:
    // private implementation
    class qore_root_ns_private* rpriv;

private:
    //! this function is not implemented
    RootQoreNamespace(const RootQoreNamespace&) = delete;

    //! this function is not implemented
    RootQoreNamespace& operator=(const RootQoreNamespace&) = delete;

    DLLLOCAL RootQoreNamespace(class qore_root_ns_private* p);
};

class QorePrivateNamespaceIterator;

//! allows all namespaces of a namespace to be iterated (including the namespace passed in the constructor)
/** @since %Qore 0.8.13
 */
class QoreNamespaceIterator {
public:
    //! creates the iterator; the namespace given will also be included in the iteration set
    DLLEXPORT QoreNamespaceIterator(QoreNamespace& ns);

    //! destroys the object
    DLLEXPORT virtual ~QoreNamespaceIterator();

    //! moves to the next position; returns true if on a valid position
    DLLEXPORT bool next();

    //! returns the namespace
    DLLEXPORT QoreNamespace* operator->();

    //! returns the namespace
    DLLEXPORT QoreNamespace* operator*();

    //! returns the namespace
    DLLEXPORT QoreNamespace& get();

    //! returns the namespace
    DLLEXPORT const QoreNamespace* operator->() const;
    //! returns the namespace
    DLLEXPORT const QoreNamespace* operator*() const;

    //! returns the namespace
    DLLEXPORT const QoreNamespace& get() const;

private:
    //! this function is not implemented
    QoreNamespaceIterator(const QoreNamespaceIterator&) = delete;

    //! this function is not implemented
    QoreNamespaceIterator& operator=(const QoreNamespaceIterator&) = delete;

    QorePrivateNamespaceIterator* priv;
};

//! allows all namespaces of a namespace to be iterated (including the namespace passed in the constructor)
/** @since %Qore 0.8.13
 */
class QoreNamespaceConstIterator {
public:
    //! creates the iterator; the namespace given will also be included in the iteration set
    DLLEXPORT QoreNamespaceConstIterator(const QoreNamespace& ns);

    //! destroys the object
    DLLEXPORT virtual ~QoreNamespaceConstIterator();

    //! moves to the next position; returns true if on a valid position
    DLLEXPORT bool next();

    //! returns the namespace
    DLLEXPORT const QoreNamespace* operator->() const;

    //! returns the namespace
    DLLEXPORT const QoreNamespace* operator*() const;

    //! returns the namespace
    DLLEXPORT const QoreNamespace& get() const;

private:
    //! this function is not implemented
    QoreNamespaceConstIterator(const QoreNamespaceConstIterator&) = delete;

    //! this function is not implemented
    QoreNamespaceConstIterator& operator=(const QoreNamespaceConstIterator&) = delete;

    QorePrivateNamespaceIterator* priv;
};

//! allows local namespaces to be iterated
/** @since %Qore 0.8.13
 */
class QoreNamespaceNamespaceIterator {
public:
    //! creates the iterator
    DLLEXPORT QoreNamespaceNamespaceIterator(const QoreNamespace& ns);

    //! destroys the object
    DLLEXPORT virtual ~QoreNamespaceNamespaceIterator();

    //! moves to the next position; returns true if on a valid position
    DLLEXPORT bool next();

    //! returns the namespace
    DLLEXPORT const QoreNamespace& get() const;

private:
    //! this function is not implemented
    QoreNamespaceNamespaceIterator(const QoreNamespaceNamespaceIterator&) = delete;

    //! this function is not implemented
    QoreNamespaceNamespaceIterator& operator=(const QoreNamespaceNamespaceIterator&) = delete;

    class qore_namespace_namespace_iterator* priv;
};

//! allows functions in a namespace to be iterated
/** @since %Qore 0.9
 */
class QoreNamespaceFunctionIterator {
public:
    //! creates the iterator
    DLLEXPORT QoreNamespaceFunctionIterator(const QoreNamespace& ns);

    //! destroys the object
    DLLEXPORT virtual ~QoreNamespaceFunctionIterator();

    //! moves to the next position; returns true if on a valid position
    DLLEXPORT bool next();

    //! returns the function
    DLLEXPORT const QoreExternalFunction& get() const;

private:
    //! this function is not implemented
    QoreNamespaceFunctionIterator(const QoreNamespaceFunctionIterator&) = delete;

    //! this function is not implemented
    QoreNamespaceFunctionIterator& operator=(const QoreNamespaceFunctionIterator&) = delete;

    class qore_namespace_function_iterator* priv;
};

//! allows constants in a namespace to be iterated
/** @since %Qore 0.9
 */
class QoreNamespaceConstantIterator {
public:
    //! creates the iterator
    DLLEXPORT QoreNamespaceConstantIterator(const QoreNamespace& ns);

    //! destroys the object
    DLLEXPORT virtual ~QoreNamespaceConstantIterator();

    //! moves to the next position; returns true if on a valid position
    DLLEXPORT bool next();

    //! returns the constant
    DLLEXPORT const QoreExternalConstant& get() const;

private:
    //! this function is not implemented
    QoreNamespaceConstantIterator(const QoreNamespaceConstantIterator&) = delete;

    //! this function is not implemented
    QoreNamespaceConstantIterator& operator=(const QoreNamespaceConstantIterator&) = delete;

    class qore_namespace_constant_iterator* priv;
};

//! allows classes in a namespace to be iterated
/** @since %Qore 0.9
 */
class QoreNamespaceClassIterator {
public:
    //! creates the iterator
    DLLEXPORT QoreNamespaceClassIterator(const QoreNamespace& ns);

    //! destroys the object
    DLLEXPORT virtual ~QoreNamespaceClassIterator();

    //! moves to the next position; returns true if on a valid position
    DLLEXPORT bool next();

    //! returns the class
    DLLEXPORT const QoreClass& get() const;

private:
    //! this function is not implemented
    QoreNamespaceClassIterator(const QoreNamespaceClassIterator&) = delete;

    //! this function is not implemented
    QoreNamespaceClassIterator& operator=(const QoreNamespaceClassIterator&) = delete;

    class ConstClassListIterator* priv;
};

//! allows global variables in a namespace to be iterated
/** @since %Qore 0.9
 */
class QoreNamespaceGlobalVarIterator {
public:
    //! creates the iterator
    DLLEXPORT QoreNamespaceGlobalVarIterator(const QoreNamespace& ns);

    //! destroys the object
    DLLEXPORT virtual ~QoreNamespaceGlobalVarIterator();

    //! moves to the next position; returns true if on a valid position
    DLLEXPORT bool next();

    //! returns the global variable
    DLLEXPORT const QoreExternalGlobalVar& get() const;

private:
    //! this function is not implemented
    QoreNamespaceGlobalVarIterator(const QoreNamespaceGlobalVarIterator&) = delete;

    //! this function is not implemented
    QoreNamespaceGlobalVarIterator& operator=(const QoreNamespaceGlobalVarIterator&) = delete;

    class qore_namespace_globalvar_iterator* priv;
};

//! allows typed hashes (hashdecls) in a namespace to be iterated
/** @since %Qore 0.9
 */
class QoreNamespaceTypedHashIterator {
public:
    //! creates the iterator
    DLLEXPORT QoreNamespaceTypedHashIterator(const QoreNamespace& ns);

    //! destroys the object
    DLLEXPORT virtual ~QoreNamespaceTypedHashIterator();

    //! moves to the next position; returns true if on a valid position
    DLLEXPORT bool next();

    //! returns the typed hash (hashdecl)
    DLLEXPORT const TypedHashDecl& get() const;

private:
    //! this function is not implemented
    QoreNamespaceTypedHashIterator(const QoreNamespaceTypedHashIterator&) = delete;

    //! this function is not implemented
    QoreNamespaceTypedHashIterator& operator=(const QoreNamespaceTypedHashIterator&) = delete;

    class ConstHashDeclListIterator* priv;
};

//! allows enums in a namespace to be iterated
/** @since %Qore 2.1
*/
class QoreNamespaceEnumIterator {
public:
    //! creates the iterator
    DLLEXPORT QoreNamespaceEnumIterator(const QoreNamespace& ns);

    //! destroys the iterator
    DLLEXPORT virtual ~QoreNamespaceEnumIterator();

    //! moves to the next position; returns true if the iterator is pointing to a valid element
    DLLEXPORT bool next();

    //! returns the enum
    DLLEXPORT const QoreEnumDecl& get() const;

private:
    //! this function is not implemented
    QoreNamespaceEnumIterator(const QoreNamespaceEnumIterator&) = delete;

    //! this function is not implemented
    QoreNamespaceEnumIterator& operator=(const QoreNamespaceEnumIterator&) = delete;

    class ConstEnumListIterator* priv;
};

#endif // QORE_NAMESPACE_H
