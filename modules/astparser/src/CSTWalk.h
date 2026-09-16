/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  CSTWalk.h

  Qore AST Parser — tree-sitter backend

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

#ifndef _QLS_CSTWALK_H
#define _QLS_CSTWALK_H

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include <qore/Qore.h>

#include <tree_sitter/api.h>

class AstParseResult;

//! Checks for cancellation in the loops of a syntax tree operation
/** The check also ends the operation after any exception has been raised, so once a check fails, every enclosing
    loop that checks with the same object ends in turn.
*/
class CSTCancelCheck {
public:
    //! The number of iterations between calls to qore_check_cancel()
    static constexpr unsigned Interval = 100;

    DLLLOCAL CSTCancelCheck(ExceptionSink* xsink, const char* operation) : xsink(xsink), operation(operation) {
        assert(xsink);
    }

    CSTCancelCheck(const CSTCancelCheck&) = delete;
    CSTCancelCheck& operator=(const CSTCancelCheck&) = delete;

    //! Returns true if the operation must end because an exception has been raised
    DLLLOCAL bool operator()() {
        if (*xsink) {
            return true;
        }
        return !(++count % Interval) && qore_check_cancel(xsink, operation);
    }

    //! Returns true if the operation has ended because an exception has been raised
    DLLLOCAL bool failed() const {
        return static_cast<bool>(*xsink);
    }

    //! Returns the exception sink of the operation
    DLLLOCAL ExceptionSink* getSink() const {
        return xsink;
    }

private:
    ExceptionSink* xsink;
    const char* operation;
    unsigned count = 0;
};

//! What a tree walk does after entering a node
enum class CSTWalkAction {
    //! Walk the node's children
    Descend,
    //! Continue with the node's next sibling
    Skip,
    //! End the walk
    Stop,
};

//! Owns a tree-sitter tree cursor
class CSTCursorHolder {
public:
    explicit CSTCursorHolder(TSNode node) : cursor(ts_tree_cursor_new(node)) {
    }

    ~CSTCursorHolder() {
        ts_tree_cursor_delete(&cursor);
    }

    CSTCursorHolder(const CSTCursorHolder&) = delete;
    CSTCursorHolder& operator=(const CSTCursorHolder&) = delete;

    TSTreeCursor* get() {
        return &cursor;
    }

private:
    TSTreeCursor cursor;
};

//! Walks a node and its visible descendants in document order
/** A syntax tree is as deep as the nesting in its source, so a walk must not use the call stack for each level.

    @param root the node where the walk starts, at depth 0
    @param cancel the cancellation check of the operation, made for each node
    @param enter called as <tt>CSTWalkAction enter(const TSTreeCursor* cursor, TSNode node, uint32_t depth)</tt>
    before the node's children; the cursor is positioned on the node, which gives its field name
    @param leave called as <tt>void leave(TSNode node, uint32_t depth)</tt> after the node's children, including for
    a node whose children were skipped; not called for the node that stops the walk or for its ancestors

    @return true if the walk ended normally, false if \c enter or the cancellation check stopped it
*/
template <typename Enter, typename Leave>
bool cst_walk(TSNode root, CSTCancelCheck& cancel, Enter&& enter, Leave&& leave) {
    CSTCursorHolder holder(root);
    TSTreeCursor* cursor = holder.get();
    uint32_t depth = 0;
    while (true) {
        if (cancel()) {
            return false;
        }
        TSNode node = ts_tree_cursor_current_node(cursor);
        CSTWalkAction action = enter(static_cast<const TSTreeCursor*>(cursor), node, depth);
        if (action == CSTWalkAction::Stop) {
            return false;
        }
        if (action == CSTWalkAction::Descend && ts_tree_cursor_goto_first_child(cursor)) {
            ++depth;
            continue;
        }
        leave(node, depth);
        // move to the next sibling of the node or of its nearest ancestor that has one
        while (!depth || !ts_tree_cursor_goto_next_sibling(cursor)) {
            if (!depth) {
                return true;
            }
            ts_tree_cursor_goto_parent(cursor);
            --depth;
            leave(ts_tree_cursor_current_node(cursor), depth);
        }
    }
}

//! Walks a node and its visible descendants in document order without a leave callback
template <typename Enter>
bool cst_walk(TSNode root, CSTCancelCheck& cancel, Enter&& enter) {
    return cst_walk(root, cancel, enter, [](TSNode, uint32_t) {});
}

//! Calls a function for each visible child of a node in order
/** ts_node_child() and ts_node_named_child() iterate over the preceding children for each call, so a loop over the
    children by index takes time quadratic in their number; this function takes linear time.

    @param node the parent node
    @param named true to only call the function for named children
    @param cancel the cancellation check of the operation, made for each child
    @param f called as <tt>bool f(TSNode child)</tt>; returns false to end the iteration

    @return true if every child was processed, false if \c f or the cancellation check ended the iteration
*/
template <typename F>
bool cst_for_each_child(TSNode node, bool named, CSTCancelCheck& cancel, F&& f) {
    CSTCursorHolder holder(node);
    TSTreeCursor* cursor = holder.get();
    if (!ts_tree_cursor_goto_first_child(cursor)) {
        return true;
    }
    do {
        if (cancel()) {
            return false;
        }
        TSNode child = ts_tree_cursor_current_node(cursor);
        if ((!named || ts_node_is_named(child)) && !f(child)) {
            return false;
        }
    } while (ts_tree_cursor_goto_next_sibling(cursor));
    return true;
}

//! Returns the first named child of a node with the given type, or a null node
DLLLOCAL TSNode cst_find_named_child(TSNode node, const char* type, CSTCancelCheck& cancel);

//! Returns true if a node is a comment
DLLLOCAL bool cst_is_comment(const char* type);

//! Finds the doc comment of each child of a node while its children are visited in order
/** A doc comment is the run of \c /\** and \c #! comments immediately before a declaration, without blank lines
    between them or before the declaration.  Siblings cannot be visited backwards in constant time, so the comments
    that precede the next child are recorded as the children are visited.
*/
class CSTDocComments {
public:
    //! Records the next child of the node, in order, and returns true if it is a comment
    /** The doc comment of a child that is not a comment is available from get() until the next call.
    */
    DLLLOCAL bool add(TSNode child);

    //! Returns the doc comment of the last child added
    DLLLOCAL std::string get(const AstParseResult* result) const;

    //! Forgets the recorded comments, when the children of another node are visited
    DLLLOCAL void reset() {
        comments.clear();
        preceding.clear();
    }

private:
    //! the comments immediately before the next child
    std::vector<TSNode> comments;
    //! the comments immediately before the last child added
    std::vector<TSNode> preceding;
    //! the last child added
    TSNode last = {};
};

#endif
