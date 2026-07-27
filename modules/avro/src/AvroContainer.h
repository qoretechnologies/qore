/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroContainer.h Avro object container file framing */
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

#ifndef _QORE_AVRO_AVROCONTAINER_H
#define _QORE_AVRO_AVROCONTAINER_H

#include "AvroDecoder.h"
#include "AvroEncoder.h"

#include <qore/InputStream.h>
#include <qore/OutputStream.h>
#include <qore/QoreFile.h>
#include <qore/QoreIteratorBase.h>

#include <memory>
#include <vector>

//! the default number of bytes a writer buffers before emitting a block
#define AVRO_DEFAULT_BLOCK_SIZE (64 * 1024)

//! the largest container-file block the reader will accept, in bytes
/** A block's byte size is read from the file before the block itself, so a corrupt or hostile
    file could otherwise ask the reader to allocate an arbitrary amount up front.
*/
#define AVRO_MAX_BLOCK_SIZE (256 * 1024 * 1024)

//! a byte source for the container-file reader
class AvroSource {
public:
    DLLLOCAL virtual ~AvroSource() {
    }

    //! reads up to \a len bytes; returns the number read, 0 at end of input, -1 on error
    DLLLOCAL virtual int64 read(void* p, size_t len, ExceptionSink* xsink) = 0;
};

//! a byte sink for the container-file writer
class AvroSink {
public:
    DLLLOCAL virtual ~AvroSink() {
    }

    //! writes \a len bytes; returns 0 for OK, -1 on error
    DLLLOCAL virtual int write(const void* p, size_t len, ExceptionSink* xsink) = 0;

    //! releases the underlying resource; returns 0 for OK, -1 on error
    DLLLOCAL virtual int close(ExceptionSink* xsink) = 0;
};

DLLLOCAL AvroSource* avro_source_from_binary(const BinaryNode* b);
DLLLOCAL AvroSource* avro_source_from_file(const char* path, ExceptionSink* xsink);
DLLLOCAL AvroSource* avro_source_from_stream(InputStream* is);

DLLLOCAL AvroSink* avro_sink_from_file(const char* path, ExceptionSink* xsink);
DLLLOCAL AvroSink* avro_sink_from_stream(OutputStream* os);

//! buffered reader over an AvroSource with the Avro primitive reads the container framing needs
class AvroInput {
public:
    DLLLOCAL AvroInput(AvroSource* src) : src(src) {
    }

    //! reads exactly \a n bytes; returns 0 for OK, -1 if the input ended early or on error
    DLLLOCAL int readExact(void* dest, size_t n, ExceptionSink* xsink);

    //! reads a zigzag varint-encoded integer; returns 0 for OK, -1 on error
    DLLLOCAL int readLong(int64& v, ExceptionSink* xsink);

    //! returns 1 at end of input, 0 if more data is available, -1 on error
    DLLLOCAL int atEnd(ExceptionSink* xsink);

private:
    std::unique_ptr<AvroSource> src;
    std::vector<unsigned char> buf;
    size_t pos = 0;
    bool src_at_end = false;

    //! makes at least \a need bytes available if possible; returns the number available, -1 on error
    DLLLOCAL int64 fill(size_t need, ExceptionSink* xsink);

    DLLLOCAL size_t avail() const {
        return buf.size() - pos;
    }
};

//! private data for the Qore AvroFileReader class
class QoreAvroFileReader : public QoreIteratorBase {
public:
    //! takes ownership of \a src and parses the container header
    DLLLOCAL QoreAvroFileReader(AvroSource* src, ExceptionSink* xsink);

    DLLLOCAL virtual const char* getName() const override {
        return "AvroFileReader";
    }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const override {
        return autoTypeInfo;
    }

    //! returns the writer's schema; the caller owns the reference returned
    DLLLOCAL AvroSchemaData* getSchemaData() const {
        data->ref();
        return data;
    }

    DLLLOCAL const AvroNode* getRoot() const {
        return root;
    }

    //! returns the file metadata as a hash of binary values; the caller owns the reference
    DLLLOCAL QoreHashNode* getMetadata() const;

    DLLLOCAL QoreStringNode* getCodec() const {
        return new QoreStringNode(codec.c_str(), QCS_UTF8);
    }

    DLLLOCAL BinaryNode* getSyncMarker() const;

    //! advances to the next object; returns true if a value is available
    DLLLOCAL bool next(ExceptionSink* xsink);

    DLLLOCAL QoreValue getValue(ExceptionSink* xsink) const;

    DLLLOCAL bool valid() const {
        return has_value;
    }

protected:
    DLLLOCAL virtual ~QoreAvroFileReader();

private:
    std::unique_ptr<AvroInput> input;
    AvroSchemaData* data = nullptr;
    const AvroNode* root = nullptr;
    ReferenceHolder<QoreHashNode> metadata;
    std::string codec;
    unsigned char sync[AVRO_SYNC_SIZE] = {};

    //! the decoded bytes of the current block
    std::vector<unsigned char> block;
    std::unique_ptr<AvroDecoder> decoder;
    int64 block_remaining = 0;
    bool has_value = false;
    bool finished = false;
    QoreValue value;

    //! reads the next block frame; returns 1 if a block was read, 0 at end of file, -1 on error
    DLLLOCAL int readBlock(ExceptionSink* xsink);
};

//! private data for the Qore AvroFileWriter class
class QoreAvroFileWriter : public AbstractPrivateData {
public:
    //! takes ownership of \a sink and writes the container header
    DLLLOCAL QoreAvroFileWriter(AvroSink* sink, AvroSchemaData* data, const AvroNode* root,
            const QoreHashNode* opts, ExceptionSink* xsink);

    DLLLOCAL int write(QoreValue v, ExceptionSink* xsink);
    DLLLOCAL int flush(ExceptionSink* xsink);
    DLLLOCAL int close(ExceptionSink* xsink);

    DLLLOCAL BinaryNode* getSyncMarker() const;

protected:
    DLLLOCAL virtual ~QoreAvroFileWriter();

private:
    mutable QoreThreadLock m;
    std::unique_ptr<AvroSink> sink;
    AvroSchemaData* data = nullptr;
    const AvroNode* root = nullptr;
    bool deflate_codec = false;
    size_t block_size = AVRO_DEFAULT_BLOCK_SIZE;
    unsigned char sync[AVRO_SYNC_SIZE] = {};
    bool closed = false;

    AvroEncoder block;
    int64 block_count = 0;

    DLLLOCAL int writeHeader(const QoreHashNode* opts, ExceptionSink* xsink);
    DLLLOCAL int flushBlock(ExceptionSink* xsink);
};

#endif // _QORE_AVRO_AVROCONTAINER_H
