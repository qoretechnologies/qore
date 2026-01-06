/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHashNode.h

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

#ifndef _QORE_QOREHASHNODE_H

#define _QORE_QOREHASHNODE_H

#include <qore/AbstractQoreNode.h>
#include <qore/common.h>

class LocalVar;
class QoreString;
class QoreStringNode;
class TypedHashDecl;

//! This is the hash or associative list container type in Qore, dynamically allocated only, reference counted
/**
   it is both a value type and can hold parse expressions as well (in which case it needs to be evaluated)
   This type also maintains the insertion order as well as offering a hash-based lookup of string keys.
   The insertion order of keys is maintained in order to support consistent serialization and
   deserialization to and from XML, JSON, YAML, etc
 */
class QoreHashNode : public AbstractQoreNode {
    friend class HashIterator;
    friend class ReverseHashIterator;
    friend class ConstHashIterator;
    friend class ReverseConstHashIterator;
    friend class HashAssignmentHelper;
    friend class hash_assignment_priv;
    friend class qore_object_private;
    friend class qore_hash_private;

public:
    //! creates an empty hash
    DLLEXPORT QoreHashNode();

    //! creates a hash of the specific type; the hash is initialized according to the hashdecl declaration
    /** @since %Qore 0.8.13
    */
    DLLEXPORT QoreHashNode(const TypedHashDecl* hd, ExceptionSink* xsink);

    //! creates an empty hash with the specific value type
    /** @since %Qore 0.8.13
    */
    DLLEXPORT QoreHashNode(const QoreTypeInfo* valueTypeInfo);

    //! returns false unless perl-boolean-evaluation is enabled, in which case it returns false only when empty
    /** @return false unless perl-boolean-evaluation is enabled, in which case it returns false only when empty
        */
    DLLEXPORT virtual bool getAsBoolImpl() const;

    //! concatenate the verbose string representation of the list (including all contained values) to an existing QoreString
    /** used for %n and %N printf formatting
        @param str the string representation of the type will be concatenated to this QoreString reference
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return -1 for exception raised, 0 = OK
    */
    DLLEXPORT virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    //! returns a QoreString giving the verbose string representation of the List (including all contained values)
    /** used for %n and %N printf formatting
        @param del if this is true when the function returns, then the returned QoreString pointer should be deleted, if false, then it must not be
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        NOTE: Use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly
        @see QoreNodeAsStringHelper
    */
    DLLEXPORT virtual QoreString* getAsString(bool &del, int foff, ExceptionSink* xsink) const;

    //! performs a copy of the hash and returns the new hash
    /** @return a copy of the QoreHashNode
    */
    DLLEXPORT virtual QoreHashNode* realCopy() const;

    //! tests for equality ("deep compare" including all contained values) with possible type conversion (soft compare)
    /**
        @param v the value to compare
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! tests for equality ("deep compare" including all contained values) without type conversions (hard compare)
    /**
        @param v the value to compare
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! returns the type name as a c string
    /** @return the type name as a c string
    */
    DLLEXPORT virtual const char* getTypeName() const;

    //! returns "this" with an incremented reference count
    /** @return "this" with an incremented reference count
    */
    DLLEXPORT QoreHashNode* hashRefSelf() const;

    //! returns the cstring value of the first key in the hash
    /** @return the cstring value of the first key in the hash
    */
    DLLEXPORT const char* getFirstKey() const;

    //! returns the cstring value of the last key in the hash
    /** @return the cstring value of the last key in the hash
    */
    DLLEXPORT const char* getLastKey() const;

