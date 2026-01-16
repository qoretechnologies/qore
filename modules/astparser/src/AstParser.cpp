/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstParser.cpp

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

#include "AstParser.h"

#include <cstdio>
#include <memory>

#include "ast/AST.h"
#include "AstTreePrinter.h"

typedef void *yyscan_t;
extern int yyparse(yyscan_t yyscanner, AstParseErrorLog* errorLog, ASTTree* parseTree);
extern struct yy_buffer_state* yy_scan_string(const char *, yyscan_t yyscanner);
extern struct yy_buffer_state* yy_create_buffer(FILE* file, int size, yyscan_t yyscanner);
extern void yy_delete_buffer(struct yy_buffer_state* buffer, yyscan_t yyscanner);
extern void yy_switch_to_buffer(struct yy_buffer_state* new_buffer, yyscan_t yyscanner);
extern int yylex_init(yyscan_t *yyscanner);
extern void yyset_in(FILE *in_str, yyscan_t yyscanner);
extern int yylex_destroy(yyscan_t yyscanner);
extern void yyset_lineno(int line_number, yyscan_t yyscanner);
extern "C" void ast_scanner_set_conditional_eval(bool enabled);
extern "C" void ast_scanner_clear_defines();
extern "C" void ast_scanner_add_define(const char* name);
extern "C" void ast_scanner_reset_conditionals();

//! Copied over from YY_BUF_SIZE from the generated flex scanner.
#define AST_BUF_SIZE 16384

ASTTree* AstParser::parseFile(const char* filename) {
    // Open a file for reading.
    FILE* f = fopen(filename, "r");
    if (!f)
        return nullptr;

    // Prepare scanner.
    yyscan_t lexer;
    yylex_init(&lexer);
    ast_scanner_reset_conditionals();
    ast_scanner_set_conditional_eval(conditionalParsing);
    ast_scanner_clear_defines();
    for (const auto& name : defines) {
        ast_scanner_add_define(name.c_str());
    }

    // Set up flex to scan the file.
    yy_buffer_state* buf = yy_create_buffer(f, AST_BUF_SIZE, lexer);
    yy_switch_to_buffer(buf, lexer);
    yyset_lineno(1, lexer);

    // Prepare an empty AST tree for holding the parsed tree.
    std::unique_ptr<ASTTree> tree(new ASTTree);

    // Parse.
    int rc = yyparse(lexer, this, tree.get());

    // Destroy buffer.
    yy_delete_buffer(buf, lexer);

    // Close the file.
    fclose(f);

    // Destroy scanner.
    yylex_destroy(lexer);

    // If error happened during parsing, don't return the tree.
    if (rc)
        return nullptr;

    // Release the created tree.
    return tree.release();
}

ASTTree* AstParser::parseFile(std::string& filename) {
    return parseFile(filename.c_str());
}

ASTTree* AstParser::parseString(const char* str) {
    if (!str)
        return nullptr;

    // Prepare scanner.
    yyscan_t lexer;
    yylex_init(&lexer);
    ast_scanner_reset_conditionals();
    ast_scanner_set_conditional_eval(conditionalParsing);
    ast_scanner_clear_defines();
    for (const auto& name : defines) {
        ast_scanner_add_define(name.c_str());
    }

    FILE* input_stub = tmpfile();
    if (!input_stub) {
        yylex_destroy(lexer);
        return nullptr;
    }

    // Set up flex to scan an in-memory string; keep yyin valid for buffer refills.
    yy_buffer_state* buf = yy_scan_string(str, lexer);
    yyset_in(input_stub, lexer);
    yyset_lineno(1, lexer);

    // Prepare an empty AST tree for holding the parsed tree.
    std::unique_ptr<ASTTree> tree(new ASTTree);

    // Parse.
    int rc = yyparse(lexer, this, tree.get());

    // Destroy buffer.
    yy_delete_buffer(buf, lexer);

    // Destroy scanner.
    yylex_destroy(lexer);

    if (input_stub) {
        fclose(input_stub);
    }

    // If error happened during parsing, don't return the tree.
    if (rc)
        return nullptr;

    // Release the created tree.
    return tree.release();
}

ASTTree* AstParser::parseString(std::string& str) {
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
