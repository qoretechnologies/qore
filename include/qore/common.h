/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    common.h

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

#ifndef _QORE_COMMON_H

#define _QORE_COMMON_H

/** @file common.h
    provides type and other definitions for the Qore library
 */

#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <list>
#include <set>
#include <set>
#include <string>
#include <strings.h>
#include <vector>

#ifdef __GNUC__
#define PACK __attribute__ ((packed))
#else
#define PACK
#endif

//! Qore's base type info class
class QoreTypeInfo;
class RuntimeConfig;

//! cross-platform define for AF_UNSPEC
#define Q_AF_UNSPEC -1

//! cross-platform define for AF_INET
#define Q_AF_INET -2

//! cross-platform define for AF_INET6
#define Q_AF_INET6 -3

//! platform-independent define for SOCK_STREAM
#define Q_SOCK_STREAM -1

//! used to identify unique Qore data and parse types (descendents of AbstractQoreNode)
typedef int16_t qore_type_t;

//! used for sizes (same range as a pointer)
typedef size_t qore_size_t;

//! used for offsets that could be negative
typedef intptr_t qore_offset_t;

//! used for the unique class ID for QoreClass objects
typedef unsigned qore_classid_t;

//! set of integers
typedef std::set<int> int_set_t;

//! qore library and module license type identifiers
enum qore_license_t {
    QL_GPL = 0,         //!< code to be used under the GPL license
    QL_LGPL = 1,        //!< code to be used under the LGPL license
    QL_MIT = 2          //!< code to be used under the MIT license
};

// class access values
enum ClassAccess : unsigned char {
    Public = 0,        // publicly accessible
    Private = 1,       // accessible only in the class hierarchy (like c++'s 'protected')
    Internal = 2,      // accessible only in the class itself
    Inaccessible = 3,  // not accessible from the class
};

#if defined _MSC_VER || ((defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__)
#define _Q_WINDOWS 1
#ifdef _WIN64
#define _Q_WINDOWS64 1
#endif
#endif

#ifdef _Q_WINDOWS
  #ifdef BUILDING_DLL
    #define DLLEXPORT __declspec(dllexport)
  #else
    #define DLLEXPORT __declspec(dllimport)
  #endif
  #define DLLLOCAL

  #define QLLD "%I64d"
  #define QLLX "%I64x"
  #define QLLDx(a) "%" #a "I64d"
  #define QORE_DIR_SEP '\\'
  #define QORE_DIR_SEP_STR "\\"
  #define QORE_PATH_SEP ';'
  #define QORE_PATH_SEP_STR ";"
  #include <winsock2.h>
#else
  #ifdef HAVE_GCC_VISIBILITY
    #define DLLEXPORT __attribute__ ((visibility("default")))
    #define DLLLOCAL __attribute__ ((visibility("hidden")))
  #else
    #define DLLEXPORT
    #define DLLLOCAL
  #endif
  #define QLLD "%lld"
  #define QLLX "%llx"
  #define QLLDx(a) "%" #a "lld"
  #define QORE_DIR_SEP '/'
  #define QORE_DIR_SEP_STR "/"
  #define QORE_PATH_SEP ':'
  #define QORE_PATH_SEP_STR ":"
#endif

#define _Q_MAKE_STRING(x) #x
#define MAKE_STRING_FROM_SYMBOL(x) _Q_MAKE_STRING(x)

// forward references
class AbstractQoreNode;
class QoreListNode;
class ExceptionSink;
class QoreObject;
class AbstractPrivateData;
class QoreMethod;
class QoreBuiltinMethod;
class QoreClass;
class TypedHashDecl;
class QoreExternalFunction;
class QoreExternalGlobalVar;
class QoreExternalConstant;
class QoreNamespace;
class QoreHashNode;

class QoreValue;

typedef std::vector<const QoreClass*> class_vec_t;
typedef std::vector<std::pair<const TypedHashDecl*, const QoreNamespace*>> hashdecl_vec_t;
typedef std::vector<const QoreExternalFunction*> func_vec_t;
typedef std::vector<const QoreNamespace*> ns_vec_t;
typedef std::vector<std::pair<const QoreExternalGlobalVar*, const QoreNamespace*>> gvar_vec_t;
typedef std::vector<std::pair<const QoreExternalConstant*, const QoreNamespace*>> const_vec_t;

//! functor template for calling free() on pointers
template <typename T> struct free_ptr {
    DLLLOCAL void operator()(T* ptr) {
        free(ptr);
    }
};

