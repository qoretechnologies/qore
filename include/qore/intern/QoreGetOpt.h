/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreGetOpt.h

  Qore Programming Language

  Copyright (C) 2003 - 2023 David Nichols

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

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#ifndef _QORE_QOREGETOPT_H

#define _QORE_QOREGETOPT_H

#include <qore/common.h>
#include <qore/safe_dslist>

#include <string>
#include <map>

#define QGO_ERR_DUP_SHORT_OPT -1
#define QGO_ERR_DUP_LONG_OPT  -2
#define QGO_ERR_DUP_NAME      -3
#define QGO_ERR_NO_NAME       -4
#define QGO_ERR_NO_OPTION     -5

#define QGO_OPT_NONE      0
#define QGO_OPT_ADDITIVE  1
#define QGO_OPT_LIST      2
#define QGO_OPT_MANDATORY 4

#define QGO_OPT_LIST_OR_ADD (QGO_OPT_ADDITIVE|QGO_OPT_LIST)

class QoreGetOptNode {
public:
   std::string name;
   char short_opt;
   std::string long_opt;
   qore_type_t argtype;
   int option;

   DLLLOCAL QoreGetOptNode(const char* n, char so, const char* lo, qore_type_t at = -1, int o = QGO_OPT_NONE) :
           name(n ? n : ""), short_opt(so), long_opt(lo ? lo : ""), argtype(at), option(o) {
   }

   DLLLOCAL ~QoreGetOptNode() {
   }
};

typedef std::map<const char*, QoreGetOptNode*, ltstr> getopt_long_map_t;
typedef std::map<char, QoreGetOptNode*, ltchar> getopt_short_map_t;
typedef safe_dslist<QoreGetOptNode*> getopt_node_list_t;

class QoreGetOpt {
private:
   getopt_long_map_t long_map;
   getopt_short_map_t short_map;
   getopt_node_list_t node_list;

   DLLLOCAL static DateTimeNode* parseDate(const char* val);
   DLLLOCAL void processLongArg(const char* arg, QoreListNode* l, QoreHashNode* h, unsigned& i, bool modify);
   DLLLOCAL int processShortArg(const char* arg, QoreListNode* l, QoreHashNode* h, unsigned& i, int& j, bool modify);
   DLLLOCAL QoreGetOptNode* find(const char* opt) const;
   DLLLOCAL QoreGetOptNode* find(char opt) const;
   DLLLOCAL void doOption(QoreGetOptNode* n, QoreHashNode* h, const char* val);
   DLLLOCAL bool getNextArgument(class QoreListNode* l, QoreHashNode* h, unsigned& i, const char* lopt,
      char sopt, std::string& arg);

public:
   DLLLOCAL QoreGetOpt() {
   }

   DLLLOCAL ~QoreGetOpt();
   // returns 0 for OK
   DLLLOCAL int add(const char* name, char short_opt, const char* long_opt, qore_type_t argtype = -1, int option = QGO_OPT_NONE);
   DLLLOCAL QoreHashNode* parse(QoreListNode* l, bool ml, ExceptionSink* xsink);
};

#endif // _QORE_QOREGETOPT_H
