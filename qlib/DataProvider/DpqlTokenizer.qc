# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DpqlTokenizer.qc DPQL tokenizer implementation

/*  DpqlTokenizer.qc Copyright 2026 Qore Technologies, s.r.o.

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

/** @defgroup dpql_token_types DPQL Token Type Constants
    These constants define the different token types recognized by @ref DpqlTokenizer
*/
#!@{

#! Field reference (\c \@field or <tt>\@\"quoted field\"</tt>)
public const DPQL_TOK_FIELD = 1;
#! String literal ("string" or 'string')
public const DPQL_TOK_STRING = 2;
#! Numeric literal (123, 45.67, -123)
public const DPQL_TOK_NUMBER = 3;
#! Boolean literal (true, false)
public const DPQL_TOK_BOOLEAN = 4;
#! Null literal
public const DPQL_TOK_NULL = 5;
#! Identifier (function names, keywords like between, and, in)
public const DPQL_TOK_IDENTIFIER = 6;
#! Date literal (ISO-8601)
public const DPQL_TOK_DATE = 7;
#! Binary literal (<tt>&lt;hex&gt;</tt>)
public const DPQL_TOK_BINARY = 8;
#! List literal (parse_to_qore_value format)
public const DPQL_TOK_LIST = 21;
#! Hash literal (parse_to_qore_value format)
public const DPQL_TOK_HASH = 22;
#! Equals operator (==)
public const DPQL_TOK_OP_EQ = 10;
#! Not equals operator (!=)
public const DPQL_TOK_OP_NE = 11;
#! Less than operator (<)
public const DPQL_TOK_OP_LT = 12;
#! Less than or equals operator (<=)
public const DPQL_TOK_OP_LE = 13;
#! Greater than operator (>)
public const DPQL_TOK_OP_GT = 14;
#! Greater than or equals operator (>=)
public const DPQL_TOK_OP_GE = 15;
#! Logical AND operator (&&)
public const DPQL_TOK_OP_AND = 16;
#! Logical OR operator (||)
public const DPQL_TOK_OP_OR = 17;
#! Logical NOT operator (!)
public const DPQL_TOK_OP_NOT = 18;
#! Regex match operator (=~)
public const DPQL_TOK_OP_REGEX = 19;
#! Regex pattern (/pattern/flags)
public const DPQL_TOK_REGEX = 20;
#! Left parenthesis
public const DPQL_TOK_LPAREN = 30;
#! Right parenthesis
public const DPQL_TOK_RPAREN = 31;
#! Comma
public const DPQL_TOK_COMMA = 32;
#! Left bracket (for index/slice accessor)
public const DPQL_TOK_LBRACKET = 33;
#! Right bracket (for index/slice accessor)
public const DPQL_TOK_RBRACKET = 34;
#! Left brace (for key accessor, NOT hash literal)
public const DPQL_TOK_LBRACE = 35;
#! Right brace (for key accessor)
public const DPQL_TOK_RBRACE = 36;
#! Range operator (..)
public const DPQL_TOK_RANGE = 37;
#! Dot (for member access after accessor)
public const DPQL_TOK_DOT = 38;
#! Template reference ($context:value)
public const DPQL_TOK_TEMPLATE = 39;
#! Whitespace (spaces, tabs)
public const DPQL_TOK_WHITESPACE = 40;
#! Newline character
public const DPQL_TOK_NEWLINE = 41;
#! End of input
public const DPQL_TOK_EOF = 50;
#! Error token (invalid input)
public const DPQL_TOK_ERROR = 99;

#!@}

#! Maps token type codes to human-readable names
public const DpqlTokenTypeNames = {
    DPQL_TOK_FIELD: "field",
    DPQL_TOK_STRING: "string",
    DPQL_TOK_NUMBER: "number",
    DPQL_TOK_BOOLEAN: "boolean",
    DPQL_TOK_NULL: "null",
    DPQL_TOK_IDENTIFIER: "identifier",
    DPQL_TOK_DATE: "date",
    DPQL_TOK_BINARY: "binary",
    DPQL_TOK_LIST: "list",
    DPQL_TOK_HASH: "hash",
    DPQL_TOK_OP_EQ: "==",
    DPQL_TOK_OP_NE: "!=",
    DPQL_TOK_OP_LT: "<",
    DPQL_TOK_OP_LE: "<=",
    DPQL_TOK_OP_GT: ">",
    DPQL_TOK_OP_GE: ">=",
    DPQL_TOK_OP_AND: "&&",
    DPQL_TOK_OP_OR: "||",
    DPQL_TOK_OP_NOT: "!",
    DPQL_TOK_OP_REGEX: "=~",
    DPQL_TOK_REGEX: "regex",
    DPQL_TOK_LPAREN: "(",
    DPQL_TOK_RPAREN: ")",
    DPQL_TOK_COMMA: ",",
    DPQL_TOK_LBRACKET: "[",
    DPQL_TOK_RBRACKET: "]",
    DPQL_TOK_LBRACE: "{",
    DPQL_TOK_RBRACE: "}",
    DPQL_TOK_RANGE: "..",
    DPQL_TOK_DOT: ".",
    DPQL_TOK_TEMPLATE: "template",
    DPQL_TOK_WHITESPACE: "whitespace",
    DPQL_TOK_NEWLINE: "newline",
    DPQL_TOK_EOF: "eof",
    DPQL_TOK_ERROR: "error",
};

