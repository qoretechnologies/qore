/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreObject.h

    thread-safe object definition

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

#ifndef _QORE_QOREOBJECT_H

#define _QORE_QOREOBJECT_H

class AutoVLock;
class VRMutex;
class BuiltinCopy;
class BuiltinNormalMethodVariantBase;
class BuiltinCopyVariantBase;
class QoreExternalMethodVariant;
class QoreProgram;
class ReferenceNode;

//! the implementation of Qore's object data type, reference counted, dynamically-allocated only
/** objects in Qore are unique unless copied explicitly (similar to Java)
    Builtin classes (those classes implemented in C++ as opposed to user classes implemented in the Qore language)
    can have "private data", which is data that maintains the state of the object per that class.  QoreObject
    objects store this data as well as any member data.

    objects have two levels of reference counts - one is for the existence of the c++ object (tRefs below)
    the other is for the scope of the object (the parent QoreReferenceCounter) - when this reaches 0 the
    object will have its destructor run (if it hasn't already been deleted).
    Only when the tRef counter reaches 0, meaning that no more pointers are pointing to this object will the
    object actually be deleted.

    @see QoreClass
*/
class QoreObject : public AbstractQoreNode {
    friend class qore_object_private;
    friend class ObjectRSetHelper;
    friend class ObjectRSet;
    friend class qore_object_dereference_helper;

public:
    //! creates an object as belonging to the given class, the QoreProgram object is referenced for the life of the object as well
    /**
        @param oc the class of the object being created
        @param p the QoreProgram object where the object "lives", this QoreProgram object is referenced for the life of the object to ensure that it is not deleted while the object still exists (for example, if the object is exported to a parent QoreProgram object)
    */
    DLLEXPORT QoreObject(const QoreClass* oc, QoreProgram* p);

    //! creates an object as belonging to the given class, the QoreProgram object is referenced for the life of the object as well, and the private data is stored with the class ID of the class
    /**
        @param oc the class of the object being created
        @param p the QoreProgram object where the object "lives", this QoreProgram object is referenced for the life of the object to ensure that it is not deleted while the object still exists (for example, if the object is exported to a parent QoreProgram object)
        @param data the private data corresponding to the class ID of the class passed
    */
    DLLEXPORT QoreObject(const QoreClass* oc, QoreProgram* p, AbstractPrivateData* data);

    //! returns true if the object has the given member (note that the member may not have a value)
    DLLEXPORT bool hasMember(const char* mem, ExceptionSink* xsink) const;

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
    DLLEXPORT virtual int getAsString(QoreString &str, int foff, ExceptionSink* xsink) const;

    //! returns a QoreString giving the verbose string representation of the List (including all contained values)
    /** used for %n and %N printf formatting
        @param del if this is true when the function returns, then the returned QoreString pointer should be deleted, if false, then it must not be
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        NOTE: Use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly
        @see QoreNodeAsStringHelper
    */
    DLLEXPORT virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    //! performs a the same object with its reference count increased
    DLLEXPORT virtual AbstractQoreNode* realCopy() const;

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
    DLLEXPORT virtual const char* getTypeName() const;

    //! returns the type name (useful in templates)
    DLLLOCAL static const char* getStaticTypeName() {
        return "object";
    }

    //! returns the type code (useful in templates)
    DLLLOCAL static qore_type_t getStaticTypeCode() {
        return NT_OBJECT;
    }

    //! returns true if this object is a valid instance of the classid passed
    /** @param cid the class ID to check

        @deprecated use validInstanceOf(const QoreClass&) instead
    */
    DLLEXPORT bool validInstanceOf(qore_classid_t cid) const;

    //! returns true if this object is a valid instance of the classid passed
    /** @param qc the class to check
    */
    DLLEXPORT bool validInstanceOf(const QoreClass& qc) const;

    //! returns true if this object is a valid instance of the classid passed; does not check for injected compatibility
    /** @param qc the class to check

        @since %Qore 1.0.3
    */
    DLLEXPORT bool validInstanceOfStrict(const QoreClass& qc) const;

