/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_string_private.h

    QoreString private implementation

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

#ifndef QORE_QORE_STRING_PRIVATE_H
#define QORE_QORE_STRING_PRIVATE_H

#include "qore/intern/QoreSandboxMemory.h"
#include "qore/intern/QoreAllocFailure.h"
#include "qore/intern/qore_encoding_private.h"

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

//! frees memory allocated with malloc() (a deleter of std::unique_ptr with no state)
struct QoreFreeDeleter {
    DLLLOCAL void operator()(void* p) const {
        free(p);
    }
};

class QoreRegexBase;
class QoreRegexSubst;
class QoreStringNode;

#define MAX_INT_STRING_LEN     48
#define MAX_BIGINT_STRING_LEN  48
#define MAX_FLOAT_STRING_LEN   48
#define STR_CLASS_BLOCK        (0x10 * 4)
#define STR_CLASS_EXTRA        (0x10 * 3)

#define MIN_SPRINTF_BUFSIZE   64

#define QUS_PATH     0
#define QUS_QUERY    1
#define QUS_FRAGMENT 2

typedef std::vector<int> intvec_t;

//! describes where characters start in the bytes of a string, for byte-oriented substring searches
/** A byte-oriented search can find a match that starts in the middle of a character in an encoding that is not
    self-synchronizing: in UTF-16, the low byte of one code unit and the high byte of the next can form another
    character, and in multi-byte encodings such as Shift_JIS, GBK, or Big5, the second byte of a double-byte character
    can be an ASCII character (ex: 0x5c, the backslash, in Shift_JIS "表").  UTF-8 and single-byte encodings cannot
    produce such a match.
*/
struct qore_char_boundary {
    //! the byte alignment of character boundaries; 1 if any byte offset can be a character boundary
    unsigned align;
    //! the character length function of a variable-width encoding whose characters can contain bytes that are
    //! other characters; a match is only taken at a character boundary found by walking the characters
    mbcs_charlen_t charlen;

    DLLLOCAL qore_char_boundary(unsigned align = 1, mbcs_charlen_t charlen = nullptr) : align(align),
            charlen(charlen) {
    }
};

#ifdef DEBUG
//! the number of bytes that the character offset functions of UTF-8 strings have walked or scanned in this thread
//! (debug builds)
/** For tests that check that a character offset in the ASCII text at the start of a string is O(1) (issue #6138); see
    dbg_get_string_scan_bytes(); counted per thread, so that the walks of other threads do not change a measurement
*/
DLLLOCAL extern thread_local int64 qore_dbg_string_scan_bytes;
//! disables the ASCII prefix cache of UTF-8 strings: every character offset is walked from the start (debug builds)
/** see dbg_set_ascii_prefix_cache()
*/
DLLLOCAL extern std::atomic<bool> qore_dbg_ascii_prefix_disabled;
//! adds to qore_dbg_string_scan_bytes
#define QORE_DBG_STRING_SCAN(n) (qore_dbg_string_scan_bytes += static_cast<int64>(n))
#else
#define QORE_DBG_STRING_SCAN(n)
#endif

struct qore_string_private {
public:
    size_t len = 0;
    //! the capacity of the owned buffer; in a view, the ASCII prefix cache (see asciiPrefixStorage())
    size_t allocated = 0;
private:
    char* buffer = nullptr;

public:
    const QoreEncoding* encoding = nullptr;

    // View support: when `view_parent` is non-null, this string is a read-only
    // view onto `view_parent`'s buffer starting at byte `view_offset` and
    // reaching the end of that buffer. The `len`
    // field is still authoritative for the view's byte length; `buffer` is
    // unused in the view state (nullptr), and `allocated` holds the ASCII prefix
    // cache instead of a capacity (see asciiPrefixStorage()). On any
    // mutation the view is materialised (bytes copied out, parent deref'd,
    // view_parent cleared) before writing.
    QoreStringNode* view_parent = nullptr;
    //! the offset of a view in its parent; in a string that owns its storage, the ASCII prefix cache
    size_t view_offset = 0;

    // Back-pointer to the embedding QoreStringNode, if any.  Populated by
    // QoreStringNode ctors; nullptr for plain QoreString instances (stack or
    // inside other refcounted types).  Used by QORE_ASSERT_MUTABLE to check
    // the CoW convention (ref>1 strings must not be mutated in place).
    const AbstractQoreNode* owning_node = nullptr;

    //! the sandbox memory charge of the buffer (see design/sandbox-resource-limits.md); synced with allocated
    QoreSandboxChargeRecord sb;

    //! Provides a writable buffer before accessing owned storage or changing its length/capacity metadata.
    /** Views materialize; empty storage left by clear()/giveBuffer() gains a terminator. */
    DLLLOCAL char*& mutableBuffer() {
        if (!buffer) {
            prepareMutableBuffer();
        }
        return buffer;
    }

    DLLLOCAL QORE_INTERN_NOINLINE void prepareMutableBuffer();

    //! Access after construction, materialization, check_char() or allocate() has established owned storage.
    /** This explicit precondition keeps the already-checked append paths free of a second view check. Read-only
        callers that can receive a view must use effective_buf(); other mutators must first use mutableBuffer().
        Keeping the raw pointer private makes an unchecked access outside the storage implementation a compile error.
    */
    DLLLOCAL char*& ownedBuffer() {
        assert(!view_parent);
        return buffer;
    }

    DLLLOCAL char* const& ownedBuffer() const {
        assert(!view_parent);
        return buffer;
    }

    // ASCII prefix cache (issue #6138)
    //
    // A character offset in UTF-8 is found by walking the characters before it, so indexing every character of a
    // string costs O(n^2).  The cache holds the byte length of a known ASCII prefix of the string, under the invariant
    // that it is not greater than len and that all bytes before it are < 0x80.  In UTF-8 such a byte is always a whole
    // and valid character, so a character offset in the prefix is its byte offset, and a walk can start at the end of
    // the prefix with the same result and the same validation of the bytes that follow.  The value depends only on the
    // bytes, not on the encoding: it is used only in UTF-8 (in other multi-byte encodings, such as Shift_JIS, a byte
    // < 0x80 can be the second byte of a character), but is kept, like the bytes, when the encoding changes.
    //
    // There is no field of its own, so the size of the private data is unchanged: an owned string keeps it in
    // view_offset, a view in allocated, the field that is not used in each state.  Readers extend it (an ASCII prefix
    // can only grow while the bytes do not change) and publish it with relaxed atomic stores, as a string that is
    // shared read-only between threads is read concurrently; all threads compute the same value from the same bytes.
    // A mutation only happens on a string that no other thread reads (see QORE_ASSERT_MUTABLE), and must lower or
    // reset the cache if it changes a byte in the prefix or makes the string shorter than it; appending bytes needs
    // nothing, so appends do no work for it.

    //! Returns the storage of the ASCII prefix cache: allocated in a view, view_offset otherwise
    DLLLOCAL size_t& asciiPrefixStorage() const {
        return const_cast<size_t&>(view_parent ? allocated : view_offset);
    }

    //! Returns the cached ASCII prefix length
    DLLLOCAL size_t getAsciiPrefix() const {
        size_t prefix = std::atomic_ref<size_t>(asciiPrefixStorage()).load(std::memory_order_relaxed);
        assert(prefix <= len);
        return prefix;
    }

    //! Resets the ASCII prefix cache of a string that owns its storage, for a mutation that can change any byte
    DLLLOCAL void resetOwnedAsciiPrefix() {
        assert(!view_parent);
        view_offset = 0;
    }

    //! Sets the ASCII prefix cache of a new string that owns its storage and holds only ASCII text
    /** For a string made from text known to be ASCII, such as a string literal, so that its first use does not scan
        it (qore#6625); no other thread can read the string yet
    */
    DLLLOCAL void setOwnedAsciiText() {
        assert(!view_parent);
#ifndef NDEBUG
        for (size_t i = 0; i < len; ++i) {
            assert(static_cast<unsigned char>(buffer[i]) < 0x80);
        }
#endif
        view_offset = len;
    }

    //! Lowers the ASCII prefix cache of a string that owns its storage to at most the given byte offset
    /** For a mutation that keeps the bytes before \a offset
    */
    DLLLOCAL void lowerOwnedAsciiPrefix(size_t offset) {
        assert(!view_parent);
        if (view_offset > offset) {
            view_offset = offset;
        }
    }

    //! Returns the length of an ASCII prefix of the string that is at least \a need bytes long unless the string has
    //! a byte >= 0x80 before it or is shorter
    /** @param b the bytes of the string: effective_buf(), or ownedBuffer() in a mutator

        The result is the first offset with a byte >= 0x80, the length of the string, or a value >= \a need
    */
    DLLLOCAL size_t getAsciiPrefix(const char* b, size_t need) const {
#ifdef DEBUG
        if (qore_dbg_ascii_prefix_disabled.load(std::memory_order_relaxed)) {
            return 0;
        }
#endif
        size_t prefix = getAsciiPrefix();
#ifndef NDEBUG
        checkAsciiPrefix(b, prefix);
#endif
        if (prefix >= need || prefix == len) {
            return prefix;
        }
        return extendAsciiPrefix(b, prefix, need);
    }

