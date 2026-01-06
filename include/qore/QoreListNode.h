/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreListNode.h

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

#ifndef _QORE_QORELISTNODE_H

#define _QORE_QORELISTNODE_H

#include <qore/AbstractQoreNode.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

class ResolvedCallReferenceNode;

//! This is the list container type in Qore, dynamically allocated only, reference counted
/**
   it is both a value type and can hold parse expressions as well (in which case it needs to be evaluated)
   the first element in the list is element 0
 */
class QoreListNode : public AbstractQoreNode {
    friend class StackList;
    friend struct qore_list_private;

private:
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreListNode(const QoreListNode&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreListNode& operator=(const QoreListNode&);

protected:
    //! this structure holds the private implementation for the type
    /** therefore changes to the implementation will not affect the C++ ABI
    */
    struct qore_list_private *priv;

    //! the destructor is protected so it cannot be called directly
    /** use the deref(ExceptionSink) function to release the reference count
        @see AbstractQoreNode::deref()
        @see QoreListNode::derefImpl()
    */
    DLLEXPORT virtual ~QoreListNode();

    //! dereferences all elements of the list
    /** The ExceptionSink argument is needed for those types that could throw
        an exception when they are deleted (ex: QoreObject) - which could be
        contained in the list
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return true if the object can be deleted, false if not (externally-managed)
    */
    DLLEXPORT virtual bool derefImpl(ExceptionSink* xsink);

    //! optionally evaluates the argument
    /** return value requires a deref(xsink) if needs_deref is true
        @see AbstractQoreNode::eval()
    */
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

public:
    //! create an empty list
    DLLEXPORT QoreListNode();

    //! creates an empty list with the given value type
    DLLEXPORT QoreListNode(const QoreTypeInfo* valueTypeInfo);

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
    DLLEXPORT int getAsString(QoreString &str, int foff, ExceptionSink* xsink) const;

    //! returns a QoreString giving the verbose string representation of the List (including all contained values)
    /** used for %n and %N printf formatting
        @param del if this is true when the function returns, then the returned QoreString pointer should be deleted, if false, then it must not be
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        NOTE: Use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly
        @see QoreNodeAsStringHelper
    */
    DLLEXPORT QoreString *getAsString(bool &del, int foff, ExceptionSink* xsink) const;

    //! returns true if the list contains parse expressions and therefore needs evaluation to return a value, false if not
    //DLLEXPORT virtual bool needs_eval() const;

    //! performs a deep copy of the list and returns the new list
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
    DLLEXPORT virtual const char *getTypeName() const;

    //! returns true if the list does not contain any parse expressions, otherwise returns false
    //DLLEXPORT virtual bool is_value() const;

    DLLLOCAL static const char *getStaticTypeName() {
        return "list";
    }

    DLLEXPORT QoreValue& getEntryReference(size_t index);

    //! returns the element at "index" (first element is index 0)
    /** the value is not referenced for the caller
        @param index the index of the element (first element is index 0)
        @return the value of the element at "index", not referenced for the caller
    */
    DLLEXPORT QoreValue retrieveEntry(size_t index);

    //! returns the element at "index" (first element is index 0)
    /** the value is not referenced for the caller
        @param index the index of the element (first element is index 0)
        @return the value of the element at "index", not referenced for the caller
    */
    DLLEXPORT const QoreValue retrieveEntry(size_t index) const;

    //! returns the element at "index" (first element is index 0), the caller owns the reference
    /**
        @param index the index of the element (first element is index 0)
        @return the value of the element at "index" with an incremented reference count for the caller
    */
    DLLEXPORT QoreValue getReferencedEntry(size_t index) const;

    //! returns the value of element at "index" as an integer (first element is index 0)
    /**
        @param index the index of the element (first element is index 0)
    */
    DLLEXPORT int getEntryAsInt(size_t index) const;

    //! sets the value of a list element
    /**
        if there is a value there already, it is dereferenced (hence "xsink" is needed to catch any exceptions)
        @param index the index of the element (first element is index 0)
        @param val the value to set, must be already referenced for the assignment
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT int setEntry(size_t index, QoreValue val, ExceptionSink* xsink);

    //! adds a value to the list
    /** @since %Qore 0.9
    */
    DLLEXPORT int push(QoreValue val, ExceptionSink* xsink);

    //! inserts a value in the first position of the list
    /** @since %Qore 0.9
    */
    DLLEXPORT int insert(QoreValue val, ExceptionSink* xsink);

    //! returns the last element of the list, the length is decremented by one, caller owns the reference
    /** if the list is empty the 0 is returned (NOTE: the last entry could also be 0 as well)

        @since %Qore 0.9
    */
    DLLEXPORT QoreValue pop();

    //! returns the first element of the list, all other entries are moved down to fill up the first position, caller owns the reference
    /** if the list is empty the 0 is returned (NOTE: the first entry could also be 0 as well)
        with the current implementation the execution time for this function is O(n) where n is the length of the list

        @since %Qore 0.9
    */
    DLLEXPORT QoreValue shift();

    //! appends the elements of "list" to this list
    /** @since %Qore 0.8.13
    */
    DLLEXPORT int merge(const QoreListNode* list, ExceptionSink* xsink);

    //! evaluates the list and returns a value (or 0)
    /** return value requires a deref(xsink)
        if the list does not require evaluation then "refSelf()" is used to
        return the same object with an incremented reference count
        NOTE: if the object requires evaluation and there is an exception, 0 will be returned
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreListNode* evalList(ExceptionSink* xsink) const;

    //! optionally evaluates the list
    /** return value requires a deref(xsink) if needs_deref is true
        NOTE: if the list requires evaluation and there is an exception, 0 will be returned
        NOTE: do not use this function directly, use the QoreListNodeEvalOptionalRefHolder class instead
        @param needs_deref this is an output parameter, if needs_deref is true then the value returned must be dereferenced
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @see QoreListNodeEvalOptionalRefHolder
    */
    DLLEXPORT QoreListNode* evalList(bool &needs_deref, ExceptionSink* xsink) const;

    //! performs a deep copy of the list and returns the new list
    DLLEXPORT QoreListNode* copy() const;

    //! performs a deep copy of the list starting from element "offset" and returns the new list
    /** therefore element 0 of the new list is element "offset" in the source list
        @param index the index of the element (first element is index 0)
    */
    DLLEXPORT QoreListNode* copyListFrom(size_t index) const;

    //! returns a new list based on quicksorting the source list ("this")
    /** "soft" comparisons are made using OP_LOG_LT, meaning that the list can be made up of
        different data types and still be sorted
    */
    DLLEXPORT QoreListNode* sort(ExceptionSink* xsink) const;

    //! returns a new list based on quicksorting the source list ("this") using the passed function reference to determine lexical order
    /**
        @param fr the function reference to be executed for each comparison to give lexical order to the elements
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreListNode* sort(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const;

    //! returns a new list based on executing mergesort on the source list ("this")
    /** "soft" comparisons are made using OP_LOG_LT, meaning that the list can be made up of
        different data types and still be sorted
    */
    DLLEXPORT QoreListNode* sortStable(ExceptionSink* xsink) const;

    //! returns a new list based on executing mergesort on the source list ("this") using the passed function reference to determine lexical order
    /**
        @param fr the function reference to be executed for each comparison to give lexical order to the elements
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreListNode* sortStable(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const;

    //! returns a new list based on quicksorting the source list ("this") in descending order
    /** "soft" comparisons are made using OP_LOG_LT, meaning that the list can be made up of
        different data types and still be sorted
    */
    DLLEXPORT QoreListNode* sortDescending(ExceptionSink* xsink) const;

    //! returns a new list based on quicksorting the source list ("this") in descending order, using the passed function reference to determine lexical order
    /**
        @param fr the function reference to be executed for each comparison to give lexical order to the elements
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreListNode* sortDescending(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const;

    //! returns a new list based on executing mergesort on the source list ("this") in descending order
    /** "soft" comparisons are made using OP_LOG_LT, meaning that the list can be made up of
        different data types and still be sorted
    */
    DLLEXPORT QoreListNode* sortDescendingStable(ExceptionSink* xsink) const;

    //! returns a new list based on executing mergesort on the source list ("this") in descending order, using the passed function reference to determine lexical order
    /**
        @param fr the function reference to be executed for each comparison to give lexical order to the elements
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreListNode* sortDescendingStable(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const;

    //! returns the element having the lowest value (determined by calling OP_LOG_LT - the less-than "<" operator)
    /** so "soft" comparisons are made, meaning that the list can be made up of different types, and, as long
        as the comparisons are meaningful, the minimum value can be returned
    */
    DLLEXPORT QoreValue min(ExceptionSink* xsink) const;

    //! returns the element having the highest value (determined by calling OP_LOG_GT - the greater-than ">" operator)
    /** so "soft" comparisons are made, meaning that the list can be made up of different types, and, as long
        as the comparisons are meaningful, the maximum value can be returned
    */
    DLLEXPORT QoreValue max(ExceptionSink* xsink) const;

    //! returns the element having the lowest value (determined by calling the function reference passed to give lexical order)
    /**
        @param fr the function reference to be executed for each comparison to give lexical order to the elements
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue min(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const;

    //! returns the element having the highest value (determined by calling the function reference passed to give lexical order)
    /**
        @param fr the function reference to be executed for each comparison to give lexical order to the elements
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue max(const ResolvedCallReferenceNode* fr, ExceptionSink* xsink) const;

    //! truncates the list at position "offset" (first element is offset 0) and returns any elements removed from the list as a new list
    /**
        @param offset the index of the element (first element is offset 0, negative offsets are offsets from the end of the list)

        @return the elements removed from the list as a new list; if no elements are removed, nullptr is returned

        @note only returns a non-nullptr list if elements were actually removed; this is the only difference between splice() and extract()
    */
    DLLEXPORT QoreListNode* splice(qore_offset_t offset);

    //! removes "length" elements at position "offset" (first element is offset 0) and returns any elements removed from the list as a new list
    /**
        @param offset the index of the element (first element is offset 0, negative offsets are offsets from the end of the list)
        @param length the number of elements to remove (negative numbers mean all except that many elements from the end)

        @return the elements removed from the list as a new list; if no elements are removed, nullptr is returned

        @note only returns a non-nullptr list if elements were actually removed; this is the only difference between splice() and extract()
    */
    DLLEXPORT QoreListNode* splice(qore_offset_t offset, qore_offset_t length);

    //! adds a single value or a list of values ("l") to list possition "offset", while removing "length" elements and returns any elements removed from the list as a new list
    /** the "l" AbstractQoreNode (or each element if it is a QoreListNode) will be referenced for the assignment in the QoreListNode

        @param offset the index of the element (first element is offset 0, negative offsets are offsets from the end of the list)
        @param length the number of elements to remove (negative numbers mean all except that many elements from the end)
        @param l the value or list of values to insert
        @param xsink if an error occurs (ex: the type of value to be inserted in the list is not compatible with the list's declared type), the Qore-language exception information will be added here

        @return the elements removed from the list as a new list; if no elements are removed, nullptr is returned

        @note only returns a non-nullptr list if elements were actually removed; this is the only difference between splice() and extract()
    */
    DLLEXPORT QoreListNode* splice(qore_offset_t offset, qore_offset_t length, const QoreValue l, ExceptionSink* xsink);

    //! truncates the list at position "offset" (first element is offset 0) and returns any elements removed from the list as a new list
    /**
        @param offset the index of the element (first element is offset 0, negative offsets are offsets from the end of the list)

        @return the elements removed from the list as a new list; if no elements are removed, an empty list is returned
    */
    DLLEXPORT QoreListNode* extract(qore_offset_t offset);

    //! removes "length" elements at position "offset" (first element is offset 0) and returns any elements removed from the list as a new list
    /**
        @param offset the index of the element (first element is offset 0, negative offsets are offsets from the end of the list)
        @param length the number of elements to remove (negative numbers mean all except that many elements from the end)

        @return the elements removed from the list as a new list; if no elements are removed, an empty list is returned
    */
    DLLEXPORT QoreListNode* extract(qore_offset_t offset, qore_offset_t length);

    //! adds a single value or a list of values ("l") to list possition "offset", while removing "length" elements and returns any elements removed from the list as a new list
    /**
        @param offset the index of the element (first element is offset 0, negative offsets are offsets from the end of the list)
        @param length the number of elements to remove (negative numbers mean all except that many elements from the end)
        @param l the value or list of values to insert
        @param xsink if an error occurs (ex: the type of value to be inserted in the list is not compatible with the list's declared type), the Qore-language exception information will be added here

        @return the elements removed from the list as a new list; if no elements are removed, an empty list is returned
    */
    DLLEXPORT QoreListNode* extract(qore_offset_t offset, qore_offset_t length, const QoreValue l, ExceptionSink* xsink);

    //! returns the number of elements in the list
    /** return the number of elements in the list
    */
    DLLEXPORT size_t size() const;

    //! returns true if the list is empty
    /** return true if the list is empty
    */
    DLLEXPORT bool empty() const;

    //! returns a list with the order of the elements reversed
    DLLEXPORT QoreListNode* reverse() const;

    //! returns "this" with an incremented reference count
    DLLEXPORT QoreListNode* listRefSelf() const;

    //! returns the value type declaration (if set)
    /** @since %Qore 0.8.13
    */
    DLLEXPORT const QoreTypeInfo* getValueTypeInfo() const;

    //! returns the type info structure for the current value; also works for lists with a specific value type
    /** @since %Qore 0.8.13
    */
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! Increments the weak reference count for this object
    /** @since %Qore 2.0
    */
    DLLEXPORT void weakRef();

    //! Decrements the weak reference count for this object
    /** @return if \c true then the object was deleted

        @since %Qore 2.0
    */
    DLLEXPORT bool weakDeref();

    // needed only while parsing
    //! this function is not exported in the qore library
    DLLLOCAL QoreListNode(bool i);

    //! this function is not exported in the qore library
    DLLLOCAL bool isFinalized() const;

    //! this function is not exported in the qore library
    DLLLOCAL void setFinalized();

    //! this function is not exported in the qore library
    DLLLOCAL bool isVariableList() const;

    //! this function is not exported in the qore library
    DLLLOCAL void setVariableList();

    //! for initialization of lists in the parse tree at parse time (always returns "this")
    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);
};

