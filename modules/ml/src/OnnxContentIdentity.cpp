/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    OnnxContentIdentity.cpp

    Qore ml module - ONNX loaded-content identity

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

#include "OnnxContentIdentity.h"

#include <qore/qore_thread.h>
#include <qore/QoreSandboxManager.h>

#include <openssl/evp.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <map>

//! maximum nesting depth for subgraphs and local functions
static constexpr unsigned ONNX_SCAN_MAX_DEPTH = 64;

//! number of protobuf messages processed between cancellation checks
static constexpr unsigned ONNX_SCAN_CANCEL_INTERVAL = 4096;

//! upper bound on messages scanned in a single model; protects against pathological input
static constexpr uint64_t ONNX_SCAN_MAX_MESSAGES = 50000000;

//! I/O chunk size used for bounded, cancellable reads
static constexpr size_t ONNX_READ_CHUNK = 1024 * 1024;

//! protobuf wire types
static constexpr uint32_t WIRE_VARINT = 0;
static constexpr uint32_t WIRE_64BIT = 1;
static constexpr uint32_t WIRE_LEN = 2;
static constexpr uint32_t WIRE_32BIT = 5;

//! TensorProto.DataLocation.EXTERNAL
static constexpr int64_t ONNX_DATA_LOCATION_EXTERNAL = 1;

namespace {

//! RAII wrapper for an OpenSSL digest context
class DigestContext {
public:
    DLLLOCAL DigestContext() : ctx(EVP_MD_CTX_new()) {
    }

    DLLLOCAL ~DigestContext() {
        if (ctx) {
            EVP_MD_CTX_free(ctx);
        }
    }

    DigestContext(const DigestContext&) = delete;
    DigestContext& operator=(const DigestContext&) = delete;

    DLLLOCAL bool init(ExceptionSink* xsink) {
        if (!ctx || !EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr)) {
            xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
                "failed to initialize the %s digest context", QORE_ONNX_CONTENT_IDENTITY_ALGORITHM);
            return false;
        }
        return true;
    }

    DLLLOCAL bool update(const void* data, size_t len, ExceptionSink* xsink) {
        if (!len) {
            return true;
        }
        if (!EVP_DigestUpdate(ctx, data, len)) {
            xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
                "failed to update the %s digest context", QORE_ONNX_CONTENT_IDENTITY_ALGORITHM);
            return false;
        }
        return true;
    }

    DLLLOCAL bool final(std::string& hex, ExceptionSink* xsink) {
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int md_len = 0;
        if (!EVP_DigestFinal_ex(ctx, md, &md_len)) {
            xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
                "failed to finalize the %s digest", QORE_ONNX_CONTENT_IDENTITY_ALGORITHM);
            return false;
        }
        static const char hex_chars[] = "0123456789abcdef";
        hex.clear();
        hex.reserve(static_cast<size_t>(md_len) * 2);
        for (unsigned int i = 0; i < md_len; ++i) {
            hex.push_back(hex_chars[(md[i] >> 4) & 0xf]);
            hex.push_back(hex_chars[md[i] & 0xf]);
        }
        return true;
    }

private:
    EVP_MD_CTX* ctx;
};

//! RAII wrapper for a file descriptor
class ScopedFd {
public:
    DLLLOCAL ScopedFd() = default;

    DLLLOCAL explicit ScopedFd(int fd) : fd(fd) {
    }

    DLLLOCAL ~ScopedFd() {
        reset();
    }

    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;

    DLLLOCAL void reset(int new_fd = -1) {
        if (fd >= 0) {
            while (::close(fd) && errno == EINTR) {
                // retry interrupted close
            }
        }
        fd = new_fd;
    }

    DLLLOCAL int get() const {
        return fd;
    }

    DLLLOCAL bool valid() const {
        return fd >= 0;
    }

private:
    int fd = -1;
};

//! Bounds-checked protobuf wire-format reader
class WireReader {
public:
    DLLLOCAL WireReader(const char* data, size_t len)
        : pos(reinterpret_cast<const uint8_t*>(data)),
          end(reinterpret_cast<const uint8_t*>(data) + len) {
    }

    DLLLOCAL bool atEnd() const {
        return pos >= end;
    }

    DLLLOCAL bool readVarint(uint64_t& out) {
        out = 0;
        unsigned shift = 0;
        while (pos < end) {
            const uint8_t b = *pos++;
            if (shift > 63) {
                return false;
            }
            out |= static_cast<uint64_t>(b & 0x7f) << shift;
            if (!(b & 0x80)) {
                return true;
            }
            shift += 7;
        }
        return false;
    }

    DLLLOCAL bool readTag(uint32_t& field, uint32_t& wire) {
        uint64_t tag;
        if (!readVarint(tag)) {
            return false;
        }
        wire = static_cast<uint32_t>(tag & 0x7);
        const uint64_t fld = tag >> 3;
        if (!fld || fld > 0x1fffffff) {
            return false;
        }
        field = static_cast<uint32_t>(fld);
        return true;
    }

