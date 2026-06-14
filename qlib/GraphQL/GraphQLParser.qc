# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLParser.qc GraphQL executable-document parser

/*  GraphQLHandler Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace GraphQLHandler {
#! base parser providing a token stream and shared grammar productions (values, types, directives)
/** This class is shared by @ref GraphQLParser (executable documents) and the SDL parser used by
    @ref GraphQLSchema. It owns the token cursor and implements the grammar productions common to
    both: arguments, directives, type references, and input values.
*/
public class GraphQLParserBase {
    private {
        #! the token stream produced by the lexer
        list<hash<GraphQLToken>> toks;
        #! the current token index
        int idx = 0;
    }

    #! creates the parser for the given source text
    /** @param source the GraphQL source text to parse

        @throw GRAPHQL-SYNTAX-ERROR the source contains an invalid token
    */
    constructor(string source) {
        toks = GraphQLLexer::tokenize(source);
    }

    #! returns the current token without consuming it
    private hash<GraphQLToken> peek() {
        return toks[idx];
    }

    #! returns the kind of the current token
    private string peekKind() {
        return toks[idx].kind;
    }

    #! returns @ref True if the current token has the given kind
    private bool at(string kind) {
        return toks[idx].kind == kind;
    }

    #! returns @ref True if the current token is a name with the given value
    private bool atKeyword(string kw) {
        return toks[idx].kind == GraphQLLexer::TK_NAME && toks[idx].value == kw;
    }

    #! consumes and returns the current token
    private hash<GraphQLToken> advance() {
        return toks[idx++];
    }

    #! consumes the current token if it has the given kind; returns @ref True if consumed
    private bool optional(string kind) {
        if (toks[idx].kind == kind) {
            ++idx;
            return True;
        }
        return False;
    }

    #! consumes a token of the given kind or throws a syntax error
    private hash<GraphQLToken> expect(string kind) {
        if (toks[idx].kind != kind) {
            throwSyntax(sprintf("expected %y but found %s", kind, describe(toks[idx])));
        }
        return toks[idx++];
    }

    #! consumes a name token and returns its value
    private string expectName() {
        return expect(GraphQLLexer::TK_NAME).value;
    }

    #! consumes a name token with the given value or throws a syntax error
    private expectKeyword(string kw) {
        if (!atKeyword(kw)) {
            throwSyntax(sprintf("expected %y but found %s", kw, describe(toks[idx])));
        }
        ++idx;
    }

    #! parses a type reference (named, list, or non-null)
    private hash<GraphQLNode> parseTypeRef() {
        hash<GraphQLNode> t;
        if (optional(GraphQLLexer::TK_BRACKET_L)) {
            hash<GraphQLNode> inner = parseTypeRef();
            expect(GraphQLLexer::TK_BRACKET_R);
            t = <GraphQLListType>{"kind": NK_LIST_TYPE, "type": inner};
        } else {
            string name = expectName();
            t = <GraphQLNamedType>{"kind": NK_NAMED_TYPE, "name": name};
        }
        if (optional(GraphQLLexer::TK_BANG)) {
            t = <GraphQLNonNullType>{"kind": NK_NON_NULL_TYPE, "type": t};
        }
        return t;
    }

    #! parses an optional argument list \c "( Argument+ )"
    /** @param is_const if @ref True, variable references are not permitted in argument values
    */
    private list<hash<GraphQLNode>> parseArguments(bool is_const) {
        list<hash<GraphQLNode>> args();
        if (optional(GraphQLLexer::TK_PAREN_L)) {
            while (!at(GraphQLLexer::TK_PAREN_R)) {
                string name = expectName();
                expect(GraphQLLexer::TK_COLON);
                hash<GraphQLNode> value = parseValue(is_const);
                push args, <GraphQLArgument>{
                    "kind": NK_ARGUMENT,
                    "name": name,
                    "value": value,
                };
            }
            expect(GraphQLLexer::TK_PAREN_R);
        }
        return args;
    }

