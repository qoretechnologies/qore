/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    StreamReader.h

    Qore Programming Language

    Copyright (C) 2016 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_STREAMREADER_H
#define _QORE_STREAMREADER_H

#include <cstdint>

#include "qore/qore_bitopts.h"
#include "qore/InputStream.h"
#include "qore/intern/StringReaderHelper.h"
#include "qore/intern/qore_encoding_private.h"

DLLLOCAL extern qore_classid_t CID_STREAMREADER;
DLLLOCAL extern QoreClass* QC_STREAMREADER;

//! Private data for the Qore::StreamReader class.
class StreamReader : public AbstractPrivateData {
public:
    DLLLOCAL StreamReader(ExceptionSink* xsink, InputStream* is, const QoreEncoding* encoding = QCS_DEFAULT) :
            in(is, xsink), enc(encoding) {
    }

    virtual DLLLOCAL ~StreamReader() {
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        return enc;
    }

    DLLLOCAL InputStream* getInputStream() {
        return *in;
    }

    DLLLOCAL const InputStream* getInputStream() const {
        return *in;
    }

    DLLLOCAL void setEncoding(const QoreEncoding* n_enc) {
        enc = n_enc;
    }

    //! Read binary data from the stream.
    /** @param limit max amount of data to read; if equal to -1, all data will be read, if equal to 0, no data will be
        read
        @param xsink exception sink

        @return Qore binary read from the stream
    */
    DLLLOCAL BinaryNode* readBinary(int64 limit, ExceptionSink* xsink) {
        if (limit == 0)
            return 0;
        SimpleRefHolder<BinaryNode> b(new BinaryNode());
        char buffer[STREAMREADER_BUFFER_SIZE];
        if (limit == -1) {
            while (true) {
                int rc = readData(xsink, buffer, STREAMREADER_BUFFER_SIZE, false);
                if (*xsink)
                    return 0;
                if (rc == 0)
                    break;
                b->append(buffer, rc);
            }
        } else {
            while (limit > 0) {
                int rc = readData(xsink, buffer, QORE_MIN(limit, STREAMREADER_BUFFER_SIZE), false);
                if (*xsink)
                    return 0;
                if (rc == 0)
                    break;
                b->append(buffer, rc);
                limit -= rc;
            }
        }

        return b->empty() ? 0 : b.release();
    }

    //! Read string data from the stream.
    /** @param size max amount of data to read as a number of characters; if equal to -1, all data will be read, if
        equal to 0, no data will be read
        @param xsink exception sink

        @return Qore string read from the stream
    */
    DLLLOCAL QoreStringNode* readString(int64 size, ExceptionSink* xsink) {
        return q_read_string(xsink, size, enc, std::bind(&StreamReader::readData, this, _3, _1, _2, false));
    }

    //! Read one line
    /** @param eol end-of-line symbol, if missing and the character encoding is ASCII-compatible, then \c "\n",
        \c "\r", or \c "\r\n" are supported), otherwise if missing, and the character encoding is not
        ASCII-compatible, then \c "\n" is assumed
        @param trim whether to trim the EOL symbols
        @param xsink exception sink

        @return Qore string read from the stream
    */
    DLLLOCAL QoreStringNode* readLine(const QoreStringNode* eol, bool trim, ExceptionSink* xsink) {
        if (!eol && !enc->isAsciiCompat()) {
            QoreString nl("\n");
            return readLineEol(&nl, trim, xsink);
        }

        return eol ? readLineEol(eol, trim, xsink) : readLine(trim, xsink);
    }

