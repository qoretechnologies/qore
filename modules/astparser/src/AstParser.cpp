/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstParser.cpp

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

#include "AstParser.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>
#include <set>
#include <sstream>
#include <stack>

// The tree-sitter Qore language function, defined in the generated parser.c
extern "C" const TSLanguage* tree_sitter_qore();

AstParser::AstParser() {
    parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_qore());
}

AstParser::~AstParser() {
    if (parser) {
        ts_parser_delete(parser);
    }
}

AstParseResult* AstParser::parseFile(const char* filename) {
    if (!filename) {
        return nullptr;
    }

    // Read file into string
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Clear previous errors
    clear();

    // Preprocess conditional directives
    std::string preprocessed = preprocessConditionals(source);

    // Parse with tree-sitter
    TSTree* tree = ts_parser_parse_string(parser, nullptr,
                                          preprocessed.c_str(), static_cast<uint32_t>(preprocessed.size()));
    if (!tree) {
        return nullptr;
    }

    // Collect parse errors from ERROR/MISSING nodes
    collectErrors(ts_tree_root_node(tree), preprocessed);

    return new AstParseResult(tree, std::move(preprocessed));
}

AstParseResult* AstParser::parseFile(std::string& filename) {
    return parseFile(filename.c_str());
}

AstParseResult* AstParser::parseString(const char* str) {
    if (!str) {
        return nullptr;
    }

    std::string source(str);

    // Clear previous errors
    clear();

    // Preprocess conditional directives
    std::string preprocessed = preprocessConditionals(source);

    // Parse with tree-sitter
    TSTree* tree = ts_parser_parse_string(parser, nullptr,
                                          preprocessed.c_str(), static_cast<uint32_t>(preprocessed.size()));
    if (!tree) {
        return nullptr;
    }

    // Collect parse errors from ERROR/MISSING nodes
    collectErrors(ts_tree_root_node(tree), preprocessed);

    return new AstParseResult(tree, std::move(preprocessed));
}

AstParseResult* AstParser::parseString(std::string& str) {
    return parseString(str.c_str());
}

void AstParser::setConditionalParsing(bool enabled) {
    conditionalParsing = enabled;
}

void AstParser::clearDefines() {
    defines.clear();
}

void AstParser::addDefine(const std::string& name) {
    if (!name.empty()) {
        defines.push_back(name);
    }
}

void AstParser::setDefines(const std::vector<std::string>& names) {
    defines = names;
}

bool AstParser::isDefined(const std::string& name) const {
    return std::find(defines.begin(), defines.end(), name) != defines.end();
}

// Simple recursive-descent evaluator for %if/%elif conditions.
// Supports: defined(X), !expr, expr && expr, expr || expr, (expr)
bool AstParser::evaluateCondition(const std::string& expr) const {
    return evaluateCondition(expr, [this](const std::string& name) {
        return isDefined(name);
    });
}

// Static version with custom lookup function (avoids code duplication)
bool AstParser::evaluateCondition(const std::string& expr,
        const std::function<bool(const std::string&)>& isDefinedFn) {
    size_t pos = 0;
    size_t len = expr.size();

    auto skipWS = [&]() {
        while (pos < len && (expr[pos] == ' ' || expr[pos] == '\t')) {
            ++pos;
        }
    };

    std::function<bool()> parseOr;
    std::function<bool()> parseAnd;
    std::function<bool()> parseUnary;
    std::function<bool()> parsePrimary;

    parsePrimary = [&]() -> bool {
        skipWS();
        if (pos >= len) {
            return false;
        }

        // defined(X)
        if (expr.compare(pos, 7, "defined") == 0 && pos + 7 < len) {
            pos += 7;
            skipWS();
            if (pos < len && expr[pos] == '(') {
                ++pos;
                skipWS();
                size_t nameStart = pos;
                while (pos < len && expr[pos] != ')' && expr[pos] != ' ') {
                    ++pos;
                }
                std::string name = expr.substr(nameStart, pos - nameStart);
                skipWS();
                if (pos < len && expr[pos] == ')') {
                    ++pos;
                }
                return isDefinedFn(name);
            }
            return false;
        }

        // (expr)
        if (expr[pos] == '(') {
            ++pos;
            bool result = parseOr();
            skipWS();
            if (pos < len && expr[pos] == ')') {
                ++pos;
            }
            return result;
        }

        // Identifier (treat as defined check)
        if (std::isalpha(expr[pos]) || expr[pos] == '_') {
            size_t nameStart = pos;
            while (pos < len && (std::isalnum(expr[pos]) || expr[pos] == '_')) {
                ++pos;
            }
            return isDefinedFn(expr.substr(nameStart, pos - nameStart));
        }

        return false;
    };

    parseUnary = [&]() -> bool {
        skipWS();
        if (pos < len && expr[pos] == '!') {
            ++pos;
            return !parseUnary();
        }
        return parsePrimary();
    };

    parseAnd = [&]() -> bool {
        bool result = parseUnary();
        while (true) {
            skipWS();
            if (pos + 1 < len && expr[pos] == '&' && expr[pos + 1] == '&') {
                pos += 2;
                result = parseUnary() && result;
            } else {
                break;
            }
        }
        return result;
    };

    parseOr = [&]() -> bool {
        bool result = parseAnd();
        while (true) {
            skipWS();
            if (pos + 1 < len && expr[pos] == '|' && expr[pos + 1] == '|') {
                pos += 2;
                result = parseAnd() || result;
            } else {
                break;
            }
        }
        return result;
    };

    return parseOr();
}

