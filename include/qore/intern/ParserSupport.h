/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ParserSupport.h

  parsing support functions and objects

  Qore Programming language

  Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#include <cstdio>
#include <map>
#include <string>
#include <cstring>
#include "qore/intern/QoreNativeParseInfo.h"
#include "qore/intern/LexicalSemanticPolicy.h"

//! a method qualifier and the functional domains declared in a callable's attribute list (\c [dom=...])
struct CallableQualifier {
    //! the method modifier bits from the qualifier (OFM_CONST)
    int mod;
    //! the declared functional domain bits (QDOM_*), or keys seen while parsing an attribute list
    int64 dom;
};

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

   QoreNativeParseInfo native_parse_info;
   LexicalSemanticPolicy semantic_policy;
   bool has_semantic_policy = false;

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
         // columns are 0-based: the column after the token is the number of characters after its last newline
         unsigned int col = 1;
         for (; ((int)col <= leng) && (text[leng - col] != '\n'); ++col) {}
         last_col = col - 1;
         last_line = lineno;
      }
   }

   //! Reverts the last update(), for a token that the scanner gives back with yyless(0) to scan it again
   /** update() made the end of the last token the start of the given-back one, so its end is restored from there
   */
   DLLLOCAL void revertUpdate() {
      last_line = first_line;
      last_col = first_col;
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
   //! The token or grammar rule currently constructing executable source nodes.
   LexicalSemanticPolicy semantic_policy;
   //! true if the parse stops at an error, and the scanner ends the input
   bool stop = false;
   //! true if the scanner has ended the input
   bool ended = false;
   //! the name after the % of the last line that started with % and a name that no parse directive's rule matched,
   //! which the scanner gives as a context reference: a syntax error in the statement that it begins (up to the next
   //! ;, { or } in the code) reports it as an unknown or invalid parse directive (issue #56)
   std::string line_start_directive;
   //! the location of line_start_directive (its line and source), or nullptr if there is none
   const QoreProgramLocation* line_start_directive_loc = nullptr;
   //! true from line_start_directive until the scanner matches the end of a statement or block (;, { or }): a
   //! syntax error before it is that of the directive, whose name the scanner gave as a context reference (issue #56)
   bool line_start_directive_active = false;
   //! Provenance of the current legacy options; tokens copy it before later directives can restore %modern.
   QoreNativeParseInfo native_parse_info;
   //! the extent of the last expression in parentheses that the parser reduced ('(' exp ')'): an operand of the
   //! chomp, trim or splice operator with this extent is in parentheses of its own (issue #1867)
   int paren_first_line = 0, paren_first_col = 0, paren_last_line = 0, paren_last_col = 0;
   //! true if the parentheses of the last expression in parentheses are those of a list: (a, b), not ((a, b)), so
   //! that its elements are the arguments of a call (issue #1867)
   bool paren_list = false;
   //! for each source file that the scanner reads and that has not ended, the first byte of the line that the last
   //! block read ends in, or -1 if it ends with a newline: a file whose last line is a parse directive without a
   //! newline gets one at its end, as the rules of the parse directives end with a newline (qore_scanner_read(),
   //! issue #2740)
   std::map<FILE*, int> file_line_first;
   //! for each file included with %include whose scan has not ended, the end of the last token of the including file
   //! (its last line and column), where the scan of the including file resumes: the location of a token starts at the
   //! end of the last one, so the scan of an included file starts at its first line and column (issue #6561)
   std::vector<std::pair<int, int>> include_resume_loc;
};

//! returns the description of a syntax error on a line that starts with % and \a name, which no parse directive's
//! rule matched: an unknown parse directive, with the closest one, or one with invalid syntax (issue #56)
DLLLOCAL std::string qore_parse_directive_error(const std::string& name);

//! returns true if \a name is the name of a parse directive (issue #56)
DLLLOCAL bool qore_parse_directive_known(const std::string& name);

// private interface to bison/flex parser/scanner
typedef void* yyscan_t;
DLLLOCAL extern int yylex_init_extra(QoreParseState* state, yyscan_t* scanner);
DLLLOCAL extern QoreParseState* yyget_extra(yyscan_t scanner);
//! Ends the input of a parse that has not reached the end of its input, as when the parser's stack is exhausted
DLLLOCAL void qore_scanner_end_input(yyscan_t scanner);
DLLLOCAL extern int yyparse(yyscan_t yyscanner);
DLLLOCAL extern struct yy_buffer_state* yy_scan_string(const char*, yyscan_t scanner);
DLLLOCAL extern struct yy_buffer_state* yy_scan_bytes(const char*, int, yyscan_t scanner);
DLLLOCAL void yyset_in(FILE* in_str, yyscan_t yyscanner);
DLLLOCAL int yylex_destroy(yyscan_t yyscanner);
DLLLOCAL void yyset_lineno(int line_number, yyscan_t yyscanner);

#endif // _QORE_PARSER_SUPPORT_H
