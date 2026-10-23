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

#! Prefix-operator argument counts for DPQL operators that evaluate arguments in a controlled context
const hash<auto> DpqlPrefixOperatorSpecs = {
    DP_OP_MAP: {"min": 2, "max": 3},
    DP_OP_HASH_MAP: {"min": 3, "max": 4},
    DP_OP_SELECT: {"min": 2, "max": 2},
    DP_OP_FIRST: {"min": 1, "max": 2},
    DP_OP_ANY: {"min": 1, "max": 2},
    DP_OP_ALL: {"min": 2, "max": 2},
    DP_OP_COUNT: {"min": 1, "max": 2},
    DP_OP_TAKE: {"min": 2, "max": 2},
    DP_OP_DROP: {"min": 2, "max": 2},
    DP_OP_TAKEWHILE: {"min": 2, "max": 2},
    DP_OP_TAKEUNTIL: {"min": 2, "max": 2},
};

#! The number of days in each month of a year that is not a leap year, as date diagnostics say them
const list<int> DpqlMonthDays = (31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);

#! The parts of a date literal that date diagnostics check: the year, month and day, and the hour, minutes and seconds
const string DpqlDateLiteralParts = "^([0-9]{4})-([0-9]{2})-([0-9]{2})"
    "(?:[Tt ]([0-9]{2}):([0-9]{2})(?::([0-9]{2}))?)?";

#! The names of the months, as date diagnostics say them
const list<string> DpqlMonthNames = ("January", "February", "March", "April", "May", "June", "July", "August",
    "September", "October", "November", "December");

#! What a diagnostic says of \c exists without a field after it
const DpqlExistsNeedsFieldMessage = "'exists' needs a field after it, such as exists @name.";

#! What a diagnostic says of a \c . without a key after it
const DpqlDotNeedsKeyMessage = "A '.' must be followed by the name of a key: pick one from the list, or remove the "
    "'.'.";

#! What a diagnostic says of the list wildcard written with something else
const DpqlListWildcardAloneMessage = "[*] stands for every item, so it is written alone: write [*], with nothing else "
    "between the brackets.";

#! What a diagnostic says of the hash wildcard written with something else
const DpqlHashWildcardAloneMessage = "{*} stands for every value, so it is written alone: write {*}, with nothing "
    "else between the braces.";

#! Parentheses that hold the arguments of a function call
const DpqlParenCall = "call";
#! Parentheses that hold the values after \c in
const DpqlParenList = "list";
#! Parentheses that group an expression
const DpqlParenGroup = "group";
#! Parentheses that hold the field of \c exists
const DpqlParenExists = "exists";

#! An example of each prefix expression operator, as diagnostics show them
/** @since DataProvider 3.11
*/
const hash<string, string> DpqlPrefixOperatorExamples = {
    DP_OP_MAP: "map @name, @users",
    DP_OP_HASH_MAP: "hash_map @name, @age, @users",
    DP_OP_SELECT: "select @users, @age >= 30",
    DP_OP_FIRST: "first @age >= 30, @users",
    DP_OP_ANY: "any @active, @users",
    DP_OP_ALL: "all @active, @users",
    DP_OP_COUNT: "count @users",
    DP_OP_TAKE: "take 2, @users",
    DP_OP_DROP: "drop 1, @users",
    DP_OP_TAKEWHILE: "takewhile @age < 35, @users",
    DP_OP_TAKEUNTIL: "takeuntil @age >= 35, @users",
};

#! The escapes of a quoted field name, as listed in messages
/** @since DataProvider 3.11
*/
public const DPQL_FIELD_NAME_ESCAPES = "\\\", \\\\, \\., \\n, \\r and \\t";

#! The tokens that start a value and that cannot follow a value
/** An identifier can follow a value as an operator word (\c in, \c between, \c and, \c like)

    @since DataProvider 3.11
*/
const DpqlValueStartTokens = {
    DPQL_TOK_STRING: True,
    DPQL_TOK_NUMBER: True,
    DPQL_TOK_BOOLEAN: True,
    DPQL_TOK_NULL: True,
    DPQL_TOK_DATE: True,
    DPQL_TOK_BINARY: True,
    DPQL_TOK_LIST: True,
    DPQL_TOK_HASH: True,
    DPQL_TOK_TEMPLATE: True,
};

#! The wildcard accessor expressions, which are written as accessors and not as function calls
/** @since DataProvider 3.10
*/
const hash<string, string> DpqlWildcardAccessors = {
    DP_FUNC_LIST_WILDCARD: "[*]",
    DP_FUNC_HASH_WILDCARD: "{*}",
    DP_FUNC_WILDCARD_ITEM: "",
};

#! The result of comparing a value with itself, by comparison operator
const hash<string, bool> DpqlSelfComparisonResults = {
    DP_SEARCH_OP_EQ: True,
    DP_SEARCH_OP_LE: True,
    DP_SEARCH_OP_GE: True,
    DP_SEARCH_OP_NE: False,
    DP_SEARCH_OP_LT: False,
    DP_SEARCH_OP_GT: False,
};

