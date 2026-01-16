/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ASTParseOption.h

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

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#ifndef _QLS_AST_ASTPARSEOPTION_H
#define _QLS_AST_ASTPARSEOPTION_H

#include <memory>
#include <string>

#include "ASTNode.h"

enum ASTParseOptionKind {
    APOK_ALLOW_BARE_REFS,
    APOK_ALLOW_DEBUGGER,
    APOK_ALLOW_INJECTION,
    APOK_ALLOW_STATEMENT_NO_EFFECT,
    APOK_ALLOW_WEAK_REFERENCES,
    APOK_APPEND_INCLUDE_PATH,
    APOK_APPEND_MODULE_PATH,
    APOK_ASSUME_GLOBAL,
    APOK_ASSUME_LOCAL,
    APOK_BROKEN_INT_ASSIGNMENTS,
    APOK_BROKEN_LIST_PARSING,
    APOK_BROKEN_LOGIC_PRECEDENCE,
    APOK_BROKEN_LOOP_STATEMENT,
    APOK_BROKEN_OPERATORS,
    APOK_BROKEN_REFERENCES,
    APOK_CORRECT_INT_ASSIGNMENTS,
    APOK_CORRECT_LIST_PARSING,
    APOK_CORRECT_LOGIC_PRECEDENCE,
    APOK_CORRECT_LOOP_STATEMENT,
    APOK_CORRECT_OPERATORS,
    APOK_CORRECT_REFERENCES,
    APOK_DEFINE,
    APOK_DISABLE_ALL_WARNINGS,
    APOK_DISABLE_WARNING,
    APOK_ENABLE_ALL_WARNINGS,
    APOK_ENABLE_DEBUG,
    APOK_DISABLE_DEBUG,
    APOK_ENABLE_WARNING,
    APOK_ENDTRY,
    APOK_EXEC_CLASS,
    APOK_INCLUDE,
    APOK_LOCKDOWN,
    APOK_LOCK_OPTIONS,
    APOK_LOCK_WARNINGS,
    APOK_LOOSE_ARGS,
    APOK_MODULE_CMD,
    APOK_MODERN,
    APOK_NEW_STYLE,
    APOK_NO_CHILD_PO_RESTRICTIONS,
    APOK_NO_CLASS_DEFS,
    APOK_NO_CONSTANT_DEFS,
    APOK_NO_DATABASE,
    APOK_NO_DEBUGGING,
    APOK_NO_EXTERNAL_PROCESS,
    APOK_NO_EXTERNAL_INFO,
    APOK_NO_EXTERNAL_ACCESS,
    APOK_NO_FILESYSTEM,
    APOK_NO_GLOBAL_VARS,
    APOK_NO_GUI,
    APOK_NO_IO,
    APOK_NO_LOCALE_CONTROL,
    APOK_NO_MODULES,
    APOK_NO_NAMESPACE_DEFS,
    APOK_NO_NETWORK,
    APOK_NO_NEW,
    APOK_NO_PROCESS_CONTROL,
    APOK_NO_REFLECTION,
    APOK_NO_SUBROUTINE_DEFS,
    APOK_NO_TERMINAL_IO,
    APOK_NO_THREADS,
    APOK_NO_THREAD_CLASSES,
    APOK_NO_THREAD_CONTROL,
    APOK_NO_THREAD_INFO,
    APOK_NO_TOP_LEVEL_STATEMENTS,
    APOK_NO_TRANSIENT,
    APOK_NO_TYPEDEF,
    APOK_NO_ENUM,
    APOK_NO_UNCONTROLLED_APIS,
    APOK_OLD_STYLE,
    APOK_PERL_BOOLEAN_EVAL,
    APOK_PUSH_PARSE_OPTIONS,
    APOK_REQUIRES,
    APOK_REQUIRE_DOLLAR,
    APOK_REQUIRE_OUR,
    APOK_REQUIRE_PROTOTYPES,
    APOK_REQUIRE_TYPES,
    APOK_SET_TIME_ZONE,
    APOK_STRICT_ARGS,
    APOK_STRICT_BOOLEAN_EVAL,
    APOK_STRONG_ENCAPSULATION,
    APOK_TRY_MODULE,
    APOK_TRY_REEXPORT_MODULE,
    APOK_BROKEN_RANGE,
    APOK_CORRECT_RANGE,
    APOK_BROKEN_SPRINTF,
    APOK_CORRECT_SPRINTF,
    APOK_BROKEN_CAST,
    APOK_CORRECT_CAST,
    APOK_BROKEN_VARARGS,
    APOK_CORRECT_VARARGS,
    APOK_ALLOW_RETURNS,
    APOK_STRICT_TYPES,
    APOK_LOOSE_TYPES,

    // TODO
    APOK_IFDEF,
    APOK_IFNDEF,
    APOK_IF,
    APOK_ELIF,
    APOK_ELSE,
    APOK_ENDIF,
};

class ASTParseOption : public ASTNode {
public:
    //! Pointer type.
    using Ptr = std::unique_ptr<ASTParseOption>;

public:
    //! Kind of parse option.
    ASTParseOptionKind kind;

public:
    ASTParseOption(ASTParseOptionKind k) : ASTNode(), kind(k) {}

    virtual ASTParseOptionKind getKind() {
        return kind;
    }

    virtual ASTNodeType getNodeType() {
        return ANT_ParseOption;
    }
};

class ASTStringParseOption : public ASTParseOption {
public:
    //! String data for the option.
    std::string str;

public:
    ASTStringParseOption(ASTParseOptionKind k, const char* s = nullptr) :
        ASTParseOption(k)
    {
        if (s)
            str = s;
    }
};

#endif // _QLS_AST_ASTPARSEOPTION_H