    //! returns the value of the given key
    /** @param key the key to return
        @param exists true if the key existed (in case no value is returned), false if not
        @param xsink Qore language exceptions are raised here (ex: key is not valid for a hashdecl-derived hash)

        @return the value of the given key, if the key is present; the value is not referenced for the return

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreValue getKeyValueExistence(const char* key, bool& exists, ExceptionSink* xsink) const;

    //! returns the value of the given key
    /** @param key the key to return
        @param exists true if the key existed (in case no value is returned), false if not

        @return the value of the given key, if the key is present; the value is not referenced for the return

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreValue getKeyValueExistence(const char* key, bool& exists) const;

    //! returns the value of the given key
    /** @param key the key to return
        @param xsink Qore language exceptions are raised here (ex: key is not valid for a hashdecl-derived hash)

        @return the value of the given key, if the key is present; the value is not referenced for the return

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreValue getKeyValue(const char* key, ExceptionSink* xsink) const;

    //! returns the value of the given key
    /** @param key the key to return

        @return the value of the given key, if the key is present; the value is not referenced for the return

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreValue getKeyValue(const char* key) const;

    //! returns the value of the given key
    /** @param key the key to return
        @param exists true if the key existed (in case no value is returned), false if not
        @param xsink Qore language exceptions are raised here (ex: key is not valid for a hashdecl-derived hash)

        @return the value of the given key, if the key is present; the value is not referenced for the return

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreValue getKeyValueExistence(const QoreString& key, bool& exists, ExceptionSink* xsink) const;

    //! returns the value of the given key
    /** @param key the key to return
        @param exists true if the key existed (in case no value is returned), false if not

        @return the value of the given key, if the key is present; the value is not referenced for the return

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreValue getKeyValueExistence(const QoreString& key, bool& exists) const;

    //! returns the value of the given key
    /** @param key the key to return
        @param xsink Qore language exceptions are raised here (ex: key is not valid for a hashdecl-derived hash)

        @return the value of the given key, if the key is present; the value is not referenced for the return

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreValue getKeyValue(const QoreString& key, ExceptionSink* xsink) const;

    //! returns the value of the key as an int64
    /** @param key the key to return the value for
        @param found returns as true if the key exists, false if not
        @return the value of the key as an int64

        @deprecated use getKeyValue() instead
    */
    DLLEXPORT int64 getKeyAsBigInt(const char* key, bool &found) const;

    //! returns the value of the key as a bool
    /** @param key the key to return the value for
        @param found returns as true if the key exists, false if not
        @return the value of the key as a bool

        @deprecated use getKeyValue() instead
    */
    DLLEXPORT bool getKeyAsBool(const char* key, bool &found) const;

    //! performs a copy of the hash and returns the new hash
    /** @return a copy of the current QoreHashNode
    */
    DLLEXPORT QoreHashNode* copy() const;

    DLLEXPORT QoreValue& getKeyValueReference(const char* key);

    //! appends all key-value pairs of "h" to this hash
    /** Note that all keys and values of the QoreHashNode "h" are copied to this hash, values are referenced as necessary for the assigment to "this".
        Qore-language exceptions could be thrown if the hash keys in "this" are overwritten with new values and the old value is an object that goes out of scope when dereferenced.
        @param h the QoreHashNode to use to merge all keys to "this"
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void merge(const QoreHashNode* h, ExceptionSink* xsink);

    //! sets the value of "key" to "value"
    /** A Qore-language exception could be thrown if the hash is derived from a hashdecl and the key is unknown, or if the given key has a current value and it's a QoreObject that goes out of scope when dereferenced (the object's destructor could throw an exception); if an exception is thrown due to encoding conversion issues (and therefore the assignment is not made) then the value to be assigned is dereferenced automatically
        @param key the key to set the value for
        @param value the value to assign to the key, must be already referenced for the assignment
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, -1 if a Qore-language exception was thrown

        @note the assignment is made even if an exception occurs when dereferencing the old value

        @since %Qore 0.8.13
    */
    DLLEXPORT int setKeyValue(const char* key, QoreValue value, ExceptionSink* xsink);

    //! sets the value of "key" to "value"
    /** A Qore-language exception could be thrown if the hash is derived from a hashdecl and the key is unknown, or converting the key string's encoding to QCS_DEFAULT, or if the given key has a current value and it's a QoreObject that goes out of scope when dereferenced (the object's destructor could throw an exception); if an exception is thrown due to encoding conversion issues (and therefore the assignment is not made) then the value to be assigned is dereferenced automatically
        @param key the key to set the value for
        @param value the value to assign to the key, must be already referenced for the assignment
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, -1 if a Qore-language exception was thrown

        @note the assignment is made even if an exception occurs when dereferencing the old value

        @since %Qore 0.8.13
    */
    DLLEXPORT int setKeyValue(const QoreString& key, QoreValue value, ExceptionSink* xsink);

