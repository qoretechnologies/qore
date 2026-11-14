/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTypeSafeReferenceHelper.h

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

#ifndef _QORE_QORETYPESAFEREFERENCEHELPER_H

#define _QORE_QORETYPESAFEREFERENCEHELPER_H

//! helper class to manage variable references passed to functions and class methods, stack only, cannot be dynamically allocated
/**
    Takes care of safely accessing ReferenceNode objects, for example when they are passed
    as arguments to a builtin function or method.  Locks are automatically acquired in the constructor
    if necessary and released in the destructor.  The constructor could raise a Qore-language
    exception if there is a deadlock acquiring any locks to access the ReferenceNode's value
    as given by the lvalue expression, so the object should be checked for this state right
    after the constructor as in the following example:
    @code
    QoreValue p = get_param_value(args, 0);
    if (p.getType() == NT_REFERENCE) {
        const ReferenceNode *r = p.get<const ReferenceNode>();
        QoreTypeSafeReferenceHelper ref(r, xsink);
        // a deadlock exception occurred accessing the reference's value pointer
        if (!ref)
            return QoreValue();

        // more code to access the reference
    }
    @endcode
 */
class QoreTypeSafeReferenceHelper {
friend struct qore_type_safe_ref_helper_priv_t;
public:
    //! Initializes the object without a reference
    /** After this, use set(const ReferenceNode* ref)
    */
    DLLEXPORT QoreTypeSafeReferenceHelper(ExceptionSink* xsink);

    //! initializes the object and tries to get the pointer to the pointer of the lvalue expression target
    /** @param ref the ReferenceNode to use
        @param xsink Qore-language exceptions raised will be added here (for example, a deadlock accessing the object)
    */
    DLLEXPORT QoreTypeSafeReferenceHelper(const ReferenceNode* ref, ExceptionSink* xsink);

    //! destroys the object
    DLLEXPORT ~QoreTypeSafeReferenceHelper();

    //! Assigns the reference to use
    /** Only to be used after QoreTypeSafeReferenceHelper(ExceptionSink* xsink)
    */
    DLLEXPORT int set(const ReferenceNode* ref);

    //! Clears any existing reference and assigns the reference to use
    /** @return -1 for error (%Qore-language exception raised), or 0 for success
    */
    DLLEXPORT int reset(const ReferenceNode* ref, ExceptionSink* xsink);

    //! returns true if the reference is valid, false if not
    /** Returns false after a failed binding or after removeHashObjKey() invokes a native member gate or accessor.
        After a native removal, use reset() before accessing a value again, or close() when finished.
    */
    DLLEXPORT operator bool() const;

    //! returns the type of the reference's value
    /** @return the type of the reference's value
    */
    DLLEXPORT qore_type_t getType() const;

    //! returns the type name of the reference's value
    /** @return the type name of the reference's value
    */
    DLLEXPORT const char* getTypeName() const;

    //! returns a pointer to the value with a unique reference count (so it can be updated in place)
    /** assumes the reference is valid and the lvalue holds a reference-counted AbstractQoreNode*, if not, this call
        will cause a segfault

        @returns a pointer to the reference's value with a unique reference count (so it can be modified), or 0 if the
        value was 0 to start with or if a Qore-language exception was raised
        @note you must check that the reference is valid before calling this function
        @note take care to only call this function on types where the AbstractQoreNode::realCopy() function has a
        valid implementation (on all value types suitable for in-place modification this function has a valid
        implementation), as in debugging builds other types will abort(); in non-debugging builds this function will
        cause a segfault

        @code
        QoreTypeSafeReferenceHelper rh(ref, xsink);
        // if the reference is not valid, then return
        if (!rh)
            return;
        if (rh.getType() == NT_LIST) {
            // get the unique value
            QoreListNode* l = static_cast<QoreListNode*>(rh.getUnique(xsink));
            // if a Qore-language exception was raised, then return
            if (*xsink)
                return;
        }
        @endcode
    */
    DLLEXPORT AbstractQoreNode* getUnique();

