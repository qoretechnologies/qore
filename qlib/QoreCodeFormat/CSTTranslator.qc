# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CSTTranslator.qc Translates tree-sitter CST nodes to old AST format

/*  CSTTranslator.qc Copyright 2026 Qore Technologies, s.r.o.

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

%modern

#! Main QoreCodeFormat namespace
public namespace QoreCodeFormat {

#! Translates tree-sitter CST nodes to the old AST format expected by CodeFormatter
public class CSTTranslator {
    private {
        #! Original source lines for text extraction
        list<string> sourceLines;

        #! Original source text
        string sourceText;
    }

    #! Translate a list of CST nodes to old AST format
    /** @param cstNodes list of CST nodes from tree.getNodesInfo()
        @param source original source text
        @return list of old-format AST nodes
    */
    static list<hash<auto>> translate(list<hash<auto>> cstNodes, string source) {
        CSTTranslator t();
        t.sourceText = source;
        t.sourceLines = source.split("\n");

        list<hash<auto>> result = ();
        foreach hash<auto> node in (cstNodes) {
            *hash<auto> translated = t.translateNode(node);
            if (exists translated) {
                result += translated;
            }
        }
        return result;
    }

    #! Translate a single CST node to old AST format
    private *hash<auto> translateNode(hash<auto> node) {
        switch (node.type) {
            # Declarations
            case "class_declaration":
                return translateClassDecl(node);
            case "function_declaration":
                return translateFunctionDecl(node);
            case "namespace_declaration":
                return translateNamespaceDecl(node);
            case "constant_declaration":
                return translateConstantDecl(node);
            case "global_variable_declaration":
                return translateGlobalVarDecl(node);
            case "hashdecl_declaration":
                return translateHashdeclDecl(node);
            case "typedef_declaration":
                return translateTypedefDecl(node);
            case "enum_declaration":
                return translateEnumDecl(node);
            case "module_declaration":
                return translateModuleDecl(node);
            case "member_group":
                return translateMemberGroup(node);
            case "constructor_declaration":
                return translateConstructorDecl(node);
            case "destructor_declaration":
                return translateDestructorDecl(node);
            case "copy_method":
                return translateCopyMethod(node);
            case "method_declaration":
                return translateMethodDecl(node);

            # Statements
            case "block":
                return translateBlock(node);
            case "if_statement":
                return translateIfStmt(node);
            case "while_statement":
                return translateWhileStmt(node);
            case "do_while_statement":
                return translateDoWhileStmt(node);
            case "for_statement":
                return translateForStmt(node);
            case "foreach_statement":
                return translateForeachStmt(node);
            case "switch_statement":
                return translateSwitchStmt(node);
            case "try_statement":
                return translateTryStmt(node);
            case "return_statement":
                return translateReturnStmt(node);
            case "throw_statement":
                return translateThrowStmt(node);
            case "rethrow_statement":
                return {"nodetype": ANT_Statement, "kind": ASK_Rethrow, "loc": node.loc};
            case "break_statement":
                return {"nodetype": ANT_Statement, "kind": ASK_Break, "loc": node.loc};
            case "continue_statement":
                return {"nodetype": ANT_Statement, "kind": ASK_Continue, "loc": node.loc};
            case "thread_exit_statement":
                return {"nodetype": ANT_Statement, "kind": ASK_ThreadExit, "loc": node.loc};
            case "on_exit_statement":
                return translateOnExitStmt(node);
            case "expression_statement":
                return translateExpressionStmt(node);
            case "local_variable_declaration":
                return translateLocalVarDecl(node);
            case "list_assignment":
                return {
                    "nodetype": ANT_Statement,
                    "kind": ASK_Expression,
                    "expression": translateListAssignment(node),
                    "loc": node.loc,
                };
            case "assert_statement":
                return {"nodetype": ANT_Statement, "kind": ASK_Assert, "loc": node.loc};
            case "debug_statement":
                return {"nodetype": ANT_Statement, "kind": ASK_Debug, "loc": node.loc};
            case "context_statement":
                return {"nodetype": ANT_Statement, "kind": ASK_Context, "loc": node.loc};
            case "summarize_statement":
                return {"nodetype": ANT_Statement, "kind": ASK_Summarize, "loc": node.loc};

            # Parse directives are extracted separately by CodeFormatter from source.
            # Skip them here to avoid duplication.
            case "parse_directive":
            case "conditional_directive":
                return;
        }

        # Fallback: return raw with source location for extractSourceText
        return {"nodetype": ANT_Expression, "kind": AEK_Name, "name": {"name": extractText(node)}, "loc": node.loc};
    }

    # ---- Helper Functions ----

    #! Find child node by field name
    private *hash<auto> findField(hash<auto> node, string fieldName) {
        if (!exists node.children) {
            return;
        }
        foreach hash<auto> child in (node.children) {
            if (child.field == fieldName) {
                return child;
            }
        }
    }

    #! Find child node by type
    private *hash<auto> findType(hash<auto> node, string typeName) {
        if (!exists node.children) {
            return;
        }
        foreach hash<auto> child in (node.children) {
            if (child.type == typeName) {
                return child;
            }
        }
    }

    #! Find all children by type
    private list<hash<auto>> findAllType(hash<auto> node, string typeName) {
        list<hash<auto>> result = ();
        if (!exists node.children) {
            return result;
        }
        foreach hash<auto> child in (node.children) {
            if (child.type == typeName) {
                result += child;
            }
        }
        return result;
    }

    #! Get leaf text from a node (recursive)
    private *string getLeafText(hash<auto> node) {
        if (exists node.text) {
            return node.text;
        }
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                *string text = getLeafText(child);
                if (exists text) {
                    return text;
                }
            }
        }
    }

    #! Get identifier name from a node (handles scoped_identifier too)
    private string getIdentName(hash<auto> node) {
        if (exists node.text) {
            return node.text;
        }
        # For scoped_identifier, reconstruct full path
        if (node.type == "scoped_identifier") {
            return extractText(node);
        }
        # For other complex types, try to get all text
        *string text = getLeafText(node);
        if (exists text) {
            return text;
        }
        return extractText(node);
    }

    #! Build a name hash {name: str, loc: loc}
    private hash<auto> makeName(*hash<auto> identNode) {
        if (!exists identNode) {
            return {"name": ""};
        }
        return {"name": getIdentName(identNode), "loc": identNode.loc};
    }

    #! Get modifier string from a CST node by looking for modifiers children
    private *string getModifiers(hash<auto> node) {
        list<string> mods = ();
        if (!exists node.children) {
            return;
        }
        foreach hash<auto> child in (node.children) {
            if (child.type == "modifiers" || child.type == "access_modifier") {
                # Modifiers node contains modifier children
                if (exists child.children) {
                    foreach hash<auto> mod in (child.children) {
                        *string modText = getLeafText(mod);
                        if (exists modText) {
                            mods += modText;
                        }
                    }
                } else if (exists child.text) {
                    mods += child.text;
                }
            } else if (child.type == "modifier") {
                *string modText = getLeafText(child);
                if (exists modText) {
                    mods += modText;
                }
            }
        }
        if (mods.size() > 0) {
            return mods.join(" ");
        }
    }

    #! Get type name string from a type node
    private *string getTypeName(*hash<auto> typeNode) {
        if (!exists typeNode) {
            return;
        }
        return extractText(typeNode);
    }

    #! Extract full text of a CST node from source using its location
    private string extractText(hash<auto> node) {
        if (exists node.text) {
            return node.text;
        }
        hash<auto> loc = node.loc;
        int fl = loc.start_line;
        int fc = loc.start_column;
        int ll = loc.end_line;
        int lc = loc.end_column;

        if (fl < 0 || fl >= sourceLines.size()) {
            return "";
        }
        if (ll >= sourceLines.size()) {
            ll = sourceLines.size() - 1;
        }

        if (fl == ll) {
            string line = sourceLines[fl];
            if (fc >= line.size()) {
                return "";
            }
            if (lc > line.size()) {
                lc = line.size();
            }
            return line.substr(fc, lc - fc);
        }

        string result = "";
        for (int i = fl; i <= ll; ++i) {
            if (i >= sourceLines.size()) {
                break;
            }
            string line = sourceLines[i];
            if (i == fl) {
                if (fc < line.size()) {
                    result += line.substr(fc);
                }
            } else if (i == ll) {
                if (lc > line.size()) {
                    lc = line.size();
                }
                result += "\n" + line.substr(0, lc);
            } else {
                result += "\n" + line;
            }
        }
        return result;
    }

    #! Format a location hash as a "line:col" string for error messages
    private static string formatLoc(hash<auto> loc) {
        return sprintf("%d:%d", loc.start_line + 1, loc.start_column + 1);
    }

    #! Extract binary operator from source between left and right children
    private string extractBinaryOp(hash<auto> node, *hash<auto> leftNode, *hash<auto> rightNode) {
        if (!exists leftNode || !exists rightNode) {
            throw "CST-TRANSLATOR-ERROR", sprintf("cannot extract binary operator: missing %s node at %s",
                !exists leftNode ? "left" : "right", formatLoc(node.loc));
        }
        # The operator is in the source text between end of left and start of right
        int opLine = leftNode.loc.end_line;
        int opCol = leftNode.loc.end_column;
        int endLine = rightNode.loc.start_line;
        int endCol = rightNode.loc.start_column;

        # Extract the text between left end and right start
        string between = "";
        if (opLine == endLine && opLine < sourceLines.size()) {
            string line = sourceLines[opLine];
            if (opCol < line.size() && endCol <= line.size()) {
                between = trim(line.substr(opCol, endCol - opCol));
            }
        } else if (opLine < sourceLines.size()) {
            # Multi-line: first check left line after left end
            string line = sourceLines[opLine];
            if (opCol < line.size()) {
                between = trim(line.substr(opCol));
                # Strip inline comment (# ...) that may follow the operator
                *list<string> m = (between =~ x/^([^#]+)/);
                if (exists m) {
                    between = trim(m[0]);
                }
            }
            # If not found on left line, check right node's line before right start
            # (operator-leading style: operator is at start of continuation line)
            if (between == "" && endLine < sourceLines.size()) {
                string rightLine = sourceLines[endLine];
                if (endCol <= rightLine.size()) {
                    between = trim(rightLine.substr(0, endCol));
                }
            }
        }

        if (between != "") {
            return between;
        }
        throw "CST-TRANSLATOR-ERROR", sprintf("cannot extract binary operator from source at %s "
            "(left end: %d:%d, right start: %d:%d)", formatLoc(node.loc), opLine, opCol, endLine, endCol);
    }

    #! Translate an expression CST node to old AST format
    private *hash<auto> translateExpression(*hash<auto> node) {
        if (!exists node) {
            return;
        }

        switch (node.type) {
            case "binary_expression":
                return translateBinaryExpr(node);
            case "unary_expression":
                return translateUnaryExpr(node);
            case "postfix_expression":
                return translatePostfixExpr(node);
            case "assignment_expression":
                return translateAssignmentExpr(node);
            case "call_expression":
                return translateCallExpr(node);
            case "member_expression":
                return translateMemberExpr(node);
            case "index_expression":
                return translateIndexExpr(node);
            case "ternary_expression":
                return translateTernaryExpr(node);
            case "cast_expression":
                return translateCastExpr(node);
            case "parenthesized_expression":
                return translateParenExpr(node);
            case "primary_expression":
                return translatePrimaryExpr(node);
            case "closure_expression":
                return translateClosureExpr(node);
            case "list_literal":
                return translateListLiteral(node);
            case "hash_literal":
                return translateHashLiteral(node, 0);
            case "paren_hash_literal":
                return translateHashLiteral(node, 1);
            case "hash_entry":
                return translateHashEntry(node);
            case "literal":
                return translateLiteral(node);
            case "string":
            case "single_quoted_string":
            case "double_quoted_string":
                return translateStringLiteral(node);
            case "integer":
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "int",
                    "value": node.text, "loc": node.loc};
            case "float":
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "float",
                    "value": node.text, "loc": node.loc};
            case "number":
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "number",
                    "value": node.text, "loc": node.loc};
            case "boolean":
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "bool",
                    "value": node.text == "True", "loc": node.loc};
            case "nothing":
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "nothing",
                    "loc": node.loc};
            case "null":
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "null",
                    "loc": node.loc};
            case "date":
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "date",
                    "value": node.text, "loc": node.loc};
            case "binary":
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "binary",
                    "value": node.text, "loc": node.loc};
            case "identifier":
                return {"nodetype": ANT_Expression, "kind": AEK_Name,
                    "name": makeName(node), "loc": node.loc};
            case "scoped_identifier":
                return {"nodetype": ANT_Expression, "kind": AEK_Name,
                    "name": makeName(node), "loc": node.loc};
            case "variable_name":
                return {"nodetype": ANT_Expression, "kind": AEK_Name,
                    "name": makeName(node), "loc": node.loc};
            case "implicit_argument":
                return {"nodetype": ANT_Expression, "kind": AEK_ImplicitArg,
                    "loc": node.loc};
            case "last_element_expression":
                return {"nodetype": ANT_Expression, "kind": AEK_ImplicitElem,
                    "loc": node.loc};
            case "regex":
                return translateRegex(node);
            case "regex_literal":
                return translateRegexLiteral(node);
            case "regex_subst":
                return translateRegexSubst(node);
            case "regex_trans":
                return translateRegexTrans(node);
            case "regex_extract":
                return translateRegexExtract(node);
            case "string_concatenation":
                return translateStringConcat(node);
            case "new_expression":
                return translateNewExpr(node);
            case "map_expression":
                return translateMapExpr(node);
            case "select_expression":
                return translateSelectExpr(node);
            case "foldl_expression":
                return translateFoldlExpr(node);
            case "foldr_expression":
                return translateFoldrExpr(node);
            case "push_expression":
            case "unshift_expression":
            case "splice_expression":
            case "extract_expression":
                return translateMultiArgExpr(node);
            case "slice_expression":
                return translateSliceExpr(node);
            case "context_reference":
                return {"nodetype": ANT_Expression, "kind": AEK_ContextRow, "loc": node.loc};
            case "type":
            case "simple_type":
            case "complex_type":
            case "nullable_type":
                return {"nodetype": ANT_Expression, "kind": AEK_Name,
                    "name": {"name": extractText(node)}, "loc": node.loc};
            case "variable_declarator":
                return translateVarDeclarator(node);
            case "local_variable_declaration":
                return translateLocalVarDeclExpr(node);
            case "parameter_list":
                return translateParameterList(node);
            case "parameter":
                return translateParameter(node);
            case "argument_list":
                return translateArgumentList(node);
            case "for_variable_declaration":
                return translateForVarDecl(node);
            case "conditional_declaration":
                return translateConditionalDecl(node);
            case "list_assignment":
                return translateListAssignment(node);
        }

        # Default: wrap as expression with source extraction
        return {"nodetype": ANT_Expression, "kind": AEK_Name,
            "name": {"name": extractText(node)}, "loc": node.loc};
    }

    # ---- Declaration Translators ----

    private hash<auto> translateClassDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *string mods = getModifiers(node);

        # Build inherits list from superclass_list
        list<hash<auto>> inheritsList = ();
        *hash<auto> scList = findType(node, "superclass_list");
        if (exists scList) {
            list<hash<auto>> supers = findAllType(scList, "superclass");
            foreach hash<auto> sc in (supers) {
                *string scMods = getModifiers(sc);
                # Get the class name (identifier or scoped_identifier)
                *hash<auto> scName;
                if (exists sc.children) {
                    foreach hash<auto> child in (sc.children) {
                        if (child.type == "identifier" || child.type == "scoped_identifier") {
                            scName = child;
                            break;
                        }
                    }
                }
                hash<auto> scEntry = {
                    "nodetype": ANT_Declaration,
                    "kind": ADK_Superclass,
                    "name": makeName(scName),
                    "loc": sc.loc,
                };
                if (exists scMods) {
                    scEntry.modifiers = scMods;
                }
                inheritsList += scEntry;
            }
        }

        # Collect class body declarations
        list<hash<auto>> decls = ();
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                switch (child.type) {
                    case "member_group":
                    case "constructor_declaration":
                    case "destructor_declaration":
                    case "copy_method":
                    case "method_declaration":
                    case "constant_declaration":
                    case "member_declaration": {
                        *hash<auto> translated = translateClassItem(child);
                        if (exists translated) {
                            decls += translated;
                        }
                        break;
                    }
                }
            }
        }

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Class,
            "name": makeName(nameNode),
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (inheritsList.size() > 0) {
            result."inherits" = inheritsList;
        }
        if (decls.size() > 0) {
            result.declarations = decls;
        }
        return result;
    }

    #! Translate a class body item
    private *hash<auto> translateClassItem(hash<auto> node) {
        switch (node.type) {
            case "member_group":
                return translateMemberGroup(node);
            case "constructor_declaration":
                return translateConstructorDecl(node);
            case "destructor_declaration":
                return translateDestructorDecl(node);
            case "copy_method":
                return translateCopyMethod(node);
            case "method_declaration":
                return translateMethodDecl(node);
            case "constant_declaration":
                return translateConstantDecl(node);
            case "member_declaration":
                return translateMemberDeclaration(node);
        }
        return translateNode(node);
    }

    private hash<auto> translateMemberGroup(hash<auto> node) {
        # member_group has access_modifier + member_declaration children
        *string mods;
        list<hash<auto>> members = ();

        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                if (child.type == "access_modifier") {
                    mods = getLeafText(child);
                } else if (child.type == "member_declaration") {
                    *hash<auto> m = translateMemberDeclaration(child);
                    if (exists m) {
                        members += m;
                    }
                } else if (child.type == "constant_declaration") {
                    *hash<auto> c = translateConstantDecl(child);
                    if (exists c) {
                        members += c;
                    }
                }
            }
        }

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_MemberGroup,
            "modifiers": mods ?? "",
            "loc": node.loc,
        };
        if (members.size() > 0) {
            result.members = members;
        }
        return result;
    }

    private hash<auto> translateMemberDeclaration(hash<auto> node) {
        *hash<auto> typeNode = findField(node, "type");
        *hash<auto> nameNode = findField(node, "name");
        *hash<auto> defaultNode = findField(node, "default");

        # Check if this is an abstract method declaration (has argument_list child AND "abstract" modifier)
        # Note: member variables with constructor args like "Counter counter()" also have argument_list
        # but don't have the "abstract" modifier
        *hash<auto> argList = findType(node, "argument_list");
        *string mods = getModifiers(node);
        if (exists argList && exists mods && mods =~ /abstract/) {
            # Abstract method declaration → translate as ADK_Function
            hash<auto> result = {
                "nodetype": ANT_Declaration,
                "kind": ADK_Function,
                "name": makeName(nameNode),
                "loc": node.loc,
            };
            if (exists mods) {
                result.modifiers = mods;
            }
            if (exists typeNode) {
                result.returnType = {"nodetype": ANT_Expression, "kind": AEK_Name,
                    "name": {"name": extractText(typeNode)}, "loc": typeNode.loc};
            }
            # Abstract methods have no body — formatter uses !exists node.body to emit ";"
            return result;
        }

        # Regular member variable declaration → DeclExpr
        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Decl,
            "loc": node.loc,
        };

        if (exists defaultNode) {
            result."init" = translateExpression(defaultNode);
        }

        return result;
    }

    private hash<auto> translateConstructorDecl(hash<auto> node) {
        *hash<auto> paramList = findType(node, "parameter_list");
        *hash<auto> body = findType(node, "block");
        *string mods = getModifiers(node);

        # Base class constructor calls
        *hash<auto> baseCallsNode = findType(node, "base_class_constructor_calls");
        *hash<auto> inits;
        if (exists baseCallsNode) {
            list<hash<auto>> initList = ();
            list<hash<auto>> calls = findAllType(baseCallsNode, "base_class_constructor_call");
            foreach hash<auto> call in (calls) {
                initList += translateExpression(call) ?? {"nodetype": ANT_Expression, "kind": AEK_Name,
                    "name": {"name": extractText(call)}, "loc": call.loc};
            }
            if (initList.size() > 0) {
                inits = {
                    "nodetype": ANT_Expression,
                    "kind": AEK_ConstrInit,
                    "inits": initList,
                    "loc": baseCallsNode.loc,
                };
            }
        }

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Function,
            "name": {"name": "constructor"},
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (exists paramList) {
            result.params = translateExpression(paramList);
        }
        if (exists inits) {
            result.inits = inits;
        }
        if (exists body) {
            result.body = translateNode(body);
        }
        return result;
    }

    private hash<auto> translateDestructorDecl(hash<auto> node) {
        *hash<auto> body = findType(node, "block");
        *string mods = getModifiers(node);

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Function,
            "name": {"name": "destructor"},
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (exists body) {
            result.body = translateNode(body);
        }
        return result;
    }

    private hash<auto> translateCopyMethod(hash<auto> node) {
        *hash<auto> body = findType(node, "block");
        *string mods = getModifiers(node);

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Function,
            "name": {"name": "copy"},
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (exists body) {
            result.body = translateNode(body);
        }
        return result;
    }

    private hash<auto> translateMethodDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *hash<auto> returnType = findField(node, "return_type");
        *hash<auto> paramList = findType(node, "parameter_list");
        *hash<auto> body = findType(node, "block");
        *string mods = getModifiers(node);

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Function,
            "name": makeName(nameNode),
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (exists returnType) {
            result.returnType = {"nodetype": ANT_Expression, "kind": AEK_Name,
                "name": {"name": extractText(returnType)}, "loc": returnType.loc};
        }
        if (exists paramList) {
            result.params = translateExpression(paramList);
        }
        if (exists body) {
            result.body = translateNode(body);
        }
        return result;
    }

    private hash<auto> translateFunctionDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *hash<auto> returnType = findField(node, "return_type");
        *hash<auto> paramList = findType(node, "parameter_list");
        *hash<auto> body = findType(node, "block");
        *string mods = getModifiers(node);

        # Check for "returns" clause (sub func() returns type { ... })
        *hash<auto> returnsNode;
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                if (child.type == "type" && child.field == "return_type") {
                    returnsNode = child;
                    break;
                }
            }
        }

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Function,
            "name": makeName(nameNode),
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (exists returnType) {
            result.returnType = {"nodetype": ANT_Expression, "kind": AEK_Name,
                "name": {"name": extractText(returnType)}, "loc": returnType.loc};
        }
        if (exists paramList) {
            result.params = translateExpression(paramList);
        }
        if (exists body) {
            result.body = translateNode(body);
        }
        return result;
    }

    private hash<auto> translateNamespaceDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *string mods = getModifiers(node);

        # Collect namespace body declarations
        list<hash<auto>> decls = ();
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                if (child.field == "name") {
                    continue;
                }
                switch (child.type) {
                    case "class_declaration":
                    case "function_declaration":
                    case "namespace_declaration":
                    case "constant_declaration":
                    case "global_variable_declaration":
                    case "hashdecl_declaration":
                    case "typedef_declaration":
                    case "enum_declaration": {
                        *hash<auto> translated = translateNode(child);
                        if (exists translated) {
                            decls += translated;
                        }
                        break;
                    }
                }
            }
        }

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Namespace,
            "name": makeName(nameNode),
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (decls.size() > 0) {
            result.declarations = decls;
        }
        return result;
    }

    private hash<auto> translateConstantDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *hash<auto> valueNode = findField(node, "value");
        *string mods = getModifiers(node);

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Constant,
            "name": makeName(nameNode),
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (exists valueNode) {
            result.value = translateExpression(valueNode);
        }
        return result;
    }

    private hash<auto> translateGlobalVarDecl(hash<auto> node) {
        *hash<auto> typeNode = findField(node, "type");
        *string mods = getModifiers(node);

        # Find variable_declarator children
        list<hash<auto>> declarators = findAllType(node, "variable_declarator");
        if (declarators.size() == 1) {
            *hash<auto> nameNode = findField(declarators[0], "name");
            hash<auto> result = {
                "nodetype": ANT_Declaration,
                "kind": ADK_Variable,
                "name": makeName(nameNode),
                "loc": node.loc,
            };
            if (exists mods) {
                result.modifiers = mods;
            }
            if (exists typeNode) {
                result.typeName = {"name": extractText(typeNode)};
            }
            return result;
        }

        # Multiple declarators: ADK_VarList
        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_VarList,
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (exists typeNode) {
            result.typeName = {"name": extractText(typeNode)};
        }
        return result;
    }

    private hash<auto> translateHashdeclDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *string mods = getModifiers(node);

        # Collect members
        list<hash<auto>> members = ();
        list<hash<auto>> hdMembers = findAllType(node, "hashdecl_member");
        foreach hash<auto> member in (hdMembers) {
            *hash<auto> mType = findField(member, "type");
            *hash<auto> mName = findField(member, "name");
            *hash<auto> mDefault = findField(member, "default");

            hash<auto> m = {
                "nodetype": ANT_Declaration,
                "kind": ADK_HashMember,
                "name": makeName(mName),
                "loc": member.loc,
            };
            if (exists mType) {
                m.typeName = {"name": extractText(mType)};
            }
            if (exists mDefault) {
                m."init" = translateExpression(mDefault);
            }
            members += m;
        }

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Hash,
            "name": makeName(nameNode),
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (members.size() > 0) {
            result.declarations = members;
        }
        return result;
    }

    private hash<auto> translateTypedefDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *hash<auto> typeNode = findField(node, "type");
        *string mods = getModifiers(node);

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Typedef,
            "name": makeName(nameNode),
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (exists typeNode) {
            result.typeName = {"name": extractText(typeNode)};
        }
        return result;
    }

    private hash<auto> translateEnumDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *string mods = getModifiers(node);

        list<hash<auto>> members = ();
        list<hash<auto>> enumMembers = findAllType(node, "enum_member");
        foreach hash<auto> member in (enumMembers) {
            *hash<auto> mName = findField(member, "name");
            *hash<auto> mValue = findField(member, "value");

            hash<auto> m = {
                "nodetype": ANT_Declaration,
                "kind": ADK_EnumMember,
                "name": makeName(mName),
                "loc": member.loc,
            };
            if (exists mValue) {
                m.value = translateExpression(mValue);
            }
            members += m;
        }

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Enum,
            "name": makeName(nameNode),
            "loc": node.loc,
        };
        if (exists mods) {
            result.modifiers = mods;
        }
        if (members.size() > 0) {
            result.declarations = members;
        }
        return result;
    }

    private hash<auto> translateModuleDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");

        # Collect module_attribute children
        list<hash<auto>> attrs = ();
        list<hash<auto>> attrNodes = findAllType(node, "module_attribute");
        foreach hash<auto> attr in (attrNodes) {
            *hash<auto> aName = findField(attr, "name");
            *hash<auto> aValue = findField(attr, "value");
            attrs += {
                "key": exists aName ? getIdentName(aName) : "",
                "value": exists aValue ? translateExpression(aValue) : NOTHING,
            };
        }

        hash<auto> result = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Module,
            "name": makeName(nameNode),
            "loc": node.loc,
        };
        if (attrs.size() > 0) {
            result.attributes = attrs;
        }
        return result;
    }

    private hash<auto> translateParseDirective(hash<auto> node) {
        return {
            "nodetype": ANT_ParseOption,
            "loc": node.loc,
        };
    }

    # ---- Statement Translators ----

    private hash<auto> translateBlock(hash<auto> node) {
        list<hash<auto>> stmts = ();
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                *hash<auto> translated = translateNode(child);
                if (exists translated) {
                    stmts += translated;
                }
            }
        }

        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_Block,
            "loc": node.loc,
        };
        if (stmts.size() > 0) {
            result.statements = stmts;
        }
        return result;
    }

    private hash<auto> translateIfStmt(hash<auto> node) {
        *hash<auto> condNode = findField(node, "condition");
        *hash<auto> thenNode = findField(node, "consequence");
        *hash<auto> elseNode = findField(node, "alternative");

        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_If,
            "loc": node.loc,
        };
        if (exists condNode) {
            result.condition = translateExpression(condNode);
        }
        if (exists thenNode) {
            result.stmtThen = translateNode(thenNode);
        }
        if (exists elseNode) {
            result.stmtElse = translateNode(elseNode);
        }
        return result;
    }

    private hash<auto> translateWhileStmt(hash<auto> node) {
        *hash<auto> condNode = findField(node, "condition");
        *hash<auto> bodyNode = findField(node, "body");

        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_While,
            "loc": node.loc,
        };
        if (exists condNode) {
            result.condition = translateExpression(condNode);
        }
        if (exists bodyNode) {
            result.statement = translateNode(bodyNode);
        }
        return result;
    }

    private hash<auto> translateDoWhileStmt(hash<auto> node) {
        *hash<auto> condNode = findField(node, "condition");
        *hash<auto> bodyNode = findField(node, "body");

        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_DoWhile,
            "loc": node.loc,
        };
        if (exists condNode) {
            result.condition = translateExpression(condNode);
        }
        if (exists bodyNode) {
            result.statement = translateNode(bodyNode);
        }
        return result;
    }

    private hash<auto> translateForStmt(hash<auto> node) {
        *hash<auto> initNode = findField(node, "init");
        *hash<auto> condNode = findField(node, "condition");
        *hash<auto> updateNode = findField(node, "update");
        *hash<auto> bodyNode = findField(node, "body");

        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_For,
            "loc": node.loc,
        };
        if (exists initNode) {
            result."init" = translateExpression(initNode);
        }
        if (exists condNode) {
            result.condition = translateExpression(condNode);
        }
        if (exists updateNode) {
            result.iteration = translateExpression(updateNode);
        }
        if (exists bodyNode) {
            result.statement = translateNode(bodyNode);
        }
        return result;
    }

    private hash<auto> translateForeachStmt(hash<auto> node) {
        *hash<auto> typeNode = findField(node, "type");
        *hash<auto> varNode = findField(node, "variable");
        *hash<auto> bodyNode = findField(node, "body");
        # Source expression: the child with no field attribute.
        # getNodesInfo() returns only named children: type(field), variable(field), expr(no field), body(field)
        # The source expression is the only child without a field assignment.
        *hash<auto> sourceNode;
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                if (!exists child.field) {
                    sourceNode = child;
                    break;
                }
            }
        }

        # Build the value expression (type + var or just var)
        # AEK_Decl loc must cover "type var" (e.g. "int v"), not just the variable
        hash<auto> valueLoc;
        if (exists typeNode && exists varNode) {
            valueLoc = {
                "start_line": typeNode.loc.start_line,
                "start_column": typeNode.loc.start_column,
                "end_line": varNode.loc.end_line,
                "end_column": varNode.loc.end_column,
            };
        } else if (exists varNode) {
            valueLoc = varNode.loc;
        } else {
            valueLoc = node.loc;
        }
        hash<auto> valueExpr = {
            "nodetype": ANT_Expression,
            "kind": AEK_Decl,
            "loc": valueLoc,
        };

        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_Foreach,
            "value": valueExpr,
            "loc": node.loc,
        };
        if (exists sourceNode) {
            result.source = translateExpression(sourceNode);
        }
        if (exists bodyNode) {
            result.statement = translateNode(bodyNode);
        }
        return result;
    }

    private hash<auto> translateSwitchStmt(hash<auto> node) {
        *hash<auto> valueNode = findField(node, "value");

        # Collect switch cases
        list<hash<auto>> cases = ();
        list<hash<auto>> switchCases = findAllType(node, "switch_case");
        list<hash<auto>> defaultCases = findAllType(node, "default_case");

        foreach hash<auto> sc in (switchCases) {
            *hash<auto> caseValue = findField(sc, "value");
            # Collect statements inside the case
            list<hash<auto>> caseStmts = ();
            if (exists sc.children) {
                foreach hash<auto> child in (sc.children) {
                    if (child.field != "value" && child.type != "case_comparison") {
                        *hash<auto> translated = translateNode(child);
                        if (exists translated) {
                            caseStmts += translated;
                        }
                    }
                }
            }

            hash<auto> caseExpr;
            if (exists caseValue) {
                caseExpr = translateExpression(caseValue);
            }

            hash<auto> caseNode = {
                "nodetype": ANT_Expression,
                "kind": AEK_Case,
                "defaultCase": False,
                "loc": sc.loc,
            };
            if (exists caseExpr) {
                caseNode.caseExpr = caseExpr;
            }
            if (caseStmts.size() > 0) {
                caseNode.statements = {
                    "nodetype": ANT_Statement,
                    "kind": ASK_Block,
                    "statements": caseStmts,
                    "loc": sc.loc,
                };
            }
            cases += caseNode;
        }

        foreach hash<auto> dc in (defaultCases) {
            list<hash<auto>> defaultStmts = ();
            if (exists dc.children) {
                foreach hash<auto> child in (dc.children) {
                    *hash<auto> translated = translateNode(child);
                    if (exists translated) {
                        defaultStmts += translated;
                    }
                }
            }

            hash<auto> defaultNode = {
                "nodetype": ANT_Expression,
                "kind": AEK_Case,
                "defaultCase": True,
                "loc": dc.loc,
            };
            if (defaultStmts.size() > 0) {
                defaultNode.statements = {
                    "nodetype": ANT_Statement,
                    "kind": ASK_Block,
                    "statements": defaultStmts,
                    "loc": dc.loc,
                };
            }
            cases += defaultNode;
        }

        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_Switch,
            "loc": node.loc,
        };
        if (exists valueNode) {
            result.variable = translateExpression(valueNode);
        }
        if (cases.size() > 0) {
            result.body = {
                "nodetype": ANT_Expression,
                "kind": AEK_SwitchBody,
                "cases": cases,
                "loc": node.loc,
            };
        }
        return result;
    }

    private hash<auto> translateTryStmt(hash<auto> node) {
        # try has a block child and catch_clause children
        *hash<auto> tryBody;
        *hash<auto> catchClause;

        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                if (child.type == "block" && !exists tryBody) {
                    tryBody = child;
                } else if (child.type == "catch_clause") {
                    catchClause = child;
                }
            }
        }

        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_Try,
            "loc": node.loc,
        };
        if (exists tryBody) {
            result.tryStmt = translateNode(tryBody);
        }
        if (exists catchClause) {
            *hash<auto> catchType = findField(catchClause, "type");
            *hash<auto> catchParam = findField(catchClause, "parameter");
            *hash<auto> catchBody = findField(catchClause, "body");

            # Build catch variable as decl expression "type name"
            string catchVarStr = "";
            if (exists catchType) {
                catchVarStr = extractText(catchType);
            }
            if (exists catchParam) {
                if (catchVarStr != "") {
                    catchVarStr += " ";
                }
                catchVarStr += getIdentName(catchParam);
            }

            result.catchVar = {
                "nodetype": ANT_Expression,
                "kind": AEK_Decl,
                "loc": exists catchParam ? catchParam.loc : catchClause.loc,
            };

            if (exists catchBody) {
                result.catchStmt = translateNode(catchBody);
            }
        }
        return result;
    }

    private hash<auto> translateReturnStmt(hash<auto> node) {
        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_Return,
            "loc": node.loc,
        };
        # Return value is any child that's an expression
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                # Skip the "return" keyword token - use the expression child
                *hash<auto> expr = translateExpression(child);
                if (exists expr) {
                    result.retval = expr;
                    break;
                }
            }
        }
        return result;
    }

    private hash<auto> translateThrowStmt(hash<auto> node) {
        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_Throw,
            "loc": node.loc,
        };

        # Throw has error, description, arg fields
        *hash<auto> errorNode = findField(node, "error");
        *hash<auto> descNode = findField(node, "description");
        *hash<auto> argNode = findField(node, "arg");

        # Build as a list expression for multiple throw args
        list<hash<auto>> throwArgs = ();
        if (exists errorNode) {
            *hash<auto> errorExpr = translateExpression(errorNode);
            if (exists errorExpr) {
                throwArgs += errorExpr;
            }
        }
        if (exists descNode) {
            *hash<auto> descExpr = translateExpression(descNode);
            if (exists descExpr) {
                throwArgs += descExpr;
            }
        }
        if (exists argNode) {
            *hash<auto> argExpr = translateExpression(argNode);
            if (exists argExpr) {
                throwArgs += argExpr;
            }
        }

        if (throwArgs.size() > 0) {
            if (throwArgs.size() == 1) {
                result.expression = throwArgs[0];
            } else {
                result.expression = {
                    "nodetype": ANT_Expression,
                    "kind": AEK_List,
                    "elements": throwArgs,
                    "loc": node.loc,
                };
            }
        }
        return result;
    }

    private hash<auto> translateOnExitStmt(hash<auto> node) {
        # Determine condition: on_exit=0, on_success=1, on_error=2
        int condition = 0;
        string srcText = extractText(node);
        if (srcText =~ /^on_success/) {
            condition = 1;
        } else if (srcText =~ /^on_error/) {
            condition = 2;
        }

        # Find the body block
        *hash<auto> body = findType(node, "block");

        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_OnBlockExit,
            "condition": condition,
            "loc": node.loc,
        };
        if (exists body) {
            result.statement = translateNode(body);
        }
        return result;
    }

    private hash<auto> translateExpressionStmt(hash<auto> node) {
        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_Expression,
            "loc": node.loc,
        };

        # The expression is the first child
        if (exists node.children && node.children.size() > 0) {
            result.expression = translateExpression(node.children[0]);
        }
        return result;
    }

    private hash<auto> translateLocalVarDecl(hash<auto> node) {
        # Local variable declaration: treated as expression statement with decl expr
        hash<auto> result = {
            "nodetype": ANT_Statement,
            "kind": ASK_Expression,
            "loc": node.loc,
        };
        result.expression = translateLocalVarDeclExpr(node);
        return result;
    }

    # ---- Expression Translators ----

    private hash<auto> translateBinaryExpr(hash<auto> node) {
        # binary_expression: [left, operator_token, right] — operator is anonymous child
        *hash<auto> leftNode;
        *hash<auto> rightNode;
        if (exists node.children && node.children.size() >= 2) {
            leftNode = node.children[0];
            rightNode = node.children[node.children.size() - 1];
        }

        # Get operator from anonymous child between left and right (children[1].type)
        string op;
        if (exists node.children && node.children.size() >= 3) {
            op = node.children[1].type;
        } else {
            # Fallback: extract from source text
            op = extractBinaryOp(node, leftNode, rightNode);
        }

        # Map operator symbol to AOK_ name
        string opName = mapBinaryOp(op);

        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Binary,
            "op": opName,
            "loc": node.loc,
        };
        if (exists leftNode) {
            result.left = translateExpression(leftNode);
        }
        if (exists rightNode) {
            result.right = translateExpression(rightNode);
        }
        return result;
    }

    private string mapBinaryOp(string op) {
        switch (op) {
            case "+": return "AOK_Plus";
            case "-": return "AOK_Minus";
            case "*": return "AOK_Multiply";
            case "/": return "AOK_Divide";
            case "%": return "AOK_Modulo";
            case "&": return "AOK_BinaryAnd";
            case "|": return "AOK_BinaryOr";
            case "^": return "AOK_BinaryXor";
            case "<<": return "AOK_ShiftLeft";
            case ">>": return "AOK_ShiftRight";
            case "==": return "AOK_Equals";
            case "!=": return "AOK_NotEquals";
            case "<=>": return "AOK_Comparison";
            case ">": return "AOK_GreaterThan";
            case ">=": return "AOK_GreaterThanOrEquals";
            case "<": return "AOK_LessThan";
            case "<=": return "AOK_LessThanOrEquals";
            case "===": return "AOK_AbsoluteEquals";
            case "!==": return "AOK_AbsoluteNotEquals";
            case "&&": return "AOK_LogicalAnd";
            case "||": return "AOK_LogicalOr";
            case "??": return "AOK_NullCoalesce";
            case "?*": return "AOK_ValueCoalesce";
            case "=~": return "AOK_RegexMatch";
            case "!~": return "AOK_RegexNotMatch";
            case "instanceof": return "AOK_Instanceof";
        }
        return op;
    }

    private hash<auto> translateUnaryExpr(hash<auto> node) {
        *hash<auto> operandNode = findField(node, "operand");
        *hash<auto> opNode = findField(node, "operator");

        string op = "";
        if (exists opNode) {
            op = exists opNode.text ? opNode.text : extractText(opNode);
        } else if (exists operandNode) {
            # operator is anonymous token — extract from source between node start and operand start
            int opLine = node.loc.start_line;
            int opCol = node.loc.start_column;
            int endLine = operandNode.loc.start_line;
            int endCol = operandNode.loc.start_column;
            if (opLine == endLine && opLine < sourceLines.size()) {
                string line = sourceLines[opLine];
                if (opCol < line.size() && endCol <= line.size()) {
                    op = trim(line.substr(opCol, endCol - opCol));
                }
            }
        }

        string opName = mapUnaryOp(op);

        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Unary,
            "op": opName,
            "loc": node.loc,
        };
        if (exists operandNode) {
            result.expression = translateExpression(operandNode);
        }
        return result;
    }

    private hash<auto> translatePostfixExpr(hash<auto> node) {
        *hash<auto> operandNode = findField(node, "operand");
        *hash<auto> opNode = findField(node, "operator");

        string op = "";
        if (exists opNode) {
            op = exists opNode.text ? opNode.text : extractText(opNode);
        } else if (exists operandNode) {
            # operator is anonymous token — extract from source between operand end and node end
            int opLine = operandNode.loc.end_line;
            int opCol = operandNode.loc.end_column;
            int endLine = node.loc.end_line;
            int endCol = node.loc.end_column;
            if (opLine == endLine && opLine < sourceLines.size()) {
                string line = sourceLines[opLine];
                if (opCol < line.size() && endCol <= line.size()) {
                    op = trim(line.substr(opCol, endCol - opCol));
                }
            }
        }

        string opName;
        if (op == "++") {
            opName = "AOK_PostIncrement";
        } else if (op == "--") {
            opName = "AOK_PostDecrement";
        } else {
            opName = op;
        }

        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Unary,
            "op": opName,
            "loc": node.loc,
        };
        if (exists operandNode) {
            result.expression = translateExpression(operandNode);
        }
        return result;
    }

    private string mapUnaryOp(string op) {
        switch (op) {
            case "!": return "AOK_LogicalNot";
            case "~": return "AOK_BinaryNot";
            case "-": return "AOK_UnaryMinus";
            case "+": return "AOK_UnaryPlus";
            case "++": return "AOK_PreIncrement";
            case "--": return "AOK_PreDecrement";
            case "\\": return "AOK_Reference";
            case "background": return "AOK_Background";
            case "chomp": return "AOK_Chomp";
            case "delete": return "AOK_Delete";
            case "elements": return "AOK_Elements";
            case "exists": return "AOK_Exists";
            case "keys": return "AOK_Keys";
            case "pop": return "AOK_Pop";
            case "remove": return "AOK_Remove";
            case "shift": return "AOK_Shift";
            case "trim": return "AOK_Trim";
            case "new": return "AOK_New";
        }
        return op;
    }

    private hash<auto> translateAssignmentExpr(hash<auto> node) {
        *hash<auto> leftNode = findField(node, "left");
        *hash<auto> rightNode = findField(node, "right");
        *hash<auto> opNode = findField(node, "operator");

        string op = "=";
        if (exists opNode && exists opNode.text) {
            op = opNode.text;
        } else if (exists leftNode && exists rightNode) {
            # operator is anonymous — extract from source between left end and right start
            op = extractBinaryOp(node, leftNode, rightNode);
        }

        # Simple assignment uses AEK_Assignment, compound uses AEK_Binary
        if (op == "=" || op == ":=") {
            hash<auto> result = {
                "nodetype": ANT_Expression,
                "kind": AEK_Assignment,
                "loc": node.loc,
            };
            if (exists leftNode) {
                result.left = translateExpression(leftNode);
            }
            if (exists rightNode) {
                result.right = translateExpression(rightNode);
            }
            return result;
        }

        # Compound assignment (+=, -=, etc) - treated as binary
        string opName = mapCompoundAssignOp(op);
        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Binary,
            "op": opName,
            "loc": node.loc,
        };
        if (exists leftNode) {
            result.left = translateExpression(leftNode);
        }
        if (exists rightNode) {
            result.right = translateExpression(rightNode);
        }
        return result;
    }

    private string mapCompoundAssignOp(string op) {
        switch (op) {
            case "+=": return "AOK_PlusEquals";
            case "-=": return "AOK_MinusEquals";
            case "*=": return "AOK_MultiplyEquals";
            case "/=": return "AOK_DivideEquals";
            case "%=": return "AOK_ModuloEquals";
            case "&=": return "AOK_AndEquals";
            case "|=": return "AOK_OrEquals";
            case "^=": return "AOK_XorEquals";
            case "<<=": return "AOK_ShiftLeftEquals";
            case ">>=": return "AOK_ShiftRightEquals";
        }
        return op;
    }

    private hash<auto> translateCallExpr(hash<auto> node) {
        *hash<auto> funcNode = findField(node, "function");

        # Arguments are in the argument_list child
        *hash<auto> argList = findType(node, "argument_list");

        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Call,
            "loc": node.loc,
        };
        if (exists funcNode) {
            result.target = translateExpression(funcNode);
        }
        if (exists argList) {
            result.args = translateExpression(argList);
        }
        return result;
    }

    private hash<auto> translateMemberExpr(hash<auto> node) {
        *hash<auto> objNode = findField(node, "object");
        *hash<auto> memberNode = findField(node, "member");

        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Access,
            "loc": node.loc,
        };
        if (exists objNode) {
            result.variable = translateExpression(objNode);
        }
        if (exists memberNode) {
            result.member = translateExpression(memberNode);
        }
        return result;
    }

    private hash<auto> translateIndexExpr(hash<auto> node) {
        *hash<auto> objNode = findField(node, "object");
        *hash<auto> indexNode = findField(node, "index");
        *hash<auto> keyNode = findField(node, "key");
        *hash<auto> rangeStartNode = findField(node, "range_start");
        *hash<auto> rangeEndNode = findField(node, "range_end");

        # Range subscript: list[start..end], list[start..], list[..end], list[..]
        # For list[..] both range_start and range_end are absent; detect via source text
        bool isRange = exists rangeStartNode || exists rangeEndNode;
        if (!isRange && !exists indexNode && !exists keyNode) {
            # Check source text for ".." to detect bare range [..]
            string srcText = extractText(node);
            if (srcText =~ /\[\.\./) {
                isRange = True;
            }
        }
        if (isRange) {
            hash<auto> rangeExpr = {
                "nodetype": ANT_Expression,
                "kind": AEK_Range,
                "loc": node.loc,
            };
            if (exists rangeStartNode) {
                rangeExpr.left = translateExpression(rangeStartNode);
            }
            if (exists rangeEndNode) {
                rangeExpr.right = translateExpression(rangeEndNode);
            }
            hash<auto> result = {
                "nodetype": ANT_Expression,
                "kind": AEK_Index,
                "indexKind": 0,
                "loc": node.loc,
            };
            if (exists objNode) {
                result.variable = translateExpression(objNode);
            }
            result.index = rangeExpr;
            return result;
        }

        # Determine index kind: [] = 0 (list), {} = 1 (hash)
        int indexKind = 0;
        if (exists keyNode) {
            indexKind = 1;
        }

        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Index,
            "indexKind": indexKind,
            "loc": node.loc,
        };
        if (exists objNode) {
            result.variable = translateExpression(objNode);
        }
        if (exists keyNode) {
            result.index = translateExpression(keyNode);
        } else if (exists indexNode) {
            result.index = translateExpression(indexNode);
        }
        return result;
    }

    private hash<auto> translateTernaryExpr(hash<auto> node) {
        *hash<auto> condNode = findField(node, "condition");
        *hash<auto> consNode = findField(node, "consequence");
        *hash<auto> altNode = findField(node, "alternative");

        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Ternary,
            "loc": node.loc,
        };
        if (exists condNode) {
            result.condition = translateExpression(condNode);
        }
        if (exists consNode) {
            result.exprTrue = translateExpression(consNode);
        }
        if (exists altNode) {
            result.exprFalse = translateExpression(altNode);
        }
        return result;
    }

    private hash<auto> translateCastExpr(hash<auto> node) {
        *hash<auto> typeNode = findField(node, "type");
        *hash<auto> valueNode = findField(node, "value");

        # Distinguish cast<Type>(expr) from <Type>expr (hashdecl hash init)
        # Check if source starts with "cast" or "<"
        bool isHashdeclHash = False;
        int startLine = node.loc.start_line;
        int startCol = node.loc.start_column;
        if (startLine < sourceLines.size()) {
            string line = sourceLines[startLine];
            if (startCol < line.size()) {
                isHashdeclHash = line[startCol] == "<";
            }
        }

        if (isHashdeclHash) {
            # <Type>{...} hashdecl hash init
            hash<auto> result = {
                "nodetype": ANT_Expression,
                "kind": AEK_HashdeclHash,
                "loc": node.loc,
            };
            if (exists typeNode) {
                result."hashdecl" = {"name": extractText(typeNode)};
            }
            if (exists valueNode) {
                result."hash" = translateExpression(valueNode);
            }
            return result;
        }

        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Cast,
            "loc": node.loc,
        };
        if (exists typeNode) {
            result.castType = {"name": extractText(typeNode)};
        }
        if (exists valueNode) {
            result.obj = translateExpression(valueNode);
        }
        return result;
    }

    private *hash<auto> translateParenExpr(hash<auto> node) {
        # Unwrap parenthesized expression - pass through to inner expression
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                return translateExpression(child);
            }
        }
    }

    private *hash<auto> translatePrimaryExpr(hash<auto> node) {
        # Primary expression wraps a single child - pass through
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                return translateExpression(child);
            }
        }
        if (exists node.text) {
            return {"nodetype": ANT_Expression, "kind": AEK_Name,
                "name": {"name": node.text}, "loc": node.loc};
        }
    }

    private hash<auto> translateClosureExpr(hash<auto> node) {
        *hash<auto> returnType = findField(node, "return_type");
        *hash<auto> paramList = findType(node, "parameter_list");
        *hash<auto> body = findType(node, "block");

        hash<auto> closureDecl = {
            "nodetype": ANT_Declaration,
            "kind": ADK_Closure,
            "loc": node.loc,
        };
        if (exists returnType) {
            closureDecl.returnType = {"nodetype": ANT_Expression, "kind": AEK_Name,
                "name": {"name": extractText(returnType)}, "loc": returnType.loc};
        }
        if (exists paramList) {
            closureDecl.params = translateExpression(paramList);
        }
        if (exists body) {
            closureDecl.body = translateNode(body);
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Closure,
            "closure": closureDecl,
            "loc": node.loc,
        };
    }

    private hash<auto> translateListLiteral(hash<auto> node) {
        list<hash<auto>> elems = ();
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                *hash<auto> expr = translateExpression(child);
                if (exists expr) {
                    elems += expr;
                }
            }
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_List,
            "elements": elems,
            "loc": node.loc,
        };
    }

    private hash<auto> translateHashLiteral(hash<auto> node, int hashKind) {
        list<hash<auto>> elems = ();
        list<hash<auto>> entries = findAllType(node, "hash_entry");
        foreach hash<auto> entry in (entries) {
            *hash<auto> translated = translateHashEntry(entry);
            if (exists translated) {
                elems += translated;
            }
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Hash,
            "hashKind": hashKind,
            "elements": elems,
            "loc": node.loc,
        };
    }

    private hash<auto> translateHashEntry(hash<auto> node) {
        *hash<auto> keyNode = findField(node, "key");
        *hash<auto> valueNode = findField(node, "value");

        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_HashElement,
            "loc": node.loc,
        };
        if (exists keyNode) {
            result.key = translateExpression(keyNode);
        }
        if (exists valueNode) {
            result.value = translateExpression(valueNode);
        }
        return result;
    }

    private hash<auto> translateLiteral(hash<auto> node) {
        # literal contains a child (boolean, null, nothing, integer, float, etc.)
        if (exists node.children && node.children.size() > 0) {
            return translateExpression(node.children[0]);
        }
        if (exists node.text) {
            # Determine literal kind from text
            if (node.text == "True" || node.text == "False") {
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "bool",
                    "value": node.text == "True", "loc": node.loc};
            }
            if (node.text == "NOTHING") {
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "nothing",
                    "loc": node.loc};
            }
            if (node.text == "NULL") {
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "null",
                    "loc": node.loc};
            }
            if (node.text == "...") {
                return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "ellipses",
                    "loc": node.loc};
            }
            return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "string",
                "value": node.text, "loc": node.loc};
        }
        return {"nodetype": ANT_Expression, "kind": AEK_Literal, "loc": node.loc};
    }

    private hash<auto> translateStringLiteral(hash<auto> node) {
        string text = extractText(node);
        # Strip quotes to get value
        string value = text;
        if (value.size() >= 2) {
            value = value.substr(1, value.size() - 2);
        }
        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Literal,
            "literalKind": "string",
            "value": value,
            "loc": node.loc,
        };
    }

    private hash<auto> translateStringConcat(hash<auto> node) {
        # Flatten the left-recursive string_concatenation tree into a flat list of parts
        list<hash<auto>> parts = flattenStringConcatParts(node);
        if (parts.size() < 2) {
            # Single part — just return it directly
            if (parts.size() == 1) {
                return parts[0];
            }
            return {"nodetype": ANT_Expression, "kind": AEK_Literal, "literalKind": "string",
                "value": extractText(node), "loc": node.loc};
        }
        return {
            "nodetype": ANT_Expression,
            "kind": AEK_StringConcat,
            "parts": parts,
            "loc": node.loc,
        };
    }

    #! Flatten a string_concatenation CST tree into a flat list of expression nodes
    private list<hash<auto>> flattenStringConcatParts(hash<auto> node) {
        list<hash<auto>> result = ();
        if (!exists node.children || node.children.size() < 2) {
            *hash<auto> translated = translateExpression(node);
            if (exists translated) {
                result += translated;
            }
            return result;
        }
        # Left child may be another string_concatenation (left-recursive)
        hash<auto> leftChild = node.children[0];
        if (leftChild.type == "string_concatenation") {
            result = flattenStringConcatParts(leftChild);
        } else {
            *hash<auto> translated = translateExpression(leftChild);
            if (exists translated) {
                result += translated;
            }
        }
        # Right child is the new string part
        hash<auto> rightChild = node.children[node.children.size() - 1];
        *hash<auto> translated = translateExpression(rightChild);
        if (exists translated) {
            result += translated;
        }
        return result;
    }

    private hash<auto> translateRegex(hash<auto> node) {
        # regex node wraps a child: regex_literal, regex_subst, regex_trans, regex_extract
        if (exists node.children && node.children.size() > 0) {
            return translateExpression(node.children[0]);
        }
        # Fallback
        return {"nodetype": ANT_Expression, "kind": AEK_Regex,
            "str": extractText(node), "loc": node.loc,
            "caseSensitive": True, "extended": False, "dotAll": False,
            "multiline": False, "global": False, "extractRegex": False};
    }

    private hash<auto> translateRegexLiteral(hash<auto> node) {
        string text = extractText(node);
        # Parse: /pattern/flags or x/pattern/flags
        bool isExtract = text =~ /^x\//;
        string rest = isExtract ? text.substr(2) : text.substr(1);

        # Find the last /
        int lastSlash = rest.size() - 1;
        while (lastSlash >= 0 && rest[lastSlash] != "/") {
            --lastSlash;
        }

        string pattern = "";
        string flags = "";
        if (lastSlash > 0) {
            pattern = rest.substr(0, lastSlash);
            flags = rest.substr(lastSlash + 1);
        }

        # Unescape forward slashes in pattern
        pattern = regex_subst(pattern, "\\\\/", "/", RE_Global);

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Regex,
            "str": pattern,
            "caseSensitive": flags !~ /i/,
            "extended": (flags =~ /x/) ? True : False,
            "dotAll": (flags =~ /s/) ? True : False,
            "multiline": (flags =~ /m/) ? True : False,
            "global": (flags =~ /g/) ? True : False,
            "extractRegex": isExtract,
            "loc": node.loc,
        };
    }

    private hash<auto> translateRegexSubst(hash<auto> node) {
        string text = extractText(node);
        # Parse: s/source/target/flags
        if (text =~ /^s\//) {
            string rest = text.substr(2);
            # Split by unescaped /
            list<string> parts = splitRegexParts(rest);
            string source = parts.size() > 0 ? parts[0] : "";
            string target = parts.size() > 1 ? parts[1] : "";
            string flags = parts.size() > 2 ? parts[2] : "";

            source = regex_subst(source, "\\\\/", "/", RE_Global);
            target = regex_subst(target, "\\\\/", "/", RE_Global);

            return {
                "nodetype": ANT_Expression,
                "kind": AEK_RegexSubst,
                "source": source,
                "target": target,
                "caseSensitive": flags !~ /i/,
                "extended": (flags =~ /x/) ? True : False,
                "dotAll": (flags =~ /s/) ? True : False,
                "multiline": (flags =~ /m/) ? True : False,
                "global": (flags =~ /g/) ? True : False,
                "loc": node.loc,
            };
        }
        return {"nodetype": ANT_Expression, "kind": AEK_RegexSubst, "source": text,
            "target": "", "loc": node.loc, "caseSensitive": True, "extended": False,
            "dotAll": False, "multiline": False, "global": False};
    }

    private hash<auto> translateRegexTrans(hash<auto> node) {
        string text = extractText(node);
        # Parse: tr/source/target/
        if (text =~ /^tr\//) {
            string rest = text.substr(3);
            list<string> parts = splitRegexParts(rest);
            string source = parts.size() > 0 ? parts[0] : "";
            string target = parts.size() > 1 ? parts[1] : "";

            source = regex_subst(source, "\\\\/", "/", RE_Global);
            target = regex_subst(target, "\\\\/", "/", RE_Global);

            return {
                "nodetype": ANT_Expression,
                "kind": AEK_RegexTrans,
                "source": source,
                "target": target,
                "loc": node.loc,
            };
        }
        return {"nodetype": ANT_Expression, "kind": AEK_RegexTrans,
            "source": text, "target": "", "loc": node.loc};
    }

    private hash<auto> translateRegexExtract(hash<auto> node) {
        # x/pattern/flags is the extract regex - translate same as regex_literal but with extract=True
        return translateRegexLiteral(node);
    }

    #! Split a regex string by unescaped / delimiters
    private list<string> splitRegexParts(string text) {
        list<string> parts = ();
        string current = "";
        bool escaped = False;
        for (int i = 0; i < text.size(); ++i) {
            if (escaped) {
                current += text[i];
                escaped = False;
            } else if (text[i] == "\\") {
                current += text[i];
                escaped = True;
            } else if (text[i] == "/") {
                parts += current;
                current = "";
            } else {
                current += text[i];
            }
        }
        if (current != "") {
            parts += current;
        }
        return parts;
    }

    private hash<auto> translateNewExpr(hash<auto> node) {
        *hash<auto> typeNode = findField(node, "type");

        # Arguments in argument_list
        *hash<auto> argList = findType(node, "argument_list");

        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Unary,
            "op": "AOK_New",
            "loc": node.loc,
        };

        # Build the expression as a call expression
        hash<auto> callExpr = {
            "nodetype": ANT_Expression,
            "kind": AEK_Call,
            "target": {"nodetype": ANT_Expression, "kind": AEK_Name,
                "name": {"name": exists typeNode ? extractText(typeNode) : ""},
                "loc": exists typeNode ? typeNode.loc : node.loc},
            "loc": node.loc,
        };
        if (exists argList) {
            callExpr.args = translateExpression(argList);
        }
        result.expression = callExpr;
        return result;
    }

    private hash<auto> translateMapExpr(hash<auto> node) {
        # map expr, list [, filter] — keyword prefix with comma-separated args
        # If expression spans multiple lines, fall back to source text to preserve indentation
        if (node.loc.start_line != node.loc.end_line) {
            return {"nodetype": ANT_Expression, "kind": AEK_Decl, "loc": node.loc};
        }
        # Extract args text (everything after "map " keyword)
        string argsText;
        if (exists node.children && node.children.size() > 0) {
            hash<auto> firstChild = node.children[0];
            hash<auto> argsLoc = {
                "start_line": firstChild.loc.start_line,
                "start_column": firstChild.loc.start_column,
                "end_line": node.loc.end_line,
                "end_column": node.loc.end_column,
            };
            argsText = extractText({"loc": argsLoc});
        } else {
            argsText = extractText(node);
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Unary,
            "op": "AOK_Map",
            "expression": {"nodetype": ANT_Expression, "kind": AEK_Name,
                "name": {"name": argsText}, "loc": node.loc},
            "loc": node.loc,
        };
    }

    private hash<auto> translateSelectExpr(hash<auto> node) {
        # select listExpr, filterExpr — keyword prefix with comma-separated args
        if (node.loc.start_line != node.loc.end_line) {
            return {"nodetype": ANT_Expression, "kind": AEK_Decl, "loc": node.loc};
        }
        string argsText;
        if (exists node.children && node.children.size() > 0) {
            hash<auto> firstChild = node.children[0];
            hash<auto> argsLoc = {
                "start_line": firstChild.loc.start_line,
                "start_column": firstChild.loc.start_column,
                "end_line": node.loc.end_line,
                "end_column": node.loc.end_column,
            };
            argsText = extractText({"loc": argsLoc});
        } else {
            argsText = extractText(node);
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Unary,
            "op": "AOK_Select",
            "expression": {"nodetype": ANT_Expression, "kind": AEK_Name,
                "name": {"name": argsText}, "loc": node.loc},
            "loc": node.loc,
        };
    }

    private hash<auto> translateFoldlExpr(hash<auto> node) {
        # foldl expr, list — keyword prefix with comma-separated args
        if (node.loc.start_line != node.loc.end_line) {
            return {"nodetype": ANT_Expression, "kind": AEK_Decl, "loc": node.loc};
        }
        string argsText;
        if (exists node.children && node.children.size() > 0) {
            hash<auto> firstChild = node.children[0];
            hash<auto> argsLoc = {
                "start_line": firstChild.loc.start_line,
                "start_column": firstChild.loc.start_column,
                "end_line": node.loc.end_line,
                "end_column": node.loc.end_column,
            };
            argsText = extractText({"loc": argsLoc});
        } else {
            argsText = extractText(node);
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Unary,
            "op": "AOK_Foldl",
            "expression": {"nodetype": ANT_Expression, "kind": AEK_Name,
                "name": {"name": argsText}, "loc": node.loc},
            "loc": node.loc,
        };
    }

    private hash<auto> translateFoldrExpr(hash<auto> node) {
        # foldr expr, list — keyword prefix with comma-separated args
        if (node.loc.start_line != node.loc.end_line) {
            return {"nodetype": ANT_Expression, "kind": AEK_Decl, "loc": node.loc};
        }
        string argsText;
        if (exists node.children && node.children.size() > 0) {
            hash<auto> firstChild = node.children[0];
            hash<auto> argsLoc = {
                "start_line": firstChild.loc.start_line,
                "start_column": firstChild.loc.start_column,
                "end_line": node.loc.end_line,
                "end_column": node.loc.end_column,
            };
            argsText = extractText({"loc": argsLoc});
        } else {
            argsText = extractText(node);
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Unary,
            "op": "AOK_Foldr",
            "expression": {"nodetype": ANT_Expression, "kind": AEK_Name,
                "name": {"name": argsText}, "loc": node.loc},
            "loc": node.loc,
        };
    }

    private hash<auto> translateMultiArgExpr(hash<auto> node) {
        # push, unshift, splice, extract - all become unary with the operator name
        string opName;
        switch (node.type) {
            case "push_expression": opName = "AOK_Push"; break;
            case "unshift_expression": opName = "AOK_Unshift"; break;
            case "splice_expression": opName = "AOK_Splice"; break;
            case "extract_expression": opName = "AOK_Extract"; break;
            default: opName = node.type; break;
        }

        # Extract args text (everything after the keyword) from first child to node end
        string argsText;
        if (exists node.children && node.children.size() > 0) {
            hash<auto> firstChild = node.children[0];
            hash<auto> argsLoc = {
                "start_line": firstChild.loc.start_line,
                "start_column": firstChild.loc.start_column,
                "end_line": node.loc.end_line,
                "end_column": node.loc.end_column,
            };
            argsText = extractText({"loc": argsLoc});
        } else {
            argsText = extractText(node);
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Unary,
            "op": opName,
            "expression": {"nodetype": ANT_Expression, "kind": AEK_Name,
                "name": {"name": argsText}, "loc": node.loc},
            "loc": node.loc,
        };
    }

    private hash<auto> translateSliceExpr(hash<auto> node) {
        *hash<auto> objNode = findField(node, "object");
        *hash<auto> rangeStart = findField(node, "range_start");
        *hash<auto> rangeEnd = findField(node, "range_end");

        hash<auto> rangeExpr = {
            "nodetype": ANT_Expression,
            "kind": AEK_Range,
            "loc": node.loc,
        };
        if (exists rangeStart) {
            rangeExpr.left = translateExpression(rangeStart);
        }
        if (exists rangeEnd) {
            rangeExpr.right = translateExpression(rangeEnd);
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Index,
            "indexKind": 0,
            "variable": exists objNode ? translateExpression(objNode) : NOTHING,
            "index": rangeExpr,
            "loc": node.loc,
        };
    }

    private hash<auto> translateParameterList(hash<auto> node) {
        list<hash<auto>> params = ();
        list<hash<auto>> paramNodes = findAllType(node, "parameter");
        foreach hash<auto> p in (paramNodes) {
            *hash<auto> translated = translateParameter(p);
            if (exists translated) {
                params += translated;
            }
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_List,
            "elements": params,
            "loc": node.loc,
        };
    }

    private hash<auto> translateParameter(hash<auto> node) {
        *hash<auto> typeNode = findField(node, "type");
        *hash<auto> nameNode = findField(node, "name");
        *hash<auto> defaultNode = findField(node, "default");

        # Build parameter as a DeclExpr with optional assignment
        string typeStr = "";
        if (exists typeNode) {
            typeStr = extractText(typeNode);
        }
        string nameStr = "";
        if (exists nameNode) {
            nameStr = getIdentName(nameNode);
        }

        if (exists defaultNode) {
            # Parameter with default: type name = value → Assignment(Decl, value)
            # AEK_Decl loc must cover "type name" only, not the "= value" part
            hash<auto> declLoc;
            if (exists typeNode && exists nameNode) {
                declLoc = {
                    "start_line": typeNode.loc.start_line,
                    "start_column": typeNode.loc.start_column,
                    "end_line": nameNode.loc.end_line,
                    "end_column": nameNode.loc.end_column,
                };
            } else if (exists nameNode) {
                declLoc = nameNode.loc;
            } else {
                declLoc = node.loc;
            }
            hash<auto> declExpr = {
                "nodetype": ANT_Expression,
                "kind": AEK_Decl,
                "loc": declLoc,
            };
            if (typeStr.val()) {
                declExpr.typeName = {"name": typeStr};
            }
            if (nameStr.val()) {
                declExpr.name = {"name": nameStr};
            }

            hash<auto> result = {
                "nodetype": ANT_Expression,
                "kind": AEK_Assignment,
                "left": declExpr,
                "right": translateExpression(defaultNode),
                "loc": node.loc,
            };
            return result;
        }

        # Simple parameter: type name → DeclExpr
        hash<auto> result = {
            "nodetype": ANT_Expression,
            "kind": AEK_Decl,
            "loc": node.loc,
        };
        if (typeStr.val()) {
            result.typeName = {"name": typeStr};
        }
        if (nameStr.val()) {
            result.name = {"name": nameStr};
        }
        return result;
    }

    private hash<auto> translateArgumentList(hash<auto> node) {
        list<hash<auto>> args = ();
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                *hash<auto> expr = translateExpression(child);
                if (exists expr) {
                    args += expr;
                }
            }
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_List,
            "elements": args,
            "loc": node.loc,
        };
    }

    private hash<auto> translateVarDeclarator(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *hash<auto> valueNode = findField(node, "value");

        if (exists valueNode) {
            # AEK_Decl loc must cover only the name, not the "= value" part
            hash<auto> declLoc = exists nameNode ? nameNode.loc : node.loc;
            hash<auto> declExpr = {
                "nodetype": ANT_Expression,
                "kind": AEK_Decl,
                "loc": declLoc,
            };

            # Extract operator between name and value from source text
            string op = "=";
            if (exists nameNode) {
                int opLine = nameNode.loc.end_line;
                int opCol = nameNode.loc.end_column;
                int endLine = valueNode.loc.start_line;
                int endCol = valueNode.loc.start_column;
                if (opLine == endLine && opLine < sourceLines.size()) {
                    string line = sourceLines[opLine];
                    if (opCol < line.size() && endCol <= line.size()) {
                        op = trim(line.substr(opCol, endCol - opCol));
                    }
                }
            }

            if (op == "=" || op == ":=") {
                return {
                    "nodetype": ANT_Expression,
                    "kind": AEK_Assignment,
                    "left": declExpr,
                    "right": translateExpression(valueNode),
                    "loc": node.loc,
                };
            }

            # Compound assignment (+=, -=, etc.) → AEK_Binary
            return {
                "nodetype": ANT_Expression,
                "kind": AEK_Binary,
                "op": mapCompoundAssignOp(op),
                "left": declExpr,
                "right": translateExpression(valueNode),
                "loc": node.loc,
            };
        }

        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Decl,
            "loc": node.loc,
        };
    }

    private hash<auto> translateLocalVarDeclExpr(hash<auto> node) {
        # local_variable_declaration has type and variable_declarator children
        *hash<auto> typeNode = findField(node, "type");
        *hash<auto> declarator = findType(node, "variable_declarator");
        if (exists declarator) {
            *hash<auto> nameNode = findField(declarator, "name");
            *hash<auto> valueNode = findField(declarator, "value");

            if (exists valueNode) {
                # Compute AEK_Decl loc: from type start (or name start) to name end
                hash<auto> declLoc;
                if (exists typeNode && exists nameNode) {
                    declLoc = {
                        "start_line": typeNode.loc.start_line,
                        "start_column": typeNode.loc.start_column,
                        "end_line": nameNode.loc.end_line,
                        "end_column": nameNode.loc.end_column,
                    };
                } else if (exists nameNode) {
                    declLoc = nameNode.loc;
                } else {
                    declLoc = declarator.loc;
                }
                hash<auto> declExpr = {
                    "nodetype": ANT_Expression,
                    "kind": AEK_Decl,
                    "loc": declLoc,
                };

                # Extract operator between name and value from source text
                # to detect compound assignments (+=, -=, etc.)
                string op = "=";
                if (exists nameNode) {
                    int opLine = nameNode.loc.end_line;
                    int opCol = nameNode.loc.end_column;
                    int endLine = valueNode.loc.start_line;
                    int endCol = valueNode.loc.start_column;
                    if (opLine == endLine && opLine < sourceLines.size()) {
                        string line = sourceLines[opLine];
                        if (opCol < line.size() && endCol <= line.size()) {
                            op = trim(line.substr(opCol, endCol - opCol));
                        }
                    }
                }

                # If op contains '.' it's a misidentified member access (e.g., self.member = value)
                # Fall back to source text extraction
                if (op =~ /\./) {
                    return {
                        "nodetype": ANT_Expression,
                        "kind": AEK_Decl,
                        "loc": node.loc,
                    };
                }

                if (op == "=" || op == ":=") {
                    return {
                        "nodetype": ANT_Expression,
                        "kind": AEK_Assignment,
                        "left": declExpr,
                        "right": translateExpression(valueNode),
                        "loc": node.loc,
                    };
                }

                # Compound assignment (+=, -=, etc.) → AEK_Binary
                return {
                    "nodetype": ANT_Expression,
                    "kind": AEK_Binary,
                    "op": mapCompoundAssignOp(op),
                    "left": declExpr,
                    "right": translateExpression(valueNode),
                    "loc": node.loc,
                };
            }
            return {
                "nodetype": ANT_Expression,
                "kind": AEK_Decl,
                "loc": node.loc,
            };
        }
        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Decl,
            "loc": node.loc,
        };
    }

    private hash<auto> translateForVarDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *hash<auto> valueNode = findField(node, "value");

        if (exists valueNode) {
            hash<auto> declLoc = exists nameNode ? nameNode.loc : node.loc;
            return {
                "nodetype": ANT_Expression,
                "kind": AEK_Assignment,
                "left": {"nodetype": ANT_Expression, "kind": AEK_Decl, "loc": declLoc},
                "right": translateExpression(valueNode),
                "loc": node.loc,
            };
        }
        return {"nodetype": ANT_Expression, "kind": AEK_Decl, "loc": node.loc};
    }

    private hash<auto> translateConditionalDecl(hash<auto> node) {
        *hash<auto> nameNode = findField(node, "name");
        *hash<auto> valueNode = findField(node, "value");

        if (exists valueNode) {
            hash<auto> declLoc = exists nameNode ? nameNode.loc : node.loc;
            return {
                "nodetype": ANT_Expression,
                "kind": AEK_Assignment,
                "left": {"nodetype": ANT_Expression, "kind": AEK_Decl, "loc": declLoc},
                "right": translateExpression(valueNode),
                "loc": node.loc,
            };
        }
        return {"nodetype": ANT_Expression, "kind": AEK_Decl, "loc": node.loc};
    }

    private hash<auto> translateListAssignment(hash<auto> node) {
        return {
            "nodetype": ANT_Expression,
            "kind": AEK_Decl,
            "loc": node.loc,
        };
    }
}

} # namespace QoreCodeFormat
