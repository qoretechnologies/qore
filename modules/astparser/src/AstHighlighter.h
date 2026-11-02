/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstHighlighter.h

  Qore AST Parser — syntax highlighting with the tree-sitter highlight query

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

#ifndef _QLS_ASTHIGHLIGHTER_H
#define _QLS_ASTHIGHLIGHTER_H

#include <cstdint>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include <tree_sitter/api.h>

class AstParseResult;
class CSTCancelCheck;

//! A highlighted range of the source: byte offsets and the index of the capture name
struct AstHighlightSpan {
    //! the byte offset of the first byte
    uint32_t start;
    //! the byte offset after the last byte
    uint32_t end;
    //! the index of the capture name, for AstHighlighter::getCaptureName()
    uint32_t capture;
};

//! A range of the source that the parser could not parse: byte offsets
struct AstErrorSpan {
    //! the byte offset of the first byte
    uint32_t start;
    //! the byte offset after the last byte; equal to start for a missing token
    uint32_t end;
    //! true for a token that the parser inserted to recover from an error
    bool missing;
};

//! Highlights parsed Qore source with the highlight query of the grammar (queries/highlights.scm)
/** The query is compiled once; the object is immutable afterwards and can be used by any number of threads.
*/
class AstHighlighter {
public:
    //! Returns the highlighter, compiling the query on the first call
    /** @param err the error if the query cannot be compiled

        @return the highlighter, or nullptr if the query cannot be compiled (the error is the same for every call)
    */
    static const AstHighlighter* get(std::string& err);

    //! Frees the query; called when the module is deleted
    static void cleanup();

    ~AstHighlighter();

    AstHighlighter(const AstHighlighter&) = delete;
    AstHighlighter& operator=(const AstHighlighter&) = delete;

    //! Returns the name of a capture
    const std::string& getCaptureName(uint32_t capture) const {
        return captureNames[capture];
    }

    //! Highlights the tree
    /** When several patterns capture the same node, the first pattern in the query wins; a capture of a node inside
        a captured node takes precedence over the outer capture.

        @param result the parse result
        @param cancel the cancellation check
        @param spans the highlighted ranges, in source order, which do not overlap; adjacent ranges have different
        captures

        @return false if the operation was cancelled
    */
    bool highlight(const AstParseResult& result, CSTCancelCheck& cancel, std::vector<AstHighlightSpan>& spans) const;

    //! Returns the ranges of the tree that could not be parsed
    /** An error node inside another error node is not reported separately.

        @return false if the operation was cancelled
    */
    static bool getErrors(const AstParseResult& result, CSTCancelCheck& cancel, std::vector<AstErrorSpan>& spans);

    //! Converts the byte offsets of UTF-8 text to character offsets
    /** @param source the UTF-8 text
        @param byteOffsets byte offsets in ascending order, which are replaced by the character offsets
        @param cancel the cancellation check

        @return false if the operation was cancelled
    */
    static bool toCharOffsets(const std::string& source, std::vector<uint32_t*>& byteOffsets,
        CSTCancelCheck& cancel);

private:
    enum class PredicateKind {
        Eq,
        AnyOf,
        Match,
    };

    //! A text predicate of a pattern: #eq?, #any-of? or #match?, or their negations
    struct Predicate {
        PredicateKind kind;
        bool negated;
        //! the capture whose text is tested
        uint32_t capture;
        //! for #eq? with a second capture: the other capture
        bool otherIsCapture = false;
        uint32_t otherCapture = 0;
        //! the strings of #eq? and #any-of?
        std::vector<std::string> values;
        //! the regular expression of #match?
        std::shared_ptr<std::regex> regex;
    };

    TSQuery* query = nullptr;
    std::vector<std::string> captureNames;
    //! the predicates of each pattern
    std::vector<std::vector<Predicate>> patternPredicates;

    AstHighlighter() = default;

    //! Compiles the query; returns false with the error if it cannot be compiled
    bool init(std::string& err);

    //! Reads the predicates of a pattern; returns false with the error if a predicate is not supported
    bool initPredicates(uint32_t pattern, std::string& err);

    //! Returns true if all predicates of the pattern of the match are satisfied
    bool matchesPredicates(const TSQueryMatch& match, const std::string& source) const;
};

#endif // _QLS_ASTHIGHLIGHTER_H