#! DPQL Token information structure
/** This structure contains information about a single token including its type,
    value, and position in the source text.
*/
public hashdecl DpqlTokenInfo {
    #! Token type (one of the DPQL_TOK_* constants)
    int type;
    #! The actual text of the token
    string value;
    #! Line number where the token starts (1-based)
    int line;
    #! Column number where the token starts (1-based)
    int column;
    #! Line number where the token ends (1-based)
    int end_line;
    #! Column number where the token ends (1-based)
    int end_column;
}

#! DPQL Diagnostic message structure
/** Contains information about errors, warnings, or informational messages
    generated during tokenization or parsing.
*/
public hashdecl DpqlDiagnostic {
    #! Severity: "error", "warning", or "info"
    string severity;
    #! Human-readable message
    string message;
    #! Error code for categorization (e.g., "DPQL-E001")
    string code;
    #! Start line number (1-based)
    int line;
    #! Start column number (1-based)
    int column;
    #! End line number (1-based)
    int end_line;
    #! End column number (1-based)
    int end_column;
}

#! Tokenizer for DPQL (Data Provider Query Language) expressions
/** This class provides lexical analysis of DPQL query expressions, correctly handling:
    - Field references (\c \@field or <tt>\@\"field with spaces\"</tt>)
    - String literals (single and double quoted)
    - Numeric literals (integers, decimals, negative numbers)
    - Date literals (ISO-8601)
    - Binary literals (<tt>&lt;hex&gt;</tt>)
    - List literals (parse_to_qore_value format)
    - Hash literals (parse_to_qore_value format)
    - Boolean and null literals
    - Comparison operators (==, !=, <, <=, >, >=)
    - Logical operators (&&, ||, !)
    - Regex match operator (=~) and patterns
    - Keywords (between, and, in, true, false, null)

    @par Example:
    @code{.py}
DpqlTokenizer tok('@name == "John" && @age > 18');
while (!tok.atEnd()) {
    hash<DpqlTokenInfo> t = tok.next();
    printf("Token: type=%s value=%y line=%d col=%d\n",
        DpqlTokenTypeNames{t.type}, t.value, t.line, t.column);
}
    @endcode
*/
public class DpqlTokenizer {
    private {
        #! Input text
        string input;
        #! Current position (character index)
        int pos = 0;
        #! Current line number (1-based)
        int line = 1;
        #! Current column number (1-based)
        int column = 1;
        #! Input length in characters
        int len;
        #! Cached next token for peek()
        *hash<DpqlTokenInfo> peeked;
        #! Collected errors during tokenization
        list<hash<DpqlDiagnostic>> errors = ();
        #! Context flag: when True, '{' is a key accessor; when False, it's a hash literal
        bool in_field_ref_context = False;
    }

    #! Creates a tokenizer for the given DPQL expression
    /** @param text the DPQL expression to tokenize
    */
    constructor(string text) {
        input = text;
        len = text.length();
    }

    #! Returns True if at end of input
    bool atEnd() {
        return !peeked && pos >= len;
    }

    #! Returns the next token without consuming it
    /** @return the next token, or a DPQL_TOK_EOF token if at end of input
    */
    hash<DpqlTokenInfo> peek() {
        if (!peeked) {
            peeked = scanToken();
        }
        return peeked;
    }

    #! Returns the next token and advances the position
    /** @return the next token, or a DPQL_TOK_EOF token if at end of input
    */
    hash<DpqlTokenInfo> next() {
        if (peeked) {
            hash<DpqlTokenInfo> result = peeked;
            delete peeked;
            return result;
        }
        return scanToken();
    }

    #! Returns the previous non-whitespace character, or "" if none
    private string prevNonWhitespaceChar() {
        for (int i = pos - 1; i >= 0; --i) {
            string c = input[i];
            if (c =~ /[ \t\r\n]/) {
                continue;
            }
            return c;
        }
        return "";
    }

    #! Returns all tokens as a list (for syntax highlighting)
    /** @return a list of all tokens in the input
    */
    list<hash<DpqlTokenInfo>> getAllTokens() {
        list<hash<DpqlTokenInfo>> tokens = ();
        while (!atEnd()) {
            tokens += next();
        }
        return tokens;
    }

    #! Returns collected tokenization errors
    /** @return a list of diagnostic messages for any errors encountered
    */
    list<hash<DpqlDiagnostic>> getErrors() {
        return errors;
    }