    DLLLOCAL bool readLengthDelimited(const char*& out, size_t& out_len) {
        uint64_t len;
        if (!readVarint(len)) {
            return false;
        }
        if (len > static_cast<uint64_t>(end - pos)) {
            return false;
        }
        out = reinterpret_cast<const char*>(pos);
        out_len = static_cast<size_t>(len);
        pos += len;
        return true;
    }

    //! Skips a field of the given wire type; returns false on malformed input
    DLLLOCAL bool skipField(uint32_t wire) {
        switch (wire) {
            case WIRE_VARINT: {
                uint64_t ignored;
                return readVarint(ignored);
            }
            case WIRE_64BIT: {
                if (end - pos < 8) {
                    return false;
                }
                pos += 8;
                return true;
            }
            case WIRE_LEN: {
                const char* data;
                size_t len;
                return readLengthDelimited(data, len);
            }
            case WIRE_32BIT: {
                if (end - pos < 4) {
                    return false;
                }
                pos += 4;
                return true;
            }
            default:
                // wire types 3 and 4 (groups) do not occur in the ONNX schema
                return false;
        }
    }

private:
    const uint8_t* pos;
    const uint8_t* end;
};

//! Scan state shared by all recursion levels
struct ScanContext {
    std::vector<OnnxExternalTensorRef>* refs = nullptr;
    std::string* unsupported = nullptr;
    ExceptionSink* xsink = nullptr;
    uint64_t messages = 0;
    bool cancelled = false;

    //! Accounts for one scanned message and performs periodic cancellation checks
    DLLLOCAL bool step() {
        if (++messages > ONNX_SCAN_MAX_MESSAGES) {
            xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
                "the ONNX model declares more than %llu protobuf messages; refusing to scan it for "
                "external tensor data dependencies",
                (unsigned long long)ONNX_SCAN_MAX_MESSAGES);
            return false;
        }
        if (!(messages % ONNX_SCAN_CANCEL_INTERVAL)
            && qore_check_cancel(xsink, "scanning an ONNX model for external tensor data")) {
            cancelled = true;
            return false;
        }
        return true;
    }

    //! Records a dependency class that cannot be enumerated from the model
    DLLLOCAL void markUnsupported(const char* reason) {
        if (unsupported->empty()) {
            *unsupported = reason;
        }
    }
};

bool scanGraph(const char* data, size_t len, unsigned depth, bool top_level, ScanContext& ctx);
bool scanTensor(const char* data, size_t len, bool bindable, ScanContext& ctx);

//! Raises the standard malformed-model exception
bool malformedModel(ExceptionSink* xsink) {
    xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
        "the ONNX model could not be parsed as a valid protobuf ModelProto message while "
        "determining its external tensor data dependencies");
    return false;
}

//! Returns true if the depth limit has been exceeded
bool checkDepth(unsigned depth, ExceptionSink* xsink) {
    if (depth > ONNX_SCAN_MAX_DEPTH) {
        xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
            "the ONNX model nests subgraphs or local functions more than %u levels deep; refusing "
            "to scan it for external tensor data dependencies", ONNX_SCAN_MAX_DEPTH);
        return false;
    }
    return true;
}

//! Parses a StringStringEntryProto into a key/value pair
bool parseStringStringEntry(const char* data, size_t len, std::string& key, std::string& value,
        ScanContext& ctx) {
    if (!ctx.step()) {
        return false;
    }
    WireReader r(data, len);
    while (!r.atEnd()) {
        uint32_t field, wire;
        if (!r.readTag(field, wire)) {
            return malformedModel(ctx.xsink);
        }
        if (wire == WIRE_LEN && (field == 1 || field == 2)) {
            const char* sdata;
            size_t slen;
            if (!r.readLengthDelimited(sdata, slen)) {
                return malformedModel(ctx.xsink);
            }
            if (field == 1) {
                key.assign(sdata, slen);
            } else {
                value.assign(sdata, slen);
            }
            continue;
        }
        if (!r.skipField(wire)) {
            return malformedModel(ctx.xsink);
        }
    }
    return true;
}

//! Parses a signed decimal value declared in an external_data entry
bool parseExternalInt(const std::string& key, const std::string& value, int64_t& out,
        ExceptionSink* xsink) {
    if (value.empty()) {
        xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
            "the ONNX model declares an empty %s value for an external tensor data reference",
            key.c_str());
        return false;
    }
    errno = 0;
    char* endptr = nullptr;
    const long long v = strtoll(value.c_str(), &endptr, 10);
    if (errno || !endptr || *endptr || v < 0) {
        xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
            "the ONNX model declares an invalid %s value '%s' for an external tensor data reference",
            key.c_str(), value.c_str());
        return false;
    }
    out = static_cast<int64_t>(v);
    return true;
}