    //! returns a pointer to the value with a unique reference count (so it can be updated in place)
    /** assumes the reference is valid and the lvalue holds a reference-counted AbstractQoreNode*, if not, this call
        will cause a segfault

        @returns a pointer to the reference's value with a unique reference count (so it can be modified), or 0 if the
        value was 0 to start with or if a Qore-language exception was raised
        @note you must check that the reference is valid before calling this function
        @note take care to only call this function on types where the AbstractQoreNode::realCopy() function has a
        valid implementation (on all value types suitable for in-place modification this function has a valid
        implementation), as in debugging builds other types will abort(); in non-debugging builds this function will
        cause a segfault

        @code
        QoreTypeSafeReferenceHelper rh(ref, xsink);
        // if the reference is not valid, then return
        if (!rh)
            return;
        if (rh.getType() == NT_LIST) {
            // get the unique value
            QoreListNode* l = static_cast<QoreListNode*>(rh.getUnique(xsink));
            // if a Qore-language exception was raised, then return
            if (*xsink)
                return;
        }
        @endcode
    */
    template <typename T>
    DLLLOCAL T* getUnique() {
        T* rv = reinterpret_cast<T*>(getUnique());
        assert(!rv || dynamic_cast<T*>(rv));
        return rv;
    }

    //! assigns a value to the reference, assumes the reference is valid
    /** @param val the value to assign (must be already referenced for the assignment)
        @return 0 if there was no error and the variable was assigned, -1 if a Qore-language exception occured
        dereferencing the current value, in this case no assignment was made and the reference count for val is
        dereferenced automatically by the QoreTypeSafeReferenceHelper object
        @note you must check that the reference is valid before calling this function
        @code
        QoreTypeSafeReferenceHelper rh(ref, xsink);
        // if the reference is not valid, then return
        if (!rh)
            return;
        // make the assignment (if the assignment fails, the value will be dereferenced automatically)
        rh.assign(val->refSelf());
        @endcode
    */
    DLLEXPORT int assign(QoreValue val);

    //! returns the reference's value
    /** @return the value of the lvalue reference
    */
    DLLEXPORT const QoreValue getValue() const;

    //! Returns the variable lock
    DLLEXPORT AutoVLock& getVLock() const;

    //! Closes the object and unlocks all locks
    /** After this call, the object cannot be used
    */
    DLLEXPORT void close();

    //! Moves the reference to the given key of the given object
    DLLEXPORT int setObjKey(QoreObject* obj, const char* key, bool for_remove = false);

    //! Moves the reference to the given key of the hash or object of the current value
    DLLEXPORT int moveToHashObjKey(const char* key, bool for_remove = false);

    //! Removes the given key of the current hash or object
    /** @param key the hash key or object member to remove
        @return the removed value, with a new reference
        @throw REMOVE-KEY-ERROR the current value is neither a hash nor an object
        @throw an exception raised by member access checks or a native member callback

        @note A removal through a native member gate or accessor invalidates this helper and releases its locks
        before calling the module: the callback may replace or remove the parent lvalue. Afterwards operator bool()
        and isLocked() return false; call reset(ref, xsink) before further value access, or close() when finished.
        The parent is not reacquired automatically, and the callback's changes are preserved. Ordinary hash keys
        and ordinary Qore members do not invalidate the helper.
    */
    DLLEXPORT QoreValue removeHashObjKey(const char* key);

    //! Returns the ExceptionSink object
    DLLEXPORT ExceptionSink* getExceptionSink() const;

    //! Returns the type of the reference
    DLLEXPORT const QoreTypeInfo* getReferenceTypeInfo() const;

    //! Returns true if managing a lock
    DLLEXPORT bool isLocked() const;

private:
    DLLLOCAL QoreTypeSafeReferenceHelper(const QoreTypeSafeReferenceHelper&) = delete;
    DLLLOCAL QoreTypeSafeReferenceHelper& operator=(const QoreTypeSafeReferenceHelper&) = delete;
    DLLLOCAL void* operator new(size_t) = delete;

    //! private implementation
    struct qore_type_safe_ref_helper_priv_t* priv;
};

#endif