    #! Returns the current line number
    int getLine() {
        return line;
    }

    #! Returns the current column number
    int getColumn() {
        return column;
    }

    #! Returns the character at the current position, or NOTHING if at end
    private:internal *string currentChar() {
        if (pos >= len) {
            return NOTHING;
        }
        return input[pos];
    }

    #! Returns the character at offset from current position, or NOTHING if out of bounds
    private:internal *string peekChar(int offset = 1) {
        int p = pos + offset;
        if (p >= len || p < 0) {
            return NOTHING;
        }
        return input[p];
    }

    #! Advances position by count characters, updating line/column
    private:internal advance(int count = 1) {
        for (int i = 0; i < count && pos < len; ++i) {
            if (input[pos] == "\n") {
                ++line;
                column = 1;
            } else {
                ++column;
            }
            ++pos;
        }
    }

    #! Creates a token with the given type and value
    private:internal hash<DpqlTokenInfo> makeToken(int type, string value, int start_line, int start_column) {
        return <DpqlTokenInfo>{
            "type": type,
            "value": value,
            "line": start_line,
            "column": start_column,
            "end_line": line,
            "end_column": column,
        };
    }

    #! Adds an error diagnostic
    private:internal addError(string message, string code, int start_line, int start_column,
            int end_line, int end_column) {
        errors += <DpqlDiagnostic>{
            "severity": "error",
            "message": message,
            "code": code,
            "line": start_line,
            "column": start_column,
            "end_line": end_line,
            "end_column": end_column,
        };
    }

