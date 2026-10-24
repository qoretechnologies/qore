/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    BinaryNode.h

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

#ifndef _QORE_BINARYNODE_H

#define _QORE_BINARYNODE_H

#include <qore/AbstractQoreNode.h>

//! holds arbitrary binary data
/** this class is implemented simply as a pointer and a length indicator
 */
class BinaryNode : public SimpleValueQoreNode {
private:
    //! pointer to memory owned by the object
    void* ptr;
    //! size of the memory block owned by the object
    size_t len;

    // not yet implemented
    DLLLOCAL BinaryNode(const BinaryNode&);
    DLLLOCAL BinaryNode& operator=(const BinaryNode&);

protected:
    //! frees and memory owned by the object
    DLLEXPORT virtual ~BinaryNode();

    DLLLOCAL void checkOffset(qore_offset_t& offset) const;
    DLLLOCAL void checkOffset(qore_offset_t& offset, qore_offset_t& num) const;

public:
    //! creates the object
    /** @param p a pointer to the memory, the BinaryNode object takes over ownership of this pointer
        @param size the byte length of the memory
    */
    DLLEXPORT BinaryNode(void* p = 0, size_t size = 0);

    //! returns false unless perl-boolean-evaluation is enabled, in which case it returns false only when empty
    /** @return false unless perl-boolean-evaluation is enabled, in which case it returns false only when empty
    */
    DLLEXPORT virtual bool getAsBoolImpl() const;

    //! concatenate the verbose string representation of the value to an existing QoreString
    /** used for %n and %N printf formatting
        @param str the string representation of the type will be concatenated to this QoreString reference
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return -1 for exception raised, 0 = OK
    */
    DLLEXPORT virtual int getAsString(QoreString &str, int foff, class ExceptionSink *xsink) const;

    //! returns a QoreString giving the verbose string representation of the List (including all contained values for container types)
    /** used for %n and %N printf formatting
        @param del if this is true when the function returns, then the returned QoreString pointer should be deleted, if false, then it must not be
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        NOTE: Use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly
        @see QoreNodeAsStringHelper
    */
    DLLEXPORT virtual QoreString *getAsString(bool &del, int foff, class ExceptionSink *xsink) const;

    DLLEXPORT virtual class AbstractQoreNode* realCopy() const;

