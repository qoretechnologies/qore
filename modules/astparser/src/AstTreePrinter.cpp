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

#include <tree_sitter/api.h>

#include "AstParser.h"

void AstTreePrinter::printTree(std::ostream& os, AstParseResult* result) {
    if (!result) {
        os << "no tree to print out\n";
        return;
    }

    char* sexp = ts_node_string(result->getRootNode());
    if (sexp) {
        os << sexp << "\n";
        free(sexp);
    }
    os.flush();
}