    #! Scans and returns the next token
    private:internal hash<DpqlTokenInfo> scanToken() {
        if (pos >= len) {
            return makeToken(DPQL_TOK_EOF, "", line, column);
        }

        int start_line = line;
        int start_column = column;
        *string c = currentChar();

        # Newline
        if (c == "\n") {
            advance();
            return makeToken(DPQL_TOK_NEWLINE, "\n", start_line, start_column);
        }

        # Whitespace (not newline)
        if (c == " " || c == "\t" || c == "\r") {
            string ws = "";
            while (pos < len) {
                c = currentChar();
                if (c != " " && c != "\t" && c != "\r") {
                    break;
                }
                ws += c;
                advance();
            }
            return makeToken(DPQL_TOK_WHITESPACE, ws, start_line, start_column);
        }

        # Field reference - sets context for accessor parsing
        if (c == "@") {
            hash<DpqlTokenInfo> tok = scanFieldRef();
            in_field_ref_context = True;
            return tok;
        }

        # Single-quoted string
        if (c == "'") {
            in_field_ref_context = False;
            return scanString("'");
        }

        # Double-quoted string
        if (c == '"') {
            in_field_ref_context = False;
            return scanString('"');
        }

        # Left bracket - index/slice accessor
        if (c == "[") {
            advance();
            # Stay in field ref context until ] is found
            return makeToken(DPQL_TOK_LBRACKET, "[", start_line, start_column);
        }

        # Right bracket - end of index/slice accessor
        if (c == "]") {
            advance();
            in_field_ref_context = True;  # After ], we can still have more accessors
            return makeToken(DPQL_TOK_RBRACKET, "]", start_line, start_column);
        }

        # Brace handling - context sensitive
        if (c == "{") {
            if (in_field_ref_context) {
                # Key accessor
                advance();
                return makeToken(DPQL_TOK_LBRACE, "{", start_line, start_column);
            }
            # Hash literal
            in_field_ref_context = False;
            return scanHashLiteral();
        }

        # Right brace - end of key accessor
        if (c == "}") {
            advance();
            in_field_ref_context = True;  # After }, we can still have more accessors
            return makeToken(DPQL_TOK_RBRACE, "}", start_line, start_column);
        }

        # Range operator (..) - must check before dot
        if (c == "." && peekChar() == ".") {
            advance(2);
            return makeToken(DPQL_TOK_RANGE, "..", start_line, start_column);
        }

        # Dot - member access after accessor
        if (c == "." && in_field_ref_context) {
            advance();
            return makeToken(DPQL_TOK_DOT, ".", start_line, start_column);
        }

        # Regex pattern (after =~ operator context, or standalone)
        if (c == "/") {
            return scanRegex();
        }

        # Date literal (YYYY-MM-DD with optional time)
        if (c =~ /[0-9]/ && looksLikeDateLiteral()) {
            return scanDateLiteral();
        }

        # Number (including negative)
        if (c =~ /[0-9]/ || (c == "-" && peekChar() =~ /[0-9]/)) {
            return scanNumber();
        }

        # Operators - reset field ref context since operators end accessor chains
        if (c == "=") {
            in_field_ref_context = False;
            if (peekChar() == "=") {
                advance(2);
                return makeToken(DPQL_TOK_OP_EQ, "==", start_line, start_column);
            }
            if (peekChar() == "~") {
                advance(2);
                return makeToken(DPQL_TOK_OP_REGEX, "=~", start_line, start_column);
            }
            # Single = is not valid in DPQL
            advance();
            addError("Invalid operator '=', did you mean '=='?", "DPQL-E004",
                start_line, start_column, line, column);
            return makeToken(DPQL_TOK_ERROR, "=", start_line, start_column);
        }

        if (c == "!") {
            in_field_ref_context = False;
            if (peekChar() == "=") {
                advance(2);
                return makeToken(DPQL_TOK_OP_NE, "!=", start_line, start_column);
            }
            # Single ! is logical NOT
            advance();
            return makeToken(DPQL_TOK_OP_NOT, "!", start_line, start_column);
        }

        if (c == "<") {
            in_field_ref_context = False;
            if (peekChar() == "=") {
                advance(2);
                return makeToken(DPQL_TOK_OP_LE, "<=", start_line, start_column);
            }
            if (looksLikeBinaryLiteral()) {
                return scanBinaryLiteral();
            }
            advance();
            return makeToken(DPQL_TOK_OP_LT, "<", start_line, start_column);
        }

        if (c == ">") {
            in_field_ref_context = False;
            if (peekChar() == "=") {
                advance(2);
                return makeToken(DPQL_TOK_OP_GE, ">=", start_line, start_column);
            }
            advance();
            return makeToken(DPQL_TOK_OP_GT, ">", start_line, start_column);
        }

        if (c == "&") {
            in_field_ref_context = False;
            if (peekChar() == "&") {
                advance(2);
                return makeToken(DPQL_TOK_OP_AND, "&&", start_line, start_column);
            }
            # Single & is invalid
            advance();
            addError("Invalid operator '&', did you mean '&&'?", "DPQL-E004",
                start_line, start_column, line, column);
            return makeToken(DPQL_TOK_ERROR, "&", start_line, start_column);
        }

        if (c == "|") {
            in_field_ref_context = False;
            if (peekChar() == "|") {
                advance(2);
                return makeToken(DPQL_TOK_OP_OR, "||", start_line, start_column);
            }
            # Single | is invalid
            advance();
            addError("Invalid operator '|', did you mean '||'?", "DPQL-E004",
                start_line, start_column, line, column);
            return makeToken(DPQL_TOK_ERROR, "|", start_line, start_column);
        }

        # Punctuation
        if (c == "(") {
            in_field_ref_context = False;
            string prev = prevNonWhitespaceChar();
            if (looksLikeListLiteral()
                && (!prev || !(prev =~ /[a-zA-Z0-9_]/ || prev == "@" || prev == "."
                    || prev == "'" || prev == '"' || prev == ")"))) {
                return scanListLiteral();
            }
            advance();
            return makeToken(DPQL_TOK_LPAREN, "(", start_line, start_column);
        }

        if (c == ")") {
            in_field_ref_context = False;
            advance();
            return makeToken(DPQL_TOK_RPAREN, ")", start_line, start_column);
        }

        if (c == ",") {
            # Reset field ref context on comma so that subsequent { is a hash literal
            # e.g., in "@a, {b=1}" the {b=1} should be a hash, not a key accessor
            in_field_ref_context = False;
            advance();
            return makeToken(DPQL_TOK_COMMA, ",", start_line, start_column);
        }

        # Template reference ($context:value)
        if (c == "$") {
            in_field_ref_context = False;
            return scanTemplateRef();
        }

        # Identifier (function names, keywords)
        if (c =~ /[a-zA-Z_]/) {
            # Only reset field-ref context if NOT in an accessor chain (i.e., not after a dot)
            # This allows expressions like "@data[0].user.name" to keep context across "user"
            string prev = prevNonWhitespaceChar();
            if (prev != ".") {
                in_field_ref_context = False;
            }
            return scanIdentifier();
        }

        # Unknown character
        advance();
        addError(sprintf("Unexpected character '%s'", c), "DPQL-E001",
            start_line, start_column, line, column);
        return makeToken(DPQL_TOK_ERROR, c, start_line, start_column);
    }

