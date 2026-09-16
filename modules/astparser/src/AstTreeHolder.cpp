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
#include "CSTWalk.h"
#include "ast/ASTComment.h"
#include "queries/GetNodesInfoQuery.h"

AstTreeHolder::AstTreeHolder(AstParseResult* r) : result(r) {
}

AstTreeHolder::~AstTreeHolder() {
    delete result;
}

void AstTreeHolder::printTree(std::ostream& os, ExceptionSink* xsink) {
    if (result) {
        CSTCancelCheck cancel(xsink, "printing an astparser syntax tree");
        AstTreePrinter::printTree(os, result, cancel);
    }
}

QoreListNode* AstTreeHolder::getNodesInfo(ExceptionSink* xsink) {
    CSTCancelCheck cancel(xsink, "getting astparser node info");
    return GetNodesInfoQuery::get(result, cancel);
}

//! Determine the comment kind from the node type and text.
static ASTCommentKind classifyComment(const char* nodeType, const std::string& text) {
    if (strcmp(nodeType, "comment") == 0) {
        // Block comment: /* ... */
        // Check if it's a doc block comment: /** ... */
        if (text.size() >= 4 && text[0] == '/' && text[1] == '*' && text[2] == '*') {
            return ACK_DocBlock;
        }
        return ACK_Block;
    }
    // Line comment: # ...
    // Check if it's a doc line comment: #! ...
    if (text.size() >= 2 && text[0] == '#' && text[1] == '!') {
        return ACK_DocLine;
    }
    return ACK_Line;
}

//! Collect all comment nodes from the tree-sitter CST.
/** @return false if an exception was raised
*/
static bool collectComments(TSNode root, const AstParseResult* result, QoreListNode* lst, CSTCancelCheck& cancel) {
    ExceptionSink* xsink = cancel.getSink();
    return cst_walk(root, cancel, [&](const TSTreeCursor*, TSNode node, uint32_t) {
        const char* type = ts_node_type(node);
        if (strcmp(type, "comment") && strcmp(type, "line_comment")) {
            // Walk all children (including unnamed/extra nodes)
            return CSTWalkAction::Descend;
        }

        std::string text = result->getNodeText(node);
        ASTCommentKind kind = classifyComment(type, text);

        TSPoint startPt = ts_node_start_point(node);
        TSPoint endPt = ts_node_end_point(node);

        ReferenceHolder<QoreHashNode> info(new QoreHashNode, xsink);
        if (*xsink) {
            return CSTWalkAction::Stop;
        }
        info->setKeyValue("kind", static_cast<int64>(kind), xsink);
        info->setKeyValue("text", new QoreStringNode(text), xsink);

        ReferenceHolder<QoreHashNode> locHash(new QoreHashNode, xsink);
        if (*xsink) {
            return CSTWalkAction::Stop;
        }
        // tree-sitter uses 0-indexed; our API uses 1-indexed
        locHash->setKeyValue("firstLine", static_cast<int64>(startPt.row + 1), xsink);
        locHash->setKeyValue("firstCol", static_cast<int64>(startPt.column + 1), xsink);
        locHash->setKeyValue("lastLine", static_cast<int64>(endPt.row + 1), xsink);
        locHash->setKeyValue("lastCol", static_cast<int64>(endPt.column + 1), xsink);
        info->setKeyValue("loc", locHash.release(), xsink);
        if (*xsink) {
            return CSTWalkAction::Stop;
        }

        lst->push(info.release(), xsink);
        // Comments don't have children to walk
        return *xsink ? CSTWalkAction::Stop : CSTWalkAction::Skip;
    });
}

QoreListNode* AstTreeHolder::getComments(ExceptionSink* xsink) {
    if (!result) {
        return nullptr;
    }

    ReferenceHolder<QoreListNode> lst(new QoreListNode, xsink);
    CSTCancelCheck cancel(xsink, "getting astparser comments");
    if (!collectComments(result->getRootNode(), result, *lst, cancel) || *xsink) {
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