    //! Scans the bytes after the cached ASCII prefix and publishes the longer prefix found
    DLLLOCAL QORE_INTERN_NOINLINE size_t extendAsciiPrefix(const char* b, size_t prefix, size_t need) const;

#ifndef NDEBUG
    //! Asserts the invariant of the ASCII prefix cache
    /** The whole prefix is checked if it is short, its start and end otherwise, so that a debug build stays O(1) per
        character offset
    */
    DLLLOCAL void checkAsciiPrefix(const char* b, size_t prefix) const;
#endif

    //! Returns the number of bytes of \a chars characters from byte offset \a start
    /** As getEncoding()->getByteLen(b + start, b + len, chars, invalid); \a start must be the offset of a character

        @param b the bytes of the string: effective_buf(), or ownedBuffer() in a mutator
    */
    DLLLOCAL size_t getByteLenFrom(const char* b, size_t start, size_t chars, bool& invalid) const {
        if (getEncoding() != QCS_UTF8) {
            return getEncoding()->getByteLen(b + start, b + len, chars, invalid);
        }
        assert(start <= len);
        // the first byte that does not have to be scanned; the character count can exceed the byte count
        size_t need = chars < len - start ? start + chars : len;
        size_t prefix = getAsciiPrefix(b, need);
        if (start > prefix) {
            size_t rc = QCS_UTF8->getByteLen(b + start, b + len, chars, invalid);
            QORE_DBG_STRING_SCAN(rc);
            return rc;
        }
        // the characters from start to the end of the prefix are single bytes
        size_t ascii = prefix - start;
        if (chars <= ascii) {
            invalid = false;
            return chars;
        }
        size_t rc = QCS_UTF8->getByteLen(b + prefix, b + len, chars - ascii, invalid);
        QORE_DBG_STRING_SCAN(rc);
        return ascii + rc;
    }

    //! As getByteLenFrom(), raising an \c INVALID-ENCODING exception and returning 0 if the string is not valid
    DLLLOCAL size_t getByteLenFrom(const char* b, size_t start, size_t chars, ExceptionSink* xsink) const {
        if (getEncoding() != QCS_UTF8) {
            return getEncoding()->getByteLen(b + start, b + len, chars, xsink);
        }
        bool invalid;
        size_t rc = getByteLenFrom(b, start, chars, invalid);
        if (invalid) {
            raiseInvalidEncoding(xsink);
            return 0;
        }
        return rc;
    }

    //! Returns the number of characters from byte offset \a start to the end of the string
    /** As getEncoding()->getLength(b + start, b + len, invalid); \a start must be the offset of a character

        @param b the bytes of the string: effective_buf(), or ownedBuffer() in a mutator
    */
    DLLLOCAL size_t getLengthFrom(const char* b, size_t start, bool& invalid) const {
        if (getEncoding() != QCS_UTF8) {
            return getEncoding()->getLength(b + start, b + len, invalid);
        }
        assert(start <= len);
        size_t prefix = getAsciiPrefix(b, len);
        if (start > prefix) {
            QORE_DBG_STRING_SCAN(len - start);
            return QCS_UTF8->getLength(b + start, b + len, invalid);
        }
        if (prefix == len) {
            invalid = false;
            return len - start;
        }
        QORE_DBG_STRING_SCAN(len - prefix);
        return (prefix - start) + QCS_UTF8->getLength(b + prefix, b + len, invalid);
    }

    //! As getLengthFrom(), raising an \c INVALID-ENCODING exception and returning 0 if the string is not valid
    DLLLOCAL size_t getLengthFrom(const char* b, size_t start, ExceptionSink* xsink) const {
        if (getEncoding() != QCS_UTF8) {
            return getEncoding()->getLength(b + start, b + len, xsink);
        }
        bool invalid;
        size_t rc = getLengthFrom(b, start, invalid);
        if (invalid) {
            raiseInvalidEncoding(xsink);
            return 0;
        }
        return rc;
    }

    //! Returns the character offset of byte offset \a end
    /** As getEncoding()->getCharPos(b, b + end, xsink)

        @param b the bytes of the string: effective_buf(), or ownedBuffer() in a mutator
    */
    DLLLOCAL size_t getCharPosTo(const char* b, size_t end, ExceptionSink* xsink) const {
        if (getEncoding() != QCS_UTF8) {
            return getEncoding()->getCharPos(b, b + end, xsink);
        }
        assert(end <= len);
        size_t prefix = getAsciiPrefix(b, end);
        if (end <= prefix) {
            return end;
        }
        bool invalid;
        QORE_DBG_STRING_SCAN(end - prefix);
        size_t rc = QCS_UTF8->getCharPos(b + prefix, b + end, invalid);
        if (invalid) {
            raiseInvalidEncoding(xsink);
            return 0;
        }
        return prefix + rc;
    }

    //! Raises the exception that QoreEncoding raises for text that is not valid in the encoding of the string
    DLLLOCAL QORE_INTERN_COLD QORE_INTERN_NOINLINE void raiseInvalidEncoding(ExceptionSink* xsink) const;

    //! Counts characters for language operations, raising INVALID-ENCODING for malformed text
    DLLLOCAL static size_t characterCount(const QoreString& str, ExceptionSink* xsink);

    //! Returns the number of characters of a string in a multi-byte encoding, counting each byte that does not start
    //! a complete character as one character
    /** The count of a string that is not valid in its encoding, for QoreString::length(), which cannot raise an
        exception: it is not less than the number of characters of its valid text (qore#6597); in an encoding made
        of units of more than one byte (UTF-16), each such unit counts as one character

        @param b the bytes of the string: effective_buf()
    */
    DLLLOCAL QORE_INTERN_NOINLINE size_t getLengthCountingInvalidBytes(const char* b) const;

    //! Raises \c INVALID-ENCODING if the string is not valid in its encoding
    /** For an operation that gives a transformed copy of a string: one that walks its characters stops at the first
        byte sequence that is not valid in the encoding, and must not give a partial result instead (qore#6595).

        The validity is that of qore_string_encoding_valid(): strict (RFC 3629) in UTF-8, as for regular expressions.
        A string in a single-byte encoding is not checked, as each byte is a character.  A string in UTF-8 whose cached
        ASCII prefix covers it is valid without a scan, and the scan of another one starts at the end of its ASCII
        prefix.

        @param str the string to check
        @param func the name of the operation, for the exception's description
        @param what the string's role, for the exception's description (ex: \c "subject", \c "replacement")
        @param xsink for the exception

        @return 0 if the string is valid, -1 if an \c INVALID-ENCODING exception was raised
    */
    DLLLOCAL static int checkValidEncoding(const QoreString& str, const char* func, const char* what,
            ExceptionSink* xsink) {
        const qore_string_private* p = str.priv;
        if (p->getEncoding() == QCS_UTF8 ? p->getAsciiPrefix() == p->len
            : !p->getEncoding()->isMultiByte() && !p->isStateful()) {
            return 0;
        }
        return checkValidEncodingIntern(str, func, what, xsink);
    }

    //! Returns the number of characters of a string, raising \c INVALID-ENCODING if it is not valid in its encoding
    /** QoreString::length() cannot raise an exception, and stops counting at the first byte sequence that is not
        valid in the encoding; an operation that resolves character offsets against the length must not take that
        count for the length of the string, or it leaves out the characters after it (qore#6595)

        @param str the string
        @param op the name of the operation, for the exception's description
        @param xsink for the exception

        @return the number of characters, or -1 if an \c INVALID-ENCODING exception was raised
    */
    DLLLOCAL static int64 getCheckedLength(const QoreString& str, const char* op, ExceptionSink* xsink) {
        const qore_string_private* p = str.priv;
        if (p->getEncoding() == QCS_UTF8) {
            if (p->getAsciiPrefix() == p->len) {
                return static_cast<int64>(p->len);
            }
            return getCheckedUtf8Length(str, op, xsink);
        }
        if (checkValidEncoding(str, op, "subject", xsink)) {
            return -1;
        }
        return static_cast<int64>(str.length());
    }

    //! The UTF-8 part of getCheckedLength(): checks the string and counts its characters
    DLLLOCAL static QORE_INTERN_NOINLINE int64 getCheckedUtf8Length(const QoreString& str, const char* op,
            ExceptionSink* xsink);

    //! The out-of-line part of checkValidEncoding(): scans the string
    DLLLOCAL static QORE_INTERN_NOINLINE int checkValidEncodingIntern(const QoreString& str, const char* func,
            const char* what, ExceptionSink* xsink);

    DLLLOCAL qore_string_private() {
    }

    DLLLOCAL qore_string_private(const qore_string_private &p) {
        allocated = p.len + STR_CLASS_EXTRA;
        allocated = (allocated / 0x10 + 1) * 0x10; // use complete cache line
        // memory that cannot be allocated fails with std::bad_alloc (qore#7466)
        ownedBuffer() = static_cast<char*>(qore_malloc_or_throw(sizeof(char) * allocated));
        sb.sync(allocated);
        len = p.len;
        if (len)
            memcpy(ownedBuffer(), p.effective_buf(), len);
        ownedBuffer()[len] = '\0';
        encoding = p.getEncoding();
    }

    DLLLOCAL ~qore_string_private() {
        sb.release();
        if (buffer) {
            free(buffer);
        }
        if (view_parent) {
            dec_view_parent();
        }
    }

    // Returns the effective read-only buffer pointer: own buffer when not a
    // view, or parent's buffer + offset when a view. The hot non-view path
    // is inlined here; the view path is out-of-line below (uses the complete
    // type of QoreStringNode to reach its priv).
    DLLLOCAL const char* effective_buf() const {
        return view_parent ? effective_buf_view_path() : buffer;
    }