    #! Scans a field reference (\c \@field or <tt>\@\"quoted\"</tt>)
    private:internal hash<DpqlTokenInfo> scanFieldRef() {
        int start_line = line;
        int start_column = column;
        string value = "@";
        advance();  # skip @

        if (pos >= len) {
            addError("Incomplete field reference", "DPQL-E003",
                start_line, start_column, line, column);
            return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
        }

        *string c = currentChar();

        # Quoted field name
        if (c == '"') {
            value += '"';
            advance();
            while (pos < len) {
                c = currentChar();
                if (c == '"') {
                    value += c;
                    advance();
                    return makeToken(DPQL_TOK_FIELD, value, start_line, start_column);
                }
                if (c == "\\") {
                    value += c;
                    advance();
                    if (pos < len) {
                        value += currentChar();
                        advance();
                    }
                } else if (c == "\n") {
                    # Newline in field name is invalid
                    addError("Unterminated quoted field reference", "DPQL-E003",
                        start_line, start_column, line, column);
                    return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
                } else {
                    value += c;
                    advance();
                }
            }
            # Reached end without closing quote
            addError("Unterminated quoted field reference", "DPQL-E003",
                start_line, start_column, line, column);
            return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
        }

        # Unquoted field name (identifier)
        if (c =~ /[a-zA-Z_]/) {
            while (pos < len) {
                c = currentChar();
                if (c !~ /[a-zA-Z0-9_.]/) {
                    break;
                }
                # Don't allow consecutive dots (e.g., @user..name)
                if (c == "." && value.size() > 0 && value[value.size() - 1] == ".") {
                    break;
                }
                value += c;
                advance();
            }
            # Strip trailing dots - they are not part of the field name
            # (e.g., "@user." should tokenize as field "@user" followed by dot)
            while (value.size() > 1 && value[value.size() - 1] == ".") {
                value = substr(value, 0, value.size() - 1);
                --pos;
                --column;
            }
            return makeToken(DPQL_TOK_FIELD, value, start_line, start_column);
        }

        # Invalid field reference
        addError(sprintf("Invalid field reference: expected identifier or quoted string after '@', got '%s'", c),
            "DPQL-E003", start_line, start_column, line, column);
        return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
    }

    #! Scans a template reference ($context:value, $context:{bracketed}, $context:value::type)
    /** Template references follow the pattern:
        - \c $context:value — simple template
        - \c $context:{value} — bracketed value (allows special chars)
        - \c $context:value::type — with type assertion
        - \c $context:A??{$ctx:B} — fallback syntax (passed through raw)

        @return a DPQL_TOK_TEMPLATE token with the raw template text as the value

        @since DataProvider 3.3
    */
    private:internal hash<DpqlTokenInfo> scanTemplateRef() {
        int start_line = line;
        int start_column = column;
        string value = "$";
        advance();  # skip $

        if (pos >= len) {
            addError("Incomplete template reference: expected context identifier after '$'", "DPQL-E030",
                start_line, start_column, line, column);
            return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
        }

        *string c = currentChar();

        # Context identifier: letters, digits, underscores, hyphens (for qore-expr etc.)
        if (c !~ /[a-zA-Z_]/) {
            addError(sprintf("Invalid template reference: expected context identifier after '$', got '%s'", c),
                "DPQL-E031", start_line, start_column, line, column);
            return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
        }

        # Scan context identifier
        while (pos < len) {
            c = currentChar();
            if (c !~ /[a-zA-Z0-9_\-]/) {
                break;
            }
            value += c;
            advance();
        }

        # Expect ':' separator
        if (pos >= len || currentChar() != ":") {
            addError("Invalid template reference: expected ':' after context identifier", "DPQL-E032",
                start_line, start_column, line, column);
            return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
        }
        value += ":";
        advance();  # skip ':'

        if (pos >= len) {
            addError("Incomplete template reference: expected value after ':'", "DPQL-E033",
                start_line, start_column, line, column);
            return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
        }

        c = currentChar();

        # Bracketed value: $context:{...}
        if (c == "{") {
            value += c;
            advance();
            int depth = 1;
            while (pos < len && depth > 0) {
                c = currentChar();
                value += c;
                advance();
                if (c == "\\") {
                    # Escaped character — consume next char without interpreting it
                    if (pos < len) {
                        value += currentChar();
                        advance();
                    }
                } else if (c == '"' || c == "'") {
                    # String literal inside brackets — scan until matching unescaped quote
                    string quote = c;
                    while (pos < len) {
                        c = currentChar();
                        value += c;
                        advance();
                        if (c == "\\") {
                            if (pos < len) {
                                value += currentChar();
                                advance();
                            }
                        } else if (c == quote) {
                            break;
                        }
                    }
                } else if (c == "{") {
                    ++depth;
                } else if (c == "}") {
                    --depth;
                }
            }
            if (depth > 0) {
                addError("Unterminated bracketed template value", "DPQL-E034",
                    start_line, start_column, line, column);
                return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
            }
        } else {
            # Unbracketed value: alphanumeric, dots, underscores, *, and ??{...} fallback
            while (pos < len) {
                c = currentChar();
                if (c =~ /[a-zA-Z0-9_.*]/) {
                    value += c;
                    advance();
                } else if (c == "?" && peekChar() == "?") {
                    # Fallback syntax: ??{...}
                    value += "??";
                    advance(2);
                    if (pos < len && currentChar() == "{") {
                        value += "{";
                        advance();
                        int depth = 1;
                        while (pos < len && depth > 0) {
                            c = currentChar();
                            value += c;
                            advance();
                            if (c == "\\") {
                                if (pos < len) {
                                    value += currentChar();
                                    advance();
                                }
                            } else if (c == '"' || c == "'") {
                                string quote = c;
                                while (pos < len) {
                                    c = currentChar();
                                    value += c;
                                    advance();
                                    if (c == "\\") {
                                        if (pos < len) {
                                            value += currentChar();
                                            advance();
                                        }
                                    } else if (c == quote) {
                                        break;
                                    }
                                }
                            } else if (c == "{") {
                                ++depth;
                            } else if (c == "}") {
                                --depth;
                            }
                        }
                    }
                } else {
                    break;
                }
            }
        }

        # Check for optional ::type suffix
        if (pos + 1 < len && currentChar() == ":" && peekChar() == ":") {
            value += "::";
            advance(2);
            # Scan type identifier: alphanumeric, *, <, >
            while (pos < len) {
                c = currentChar();
                if (c =~ /[a-zA-Z0-9_*<>]/) {
                    value += c;
                    advance();
                } else {
                    break;
                }
            }
        }

        return makeToken(DPQL_TOK_TEMPLATE, value, start_line, start_column);
    }

