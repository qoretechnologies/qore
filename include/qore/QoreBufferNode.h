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

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

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
    String,
    Decimal128,
};

//! Physical storage for buffer<decimal128> values.
/** Values are stored as signed two's-complement 128-bit unscaled integers.
    The owning QoreBufferNode carries the decimal precision and scale metadata.
 */
struct QoreBufferDecimal128 {
    uint64_t low;
    int64_t high;
};

//! Device or accelerator family for external buffer storage.
/** The values are intentionally provider-neutral.  Device-specific modules
    keep their own owner object alive through QoreBufferNode and use this
    descriptor only to identify the memory location to other integrations.
 */
enum class QoreBufferDeviceKind : uint8_t {
    Unknown = 0,
    Cuda,
    Rocm,
    OpenCL,
    Vulkan,
    OneAPI,
    Metal,
    Java,
    Other,
};

//! Returns the stable source/API name for a buffer device kind.
/** @param kind the device or accelerator family
    @return a lower-case stable name such as \c cuda or \c rocm
 */
DLLEXPORT const char* qore_buffer_device_kind_name(QoreBufferDeviceKind kind);

//! Describes the location of external device buffer storage.
struct QoreBufferDeviceInfo {
    QoreBufferDeviceKind kind = QoreBufferDeviceKind::Unknown;
    int64_t device_id = -1;
    std::string name;
};

//! Copies external device buffer storage into Qore-owned host memory.
/** The callback is called with storage for the root buffer, not a slice view.
    It must copy @a length logical elements starting at the beginning of
    @a device_data into @a host_data.  For nullable buffers, @a host_validity is
    a Qore/Arrow-compatible validity bitmap destination when @a device_validity
    is not null; otherwise it is null and libqore fills an all-valid bitmap.

    @param host_data destination host data storage
    @param host_validity destination host validity bitmap, or nullptr
    @param length number of logical elements to copy
    @param device_data source provider-specific device data pointer
    @param device_validity source provider-specific device validity pointer, or nullptr
    @param device_info device descriptor supplied when the buffer was wrapped
    @param owner provider-specific owner object kept alive by the buffer
    @param xsink exception sink for transfer errors
    @return 0 on success, non-zero on error
 */
using QoreBufferDeviceCopyToHostCallback = int (*)(void* host_data, uint8_t* host_validity, size_t length,
    const void* device_data, const uint8_t* device_validity, const QoreBufferDeviceInfo& device_info,
    const void* owner, ExceptionSink* xsink);

//! Elementwise operation supported by dense buffer helpers.
enum class QoreBufferBinaryOperation : uint8_t {
    Add = 0,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual,
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

//! Tests whether the buffer element type stores fixed-precision decimal values.
/** @param element_type the buffer element type
    @return true for decimal128
 */
DLLEXPORT bool qore_buffer_element_type_is_decimal(QoreBufferElementType element_type);

//! Returns true if either operand is a dense buffer value.
/** @param left the left operand
    @param right the right operand
    @return true if an elementwise buffer operation should be attempted
 */
DLLEXPORT bool qore_buffer_binary_op_applies(const QoreValue& left, const QoreValue& right);

//! Returns the parse-time result type for an elementwise dense buffer operation.
/** @param leftTypeInfo the left operand type
    @param rightTypeInfo the right operand type
    @param op the operation to analyze
    @return the result type, or nullptr if neither operand is a buffer
 */
DLLEXPORT const QoreTypeInfo* qore_buffer_binary_op_type(const QoreTypeInfo* leftTypeInfo,
    const QoreTypeInfo* rightTypeInfo, QoreBufferBinaryOperation op);

//! Applies an elementwise dense buffer operation.
/** One operand must be a buffer. The other operand may be a buffer of the same length or a supported scalar.
    Null elements propagate to nullable result buffers.

    @param left the left operand
    @param right the right operand
    @param op the operation to apply
    @param xsink exception sink for validation, allocation, range, division-by-zero, and cancellation errors
    @return the result buffer, or NOTHING on error
 */
DLLEXPORT QoreValue qore_buffer_binary_op(const QoreValue& left, const QoreValue& right,
    QoreBufferBinaryOperation op, ExceptionSink* xsink);

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

