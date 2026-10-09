/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreDeferredRelease.h

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

#ifndef _QORE_INTERN_QOREDEFERREDRELEASE_H
#define _QORE_INTERN_QOREDEFERREDRELEASE_H

#include <qore/node_types.h>

class AbstractQoreNode;
class AbstractStatement;
class ExceptionSink;

//! The number of nested releases that a thread makes on its stack before it defers the deeper ones
/** Releasing the last reference to a container or a parse node releases what it holds, which releases what that
    holds, and so on: without a bound, freeing data or code nested deeply recurses once per level and overflows the
    stack of a thread whose stack is smaller than the nesting.  Releases nested more deeply than this are queued and
    made iteratively by the outermost release before it returns.  See design/deferred-release.md.
*/
constexpr unsigned QORE_RELEASE_RECURSION_DEPTH = 16;

//! Returns true if nodes of the given type are code: parse and expression nodes
/** Releases of code are deferred beyond QORE_RELEASE_RECURSION_DEPTH.  Runtime values are not: lists and hashes
    bound their own recursion in depth-first order (qore_container_free_helper), as objects do
    (qore_object_private::deleteOrDefer()), and the others (references, call references, closures, weak references,
    numbers, enums, buffers and plugin values) are released as they always were, so that the objects they hold are
    destroyed in the same order and at the same point.  See design/deferred-release.md.
*/
DLLLOCAL constexpr bool qore_is_code_node_type(qore_type_t t) {
    switch (t) {
        case NT_CONTEXTREF:
        case NT_COMPLEXCONTEXTREF:
        case NT_VARREF:
        case NT_TREE:
        case NT_FIND:
        case NT_FUNCTION_CALL:
        case NT_SELF_VARREF:
        case NT_SCOPE_REF:
        case NT_CONSTANT:
        case NT_BAREWORD:
        case NT_CONTEXT_ROW:
        case NT_CLASSREF:
        case NT_OBJMETHREF:
        case NT_FUNCREFCALL:
        case NT_CLOSURE:
        case NT_IMPLICIT_ARG:
        case NT_METHOD_CALL:
        case NT_STATIC_METHOD_CALL:
        case NT_SELF_CALL:
        case NT_OPERATOR:
        case NT_IMPLICIT_ELEMENT:
        case NT_CLASS_VARREF:
        case NT_PROGRAM_FUNC_CALL:
        case NT_PARSEREFERENCE:
        case NT_BACKQUOTE:
        case NT_RTCONSTREF:
        case NT_PARSE_HASH:
        case NT_PARSE_LIST:
        case NT_PARSE_NEW_COMPLEX_TYPE:
        case NT_NEW_HASHDECL:
        case NT_ELLIPSES:
        case NT_NEW_OBJECT:
            return true;
        default:
            return false;
    }
}

//! Releases a node whose last reference has been released: calls derefImpl() and deletes the node if it returns true
/** Called by AbstractQoreNode::deref(ExceptionSink*) for a code node (qore_is_code_node_type()).  A release nested
    more than QORE_RELEASE_RECURSION_DEPTH levels deep is deferred to the outermost release of the thread, which makes
    it with its own exception sink before it returns.
*/
DLLLOCAL void qore_release_node(AbstractQoreNode* node, ExceptionSink* xsink);

//! Deletes a node whose last reference has been released and that has no derefImpl() to call
/** Called by SimpleQoreNode::deref() for a code node (qore_is_code_node_type()); deferred like
    qore_release_node().
*/
DLLLOCAL void qore_release_simple_node(AbstractQoreNode* node);

//! Deletes a statement, which deletes the statements it holds; deferred like qore_release_node()
/** Statements are not reference-counted nodes, but they nest with the code and delete their nested statements when
    they are deleted, so a block nested deeply recurses like nested data.

    @param statement the statement to delete; may be nullptr
*/
DLLLOCAL void qore_delete_statement(AbstractStatement* statement);

#endif // _QORE_INTERN_QOREDEFERREDRELEASE_H
