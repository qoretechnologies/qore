/*
    BinaryNode.cpp

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

#include <qore/Qore.h>
#include "qore/intern/LexicalSemanticPolicy.h"
#include "qore/intern/QoreSandboxMemory.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

// the sandbox memory charge of the buffer is kept in a sparse table (see qore_sb_binary_resize()), since the class
// has no private data; the charge is the capacity of the buffer (capacity())

// the capacity is kept in the tail padding of the base class: the layout that modules built with the previous header
// use is unchanged
static_assert(sizeof(BinaryNode) == sizeof(SimpleValueQoreNode) + sizeof(void*) + sizeof(size_t),
    "BinaryNode::cap_code must not change the size of the class");

//! the smallest exponent of a capacity code (BinaryNode::cap_code): the smallest capacity is 16 bytes
static constexpr unsigned QORE_BINARY_MIN_CAP_EXP = 4;
//! the largest exponent of a capacity code: 7 << 60 is the largest capacity that a 64-bit size_t holds
static constexpr unsigned QORE_BINARY_MAX_CAP_EXP = sizeof(size_t) * 8 - 2;

//! returns the byte offset of \a p in the buffer \a buf of \a len bytes, or -1 if it does not point into it
static inline int64 qore_binary_own_offset(const void* buf, size_t len, const void* p) {
    if (!buf) {
        return -1;
    }
    uintptr_t b = reinterpret_cast<uintptr_t>(buf);
    uintptr_t q = reinterpret_cast<uintptr_t>(p);
    return (q >= b && q < b + len) ? static_cast<int64>(q - b) : -1;
}

//! returns the capacity that a capacity code gives
static inline size_t qore_binary_code_capacity(unsigned code) {
    return static_cast<size_t>(4 + (code & 3)) << ((code >> 2) - 2);
}

//! returns the code of the smallest capacity that holds the given size, or 0 if none does
/** The capacities of the codes are (4 + m) * 2^(e - 2) bytes (m = 0..3): consecutive ones grow by factors of 1.14 to
    1.25, as strings do (qore_string_private::check_char())
*/
static unsigned qore_binary_capacity_code(size_t size) {
    unsigned e = QORE_BINARY_MIN_CAP_EXP;
    while (e < QORE_BINARY_MAX_CAP_EXP && (static_cast<size_t>(1) << (e + 1)) <= size) {
        ++e;
    }
    for (; e <= QORE_BINARY_MAX_CAP_EXP; ++e) {
        for (unsigned m = 0; m < 4; ++m) {
            unsigned code = (e << 2) | m;
            if (qore_binary_code_capacity(code) >= size) {
                return code;
            }
        }
    }
    return 0;
}

BinaryNode::BinaryNode(void* p, size_t size) : SimpleValueQoreNode(NT_BINARY) {
    cap_code = 0;
    ptr = p;
    len = size;
    if (p) {
        qore_sb_binary_resize_check(this, 0, size);
    }
}

BinaryNode::~BinaryNode() {
    // released even without a buffer: a reallocation that failed has freed it, but not its charge; the charge
    // released is the one recorded, which is the capacity of the buffer
    qore_sb_binary_free_check(this, capacity());
    if (ptr) {
        free(ptr);
    }
}

void BinaryNode::clear() {
    // issue #2982: must check 'ptr', len may be 0 with memory allocated
    // NOTE: we check and then free & update to avoid writing to memory
    // to avoid cache flushing on SMP systems (as used in the Linux
    // kernel for example)
    // the charge released is the one recorded, which is the capacity of the buffer
    qore_sb_binary_free_check(this, capacity());
    if (ptr) {
        free(ptr);
        if (len) {
            len = 0;
        }
        if (cap_code) {
            cap_code = 0;
        }
        ptr = nullptr;
    } else {
        assert(!len);
        assert(!cap_code);
    }
}

//! the internal operations of BinaryNode that its public header does not declare (qore#4380)
class qore_binary_node_private {
public:
    //! ensures that the buffer of \a b can hold \a new_len bytes; 0 if OK, -1 if out of memory (the node unchanged)
    DLLLOCAL static int reserve(BinaryNode& b, size_t new_len);
};