//! Parses a TensorProto, recording any external data reference it declares
/** @param bindable whether the tensor's external data can be supplied to ONNX Runtime as an
    in-memory initializer file; only the top-level graph's initializers can be
*/
bool scanTensor(const char* data, size_t len, bool bindable, ScanContext& ctx) {
    if (!ctx.step()) {
        return false;
    }
    WireReader r(data, len);
    int64_t data_location = 0;
    OnnxExternalTensorRef ref;
    bool have_location = false;
    bool have_entries = false;

    while (!r.atEnd()) {
        uint32_t field, wire;
        if (!r.readTag(field, wire)) {
            return malformedModel(ctx.xsink);
        }
        // TensorProto.data_location
        if (field == 14 && wire == WIRE_VARINT) {
            uint64_t v;
            if (!r.readVarint(v)) {
                return malformedModel(ctx.xsink);
            }
            data_location = static_cast<int64_t>(v);
            continue;
        }
        // TensorProto.external_data
        if (field == 13 && wire == WIRE_LEN) {
            const char* edata;
            size_t elen;
            if (!r.readLengthDelimited(edata, elen)) {
                return malformedModel(ctx.xsink);
            }
            std::string key, value;
            if (!parseStringStringEntry(edata, elen, key, value, ctx)) {
                return false;
            }
            have_entries = true;
            if (key == "location") {
                ref.location = value;
                have_location = true;
            } else if (key == "offset") {
                if (!parseExternalInt(key, value, ref.offset, ctx.xsink)) {
                    return false;
                }
            } else if (key == "length") {
                if (!parseExternalInt(key, value, ref.length, ctx.xsink)) {
                    return false;
                }
            }
            // "checksum" is ignored: the identity is established from the bytes actually consumed
            continue;
        }
        if (!r.skipField(wire)) {
            return malformedModel(ctx.xsink);
        }
    }

    if (data_location != ONNX_DATA_LOCATION_EXTERNAL) {
        return true;
    }
    if (!have_location || ref.location.empty()) {
        ctx.xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
            "the ONNX model declares a tensor with external data but %s",
            have_entries ? "without a \"location\" entry" : "without any external_data entries");
        return false;
    }
    ref.bindable = bindable;
    ctx.refs->push_back(ref);
    return true;
}

//! Parses a SparseTensorProto (values and indices are TensorProto messages)
bool scanSparseTensor(const char* data, size_t len, ScanContext& ctx) {
    if (!ctx.step()) {
        return false;
    }
    WireReader r(data, len);
    while (!r.atEnd()) {
        uint32_t field, wire;
        if (!r.readTag(field, wire)) {
            return malformedModel(ctx.xsink);
        }
        if (wire == WIRE_LEN && (field == 1 || field == 2)) {
            const char* tdata;
            size_t tlen;
            if (!r.readLengthDelimited(tdata, tlen)) {
                return malformedModel(ctx.xsink);
            }
            // ONNX Runtime resolves sparse tensor data itself; it cannot be supplied in memory
            if (!scanTensor(tdata, tlen, false, ctx)) {
                return false;
            }
            continue;
        }
        if (!r.skipField(wire)) {
            return malformedModel(ctx.xsink);
        }
    }
    return true;
}

//! Values pulled out of an AttributeProto while scanning it
struct AttributeValues {
    std::string name;
    std::string s;
    int64_t i = 0;
    bool have_i = false;
    bool have_s = false;
};

//! Parses an AttributeProto, scanning nested tensors and subgraphs
bool scanAttribute(const char* data, size_t len, unsigned depth, ScanContext& ctx,
        AttributeValues& values) {
    if (!ctx.step()) {
        return false;
    }
    WireReader r(data, len);
    while (!r.atEnd()) {
        uint32_t field, wire;
        if (!r.readTag(field, wire)) {
            return malformedModel(ctx.xsink);
        }
        // AttributeProto.i
        if (field == 3 && wire == WIRE_VARINT) {
            uint64_t v;
            if (!r.readVarint(v)) {
                return malformedModel(ctx.xsink);
            }
            values.i = static_cast<int64_t>(v);
            values.have_i = true;
            continue;
        }
        if (wire != WIRE_LEN) {
            if (!r.skipField(wire)) {
                return malformedModel(ctx.xsink);
            }
            continue;
        }
        const char* adata;
        size_t alen;
        if (!r.readLengthDelimited(adata, alen)) {
            return malformedModel(ctx.xsink);
        }
        switch (field) {
            case 1:  // name
                values.name.assign(adata, alen);
                break;
            case 4:  // s
                values.s.assign(adata, alen);
                values.have_s = true;
                break;
            case 5:   // t
            case 10:  // tensors
                if (!scanTensor(adata, alen, false, ctx)) {
                    return false;
                }
                break;
            case 6:   // g
            case 11:  // graphs
                if (!scanGraph(adata, alen, depth + 1, false, ctx)) {
                    return false;
                }
                break;
            case 22:  // sparse_tensor
            case 23:  // sparse_tensors
                if (!scanSparseTensor(adata, alen, ctx)) {
                    return false;
                }
                break;
            default:
                break;
        }
    }
    return true;
}