    DLLLOCAL QoreStringNode* readLineEol(const QoreString* eol, bool trim, ExceptionSink* xsink) {
        // the byte order of a stream in the generic UTF-16 encoding (or a Unicode encoding created on the fly with
        // a byte order mark, ex: "UTF-32") is given by a byte order mark at its start; it must be known before the
        // end-of-line marker is converted to the encoding of the stream
        char start[4];
        size_t start_len = 0;
        if (enc == QCS_UTF16) {
            if (resolveUtf16ByteOrder(start, start_len, xsink)) {
                return nullptr;
            }
        } else if (!qore_encoding_private::get(*enc)->swapped_bom.empty()
                && resolveByteOrder(start, start_len, xsink)) {
            return nullptr;
        }

        TempEncodingHelper eolstr(eol, enc, xsink);
        if (*xsink) {
            return nullptr;
        }
        eolstr.removeBom();

        SimpleRefHolder<QoreStringNode> str(new QoreStringNode(enc));

        const size_t eolsize = eolstr->size();
        // the width of the smallest character; the end-of-line marker is a whole number of characters
        const size_t width = enc->getMinCharWidth();

        // adds a byte to the line; returns true if the line is complete
        auto add_byte = [&](char c) -> bool {
            str->concat(c);
            // the marker only ends the line at a character boundary: in an encoding with characters of more than one
            // byte (ex: UTF-32), the bytes of the marker can also be the end of one character and the start of the
            // next one; memcmp() is used, as the encoding can have null bytes in characters (ex: UTF-16*)
            size_t size = str->size();
            return eolsize && size >= eolsize && !(size % width)
                && !memcmp(str->c_str() + size - eolsize, eolstr->c_str(), eolsize);
        };

        // returns the complete line
        auto finish_line = [&]() -> QoreStringNode* {
            if (trim) {
                str->terminate(str->size() - eolsize);
            }
            return q_remove_bom_utf16(str.release(), enc);
        };

        // bytes read to resolve the byte order that are not a byte order mark belong to the line
        for (size_t i = 0; i < start_len; ++i) {
            if (add_byte(start[i])) {
                return finish_line();
            }
        }

        for (size_t i = 1; true; ++i) {
            // a line can be arbitrarily long, so the read can be cancelled
            if (!(i % 1024) && qore_check_cancel(xsink, "StreamReader line read")) {
                return nullptr;
            }
            char c;
            int64 rc = readData(xsink, &c, 1, false);
            //printd(5, "StreamReader::readLineEol() eol size: %zu rc: %d c: %d str: '%s' (%s)\n", eolsize, rc, c,
            //    str->c_str(), enc->getCode());
            if (*xsink)
                return 0;
            if (!rc)
                return str->empty() ? 0 : q_remove_bom_utf16(str.release(), enc);

            if (add_byte(c)) {
                return finish_line();
            }
        }
    }

    DLLLOCAL QoreStringNode* readNullTerminatedString(ExceptionSink* xsink) {
        SimpleRefHolder<QoreStringNode> str(new QoreStringNode(enc));

        while (true) {
            char c;
            int64 rc = readData(xsink, &c, 1, false);
            if (*xsink) {
                return nullptr;
            }
            if (!rc) { // End of stream
                xsink->raiseException("END-OF-STREAM-ERROR", "%d byte%s read of null-terminated string; end of "
                    "stream encountered without a null", (int)str->size(), str->size() == 1 ? "" : "s");
                return nullptr;
            }

            if (!c) {
                break;
            }
            str->concat(c);
        }
        return str.release();
    }

    DLLLOCAL QoreStringNode* readExactString(size_t size, ExceptionSink* xsink) {
        SimpleRefHolder<QoreStringNode> str(readString((int64)size, xsink));
        if (*xsink) {
            return nullptr;
        }
        if (str->size() < size) {
            xsink->raiseException("END-OF-STREAM-ERROR", QLLD " byte%s read of " QLLD "-byte string; end of stream "
                "encountered", str->size(), str->size() == 1 ? "" : "s", size);
            return nullptr;
        }
        assert(str->size() == size);
        return str.release();
    }