    //! performs a delete operation on the value of the given key
    /** The delete operation means a simple dereference for all types except QoreObject, on this type the destructor will be run immediately.
        A Qore-language exception could occur eitherin converting the key string's encoding to QCS_DEFAULT, or in the destructor of a deleted object.
        @param key the key of the value to delete
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void deleteKey(const QoreString* key, ExceptionSink* xsink);

    //! performs a delete operation on the value of the given key
    /** the delete operation means a simple dereference for all types except QoreObject, on this type the destructor will be run immediately
        @param key the key of the value to delete
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void deleteKey(const char* key, ExceptionSink* xsink);

    //! removes the value from the hash and returns the value removed, if any
    DLLEXPORT QoreValue takeKeyValue(const char* key);

    //! returns a QoreListNode of QoreStringNode pointers representing all keys in the hash, caller owns the reference count returned
    /** to iterate through a hash, use HashIterator, ReverseHashIterator, ConstHashIterator, or ReverseConstHashIterator
        @return a QoreListNode of QoreStringNode pointers representing all keys in the hash, caller owns the reference count returned
    */
    DLLEXPORT QoreListNode* getKeys() const;

    //! returns a QoreListNode of AbstractQoreNode pointers representing all values in the hash; caller owns the reference count returned
    /** to iterate through a hash, use HashIterator, ReverseHashIterator, ConstHashIterator, or ReverseConstHashIterator
        @return a QoreListNode of AbstractQoreNode pointers representing all values in the hash; caller owns the reference count returned

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreListNode* getValues() const;

    //! does a deep "soft" compare of all hash elements (types may be converted for the comparison) and returns true if the hashes are equal
    /** @note that if the hashes have a different number or names of keys then the comparison fails immediately
        @return true if the hashes have the same number and names of keys and all elements are equal (types may be converted for the comparison)
    */
    DLLEXPORT bool compareSoft(const QoreHashNode* h, ExceptionSink* xsink) const;

    //! does a deep "hard" compare of all hash elements (no type conversions are performed) and returns true if the hashes are equal
    /** @note that if the hashes have a different number of keys then the comparison fails immediately
        @return true if the hashes have the same number and values of keys and all elements are equal and of the same type (no type conversions are performed)
    */
    DLLEXPORT bool compareHard(const QoreHashNode* h, ExceptionSink* xsink) const;

    //! returns the number of members in the hash, executes in constant time
    /** @return the number of members in the hash
    */
    DLLEXPORT size_t size() const;

    //! returns true if the hash has no members, false if not
    /** @return true if the hash has no members, false if not
    */
    DLLEXPORT bool empty() const;

    //! returns true if the hash contains the given key
    /** @note that if this returns true, it does not mean the key has a value; it just means that the hash contains the key; the key's value could be NOTHING
        @param key the key name to check, must be in default encoding QCS_DEFAULT
        @return true if the hash contains the given key
    */
    DLLEXPORT bool existsKey(const char* key) const;

    //! returns true if the hash contains the given key and the key has a value (i.e. is not NOTHING)
    /** @param key the key name to check, must be in default encoding QCS_DEFAULT
        @return true if the hash contains the given key and the key has a value
    */
    DLLEXPORT bool existsKeyValue(const char* key) const;

    //! removes the given key from the hash and derefences its value, if any
    /** A Qore-language exception could occur either in converting the key string's encoding to QCS_DEFAULT, or when dereferencing the contained value
        @param key the key of the value to delete
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void removeKey(const QoreString* key, ExceptionSink* xsink);

    //! removes the given key from the hash and derefences its value, if any
    /** A Qore-language exception could occur when dereferencing the contained value
        @param key the key of the value to delete, must be in QCS_DEFAULT encoding
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void removeKey(const char* key, ExceptionSink* xsink);

    //! returns the hash's type declaration, if any
    /** @since %Qore 0.8.13
    */
    DLLEXPORT const TypedHashDecl* getHashDecl() const;

