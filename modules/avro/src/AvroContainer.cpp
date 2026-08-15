/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroContainer.cpp Avro object container file framing */
/*
    Qore avro module

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

#include "AvroContainer.h"

#include <qore/ReferenceHolder.h>

#include <openssl/rand.h>
#include <zlib.h>

#include <cstring>

//! the container file magic: "Obj" followed by the format version
static const unsigned char avro_magic[4] = {'O', 'b', 'j', 1};

//! the number of bytes read from the underlying source at a time
#define AVRO_READ_CHUNK 16384

// -----------------------------------------------------------------------------------------------
// sources

namespace {

class AvroBinarySource : public AvroSource {
public:
    DLLLOCAL AvroBinarySource(const BinaryNode* b) : b(b->binRefSelf()) {
    }

    DLLLOCAL virtual ~AvroBinarySource() {
        b->deref();
    }

    DLLLOCAL virtual int64 read(void* p, size_t len, ExceptionSink* xsink) override {
        size_t avail = b->size() - pos;
        if (!avail) {
            return 0;
        }
        if (len > avail) {
            len = avail;
        }
        memcpy(p, static_cast<const unsigned char*>(b->getPtr()) + pos, len);
        pos += len;
        return (int64)len;
    }

private:
    BinaryNode* b;
    size_t pos = 0;
};

class AvroFileSource : public AvroSource {
public:
    DLLLOCAL int open(const char* path, ExceptionSink* xsink) {
        return f.open2(xsink, path, O_RDONLY);
    }

    DLLLOCAL virtual int64 read(void* p, size_t len, ExceptionSink* xsink) override {
        size_t rc = f.read(p, len, -1, xsink);
        return *xsink ? -1 : (int64)rc;
    }

private:
    QoreFile f;
};

//! reads from a Qore InputStream; takes ownership of the reference passed to the constructor
class AvroStreamSource : public AvroSource {
public:
    DLLLOCAL AvroStreamSource(InputStream* is) : is(is) {
    }

    DLLLOCAL virtual ~AvroStreamSource() {
        is->deref();
    }

    DLLLOCAL virtual int64 read(void* p, size_t len, ExceptionSink* xsink) override {
        int64 rc = is->read(p, (int64)len, xsink);
        return *xsink ? -1 : rc;
    }

private:
    InputStream* is;
};

class AvroFileSink : public AvroSink {
public:
    DLLLOCAL int open(const char* path, ExceptionSink* xsink) {
        return f.open2(xsink, path, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    }

    DLLLOCAL virtual int write(const void* p, size_t len, ExceptionSink* xsink) override {
        return f.write(p, len, xsink) < 0 ? -1 : 0;
    }

    DLLLOCAL virtual int close(ExceptionSink* xsink) override {
        return f.close();
    }

private:
    QoreFile f;
};

//! writes to a Qore OutputStream; takes ownership of the reference passed to the constructor
class AvroStreamSink : public AvroSink {
public:
    DLLLOCAL AvroStreamSink(OutputStream* os) : os(os) {
    }

    DLLLOCAL virtual ~AvroStreamSink() {
        os->deref();
    }

    DLLLOCAL virtual int write(const void* p, size_t len, ExceptionSink* xsink) override {
        os->write(p, (int64)len, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL virtual int close(ExceptionSink* xsink) override {
        // the stream is owned by the caller, who decides when to close it
        return 0;
    }

private:
    OutputStream* os;
};

}

AvroSource* avro_source_from_binary(const BinaryNode* b) {
    return new AvroBinarySource(b);
}

AvroSource* avro_source_from_file(const char* path, ExceptionSink* xsink) {
    std::unique_ptr<AvroFileSource> src(new AvroFileSource);
    if (src->open(path, xsink)) {
        return nullptr;
    }
    return src.release();
}

AvroSource* avro_source_from_stream(InputStream* is) {
    return new AvroStreamSource(is);
}

AvroSink* avro_sink_from_file(const char* path, ExceptionSink* xsink) {
    std::unique_ptr<AvroFileSink> sink(new AvroFileSink);
    if (sink->open(path, xsink)) {
        return nullptr;
    }
    return sink.release();
}

AvroSink* avro_sink_from_stream(OutputStream* os) {
    return new AvroStreamSink(os);
}

// -----------------------------------------------------------------------------------------------
// buffered input

int64 AvroInput::fill(size_t need, ExceptionSink* xsink) {
    if (avail() >= need) {
        return (int64)avail();
    }
    // drop the consumed prefix before growing the buffer
    if (pos) {
        buf.erase(buf.begin(), buf.begin() + pos);
        pos = 0;
    }
    while (buf.size() < need && !src_at_end) {
        size_t chunk = need - buf.size();
        if (chunk < AVRO_READ_CHUNK) {
            chunk = AVRO_READ_CHUNK;
        }
        size_t old = buf.size();
        buf.resize(old + chunk);
        int64 rc = src->read(&buf[old], chunk, xsink);
        if (rc < 0) {
            buf.resize(old);
            return -1;
        }
        buf.resize(old + (size_t)rc);
        if (!rc) {
            src_at_end = true;
        }
    }
    return (int64)avail();
}

int AvroInput::readExact(void* dest, size_t n, ExceptionSink* xsink) {
    int64 have = fill(n, xsink);
    if (have < 0) {
        return -1;
    }
    if ((size_t)have < n) {
        xsink->raiseException("AVRO-FILE-ERROR", "truncated Avro container file: %zu more byte%s "
            "needed, but only " QLLD " remain", n, n == 1 ? " is" : "s are", have);
        return -1;
    }
    memcpy(dest, &buf[pos], n);
    pos += n;
    return 0;
}

int AvroInput::readLong(int64& v, ExceptionSink* xsink) {
    uint64_t result = 0;
    for (int shift = 0; shift <= 63; shift += 7) {
        int64 have = fill(1, xsink);
        if (have < 0) {
            return -1;
        }
        if (!have) {
            xsink->raiseException("AVRO-FILE-ERROR", "truncated Avro container file: a "
                "variable-length integer is cut short at the end of the input");
            return -1;
        }
        unsigned char b = buf[pos++];
        if (shift == 63 && (b & 0xfe)) {
            xsink->raiseException("AVRO-FILE-ERROR", "variable-length integer in the Avro "
                "container framing overflows 64 bits");
            return -1;
        }
        result |= (uint64_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) {
            v = (int64)((result >> 1) ^ (~(result & 1) + 1));
            return 0;
        }
    }
    xsink->raiseException("AVRO-FILE-ERROR", "variable-length integer in the Avro container "
        "framing overflows 64 bits");
    return -1;
}

int AvroInput::atEnd(ExceptionSink* xsink) {
    int64 have = fill(1, xsink);
    if (have < 0) {
        return -1;
    }
    return have ? 0 : 1;
}

// -----------------------------------------------------------------------------------------------
// codecs

//! compresses \a len bytes of \a in with raw DEFLATE (RFC 1951), as the Avro "deflate" codec uses
static int avro_raw_deflate(std::vector<unsigned char>& out, const void* in, size_t len,
        ExceptionSink* xsink) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    // windowBits -15 selects a raw deflate stream with no zlib header or trailer
    int rc = deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (rc != Z_OK) {
        xsink->raiseException("AVRO-CODEC-ERROR", "zlib deflateInit2() failed with code %d", rc);
        return -1;
    }
    ON_BLOCK_EXIT(deflateEnd, &zs);

    out.resize(deflateBound(&zs, (uLong)len) + 16);
    // zlib does not modify next_in, but its type is not const on all supported zlib versions
    zs.next_in = const_cast<Bytef*>(static_cast<const Bytef*>(in));
    zs.avail_in = (uInt)len;
    zs.next_out = out.empty() ? nullptr : &out[0];
    zs.avail_out = (uInt)out.size();
    rc = deflate(&zs, Z_FINISH);
    if (rc != Z_STREAM_END) {
        xsink->raiseException("AVRO-CODEC-ERROR", "zlib deflate() failed with code %d", rc);
        return -1;
    }
    out.resize(out.size() - zs.avail_out);
    return 0;
}

//! decompresses a raw DEFLATE stream
static int avro_raw_inflate(std::vector<unsigned char>& out, const void* in, size_t len,
        ExceptionSink* xsink) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    int rc = inflateInit2(&zs, -15);
    if (rc != Z_OK) {
        xsink->raiseException("AVRO-CODEC-ERROR", "zlib inflateInit2() failed with code %d", rc);
        return -1;
    }
    ON_BLOCK_EXIT(inflateEnd, &zs);

    zs.next_in = const_cast<Bytef*>(static_cast<const Bytef*>(in));
    zs.avail_in = (uInt)len;
    out.resize(len ? len * 4 + 64 : 64);
    size_t written = 0;
    while (true) {
        // a compressed block may expand by orders of magnitude, so the expansion loop is a
        // cancellation point as well as being bounded by AVRO_MAX_BLOCK_SIZE
        if (qore_check_cancel(xsink, "decompressing an Avro container file block")) {
            return -1;
        }
        zs.next_out = &out[written];
        zs.avail_out = (uInt)(out.size() - written);
        rc = inflate(&zs, Z_NO_FLUSH);
        written = out.size() - zs.avail_out;
        if (rc == Z_STREAM_END) {
            break;
        }
        if (rc != Z_OK && rc != Z_BUF_ERROR) {
            xsink->raiseException("AVRO-CODEC-ERROR", "zlib inflate() failed with code %d; the "
                "deflate-compressed block is corrupt", rc);
            return -1;
        }
        if (written == out.size()) {
            if (out.size() > (size_t)AVRO_MAX_BLOCK_SIZE) {
                xsink->raiseException("AVRO-CODEC-ERROR", "a deflate-compressed block expands to "
                    "more than the maximum of %d bytes", AVRO_MAX_BLOCK_SIZE);
                return -1;
            }
            out.resize(out.size() * 2);
            continue;
        }
        if (!zs.avail_in && rc == Z_BUF_ERROR) {
            xsink->raiseException("AVRO-CODEC-ERROR", "a deflate-compressed block ends before the "
                "compressed stream is complete");
            return -1;
        }
    }
    out.resize(written);
    return 0;
}

// -----------------------------------------------------------------------------------------------
// reader

QoreAvroFileReader::QoreAvroFileReader(AvroSource* src, ExceptionSink* xsink)
        : input(new AvroInput(src)), metadata(new QoreHashNode(binaryTypeInfo), xsink) {
    unsigned char magic[4];
    if (input->readExact(magic, 4, xsink)) {
        return;
    }
    if (memcmp(magic, avro_magic, 4)) {
        xsink->raiseException("AVRO-FILE-ERROR", "not an Avro object container file: the first "
            "four bytes are %02x %02x %02x %02x, not 'O' 'b' 'j' 0x01", magic[0], magic[1],
            magic[2], magic[3]);
        return;
    }

    // the header metadata is an Avro map<string, bytes>
    int64 count;
    int64 total = 0;
    while (true) {
        if (input->readLong(count, xsink)) {
            return;
        }
        if (!count) {
            break;
        }
        if (count < 0) {
            if (count == INT64_MIN) {
                xsink->raiseException("AVRO-FILE-ERROR", "header metadata block count overflows "
                    "when negated");
                return;
            }
            count = -count;
            int64 block_bytes;
            if (input->readLong(block_bytes, xsink)) {
                return;
            }
            if (block_bytes < 0) {
                xsink->raiseException("AVRO-FILE-ERROR", "negative header metadata block size "
                    QLLD, block_bytes);
                return;
            }
        }
        // check before accumulating: a hostile count near INT64_MAX would otherwise overflow
        if (count > AVRO_MAX_ZERO_WIDTH_ELEMENTS
            || total > AVRO_MAX_ZERO_WIDTH_ELEMENTS - count) {
            xsink->raiseException("AVRO-FILE-ERROR", "the container file header declares more "
                "than the maximum of %d metadata entries", AVRO_MAX_ZERO_WIDTH_ELEMENTS);
            return;
        }
        total += count;
        for (int64 i = 0; i < count; ++i) {
            if (qore_check_cancel(xsink, "reading an Avro container file header")) {
                return;
            }
            int64 klen;
            if (input->readLong(klen, xsink)) {
                return;
            }
            if (klen < 0 || klen > AVRO_MAX_BLOCK_SIZE) {
                xsink->raiseException("AVRO-FILE-ERROR", "invalid header metadata key length "
                    QLLD, klen);
                return;
            }
            QoreString key(QCS_UTF8);
            key.reserve((size_t)klen);
            if (klen) {
                std::vector<char> kbuf((size_t)klen);
                if (input->readExact(&kbuf[0], (size_t)klen, xsink)) {
                    return;
                }
                if (memchr(&kbuf[0], 0, (size_t)klen)) {
                    xsink->raiseException("AVRO-FILE-ERROR", "a container file header metadata "
                        "key contains an embedded NUL byte");
                    return;
                }
                key.concat(&kbuf[0], (size_t)klen);
            }
            int64 vlen;
            if (input->readLong(vlen, xsink)) {
                return;
            }
            if (vlen < 0 || vlen > AVRO_MAX_BLOCK_SIZE) {
                xsink->raiseException("AVRO-FILE-ERROR", "invalid header metadata value length "
                    QLLD " for key '%s'", vlen, key.c_str());
                return;
            }
            SimpleRefHolder<BinaryNode> val(new BinaryNode);
            if (vlen) {
                val->preallocate((size_t)vlen);
                if (input->readExact(const_cast<void*>(val->getPtr()), (size_t)vlen, xsink)) {
                    return;
                }
                val->setSize((size_t)vlen);
            }
            metadata->setKeyValue(key.c_str(), val.release(), xsink);
            if (*xsink) {
                return;
            }
        }
    }

    if (input->readExact(sync, AVRO_SYNC_SIZE, xsink)) {
        return;
    }

    QoreValue sv = metadata->getKeyValue("avro.schema");
    if (sv.getType() != NT_BINARY) {
        xsink->raiseException("AVRO-FILE-ERROR", "the container file header has no 'avro.schema' "
            "metadata entry");
        return;
    }
    const BinaryNode* sb = sv.get<const BinaryNode>();
    QoreString schema_json(static_cast<const char*>(sb->getPtr()), sb->size(), QCS_UTF8);
    data = AvroSchemaData::parseJson(schema_json, xsink);
    if (!data) {
        return;
    }
    root = data->getRoot();

    QoreValue cv = metadata->getKeyValue("avro.codec");
    if (cv.getType() == NT_BINARY) {
        const BinaryNode* cb = cv.get<const BinaryNode>();
        codec.assign(static_cast<const char*>(cb->getPtr()), cb->size());
    }
    if (codec.empty()) {
        codec = "null";
    }
    if (codec != "null" && codec != "deflate") {
        xsink->raiseException("AVRO-CODEC-ERROR", "the container file uses the '%s' codec, which "
            "this module does not implement; only 'null' and 'deflate' are supported",
            codec.c_str());
        return;
    }
}

QoreAvroFileReader::~QoreAvroFileReader() {
    value.discard(nullptr);
    if (data) {
        data->deref();
    }
}

QoreHashNode* QoreAvroFileReader::getMetadata() const {
    return metadata->hashRefSelf();
}

BinaryNode* QoreAvroFileReader::getSyncMarker() const {
    SimpleRefHolder<BinaryNode> b(new BinaryNode);
    b->append(sync, AVRO_SYNC_SIZE);
    return b.release();
}

int QoreAvroFileReader::readBlock(ExceptionSink* xsink) {
    int rc = input->atEnd(xsink);
    if (rc) {
        return rc < 0 ? -1 : 0;
    }

    int64 count;
    if (input->readLong(count, xsink)) {
        return -1;
    }
    if (count < 0) {
        xsink->raiseException("AVRO-FILE-ERROR", "negative object count " QLLD " in a container "
            "file block", count);
        return -1;
    }
    int64 nbytes;
    if (input->readLong(nbytes, xsink)) {
        return -1;
    }
    if (nbytes < 0 || nbytes > AVRO_MAX_BLOCK_SIZE) {
        xsink->raiseException("AVRO-FILE-ERROR", "container file block declares a size of " QLLD
            " bytes, which is negative or above the maximum of %d", nbytes, AVRO_MAX_BLOCK_SIZE);
        return -1;
    }

    std::vector<unsigned char> raw((size_t)nbytes);
    if (nbytes && input->readExact(&raw[0], (size_t)nbytes, xsink)) {
        return -1;
    }

    unsigned char file_sync[AVRO_SYNC_SIZE];
    if (input->readExact(file_sync, AVRO_SYNC_SIZE, xsink)) {
        return -1;
    }
    if (memcmp(file_sync, sync, AVRO_SYNC_SIZE)) {
        xsink->raiseException("AVRO-FILE-ERROR", "the sync marker following a container file "
            "block does not match the one in the header; the file is corrupt");
        return -1;
    }

    if (codec == "deflate") {
        if (avro_raw_inflate(block, nbytes ? &raw[0] : nullptr, (size_t)nbytes, xsink)) {
            return -1;
        }
    } else {
        block.swap(raw);
    }

    static const unsigned char empty_block = 0;
    decoder.reset(new AvroDecoder(block.empty() ? &empty_block : &block[0], block.size()));
    block_remaining = count;
    return 1;
}

bool QoreAvroFileReader::next(ExceptionSink* xsink) {
    if (check(xsink)) {
        return false;
    }
    value.discard(xsink);
    value = QoreValue();
    has_value = false;
    if (finished) {
        // a subsequent call restarts nothing: container files are read forwards only
        return false;
    }

    while (!block_remaining) {
        int rc = readBlock(xsink);
        if (rc < 0) {
            finished = true;
            return false;
        }
        if (!rc) {
            finished = true;
            return false;
        }
    }

    if (qore_check_cancel(xsink, "reading an Avro container file")) {
        finished = true;
        return false;
    }

    value = decoder->decode(root, xsink);
    if (*xsink) {
        value.discard(xsink);
        value = QoreValue();
        finished = true;
        return false;
    }
    --block_remaining;
    has_value = true;
    return true;
}

QoreValue QoreAvroFileReader::getValue(ExceptionSink* xsink) const {
    if (check(xsink)) {
        return QoreValue();
    }
    if (!has_value) {
        xsink->raiseException("INVALID-ITERATOR", "the AvroFileReader is not pointing at a valid "
            "element; call AvroFileReader::next() and check the return value before calling this "
            "method");
        return QoreValue();
    }
    return value.refSelf();
}

// -----------------------------------------------------------------------------------------------
// writer

QoreAvroFileWriter::QoreAvroFileWriter(AvroSink* sink, AvroSchemaData* data, const AvroNode* root,
        const QoreHashNode* opts, ExceptionSink* xsink) : sink(sink), data(data), root(root) {
    data->ref();
    writeHeader(opts, xsink);
}

QoreAvroFileWriter::~QoreAvroFileWriter() {
    if (data) {
        data->deref();
    }
}

BinaryNode* QoreAvroFileWriter::getSyncMarker() const {
    SimpleRefHolder<BinaryNode> b(new BinaryNode);
    b->append(sync, AVRO_SYNC_SIZE);
    return b.release();
}

int QoreAvroFileWriter::writeHeader(const QoreHashNode* opts, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> user_meta(xsink);

    if (opts) {
        QoreValue cv = opts->getKeyValue("codec");
        if (!cv.isNothing()) {
            if (cv.getType() != NT_STRING) {
                xsink->raiseException("AVRO-CODEC-ERROR", "the 'codec' option must be a string; "
                    "got type '%s'", cv.getTypeName());
                return -1;
            }
            // note: the codec name is held in inline short string storage ("null"), which has no
            // QoreStringNode; the helper must stay in scope for as long as "c" is used below
            QoreStringDataHelper codec_data(cv);
            const char* c = codec_data.c_str();
            if (!strcmp(c, "deflate")) {
                deflate_codec = true;
            } else if (strcmp(c, "null")) {
                xsink->raiseException("AVRO-CODEC-ERROR", "unsupported codec '%s'; this module "
                    "implements 'null' and 'deflate'", c);
                return -1;
            }
        }

        QoreValue bv = opts->getKeyValue("block_size");
        if (!bv.isNothing()) {
            if (bv.getType() != NT_INT) {
                xsink->raiseException("AVRO-FILE-ERROR", "the 'block_size' option must be an "
                    "integer; got type '%s'", bv.getTypeName());
                return -1;
            }
            int64 bs = bv.getAsBigInt();
            if (bs <= 0 || bs > AVRO_MAX_BLOCK_SIZE) {
                xsink->raiseException("AVRO-FILE-ERROR", "the 'block_size' option must be between "
                    "1 and %d; got " QLLD, AVRO_MAX_BLOCK_SIZE, bs);
                return -1;
            }
            block_size = (size_t)bs;
        }

        QoreValue sv = opts->getKeyValue("sync");
        if (!sv.isNothing()) {
            if (sv.getType() != NT_BINARY
                || sv.get<const BinaryNode>()->size() != AVRO_SYNC_SIZE) {
                xsink->raiseException("AVRO-FILE-ERROR", "the 'sync' option must be a binary "
                    "value of exactly %d bytes", AVRO_SYNC_SIZE);
                return -1;
            }
            memcpy(sync, sv.get<const BinaryNode>()->getPtr(), AVRO_SYNC_SIZE);
        } else if (RAND_bytes(sync, AVRO_SYNC_SIZE) != 1) {
            xsink->raiseException("AVRO-FILE-ERROR", "failed to generate a random sync marker");
            return -1;
        }

        QoreValue mv = opts->getKeyValue("metadata");
        if (!mv.isNothing()) {
            if (mv.getType() != NT_HASH) {
                xsink->raiseException("AVRO-FILE-ERROR", "the 'metadata' option must be a hash; "
                    "got type '%s'", mv.getTypeName());
                return -1;
            }
            user_meta = mv.get<const QoreHashNode>()->hashRefSelf();
        }
    } else if (RAND_bytes(sync, AVRO_SYNC_SIZE) != 1) {
        xsink->raiseException("AVRO-FILE-ERROR", "failed to generate a random sync marker");
        return -1;
    }

    const QoreJsonApi* json = avro_get_json_api(xsink);
    if (!json) {
        return -1;
    }

    // getSchemaValue() returns a new reference and generate() does not consume it
    ValueHolder schema_value(data->getSchemaValue(), xsink);
    SimpleRefHolder<QoreStringNode> schema_json(json->generate(*schema_value, JGF_NONE, QCS_UTF8,
        xsink));
    if (!schema_json) {
        return -1;
    }

    AvroEncoder hdr;
    hdr.writeRaw(avro_magic, 4);

    // build the metadata map: avro.schema, avro.codec, then any user entries
    int64 nmeta = 2 + (user_meta ? (int64)user_meta->size() : 0);
    hdr.writeLong(nmeta);
    hdr.writeByteSeq("avro.schema", 11);
    hdr.writeByteSeq(schema_json->c_str(), schema_json->size());
    hdr.writeByteSeq("avro.codec", 10);
    if (deflate_codec) {
        hdr.writeByteSeq("deflate", 7);
    } else {
        hdr.writeByteSeq("null", 4);
    }
    if (user_meta) {
        ConstHashIterator hi(*user_meta);
        while (hi.next()) {
            if (!strncmp(hi.getKey(), "avro.", 5)) {
                xsink->raiseException("AVRO-FILE-ERROR", "metadata key '%s' uses the 'avro.' "
                    "prefix, which the specification reserves", hi.getKey());
                return -1;
            }
            QoreString key(hi.getKey(), QCS_UTF8);
            hdr.writeByteSeq(key.c_str(), key.size());
            QoreValue v = hi.get();
            if (v.getType() == NT_BINARY) {
                const BinaryNode* b = v.get<const BinaryNode>();
                hdr.writeByteSeq(b->getPtr(), b->size());
            } else if (v.getType() == NT_STRING) {
                // note: the value can be held in inline short string storage, which has no
                // QoreStringNode; the node value helper materializes such values
                QoreStringNodeValueHelper str(v);
                TempEncodingHelper utf8(*str, QCS_UTF8, xsink);
                if (*xsink) {
                    return -1;
                }
                hdr.writeByteSeq(utf8->c_str(), utf8->size());
            } else {
                xsink->raiseException("AVRO-FILE-ERROR", "metadata value for key '%s' must be a "
                    "string or binary value; got type '%s'", hi.getKey(), v.getTypeName());
                return -1;
            }
        }
    }
    hdr.writeLong(0);
    hdr.writeRaw(sync, AVRO_SYNC_SIZE);

    return sink->write(&hdr.getBuffer()[0], hdr.size(), xsink);
}

int QoreAvroFileWriter::write(QoreValue v, ExceptionSink* xsink) {
    AutoLocker al(m);
    if (closed) {
        xsink->raiseException("AVRO-FILE-ERROR", "this AvroFileWriter object has already been "
            "closed");
        return -1;
    }
    if (block.encode(root, v, xsink)) {
        // the partially-encoded value would corrupt the block, so the writer is left unusable
        closed = true;
        return -1;
    }
    ++block_count;
    if (block.size() >= block_size) {
        return flushBlock(xsink);
    }
    return 0;
}

int QoreAvroFileWriter::flushBlock(ExceptionSink* xsink) {
    if (!block_count) {
        return 0;
    }
    const std::vector<unsigned char>* payload = &block.getBuffer();
    std::vector<unsigned char> compressed;
    if (deflate_codec) {
        if (avro_raw_deflate(compressed, block.size() ? &block.getBuffer()[0] : nullptr,
                block.size(), xsink)) {
            return -1;
        }
        payload = &compressed;
    }

    AvroEncoder hdr;
    hdr.writeLong(block_count);
    hdr.writeLong((int64)payload->size());
    if (sink->write(&hdr.getBuffer()[0], hdr.size(), xsink)) {
        return -1;
    }
    if (!payload->empty() && sink->write(&(*payload)[0], payload->size(), xsink)) {
        return -1;
    }
    if (sink->write(sync, AVRO_SYNC_SIZE, xsink)) {
        return -1;
    }

    block.clear();
    block_count = 0;
    return 0;
}

int QoreAvroFileWriter::flush(ExceptionSink* xsink) {
    AutoLocker al(m);
    if (closed) {
        xsink->raiseException("AVRO-FILE-ERROR", "this AvroFileWriter object has already been "
            "closed");
        return -1;
    }
    return flushBlock(xsink);
}

int QoreAvroFileWriter::close(ExceptionSink* xsink) {
    AutoLocker al(m);
    if (closed) {
        return 0;
    }
    closed = true;
    int rc = flushBlock(xsink);
    if (sink->close(xsink)) {
        rc = -1;
    }
    return rc;
}