    DLLLOCAL QoreStringNode* readLine(bool trim, ExceptionSink* xsink) {
        SimpleRefHolder<QoreStringNode> str(new QoreStringNode(enc));

        while (true) {
            char c;
            int64 rc = readData(xsink, &c, 1, false);
            if (*xsink) {
                return nullptr;
            }
            if (!rc) { // End of stream.
                return str->empty() ? nullptr : str.release();
            }

            if (c == '\n') {
                if (!trim) {
                    str->concat(c);
                }
                return str.release();
            } else if (c == '\r') {
                if (!trim) {
                    str->concat(c);
                }
                int64 p = peek(xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (p == '\n') {
                    readData(xsink, &c, 1);
                    if (!trim) {
                        str->concat((char)p);
                    }
                }
                return str.release();
            }
            str->concat(c);
        }
    }

    DLLLOCAL int64 readi1(ExceptionSink* xsink) {
        signed char i = 0;
        if (readData(xsink, &i, 1) < 0) {
            return 0;
        }
        return i;
    }

    DLLLOCAL int64 readi2(ExceptionSink* xsink) {
        signed short i = 0;
        if (readData(xsink, &i, 2) < 0)
            return 0;
        i = ntohs(i);
        return i;
    }

    DLLLOCAL int64 readi4(ExceptionSink* xsink) {
        int32_t i = 0;
        if (readData(xsink, &i, 4) < 0)
            return 0;
        i = ntohl(i);
        return i;
    }

    DLLLOCAL int64 readi8(ExceptionSink* xsink) {
        int64 i = 0;
        if (readData(xsink, &i, 8) < 0)
            return 0;
        i = i8MSB(i);
        return i;
    }

    DLLLOCAL int64 readi2LSB(ExceptionSink* xsink) {
        signed short i = 0;
        if (readData(xsink, &i, 2) < 0)
            return 0;
        i = i2LSB(i);
        return i;
    }

    DLLLOCAL int64 readi4LSB(ExceptionSink* xsink) {
        int32_t i = 0;
        if (readData(xsink, &i, 4) < 0)
            return 0;
        i = i4LSB(i);
        return i;
    }

    DLLLOCAL int64 readi8LSB(ExceptionSink* xsink) {
        int64 i = 0;
        if (readData(xsink, &i, 8) < 0)
            return 0;
        i = i8LSB(i);
        return i;
    }

    DLLLOCAL int64 readu1(ExceptionSink* xsink) {
        unsigned char i = 0;
        if (readData(xsink, &i, 1) < 0) {
            return 0;
        }
        return i;
    }

    DLLLOCAL int64 readu2(ExceptionSink* xsink) {
        unsigned short i = 0;
        if (readData(xsink, &i, 2) < 0)
            return 0;
        i = ntohs(i);
        return i;
    }

    DLLLOCAL int64 readu4(ExceptionSink* xsink) {
        uint32_t i = 0;
        if (readData(xsink, &i, 4) < 0)
            return 0;
        i = ntohl(i);
        return i;
    }

    DLLLOCAL int64 readu2LSB(ExceptionSink* xsink) {
        unsigned short i = 0;
        if (readData(xsink, &i, 2) < 0)
            return 0;
        i = i2LSB(i);
        return i;
    }

    DLLLOCAL int64 readu4LSB(ExceptionSink* xsink) {
        uint32_t i = 0;
        if (readData(xsink, &i, 4) < 0)
            return 0;
        i = i4LSB(i);
        return i;
    }

    /**
        @brief Peeks the next byte from the input stream.
        @param xsink the exception sink
        @return the next byte available to be read, -1 indicates an error (end of stream is treated as an error)
    */
    int64 peekCheck(ExceptionSink* xsink) {
        int64 rc = peek(xsink);
        if (rc < 0) {
            if (!*xsink) {
                if (rc == -1) {
                    xsink->raiseException("END-OF-STREAM-ERROR", "there is not enough data available in the stream; "
                        "1 byte was requested, and 0 were read");
                } else {
                    assert(*xsink);
                }
            }
            return -1;
        }
        return rc;
    }

    //! Read data until a limit.
    /** @param xsink exception sink
        @param dest destination buffer
        @param limit maximum amount of data to read
        @param require_all if true then throw an exception if the required amount of data cannot be read from the
        stream

        @return amount of data read, -1 in case of error

        @since %Qore 0.9
    */
    DLLLOCAL virtual qore_offset_t read(ExceptionSink* xsink, void* dest, size_t limit, bool require_all = true) {
        return readData(xsink, dest, limit, require_all);
    }

    DLLLOCAL virtual const char* getName() const { return "StreamReader"; }

protected:
    // default buffer size (note that I/O is generally unbuffered in this class)
    static const int STREAMREADER_BUFFER_SIZE = 4096;

    //! Source input stream.
    ReferenceHolder<InputStream> in;

    //! Encoding of the source input stream.
    const QoreEncoding* enc;

private:
    //! Resolves the byte order of a stream in the generic UTF-16 encoding from a byte order mark
    /** If the next bytes are a UTF-16 byte order mark, they are consumed, and the encoding of the stream is set to
        UTF-16LE or UTF-16BE accordingly; otherwise the stream stays in the generic (big-endian) UTF-16 encoding, and
        the bytes read are returned to the caller as content.

        @param start receives the bytes read that are not a byte order mark
        @param start_len receives the number of bytes in \a start (0 - 2)
        @param xsink exception sink

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int resolveUtf16ByteOrder(char* start, size_t& start_len, ExceptionSink* xsink) {
        assert(enc == QCS_UTF16);
        start_len = 0;
        // the bytes are read rather than peeked, as a byte order mark has two bytes, and only one can be peeked
        int64 rc = readData(xsink, start, 1, false);
        if (*xsink) {
            return -1;
        }
        if (!rc) {
            return 0;
        }
        start_len = 1;
        unsigned char b0 = static_cast<unsigned char>(start[0]);
        if (b0 != 0xfe && b0 != 0xff) {
            return 0;
        }
        rc = readData(xsink, start + 1, 1, false);
        if (*xsink) {
            return -1;
        }
        if (!rc) {
            return 0;
        }
        start_len = 2;
        unsigned char b1 = static_cast<unsigned char>(start[1]);
        if (b0 == 0xfe && b1 == 0xff) {
            enc = QCS_UTF16BE;
            start_len = 0;
        } else if (b0 == 0xff && b1 == 0xfe) {
            enc = QCS_UTF16LE;
            start_len = 0;
        }
        return 0;
    }

    //! Resolves the byte order of a stream in a Unicode encoding created on the fly with a byte order mark
    /** As resolveUtf16ByteOrder(), for an encoding such as \c "UTF-32": if the next bytes are a byte order mark of
        the encoding in either byte order, they are consumed, and the encoding of the stream is set to the encoding
        of that byte order (ex: \c "UTF-32LE"); otherwise the bytes read are returned to the caller as content.

        @param start receives the bytes read that are not a byte order mark; must have space for four bytes
        @param start_len receives the number of bytes in \a start (0 - 4)
        @param xsink exception sink

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int resolveByteOrder(char* start, size_t& start_len, ExceptionSink* xsink) {
        const qore_encoding_private* ep = qore_encoding_private::get(*enc);
        assert(!ep->bom.empty() && !ep->swapped_bom.empty());
        assert(ep->bom.size() <= 4 && ep->swapped_bom.size() <= 4);
        start_len = 0;
        // returns true if the bytes read are the start of the given byte order mark
        auto is_prefix = [&](const std::string& bom) -> bool {
            return start_len <= bom.size() && !memcmp(start, bom.data(), start_len);
        };
        // the bytes are read one at a time while they can be the start of a byte order mark
        while (true) {
            int64 rc = readData(xsink, start + start_len, 1, false);
            if (*xsink) {
                return -1;
            }
            if (!rc) {
                return 0;
            }
            ++start_len;
            if (!is_prefix(ep->bom) && !is_prefix(ep->swapped_bom)) {
                return 0;
            }
            size_t bom_len;
            const QoreEncoding* bom_enc = ep->getBomEncoding(enc, start, start_len, bom_len);
            if (bom_enc) {
                assert(bom_len == start_len);
                enc = bom_enc;
                start_len = 0;
                return 0;
            }
        }
    }

    //! Read data until a limit.
    /** @param xsink exception sink
        @param dest destination buffer
        @param limit maximum amount of data to read
        @param require_all if true then throw an exception if the required amount of data cannot be read from the stream

        @return amount of data read, -1 in case of error
    */
    DLLLOCAL virtual qore_offset_t readData(ExceptionSink* xsink, void* dest, size_t limit, bool require_all = true) {
        assert(dest);
        assert(limit > 0);
        char* destPtr = static_cast<char*>(dest);
        size_t read = 0;
        while (true) {
            int64 rc = in->read(destPtr + read, limit - read, xsink);
            if (*xsink)
                return -1;
            //printd(5, "StreamReader::readData() dest: %p limit: " QLLD " read: " QLLD " rc: " QLLD " char: %d\n",
            //    dest, limit, read, rc, destPtr[0]);
            if (!rc) {
                if (require_all) {
                    xsink->raiseException("END-OF-STREAM-ERROR", "there is not enough data available in the stream; "
                        QSD " byte%s requested, but only " QSD " could be read", limit,
                        limit == 1 ? " was" : "s were", read);
                    return -1;
                }
                break;
            }
            read += rc;
            if (read == limit)
                break;
        }
        return read;
    }

    /**
        @brief Peeks the next byte from the input stream.
        @param xsink the exception sink
        @return the next byte available to be read, -1 indicates end of the stream, -2 indicates an error
    */
    virtual int64 peek(ExceptionSink* xsink) {
        return in->peek(xsink);
    }
};

#endif // _QORE_STREAMREADER_H