#include <qore/ReferenceHolder.h>

//! For use on the stack only: manages a QoreListNode reference count
/**
   @see ReferenceHolder
*/
typedef ReferenceHolder<QoreListNode> QoreListNodeHolder;

//! For use on the stack only: iterates through a the elements of a QoreListNode
/**
    @code
    // iterate forward through the list
    ListIterator li(l);
    while (li.next()) {
        QoreStringValueHelper str(li.getValue());
        printf("%d: '%s'\n", li.index(), str->getBuffer());
    }
    @endcode
    @code
    // iterate backwards through the list
    ListIterator li(l);
    while (li.prev()) {
        QoreStringValueHelper str(li.getValue());
        printf("%d: '%s'\n", li.index(), str->getBuffer());
    }
    @endcode
    @see ConstListIterator
*/
class ListIterator {
protected:
    QoreListNode* l;
    qore_offset_t pos;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL void *operator new(size_t);

public:
    //! initializes the iterator to the position given or, if omitted, just before the first element
    /** @param lst the list to iterate
        @param n_pos the starting position (-1 means just before the first element so that the initial call to next() or prev() will put the iterator on the first element)
    */
    DLLEXPORT ListIterator(QoreListNode* lst, size_t n_pos = -1);

    //! initializes the iterator to the position given or, if omitted, just before the first element
    /** @param lst the list to iterate
        @param n_pos the starting position (-1 means just before the first element so that the initial call to next() or prev() will put the iterator on the first element)
    */
    DLLEXPORT ListIterator(QoreListNode& lst, size_t n_pos = -1);