    // Out-of-line slow path for views: returns the parent's buffer + offset, terminated by the parent's own
    // terminator, as a view always reaches the end of its parent (QoreStringNode::makeView()); a read never copies,
    // so reads of a view shared between threads are safe (issue #6239).
    // Never inlined: the view path is cold, and a reader that inlines it needs a register for view_parent on its
    // hot path for strings that are not views as well.
    DLLLOCAL QORE_INTERN_NOINLINE const char* effective_buf_view_path() const;

    // Drops the strong reference held on view_parent. Out-of-line because
    // QoreStringNode::deref() is a complete-type call.
    DLLLOCAL void dec_view_parent();

    // Copy-on-write: if a view, copy the viewed bytes into a fresh owned
    // buffer and release the parent. No-op on a non-view. Must be called
    // before any in-place mutation of buffer/len/allocated.
    DLLLOCAL void materialize();

    //! Makes this empty string own a copy of \a n bytes at \a src, with the given known ASCII prefix
    /** Allocates as materialize() does: the exact size, without the growth of an append (issue #6239)
    */
    DLLLOCAL void adoptCopy(const char* src, size_t n, size_t ascii_prefix);

    // Drop the view without copying bytes. Used by mutations that fully
    // replace content (set/take/clear). After return: buffer=nullptr,
    // allocated=0, len=0, view_parent=nullptr. No-op on a non-view.
    DLLLOCAL void abandon_view();

    // Compute the byte range [byte_offset, byte_offset + byte_len) corresponding
    // to a substr(offset) or substr(offset, length) request, handling negative
    // offsets/lengths and both single-byte and multi-byte encodings.
    // Returns 0 on success, -1 if offset is out of range (and no substring
    // should be produced). For multi-byte encodings, xsink may be set on an
    // invalid encoding error; the caller must check.
    DLLLOCAL int compute_substr_range(qore_offset_t offset, size_t& byte_offset, size_t& byte_len,
            ExceptionSink* xsink) const;
    DLLLOCAL int compute_substr_range(qore_offset_t offset, qore_offset_t length, size_t& byte_offset,
            size_t& byte_len, ExceptionSink* xsink) const;

    //! returns the byte offset of \a p in the string's text, or -1 if it does not point into it
    /** The text can move when the string changes; a source that is part of the target's text (including a view) is
        located again at its offset after the buffer changes, or copied first
    */
    DLLLOCAL qore_offset_t ownDataOffset(const char* p) const {
        if (view_parent) {
            return ownDataOffsetViewPath(p);
        }
        // one unsigned comparison: a pointer before the buffer gives a difference larger than any length
        uintptr_t d = reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(buffer);
        return d < len ? static_cast<qore_offset_t>(d) : -1;
    }

    //! returns true if \a src is \a target or its text is part of the text of \a target
    DLLLOCAL static bool isOwnSource(const qore_string_private& target, const qore_string_private& src) {
        return &target == &src || (src.len && target.ownDataOffset(src.effective_buf()) >= 0);
    }

    //! appends a null-terminated source that is part of the string's text
    DLLLOCAL void concatOwnText(const char* str);

    //! the view path of ownDataOffset(): the text is in the parent's buffer
    DLLLOCAL qore_offset_t ownDataOffsetViewPath(const char* p) const;

    //! grows the string to hold \a i bytes (check_char()) and returns where \a src is then: in the moved text if it
    //! was part of it, else \a src (out of line: the slow path of the operations that append a source)
    DLLLOCAL const char* growKeepingSource(size_t i, const char* src);

    DLLLOCAL void check_char(size_t i) {
        if (view_parent) {
            materialize();
        }
        if (i >= allocated) {
            size_t d = i >> 2;
            allocated = i + (d < STR_CLASS_BLOCK ? STR_CLASS_BLOCK : d);
            allocated = (allocated / 0x10 + 1) * 0x10; // use complete cache line
            char* p = static_cast<char*>(qore_realloc_or_null(ownedBuffer(), allocated * sizeof(char)));
            if (__builtin_expect(!p, 0)) {
                // memory that cannot be allocated fails the operation and leaves the string's text unchanged: the
                // buffer was replaced with a null pointer, and the next write ended the process (qore#7466); the
                // string keeps its buffer, which holds at least its text and terminator (if it has one)
                allocated = ownedBuffer() ? len + 1 : 0;
                qore_throw_bad_alloc();
            }
            ownedBuffer() = p;
            sb.sync(allocated);
        }
    }

    //! grows the string to hold \a i bytes, as check_char() does; memory that cannot be allocated raises
    //! MEMORY-ALLOCATION-ERROR in \a xsink and returns -1, or, with no sink, fails with std::bad_alloc; the string is
    //! then unchanged (qore#7466)
    DLLLOCAL int check_char(size_t i, ExceptionSink* xsink) {
        if (view_parent) {
            materialize();
        }
        if (__builtin_expect(i >= allocated, 0)) {
            return growOrRaise(i, xsink);
        }
        return 0;
    }

    //! the slow path of check_char(size_t, ExceptionSink*) (qore#7466)
    DLLLOCAL int growOrRaise(size_t i, ExceptionSink* xsink) __attribute__((noinline, cold));

    //! grows the string to hold \a i bytes, as check_char() does, without ending the process if the memory cannot be
    //! allocated
    /** @return 0 for OK, -1 if the memory could not be allocated; the string is then unchanged
    */
    DLLLOCAL int tryGrow(size_t i) {
        if (view_parent) {
            materialize();
        }
        if (i < allocated) {
            return 0;
        }
        size_t d = i >> 2;
        size_t new_allocated = i + (d < STR_CLASS_BLOCK ? STR_CLASS_BLOCK : d);
        new_allocated = (new_allocated / 0x10 + 1) * 0x10; // use complete cache line
        char* nbuf = static_cast<char*>(realloc(ownedBuffer(), new_allocated));
        if (!nbuf) {
            return -1;
        }
        ownedBuffer() = nbuf;
        allocated = new_allocated;
        sb.sync(allocated);
        return 0;
    }

    DLLLOCAL size_t check_offset(qore_offset_t offset) {
        if (offset < 0) {
            offset = len + offset;
            return offset < 0 ? 0 : offset;
        }

        return ((size_t)offset > len) ? len : offset;
    }

    DLLLOCAL void check_offset(qore_offset_t offset, qore_offset_t num, size_t &n_offset, size_t &n_num) {
        n_offset = check_offset(offset);

        if (num < 0) {
            num = len + num - n_offset;
            if (num < 0)
                n_num = 0;
            else
                n_num = num;
            return;
        }
        n_num = num;
    }

    // NOTE: this is purely byte oriented - no character semantics here
    DLLLOCAL qore_offset_t find(char c, qore_offset_t pos = 0) {
        if (pos < 0) {
            pos = len + pos;
            if (pos < 0)
                pos = 0;
        } else if (pos > 0 && pos > (qore_offset_t)len)
            return -1;
        const char* b = effective_buf();
        const char* p;
        if (!(p = strchr(b + pos, c)))
            return -1;
        return (qore_offset_t)(p - b);
    }

    // NOTE: this is purely byte oriented - no character semantics here
    DLLLOCAL qore_offset_t rfind(char c, qore_offset_t pos = -1) {
        if (pos < 0) {
            pos = len + pos;
            if (pos < 0) {
                return -1;
            }
        } else if (pos >= (qore_offset_t)len) {
            // a position at or past the end is the last byte; the terminator is not part of the string
            if (!len) {
                return -1;
            }
            pos = len - 1;
        }

        const char* b = effective_buf();
        const char* p = b + pos;
        while (p >= b) {
            if (*p == c) {
                return (qore_offset_t)(p - b);
            }
            --p;
        }
        return -1;
    }

    // NOTE: this is purely byte oriented - no character semantics here
    // returns the first byte position at or after pos of any of the bytes in str, like rfindAny() searching forwards
    DLLLOCAL qore_offset_t findAny(const char* str, qore_offset_t pos = 0) {
        if (pos < 0) {
            pos = len + pos;
            if (pos < 0) {
                pos = 0;
            }
        } else if (pos >= (qore_offset_t)len) {
            return -1;
        }
        const char* b = effective_buf();
        // the search covers the string's bytes only (it can contain null bytes), not its terminator
        for (const char* p = b + pos, * e = b + len; p < e; ++p) {
            for (const char* t = str; *t; ++t) {
                if (*p == *t) {
                    return (qore_offset_t)(p - b);
                }
            }
        }
        return -1;
    }

    // NOTE: this is purely byte oriented - no character semantics here
    DLLLOCAL qore_offset_t rfindAny(const char* str, qore_offset_t pos = -1) {
        if (pos < 0) {
            pos = len + pos;
            if (pos < 0) {
                return -1;
            }
        } else if (pos >= (qore_offset_t)len) {
            // a position at or past the end is the last byte; the terminator is not part of the string
            if (!len) {
                return -1;
            }
            pos = len - 1;
        }

        const char* b = effective_buf();
        const char* p = b + pos;
        while (p >= b) {
            for (const char* t = str; *t; ++t) {
                if (*p == *t) {
                    return (qore_offset_t)(p - b);
                }
            }
            --p;
        }
        return -1;
    }

