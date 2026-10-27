/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_AstTreeSearcher.h

  Qore AST Parser

  Copyright (C) 2023 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QLS_QC_ASTTREESEARCHER_H
#define _QLS_QC_ASTTREESEARCHER_H

DLLEXPORT extern qore_classid_t CID_ASTTREESEARCHER;
DLLEXPORT extern QoreClass *QC_ASTTREESEARCHER;
DLLLOCAL QoreClass* initAstTreeSearcherClass(QoreNamespace& ns);

DLLLOCAL TypedHashDecl* init_hashdecl_AstExpressionLink(QoreNamespace& ns);
DLLLOCAL extern const TypedHashDecl* hashdeclAstExpressionLink;

#endif // _QLS_QC_ASTTREESEARCHER_H