//! functor template for deleting elements
template <typename T> struct simple_delete {
    DLLLOCAL void operator()(T* ptr) {
        delete ptr;
    }
};

//! functor template for dereferencing elements
template <typename T> struct simple_deref {
    DLLLOCAL void operator()(T* ptr) {
        ptr->deref();
    }
    DLLLOCAL void operator()(T* ptr, ExceptionSink* xsink) {
        ptr->deref(xsink);
    }
};

//! for simple c-string less-than comparisons
class ltstr {
public:
    DLLLOCAL bool operator()(const char* s1, const char* s2) const {
        return strcmp(s1, s2) < 0;
    }
};

//! for simple c-string case-insensitive less-than comparisons
class ltcstrcase {
public:
    DLLLOCAL bool operator()(const char* s1, const char* s2) const {
        return strcasecmp(s1, s2) < 0;
    }
};

//! for std::string case-insensitive less-than comparisons
class ltstrcase {
public:
    DLLLOCAL bool operator()(std::string s1, std::string s2) const {
        return strcasecmp(s1.c_str(), s2.c_str()) < 0;
    }
};

class eqstr {
public:
    DLLLOCAL bool operator()(const char* s1, const char* s2) const {
        return !strcmp(s1, s2);
    }
};

class eqstrcase {
public:
    DLLLOCAL bool operator()(const char* s1, const char* s2) const {
        return !strcasecmp(s1, s2);
    }
};

//! for char less-than comparisons
class ltchar {
public:
    DLLLOCAL bool operator()(const char s1, const char s2) const {
        return s1 < s2;
    }
};

//! non-thread-safe vector for storing "char*" that you want to delete
class cstr_vector_t : public std::vector<char*> {
public:
    DLLLOCAL ~cstr_vector_t() {
        clear();
    }

    DLLLOCAL void clear() {
        std::for_each(begin(), end(), free_ptr<char>());
        std::vector<char*>::clear();
    }
};

//! vector of type information for parameter lists
typedef std::vector<const QoreTypeInfo*> type_vec_t;

//! vector of value information for default argument lists
typedef std::vector<QoreValue> arg_vec_t;

//! vector of parameter names for parameter lists
typedef std::vector<std::string> name_vec_t;

//! 64bit integer type, cannot use int64_t here since it breaks the API on some 64-bit systems due to equivalence between long int and int
typedef long long int64;

//! runtime code execution flags
typedef uint64_t q_rt_flags_t;

//! Serialization flag; include static class vars
constexpr size_t QSF_INCLUDE_STATIC_CLASS_VARS = (1 << 0);

//! Deserialization flag: deserialize static class vars
constexpr size_t QDF_STATIC_CLASS_VARS = (1 << 0);

//! Deserialization flag: deserialize and merge static class vars; implies QDF_STATIC_CLASS_VARS
constexpr size_t QDF_MERGE_STATIC_CLASS_VARS = (1 << 1) | QDF_STATIC_CLASS_VARS;

//! serialization context object used in builtin serializer methods
/** @since %Qore 0.9
*/
class QoreSerializationContext {
public:
    //! Serializes the given object and returns the serialization index
    /** Throws a Qore-language exception if there is an error serializing the object

        @note When serializing weak references to objects, a strong reference to the object must also be serialized in
        the same operation, or any weak references will be invalid after deserialization
    */
    DLLEXPORT int serializeObject(const QoreObject& obj, std::string& index, ExceptionSink* xsink);

    //! Serializes the given hash and returns the serialization index
    /** Throws a Qore-language exception if there is an error serializing the hash

        @note When serializing weak references to hashes, a strong reference to the hash must also be serialized in
        the same operation, or any weak references will be invalid after deserialization
    */
    DLLEXPORT int serializeHash(const QoreHashNode& h, std::string& index, ExceptionSink* xsink);

    //! Serializes the given list and returns the serialization index
    /** Throws a Qore-language exception if there is an error serializing the list

        @note When serializing weak references to lists, a strong reference to the list must also be serialized in
        the same operation, or any weak references will be invalid after deserialization
    */
    DLLEXPORT int serializeList(const QoreListNode& l, std::string& index, ExceptionSink* xsink);

    //! Serializes the given value and returns a serialized value representing the value to be serialized
    /** throws a Qore-language exception if there is an error serializing the object
    */
    DLLEXPORT QoreValue serializeValue(const QoreValue val, ExceptionSink* xsink);