    //! moves the iterator to the next element, returns true if the iterator is pointing to an element of the list
    /** if the iterator is on the last element, it moves to an invalid position before the first element and returns false
        note that a subsequent call to next() after it returns false will move the iterator to the first element again
        (assuming there is at least one element in the list)
        @return returns true if the iterator has been moved to point to a valid element of the list, false if there are no more elements to iterate
    */
    DLLEXPORT bool next();

    //! moves the iterator to the previous element, returns true if the iterator is pointing to an element of the list
    /** if the iterator is on the first element, it moves to an invalid position before the first element and returns false
        note that a subsequent call to prev() after it returns false will move the iterator to the last element again
        (assuming there is at least one element in the list)
        @return returns true if the iterator has been moved to point to a valid element of the list, false if there are no more elements to iterate
        @note after this function returns false, do not use the iterator until it points to a valid element, otherwise a crash will result
    */
    DLLEXPORT bool prev();

    //! sets the iterator to a specific position in the list
    /** In the case an invalid position is given (element not present in the list), the iterator will not be pointing to a valid element in the list
        @param n_pos the position in the list to set (first element is position 0)
        @return 0 for OK, -1 for invalid position
        @note if this function returns -1, do not use the iterator until it points to a valid element, otherwise a crash will result
    */
    DLLEXPORT int set(size_t n_pos);

