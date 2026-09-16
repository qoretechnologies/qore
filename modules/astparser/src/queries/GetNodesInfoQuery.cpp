/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  GetNodesInfoQuery.cpp

  Qore AST Parser — tree-sitter backend

  Copyright (C) 2017 - 2026 Qore Technologies, s.r.o.

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

#include "queries/GetNodesInfoQuery.h"

#include <cstring>
#include <vector>

#include <tree_sitter/api.h>

#include "qore/Qore.h"

#include "AstParser.h"
#include "CSTWalk.h"

//! Create a location hash from a tree-sitter node.
/** Location uses 0-indexed line/column (LSP convention).
    Tree-sitter uses 0-indexed internally.
*/
static QoreHashNode* getLocation(TSNode node, ExceptionSink* xsink) {
    TSPoint startPt = ts_node_start_point(node);
    TSPoint endPt = ts_node_end_point(node);

    ReferenceHolder<QoreHashNode> loc(new QoreHashNode, xsink);
    if (*xsink) {
        return nullptr;
    }

    loc->setKeyValue("start_line", static_cast<int64>(startPt.row), xsink);
    loc->setKeyValue("start_column", static_cast<int64>(startPt.column), xsink);
    loc->setKeyValue("end_line", static_cast<int64>(endPt.row), xsink);
    loc->setKeyValue("end_column", static_cast<int64>(endPt.column), xsink);
    if (*xsink) {
        return nullptr;
    }
    return loc.release();
}

static bool isComment(const char* type) {
    return !strcmp(type, "comment") || !strcmp(type, "line_comment");
}

QoreListNode* GetNodesInfoQuery::get(AstParseResult* result, CSTCancelCheck& cancel) {
    if (!result) {
        return nullptr;
    }

    ExceptionSink* xsink = cancel.getSink();
    ReferenceHolder<QoreListNode> lst(new QoreListNode, xsink);

    // lists[depth] receives the hashes of the named children of the node at that depth; the list of each node is
    // owned by the node's hash, which is owned by its parent's list
    std::vector<QoreListNode*> lists;
    bool ok = cst_walk(result->getRootNode(), cancel, [&](const TSTreeCursor* cursor, TSNode node, uint32_t depth) {
        if (!depth) {
            // the root node itself is not included; its named children are the top-level list
            lists.assign(1, *lst);
            return CSTWalkAction::Descend;
        }

        const char* type = ts_node_type(node);
        // Skip anonymous nodes and comment nodes (comments are handled separately by getComments)
        if (!ts_node_is_named(node) || isComment(type)) {
            return CSTWalkAction::Skip;
        }

        ReferenceHolder<QoreHashNode> info(new QoreHashNode, xsink);
        info->setKeyValue("type", new QoreStringNode(type), xsink);
        info->setKeyValue("loc", getLocation(node, xsink), xsink);
        if (*xsink) {
            return CSTWalkAction::Stop;
        }

        // For leaf/terminal nodes, include the text; for other nodes, include the children
        CSTWalkAction action;
        if (!ts_node_named_child_count(node)) {
            info->setKeyValue("text", new QoreStringNode(result->getNodeText(node)), xsink);
            action = CSTWalkAction::Skip;
        } else {
            QoreListNode* children = new QoreListNode;
            info->setKeyValue("children", children, xsink);
            if (lists.size() <= depth) {
                lists.resize(depth + 1);
            }
            lists[depth] = children;
            action = CSTWalkAction::Descend;
        }
        if (*xsink) {
            return CSTWalkAction::Stop;
        }

        // Include the field name if available; top-level nodes have no field
        if (depth > 1) {
            const char* fieldName = ts_tree_cursor_current_field_name(cursor);
            if (fieldName) {
                info->setKeyValue("field", new QoreStringNode(fieldName), xsink);
                if (*xsink) {
                    return CSTWalkAction::Stop;
                }
            }
        }

        lists[depth - 1]->push(info.release(), xsink);
        return *xsink ? CSTWalkAction::Stop : action;
    });

    if (!ok || *xsink) {
        return nullptr;
    }
    return lst.release();
}