//! Parses a NodeProto, scanning its attributes and detecting non-embedded EP context binaries
bool scanNode(const char* data, size_t len, unsigned depth, ScanContext& ctx) {
    if (!ctx.step()) {
        return false;
    }
    WireReader r(data, len);
    std::string op_type;
    bool ep_context_external = false;
    bool ep_context_embed_mode_seen = false;
    bool ep_context_has_cache = false;

    while (!r.atEnd()) {
        uint32_t field, wire;
        if (!r.readTag(field, wire)) {
            return malformedModel(ctx.xsink);
        }
        if (wire != WIRE_LEN) {
            if (!r.skipField(wire)) {
                return malformedModel(ctx.xsink);
            }
            continue;
        }
        const char* ndata;
        size_t nlen;
        if (!r.readLengthDelimited(ndata, nlen)) {
            return malformedModel(ctx.xsink);
        }
        if (field == 4) {  // op_type
            op_type.assign(ndata, nlen);
            continue;
        }
        if (field == 5) {  // attribute
            AttributeValues values;
            if (!scanAttribute(ndata, nlen, depth, ctx, values)) {
                return false;
            }
            if (values.name == "embed_mode" && values.have_i) {
                ep_context_embed_mode_seen = true;
                if (!values.i) {
                    ep_context_external = true;
                }
            } else if (values.name == "ep_cache_context" && values.have_s && !values.s.empty()) {
                ep_context_has_cache = true;
            }
            continue;
        }
    }

    // an EPContext node with embed_mode == 0 loads its compiled context from a separate binary
    // that is not an ONNX external tensor: the loaded content cannot be bound from the model
    if (op_type == "EPContext" && ep_context_embed_mode_seen && ep_context_external
        && ep_context_has_cache) {
        ctx.markUnsupported("the model contains an EPContext node with a non-embedded execution "
            "provider context binary, which is not an ONNX external tensor dependency and cannot "
            "be bound to the loaded session");
    }
    return true;
}

//! Parses a GraphProto
/** @param top_level whether this is the model's own graph, whose initializers ONNX Runtime can
    take as in-memory initializer files
*/
bool scanGraph(const char* data, size_t len, unsigned depth, bool top_level, ScanContext& ctx) {
    if (!checkDepth(depth, ctx.xsink) || !ctx.step()) {
        return false;
    }
    WireReader r(data, len);
    while (!r.atEnd()) {
        uint32_t field, wire;
        if (!r.readTag(field, wire)) {
            return malformedModel(ctx.xsink);
        }
        if (wire != WIRE_LEN) {
            if (!r.skipField(wire)) {
                return malformedModel(ctx.xsink);
            }
            continue;
        }
        const char* gdata;
        size_t glen;
        if (!r.readLengthDelimited(gdata, glen)) {
            return malformedModel(ctx.xsink);
        }
        switch (field) {
            case 1:  // node
                if (!scanNode(gdata, glen, depth, ctx)) {
                    return false;
                }
                break;
            case 5:  // initializer
                if (!scanTensor(gdata, glen, top_level, ctx)) {
                    return false;
                }
                break;
            case 15:  // sparse_initializer
                if (!scanSparseTensor(gdata, glen, ctx)) {
                    return false;
                }
                break;
            default:
                break;
        }
    }
    return true;
}

//! Parses a FunctionProto (local function bodies can carry constant tensors)
bool scanFunction(const char* data, size_t len, unsigned depth, ScanContext& ctx) {
    if (!checkDepth(depth, ctx.xsink) || !ctx.step()) {
        return false;
    }
    WireReader r(data, len);
    while (!r.atEnd()) {
        uint32_t field, wire;
        if (!r.readTag(field, wire)) {
            return malformedModel(ctx.xsink);
        }
        if (wire != WIRE_LEN) {
            if (!r.skipField(wire)) {
                return malformedModel(ctx.xsink);
            }
            continue;
        }
        const char* fdata;
        size_t flen;
        if (!r.readLengthDelimited(fdata, flen)) {
            return malformedModel(ctx.xsink);
        }
        if (field == 4) {  // node
            if (!scanNode(fdata, flen, depth, ctx)) {
                return false;
            }
        }
    }
    return true;
}

