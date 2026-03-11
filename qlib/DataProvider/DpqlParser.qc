# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DpqlParser.qc DPQL parser implementation

/*  DpqlParser.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {

#! DPQL Parse result structure
/** Contains the result of parsing a DPQL expression, including the parsed expression,
    any diagnostic messages, and the token stream for syntax highlighting.
*/
public hashdecl DpqlParseResult {
    #! The parsed expression, if parsing was successful
    *hash<DataProviderExpression> expression;
    #! List of diagnostic messages (errors and warnings)
    list<hash<DpqlDiagnostic>> diagnostics;
    #! True if parsing was successful with no errors
    bool success;
    #! Token stream for syntax highlighting
    list<hash<DpqlTokenInfo>> tokens;
}

#! Parser for DPQL (Data Provider Query Language) expressions
/** This class provides parsing of DPQL query expressions into DataProviderExpression
    structures. It implements a recursive descent parser for the DPQL grammar.

    The parser supports:
    - Comparison operators: ==, !=, <, <=, >, >=
    - Logical operators: &&, ||, !
    - Range operators: between X and Y
    - Collection operators: in (val1, val2, ...)
    - Regex matching: =~ /pattern/flags
    - Function calls: func(arg1, arg2, ...)
    - Field references: \c \@field, <tt>\@\"field with spaces\"</tt>
    - Literals: strings, numbers, booleans, null

    @par Example:
    @code{.py}
DpqlParser parser();
hash<DataProviderExpression> exp = parser.parse('@name == "John" && @age > 18');
# Returns: {"exp": "&&", "args": [{"exp": "==", "args": [{"field": "name"}, "John"]}, ...]}
    @endcode
*/
public class DpqlParser {
    private {
        #! Tokenizer instance
        DpqlTokenizer tokenizer;
        #! Collected errors
        list<hash<DpqlDiagnostic>> errors = ();
        #! Token stream for result
        list<hash<DpqlTokenInfo>> tokens = ();
        #! Available expressions (for validation)
        *hash<string, hash<DataProviderExpressionInfo>> expressions;
        #! Map of DPQL symbol to expression name (for symbol-based lookup)
        /** @since DataProvider 3.4
        */
        hash<string, string> symbol_to_name;
        #! Available fields for type resolution
        /** @since DataProvider 3.4
        */
        *hash<string, AbstractDataField> fields;
        #! Current line number for error reporting
        int line = 1;
        #! Current column number for error reporting
        int column = 1;
    }

    #! Creates a parser with optional expression map and fields for validation
    /** @param expressions optional map of expression names to their info for validation
        @param fields optional map of field names to their definitions for type resolution

        @since DataProvider 3.4 added fields parameter
    */
    constructor(*hash<string, hash<DataProviderExpressionInfo>> expressions,
            *hash<string, AbstractDataField> fields) {
        self.expressions = expressions;
        self.fields = fields;
        if (expressions) {
            buildSymbolToNameMap();
        }
    }

    #! Builds a symbol-to-name lookup map from the expression map
    /** Maps DPQL symbols (with trailing "()" stripped) to their internal expression names
        (the keys in the expression map). This allows the parser to accept symbol-based
        syntax in DPQL text while storing the canonical name in \c exp.exp.

        @since DataProvider 3.4
    */
    private buildSymbolToNameMap() {
        foreach string ename in (keys expressions) {
            hash<DataProviderExpressionInfo> info = expressions{ename};
            if (!info.symbol) {
                continue;
            }
            string sym = info.symbol;
            # Strip trailing () from display symbols (e.g. "substr()" -> "substr")
            if (sym =~ /\(\)$/) {
                sym = substr(sym, 0, sym.size() - 2);
            }
            symbol_to_name{sym} = ename;
        }
    }

    #! Parses a DPQL expression string
    /** @param text the DPQL expression to parse
        @return the parsed DataProviderExpression

        @throw DPQL-PARSE-ERROR if the expression contains syntax errors
    */
    hash<DataProviderExpression> parse(string text) {
        hash<DpqlParseResult> result = parseWithInfo(text);
        if (!result.success) {
            string msg = "DPQL parse error";
            if (result.diagnostics) {
                msg += ": " + result.diagnostics[0].message;
                msg += sprintf(" at line %d, column %d", result.diagnostics[0].line,
                    result.diagnostics[0].column);
            }
            throw "DPQL-PARSE-ERROR", msg, result.diagnostics;
        }
        return result.expression;
    }