    //! returns the value type declaration (only possible if there is no hashdecl set)
    /** @since %Qore 0.8.13
    */
    DLLEXPORT const QoreTypeInfo* getValueTypeInfo() const;

    //! returns the type info structure for the current value; also works for hashes derived from a TypedHashDecl or with a specific value type
    /** @since %Qore 0.8.13
    */
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! returns the type name (useful in templates)
    /** @return the type name
    */
    DLLLOCAL static const char* getStaticTypeName() {
        return "hash";
    }

    //! returns the type code (useful in templates)
    DLLLOCAL static qore_type_t getStaticTypeCode() {
        return NT_HASH;
    }

    static const qore_type_t TYPE = NT_HASH;

    //! Increments the weak reference count for this object
    /** @since %Qore 2.0
    */
    DLLEXPORT void weakRef();

    //! Decrements the weak reference count for this object
    /** @return if \c true then the object was deleted

        @since %Qore 2.0
    */
    DLLEXPORT bool weakDeref();

    //! initializes during parsing
    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL QoreHashNode(bool ne);
    DLLLOCAL void clear(ExceptionSink* xsink, bool reverse = false);

    //! sets "needs_eval" to false and "value" to true
    DLLLOCAL void clearNeedsEval();

    //! sets "needs_eval" to true and "value" to false
    DLLLOCAL void setNeedsEval();

    // returns a new hash consisting of just the members of value_list
    DLLLOCAL QoreHashNode* getSlice(const QoreListNode* value_list, ExceptionSink* xsink) const;

protected:
    //! private implementation of the class
    class qore_hash_private *priv;

    //! dereferences all elements of the hash
    /** The ExceptionSink argument is needed for those types that could throw
        an exception when they are deleted (ex: QoreObject) - which could be
        contained in the hash
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return true if the object can be deleted, false if not (externally-managed)
    */
    DLLEXPORT virtual bool derefImpl(ExceptionSink* xsink);

    //! optionally evaluates the argument
    /** return value requires a deref(xsink) if needs_deref is true
        @see AbstractQoreNode::eval()
    */
    DLLLOCAL virtual QoreValue evalImpl(bool &needs_deref, ExceptionSink* xsink) const;

    //! deletes the object, cannot be called directly (use deref(ExceptionSink*) instead)
    /** @see AbstractQoreNode::deref()
        @see QoreHashNode::derefImpl()
    */
    DLLEXPORT virtual ~QoreHashNode();

private:
    DLLLOCAL QoreHashNode(const QoreHashNode&) = delete;
    DLLLOCAL QoreHashNode& operator=(const QoreHashNode&) = delete;
};

#include <qore/ReferenceHolder.h>

//! For use on the stack only: manages a QoreHashNode reference count
/**
   @see ReferenceHolder
*/
typedef ReferenceHolder<QoreHashNode> QoreHashNodeHolder;

class qhi_priv;

