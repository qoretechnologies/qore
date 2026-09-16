/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  CSTWalk.cpp

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

#include "CSTWalk.h"

#include <cstring>

#include "AstParser.h"

TSNode cst_find_named_child(TSNode node, const char* type, CSTCancelCheck& cancel) {
    TSNode found = {};
    cst_for_each_child(node, true, cancel, [&](TSNode child) {
        if (strcmp(ts_node_type(child), type)) {
            return true;
        }
        found = child;
        return false;
    });
    return found;
}

bool cst_is_comment(const char* type) {
    return !strcmp(type, "comment") || !strcmp(type, "line_comment");
}

//! Returns true if a comment is a doc comment (/** or #!)
static bool cst_is_doc_comment(const std::string& text) {
    return (text.size() >= 3 && text[0] == '/' && text[1] == '*' && text[2] == '*')
        || (text.size() >= 2 && text[0] == '#' && text[1] == '!');
}

bool CSTDocComments::add(TSNode child) {
    if (cst_is_comment(ts_node_type(child))) {
        comments.push_back(child);
        return true;
    }
    preceding.swap(comments);
    comments.clear();
    last = child;
    return false;
}

std::string CSTDocComments::get(const AstParseResult* result) const {
    if (preceding.empty()) {
        return std::string();
    }

    // Collect the consecutive doc comments immediately before the child, from the last one backwards; a blank line
    // or a comment that is not a doc comment ends the doc comment
    uint32_t nextStartLine = ts_node_start_point(last).row;
    size_t first = preceding.size();
    while (first) {
        TSNode comment = preceding[first - 1];
        if (!cst_is_doc_comment(result->getNodeText(comment))
            || nextStartLine > ts_node_end_point(comment).row + 1) {
            break;
        }
        nextStartLine = ts_node_start_point(comment).row;
        --first;
    }

    std::string docComment;
    for (size_t i = first; i < preceding.size(); ++i) {
        if (i > first) {
            docComment += "\n";
        }
        docComment += result->getNodeText(preceding[i]);
    }
    return docComment;
}