    //! returns the referenced value of the list element
    /** @since %Qore 0.9
    */
    DLLEXPORT QoreValue getReferencedValue() const;

    //! returns a pointer to the value of the list element
    DLLEXPORT QoreValue getValue() const;

    //! returns true when the iterator is pointing to the first element in the list
    DLLEXPORT bool first() const;

    //! returns true when the iterator is pointing to the last element in the list
    DLLEXPORT bool last() const;

    //! returns the current iterator position in the list or -1 if not pointing at a valid element
    DLLLOCAL size_t index() const { return pos; }

    //! returns the number of elements in the list
    DLLLOCAL size_t max() const { return l->size(); }

    //! returns the list
    DLLLOCAL QoreListNode* getList() { return l; }

    //! returns true if the list is empty
    DLLLOCAL bool empty() const {
        return l->empty();
    }

    //! returns true if the iterator is pointing to a valid element
    DLLLOCAL bool valid() const {
        return pos == -1 ? false : true;
    }
};

//! For use on the stack only: iterates through elements of a const QoreListNode
/**
   @code
   // iterate forward through the list
   ConstListIterator li(l);
   while (li.next()) {
      QoreStringValueHelper str(li.getValue());
      printf("%d: '%s'\n", li.index(), str->getBuffer());
   }
   @endcode
   @code
   // iterate backwards through the list
   ConstListIterator li(l);
   while (li.prev()) {
      QoreStringValueHelper str(li.getValue());
      printf("%d: '%s'\n", li.index(), str->getBuffer());
   }
   @endcode
   @see ListIterator
*/
class ConstListIterator {
protected:
    const QoreListNode* l;
    qore_offset_t pos;

public:
    //! initializes the iterator to the position given or, if omitted, just before the first element
    /** @param lst the list to iterate
        @param n_pos the starting position (-1 means just before the first element so that the initial call to next() or prev() will put the iterator on the first element)
    */
    DLLEXPORT ConstListIterator(const QoreListNode* lst, size_t n_pos = -1);