    //! returns the character boundaries of the given encoding for byte-oriented searches
    /** See qore_char_boundary.

        @param enc the encoding to check; may be nullptr for binary data

        @return the character boundaries; the default value means that any byte offset is a valid character boundary
     */
    DLLLOCAL static qore_char_boundary get_char_alignment(const QoreEncoding* enc) {
        if (!enc) {
            return qore_char_boundary();
        }
        const qore_encoding_private* ep = qore_encoding_private::get(*enc);
        if (ep->needs_boundary_check) {
            assert(ep->fcharlen);
            return qore_char_boundary(1, ep->fcharlen);
        }
        unsigned w = ep->getMinCharWidth();
        return qore_char_boundary(w > 1 ? w : 1);
    }

    //! returns the character boundaries of this string's encoding for byte-oriented searches
    DLLLOCAL qore_char_boundary getCharAlignment() const {
        return get_char_alignment(getEncoding());
    }

    //! finds the first occurrence of the needle in the haystack at a character boundary
    /** @param haystack the data to search
        @param hlen the length of \a haystack in bytes
        @param needle the data to find
        @param nlen the length of \a needle in bytes
        @param b the character boundaries; see get_char_alignment()
        @param base the byte offset of \a haystack within the string, used for the alignment check

        @return a pointer to the first match starting at a character boundary or nullptr if there is
        none

        @note \a haystack must start at a character boundary
     */
    DLLLOCAL static const char* memmem_aligned(const char* haystack, size_t hlen, const char* needle,
            size_t nlen, const qore_char_boundary& b = qore_char_boundary(), size_t base = 0) {
        if (b.charlen && nlen) {
            return memmem_boundary(haystack, hlen, needle, nlen, b.charlen);
        }
        const unsigned align = b.align;
        if (align < 2 || !nlen) {
            return static_cast<const char*>(q_memmem(haystack, hlen, needle, nlen));
        }
        const char* p = haystack;
        size_t left = hlen;
        while (left >= nlen) {
            const char* m = static_cast<const char*>(q_memmem(p, left, needle, nlen));
            if (!m) {
                return nullptr;
            }
            if (!((base + (m - haystack)) % align)) {
                return m;
            }
            // the match does not start at a character boundary; resume at the next byte
            left -= (m - p) + 1;
            p = m + 1;
        }
        return nullptr;
    }

    //! finds the last occurrence of the needle in the haystack at a character boundary
    /** @param haystack the data to search
        @param hlen the length of \a haystack in bytes
        @param needle the data to find
        @param nlen the length of \a needle in bytes
        @param b the character boundaries; see get_char_alignment()

        @return a pointer to the last match starting at a character boundary or nullptr if there is
        none

        @note \a haystack must start at a character boundary
     */
    DLLLOCAL static const char* memrmem_aligned(const char* haystack, size_t hlen, const char* needle,
            size_t nlen, const qore_char_boundary& b = qore_char_boundary()) {
        if (b.charlen && nlen) {
            // the characters can only be found from the start; the last match found searching forwards is taken
            const char* last = nullptr;
            const char* p = haystack;
            const char* end = haystack + hlen;
            while (const char* m = memmem_boundary(p, end - p, needle, nlen, b.charlen)) {
                last = m;
                qore_offset_t l = b.charlen(m, end - m);
                p = m + (l > 0 ? l : 1);
            }
            return last;
        }
        const unsigned align = b.align;
        if (align < 2 || !nlen) {
            return static_cast<const char*>(q_memrmem(haystack, hlen, needle, nlen));
        }
        size_t left = hlen;
        while (left >= nlen) {
            const char* m = static_cast<const char*>(q_memrmem(haystack, left, needle, nlen));
            if (!m) {
                return nullptr;
            }
            if (!((m - haystack) % align)) {
                return m;
            }
            // the match does not start at a character boundary; search again ending before it
            left = (m - haystack) + nlen - 1;
        }
        return nullptr;
    }

    //! finds the first occurrence of the needle that starts at a character boundary in a variable-width encoding
    /** @param haystack the data to search; must start at a character boundary
        @param hlen the length of \a haystack in bytes
        @param needle the data to find
        @param nlen the length of \a needle in bytes; must not be 0
        @param charlen the character length function of the encoding

        @return a pointer to the first match starting at a character boundary or nullptr if there is none
     */
    DLLLOCAL static const char* memmem_boundary(const char* haystack, size_t hlen, const char* needle, size_t nlen,
            mbcs_charlen_t charlen) {
        assert(nlen);
        const char* end = haystack + hlen;
        // the next character boundary
        const char* c = haystack;
        const char* p = haystack;
        while (static_cast<size_t>(end - p) >= nlen) {
            const char* m = static_cast<const char*>(q_memmem(p, end - p, needle, nlen));
            if (!m) {
                return nullptr;
            }
            while (c < m) {
                qore_offset_t l = charlen(c, end - c);
                // an invalid or incomplete character is skipped one byte at a time
                c += l > 0 ? l : 1;
            }
            if (c == m) {
                return m;
            }
            // the match starts inside a character; search again from the next character
            p = c;
        }
        return nullptr;
    }

    DLLLOCAL static qore_offset_t index_simple(const char* haystack, size_t hlen, const char* needle, size_t nlen,
        qore_offset_t pos = 0, const qore_char_boundary& align = qore_char_boundary(), size_t base = 0) {
        const char* start = haystack + pos;
        const char* ptr = memmem_aligned(start, hlen - pos, needle, nlen, align, base + pos);
        if (!ptr) {
            return -1;
        }
        return ptr - start + pos;
    }

    //! Returns the position of an empty needle when searching forwards from \a pos
    /** An empty string is found at every position from 0 to the end of the string inclusive, so the search
        position is returned as with any other match: a negative \a pos is an offset from the end of the string,
        and a position after the end of the string has no match

        @param pos the search position
        @param slen the length of the string in the units of \a pos (characters or bytes)

        @return the position of the match or -1 if there is none
    */
    DLLLOCAL static qore_offset_t emptyNeedleIndex(qore_offset_t pos, size_t slen) {
        if (pos < 0) {
            pos += slen;
            if (pos < 0) {
                pos = 0;
            }
        }
        return pos > (qore_offset_t)slen ? -1 : pos;
    }

    //! Returns the position of an empty needle when searching backwards from \a pos
    /** An empty string is found at every position from 0 to the end of the string inclusive; -1 means the end of
        the string, so the last match is the end of the string, and other negative positions are offsets before
        it; a position after the end of the string is the end of the string

        @param pos the search position
        @param slen the length of the string in the units of \a pos (characters or bytes)

        @return the position of the match or -1 if there is none
    */
    DLLLOCAL static qore_offset_t emptyNeedleRindex(qore_offset_t pos, size_t slen) {
        if (pos < 0) {
            pos += slen + 1;
            return pos < 0 ? -1 : pos;
        }
        return pos > (qore_offset_t)slen ? (qore_offset_t)slen : pos;
    }

    //! returns true if the string is in a stateful encoding
    /** A stateful encoding (ex: UTF-7, ISO-2022-JP; see qore_encoding_private::stateful) has no byte sequence per
        character: character operations on a string in such an encoding are made on its text in UTF-8 (see
        statefulToUtf8()), and text results are converted back to the encoding (see statefulFromUtf8())
    */
    DLLLOCAL bool isStateful() const {
        return qore_encoding_private::isStateful(getEncoding());
    }

    //! appends the text of the string, which is in a stateful encoding, in UTF-8 to the given UTF-8 string
    /** @return 0 for OK, -1 if an exception was raised (invalid text)
    */
    DLLLOCAL int statefulToUtf8(QoreString& utf8, ExceptionSink* xsink) const;

    //! appends the given UTF-8 text in the encoding of this string, which is stateful, to the target string
    /** @param utf8 the text in UTF-8
        @param targ the target, which must be in the encoding of this string

        @return 0 for OK, -1 if an exception was raised (a character that cannot be represented in the encoding)
    */
    DLLLOCAL int statefulFromUtf8(const QoreString& utf8, QoreString& targ, ExceptionSink* xsink) const;

    //! removes characters from the string, which is in a stateful encoding, and optionally inserts text in their place
    /** The operation is made on the text in UTF-8, and the string takes the result converted back to its encoding

        @param offset the character offset of the first character to remove
        @param num the number of characters to remove (as for splice_complex()); ignored if \a to_end is true
        @param to_end true to remove all characters from \a offset
        @param repl the text to insert, or nullptr
        @param extract if not nullptr, set to the characters removed, in the encoding of the string
        @param xsink for exceptions

        @return 0 for OK, -1 if an exception was raised; the string is not changed if an exception is raised
    */
    DLLLOCAL int statefulSplice(qore_offset_t offset, qore_offset_t num, bool to_end, const QoreString* repl,
            QoreString* extract, ExceptionSink* xsink);

