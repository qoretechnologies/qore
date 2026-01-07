/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  SymbolInfoFixes.h

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

#ifndef _QLS_QUERIES_SYMBOLINFOFIXES_H
#define _QLS_QUERIES_SYMBOLINFOFIXES_H

#include <vector>

#include "ast/ASTSymbolInfo.h"

class ASTTree;

class SymbolInfoFixes {
public:
    SymbolInfoFixes() = delete;
    SymbolInfoFixes(const SymbolInfoFixes& other) = delete;

    //! Fix symbol info for the passed class.
    /**
        @param tree AST tree of the symbols
        @param si symbol info to fix
        @param bareNames whether to leave symbol name bare (without namespace and class prefixes)
     */
    static void fixClassInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames);
    
    //! Fix symbol info for the passed constant.
    /**
        @param tree AST tree of the symbols
        @param si symbol info to fix
        @param bareNames whether to leave symbol name bare (without namespace and class prefixes)
     */
    static void fixConstantInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames);
    
    //! Fix symbol info for the passed function.
    /**
        @param tree AST tree of the symbols
        @param si symbol info to fix
        @param bareNames whether to leave symbol name bare (without namespace and class prefixes)
     */
    static void fixFunctionInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames);
    
    //! Fix symbol info for the passed hashdecl.
    /**
        @param tree AST tree of the symbols
        @param si symbol info to fix
        @param bareNames whether to leave symbol name bare (without namespace and class prefixes)
     */
    static void fixHashDeclInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames);
    
    //! Fix symbol info for the passed hash member.
    /**
        @param tree AST tree of the symbols
        @param si symbol info to fix
        @param bareNames whether to leave symbol name bare (without namespace and class prefixes)
     */
    static void fixHashMemberInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames);
    
    //! Fix symbol info for the passed variable.
    /**
        @param tree AST tree of the symbols
        @param si symbol info to fix
        @param bareNames whether to leave symbol name bare (without namespace and class prefixes)
     */
    static void fixVariableInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames);

    //! Fix symbol info for the passed typedef.
    /**
        @param tree AST tree of the symbols
        @param si symbol info to fix
        @param bareNames whether to leave symbol name bare (without namespace prefix)
     */
    static void fixTypedefInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames);

    //! Fix symbol infos for the passed symbol info vector.
    /**
        @param tree AST tree of the symbols
        @param vec vector of symbol infos to fix
        @param bareNames whether to return bare symbol names (without namespace and class prefixes)
     */
    static void fixSymbolInfos(ASTTree* tree, std::vector<ASTSymbolInfo>& vec, bool bareNames);
};

#endif // _QLS_QUERIES_SYMBOLINFOFIXES_H
