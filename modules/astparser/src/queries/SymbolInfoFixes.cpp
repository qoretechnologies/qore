/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  SymbolInfoFixes.cpp

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

#include "queries/SymbolInfoFixes.h"

#include <memory>

#include "ast/AST.h"
#include "queries/FindNodeAndParentsQuery.h"

namespace astparser_intern {
    static std::vector<ASTNode*>* getNodes(ASTTree* tree, ASTSymbolInfo& si) {
        return FindNodeAndParentsQuery::find(tree, si.loc.firstLine, si.loc.firstCol);
    }
}

void SymbolInfoFixes::fixClassInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames) {
    if (bareNames)
        return;

    // Target node is first, all the parents follow.
    std::unique_ptr<std::vector<ASTNode*> > nodes(astparser_intern::getNodes(tree, si));
    if (!nodes)
        return;

    size_t nextNode = 0;
    size_t nodeCount = nodes->size();
    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() != ANT_Declaration)
            continue;
        ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
        if (decl->getKind() == ASTDeclarationKind::ADK_Class) {
            nextNode++;
            break;
        }
    }

    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() == ANT_Declaration) {
            ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
            if (decl->getKind() == ASTDeclarationKind::ADK_Class)
                si.name.insert(0, static_cast<ASTClassDeclaration*>(decl)->name.name + "::");
            else if (decl->getKind() == ASTDeclarationKind::ADK_Namespace)
                si.name.insert(0, static_cast<ASTNamespaceDeclaration*>(decl)->name.name + "::");
        }
    }
}

void SymbolInfoFixes::fixConstantInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames) {
    if (bareNames)
        return;

    // Target node is first, all the parents follow.
    std::unique_ptr<std::vector<ASTNode*> > nodes(astparser_intern::getNodes(tree, si));
    if (!nodes)
        return;

    size_t nextNode = 0;
    size_t nodeCount = nodes->size();
    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() != ANT_Declaration)
            continue;
        ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
        if (decl->getKind() == ASTDeclarationKind::ADK_Constant) {
            nextNode++;
            break;
        }
    }

    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() == ANT_Declaration) {
            ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
            if (decl->getKind() == ASTDeclarationKind::ADK_Function)
                break;
            else if (decl->getKind() == ASTDeclarationKind::ADK_Class)
                si.name.insert(0, static_cast<ASTClassDeclaration*>(decl)->name.name + "::");
            else if (decl->getKind() == ASTDeclarationKind::ADK_Namespace)
                si.name.insert(0, static_cast<ASTNamespaceDeclaration*>(decl)->name.name + "::");
        }
    }
}

void SymbolInfoFixes::fixFunctionInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames) {
    // Target node is first, all the parents follow.
    std::unique_ptr<std::vector<ASTNode*> > nodes(astparser_intern::getNodes(tree, si));
    if (!nodes)
        return;

    size_t nextNode = 0;
    size_t nodeCount = nodes->size();
    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() != ANT_Declaration)
            continue;
        ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
        if (decl->getKind() == ASTDeclarationKind::ADK_Function) {
            nextNode++;
            break;
        }
    }

    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() == ANT_Declaration) {
            ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
            if (decl->getKind() == ASTDeclarationKind::ADK_Class) {
                si.kind = (si.name == "constructor") ? ASYK_Constructor : ASYK_Method;
                if (!bareNames)
                    si.name.insert(0, static_cast<ASTClassDeclaration*>(decl)->name.name + "::");
            }
            else if (decl->getKind() == ASTDeclarationKind::ADK_Namespace) {
                if (!bareNames)
                    si.name.insert(0, static_cast<ASTNamespaceDeclaration*>(decl)->name.name + "::");
            }
        }
    }
}

void SymbolInfoFixes::fixHashDeclInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames) {
    if (bareNames)
        return;

    // Target node is first, all the parents follow.
    std::unique_ptr<std::vector<ASTNode*> > nodes(astparser_intern::getNodes(tree, si));
    if (!nodes)
        return;

    size_t nextNode = 0;
    size_t nodeCount = nodes->size();
    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() != ANT_Declaration)
            continue;
        ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
        if (decl->getKind() == ASTDeclarationKind::ADK_Hash) {
            nextNode++;
            break;
        }
    }

    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() == ANT_Declaration) {
            ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
            if (decl->getKind() == ASTDeclarationKind::ADK_Class)
                si.name.insert(0, static_cast<ASTClassDeclaration*>(decl)->name.name + "::");
            else if (decl->getKind() == ASTDeclarationKind::ADK_Namespace)
                si.name.insert(0, static_cast<ASTNamespaceDeclaration*>(decl)->name.name + "::");
        }
    }
}