    //! appends the characters of the given string in the same stateful encoding from a character offset
    /** @param str the source string, in the encoding of this string
        @param pos the character offset in \a str (negative: from the end)
        @param plen the number of characters (negative: up to that many characters from the end), or nullptr for all
        characters from \a pos
        @param xsink for exceptions

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int concatStateful(const qore_string_private& str, qore_offset_t pos, const qore_offset_t* plen,
            ExceptionSink* xsink);

    //! raises an exception for an operation on the byte position of a character in a string in a stateful encoding
    DLLLOCAL void raiseStatefulByteOffsetError(ExceptionSink* xsink) const;

    //! removes the bytes that iconv writes at the start of text in a stateful encoding from a short sequence
    /** For a sequence converted on its own that is searched for in text in the encoding (ex: an end-of-line marker);
        see qore_encoding_private::stateful_prefix
    */
    DLLLOCAL void removeStatefulPrefix() {
        if (!isStateful()) {
            return;
        }
        const std::string& prefix = qore_encoding_private::get(*getEncoding())->stateful_prefix;
        if (prefix.empty() || len <= prefix.size() || memcmp(effective_buf(), prefix.data(), prefix.size())) {
            return;
        }
        if (view_parent) {
            materialize();
        }
        memmove(ownedBuffer(), ownedBuffer() + prefix.size(), len - prefix.size());
        len -= prefix.size();
        ownedBuffer()[len] = '\0';
        resetOwnedAsciiPrefix();
    }

    //! returns true if the string contains the given text, which must be in the encoding of the string
    /** The text of a string in a stateful encoding is searched by characters, as its characters have no bytes of
        their own; any other string is searched byte by byte at character boundaries

        @param needle the text to search for, in the encoding of the string
        @param xsink for exceptions (text that is not valid in a stateful encoding); may be nullptr, in which case
        such text is not found
    */
    DLLLOCAL bool containsText(const QoreString& needle, ExceptionSink* xsink) const;

    //! trims the characters in the given vector from the start or end of the string, which is in a stateful encoding
    /** The characters are trimmed from the text in UTF-8, and the string takes the result converted back to its
        encoding

        @return 0 for OK, -1 if an exception was raised; the string is not changed in this case
    */
    DLLLOCAL int statefulTrim(ExceptionSink* xsink, const intvec_t& cvec, bool leading);

    //! replaces the source string of an iterator with a new string with its text in UTF-8
    /** For iterators over the characters of a string in a stateful encoding, whose characters have no bytes of their
        own, and that walk its text in UTF-8 instead

        @param src the source string; on success, the reference to it is released and it is set to the new string
        @param xsink for exceptions

        @return 0 for OK, -1 if an exception was raised (text that is not valid in its encoding); \a src is not changed
        in this case
    */
    DLLLOCAL static int replaceWithUtf8(QoreStringNode*& src, ExceptionSink* xsink);

    //! sets the target to the characters of the string, which is in a stateful encoding, in reverse order
    /** @param targ the target string, which must be empty and in the encoding of this string

        @return 0 for OK, -1 if the text is not valid in the encoding or its reverse cannot be represented in it; the
        target is not changed in this case
    */
    DLLLOCAL int concat_reverse_stateful(qore_string_private& targ) const;

    //! replaces the text of the string with the text of the given string in the same encoding, which takes the old
    //! text
    DLLLOCAL void swapText(qore_string_private& other) {
        assert(other.getEncoding() == getEncoding());
        if (view_parent) {
            materialize();
        }
        if (other.view_parent) {
            other.materialize();
        }
        std::swap(ownedBuffer(), other.ownedBuffer());
        std::swap(len, other.len);
        std::swap(allocated, other.allocated);
        // the ASCII prefix caches of owned strings go with their bytes
        std::swap(view_offset, other.view_offset);
        sb.swap(other.sb);
    }

