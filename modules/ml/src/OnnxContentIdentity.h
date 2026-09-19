/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    OnnxContentIdentity.h

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

#ifndef _QORE_MODULE_ML_ONNXCONTENTIDENTITY_H
#define _QORE_MODULE_ML_ONNXCONTENTIDENTITY_H

#include <qore/Qore.h>

#include <cstdint>
#include <string>
#include <vector>

//! ONNX content-identity error code
#define ML_ONNX_CONTENT_IDENTITY_ERROR "ML-ONNX-CONTENT-IDENTITY-ERROR"

//! Identity manifest schema version
/** Bump whenever the manifest layout or digest composition changes; the value is
    reported to callers so a stored fingerprint can never be silently reinterpreted.
*/
#define QORE_ONNX_CONTENT_IDENTITY_VERSION 1

//! Digest algorithm used for all content identity digests
#define QORE_ONNX_CONTENT_IDENTITY_ALGORITHM "SHA-256"

//! ONNX content identity hashdecls (defined by the qpp-generated registration code in ql_ml.cpp)
extern const TypedHashDecl* hashdeclOnnxContentIdentity;
extern const TypedHashDecl* hashdeclOnnxContentDependency;

//! Content identity capture mode (from OnnxSessionConfig::content_identity)
enum class OnnxContentIdentityMode {
    Disabled,    //!< no capture; the identity is reported as unknown
    BestEffort,  //!< capture when possible; report unknown with a reason otherwise
    Required,    //!< capture or raise an exception
};

//! One external data file captured in full and supplied to ONNX Runtime
/** The captured bytes are handed to ONNX Runtime as the session's external
    initializer content, so the digest describes exactly the bytes the loaded
    session consumed; there is no window in which the file could be replaced
    between validation and use.
*/
struct OnnxExternalFileCapture {
    //! external data location exactly as declared in the model
    std::string location;
    //! captured file bytes; must outlive the ONNX Runtime session
    std::vector<char> data;
    //! hex SHA-256 digest of \a data
    std::string digest;
};

//! One distinct byte range consumed by an external tensor
struct OnnxExternalRangeCapture {
    //! external data location exactly as declared in the model
    std::string location;
    //! byte offset of the tensor data in the external file
    int64_t offset = 0;
    //! resolved byte length of the tensor data
    int64_t length = 0;
    //! hex SHA-256 digest of the consumed bytes
    std::string digest;
    //! number of tensors that consume this exact range
    size_t references = 0;
};

//! A raw external data reference as declared by a TensorProto
struct OnnxExternalTensorRef {
    //! location declared by the "location" key
    std::string location;
    //! offset declared by the "offset" key; 0 when absent
    int64_t offset = 0;
    //! length declared by the "length" key; -1 when absent (to end of file)
    int64_t length = -1;
    //! whether the tensor can be supplied to ONNX Runtime as an in-memory initializer file
    /** Only the top-level graph's initializers can be bound this way; external data declared in a
        subgraph, a local function body or a node attribute is still resolved by ONNX Runtime from
        the model's directory, so its bytes cannot be attributed to the loaded session.
    */
    bool bindable = false;
};

//! Immutable content identity captured while the ONNX Runtime session was created
/** The identity binds the actual model graph bytes loaded into the session and every
    external tensor byte the session consumed. It never contains raw model content,
    local filesystem paths or credentials, and is not affected by content-preserving
    relocation or by timestamp changes.
*/
class OnnxContentIdentity {
public:
    //! Marks the identity as not determinable, with a caller-safe reason
    DLLLOCAL void setUnknown(const std::string& reason);

    //! Discards all captured content and marks the identity unknown
    /** Used when the content cannot be bound authoritatively; the model then loads exactly as it
        would without capture, and no partial or unattributable digest is reported.
    */
    DLLLOCAL void discardCapture(const std::string& reason);

    //! Returns true if the identity completely describes the loaded session content
    DLLLOCAL bool isComplete() const {
        return complete;
    }

    //! Returns true if authoritative content is held for this session
    DLLLOCAL bool isCaptured() const {
        return captured;
    }