    //! tests for equality
    /** this function does not throw a Qore-language exception with the BinaryNode class
        @param v the value to compare
        @param xsink is not used in this implementation of the function
    */
    DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink *xsink) const;

    //! tests for equality
    /** this function does not throw a Qore-language exception with the BinaryNode class
        @param v the value to compare
        @param xsink is not used in this implementation of the function
    */
    DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink *xsink) const;

    //! returns the type name as a c string
    DLLEXPORT virtual const char *getTypeName() const;

    //! returns the type information
    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);

    //! returns 0 = equal, 1 = not equal
    DLLEXPORT int compare(const BinaryNode* obj) const;

    //! returns the number of bytes in the object
    DLLEXPORT size_t size() const;

    //! returns true if empty
    DLLEXPORT bool empty() const;

    //! returns a copy of the object
    /**
        @return a copy of the current object
    */
    DLLEXPORT BinaryNode* copy() const;

    //! returns the pointer to the data
    /** @return the pointer to the data; never nullptr: an empty value returns a valid address with no bytes after it,
        so that it is not taken for a missing value (a database driver binds a null data pointer as SQL NULL)

        @since %Qore 3.0 never returns nullptr; an empty value made with no buffer returned nullptr, while one made
        from empty data could return a valid address
    */
    DLLEXPORT const void* getPtr() const;

    //! resizes the object and appends a copy of the data passed to the object
    DLLEXPORT void append(const void* nptr, size_t size);

    //! resizes the object and appends a copy of the data passed to the object
    DLLEXPORT void append(const BinaryNode* b);

    //! resizes the object and appends a copy of the data passed to the object
    DLLEXPORT void append(const BinaryNode &b);

    //! writes the given data to the given location in the object
    /** resizes the object if necessary

        @param pos the byte position where to start writing
        @param nptr the data to copy to the object
        @param size the number of bytes to copy to the object

        @return -1 if the resize failed (out of memory), 0 if OK

        @since %Qore 1.12
    */
    DLLEXPORT int writeTo(size_t pos, const void* nptr, size_t size);

    //! resizes the object and prepends a copy of the data passed to the beginning of the object
    DLLEXPORT void prepend(const void* nptr, size_t size);

    //! returns the data being managed and leaves this object empty
    /**
        @return the data being managed (leaves this object empty)
        @note it would be a grevious error to call this function on an object
        with a reference_count > 1 (i.e. is_unique() is false)
    */
    DLLEXPORT void* giveBuffer();

    //! pre-allocates a buffer of a certain size
    /** This function can be used to write data directly to a new BinaryNode object.
        This call can be made more than once, subsequent calls will cause realloc()
        to be called on the buffer which can be used to extend the buffer size.
        @param size the number of bytes to pre-allocate
        @return 0 for OK, -1 for error (memory could not be allocated)
    */
    DLLEXPORT int preallocate(size_t size);

    //! sets the buffer size after preallocation
    /** This function is designed to be used with BinaryNode::preallocate().  The
        size to be set must be less than the currently allocated size.
        @param size the size of the BinaryNode to set
        @return 0 for OK, -1 for error (size > current size)
    */
    DLLEXPORT int setSize(size_t size);

    //! removes "length" characters from the binary data starting at position "offset"
    /** @param offset byte position to start (rest of the data is removed) (offset starts with 0, negative offset means that many positions from the end of the data)
        @param length the number of bytes to remove (negative length means all but that many bytes from the end of the data)
        @param extract if non-null, the data removed will be written to this argument

        @since Qore 0.8.8
    */
    DLLEXPORT void splice(qore_offset_t offset, qore_offset_t length, BinaryNode* extract = 0);

    //! removes "length" characters from the binary data starting at position "offset" and replaces them with the data passed
    /** @param offset byte position to start (rest of the data is removed) (offset starts with 0, negative offset means that many positions from the end of the data)
        @param length the number of bytes to remove (negative length means all but that many bytes from the end of the data)
        @param data the data to insert at byte position "offset" after "length" characters are removed
        @param data_len the lenght of the data to insert
        @param extract if non-null, the data removed will be written to this argument

        @since Qore 0.8.8
    */
    DLLEXPORT void splice(qore_offset_t offset, qore_offset_t length, const void* data, size_t data_len, BinaryNode* extract = 0);

    //! copies data to the BinaryNode argument starting with byte position "offset"
    /** @param b the target for copying the data
        @param offset the offset in bytes from the beginning of the data (starting with 0)
        @return 0 = OK, -1 = error (invalid offset)

        @since Qore 0.8.8
    */
    DLLEXPORT int substr(BinaryNode& b, qore_offset_t offset) const;

    //! copies data to the BinaryNode argument starting with byte position "offset"
    /** @param b the target for copying the data
        @param offset the offset in bytes from the beginning of the data (starting with 0)
        @param length the number of bytes to copy
        @return 0 = OK, -1 = error (invalid offset)

        @since Qore 0.8.8
    */
    DLLEXPORT int substr(BinaryNode& b, qore_offset_t offset, qore_offset_t length) const;

    //! frees any managed memory and sets the size to 0
    DLLEXPORT void clear();

    // references the object and returns itself
    /** @since %Qore 0.9
    */
    DLLEXPORT BinaryNode* binRefSelf() const;

    //! returns the type name (useful in templates)
    DLLLOCAL static const char *getStaticTypeName() {
        return "binary";
    }

    //! returns the type code (useful in templates)
    DLLLOCAL static qore_type_t getStaticTypeCode() {
        return NT_BINARY;
    }
};

typedef SimpleRefHolder<BinaryNode> BinaryNodeHolder;

#endif // _QORE_BINARYNODE_H
