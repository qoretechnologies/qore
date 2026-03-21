/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_serialization.cpp

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

#include "ml_serialization.h"

#include <cstring>
#include <map>

namespace MLSerialization {

// --- Algorithm registry ---

static std::map<std::string, DeserializeFunc>& getRegistry() {
    static std::map<std::string, DeserializeFunc> registry;
    return registry;
}

void registerAlgorithm(const std::string& name, DeserializeFunc func) {
    getRegistry()[name] = std::move(func);
}

DeserializeFunc getDeserializer(const std::string& name) {
    auto& reg = getRegistry();
    auto it = reg.find(name);
    if (it != reg.end()) {
        return it->second;
    }
    return nullptr;
}

// --- Helper: check remaining bytes ---

static bool checkRemaining(size_t remaining, size_t needed, ExceptionSink* xsink) {
    if (remaining < needed) {
        xsink->raiseException("ML-DESERIALIZE-ERROR",
            "unexpected end of data: need %zu bytes but only %zu remain",
            needed, remaining);
        return false;
    }
    return true;
}

// --- Write primitives ---

void writeScalar(std::vector<uint8_t>& buf, double val) {
    size_t pos = buf.size();
    buf.resize(pos + sizeof(double));
    std::memcpy(buf.data() + pos, &val, sizeof(double));
}

void writeInt32(std::vector<uint8_t>& buf, int32_t val) {
    size_t pos = buf.size();
    buf.resize(pos + sizeof(int32_t));
    std::memcpy(buf.data() + pos, &val, sizeof(int32_t));
}

void writeInt64(std::vector<uint8_t>& buf, int64_t val) {
    size_t pos = buf.size();
    buf.resize(pos + sizeof(int64_t));
    std::memcpy(buf.data() + pos, &val, sizeof(int64_t));
}

void writeUint16(std::vector<uint8_t>& buf, uint16_t val) {
    size_t pos = buf.size();
    buf.resize(pos + sizeof(uint16_t));
    std::memcpy(buf.data() + pos, &val, sizeof(uint16_t));
}

void writeUint32(std::vector<uint8_t>& buf, uint32_t val) {
    size_t pos = buf.size();
    buf.resize(pos + sizeof(uint32_t));
    std::memcpy(buf.data() + pos, &val, sizeof(uint32_t));
}

void writeUint64(std::vector<uint8_t>& buf, uint64_t val) {
    size_t pos = buf.size();
    buf.resize(pos + sizeof(uint64_t));
    std::memcpy(buf.data() + pos, &val, sizeof(uint64_t));
}

void writeBool(std::vector<uint8_t>& buf, bool val) {
    buf.push_back(val ? 1 : 0);
}

void writeString(std::vector<uint8_t>& buf, const std::string& s) {
    writeUint32(buf, static_cast<uint32_t>(s.size()));
    if (!s.empty()) {
        size_t pos = buf.size();
        buf.resize(pos + s.size());
        std::memcpy(buf.data() + pos, s.data(), s.size());
    }
}

void writeStringVector(std::vector<uint8_t>& buf, const std::vector<std::string>& v) {
    writeUint32(buf, static_cast<uint32_t>(v.size()));
    for (const auto& s : v) {
        writeString(buf, s);
    }
}

void writeDoubleVector(std::vector<uint8_t>& buf, const std::vector<double>& v) {
    writeUint32(buf, static_cast<uint32_t>(v.size()));
    for (double d : v) {
        writeScalar(buf, d);
    }
}

void writeIntVector(std::vector<uint8_t>& buf, const std::vector<int>& v) {
    writeUint32(buf, static_cast<uint32_t>(v.size()));
    for (int i : v) {
        writeInt32(buf, i);
    }
}

void writeVector(std::vector<uint8_t>& buf, const VectorXd& vec) {
    writeInt32(buf, static_cast<int32_t>(vec.size()));
    for (Eigen::Index i = 0; i < vec.size(); ++i) {
        writeScalar(buf, vec(i));
    }
}

void writeMatrix(std::vector<uint8_t>& buf, const MatrixXd& mat) {
    writeInt32(buf, static_cast<int32_t>(mat.rows()));
    writeInt32(buf, static_cast<int32_t>(mat.cols()));
    for (Eigen::Index r = 0; r < mat.rows(); ++r) {
        for (Eigen::Index c = 0; c < mat.cols(); ++c) {
            writeScalar(buf, mat(r, c));
        }
    }
}

// --- Read primitives ---

double readScalar(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    if (!checkRemaining(remaining, sizeof(double), xsink)) {
        return 0.0;
    }
    double val;
    std::memcpy(&val, ptr, sizeof(double));
    ptr += sizeof(double);
    remaining -= sizeof(double);
    return val;
}

int32_t readInt32(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    if (!checkRemaining(remaining, sizeof(int32_t), xsink)) {
        return 0;
    }
    int32_t val;
    std::memcpy(&val, ptr, sizeof(int32_t));
    ptr += sizeof(int32_t);
    remaining -= sizeof(int32_t);
    return val;
}

int64_t readInt64(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    if (!checkRemaining(remaining, sizeof(int64_t), xsink)) {
        return 0;
    }
    int64_t val;
    std::memcpy(&val, ptr, sizeof(int64_t));
    ptr += sizeof(int64_t);
    remaining -= sizeof(int64_t);
    return val;
}

uint16_t readUint16(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    if (!checkRemaining(remaining, sizeof(uint16_t), xsink)) {
        return 0;
    }
    uint16_t val;
    std::memcpy(&val, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    remaining -= sizeof(uint16_t);
    return val;
}

uint32_t readUint32(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    if (!checkRemaining(remaining, sizeof(uint32_t), xsink)) {
        return 0;
    }
    uint32_t val;
    std::memcpy(&val, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);
    return val;
}

uint64_t readUint64(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    if (!checkRemaining(remaining, sizeof(uint64_t), xsink)) {
        return 0;
    }
    uint64_t val;
    std::memcpy(&val, ptr, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    remaining -= sizeof(uint64_t);
    return val;
}

bool readBool(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    if (!checkRemaining(remaining, 1, xsink)) {
        return false;
    }
    bool val = (*ptr != 0);
    ptr += 1;
    remaining -= 1;
    return val;
}

std::string readString(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    uint32_t len = readUint32(ptr, remaining, xsink);
    if (*xsink) {
        return {};
    }
    if (!checkRemaining(remaining, len, xsink)) {
        return {};
    }
    std::string result(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    remaining -= len;
    return result;
}

std::vector<std::string> readStringVector(const uint8_t*& ptr, size_t& remaining,
    ExceptionSink* xsink) {
    uint32_t count = readUint32(ptr, remaining, xsink);
    if (*xsink) {
        return {};
    }
    std::vector<std::string> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        result.push_back(readString(ptr, remaining, xsink));
        if (*xsink) {
            return {};
        }
    }
    return result;
}

std::vector<double> readDoubleVector(const uint8_t*& ptr, size_t& remaining,
    ExceptionSink* xsink) {
    uint32_t count = readUint32(ptr, remaining, xsink);
    if (*xsink) {
        return {};
    }
    std::vector<double> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        result.push_back(readScalar(ptr, remaining, xsink));
        if (*xsink) {
            return {};
        }
    }
    return result;
}

std::vector<int> readIntVector(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    uint32_t count = readUint32(ptr, remaining, xsink);
    if (*xsink) {
        return {};
    }
    std::vector<int> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        result.push_back(readInt32(ptr, remaining, xsink));
        if (*xsink) {
            return {};
        }
    }
    return result;
}

VectorXd readVector(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    int32_t size = readInt32(ptr, remaining, xsink);
    if (*xsink) {
        return VectorXd();
    }
    if (size < 0) {
        xsink->raiseException("ML-DESERIALIZE-ERROR",
            "invalid vector size: %d", size);
        return VectorXd();
    }
    VectorXd vec(size);
    for (int32_t i = 0; i < size; ++i) {
        vec(i) = readScalar(ptr, remaining, xsink);
        if (*xsink) {
            return VectorXd();
        }
    }
    return vec;
}

MatrixXd readMatrix(const uint8_t*& ptr, size_t& remaining, ExceptionSink* xsink) {
    int32_t rows = readInt32(ptr, remaining, xsink);
    if (*xsink) {
        return MatrixXd();
    }
    int32_t cols = readInt32(ptr, remaining, xsink);
    if (*xsink) {
        return MatrixXd();
    }
    if (rows < 0 || cols < 0) {
        xsink->raiseException("ML-DESERIALIZE-ERROR",
            "invalid matrix dimensions: %d x %d", rows, cols);
        return MatrixXd();
    }
    MatrixXd mat(rows, cols);
    for (int32_t r = 0; r < rows; ++r) {
        for (int32_t c = 0; c < cols; ++c) {
            mat(r, c) = readScalar(ptr, remaining, xsink);
            if (*xsink) {
                return MatrixXd();
            }
        }
    }
    return mat;
}

// --- CRC-32 ---

uint32_t computeCRC32(const uint8_t* data, size_t len) {
    // Standard CRC-32 with polynomial 0xEDB88320
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}

// --- High-level serialize/parse ---

BinaryNode* serializeModel(const std::string& algorithm,
    const std::vector<uint8_t>& model_data,
    const std::string& hyperparams_json,
    const std::vector<std::string>& field_names,
    ExceptionSink* xsink) {

    std::vector<uint8_t> buf;
    buf.reserve(model_data.size() + algorithm.size() + hyperparams_json.size() + 256);

    // Magic bytes
    buf.insert(buf.end(), MAGIC, MAGIC + 4);

    // Format version
    writeUint16(buf, FORMAT_VERSION);

    // Algorithm name
    writeString(buf, algorithm);

    // Hyperparameters JSON string
    writeString(buf, hyperparams_json);

    // Field names
    writeStringVector(buf, field_names);

    // Model data length + data
    writeUint64(buf, static_cast<uint64_t>(model_data.size()));
    buf.insert(buf.end(), model_data.begin(), model_data.end());

    // CRC-32 over everything before the CRC
    uint32_t crc = computeCRC32(buf.data(), buf.size());
    writeUint32(buf, crc);

    // Create BinaryNode
    void* raw = malloc(buf.size());
    if (!raw) {
        xsink->raiseException("ML-SERIALIZE-ERROR", "memory allocation failed");
        return nullptr;
    }
    std::memcpy(raw, buf.data(), buf.size());
    return new BinaryNode(raw, buf.size());
}

ModelHeader parseModel(const void* data, size_t len, ExceptionSink* xsink) {
    ModelHeader header;

    if (len < 4 + 2 + 4) { // magic + version + minimum CRC
        xsink->raiseException("ML-DESERIALIZE-ERROR",
            "data too small to be a valid .qml file (%zu bytes)", len);
        return header;
    }

    const uint8_t* raw = static_cast<const uint8_t*>(data);

    // Verify magic
    if (std::memcmp(raw, MAGIC, 4) != 0) {
        xsink->raiseException("ML-DESERIALIZE-ERROR",
            "invalid magic bytes: not a .qml file");
        return header;
    }

    // Verify CRC (last 4 bytes)
    if (len < 4) {
        xsink->raiseException("ML-DESERIALIZE-ERROR",
            "data too small to contain CRC");
        return header;
    }
    uint32_t stored_crc;
    std::memcpy(&stored_crc, raw + len - 4, sizeof(uint32_t));
    uint32_t computed_crc = computeCRC32(raw, len - 4);
    if (stored_crc != computed_crc) {
        xsink->raiseException("ML-DESERIALIZE-ERROR",
            "CRC mismatch: data is corrupted (stored=0x%08x computed=0x%08x)",
            stored_crc, computed_crc);
        return header;
    }

    // Parse fields
    const uint8_t* ptr = raw + 4; // skip magic
    size_t remaining = len - 4 - 4; // exclude magic and CRC

    // Format version
    uint16_t version = readUint16(ptr, remaining, xsink);
    if (*xsink) {
        return header;
    }
    if (version != FORMAT_VERSION) {
        xsink->raiseException("ML-DESERIALIZE-ERROR",
            "unsupported format version %u (expected %u)", version, FORMAT_VERSION);
        return header;
    }

    // Algorithm name
    header.algorithm = readString(ptr, remaining, xsink);
    if (*xsink) {
        return header;
    }

    // Hyperparameters JSON
    header.hyperparams_json = readString(ptr, remaining, xsink);
    if (*xsink) {
        return header;
    }

    // Field names
    header.field_names = readStringVector(ptr, remaining, xsink);
    if (*xsink) {
        return header;
    }

    // Model data
    uint64_t model_data_len = readUint64(ptr, remaining, xsink);
    if (*xsink) {
        return header;
    }
    if (remaining < model_data_len) {
        xsink->raiseException("ML-DESERIALIZE-ERROR",
            "model data length %llu exceeds remaining bytes %zu",
            static_cast<unsigned long long>(model_data_len), remaining);
        return header;
    }

    header.model_data = ptr;
    header.model_data_len = static_cast<size_t>(model_data_len);

    return header;
}

} // namespace MLSerialization