    //! sets the value of the given member to the given value
    /** the value must be already referenced for the assignment to the object
        @param key the name of the member
        @param val the value to set for the member (must be already referenced for the assignment, 0 is OK too)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void setValue(const char* key, QoreValue val, ExceptionSink* xsink);

    //! returns the list of members, caller owns the list returned
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreListNode* getMemberList(ExceptionSink* xsink) const;

    //! removes a member from the object, if the member's value is an object it is deleted as well (destructor is called)
    /**
        @param key the name of the member to delete
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void deleteMemberValue(const QoreString* key, ExceptionSink* xsink);

    //! removes a member from the object, if the member's value is an object it is deleted as well (destructor is called)
    /**
        @param key the name of the member to delete, assumed to be in the default encoding (QCS_DEFAULT)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void deleteMemberValue(const char* key, ExceptionSink* xsink);

    //! removes a member from the object without explicitly calling destructors; the value is only dereferenced
    /** objects will be destructed if they go out of scope, however
        @param key the name of the member to remove
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void removeMember(const QoreString* key, ExceptionSink* xsink);

    //! removes a member from the object without explicitly calling destructors; the value is only dereferenced
    /** objects will be destructed if they go out of scope, however
        @param key the name of the member to remove, assumed to be in the default encoding (QCS_DEFAULT)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void removeMember(const char* key, ExceptionSink* xsink);

    //! removes a member from the object without explicitly calling destructors and returns the value removed; the caller owns the reference returned
    /** if a Qore-language exception is raised, the return value is always 0
        @param key the name of the member to remove
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return the value removed from the object; caller owns the reference
    */
    DLLEXPORT QoreValue takeMember(const QoreString* key, ExceptionSink* xsink);

    //! removes a member from the object without explicitly calling destructors and returns the value removed; the caller owns the reference returned
    /** if a Qore-language exception is raised, the return value is always 0
        @param key the name of the member to remove, assumed to be in the default encoding (QCS_DEFAULT)
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return the value removed from the object; caller owns the reference
    */
    DLLEXPORT QoreValue takeMember(const char* key, ExceptionSink* xsink);

    //! returns the number of members of the object
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT int size(ExceptionSink* xsink) const;

    //! tests for equality ("deep compare" including all contained values) with possible type conversion of contained elements (soft compare)
    /**
        @param obj the object to compare
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT bool compareSoft(const QoreObject* obj, ExceptionSink* xsink) const;

    //! tests for equality ("deep compare" including all contained values) with possible type conversion of contained elements (hard compare)
    /**
        @param obj the object to compare
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT bool compareHard(const QoreObject* obj, ExceptionSink* xsink) const;

    //! returns the value of the given member with the reference count incremented, the caller owns any reference returned
    /**
        @param mem the name member to retrieve the value for
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return the value of the given member with the reference count incremented, the caller owns any reference returned
    */
    DLLEXPORT QoreValue getReferencedMemberNoMethod(const char* mem, ExceptionSink* xsink) const;

    //! Returns a reference to the given member, if the member is valid and accessible
    /**
        @param mem the name member to retrieve the reference for
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a reference to the given member; the caller owns any reference returned
    */
    DLLEXPORT ReferenceNode* getReferenceToMember(const char* mem, ExceptionSink* xsink);

    //! returns the value of the given member as an int64
    /**
        @param mem the name member to retrieve the value for
        @param found returns true if the member was found, false if not
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return the value of the given member as an int64
    */
    DLLEXPORT int64 getMemberAsBigInt(const char* mem, bool& found, ExceptionSink* xsink) const;

    //! retuns all member data of the object (or 0 if there's an exception), caller owns the QoreHashNode reference returned
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreHashNode* copyData(ExceptionSink* xsink) const;

    //! sets private data for the object against the class ID passed, used in C++ functions implementing Qore constructors
    /**
        @param key the class ID of the class to set the private data for
        @param pd the private data for the given class ID
    */
    DLLEXPORT void setPrivate(qore_classid_t key, AbstractPrivateData* pd);