//! iterator class for QoreHashNode, to be only created on the stack
/**
   @code
   HashIterator hi(h);
   while (hi.next()) {
      QoreStringValueHelper str(hi.get());
      printf("key: '%s', value: '%s'\n", hi.getKey(), str->getBuffer());
   }
   @endcode
*/
class HashIterator {
    friend class HashAssignmentHelper;
    friend class qhi_priv;

protected:
    QoreHashNode* h;
    qhi_priv* priv;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL HashIterator(const HashIterator&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL HashIterator& operator=(const HashIterator&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL void* operator new(size_t);

public:
    //! initializes the iterator with the passed hash
    DLLEXPORT HashIterator(QoreHashNode* h);

    //! initializes the iterator with the passed hash
    DLLEXPORT HashIterator(QoreHashNode& h);

    //! Destroys the iterator
    DLLEXPORT ~HashIterator();

    //! moves to the next element, returns false when there are no more elements to iterate
    /** also moves to the first element if the object has just been initialized after a complete iteration
        (assuming there is at least one element in the hash)
    */
    DLLEXPORT bool next();

    //! moves to the previous element, returns false when there are no more elements to iterate
    /** also moves to the last element if the object has just been initialized after a complete iteration
        (assuming there is at least one element in the hash)
    */
    DLLEXPORT bool prev();

    //! returns the current key
    DLLEXPORT const char* getKey() const;

    //! returns a QoreString for the current key, the caller owns QoreString returned
    DLLEXPORT QoreString* getKeyString() const;

    //! returns a QoreStringNode for the current key, the caller owns QoreString returned
    DLLEXPORT QoreStringNode* getKeyStringNode() const;

    //! returns the value of the current key
    DLLEXPORT QoreValue get() const;

    //! returns the type info for the current value
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! deletes the key from the hash and dereferences the value
    /** the pointer is moved to the previous element (or before the beginning)
        so that the next call to next() will put the pointer on the element after
        the one being deleted
    */
    DLLEXPORT void deleteKey(ExceptionSink* xsink);

    //! Assigns the given value to the key and dereferences any current value
    /**
        @param val the value to assign; the value must already be referenced for the assignment
        @param xsink type exceptions with the value to be assigned and any exception dereferencing the current value
        will be stored here

        @return -1 = the iterator is invalid or an exception was thrown dereferencing the current value, or the
        current key cannot accept the value due to type restrictions; 0 = OK

        If the value cannoe be assigned because the iterator is invalid or there is a conflict with the type, the
        value will be dereferenced in the call.

        @since %Qore 1.14
    */
    DLLEXPORT int assign(QoreValue val, ExceptionSink* xsink);

    //! removes the key value and returns the value returned
    DLLEXPORT QoreValue removeKeyValue();

    //! returns the value of the current key with an incremented reference count
    DLLEXPORT QoreValue getReferenced() const;

    //! returns the hash
    DLLEXPORT QoreHashNode* getHash() const;

    //! returns true if on the first key of the hash
    DLLEXPORT bool first() const;

    //! returns true if on the last key of the hash
    DLLEXPORT bool last() const;

    //! returns true if the hash is empty
    DLLEXPORT bool empty() const;

    //! returns true if the iterator is currently pointing at a valid element
    DLLEXPORT bool valid() const;
};

//! reverse iterator class for QoreHashNode, to be only created on the stack
/**
    @code
    ReverseHashIterator hi(h);
    while (hi.next()) {
        QoreStringValueHelper str(hi.get());
        printf("key: '%s', value: '%s'\n", hi.getKey(), str->getBuffer());
    }
    @endcode
*/
class ReverseHashIterator : public HashIterator {
public:
    //! initializes the iterator with the passed hash
    DLLEXPORT ReverseHashIterator(QoreHashNode* h);

    //! initializes the iterator with the passed hash
    DLLEXPORT ReverseHashIterator(QoreHashNode& h);

    //! Destroys the iterator
    DLLEXPORT ~ReverseHashIterator();

    //! moves to the next element in reverse order, returns false when there are no more elements to iterate
    /** also moves to the last element if the object has just been initialized after a complete iteration
         (assuming there is at least one element in the hash)
    */
    DLLEXPORT bool next();

    //! moves to the previous element in reverse order, returns false when there are no more elements to iterate
    /** also moves to the first element if the object has just been initialized after a complete iteration
         (assuming there is at least one element in the hash)
    */
    DLLEXPORT bool prev();

    //! returns true if on the last key of the hash
    DLLEXPORT bool first() const;

    //! returns true if on the first key of the hash
    DLLEXPORT bool last() const;
};

//! constant iterator class for QoreHashNode, to be only created on the stack
/**
    @code
    ConstHashIterator hi(h);
    while (hi.next()) {
        QoreStringValueHelper str(hi.get());
        printf("key: '%s', value: '%s'\n", hi.getKey(), str->getBuffer());
    }
    @endcode
*/
class ConstHashIterator {
protected:
    const QoreHashNode* h;
    qhi_priv* priv;

    //! this function is not implemented; it is here as a protected function in order to prohibit it from being used
    DLLLOCAL ConstHashIterator& operator=(const HashIterator&);

public:
    //! initializes the iterator with the passed hash
    DLLEXPORT ConstHashIterator(const QoreHashNode* h);

    //! initializes the iterator with the passed hash
    DLLEXPORT ConstHashIterator(const QoreHashNode& h);

    //! copy constructor
    DLLLOCAL ConstHashIterator(const ConstHashIterator&);

    //! Destroys the iterator
    DLLEXPORT ~ConstHashIterator();

    //! moves to the next element, returns false when there are no more elements to iterate
    /** also moves to the first element if the object has just been initialized after a complete iteration
        (assuming there is at least one element in the hash)
    */
    DLLEXPORT bool next();

    //! moves to the previous element, returns false when there are no more elements to iterate
    /** also moves to the last element if the object has just been initialized after a complete iteration
        (assuming there is at least one element in the hash)
    */
    DLLEXPORT bool prev();

    //! returns the current key
    DLLEXPORT const char* getKey() const;

    //! returns a QoreString for the current key, the caller owns QoreString returned
    DLLEXPORT QoreString* getKeyString() const;

    //! returns a QoreStringNode for the current key, the caller owns QoreString returned
    DLLEXPORT QoreStringNode* getKeyStringNode() const;

    //! returns the value of the current key
    DLLEXPORT const QoreValue get() const;

    //! returns the type info for the current value
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! returns the value of the current key with an incremented reference count
    DLLEXPORT QoreValue getReferenced() const;

    //! returns the hash
    DLLEXPORT const QoreHashNode* getHash() const;

    //! returns true if on the first key of the hash
    DLLEXPORT bool first() const;

    //! returns true if on the last key of the hash
    DLLEXPORT bool last() const;

    //! returns true if the hash is empty
    DLLEXPORT bool empty() const;

    //! returns true if the iterator is currently pointing at a valid element
    DLLEXPORT bool valid() const;

    //! resets the iterator to its initial state
    DLLEXPORT void reset();
};

//! reverse constant iterator class for QoreHashNode, to be only created on the stack
/**
    @code
    ReverseConstHashIterator hi(h);
    while (hi.next()) {
        QoreStringValueHelper str(hi.get());
        printf("key: '%s', value: '%s'\n", hi.getKey(), str->getBuffer());
    }
    @endcode
*/
class ReverseConstHashIterator : public ConstHashIterator {
public:
    //! initializes the iterator with the passed hash
    DLLEXPORT ReverseConstHashIterator(const QoreHashNode* h);

    //! initializes the iterator with the passed hash
    DLLEXPORT ReverseConstHashIterator(const QoreHashNode& h);

    //! Destroys the iterator
    DLLEXPORT ~ReverseConstHashIterator();

    //! moves to the next element in reverse order, returns false when there are no more elements to iterate
    /** also moves to the last element if the object has just been initialized after a complete iteration
         (assuming there is at least one element in the hash)
    */
    DLLEXPORT bool next();

    //! moves to the previous element in reverse order, returns false when there are no more elements to iterate
    /** also moves to the first element if the object has just been initialized after a complete iteration
         (assuming there is at least one element in the hash)
    */
    DLLEXPORT bool prev();

    //! returns true if on the last key of the hash
    DLLEXPORT bool first() const;

    //! returns true if on the first key of the hash
    DLLEXPORT bool last() const;
};

//! use this class to make assignments to hash keys from a pointer to the key value
class HashAssignmentHelper {
friend class hash_assignment_priv;
public:
    //! constructor taking a const char*
    /** @param n_h the hash to use
        @param key the key to assign
        @param must_already_exist if true, then this constructor will only succeed if the key already exists
    */
    DLLEXPORT HashAssignmentHelper(QoreHashNode& n_h, const char* key, bool must_already_exist = false);

    //! constructor taking a const std::string&
    /** @param n_h the hash to use
        @param key the key to assign
        @param must_already_exist if true, then this constructor will only succeed if the key already exists
    */
    DLLEXPORT HashAssignmentHelper(QoreHashNode& n_h, const std::string& key, bool must_already_exist = false);

    //! constructor taking a const QoreString&
    /** this constructor may raise a Qore-language exception if the key argument cannot be successfully converted to the default encoding,
        in which case no further functions should be called on the object
        @param xsink the container object for Qore-language exceptions, in case one is thrown trying to covert the key encoding to the default encoding
        @param n_h the hash to use
        @param key the key to assign
        @param must_already_exist if true, then this constructor will only succeed if the key already exists
    */
    DLLEXPORT HashAssignmentHelper(ExceptionSink* xsink, QoreHashNode& n_h, const QoreString& key, bool must_already_exist = false);

    //! constructor taking a const QoreString&
    /** this constructor may raise a Qore-language exception if the key argument cannot be successfully converted to the default encoding,
        in which case no further functions should be called on the object
        @param xsink the container object for Qore-language exceptions, in case one is thrown trying to covert the key encoding to the default encoding
        @param n_h the hash to use
        @param key the key to assign
        @param must_already_exist if true, then this constructor will only succeed if the key already exists
    */
    DLLEXPORT HashAssignmentHelper(ExceptionSink* xsink, QoreHashNode& n_h, const QoreString* key, bool must_already_exist = false);

    //! constructor taking a HashIterator&
    /** @param hi the HashIterator to use for the key and hash
    */
    DLLEXPORT HashAssignmentHelper(HashIterator &hi);

    //! destroys the object and does post processing on the new value
    DLLEXPORT ~HashAssignmentHelper();

    //! reassigns the object to the given key for a new assignment
    /** @param key the key to assign
        @param must_already_exist if true, then this function will not create a new key

        @since %Qore 0.8.12
    */
    DLLEXPORT void reassign(const char* key, bool must_already_exist = false);

    //! reassigns the object to the given key for a new assignment
    /** @param key the key to assign
        @param must_already_exist if true, then this function will not create a new key

        @since %Qore 0.8.12
    */
    DLLEXPORT void reassign(const std::string& key, bool must_already_exist = false);

    //! returns true if the object is holding a valid pointer, false if not
    /** in case this function returns false
    */
    DLLEXPORT operator bool() const;

    //! assigns a value to the hash key, dereferences any old value, assumes that the value is already referenced for the assignment
    /** a Qore-language exception could be raised when the existing value is dereferenced
        (i.e. if it's an object that goes out of scope and the destructor raises an
        exception, for example)
    */
    DLLEXPORT void assign(QoreValue v, ExceptionSink* xsink);

    //! swaps the current value with the new value of the hash key, assumes that the new value is already referenced for the assignment; returns the old value
    /** could throw a Qore-language exception if there is a type error; in this case 0 is returned and the value passed for the assignment is dereferenced
        @return the old value of the hash key including its reference count (the old value is not dereferenced); the caller owns the value returned
    */
    DLLEXPORT QoreValue swap(QoreValue v, ExceptionSink* xsink);

    //! returns the current value of the hash key; the pointer returned is still owned by the hash
    /** @return the current value of the hash key

        @since %Qore 0.9
    */
    DLLEXPORT QoreValue get() const;

    //! returns the current value of the hash key; any pointer returned is still owned by the hash
    /** @return the current value of the hash key
    */
    DLLEXPORT QoreValue operator*() const;

    //! Returns the current key
    /** @return the current key if the iterator is valid, nullptr if not

        @since %Qore 1.14
    */
    DLLEXPORT const char* getKey() const;

protected:
    //! private implementation
    class hash_assignment_priv *priv;

private:
    DLLLOCAL HashAssignmentHelper(const HashAssignmentHelper&) = delete;
    DLLLOCAL HashAssignmentHelper& operator=(const HashAssignmentHelper&) = delete;
    DLLLOCAL void* operator new(size_t) = delete;
};

#endif // _QORE_HASH_H
