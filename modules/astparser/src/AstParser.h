/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstParser.h

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

#ifndef _QLS_ASTPARSER_H
#define _QLS_ASTPARSER_H

#include <functional>
#include <string>
#include <vector>

#include <tree_sitter/api.h>

#include "AstParseErrorLog.h"

//! Holds the result of a tree-sitter parse: the TSTree and the source text.
class AstParseResult {
public:
    AstParseResult(TSTree* tree, std::string&& source)
        : tree(tree), source(std::move(source)) {}

    ~AstParseResult() {
        if (tree) {
            ts_tree_delete(tree);
        }
    }

    // Non-copyable
    AstParseResult(const AstParseResult&) = delete;
    AstParseResult& operator=(const AstParseResult&) = delete;

    // Movable
    AstParseResult(AstParseResult&& other) noexcept
        : tree(other.tree), source(std::move(other.source)) {
        other.tree = nullptr;
    }

    AstParseResult& operator=(AstParseResult&& other) noexcept {
        if (this != &other) {
            if (tree) {
                ts_tree_delete(tree);
            }
            tree = other.tree;
            source = std::move(other.source);
            other.tree = nullptr;
        }
        return *this;
    }

    //! Get the tree-sitter tree (owned by this object).
    TSTree* getTree() const { return tree; }

    //! Get the root node of the tree.
    TSNode getRootNode() const { return ts_tree_root_node(tree); }

    //! Get the source text.
    const std::string& getSource() const { return source; }

    //! Get a substring of the source for a given node.
    std::string getNodeText(TSNode node) const {
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        if (start <= end && end <= source.size()) {
            return source.substr(start, end - start);
        }
        return std::string();
    }

private:
    TSTree* tree;
    std::string source;
};

class AstParser : public AstParseErrorLog {
public:
    AstParser();
    ~AstParser();

    //! Parse Qore source file.
    /**
        @param filename source file name
        @return parsed tree result, or nullptr on failure
     */
    AstParseResult* parseFile(const char* filename);

    //! Parse Qore source file.
    /**
        @param filename source file name
        @return parsed tree result, or nullptr on failure
     */
    AstParseResult* parseFile(std::string& filename);

    //! Parse Qore code from string.
    /**
        @param str Qore code
        @return parsed tree result, or nullptr on failure
     */
    AstParseResult* parseString(const char* str);

    //! Parse Qore code from string.
    /**
        @param str Qore code
        @return parsed tree result, or nullptr on failure
     */
    AstParseResult* parseString(std::string& str);

    //! Enable or disable conditional parsing mode.
    /**
        When enabled, %ifdef/%ifndef/%if directives are evaluated and inactive
        branches are skipped during parsing.
     */
    void setConditionalParsing(bool enabled);

    //! Clears all predefined symbols for conditional parsing.
    void clearDefines();

    //! Adds a predefined symbol for conditional parsing.
    /**
        @param name symbol name
     */
    void addDefine(const std::string& name);

    //! Sets the predefined symbols for conditional parsing.
    /**
        @param names list of symbol names
     */
    void setDefines(const std::vector<std::string>& names);

    //! Returns true if conditional parsing is enabled.
    bool getConditionalParsing() const {
        return conditionalParsing;
    }

    //! Returns the current predefined symbols.
    const std::vector<std::string>& getDefines() const {
        return defines;
    }

private:
    //! Shared parser instance (reused across parses).
    TSParser* parser;

    bool conditionalParsing = false;
    std::vector<std::string> defines;

    //! Extract parse errors from tree-sitter ERROR/MISSING nodes.
    void collectErrors(TSNode node, const std::string& source);

    //! Preprocess source to resolve conditional compilation directives.
    /** Evaluates %ifdef/%ifndef/%if/%elif/%else/%endif directives using
        the current defines.  Inactive lines are replaced with empty lines
        to preserve line numbers.  Always runs when conditional directives
        are detected in the source.
        @param source source text to preprocess
        @return preprocessed source with inactive branches blanked out
    */
    std::string preprocessConditionals(const std::string& source) const;

    //! Check if a symbol is defined.
    bool isDefined(const std::string& name) const;

    //! Evaluate a %if/%elif condition expression.
    /** Supports: defined(X), !defined(X), &&, ||, parentheses.
        @param expr condition expression string
        @return evaluation result
    */
    bool evaluateCondition(const std::string& expr) const;

    //! Evaluate a parse-directive condition using a custom lookup function.
    /** @param expr condition expression string
        @param isDefinedFn function that returns true if a name is defined
        @return evaluation result
    */
    static bool evaluateCondition(const std::string& expr,
        const std::function<bool(const std::string&)>& isDefinedFn);
};

#endif // _QLS_ASTPARSER_H