void SymbolInfoFixes::fixHashMemberInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames) {
    if (bareNames)
        return;

    // Target node is first, all the parents follow.
    std::unique_ptr<std::vector<ASTNode*> > nodes(astparser_intern::getNodes(tree, si));
    if (!nodes)
        return;

    size_t nextNode = 0;
    size_t nodeCount = nodes->size();
    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() != ANT_Declaration)
            continue;
        ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
        if (decl->getKind() == ASTDeclarationKind::ADK_HashMember) {
            nextNode++;
            break;
        }
    }

    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() == ANT_Declaration) {
            ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
            if (decl->getKind() == ASTDeclarationKind::ADK_Function)
                break;
            else if (decl->getKind() == ASTDeclarationKind::ADK_Class)
                si.name.insert(0, static_cast<ASTClassDeclaration*>(decl)->name.name + "::");
            else if (decl->getKind() == ASTDeclarationKind::ADK_Hash)
                si.name.insert(0, static_cast<ASTHashDeclaration*>(decl)->name.name + "::");
            else if (decl->getKind() == ASTDeclarationKind::ADK_Namespace)
                si.name.insert(0, static_cast<ASTNamespaceDeclaration*>(decl)->name.name + "::");
        }
    }
}

void SymbolInfoFixes::fixVariableInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames) {
    if (bareNames)
        return;

    // Target node is first, all the parents follow.
    std::unique_ptr<std::vector<ASTNode*> > nodes(astparser_intern::getNodes(tree, si));
    if (!nodes)
        return;

    size_t nextNode = 0;
    size_t nodeCount = nodes->size();
    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() != ANT_Declaration)
            continue;
        ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
        if (decl->getKind() == ASTDeclarationKind::ADK_Variable) {
            nextNode++;
            break;
        }
    }

    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() == ANT_Declaration) {
            ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
            if (decl->getKind() == ASTDeclarationKind::ADK_Function)
                break;
            else if (decl->getKind() == ASTDeclarationKind::ADK_Class)
                si.name.insert(0, static_cast<ASTClassDeclaration*>(decl)->name.name + "::");
            else if (decl->getKind() == ASTDeclarationKind::ADK_Namespace)
                si.name.insert(0, static_cast<ASTNamespaceDeclaration*>(decl)->name.name + "::");
        }
    }
}

void SymbolInfoFixes::fixTypedefInfo(ASTTree* tree, ASTSymbolInfo& si, bool bareNames) {
    if (bareNames)
        return;

    // Target node is first, all the parents follow.
    std::unique_ptr<std::vector<ASTNode*> > nodes(astparser_intern::getNodes(tree, si));
    if (!nodes)
        return;

    size_t nextNode = 0;
    size_t nodeCount = nodes->size();
    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() != ANT_Declaration)
            continue;
        ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
        if (decl->getKind() == ASTDeclarationKind::ADK_Typedef) {
            nextNode++;
            break;
        }
    }

    for (; nextNode < nodeCount; nextNode++) {
        ASTNode* node = nodes->at(nextNode);
        if (node->getNodeType() == ANT_Declaration) {
            ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
            if (decl->getKind() == ASTDeclarationKind::ADK_Namespace)
                si.name.insert(0, static_cast<ASTNamespaceDeclaration*>(decl)->name.name + "::");
        }
    }
}

void SymbolInfoFixes::fixSymbolInfos(ASTTree* tree, std::vector<ASTSymbolInfo>& vec, bool bareNames) {
    for (size_t i = 0, count = vec.size(); i < count; i++) {
        ASTSymbolInfo& si = vec[i];

        // Do this only for classes, constants, functions, variables, hashdecl, hash members, and typedefs.
        if (!(si.kind == ASYK_Constructor ||
              si.kind == ASYK_Function ||
              si.kind == ASYK_Method ||
              si.kind == ASYK_Variable ||
              si.kind == ASYK_Class ||
              si.kind == ASYK_Constant ||
              si.kind == ASYK_Field ||
              si.kind == ASYK_Interface ||
              si.kind == ASYK_TypeAlias))
            continue;

        // Fix the symbol info.
        switch (si.kind) {
            case ASYK_Class:
                fixClassInfo(tree, si, bareNames); break;
            case ASYK_Constant:
                fixConstantInfo(tree, si, bareNames); break;
            case ASYK_Constructor:
            case ASYK_Function:
            case ASYK_Method:
                fixFunctionInfo(tree, si, bareNames); break;
            case ASYK_Field:
                fixHashMemberInfo(tree, si, bareNames); break;
            case ASYK_Interface:
                fixHashDeclInfo(tree, si, bareNames); break;
            case ASYK_Variable:
                fixVariableInfo(tree, si, bareNames); break;
            case ASYK_TypeAlias:
                fixTypedefInfo(tree, si, bareNames); break;
            default:
                break;
        }
    }
}