    #! Scans a string literal
    private:internal hash<DpqlTokenInfo> scanString(string quote) {
        int start_line = line;
        int start_column = column;
        string value = quote;
        advance();  # skip opening quote

        while (pos < len) {
            *string c = currentChar();
            if (c == quote) {
                value += c;
                advance();
                return makeToken(DPQL_TOK_STRING, value, start_line, start_column);
            }
            if (c == "\\") {
                value += c;
                advance();
                if (pos < len) {
                    value += currentChar();
                    advance();
                }
            } else if (c == "\n") {
                # Allow multi-line strings
                value += c;
                advance();
            } else {
                value += c;
                advance();
            }
        }

        # Reached end without closing quote
        addError("Unterminated string literal", "DPQL-E002",
            start_line, start_column, line, column);
        return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
    }

    #! Scans a numeric literal
    private:internal hash<DpqlTokenInfo> scanNumber() {
        int start_line = line;
        int start_column = column;
        string value = "";

        # Handle negative sign
        if (currentChar() == "-") {
            value += "-";
            advance();
        }

        # Integer part
        while (pos < len && currentChar() =~ /[0-9]/) {
            value += currentChar();
            advance();
        }

        # Decimal part
        if (pos < len && currentChar() == "." && peekChar() =~ /[0-9]/) {
            value += ".";
            advance();
            while (pos < len && currentChar() =~ /[0-9]/) {
                value += currentChar();
                advance();
            }
        }

        # Exponent part (optional)
        if (pos < len && currentChar() =~ /[eE]/) {
            string exp = currentChar();
            advance();
            if (pos < len && currentChar() =~ /[+\-]/) {
                exp += currentChar();
                advance();
            }
            if (pos < len && currentChar() =~ /[0-9]/) {
                value += exp;
                while (pos < len && currentChar() =~ /[0-9]/) {
                    value += currentChar();
                    advance();
                }
            } else {
                # Invalid exponent - revert
                addError("Invalid numeric literal: incomplete exponent", "DPQL-E001",
                    start_line, start_column, line, column);
            }
        }

        return makeToken(DPQL_TOK_NUMBER, value, start_line, start_column);
    }

    #! Scans an identifier (keyword or function name)
    /** Supports hyphenated identifiers (e.g. \c ends-with, \c starts-with, \c list-contains)
        by treating \c - followed by a letter as part of the identifier.  A \c - followed by
        a digit or whitespace is left for the caller to handle as a minus operator.
    */
    private:internal hash<DpqlTokenInfo> scanIdentifier() {
        int start_line = line;
        int start_column = column;
        string value = "";

        while (pos < len) {
            string c = currentChar();
            if (c =~ /[a-zA-Z0-9_]/) {
                value += c;
                advance();
            } else if (c == "-" && pos + 1 < len && input[pos + 1] =~ /[a-zA-Z]/) {
                # Hyphenated identifier: treat '-' followed by a letter as part of the name
                value += c;
                advance();
            } else {
                break;
            }
        }

        # Check for keywords
        switch (value.lwr()) {
            case "true":
            case "false":
                return makeToken(DPQL_TOK_BOOLEAN, value, start_line, start_column);

            case "null":
                return makeToken(DPQL_TOK_NULL, value, start_line, start_column);

            default:
                # Regular identifier (between, and, in, or function name)
                return makeToken(DPQL_TOK_IDENTIFIER, value, start_line, start_column);
        }
    }

