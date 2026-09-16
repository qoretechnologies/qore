/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstTreePrinter.cpp

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

#include "AstTreePrinter.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <tree_sitter/api.h>

#include "AstParser.h"
#include "CSTWalk.h"

// ts_node_string() uses the call stack for each level of a tree, so it writes only subtrees up to this height; the
// trees of real source files are about 100 levels high, and a Qore thread has a 512KB stack by default
static constexpr uint32_t MAX_STRING_HEIGHT = 512;

//! Writes the S-expression of a subtree whose height is at most MAX_STRING_HEIGHT
static void writeNodeString(std::string& out, TSNode node) {
    char* sexp = ts_node_string(node);
    if (sexp) {
        out += sexp;
        free(sexp);
    }
}

void AstTreePrinter::printTree(std::ostream& os, AstParseResult* result) {
    if (!result) {
        os << "no tree to print out\n";
        return;
    }

    TSNode root = result->getRootNode();

    // the heights of the subtrees higher than MAX_STRING_HEIGHT, by node id; the other subtrees are written with
    // ts_node_string()
    std::unordered_map<const void*, uint32_t> highNodes;
    // the height of the tallest child of the node at each depth
    std::vector<uint32_t> childHeights;
    cst_walk(root, [&](const TSTreeCursor*, TSNode, uint32_t depth) {
        if (childHeights.size() <= depth) {
            childHeights.resize(depth + 1);
        }
        childHeights[depth] = 0;
        return CSTWalkAction::Descend;
    }, [&](TSNode node, uint32_t depth) {
        uint32_t height = childHeights[depth] + 1;
        if (height > MAX_STRING_HEIGHT) {
            highNodes[node.id] = height;
        }
        if (depth && height > childHeights[depth - 1]) {
            childHeights[depth - 1] = height;
        }
    });

    std::string out;
    if (highNodes.find(root.id) == highNodes.end()) {
        writeNodeString(out, root);
    } else {
        // Writes the S-expression of ts_node_string() without using the call stack for each level: the node API
        // cannot return a missing token of a hidden rule, so such a token is only written in the subtrees written
        // with ts_node_string()
        std::vector<bool> closeNode;
        cst_walk(root, [&](const TSTreeCursor* cursor, TSNode node, uint32_t depth) {
            if (closeNode.size() <= depth) {
                closeNode.resize(depth + 1);
            }
            closeNode[depth] = false;
            // anonymous nodes are not written, except for missing tokens
            bool visible = !depth || ts_node_is_named(node) || ts_node_is_missing(node);
            if (!visible) {
                return CSTWalkAction::Descend;
            }
            if (depth) {
                out += ' ';
                const char* field = ts_tree_cursor_current_field_name(cursor);
                if (field) {
                    out += field;
                    out += ": ";
                }
            }
            if (highNodes.find(node.id) == highNodes.end()) {
                writeNodeString(out, node);
                return CSTWalkAction::Skip;
            }
            out += '(';
            out += ts_node_type(node);
            closeNode[depth] = true;
            return CSTWalkAction::Descend;
        }, [&](TSNode, uint32_t depth) {
            if (closeNode[depth]) {
                out += ')';
            }
        });
    }

    os << out << "\n";
    os.flush();
}