    //! Adds a module to be loaded when the data is deserialized
    DLLEXPORT void addModule(const char* module);

    //! Returns the current serialization flag bitfield
    DLLEXPORT int64 getFlags() const;

private:
    //! This class is a wrapper class that cannot be constructed
    DLLLOCAL QoreSerializationContext();
};

//! deserialization context object used in builtin deserializer methods
/** @since %Qore 0.9
*/
class QoreDeserializationContext {
public:
    //! returns the container corresponding to the given serialization index
    /** throws a Qore-language exception if there is an error deserializing the value
    */
    DLLEXPORT QoreValue deserializeContainer(const char* index, ExceptionSink* xsink);

    //! deserializes the given data value and returns the deserialized value
    /** throws a Qore-language exception if there is an error deserializing the object
    */
    DLLEXPORT QoreValue deserializeValue(const QoreValue val, ExceptionSink* xsink);

    //! Returns the current deserialization flag bitfield
    DLLEXPORT int64 getFlags() const;

private:
    //! this class is a wrapper class that cannot be constructed
    DLLLOCAL QoreDeserializationContext();
};

//! the type used for builtin function signatures
/** @param args the list of arguments to the function (could be 0), use inline functions in params.h to access
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()

    @return the return value of the function; the caller owns any reference returned in the return value
 */
typedef QoreValue (*q_func_t)(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink);

//! the type used for builtin function signatures for external functions
/** @param ptr  a pointer to user-defined data set when the variant is added to the function
    @param args the list of arguments to the function (could be 0), use inline functions in params.h to access
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()

    @return the return value of the function; the caller owns any reference returned in the return value

    @since %Qore 0.9.5
 */
typedef QoreValue (*q_external_func_t)(const void* ptr, const QoreListNode* args, RuntimeConfig& rc,
    ExceptionSink* xsink);

//! the type used for builtin QoreClass method signatures
/** @param self the QoreObject that the function is being executed on
    @param private_data the object's private data representing the state of the object
    @param args the list of arguments to the function (could be 0), use inline functions in params.h to access
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()

    @return the return value of the function (can be 0); the caller owns any reference returned in the return value
 */
typedef QoreValue (*q_method_t)(QoreObject* self, AbstractPrivateData* private_data, const QoreListNode* args,
    RuntimeConfig& rc, ExceptionSink* xsink);

//! the type used for builtin QoreClass method signatures
/** @param method a constant reference to the QoreMethod being called
    @param ptr a pointer to user-defined data set when the variant is added to the method
    @param self the QoreObject that the function is being executed on
    @param private_data the object's private data representing the state of the object
    @param args the list of arguments to the function (could be 0), use inline functions in params.h to access
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()

    @return the return value of the function (can be 0); the caller owns any reference returned in the return value

    @since %Qore 0.8.13
 */
typedef QoreValue (*q_external_method_t)(const QoreMethod& method, const void* ptr, QoreObject* self,
    AbstractPrivateData* private_data, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink);

//! the type used for external static methods
/** @param method a constant reference to the QoreMethod being called
    @param ptr a pointer to user-defined data set when the variant is added to the method
    @param args the list of arguments to the function (could be 0), use inline functions in params.h to access
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()

    @return the return value of the function; the caller owns any reference returned in the return value

    @since %Qore 0.8.13
 */
typedef QoreValue (*q_external_static_method_t)(const QoreMethod& method, const void* ptr, const QoreListNode* args,
    RuntimeConfig& rc, ExceptionSink* xsink);

//! the type used for builtin QoreClass constructor method signatures
/** @param self the QoreObject that the function is being executed on
    @param args the list of arguments to the function (could be 0), use inline functions in params.h to access
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()
 */
typedef void (*q_constructor_t)(QoreObject* self, const QoreListNode* args, RuntimeConfig& rc,
    ExceptionSink* xsink);

//! the type used for builtin QoreClass constructor method signatures
/** @param method a constant reference to the QoreMethod being called for the constructor (in a heirarchy, the class of this method could be different than the QoreClass returned from QoreObject::getClass() if the constructor for a base class is being executed)
    @param ptr a pointer to user-defined data set when the variant is added to the method
    @param self the QoreObject that the function is being executed on
    @param args the list of arguments to the function (could be 0), use inline functions in params.h to access
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()

    @since %Qore 0.8.13
 */
typedef void (*q_external_constructor_t)(const QoreMethod& method, const void* ptr, QoreObject* self,
    const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink);

