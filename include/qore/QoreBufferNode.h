/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreBufferNode.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QOREBUFFERNODE_H

#define _QORE_QOREBUFFERNODE_H

#include <qore/AbstractQoreNode.h>

#include <cstdint>
#include <cstddef>

//! Dense storage element types supported by buffer<T>.
enum class QoreBufferElementType : uint8_t {
    Invalid = 0,
    Int8,
    Int16,
    Int32,
    Int64,
    Float32,
    Float64,
    Bool,
};

//! Returns the Qore source name for the given buffer element type.
/** @param element_type the buffer element type
    @return the Qore source name, or "invalid" for invalid element types
 */
DLLEXPORT const char* qore_buffer_element_type_name(QoreBufferElementType element_type);

//! Parses a Qore buffer element type name.
/** @param name the Qore source name to parse
    @param element_type set to the parsed element type on success, or Invalid on failure
    @return true if @a name identifies a supported buffer element type
 */
DLLEXPORT bool qore_buffer_element_type_from_name(const char* name, QoreBufferElementType& element_type);

//! Returns the scalar Qore type corresponding to a buffer element type.
/** @param element_type the buffer element type
    @param nullable whether to return the nullable scalar type
    @return the scalar type info used for values read from the buffer
 */
DLLEXPORT const QoreTypeInfo* qore_buffer_element_scalar_type_info(QoreBufferElementType element_type,
    bool nullable = false);

//! Returns the number of bytes used by one physical element.
/** @param element_type the buffer element type
    @return the element storage size in bytes, or 0 for bit-packed bool and invalid types
 */
DLLEXPORT size_t qore_buffer_element_storage_size(QoreBufferElementType element_type);

//! Tests whether the buffer element type stores signed integers.
/** @param element_type the buffer element type
    @return true for int8, int16, int32, and int64
 */
DLLEXPORT bool qore_buffer_element_type_is_integer(QoreBufferElementType element_type);

//! Tests whether the buffer element type stores floating-point values.
/** @param element_type the buffer element type
    @return true for float32 and float64
 */
DLLEXPORT bool qore_buffer_element_type_is_float(QoreBufferElementType element_type);

//! Dense, homogeneous primitive array value for buffer<T>.
class QoreBufferNode : public AbstractQoreNode {
private:
    class AlignedByteBuffer {
    public:
        DLLLOCAL AlignedByteBuffer() = default;
        DLLLOCAL AlignedByteBuffer(const AlignedByteBuffer& old);
        DLLLOCAL AlignedByteBuffer& operator=(const AlignedByteBuffer& old);
        DLLLOCAL ~AlignedByteBuffer();

        DLLLOCAL void resize(size_t n_size, bool clear = false);
        DLLLOCAL void clear();

        DLLLOCAL uint8_t* data() {
            return ptr;
        }

        DLLLOCAL const uint8_t* data() const {
            return ptr;
        }

        DLLLOCAL size_t size() const {
            return len;
        }

