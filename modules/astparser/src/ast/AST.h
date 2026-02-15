/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AST.h

  Qore AST Parser

  Copyright (C) 2023 - 2024 Qore Technologies, sro

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

#ifndef _QLS_AST_AST_H
#define _QLS_AST_AST_H

#include "ast/ASTComment.h"
#include "ast/ASTDeclaration.h"
#include "ast/ASTDeclarationKind.h"
#include "ast/ASTExpression.h"
#include "ast/ASTExpressionKind.h"
#include "ast/ASTModifiers.h"
#include "ast/ASTName.h"
#include "ast/ASTNode.h"
#include "ast/ASTNodeType.h"
#include "ast/ASTOperator.h"
#include "ast/ASTParseError.h"
#include "ast/ASTParseLocation.h"
#include "ast/ASTParseOption.h"
#include "ast/ASTStatement.h"
#include "ast/ASTStatementKind.h"
#include "ast/ASTSymbolInfo.h"
#include "ast/ASTSymbolKind.h"
#include "ast/ASTSymbolUsageKind.h"
#include "ast/ASTTree.h"

#include "ast/declarations/ASTClassDeclaration.h"
#include "ast/declarations/ASTConstantDeclaration.h"
#include "ast/declarations/ASTEnumDeclaration.h"
#include "ast/declarations/ASTEnumMemberDeclaration.h"
#include "ast/declarations/ASTModuleDeclaration.h"
#include "ast/declarations/ASTFunctionDeclaration.h"
#include "ast/declarations/ASTHashDeclaration.h"
#include "ast/declarations/ASTHashMemberDeclaration.h"
#include "ast/declarations/ASTMemberGroupDeclaration.h"
#include "ast/declarations/ASTNamespaceDeclaration.h"
#include "ast/declarations/ASTSuperclassDeclaration.h"
#include "ast/declarations/ASTTypedefDeclaration.h"
#include "ast/declarations/ASTVariableDeclaration.h"
#include "ast/declarations/ASTVarListDeclaration.h"

#include "ast/expressions/ASTAccessExpression.h"
#include "ast/expressions/ASTAssignmentExpression.h"
#include "ast/expressions/ASTBackquoteExpression.h"
#include "ast/expressions/ASTBinaryExpression.h"
#include "ast/expressions/ASTCallExpression.h"
#include "ast/expressions/ASTCaseExpression.h"
#include "ast/expressions/ASTCastExpression.h"
#include "ast/expressions/ASTClosureExpression.h"
#include "ast/expressions/ASTConstrInitExpression.h"
#include "ast/expressions/ASTContextModExpression.h"
#include "ast/expressions/ASTContextRowExpression.h"
#include "ast/expressions/ASTDeclExpression.h"
#include "ast/expressions/ASTFindExpression.h"
#include "ast/expressions/ASTHashdeclHashExpression.h"
#include "ast/expressions/ASTHashElementExpression.h"
#include "ast/expressions/ASTHashExpression.h"
#include "ast/expressions/ASTImplicitArgExpression.h"
#include "ast/expressions/ASTImplicitElemExpression.h"
#include "ast/expressions/ASTIndexExpression.h"
#include "ast/expressions/ASTListExpression.h"
#include "ast/expressions/ASTLiteralExpression.h"
#include "ast/expressions/ASTNameExpression.h"
#include "ast/expressions/ASTRangeExpression.h"
#include "ast/expressions/ASTRegexExpression.h"
#include "ast/expressions/ASTRegexSubstExpression.h"
#include "ast/expressions/ASTRegexTransExpression.h"
#include "ast/expressions/ASTReturnsExpression.h"
#include "ast/expressions/ASTSwitchBodyExpression.h"
#include "ast/expressions/ASTTernaryExpression.h"
#include "ast/expressions/ASTUnaryExpression.h"

#include "ast/statements/ASTAssertStatement.h"
#include "ast/statements/ASTBreakStatement.h"
#include "ast/statements/ASTCallStatement.h"
#include "ast/statements/ASTContextStatement.h"
#include "ast/statements/ASTContinueStatement.h"
#include "ast/statements/ASTDebugStatement.h"
#include "ast/statements/ASTDoWhileStatement.h"
#include "ast/statements/ASTExpressionStatement.h"
#include "ast/statements/ASTForeachStatement.h"
#include "ast/statements/ASTForStatement.h"
#include "ast/statements/ASTIfStatement.h"
#include "ast/statements/ASTOnBlockExitStatement.h"
#include "ast/statements/ASTRethrowStatement.h"
#include "ast/statements/ASTReturnStatement.h"
#include "ast/statements/ASTStatementBlock.h"
#include "ast/statements/ASTSummarizeStatement.h"
#include "ast/statements/ASTSwitchStatement.h"
#include "ast/statements/ASTThreadExitStatement.h"
#include "ast/statements/ASTThrowStatement.h"
#include "ast/statements/ASTTryStatement.h"
#include "ast/statements/ASTWhileStatement.h"

#endif // _QLS_AST_AST_H