int qore_binary_node_private::reserve(BinaryNode& b, size_t new_len) {
    size_t old_cap = b.capacity();
    if (new_len <= old_cap) {
        return 0;
    }
    // the first data of an empty value is allocated with its exact size (a value made with one append does not keep
    // unused memory); a value that grows again grows geometrically by factors of 1.14 to 1.25, so a series of appends
    // reallocates the buffer a logarithmic number of times (issue #6312: an allocator whose realloc() copies made it
    // quadratic)
    unsigned code = b.len ? qore_binary_capacity_code(new_len) : 0;
    size_t new_cap = code ? qore_binary_code_capacity(code) : new_len;
    void* np = realloc(b.ptr, new_cap);
    if (!np && code) {
        // the exact size may still be available
        code = 0;
        new_cap = new_len;
        np = realloc(b.ptr, new_cap);
    }
    if (!np) {
        // the old buffer is unchanged
        return -1;
    }
    b.ptr = np;
    b.cap_code = static_cast<unsigned char>(code);
    qore_sb_binary_resize_check(&b, old_cap, new_cap);
    return 0;
}

void BinaryNode::trim() {
    if (!cap_code) {
        return;
    }
    size_t old_cap = capacity();
    if (!len) {
        free(ptr);
        ptr = nullptr;
        cap_code = 0;
        qore_sb_binary_free_check(this, old_cap);
        return;
    }
    void* np = realloc(ptr, len);
    if (!np) {
        // the value keeps its buffer
        return;
    }
    ptr = np;
    cap_code = 0;
    qore_sb_binary_resize_check(this, old_cap, len);
}

// returns 0 = equal, 1 = not equal
int BinaryNode::compare(const BinaryNode* obj) const {
    // if the sizes are not equal, then the objects can't be equal
    if (len != obj->len)
        return 1;

    // if both are zero, then they are equal
    if (!len)
        return 0;

    return memcmp(ptr, obj->ptr, len);
}

size_t BinaryNode::size() const {
    return len;
}

bool BinaryNode::empty() const {
    return !len;
}

BinaryNode* BinaryNode::copy() const {
    if (!len)
        return new BinaryNode();

    void* np = malloc(len);
    memcpy(np, ptr, len);
    return new BinaryNode(np, len);
}

//! the data of an empty binary value: a valid address with no bytes after it
/** An empty value has no buffer, and a null data pointer is taken for a missing value by the C APIs of the database
    drivers, which bind it as SQL NULL (PostgreSQL's PQexecParams(), sqlite3_bind_blob())
*/
static const char qore_empty_binary_data[1] = {0};

const void* BinaryNode::getPtr() const {
    return ptr ? ptr : static_cast<const void*>(qore_empty_binary_data);
}

int BinaryNode::writeTo(size_t pos, const void* nptr, size_t size) {
    // the end of the write cannot be represented: no buffer can hold it
    if (size > SIZE_MAX - pos) {
        return -1;
    }
    // data in the buffer itself is found again at its offset if the buffer moves
    const char* src = static_cast<const char*>(nptr);
    int64 own = size ? qore_binary_own_offset(ptr, len, src) : -1;
    bool self = own >= 0;
    size_t self_offset = self ? static_cast<size_t>(own) : 0;
    if ((pos + size) > len) {
        // resize buffer; the value is unchanged if it fails
        if (qore_binary_node_private::reserve(*this, pos + size)) {
            return -1;
        }
        if (self) {
            src = static_cast<const char*>(ptr) + self_offset;
        }
        // a gap between the old end and the position written becomes part of the value: it must not expose
        // uninitialized memory
        if (pos > len) {
            memset(static_cast<char*>(ptr) + len, 0, pos - len);
        }
        // the data written is part of the value
        len = pos + size;
    }
    // the data may overlap the bytes written
    if (size) {
        memmove(static_cast<char*>(ptr) + pos, src, size);
    }
    return 0;
}