    #! Parses a DPQL expression with full diagnostic information
    /** @param text the DPQL expression to parse
        @return a DpqlParseResult containing the expression, diagnostics, and tokens
    */
    hash<DpqlParseResult> parseWithInfo(string text) {
        # Reset state
        errors = ();
        tokens = ();

        # Initialize tokenizer
        tokenizer = new DpqlTokenizer(text);

        # Collect all tokens first for syntax highlighting
        tokens = tokenizer.getAllTokens();

        # Get tokenizer errors from full scan (these should come first)
        list<hash<DpqlDiagnostic>> tokenizer_errors = tokenizer.getErrors();

        # Reset tokenizer for parsing
        tokenizer = new DpqlTokenizer(text);

        # Parse the expression
        *hash<DataProviderExpression> exp;
        bool parse_failed = False;
        try {
            skipWhitespace();
            if (!atEnd()) {
                auto parsed = parseOrExpr();
                skipWhitespace();
                if (!atEnd()) {
                    hash<DpqlTokenInfo> tok = peek();
                    addError(sprintf("Unexpected token '%s' after expression", tok.value),
                        "DPQL-E001", tok);
                }
                # Wrap non-expression results (field references, bare values) in a value expression
                # This only happens at the top level, not for nested expressions or function args
                if (parsed instanceof hash<DataProviderExpression>) {
                    exp = parsed;
                } else if (parsed instanceof hash<DataProviderTemplateReference>) {
                    exp = <DataProviderExpression>{
                        "exp": DP_EXPR_TEMPLATE,
                        "args": (parsed,),
                    };
                } else {
                    exp = <DataProviderExpression>{
                        "exp": DP_EXPR_VALUE,
                        "args": (parsed,),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            parse_failed = True;
            if (ex.err != "DPQL-PARSE-ERROR") {
                addError(sprintf("Internal parser error: %s", ex.desc), "DPQL-E001",
                    line, column, line, column);
            }
        }

        # Prepend tokenizer errors (they should come first)
        errors = tokenizer_errors + errors;

        # Determine success: warnings don't count as failures
        bool has_errors = (select errors, $1.severity == "error") ? True : False;

        return <DpqlParseResult>{
            "expression": exp,
            "diagnostics": errors,
            "success": !parse_failed && !has_errors,
            "tokens": tokens,
        };
    }

    #! Validates an expression against available fields
    /** @param exp the expression to validate
        @param fields the available fields
        @return a list of diagnostic messages for any validation errors
    */
    list<hash<DpqlDiagnostic>> validate(hash<DataProviderExpression> exp,
            hash<string, AbstractDataField> fields) {
        list<hash<DpqlDiagnostic>> diags = ();
        validateExprRecursive(exp, fields, \diags);
        return diags;
    }

    #! Recursively validates an expression
    private validateExprRecursive(hash<DataProviderExpression> exp,
            hash<string, AbstractDataField> fields, reference<list<hash<DpqlDiagnostic>>> diags) {
        # Validate each argument
        foreach auto arg in (exp.args) {
            if (arg.typeCode() == NT_HASH) {
                if (arg.hasKey("field")) {
                    # Field reference - validate field exists
                    string field_name = arg.field;
                    # Handle dotted field names
                    int dot_pos = index(field_name, ".");
                    string base_field = dot_pos >= 0 ? substr(field_name, 0, dot_pos) : field_name;
                    if (!fields.hasKey(base_field)) {
                        diags += <DpqlDiagnostic>{
                            "severity": "error",
                            "message": sprintf("Unknown field '%s'", field_name),
                            "code": "DPQL-E010",
                            "line": 1,
                            "column": 1,
                            "end_line": 1,
                            "end_column": 1,
                        };
                    }
                } else if (arg.hasKey("exp")) {
                    # Nested expression
                    validateExprRecursive(arg, fields, \diags);
                }
            }
        }
    }

    #! Returns True if at end of input (ignoring whitespace)
    private bool atEnd() {
        return tokenizer.atEnd();
    }

    #! Peeks at the next non-whitespace token
    private hash<DpqlTokenInfo> peek() {
        hash<DpqlTokenInfo> tok = tokenizer.peek();
        line = tok.line;
        column = tok.column;
        return tok;
    }

    #! Gets the next non-whitespace token
    private hash<DpqlTokenInfo> nextToken() {
        hash<DpqlTokenInfo> tok = tokenizer.next();
        line = tok.line;
        column = tok.column;
        return tok;
    }

    #! Skips whitespace tokens
    private skipWhitespace() {
        while (!tokenizer.atEnd()) {
            hash<DpqlTokenInfo> tok = tokenizer.peek();
            if (tok.type != DPQL_TOK_WHITESPACE && tok.type != DPQL_TOK_NEWLINE) {
                break;
            }
            tokenizer.next();
        }
    }

    #! Adds an error diagnostic
    private addError(string message, string code, hash<DpqlTokenInfo> tok) {
        errors += <DpqlDiagnostic>{
            "severity": "error",
            "message": message,
            "code": code,
            "line": tok.line,
            "column": tok.column,
            "end_line": tok.end_line,
            "end_column": tok.end_column,
        };
    }

    #! Adds a warning diagnostic from a token
    private addWarning(string message, string code, hash<DpqlTokenInfo> tok) {
        errors += <DpqlDiagnostic>{
            "severity": "warning",
            "message": message,
            "code": code,
            "line": tok.line,
            "column": tok.column,
            "end_line": tok.end_line,
            "end_column": tok.end_column,
        };
    }

    #! Adds an error diagnostic with explicit position
    private addError(string message, string code, int start_line, int start_col,
            int end_line, int end_col) {
        errors += <DpqlDiagnostic>{
            "severity": "error",
            "message": message,
            "code": code,
            "line": start_line,
            "column": start_col,
            "end_line": end_line,
            "end_column": end_col,
        };
    }

    #! Expects a specific token type and returns it
    private hash<DpqlTokenInfo> expect(int token_type, string description) {
        skipWhitespace();
        if (atEnd()) {
            addError(sprintf("Unexpected end of input, expected %s", description),
                "DPQL-E001", line, column, line, column);
            throw "DPQL-PARSE-ERROR", "Unexpected end of input";
        }
        hash<DpqlTokenInfo> tok = nextToken();
        if (tok.type != token_type) {
            addError(sprintf("Expected %s, got '%s'", description, tok.value),
                "DPQL-E001", tok);
            throw "DPQL-PARSE-ERROR", sprintf("Expected %s", description);
        }
        return tok;
    }

    #! Parses or_expr: and_expr { "||" and_expr }
    private auto parseOrExpr() {
        auto left = parseAndExpr();
        skipWhitespace();

        list<auto> args = (left,);
        while (!atEnd()) {
            hash<DpqlTokenInfo> tok = peek();
            if (tok.type != DPQL_TOK_OP_OR) {
                break;
            }
            nextToken();  # consume ||
            skipWhitespace();
            args += parseAndExpr();
            skipWhitespace();
        }

        if (args.size() == 1) {
            # Return single value/expression as-is
            # Field references, expressions, and bare values are all returned unchanged
            return args[0];
        }

        return <DataProviderExpression>{
            "exp": DP_OP_OR,
            "args": args,
        };
    }

    #! Parses and_expr: unary_expr { "&&" unary_expr }
    private auto parseAndExpr() {
        auto left = parseUnaryExpr();
        skipWhitespace();

        list<auto> args = (left,);
        while (!atEnd()) {
            hash<DpqlTokenInfo> tok = peek();
            if (tok.type != DPQL_TOK_OP_AND) {
                break;
            }
            nextToken();  # consume &&
            skipWhitespace();
            args += parseUnaryExpr();
            skipWhitespace();
        }

        if (args.size() == 1) {
            return args[0];
        }

        return <DataProviderExpression>{
            "exp": DP_OP_AND,
            "args": args,
        };
    }

    #! Parses unary_expr: "!" unary_expr | primary_expr
    private auto parseUnaryExpr() {
        skipWhitespace();
        if (atEnd()) {
            addError("Unexpected end of input", "DPQL-E001", line, column, line, column);
            throw "DPQL-PARSE-ERROR", "Unexpected end of input";
        }

        hash<DpqlTokenInfo> tok = peek();
        if (tok.type == DPQL_TOK_OP_NOT) {
            nextToken();  # consume !
            skipWhitespace();
            auto operand = parseUnaryExpr();
            return <DataProviderExpression>{
                "exp": DP_SEARCH_OP_NOT,
                "args": (operand,),
            };
        }

        return parsePrimaryExpr();
    }

    #! Parses primary_expr: comparison (which handles values, function calls, subexpressions)
    private auto parsePrimaryExpr() {
        skipWhitespace();
        if (atEnd()) {
            addError("Unexpected end of input", "DPQL-E001", line, column, line, column);
            throw "DPQL-PARSE-ERROR", "Unexpected end of input";
        }

        # parseComparison handles all primary expression types via parseValue
        return parseComparison();
    }

    #! Parses a comparison expression
    private auto parseComparison() {
        auto left = parseValue();
        skipWhitespace();

        if (atEnd()) {
            return left;
        }

        hash<DpqlTokenInfo> tok = peek();

        # Check for comparison operators
        switch (tok.type) {
            case DPQL_TOK_OP_EQ:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_EQ,
                    "args": (left, parseValue()),
                };

            case DPQL_TOK_OP_NE:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_NE,
                    "args": (left, parseValue()),
                };

            case DPQL_TOK_OP_LT:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_LT,
                    "args": (left, parseValue()),
                };

            case DPQL_TOK_OP_LE:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_LE,
                    "args": (left, parseValue()),
                };

            case DPQL_TOK_OP_GT:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_GT,
                    "args": (left, parseValue()),
                };

