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

#include <cstdint>

#include <tree_sitter/api.h>

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
    @param enter called as <tt>CSTWalkAction enter(const TSTreeCursor* cursor, TSNode node, uint32_t depth)</tt>
    before the node's children; the cursor is positioned on the node, which gives its field name
    @param leave called as <tt>void leave(TSNode node, uint32_t depth)</tt> after the node's children, including for
    a node whose children were skipped; not called for the node that stops the walk or for its ancestors

    @return true if the walk ended normally, false if \c enter stopped it
*/
template <typename Enter, typename Leave>
bool cst_walk(TSNode root, Enter&& enter, Leave&& leave) {
    CSTCursorHolder holder(root);
    TSTreeCursor* cursor = holder.get();
    uint32_t depth = 0;
    while (true) {
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
bool cst_walk(TSNode root, Enter&& enter) {
    return cst_walk(root, enter, [](TSNode, uint32_t) {});
}

#endif