//! Reads \a size bytes from \a fd into \a out in bounded, cancellable chunks
bool readAllBytes(int fd, size_t size, const char* what, const std::string& display,
        std::vector<char>& out, ExceptionSink* xsink) {
    out.clear();
    out.resize(size);
    size_t total = 0;
    while (total < size) {
        if (qore_check_cancel(xsink, "reading ONNX model content")) {
            return false;
        }
        const size_t want = std::min(ONNX_READ_CHUNK, size - total);
        const ssize_t rc = ::read(fd, out.data() + total, want);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            xsink->raiseErrnoException(ML_ONNX_CONTENT_IDENTITY_ERROR, errno,
                "failed to read %s '%s' while capturing ONNX model content identity", what,
                display.c_str());
            return false;
        }
        if (!rc) {
            xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
                "%s '%s' was truncated while capturing ONNX model content identity: expected %zu "
                "bytes but only %zu could be read", what, display.c_str(), size, total);
            return false;
        }
        total += static_cast<size_t>(rc);
    }

    // a file that grew during the read cannot be attributed to a single content state
    char extra;
    ssize_t rc;
    while ((rc = ::read(fd, &extra, 1)) < 0 && errno == EINTR) {
        // retry interrupted read
    }
    if (rc > 0) {
        xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
            "%s '%s' changed size while capturing ONNX model content identity; its content cannot "
            "be attributed to the loaded session", what, display.c_str());
        return false;
    }
    return true;
}

//! Opens and reads a file in full, returning its content
/** External data locations are declared by the model itself, so every path read here is subject
    to the calling program's filesystem sandbox policy.
*/
bool readWholeFile(const char* path, const char* what, const std::string& display,
        std::vector<char>& out, ExceptionSink* xsink) {
    QoreSandboxManagerHelper smh;
    if (smh) {
        if (!smh->checkFilesystemAccess(path, QSEC_READ, xsink)) {
            return false;
        }
    }
    ScopedFd fd(::open(path, O_RDONLY | O_CLOEXEC));
    if (!fd.valid()) {
        xsink->raiseErrnoException(ML_ONNX_CONTENT_IDENTITY_ERROR, errno,
            "failed to open %s '%s' while capturing ONNX model content identity", what,
            display.c_str());
        return false;
    }
    struct stat sb;
    if (::fstat(fd.get(), &sb)) {
        xsink->raiseErrnoException(ML_ONNX_CONTENT_IDENTITY_ERROR, errno,
            "failed to stat %s '%s' while capturing ONNX model content identity", what,
            display.c_str());
        return false;
    }
    if (!S_ISREG(sb.st_mode)) {
        xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
            "%s '%s' is not a regular file; ONNX model content identity can only be established "
            "from regular files", what, display.c_str());
        return false;
    }
    return readAllBytes(fd.get(), static_cast<size_t>(sb.st_size), what, display, out, xsink);
}

//! Computes the hex SHA-256 digest of a byte range
bool digestBytes(const char* data, size_t len, std::string& out, ExceptionSink* xsink) {
    DigestContext ctx;
    if (!ctx.init(xsink)) {
        return false;
    }
    size_t done = 0;
    while (done < len) {
        if (qore_check_cancel(xsink, "computing an ONNX content identity digest")) {
            return false;
        }
        const size_t chunk = std::min(ONNX_READ_CHUNK, len - done);
        if (!ctx.update(data + done, chunk, xsink)) {
            return false;
        }
        done += chunk;
    }
    return ctx.final(out, xsink);
}

//! Returns the directory component of a path, or "." when there is none
std::string dirNameOf(const char* path) {
    const std::string p(path);
    const size_t pos = p.find_last_of('/');
    if (pos == std::string::npos) {
        return ".";
    }
    if (!pos) {
        return "/";
    }
    return p.substr(0, pos);
}

//! Validates an external data location declared by the model
/** ONNX requires external data locations to be relative paths that do not escape the model
    directory; anything else is rejected rather than followed.
*/
bool validateExternalLocation(const std::string& location, ExceptionSink* xsink) {
    if (location.empty()) {
        xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
            "the ONNX model declares an empty external tensor data location");
        return false;
    }
    if (location[0] == '/' || location.find('\\') != std::string::npos) {
        xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
            "the ONNX model declares external tensor data location '%s'; only relative paths "
            "with \"/\" separators are accepted", location.c_str());
        return false;
    }
    size_t start = 0;
    while (start <= location.size()) {
        const size_t sep = location.find('/', start);
        const std::string component = location.substr(start,
            sep == std::string::npos ? std::string::npos : sep - start);
        if (component == "..") {
            xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
                "the ONNX model declares external tensor data location '%s'; locations that "
                "traverse outside the model directory are not accepted", location.c_str());
            return false;
        }
        if (sep == std::string::npos) {
            break;
        }
        start = sep + 1;
    }
    return true;
}

