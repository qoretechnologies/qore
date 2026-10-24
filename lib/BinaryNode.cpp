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

#include <cstdlib>
#include <cstring>

BinaryNode::BinaryNode(void* p, size_t size) : SimpleValueQoreNode(NT_BINARY) {
    ptr = p;
    len = size;
}

BinaryNode::~BinaryNode() {
    if (ptr) {
        free(ptr);
    }
}

void BinaryNode::clear() {
    // issue #2982: must check 'ptr', len may be 0 with memory allocated
    // NOTE: we check and then free & update to avoid writing to memory
    // to avoid cache flushing on SMP systems (as used in the Linux
    // kernel for example)
    if (ptr) {
        free(ptr);
        if (len) {
            len = 0;
        }
        ptr = nullptr;
    }
    else {
        assert(!len);
    }
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

const void* BinaryNode::getPtr() const {
    return ptr;
}

int BinaryNode::writeTo(size_t pos, const void* nptr, size_t size) {
    if ((pos + size) > len) {
        // resize buffer
        ptr = q_realloc(ptr, pos + size);
        if (!ptr) {
            return -1;
        }
    }
    memcpy((char*)ptr + pos, nptr, size);
    return 0;
}

void BinaryNode::append(const void* nptr, size_t size) {
    // nothing is appended; a reallocation to 0 bytes would free an empty buffer
    if (!size) {
        return;
    }
    bool self_copy = nptr == ptr;
    ptr = realloc(ptr, len + size);
    if (self_copy) {
        assert(size == len);
        nptr = ptr;
    }
    memcpy((char*)ptr + len, nptr, size);
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
    ptr = realloc(ptr, len + size);
    // move memory forward
    memmove((char*)ptr + size, ptr, len);
    // copy new memory to beginning
    memcpy((char*)ptr, nptr, size);
    len += size;
}

void* BinaryNode::giveBuffer() {
    void* p = ptr;
    ptr = nullptr;
    len = 0;
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
        str.sprintf("binary object %p (" QSD " byte%s)", getPtr(), size(), size() == 1 ? "" : "s");
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
    ptr = q_realloc(ptr, size);
    if (ptr) {
        len = size;
        return 0;
    }
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
    if (runtime_check_parse_option(PO_STRICT_BOOLEAN_EVAL))
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

    // add to extract string if any
    if (extract && length)
        extract->append((char*)ptr + offset, length);

    // get number of entries to insert
    if ((qore_offset_t)data_len > length) { // make bigger
        size_t ol = len;

        // resize buffer
        ptr = q_realloc(ptr, len - length + data_len);

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
    printd(5, "BinaryNode::substr(offset: " QSD ") this: %p len: " QSD ")\n", offset, this, len);

    checkOffset(offset);
    if (offset == (qore_offset_t)len)
        return -1;

    b.append((char*)ptr + offset, len - offset);
    return 0;
}

int BinaryNode::substr(BinaryNode& b, qore_offset_t offset, qore_offset_t length) const {
    printd(5, "BinaryNode::substr(offset: " QSD ", length: " QSD ") this: %p len: " QSD "\n", offset, length, this, len);

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