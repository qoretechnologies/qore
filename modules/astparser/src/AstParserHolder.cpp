/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstParserHolder.cpp

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

#include "AstParserHolder.h"
#include "AstParser.h"

AstParserHolder::AstParserHolder() {
    parser = new AstParser;
}

AstParserHolder::~AstParserHolder() {
    delete parser;
}

AstParseResult* AstParserHolder::parseFile(const char* filename) {
    return parser->parseFile(filename);
}

AstParseResult* AstParserHolder::parseFile(std::string& filename) {
    return parser->parseFile(filename);
}

AstParseResult* AstParserHolder::parseString(const char* str) {
    return parser->parseString(str);
}

AstParseResult* AstParserHolder::parseString(std::string& str) {
    return parser->parseString(str);
}

void AstParserHolder::setConditionalParsing(bool enabled) {
    parser->setConditionalParsing(enabled);
}

void AstParserHolder::clearDefines() {
    parser->clearDefines();
}

void AstParserHolder::addDefine(const char* name) {
    if (name) {
        parser->addDefine(name);
    }
}

void AstParserHolder::setDefines(const QoreListNode* names) {
    parser->clearDefines();
    if (!names) {
        return;
    }
    ConstListIterator li(names);
    while (li.next()) {
        QoreStringValueHelper str(li.getValue());
        if (str->strlen()) {
            parser->addDefine(str->c_str());
        }
    }
}

size_t AstParserHolder::getErrorCount() const {
    return parser->getErrorCount();
}

ASTParseError* AstParserHolder::getError(size_t index) {
    return parser->getError(index);
}