std::string AstParser::preprocessConditionals(const std::string& source) const {
    std::string result;
    result.reserve(source.size());

    // Local defines from %define directives in the source
    std::set<std::string> localDefines;

    // Helper: check if a name is defined (either in parser defines or local defines)
    auto isDefinedLocal = [&](const std::string& name) -> bool {
        return isDefined(name) || localDefines.count(name) > 0;
    };

    // Local version of evaluateCondition that uses both parser defines and local defines
    auto evalCondLocal = [&](const std::string& expr) -> bool {
        return evaluateCondition(expr, isDefinedLocal);
    };

    // Track nested conditional state: each level has (active, seen_true_branch)
    struct CondState {
        bool active;       // Is this branch currently active?
        bool parentActive; // Was the parent level active?
        bool seenTrue;     // Has a true branch been seen in this if/elif/else chain?
    };
    std::stack<CondState> condStack;

    // Helper: is the current position active?
    auto isActive = [&]() -> bool {
        if (condStack.empty()) {
            return true;
        }
        return condStack.top().active && condStack.top().parentActive;
    };

    size_t pos = 0;
    while (pos < source.size()) {
        // Find end of current line
        size_t lineEnd = source.find('\n', pos);
        if (lineEnd == std::string::npos) {
            lineEnd = source.size();
        }

        std::string line = source.substr(pos, lineEnd - pos);

        // Trim leading whitespace for directive detection
        size_t firstNonSpace = line.find_first_not_of(" \t");
        std::string trimmed;
        if (firstNonSpace != std::string::npos) {
            trimmed = line.substr(firstNonSpace);
        }

        bool isDirective = false;

        if (trimmed.compare(0, 6, "%ifdef") == 0 && trimmed.size() > 6 && (trimmed[6] == ' ' || trimmed[6] == '\t')) {
            isDirective = true;
            std::string name = trimmed.substr(7);
            // Trim whitespace from name
            size_t ns = name.find_first_not_of(" \t");
            size_t ne = name.find_last_not_of(" \t\r");
            if (ns != std::string::npos) {
                name = name.substr(ns, ne - ns + 1);
            }
            bool condTrue = isActive() && isDefinedLocal(name);
            condStack.push({condTrue, isActive(), condTrue});
        } else if (trimmed.compare(0, 7, "%ifndef") == 0 && trimmed.size() > 7 && (trimmed[7] == ' ' || trimmed[7] == '\t')) {
            isDirective = true;
            std::string name = trimmed.substr(8);
            size_t ns = name.find_first_not_of(" \t");
            size_t ne = name.find_last_not_of(" \t\r");
            if (ns != std::string::npos) {
                name = name.substr(ns, ne - ns + 1);
            }
            bool condTrue = isActive() && !isDefinedLocal(name);
            condStack.push({condTrue, isActive(), condTrue});
        } else if (trimmed.compare(0, 3, "%if") == 0 && trimmed.size() > 3 && (trimmed[3] == ' ' || trimmed[3] == '\t')) {
            isDirective = true;
            std::string condExpr = trimmed.substr(4);
            // Trim trailing \r for CRLF line endings
            if (!condExpr.empty() && condExpr.back() == '\r') {
                condExpr.pop_back();
            }
            bool condTrue = isActive() && evalCondLocal(condExpr);
            condStack.push({condTrue, isActive(), condTrue});
        } else if (trimmed.compare(0, 5, "%elif") == 0 && trimmed.size() > 5 && (trimmed[5] == ' ' || trimmed[5] == '\t')) {
            isDirective = true;
            if (!condStack.empty()) {
                std::string condExpr = trimmed.substr(6);
                // Trim trailing \r for CRLF line endings
                if (!condExpr.empty() && condExpr.back() == '\r') {
                    condExpr.pop_back();
                }
                bool condTrue = condStack.top().parentActive && !condStack.top().seenTrue
                    && evalCondLocal(condExpr);
                condStack.top().active = condTrue;
                if (condTrue) {
                    condStack.top().seenTrue = true;
                }
            }
        } else if (trimmed.compare(0, 5, "%else") == 0 && (trimmed.size() == 5 || trimmed[5] == ' ' || trimmed[5] == '\t' || trimmed[5] == '\r')) {
            isDirective = true;
            if (!condStack.empty()) {
                condStack.top().active = condStack.top().parentActive && !condStack.top().seenTrue;
                condStack.top().seenTrue = true;
            }
        } else if (trimmed.compare(0, 6, "%endif") == 0 && (trimmed.size() == 6 || trimmed[6] == ' ' || trimmed[6] == '\t' || trimmed[6] == '\r')) {
            isDirective = true;
            if (!condStack.empty()) {
                condStack.pop();
            }
        } else if (trimmed.compare(0, 7, "%define") == 0 && trimmed.size() > 7 && (trimmed[7] == ' ' || trimmed[7] == '\t')) {
            isDirective = true;
            if (isActive()) {
                std::string name = trimmed.substr(8);
                size_t ns = name.find_first_not_of(" \t");
                size_t ne = name.find_last_not_of(" \t\r");
                if (ns != std::string::npos) {
                    name = name.substr(ns, ne - ns + 1);
                    localDefines.insert(name);
                }
            }
        }

        if (isDirective || !isActive()) {
            // Replace the line content with spaces (preserve column positions for error messages)
            // but keep the newline
            for (size_t i = pos; i < lineEnd && i < source.size(); ++i) {
                result.push_back(' ');
            }
        } else {
            result.append(source, pos, lineEnd - pos);
        }

        // Add the newline if present
        if (lineEnd < source.size()) {
            result.push_back('\n');
            pos = lineEnd + 1;
        } else {
            pos = lineEnd;
        }
    }

    return result;
}