    #! parses an optional directive list
    /** @param is_const if @ref True, variable references are not permitted in directive arguments
    */
    private list<hash<GraphQLNode>> parseDirectives(bool is_const) {
        list<hash<GraphQLNode>> dirs();
        while (at(GraphQLLexer::TK_AT)) {
            advance();
            string name = expectName();
            list<hash<GraphQLNode>> args = parseArguments(is_const);
            push dirs, <GraphQLDirective>{
                "kind": NK_DIRECTIVE,
                "name": name,
                "arguments": args,
            };
        }
        return dirs;
    }

    #! parses an input value
    /** @param is_const if @ref True, variable references are not permitted (constant context)

        @return the parsed value node

        @throw GRAPHQL-SYNTAX-ERROR an invalid value was encountered
    */
    private hash<GraphQLNode> parseValue(bool is_const) {
        hash<GraphQLToken> tok = peek();
        switch (tok.kind) {
            case GraphQLLexer::TK_DOLLAR: {
                if (is_const) {
                    throwSyntax("unexpected variable in constant value");
                }
                advance();
                string name = expectName();
                return <GraphQLVariable>{"kind": NK_VARIABLE, "name": name};
            }
            case GraphQLLexer::TK_INT:
                advance();
                return <GraphQLIntValue>{"kind": NK_INT, "value": tok.value};
            case GraphQLLexer::TK_FLOAT:
                advance();
                return <GraphQLFloatValue>{"kind": NK_FLOAT, "value": tok.value};
            case GraphQLLexer::TK_STRING:
                advance();
                return <GraphQLStringValue>{"kind": NK_STRING, "value": tok.value};
            case GraphQLLexer::TK_NAME: {
                advance();
                switch (tok.value) {
                    case "true":
                        return <GraphQLBooleanValue>{"kind": NK_BOOLEAN, "value": True};
                    case "false":
                        return <GraphQLBooleanValue>{"kind": NK_BOOLEAN, "value": False};
                    case "null":
                        return <GraphQLNode>{"kind": NK_NULL};
                    default:
                        return <GraphQLEnumValue>{"kind": NK_ENUM, "value": tok.value};
                }
            }
            case GraphQLLexer::TK_BRACKET_L: {
                advance();
                list<hash<GraphQLNode>> values();
                while (!at(GraphQLLexer::TK_BRACKET_R)) {
                    push values, parseValue(is_const);
                }
                expect(GraphQLLexer::TK_BRACKET_R);
                return <GraphQLListValue>{"kind": NK_LIST, "values": values};
            }
            case GraphQLLexer::TK_BRACE_L: {
                advance();
                list<hash<GraphQLNode>> fields();
                while (!at(GraphQLLexer::TK_BRACE_R)) {
                    string name = expectName();
                    expect(GraphQLLexer::TK_COLON);
                    hash<GraphQLNode> value = parseValue(is_const);
                    push fields, <GraphQLObjectField>{
                        "kind": NK_OBJECT_FIELD,
                        "name": name,
                        "value": value,
                    };
                }
                expect(GraphQLLexer::TK_BRACE_R);
                return <GraphQLObjectValue>{"kind": NK_OBJECT, "fields": fields};
            }
            default:
                throwSyntax(sprintf("unexpected %s where a value was expected", describe(tok)));
        }
    }

    #! returns a human-readable description of a token for error messages
    private static string describe(hash<GraphQLToken> tok) {
        switch (tok.kind) {
            case GraphQLLexer::TK_NAME:
                return sprintf("name %y", tok.value);
            case GraphQLLexer::TK_INT:
            case GraphQLLexer::TK_FLOAT:
                return sprintf("%s %y", tok.kind, tok.value);
            case GraphQLLexer::TK_STRING:
                return sprintf("string %y", tok.value);
            case GraphQLLexer::TK_EOF:
                return "<end of input>";
            default:
                return sprintf("%y", tok.kind);
        }
    }