    #! Scans a regex pattern (/pattern/flags)
    private:internal hash<DpqlTokenInfo> scanRegex() {
        int start_line = line;
        int start_column = column;
        string value = "/";
        advance();  # skip opening /

        # Scan pattern
        while (pos < len) {
            *string c = currentChar();
            if (c == "/") {
                value += c;
                advance();
                # Scan optional flags
                while (pos < len && currentChar() =~ /[gimsxu]/) {
                    value += currentChar();
                    advance();
                }
                return makeToken(DPQL_TOK_REGEX, value, start_line, start_column);
            }
            if (c == "\\") {
                value += c;
                advance();
                if (pos < len) {
                    value += currentChar();
                    advance();
                }
            } else if (c == "\n") {
                # Newline in regex is invalid
                addError("Unterminated regex pattern", "DPQL-E009",
                    start_line, start_column, line, column);
                return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
            } else {
                value += c;
                advance();
            }
        }

        # Reached end without closing /
        addError("Unterminated regex pattern", "DPQL-E009",
            start_line, start_column, line, column);
        return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
    }

    #! Returns True if the next token is a list literal
    private bool looksLikeListLiteral() {
        if (input[pos] != "(") {
            return False;
        }

        int depth = 0;
        int brace_depth = 0;
        bool in_single = False;
        bool in_double = False;
        bool escape = False;
        bool has_delim = False;

        for (int i = pos; i < len; ++i) {
            string c = input[i];
            if (escape) {
                escape = False;
                continue;
            }
            if (in_single) {
                if (c == "\\") {
                    escape = True;
                } else if (c == "'") {
                    in_single = False;
                }
                continue;
            }
            if (in_double) {
                if (c == "\\") {
                    escape = True;
                } else if (c == '"') {
                    in_double = False;
                }
                continue;
            }

            if (c == "'") {
                in_single = True;
                continue;
            }
            if (c == '"') {
                in_double = True;
                continue;
            }
            if (c == "(") {
                ++depth;
                continue;
            }
            if (c == ")") {
                --depth;
                if (depth == 0) {
                    return has_delim;
                }
                continue;
            }
            if (c == "{") {
                ++brace_depth;
                continue;
            }
            if (c == "}") {
                if (brace_depth > 0) {
                    --brace_depth;
                }
                continue;
            }
            if (depth == 1 && brace_depth == 0) {
                if (c == ",") {
                    has_delim = True;
                } else if (c == "=") {
                    if ((i + 1) < len && (input[i + 1] == "=" || input[i + 1] == "~")) {
                        continue;
                    }
                    if (i > 0 && (input[i - 1] == "!" || input[i - 1] == "="
                        || input[i - 1] == "<" || input[i - 1] == ">")) {
                        continue;
                    }
                    has_delim = True;
                }
            }
        }
        return False;
    }

    #! Scans a list literal (parse_to_qore_value format)
    private:internal hash<DpqlTokenInfo> scanListLiteral() {
        int start_line = line;
        int start_column = column;
        string value = "";
        int depth = 0;
        int brace_depth = 0;
        bool in_single = False;
        bool in_double = False;
        bool escape = False;

        while (pos < len) {
            string c = currentChar();
            value += c;
            advance();

            if (escape) {
                escape = False;
                continue;
            }
            if (in_single) {
                if (c == "\\") {
                    escape = True;
                } else if (c == "'") {
                    in_single = False;
                }
                continue;
            }
            if (in_double) {
                if (c == "\\") {
                    escape = True;
                } else if (c == '"') {
                    in_double = False;
                }
                continue;
            }

            if (c == "'") {
                in_single = True;
                continue;
            }
            if (c == '"') {
                in_double = True;
                continue;
            }
            if (c == "(") {
                ++depth;
                continue;
            }
            if (c == ")") {
                --depth;
                if (depth == 0) {
                    return makeToken(DPQL_TOK_LIST, value, start_line, start_column);
                }
                continue;
            }
            if (c == "{") {
                ++brace_depth;
                continue;
            }
            if (c == "}") {
                if (brace_depth > 0) {
                    --brace_depth;
                }
                continue;
            }
        }

        addError("Unterminated list literal", "DPQL-E013", start_line, start_column, line, column);
        return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
    }

    #! Scans a hash literal (parse_to_qore_value format)
    private:internal hash<DpqlTokenInfo> scanHashLiteral() {
        int start_line = line;
        int start_column = column;
        string value = "";
        int depth = 0;
        int paren_depth = 0;
        bool in_single = False;
        bool in_double = False;
        bool escape = False;

        while (pos < len) {
            string c = currentChar();
            value += c;
            advance();

            if (escape) {
                escape = False;
                continue;
            }
            if (in_single) {
                if (c == "\\") {
                    escape = True;
                } else if (c == "'") {
                    in_single = False;
                }
                continue;
            }
            if (in_double) {
                if (c == "\\") {
                    escape = True;
                } else if (c == '"') {
                    in_double = False;
                }
                continue;
            }

            if (c == "'") {
                in_single = True;
                continue;
            }
            if (c == '"') {
                in_double = True;
                continue;
            }
            if (c == "{") {
                ++depth;
                continue;
            }
            if (c == "}") {
                --depth;
                if (depth == 0) {
                    return makeToken(DPQL_TOK_HASH, value, start_line, start_column);
                }
                continue;
            }
            if (c == "(") {
                ++paren_depth;
                continue;
            }
            if (c == ")") {
                if (paren_depth > 0) {
                    --paren_depth;
                }
                continue;
            }
        }

        addError("Unterminated hash literal", "DPQL-E013", start_line, start_column, line, column);
        return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
    }