//! the type used for builtin QoreClass system constructor method signatures
/** System constructors are called for objects that are created automatically by the library, normally to be assigned to constants.
    System objects are treated specially by the Qore library as they are not associated with any QoreProgram object.
    Additionally, system object constructors are not allowed to raise exceptions.

    @param self the QoreObject that the function is being executed on
    @param code this argument is necessary in order to be able to provide the va_list in the following argument due
    to the way QoreClass::execSystemConstructor() is called.  If not required by the constuctor, this argument can be ignored.
    @param args a variable-length list of arguments to the system constructor
*/
typedef void (*q_system_constructor_t)(QoreObject* self, int code, va_list args);

//! the type used for builtin QoreClass destructor signatures
/** destructors are optional, but, if present, must call AbstractPrivateData::deref() on any private data (if present)
    @param self the QoreObject that the function is being executed on
    @param private_data the object's private data representing the state of the object for the current builtin class
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()
 */
typedef void (*q_destructor_t)(QoreObject* self, AbstractPrivateData* private_data, RuntimeConfig& rc,
    ExceptionSink* xsink);

//! the type used for builtin QoreClass destructor signatures with the new generic calling convention and user-defined data
/** destructors are optional, but, if present, must call AbstractPrivateData::deref() on any private data (if present)
    @param thisclass a constant reference to the QoreClass
    @param ptr a pointer to user-defined data set when the variant is added to the method
    @param self the QoreObject that the function is being executed on
    @param private_data the object's private data representing the state of the object for the current builtin class
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()
 */
typedef void (*q_external_destructor_t)(const QoreClass& thisclass, const void* ptr, QoreObject* self,
    AbstractPrivateData* private_data, RuntimeConfig& rc, ExceptionSink* xsink);

//! the type used for builtin QoreClass copy signatures
/** this function must set any private data against the new object by calling QoreObject::setPrivate() on \c self
    @param self the QoreObject that the function is being executed on (the new copy of the object)
    @param old the object being copied
    @param private_data the object's private data representing the state of the object for the current builtin class
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()
 */
typedef void (*q_copy_t)(QoreObject* self, QoreObject* old, AbstractPrivateData* private_data, RuntimeConfig& rc,
    ExceptionSink* xsink);

//! the type used for builtin QoreClass copy signatures with the new generic calling convention
/** this function must set any private data against the new object by calling QoreObject::setPrivate() on \c self
    @param thisclass a constant reference to the QoreClass being copied
    @param ptr a pointer to user-defined data set when the variant is added to the method
    @param self the QoreObject that the function is being executed on (the new copy of the object)
    @param old the object being copied
    @param private_data the object's private data representing the state of the object for the current builtin class
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()
 */
typedef void (*q_external_copy_t)(const QoreClass& thisclass, const void* ptr, QoreObject* self, QoreObject* old,
    AbstractPrivateData* private_data, RuntimeConfig& rc, ExceptionSink* xsink);

//! the type used for builtin QoreClass serializer method signatures
/** @param self the QoreObject that the function is being executed on
    @param data the private data for the builtin class
    @param context the serialization context object
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()

    @return the serialized private data hash for the object to be deserialized to object state and/or members in the deserialization method (may be nullptr)
 */
typedef QoreHashNode* (*q_serializer_t)(const QoreObject& self, const AbstractPrivateData& data,
    QoreSerializationContext& context, RuntimeConfig& rc, ExceptionSink* xsink);

//! the type used for builtin QoreClass deserializer method signatures
/** @param self the QoreObject that the function is being executed on
    @param sdata the data that was returned by the serializer containing the object state and/or any members
    @param context the deserialization context object
    @param rc runtime configuration for the call
    @param xsink Qore-language exception information should be stored here by calling ExceptionSink::raiseException()

    @return the hash to be written to the class as members (may be nullptr)

    This method must also set any private data against the object
 */
typedef void (*q_deserializer_t)(QoreObject& self, const QoreHashNode* sdata, QoreDeserializationContext& context,
    RuntimeConfig& rc, ExceptionSink* xsink);

//! type for thread resource IDs (unique within a single running qore library process)
/** @see qore_get_trid()
 */
typedef unsigned q_trid_t;

//! returns an integer value for a string
DLLEXPORT long long q_atoll(const char* str);

//! returns the type name for an opaqua QoreTypeInfo ptr
/** @since %Qore 0.9
*/
DLLEXPORT const char* type_get_name(const QoreTypeInfo* t);

#endif // _QORE_COMMON_H