    //! returns the private data corresponding to the class ID passed with an incremented reference count, caller owns the reference
    /**
        @param key the class ID of the class to get the private data for
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT AbstractPrivateData* getReferencedPrivateData(qore_classid_t key, ExceptionSink* xsink) const;

    //! returns the private data corresponding to the class ID passed with an incremented reference count, caller owns the reference
    /**
        @param key the class ID of the class to get the private data for
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @since %Qore 1.12
    */
    template <class T>
    DLLLOCAL T* getReferencedPrivateData(qore_classid_t key, ExceptionSink* xsink) const {
        AbstractPrivateData* rv = getReferencedPrivateData(key, xsink);
        assert(!rv || dynamic_cast<T*>(rv));
        return reinterpret_cast<T*>(rv);
    }

    //! returns the private data corresponding to the class ID passed with an incremented reference count if it exists, caller owns the reference
    /**
        @param key the class ID of the class to get the private data for
        @param xsink if an error occurs (the object has already been deleted), the Qore-language exception information will be added here

        @since %Qore 0.8.13
    */
    DLLEXPORT AbstractPrivateData* tryGetReferencedPrivateData(qore_classid_t key, ExceptionSink* xsink) const;

    //! returns the private data corresponding to the class ID passed with an incremented reference count if it exists, caller owns the reference
    /**
        @param key the class ID of the class to get the private data for
        @param xsink if an error occurs (the object has already been deleted), the Qore-language exception information will be added here

        @since %Qore 1.12
    */
    template <class T>
    DLLLOCAL T* tryGetReferencedPrivateData(qore_classid_t key, ExceptionSink* xsink) const {
        AbstractPrivateData* rv = tryGetReferencedPrivateData(key, xsink);
        assert(!rv || dynamic_cast<T*>(rv));
        return reinterpret_cast<T*>(rv);
    }

    //! evaluates the given method with the arguments passed and returns the return value, caller owns any reference returned
    /**
        @param name the name of the method to evaluate
        @param args the arguments for the method (may be 0)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue evalMethod(const QoreString* name, const QoreListNode* args, ExceptionSink* xsink);

    //! evaluates the given method with the arguments passed and returns the return value, caller owns any reference returned
    /**
        @param name the name of the method to evaluate, must be in QCS_DEFAULT encoding
        @param args the arguments for the method (may be 0)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue evalMethod(const char* name, const QoreListNode* args, ExceptionSink* xsink);

    //! evaluates the given method with the arguments passed and returns the return value, caller owns the AbstractQoreNode (reference) returned
    /**
        @param method the method to evaluate
        @param args the arguments for the method (may be 0)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue evalMethod(const QoreMethod& method, const QoreListNode* args, ExceptionSink* xsink);

    //! evaluates the given method with the given class context and arguments passed and returns the return value
    /** The caller owns the AbstractQoreNode (reference) returned

        @param name the method to evaluate
        @param class_ctx the class context for the method evaluation for evaluating private:internal methods
        @param args the arguments for the method (may be 0)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the value returned by the method; the caller owns any reference returned

        @since %Qore 2.0
    */
    DLLEXPORT QoreValue evalMethod(const char* name, const QoreClass* class_ctx, const QoreListNode* args,
            ExceptionSink* xsink);

    //! evaluates the given method with the given class context and arguments passed and returns the return value, caller owns the AbstractQoreNode (reference) returned
    /**
        @param method the method to evaluate
        @param class_ctx the class context for the method evaluation for evaluating private:internal methods
        @param args the arguments for the method (may be 0)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the value returned by the method; the caller owns any reference returned

        @since %Qore 0.9
    */
    DLLEXPORT QoreValue evalMethod(const QoreMethod& method, const QoreClass* class_ctx, const QoreListNode* args, ExceptionSink* xsink);

    //! runs the destructor on the object (if it hasn't already been deleted)
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void doDelete(ExceptionSink* xsink);