    //! initializes the iterator to the position given or, if omitted, just before the first element
    /** @param lst the list to iterate
        @param n_pos the starting position (-1 means just before the first element so that the initial call to next() or prev() will put the iterator on the first element)
    */
    DLLEXPORT ConstListIterator(const QoreListNode& lst, size_t n_pos = -1);

    //! moves the iterator to the next element, returns true if the iterator is pointing to an element of the list
    /** if the iterator is on the last element, it moves to an invalid position before the first element and returns false
        note that a subsequent call to next() after it returns false will move the iterator to the first element again
        (assuming there is at least one element in the list)
        @return returns true if the iterator has been moved to point to a valid element of the list, false if there are no more elements to iterate
    */
    DLLEXPORT bool next();

    //! moves the iterator to the previous element, returns true if the iterator is pointing to an element of the list
    /** if the iterator is on the first element, it moves to an invalid position before the first element and returns false
        note that a subsequent call to prev() after it returns false will move the iterator to the last element again
        (assuming there is at least one element in the list)
        @return returns true if the iterator has been moved to point to a valid element of the list, false if there are no more elements to iterate
        @note after this function returns false, do not use the iterator until it points to a valid element, otherwise a crash will result
    */
    DLLEXPORT bool prev();

    //! sets the iterator to a specific position in the list
    /** In the case an invalid position is given (element not present in the list), the iterator will not be pointing to a valid element in the list
        @param n_pos the position in the list to set (first element is position 0)
        @return 0 for OK, -1 for invalid position
        @note if this function returns -1, do not use the iterator until it points to a valid element, otherwise a crash will result
    */
    DLLEXPORT int set(size_t n_pos);

    //! returns the referenced value of the list element
    /** @since %Qore 0.9
    */
    DLLEXPORT QoreValue getReferencedValue() const;

    //! returns a pointer to the value of the list element
    DLLEXPORT const QoreValue getValue() const;

    //! returns true when the iterator is pointing to the first element in the list
    DLLEXPORT bool first() const;

    //! returns true when the iterator is pointing to the last element in the list
    DLLEXPORT bool last() const;

    //! returns the current iterator position in the list or -1 if not pointing at a valid element
    DLLLOCAL size_t index() const { return pos; }

    //! returns the number of elements in the list
    DLLLOCAL size_t max() const { return l->size(); }

    //! returns the list
    DLLLOCAL const QoreListNode* getList() const { return l; }

    //! returns true if the list is empty
    DLLLOCAL bool empty() const {
        return l->empty();
    }

    //! returns true if the iterator is pointing to a valid element
    DLLLOCAL bool valid() const {
        return pos == -1 ? false : true;
    }

    //! resets the iterator to its initial state
    DLLEXPORT void reset();
};

#endif
