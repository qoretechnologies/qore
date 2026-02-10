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

#include <tree_sitter/api.h>

#include "qore/Qore.h"

#include "AstParser.h"

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

//! Recursively convert a tree-sitter CST node to a Qore hash.
static QoreHashNode* getNodeInfo(TSNode node, const AstParseResult* result,
                                 ExceptionSink* xsink) {
    if (ts_node_is_null(node)) {
        return nullptr;
    }

    const char* type = ts_node_type(node);

    // Skip comment nodes (handled separately by getComments)
    if (strcmp(type, "comment") == 0 || strcmp(type, "line_comment") == 0) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> info(new QoreHashNode, xsink);
    if (*xsink) {
        return nullptr;
    }

    info->setKeyValue("type", new QoreStringNode(type), xsink);
    info->setKeyValue("loc", getLocation(node, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    // For leaf/terminal nodes, include the text
    uint32_t childCount = ts_node_named_child_count(node);
    if (childCount == 0) {
        std::string text = result->getNodeText(node);
        info->setKeyValue("text", new QoreStringNode(text), xsink);
        if (*xsink) {
            return nullptr;
        }
    } else {
        // For non-leaf nodes, include children
        ReferenceHolder<QoreListNode> children(new QoreListNode, xsink);
        if (*xsink) {
            return nullptr;
        }

        uint32_t totalChildren = ts_node_child_count(node);
        for (uint32_t i = 0; i < totalChildren; i++) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_named(child)) {
                continue;
            }

            QoreHashNode* childInfo = getNodeInfo(child, result, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (childInfo) {
                // Include field name if available
                const char* fieldName = ts_node_field_name_for_child(node, i);
                if (fieldName) {
                    childInfo->setKeyValue("field", new QoreStringNode(fieldName), xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                }
                children->push(childInfo, xsink);
                if (*xsink) {
                    return nullptr;
                }
            }
        }
        info->setKeyValue("children", children.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    return info.release();
}

QoreListNode* GetNodesInfoQuery::get(AstParseResult* result) {
    if (!result) {
        return nullptr;
    }

    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> lst(new QoreListNode, &xsink);
    if (xsink) {
        lst = nullptr;
        xsink.clear();
        return nullptr;
    }

    TSNode root = result->getRootNode();
    uint32_t childCount = ts_node_child_count(root);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(root, i);
        if (!ts_node_is_named(child)) {
            continue;
        }

        QoreHashNode* nodeInfo = getNodeInfo(child, result, &xsink);
        if (xsink) {
            lst = nullptr;
            xsink.clear();
            return nullptr;
        }
        if (nodeInfo) {
            lst->push(nodeInfo, &xsink);
            if (xsink) {
                lst = nullptr;
                xsink.clear();
                return nullptr;
            }
        }
    }

    return lst.release();
}
