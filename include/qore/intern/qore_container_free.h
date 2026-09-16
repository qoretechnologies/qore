/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_container_free.h

    Qore Programming Language

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QORE_CONTAINER_FREE_H
#define _QORE_INTERN_QORE_CONTAINER_FREE_H

//! The nesting depth of lists and hashes whose entries are freed recursively
/** A thread's stack may be much smaller than the data it frees, so the entries of more deeply nested lists and
    hashes are freed by a loop with an explicit stack.
*/
#define QORE_CONTAINER_FREE_RECURSION_DEPTH 16

//! Frees the entries of a list or hash whose reference count has reached zero
/** QoreListNode::derefImpl() and QoreHashNode::derefImpl() free their entries themselves while the nesting depth
    of the lists and hashes being freed in the thread is below QORE_CONTAINER_FREE_RECURSION_DEPTH.  A deeper
    container is freed with its nested lists and hashes by a loop, which frees the entries in the same depth-first
    order as the recursion, so that objects are destroyed in the same order.
*/
class qore_container_free_helper {
public:
    //! Frees the entries of the container unless the caller has to free them recursively
    /** @param container a list or hash whose reference count has reached zero
        @param xsink for exceptions raised while freeing the entries
    */
    DLLLOCAL qore_container_free_helper(AbstractQoreNode* container, ExceptionSink* xsink);

    DLLLOCAL ~qore_container_free_helper();

    qore_container_free_helper(const qore_container_free_helper&) = delete;
    qore_container_free_helper& operator=(const qore_container_free_helper&) = delete;

    //! Returns true if the caller has to free the entries of the container and finish freeing it
    DLLLOCAL bool freeEntries() const {
        return recursive;
    }

    //! Frees an entry of a list or hash that is being freed
    /** A list or hash entry whose reference count reaches zero is freed by the loop that frees its container, if
        any.
    */
    DLLLOCAL static void freeEntry(QoreValue& entry, ExceptionSink* xsink);

private:
    bool recursive;
};

#endif
