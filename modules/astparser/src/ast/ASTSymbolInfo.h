/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ASTSymbolInfo.h

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

#ifndef _QLS_AST_ASTSYMBOLINFO_H
#define _QLS_AST_ASTSYMBOLINFO_H

#include "ast/ASTName.h"
#include "ast/ASTNode.h"
#include "ast/ASTSymbolKind.h"
#include "ast/ASTSymbolUsageKind.h"

class ASTSymbolInfo : public ASTNode {
public:
    ASTSymbolInfo() : ASTNode(), kind(ASYK_None), usage(ASUK_None) {}

    ASTSymbolInfo(ASTSymbolKind k, ASTSymbolUsageKind u, ASTName* n) :
        ASTNode(n->loc),
        kind(k),
        usage(u),
        name(n->name) {}

    ASTSymbolInfo(ASTSymbolKind k, ASTSymbolUsageKind u, const ASTParseLocation& nloc, std::string&& n) :
        ASTNode(nloc),
        kind(k),
        usage(u),
        name(std::move(n)) {}

    ASTSymbolInfo(ASTSymbolKind k, ASTSymbolUsageKind u, const ASTParseLocation& nloc, const std::string& n) :
        ASTNode(nloc),
        kind(k),
        usage(u),
        name(n) {}

    ASTSymbolInfo(ASTSymbolKind k, ASTName* n) :
        ASTNode(n->loc),
        kind(k),
        usage(ASUK_None),
        name(n->name) {}

    ASTSymbolInfo(ASTSymbolKind k, const ASTParseLocation& nloc, std::string&& n) :
        ASTNode(nloc),
        kind(k),
        usage(ASUK_None),
        name(std::move(n)) {}

    ASTSymbolInfo(ASTSymbolKind k, const ASTParseLocation& nloc, const std::string& n) :
        ASTNode(nloc),
        kind(k),
        usage(ASUK_None),
        name(n) {}

    //! Copy constructor.
    ASTSymbolInfo(const ASTSymbolInfo& si) : ASTNode(si.loc), kind(si.kind), usage(si.usage), name(si.name) {}

    //! Move constructor.
    ASTSymbolInfo(ASTSymbolInfo&& si) : ASTNode(si.loc), kind(si.kind), usage(si.usage), name(std::move(si.name)) {
        si.kind = ASYK_None;
        si.usage = ASUK_None;
    }

    //! Assignment operator.
    ASTSymbolInfo& operator=(const ASTSymbolInfo& si) {
        loc = si.loc;
        kind = si.kind;
        usage = si.usage;
        name = si.name;
        return *this;
    }

    //! Move assignment operator.
    ASTSymbolInfo& operator=(ASTSymbolInfo&& si) {
        loc = si.loc;
        kind = si.kind;
        usage = si.usage;
        name = std::move(si.name);
        si.kind = ASYK_None;
        si.usage = ASUK_None;
        return *this;
    }

    //! Symbol's kind.
    ASTSymbolKind kind;

    //! Symbol's usage kind.
    ASTSymbolUsageKind usage;

    //! Symbol's name.
    std::string name;
};

#endif // _QLS_AST_ASTSYMBOLINFO_H
