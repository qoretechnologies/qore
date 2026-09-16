/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ParserSupport.h

  parsing support functions and objects

  Qore Programming language

  Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_PARSER_SUPPORT_H

#define _QORE_PARSER_SUPPORT_H

#define HE_TAG_CONST        1
#define HE_TAG_SCOPED_CONST 2

#include <cstring>

class QoreParserLocation {
public:
   int first_line = 1;
   int last_line = 1;
   int first_col = 0;
   int last_col = 0;

   int saved_first_line = 0;
   int saved_first_col = 0;

   //! the nesting depth of the rule in the parse tree; 0 for a token
   unsigned depth = 0;

   DLLLOCAL void saveFirst() {
      //printd(0, "QoreParserLocation::setFirst: current: %d:%d - %d:%d\n", first_line, first_col, last_line, last_col);
      saved_first_line = first_line;
      saved_first_col = first_col;
   }

   DLLLOCAL void restoreFirst() {
      first_line = saved_first_line;
      first_col = saved_first_col;
   }

   DLLLOCAL void update(int lineno, int leng, const char* text) {
      assert(leng >= 0);
      first_line = last_line;
      first_col = last_col;
      if (first_line == lineno)
         last_col += leng;
      else {
         unsigned int col = 1;
         for (; ((int)col <= leng) && (text[leng - col] != '\n'); ++col) {}
         last_col = col;
         last_line = lineno;
      }
   }
};

struct TryModuleError {
   char* var;
   QoreException* ex;

   DLLLOCAL TryModuleError(char* v, QoreException* e) : var(v), ex(e) {
   }

   DLLLOCAL ~TryModuleError() {
      if (var)
         free(var);
      if (ex)
         ex->del(0);
   }

   // move down string to remove '$' sign from beginning
   DLLLOCAL void fixName() {
      size_t len = strlen(var);
      assert(len);
      // move string + trailing null
      memmove(var, var + 1, len);
   }

   DLLLOCAL QoreHashNode* takeExceptionHash() {
      assert(ex);
      QoreHashNode* h = ex->makeExceptionObjectAndDelete(0);
      ex = 0;
      return h;
   }

   DLLLOCAL char* takeName() {
      char* str = var;
      var = 0;
      return str;
   }
};

#define YYLTYPE class QoreParserLocation

//! The state of a parse that the parser shares with its scanner
struct QoreParseState {
   //! true if the parse stops at an error, and the scanner ends the input
   bool stop = false;
   //! true if the scanner has ended the input
   bool ended = false;
};

// private interface to bison/flex parser/scanner
typedef void* yyscan_t;
DLLLOCAL extern int yylex_init_extra(QoreParseState* state, yyscan_t* scanner);
DLLLOCAL extern QoreParseState* yyget_extra(yyscan_t scanner);
//! Ends the input of a parse that has not reached the end of its input, as when the parser's stack is exhausted
DLLLOCAL void qore_scanner_end_input(yyscan_t scanner);
DLLLOCAL extern int yyparse(yyscan_t yyscanner);
DLLLOCAL extern struct yy_buffer_state* yy_scan_string(const char*, yyscan_t scanner);
DLLLOCAL void yyset_in(FILE* in_str, yyscan_t yyscanner);
DLLLOCAL int yylex_destroy(yyscan_t yyscanner);
DLLLOCAL void yyset_lineno(int line_number, yyscan_t yyscanner);

#endif // _QORE_PARSER_SUPPORT_H