    //! Creates a buffer with the given length and decimal metadata.
    /** Decimal metadata is used only for buffer<decimal128>; other element types ignore it.
        @param element_type the primitive element storage type
        @param nullable_elements true if individual elements may be NOTHING
        @param length the number of elements to allocate
        @param decimal_precision decimal precision for decimal128 buffers; defaults to 38 when <= 0
        @param decimal_scale decimal scale for decimal128 buffers; defaults to 0 when < 0
        @throw assert failure if @a element_type is Invalid
     */
    DLLEXPORT QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements, size_t length,
        int32_t decimal_precision, int32_t decimal_scale);

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

    //! Creates a buffer from a Qore list with explicit decimal metadata.
    /** Decimal metadata is used only for buffer<decimal128>; other element types ignore it.
        @param element_type the primitive element storage type
        @param nullable_elements true if individual elements may be NOTHING
        @param list source values; null creates an empty buffer
        @param xsink exception sink for type, range, allocation, and cancellation errors
        @param decimal_precision decimal precision for decimal128 buffers; defaults to 38 when <= 0
        @param decimal_scale decimal scale for decimal128 buffers; infers from @a list when < 0
        @throw BUFFER-TYPE-ERROR if a source element cannot be stored in the buffer
        @throw BUFFER-RANGE-ERROR if a source element is outside the element type range
     */
    DLLEXPORT QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements, const QoreListNode* list,
        ExceptionSink* xsink, int32_t decimal_precision, int32_t decimal_scale);

    //! Wraps immutable external fixed-width storage without copying.
    /** The returned buffer keeps @a owner alive for the lifetime of the buffer
        storage.  The external memory is treated as immutable; any Qore write
        path detaches the storage into an owned buffer before modifying it.

        @param element_type the primitive element storage type; string storage is not supported
        @param nullable_elements true if individual elements may be NOTHING
        @param length number of visible elements
        @param data pointer to the external data buffer
        @param validity pointer to the external validity bitmap, or nullptr when all elements are valid
        @param owner shared owner for the external data and validity buffers
        @param null_count number of null values, or -1 to calculate it from @a validity
        @param xsink exception sink for validation and cancellation errors
        @return a buffer wrapping the external storage, or nullptr on error
        @throw BUFFER-TYPE-ERROR if @a element_type is unsupported
        @throw BUFFER-EXTERNAL-STORAGE-ERROR if required pointers or ownership are missing
     */
    DLLEXPORT static QoreBufferNode* wrapExternalStorage(QoreBufferElementType element_type,
        bool nullable_elements, size_t length, const void* data, const uint8_t* validity,
        std::shared_ptr<const void> owner, int64_t null_count, ExceptionSink* xsink);

    //! Wraps immutable external fixed-width storage with decimal metadata.
    /** This overload is required for buffer<decimal128>, whose physical storage
        is not self-describing. Non-decimal element types ignore the decimal
        metadata.

        @param element_type the primitive element storage type; string storage is not supported
        @param nullable_elements true if individual elements may be NOTHING
        @param length number of visible elements
        @param data pointer to the external data buffer
        @param validity pointer to the external validity bitmap, or nullptr when all elements are valid
        @param owner shared owner for the external data and validity buffers
        @param null_count number of null values, or -1 to calculate it from @a validity
        @param decimal_precision decimal precision for decimal128 buffers; must be 1..38
        @param decimal_scale decimal scale for decimal128 buffers; must be 0..precision
        @param xsink exception sink for validation and cancellation errors
        @return a buffer wrapping the external storage, or nullptr on error
        @throw BUFFER-TYPE-ERROR if @a element_type is unsupported
        @throw BUFFER-RANGE-ERROR if decimal metadata is invalid
        @throw BUFFER-EXTERNAL-STORAGE-ERROR if required pointers or ownership are missing
     */
    DLLEXPORT static QoreBufferNode* wrapExternalStorage(QoreBufferElementType element_type,
        bool nullable_elements, size_t length, const void* data, const uint8_t* validity,
        std::shared_ptr<const void> owner, int64_t null_count, int32_t decimal_precision,
        int32_t decimal_scale, ExceptionSink* xsink);

    //! Wraps immutable external fixed-width storage with an initial element offset.
    /** This overload is intended for Arrow-style arrays where the data and
        validity buffers may have a logical element offset.  The returned buffer
        has @a length visible elements and uses @a offset only for physical
        indexing into @a data and @a validity.

        @param element_type the primitive element storage type; string storage is not supported
        @param nullable_elements true if individual elements may be NOTHING
        @param offset first visible element in the external buffers
        @param length number of visible elements
        @param data pointer to the external data buffer
        @param validity pointer to the external validity bitmap, or nullptr when all elements are valid
        @param owner shared owner for the external data and validity buffers
        @param null_count number of null values in the visible range, or -1 to calculate it from @a validity
        @param xsink exception sink for validation and cancellation errors
        @return a buffer wrapping the external storage, or nullptr on error
        @throw BUFFER-TYPE-ERROR if @a element_type is unsupported
        @throw BUFFER-EXTERNAL-STORAGE-ERROR if required pointers or ownership are missing
     */
    DLLEXPORT static QoreBufferNode* wrapExternalStorage(QoreBufferElementType element_type,
        bool nullable_elements, size_t offset, size_t length, const void* data, const uint8_t* validity,
        std::shared_ptr<const void> owner, int64_t null_count, ExceptionSink* xsink);

    //! Wraps immutable external fixed-width storage with an initial element offset and decimal metadata.
    /** This overload is required for Arrow-style decimal128 arrays, where the
        raw data buffer carries a logical element offset and decimal precision
        and scale must be supplied separately.

        @param element_type the primitive element storage type; string storage is not supported
        @param nullable_elements true if individual elements may be NOTHING
        @param offset first visible element in the external buffers
        @param length number of visible elements
        @param data pointer to the external data buffer
        @param validity pointer to the external validity bitmap, or nullptr when all elements are valid
        @param owner shared owner for the external data and validity buffers
        @param null_count number of null values in the visible range, or -1 to calculate it from @a validity
        @param decimal_precision decimal precision for decimal128 buffers; must be 1..38
        @param decimal_scale decimal scale for decimal128 buffers; must be 0..precision
        @param xsink exception sink for validation and cancellation errors
        @return a buffer wrapping the external storage, or nullptr on error
        @throw BUFFER-TYPE-ERROR if @a element_type is unsupported
        @throw BUFFER-RANGE-ERROR if decimal metadata is invalid
        @throw BUFFER-EXTERNAL-STORAGE-ERROR if required pointers or ownership are missing
     */
    DLLEXPORT static QoreBufferNode* wrapExternalStorage(QoreBufferElementType element_type,
        bool nullable_elements, size_t offset, size_t length, const void* data, const uint8_t* validity,
        std::shared_ptr<const void> owner, int64_t null_count, int32_t decimal_precision,
        int32_t decimal_scale, ExceptionSink* xsink);

    //! Wraps immutable external device storage without copying.
    /** The returned buffer keeps @a owner alive for the lifetime of the device
        storage.  Ordinary host reads require explicit or automatic
        materialization through @a copy_to_host; Qore write paths materialize and
        detach the storage into an owned host buffer before modification.

        @param element_type the primitive element storage type; string storage is not supported
        @param nullable_elements true if individual elements may be NOTHING
        @param length number of visible elements
        @param device_data provider-specific device data pointer
        @param device_validity provider-specific device validity pointer, or nullptr when all elements are valid
        @param owner shared owner for the device buffers
        @param null_count number of null values, or -1 when unknown
        @param device_info provider-neutral device descriptor
        @param copy_to_host callback used to materialize host storage
        @param xsink exception sink for validation errors
        @return a buffer wrapping the device storage, or nullptr on error
        @throw BUFFER-TYPE-ERROR if @a element_type is unsupported
        @throw BUFFER-EXTERNAL-STORAGE-ERROR if required pointers, ownership, or callbacks are missing
     */
    DLLEXPORT static QoreBufferNode* wrapExternalDeviceStorage(QoreBufferElementType element_type,
        bool nullable_elements, size_t length, const void* device_data, const uint8_t* device_validity,
        std::shared_ptr<const void> owner, int64_t null_count, const QoreBufferDeviceInfo& device_info,
        QoreBufferDeviceCopyToHostCallback copy_to_host, ExceptionSink* xsink);

    //! Wraps immutable external device storage with decimal metadata.
    /** This overload is required for \c buffer<decimal128>, whose physical
        storage is not self-describing.  Non-decimal element types ignore the
        decimal metadata.

        @param element_type the primitive element storage type; string storage is not supported
        @param nullable_elements true if individual elements may be NOTHING
        @param length number of visible elements
        @param device_data provider-specific device data pointer
        @param device_validity provider-specific device validity pointer, or nullptr when all elements are valid
        @param owner shared owner for the device buffers
        @param null_count number of null values, or -1 when unknown
        @param device_info provider-neutral device descriptor
        @param copy_to_host callback used to materialize host storage
        @param decimal_precision decimal precision for decimal128 buffers; must be 1..38
        @param decimal_scale decimal scale for decimal128 buffers; must be 0..precision
        @param xsink exception sink for validation errors
        @return a buffer wrapping the device storage, or nullptr on error
        @throw BUFFER-TYPE-ERROR if @a element_type is unsupported
        @throw BUFFER-RANGE-ERROR if decimal metadata is invalid
        @throw BUFFER-EXTERNAL-STORAGE-ERROR if required pointers, ownership, or callbacks are missing
     */
    DLLEXPORT static QoreBufferNode* wrapExternalDeviceStorage(QoreBufferElementType element_type,
        bool nullable_elements, size_t length, const void* device_data, const uint8_t* device_validity,
        std::shared_ptr<const void> owner, int64_t null_count, const QoreBufferDeviceInfo& device_info,
        QoreBufferDeviceCopyToHostCallback copy_to_host, int32_t decimal_precision, int32_t decimal_scale,
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
    /** @param xsink optional exception sink for cooperative cancellation
        @return a new buffer with the same element type, nullability, values, and validity bitmap
     */
    DLLEXPORT QoreBufferNode* copy(ExceptionSink* xsink = nullptr) const;

    //! Creates a deep copy of the buffer with preserved or widened element nullability.
    /** This can widen a non-nullable buffer<T> to buffer<*T> by adding an all-valid validity bitmap.
        @param n_nullable_elements true if the copied buffer should have nullable elements
        @param xsink optional exception sink for cooperative cancellation
        @return a new buffer with the requested nullability and copied values
     */
    DLLLOCAL QoreBufferNode* copy(bool n_nullable_elements, ExceptionSink* xsink = nullptr) const;

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

    //! Returns decimal precision metadata for buffer<decimal128>.
    /** @return precision in decimal digits; non-decimal buffers return 0
     */
    DLLEXPORT int32_t getDecimalPrecision() const {
        return element_type == QoreBufferElementType::Decimal128 ? decimal_precision : 0;
    }

    //! Returns decimal scale metadata for buffer<decimal128>.
    /** @return scale in decimal digits; non-decimal buffers return 0
     */
    DLLEXPORT int32_t getDecimalScale() const {
        return element_type == QoreBufferElementType::Decimal128 ? decimal_scale : 0;
    }

    //! Returns a mutable pointer to the raw dense element storage.
    /** This is an internal helper for runtime/JIT integrations that already validate element storage semantics.
        The returned pointer uses the buffer's physical representation and includes this buffer's view offset;
        buffer<bool> is bit-packed and may start at a non-byte-aligned bit offset.
        @return the raw storage pointer, or nullptr for an empty buffer
     */
    DLLEXPORT void* getRawData();

    //! Returns a const pointer to the raw dense element storage.
    /** This is an internal helper for runtime/JIT integrations that already validate element storage semantics.
        The returned pointer uses the buffer's physical representation and includes this buffer's view offset;
        buffer<bool> is bit-packed and may start at a non-byte-aligned bit offset.
        @return the raw storage pointer, or nullptr for an empty buffer
     */
    DLLEXPORT const void* getRawData() const;

    //! Returns a const pointer to the raw validity bitmap storage.
    /** The returned pointer uses the buffer's physical representation and
        includes this buffer's byte-aligned view offset.  Use
        getRawValidityBitOffset() for the bit offset of the first visible
        element when the value is not byte-aligned.

        @return the raw validity bitmap pointer, or nullptr for non-nullable buffers or all-valid external storage
     */
    DLLEXPORT const uint8_t* getRawValidityData() const;

    //! Returns the bit offset of the first visible element in getRawData() for bit-packed bool buffers.
    /** @return a value in the range [0, 7], or zero for byte-addressable element types
     */
    DLLEXPORT size_t getRawDataBitOffset() const;

    //! Returns the bit offset of the first visible element in getRawValidityData().
    /** @return a value in the range [0, 7], or zero for non-nullable buffers
     */
    DLLEXPORT size_t getRawValidityBitOffset() const;

    //! Tests whether this buffer's storage is backed by immutable external memory.
    /** @return true if this buffer or its storage root wraps external memory
     */
    DLLEXPORT bool hasExternalStorage() const;

    //! Tests whether this buffer's storage is backed by immutable external device memory.
    /** @return true if this buffer or its storage root wraps external device memory
     */
    DLLEXPORT bool hasExternalDeviceStorage() const;

    //! Tests whether host storage is immediately available for raw-data readers.
    /** @return true if an empty buffer or a host data pointer is available without a device transfer
     */
    DLLEXPORT bool hasHostStorage() const;

    //! Materializes external device storage into Qore-owned host memory.
    /** This is a no-op for ordinary host buffers and already-materialized device
        buffers.  It does not detach device ownership; write paths detach and
        clear device metadata before modifying the host copy.

        @param xsink exception sink for transfer and cancellation errors
        @return 0 on success, -1 on error
        @throw BUFFER-DEVICE-STORAGE-ERROR if the device buffer cannot be copied to host memory
     */
    DLLEXPORT int ensureHostStorage(ExceptionSink* xsink) const;

    //! Returns the provider-specific external device data pointer.
    /** @return the device data pointer, or nullptr if this is not a device-backed buffer
     */
    DLLEXPORT const void* getDeviceData() const;

    //! Returns the provider-specific external device validity pointer.
    /** @return the device validity pointer, or nullptr for non-nullable, all-valid, or host-only buffers
     */
    DLLEXPORT const uint8_t* getDeviceValidityData() const;

    //! Returns this buffer's device descriptor.
    /** @return the device descriptor, or nullptr if this is not a device-backed buffer
     */
    DLLEXPORT const QoreBufferDeviceInfo* getDeviceInfo() const;

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

    //! Reads an element without transferring ownership from the buffer.
    /** This overload can materialize external device storage before reading.
        @param index zero-based element index
        @param xsink exception sink for device transfer errors
        @return the element value, or NOTHING if @a index is out of range, the element is null, or an error occurs
     */
    DLLEXPORT QoreValue getReferencedEntry(size_t index, ExceptionSink* xsink) const;

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

    //! Fills every element with a value.
    /** @param value the source value to store in every element
        @param xsink exception sink for type and range errors
        @return 0 on success, -1 on error
     */
    DLLEXPORT int fill(QoreValue value, ExceptionSink* xsink);

    //! Converts the buffer to a typed Qore list.
    /** @param xsink exception sink for allocation and cancellation errors
        @return a new list containing all buffer elements, or null on error
     */
    DLLEXPORT QoreListNode* toList(ExceptionSink* xsink) const;

    //! Counts valid values in the buffer.
    /** @param xsink optional exception sink for cooperative cancellation
        @return the number of non-null elements; for non-nullable buffers this is identical to size()
     */
    DLLEXPORT size_t countValid(ExceptionSink* xsink = nullptr) const;

    //! Sums valid numeric buffer values.
    /** @param xsink exception sink for cancellation errors
        @return an integer sum for integer and boolean buffers, a floating-point sum for floating-point buffers
     */
    DLLEXPORT QoreValue sum(ExceptionSink* xsink) const;

    //! Calculates the arithmetic mean of valid buffer values.
    /** @param xsink exception sink for cancellation errors
        @return the mean as a floating-point value, or NOTHING if the buffer has no valid values
     */
    DLLEXPORT QoreValue mean(ExceptionSink* xsink) const;

    //! Finds the minimum valid buffer value.
    /** @param xsink exception sink for cancellation errors
        @return the minimum value, or NOTHING if the buffer has no valid values
     */
    DLLEXPORT QoreValue min(ExceptionSink* xsink) const;

    //! Finds the maximum valid buffer value.
    /** @param xsink exception sink for cancellation errors
        @return the maximum value, or NOTHING if the buffer has no valid values
     */
    DLLEXPORT QoreValue max(ExceptionSink* xsink) const;

    //! Creates a deep copy of a contiguous element range.
    /** @param offset first element to copy
        @param count number of elements to copy
        @param xsink optional exception sink for cancellation errors in bitmap copy loops
        @return a new buffer containing the requested range
        @throw assert failure if the requested range is out of bounds
     */
    DLLEXPORT QoreBufferNode* slice(size_t offset, size_t count, ExceptionSink* xsink = nullptr) const;

    //! Creates a zero-copy view over a contiguous element range.
    /** The returned buffer shares storage with this buffer; writes through either buffer are visible through the
        other buffer. The caller owns the returned reference.
        @param offset first element in the view
        @param count number of elements in the view
        @return a new buffer view over the requested range
        @throw assert failure if the requested range is out of bounds
     */
    DLLEXPORT QoreBufferNode* view(size_t offset, size_t count) const;

    //! Returns true if this buffer can be mutated without breaking ordinary value-copy semantics.
    /** Zero-copy views intentionally alias their root buffer and are ignored by this check. Ordinary references to
        the same root buffer still force copy-on-write before mutation.
        @return true if direct element mutation should update this buffer's shared storage
     */
    DLLLOCAL bool isUniqueForMutation() const;

    //! Creates a deep copy of an inclusive element range, supporting reverse ranges.
    /** @param start first element to copy
        @param stop final element to copy
        @param xsink exception sink for cancellation errors
        @return a new buffer containing the requested range
        @throw assert failure if either index is out of bounds
     */
    DLLEXPORT QoreBufferNode* sliceRange(size_t start, size_t stop, ExceptionSink* xsink) const;

protected:
    //! @copydoc AbstractQoreNode::~AbstractQoreNode()
    DLLEXPORT virtual ~QoreBufferNode();

    //! @copydoc AbstractQoreNode::evalImpl(bool&, ExceptionSink*) const
    DLLEXPORT virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

private:
    QoreBufferElementType element_type;
    bool nullable_elements;
    int32_t decimal_precision = 38;
    int32_t decimal_scale = 0;
    size_t length = 0;
    mutable int64 null_count = 0;
    AlignedByteBuffer data_buffer;
    AlignedByteBuffer validity_buffer;
    std::shared_ptr<const void> external_owner;
    const uint8_t* external_data = nullptr;
    const uint8_t* external_validity = nullptr;
    std::shared_ptr<const void> external_device_owner;
    const void* external_device_data = nullptr;
    const uint8_t* external_device_validity = nullptr;
    QoreBufferDeviceInfo external_device_info;
    QoreBufferDeviceCopyToHostCallback external_device_copy_to_host = nullptr;
    mutable std::mutex storage_mutex;
    QoreBufferNode* view_parent = nullptr;
    size_t view_offset = 0;
    mutable std::atomic_int view_ref_count{0};

    DLLLOCAL QoreBufferNode(QoreBufferNode* parent, size_t offset, size_t length);
    DLLLOCAL QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements, size_t length,
        const void* data, const uint8_t* validity, std::shared_ptr<const void> owner, int64_t null_count,
        int32_t decimal_precision, int32_t decimal_scale);
    DLLLOCAL QoreBufferNode(QoreBufferElementType element_type, bool nullable_elements, size_t length,
        const void* device_data, const uint8_t* device_validity, std::shared_ptr<const void> owner,
        int64_t null_count, const QoreBufferDeviceInfo& device_info,
        QoreBufferDeviceCopyToHostCallback copy_to_host, int32_t decimal_precision, int32_t decimal_scale);
    DLLLOCAL static QoreBufferNode* wrapExternalStorageImpl(QoreBufferElementType element_type,
        bool nullable_elements, size_t offset, size_t length, const void* data, const uint8_t* validity,
        std::shared_ptr<const void> owner, int64_t null_count, bool has_decimal_metadata,
        int32_t decimal_precision, int32_t decimal_scale, ExceptionSink* xsink);
    DLLLOCAL static QoreBufferNode* wrapExternalDeviceStorageImpl(QoreBufferElementType element_type,
        bool nullable_elements, size_t length, const void* device_data, const uint8_t* device_validity,
        std::shared_ptr<const void> owner, int64_t null_count, const QoreBufferDeviceInfo& device_info,
        QoreBufferDeviceCopyToHostCallback copy_to_host, bool has_decimal_metadata,
        int32_t decimal_precision, int32_t decimal_scale, ExceptionSink* xsink);
    DLLLOCAL void normalizeDecimalMetadata();
    DLLLOCAL bool isView() const {
        return view_parent;
    }
    DLLLOCAL QoreBufferNode* storageRoot();
    DLLLOCAL const QoreBufferNode* storageRoot() const;
    DLLLOCAL size_t physicalIndex(size_t index) const;
    DLLLOCAL uint8_t* dataBytes();
    DLLLOCAL const uint8_t* dataBytes() const;
    DLLLOCAL uint8_t* validityBytes();
    DLLLOCAL const uint8_t* validityBytes() const;
    DLLLOCAL uint64_t* stringOffsets();
    DLLLOCAL const uint64_t* stringOffsets() const;
    DLLLOCAL char* stringBytes();
    DLLLOCAL const char* stringBytes() const;
    DLLLOCAL size_t dataByteSize(size_t n_length) const;
    DLLLOCAL void resizeStorage(size_t n_length);
    DLLLOCAL int materializeDeviceStorageUnlocked(ExceptionSink* xsink);
    DLLLOCAL int materializeDeviceStorage(ExceptionSink* xsink);
    DLLLOCAL int detachExternalStorage(ExceptionSink* xsink);
    DLLLOCAL int ensureOwnedStorage(ExceptionSink* xsink);
    DLLLOCAL bool getValidityBit(size_t index) const;
    DLLLOCAL void setValidityBit(size_t index, bool valid);
    DLLLOCAL bool getBoolBit(size_t index) const;
    DLLLOCAL void setBoolBit(size_t index, bool value);
    DLLLOCAL QoreBufferDecimal128 getDecimalStorage(size_t index) const;
    DLLLOCAL void setDecimalStorage(size_t index, QoreBufferDecimal128 value);
    DLLLOCAL void setNull(size_t index);
    DLLLOCAL int assignStringStorage(const std::vector<std::string>& values, const std::vector<uint8_t>& nulls,
        ExceptionSink* xsink);
    DLLLOCAL int assignStringRange(const QoreBufferNode& source, size_t offset, size_t count, bool reverse,
        ExceptionSink* xsink);
    DLLLOCAL int assignStringList(const QoreListNode* list, ExceptionSink* xsink);
    DLLLOCAL int fillString(QoreValue value, ExceptionSink* xsink);
    DLLLOCAL int setStringValue(size_t index, QoreValue value, ExceptionSink* xsink);
    DLLLOCAL int setValue(size_t index, QoreValue value, ExceptionSink* xsink);
};

#endif
