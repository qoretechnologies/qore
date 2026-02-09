/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstTreeHolder.cpp

  Qore AST Parser — tree-sitter backend

  Copyright (C) 2023 - 2026 Qore Technologies, s.r.o.

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

#include "AstTreeHolder.h"

#include <cstring>

#include <tree_sitter/api.h>

#include "AstParser.h"
#include "AstTreePrinter.h"
#include "queries/GetNodesInfoQuery.h"

AstTreeHolder::AstTreeHolder(AstParseResult* r) : result(r) {
}

AstTreeHolder::~AstTreeHolder() {
    delete result;
}

void AstTreeHolder::printTree(std::ostream& os) {
    if (result) {
        AstTreePrinter::printTree(os, result);
    }
}

QoreListNode* AstTreeHolder::getNodesInfo() {
    return GetNodesInfoQuery::get(result);
}

//! Comment kind enumeration (matching the old ACK_* values).
enum CommentKind {
    CK_Line = 0,       //!< Line comment: # ...
    CK_Block = 1,       //!< Block comment: /* ... */
    CK_DocLine = 2,     //!< Doc line comment: #! ...
    CK_DocBlock = 3,    //!< Doc block comment: /** ... */
};

//! Determine the comment kind from the node type and text.
static CommentKind classifyComment(const char* nodeType, const std::string& text) {
    if (strcmp(nodeType, "comment") == 0) {
        // Block comment: /* ... */
        // Check if it's a doc block comment: /** ... */
        if (text.size() >= 4 && text[0] == '/' && text[1] == '*' && text[2] == '*') {
            return CK_DocBlock;
        }
        return CK_Block;
    }
    // Line comment: # ...
    // Check if it's a doc line comment: #! ...
    if (text.size() >= 2 && text[0] == '#' && text[1] == '!') {
        return CK_DocLine;
    }
    return CK_Line;
}

//! Recursively collect all comment nodes from the tree-sitter CST.
static void collectComments(TSNode node, const AstParseResult* result,
                            QoreListNode* lst, ExceptionSink* xsink) {
    const char* type = ts_node_type(node);

    if (strcmp(type, "comment") == 0 || strcmp(type, "line_comment") == 0) {
        std::string text = result->getNodeText(node);
        CommentKind kind = classifyComment(type, text);

        TSPoint startPt = ts_node_start_point(node);
        TSPoint endPt = ts_node_end_point(node);

        ReferenceHolder<QoreHashNode> info(new QoreHashNode, xsink);
        if (*xsink) {
            return;
        }
        info->setKeyValue("kind", static_cast<int64>(kind), xsink);
        info->setKeyValue("text", new QoreStringNode(text), xsink);

        ReferenceHolder<QoreHashNode> locHash(new QoreHashNode, xsink);
        if (*xsink) {
            return;
        }
        // tree-sitter uses 0-indexed; our API uses 1-indexed
        locHash->setKeyValue("firstLine", static_cast<int64>(startPt.row + 1), xsink);
        locHash->setKeyValue("firstCol", static_cast<int64>(startPt.column + 1), xsink);
        locHash->setKeyValue("lastLine", static_cast<int64>(endPt.row + 1), xsink);
        locHash->setKeyValue("lastCol", static_cast<int64>(endPt.column + 1), xsink);
        info->setKeyValue("loc", locHash.release(), xsink);

        if (*xsink) {
            return;
        }
        lst->push(info.release(), xsink);
        if (*xsink) {
            return;
        }
        return; // Comments don't have children to recurse into
    }

    // Recurse into all children (including unnamed/extra nodes)
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        collectComments(ts_node_child(node, i), result, lst, xsink);
        if (*xsink) {
            return;
        }
    }
}

QoreListNode* AstTreeHolder::getComments() {
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
    collectComments(root, result, *lst, &xsink);

    if (xsink) {
        lst = nullptr;
        xsink.clear();
        return nullptr;
    }

    return lst.release();
}

void AstTreeHolder::set(AstParseResult* r) {
    if (result != r) {
        delete result;
        result = r;
    }
}

AstParseResult* AstTreeHolder::get() {
    return result;
}

AstParseResult* AstTreeHolder::release() {
    AstParseResult* r = result;
    result = nullptr;
    return r;
}
