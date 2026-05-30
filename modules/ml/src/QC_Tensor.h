/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Tensor.h

    Qore ml module - Tensor class

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
*/

#ifndef _QORE_MODULE_ML_QC_TENSOR_H
#define _QORE_MODULE_ML_QC_TENSOR_H

#include <qore/Qore.h>

#include <cstddef>
#include <cstdint>
#include <vector>

DLLEXPORT extern qore_classid_t CID_TENSOR;
DLLLOCAL extern QoreClass* QC_TENSOR;

DLLLOCAL void preinitTensorClass();
DLLLOCAL QoreClass* initTensorClass(QoreNamespace& ns);

//! Dense tensor metadata and storage wrapper for model inference.
class QoreTensor : public AbstractPrivateData {
public:
    //! Creates a tensor by taking ownership of one reference to @a n_buffer.
    DLLLOCAL QoreTensor(QoreBufferNode* n_buffer, std::vector<int64_t> n_shape);

    //! Creates a tensor by taking a reference to @a n_buffer.
    DLLLOCAL QoreTensor(const QoreBufferNode* n_buffer, std::vector<int64_t> n_shape);

    DLLLOCAL virtual ~QoreTensor();

    //! Creates a tensor from a Qore value and optional shape and dtype metadata.
    DLLLOCAL static QoreTensor* fromValue(QoreValue data, const QoreListNode* shape,
        const QoreStringNode* dtype, bool zero_copy, ExceptionSink* xsink);

    //! Creates a row-major 2D tensor from dense equal-length column buffers.
    DLLLOCAL static QoreTensor* fromColumns(const QoreListNode* columns, const QoreStringNode* dtype,
        ExceptionSink* xsink);

    //! Concatenates row-major tensors along their first dimension.
    DLLLOCAL static QoreTensor* concatRows(const QoreListNode* tensors, ExceptionSink* xsink);

    //! Creates a zero-copy tensor with a different shape.
    DLLLOCAL QoreTensor* reshape(const QoreListNode* shape, ExceptionSink* xsink) const;

    //! Creates a tensor containing a contiguous range of first-dimension rows.
    DLLLOCAL QoreTensor* sliceRows(int64_t offset, int64_t count, bool zero_copy, ExceptionSink* xsink) const;

    //! Returns the underlying buffer with a new reference.
    DLLLOCAL QoreBufferNode* refBuffer() const;

    //! Returns the shape as a new Qore list.
    DLLLOCAL QoreListNode* getShapeList(ExceptionSink* xsink) const;

    //! Returns nested Qore list data according to this tensor's shape.
    DLLLOCAL QoreValue toList(ExceptionSink* xsink) const;

    DLLLOCAL const std::vector<int64_t>& getShape() const {
        return shape;
    }

    DLLLOCAL QoreBufferElementType getElementType() const {
        return buffer->getElementType();
    }

    DLLLOCAL const QoreBufferNode* getBuffer() const {
        return buffer;
    }

    DLLLOCAL QoreBufferNode* getMutableBuffer() const {
        return buffer;
    }

    DLLLOCAL size_t getElementCount() const {
        return buffer->size();
    }

    DLLLOCAL int64_t getRank() const {
        return static_cast<int64_t>(shape.size());
    }

private:
    QoreBufferNode* buffer = nullptr;
    std::vector<int64_t> shape;

    DLLLOCAL static int parseShape(const QoreListNode* shape, std::vector<int64_t>& out,
        ExceptionSink* xsink);
    DLLLOCAL static int64_t shapeElementCount(const std::vector<int64_t>& shape, ExceptionSink* xsink);
    DLLLOCAL static int inferListShape(QoreValue value, std::vector<int64_t>& shape, ExceptionSink* xsink);
    DLLLOCAL static int flattenToList(QoreValue value, QoreListNode& out, ExceptionSink* xsink);
    DLLLOCAL static QoreBufferElementType inferElementType(QoreValue value, ExceptionSink* xsink);
    DLLLOCAL QoreValue toListImpl(size_t depth, size_t& offset, ExceptionSink* xsink) const;
};

DLLLOCAL QoreObject* qore_ml_tensor_to_object(QoreTensor* tensor, QoreProgram* pgm,
    ExceptionSink* xsink);

//! Testing/diagnostic helper: returns a copy of host tagged as residing on the
//! named device, with a host-copy-back callback (no real accelerator required).
DLLLOCAL QoreTensor* qore_ml_make_mock_device_tensor(const QoreTensor* host,
    const char* device_kind, int64_t device_id, ExceptionSink* xsink);

//! Uploads a host tensor's data to accelerator device memory (CUDA in v1) and returns
//! a device-backed tensor with a real device->host copy-back callback.  Raises
//! ML-TENSOR-DEVICE-ERROR when the device kind is unsupported, the build has no device
//! runtime, or a device allocation/copy fails.
DLLLOCAL QoreTensor* qore_ml_make_device_tensor(const QoreTensor* host,
    const char* device_kind, int64_t device_id, ExceptionSink* xsink);

#endif // _QORE_MODULE_ML_QC_TENSOR_H