    //! returns a pointer to a QoreClass object if the class ID passed is a valid class in the hierarchy
    /**
        @param cid the class ID passed
        @return a pointer to a QoreClass object if the class ID passed is a valid class in the hierarchy
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

    //! returns the accessibility of the class in the object's hierachy or Inaccessible the object does not inherit the class at all
    /** @param cls the class to check

        @return the accessibility of the class in the object's hierachy or Inaccessible the object does not inherit the class at all

        @since %Qore 0.9
    */
    DLLEXPORT ClassAccess getClassAccess(const QoreClass& cls) const;

    //! returns a pointer to the QoreClass of this object
    /**
        @return a pointer to the QoreClass of this object
    */
    DLLEXPORT const QoreClass* getClass() const;

    //! returns the name of the class
    /**
        @return the name of the class
    */
    DLLEXPORT const char* getClassName() const;

    //! returns a pointer to the QoreClass of this object or to the injection target class in case the class was injected
    /**
        @return a pointer to the QoreClass of this object or to the injection target class in case the class was injected

        If the class was not injected, this this method returns the same value as getClass()

        @since %Qore 1.0.3
    */
    DLLEXPORT const QoreClass* getSurfaceClass() const;

    //! returns the name of the class or to the name of the injection target class in case the class was injected
    /**
        @return the name of the class or to the name of the injection target class in case the class was injected

        If the class was not injected, this this method returns the same value as getClassName()

        @since %Qore 1.0.3
    */
    DLLEXPORT const char* getSurfaceClassName() const;

    //! returns true if the object is valid
    /**
        @return true if the object is valid
    */
    DLLEXPORT bool isValid() const;

    //! returns true if the object is valid and not being destroyed
    /** Unlike isValid(), this method returns false while the destructor is running.
        This is useful for detecting objects that are in the process of being deleted
        but whose isValid() would still return true.

        @return true if the object is valid and not in the process of being destroyed

        @since %Qore 2.3
    */
    DLLEXPORT bool isCurrent() const;

    //! returns the QoreProgram object associated with this object
    /** for system objects only (created with the system constructor) this will be 0
        @return the QoreProgram object associated with this object
    */
    DLLEXPORT QoreProgram* getProgram() const;

    //! returns true if the object is a system object (created with the system constructor)
    /**
        @return true if the object is a system object (created with the system constructor)
    */
    DLLEXPORT bool isSystemObject() const;

    //! increments the existence reference count
    /** Will not prolong the scope of the object (use QoreObject::ref() to prolong the scope of the object).
        To derecrement the count, call QoreObject::tDeref()
        @see QoreObject::tDeref()
        @see QoreObject::ref()
    */
    DLLEXPORT void tRef() const;

    //! decrements the existence reference count, when it reaches 0 the C++ object ("this") will be deleted
    /** To increment the existence reference count, call QoreObject::tRef()
        @see QoreObject::tRef()
    */
    DLLEXPORT void tDeref();

    //! increments the standard reference count of the object for references that cannot be part of a recursive graph
    DLLEXPORT void realRef();

    //! decrements the standard reference count of the object for references that cannot be part of a recursive graph
    DLLEXPORT void realDeref(ExceptionSink* xsink);

    //! returns the pointer to the value of the member
    /** if the member exists, the lock is held and added to the AutoVLock "vl", otherwise the lock is released
        an exception will be thrown if the character encoding conversion fails
        also if the object has a deleted status an exception will be thrown
        NOTE: the value returned is not referenced by this function, but rather the object is locked
        @param key the name of the member
        @param vl the AutoVLock container for nested locking
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue getMemberValueNoMethod(const QoreString* key, AutoVLock* vl, ExceptionSink* xsink) const;

    //! returns the pointer to the value of the member
    /**
        NOTE: the value returned is not referenced by this function, but rather the object is locked
        @param key the name of the member, assumed to be in the default encoding (QCS_DEFAULT)
        @param vl the AutoVLock container for nested locking
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue getMemberValueNoMethod(const char* key, AutoVLock* vl, ExceptionSink* xsink) const;

    //! returns the pointer to the value of the member
    /**
        NOTE: the value returned is not referenced by this function, but rather the object is locked
        @param key the name of the member, assumed to be in the default encoding (QCS_DEFAULT)
        @param cls the class context for private:internal members; may be nullptr
        @param vl the AutoVLock container for nested locking
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @since %Qore 2.0
    */
    DLLEXPORT QoreValue getMemberValueNoMethod(const char* key, const QoreClass* cls, AutoVLock* vl,
            ExceptionSink* xsink) const;