    #! throws a @ref GRAPHQL_SYNTAX_ERROR exception positioned at the current token
    private throwSyntax(string msg) {
        hash<GraphQLToken> tok = toks[idx];
        throw GRAPHQL_SYNTAX_ERROR, sprintf("syntax error at line %d, column %d: %s",
            tok.line, tok.column, msg), {"line": tok.line, "column": tok.column};
    }
}

#! Parses GraphQL executable documents (operations and fragment definitions) into an AST
/** @par Example:
    @code{.py}
hash<GraphQLDocument> doc = GraphQLParser::parse("query { hero { name } }");
    @endcode
*/
public class GraphQLParser inherits GraphQLParserBase {
    #! the set of operation keywords
    const OperationKeywords = {
        "query": True,
        "mutation": True,
        "subscription": True,
    };

    #! creates the parser for the given source text
    /** @param source the GraphQL executable document source

        @throw GRAPHQL-SYNTAX-ERROR the source contains an invalid token
    */
    constructor(string source) : GraphQLParserBase(source) {
    }

    #! parses the given source into a document AST
    /** @param source the GraphQL executable document source

        @return the parsed document node

        @throw GRAPHQL-SYNTAX-ERROR the source is not a valid GraphQL executable document
    */
    static hash<GraphQLDocument> parse(string source) {
        GraphQLParser parser(source);
        return parser.parseDocument();
    }

    #! parses the document supplied to the constructor
    /** @return the parsed document node

        @throw GRAPHQL-SYNTAX-ERROR the source is not a valid GraphQL executable document
    */
    hash<GraphQLDocument> parseDocument() {
        list<hash<GraphQLNode>> defs();
        while (!at(GraphQLLexer::TK_EOF)) {
            push defs, parseDefinition();
        }
        if (!defs) {
            throwSyntax("a document must contain at least one definition");
        }
        return <GraphQLDocument>{"kind": NK_DOCUMENT, "definitions": defs};
    }

    #! parses a single top-level definition (operation or fragment)
    private hash<GraphQLNode> parseDefinition() {
        if (at(GraphQLLexer::TK_BRACE_L)) {
            # anonymous query shorthand
            return parseOperation();
        }
        if (at(GraphQLLexer::TK_NAME)) {
            string kw = peek().value;
            if (OperationKeywords{kw}) {
                return parseOperation();
            }
            if (kw == "fragment") {
                return parseFragmentDefinition();
            }
            throwSyntax(sprintf("unexpected keyword %y; expected an operation or fragment definition", kw));
        }
        throwSyntax(sprintf("unexpected %s at the start of a definition", describe(peek())));
    }

    #! parses an operation definition
    private hash<GraphQLNode> parseOperation() {
        if (at(GraphQLLexer::TK_BRACE_L)) {
            hash<GraphQLNode> ss = parseSelectionSet();
            return <GraphQLOperationDefinition>{
                "kind": NK_OPERATION,
                "operation": "query",
                "selectionSet": ss,
            };
        }
        string operation = expectName();
        *string name;
        if (at(GraphQLLexer::TK_NAME)) {
            name = advance().value;
        }
        list<hash<GraphQLNode>> varDefs = parseVariableDefinitions();
        list<hash<GraphQLNode>> directives = parseDirectives(False);
        hash<GraphQLNode> ss = parseSelectionSet();
        return <GraphQLOperationDefinition>{
            "kind": NK_OPERATION,
            "operation": operation,
            "name": name,
            "variableDefinitions": varDefs,
            "directives": directives,
            "selectionSet": ss,
        };
    }