void AstParser::collectErrors(TSNode node, const std::string& source) {
    // Check if this node is an error
    if (ts_node_is_error(node) || ts_node_is_missing(node)) {
        TSPoint startPt = ts_node_start_point(node);
        TSPoint endPt = ts_node_end_point(node);

        // tree-sitter uses 0-indexed lines/columns; ASTParseLocation uses 1-indexed
        ASTParseLocation loc;
        loc.firstLine = static_cast<int>(startPt.row + 1);
        loc.firstCol = static_cast<int>(startPt.column + 1);
        loc.lastLine = static_cast<int>(endPt.row + 1);
        loc.lastCol = static_cast<int>(endPt.column + 1);

        if (ts_node_is_missing(node)) {
            std::string msg = "Missing ";
            msg += ts_node_type(node);
            reportError(loc, msg.c_str());
        } else {
            // Extract a snippet of the source around the error for context
            uint32_t start = ts_node_start_byte(node);
            uint32_t end = ts_node_end_byte(node);
            if (end > start + 50) {
                end = start + 50;
            }
            std::string snippet;
            if (start < source.size()) {
                snippet = source.substr(start, end - start);
                // Replace newlines with spaces for single-line error messages
                for (char& c : snippet) {
                    if (c == '\n' || c == '\r') {
                        c = ' ';
                    }
                }
            }
            std::string msg = "Syntax error";
            if (!snippet.empty()) {
                msg += " near: " + snippet;
            }
            reportError(loc, msg.c_str());
        }
    }

    // Recurse into children
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        collectErrors(ts_node_child(node, i), source);
    }
}