    //! returns the value of the given member as accessed from the given class; caller owns any reference returned
    /** @param key the name of the member, assumed to be in the default encoding (QCS_DEFAULT)
        @param cls the class context for private:internal members; may be nullptr
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @since %Qore 0.9
    */
    DLLEXPORT QoreValue getReferencedMemberNoMethod(const char* key, const QoreClass* cls, ExceptionSink* xsink) const;

    //! sets the value of the given member as accessed from the given class
    /** The reference in \a val is transferred to the object (consuming semantics); callers
        that wish to retain a reference to the value must pass \c val.refSelf() instead.
        This matches the ownership contract of setValue() and avoids leaks when passing
        freshly-allocated heap values.

        @param key the name of the member, assumed to be in the default encoding (QCS_DEFAULT)
        @param cls the class context for private:internal members; may be nullptr
        @param val the value to set; its reference is consumed by this call
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 = no errors, -1 = Qore-language exception raised

        @since %Qore 0.9
    */
    DLLEXPORT int setMemberValue(const char* key, const QoreClass* cls, QoreValue val, ExceptionSink* xsink);

    //! call this function when an object's private data is deleted externally
    /** this function will clear the private data and delete the object
        @param key the class ID of the class that owns the private data
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void externalDelete(qore_classid_t key, ExceptionSink* xsink);

    //! executes a normal object method variant
    DLLEXPORT QoreValue evalMethodVariant(const QoreMethod& method, const QoreExternalMethodVariant* variant,
            const QoreListNode* args, ExceptionSink* xsink);

    //! executes a normal object method variant
    /** @since %Qore 0.9
    */
    DLLEXPORT QoreValue evalMethodVariant(const QoreMethod& method, const QoreClass* class_ctx,
                const QoreExternalMethodVariant* variant, const QoreListNode* args, ExceptionSink* xsink);

    //! executes a static method
    /** @since %Qore 0.9
    */
    DLLEXPORT static QoreValue evalStaticMethod(const QoreMethod& method, const QoreListNode* args, ExceptionSink* xsink);

    //! executes a static method with the given class context to access private:internal methods
    /** @since %Qore 0.9
    */
    DLLEXPORT static QoreValue evalStaticMethod(const QoreMethod& method, const QoreClass* class_ctx,
            const QoreListNode* args, ExceptionSink* xsink);

    //! executes a static method variant
    /** @since %Qore 0.9
    */
    DLLEXPORT static QoreValue evalStaticMethodVariant(const QoreMethod& method,
            const QoreExternalMethodVariant* variant, const QoreListNode* args, ExceptionSink* xsink);

    //! executes a static method variant with the given class context to access private:internal methods
    /** @since %Qore 0.9
    */
    DLLEXPORT static QoreValue evalStaticMethodVariant(const QoreMethod& method, const QoreClass* class_ctx,
            const QoreExternalMethodVariant* variant, const QoreListNode* args, ExceptionSink* xsink);

    //! returns the value of the member with an incremented reference count, or executes the memberGate() method and returns the value
    /**
        @param member the name of the member to get the value for (or evaluate the memberGate() method against)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @since %Qore 0.9.5 part of the public API
    */
    DLLEXPORT QoreValue evalMember(const QoreString* member, ExceptionSink* xsink);

    //! returns the value of the member with an incremented reference count, or executes the memberGate() method and returns the value
    /**
        @param member the name of the member to get the value for (or evaluate the memberGate() method against); must
        be in the default encoding (normally UTF-8)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @since %Qore 0.9.5
    */
    DLLEXPORT QoreValue evalMember(const char* member, ExceptionSink* xsink);

