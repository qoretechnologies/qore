/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  GetNodesInfoQuery.h

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

#ifndef _QLS_QUERIES_GETNODESINFOQUERY_H
#define _QLS_QUERIES_GETNODESINFOQUERY_H

class AstParseResult;

class ExceptionSink;
class QoreHashNode;
class QoreListNode;

class GetNodesInfoQuery {
public:
    GetNodesInfoQuery() = delete;
    GetNodesInfoQuery(const GetNodesInfoQuery& other) = delete;

    //! Get info about nodes in the given tree-sitter parse result.
    /**
        @param result tree-sitter parse result to query
        @return list of info about nodes
    */
    static QoreListNode* get(AstParseResult* result);
};

#endif // _QLS_QUERIES_GETNODESINFOQUERY_H