#! The operators whose result depends only on their arguments, so that identical arguments give the same result
const hash<string, bool> DpqlDeterministicOperators = {
    DP_EXPR_VALUE: True,
    DP_OP_PLUS: True,
    DP_OP_MINUS: True,
    DP_OP_MULT: True,
    DP_OP_DIV: True,
    DP_OP_MOD: True,
};

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
        #! The opening parenthesis directly following a prefix operator name
        /** A group there is the operator's first argument, unless it holds a comma-separated list, which is
            function-call syntax and is reported as such

            @since %DataProvider 3.9
        */
        *hash<DpqlTokenInfo> prefix_op_paren_tok;
        #! The prefix operator name token preceding \c prefix_op_paren_tok
        /** @since %DataProvider 3.9
        */
        *hash<DpqlTokenInfo> prefix_op_name_tok;
        #! The last token read that is not a space or a line break, which a diagnostic of a missing value names
        *hash<DpqlTokenInfo> last_tok;
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

        Keys are stored lowercased: DPQL surface symbols are case-insensitive like the
        \c BETWEEN / \c IN keywords, and the infix-operator path looks up the lowercased
        token — camelCase symbols (e.g. \c startsWith) could never match case-sensitively
        (issue #5348).

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
            symbol_to_name{sym.lwr()} = ename;
        }
    }

    #! Resolves a DPQL surface symbol to the internal expression name
    /** Symbol lookup is case-insensitive (lowercased keys — see buildSymbolToNameMap());
        the canonical-name fallback stays exact-case.
    */
    private string resolveExpressionName(string token_name) {
        *string ename = symbol_to_name{token_name.lwr()};
        if (ename) {
            return ename;
        }
        if (expressions{token_name}) {
            return token_name;
        }
        return token_name;
    }

    #! Parses a DPQL expression string
    /** @param text the DPQL expression to parse
        @return the parsed DataProviderExpression

        @throw DPQL-PARSE-ERROR if the expression contains syntax errors; the description gives the position and the
        message of the first diagnostic (<tt>DPQL parse error at line 1, column 4: ...</tt>), and the argument is the
        list of diagnostics

        @since %DataProvider 3.11 the description gives the position of the first diagnostic before its message, as a
        message is a sentence that says how to make the text valid
    */
    hash<DataProviderExpression> parse(string text) {
        hash<DpqlParseResult> result = parseWithInfo(text);
        if (!result.success) {
            string msg = "DPQL parse error";
            if (result.diagnostics) {
                msg += sprintf(" at line %d, column %d: %s", result.diagnostics[0].line, result.diagnostics[0].column,
                    result.diagnostics[0].message);
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
        remove prefix_op_paren_tok;
        remove prefix_op_name_tok;
        remove last_tok;

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
                    # a token that the tokenizer could not read has been reported by it
                    if (tok.type != DPQL_TOK_ERROR) {
                        addError(getTrailingTokenMessage(tok), "DPQL-E001", tok);
                    }
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

    #! Validates search expression rules (only applied in search context)
    /** Enforces rules about field references and server-supported vs local-only operators
        in search expressions (where_cond for find/update/delete).

        These rules are ONLY applied when \a server_expressions is provided (search context).
        In non-search contexts (e.g., value mapping, transformation pipelines), any expression
        is valid without restriction.

        @param exp the parsed expression to validate
        @param server_expressions the provider-specific expressions (server-supported operators)
        @return a list of diagnostic messages for search expression rule violations

        @since DataProvider 3.4
    */
    list<hash<DpqlDiagnostic>> validateSearchExpression(hash<DataProviderExpression> exp,
            hash<string, hash<DataProviderExpressionInfo>> server_expressions) {
        list<hash<DpqlDiagnostic>> diags = ();

        # DPQL-E040: Check that at least one top-level condition references a field
        if (!expressionHasFieldRef(exp)) {
            diags += <DpqlDiagnostic>{
                "severity": "error",
                "message": "A search condition must compare at least one field of the records, for example "
                    "@status == \"open\".",
                "code": "DPQL-E040",
                "line": 1,
                "column": 1,
                "end_line": 1,
                "end_column": 1,
            };
        }

        # Walk the expression tree checking server-supported vs local-only operator rules
        validateSearchExprRecursive(exp, server_expressions, \diags, True);

        return diags;
    }

    #! Recursively validates search expression rules
    /** @param exp the expression to validate
        @param server_expressions server-supported expressions
        @param diags output diagnostics list
        @param is_top_level True if this expression is at the top level or inside &&/||
    */
    private validateSearchExprRecursive(hash<DataProviderExpression> exp,
            hash<string, hash<DataProviderExpressionInfo>> server_expressions,
            reference<list<hash<DpqlDiagnostic>>> diags, bool is_top_level) {
        # &&/|| are structural — recurse into children at the same level
        if (exp.exp == DP_OP_AND || exp.exp == DP_OP_OR) {
            # DPQL-E043: Check if the provider supports AND/OR at top level
            if (is_top_level && !server_expressions{exp.exp}) {
                string op_name = exp.exp == DP_OP_AND ? "&&" : "||";
                diags += <DpqlDiagnostic>{
                    "severity": "error",
                    "message": sprintf("This data provider cannot search with %s at the top level. Join the "
                        "conditions with operators it supports, or search in separate steps.", op_name),
                    "code": "DPQL-E043",
                    "line": 1,
                    "column": 1,
                    "end_line": 1,
                    "end_column": 1,
                };
            }
            foreach auto arg in (exp.args) {
                if (arg instanceof hash<DataProviderExpression>) {
                    validateSearchExprRecursive(arg, server_expressions, \diags, is_top_level);
                }
            }
            return;
        }

        # NOT operator
        if (exp.exp == DP_SEARCH_OP_NOT) {
            bool is_server = server_expressions.hasKey(exp.exp);
            if (!is_server) {
                # Local-only NOT — check for field refs inside
                checkNoFieldRefsInLocalOp(exp, exp.exp, server_expressions, \diags);
            } else {
                # Server NOT — recurse normally
                foreach auto arg in (exp.args) {
                    if (arg instanceof hash<DataProviderExpression>) {
                        validateSearchExprRecursive(arg, server_expressions, \diags, False);
                    }
                }
            }
            return;
        }

        # For all other expressions: check server-supported vs local-only rules
        bool is_server_op = server_expressions.hasKey(exp.exp);

        if (is_server_op) {
            # DPQL-E044: Server operator — at least one arg must be a field reference
            if (exp.args.size() > 0 && isComparisonOp(exp.exp)) {
                bool has_field_ref = False;
                foreach auto arg in (exp.args) {
                    if (arg instanceof hash<DataProviderFieldReference>) {
                        has_field_ref = True;
                        break;
                    }
                }
                if (!has_field_ref) {
                    string op_sym = getOpSymbol(exp.exp);
                    diags += <DpqlDiagnostic>{
                        "severity": "error",
                        "message": sprintf("Server operator '%s' requires a record field reference "
                            "(e.g., `@field_name %s value`). The server needs to know which "
                            "field to compare.", op_sym, op_sym),
                        "code": "DPQL-E044",
                        "line": 1,
                        "column": 1,
                        "end_line": 1,
                        "end_column": 1,
                    };
                }
            }
            # Recurse into nested expression args (e.g., value-side expressions)
            foreach auto arg in (exp.args) {
                if (arg instanceof hash<DataProviderExpression>) {
                    validateSearchExprRecursive(arg, server_expressions, \diags, False);
                }
            }
        } else {
            # Local-only operator
            # DPQL-E041: local-only op with field ref
            if (expressionHasFieldRef(exp)) {
                # Find the first field ref for the error message
                *string field_name = findFirstFieldRef(exp);
                string op_sym = getOpSymbol(exp.exp);
                # Build list of server operator symbols for guidance
                list<string> server_ops = ();
                foreach string sname in (keys server_expressions) {
                    if (isComparisonOp(sname)) {
                        server_ops += getOpSymbol(sname);
                    }
                }
                string server_list = server_ops ? server_ops.join(", ") : "(none available)";
                diags += <DpqlDiagnostic>{
                    "severity": "error",
                    "message": sprintf("Operator '%s' is evaluated locally, not on the data provider "
                        "server. Field references like '@%s' can only be used with server-supported "
                        "operators (%s). Use a server operator, or remove the field reference.",
                        op_sym, field_name ?? "field", server_list),
                    "code": "DPQL-E041",
                    "line": 1,
                    "column": 1,
                    "end_line": 1,
                    "end_column": 1,
                };
            }
        }
    }

    #! Checks that a local-only operator subtree contains no field references
    private checkNoFieldRefsInLocalOp(hash<DataProviderExpression> exp, string parent_op,
            hash<string, hash<DataProviderExpressionInfo>> server_expressions,
            reference<list<hash<DpqlDiagnostic>>> diags) {
        foreach auto arg in (exp.args) {
            if (arg instanceof hash<DataProviderFieldReference>) {
                string op_sym = getOpSymbol(parent_op);
                diags += <DpqlDiagnostic>{
                    "severity": "error",
                    "message": sprintf("@%s cannot be used inside %s: this data provider compares fields only "
                        "at the top level. Move the comparison of @%s out of %s.", arg.field, op_sym, arg.field,
                        op_sym),
                    "code": "DPQL-E042",
                    "line": 1,
                    "column": 1,
                    "end_line": 1,
                    "end_column": 1,
                };
            } else if (arg instanceof hash<DataProviderExpression>) {
                checkNoFieldRefsInLocalOp(arg, parent_op, server_expressions, \diags);
            }
        }
    }

    #! Returns True if the expression tree contains any field references
    private bool expressionHasFieldRef(auto node) {
        if (node instanceof hash<DataProviderFieldReference>) {
            return True;
        }
        if (node instanceof hash<DataProviderExpression>) {
            return any expressionHasFieldRef($1), node.args;
        }
        return False;
    }

    #! Finds the first field reference name in an expression tree
    private *string findFirstFieldRef(auto node) {
        if (node instanceof hash<DataProviderFieldReference>) {
            return node.field;
        }
        if (node instanceof hash<DataProviderExpression>) {
            foreach auto arg in (node.args) {
                *string name = findFirstFieldRef(arg);
                if (name) {
                    return name;
                }
            }
        }
        return NOTHING;
    }

    #! Returns True if the expression name is a comparison operator
    private bool isComparisonOp(string exp_name) {
        switch (exp_name) {
            case DP_SEARCH_OP_EQ:
            case DP_SEARCH_OP_NE:
            case DP_SEARCH_OP_LT:
            case DP_SEARCH_OP_LE:
            case DP_SEARCH_OP_GT:
            case DP_SEARCH_OP_GE:
            case DP_SEARCH_OP_BETWEEN:
            case DP_SEARCH_OP_IN:
            case DP_SEARCH_OP_REGEX:
            case DP_OP_REGEX_MATCH:
            case DP_OP_REGEX_NMATCH:
                return True;
        }
        # Check for provider-specific comparison operators (e.g. "like")
        if (expressions{exp_name} && inlist("Comparison", expressions{exp_name}.groups)) {
            return True;
        }
        return False;
    }

    #! Returns True if the expression uses DPQL prefix-operator syntax
    private bool isPrefixExpressionOperator(string exp_name) {
        return DpqlPrefixOperatorSpecs{exp_name}.val();
    }

    #! Returns the DPQL symbol for an operator name
    private string getOpSymbol(string exp_name) {
        # For built-in ops, the name IS the symbol
        if (exp_name =~ /^[^a-zA-Z]/ || exp_name == "between" || exp_name == "in"
                || exp_name == "regex") {
            return exp_name;
        }
        # Check expression map for a symbol
        if (expressions{exp_name} && expressions{exp_name}.symbol) {
            string sym = expressions{exp_name}.symbol;
            if (sym =~ /\(\)$/) {
                sym = substr(sym, 0, sym.size() - 2);
            }
            return sym;
        }
        return exp_name;
    }

    #! Returns True if an argument of an expression is evaluated for each item of a source, not on the record
    /** The field references in such an argument refer to the fields of each item of the source, not to the fields
        of the record that the expression is evaluated on, so a walker that collects or validates the fields of the
        record must not descend into it (DPQL validation does not).  The arguments evaluated for each item are:
        - \c map (<tt>map body, source[, filter]</tt>): the body and the filter
        - \c hash_map (<tt>hash_map key, value, source[, filter]</tt>): the key, the value and the filter
        - \c select (<tt>select source, predicate</tt>): the predicate
        - \c first, \c any and \c count with two arguments (<tt>first predicate, source</tt>): the predicate; with
          one argument, it is the source
        - \c all, \c takewhile and \c takeuntil (<tt>all predicate, source</tt>): the predicate
        - the wildcard accessors @ref DP_FUNC_LIST_WILDCARD and @ref DP_FUNC_HASH_WILDCARD
          (<tt>\@orders[*].id</tt>, <tt>\@prices{*}.amount</tt>): the expression after the wildcard, whose
          accessors on the item itself start from @ref DP_FUNC_WILDCARD_ITEM, which has no arguments

        Every other argument, including the source of each of these and the count of \c take and \c drop, is
        evaluated on the record.

        @param exp the expression
        @param pos the position of the argument (0-based)

        @return True if the argument at \a pos is evaluated for each item of a source, so that its field references
        are fields of the item; False for any other argument and for a position that is not an argument

        @par Example:
        @code{.py}
# map @id, @orders, @active: the body (0) and the filter (2) refer to the fields of each order
hash<DataProviderExpression> exp = DataProvider::parseDpqlExpression("map @id, @orders, @active");
# (True, False, True)
list<bool> scoped = map DpqlParser::isItemScopedArg(exp, $#), exp.args;

# @orders[*].id: the expression after [*] (1) refers to the fields of each order; the source (0) does not
exp = DataProvider::parseDpqlExpression("@orders[*].id");
# (False, True)
scoped = map DpqlParser::isItemScopedArg(exp, $#), exp.args;
        @endcode

        @since DataProvider 3.10.1
    */
    static bool isItemScopedArg(hash<DataProviderExpression> exp, int pos) {
        if (pos < 0 || pos >= exp.args.size()) {
            return False;
        }
        switch (exp.exp) {
            case DP_OP_MAP:
                return pos == 0 || pos == 2;
            case DP_OP_HASH_MAP:
                return pos != 2;
            case DP_OP_SELECT:
                return pos == 1;
            case DP_OP_FIRST:
            case DP_OP_ANY:
            case DP_OP_COUNT:
                # the one-argument form is the source
                return exp.args.size() == 2 && pos == 0;
            case DP_OP_ALL:
            case DP_OP_TAKEWHILE:
            case DP_OP_TAKEUNTIL:
                return pos == 0;
            case DP_FUNC_LIST_WILDCARD:
            case DP_FUNC_HASH_WILDCARD:
                return pos == 1;
        }
        return False;
    }

    #! Recursively validates an expression
    private validateExprRecursive(hash<DataProviderExpression> exp,
            hash<string, AbstractDataField> fields, reference<list<hash<DpqlDiagnostic>>> diags) {
        # the field of exists is looked for, not read: a field that the record type does not have is not an error
        if (exp.exp == DP_OP_EXISTS) {
            return;
        }
        # Validate each argument
        foreach auto arg in (exp.args) {
            # an argument evaluated for each item of a source refers to the fields of the item
            if (DpqlParser::isItemScopedArg(exp, $#)) {
                continue;
            }
            if (arg.typeCode() == NT_HASH) {
                if (arg.hasKey("field")) {
                    # Field reference - validate field exists
                    string field_name = arg.field;
                    # the first key of the path is a field of the record
                    string base_field = AbstractDataProvider::getFieldPath(field_name)[0];
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

        # Validate logical operator arguments (&&, ||, !): their operands are conditions
        if (exp.exp == DP_OP_AND || exp.exp == DP_OP_OR || exp.exp == DP_SEARCH_OP_NOT) {
            string op_name = exp.exp == DP_OP_AND ? "AND" : (exp.exp == DP_OP_OR ? "OR" : "NOT");
            for (int i = 0; i < exp.args.size(); ++i) {
                *AbstractDataProviderType arg_type = inferArgType(exp.args[i]);
                if (arg_type && !AbstractDataProvider::isConditionType(arg_type)) {
                    diags += <DpqlDiagnostic>{
                        "severity": "warning",
                        "message": sprintf("Part %d of the %s evaluates to %s, but each part of a condition must "
                            "evaluate to true or false. Compare the value instead, for example "
                            "@status == \"open\".", i + 1, op_name, getTypeSentenceForm(arg_type)),
                        "code": "DPQL-W004",
                        "line": 1,
                        "column": 1,
                        "end_line": 1,
                        "end_column": 1,
                    };
                }
            }
        }

        # Validate comparison operator cross-type compatibility
        if (exp.exp == DP_SEARCH_OP_EQ || exp.exp == DP_SEARCH_OP_NE
                || exp.exp == DP_SEARCH_OP_LT || exp.exp == DP_SEARCH_OP_LE
                || exp.exp == DP_SEARCH_OP_GT || exp.exp == DP_SEARCH_OP_GE) {
            if (exp.args.size() >= 2) {
                *AbstractDataProviderType lhs_type = inferArgType(exp.args[0]);
                *AbstractDataProviderType rhs_type = inferArgType(exp.args[1]);
                if (lhs_type && rhs_type
                        && lhs_type.getName() != "any" && rhs_type.getName() != "any"
                        && !lhs_type.isAssignableFrom(rhs_type)
                        && !rhs_type.isAssignableFrom(lhs_type)) {
                    string op_sym;
                    switch (exp.exp) {
                        case DP_SEARCH_OP_EQ: op_sym = "=="; break;
                        case DP_SEARCH_OP_NE: op_sym = "!="; break;
                        case DP_SEARCH_OP_LT: op_sym = "<"; break;
                        case DP_SEARCH_OP_LE: op_sym = "<="; break;
                        case DP_SEARCH_OP_GT: op_sym = ">"; break;
                        case DP_SEARCH_OP_GE: op_sym = ">="; break;
                    }
                    diags += <DpqlDiagnostic>{
                        "severity": "warning",
                        "message": sprintf("Type mismatch in comparison: left side is '%s', right side "
                            "is '%s' (operator '%s')", lhs_type.getName(), rhs_type.getName(), op_sym),
                        "code": "DPQL-W005",
                        "line": 1,
                        "column": 1,
                        "end_line": 1,
                        "end_column": 1,
                    };
                }
            }
        }

        # A comparison of a value with itself, or of two literal values, has the same result for every record
        if (DpqlSelfComparisonResults.hasKey(exp.exp) && exp.args.size() == 2) {
            checkConstantComparison(exp, \diags);
        }

        # Arithmetic operand types: an operand that the operator cannot use would be converted silently
        if (ExpressionTypes::ArithmeticAccepts{exp.exp}) {
            checkArithmeticOperands(exp, \diags);
        }

        # Validate between type compatibility (all 3 args should be compatible)
        if (exp.exp == DP_SEARCH_OP_BETWEEN && exp.args.size() >= 3) {
            *AbstractDataProviderType val_type = inferArgType(exp.args[0]);
            *AbstractDataProviderType low_type = inferArgType(exp.args[1]);
            *AbstractDataProviderType high_type = inferArgType(exp.args[2]);
            if (val_type && val_type.getName() != "any") {
                if (low_type && low_type.getName() != "any"
                        && !val_type.isAssignableFrom(low_type)
                        && !low_type.isAssignableFrom(val_type)) {
                    diags += <DpqlDiagnostic>{
                        "severity": "warning",
                        "message": sprintf("Type mismatch in 'between': value is '%s' but lower bound "
                            "is '%s'", val_type.getName(), low_type.getName()),
                        "code": "DPQL-W005",
                        "line": 1,
                        "column": 1,
                        "end_line": 1,
                        "end_column": 1,
                    };
                }
                if (high_type && high_type.getName() != "any"
                        && !val_type.isAssignableFrom(high_type)
                        && !high_type.isAssignableFrom(val_type)) {
                    diags += <DpqlDiagnostic>{
                        "severity": "warning",
                        "message": sprintf("Type mismatch in 'between': value is '%s' but upper bound "
                            "is '%s'", val_type.getName(), high_type.getName()),
                        "code": "DPQL-W005",
                        "line": 1,
                        "column": 1,
                        "end_line": 1,
                        "end_column": 1,
                    };
                }
            }
        }

        # Validate 'in' type compatibility (list elements vs first arg)
        if (exp.exp == DP_SEARCH_OP_IN && exp.args.size() >= 2) {
            *AbstractDataProviderType val_type = inferArgType(exp.args[0]);
            if (val_type && val_type.getName() != "any") {
                for (int i = 1; i < exp.args.size(); ++i) {
                    *AbstractDataProviderType elem_type = inferArgType(exp.args[i]);
                    if (elem_type && elem_type.getName() != "any"
                            && !val_type.isAssignableFrom(elem_type)
                            && !elem_type.isAssignableFrom(val_type)) {
                        diags += <DpqlDiagnostic>{
                            "severity": "warning",
                            "message": sprintf("Type mismatch in 'in': value is '%s' but element %d "
                                "is '%s'", val_type.getName(), i, elem_type.getName()),
                            "code": "DPQL-W005",
                            "line": 1,
                            "column": 1,
                            "end_line": 1,
                            "end_column": 1,
                        };
                    }
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
        if (tok.type != DPQL_TOK_WHITESPACE && tok.type != DPQL_TOK_NEWLINE && tok.type != DPQL_TOK_EOF) {
            last_tok = tok;
        }
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

    #! Returns what a diagnostic says of a token that follows a complete expression
    /** @param tok the token after the expression

        @return the message, which says how to make the text valid

        @since %DataProvider 3.11
    */
    private static string getTrailingTokenMessage(hash<DpqlTokenInfo> tok) {
        switch (tok.type) {
            case DPQL_TOK_RPAREN:
                return "This ')' has no '(' to close: remove it.";
            case DPQL_TOK_RBRACKET:
                return "This ']' has no '[' to close: remove it.";
            case DPQL_TOK_RBRACE:
                return "This '}' has no '{' to close: remove it.";
            case DPQL_TOK_COMMA:
                return "A ',' separates the values of a function or a list, and there's none here: remove it.";
            case DPQL_TOK_IDENTIFIER:
                switch (tok.value.lwr()) {
                    case "and":
                        return "Use && to join two conditions: 'and' only joins the two bounds of between.";
                    case "or":
                        return "Use || to join two conditions: 'or' isn't an operator.";
                    case "not":
                        return "'not' goes before in or between, as in @status not in (\"closed\", \"lost\").";
                }
                break;
        }
        return "Something follows a complete value here: join them with an operator, such as +, == or &&.";
    }

    #! Returns what a diagnostic says of a value that is missing at the end of the text
    /** @return the message, which names the last token read, after which the value is missing

        @since %DataProvider 3.11
    */
    private string getMissingValueMessage() {
        return last_tok
            ? sprintf("Something is missing at the end: a value after '%s'.", last_tok.value)
            : "Something is missing at the end: a value.";
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
    /** @param token_type the token type expected
        @param description the token expected, as a diagnostic names it
        @param end_message what a diagnostic says when the text ends where the token is expected; by default, that
        the token is missing at the end
        @param other_message what a diagnostic says when another token is where the token is expected; by default,
        that the token is missing before it

        @return the token
    */
    private hash<DpqlTokenInfo> expect(int token_type, string description, *string end_message,
            *string other_message) {
        skipWhitespace();
        if (atEnd()) {
            addError(end_message ?? sprintf("Something is missing at the end: %s.", description),
                "DPQL-E001", line, column, line, column);
            throw "DPQL-PARSE-ERROR", "Unexpected end of input";
        }
        hash<DpqlTokenInfo> tok = nextToken();
        if (tok.type != token_type) {
            # a token that the tokenizer could not read has been reported by it
            if (tok.type != DPQL_TOK_ERROR) {
                addError(other_message ?? sprintf("Something is missing before '%s': %s.", tok.value, description),
                    "DPQL-E001", tok);
            }
            throw "DPQL-PARSE-ERROR", sprintf("Expected %s", description);
        }
        return tok;
    }

    #! Expects the \c ')' that closes a \c '(' and returns it
    /** @param kind what the parentheses hold: one of the \c DpqlParen* constants, which say what a diagnostic says
        when something else is where the \c ')' is expected

        @return the token

        @since %DataProvider 3.11
    */
    private hash<DpqlTokenInfo> expectClosingParen(string kind) {
        skipWhitespace();
        if (atEnd()) {
            addError("Something is missing at the end: a ')' to close the '('.", "DPQL-E001", line, column, line,
                column);
            throw "DPQL-PARSE-ERROR", "Unexpected end of input";
        }
        hash<DpqlTokenInfo> tok = nextToken();
        if (tok.type == DPQL_TOK_RPAREN) {
            return tok;
        }
        # a token that the tokenizer could not read has been reported by it
        if (tok.type != DPQL_TOK_ERROR) {
            string msg;
            switch (kind) {
                case DpqlParenCall:
                case DpqlParenList:
                    msg = "Something follows a complete value here: separate the values with ',', or close the '(' "
                        "with ')'.";
                    break;
                case DpqlParenExists:
                    msg = "'exists' takes one field in its parentheses: close the '(' with ')' after the field.";
                    break;
                default:
                    msg = tok.type == DPQL_TOK_COMMA
                        ? "A ',' here separates the values of a list: close the list with ')' after its last value, "
                            "such as (1, 2)."
                        : "Something follows a complete value here: join them with an operator, or close the '(' "
                            "with ')'.";
            }
            addError(msg, "DPQL-E001", tok);
        }
        throw "DPQL-PARSE-ERROR", "Expected ')'";
    }

    #! Parses or_expr: and_expr { "||" and_expr }
    private auto parseOrExpr() {
        auto left = parseAndExpr();
        skipWhitespace();

        # Use list<auto!> to prevent runtime type narrowing — args can hold mixed types
        # (expressions, field references, bare values)
        list<auto!> args = (left,);
        while (!atEnd()) {
            hash<DpqlTokenInfo> tok = peek();
            if (tok.type != DPQL_TOK_OP_OR) {
                break;
            }
            nextToken();  # consume ||
            skipWhitespace();
            # push, so that a list literal operand stays one operand
            push args, parseAndExpr();
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

        # Use list<auto!> to prevent runtime type narrowing — args can hold mixed types
        # (expressions, field references, bare values)
        list<auto!> args = (left,);
        while (!atEnd()) {
            hash<DpqlTokenInfo> tok = peek();
            if (tok.type != DPQL_TOK_OP_AND) {
                break;
            }
            nextToken();  # consume &&
            skipWhitespace();
            # push, so that a list literal operand stays one operand
            push args, parseUnaryExpr();
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
            addError(getMissingValueMessage(), "DPQL-E001", line, column, line, column);
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
            addError(getMissingValueMessage(), "DPQL-E001", line, column, line, column);
            throw "DPQL-PARSE-ERROR", "Unexpected end of input";
        }

        # parseComparison handles all primary expression types via parseValue
        return parseComparison();
    }

    #! Parses additive_expr: multiplicative_expr { ("+" | "-") multiplicative_expr }
    private auto parseAdditiveExpr() {
        auto left = parseMultiplicativeExpr();
        skipWhitespace();

        while (!atEnd()) {
            hash<DpqlTokenInfo> tok = peek();
            string op;
            if (tok.type == DPQL_TOK_OP_PLUS) {
                op = DP_OP_PLUS;
            } else if (tok.type == DPQL_TOK_OP_MINUS) {
                op = DP_OP_MINUS;
            } else {
                break;
            }
            nextToken();
            skipWhitespace();
            auto right = parseMultiplicativeExpr();
            left = <DataProviderExpression>{
                "exp": op,
                "args": (left, right),
            };
            skipWhitespace();
        }
        return left;
    }

    #! Parses multiplicative_expr: value { ("*" | "/" | "%") value }
    private auto parseMultiplicativeExpr() {
        auto left = parseValue();
        skipWhitespace();

        while (!atEnd()) {
            hash<DpqlTokenInfo> tok = peek();
            string op;
            if (tok.type == DPQL_TOK_OP_MULTIPLY) {
                op = DP_OP_MULT;
            } else if (tok.type == DPQL_TOK_OP_DIVIDE) {
                op = DP_OP_DIV;
            } else if (tok.type == DPQL_TOK_OP_MOD) {
                op = DP_OP_MOD;
            } else {
                break;
            }
            nextToken();
            skipWhitespace();
            auto right = parseValue();
            left = <DataProviderExpression>{
                "exp": op,
                "args": (left, right),
            };
            skipWhitespace();
        }
        return left;
    }

    #! Parses a comparison expression
    private auto parseComparison() {
        auto left = parseAdditiveExpr();
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
                    "args": (left, parseAdditiveExpr()),
                };

            case DPQL_TOK_OP_NE:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_NE,
                    "args": (left, parseAdditiveExpr()),
                };

            case DPQL_TOK_OP_LT:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_LT,
                    "args": (left, parseAdditiveExpr()),
                };

            case DPQL_TOK_OP_LE:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_LE,
                    "args": (left, parseAdditiveExpr()),
                };

            case DPQL_TOK_OP_GT:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_GT,
                    "args": (left, parseAdditiveExpr()),
                };

            case DPQL_TOK_OP_GE:
                nextToken();
                skipWhitespace();
                return <DataProviderExpression>{
                    "exp": DP_SEARCH_OP_GE,
                    "args": (left, parseAdditiveExpr()),
                };

            case DPQL_TOK_OP_REGEX:
                nextToken();  # consume =~
                skipWhitespace();
                return parseRegexExpr(left, DP_OP_REGEX_MATCH, "=~");

            case DPQL_TOK_OP_NMATCH:
                nextToken();  # consume !~
                skipWhitespace();
                return parseRegexExpr(left, DP_OP_REGEX_NMATCH, "!~");

            case DPQL_TOK_IDENTIFIER: {
                string kw = tok.value.lwr();
                if (kw == "between" || kw == "in"
                        || (kw == "not" && tokenizer.peekAheadSkipWhitespace() == "in")
                        || (kw == "not" && tokenizer.peekAheadSkipWhitespace() == "between")) {
                    # Handle "not in" and "not between" keyword pairs
                    bool negated = kw == "not";
                    if (negated) {
                        nextToken();  # consume 'not'
                        skipWhitespace();
                        kw = peek().value.lwr();
                    }
                    if (kw == "in") {
                        nextToken();  # consume 'in'
                        skipWhitespace();
                        list<auto> values = ();
                        if (!atEnd() && peek().type == DPQL_TOK_LIST) {
                            auto parsed = parseStructuredLiteral();
                            if (parsed.typeCode() != NT_LIST) {
                                addError("'in' needs its values in parentheses, such as in (1, 2).", "DPQL-E013",
                                    line, column, line, column);
                                throw "DPQL-PARSE-ERROR", "Expected list literal";
                            }
                            values = parsed;
                        } else {
                            expect(DPQL_TOK_LPAREN, "'('",
                                "Something is missing at the end: the values after 'in', in parentheses, such as "
                                    "in (1, 2).",
                                "'in' needs its values in parentheses, such as in (1, 2).");
                            skipWhitespace();
                            values = parseValueList();
                            skipWhitespace();
                            expectClosingParen(DpqlParenList);
                        }
                        return <DataProviderExpression>{
                            "exp": negated ? DP_OP_NOT_IN : DP_SEARCH_OP_IN,
                            "args": (left,) + values,
                        };
                    }
                    if (kw == "between") {
                        nextToken();  # consume 'between'
                        skipWhitespace();
                        auto low = parseAdditiveExpr();
                        skipWhitespace();
                        hash<DpqlTokenInfo> and_tok = peek();
                        if (and_tok.type != DPQL_TOK_IDENTIFIER || and_tok.value.lwr() != "and") {
                            addError("'between' needs 'and' between its two bounds, as in @age between 18 and 65.",
                                "DPQL-E001", and_tok);
                            throw "DPQL-PARSE-ERROR", "Expected 'and'";
                        }
                        nextToken();  # consume 'and'
                        skipWhitespace();
                        auto high = parseAdditiveExpr();
                        if (negated) {
                            return <DataProviderExpression>{
                                "exp": DP_SEARCH_OP_NOT,
                                "args": (<DataProviderExpression>{
                                    "exp": DP_SEARCH_OP_BETWEEN,
                                    "args": (left, low, high),
                                },),
                            };
                        }
                        return <DataProviderExpression>{
                            "exp": DP_SEARCH_OP_BETWEEN,
                            "args": (left, low, high),
                        };
                    }
                }
                # Check for provider-specific infix comparison operators (e.g. "like")
                *string exp_name = symbol_to_name{kw};
                if (exp_name && expressions{exp_name}) {
                    hash<DataProviderExpressionInfo> info = expressions{exp_name};
                    if (info.type == DET_Operator && inlist("Comparison", info.groups)
                            && info.args.size() >= 2) {
                        nextToken();  # consume the operator keyword
                        skipWhitespace();
                        return <DataProviderExpression>{
                            "exp": exp_name,
                            "args": (left, parseAdditiveExpr()),
                        };
                    }
                }
                # Not a keyword we recognize here
                break;
            }
        }

        # No operator - return the value as-is
        return left;
    }

    #! Parses a regex expression after =~ or !~
    /** @param left the left-hand side expression
        @param op_name the canonical expression-map operator name (DP_OP_REGEX_MATCH or DP_OP_REGEX_NMATCH)
        @param op_sym the source-level operator symbol used in diagnostics (\"=~\" or \"!~\")
    */
    private hash<DataProviderExpression> parseRegexExpr(auto left, string op_name, string op_sym) {
        if (atEnd()) {
            addError(sprintf("Something is missing at the end: a pattern after '%s', between slashes, such as /abc/.",
                op_sym), "DPQL-E009", line, column, line, column);
            throw "DPQL-PARSE-ERROR", "Expected regex pattern";
        }

        hash<DpqlTokenInfo> tok = peek();
        if (tok.type != DPQL_TOK_REGEX) {
            # a pattern that the tokenizer could not read has been reported by it
            if (tok.type != DPQL_TOK_ERROR) {
                addError(sprintf("'%s' needs a pattern between slashes after it, such as /abc/.", op_sym),
                    "DPQL-E009", tok);
            }
            throw "DPQL-PARSE-ERROR", "Expected regex pattern";
        }

        nextToken();  # consume regex
        string pattern = tok.value;

        # Parse the regex: /pattern/flags
        # Extract pattern and flags
        if (pattern.size() < 2 || pattern[0] != "/") {
            addError("A pattern is written between slashes, such as /abc/.", "DPQL-E009", tok);
            throw "DPQL-PARSE-ERROR", "Invalid regex pattern";
        }

        int last_slash = rindex(pattern, "/");
        if (last_slash <= 0) {
            addError("A pattern is written between slashes, such as /abc/.", "DPQL-E009", tok);
            throw "DPQL-PARSE-ERROR", "Invalid regex pattern";
        }

        string regex_pattern = substr(pattern, 1, last_slash - 1);
        string flags = substr(pattern, last_slash + 1);

        list<auto> args = (left, regex_pattern);

        # Collect flags as a list of string flag names for args[2]
        if (flags) {
            list<string> flag_list = ();
            for (int i = 0; i < flags.size(); ++i) {
                string flag = flags[i];
                switch (flag) {
                    case "i":
                    case "s":
                    case "m":
                    case "x":
                    case "u":
                        flag_list += flag;
                        break;
                    default:
                        addError(sprintf("'%s' isn't a pattern option: the options are i, s, m, x and u, after the "
                            "closing '/'.", flag), "DPQL-E009", tok);
                }
            }
            if (flag_list) {
                args += (flag_list,);
            }
        }

        return <DataProviderExpression>{
            "exp": op_name,
            "args": args,
        };
    }

    #! Parses a value (field ref, literal, function call, or grouped expression)
    private auto parseValue() {
        skipWhitespace();
        if (atEnd()) {
            addError(getMissingValueMessage(), "DPQL-E001", line, column, line, column);
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

            case DPQL_TOK_LPAREN: {
                # Grouped expression; function calls are handled with their identifier
                nextToken();  # consume (
                # a group directly after a prefix operator name is the operator's first argument
                *hash<DpqlTokenInfo> prefix_name_tok;
                if (prefix_op_paren_tok && prefix_op_paren_tok.line == tok.line
                        && prefix_op_paren_tok.column == tok.column) {
                    prefix_name_tok = prefix_op_name_tok;
                }
                remove prefix_op_paren_tok;
                remove prefix_op_name_tok;
                skipWhitespace();
                auto exp = parseOrExpr();
                skipWhitespace();
                if (prefix_name_tok && !atEnd() && peek().type == DPQL_TOK_COMMA) {
                    addError(sprintf("'%s' is written without parentheses around its values, as in %s.",
                        prefix_name_tok.value, getPrefixOperatorExample(prefix_name_tok)), "DPQL-E001",
                        prefix_name_tok);
                    throw "DPQL-PARSE-ERROR", "Invalid operator syntax";
                }
                expectClosingParen(DpqlParenGroup);
                return exp;
            }

            case DPQL_TOK_IDENTIFIER: {
                hash<DpqlTokenInfo> id_tok = nextToken();
                skipWhitespace();
                # "exists" followed by a field reference, also in parentheses, tests the record for the field; it
                # binds tighter than any other operator; otherwise the word keeps its meaning (an unquoted value)
                if (id_tok.value.lwr() == DP_OP_EXISTS && !atEnd()) {
                    if (peek().type == DPQL_TOK_FIELD || peek().type == DPQL_TOK_LPAREN) {
                        return parseExists(id_tok);
                    }
                    # a value after it cannot be an operand of anything else
                    if (DpqlValueStartTokens{peek().type}) {
                        addError(DpqlExistsNeedsFieldMessage, "DPQL-E025",
                            peek());
                        throw "DPQL-PARSE-ERROR", "Expected a field reference";
                    }
                }
                string name = resolveExpressionName(id_tok.value);
                if (isPrefixExpressionOperator(name)) {
                    # a parenthesized first argument is accepted (ex: "map (@x + 1) * 2, @list"), but a
                    # parenthesized argument list is reported as function-call syntax when its comma is reached
                    if (!atEnd() && peek().type == DPQL_TOK_LPAREN) {
                        prefix_op_paren_tok = peek();
                        prefix_op_name_tok = id_tok;
                    }
                    return parsePrefixExpressionOperator(id_tok, name);
                }
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
                addError(sprintf("Something is missing before '%s': a value, such as a field or a template.",
                    tok.value), "DPQL-E001", tok);
                throw "DPQL-PARSE-ERROR", "Unexpected token";
        }
    }

    #! Parses \c exists and its field reference, which can be in parentheses
    /** @param name_tok the \c exists token, which has been read; the next token is a field reference or \c (

        @return the @ref DP_OP_EXISTS expression

        @throw DPQL-PARSE-ERROR the operand is not a field reference with accessors that each read one key or one
        position

        @since DataProvider 3.11
    */
    private hash<DataProviderExpression> parseExists(hash<DpqlTokenInfo> name_tok) {
        bool paren = peek().type == DPQL_TOK_LPAREN;
        if (paren) {
            nextToken();  # consume (
            skipWhitespace();
        }
        if (atEnd() || peek().type != DPQL_TOK_FIELD) {
            addError(DpqlExistsNeedsFieldMessage, "DPQL-E025",
                atEnd() ? name_tok : peek());
            throw "DPQL-PARSE-ERROR", "Expected a field reference";
        }
        hash<DpqlTokenInfo> field_tok = peek();
        auto operand = parseFieldRef();
        if (*string issue = dp_exists_operand_issue(operand)) {
            addError("'exists' " + issue + ".", "DPQL-E025", field_tok);
            throw "DPQL-PARSE-ERROR", "Invalid operand of exists";
        }
        if (paren) {
            skipWhitespace();
            expectClosingParen(DpqlParenExists);
        }
        return <DataProviderExpression>{
            "exp": DP_OP_EXISTS,
            "args": (operand,),
        };
    }

    #! Parses a field reference with optional accessor chain as expression nodes
    /** Returns a plain DataProviderFieldReference if no accessors follow, or a nested
        expression tree using DP_FUNC_INDEX/DP_FUNC_SLICE/DP_FUNC_KEY operators.
    */
    private auto parseFieldRef() {
        hash<DpqlTokenInfo> tok = nextToken();
        string value = tok.value;

        # Extract field name from @field or @"field"
        if (value.size() <= 1) {
            addError("Invalid field reference", "DPQL-E003", tok);
            throw "DPQL-PARSE-ERROR", "Invalid field reference";
        }
        string field_name = getFieldTokenName(tok);

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
            # Bracketed value: find the matching closing '}' the same way the tokenizer did when it ended the token
            # (DpqlTokenizer::scanTemplateRef()): a backslash escapes the next character, and a quoted string ('...'
            # or "...", with backslash escapes inside) is skipped, so a brace in either does not count.  Counting
            # every brace instead closed the value early, truncating values such as {c.a\}b} or {c."x}y"}.
            int depth = 0;
            int close_pos = -1;
            int size = rest.size();
            for (int i = 0; i < size; ++i) {
                string ch = rest[i];
                if (ch == "\\") {
                    # skip the escaped character
                    ++i;
                } else if (ch == '"' || ch == "'") {
                    # skip the quoted string, honoring backslash escapes inside it
                    string quote = ch;
                    for (++i; i < size; ++i) {
                        ch = rest[i];
                        if (ch == "\\") {
                            ++i;
                        } else if (ch == quote) {
                            break;
                        }
                    }
                } else if (ch == "{") {
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
                # Index or slice accessor: [...], or the list wildcard [*]
                hash<DpqlTokenInfo> lbracket = nextToken();  # consume [
                skipWhitespace();
                if (!atEnd() && peek().type == DPQL_TOK_WILDCARD) {
                    # the wildcard's item expression takes the rest of the accessor chain
                    return parseWildcard(result, lbracket, DP_FUNC_LIST_WILDCARD);
                }
                result = parseIndexAccessor(result, lbracket);
            } else if (tok.type == DPQL_TOK_LBRACE) {
                # Key accessor: {...}, or the hash wildcard {*}
                hash<DpqlTokenInfo> lbrace = nextToken();  # consume {
                skipWhitespace();
                if (!atEnd() && peek().type == DPQL_TOK_WILDCARD) {
                    # the wildcard's value expression takes the rest of the accessor chain
                    return parseWildcard(result, lbrace, DP_FUNC_HASH_WILDCARD);
                }
                result = parseKeyAccessor(result, lbrace);
            } else if (tok.type == DPQL_TOK_DOT) {
                # Member access after accessor: .identifier
                nextToken();  # consume .
                skipWhitespace();
                if (atEnd()) {
                    addError(DpqlDotNeedsKeyMessage, "DPQL-E020", tok);
                    throw "DPQL-PARSE-ERROR", "Expected identifier after '.'";
                }
                hash<DpqlTokenInfo> id_tok = peek();
                if (id_tok.type != DPQL_TOK_IDENTIFIER && id_tok.type != DPQL_TOK_FIELD) {
                    addError(DpqlDotNeedsKeyMessage, "DPQL-E020", id_tok);
                    throw "DPQL-PARSE-ERROR", "Expected identifier after '.'";
                }
                # @field or @"field" after the dot is the name of the field
                string member_name = id_tok.type == DPQL_TOK_FIELD ? getFieldTokenKey(id_tok) : id_tok.value;
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

    #! Parses a wildcard accessor (\c [*] or \c {*}) and the accessors after it
    /** The wildcard is \c map written as an accessor: the accessors after it are the expression evaluated for each
        item of the source, which is the expression before the wildcard:
        - <tt>source[*]</tt>: a first step that names a field (<tt>.name</tt>, <tt>.\@name</tt> or a single
          <tt>{"key"}</tt>) is a field reference of the item, as in the body of \c map, with the following
          <tt>.name</tt> steps (<tt>\@orders[*].customer.name</tt> is <tt>map \@customer.name, \@orders</tt>);
          other accessors apply to the item itself (@ref DP_FUNC_WILDCARD_ITEM), and so does a trailing \c [*]
        - <tt>source{*}</tt>: the accessors apply to \c \@value (<tt>\@prices{*}.amount</tt> is
          <tt>map \@value.amount, \@prices</tt>)

        @param source the expression before the wildcard
        @param open_tok the opening bracket or brace, which has been read; the next token is the \c *
        @param op @ref DP_FUNC_LIST_WILDCARD or @ref DP_FUNC_HASH_WILDCARD

        @return the wildcard expression, which takes the rest of the accessor chain

        @since DataProvider 3.10
    */
    private hash<DataProviderExpression> parseWildcard(auto source, hash<DpqlTokenInfo> open_tok, string op) {
        bool list_wildcard = op == DP_FUNC_LIST_WILDCARD;
        string close = list_wildcard ? "]" : "}";
        string code = list_wildcard ? "DPQL-E022" : "DPQL-E024";
        nextToken();  # consume *
        skipWhitespace();
        if (atEnd() || peek().type == DPQL_TOK_EOF) {
            addError(list_wildcard
                    ? "This '[' isn't closed: add a ']' after the '*', as in [*]."
                    : "This '{' isn't closed: add a '}' after the '*', as in {*}.", code,
                open_tok);
            throw "DPQL-PARSE-ERROR", sprintf("Unclosed %s accessor", list_wildcard ? "index" : "key");
        }
        hash<DpqlTokenInfo> tok = peek();
        if (tok.type != (list_wildcard ? DPQL_TOK_RBRACKET : DPQL_TOK_RBRACE)) {
            addError(list_wildcard ? DpqlListWildcardAloneMessage : DpqlHashWildcardAloneMessage, code, tok);
            throw "DPQL-PARSE-ERROR", sprintf("Expected '%s' after '*'", close);
        }
        nextToken();  # consume ] or }

        auto body = list_wildcard
            ? parseListWildcardBody()
            # the accessors after {*} apply to @value, as in the body of map
            : parseWildcardFieldPath("value");
        return <DataProviderExpression>{
            "exp": op,
            "args": (source, body),
        };
    }

    #! Parses the accessors after \c [*]: the expression evaluated for each item
    /** @since DataProvider 3.10
    */
    private auto parseListWildcardBody() {
        hash<DataProviderExpression> item = <DataProviderExpression>{
            "exp": DP_FUNC_WILDCARD_ITEM,
            "args": (),
        };
        skipWhitespace();
        if (atEnd()) {
            return item;
        }
        hash<DpqlTokenInfo> tok = peek();
        if (tok.type == DPQL_TOK_DOT) {
            # a field of the item, as in the body of map: @orders[*].id is map @id, @orders
            nextToken();  # consume .
            skipWhitespace();
            if (atEnd()) {
                addError(DpqlDotNeedsKeyMessage, "DPQL-E020", tok);
                throw "DPQL-PARSE-ERROR", "Expected identifier after '.'";
            }
            hash<DpqlTokenInfo> id_tok = peek();
            if (id_tok.type == DPQL_TOK_IDENTIFIER) {
                nextToken();
                return parseWildcardFieldPath(id_tok.value);
            }
            if (id_tok.type == DPQL_TOK_FIELD) {
                nextToken();
                if (id_tok.value.size() > 1 && id_tok.value[1] == '"') {
                    # a quoted name is one key, as with {"key"}
                    return parseAccessorChain(<DataProviderFieldReference>{
                        "field": AbstractDataProvider::escapeFieldPathKey(getFieldTokenKey(id_tok)),
                    });
                }
                return parseWildcardFieldPath(getFieldTokenName(id_tok));
            }
            addError(DpqlDotNeedsKeyMessage, "DPQL-E020", id_tok);
            throw "DPQL-PARSE-ERROR", "Expected identifier after '.'";
        }
        if (tok.type == DPQL_TOK_LBRACE) {
            hash<DpqlTokenInfo> lbrace = nextToken();  # consume {
            skipWhitespace();
            if (!atEnd() && peek().type == DPQL_TOK_WILDCARD) {
                return parseWildcard(item, lbrace, DP_FUNC_HASH_WILDCARD);
            }
            hash<DataProviderExpression> key_exp = parseKeyAccessor(item, lbrace);
            if (key_exp.args.size() == 2) {
                # a single key is a field of the item: @orders[*]{"order-id"} is map @"order-id", @orders
                return parseAccessorChain(<DataProviderFieldReference>{
                    "field": AbstractDataProvider::escapeFieldPathKey(key_exp.args[1]),
                });
            }
            # several keys apply to the item itself
            return parseAccessorChain(key_exp);
        }
        # any other accessor applies to the item itself, as does a trailing [*]
        return parseAccessorChain(item);
    }

    #! Parses the field path that the accessors after a wildcard start with, and the accessors after it
    /** The <tt>.name</tt> steps that follow are part of the field reference, as in <tt>\@customer.name</tt>; the
        accessor chain continues after them.

        @param path the first part of the path

        @since DataProvider 3.10
    */
    private auto parseWildcardFieldPath(string path) {
        while (True) {
            skipWhitespace();
            if (atEnd() || peek().type != DPQL_TOK_DOT) {
                break;
            }
            hash<DpqlTokenInfo> dot = nextToken();  # consume .
            skipWhitespace();
            if (atEnd()) {
                addError(DpqlDotNeedsKeyMessage, "DPQL-E020", dot);
                throw "DPQL-PARSE-ERROR", "Expected identifier after '.'";
            }
            hash<DpqlTokenInfo> id_tok = peek();
            if (id_tok.type == DPQL_TOK_IDENTIFIER) {
                nextToken();
                path += "." + id_tok.value;
                continue;
            }
            if (id_tok.type == DPQL_TOK_FIELD) {
                # member access after a field reference, as in parseAccessorChain()
                nextToken();
                return parseAccessorChain(<DataProviderExpression>{
                    "exp": DP_FUNC_KEY,
                    "args": (<DataProviderFieldReference>{"field": path}, getFieldTokenKey(id_tok)),
                });
            }
            addError(DpqlDotNeedsKeyMessage, "DPQL-E020", id_tok);
            throw "DPQL-PARSE-ERROR", "Expected identifier after '.'";
        }
        return parseAccessorChain(<DataProviderFieldReference>{"field": path});
    }

    #! Returns the field path of a field token: the text after \c @, or the text between the quotes of
    #! <tt>\@"..."</tt> with its escapes decoded
    /** Dots separate the parts of the path in quoted names as in bare ones; quotes are only for characters that a
        bare name cannot hold.  In a quoted name, \c \\" is a quote, \c \\\\ a backslash, \c \\n, \c \\r and
        \c \\t a line feed, a carriage return and a tab, and \c \\. a dot that is part of a key, which the path
        keeps escaped (<tt>\@"a\\.b"</tt> is the field path <tt>a\\.b</tt>, the key <tt>a.b</tt>); any other escape
        is an error (@ref DPQL_FIELD_NAME_ESCAPES)

        @param tok a @ref DPQL_TOK_FIELD token

        @return the field path

        @throw DPQL-PARSE-ERROR a quoted name has an escape that field names do not have

        @since DataProvider 3.10.1
        @since DataProvider 3.11 an escaped dot is part of a key, and an unknown escape is an error; the backslash of
        an escaped dot was dropped, so it separated the parts of the path, as was the backslash of any unknown escape
    */
    private string getFieldTokenName(hash<DpqlTokenInfo> tok) {
        string value = tok.value;
        if (value.size() > 1 && value[1] == '"') {
            return unescapeFieldName(tok, substr(value, 2, -1), True);
        }
        return substr(value, 1);
    }

    #! Returns the key that a field token names after a dot that follows an accessor
    /** As getFieldTokenName(), but the name is one key, in which an escaped dot is a dot
        (<tt>\@r[0].\@"a\\.b"</tt> and <tt>\@r[0].\@"a.b"</tt> are both the key <tt>a.b</tt>)

        @param tok a @ref DPQL_TOK_FIELD token

        @return the key

        @throw DPQL-PARSE-ERROR a quoted name has an escape that field names do not have

        @since DataProvider 3.11
    */
    private string getFieldTokenKey(hash<DpqlTokenInfo> tok) {
        string value = tok.value;
        if (value.size() > 1 && value[1] == '"') {
            return unescapeFieldName(tok, substr(value, 2, -1), False);
        }
        return substr(value, 1);
    }

    #! Decodes the escapes of a quoted field name
    /** @param tok the field token, for the error
        @param content the text between the quotes
        @param path True to keep an escaped dot escaped, as a field path holds a dot that is part of a key; False to
        decode it to a dot, for a key

        @return the field name

        @throw DPQL-PARSE-ERROR an escape that field names do not have

        @since DataProvider 3.11
    */
    private string unescapeFieldName(hash<DpqlTokenInfo> tok, string content, bool path) {
        if (index(content, "\\") < 0) {
            return content;
        }
        string result = "";
        bool escaped = False;
        foreach string ch in (content.iterator()) {
            if (!escaped) {
                if (ch == "\\") {
                    escaped = True;
                } else {
                    result += ch;
                }
                continue;
            }
            escaped = False;
            switch (ch) {
                case "n":
                    result += "\n";
                    break;
                case "r":
                    result += "\r";
                    break;
                case "t":
                    result += "\t";
                    break;
                case ".":
                    result += path ? "\\." : ".";
                    break;
                case '"':
                case "\\":
                    result += ch;
                    break;
                default:
                    addError(sprintf("Unknown escape '\\%s' in a quoted field name; the escapes are %s", ch,
                        DPQL_FIELD_NAME_ESCAPES), "DPQL-E003", tok);
                    throw "DPQL-PARSE-ERROR", "Unknown escape in a quoted field name";
            }
        }
        if (escaped) {
            addError("A quoted field name cannot end with a backslash", "DPQL-E003", tok);
            throw "DPQL-PARSE-ERROR", "Unknown escape in a quoted field name";
        }
        return result;
    }

    #! Parses index accessor: [n], [n..m], [n,m,p], [n..m,p], [2..], [..5]
    /** @param base the base expression to index into
        @param lbracket the opening bracket, which has been read
        @return a DP_FUNC_INDEX or DP_FUNC_SLICE expression node
    */
    private hash<DataProviderExpression> parseIndexAccessor(auto base, hash<DpqlTokenInfo> lbracket) {
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
                    addError("Separate the positions with ',', or close the '[' with ']'.", "DPQL-E022", tok);
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

            if (tok.type == DPQL_TOK_OP_MULTIPLY || tok.type == DPQL_TOK_WILDCARD) {
                addError(DpqlListWildcardAloneMessage, "DPQL-E022", tok);
                throw "DPQL-PARSE-ERROR", "Wildcard combined with indexes";
            }

            # Parse index item: int or range
            items += parseIndexItem();
        }

        if (!closed) {
            addError("This '[' isn't closed: add a ']' after the position.", "DPQL-E022", lbracket);
            throw "DPQL-PARSE-ERROR", "Unclosed index accessor";
        }

        if (!items) {
            addError("Write a position between the brackets, such as [0], or remove them.", "DPQL-E022", lbracket);
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
                addError("A range needs a first or a last position, such as [0..2], [2..] or [..2].", "DPQL-E021",
                    tok);
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
            addError("A position is a whole number, such as [0], [-1] or [1..3].", "DPQL-E022", tok);
            throw "DPQL-PARSE-ERROR", "Expected integer in index";
        }
        nextToken();  # consume number

        string value = tok.value;
        # Check for float
        if (index(value, ".") >= 0) {
            addError(sprintf("A position is a whole number, such as [1], not %s.", tok.value), "DPQL-E023", tok);
            throw "DPQL-PARSE-ERROR", "Index must be integer, not float";
        }
        return value.toInt();
    }

    #! Parses key accessor: {key}, {"key"}, {key1, key2}
    /** @param base the base expression to apply key access to
        @param lbrace the opening brace, which has been read
        @return a DP_FUNC_KEY expression node
    */
    private hash<DataProviderExpression> parseKeyAccessor(auto base, hash<DpqlTokenInfo> lbrace) {
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
                    addError("Separate the keys with ',', or close the '{' with '}'.", "DPQL-E024", tok);
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

            if (tok.type == DPQL_TOK_OP_MULTIPLY || tok.type == DPQL_TOK_WILDCARD) {
                addError(DpqlHashWildcardAloneMessage, "DPQL-E024", tok);
                throw "DPQL-PARSE-ERROR", "Wildcard combined with keys";
            }

            key_list += parseKeyName();
        }

        if (!closed) {
            addError("This '{' isn't closed: add a '}' after the key.", "DPQL-E024", lbrace);
            throw "DPQL-PARSE-ERROR", "Unclosed key accessor";
        }

        if (!key_list) {
            addError("Write a key between the braces, such as {name}, or remove them.", "DPQL-E024", lbrace);
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

        addError("A key is a name, or text in quotes, such as {name} or {\"order id\"}.", "DPQL-E024", tok);
        throw "DPQL-PARSE-ERROR", "Expected key name";
    }

    #! Parses a string literal
    private string parseStringLiteral() {
        hash<DpqlTokenInfo> tok = nextToken();
        string value = tok.value;

        # substr() counts characters; exclude the final quote without using a byte count.
        string content = substr(value, 1, -1);
        return unescapeString(content);
    }

    #! Unescapes a string by processing escape sequences
    /** Handles \\n, \\r, \\t, \\\\, \\', \\" and passes through other backslash sequences
    */
    private string unescapeString(string content) {
        if (index(content, "\\") < 0) {
            return content;
        }
        string result = "";
        bool escaped = False;
        # Iterate by character once, including multibyte characters after a backslash.
        foreach string ch in (content.iterator()) {
            if (escaped) {
                switch (ch) {
                    case "n":
                        result += "\n";
                        break;
                    case "r":
                        result += "\r";
                        break;
                    case "t":
                        result += "\t";
                        break;
                    default:
                        result += ch;
                }
                escaped = False;
            } else if (ch == "\\") {
                escaped = True;
            } else {
                result += ch;
            }
        }
        if (escaped) {
            result += "\\";
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
        # a part of the date or time that is out of its range is reported as such
        if (*string issue = getDateLiteralIssue(tok.value)) {
            addError(issue, "DPQL-E011", tok);
            throw "DPQL-PARSE-ERROR", "Invalid date literal";
        }
        try {
            return date(tok.value);
        } catch (hash<ExceptionInfo> ex) {
            addError(sprintf("'%s' can't be read as a date: write it as 2026-10-23, or with a time as "
                "2026-10-23T14:30:00.", tok.value), "DPQL-E011", tok);
            throw "DPQL-PARSE-ERROR", "Invalid date literal";
        }
    }

    #! Returns what is wrong with a part of a date literal that is out of its range, or nothing if no part is
    /** The tokenizer has read the literal as a date (@ref DpqlTokenizer): \c YYYY-MM-DD, optionally followed by a
        time (\c T or a space, \c HH:MM, optional \c :SS and fraction, and an optional time zone)

        @param text the date literal

        @return the message that names the part and its range, or @ref nothing

        @since %DataProvider 3.11
    */
    private static *string getDateLiteralIssue(string text) {
        *list<*string> parts = regex_extract(text, DpqlDateLiteralParts);
        if (!parts) {
            return;
        }
        int year = parts[0].toInt();
        int month = parts[1].toInt();
        int day = parts[2].toInt();
        if (month < 1 || month > 12) {
            return sprintf("'%s' isn't a real date: the month must be from 1 to 12.", text);
        }
        int days = DpqlMonthDays[month - 1];
        if (month == 2 && ((!(year % 4) && (year % 100)) || !(year % 400))) {
            days = 29;
        }
        if (day < 1 || day > days) {
            return sprintf("'%s' isn't a real date: the day must be from 1 to %d in %s %d.", text, days,
                DpqlMonthNames[month - 1], year);
        }
        if (exists parts[3]) {
            if (parts[3].toInt() > 23) {
                return sprintf("'%s' isn't a real time of day: the hour must be from 0 to 23.", text);
            }
            if (parts[4].toInt() > 59) {
                return sprintf("'%s' isn't a real time of day: the minutes must be from 0 to 59.", text);
            }
            if (exists parts[5] && parts[5].toInt() > 59) {
                return sprintf("'%s' isn't a real time of day: the seconds must be from 0 to 59.", text);
            }
        }
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
            addError(tok.type == DPQL_TOK_HASH
                    ? "These key-value pairs can't be read: write them between braces, such as {a=1, b=2}."
                    : "This list can't be read: write its values between parentheses, separated by commas, such as "
                        "(1, 2).", "DPQL-E013", tok);
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
        string name = resolveExpressionName(token_name);

        *string accessor = DpqlWildcardAccessors{name};
        if (exists accessor) {
            addError(accessor
                ? sprintf("'%s' is written as the accessor %s, as in @orders%s.id", token_name, accessor, accessor)
                : sprintf("'%s' is the item of a [*] wildcard and is not written in DPQL", token_name),
                "DPQL-E001", name_tok);
            throw "DPQL-PARSE-ERROR", "Wildcard accessor written as a function call";
        }

        expect(DPQL_TOK_LPAREN, "'('");
        skipWhitespace();

        list<auto> args = ();
        list<hash<DpqlTokenInfo>> arg_tokens = ();
        if (!atEnd() && peek().type != DPQL_TOK_RPAREN) {
            args = parseArgList(\arg_tokens);
        }

        skipWhitespace();
        expectClosingParen(DpqlParenCall);

        # Validate function name against expression map
        if (expressions && !expressions{name}) {
            addWarning(sprintf("'%s' isn't a function that can be used here: pick one from the list.", token_name),
                "DPQL-W001", name_tok);
        } else if (expressions{name}) {
            hash<DataProviderExpressionInfo> info = expressions{name};
            int expected = info.args.size();
            int actual = args.size();
            if (info.varargs) {
                int min = AbstractDataProvider::getRequiredArgCount(info);
                if (actual < min) {
                    addWarning(
                        sprintf("'%s' needs at least %d value%s in its parentheses; it has %d.",
                            token_name, min, min == 1 ? "" : "s", actual),
                        "DPQL-W002", name_tok);
                }
            } else if (exists info.args && actual != expected) {
                # an expression without an argument signature is not checked; an empty one takes no arguments
                int min_required = AbstractDataProvider::getRequiredArgCount(info);
                if (actual < min_required) {
                    addWarning(
                        sprintf("'%s' needs %s%d value%s in its parentheses; it has %d.",
                            token_name,
                            min_required != expected ? "at least " : "",
                            min_required, min_required == 1 ? "" : "s", actual),
                        "DPQL-W002", name_tok);
                } else if (actual > expected) {
                    addWarning(
                        sprintf("'%s' takes at most %d value%s in its parentheses; it has %d.",
                            token_name, expected, expected == 1 ? "" : "s", actual),
                        "DPQL-W002", name_tok);
                }
            }

            # Type-aware argument validation
            validateFunctionArgTypes(token_name, info, args, arg_tokens);
        }

        return <DataProviderExpression>{
            "exp": name,
            "args": args,
        };
    }

    #! Parses prefix expression operators such as \c map and \c hash_map
    /** Prefix expression operators evaluate one or more argument expressions later
        in an operator-controlled context. They therefore use Qore-like operator
        syntax instead of function-call syntax.
    */
    private hash<DataProviderExpression> parsePrefixExpressionOperator(hash<DpqlTokenInfo> name_tok, string name) {
        hash<auto> spec = DpqlPrefixOperatorSpecs{name};
        int min_args = spec.min;
        int max_args = spec.max;
        list<auto> args = ();
        list<hash<DpqlTokenInfo>> arg_tokens = ();

        while (args.size() < max_args) {
            skipWhitespace();
            if (atEnd()) {
                addError(args.size() < min_args
                    ? getPrefixOperatorArityMessage(name_tok, name, min_args, max_args)
                    : getMissingValueMessage(),
                    "DPQL-E001", name_tok);
                throw "DPQL-PARSE-ERROR", "Unexpected end of input";
            }

            hash<DpqlTokenInfo> start_tok = peek();
            # push, so that a list literal argument stays one argument
            push args, parseOrExpr();
            arg_tokens += start_tok;
            skipWhitespace();

            if (atEnd() || peek().type != DPQL_TOK_COMMA) {
                break;
            }

            if (args.size() >= max_args) {
                addError(sprintf("'%s' takes at most %d value%s, as in %s: remove this ',' and what follows it.",
                    name_tok.value, max_args, max_args == 1 ? "" : "s", getPrefixOperatorExample(name_tok)),
                    "DPQL-E001", peek());
                throw "DPQL-PARSE-ERROR", "Too many arguments";
            }
            nextToken();
        }

        if (args.size() < min_args) {
            addError(getPrefixOperatorArityMessage(name_tok, name, min_args, max_args), "DPQL-E001", name_tok);
            throw "DPQL-PARSE-ERROR", "Too few arguments";
        }

        if (expressions && expressions{name}) {
            validateFunctionArgTypes(name_tok.value, expressions{name}, args, arg_tokens);
        }

        return <DataProviderExpression>{
            "exp": name,
            "args": args,
        };
    }

    #! Returns what a diagnostic says of a prefix expression operator that has fewer values than it needs
    /** @param name_tok the operator's name as written
        @param name the operator's expression name
        @param min_args the number of values that it needs
        @param max_args the number of values that it takes

        @return the message, with an example of the operator

        @since %DataProvider 3.11
    */
    private string getPrefixOperatorArityMessage(hash<DpqlTokenInfo> name_tok, string name, int min_args,
            int max_args) {
        if (min_args == 1) {
            return sprintf("'%s' needs a value after it, as in %s.", name_tok.value,
                getPrefixOperatorExample(name_tok));
        }
        return sprintf("'%s' needs %s%d values, separated by ',', as in %s.", name_tok.value,
            min_args < max_args ? "at least " : "", min_args, getPrefixOperatorExample(name_tok));
    }

    #! Returns an example of a prefix expression operator, written with its name as it is written in the text
    /** @param name_tok the operator's name as written

        @return the example, such as <tt>map \@name, \@users</tt>

        @since %DataProvider 3.11
    */
    private string getPrefixOperatorExample(hash<DpqlTokenInfo> name_tok) {
        string name = resolveExpressionName(name_tok.value);
        *string example = DpqlPrefixOperatorExamples{name};
        if (!example) {
            return name_tok.value + " ...";
        }
        # the example is written with the operator's name as the text writes it
        int space = example.find(" ");
        return name_tok.value + example.substr(space);
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
                addWarning(sprintf("%s must be a field, such as @name.", describeArgument(name, info, i)),
                    "DPQL-W003", tok);
                continue;
            }
            if (sig.type_code == ST_Value && is_field_ref) {
                addWarning(sprintf("%s must be written out, not taken from a field.", describeArgument(name, info, i)),
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
            if (!AbstractDataProvider::isArgTypeCompatible(sig.type, arg_type)) {
                addWarning(sprintf("%s must be %s, not %s.", describeArgument(name, info, i),
                    getTypeSentenceForm(sig.type), getTypeSentenceForm(arg_type)), "DPQL-W004", tok);
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
                    addWarning(sprintf("Value %d of '%s' must be a field, such as @name.", i + 1, name),
                        "DPQL-W003", tok);
                    continue;
                }
                if (last_sig.type_code == ST_Value && is_field_ref) {
                    addWarning(sprintf("Value %d of '%s' must be written out, not taken from a field.", i + 1, name),
                        "DPQL-W003", tok);
                    continue;
                }

                *AbstractDataProviderType arg_type = inferArgType(arg);
                if (arg_type && !AbstractDataProvider::isArgTypeCompatible(last_sig.type, arg_type)) {
                    addWarning(sprintf("Value %d of '%s' must be %s, not %s.", i + 1, name,
                        getTypeSentenceForm(last_sig.type), getTypeSentenceForm(arg_type)), "DPQL-W004", tok);
                }
            }
        }
    }

    #! Returns how a diagnostic names an argument of a function: by its display name, or by its position
    /** @param name the function's name as written
        @param info the function's definition
        @param i the argument's position, from 0

        @return <tt>The value 'Start' of 'substr'</tt>, or <tt>Value 2 of 'substr'</tt> for an argument without a
        display name

        @since %DataProvider 3.11
    */
    private static string describeArgument(string name, hash<DataProviderExpressionInfo> info, int i) {
        *string display_name = info.arg_info[i].display_name;
        return display_name.val()
            ? sprintf("The value '%s' of '%s'", display_name, name)
            : sprintf("Value %d of '%s'", i + 1, name);
    }

    #! Returns how a sentence names a type: its sentence form in the type catalogue, or its name
    /** @param type the type

        @return the type's @ref DataProvider::DataTypeCatalogueInfo::sentence_form "sentence_form" in the
        @ref DataProvider::DataTypeCatalogue "DataTypeCatalogue" (\c "a whole number"), or its type name for a type
        that the catalogue does not name

        @since %DataProvider 3.11
    */
    private static string getTypeSentenceForm(AbstractDataProviderType type) {
        return DataTypeCatalogue::lookupType(type).sentence_form ?? type.getName();
    }

    #! Reports a comparison whose result is the same for every record
    /** A value compared with itself (the same field, template value or literal, or the same arithmetic on them), or
        a literal compared with a literal, gives the same result whatever the record is, which is almost always a
        mistake in the expression (ex: <tt>\@price > \@price</tt> written for <tt>\@price > \@cost</tt>).

        @param exp a comparison with two arguments
        @param diags the diagnostics to add a \c DPQL-W006 warning to
    */
    private checkConstantComparison(hash<DataProviderExpression> exp, reference<list<hash<DpqlDiagnostic>>> diags) {
        auto left = exp.args[0];
        auto right = exp.args[1];
        *bool result;
        string reason;
        if (DpqlParser::isLiteralArg(left) && DpqlParser::isLiteralArg(right)) {
            # evaluated as a record would evaluate it; a comparison that fails is left to evaluation
            try {
                result = AbstractDataProvider::evalGenericExpressionValue({}, exp, NOTHING).toBool();
            } catch (hash<ExceptionInfo> ex) {
                return;
            }
            reason = "both sides are literal values";
        } else if (left === right && DpqlParser::isStableArg(left)) {
            result = DpqlSelfComparisonResults{exp.exp};
            reason = "it compares a value with itself";
        } else {
            return;
        }
        diags += <DpqlDiagnostic>{
            "severity": "warning",
            "message": sprintf("This comparison is always %s, for every record: %s", result ? "true" : "false",
                reason),
            "code": "DPQL-W006",
            "line": 1,
            "column": 1,
            "end_line": 1,
            "end_column": 1,
        };
    }

    #! Returns True if the argument is a literal value
    private static bool isLiteralArg(auto arg) {
        if (arg instanceof hash<DataProviderExpression>) {
            return arg.exp == DP_EXPR_VALUE && arg.args.size() == 1 && DpqlParser::isLiteralArg(arg.args[0]);
        }
        if (arg instanceof hash<DataProviderFieldReference> || arg instanceof hash<DataProviderTemplateReference>) {
            return False;
        }
        if (arg.typeCode() == NT_LIST) {
            return all DpqlParser::isLiteralArg($1), arg;
        }
        if (arg.typeCode() == NT_HASH) {
            return all DpqlParser::isLiteralArg($1), arg.iterator();
        }
        return True;
    }

    #! Returns True if the argument has one value while an expression is evaluated for a record
    /** A field, a template value and a literal have one value for a record; an expression has one if its operator
        gives the same result for the same arguments.  A function such as \c now() does not.
    */
    private static bool isStableArg(auto arg) {
        if (arg instanceof hash<DataProviderFieldReference> || arg instanceof hash<DataProviderTemplateReference>) {
            return True;
        }
        if (arg instanceof hash<DataProviderExpression>) {
            return DpqlDeterministicOperators{arg.exp} && (all DpqlParser::isStableArg($1), arg.args);
        }
        return DpqlParser::isLiteralArg(arg);
    }

    #! Reports arithmetic operands that the operator cannot combine
    /** Each operand whose kind of value is known (from a literal, a field's type, or nested arithmetic) is checked
        against the operator, from left to right, as the operator combines them; see
        @ref ExpressionTypes::checkArithmetic().

        @param exp an arithmetic expression
        @param diags the diagnostics to add a \c DPQL-E050 error to
    */
    private checkArithmeticOperands(hash<DataProviderExpression> exp, reference<list<hash<DpqlDiagnostic>>> diags) {
        *hash<ExpressionArithmeticMismatch> mismatch = ExpressionTypes::checkArithmetic(exp, fields, expressions);
        if (!mismatch) {
            return;
        }
        diags += <DpqlDiagnostic>{
            "severity": "error",
            "message": sprintf("Operator '%s' cannot be applied to %s and %s: %s", getOpSymbol(mismatch.op),
                mismatch.left_combined
                    ? ExpressionTypes::OperandKindLabels{mismatch.left_kind}
                    : DpqlParser::describeOperand(mismatch.left, mismatch.left_kind),
                DpqlParser::describeOperand(mismatch.right, mismatch.right_kind),
                ExpressionTypes::ArithmeticAccepts{mismatch.op}),
            "code": "DPQL-E050",
            "line": 1,
            "column": 1,
            "end_line": 1,
            "end_column": 1,
        };
    }

    #! Describes an arithmetic operand in a diagnostic
    private static string describeOperand(auto arg, string kind) {
        string label = ExpressionTypes::OperandKindLabels{kind};
        if (arg instanceof hash<DataProviderFieldReference>) {
            return sprintf("'@%s' (%s)", arg.field, label);
        }
        return label;
    }

    #! Infers the type of a parsed argument
    /** @return the type (@ref ExpressionTypes::getType()), or NOTHING if the type is only known when the argument
        is evaluated

        @since DataProvider 3.4
    */
    private *AbstractDataProviderType inferArgType(auto arg) {
        return ExpressionTypes::getType(arg, fields, expressions);
    }

    #! Infers the return type of a parsed expression node
    /** @param exp the parsed expression node
        @return the type (@ref ExpressionTypes::getType()), or NOTHING if the type is only known when the expression
        is evaluated

        @since DataProvider 3.4
        @since %DataProvider 3.9 the type is given by the expression type rules (@ref ExpressionTypes), so for
        example \c 1 \c + \c 2.5 gives \c float, and \c abs(\@qty) gives the type of the field \c qty
    */
    *AbstractDataProviderType inferExpressionReturnType(hash<DataProviderExpression> exp) {
        return ExpressionTypes::getType(exp, fields, expressions);
    }

    #! Parses an argument list, also collecting the start token of each argument
    /** @param arg_tokens output list populated with the start token of each argument

        @since DataProvider 3.4 added arg_tokens parameter
    */
    private list<auto> parseArgList(reference<list<hash<DpqlTokenInfo>>> arg_tokens) {
        list<auto> args = ();

        # First argument; pushed, so that a list literal argument stays one argument
        hash<DpqlTokenInfo> start_tok = peek();
        push args, parseOrExpr();
        arg_tokens += start_tok;
        skipWhitespace();

        # Additional arguments
        while (!atEnd() && peek().type == DPQL_TOK_COMMA) {
            nextToken();  # consume comma
            skipWhitespace();
            start_tok = peek();
            push args, parseOrExpr();
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