    //! Returns the length of the string in characters
    DLLLOCAL int getCharLength(size_t& clen, ExceptionSink* xsink) const {
        if (isStateful()) {
            QoreString utf8(QCS_UTF8);
            if (statefulToUtf8(utf8, xsink)) {
                return -1;
            }
            return utf8.priv->getCharLength(clen, xsink);
        }
        if (!getEncoding()->isMultiByte()) {
            clen = len;
            return 0;
        }
        const char* b = effective_buf();
        clen = getLengthFrom(b, 0, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL qore_offset_t index(const QoreString &orig_needle, qore_offset_t pos, ExceptionSink *xsink) const {
        assert(xsink);
        if (isStateful()) {
            // character offsets are the same in the text in UTF-8
            QoreString utf8(QCS_UTF8);
            if (statefulToUtf8(utf8, xsink)) {
                return -1;
            }
            return utf8.priv->index(orig_needle, pos, xsink);
        }
        TempEncodingHelper needle(orig_needle, getEncoding(), xsink);
        if (!needle)
            return -1;

        if (!needle->size()) {
            size_t clen;
            if (getCharLength(clen, xsink)) {
                return -1;
            }
            return emptyNeedleIndex(pos, clen);
        }

        const char* b = effective_buf();
        // do simple index
        if (!getEncoding()->isMultiByte()) {
            if (pos < 0) {
                pos = len + pos;
                if (pos < 0) {
                    pos = 0;
                }
            } else if (pos >= (qore_offset_t)len) {
                return -1;
            }

            return index_simple(b, len, needle->c_str(), needle->size(), pos, getCharAlignment());
        }

        // do multibyte index()
        if (findByteOffset(pos, xsink))
            return -1;
        if (pos < 0)
            pos = 0;
        else if (pos >= (qore_offset_t)len)
            return -1;

        qore_offset_t ind = index_simple(b + pos, len - pos, needle->c_str(), needle->size(), 0,
            getCharAlignment(), pos);
        if (ind != -1) {
            ind = getCharPosTo(b, pos + ind, xsink);
            if (*xsink)
                return -1;
        }

        return ind;
    }

    DLLLOCAL qore_offset_t bindex(const QoreString& needle, qore_offset_t pos) const {
        if (!needle.strlen()) {
            return emptyNeedleIndex(pos, len);
        }
        // a negative position is an offset from the end, which bindex(const char*, ...) resolves
        if (pos >= 0 && needle.strlen() + pos > len)
            return -1;

        return bindex(needle.c_str(), pos, needle.size());
    }

    DLLLOCAL qore_offset_t bindex(const std::string& needle, qore_offset_t pos) const {
        if (needle.empty()) {
            return emptyNeedleIndex(pos, len);
        }
        // a negative position is an offset from the end, which bindex(const char*, ...) resolves
        if (pos >= 0 && needle.size() + pos > len)
            return -1;

        return bindex(needle.c_str(), pos, needle.size());
    }

    DLLLOCAL qore_offset_t bindex(const char* needle, qore_offset_t pos, size_t nsize = 0) const {
        if (!nsize) {
            nsize = strlen(needle);
        }
        if (!nsize) {
            return emptyNeedleIndex(pos, len);
        }
        if (pos < 0) {
            pos = len + pos;
            if (pos < 0) {
                pos = 0;
            }
        } else if (pos >= (qore_offset_t)len) {
            return -1;
        }

        return index_simple(effective_buf(), len, needle, nsize, pos, getCharAlignment());
    }

    // finds the last occurrence of needle in haystack at or before position pos
    // pos must be a non-negative valid byte offset in haystack
    DLLLOCAL static qore_offset_t rindex_simple(const char* haystack, size_t hlen, const char* needle,
            size_t nlen, qore_offset_t pos = -1, const qore_char_boundary& align = qore_char_boundary()) {
        if (pos < 0) {
            pos = hlen + pos;
            if (pos < 0) {
                return -1;
            }
        } else if (pos >= (qore_offset_t)hlen) {
            pos = hlen - 1;
        }

        assert(pos < (qore_offset_t)hlen);
        const char* ptr = memrmem_aligned(haystack, pos + 1, needle, nlen, align);
        if (!ptr) {
            return -1;
        }
        return static_cast<qore_offset_t>(ptr - haystack);
    }

    // start is a byte offset that has to point to the start of a valid character
    DLLLOCAL int findByteOffset(qore_offset_t& pos, ExceptionSink* xsink, size_t start = 0) const {
        assert(xsink);
        assert(getEncoding()->isMultiByte());
        if (!pos)
            return 0;
        const char* b = effective_buf();
        // get positive character offset if negative
        if (pos < 0) {
            // get the length of the string in characters
            size_t clen = getLengthFrom(b, start, xsink);
            if (*xsink)
                return -1;
            pos = clen + pos;
            // a position before the start stays negative: the caller handles it as with a single-byte encoding
            if (pos <= 0) {
                return 0;
            }
        }
        // now get the byte position from this character offset
        pos = getByteLenFrom(b, start, pos, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL qore_offset_t rindex(const QoreString &orig_needle, qore_offset_t pos, ExceptionSink *xsink) const {
        assert(xsink);
        if (isStateful()) {
            // character offsets are the same in the text in UTF-8
            QoreString utf8(QCS_UTF8);
            if (statefulToUtf8(utf8, xsink)) {
                return -1;
            }
            return utf8.priv->rindex(orig_needle, pos, xsink);
        }
        TempEncodingHelper needle(orig_needle, getEncoding(), xsink);
        if (!needle)
            return -1;

        if (!needle->size()) {
            size_t clen;
            if (getCharLength(clen, xsink)) {
                return -1;
            }
            return emptyNeedleRindex(pos, clen);
        }

        const char* b = effective_buf();
        if (!getEncoding()->isMultiByte()) {
            if (pos < 0) {
                pos = len + pos;
                if (pos < 0)
                return -1;
            }

            return rindex_simple(b, len, needle->c_str(), needle->size(), pos, getCharAlignment());
        }

        // do multi-byte rindex: as with a single-byte encoding, the search covers the characters from the start up
        // to and including the character at pos, so the byte range ends after that character, not at its first byte
        size_t clen = 0;
        if (pos < 0) {
            if (getCharLength(clen, xsink)) {
                return -1;
            }
            pos += clen;
            if (pos < 0) {
                return -1;
            }
        }
        // the number of bytes in the characters up to and including the one at pos; the default position (-1, the
        // last character) needs no second scan
        size_t blen = (clen && (size_t)pos + 1 == clen)
            ? len
            : getEncoding()->getByteLen(b, b + len, (size_t)pos + 1, xsink);
        if (*xsink) {
            return -1;
        }
        if (!blen) {
            return -1;
        }

        // get byte rindex position
        qore_offset_t ind = rindex_simple(b, len, needle->c_str(), needle->size(), blen - 1, getCharAlignment());

        // calculate character position from byte position
        if (ind && ind != -1) {
            ind = getCharPosTo(b, ind, xsink);
            if (*xsink)
                return 0;
        }

        return ind;
    }

    DLLLOCAL qore_offset_t brindex(const QoreString& needle, qore_offset_t pos) const {
        return brindex(needle.getBuffer(), needle.strlen(), pos);
    }

    DLLLOCAL qore_offset_t brindex(const std::string& needle, qore_offset_t pos) const {
        return brindex(needle.c_str(), needle.size(), pos);
    }

    DLLLOCAL qore_offset_t brindex(const char* needle, size_t needle_len, qore_offset_t pos) const {
        if (!needle_len) {
            return emptyNeedleRindex(pos, len);
        }

        if (pos < 0) {
            pos = len + pos;
        }

        if (pos >= (qore_offset_t)len) {
            pos = len - 1;
        }

        if (pos < 0) {
            return -1;
        }

        // the match lies in the bytes up to and including pos, as with rindex()
        if (needle_len > (size_t)pos + 1) {
            return -1;
        }

        return rindex_simple(effective_buf(), len, needle, needle_len, pos, getCharAlignment());
    }

    // memcmp() and not strncmp(): strings can contain embedded nulls, either because the encoding
    // uses them (UTF-16*) or because the string data itself contains them
    DLLLOCAL bool startsWith(const char* str, size_t ssize) const {
        if (ssize > len) {
            return false;
        }
        return !memcmp(str, effective_buf(), ssize);
    }

    DLLLOCAL bool endsWith(const char* str, size_t ssize) const {
        if (ssize > len) {
            return false;
        }
        return !memcmp(str, effective_buf() + len - ssize, ssize);
    }

    DLLLOCAL bool isDataPrintableAscii() const {
        const char* b = effective_buf();
        for (size_t i = 0; i < len; ++i) {
            if (b[i] < 32 || b[i] > 126)
                return false;
        }
        return true;
    }

    DLLLOCAL bool isDataAscii() const {
        const char* b = effective_buf();
        for (size_t i = 0; i < len; ++i) {
            if ((unsigned char)(b[i]) > 127)
                return false;
        }
        return true;
    }

    DLLLOCAL void concat_intern(const char* p, size_t plen) {
        assert(p);
        assert(plen);
        // only growth or a view moves the text (the test is the one that check_char() makes)
        if (view_parent || len + plen >= allocated) {
            p = growKeepingSource(len + plen, p);
        }
        memcpy(ownedBuffer() + len, p, plen);
        len += plen;
        ownedBuffer()[len] = '\0';
    }

    DLLLOCAL void concat_simple(const qore_string_private& str, qore_offset_t pos) {
        if (pos < 0) {
            pos = str.len + pos;
            if (pos < 0)
                pos = 0;
        }
        // nothing is concatenated from the end of str (or from an empty str)
        if (pos >= (qore_offset_t)str.len)
            return;

        concat_intern(str.effective_buf() + pos, str.len - pos);
    }

    DLLLOCAL int concat(const qore_string_private& str, qore_offset_t pos, ExceptionSink* xsink) {
        assert(str.getEncoding() == getEncoding());

        if (isStateful()) {
            return concatStateful(str, pos, nullptr, xsink);
        }
        if (!getEncoding()->isMultiByte()) {
            concat_simple(str, pos);
            return 0;
        }

        // find byte positions from character positions
        if (pos) {
            if (str.findByteOffset(pos, xsink))
                return -1;
            if (pos < 0)
                pos = 0;
        }
        // nothing is concatenated from the end of str (or from an empty str)
        if (pos >= (qore_offset_t)str.len)
            return 0;

        concat_intern(str.effective_buf() + pos, str.len - pos);
        return 0;
    }

    // concatenates plen bytes of str from pos; a negative plen leaves that many bytes off the end of str; the range
    // is clamped to str
    DLLLOCAL void concat_simple(const qore_string_private& str, qore_offset_t pos, qore_offset_t plen) {
        if (pos < 0) {
            pos = str.len + pos;
            if (pos < 0)
                pos = 0;
        }
        // nothing is concatenated from the end of str (or from an empty str)
        if (pos >= (qore_offset_t)str.len)
            return;

        // the number of bytes after pos
        qore_offset_t rest = str.len - pos;
        if (plen < 0) {
            plen = rest + plen;
            if (plen <= 0)
                return;
        } else if (plen > rest)
            plen = rest;

        concat_intern(str.effective_buf() + pos, plen);
    }

    DLLLOCAL int concat(const qore_string_private& str, qore_offset_t pos, qore_offset_t plen, ExceptionSink* xsink) {
        assert(str.getEncoding() == getEncoding());
        assert(plen);

        if (isStateful()) {
            return concatStateful(str, pos, &plen, xsink);
        }
        if (!getEncoding()->isMultiByte()) {
            concat_simple(str, pos, plen);
            return 0;
        }

        // find byte positions from character positions
        if (pos) {
            if (str.findByteOffset(pos, xsink))
                return -1;
            if (pos < 0)
                pos = 0;
            else if (pos > (qore_offset_t)str.len)
                return 0;
        }

        // find the byte position from the starting byte
        if (str.findByteOffset(plen, xsink, pos))
            return -1;
        if (plen <= 0)
            return 0;
        // the bytes after pos
        if (plen > (qore_offset_t)str.len - pos)
            plen = str.len - pos;

        concat_intern(str.effective_buf() + pos, plen);
        return 0;
    }

    DLLLOCAL qore_offset_t getByteOffset(size_t i, ExceptionSink* xsink) const {
        assert(xsink);
        if (isStateful()) {
            // a character has no byte offset of its own in a stateful encoding
            raiseStatefulByteOffsetError(xsink);
            return -1;
        }
        size_t rc;
        if (i) {
            const char* b = effective_buf();
            rc = getByteLenFrom(b, 0, i, xsink);
            if (*xsink)
                return -1;
        } else
            rc = 0;
        return rc > len ? -1 : (qore_offset_t)rc;
    }

    DLLLOCAL size_t removeBytes(size_t num) {
        if (num >= len) {
            clear();
            return len;
        }
        if (view_parent) {
            materialize();
        }
        memmove(ownedBuffer(), ownedBuffer() + num, len - num);
        len -= num;
        resetOwnedAsciiPrefix();
        return num;
    }

    DLLLOCAL void clear() {
        if (view_parent) {
            abandon_view();
            return;
        }
        if (allocated) {
            len = 0;
            ownedBuffer()[0] = '\0';
            resetOwnedAsciiPrefix();
        }
    }

    DLLLOCAL void concat(char c) {
        if (view_parent) {
            materialize();
        }
        if (allocated) {
            // the string is grown first, so that memory that cannot be allocated leaves it unchanged (qore#7466)
            check_char(len + 1);
            ownedBuffer()[len] = c;
            ownedBuffer()[++len] = '\0';
            return;
        }
        // allocate new string buffer
        // memory that cannot be allocated fails with std::bad_alloc and leaves the string unchanged (qore#7466)
        ownedBuffer() = static_cast<char*>(qore_malloc_or_throw(sizeof(char) * STR_CLASS_BLOCK));
        allocated = STR_CLASS_BLOCK;
        len = 1;
        sb.sync(allocated);
        ownedBuffer()[0] = c;
        ownedBuffer()[1] = '\0';
    }

    //! Appends a name of the library (a static or global string of ASCII characters) in the encoding of this string
    /** The encoding of such a string depends on the order of the static initialization of the library: it has no
        encoding when it is created before the default encoding is set, and the default encoding (UTF-8) otherwise;
        its characters are ASCII, and so the same in the encoding of any string that a name is written to
        (qore#6820)
    */
    DLLLOCAL void concatAsciiName(const QoreString& name) {
        const qore_string_private* p = name.priv;
        // the bytes are the same in this string's encoding only if it is ASCII-compatible
        assert(!encoding || encoding->isAsciiCompat());
#ifdef DEBUG
        for (size_t i = 0; i < p->len; ++i) {
            assert(!(static_cast<unsigned char>(p->effective_buf()[i]) & 0x80));
        }
#endif
        if (p->len) {
            check_char(p->len + len + STR_CLASS_EXTRA);
            memcpy(ownedBuffer() + len, p->effective_buf(), p->len);
            len += p->len;
            ownedBuffer()[len] = '\0';
        }
    }

    DLLLOCAL void concat(const qore_string_private* str) {
        assert(!str || (str->encoding == encoding) || !str->encoding);

        // if it's not a null string
        if (str && str->len) {
            // if priv->buffer needs to be resized
            check_char(str->len + len + STR_CLASS_EXTRA);
            // concatenate new string (source may be a view)
            memcpy(ownedBuffer() + len, str->effective_buf(), str->len);
            len += str->len;
            ownedBuffer()[len] = '\0';
        }
    }

    // concatenate QoreString without encoding conversion
    DLLLOCAL void concat(const QoreString* str) {
        if (str) {
            concat(str->priv);
        }
    }

    DLLLOCAL void concat(const char *str) {
        // a source that is part of the string is appended with its size, as the string grows while it is read (out
        // of line, so that this function stays small enough to be inlined)
        if (str && ownDataOffset(str) >= 0) {
            concatOwnText(str);
            return;
        }
        // if it's not a null string
        if (str) {
            size_t i = 0;
            // iterate through new string
            while (str[i]) {
                // if priv->buffer needs to be resized
                check_char(len);
                // concatenate one character at a time
                ownedBuffer()[len++] = str[i++];
            }
            // see if priv->buffer needs to be resized for '\0'
            check_char(len);
            // terminate string
            ownedBuffer()[len] = '\0';
        }
    }

    DLLLOCAL int concat(const QoreString* str, ExceptionSink* xsink);

    // return 0 for success
    //! Appends the output of a printf() format
    /** @param fmt the format
        @param args the arguments of the format
        @param failed set to true if the output cannot be formatted

        @return -1 if the buffer was too small and the call must be repeated with the same arguments, 0 otherwise;
        nothing is appended if the output cannot be formatted, such as an output longer than INT_MAX characters
    */
    DLLLOCAL int vsprintf(const char *fmt, va_list args, bool* failed = nullptr) {
        // the arguments may be part of the string's text: it is formatted separately before the string is changed
        char sbuf[256];
        va_list args2;
        va_copy(args2, args);
        int i = ::vsnprintf(sbuf, sizeof(sbuf), fmt, args);
        std::unique_ptr<char, QoreFreeDeleter> big;
        const char* out = sbuf;
#ifdef HPUX
        // vsnprintf() fails without giving the size needed: the buffer is doubled until it is large enough
        size_t big_size = sizeof(sbuf);
        while (i < 0 && big_size < (static_cast<size_t>(1) << 30)) {
            big_size *= 2;
            big.reset(static_cast<char*>(malloc(big_size)));
            if (!big) {
                i = -1;
                break;
            }
            va_list args3;
            va_copy(args3, args2);
            i = ::vsnprintf(big.get(), big_size, fmt, args3);
            va_end(args3);
            out = big.get();
        }
#else
        if (i >= static_cast<int>(sizeof(sbuf))) {
            big.reset(static_cast<char*>(malloc(static_cast<size_t>(i) + 1)));
            // a buffer that cannot be allocated fails the call as an output that cannot be formatted
            i = big ? ::vsnprintf(big.get(), static_cast<size_t>(i) + 1, fmt, args2) : -1;
            out = big.get();
        }
#endif
        va_end(args2);
        if (i < 0) {
            // the output cannot be formatted: nothing is appended, and repeating the call cannot change that
            mutableBuffer();
            if (ownedBuffer()) {
                *(ownedBuffer() + len) = '\0';
            }
            if (failed) {
                *failed = true;
            }
            return 0;
        }
        check_char(len + i);
        memcpy(ownedBuffer() + len, out, i);
        len += i;
        ownedBuffer()[len] = '\0';
        return 0;
    }

    DLLLOCAL int sprintf(const char *fmt, ...) {
        va_list args;
        while (true) {
            va_start(args, fmt);
            int rc = vsprintf(fmt, args);
            va_end(args);
            if (!rc) {
                break;
            }
        }
        return 0;
    }

    //! Appends the output of a printf() format, and reports if it cannot be formatted
    /** @param fmt the format
        @param ... the arguments of the format

        @return 0 if the output was appended, -1 if it cannot be formatted (it is then not appended), such as an
        output longer than INT_MAX characters
    */
    DLLLOCAL int sprintfChecked(const char *fmt, ...) {
        va_list args;
        bool failed = false;
        while (true) {
            va_start(args, fmt);
            int rc = vsprintf(fmt, args, &failed);
            va_end(args);
            if (!rc) {
                break;
            }
        }
        return failed ? -1 : 0;
    }

    DLLLOCAL void concatUTF8FromUnicode(unsigned code);

    // writes a new qore_string_private with the characters reversed from this string
    // assumes the encoding is the same and the target length is 0
    DLLLOCAL void concat_reverse(qore_string_private& targ) const {
        assert(targ.getEncoding() == getEncoding());
        assert(!targ.len);

        if (isStateful() && !concat_reverse_stateful(targ)) {
            return;
        }

        targ.check_char(len);
        const char* src = effective_buf();
        if (getEncoding()->isMultiByte()) {
            const char* p = src;
            char* targ_end = targ.ownedBuffer() + len;
            const char* end = src + len;
            // an encoding made of units of more than one byte (UTF-16) reverses a unit that does not start a
            // character as a character of its own, so that the text after it is read in whole units, as length()
            // counts it; a byte is reversed on its own where a whole unit does not remain (qore#6604)
            const size_t unit = getEncoding()->getMinCharWidth();
            while (p < end) {
                bool invalid;
                size_t bl = getEncoding()->getByteLen(p, end, 1, invalid);
                if (invalid) {
                    bl = unit > 1 && static_cast<size_t>(end - p) >= unit ? unit : 1;
                }
                targ_end -= bl;
                if (targ_end < targ.ownedBuffer()) {
                    break;
                }
                // memcpy() and not strncpy(): multi-byte encodings such as UTF-16* have characters
                // with embedded nulls, and strncpy() would stop at the first null and null-pad the
                // rest of the character
                memcpy(targ_end, p, bl);
                p += bl;
            }
        } else {
            for (size_t i = 0; i < len; ++i) {
                targ.ownedBuffer()[i] = src[len - i - 1];
            }
        }

        targ.ownedBuffer()[len] = '\0';
        targ.len = len;
    }

    DLLLOCAL int concatUnicode(unsigned code, ExceptionSink *xsink) {
        assert(xsink);
        if (getEncoding() == QCS_UTF8) {
            concatUTF8FromUnicode(code);
            return 0;
        }

        QoreString tmp(QCS_UTF8);
        tmp.concatUTF8FromUnicode(code);
        TempString ns(tmp.convertEncoding(getEncoding(), xsink));
        if (*xsink)
            return -1;
        concat(ns->priv);
        return 0;
    }

    DLLLOCAL void setRegexBaseOpts(QoreRegexBase& re, int opts);

    DLLLOCAL void setRegexOpts(QoreRegexSubst& re, int opts);

    DLLLOCAL void splice_simple(size_t offset, size_t length, QoreString* extract = nullptr);
    DLLLOCAL void splice_simple(size_t offset, size_t length, const char* str, size_t str_len,
            QoreString* extract = nullptr);
    DLLLOCAL void splice_complex(qore_offset_t offset, ExceptionSink* xsink, QoreString* extract = nullptr);
    DLLLOCAL void splice_complex(qore_offset_t offset, qore_offset_t length, ExceptionSink* xsink,
            QoreString* extract = nullptr);
    DLLLOCAL void splice_complex(qore_offset_t offset, qore_offset_t length, const QoreString* str,
            ExceptionSink* xsink, QoreString* extract = nullptr);
    DLLLOCAL int substr_simple(QoreString* str, qore_offset_t offset) const;
    DLLLOCAL int substr_simple(QoreString* str, qore_offset_t offset, qore_offset_t length) const;
    DLLLOCAL int substr_complex(QoreString* str, qore_offset_t offset, ExceptionSink* xsink) const;
    DLLLOCAL int substr_complex(QoreString* str, qore_offset_t offset, qore_offset_t length,
            ExceptionSink* xsink) const;

    DLLLOCAL int trimLeading(ExceptionSink* xsink, const intvec_t& vec);
    DLLLOCAL int trimLeading(ExceptionSink* xsink, const qore_string_private* chars);
    DLLLOCAL int trimTrailing(ExceptionSink* xsink, const intvec_t& vec);
    DLLLOCAL int trimTrailing(ExceptionSink* xsink, const qore_string_private* chars);

    DLLLOCAL void terminate(size_t size);

    DLLLOCAL int concatUnicode(unsigned code);

    DLLLOCAL int concatDecodeUriIntern(ExceptionSink* xsink, const qore_string_private& str,
            bool detect_query = false);

    DLLLOCAL int concatEncodeUriRequest(ExceptionSink* xsink, const qore_string_private& str);

    DLLLOCAL unsigned int getUnicodePointFromBytePos(size_t offset, unsigned& len, ExceptionSink* xsink) const;

    DLLLOCAL int concatEncode(ExceptionSink* xsink, const QoreString& str, unsigned code = CE_XHTML);
    DLLLOCAL int concatDecode(ExceptionSink* xsink, const QoreString& str, unsigned code = CD_ALL);

    DLLLOCAL int getUnicodeCharArray(intvec_t& vec, ExceptionSink* xsink) const {
        if (isStateful()) {
            QoreString utf8(QCS_UTF8);
            if (statefulToUtf8(utf8, xsink)) {
                return -1;
            }
            return utf8.priv->getUnicodeCharArray(vec, xsink);
        }
        size_t j = 0;
        while (j < len) {
            unsigned clen;
            int c = getUnicodePointFromBytePos((qore_offset_t)j, clen, xsink);
            if (*xsink)
                return -1;
            vec.push_back(c);
            j += clen;
        }
        return 0;
    }

    DLLLOCAL int allocate(unsigned requested_size) {
        if (view_parent) {
            materialize();
        }
        if ((unsigned)allocated >= requested_size)
            return 0;
        requested_size = (requested_size / 0x10 + 1) * 0x10; // fill complete cache line
        // memory that cannot be allocated fails with std::bad_alloc and leaves the string unchanged (qore#7466)
        ownedBuffer() = static_cast<char*>(qore_realloc_or_throw(ownedBuffer(), requested_size * sizeof(char)));
        allocated = requested_size;
        sb.sync(allocated);
        return 0;
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        return encoding ? encoding : QCS_USASCII;
    }

    DLLLOCAL size_t getCharWidth(ExceptionSink* xsink) const;

    DLLLOCAL void concatBase64(const char* bbuf, size_t size, size_t maxlinelen, bool url_encode = false);

    DLLLOCAL static bool inVector(int c, const intvec_t& vec) {
        for (unsigned j = 0; j < vec.size(); ++j) {
            if ((int)vec[j] == c)
                return true;
        }
        return false;
    }

    DLLLOCAL static qore_string_private* get(QoreString& str) {
        return str.priv;
    }

    DLLLOCAL static qore_string_private* get(QoreString* str) {
        return str ? str->priv : nullptr;
    }

    DLLLOCAL static const qore_string_private* get(const QoreString& str) {
        return str.priv;
    }

    DLLLOCAL static const qore_string_private* get(const QoreString* str) {
        return str ? str->priv : nullptr;
    }

    DLLLOCAL static void adopt(QoreString& str, qore_string_private* p) {
        delete str.priv;
        str.priv = p;
    }

    DLLLOCAL static int getHex(const char*& p) {
        if (*p == '%' && isxdigit(*(p + 1)) && isxdigit(*(p + 2))) {
            char x[3] = { *(p + 1), *(p + 2), '\0' };
            p += 3;
            return strtol(x, 0, 16);
        }
        return -1;
    }

    //! Converts \a src from encoding \a from to encoding \a nccs, storing the result in \a targ
    /** @param verify verify the result by converting it back and comparing it with the source,
        when the platform's iconv does not report non-reversible conversions; passed as \c false
        for the reverse conversion itself so that the check cannot recurse
        @param locate if a character cannot be converted, report the byte offset of the first character of the
        source that cannot be converted (see find_unconvertible_char()); passed as \c false for the conversions
        that find it, so that the search cannot recurse
        @param err_offset if given, the byte offset of the illegal or unconvertible sequence is stored here when the
        conversion fails because of one, also when no exception is raised because \a xsink is \c nullptr
    */
    DLLLOCAL static int convert_encoding_intern(const char* src, size_t src_len, const QoreEncoding* from,
            QoreString& targ, const QoreEncoding* nccs, ExceptionSink* xsink, bool verify = true, bool locate = true,
            size_t* err_offset = nullptr);

    //! Returns the byte offset of the first character of \a src that a conversion to \a to cannot keep
    /** The source is decoded one character at a time (see Utf8OffsetDecoder), and each character is converted to
        the target and back; an invalid or incomplete character, if there is one before the first character that
        cannot be converted, is reported instead.  Only called when a conversion has failed.

        @param src the source text
        @param len the byte length of the source text
        @param from the encoding of the source text
        @param to the target encoding
        @param fallback the offset returned if no such character is found

        @return the byte offset of the character
    */
    DLLLOCAL static size_t find_unconvertible_char(const char* src, size_t len, const QoreEncoding* from,
            const QoreEncoding* to, size_t fallback);

    //! Converts text in a stateful encoding by decoding it one byte at a time
    /** Used where the platform's iconv does not fail with \c EINVAL for stateful text cut off in a character (see
        IconvHelper::reportsTruncatedStatefulInput()); the text is decoded to UTF-8 with Utf8OffsetDecoder and then
        converted to the target encoding.  The arguments are as for convert_encoding_intern().
    */
    DLLLOCAL static int convert_stateful_intern(const char* src, size_t src_len, const QoreEncoding* from,
            QoreString& targ, const QoreEncoding* nccs, ExceptionSink* xsink, bool verify, bool locate,
            size_t* err_offset);

    //! Returns true if a conversion from \a from to \a to can lose data and must be verified
    /** Conversions to a Unicode encoding can represent every source character, and an all-ASCII
        source moving between ASCII-compatible encodings cannot lose anything either, so those
        skip the reverse conversion entirely.
    */
    DLLLOCAL static bool conversion_needs_roundtrip_check(const QoreEncoding* from, const QoreEncoding* to,
            const char* src, size_t src_len);
};

template <typename T>
DLLLOCAL T* binary_to_string(BinaryNode* bin, const QoreEncoding* qe) {
    SimpleRefHolder<BinaryNode> b(bin);
    if (!b) {
        return nullptr;
    }

    if (b->empty()) {
        return new QoreStringNode;
    }

    // the string's data is released if the conversion fails with std::bad_alloc (qore#7466)
    std::unique_ptr<qore_string_private> p(new qore_string_private);
    p->len = b->size() - 1;
    p->ownedBuffer() = (char*)b->giveBuffer();
    p->encoding = qe;

    // free binary object
    b = 0;

    // check for null termination
    if (p->ownedBuffer()[p->len]) {
        // memory that cannot be allocated fails with std::bad_alloc and leaves the string unchanged (qore#7466)
        p->ownedBuffer() = static_cast<char*>(qore_realloc_or_throw(p->ownedBuffer(), p->len + 2));
        ++p->len;
        p->ownedBuffer()[p->len] = '\0';
    }

    p->allocated = p->len + 1;
    p->sb.sync(p->allocated);
    T* rv = new T();
    qore_string_private::adopt(*rv, p.release());
    return rv;
}

//! Returns the value of a string with the given content in a boolean context
/** The same rule applies to a string in a QoreStringNode and to a short string stored inline in a QoreValue:
    with @ref strict-bool-eval "%strict-bool-eval", the string is converted to a number, otherwise only an empty
    string and the string \c "0" are False

    @param str the string content; must be null-terminated
    @param len the length of the string in bytes
*/
DLLLOCAL bool qore_string_get_as_bool(const char* str, size_t len);

//! Returns true if the string is valid in its encoding; never raises an exception
/** A UTF-8 string is checked strictly (RFC 3629: no overlong forms, surrogates, values above U+10FFFF or unexpected
    continuation bytes; see IconvHelper::validUtf8Prefix()), as the regular expression engine checks it; ASCII text in
    an ASCII-compatible encoding that is not stateful is valid; any other string is valid if it can be converted to
    UTF-8 (so a string with a byte that its encoding does not define, or with text in an encoding that iconv cannot
    convert, is not valid).  O(n) in the size of the string; no exception is raised or cleared (qore#6554).
*/
DLLLOCAL bool qore_string_encoding_valid(const QoreString& str);

//! Returns a new string for an HTTP field value (a header or trailer value)
/** The value is in the default encoding, or in ISO-8859-1 with its bytes unchanged if it is not valid in it: a field
    value with bytes that are not valid UTF-8 is obs-text (RFC 9110 section 5.5), which is valid text in ISO-8859-1
    (qore#6554)
*/
DLLLOCAL QoreStringNode* qore_http_field_value_string(const char* p, size_t len);

//! Gives an HTTP field value made by concatenation in ISO-8859-1 if it is not valid in its encoding (qore#6554)
DLLLOCAL void qore_http_field_value_check(QoreStringNode* str);

//! Returns true if an HTTP request target (a path with an optional query) is text that the HTTP server accepts
/** A target with a raw byte above 0x7f or a raw NUL is not accepted (the normal path decides, in the encoding of the
    socket); otherwise the path and the query are decoded with the decoder of decode_url(), as \c HttpServer decodes
    them, and each must decode to valid UTF-8; the decoded path must not have a NUL character (qore#6554).  No exception is left in an
    exception sink; O(n), with no allocation for a target without \c "%" and bytes above 0x7f.
*/
DLLLOCAL bool qore_http_target_text_valid(const char* p, size_t len);


//! a source for a QoreString operation: a copy if the source is part of the string's text, which the operation moves
class QoreStringOwnSource {
public:
    //! a source of \a n bytes
    DLLLOCAL QoreStringOwnSource(const qore_string_private& str, const char* p, size_t n) : ptr(p) {
        if (p && n && str.ownDataOffset(p) >= 0) {
            copyIntern(p, n);
        }
    }

    //! a null-terminated source
    DLLLOCAL QoreStringOwnSource(const qore_string_private& str, const char* p) : ptr(p) {
        if (p && str.ownDataOffset(p) >= 0) {
            copyIntern(p, ::strlen(p));
        }
    }

    DLLLOCAL const char* get() const {
        return ptr;
    }

    //! returns false if the source had to be copied and the copy could not be made (the operation then fails)
    DLLLOCAL bool ok() const {
        return !failed;
    }

private:
    std::unique_ptr<char, QoreFreeDeleter> copy;
    const char* ptr;
    bool failed = false;

    //! copies the source with a terminator
    DLLLOCAL void copyIntern(const char* p, size_t n) {
        copy.reset(static_cast<char*>(malloc(n + 1)));
        if (!copy) {
            ptr = nullptr;
            failed = true;
            return;
        }
        memcpy(copy.get(), p, n);
        copy.get()[n] = '\0';
        ptr = copy.get();
    }
};

#endif
