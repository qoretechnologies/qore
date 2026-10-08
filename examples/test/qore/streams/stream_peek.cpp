/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Checks that the native stream readers return every byte value (0 - 255) from peek and keep bytes 0x80 - 0xff in
 * null-terminated string reads; run by stream-peek-bytes.qtest
 */
#include <qore/Qore.h>
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/BinaryInputStream.h"
#include "qore/intern/StringInputStream.h"
#include "qore/intern/BufferedStreamReader.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

static void require(bool valid, const std::string& message) {
    if (!valid) {
        throw std::runtime_error(message);
    }
}

//! returns a binary with every byte value from 0 to 255
static BinaryNode* make_all_bytes() {
    BinaryNode* b = new BinaryNode;
    for (int i = 0; i < 256; ++i) {
        unsigned char c = static_cast<unsigned char>(i);
        b->append(&c, 1);
    }
    return b;
}

//! creates a reader of the given data: an unbuffered StreamReader if bufsize is 0, otherwise a BufferedStreamReader
static StreamReader* make_reader(InputStream* is, int64 bufsize, ExceptionSink* xsink) {
    if (!bufsize) {
        return new StreamReader(xsink, is, QCS_ISO_8859_1);
    }
    return new BufferedStreamReader(xsink, is, QCS_ISO_8859_1, bufsize);
}

//! checks peek on each byte of a stream with every byte value through the given reader
static void check_reader_peek(int64 bufsize) {
    std::string label = bufsize ? "BufferedStreamReader (buffer " + std::to_string(bufsize) + ")" : "StreamReader";
    ExceptionSink xsink;
    SimpleRefHolder<BinaryNode> data(make_all_bytes());
    ReferenceHolder<StreamReader> reader(make_reader(new BinaryInputStream(*data), bufsize, &xsink), &xsink);
    require(!xsink, label + ": constructor");
    for (int i = 0; i < 256; ++i) {
        int64 p = reader->peekCheck(&xsink);
        require(!xsink, label + ": peek raised an exception at byte " + std::to_string(i));
        require(p == i, label + ": peek returned " + std::to_string(p) + " for byte " + std::to_string(i));
        // peek does not consume the byte
        require(reader->peekCheck(&xsink) == i && !xsink, label + ": second peek at byte " + std::to_string(i));
        int64 v = reader->readu1(&xsink);
        require(!xsink && v == i, label + ": read returned " + std::to_string(v) + " for byte "
            + std::to_string(i));
    }
    // only the end of the stream is reported as the end of the stream
    require(reader->peekCheck(&xsink) == -1, label + ": peek at the end of the stream");
    require(xsink.isException(), label + ": no exception at the end of the stream");
    QoreValue err = xsink.getExceptionErr();
    require(err.getType() == NT_STRING
        && !strcmp(err.get<const QoreStringNode>()->c_str(), "END-OF-STREAM-ERROR"),
        label + ": wrong exception at the end of the stream");
    xsink.clear();
}

//! checks that null-terminated string reads keep bytes 0x80 - 0xff (line reads are checked by the Qore test)
static void check_reader_strings(int64 bufsize) {
    std::string label = bufsize ? "BufferedStreamReader (buffer " + std::to_string(bufsize) + ")" : "StreamReader";
    ExceptionSink xsink;
    // "\x80\xfe" + null + "\xfd\xff" + null
    const char text[] = "\x80\xfe\0\xfd\xff";
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(text, sizeof(text), QCS_ISO_8859_1));
    ReferenceHolder<StreamReader> reader(make_reader(new StringInputStream(*str), bufsize, &xsink), &xsink);
    require(!xsink, label + ": constructor");

    SimpleRefHolder<QoreStringNode> s(reader->readNullTerminatedString(&xsink));
    require(!xsink && s && s->size() == 2 && !memcmp(s->c_str(), "\x80\xfe", 2),
        label + ": null-terminated string with bytes 0x80 - 0xff");
    s = reader->readNullTerminatedString(&xsink);
    require(!xsink && s && s->size() == 2 && !memcmp(s->c_str(), "\xfd\xff", 2),
        label + ": second null-terminated string with bytes 0x80 - 0xff");
}

//! checks peek on the input streams directly
static void check_stream_peek() {
    ExceptionSink xsink;
    SimpleRefHolder<BinaryNode> data(make_all_bytes());
    ReferenceHolder<InputStream> bis(new BinaryInputStream(*data), &xsink);
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(static_cast<const char*>(data->getPtr()), data->size(),
        QCS_ISO_8859_1));
    ReferenceHolder<InputStream> sis(new StringInputStream(*str), &xsink);
    for (InputStream* is : {*bis, *sis}) {
        std::string label = is->getName();
        for (int i = 0; i < 256; ++i) {
            int64 p = is->peek(&xsink);
            require(!xsink && p == i, label + ": peek returned " + std::to_string(p) + " for byte "
                + std::to_string(i));
            unsigned char c;
            require(is->read(&c, 1, &xsink) == 1 && !xsink && c == i, label + ": read at byte "
                + std::to_string(i));
        }
        require(is->peek(&xsink) == -1 && !xsink, label + ": peek at the end of the stream");
    }
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        check_stream_peek();
        for (int64 bufsize : {0, 1, 3, 4096}) {
            check_reader_peek(bufsize);
            check_reader_strings(bufsize);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: stream peek\n");
    }
    return status;
}
