/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstParser.h

  Qore AST Parser

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

#include <string>
#include <vector>

#include "AstParseErrorLog.h"

class ASTTree;

class AstParser : public AstParseErrorLog {
public:
    AstParser() {}
    ~AstParser() {}

    //! Parse Qore source file.
    /**
        @param filename source file name
        @return parsed AST tree
     */
    ASTTree* parseFile(const char* filename);

    //! Parse Qore source file.
    /**
        @param filename source file name
        @return parsed AST tree
     */
    ASTTree* parseFile(std::string& filename);

    //! Parse Qore code from string.
    /**
        @param str Qore code
        @return parsed AST tree
     */
    ASTTree* parseString(const char* str);

    //! Parse Qore code from string.
    /**
        @param str Qore code
        @return parsed AST tree
     */
    ASTTree* parseString(std::string& str);

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
    bool conditionalParsing = false;
    std::vector<std::string> defines;
};

#endif // _QLS_ASTPARSER_H