    #! Returns True if the next token is a date literal
    private bool looksLikeDateLiteral() {
        if (pos + 9 >= len) {
            return False;
        }
        if (input[pos] !~ /[0-9]/ || input[pos + 1] !~ /[0-9]/ || input[pos + 2] !~ /[0-9]/
            || input[pos + 3] !~ /[0-9]/) {
            return False;
        }
        if (input[pos + 4] != "-" || input[pos + 7] != "-") {
            return False;
        }
        if (input[pos + 5] !~ /[0-9]/ || input[pos + 6] !~ /[0-9]/ || input[pos + 8] !~ /[0-9]/
            || input[pos + 9] !~ /[0-9]/) {
            return False;
        }
        return True;
    }

    #! Scans a date literal in ISO-8601 format
    private:internal hash<DpqlTokenInfo> scanDateLiteral() {
        int start_line = line;
        int start_column = column;
        string value = "";

        if (!readDigits(4, \value) || !expectChar("-", \value) || !readDigits(2, \value) || !expectChar("-", \value)
            || !readDigits(2, \value)) {
            addError("Invalid date literal", "DPQL-E011", start_line, start_column, line, column);
            return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
        }

        if (pos < len && (currentChar() == "T" || currentChar() == "t"
            || (currentChar() == " " && peekChar() =~ /[0-9]/))) {
            value += currentChar();
            advance();
            if (!readDigits(2, \value) || !expectChar(":", \value) || !readDigits(2, \value)) {
                addError("Invalid time literal in date", "DPQL-E011", start_line, start_column, line, column);
                return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
            }
            if (pos < len && currentChar() == ":") {
                value += ":";
                advance();
                if (!readDigits(2, \value)) {
                    addError("Invalid seconds in date literal", "DPQL-E011", start_line, start_column, line, column);
                    return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
                }
            }
            if (pos < len && currentChar() == ".") {
                value += ".";
                advance();
                if (!readDigits(1, \value, True)) {
                    addError("Invalid fractional seconds in date literal", "DPQL-E011", start_line, start_column,
                        line, column);
                    return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
                }
            }
            if (pos < len && (currentChar() == "Z" || currentChar() == "z")) {
                value += currentChar();
                advance();
            } else if (pos < len && (currentChar() == "+" || currentChar() == "-")) {
                value += currentChar();
                advance();
                if (!readDigits(2, \value) || !expectChar(":", \value) || !readDigits(2, \value)) {
                    addError("Invalid timezone in date literal", "DPQL-E011", start_line, start_column, line, column);
                    return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
                }
            }
        }

        return makeToken(DPQL_TOK_DATE, value, start_line, start_column);
    }

    #! Returns True if the next token is a binary literal
    private bool looksLikeBinaryLiteral() {
        if (pos + 2 >= len || input[pos] != "<") {
            return False;
        }
        if (input[pos + 1] !~ /[0-9A-Fa-f]/) {
            return False;
        }
        for (int i = pos + 1; i < len; ++i) {
            if (input[i] == ">") {
                return True;
            }
            if (input[i] !~ /[0-9A-Fa-f]/) {
                return False;
            }
        }
        return False;
    }

    #! Scans a binary literal (<tt>&lt;hex&gt;</tt>)
    private:internal hash<DpqlTokenInfo> scanBinaryLiteral() {
        int start_line = line;
        int start_column = column;
        string value = "<";
        advance();

        int hex_len = 0;
        while (pos < len) {
            string c = currentChar();
            if (c == ">") {
                value += c;
                advance();
                if (hex_len == 0 || (hex_len % 2) != 0) {
                    addError("Invalid binary literal length", "DPQL-E012", start_line, start_column, line, column);
                    return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
                }
                return makeToken(DPQL_TOK_BINARY, value, start_line, start_column);
            }
            if (c !~ /[0-9A-Fa-f]/) {
                addError("Invalid binary literal", "DPQL-E012", start_line, start_column, line, column);
                return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
            }
            value += c;
            ++hex_len;
            advance();
        }

        addError("Unterminated binary literal", "DPQL-E012", start_line, start_column, line, column);
        return makeToken(DPQL_TOK_ERROR, value, start_line, start_column);
    }

    private bool readDigits(int count, reference<string> out, *bool allow_more) {
        int read = 0;
        while (pos < len && currentChar() =~ /[0-9]/ && (read < count || allow_more)) {
            out += currentChar();
            advance();
            ++read;
        }
        return read >= count;
    }

    private bool expectChar(string expected, reference<string> out) {
        if (pos >= len || currentChar() != expected) {
            return False;
        }
        out += expected;
        advance();
        return True;
    }
}

} # namespace DataProvider