            case DPQL_TOK_OP_GE:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_GE,
                    "args": (left, parseValue()),
                };

            case DPQL_TOK_OP_REGEX:
                nextToken();  # consume =~
                skipWhitespace();
                return parseRegexExpr(left);

            case DPQL_TOK_IDENTIFIER: {
                string kw = tok.value.lwr();
                if (kw == "between") {
                    nextToken();  # consume 'between'
                    skipWhitespace();
                    auto low = parseValue();
                    skipWhitespace();
                    # Expect 'and'
                    hash<DpqlTokenInfo> and_tok = peek();
                    if (and_tok.type != DPQL_TOK_IDENTIFIER || and_tok.value.lwr() != "and") {
                        addError("Expected 'and' in between expression", "DPQL-E001", and_tok);
                        throw "DPQL-PARSE-ERROR", "Expected 'and'";
                    }
                    nextToken();  # consume 'and'
                    skipWhitespace();
                    auto high = parseValue();
                    return <DataProviderExpression>{
                        "exp": DP_SEARCH_OP_BETWEEN,
                        "args": (left, low, high),
                    };
                }
                if (kw == "in") {
                    nextToken();  # consume 'in'
                    skipWhitespace();
                    list<auto> values = ();
                    if (!atEnd() && peek().type == DPQL_TOK_LIST) {
                        auto parsed = parseStructuredLiteral();
                        if (parsed.typeCode() != NT_LIST) {
                            addError("Expected list literal after 'in'", "DPQL-E013", line, column, line, column);
                            throw "DPQL-PARSE-ERROR", "Expected list literal";
                        }
                        values = parsed;
                    } else {
                        expect(DPQL_TOK_LPAREN, "'('");
                        skipWhitespace();
                        values = parseValueList();
                        skipWhitespace();
                        expect(DPQL_TOK_RPAREN, "')'");
                    }
                    return <DataProviderExpression>{
                        "exp": DP_SEARCH_OP_IN,
                        "args": (left,) + values,
                    };
                }
                # Not a keyword we recognize here
                break;
            }
        }

        # No operator - return the value as-is
        return left;
    }

    #! Parses a regex expression after =~
    private hash<DataProviderExpression> parseRegexExpr(auto left) {
        if (atEnd()) {
            addError("Expected regex pattern after '=~'", "DPQL-E009", line, column, line, column);
            throw "DPQL-PARSE-ERROR", "Expected regex pattern";
        }

        hash<DpqlTokenInfo> tok = peek();
        if (tok.type != DPQL_TOK_REGEX) {
            addError("Expected regex pattern after '=~'", "DPQL-E009", tok);
            throw "DPQL-PARSE-ERROR", "Expected regex pattern";
        }

        nextToken();  # consume regex
        string pattern = tok.value;

        # Parse the regex: /pattern/flags
        # Extract pattern and flags
        if (pattern.size() < 2 || pattern[0] != "/") {
            addError("Invalid regex pattern", "DPQL-E009", tok);
            throw "DPQL-PARSE-ERROR", "Invalid regex pattern";
        }

        int last_slash = rindex(pattern, "/");
        if (last_slash <= 0) {
            addError("Invalid regex pattern", "DPQL-E009", tok);
            throw "DPQL-PARSE-ERROR", "Invalid regex pattern";
        }

        string regex_pattern = substr(pattern, 1, last_slash - 1);
        string flags = substr(pattern, last_slash + 1);

        list<auto> args = (left, regex_pattern);

        # Convert flags to regex option constants
        if (flags) {
            for (int i = 0; i < flags.size(); ++i) {
                string flag = flags[i];
                switch (flag) {
                    case "i":
                        args += RE_Caseless;
                        break;
                    case "s":
                        args += RE_DotAll;
                        break;
                    case "m":
                        args += RE_MultiLine;
                        break;
                    case "x":
                        args += RE_Extended;
                        break;
                    case "u":
                        args += RE_Unicode;
                        break;
                    default:
                        addError(sprintf("Unknown regex flag '%s'", flag), "DPQL-E009", tok);
                }
            }
        }

        return <DataProviderExpression>{
            "exp": DP_SEARCH_OP_REGEX,
            "args": args,
        };
    }

    #! Parses a value (field ref, literal, function call, or grouped expression)
    private auto parseValue() {
        skipWhitespace();
        if (atEnd()) {
            addError("Unexpected end of input, expected value", "DPQL-E001",
                line, column, line, column);
            throw "DPQL-PARSE-ERROR", "Expected value";
        }

        hash<DpqlTokenInfo> tok = peek();

        switch (tok.type) {
            case DPQL_TOK_FIELD:
                return parseFieldRef();

            case DPQL_TOK_TEMPLATE:
                return parseTemplateRef();

            case DPQL_TOK_STRING:
                return parseStringLiteral();

            case DPQL_TOK_NUMBER:
                return parseNumberLiteral();

            case DPQL_TOK_DATE:
                return parseDateLiteral();

            case DPQL_TOK_BINARY:
                return parseBinaryLiteral();

            case DPQL_TOK_LIST:
            case DPQL_TOK_HASH:
                return parseStructuredLiteral();

            case DPQL_TOK_BOOLEAN:
                nextToken();
                return tok.value.lwr() == "true";

            case DPQL_TOK_NULL:
                nextToken();
                return NOTHING;

            case DPQL_TOK_LPAREN:
                # Could be grouped expression or function call
                nextToken();  # consume (
                skipWhitespace();
                auto exp = parseOrExpr();
                skipWhitespace();
                expect(DPQL_TOK_RPAREN, "')'");
                return exp;

            case DPQL_TOK_IDENTIFIER: {
                hash<DpqlTokenInfo> id_tok = nextToken();
                skipWhitespace();
                # Check for function call
                if (!atEnd() && peek().type == DPQL_TOK_LPAREN) {
                    return parseFunctionCall(id_tok);
                }
                # Check for keywords that can be values
                string lwr = id_tok.value.lwr();
                if (lwr == "true") {
                    return True;
                }
                if (lwr == "false") {
                    return False;
                }
                if (lwr == "null") {
                    return NOTHING;
                }
                # Treat unquoted identifiers as strings
                return id_tok.value;
            }

            case DPQL_TOK_ERROR:
                # Error token from tokenizer - don't add another error, just throw
                nextToken();  # consume the error token
                throw "DPQL-PARSE-ERROR", "Tokenizer error";

            default:
                addError(sprintf("Unexpected token '%s' in value position", tok.value),
                    "DPQL-E001", tok);
                throw "DPQL-PARSE-ERROR", "Unexpected token";
        }
    }

    #! Parses a field reference with optional accessor chain as expression nodes
    /** Returns a plain DataProviderFieldReference if no accessors follow, or a nested
        expression tree using DP_FUNC_INDEX/DP_FUNC_SLICE/DP_FUNC_KEY operators.
    */
    private auto parseFieldRef() {
        hash<DpqlTokenInfo> tok = nextToken();
        string value = tok.value;

        # Extract field name from @field or @"field"
        string field_name;
        if (value.size() > 1) {
            if (value[1] == '"') {
                # Quoted field: @"field name"
                # Remove @" prefix and " suffix
                string content = substr(value, 2, value.size() - 3);
                # Process escape sequences (same handling as parseStringLiteral)
                field_name = unescapeString(content);
            } else {
                # Unquoted field: @field
                field_name = substr(value, 1);
            }
        } else {
            addError("Invalid field reference", "DPQL-E003", tok);
            throw "DPQL-PARSE-ERROR", "Invalid field reference";
        }

        auto result = <DataProviderFieldReference>{
            "field": field_name,
        };

        # Parse optional accessor chain, wrapping in expression nodes
        return parseAccessorChain(result);
    }

    #! Parses a template reference token into a DataProviderTemplateReference hashdecl
    /** Extracts context, value, and optional type assertion from the raw template token text.

        @par Supported forms
        - Simple: \c $context:value
        - Bracketed: \c $context:{value}
        - Type-asserted: \c $context:value::type
        - Fallback: \c $context:A??{$ctx:B}

        @return a DataProviderTemplateReference hashdecl

        @since DataProvider 3.3
    */
    private hash<DataProviderTemplateReference> parseTemplateRef() {
        hash<DpqlTokenInfo> tok = nextToken();
        string raw = tok.value;

        # Strip leading '$'
        string rest = substr(raw, 1);

        # Extract context (up to first ':')
        int colon_pos = index(rest, ":");
        # The tokenizer guarantees ':' exists in a DPQL_TOK_TEMPLATE token (validated by DPQL-E032)
        @assert(colon_pos >= 0);
        string ctx = substr(rest, 0, colon_pos);
        rest = substr(rest, colon_pos + 1);

        string val;

        if (rest[0] == "{") {
            # Bracketed value: find matching closing '}'
            int depth = 0;
            int close_pos = -1;
            for (int i = 0; i < rest.size(); ++i) {
                string ch = rest[i];
                if (ch == "{") {
                    ++depth;
                } else if (ch == "}") {
                    --depth;
                    if (depth == 0) {
                        close_pos = i;
                        break;
                    }
                }
            }

            if (close_pos > 0) {
                # Content between the outermost { and }
                val = substr(rest, 1, close_pos - 1);
            } else {
                # Fallback: no matching '}' found; strip outer braces as best effort
                if (rest.size() > 1 && rest[rest.size() - 1] == "}") {
                    val = substr(rest, 1, rest.size() - 2);
                } else {
                    val = substr(rest, 1);
                }
            }
        } else {
            val = rest;
        }

        return <DataProviderTemplateReference>{
            "tmpl_context": ctx,
            "tmpl_value": val,
            "raw": raw,
        };
    }

    #! Parses accessor chain and wraps the base expression in nested operator nodes
    /** @param base the base value (field reference or expression) to wrap
        @return the base if no accessors, or a nested expression tree
    */
    private auto parseAccessorChain(auto base) {
        auto result = base;

        while (!atEnd()) {
            skipWhitespace();
            if (atEnd()) {
                break;
            }

            hash<DpqlTokenInfo> tok = peek();

            if (tok.type == DPQL_TOK_LBRACKET) {
                # Index or slice accessor: [...]
                result = parseIndexAccessor(result);
            } else if (tok.type == DPQL_TOK_LBRACE) {
                # Key accessor: {...}
                result = parseKeyAccessor(result);
            } else if (tok.type == DPQL_TOK_DOT) {
                # Member access after accessor: .identifier
                nextToken();  # consume .
                skipWhitespace();
                if (atEnd()) {
                    addError("Expected identifier after '.'", "DPQL-E020", tok);
                    throw "DPQL-PARSE-ERROR", "Expected identifier after '.'";
                }
                hash<DpqlTokenInfo> id_tok = peek();
                if (id_tok.type != DPQL_TOK_IDENTIFIER && id_tok.type != DPQL_TOK_FIELD) {
                    addError("Expected identifier after '.'", "DPQL-E020", id_tok);
                    throw "DPQL-PARSE-ERROR", "Expected identifier after '.'";
                }
                string member_name;
                if (id_tok.type == DPQL_TOK_FIELD) {
                    # Handle @field after dot - strip the @
                    member_name = substr(id_tok.value, 1);
                } else {
                    member_name = id_tok.value;
                }
                nextToken();  # consume identifier
                # Member access is a single-key expression node
                result = <DataProviderExpression>{
                    "exp": DP_FUNC_KEY,
                    "args": (result, member_name),
                };
            } else {
                # No more accessors
                break;
            }
        }

        return result;
    }

    #! Parses index accessor: [n], [n..m], [n,m,p], [n..m,p], [2..], [..5]
    /** @param base the base expression to index into
        @return a DP_FUNC_INDEX or DP_FUNC_SLICE expression node
    */
    private hash<DataProviderExpression> parseIndexAccessor(auto base) {
        hash<DpqlTokenInfo> lbracket = nextToken();  # consume [
        skipWhitespace();

        list<auto> items = ();
        bool first = True;
        bool closed = False;

        while (True) {
            skipWhitespace();
            hash<DpqlTokenInfo> tok = peek();

            # Check for EOF - unclosed accessor
            if (tok.type == DPQL_TOK_EOF) {
                break;
            }

            if (tok.type == DPQL_TOK_RBRACKET) {
                nextToken();  # consume ]
                closed = True;
                break;
            }

            if (!first) {
                if (tok.type != DPQL_TOK_COMMA) {
                    addError("Expected ',' or ']' in index accessor", "DPQL-E022", tok);
                    throw "DPQL-PARSE-ERROR", "Expected ',' or ']'";
                }
                nextToken();  # consume ,
                skipWhitespace();
                tok = peek();
                # Check for EOF after comma
                if (tok.type == DPQL_TOK_EOF) {
                    break;
                }
            }
            first = False;

            # Parse index item: int or range
            items += parseIndexItem();
        }

        if (!closed) {
            addError("Unclosed index accessor, expected ']'", "DPQL-E022", lbracket);
            throw "DPQL-PARSE-ERROR", "Unclosed index accessor";
        }

        if (!items) {
            addError("Empty index accessor is not allowed", "DPQL-E022", lbracket);
            throw "DPQL-PARSE-ERROR", "Empty index accessor";
        }

        # Determine expression type: INDEX (single int) or SLICE (multiple or range)
        if (items.size() == 1 && items[0].typeCode() == NT_INT) {
            return <DataProviderExpression>{
                "exp": DP_FUNC_INDEX,
                "args": (base, items[0]),
            };
        }

        return <DataProviderExpression>{
            "exp": DP_FUNC_SLICE,
            "args": (base,) + items,
        };
    }

    #! Parses a single index item: integer or range
    private auto parseIndexItem() {
        skipWhitespace();
        hash<DpqlTokenInfo> tok = peek();

        # Check for open-start range: ..n
        if (tok.type == DPQL_TOK_RANGE) {
            nextToken();  # consume ..
            skipWhitespace();
            tok = peek();
            if (tok.type == DPQL_TOK_RBRACKET || tok.type == DPQL_TOK_COMMA) {
                # [..] alone is not allowed
                addError("Range must have at least one bound; '[..]' is not supported", "DPQL-E021", tok);
                throw "DPQL-PARSE-ERROR", "Range must have at least one bound";
            }
            int end_idx = parseInteger();
            return <DataProviderRangeSpec>{
                "end": end_idx,
            };
        }

        # Parse first integer
        int start_idx = parseInteger();

        # Check for range continuation: n..m or n..
        skipWhitespace();
        tok = peek();
        if (tok.type == DPQL_TOK_RANGE) {
            nextToken();  # consume ..
            skipWhitespace();
            tok = peek();
            # Check for open-end range: n..
            if (tok.type == DPQL_TOK_RBRACKET || tok.type == DPQL_TOK_COMMA) {
                return <DataProviderRangeSpec>{
                    "start": start_idx,
                };
            }
            # Full range: n..m
            int end_idx = parseInteger();
            return <DataProviderRangeSpec>{
                "start": start_idx,
                "end": end_idx,
            };
        }

        # Single integer
        return start_idx;
    }

    #! Parses an integer, errors if float
    private int parseInteger() {
        hash<DpqlTokenInfo> tok = peek();
        if (tok.type != DPQL_TOK_NUMBER) {
            addError("Expected integer in index", "DPQL-E022", tok);
            throw "DPQL-PARSE-ERROR", "Expected integer in index";
        }
        nextToken();  # consume number

        string value = tok.value;
        # Check for float
        if (index(value, ".") >= 0) {
            addError("Index must be integer, not float", "DPQL-E023", tok);
            throw "DPQL-PARSE-ERROR", "Index must be integer, not float";
        }
        return value.toInt();
    }

    #! Parses key accessor: {key}, {"key"}, {key1, key2}
    /** @param base the base expression to apply key access to
        @return a DP_FUNC_KEY expression node
    */
    private hash<DataProviderExpression> parseKeyAccessor(auto base) {
        hash<DpqlTokenInfo> lbrace = nextToken();  # consume {
        skipWhitespace();

        list<string> key_list = ();
        bool first = True;
        bool closed = False;

        while (True) {
            skipWhitespace();
            hash<DpqlTokenInfo> tok = peek();

            # Check for EOF - unclosed accessor
            if (tok.type == DPQL_TOK_EOF) {
                break;
            }

            if (tok.type == DPQL_TOK_RBRACE) {
                nextToken();  # consume }
                closed = True;
                break;
            }

            if (!first) {
                if (tok.type != DPQL_TOK_COMMA) {
                    addError("Expected ',' or '}' in key accessor", "DPQL-E024", tok);
                    throw "DPQL-PARSE-ERROR", "Expected ',' or '}'";
                }
                nextToken();  # consume ,
                skipWhitespace();
                tok = peek();
                # Check for EOF after comma
                if (tok.type == DPQL_TOK_EOF) {
                    break;
                }
            }
            first = False;

            key_list += parseKeyName();
        }

        if (!closed) {
            addError("Unclosed key accessor, expected '}'", "DPQL-E024", lbrace);
            throw "DPQL-PARSE-ERROR", "Unclosed key accessor";
        }

        if (!key_list) {
            addError("Empty key accessor is not allowed", "DPQL-E024", lbrace);
            throw "DPQL-PARSE-ERROR", "Empty key accessor";
        }

        return <DataProviderExpression>{
            "exp": DP_FUNC_KEY,
            "args": (base,) + key_list,
        };
    }

    #! Parses a key name: identifier or string literal
    private string parseKeyName() {
        hash<DpqlTokenInfo> tok = peek();

        if (tok.type == DPQL_TOK_STRING) {
            return parseStringLiteral();
        }

        if (tok.type == DPQL_TOK_IDENTIFIER) {
            nextToken();
            return tok.value;
        }

        if (tok.type == DPQL_TOK_NUMBER) {
            # Allow numeric keys
            nextToken();
            return tok.value;
        }

        addError("Expected key name in key accessor", "DPQL-E024", tok);
        throw "DPQL-PARSE-ERROR", "Expected key name";
    }

    #! Parses a string literal
    private string parseStringLiteral() {
        hash<DpqlTokenInfo> tok = nextToken();
        string value = tok.value;

        # Remove quotes and process escapes
        string content = substr(value, 1, value.size() - 2);
        return unescapeString(content);
    }

    #! Unescapes a string by processing escape sequences
    /** Handles \\n, \\r, \\t, \\\\, \\', \\" and passes through other backslash sequences
    */
    private string unescapeString(string content) {
        string result = "";
        int i = 0;
        while (i < content.size()) {
            if (content[i] == "\\") {
                if (i + 1 < content.size()) {
                    string next = content[i + 1];
                    switch (next) {
                        case "n":
                            result += "\n";
                            break;
                        case "r":
                            result += "\r";
                            break;
                        case "t":
                            result += "\t";
                            break;
                        case "\\":
                            result += "\\";
                            break;
                        case "'":
                            result += "'";
                            break;
                        case '"':
                            result += '"';
                            break;
                        default:
                            result += next;
                    }
                    i += 2;
                } else {
                    result += "\\";
                    ++i;
                }
            } else {
                result += content[i];
                ++i;
            }
        }
        return result;
    }

    #! Parses a numeric literal
    private auto parseNumberLiteral() {
        hash<DpqlTokenInfo> tok = nextToken();
        string value = tok.value;

        # Determine if float or int
        if (index(value, ".") >= 0 || index(value, "e") >= 0 || index(value, "E") >= 0) {
            return value.toFloat();
        }
        return value.toInt();
    }

    #! Parses a date literal
    private date parseDateLiteral() {
        hash<DpqlTokenInfo> tok = nextToken();
        return date(tok.value);
    }

    #! Parses a binary literal
    private binary parseBinaryLiteral() {
        hash<DpqlTokenInfo> tok = nextToken();
        string value = tok.value;
        if (value.size() < 3 || value[0] != "<" || value[value.size() - 1] != ">") {
            throw "DPQL-PARSE-ERROR", "Invalid binary literal";
        }
        string hex = substr(value, 1, value.size() - 2);
        return parse_hex_string(hex);
    }

    #! Parses a list or hash literal
    private auto parseStructuredLiteral() {
        hash<DpqlTokenInfo> tok = nextToken();
        try {
            return parse_to_qore_value(tok.value);
        } catch (hash<ExceptionInfo> ex) {
            addError(ex.desc ?? "Invalid list/hash literal", "DPQL-E013", tok);
            throw "DPQL-PARSE-ERROR", "Invalid list/hash literal";
        }
    }

    #! Parses a function call
    /** Resolves the DPQL symbol to the internal expression name using the symbol-to-name
        map. Falls back to direct expression map lookup for backward compatibility with
        old name-based syntax.

        @since DataProvider 3.4 symbol-based resolution
    */
    private hash<DataProviderExpression> parseFunctionCall(hash<DpqlTokenInfo> name_tok) {
        string token_name = name_tok.value;

        # Resolve symbol to internal expression name
        # 1. Try symbol→name lookup (new symbol-based syntax)
        # 2. Fall back to direct expression map key lookup (backward compat)
        string name;
        if (symbol_to_name{token_name}) {
            name = symbol_to_name{token_name};
        } else if (expressions{token_name}) {
            name = token_name;
        } else {
            name = token_name;
        }

        expect(DPQL_TOK_LPAREN, "'('");
        skipWhitespace();

        list<auto> args = ();
        list<hash<DpqlTokenInfo>> arg_tokens = ();
        if (!atEnd() && peek().type != DPQL_TOK_RPAREN) {
            args = parseArgList(\arg_tokens);
        }

        skipWhitespace();
        expect(DPQL_TOK_RPAREN, "')'");

        # Validate function name against expression map
        if (expressions && !expressions{name}) {
            addWarning(sprintf("Unknown function '%s'", token_name), "DPQL-W001", name_tok);
        } else if (expressions{name}) {
            hash<DataProviderExpressionInfo> info = expressions{name};
            int expected = info.args.size();
            int actual = args.size();
            if (info.varargs) {
                int min = info.min_args ?? (expected > 0 ? expected : 0);
                if (actual < min) {
                    addWarning(
                        sprintf("Function '%s' expects at least %d argument%s, got %d",
                            token_name, min, min == 1 ? "" : "s", actual),
                        "DPQL-W002", name_tok);
                }
            } else if (expected > 0 && actual != expected) {
                addWarning(
                    sprintf("Function '%s' expects %d argument%s, got %d",
                        token_name, expected, expected == 1 ? "" : "s", actual),
                    "DPQL-W002", name_tok);
            }

            # Type-aware argument validation
            validateFunctionArgTypes(token_name, info, args, arg_tokens);
        }

        return <DataProviderExpression>{
            "exp": name,
            "args": args,
        };
    }

    #! Validates argument types against function signature
    /** Checks:
        - \c type_code constraints: ST_Field requires a field reference, ST_Value requires a literal/expression
        - Data type compatibility: inferred argument type vs expected signature type

        @since DataProvider 3.4
    */
    private validateFunctionArgTypes(string name, hash<DataProviderExpressionInfo> info,
            list<auto> args, list<hash<DpqlTokenInfo>> arg_tokens) {
        int check_count = args.size() < info.args.size() ? args.size() : info.args.size();
        for (int i = 0; i < check_count; ++i) {
            hash<DataProviderSignatureTypeInfo> sig = info.args[i];
            auto arg = args[i];
            hash<DpqlTokenInfo> tok = arg_tokens[i];

            bool is_field_ref = arg instanceof hash<DataProviderFieldReference>;

            # Check type_code constraints
            if (sig.type_code == ST_Field && !is_field_ref) {
                string pname = info.arg_info[i].display_name ?? sprintf("arg%d", i);
                addWarning(
                    sprintf("Argument '%s' of '%s' expects a field reference (@field)", pname, name),
                    "DPQL-W003", tok);
                continue;
            }
            if (sig.type_code == ST_Value && is_field_ref) {
                string pname = info.arg_info[i].display_name ?? sprintf("arg%d", i);
                addWarning(
                    sprintf("Argument '%s' of '%s' expects a literal value, not a field reference",
                        pname, name),
                    "DPQL-W003", tok);
                continue;
            }

            # Infer argument type and check compatibility
            *AbstractDataProviderType arg_type = inferArgType(arg);
            if (!arg_type) {
                # Cannot determine type (template, complex expression) — skip
                continue;
            }

            # Check if the argument type is compatible with the expected type
            if (!sig.type.isAssignableFrom(arg_type)) {
                string pname = info.arg_info[i].display_name ?? sprintf("arg%d", i);
                addWarning(
                    sprintf("Argument '%s' of '%s' has type '%s', expected '%s'",
                        pname, name, arg_type.getName(), sig.type.getName()),
                    "DPQL-W004", tok);
            }
        }

        # For varargs, validate remaining args against the last signature type
        if (info.varargs && info.args.size() > 0 && args.size() > info.args.size()) {
            hash<DataProviderSignatureTypeInfo> last_sig = info.args[info.args.size() - 1];
            for (int i = info.args.size(); i < args.size(); ++i) {
                auto arg = args[i];
                hash<DpqlTokenInfo> tok = arg_tokens[i];
                bool is_field_ref = arg instanceof hash<DataProviderFieldReference>;

                if (last_sig.type_code == ST_Field && !is_field_ref) {
                    addWarning(
                        sprintf("Varargs argument %d of '%s' expects a field reference",
                            i + 1, name),
                        "DPQL-W003", tok);
                    continue;
                }
                if (last_sig.type_code == ST_Value && is_field_ref) {
                    addWarning(
                        sprintf("Varargs argument %d of '%s' expects a literal value",
                            i + 1, name),
                        "DPQL-W003", tok);
                    continue;
                }

                *AbstractDataProviderType arg_type = inferArgType(arg);
                if (arg_type && !last_sig.type.isAssignableFrom(arg_type)) {
                    addWarning(
                        sprintf("Varargs argument %d of '%s' has type '%s', expected '%s'",
                            i + 1, name, arg_type.getName(), last_sig.type.getName()),
                        "DPQL-W004", tok);
                }
            }
        }
    }

    #! Infers the type of a parsed argument
    /** @return the inferred type, or NOTHING if the type cannot be determined

        @since DataProvider 3.4
    */
    private *AbstractDataProviderType inferArgType(auto arg) {
        # Literal values: determine type from Qore type code
        switch (arg.typeCode()) {
            case NT_STRING:
                return AbstractDataProviderType::get(StringType);
            case NT_INT:
                return AbstractDataProviderType::get(IntType);
            case NT_FLOAT:
                return AbstractDataProviderType::get(FloatType);
            case NT_BOOLEAN:
                return AbstractDataProviderType::get(BoolType);
            case NT_NUMBER:
                return AbstractDataProviderType::get(NumberType);
            case NT_DATE:
                return AbstractDataProviderType::get(DateType);
            case NT_BINARY:
                return AbstractDataProviderType::get(BinaryType);
            case NT_LIST:
                return AbstractDataProviderType::get(SoftListType);
            case NT_NULL:
            case NT_NOTHING:
                return AbstractDataProviderType::get(NothingType);
        }

        # Hash types: field references and expressions
        if (arg instanceof hash<DataProviderFieldReference>) {
            # Look up field type from fields map
            if (fields{arg.field}) {
                return fields{arg.field}.getType();
            }
            # Unknown field — cannot determine type
            return NOTHING;
        }

        if (arg instanceof hash<DataProviderExpression>) {
            # Function call — look up return type from expression map
            if (expressions{arg.exp}) {
                return expressions{arg.exp}.return_type;
            }
            # Built-in operators — return type depends on operator
            return NOTHING;
        }

        return NOTHING;
    }

    #! Parses an argument list, also collecting the start token of each argument
    /** @param arg_tokens output list populated with the start token of each argument

        @since DataProvider 3.4 added arg_tokens parameter
    */
    private list<auto> parseArgList(reference<list<hash<DpqlTokenInfo>>> arg_tokens) {
        list<auto> args = ();

        # First argument
        hash<DpqlTokenInfo> start_tok = peek();
        args += parseOrExpr();
        arg_tokens += start_tok;
        skipWhitespace();

        # Additional arguments
        while (!atEnd() && peek().type == DPQL_TOK_COMMA) {
            nextToken();  # consume comma
            skipWhitespace();
            start_tok = peek();
            args += parseOrExpr();
            arg_tokens += start_tok;
            skipWhitespace();
        }

        return args;
    }

    #! Parses a value list (for 'in' operator)
    private list<auto> parseValueList() {
        list<auto> values = ();

        # First value
        values += parseValue();
        skipWhitespace();

        # Additional values
        while (!atEnd() && peek().type == DPQL_TOK_COMMA) {
            nextToken();  # consume comma
            skipWhitespace();
            values += parseValue();
            skipWhitespace();
        }

        return values;
    }
}

} # namespace DataProvider
