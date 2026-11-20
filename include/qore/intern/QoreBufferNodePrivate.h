/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_buffer_node_private.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREBUFFERNODEPRIVATE_H
#define _QORE_INTERN_QOREBUFFERNODEPRIVATE_H

#include <qore/QoreBufferNode.h>

#include <cassert>
#include <utility>

//! The library-internal operations of QoreBufferNode (qore#4380)
/** These functions were non-inline DLLLOCAL members of the public QoreBufferNode class: they were never exported, so
    they are not part of the module API and live here, in the implementation of the library.  Construct the object
    from the buffer node and call the operation:

    @code
    qore_buffer_node_private(buf).getValidityBit(i);
    @endcode

    The object holds only the pointer to the buffer node, and it is a friend of QoreBufferNode.
*/
class qore_buffer_node_private {
public:
    //! the buffer node
    QoreBufferNode* b;

    DLLLOCAL explicit qore_buffer_node_private(QoreBufferNode* n_b) : b(n_b) {
    }

    //! for a holder of the buffer node (ReferenceHolder, SimpleRefHolder, ...)
    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_buffer_node_private(H& holder) : qore_buffer_node_private(holder.operator->()) {
    }

    //! returns a const holder of \a n_b, for the operations that have a const and a non-const form (dataBytes() and so on)
    /** A temporary holder is not const, so it would select the non-const form of the operation, which differs from the
        const one (it ignores external storage): a caller that has a const buffer node uses view()
    */
    DLLLOCAL static const qore_buffer_node_private view(const QoreBufferNode* n_b) {
        return qore_buffer_node_private(n_b);
    }

    //! for the operations that are const members of the buffer node
    DLLLOCAL explicit qore_buffer_node_private(const QoreBufferNode* n_b) : b(const_cast<QoreBufferNode*>(n_b)) {
    }

    //! Creates a deep copy of the buffer with preserved or widened element nullability.
    /** This can widen a non-nullable buffer<T> to buffer<*T> by adding an all-valid validity bitmap.
        @param n_nullable_elements true if the copied buffer should have nullable elements
        @param xsink optional exception sink for cooperative cancellation
        @return a new buffer with the requested nullability and copied values
     */
    DLLLOCAL static QoreBufferNode* copyImpl(QoreBufferNode* b, bool n_nullable_elements, ExceptionSink* xsink = nullptr);

    DLLLOCAL QoreBufferNode* copy(bool n_nullable_elements, ExceptionSink* xsink = nullptr) const {
        return copyImpl(b, n_nullable_elements, nullptr);
    }

    //! Returns true if this buffer can be mutated without breaking ordinary value-copy semantics.
    /** Zero-copy views intentionally alias their root buffer and are ignored by this check. Ordinary references to
        the same root buffer still force copy-on-write before mutation.
        @return true if direct element mutation should update this buffer's shared storage
     */
    DLLLOCAL static bool isUniqueForMutationImpl(QoreBufferNode* b);

    DLLLOCAL bool isUniqueForMutation() const {
        return isUniqueForMutationImpl(b);
    }

    DLLLOCAL static QoreBufferNode* wrapExternalStorageImpl(QoreBufferElementType element_type,
        bool nullable_elements, size_t offset, size_t length, const void* data, const uint8_t* validity,
        std::shared_ptr<const void> owner, int64_t null_count, bool has_decimal_metadata,
        int32_t decimal_precision, int32_t decimal_scale, ExceptionSink* xsink);

    DLLLOCAL static QoreBufferNode* wrapExternalDeviceStorageImpl(QoreBufferElementType element_type,
        bool nullable_elements, size_t length, const void* device_data, const uint8_t* device_validity,
        std::shared_ptr<const void> owner, int64_t null_count, const QoreBufferDeviceInfo& device_info,
        QoreBufferDeviceCopyToHostCallback copy_to_host, bool has_decimal_metadata,
        int32_t decimal_precision, int32_t decimal_scale, ExceptionSink* xsink);

    DLLLOCAL static void normalizeDecimalMetadataImpl(QoreBufferNode* b);

    DLLLOCAL void normalizeDecimalMetadata() {
        return normalizeDecimalMetadataImpl(b);
    }

    DLLLOCAL QoreBufferNode* storageRoot() {
        return b->view_parent ? b->view_parent : b;
    }

    DLLLOCAL const QoreBufferNode* storageRoot() const {
        return b->view_parent ? b->view_parent : b;
    }

    DLLLOCAL size_t physicalIndex(size_t index) const {
        assert(index < b->length);
        return b->view_offset + index;
    }

