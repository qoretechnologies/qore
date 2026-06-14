# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLLexer.qc GraphQL lexer (tokenizer)

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
#! a single lexical token produced by @ref GraphQLLexer
public hashdecl GraphQLToken {
    #! the token kind; one of the \c GraphQLLexer \c TK_* constants
    string kind;

    #! the token value (the name, parsed numeric value, or decoded string); @ref NOTHING for punctuators
    auto value;

    #! the 1-based line number where the token starts
    int line;

    #! the 1-based column number where the token starts
    int column;
}

#! Tokenizes GraphQL source text into a list of @ref GraphQLToken "GraphQLToken" hashes
/** The lexer implements the lexical grammar of the GraphQL specification: names, integer and
    float values, single-line and block strings, and punctuators. Whitespace, line terminators,
    commas, the unicode BOM, and \c "#" comments are ignored (not emitted as tokens).

    @par Example:
    @code{.py}
list<hash<GraphQLToken>> tokens = GraphQLLexer::tokenize("{ hero { name } }");
    @endcode
*/
public class GraphQLLexer {
    public {
        #! a name token (identifier or keyword), e.g. \c query or \c hero
        const TK_NAME = "Name";
        #! an integer value token
        const TK_INT = "Int";
        #! a float value token
        const TK_FLOAT = "Float";
        #! a string value token (single-line or block)
        const TK_STRING = "String";
        #! the \c "!" punctuator
        const TK_BANG = "!";
        #! the \c "$" punctuator
        const TK_DOLLAR = "\$";
        #! the \c "&" punctuator
        const TK_AMP = "&";
        #! the \c "(" punctuator
        const TK_PAREN_L = "(";
        #! the \c ")" punctuator
        const TK_PAREN_R = ")";
        #! the \c "..." punctuator (spread)
        const TK_SPREAD = "...";
        #! the \c ":" punctuator
        const TK_COLON = ":";
        #! the \c "=" punctuator
        const TK_EQUALS = "=";
        #! the \c "@" punctuator
        const TK_AT = "@";
        #! the \c "[" punctuator
        const TK_BRACKET_L = "[";
        #! the \c "]" punctuator
        const TK_BRACKET_R = "]";
        #! the \c "{" punctuator
        const TK_BRACE_L = "{";
        #! the \c "}" punctuator
        const TK_BRACE_R = "}";
        #! the \c "|" punctuator
        const TK_PIPE = "|";
        #! the end-of-input token
        const TK_EOF = "<EOF>";

        #! maps a single-character punctuator to its token kind
        const SingleCharPunctuators = {
            "!": TK_BANG,
            "\$": TK_DOLLAR,
            "&": TK_AMP,
            "(": TK_PAREN_L,
            ")": TK_PAREN_R,
            ":": TK_COLON,
            "=": TK_EQUALS,
            "@": TK_AT,
            "[": TK_BRACKET_L,
            "]": TK_BRACKET_R,
            "{": TK_BRACE_L,
            "}": TK_BRACE_R,
            "|": TK_PIPE,
        };
    }

    private {
        #! the source text being tokenized
        string src;
        #! the source length in characters
        int len;
        #! the current character offset
        int pos = 0;
        #! the current 1-based line number
        int line = 1;
        #! the character offset of the start of the current line (for column computation)
        int line_start = 0;
    }

    #! creates the lexer for the given source text
    /** @param source the GraphQL source text to tokenize
    */
    constructor(string source) {
        src = source;
        len = source.length();
    }

    #! tokenizes the given source text and returns the full list of tokens (including a trailing EOF)
    /** @param source the GraphQL source text to tokenize

        @return the list of tokens, always terminated by a single @ref TK_EOF token

        @throw GRAPHQL-SYNTAX-ERROR the source contains an invalid token
    */
    static list<hash<GraphQLToken>> tokenize(string source) {
        GraphQLLexer lexer(source);
        return lexer.run();
    }

    #! tokenizes the source text supplied to the constructor
    /** @return the list of tokens, always terminated by a single @ref TK_EOF token

        @throw GRAPHQL-SYNTAX-ERROR the source contains an invalid token
    */
    list<hash<GraphQLToken>> run() {
        list<hash<GraphQLToken>> tokens();
        while (True) {
            hash<GraphQLToken> tok = nextToken();
            push tokens, tok;
            if (tok.kind == TK_EOF) {
                break;
            }
        }
        return tokens;
    }

    #! returns the current 1-based column
    private int col() {
        return pos - line_start + 1;
    }