    //! Returns the object with the reference count increased
    /** @since %Qore 1.12
    */
    DLLLOCAL QoreObject* objectRefSelf() const {
        ref();
        return const_cast<QoreObject*>(this);
    }

    DLLLOCAL int getStatus() const;

    DLLLOCAL class KeyNode* getReferencedPrivateDataNode(qore_classid_t key);

    //! retrieves the private data pointer and clears it from the object's private data store, used when executing destructors
    /**
        @param key the class key to use
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLLOCAL AbstractPrivateData* getAndClearPrivateData(qore_classid_t key, ExceptionSink* xsink);

    //! called to evaluate a builtin method when private data is available
    /**
        @param method a constant reference to the QoreMethod object
        @param meth the name of the method to evalute
        @param args the arguments for the method
        @param rc runtime configuration for the call
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLLOCAL QoreValue evalBuiltinMethodWithPrivateData(const QoreMethod& method,
            const BuiltinNormalMethodVariantBase* meth, const QoreListNode* args, RuntimeConfig& rc,
            ExceptionSink* xsink);

    //! called on the old object (this) to acquire private data, copy method called with pointer to "self" (new copy)
    DLLLOCAL void evalCopyMethodWithPrivateData(const QoreClass &thisclass, const BuiltinCopyVariantBase* meth,
            QoreObject* self, RuntimeConfig& rc, ExceptionSink* xsink);

    //! concatenates info about private data to a string
    /**
        @param str the string to concatenate to
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLLOCAL void addPrivateDataToString(QoreString* str, ExceptionSink* xsink) const;

    //! Destroys all members and dereferences all private data structures
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLLOCAL void obliterate(ExceptionSink* xsink);

    //! runs the destructor for system objects
    /**
        @param classID the ID of the class
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLLOCAL void defaultSystemDestructor(qore_classid_t classID, ExceptionSink* xsink);

    // returns a new hash consisting of just the members of value_list
    DLLLOCAL QoreHashNode* getSlice(const QoreListNode* value_list, ExceptionSink* xsink) const;

    //! creates the object with the initial data passed as "d", used by the copy constructor
    DLLLOCAL QoreObject(const QoreClass* oc, QoreProgram* p, QoreHashNode* d);

    //! returns true if the class has a memberNotification method
    DLLLOCAL bool hasMemberNotification() const;

    //! executes the member notification on the object the given member
    DLLLOCAL void execMemberNotification(const char* member, ExceptionSink* xsink);

protected:
    //! runs the destructor if necessary and dereferences all members
    /** Note that other objects could be deleted as well if they
        are members of this object, any exceptions thrown there will also be added to "xsink"
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT virtual bool derefImpl(ExceptionSink* xsink);

    //! should never be called, does nothing
    /** in debugging builds calls to this function will abort
    */
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    //! custom reference handler
    DLLLOCAL virtual void customRef() const;

    //! custom dereference handler - with delete
    DLLLOCAL virtual void customDeref(ExceptionSink* xsink);

    //! destructor
    DLLLOCAL virtual ~QoreObject();

private:
    //! the private implementation of the class
    class qore_object_private* priv;

    //! called at parse time
    DLLLOCAL int parseInit(QoreValue& val, QoreParseContext& parse_context);

    QoreObject(const QoreObject& old) = delete;
    QoreObject& operator=(const QoreObject&) = delete;
};

//! Convenience class for holding temporary / weak references to objects
/** @since %Qore 1.12
*/
class QoreObjectWeakRefHolder {
public:
    DLLLOCAL QoreObjectWeakRefHolder() {
    }

    DLLLOCAL QoreObjectWeakRefHolder(QoreObject* obj) : obj(obj) {
        if (obj) {
            obj->tRef();
        }
    }

    DLLLOCAL ~QoreObjectWeakRefHolder() {
        discard();
    }

    DLLLOCAL QoreObject* release() {
        QoreObject* rv = obj;
        obj = nullptr;
        return rv;
    }