void BinaryNode::append(const void* nptr, size_t size) {
    // nothing is appended; a reallocation to 0 bytes would free an empty buffer
    if (!size) {
        return;
    }
    if (size > SIZE_MAX - len) {
        return;
    }
    // data in the buffer itself is found again at its offset if the buffer moves
    const char* src = static_cast<const char*>(nptr);
    int64 own = qore_binary_own_offset(ptr, len, src);
    bool self = own >= 0;
    size_t self_offset = self ? static_cast<size_t>(own) : 0;
    if (qore_binary_node_private::reserve(*this, len + size)) {
        return;
    }
    if (self) {
        assert(self_offset + size <= len);
        src = static_cast<const char*>(ptr) + self_offset;
    }
    memcpy(static_cast<char*>(ptr) + len, src, size);
    len += size;
}

void BinaryNode::append(const BinaryNode* b) {
    append(b->ptr, b->len);
}

void BinaryNode::append(const BinaryNode &b) {
    append(b.ptr, b.len);
}

void BinaryNode::prepend(const void* nptr, size_t size) {
    // nothing is prepended; a reallocation to 0 bytes would free an empty buffer
    if (!size) {
        return;
    }
    if (size > SIZE_MAX - len) {
        return;
    }
    // data in the buffer itself is found again at its offset if the buffer moves, and is then moved forward
    const char* src = static_cast<const char*>(nptr);
    int64 own = qore_binary_own_offset(ptr, len, src);
    bool self = own >= 0;
    size_t self_offset = self ? static_cast<size_t>(own) : 0;
    if (qore_binary_node_private::reserve(*this, len + size)) {
        return;
    }
    // move memory forward
    memmove(static_cast<char*>(ptr) + size, ptr, len);
    if (self) {
        assert(self_offset + size <= len);
        src = static_cast<const char*>(ptr) + size + self_offset;
    }
    // copy new memory to beginning
    memcpy(ptr, src, size);
    len += size;
}

void* BinaryNode::giveBuffer() {
    // the buffer leaves the node, and with it its sandbox charge
    if (ptr) {
        // the charge released is the one recorded, which is the capacity of the buffer
        qore_sb_binary_free_check(this, capacity());
    }
    void* p = ptr;
    ptr = nullptr;
    len = 0;
    cap_code = 0;
    return p;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly
QoreString* BinaryNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString *rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

int BinaryNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    if (foff == FMT_YAML_SHORT) {
        QoreString bstr(this, -1);
        str.sprintf("!!binary %s", (bstr.empty() ? "" : bstr.c_str()));
    } else {
        str.sprintf("binary object %p (" QLLD " byte%s)", getPtr(), (int64)(size()), size() == 1 ? "" : "s");
    }
    return 0;
}

AbstractQoreNode* BinaryNode::realCopy() const {
    return copy();
}

// performs a lexical compare, return -1, 0, or 1 if the "this" value is less than, equal, or greater than
// the "val" passed
//DLLLOCAL virtual int compare(const AbstractQoreNode* val) const;
// the type passed must always be equal to the current type
bool BinaryNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink *xsink) const {
    const BinaryNode* b = dynamic_cast<const BinaryNode*>(v);
    if (!b)
        return false;
    return !compare(b);
}

bool BinaryNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink *xsink) const {
    return is_equal_soft(v, xsink);
}

// returns the type name as a c string
const char *BinaryNode::getTypeName() const {
    return getStaticTypeName();
}

int BinaryNode::preallocate(size_t size) {
    //printd(5, "BinaryNode::preallocate(%zu) this: %p ptr: %p len: %zu\n", size, this, ptr, len);
    size_t old_cap = capacity();
    // the buffer is allocated with the size given
    cap_code = 0;
    ptr = q_realloc(ptr, size);
    if (ptr) {
        len = size;
        qore_sb_binary_resize_check(this, old_cap, size);
        return 0;
    }
    qore_sb_binary_free_check(this, old_cap);
    len = 0;
    return -1;
}

int BinaryNode::setSize(size_t size) {
    if (size > len)
        return -1;

    len = size;
    return 0;
}

int BinaryNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = binaryTypeInfo;
    return 0;
}

bool BinaryNode::getAsBoolImpl() const {
    // check if we should do perl-style boolean evaluation
    if (qore_get_native_semantic_policy().strictBoolean())
        return false;
    return !empty();
}