    #! advances past ignored tokens (whitespace, commas, BOM, comments)
    private skipIgnored() {
        while (pos < len) {
            string c = src[pos];
            switch (c) {
                case "\n":
                    ++pos;
                    ++line;
                    line_start = pos;
                    break;
                case "\r":
                    ++pos;
                    # treat \r\n as a single line terminator
                    if (pos < len && src[pos] == "\n") {
                        ++pos;
                    }
                    ++line;
                    line_start = pos;
                    break;
                case " ":
                case "\t":
                case ",":
                case "\x{feff}":
                    ++pos;
                    break;
                case "#":
                    # comment: skip to end of line
                    ++pos;
                    while (pos < len && src[pos] != "\n" && src[pos] != "\r") {
                        ++pos;
                    }
                    break;
                default:
                    return;
            }
        }
    }

    #! reads and returns the next token
    private hash<GraphQLToken> nextToken() {
        skipIgnored();
        if (pos >= len) {
            return <GraphQLToken>{"kind": TK_EOF, "line": line, "column": col()};
        }
        int start_line = line;
        int start_col = col();
        string c = src[pos];

        # spread "..."
        if (c == "." ) {
            if (pos + 2 < len && src[pos + 1] == "." && src[pos + 2] == ".") {
                pos += 3;
                return <GraphQLToken>{"kind": TK_SPREAD, "line": start_line, "column": start_col};
            }
            throwSyntax(sprintf("unexpected character %y (expected \"...\")", c), start_line, start_col);
        }

        # single-character punctuators
        *string punct = SingleCharPunctuators{c};
        if (exists punct) {
            ++pos;
            return <GraphQLToken>{"kind": punct, "line": start_line, "column": start_col};
        }

        # names
        if (isNameStart(c)) {
            return readName(start_line, start_col);
        }

        # numbers
        if (c == "-" || isDigit(c)) {
            return readNumber(start_line, start_col);
        }

        # strings
        if (c == "\"") {
            return readString(start_line, start_col);
        }

        throwSyntax(sprintf("unexpected character %y", c), start_line, start_col);
    }

    #! returns @ref True if the character can start a name
    private static bool isNameStart(string c) {
        return c == "_" || (c >= "A" && c <= "Z") || (c >= "a" && c <= "z");
    }

    #! returns @ref True if the character can continue a name
    private static bool isNameContinue(string c) {
        return isNameStart(c) || isDigit(c);
    }

    #! returns @ref True if the character is a decimal digit
    private static bool isDigit(string c) {
        return c >= "0" && c <= "9";
    }

    #! reads a name token
    private hash<GraphQLToken> readName(int start_line, int start_col) {
        int start = pos;
        while (pos < len && isNameContinue(src[pos])) {
            ++pos;
        }
        return <GraphQLToken>{
            "kind": TK_NAME,
            "value": src.substr(start, pos - start),
            "line": start_line,
            "column": start_col,
        };
    }

    #! reads an integer or float token
    private hash<GraphQLToken> readNumber(int start_line, int start_col) {
        int start = pos;
        bool is_float = False;

        if (src[pos] == "-") {
            ++pos;
        }
        # integer part
        if (pos < len && src[pos] == "0") {
            ++pos;
        } else if (pos < len && isDigit(src[pos])) {
            while (pos < len && isDigit(src[pos])) {
                ++pos;
            }
        } else {
            throwSyntax("invalid number: missing integer part", start_line, start_col);
        }
        # fractional part
        if (pos < len && src[pos] == ".") {
            is_float = True;
            ++pos;
            if (!(pos < len && isDigit(src[pos]))) {
                throwSyntax("invalid number: missing digits after decimal point", start_line, start_col);
            }
            while (pos < len && isDigit(src[pos])) {
                ++pos;
            }
        }
        # exponent part
        if (pos < len && (src[pos] == "e" || src[pos] == "E")) {
            is_float = True;
            ++pos;
            if (pos < len && (src[pos] == "+" || src[pos] == "-")) {
                ++pos;
            }
            if (!(pos < len && isDigit(src[pos]))) {
                throwSyntax("invalid number: missing digits in exponent", start_line, start_col);
            }
            while (pos < len && isDigit(src[pos])) {
                ++pos;
            }
        }
        # a number may not be immediately followed by a name char or "."
        if (pos < len && (isNameStart(src[pos]) || src[pos] == ".")) {
            throwSyntax(sprintf("invalid number: unexpected character %y", src[pos]), start_line, start_col);
        }

        string text = src.substr(start, pos - start);
        return <GraphQLToken>{
            "kind": is_float ? TK_FLOAT : TK_INT,
            "value": is_float ? text.toFloat() : text.toInt(),
            "line": start_line,
            "column": start_col,
        };
    }