    DLLLOCAL uint8_t* dataBytes() {
        return storageRoot()->data_buffer.data();
    }

    DLLLOCAL const uint8_t* dataBytes() const {
        const QoreBufferNode* root = storageRoot();
        return root->external_data ? root->external_data : root->data_buffer.data();
    }

    DLLLOCAL uint8_t* validityBytes() {
        return storageRoot()->validity_buffer.data();
    }

    DLLLOCAL const uint8_t* validityBytes() const {
        const QoreBufferNode* root = storageRoot();
        return root->external_data ? root->external_validity : root->validity_buffer.data();
    }

    DLLLOCAL uint64_t* stringOffsets() {
        assert(b->element_type == QoreBufferElementType::String);
        return reinterpret_cast<uint64_t*>(storageRoot()->data_buffer.data());
    }

    DLLLOCAL const uint64_t* stringOffsets() const {
        assert(b->element_type == QoreBufferElementType::String);
        return reinterpret_cast<const uint64_t*>(storageRoot()->data_buffer.data());
    }

    DLLLOCAL char* stringBytes() {
        assert(b->element_type == QoreBufferElementType::String);
        QoreBufferNode* root = storageRoot();
        return reinterpret_cast<char*>(root->data_buffer.data() + qore_buffer_node_private(root).dataByteSize(root->length));
    }

    DLLLOCAL const char* stringBytes() const {
        assert(b->element_type == QoreBufferElementType::String);
        const QoreBufferNode* root = storageRoot();
        return reinterpret_cast<const char*>(root->data_buffer.data() + qore_buffer_node_private(root).dataByteSize(root->length));
    }

    DLLLOCAL size_t dataByteSize(size_t n_length) const {
        if (b->element_type == QoreBufferElementType::Bool) {
            return ((n_length + 63) / 64) * 8;
        }
        if (b->element_type == QoreBufferElementType::String) {
            return (n_length + 1) * sizeof(uint64_t);
        }
        return n_length * qore_buffer_element_storage_size(b->element_type);
    }

    DLLLOCAL static void resizeStorageImpl(QoreBufferNode* b, size_t n_length);

    DLLLOCAL void resizeStorage(size_t n_length) {
        return resizeStorageImpl(b, n_length);
    }

    DLLLOCAL int materializeDeviceStorageUnlocked(ExceptionSink* xsink) {
        assert(!b->isView());
        // the common case (host storage only) returns here without a call
        if (!b->external_device_data) {
            return 0;
        }
        return materializeDeviceStorageSlow(xsink);
    }

    //! materializes the external device storage into host storage; the caller holds the storage lock
    DLLLOCAL static int materializeDeviceStorageSlowImpl(QoreBufferNode* b, ExceptionSink* xsink);

    DLLLOCAL int materializeDeviceStorageSlow(ExceptionSink* xsink) {
        return materializeDeviceStorageSlowImpl(b, xsink);
    }

    DLLLOCAL static int materializeDeviceStorageImpl(QoreBufferNode* b, ExceptionSink* xsink);

    DLLLOCAL int materializeDeviceStorage(ExceptionSink* xsink) {
        return materializeDeviceStorageImpl(b, xsink);
    }

    DLLLOCAL static int detachExternalStorageImpl(QoreBufferNode* b, ExceptionSink* xsink);

    DLLLOCAL int detachExternalStorage(ExceptionSink* xsink) {
        return detachExternalStorageImpl(b, xsink);
    }

    DLLLOCAL int ensureOwnedStorage(ExceptionSink* xsink) {
        return qore_buffer_node_private(storageRoot()).detachExternalStorage(xsink);
    }

    DLLLOCAL bool getValidityBit(size_t index) const {
        assert(b->nullable_elements);
        assert(index < b->length);
        size_t physical = physicalIndex(index);
        const uint8_t* bytes = validityBytes();
        if (!bytes) {
            return true;
        }
        return bytes[physical / 8] & (uint8_t(1) << (physical % 8));
    }

    DLLLOCAL void setValidityBit(size_t index, bool valid) {
        assert(b->nullable_elements);
        assert(index < b->length);
        QoreBufferNode* root = storageRoot();
        size_t physical = physicalIndex(index);
        uint8_t* bytes = root->validity_buffer.data();
        uint8_t mask = uint8_t(1) << (physical % 8);
        bool was_valid = bytes[physical / 8] & mask;
        if (valid) {
            bytes[physical / 8] |= mask;
            if (!was_valid && root->null_count > 0) {
                --root->null_count;
            }
        } else {
            bytes[physical / 8] &= ~mask;
            if (was_valid) {
                ++root->null_count;
            }
        }
    }