//! Applies a capture failure according to the capture mode
bool applyCaptureFailure(OnnxContentIdentityMode mode, OnnxContentIdentity& identity,
        ExceptionSink* xsink) {
    if (mode == OnnxContentIdentityMode::Required) {
        return false;
    }
    // best effort: report the failure as an unknown identity rather than failing the load
    std::string reason;
    if (*xsink) {
        const QoreValue err = xsink->getExceptionErr();
        const QoreValue desc = xsink->getExceptionDesc();
        if (desc.getType() == NT_STRING) {
            reason = desc.get<const QoreStringNode>()->c_str();
        } else if (err.getType() == NT_STRING) {
            reason = err.get<const QoreStringNode>()->c_str();
        }
        xsink->clear();
    }
    if (reason.empty()) {
        reason = "the model content could not be captured";
    }
    // discard the partial capture so the model loads exactly as it would without capture
    identity.discardCapture(reason);
    return true;
}

//! Captures model content identity from model bytes and a resolution base directory
bool captureFromBytes(std::vector<char>&& model_bytes, const std::string& base_dir,
        const char* source, OnnxContentIdentityMode mode, OnnxContentIdentity& identity,
        ExceptionSink* xsink) {
    identity.source = source;
    identity.captured = true;
    identity.attempted = true;
    identity.model_bytes = std::move(model_bytes);

    if (!digestBytes(identity.model_bytes.data(), identity.model_bytes.size(),
            identity.model_digest, xsink)) {
        return applyCaptureFailure(mode, identity, xsink);
    }

    std::vector<OnnxExternalTensorRef> refs;
    std::string unsupported;
    if (!scanOnnxExternalTensorRefs(identity.model_bytes.data(), identity.model_bytes.size(), refs,
            unsupported, xsink)) {
        return applyCaptureFailure(mode, identity, xsink);
    }

    // external data that ONNX Runtime resolves by itself cannot be attributed to the loaded
    // session: report an unknown identity rather than a digest of bytes the session may not have
    // consumed, and never change how such a model loads
    size_t scanned = 0;
    for (const OnnxExternalTensorRef& ref : refs) {
        if (++scanned % 100 == 0
            && qore_check_cancel(xsink, "checking ONNX external tensor data references")) {
            return applyCaptureFailure(mode, identity, xsink);
        }
        if (ref.bindable) {
            continue;
        }
        const char* reason = "the model declares external tensor data outside its top-level "
            "initializers (in a subgraph, a local function body, a node attribute or a sparse "
            "tensor), which ONNX Runtime resolves from the model directory itself, so those bytes "
            "cannot be bound to the loaded session";
        if (mode == OnnxContentIdentityMode::Required) {
            xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
                "a complete ONNX content identity was required but could not be established: %s",
                reason);
            return false;
        }
        identity.discardCapture(reason);
        return true;
    }

    // capture each distinct external file exactly once
    std::map<std::string, size_t> file_index;
    size_t checked = 0;
    for (const OnnxExternalTensorRef& ref : refs) {
        if (++checked % 100 == 0
            && qore_check_cancel(xsink, "capturing ONNX external tensor data")) {
            return applyCaptureFailure(mode, identity, xsink);
        }
        if (!validateExternalLocation(ref.location, xsink)) {
            return applyCaptureFailure(mode, identity, xsink);
        }
        if (file_index.find(ref.location) != file_index.end()) {
            continue;
        }
        OnnxExternalFileCapture capture;
        capture.location = ref.location;
        const std::string path = base_dir + "/" + ref.location;
        if (!readWholeFile(path.c_str(), "external tensor data file", ref.location, capture.data,
                xsink)) {
            return applyCaptureFailure(mode, identity, xsink);
        }
        if (!digestBytes(capture.data.data(), capture.data.size(), capture.digest, xsink)) {
            return applyCaptureFailure(mode, identity, xsink);
        }
        file_index[ref.location] = identity.files.size();
        identity.files.push_back(std::move(capture));
    }

    // digest each distinct byte range consumed by the model
    std::map<std::string, size_t> range_index;
    checked = 0;
    for (const OnnxExternalTensorRef& ref : refs) {
        if (++checked % 100 == 0
            && qore_check_cancel(xsink, "digesting ONNX external tensor data")) {
            return applyCaptureFailure(mode, identity, xsink);
        }
        const OnnxExternalFileCapture& file = identity.files[file_index[ref.location]];
        const int64_t file_size = static_cast<int64_t>(file.data.size());
        const int64_t length = ref.length < 0 ? file_size - ref.offset : ref.length;
        if (ref.offset > file_size || length < 0 || ref.offset + length > file_size) {
            xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
                "the ONNX model declares an external tensor at offset %lld with length %lld in "
                "'%s', but that file holds only %lld bytes", (long long)ref.offset,
                (long long)(ref.length < 0 ? length : ref.length), ref.location.c_str(),
                (long long)file_size);
            return applyCaptureFailure(mode, identity, xsink);
        }

        char key[64];
        snprintf(key, sizeof(key), "%lld:%lld", (long long)ref.offset, (long long)length);
        const std::string range_key = ref.location + "\n" + key;
        auto i = range_index.find(range_key);
        if (i != range_index.end()) {
            ++identity.ranges[i->second].references;
            continue;
        }

        OnnxExternalRangeCapture range;
        range.location = ref.location;
        range.offset = ref.offset;
        range.length = length;
        range.references = 1;
        if (!digestBytes(file.data.data() + ref.offset, static_cast<size_t>(length), range.digest,
                xsink)) {
            return applyCaptureFailure(mode, identity, xsink);
        }
        range_index[range_key] = identity.ranges.size();
        identity.ranges.push_back(std::move(range));
    }

    if (!unsupported.empty()) {
        if (mode == OnnxContentIdentityMode::Required) {
            xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
                "a complete ONNX content identity was required but could not be established: %s",
                unsupported.c_str());
            return false;
        }
        identity.discardCapture(unsupported);
        return true;
    }

    identity.complete = true;
    identity.finalize();
    if (identity.getDigest().empty()) {
        // finalize() could not compute the manifest digest
        identity.complete = false;
        if (mode == OnnxContentIdentityMode::Required) {
            xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
                "a complete ONNX content identity was required but the identity manifest digest "
                "could not be computed");
            return false;
        }
        identity.discardCapture("the identity manifest digest could not be computed");
    }
    return true;
}
}  // anonymous namespace

