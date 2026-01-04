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
    - Field references: @field, @"field with spaces"
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
        #! Current line number for error reporting
        int line = 1;
        #! Current column number for error reporting
        int column = 1;
    }

    #! Creates a parser with optional expression map for validation
    /** @param expressions optional map of expression names to their info for validation
    */
    constructor(*hash<string, hash<DataProviderExpressionInfo>> expressions) {
        self.expressions = expressions;
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
                exp = parseOrExpr();
                skipWhitespace();
                if (!atEnd()) {
                    hash<DpqlTokenInfo> tok = peek();
                    addError(sprintf("Unexpected token '%s' after expression", tok.value),
                        "DPQL-E001", tok);
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

        return <DpqlParseResult>{
            "expression": exp,
            "diagnostics": errors,
            "success": !parse_failed && !errors,
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
                    expect(DPQL_TOK_LPAREN, "'('");
                    skipWhitespace();
                    list<auto> values = parseValueList();
                    skipWhitespace();
                    expect(DPQL_TOK_RPAREN, "')'");
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

            case DPQL_TOK_STRING:
                return parseStringLiteral();

            case DPQL_TOK_NUMBER:
                return parseNumberLiteral();

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
                    return parseFunctionCall(id_tok.value);
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
                # Unknown identifier as value
                addError(sprintf("Unexpected identifier '%s' in value position", id_tok.value),
                    "DPQL-E001", id_tok);
                throw "DPQL-PARSE-ERROR", "Unexpected identifier";
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

    #! Parses a field reference
    private hash<DataProviderFieldReference> parseFieldRef() {
        hash<DpqlTokenInfo> tok = nextToken();
        string value = tok.value;

        # Extract field name from @field or @"field"
        string field_name;
        if (value.size() > 1) {
            if (value[1] == '"') {
                # Quoted field: @"field name"
                # Remove @" prefix and " suffix
                field_name = substr(value, 2, value.size() - 3);
                # Process escape sequences
                field_name = replace(replace(field_name, "\\\"", "\""), "\\\\", "\\");
            } else {
                # Unquoted field: @field
                field_name = substr(value, 1);
            }
        } else {
            addError("Invalid field reference", "DPQL-E003", tok);
            throw "DPQL-PARSE-ERROR", "Invalid field reference";
        }

        return <DataProviderFieldReference>{
            "field": field_name,
        };
    }

    #! Parses a string literal
    private string parseStringLiteral() {
        hash<DpqlTokenInfo> tok = nextToken();
        string value = tok.value;

        # Remove quotes and process escapes
        string quote = value[0];
        string content = substr(value, 1, value.size() - 2);

        # Process escape sequences
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

    #! Parses a function call
    private hash<DataProviderExpression> parseFunctionCall(string name) {
        expect(DPQL_TOK_LPAREN, "'('");
        skipWhitespace();

        list<auto> args = ();
        if (!atEnd() && peek().type != DPQL_TOK_RPAREN) {
            args = parseArgList();
        }

        skipWhitespace();
        expect(DPQL_TOK_RPAREN, "')'");

        return <DataProviderExpression>{
            "exp": name,
            "args": args,
        };
    }

    #! Parses an argument list
    private list<auto> parseArgList() {
        list<auto> args = ();

        # First argument
        args += parseOrExpr();
        skipWhitespace();

        # Additional arguments
        while (!atEnd() && peek().type == DPQL_TOK_COMMA) {
            nextToken();  # consume comma
            skipWhitespace();
            args += parseOrExpr();
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