    private:
        uint8_t* ptr = nullptr;
        size_t len = 0;
    };

public:
    //! Creates an empty buffer.
    /** @param element_type the primitive element storage type
        @param nullable_elements true if individual elements may be NOTHING
        @throw assert failure if @a element_type is Invalid
     */
    DLLEXPORT QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements = false);

    //! Creates a buffer with the given length.
    /** @param element_type the primitive element storage type
        @param nullable_elements true if individual elements may be NOTHING
        @param length the number of elements to allocate
        @throw assert failure if @a element_type is Invalid
     */
    DLLEXPORT QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements, size_t length);

    //! Creates a buffer from a Qore list.
    /** @param element_type the primitive element storage type
        @param nullable_elements true if individual elements may be NOTHING
        @param list source values; null creates an empty buffer
        @param xsink exception sink for type, range, allocation, and cancellation errors
        @throw BUFFER-TYPE-ERROR if a source element cannot be stored in the buffer
        @throw BUFFER-RANGE-ERROR if a numeric source element is outside the element type range
     */
    DLLEXPORT QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements, const QoreListNode* list,
        ExceptionSink* xsink);

    //! @copydoc AbstractQoreNode::getAsBoolImpl()
    DLLEXPORT virtual bool getAsBoolImpl() const;

    //! @copydoc AbstractQoreNode::getAsString(QoreString&, int, ExceptionSink*) const
    DLLEXPORT virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    //! @copydoc AbstractQoreNode::getAsString(bool&, int, ExceptionSink*) const
    DLLEXPORT virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    //! @copydoc AbstractQoreNode::realCopy() const
    DLLEXPORT virtual AbstractQoreNode* realCopy() const;

    //! @copydoc AbstractQoreNode::is_equal_soft(const AbstractQoreNode*, ExceptionSink*) const
    DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! @copydoc AbstractQoreNode::is_equal_hard(const AbstractQoreNode*, ExceptionSink*) const
    DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! @copydoc AbstractQoreNode::getTypeName() const
    DLLEXPORT virtual const char* getTypeName() const;

    //! Returns the base type name for buffer values.
    /** @return the string "buffer"
     */
    DLLLOCAL static const char* getStaticTypeName() {
        return "buffer";
    }

    //! Creates a deep copy of the buffer.
    /** @return a new buffer with the same element type, nullability, values, and validity bitmap
     */
    DLLEXPORT QoreBufferNode* copy() const;

    //! Returns the number of elements in the buffer.
    /** @return the buffer length
     */
    DLLEXPORT size_t size() const {
        return length;
    }

    //! Tests whether the buffer is empty.
    /** @return true if size() is zero
     */
    DLLEXPORT bool empty() const {
        return !length;
    }

    //! Returns the primitive element storage type.
    /** @return the buffer element type
     */
    DLLEXPORT QoreBufferElementType getElementType() const {
        return element_type;
    }

    //! Tests whether elements may be NOTHING.
    /** @return true if the buffer has an element validity bitmap
     */
    DLLEXPORT bool hasNullableElements() const {
        return nullable_elements;
    }

    //! Returns this buffer's complex Qore type.
    /** @return type info for buffer<T> or buffer<*T>
     */
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! Returns the Qore type of values read from the buffer.
    /** @return scalar element type info, including NOTHING when elements are nullable
     */
    DLLEXPORT const QoreTypeInfo* getElementTypeInfo() const;

    //! Tests whether an element is null.
    /** @param index zero-based element index
        @return true if @a index is in range and the element is NOTHING
     */
    DLLEXPORT bool isElementNull(size_t index) const;

    //! Reads an element without transferring ownership from the buffer.
    /** @param index zero-based element index
        @return the element value, or NOTHING if @a index is out of range or the element is null
     */
    DLLEXPORT QoreValue getReferencedEntry(size_t index) const;

    //! Stores a value in an existing element.
    /** @param index zero-based element index
        @param value the source value to store
        @param xsink exception sink for type and range errors
        @return 0 on success, -1 on error
        @throw BUFFER-INDEX-ERROR if @a index is out of range
        @throw BUFFER-TYPE-ERROR if @a value cannot be stored in the element type
        @throw BUFFER-RANGE-ERROR if a numeric value is outside the element type range
     */
    DLLEXPORT int setEntry(size_t index, QoreValue value, ExceptionSink* xsink);

    //! Converts the buffer to a typed Qore list.
    /** @param xsink exception sink for allocation and cancellation errors
        @return a new list containing all buffer elements, or null on error
     */
    DLLEXPORT QoreListNode* toList(ExceptionSink* xsink) const;

    //! Creates a deep copy of a contiguous element range.
    /** @param offset first element to copy
        @param count number of elements to copy
        @param xsink optional exception sink for cancellation errors in bitmap copy loops
        @return a new buffer containing the requested range
        @throw assert failure if the requested range is out of bounds
     */
    DLLEXPORT QoreBufferNode* slice(size_t offset, size_t count, ExceptionSink* xsink = nullptr) const;

protected:
    //! @copydoc AbstractQoreNode::~AbstractQoreNode()
    DLLEXPORT virtual ~QoreBufferNode() = default;

    //! @copydoc AbstractQoreNode::evalImpl(bool&, ExceptionSink*) const
    DLLEXPORT virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

private:
    QoreBufferElementType element_type;
    bool nullable_elements;
    size_t length = 0;
    mutable int64 null_count = 0;
    AlignedByteBuffer data_buffer;
    AlignedByteBuffer validity_buffer;

    DLLLOCAL size_t dataByteSize(size_t n_length) const;
    DLLLOCAL void resizeStorage(size_t n_length);
    DLLLOCAL bool getValidityBit(size_t index) const;
    DLLLOCAL void setValidityBit(size_t index, bool valid);
    DLLLOCAL bool getBoolBit(size_t index) const;
    DLLLOCAL void setBoolBit(size_t index, bool value);
    DLLLOCAL void setNull(size_t index);
    DLLLOCAL int setValue(size_t index, QoreValue value, ExceptionSink* xsink);
};

#endif