void OnnxContentIdentity::setUnknown(const std::string& reason) {
    complete = false;
    digest.clear();
    unknown_reason = reason;
}

void OnnxContentIdentity::discardCapture(const std::string& reason) {
    captured = false;
    model_bytes.clear();
    model_bytes.shrink_to_fit();
    model_digest.clear();
    files.clear();
    ranges.clear();
    setUnknown(reason);
}

void OnnxContentIdentity::finalize() {
    ExceptionSink xsink;
    std::string manifest;
    manifest.reserve(256 + ranges.size() * 160);
    manifest += "qore-onnx-content-identity/";
    manifest += std::to_string(QORE_ONNX_CONTENT_IDENTITY_VERSION);
    manifest += "\nalgorithm:" QORE_ONNX_CONTENT_IDENTITY_ALGORITHM "\n";
    manifest += "source:" + source + "\n";
    manifest += "model:" + model_digest + ":" + std::to_string(model_bytes.size()) + "\n";

    // dependency lines never carry the declared location itself, so a content-preserving
    // relocation of the model package cannot change the identity
    std::vector<std::string> lines;
    lines.reserve(ranges.size());
    for (const OnnxExternalRangeCapture& range : ranges) {
        if (qore_check_cancel(&xsink, "building the ONNX content identity manifest")) {
            xsink.clear();
            digest.clear();
            return;
        }
        std::string location_id;
        if (!digestBytes(range.location.c_str(), range.location.size(), location_id, &xsink)) {
            xsink.clear();
            digest.clear();
            return;
        }
        lines.push_back("dep:" + location_id + ":" + std::to_string(range.offset) + ":"
            + std::to_string(range.length) + ":" + range.digest + "\n");
    }
    std::sort(lines.begin(), lines.end());
    for (const std::string& line : lines) {
        manifest += line;
    }

    if (!digestBytes(manifest.c_str(), manifest.size(), digest, &xsink)) {
        xsink.clear();
        digest.clear();
    }
}

