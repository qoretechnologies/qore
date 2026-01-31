/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ASTModuleDeclaration.h

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

#ifndef _QLS_AST_DECLARATIONS_ASTMODULEDECLARATION_H
#define _QLS_AST_DECLARATIONS_ASTMODULEDECLARATION_H

#include <string>
#include <vector>
#include <utility>

#include "ast/ASTDeclaration.h"
#include "ast/ASTExpression.h"
#include "ast/ASTName.h"

//! Represents a module declaration with metadata attributes.
/**
    Example:
    @code
    module TestWorkflow2 {
        version = "1.0";
        desc = "workflow dev extension test module";
        author = "Qore Technologies <info@qoretechnologies.com>";
        url = "http://www.qoretechnologies.com";
    }
    @endcode
*/
class ASTModuleDeclaration : public ASTDeclaration {
public:
    //! A single module attribute (key = value).
    struct Attribute {
        std::string key;
        ASTExpression::Ptr value;

        Attribute(const std::string& k, ASTExpression* v) : key(k), value(v) {}
        Attribute(std::string&& k, ASTExpression* v) : key(std::move(k)), value(v) {}
    };

    //! Name of the module.
    ASTName name;

    //! Module attributes (version, desc, author, url, etc.).
    std::vector<Attribute> attributes;

public:
    ASTModuleDeclaration(ASTName&& n) :
        ASTDeclaration(),
        name(std::move(n)) {}

    ASTModuleDeclaration(const ASTName& n) :
        ASTDeclaration(),
        name(n) {}

    void addAttribute(const std::string& key, ASTExpression* value) {
        attributes.emplace_back(key, value);
    }

    void addAttribute(std::string&& key, ASTExpression* value) {
        attributes.emplace_back(std::move(key), value);
    }

    virtual ASTDeclarationKind getKind() const override {
        return ASTDeclarationKind::ADK_Module;
    }
};

#endif // _QLS_AST_DECLARATIONS_ASTMODULEDECLARATION_H