    #! parses an optional variable-definition list
    private list<hash<GraphQLNode>> parseVariableDefinitions() {
        list<hash<GraphQLNode>> defs();
        if (optional(GraphQLLexer::TK_PAREN_L)) {
            while (!at(GraphQLLexer::TK_PAREN_R)) {
                expect(GraphQLLexer::TK_DOLLAR);
                string name = expectName();
                expect(GraphQLLexer::TK_COLON);
                hash<GraphQLNode> type = parseTypeRef();
                *hash<GraphQLNode> defaultValue;
                if (optional(GraphQLLexer::TK_EQUALS)) {
                    defaultValue = parseValue(True);
                }
                list<hash<GraphQLNode>> directives = parseDirectives(True);
                push defs, <GraphQLVariableDefinition>{
                    "kind": NK_VARIABLE_DEFINITION,
                    "name": name,
                    "type": type,
                    "defaultValue": defaultValue,
                    "directives": directives,
                };
            }
            expect(GraphQLLexer::TK_PAREN_R);
        }
        return defs;
    }

    #! parses a selection set \c "{ Selection+ }"
    private hash<GraphQLNode> parseSelectionSet() {
        expect(GraphQLLexer::TK_BRACE_L);
        list<hash<GraphQLNode>> selections();
        while (!at(GraphQLLexer::TK_BRACE_R)) {
            push selections, parseSelection();
        }
        expect(GraphQLLexer::TK_BRACE_R);
        if (!selections) {
            throwSyntax("a selection set must contain at least one selection");
        }
        return <GraphQLSelectionSet>{
            "kind": NK_SELECTION_SET,
            "selections": selections,
        };
    }

    #! parses a single selection (field, fragment spread, or inline fragment)
    private hash<GraphQLNode> parseSelection() {
        if (optional(GraphQLLexer::TK_SPREAD)) {
            # "... on Type" or "... FragmentName" or "... { ... }" / "... @dir { ... }"
            if (atKeyword("on")) {
                advance();
                string typeCondition = expectName();
                list<hash<GraphQLNode>> directives = parseDirectives(False);
                hash<GraphQLNode> ss = parseSelectionSet();
                return <GraphQLInlineFragment>{
                    "kind": NK_INLINE_FRAGMENT,
                    "typeCondition": typeCondition,
                    "directives": directives,
                    "selectionSet": ss,
                };
            }
            if (at(GraphQLLexer::TK_NAME)) {
                string name = advance().value;
                list<hash<GraphQLNode>> directives = parseDirectives(False);
                return <GraphQLFragmentSpread>{
                    "kind": NK_FRAGMENT_SPREAD,
                    "name": name,
                    "directives": directives,
                };
            }
            # inline fragment without type condition
            list<hash<GraphQLNode>> directives = parseDirectives(False);
            hash<GraphQLNode> ss = parseSelectionSet();
            return <GraphQLInlineFragment>{
                "kind": NK_INLINE_FRAGMENT,
                "directives": directives,
                "selectionSet": ss,
            };
        }
        return parseField();
    }

    #! parses a field selection
    private hash<GraphQLNode> parseField() {
        string nameOrAlias = expectName();
        *string alias;
        string name = nameOrAlias;
        if (optional(GraphQLLexer::TK_COLON)) {
            alias = nameOrAlias;
            name = expectName();
        }
        list<hash<GraphQLNode>> args = parseArguments(False);
        list<hash<GraphQLNode>> directives = parseDirectives(False);
        *hash<GraphQLNode> ss;
        if (at(GraphQLLexer::TK_BRACE_L)) {
            ss = parseSelectionSet();
        }
        return <GraphQLField>{
            "kind": NK_FIELD,
            "alias": alias,
            "name": name,
            "arguments": args,
            "directives": directives,
            "selectionSet": ss,
        };
    }

    #! parses a fragment definition
    private hash<GraphQLNode> parseFragmentDefinition() {
        expectKeyword("fragment");
        string name = expectName();
        if (name == "on") {
            throwSyntax("fragment name cannot be \"on\"");
        }
        expectKeyword("on");
        string typeCondition = expectName();
        list<hash<GraphQLNode>> directives = parseDirectives(False);
        hash<GraphQLNode> ss = parseSelectionSet();
        return <GraphQLFragmentDefinition>{
            "kind": NK_FRAGMENT_DEFINITION,
            "name": name,
            "typeCondition": typeCondition,
            "directives": directives,
            "selectionSet": ss,
        };
    }
}
}