    #! reads a single-line or block string token
    private hash<GraphQLToken> readString(int start_line, int start_col) {
        # block string: """
        if (pos + 2 < len && src[pos + 1] == "\"" && src[pos + 2] == "\"") {
            return readBlockString(start_line, start_col);
        }
        # single-line string
        ++pos;
        string value = "";
        while (pos < len) {
            string c = src[pos];
            if (c == "\"") {
                ++pos;
                return <GraphQLToken>{
                    "kind": TK_STRING,
                    "value": value,
                    "line": start_line,
                    "column": start_col,
                };
            }
            if (c == "\n" || c == "\r") {
                throwSyntax("unterminated string", start_line, start_col);
            }
            if (c == "\\") {
                ++pos;
                if (pos >= len) {
                    throwSyntax("unterminated string escape", start_line, start_col);
                }
                value += readEscape(start_line, start_col);
            } else {
                value += c;
                ++pos;
            }
        }
        throwSyntax("unterminated string", start_line, start_col);
    }

    #! reads a string escape sequence (the backslash has already been consumed)
    private string readEscape(int start_line, int start_col) {
        string e = src[pos];
        ++pos;
        switch (e) {
            case "\"": return "\"";
            case "\\": return "\\";
            case "/": return "/";
            case "b": return "\b";
            case "f": return "\f";
            case "n": return "\n";
            case "r": return "\r";
            case "t": return "\t";
            case "u": {
                if (pos + 4 > len) {
                    throwSyntax("invalid unicode escape", start_line, start_col);
                }
                string hex = src.substr(pos, 4);
                if (hex !~ /^[0-9A-Fa-f]{4}$/) {
                    throwSyntax(sprintf("invalid unicode escape \\u%s", hex), start_line, start_col);
                }
                pos += 4;
                int code = hex.toInt(16);
                return chr(code);
            }
            default:
                throwSyntax(sprintf("invalid escape sequence \\%s", e), start_line, start_col);
        }
    }

    #! reads a block string token (\c \"\"\" delimited) and applies block-string dedent
    private hash<GraphQLToken> readBlockString(int start_line, int start_col) {
        pos += 3;
        string raw = "";
        while (pos < len) {
            # end delimiter
            if (src[pos] == "\"" && pos + 2 < len && src[pos + 1] == "\"" && src[pos + 2] == "\"") {
                pos += 3;
                return <GraphQLToken>{
                    "kind": TK_STRING,
                    "value": dedentBlockString(raw),
                    "line": start_line,
                    "column": start_col,
                };
            }
            # escaped triple-quote
            if (src[pos] == "\\" && pos + 3 < len && src[pos + 1] == "\"" && src[pos + 2] == "\""
                    && src[pos + 3] == "\"") {
                raw += "\"\"\"";
                pos += 4;
                continue;
            }
            if (src[pos] == "\n") {
                ++line;
                line_start = pos + 1;
            } else if (src[pos] == "\r") {
                ++line;
                line_start = pos + 1;
            }
            raw += src[pos];
            ++pos;
        }
        throwSyntax("unterminated block string", start_line, start_col);
    }

    #! applies the GraphQL block-string formatting (common-indent removal and blank-line trimming)
    static string dedentBlockString(string raw) {
        # normalize line terminators and split into lines
        raw =~ s/\r\n/\n/g;
        raw =~ s/\r/\n/g;
        list<string> lines = raw.split("\n");

        # determine common indentation of all lines after the first that contain non-whitespace
        *int common_indent;
        for (int i = 1; i < lines.size(); ++i) {
            string l = lines[i];
            int indent = leadingWhitespace(l);
            if (indent < l.length()) {
                # line contains non-whitespace
                if (!exists common_indent || indent < common_indent) {
                    common_indent = indent;
                }
            }
        }
        if (exists common_indent && common_indent > 0) {
            for (int i = 1; i < lines.size(); ++i) {
                lines[i] = lines[i].length() >= common_indent ? lines[i].substr(common_indent) : "";
            }
        }
        # remove leading and trailing blank lines
        while (lines && isBlank(lines[0])) {
            shift lines;
        }
        while (lines && isBlank(lines[lines.size() - 1])) {
            pop lines;
        }
        return lines.join("\n");
    }

    #! returns the number of leading whitespace characters (spaces and tabs) in the string
    private static int leadingWhitespace(string l) {
        int i = 0;
        while (i < l.length() && (l[i] == " " || l[i] == "\t")) {
            ++i;
        }
        return i;
    }

    #! returns @ref True if the string is empty or contains only whitespace
    private static bool isBlank(string l) {
        return leadingWhitespace(l) == l.length();
    }

    #! throws a @ref GRAPHQL_SYNTAX_ERROR exception with position information
    private throwSyntax(string msg, int l, int c) {
        throw GRAPHQL_SYNTAX_ERROR, sprintf("syntax error at line %d, column %d: %s", l, c, msg),
            {"line": l, "column": c};
    }
}
}