    DLLLOCAL void reset(QoreObject* obj = nullptr) {
        discard();
        this->obj = obj;
    }

    DLLLOCAL QoreObject* operator*() {
        return obj;
    }

    DLLLOCAL const QoreObject* operator*() const {
        return obj;
    }

    DLLLOCAL QoreObject* operator->() {
        return obj;
    }

    DLLLOCAL const QoreObject* operator->() const {
        return obj;
    }

    DLLLOCAL operator bool() {
        return (bool)obj;
    }

    DLLLOCAL operator bool() const {
        return (bool)obj;
    }

private:
    QoreObject* obj = nullptr;

    DLLLOCAL void discard() {
        if (obj) {
            obj->tDeref();
        }
    }

    void* operator new(size_t) = delete;
    QoreObjectWeakRefHolder(const QoreObjectWeakRefHolder& old) = delete;
    QoreObjectWeakRefHolder& operator=(QoreObjectWeakRefHolder& orig) = delete;
};

//! convenience class for holding AbstractPrivateData references
template <class T>
class PrivateDataRefHolder : public ReferenceHolder<T> {
public:
    DLLLOCAL PrivateDataRefHolder(ExceptionSink* xsink) : ReferenceHolder<T>(xsink) {
    }

    DLLLOCAL PrivateDataRefHolder(const QoreObject* o, qore_classid_t cid, ExceptionSink* xsink) : ReferenceHolder<T>(reinterpret_cast<T*>(o->getReferencedPrivateData(cid, xsink)), xsink) {
    }

    //! assigns a new pointer to the holder, dereferences the current pointer if any
    DLLLOCAL void operator=(T *nv) {
        if (this->p)
            this->p->deref(this->xsink);
        this->p = nv;
    }
};

//! convenience class for holding AbstractPrivateData references
template <class T>
class TryPrivateDataRefHolder : public ReferenceHolder<T> {
public:
    DLLLOCAL TryPrivateDataRefHolder(const QoreObject* o, qore_classid_t cid, ExceptionSink* xsink) : ReferenceHolder<T>(reinterpret_cast<T*>(o->tryGetReferencedPrivateData(cid, xsink)), xsink) {
    }

    //! assigns a new pointer to the holder, dereferences the current pointer if any
    DLLLOCAL void operator=(T *nv) {
        if (this->p)
            this->p->deref(this->xsink);
        this->p = nv;
    }
};

class QorePrivateObjectAccessHelper {
public:
    DLLLOCAL QorePrivateObjectAccessHelper(ExceptionSink* xs) : xsink(xs), ptr(0) {
    }

    DLLLOCAL operator bool() const {
        return (bool)ptr;
    }

private:
    DLLLOCAL QorePrivateObjectAccessHelper(const QorePrivateObjectAccessHelper&) = delete;
    DLLLOCAL QorePrivateObjectAccessHelper& operator=(const QorePrivateObjectAccessHelper&) = delete;
    DLLLOCAL void* operator new(size_t) = delete;

protected:
    ExceptionSink* xsink;
    void* ptr;
};

//! RAII helper that holds a strong ("real") reference to a QoreObject
/** Calls realRef() in the constructor and realDeref() in the destructor.
    Used to keep an object alive during a method call on it.

    @since %Qore 2.1
*/
class QoreObjectRealRefHelper {
public:
    DLLLOCAL QoreObjectRealRefHelper(QoreObject* obj, ExceptionSink* xsink) : obj(obj), xsink(xsink) {
        obj->realRef();
    }

    DLLLOCAL ~QoreObjectRealRefHelper() {
        obj->realDeref(xsink);
    }

private:
    DLLLOCAL QoreObjectRealRefHelper(const QoreObjectRealRefHelper&) = delete;
    DLLLOCAL QoreObjectRealRefHelper& operator=(const QoreObjectRealRefHelper&) = delete;
    DLLLOCAL void* operator new(size_t) = delete;

    QoreObject* obj;
    ExceptionSink* xsink;
};

#endif