QoreHashNode* OnnxContentIdentity::toHash(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclOnnxContentIdentity, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    rv->setKeyValue("version", (int64_t)QORE_ONNX_CONTENT_IDENTITY_VERSION, xsink);
    rv->setKeyValue("algorithm", new QoreStringNode(QORE_ONNX_CONTENT_IDENTITY_ALGORITHM), xsink);
    rv->setKeyValue("complete", complete, xsink);
    rv->setKeyValue("status", new QoreStringNode(complete ? "complete" : "unknown"), xsink);
    rv->setKeyValue("source", new QoreStringNode(source.empty() ? "unknown" : source.c_str()),
        xsink);
    if (!complete && !unknown_reason.empty()) {
        rv->setKeyValue("unknown_reason", new QoreStringNode(unknown_reason), xsink);
    }
    if (complete && !digest.empty()) {
        rv->setKeyValue("digest", new QoreStringNode(digest), xsink);
    }
    if (!model_digest.empty()) {
        rv->setKeyValue("model_digest", new QoreStringNode(model_digest), xsink);
        rv->setKeyValue("model_size", (int64_t)model_bytes.size(), xsink);
    }
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreListNode> deps(new QoreListNode(hashdeclOnnxContentDependency
        ->getTypeInfo(false)), xsink);
    for (const OnnxExternalRangeCapture& range : ranges) {
        if (qore_check_cancel(xsink, "building ONNX content identity info")) {
            return nullptr;
        }
        std::string location_id;
        if (!digestBytes(range.location.c_str(), range.location.size(), location_id, xsink)) {
            return nullptr;
        }
        ReferenceHolder<QoreHashNode> dep(new QoreHashNode(hashdeclOnnxContentDependency, xsink),
            xsink);
        if (*xsink) {
            return nullptr;
        }
        dep->setKeyValue("id", new QoreStringNode(location_id), xsink);
        dep->setKeyValue("offset", range.offset, xsink);
        dep->setKeyValue("length", range.length, xsink);
        dep->setKeyValue("digest", new QoreStringNode(range.digest), xsink);
        dep->setKeyValue("references", (int64_t)range.references, xsink);
        if (*xsink) {
            return nullptr;
        }
        deps->push(dep.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    rv->setKeyValue("dependencies", deps.release(), xsink);
    if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

bool parseOnnxContentIdentityMode(const QoreHashNode* config, OnnxContentIdentityMode& mode,
        ExceptionSink* xsink) {
    mode = OnnxContentIdentityMode::Disabled;
    if (!config) {
        return true;
    }
    const QoreValue val = config->getKeyValue("content_identity");
    if (val.isNullOrNothing()) {
        return true;
    }
    QoreStringValueHelper str(val);
    const char* value = str->c_str();
    if (!strcmp(value, "disabled")) {
        mode = OnnxContentIdentityMode::Disabled;
        return true;
    }
    if (!strcmp(value, "best_effort")) {
        mode = OnnxContentIdentityMode::BestEffort;
        return true;
    }
    if (!strcmp(value, "required")) {
        mode = OnnxContentIdentityMode::Required;
        return true;
    }
    xsink->raiseException(ML_ONNX_CONTENT_IDENTITY_ERROR,
        "invalid content_identity value '%s'; expected \"disabled\", \"best_effort\" or "
        "\"required\"", value);
    return false;
}

bool scanOnnxExternalTensorRefs(const char* data, size_t len, std::vector<OnnxExternalTensorRef>& refs,
        std::string& unsupported, ExceptionSink* xsink) {
    ScanContext ctx;
    ctx.refs = &refs;
    ctx.unsupported = &unsupported;
    ctx.xsink = xsink;

    if (!ctx.step()) {
        return false;
    }
    WireReader r(data, len);
    while (!r.atEnd()) {
        uint32_t field, wire;
        if (!r.readTag(field, wire)) {
            return malformedModel(xsink);
        }
        if (wire != WIRE_LEN) {
            if (!r.skipField(wire)) {
                return malformedModel(xsink);
            }
            continue;
        }
        const char* mdata;
        size_t mlen;
        if (!r.readLengthDelimited(mdata, mlen)) {
            return malformedModel(xsink);
        }
        if (field == 7) {  // ModelProto.graph
            if (!scanGraph(mdata, mlen, 0, true, ctx)) {
                return false;
            }
        } else if (field == 25) {  // ModelProto.functions
            if (!scanFunction(mdata, mlen, 0, ctx)) {
                return false;
            }
        }
    }
    return true;
}

bool captureOnnxContentIdentityFromPath(const char* model_path, OnnxContentIdentityMode mode,
        OnnxContentIdentity& identity, ExceptionSink* xsink) {
    assert(mode != OnnxContentIdentityMode::Disabled);
    std::vector<char> model_bytes;
    // the model file is read once and the bytes are handed to ONNX Runtime, so the digest
    // describes exactly the graph the session loaded: the path is never re-read afterwards
    if (!readWholeFile(model_path, "ONNX model file", std::string(model_path), model_bytes,
            xsink)) {
        identity.source = "file";
        identity.attempted = true;
        return applyCaptureFailure(mode, identity, xsink);
    }
    return captureFromBytes(std::move(model_bytes), dirNameOf(model_path), "file", mode, identity,
        xsink);
}

bool captureOnnxContentIdentityFromMemory(const void* data, size_t len,
        OnnxContentIdentityMode mode, OnnxContentIdentity& identity, ExceptionSink* xsink) {
    assert(mode != OnnxContentIdentityMode::Disabled);
    const char* bytes = static_cast<const char*>(data);
    std::vector<char> model_bytes(bytes, bytes + len);

    // ONNX Runtime resolves external data for in-memory models against the current working
    // directory, so the capture must use the same base
    std::string base_dir = ".";
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        base_dir = cwd;
    }
    return captureFromBytes(std::move(model_bytes), base_dir, "memory", mode, identity, xsink);
}

QoreHashNode* makeUnknownOnnxContentIdentity(const char* source, const char* reason,
        ExceptionSink* xsink) {
    OnnxContentIdentity identity;
    identity.source = source;
    identity.attempted = true;
    identity.setUnknown(reason);
    return identity.toHash(xsink);
}
