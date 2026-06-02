/* -*- mode: objc; indent-tabs-mode: nil -*- */
/*
    Tensor_Metal.mm

    Qore ml module - Apple Silicon / macOS Metal (UMA) device-buffer allocator
    for ML::Tensor::toDevice("metal", ...).  Built only when HAVE_METAL is
    defined (Apple platforms with the Metal framework and an ONNX Runtime
    build).  ARC is enabled via -fobjc-arc in CMakeLists.txt.

    Apple Silicon's Unified Memory Architecture (UMA) places the CPU and GPU
    in one physical memory pool, so an MTLBuffer with MTLStorageModeShared is
    accessible to both at the same address (via [buffer contents] from host
    code).  A "device buffer" on UMA is therefore the same physical bytes as
    its host view: the copy-to-host callback is a memcpy from those same
    bytes (the substrate fills its own host slot), and -- critically -- no
    host<->device DMA hop occurs.  The transfer counter contract is enforced
    one level up in OnnxModel.cpp by skipping Metal-kind buffers in the
    db_host_to_device_transfers / db_device_to_host_transfers bumps.

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

#import <Metal/Metal.h>

#include "QC_Tensor.h"

#include <cstring>
#include <memory>
#include <string>

namespace {

//! Owner for a Metal MTLBuffer (shared storage mode) used to back a Qore
//! device-tagged buffer.  ARC retains the buffer when assigned and releases
//! it when this struct is destroyed.
struct MetalSharedOwner {
    id<MTLBuffer> buffer = nil;     // strong under ARC
    size_t element_size = 0;
    int64_t device_id = 0;

    ~MetalSharedOwner() {
        buffer = nil;               // ARC releases
    }
};

//! Copy-to-host callback for Metal/UMA device buffers.  On UMA the device
//! bytes are already host-visible via [buffer contents], so device_data
//! aliases the same physical memory.  We perform a memcpy into the
//! substrate's owned host slot (the contract of ensureHostStorage()) -- no
//! host<->device DMA hop occurs.  The transfer counter must NOT be bumped
//! here; callers in OnnxModel.cpp skip counter bumps for Metal-kind buffers
//! so the truthful zero-transfer accounting is preserved.
int metalSharedCopyToHost(void* host_data, uint8_t* /*host_validity*/, size_t length,
        const void* device_data, const uint8_t* /*device_validity*/,
        const QoreBufferDeviceInfo& /*info*/, const void* owner, ExceptionSink* /*xsink*/) {
    const MetalSharedOwner* o = static_cast<const MetalSharedOwner*>(owner);
    size_t bytes = length * (o ? o->element_size : 0);
    if (bytes) {
        memcpy(host_data, device_data, bytes);
    }
    return 0;
}

} // anonymous namespace

QoreTensor* qore_ml_make_metal_uma_tensor(const QoreTensor* host, int64_t device_id,
        ExceptionSink* xsink) {
    if (device_id < 0) {
        xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
            "device_id must be >= 0; got " QLLD, (long long)device_id);
        return nullptr;
    }

    const QoreBufferNode* host_buf = host->getBuffer();
    QoreBufferElementType et = host_buf->getElementType();
    if (et == QoreBufferElementType::Bool) {
        xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
            "Metal device tensors do not support bit-packed bool buffers");
        return nullptr;
    }
    // a device-resident source must be materialized to host before re-uploading
    if (host_buf->hasExternalDeviceStorage() && host_buf->ensureHostStorage(xsink)) {
        return nullptr;
    }

    size_t n = host_buf->size();
    size_t elem_size = qore_buffer_element_storage_size(et);
    size_t byte_size = n * elem_size;

    // MTLCreateSystemDefaultDevice() returns the single integrated Apple GPU on Apple
    // Silicon; on Intel Macs with multiple devices it returns the primary.  device_id
    // is accepted for API parity with CUDA/ROCm but only id 0 is currently honored.
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    if (!dev) {
        xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
            "no Metal-capable device is available on this system");
        return nullptr;
    }

    auto owner = std::make_shared<MetalSharedOwner>();
    owner->element_size = elem_size;
    owner->device_id = device_id;

    // MTLResourceStorageModeShared: CPU and GPU see the same physical bytes (UMA).
    // newBufferWithLength returns an autoreleased object that ARC retains when
    // assigned to the strong field.
    if (byte_size) {
        id<MTLBuffer> buf = [dev newBufferWithLength: byte_size
                                            options: MTLResourceStorageModeShared];
        if (!buf) {
            xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
                "failed to allocate %zu-byte Metal shared buffer", byte_size);
            return nullptr;
        }
        memcpy([buf contents], host_buf->getRawData(), byte_size);
        owner->buffer = buf;
    } else {
        // empty tensor: allocate a 1-byte MTLBuffer so [contents] is non-null and
        // remains valid for the lifetime of the owner; the buffer is unused
        id<MTLBuffer> buf = [dev newBufferWithLength: 1
                                            options: MTLResourceStorageModeShared];
        if (!buf) {
            xsink->raiseException("ML-TENSOR-DEVICE-ERROR",
                "failed to allocate sentinel Metal shared buffer");
            return nullptr;
        }
        owner->buffer = buf;
    }

    QoreBufferDeviceInfo info;
    info.kind = QoreBufferDeviceKind::Metal;
    info.device_id = device_id;
    info.name = std::string(qore_buffer_device_kind_name(info.kind)) + ":"
        + std::to_string(device_id);

    const void* device_data = byte_size ? [owner->buffer contents] : nullptr;
    ReferenceHolder<QoreBufferNode> dev_buf(
        QoreBufferNode::wrapExternalDeviceStorage(et, false, n, device_data, nullptr,
            std::static_pointer_cast<const void>(owner), 0, info, metalSharedCopyToHost, xsink),
        xsink);
    if (*xsink) {
        return nullptr;
    }
    return new QoreTensor(dev_buf.release(), host->getShape());
}