    //! Returns true if a capture was attempted, so the reported identity is meaningful
    DLLLOCAL bool wasAttempted() const {
        return attempted;
    }

    //! Computes the manifest and its digest from the captured content
    DLLLOCAL void finalize();

    //! Returns the identity as an OnnxContentIdentity hash
    DLLLOCAL QoreHashNode* toHash(ExceptionSink* xsink) const;

    //! Returns the ONNX Runtime external initializer file names (declared locations)
    DLLLOCAL const std::vector<OnnxExternalFileCapture>& getFiles() const {
        return files;
    }

    //! Returns the captured model bytes
    DLLLOCAL const std::vector<char>& getModelBytes() const {
        return model_bytes;
    }

    //! True when the capture holds authoritative model bytes to load from
    DLLLOCAL bool hasModelBytes() const {
        return captured && !model_bytes.empty();
    }

    //! Returns the manifest digest, or an empty string when the identity is unknown
    DLLLOCAL const std::string& getDigest() const {
        return digest;
    }

    //! capture source: "file" or "memory"
    std::string source;
    //! whether authoritative content is held for this session
    bool captured = false;
    //! whether a capture was attempted; set once and never cleared
    bool attempted = false;
    //! whether the captured content completely identifies the loaded session
    bool complete = false;
    //! caller-safe reason why the identity is not complete
    std::string unknown_reason;
    //! authoritative model bytes loaded into the session
    std::vector<char> model_bytes;
    //! hex SHA-256 digest of \a model_bytes
    std::string model_digest;
    //! external data files captured in full
    std::vector<OnnxExternalFileCapture> files;
    //! distinct external byte ranges consumed by the model
    std::vector<OnnxExternalRangeCapture> ranges;

private:
    //! hex SHA-256 digest over the identity manifest
    std::string digest;
};

//! Parses OnnxSessionConfig::content_identity
/** @param config the session configuration hash (may be nullptr)
    @param mode set to the parsed mode
    @param xsink exception sink

    @return false if an exception was raised
*/
DLLLOCAL bool parseOnnxContentIdentityMode(const QoreHashNode* config, OnnxContentIdentityMode& mode,
    ExceptionSink* xsink);

//! Scans ONNX model bytes for external tensor data references
/** Performs a bounded, cancellable wire-format scan of the ModelProto covering the main graph,
    nested subgraphs, local functions and sparse tensors.

    @param data the model bytes
    @param len the length of \a data in bytes
    @param refs external tensor references found in the model
    @param unsupported set to a caller-safe reason when the model declares a dependency class
        that cannot be enumerated (ex: non-embedded execution provider context binaries)
    @param xsink exception sink

    @return false if the model could not be parsed or the operation was cancelled
*/
DLLLOCAL bool scanOnnxExternalTensorRefs(const char* data, size_t len,
    std::vector<OnnxExternalTensorRef>& refs, std::string& unsupported, ExceptionSink* xsink);

//! Captures content identity for a model loaded from a filesystem path
/** @param model_path the model file path
    @param mode the capture mode; must not be OnnxContentIdentityMode::Disabled
    @param identity the identity to populate
    @param xsink exception sink

    @return false if an exception was raised
*/
DLLLOCAL bool captureOnnxContentIdentityFromPath(const char* model_path, OnnxContentIdentityMode mode,
    OnnxContentIdentity& identity, ExceptionSink* xsink);

//! Captures content identity for a model loaded from memory
/** External data locations are resolved against the current working directory, matching
    ONNX Runtime's own resolution for in-memory models.

    @param data the model bytes
    @param len the length of \a data in bytes
    @param mode the capture mode; must not be OnnxContentIdentityMode::Disabled
    @param identity the identity to populate
    @param xsink exception sink

    @return false if an exception was raised
*/
DLLLOCAL bool captureOnnxContentIdentityFromMemory(const void* data, size_t len,
    OnnxContentIdentityMode mode, OnnxContentIdentity& identity, ExceptionSink* xsink);

//! Returns an OnnxContentIdentity hash reporting that no identity was captured
DLLLOCAL QoreHashNode* makeUnknownOnnxContentIdentity(const char* source, const char* reason,
    ExceptionSink* xsink);

#endif
