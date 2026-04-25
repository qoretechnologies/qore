/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Scanner.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QC_SCANNER_H
#define _QORE_QC_SCANNER_H

#include <qore/AbstractPrivateData.h>

DLLEXPORT extern QoreClass* QC_SCANNER;
DLLEXPORT extern qore_classid_t CID_SCANNER;

//! Stateful scanner over a string for tokenizer / parser implementations.
/** Holds a reference to a source string plus a byte cursor, line, and column
    counter.  Provides constant-time byte access and amortised constant-time
    codepoint access (subject to the underlying string's encoding).

    Designed to replace the per-char @c input[pos] pattern used in
    @c Qdx::QoreTokenizer, @c OpenApi3SchemaTokenizer,
    @c AsyncApiSchemaTokenizer, and @c DpqlTokenizer.  The per-char pattern
    allocates a 1-character QoreStringNode on every access and walks UTF-8
    boundaries from byte 0 each call, producing O(N^2) cost on a tokenizer
    scan.  Scanner does both in O(1) per call.

    The class is thread-bound (created on one thread, must be used from the
    same thread).  Use ::reset() to rewind the cursor without rebuilding.
*/
class QoreScanner : public AbstractPrivateData {
public:
    DLLLOCAL QoreScanner(const QoreStringNode* s);
    DLLLOCAL QoreScanner(const QoreScanner& old);

    //! Returns the raw byte at the current position, or -1 at end of input.
    DLLLOCAL int peekByte() const {
        return cursor < src_size ? (unsigned char)src_buf[cursor] : -1;
    }

    //! Returns the raw byte at the given offset from the current position, or -1 if out of range.
    DLLLOCAL int peekByteAt(int byte_offset) const {
        size_t p = cursor + byte_offset;
        if (byte_offset < 0 && (size_t)(-byte_offset) > cursor) {
            return -1;
        }
        return p < src_size ? (unsigned char)src_buf[p] : -1;
    }

    //! Returns the codepoint at the current position, or -1 at end of input.
    /** May surface an encoding error via xsink for malformed input. */
    DLLLOCAL int peek(ExceptionSink* xsink) const;

    //! Returns the codepoint at the given character offset from the current
    //! position, or -1 if out of range.
    /** Walks codepoints — O(|offset|) per call.  Negative offsets walk
        backwards from the cursor; positive offsets walk forward.
        For ASCII-anchored tokenizers, prefer @ref peekByteAt() (constant time).
     */
    DLLLOCAL int peekAt(int char_offset, ExceptionSink* xsink) const;

    //! Advances by one codepoint.  Returns true if advanced, false if at end.
    /** Updates line and column counters. */
    DLLLOCAL bool advance(ExceptionSink* xsink);

    //! Advances by N codepoints (N may be 0 — no-op).  Returns the count actually advanced.
    DLLLOCAL int advanceN(int n, ExceptionSink* xsink);

    //! Advances by one byte (no codepoint decoding).  Returns true if advanced.
    /** Updates the column counter; updates the line counter if the advanced
        byte is '\n'.  Caller is responsible for not splitting a multi-byte
        codepoint.  Use only for ASCII-anchored advances after peekByte().
     */
    DLLLOCAL bool advanceByte();

    //! True iff cursor is at end-of-input.
    DLLLOCAL bool atEnd() const { return cursor >= src_size; }

    //! Returns the byte offset of the cursor.
    DLLLOCAL int64 getBytePos() const { return (int64)cursor; }

    //! Current line number (1-based).
    DLLLOCAL int64 getLine() const { return line; }

    //! Current column number (1-based, in codepoints — line/column reset on newline).
    DLLLOCAL int64 getColumn() const { return column; }

    //! Returns the number of bytes remaining from the cursor to end-of-input.
    DLLLOCAL int64 bytesRemaining() const {
        return cursor < src_size ? (int64)(src_size - cursor) : 0;
    }

    //! Returns true if the bytes starting at cursor match the given literal.
    /** Does not advance.  Compares raw bytes — for non-ASCII literals the
        caller's literal must already be in the same encoding as the source
        string. */
    DLLLOCAL bool startsWith(const char* lit, size_t lit_len) const {
        if (lit_len == 0) {
            return true;
        }
        if (cursor + lit_len > src_size) {
            return false;
        }
        return memcmp(src_buf + cursor, lit, lit_len) == 0;
    }

    //! If startsWith(lit) is true, advance past it and return true; otherwise return false.
    DLLLOCAL bool match(const char* lit, size_t lit_len);

    //! Returns a fresh QoreStringNode containing the bytes at [start_byte, start_byte+len_bytes).
    /** Returns nullptr if the range is out of bounds. */
    DLLLOCAL QoreStringNode* extractRange(size_t start_byte, size_t len_bytes) const;

    //! Resets the cursor to the start; line and column are reset to (1, 1).
    DLLLOCAL void reset();

    //! Returns a fresh strong reference to the source string.
    DLLLOCAL QoreStringNode* getStringRefSelf() const;

protected:
    DLLLOCAL virtual ~QoreScanner();

private:
    // Source string; ref'd in ctor, deref'd in dtor.
    QoreStringNode* src;
    // Cached pointer + size for fast access.  src_buf is owned by src and
    // is stable for the lifetime of src (QoreStringNode buffer never moves
    // unless mutated; src has refcount >= 1 while we hold it).
    const char* src_buf;
    size_t src_size;
    // Byte cursor.
    size_t cursor = 0;
    // 1-based line and column.
    int64 line = 1;
    int64 column = 1;
    // Thread that constructed the scanner.
    int tid;
};

#endif
