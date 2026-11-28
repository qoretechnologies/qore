/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_serialization.h

    Qore ml module - model serialization infrastructure

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

#ifndef _QORE_MODULE_ML_SERIALIZATION_H
#define _QORE_MODULE_ML_SERIALIZATION_H

#include "ml_common.h"
#include <vector>
#include <cstdint>
#include <functional>
#include <string>
#include <map>
#include <initializer_list>

namespace MLSerialization {
    // Write primitives
    DLLLOCAL void writeMatrix(std::vector<uint8_t>& buf, const MatrixXd& mat);
    DLLLOCAL void writeVector(std::vector<uint8_t>& buf, const VectorXd& vec);
    DLLLOCAL void writeScalar(std::vector<uint8_t>& buf, double val);
    DLLLOCAL void writeInt32(std::vector<uint8_t>& buf, int32_t val);
    DLLLOCAL void writeInt64(std::vector<uint8_t>& buf, int64_t val);
    DLLLOCAL void writeUint16(std::vector<uint8_t>& buf, uint16_t val);
    DLLLOCAL void writeUint32(std::vector<uint8_t>& buf, uint32_t val);
    DLLLOCAL void writeUint64(std::vector<uint8_t>& buf, uint64_t val);
    DLLLOCAL void writeString(std::vector<uint8_t>& buf, const std::string& s);
    DLLLOCAL void writeStringVector(std::vector<uint8_t>& buf, const std::vector<std::string>& v);
    DLLLOCAL void writeBool(std::vector<uint8_t>& buf, bool val);
    DLLLOCAL void writeDoubleVector(std::vector<uint8_t>& buf, const std::vector<double>& v);
    DLLLOCAL void writeIntVector(std::vector<uint8_t>& buf, const std::vector<int>& v);

    // Read primitives (advance ptr, decrement remaining, raise exception on underflow)
    DLLLOCAL MatrixXd readMatrix(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);
    DLLLOCAL VectorXd readVector(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);
    DLLLOCAL double readScalar(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);
    DLLLOCAL int32_t readInt32(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);
    DLLLOCAL int64_t readInt64(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);
    DLLLOCAL uint16_t readUint16(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);
    DLLLOCAL uint32_t readUint32(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);
    DLLLOCAL uint64_t readUint64(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);
    DLLLOCAL std::string readString(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);
    DLLLOCAL std::vector<std::string> readStringVector(const uint8_t*& ptr, size_t& remaining,
        ExceptionSink* xsink);
    DLLLOCAL bool readBool(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink);
    DLLLOCAL std::vector<double> readDoubleVector(const uint8_t*& ptr, size_t& remaining,
        ExceptionSink* xsink);
    DLLLOCAL std::vector<int> readIntVector(const uint8_t*& ptr, size_t& remaining,
        ExceptionSink* xsink);

    // Validation of restored model state (qore#7844): each check raises ML-DESERIALIZE-ERROR with the model and the
    // field named and returns -1 when the value breaks an invariant that training establishes, else 0; a
    // deserializer checks every field it restores before it creates the model, so that a refused state leaves no
    // model
    DLLLOCAL int stateError(ExceptionSink* xsink, const char* model, const char* fmt, ...);
    //! the value must be in [lo, hi]
    DLLLOCAL int checkRange(int64_t v, int64_t lo, int64_t hi, const char* model, const char* field,
        ExceptionSink* xsink);
    //! the value must be in [lo, hi] and not NaN
    DLLLOCAL int checkValueRange(double v, double lo, double hi, const char* model, const char* field,
        ExceptionSink* xsink);
    //! the size must equal the expected size
    DLLLOCAL int checkSize(int64_t size, int64_t expected, const char* model, const char* what,
        ExceptionSink* xsink);
    //! the value must be one of the given strings
    DLLLOCAL int checkOneOf(const std::string& v, std::initializer_list<const char*> allowed, const char* model,
        const char* field, ExceptionSink* xsink);
    //! every value must be finite
    DLLLOCAL int checkFinite(const MatrixXd& m, const char* model, const char* what, ExceptionSink* xsink);
    DLLLOCAL int checkFinite(const VectorXd& v, const char* model, const char* what, ExceptionSink* xsink);
    DLLLOCAL int checkFinite(const std::vector<double>& v, const char* model, const char* what,
        ExceptionSink* xsink);
    DLLLOCAL int checkFinite(double v, const char* model, const char* what, ExceptionSink* xsink);
    //! the state must have been read to its end
    DLLLOCAL int checkEnd(size_t remaining, const char* model, ExceptionSink* xsink);

    // CRC-32
    DLLLOCAL uint32_t computeCRC32(const uint8_t* data, size_t len);

    // File format constants
    constexpr uint8_t MAGIC[4] = {'Q', 'M', 'L', 0x01};
    constexpr uint16_t FORMAT_VERSION = 1;

    // Algorithm type registry
    using DeserializeFunc = std::function<AbstractPrivateData*(const uint8_t*, size_t,
        ExceptionSink*)>;
    DLLLOCAL void registerAlgorithm(const std::string& name, DeserializeFunc func);
    DLLLOCAL DeserializeFunc getDeserializer(const std::string& name);

    // High-level save/load (native .qml format)
    DLLLOCAL BinaryNode* serializeModel(const std::string& algorithm,
        const std::vector<uint8_t>& model_data,
        const std::string& hyperparams_json,
        const std::vector<std::string>& field_names,
        ExceptionSink* xsink);

    // Parse the header and return algorithm name + model data
    struct ModelHeader {
        std::string algorithm;
        std::string hyperparams_json;
        std::vector<std::string> field_names;
        const uint8_t* model_data;
        size_t model_data_len;
    };
    DLLLOCAL ModelHeader parseModel(const void* data, size_t len, ExceptionSink* xsink);
}

#endif // _QORE_MODULE_ML_SERIALIZATION_H