    DLLLOCAL bool getBoolBit(size_t index) const {
        assert(b->element_type == QoreBufferElementType::Bool);
        assert(index < b->length);
        size_t physical = physicalIndex(index);
        const uint8_t* bytes = dataBytes();
        return bytes[physical / 8] & (uint8_t(1) << (physical % 8));
    }

    DLLLOCAL void setBoolBit(size_t index, bool value) {
        assert(b->element_type == QoreBufferElementType::Bool);
        assert(index < b->length);
        size_t physical = physicalIndex(index);
        uint8_t* bytes = dataBytes();
        uint8_t mask = uint8_t(1) << (physical % 8);
        if (value) {
            bytes[physical / 8] |= mask;
        } else {
            bytes[physical / 8] &= ~mask;
        }
    }

    DLLLOCAL QoreBufferDecimal128 getDecimalStorage(size_t index) const {
        assert(b->element_type == QoreBufferElementType::Decimal128);
        assert(index < b->length);
        size_t physical = physicalIndex(index);
        QoreBufferDecimal128 rv;
        memcpy(&rv, dataBytes() + (physical * sizeof(rv)), sizeof(rv));
        return rv;
    }

    DLLLOCAL void setDecimalStorage(size_t index, QoreBufferDecimal128 value) {
        assert(b->element_type == QoreBufferElementType::Decimal128);
        assert(index < b->length);
        size_t physical = physicalIndex(index);
        memcpy(dataBytes() + (physical * sizeof(value)), &value, sizeof(value));
    }

    DLLLOCAL void setNull(size_t index) {
        assert(b->nullable_elements);
        setValidityBit(index, false);
    }

    //! Sets the elements of \a b to the elements of \a source at \a indices (qore#6220)
    /** \a b is a new buffer<decimal128> of \a count elements with the precision, scale and element nullability of
        \a source; the 128-bit storage and the null flag of each element are copied, without converting the value to
        a number and back.  An element whose value does not fit the precision of the buffer (only external storage can
        hold one) is assigned as a number, which raises the BUFFER-RANGE-ERROR of the assignment.

        @param b the new buffer
        @param source the source buffer<decimal128>
        @param indices the indices of the elements of \a source, each less than its size
        @param count the number of indices, the size of \a b
        @param cancel_context the context of the cancellation check made every 100 elements
        @param xsink for errors

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL static int takeDecimalImpl(QoreBufferNode* b, const QoreBufferNode* source, const size_t* indices,
            size_t count, const char* cancel_context, ExceptionSink* xsink);

    DLLLOCAL static int assignStringStorageImpl(QoreBufferNode* b, const std::vector<std::string>& values, const std::vector<uint8_t>& nulls, ExceptionSink* xsink);

    DLLLOCAL int assignStringStorage(const std::vector<std::string>& values, const std::vector<uint8_t>& nulls, ExceptionSink* xsink) {
        return assignStringStorageImpl(b, values, nulls, xsink);
    }

    DLLLOCAL static int assignStringRangeImpl(QoreBufferNode* b, const QoreBufferNode& source, size_t offset, size_t count, bool reverse, ExceptionSink* xsink);

    DLLLOCAL int assignStringRange(const QoreBufferNode& source, size_t offset, size_t count, bool reverse, ExceptionSink* xsink) {
        return assignStringRangeImpl(b, source, offset, count, reverse, xsink);
    }

    DLLLOCAL static int assignStringListImpl(QoreBufferNode* b, const QoreListNode* list, ExceptionSink* xsink);

    DLLLOCAL int assignStringList(const QoreListNode* list, ExceptionSink* xsink) {
        return assignStringListImpl(b, list, xsink);
    }

    DLLLOCAL static int fillStringImpl(QoreBufferNode* b, QoreValue value, ExceptionSink* xsink);

    DLLLOCAL int fillString(QoreValue value, ExceptionSink* xsink) {
        return fillStringImpl(b, value, xsink);
    }

    DLLLOCAL static int setStringValueImpl(QoreBufferNode* b, size_t index, QoreValue value, ExceptionSink* xsink);

    DLLLOCAL int setStringValue(size_t index, QoreValue value, ExceptionSink* xsink) {
        return setStringValueImpl(b, index, value, xsink);
    }

    DLLLOCAL static int setValueImpl(QoreBufferNode* b, size_t index, QoreValue value, ExceptionSink* xsink);

    DLLLOCAL int setValue(size_t index, QoreValue value, ExceptionSink* xsink) {
        return setValueImpl(b, index, value, xsink);
    }
};

#endif // _QORE_INTERN_QOREBUFFERNODEPRIVATE_H