void BinaryNode::checkOffset(qore_offset_t& offset) const {
    if (offset < 0) {
        offset = len + offset;
        if (offset < 0)
            offset = 0;
        return;
    }
    if ((size_t)offset > len)
        offset = len;
    return;
}

void BinaryNode::checkOffset(qore_offset_t& offset, qore_offset_t& num) const {
    checkOffset(offset);

    if (num < 0) {
        num = len + num - offset;
        if (num < 0)
            num = 0;
        return;
    }
}

void BinaryNode::splice(qore_offset_t offset, qore_offset_t length, BinaryNode* extract) {
    checkOffset(offset, length);
    //printd(5, "BinaryNode::splice(offset=" QSD ", length=" QSD ", priv->len=" QSD ")\n", offset, length, len);

    if (offset == (qore_offset_t)len || !length)
        return;

    size_t end;
    if (length > (qore_offset_t)(len - offset)) {
        end = len;
        length = len - offset;
    }
    else
        end = offset + length;

    // add to extract string if any
    if (extract && length)
        extract->append((char*)ptr + offset, length);

    // move down entries if necessary
    if (end != len)
        memmove((char*)ptr + offset, (char*)ptr + end, len - end);

    // calculate new length
    len -= length;
}

void BinaryNode::splice(qore_offset_t offset, qore_offset_t length, const void* data, size_t data_len, BinaryNode* extract) {
    //printd(5, "BinaryNode::splice() before offset: %lld length: %lld (len: %lld data_len: %lld)\n", offset, length, len, data_len);
    checkOffset(offset, length);

    if (offset == (qore_offset_t)len) {
        if (!data_len)
            return;
        length = 0;
    }

    //printd(5, "BinaryNode::splice(offset=" QSD ", length=" QSD ", priv->len=" QSD ")\n", offset, length, len);

    size_t end;
    if (length > (qore_offset_t)(len - offset)) {
        end = len;
        length = len - offset;
    }
    else
        end = offset + length;

    // data in the buffer itself is moved by the changes below: it is copied first, before anything is changed
    std::unique_ptr<char, decltype(&free)> self_copy(nullptr, &free);
    if (data_len && qore_binary_own_offset(ptr, len, data) >= 0) {
        self_copy.reset(static_cast<char*>(malloc(data_len)));
        if (!self_copy) {
            return;
        }
        memcpy(self_copy.get(), data, data_len);
        data = self_copy.get();
    }

    // add to extract string if any
    if (extract && length)
        extract->append((char*)ptr + offset, length);

    // get number of entries to insert
    if ((qore_offset_t)data_len > length) { // make bigger
        size_t ol = len;

        // resize buffer
        if (qore_binary_node_private::reserve(*this, len - length + data_len)) {
            return;
        }

        // move trailing entries forward if necessary
        if (end != ol)
            memmove((char*)ptr + (end - length + data_len), (char*)ptr + end, ol - end);
    }
    else if (length > (qore_offset_t)data_len) // make smaller
        memmove((char*)ptr + offset + data_len, (char*)ptr + offset + length, len - offset - length);

    memcpy((char*)ptr + offset, data, data_len);

    // calculate new length
    len = len - length + data_len;
}

int BinaryNode::substr(BinaryNode& b, qore_offset_t offset) const {
    printd(5, "BinaryNode::substr(offset: " QLLD ") this: %p len: " QLLD ")\n", (int64)offset, this, (int64)len);

    checkOffset(offset);
    if (offset == (qore_offset_t)len)
        return -1;

    b.append((char*)ptr + offset, len - offset);
    return 0;
}

int BinaryNode::substr(BinaryNode& b, qore_offset_t offset, qore_offset_t length) const {
    printd(5, "BinaryNode::substr(offset: " QLLD ", length: " QLLD ") this: %p len: " QLLD "\n", (int64)offset, (int64)length, this, (int64)len);

    checkOffset(offset, length);

    if (offset == (qore_offset_t)len)
        return -1;

    if (length > (qore_offset_t)(len - offset))
        length = len - offset;

    b.append((char*)ptr + offset, length);
    return 0;
}

BinaryNode* BinaryNode::binRefSelf() const {
    ref();
    return const_cast<BinaryNode*>(this);
}