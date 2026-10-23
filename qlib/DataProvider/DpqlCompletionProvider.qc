# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DpqlCompletionProvider.qc DPQL completion provider implementation

/*  DpqlCompletionProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! DPQL Completion item structure
/** Contains information about a single completion suggestion.
*/
public hashdecl DpqlCompletionItem {
    #! Display text shown in the completion menu
    string label;
    #! Item kind: "field", "operator", "function", "keyword", "value"
    string kind;
    #! Text to insert when this completion is selected
    string insert_text;
    #! Optional snippet text with parameter placeholders (LSP snippet syntax)
    /** When present, clients should use this instead of \c insert_text and set
        \c insertTextFormat to \c 2 (snippet).

        @since DataProvider 3.4
    */
    *string snippet_text;
    #! Optional documentation or help text
    *string documentation;
    #! Optional detail text shown next to the label, in plain words
    /** Displayed in a muted/grey style by most editors. Used to show, with the type names of the
        @ref DataProvider::DataTypeCatalogue "DataTypeCatalogue":
        - the type of a field or a template value, by its display name (e.g., \c "Text", \c "Whole number")
        - what a function or an operator gives (e.g., \c "gives true or false")
        - the type of the argument that the cursor is in, when inside a function call
          (e.g., \c "this argument takes text", also after the type of a field: <tt>"Whole number; this argument
          takes text"</tt>)
        - that a template context needs a value after its \c ':' (\c "needs a value after ':'")

        A type that the catalogue does not name is shown by its type name.

        @since DataProvider 3.4
        @since DataProvider 3.11 in plain words, with the type names of the
        @ref DataProvider::DataTypeCatalogue "DataTypeCatalogue"; before, type names (\c "int"), a return type after
        an arrow (\c "→ bool"), \c "expected: string" and \c "requires argument"
    */
    *string detail;
    #! Optional text used by LSP clients to filter the typed word against this item
    /** LSP clients filter the user's typed prefix against \c filterText, falling back to
        \c label when not present. This must be set when \c label is a human-readable
        display name that differs from the text the user actually types (e.g. a template
        value whose label is its \c display_name); otherwise typed-prefix filtering would
        match against the display name instead of the raw template text.

        When not present, clients filter against \c label (the current behavior).

        @since DataProvider 3.5.1
    */
    *string filter_text;
    #! Sort priority (lower = higher priority)
    int sort_priority = 100;
}

#! DPQL signature help parameter info
/** @since DataProvider 3.4
*/
public hashdecl DpqlParameterInfo {
    #! Parameter label (e.g. "String Value")
    string label;
    #! Parameter documentation
    *string documentation;
}

#! DPQL signature help result
/** @since DataProvider 3.4
*/
public hashdecl DpqlSignatureHelp {
    #! Function/operator label (e.g. "ends-with(String, Suffix, Ignore Case?)")
    string label;
    #! Function/operator documentation
    *string documentation;
    #! Parameter information
    list<hash<DpqlParameterInfo>> parameters;
    #! Active parameter index (0-based)
    int active_parameter;
}

#! Expression operators rendered with DPQL prefix-operator syntax
const list<auto> DpqlPrefixExpressionOperators = (
    DP_OP_MAP,
    DP_OP_HASH_MAP,
    DP_OP_SELECT,
    DP_OP_FIRST,
    DP_OP_ANY,
    DP_OP_ALL,
    DP_OP_COUNT,
    DP_OP_TAKE,
    DP_OP_DROP,
    DP_OP_TAKEWHILE,
    DP_OP_TAKEUNTIL,
    DP_OP_EXISTS,
);

#! DPQL Completion context structure
/** Contains context information for generating completions.
*/
public hashdecl DpqlCompletionContext {
    #! The input text
    string text;
    #! Cursor position (0-based character offset)
    int position;
    #! Available fields
    hash<string, AbstractDataField> fields;
    #! Available expressions
    *hash<string, hash<DataProviderExpressionInfo>> expressions;
    #! Optional template context data for filtering (e.g., sandboxed, interface_type)
    *hash<auto> template_context;
    #! Provider-specific (server-supported) expressions for search context filtering
    /** When set, completions are filtered to enforce search expression rules:
        - At top level: only server operators and field references
        - After field reference: only server comparison operators
        - Inside local-only subtree: no field references

        @since DataProvider 3.4
    */
    *hash<string, hash<DataProviderExpressionInfo>> server_expressions;
}

#! Completion context types
const string DPQL_CTX_START = "start";              # Start of expression
const string DPQL_CTX_AFTER_FIELD = "after_field";  # After a field reference
const string DPQL_CTX_AFTER_OP = "after_op";        # After an operator
const string DPQL_CTX_AFTER_VALUE = "after_value";  # After a value
const string DPQL_CTX_IN_FIELD = "in_field";        # Inside a field reference (after @)
const string DPQL_CTX_IN_STRING = "in_string";      # Inside a string
const string DPQL_CTX_IN_PAREN = "in_paren";        # Inside parentheses
const string DPQL_CTX_IN_INDEX = "in_index";        # Inside index accessor [...]
const string DPQL_CTX_IN_KEY = "in_key";            # Inside key accessor {...}
const string DPQL_CTX_AFTER_ACCESSOR = "after_acc"; # After ] or } - can add more accessors
const string DPQL_CTX_IN_TEMPLATE = "in_template"; # Inside a template reference (after $)
const string DPQL_CTX_AFTER_EXISTS = "after_exists"; # After exists, which takes only a field reference
const string DPQL_CTX_IN_MEMBER = "in_member";      # A member after the dot that follows an accessor

#! Completion provider for DPQL expressions
/** This class provides context-aware code completion for DPQL query expressions.
    It analyzes the cursor position and surrounding text to suggest relevant
    completions.

    @par Example:
    @code{.py}
hash<string, AbstractDataField> fields = {
    "name": new QoreDataField("name", "Name", new QoreStringDataType()),
    "age": new QoreDataField("age", "Age", new QoreIntDataType()),
};

DpqlCompletionProvider provider();
list<hash<DpqlCompletionItem>> items = provider.getCompletions('@na', 3, fields);
# Returns: [{"label": "name", "kind": "field", "insert_text": "name", ...}]
    @endcode
*/
public class DpqlCompletionProvider {
    private {
        #! Available expressions
        *hash<string, hash<DataProviderExpressionInfo>> expressions;
        #! Map of expression name to DPQL symbol (stripped of trailing "()")
        /** @since DataProvider 3.4
        */
        hash<string, string> name_to_symbol;

        #! Comparison operators with descriptions
        const list<auto> ComparisonOperators = (
            {"label": "==", "kind": "operator", "insert_text": "== ",
                "documentation": "Equals comparison", "sort_priority": 10},
            {"label": "!=", "kind": "operator", "insert_text": "!= ",
                "documentation": "Not equals comparison", "sort_priority": 11},
            {"label": "<", "kind": "operator", "insert_text": "< ",
                "documentation": "Less than comparison", "sort_priority": 12},
            {"label": "<=", "kind": "operator", "insert_text": "<= ",
                "documentation": "Less than or equals comparison", "sort_priority": 13},
            {"label": ">", "kind": "operator", "insert_text": "> ",
                "documentation": "Greater than comparison", "sort_priority": 14},
            {"label": ">=", "kind": "operator", "insert_text": ">= ",
                "documentation": "Greater than or equals comparison", "sort_priority": 15},
            {"label": "between", "kind": "keyword", "insert_text": "between ",
                "documentation": "Range comparison: `@field between lower_bound and upper_bound`",
                "sort_priority": 20},
            {"label": "in", "kind": "keyword", "insert_text": "in (",
                "documentation": "Collection membership: in (val1, val2, ...)", "sort_priority": 21},
            {"label": "=~", "kind": "snippet", "insert_text": "=~ /",
                "snippet_text": "=~ /${1:pattern}/$0",
                "documentation": "Regex pattern match", "sort_priority": 22},
        );

        #! Logical operators with descriptions
        const list<auto> LogicalOperators = (
            {"label": "&&", "kind": "operator", "insert_text": "&& ",
                "documentation": "Logical AND", "sort_priority": 30},
            {"label": "||", "kind": "operator", "insert_text": "|| ",
                "documentation": "Logical OR", "sort_priority": 31},
        );

        #! Keyword completions
        const list<auto> Keywords = (
            {"label": "true", "kind": "keyword", "insert_text": "true",
                "documentation": "Boolean true value", "sort_priority": 50},
            {"label": "false", "kind": "keyword", "insert_text": "false",
                "documentation": "Boolean false value", "sort_priority": 51},
            {"label": "null", "kind": "keyword", "insert_text": "null",
                "documentation": "Null/nothing value", "sort_priority": 52},
            {"label": "and", "kind": "keyword", "insert_text": "and ",
                "documentation": "Used in `@field between lower_bound and upper_bound`",
                "sort_priority": 53},
        );

        #! Accessor completions
        const list<auto> AccessorCompletions = (
            {"label": "[", "kind": "operator", "insert_text": "[",
                "documentation": "Index accessor: [0], [-1], [0..3]", "sort_priority": 5},
            {"label": "{", "kind": "operator", "insert_text": "{",
                "documentation": "Key accessor: {key}, {a, b, c}", "sort_priority": 6},
            {"label": ".", "kind": "operator", "insert_text": ".",
                "documentation": "Member access", "sort_priority": 7},
        );

        #! Wildcard accessor completions, offered by the type of the value before the cursor
        /** @since DataProvider 3.10
        */
        const ListWildcardCompletion = {"label": "[*]", "kind": "operator", "insert_text": "[*]",
            "documentation": "Each item of the list: @orders[*].id is the id of each order (map @id, @orders)",
            "sort_priority": 8};
        const HashWildcardCompletion = {"label": "{*}", "kind": "operator", "insert_text": "{*}",
            "documentation": "Each value of the hash, in key order: @prices{*} is map @value, @prices",
            "sort_priority": 9};

        #! The wildcards offered inside an accessor that has been opened, after a list (\c [) or a hash (\c {)
        /** @since DataProvider 3.11
        */
        const ListWildcardInAccessor = {"label": "*", "kind": "operator", "insert_text": "*",
            "documentation": "Each item of the list: @orders[*].id is the id of each order (map @id, @orders)",
            "sort_priority": 0};
        const HashWildcardInAccessor = {"label": "*", "kind": "operator", "insert_text": "*",
            "documentation": "Each value of the hash, in key order: @prices{*} is map @value, @prices",
            "sort_priority": 0};

        #! The detail of a template context that needs a value after its \c ':'
        /** @since DataProvider 3.11
        */
        const TemplateNeedsValueDetail = "needs a value after ':'";

        #! Well-known template contexts with descriptions
        const list<auto> DefaultTemplateContexts = (
            {"label": "$static:", "kind": "template", "insert_text": "$static:",
                "documentation": "Static context values", "sort_priority": 1},
            {"label": "$dynamic:", "kind": "template", "insert_text": "$dynamic:",
                "documentation": "Dynamic runtime values", "sort_priority": 2},
            {"label": "$config:", "kind": "template", "insert_text": "$config:",
                "documentation": "Configuration values", "sort_priority": 3},
            {"label": "$var:", "kind": "template", "insert_text": "$var:",
                "documentation": "Variable values", "sort_priority": 4},
            {"label": "$transient:", "kind": "template", "insert_text": "$transient:",
                "documentation": "Transient context values", "sort_priority": 5},
            {"label": "$env:", "kind": "template", "insert_text": "$env:",
                "documentation": "Environment variables", "sort_priority": 6},
            {"label": "$timestamp:", "kind": "template", "insert_text": "$timestamp:",
                "documentation": "Timestamp values", "sort_priority": 7},
            {"label": "$qore-expr:", "kind": "template", "insert_text": "$qore-expr:{",
                "documentation": "Qore expression evaluation", "sort_priority": 8},
            {"label": "$rest:", "kind": "template", "insert_text": "$rest:",
                "documentation": "REST context values", "sort_priority": 9},
        );

        #! Index accessor suggestions
        const list<auto> IndexSuggestions = (
            {"label": "0", "kind": "value", "insert_text": "0",
                "documentation": "First element", "sort_priority": 1},
            {"label": "-1", "kind": "value", "insert_text": "-1",
                "documentation": "Last element", "sort_priority": 2},
            {"label": "..", "kind": "operator", "insert_text": "..",
                "documentation": "Range operator (inclusive)", "sort_priority": 3},
        );
    }

    #! Creates a completion provider with optional expression map
    /** @param expressions optional map of expression names to their info
    */
    constructor(*hash<string, hash<DataProviderExpressionInfo>> expressions) {
        self.expressions = expressions;
        if (expressions) {
            buildSymbolMaps();
        }
    }

    #! Builds name-to-symbol and symbol-to-name lookup maps from the expression map
    private buildSymbolMaps() {
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
            name_to_symbol{ename} = sym;
        }
    }

    #! Gets completions at the given position
    /** @param text the DPQL expression text
        @param position cursor position (0-based character offset)
        @param fields available fields for completion
        @param template_context optional template context data for filtering
        @return a list of completion items
    */
    list<hash<DpqlCompletionItem>> getCompletions(string text, int position,
            hash<string, AbstractDataField> fields, *hash<auto> template_context) {
        # Normalize to UTF-8 once at the public boundary; the private scan
        # paths use byte-indexed access and require ASCII-compatible bytes
        # (UTF-8 / ASCII / Latin-1).  No-op if already UTF-8.
        if (text.encoding() != "UTF-8") {
            text = convert_encoding(text, "UTF-8");
        }
        return getCompletionsWithContext(<DpqlCompletionContext>{
            "text": text,
            "position": position,
            "fields": fields,
            "expressions": expressions,
            "template_context": template_context,
        });
    }

    #! Gets completions with full context
    /** @param ctx the completion context
        @return a list of completion items
    */
    list<hash<DpqlCompletionItem>> getCompletionsWithContext(hash<DpqlCompletionContext> ctx) {
        # Analyze context to determine what completions are appropriate
        hash<auto> analysis = analyzeContext(ctx.text, ctx.position);
        bool in_search = ctx.server_expressions ? True : False;

        list<hash<DpqlCompletionItem>> items = ();

        switch (analysis.ctx) {
            case DPQL_CTX_START:
            case DPQL_CTX_AFTER_OP:
            case DPQL_CTX_IN_PAREN:
                # After =~, only offer regex pattern completion
                if (analysis.operator == "=~") {
                    if (!analysis.prefix || index("/", analysis.prefix) == 0) {
                        items += <DpqlCompletionItem>{
                            "label": "/pattern/",
                            "kind": "snippet",
                            "insert_text": "/",
                            "snippet_text": "/${1:pattern}/$0",
                            "documentation": "Regular expression pattern; use `i` flag after closing `/` "
                                "for case-insensitive matching (e.g. `/hello/i`)",
                            "sort_priority": 1,
                        };
                    }
                    break;
                }
                # Resolve function argument context for type-aware completions
                *hash<auto> arg_ctx = getArgContext(analysis, ctx.expressions);

                # If inside a function arg with allowed_values, offer only those
                if (arg_ctx.allowed_values) {
                    items += getAllowedValueCompletions(arg_ctx.allowed_values,
                        arg_ctx.expected_type_detail, analysis.prefix);
                    break;
                }

                # Filter completions based on argument type constraints
                if (arg_ctx.type_code != ST_Value) {
                    # Field refs suppressed for ST_Value (only literal values expected)
                    items += getFieldCompletions(ctx.fields, analysis.prefix);
                }
                if (arg_ctx.type_code != ST_Field) {
                    # Values suppressed for ST_Field (only field references expected)
                    items += getValueCompletions(analysis.prefix);
                }
                if (arg_ctx.type_code != ST_Field && arg_ctx.type_code != ST_Value) {
                    # Functions suppressed for both ST_Field and ST_Value constraints
                    if (in_search) {
                        items += getFunctionCompletions(analysis.prefix,
                            ctx.server_expressions);
                        items += getPrefixExpressionOperatorCompletions(analysis.prefix,
                            ctx.server_expressions);
                    } else {
                        items += getFunctionCompletions(analysis.prefix);
                        items += getPrefixExpressionOperatorCompletions(analysis.prefix);
                    }
                }
                items += getTemplateCompletions(analysis.prefix, ctx.template_context);
                if (!analysis.prefix || index("!", analysis.prefix) == 0) {
                    items += <DpqlCompletionItem>{
                        "label": "!",
                        "kind": "operator",
                        "insert_text": "!",
                        "documentation": "Logical NOT",
                        "sort_priority": 5,
                    };
                }

                # Annotate items with expected type when inside a typed argument
                if (arg_ctx.expected_type_detail) {
                    items = annotateExpectedType(items, arg_ctx.expected_type_detail);
                }
                break;

            case DPQL_CTX_IN_TEMPLATE:
                # Completing template context or value after $
                items += getTemplateCompletions(analysis.prefix, ctx.template_context);
                break;

            case DPQL_CTX_IN_FIELD:
                # Completing field name after @, or a key of a field in the path typed so far; in an open quoted
                # field reference, the field name is the key that the path typed so far is
                items += getFieldCompletions(ctx.fields, analysis.field_prefix ?? analysis.prefix);
                items += getFieldPathCompletions(ctx.fields, analysis.prefix);
                break;

            case DPQL_CTX_AFTER_EXISTS:
                # exists takes only a field reference
                items += getFieldCompletions(ctx.fields, analysis.prefix);
                items += getFieldPathCompletions(ctx.fields, analysis.prefix);
                break;

            case DPQL_CTX_IN_MEMBER:
                # a key of the value that the accessors before the dot give
                items += getMemberCompletions(ctx.fields, analysis.base_text, analysis.prefix);
                break;

            case DPQL_CTX_AFTER_FIELD:
                # After field, suggest accessors and comparison operators
                items += getAccessorCompletions(analysis.prefix);
                items += getWildcardCompletions(substr(ctx.text, 0, ctx.position), ctx.fields, analysis.prefix);
                if (in_search) {
                    # In search context after a field ref: only server-supported comparison ops
                    items += getServerOperatorCompletions(analysis.prefix, ctx.server_expressions);
                    items += getInfixComparisonCompletions(analysis.prefix, ctx.server_expressions);
                } else {
                    items += getOperatorCompletions(analysis.prefix, True);
                    items += getInfixComparisonCompletions(analysis.prefix);
                }
                break;

            case DPQL_CTX_AFTER_VALUE:
                # After value, suggest logical operators or more comparisons
                items += getOperatorCompletions(analysis.prefix, False);
                break;

            case DPQL_CTX_IN_STRING:
                # Inside string - no completions
                break;

            case DPQL_CTX_IN_INDEX: {
                # Inside index accessor - suggest the wildcard for a list, integers and the range operator
                hash<string, bool> kinds = getWildcardKinds(substr(ctx.text, 0, ctx.position), ctx.fields);
                # only a list type that is known: a type that is not known (auto) also accepts lists
                if (kinds.known && kinds.list
                        && (!expressions || expressions{DP_FUNC_LIST_WILDCARD})
                        && (!analysis.prefix || index(ListWildcardInAccessor.label, analysis.prefix) == 0)) {
                    items += cast<hash<DpqlCompletionItem>>(ListWildcardInAccessor);
                }
                items += getIndexCompletions(analysis.prefix);
                break;
            }

            case DPQL_CTX_IN_KEY:
                # Inside key accessor - suggest the wildcard for a hash, and keys from schema if available
                if (analysis.field_path) {
                    *AbstractDataField base_field = ctx.fields{analysis.field_path.field_name};
                    if (base_field) {
                        *AbstractDataProviderType target_type = traverseTypeWithAccessors(
                            base_field.getType(), analysis.field_path.accessors);
                        if (target_type) {
                            # only a hash type that is known; not after a slice, which gives a list
                            hash<string, bool> kinds = getWildcardKinds(substr(ctx.text, 0, ctx.position),
                                ctx.fields);
                            if (kinds.known && kinds.hash
                                    && (!expressions || expressions{DP_FUNC_HASH_WILDCARD})
                                    && (!analysis.prefix
                                        || index(HashWildcardInAccessor.label, analysis.prefix) == 0)) {
                                items += cast<hash<DpqlCompletionItem>>(HashWildcardInAccessor);
                            }
                            items += getKeyCompletions(target_type, analysis.prefix);
                        }
                    }
                }
                break;

            case DPQL_CTX_AFTER_ACCESSOR:
                # After ] or } - can add more accessors or operators
                items += getAccessorCompletions(analysis.prefix);
                items += getWildcardCompletions(substr(ctx.text, 0, ctx.position), ctx.fields, analysis.prefix);
                items += getOperatorCompletions(analysis.prefix, True);
                break;
        }

        # Sort by priority
        items = sort(items, int sub (hash<DpqlCompletionItem> a, hash<DpqlCompletionItem> b) {
            return a.sort_priority <=> b.sort_priority;
        });

        return items;
    }

    #! Analyzes the context at the given position
    private hash<auto> analyzeContext(string text, int position) {
        # Ensure position is within bounds
        if (position > text.size()) {
            position = text.size();
        }

        string before = position > 0 ? substr(text, 0, position) : "";
        string prefix = "";

        # Check if we're inside a string; an open quoted field reference (<tt>\@"create.it</tt>) completes the path
        # typed in it, and an open quoted member after an accessor (<tt>\@orders[*].\@"Unit.</tt>) the key typed in it
        int string_start = getOpenStringStart(before);
        if (string_start >= 0) {
            return analyzeOpenQuotedReference(before, string_start) ?? {
                "ctx": DPQL_CTX_IN_STRING,
                "prefix": "",
            };
        }

        # Find the start of the current token
        # Include brackets and braces as token delimiters
        int token_start = position;
        while (token_start > 0 && before[token_start - 1] !~ /[\s(),\[\]{}]/) {
            --token_start;
        }

        if (token_start < position) {
            prefix = substr(before, token_start);
        }

        # A member after the dot that follows an accessor or a quoted field name (ex: "@orders[*].items[0].na",
        # "@orders[*].@\"Unit.Price\".", "@\"my field\".na"): the keys of the value before the last dot; the members
        # typed between the accessor and it are part of that value.  The value is read from the text before the dot,
        # not from the current token, as a quoted name can have spaces and brackets
        if (before =~ /\.[\p{L}\p{M}\p{N}_\-]*$/) {
            # the part of the member typed after the dot; an empty capture gives no list
            string partial = (before =~ x/\.([\p{L}\p{M}\p{N}_\-]+)$/)[0] ?? "";
            string base_text = substr(before, 0, before.length() - partial.length() - 1);
            *hash<auto> chain = extractFieldPath(base_text);
            # after a bare field token member (<tt>.\@name</tt>), a dot is part of the token's key
            if (chain.member && chain.end == base_text.size() && base_text !~ /\.@[\p{L}\p{M}\p{N}_.]*$/) {
                return {
                    "ctx": DPQL_CTX_IN_MEMBER,
                    "prefix": partial,
                    "base_text": base_text,
                };
            }
        }

        # Check if we're in a field reference (after @)
        if (prefix =~ /^@/) {
            return {
                "ctx": DPQL_CTX_IN_FIELD,
                "prefix": prefix.size() > 1 ? substr(prefix, 1) : "",
            };
        }

        # Check if we're in a template reference (after $)
        if (prefix =~ /^\$/) {
            return {
                "ctx": DPQL_CTX_IN_TEMPLATE,
                "prefix": prefix,
            };
        }

        # Look at what came before (ignoring whitespace)
        string trimmed = rtrim(before);
        if (!trimmed) {
            return {
                "ctx": DPQL_CTX_START,
                "prefix": prefix,
            };
        }

        # After exists, or exists and a parenthesis, only its field reference can follow
        if (rtrim(substr(before, 0, token_start)) =~ /(?:^|[^\p{L}\p{M}\p{N}_@.$"'-])exists(?:\s*\()?$/i
                && token_start > 0 && before[token_start - 1] =~ /[\s(]/) {
            return {
                "ctx": DPQL_CTX_AFTER_EXISTS,
                "prefix": prefix,
            };
        }

        # Check the last non-whitespace character/token
        *string last_char = trimmed.size() > 0 ? trimmed[trimmed.size() - 1] : NOTHING;

        # Check if we're inside an index accessor [...]
        if (checkInBrackets(before)) {
            return {
                "ctx": DPQL_CTX_IN_INDEX,
                "prefix": prefix,
            };
        }

        # Check if we're inside a key accessor {...} (after field ref context)
        if (checkInBraces(before)) {
            *hash<auto> field_path = extractFieldPath(before);
            return {
                "ctx": DPQL_CTX_IN_KEY,
                "prefix": prefix,
                "field_path": field_path,
            };
        }

        # After closing bracket or brace - can add more accessors
        if (last_char == "]" || last_char == "}") {
            return {
                "ctx": DPQL_CTX_AFTER_ACCESSOR,
                "prefix": prefix,
            };
        }

        # After opening paren — detect function call context
        if (last_char == "(") {
            *hash<auto> func_ctx = findEnclosingFunctionContext(before);
            return {
                "ctx": DPQL_CTX_IN_PAREN,
                "prefix": prefix,
            } + (func_ctx ?? {});
        }

        # After comma — detect function argument position
        if (last_char == ",") {
            *hash<auto> func_ctx = findEnclosingFunctionContext(before);
            return {
                "ctx": DPQL_CTX_AFTER_OP,
                "prefix": prefix,
            } + (func_ctx ?? {});
        }

        # Check if last token was a field reference
        if (trimmed =~ /@[\p{L}\p{M}\p{N}_."]+\s*$/) {
            return {
                "ctx": DPQL_CTX_AFTER_FIELD,
                "prefix": prefix,
            };
        }

        # A member name after accessors, followed by whitespace (ex: "@orders[*].items "): more accessors or
        # operators can follow, as after ] or }
        if (!prefix && trimmed.size() < before.size() && trimmed =~ /[\]\}]\.[\p{L}\p{M}\p{N}_.\-]+$/) {
            *hash<auto> chain = extractFieldPath(trimmed);
            if (chain && chain.end == trimmed.size() && chain.accessors) {
                return {
                    "ctx": DPQL_CTX_AFTER_ACCESSOR,
                    "prefix": prefix,
                };
            }
        }

        # Check if last token was an operator
        *list<*string> op_match = (trimmed =~ x/(==|!=|<=|>=|<|>|&&|\|\||=~|between|in|and)\s*$/);
        if (op_match) {
            return {
                "ctx": DPQL_CTX_AFTER_OP,
                "prefix": prefix,
                "operator": op_match[0],
            };
        }

        # Check if last token was a value (string, number, boolean, null)
        if (trimmed =~ /("[^"]*"|'[^']*'|-?[0-9]+\.?[0-9]*|true|false|null|\))\s*$/) {
            return {
                "ctx": DPQL_CTX_AFTER_VALUE,
                "prefix": prefix,
            };
        }

        # Default to start context — also check for enclosing function call
        *hash<auto> func_ctx = findEnclosingFunctionContext(before);
        return {
            "ctx": DPQL_CTX_START,
            "prefix": prefix,
        } + (func_ctx ?? {});
    }

    #! Analyzes the text before the cursor when it ends in an open quoted field reference or member
    /** In an open quoted field reference (<tt>\@"create.it</tt>), the path typed in the quotes is read as the parser
        reads it: dots separate its keys, an escaped dot is part of a key, and the other escapes are decoded.  In an
        open quoted member after the dot that follows an accessor or a quoted field name
        (<tt>\@orders[*].\@"Unit.</tt>), the text typed in the quotes is one key, dots included.

        @param before the text before the cursor
        @param quote the byte offset of the quote that opens the string that \a before ends in

        @return the context: \c DPQL_CTX_IN_FIELD with the path typed as \c prefix and the field name that it would
        be as \c field_prefix, or \c DPQL_CTX_IN_MEMBER with the key typed as \c prefix and the text before the dot
        as \c base_text; @ref nothing if the string is not a field reference or a member

        @since DataProvider 3.11
    */
    private *hash<auto> analyzeOpenQuotedReference(string before, int quote) {
        if (quote < 1 || chr(before.getByte(quote)) != '"' || chr(before.getByte(quote - 1)) != "@") {
            return;
        }
        string typed = byteSubstr(before, quote + 1, before.size() - quote - 1);
        # a member after a dot, which is not one of the dots of a range (<tt>[0..\@"n</tt>)
        int at = quote - 1;
        if (at > 0 && chr(before.getByte(at - 1)) == "." && (at == 1 || chr(before.getByte(at - 2)) != ".")) {
            string base_text = byteSubstr(before, 0, at - 1);
            *hash<auto> chain = extractFieldPath(base_text);
            if (!chain.member || chain.end != base_text.size()) {
                return;
            }
            return {
                "ctx": DPQL_CTX_IN_MEMBER,
                "prefix": unescapeString(typed),
                "base_text": base_text,
            };
        }
        # an escaped dot stays escaped in the path, as it is part of a key
        string path = unescapeFieldPath(typed);
        list<string> parts = AbstractDataProvider::getFieldPath(path);
        return {
            "ctx": DPQL_CTX_IN_FIELD,
            "prefix": path,
            # one key, or the whole text for a field whose name has a dot, as after a bare @
            "field_prefix": parts.size() == 1 ? parts[0] : path,
        };
    }

    #! Finds the enclosing function call context from text before cursor
    /** Scans backwards from the end of \a text to find the enclosing open parenthesis,
        counting commas to determine the argument position, and then extracts the
        function name preceding the paren.

        @param text the text up to (and including) the cursor position
        @return a hash with \c func_name (string), \c arg_index (int, 0-based),
            and \c func_expr_name (string, internal expression name), or \c NOTHING
            if the cursor is not inside a function call

        @since DataProvider 3.4
    */
    private *hash<auto> findEnclosingFunctionContext(string text) {
        # Scan backwards to find matching open paren, counting commas
        int depth = 0;
        int comma_count = 0;
        int paren_pos = -1;

        for (int i = text.size() - 1; i >= 0; --i) {
            string c = chr(text.getByte(i));
            if (c == ")") {
                ++depth;
            } else if (c == "(") {
                if (depth > 0) {
                    --depth;
                } else {
                    paren_pos = i;
                    break;
                }
            } else if (c == "," && depth == 0) {
                ++comma_count;
            }
        }

        if (paren_pos < 0) {
            return NOTHING;
        }

        # Extract function name before the paren
        int name_start = paren_pos - 1;
        while (name_start >= 0 && chr(text.getByte(name_start)) =~ /\s/) {
            --name_start;
        }
        if (name_start < 0) {
            return NOTHING;
        }
        int id_end = name_start + 1;
        while (name_start >= 0) {
            string sc = chr(text.getByte(name_start));
            if (sc =~ /[a-zA-Z0-9_]/) {
                --name_start;
            } else if (sc == "-" && name_start + 1 < id_end && chr(text.getByte(name_start + 1)) =~ /[a-zA-Z]/) {
                --name_start;
            } else {
                break;
            }
        }
        ++name_start;
        if (name_start >= id_end) {
            return NOTHING;
        }

        string func_name = text.substr(name_start, id_end - name_start);

        # Resolve to internal expression name
        *string resolved_name;
        foreach string ename in (keys name_to_symbol) {
            if (name_to_symbol{ename} == func_name) {
                resolved_name = ename;
                break;
            }
        }
        if (!resolved_name && expressions{func_name}) {
            resolved_name = func_name;
        }
        if (!resolved_name) {
            return NOTHING;
        }

        return {
            "func_name": func_name,
            "func_expr_name": resolved_name,
            "arg_index": comma_count,
        };
    }

    #! Checks if position is inside brackets [...] (for index accessor)
    private bool checkInBrackets(string text) {
        int depth = 0;
        bool in_string = False;
        bool escaped = False;
        string quote_char;

        for (int i = 0; i < text.size(); ++i) {
            string c = chr(text.getByte(i));
            if (escaped) {
                escaped = False;
                continue;
            }
            if (in_string) {
                if (c == "\\") {
                    escaped = True;
                } else if (c == quote_char) {
                    in_string = False;
                }
                continue;
            }
            if (c == '"' || c == "'") {
                in_string = True;
                quote_char = c;
                continue;
            }
            if (c == "[") {
                ++depth;
            } else if (c == "]") {
                --depth;
            }
        }
        return depth > 0;
    }

    #! Checks if position is inside braces {...} (for key accessor)
    private bool checkInBraces(string text) {
        int depth = 0;
        bool in_string = False;
        bool escaped = False;
        string quote_char;
        # Only count braces that appear after @ (field context)
        bool in_field_context = False;

        for (int i = 0; i < text.size(); ++i) {
            string c = chr(text.getByte(i));
            if (escaped) {
                escaped = False;
                continue;
            }
            if (in_string) {
                if (c == "\\") {
                    escaped = True;
                } else if (c == quote_char) {
                    in_string = False;
                }
                continue;
            }
            if (c == '"' || c == "'") {
                in_string = True;
                quote_char = c;
                continue;
            }
            if (c == "@") {
                in_field_context = True;
            } else if (c == "{" && in_field_context) {
                ++depth;
            } else if (c == "}" && depth > 0) {
                --depth;
            } else if (c =~ /[=<>!&|]/) {
                # Operators end field context
                in_field_context = False;
            }
        }
        return depth > 0;
    }

    #! Checks if position is inside a string literal
    private int checkInString(string text) {
        return getOpenStringStart(text) >= 0 ? 1 : 0;
    }

    #! Returns the byte offset of the quote that opens the string that the text ends in, or -1 if it ends in none
    /** A backslash escapes the next character, and a quote of one kind is part of a string in quotes of the other

        @param text the text before the cursor

        @return the byte offset of the opening quote of the string that is not closed, or -1

        @since DataProvider 3.11
    */
    private int getOpenStringStart(string text) {
        int start = -1;
        string quote_char;
        bool escaped = False;
        for (int i = 0; i < text.size(); ++i) {
            string c = chr(text.getByte(i));
            if (escaped) {
                escaped = False;
                continue;
            }
            if (c == "\\") {
                escaped = True;
                continue;
            }
            if (start >= 0) {
                if (c == quote_char) {
                    start = -1;
                }
            } else if (c == '"' || c == "'") {
                start = i;
                quote_char = c;
            }
        }
        return start;
    }

    #! Gets field completions
    private list<hash<DpqlCompletionItem>> getFieldCompletions(
            hash<string, AbstractDataField> fields, string prefix) {
        list<hash<DpqlCompletionItem>> items = ();
        string lwr_prefix = prefix.lwr();

        foreach string field_name in (keys fields) {
            if (!prefix || index(field_name.lwr(), lwr_prefix) == 0) {
                AbstractDataField field = fields{field_name};
                *hash<DataFieldInfo> info = field.getInfo();

                # a field of the record is one key: a name with a dot is quoted, with the dot escaped, as a dot in a
                # field reference separates the parts of a path
                string insert_text = DpqlSerializer::fieldReferenceForKey(field_name);

                items += <DpqlCompletionItem>{
                    "label": field_name,
                    "kind": "field",
                    "insert_text": insert_text,
                    "detail": getTypeDetail(field.getType()),
                    "documentation": info.short_desc ?? info.desc,
                    "sort_priority": 1,
                };
            }
        }

        return items;
    }

    #! Gets the keys of a field of a field path typed after \c @
    /** The text typed after \c @ is read as a field path (@ref AbstractDataProvider::getFieldPath()): the keys
        before its last dot are resolved through the field types (the field, then the field of its hash type for each
        key), and the keys of the type they reach that start with the last part are offered.  A type without known
        fields, or a key that the types do not have, offers nothing.

        Each item inserts the field reference to the path, as the serializer writes it
        (@ref DpqlSerializer::fieldReferenceForPath()): \c @create.items, <tt>\@"create.order-id"</tt>.

        @param fields the fields of the record
        @param prefix the text typed after \c @

        @return the key completion items

        @since DataProvider 3.11
    */
    private list<hash<DpqlCompletionItem>> getFieldPathCompletions(hash<string, AbstractDataField> fields,
            string prefix) {
        list<hash<DpqlCompletionItem>> items = ();
        list<string> parts = AbstractDataProvider::getFieldPath(prefix);
        if (parts.size() < 2) {
            return items;
        }
        string partial = pop parts;
        *AbstractDataField field = fields{parts[0]};
        if (!field) {
            return items;
        }
        AbstractDataProviderType type = field.getType();
        for (int i = 1; i < parts.size(); ++i) {
            *AbstractDataField key_field = type.getField(parts[i]);
            if (!key_field) {
                return items;
            }
            type = key_field.getType();
        }
        *hash<string, AbstractDataField> type_fields = type.getFields();
        if (!type_fields) {
            return items;
        }
        string lwr_partial = partial.lwr();
        foreach hash<auto> i in (type_fields.pairIterator()) {
            if (partial && index(i.key.lwr(), lwr_partial) != 0) {
                continue;
            }
            AbstractDataField key_field = i.value;
            *hash<DataFieldInfo> info = key_field.getInfo();
            items += <DpqlCompletionItem>{
                "label": i.key,
                "kind": "field",
                "insert_text": DpqlSerializer::fieldReferenceForPath(parts + (i.key,)),
                "detail": getTypeDetail(key_field.getType()),
                "documentation": info.short_desc ?? info.desc,
                "sort_priority": 1,
            };
        }
        return items;
    }

    #! Gets the keys of the value that the accessors before a dot give, for the member after the dot
    /** @param fields the fields of the record
        @param base_text the text before the dot: a field reference followed by accessors, or a quoted field
        reference, after which a dot is followed by a member (<tt>\@"my field".name</tt>)
        @param prefix the part of the member typed after the dot

        @return the key completion items; each inserts the member as the parser reads it after the dot
        (@ref DpqlSerializer::memberNameForKey()); nothing when the type of the value has no known fields; the keys of
        a list are the keys of its items, as a member of a list is the key of each item

        @since DataProvider 3.11
    */
    private list<hash<DpqlCompletionItem>> getMemberCompletions(hash<string, AbstractDataField> fields,
            string base_text, string prefix) {
        list<hash<DpqlCompletionItem>> items = ();
        *hash<auto> chain = extractFieldPath(base_text);
        if (!chain.member || chain.end != base_text.size()) {
            return items;
        }
        *AbstractDataField field = fields{chain.field_name};
        if (!field) {
            return items;
        }
        *AbstractDataProviderType type = traverseTypeWithAccessors(field.getType(), chain.accessors);
        # a member of a list is the key of each of its items
        int depth;
        *hash<string, AbstractDataField> type_fields = type ? getKeyedType(type, \depth).getFields() : NOTHING;
        if (!type_fields) {
            return items;
        }
        string lwr_prefix = prefix.lwr();
        foreach hash<auto> i in (type_fields.pairIterator()) {
            if (prefix && index(i.key.lwr(), lwr_prefix) != 0) {
                continue;
            }
            AbstractDataField key_field = i.value;
            *hash<DataFieldInfo> info = key_field.getInfo();
            items += <DpqlCompletionItem>{
                "label": i.key,
                "kind": "field",
                "insert_text": DpqlSerializer::memberNameForKey(i.key),
                "detail": getTypeDetail(key_field.getType()),
                "documentation": info.short_desc ?? info.desc,
                "sort_priority": 1,
            };
        }
        return items;
    }

    #! Gets operator completions
    private list<hash<DpqlCompletionItem>> getOperatorCompletions(string prefix, bool comparison) {
        list<hash<DpqlCompletionItem>> items = ();
        string lwr_prefix = prefix.lwr();

        if (comparison) {
            foreach hash<auto> op in (ComparisonOperators) {
                if (!prefix || index(op.label.lwr(), lwr_prefix) == 0) {
                    items += cast<hash<DpqlCompletionItem>>(op);
                }
            }
        }

        foreach hash<auto> op in (LogicalOperators) {
            if (!prefix || index(op.label.lwr(), lwr_prefix) == 0) {
                items += cast<hash<DpqlCompletionItem>>(op);
            }
        }

        return items;
    }

    #! Gets value completions (true, false, null)
    private list<hash<DpqlCompletionItem>> getValueCompletions(string prefix) {
        list<hash<DpqlCompletionItem>> items = ();
        string lwr_prefix = prefix.lwr();

        foreach hash<auto> kw in (Keywords) {
            if (!prefix || index(kw.label.lwr(), lwr_prefix) == 0) {
                items += cast<hash<DpqlCompletionItem>>(kw);
            }
        }

        return items;
    }

    #! Gets operator completions filtered to server-supported operators only (search context)
    /** @param prefix the current prefix to filter by
        @param server_expressions the provider-specific expression map

        @since DataProvider 3.4
    */
    private list<hash<DpqlCompletionItem>> getServerOperatorCompletions(string prefix,
            hash<string, hash<DataProviderExpressionInfo>> server_expressions) {
        list<hash<DpqlCompletionItem>> items = ();
        string lwr_prefix = prefix.lwr();

        foreach hash<auto> op in (ComparisonOperators) {
            # Map operator label to expression name for server check
            *string exp_name = getExpNameForOp(op.label);
            if (exp_name && !server_expressions{exp_name}) {
                continue;
            }
            if (!prefix || index(op.label.lwr(), lwr_prefix) == 0) {
                hash<DpqlCompletionItem> item = cast<hash<DpqlCompletionItem>>(op);
                item.documentation = (item.documentation ?? "") + " (server operator)";
                items += item;
            }
        }

        # Always include logical operators
        foreach hash<auto> op in (LogicalOperators) {
            if (!prefix || index(op.label.lwr(), lwr_prefix) == 0) {
                items += cast<hash<DpqlCompletionItem>>(op);
            }
        }

        return items;
    }

    #! Maps an operator label to its expression name constant
    private *string getExpNameForOp(string label) {
        switch (label) {
            case "==": return DP_SEARCH_OP_EQ;
            case "!=": return DP_SEARCH_OP_NE;
            case "<": return DP_SEARCH_OP_LT;
            case "<=": return DP_SEARCH_OP_LE;
            case ">": return DP_SEARCH_OP_GT;
            case ">=": return DP_SEARCH_OP_GE;
            case "between": return DP_SEARCH_OP_BETWEEN;
            case "in": return DP_SEARCH_OP_IN;
            case "=~": return DP_OP_REGEX_MATCH;
            case "!~": return DP_OP_REGEX_NMATCH;
        }
        return NOTHING;
    }

    #! Gets infix comparison operators from the expression map (e.g. "like")
    /** Returns completions for provider-specific operators that have \c groups containing
        "Comparison", \c type = DET_Operator, and exactly 2 args. These are rendered as
        infix operators in DPQL (e.g. \c \@field \c like \c "pattern") rather than function
        calls.  @ref DataProvider::DataProviderExpressionBaseInfo::internal "Internal" expressions are not offered.

        @param prefix the current prefix to filter by
        @param filter_expressions optional expression map to filter by (for search context)

        @return a list of completion items for infix comparison operators

        @since DataProvider 3.4
        @since DataProvider 3.11 internal expressions are not offered
    */
    private list<hash<DpqlCompletionItem>> getInfixComparisonCompletions(string prefix,
            *hash<string, hash<DataProviderExpressionInfo>> filter_expressions) {
        if (!expressions) {
            return ();
        }

        # Built-in comparison operators already handled by ComparisonOperators constant
        hash<string, bool> builtin_ops;
        foreach hash<auto> op in (ComparisonOperators) {
            *string exp_name = getExpNameForOp(op.label);
            if (exp_name) {
                builtin_ops{exp_name} = True;
            }
        }

        list<hash<DpqlCompletionItem>> items = ();
        string lwr_prefix = prefix.lwr();

        foreach string ename in (keys expressions) {
            # Skip built-in operators already in ComparisonOperators
            if (builtin_ops{ename}) {
                continue;
            }
            hash<DataProviderExpressionInfo> info = expressions{ename};
            # an internal expression is never offered
            if (info.internal) {
                continue;
            }
            # Only infix comparison operators with exactly 2 args
            if (info.type != DET_Operator || !inlist("Comparison", info.groups)
                    || info.args.size() != 2) {
                continue;
            }
            # If filtering by server expressions, skip if not supported
            if (filter_expressions && !filter_expressions{ename}) {
                continue;
            }
            string sym = name_to_symbol{ename} ?? ename;
            if (!prefix || index(sym.lwr(), lwr_prefix) == 0) {
                items += <DpqlCompletionItem>{
                    "label": sym,
                    "kind": "keyword",
                    "insert_text": sym + " ",
                    "documentation": info.short_desc ?? info.desc,
                    "detail": info.return_type ? getGivesDetail(info.return_type) : NOTHING,
                    "sort_priority": 23,
                };
            }
        }

        return items;
    }

    #! Returns True if the expression uses prefix-operator syntax in DPQL
    private bool isPrefixExpressionOperator(string ename) {
        return inlist(ename, DpqlPrefixExpressionOperators);
    }

    #! Gets prefix expression operator completions
    private list<hash<DpqlCompletionItem>> getPrefixExpressionOperatorCompletions(string prefix,
            *hash<string, hash<DataProviderExpressionInfo>> filter_expressions) {
        if (!expressions) {
            return ();
        }

        list<hash<DpqlCompletionItem>> items = ();
        string lwr_prefix = prefix.lwr();

        foreach string ename in (DpqlPrefixExpressionOperators) {
            if (!expressions{ename}) {
                continue;
            }
            if (filter_expressions && !filter_expressions{ename}) {
                continue;
            }
            hash<DataProviderExpressionInfo> info = expressions{ename};
            if (info.type != DET_Operator) {
                continue;
            }
            string sym = name_to_symbol{ename} ?? ename;
            if (prefix && index(sym.lwr(), lwr_prefix) != 0) {
                continue;
            }

            list<string> placeholders = ();
            for (int i = 0; i < info.args.size(); ++i) {
                string pname;
                if (info.arg_info[i].display_name) {
                    pname = info.arg_info[i].display_name;
                } else {
                    pname = "arg" + string(i);
                }
                placeholders += sprintf("${%d:%s}", i + 1, pname);
            }

            items += <DpqlCompletionItem>{
                "label": sym,
                "kind": "operator",
                "insert_text": sym + " ",
                "snippet_text": sym + " " + placeholders.join(", ") + "$0",
                "documentation": info.short_desc ?? info.desc,
                "detail": info.return_type ? getGivesDetail(info.return_type) : NOTHING,
                "sort_priority": 35,
            };
        }

        return items;
    }

    #! Resolves function argument context from analysis data
    /** Looks up the enclosing function and determines the expected type,
        type constraint code, and allowed values for the current argument position.

        @param analysis the context analysis hash (may contain \c func_expr_name and \c arg_index)
        @param expressions the available expressions map
        @return a hash with \c expected_type_name (string), \c expected_type_detail (string: the detail text that
            says the type, such as \c "this argument takes text"), \c type_code (int), and \c allowed_values (list),
            or an empty hash if not in a function arg context

        @since DataProvider 3.4
    */
    private hash<auto> getArgContext(hash<auto> analysis,
            *hash<string, hash<DataProviderExpressionInfo>> expressions) {
        if (!analysis.func_expr_name || !expressions) {
            return {};
        }
        *hash<DataProviderExpressionInfo> info = expressions{analysis.func_expr_name};
        if (!info) {
            return {};
        }

        int idx = analysis.arg_index;
        # Clamp to last arg for varargs functions
        if (info.varargs && idx >= info.args.size()) {
            idx = info.args.size() - 1;
        }
        if (idx < 0 || idx >= info.args.size()) {
            return {};
        }

        hash<auto> result = {};
        # Type constraint
        hash<DataProviderSignatureTypeInfo> sig = info.args[idx];
        result.type_code = sig.type_code;
        if (sig.type) {
            string tname = sig.type.getName();
            if (tname != "any" && tname != "auto") {
                result.expected_type_name = tname;
                result.expected_type_detail = sprintf("this argument takes %s", getTypeSentenceForm(sig.type));
            }
        }

        # Allowed values from arg_info
        if (info.arg_info && idx < info.arg_info.size()) {
            hash<DataProviderArgInfo> ai = info.arg_info[idx];
            if (ai.allowed_values && !ai.allowed_values_creatable) {
                result.allowed_values = ai.allowed_values;
            }
        }

        return result;
    }

    #! Gets completions for a restricted set of allowed values
    /** @param allowed_values list of allowed value info hashes
        @param expected_type_detail optional detail text that says the type of the argument
        @param prefix the current prefix to filter by
        @return a list of completion items for the allowed values

        @since DataProvider 3.4
    */
    private list<hash<DpqlCompletionItem>> getAllowedValueCompletions(
            list<hash<AllowedValueInfo>> allowed_values, *string expected_type_detail,
            string prefix) {
        list<hash<DpqlCompletionItem>> items = ();
        string lwr_prefix = prefix.lwr();

        foreach hash<AllowedValueInfo> av in (allowed_values) {
            string val_str = string(av.value);
            if (!prefix || index(val_str.lwr(), lwr_prefix) == 0) {
                # Quote string values
                string insert;
                if (av.value.typeCode() == NT_STRING) {
                    insert = '"' + replace(val_str, '"', '\\"') + '"';
                } else {
                    insert = val_str;
                }
                items += <DpqlCompletionItem>{
                    "label": val_str,
                    "kind": "value",
                    "insert_text": insert,
                    "detail": expected_type_detail,
                    "documentation": av.short_desc ?? av.desc,
                    "sort_priority": 10,
                };
            }
        }

        return items;
    }

    #! Annotates completion items with the expected type for the current argument position
    /** Adds what the argument takes to each item's \c detail field, after the detail that the item has, if any:
        <tt>"Whole number; this argument takes text"</tt>

        @param items the list of completion items to annotate
        @param annotation the detail text that says the type of the argument (\c "this argument takes text")
        @return the annotated list

        @since DataProvider 3.4
    */
    private list<hash<DpqlCompletionItem>> annotateExpectedType(
            list<hash<DpqlCompletionItem>> items, string annotation) {
        for (int i = 0; i < items.size(); ++i) {
            if (items[i].detail) {
                items[i].detail += "; " + annotation;
            } else {
                items[i].detail = annotation;
            }
        }
        return items;
    }

    #! Returns the detail text that names a type: its display name in the type catalogue
    /** @param type the type

        @return the type's @ref DataProvider::DataTypeCatalogueInfo::display_name "display_name" in the
        @ref DataProvider::DataTypeCatalogue "DataTypeCatalogue" (\c "Whole number"), or its type name for a type that
        the catalogue does not name

        @since DataProvider 3.11
    */
    private static string getTypeDetail(AbstractDataProviderType type) {
        return DataTypeCatalogue::lookupType(type).display_name ?? type.getName();
    }

    #! Returns how a sentence names a type: its sentence form in the type catalogue
    /** @param type the type

        @return the type's @ref DataProvider::DataTypeCatalogueInfo::sentence_form "sentence_form" in the
        @ref DataProvider::DataTypeCatalogue "DataTypeCatalogue" (\c "a whole number"), or its type name for a type
        that the catalogue does not name

        @since DataProvider 3.11
    */
    private static string getTypeSentenceForm(AbstractDataProviderType type) {
        return DataTypeCatalogue::lookupType(type).sentence_form ?? type.getName();
    }

    #! Returns the detail text that says what a function or an operator gives
    /** @param type the return type

        @return \c "gives" and the type's sentence form (\c "gives true or false")

        @since DataProvider 3.11
    */
    private static string getGivesDetail(AbstractDataProviderType type) {
        return "gives " + getTypeSentenceForm(type);
    }

    #! Gets function completions for DPQL
    /** Includes all expressions that use function-call syntax in DPQL:
        - All \c DET_Function expressions (e.g. \c substr, \c encode_url, \c encrypt)
        - \c DET_Operator expressions with identifier-style names (e.g. \c ends-with, \c contains,
          \c between) — these are operators invoked as \c name(args...) in DPQL text

        Excludes operators with symbolic names (\c ==, \c &&, \c !) which are handled
        separately by \c getOperatorCompletions(), and
        @ref DataProvider::DataProviderExpressionBaseInfo::internal "internal" expressions, which are never offered.

        @param prefix the current prefix to filter by
        @param filter_expressions optional expression map to filter by; when set, only functions
            present in this map are offered (search context filtering)

        @since DataProvider 3.4 added filter_expressions parameter
        @since DataProvider 3.11 internal expressions are not offered
    */
    private list<hash<DpqlCompletionItem>> getFunctionCompletions(string prefix,
            *hash<string, hash<DataProviderExpressionInfo>> filter_expressions) {
        list<hash<DpqlCompletionItem>> items = ();

        if (!expressions) {
            return items;
        }

        string lwr_prefix = prefix.lwr();

        foreach string ename in (keys expressions) {
            hash<DataProviderExpressionInfo> info = expressions{ename};
            # an internal expression is never offered
            if (info.internal) {
                continue;
            }
            # In search context, skip functions not supported by the server
            if (filter_expressions && !filter_expressions{ename}) {
                continue;
            }
            # Use symbol for DPQL surface syntax
            string sym = name_to_symbol{ename} ?? ename;
            # Include all functions and operators with identifier-style symbols
            # (e.g., between, contains, substr, encode_url).
            # Operators with symbolic names (==, &&, !) are handled by getOperatorCompletions().
            # Skip infix comparison operators (e.g. "like") — they are offered
            # by getInfixComparisonCompletions() in infix form, not function-call form
            if (isPrefixExpressionOperator(ename)) {
                continue;
            }
            # wildcard accessors are written [*] and {*}, never as a call
            if (DpqlWildcardAccessors.hasKey(ename)) {
                continue;
            }
            if (info.type == DET_Operator && inlist("Comparison", info.groups)
                    && info.args.size() == 2 && sym =~ /^[a-zA-Z]/) {
                continue;
            }
            if (info.type == DET_Function
                    || (info.type == DET_Operator && sym =~ /^[a-zA-Z]/)) {
                if (!prefix || index(sym.lwr(), lwr_prefix) == 0) {
                    # Build snippet with parameter placeholders
                    *string snippet;
                    if (info.args) {
                        list<string> placeholders = ();
                        for (int i = 0; i < info.args.size(); ++i) {
                            string pname;
                            if (info.arg_info[i].display_name) {
                                pname = info.arg_info[i].display_name;
                            } else {
                                pname = "arg" + string(i);
                            }
                            placeholders += sprintf("${%d:%s}", i + 1, pname);
                        }
                        snippet = sym + "(" + placeholders.join(", ") + ")$0";
                    }
                    items += <DpqlCompletionItem>{
                        "label": sym,
                        "kind": "function",
                        "insert_text": sym + "(",
                        "snippet_text": snippet,
                        "detail": info.return_type ? getGivesDetail(info.return_type) : NOTHING,
                        "documentation": info.short_desc ?? info.desc,
                        "sort_priority": 40,
                    };
                }
            }
        }

        return items;
    }

    #! Gets accessor completions for after a field reference
    /** Only offers accessor syntax when the corresponding expression operators are available
        in the expression map. If no expression map is set (generic context), all accessors
        are offered.
    */
    private list<hash<DpqlCompletionItem>> getAccessorCompletions(string prefix) {
        list<hash<DpqlCompletionItem>> items = ();
        string lwr_prefix = prefix.lwr();

        foreach hash<auto> acc in (AccessorCompletions) {
            if (!prefix || index(acc.label.lwr(), lwr_prefix) == 0) {
                # Only offer accessor completions if the provider supports the corresponding operator
                if (expressions) {
                    if (acc.label == "[" && !expressions{DP_FUNC_INDEX} && !expressions{DP_FUNC_SLICE}) {
                        continue;
                    }
                    if (acc.label == "{" && !expressions{DP_FUNC_KEY}) {
                        continue;
                    }
                    if (acc.label == "." && !expressions{DP_FUNC_KEY}) {
                        continue;
                    }
                }
                items += cast<hash<DpqlCompletionItem>>(acc);
            }
        }

        return items;
    }

    #! Returns the wildcard accessors that the value before the cursor can take
    /** The type of the field reference and accessors before the cursor decides: a list takes \c [*] and a hash
        takes \c {*}; when the type is not known, both are possible.  A slice of a list, and a key of a list, give a
        list.

        @param before the text before the cursor
        @param fields the fields of the record

        @return a hash with \c list (\c [*]) and \c hash (\c {*}) keys, each True if the wildcard can follow, and
        \c known, True if the type of the value is known

        @since DataProvider 3.10
        @since DataProvider 3.11 a slice of a list and a key of a list take \c [*], not \c {*}, and the \c known key
    */
    private hash<string, bool> getWildcardKinds(string before, *hash<string, AbstractDataField> fields) {
        *AbstractDataProviderType type;
        if (*hash<auto> chain = extractFieldPath(before)) {
            if (*AbstractDataField field = fields{chain.field_name}) {
                # a slice of a list, and a key of a list, give a list
                type = traverseTypeWithAccessors(field.getType(), chain.accessors);
            }
        }
        if (!type || !type.hasType()) {
            return {"list": True, "hash": True, "known": False};
        }
        return {
            "list": type.isList(),
            "hash": type.getBaseTypeName() == "hash",
            "known": True,
        };
    }

    #! Gets the wildcard accessor completions after a field reference or an accessor
    /** \c [*] is offered after a list and \c {*} after a hash; both are offered when the type is not known

        @param before the text before the cursor
        @param fields the fields of the record
        @param prefix the text typed so far, which the wildcards must start with

        @return the wildcard completion items

        @since DataProvider 3.10
    */
    private list<hash<DpqlCompletionItem>> getWildcardCompletions(string before,
            *hash<string, AbstractDataField> fields, string prefix) {
        list<hash<DpqlCompletionItem>> items = ();
        hash<string, bool> kinds = getWildcardKinds(before, fields);
        if (kinds.list && (!expressions || expressions{DP_FUNC_LIST_WILDCARD})
                && (!prefix || index(ListWildcardCompletion.label, prefix) == 0)) {
            items += cast<hash<DpqlCompletionItem>>(ListWildcardCompletion);
        }
        if (kinds.hash && (!expressions || expressions{DP_FUNC_HASH_WILDCARD})
                && (!prefix || index(HashWildcardCompletion.label, prefix) == 0)) {
            items += cast<hash<DpqlCompletionItem>>(HashWildcardCompletion);
        }
        return items;
    }

    #! Gets template completions based on prefix
    /** Dispatches to context or value completions depending on whether the prefix
        contains a colon separator.

        @param prefix the current prefix (starting with \c $)
        @param template_context optional template context data for filtering
        @return a list of template completion items

        @since DataProvider 3.3
    */
    private list<hash<DpqlCompletionItem>> getTemplateCompletions(string prefix,
            *hash<auto> template_context) {
        if (!prefix || prefix == "$") {
            # Just $ typed — show template context completions
            return getTemplateContextCompletions("", template_context);
        }
        if (prefix =~ /^\$/) {
            # Check if we have a colon separator
            string rest = substr(prefix, 1);
            int colon_pos = index(rest, ":");
            if (colon_pos >= 0) {
                # After $ctx: — show template value completions
                string ctx = substr(rest, 0, colon_pos);
                string value_prefix = substr(rest, colon_pos + 1);
                return getTemplateValueCompletions(ctx, value_prefix, template_context);
            }
            # Still typing context name — filter context completions
            return getTemplateContextCompletions(rest, template_context);
        }
        return ();
    }

    #! Gets template context completions
    /** @param prefix the context name prefix to filter by
        @param template_context optional template context data for filtering
        @return a list of template context completion items

        @since DataProvider 3.3
    */
    private list<hash<DpqlCompletionItem>> getTemplateContextCompletions(string prefix,
            *hash<auto> template_context) {
        # Use registered callback if available
        if (*list<hash<auto>> contexts = AbstractDataProvider::getTemplateContexts(template_context)) {
            list<hash<DpqlCompletionItem>> items = ();
            string lwr_prefix = prefix.lwr();
            foreach hash<auto> ctx in (contexts) {
                # the raw template string is always what the user types; it drives insertion and
                # (via filter_text) typed-prefix filtering, so it stays byte-identical to before
                string raw = "$" + ctx.name + ":";
                if (!prefix || index(ctx.name.lwr(), lwr_prefix) == 0) {
                    items += <DpqlCompletionItem>{
                        # use the host-supplied display name when present; fall back to the raw
                        # template string otherwise so existing label assertions are unchanged
                        "label": ctx.display_name ?? raw,
                        "kind": "template",
                        "insert_text": raw,
                        "detail": ctx.needs_arg ? TemplateNeedsValueDetail : NOTHING,
                        # always filter against the raw template text the user types
                        "filter_text": raw,
                        "documentation": ctx.description ?? sprintf("Template context: %s", ctx.name),
                        # Sort alphabetically by context name
                        "sort_priority": ctx.sort_priority ?? 10,
                    };
                }
            }
            # Sort alphabetically by label
            items = sort(items, int sub (hash<DpqlCompletionItem> a, hash<DpqlCompletionItem> b) {
                return a.label <=> b.label;
            });
            # Assign sequential sort priorities so the alphabetic order is preserved in the UI
            for (int i = 0; i < items.size(); i++) {
                items[i].sort_priority = i + 1;
            }
            return items;
        }

        # Fall back to default well-known contexts
        list<hash<DpqlCompletionItem>> items = ();
        string lwr_prefix = prefix.lwr();
        foreach hash<auto> tmpl in (DefaultTemplateContexts) {
            # Match against context name (strip leading $ and trailing :)
            string ctx_name = substr(tmpl.label, 1, tmpl.label.size() - 2);
            if (!prefix || index(ctx_name.lwr(), lwr_prefix) == 0) {
                items += cast<hash<DpqlCompletionItem>>(tmpl);
            }
        }
        return items;
    }

    #! Gets template value completions for a given context
    /** @param ctx the template context name
        @param prefix the value prefix to filter by
        @param template_context optional template context data for filtering
        @return a list of template value completion items

        @since DataProvider 3.3
    */
    private list<hash<DpqlCompletionItem>> getTemplateValueCompletions(string ctx, string prefix,
            *hash<auto> template_context) {
        *list<hash<auto>> values = AbstractDataProvider::getTemplateValues(ctx, prefix, template_context);
        if (!values) {
            return ();
        }
        list<hash<DpqlCompletionItem>> items = ();
        foreach hash<auto> val in (values) {
            # the raw template string is always what the user types; it drives insertion and
            # (via filter_text) typed-prefix filtering, so it stays byte-identical to before
            string raw = "$" + ctx + ":" + val.name;
            items += <DpqlCompletionItem>{
                # use the host-supplied display name when present; fall back to the raw template
                # string otherwise so static contexts and existing label assertions are unchanged
                "label": val.display_name ?? raw,
                "kind": "template",
                "insert_text": raw,
                # show the value type next to the label when the host provides it, by its catalogue name
                "detail": val.type.typeCode() == NT_STRING
                    ? (DataTypeCatalogue::lookup(val.type).display_name ?? val.type)
                    : val.type,
                # always filter against the raw template text the user types, regardless of label
                "filter_text": raw,
                "documentation": val.description ?? sprintf("Template value: %s", val.name),
                "sort_priority": val.sort_priority ?? 10,
            };
        }
        return items;
    }

    #! Gets index accessor completions
    private list<hash<DpqlCompletionItem>> getIndexCompletions(string prefix) {
        list<hash<DpqlCompletionItem>> items = ();
        string lwr_prefix = prefix.lwr();

        foreach hash<auto> sugg in (IndexSuggestions) {
            if (!prefix || index(sugg.label.lwr(), lwr_prefix) == 0) {
                items += cast<hash<DpqlCompletionItem>>(sugg);
            }
        }

        return items;
    }

    #! Returns the part of a UTF-8 string between two byte offsets
    /** The completion scans text by bytes; a name cut out of it is cut by bytes too, so that a name after
        non-ASCII text, or one holding non-ASCII letters, is cut where it is. */
    private static string byteSubstr(string text, int start, int len) {
        if (len <= 0) {
            return "";
        }
        return binary_to_string(binary(text).substr(start, len), "UTF-8");
    }

    #! Returns the number of bytes of the character at a byte offset if it can be part of a bare field name or key
    /** The rule is the tokenizer's (see @ref DpqlTokenizer::isBareFieldName()): a letter of any script, a combining
        mark, a digit or \c _; any other character, such as punctuation or a symbol, ends the name.

        @param bytes the UTF-8 bytes of the text
        @param pos the byte offset of the character

        @return the number of bytes of the character, or 0 if it cannot be part of a bare name or if there is no
        complete UTF-8 character at \a pos
    */
    private static int fieldNameCharBytes(binary bytes, int pos) {
        if (pos >= bytes.size()) {
            return 0;
        }
        int b = bytes[pos];
        if (b < 0x80) {
            return ((b >= 0x30 && b <= 0x39) || (b >= 0x41 && b <= 0x5a) || (b >= 0x61 && b <= 0x7a) || b == 0x5f)
                ? 1
                : 0;
        }
        int n = (b & 0xe0) == 0xc0 ? 2 : ((b & 0xf0) == 0xe0 ? 3 : ((b & 0xf8) == 0xf0 ? 4 : 0));
        if (!n || pos + n > bytes.size()) {
            return 0;
        }
        for (int i = pos + 1; i < pos + n; ++i) {
            if ((bytes[i] & 0xc0) != 0x80) {
                return 0;
            }
        }
        return binary_to_string(bytes.substr(pos, n), "UTF-8") =~ /^[\p{L}\p{M}\p{N}_]$/ ? n : 0;
    }

    #! Returns the byte offset after the bare field name or key that starts at a byte offset
    private static int scanFieldName(binary bytes, int pos) {
        while (True) {
            int n = fieldNameCharBytes(bytes, pos);
            if (!n) {
                return pos;
            }
            pos += n;
        }
    }

    #! Extracts field path from text ending before/inside an opening brace
    /** @param text the text up to cursor position
        @return hash with "field_name" and "accessors" list, "end", the byte offset where the field reference and its
        accessors end, and "member", @ref True if a dot at "end" would be followed by a member (after an accessor or
        a quoted field name); or NOTHING if not a valid path
    */
    private *hash<auto> extractFieldPath(string text) {
        # Find the last @ that starts a field reference
        int at_pos = -1;
        bool in_string = False;
        bool escaped = False;
        string quote_char;

        for (int i = text.size() - 1; i >= 0; --i) {
            string c = chr(text.getByte(i));
            # When scanning backwards, we handle strings differently
            # Just look for @ not inside a string
            if (c == "@" && !in_string) {
                # a field token after the dot that follows an accessor is a member (<tt>.\@"Unit.Price"</tt>), not
                # the start of the field reference; the dots of a range (<tt>[0..\@n]</tt>) are not a member's
                if (i > 0 && chr(text.getByte(i - 1)) == "." && (i == 1 || chr(text.getByte(i - 2)) != ".")) {
                    continue;
                }
                at_pos = i;
                break;
            }
            # Track string state (simplified backwards scan)
            if ((c == '"' || c == "'") && (i == 0 || chr(text.getByte(i-1)) != "\\")) {
                if (in_string && c == quote_char) {
                    in_string = False;
                } else if (!in_string) {
                    in_string = True;
                    quote_char = c;
                }
            }
        }

        if (at_pos < 0) {
            return NOTHING;
        }

        # Extract field name starting at at_pos
        int pos = at_pos + 1;  # Skip @
        if (pos >= text.size()) {
            return NOTHING;
        }
        # the text is scanned by bytes; a name's characters are read from its bytes
        binary bytes = binary(text);

        string field_name;
        # the parts of a quoted path after its first, as key accessors
        list<hash<auto>> path_keys = ();
        # True when a dot is followed by a member, as the parser reads it: after a quoted field name or an
        # accessor; in a bare field name, a dot separates the parts of its path
        bool member = False;
        if (chr(text.getByte(pos)) == '"') {
            # Quoted field name
            ++pos;  # Skip opening quote
            int start = pos;
            while (pos < text.size() && chr(text.getByte(pos)) != '"') {
                if (chr(text.getByte(pos)) == "\\" && pos + 1 < text.size()) {
                    pos += 2;
                } else {
                    ++pos;
                }
            }
            # dots separate the parts of the path as in a bare name, and an escaped dot is part of a key
            list<string> parts = AbstractDataProvider::getFieldPath(
                unescapeFieldPath(byteSubstr(text, start, pos - start)));
            field_name = shift parts;
            path_keys = map ("type": DP_FUNC_KEY, "key_list": ($1,)), parts;
            ++pos;  # Skip closing quote
            member = True;
        } else {
            # Unquoted field name: stop at first '.' so nested segments are handled
            # as accessors (matches eval semantics where @record.address traverses
            # field "record" then key "address", not a single field "record.address")
            int start = pos;
            pos = scanFieldName(bytes, pos);
            field_name = byteSubstr(text, start, pos - start);
        }

        if (!field_name) {
            return NOTHING;
        }

        # Parse accessor chain from pos to end of text
        list<hash<auto>> accessors = path_keys;

        while (pos < text.size()) {
            string c = chr(text.getByte(pos));

            if (c == "[") {
                # Index/slice accessor
                ++pos;
                int bracket_start = pos;
                int depth = 1;
                while (pos < text.size() && depth > 0) {
                    string ac = chr(text.getByte(pos));
                    # quoted text is skipped: a bracket in it does not end the accessor
                    if (ac == '"' || ac == "'") {
                        pos = skipQuoted(text, pos);
                        continue;
                    }
                    if (ac == "[") ++depth;
                    else if (ac == "]") --depth;
                    ++pos;
                }
                # Only add accessor if it was properly closed
                if (depth == 0) {
                    # We've consumed the closing ], so content is bracket_start to pos-1
                    string content = byteSubstr(text, bracket_start, pos - 1 - bracket_start);
                    # Simplified: treat as INDEX for type traversal; [*] gives each element as well
                    accessors += {"type": trim(content) == "*" ? DP_FUNC_LIST_WILDCARD : DP_FUNC_INDEX,
                        "content": content};
                    member = True;
                }
                # If not closed, we're still inside this accessor - stop parsing
                else {
                    break;
                }
            } else if (c == "{") {
                # Key accessor
                ++pos;
                int brace_start = pos;
                int depth = 1;
                while (pos < text.size() && depth > 0) {
                    string ac = chr(text.getByte(pos));
                    # quoted text is skipped: a brace in a quoted key (<tt>{"a}b"}</tt>) does not end the accessor
                    if (ac == '"' || ac == "'") {
                        pos = skipQuoted(text, pos);
                        continue;
                    }
                    if (ac == "{") ++depth;
                    else if (ac == "}") --depth;
                    ++pos;
                }
                # Only add accessor if it was properly closed
                if (depth == 0) {
                    # Extract keys from content
                    string content = byteSubstr(text, brace_start, pos - 1 - brace_start);
                    if (trim(content) == "*") {
                        # {*}: each value of the hash
                        accessors += {"type": DP_FUNC_HASH_WILDCARD};
                    } else {
                        list<string> key_list = parseKeyList(content);
                        accessors += {"type": DP_FUNC_KEY, "key_list": key_list};
                    }
                    member = True;
                }
                # If not closed, we're still inside this accessor - stop parsing
                else {
                    break;
                }
            } else if (c == ".") {
                # Dot accessor - next identifier becomes a key accessor
                if (member) {
                    # a member is one key, as the parser reads it; without one, the path ends before the dot
                    int member_pos = pos + 1;
                    *string key = scanMember(text, bytes, \member_pos);
                    if (!exists key) {
                        break;
                    }
                    pos = member_pos;
                    accessors += {"type": DP_FUNC_KEY, "key_list": (key,)};
                    continue;
                }
                ++pos;
                int start = pos;
                pos = scanFieldName(bytes, pos);
                if (pos > start) {
                    string key = byteSubstr(text, start, pos - start);
                    accessors += {"type": DP_FUNC_KEY, "key_list": (key,)};
                }
            } else {
                # Unknown character, stop parsing
                break;
            }
        }

        return {
            "field_name": field_name,
            "accessors": accessors,
            # the byte offset where the field reference and its accessors end
            "end": pos,
            "member": member,
        };
    }

    #! Returns the byte offset after the quoted text that starts at a byte offset
    /** A backslash escapes the next character

        @param text the text
        @param pos the byte offset of the opening quote

        @return the byte offset after the closing quote, or the size of the text if the quote is not closed

        @since DataProvider 3.11
    */
    private static int skipQuoted(string text, int pos) {
        string quote_char = chr(text.getByte(pos));
        ++pos;
        while (pos < text.size()) {
            string c = chr(text.getByte(pos));
            if (c == "\\") {
                pos += 2;
                continue;
            }
            ++pos;
            if (c == quote_char) {
                return pos;
            }
        }
        return text.size();
    }

    #! Reads the member after the dot that follows an accessor or a quoted field name, as the parser reads it
    /** A member is one key: a field token (<tt>\@name</tt>, or <tt>\@"name"</tt> whose escapes are decoded, an
        escaped dot to a dot; the dots of either are part of the key), or an identifier: letters, combining marks,
        digits and \c _, and a \c - followed by an ASCII letter (<tt>order-id</tt>), which is not a keyword
        (\c true, \c false, \c null are values, which the parser does not take as a member)

        @param text the text
        @param bytes the UTF-8 bytes of the text
        @param pos the byte offset after the dot; set to the byte offset after the member if there is one

        @return the key, or @ref nothing if there is no member at \a pos, or if its quote is not closed

        @since DataProvider 3.11
    */
    private *string scanMember(string text, binary bytes, reference<int> pos) {
        if (pos < text.size() && chr(text.getByte(pos)) == "@") {
            int start = pos + 1;
            if (start < text.size() && chr(text.getByte(start)) == '"') {
                int end = start + 1;
                while (end < text.size() && chr(text.getByte(end)) != '"') {
                    end += (chr(text.getByte(end)) == "\\" && end + 1 < text.size()) ? 2 : 1;
                }
                if (end >= text.size()) {
                    return;
                }
                pos = end + 1;
                return unescapeString(byteSubstr(text, start + 1, end - start - 1));
            }
            int end = scanFieldName(bytes, start);
            while (end > start && end + 1 < text.size() && chr(text.getByte(end)) == "."
                    && fieldNameCharBytes(bytes, end + 1)) {
                end = scanFieldName(bytes, end + 1);
            }
            if (end == start) {
                return;
            }
            pos = end;
            return byteSubstr(text, start, end - start);
        }
        int end = pos;
        while (True) {
            int n = fieldNameCharBytes(bytes, end);
            if (n) {
                end += n;
            } else if (end > pos && end + 1 < text.size() && chr(text.getByte(end)) == "-"
                    && chr(text.getByte(end + 1)) =~ /^[a-zA-Z]$/) {
                ++end;
            } else {
                break;
            }
        }
        if (end == pos) {
            return;
        }
        string key = byteSubstr(text, pos, end - pos);
        if (DpqlKeywordIdentifiers{key.lwr()}) {
            return;
        }
        pos = end;
        return key;
    }

    #! Parses a comma-separated list of keys from accessor content
    private list<string> parseKeyList(string content) {
        list<string> key_list = ();
        string trimmed = trim(content);
        if (!trimmed) {
            return key_list;
        }

        # Simple parsing: split by comma, handling quoted strings
        int pos = 0;
        while (pos < trimmed.size()) {
            # Skip whitespace
            while (pos < trimmed.size() && trimmed[pos] =~ /\s/) {
                ++pos;
            }
            if (pos >= trimmed.size()) break;

            string key;
            if (trimmed[pos] == '"' || trimmed[pos] == "'") {
                # Quoted key, in double or single quotes, as the parser reads it - extract and unescape
                string quote_char = trimmed[pos];
                ++pos;
                int start = pos;
                while (pos < trimmed.size() && trimmed[pos] != quote_char) {
                    if (trimmed[pos] == "\\" && pos + 1 < trimmed.size()) {
                        pos += 2;
                    } else {
                        ++pos;
                    }
                }
                # Unescape the key to match actual field names in schema
                key = unescapeString(substr(trimmed, start, pos - start));
                if (pos < trimmed.size()) ++pos;  # Skip closing quote
            } else {
                # Unquoted key
                int start = pos;
                while (pos < trimmed.size() && trimmed[pos] !~ /[\s,]/) {
                    ++pos;
                }
                key = substr(trimmed, start, pos - start);
            }

            if (key) {
                key_list += key;
            }

            # Skip to next key (past comma)
            while (pos < trimmed.size() && trimmed[pos] =~ /[\s,]/) {
                ++pos;
            }
        }

        return key_list;
    }

    #! Traverses type through accessor chain to get the final type
    /** As at run time, a slice of a list is a list of the same type, and a key of a list is the key of each of its
        items, through nested lists, which gives a list of the key's values

        @param base_type the starting field type
        @param accessors list of accessor specifications
        @return the resulting type after all accessors, or NOTHING if path is invalid
    */
    private *AbstractDataProviderType traverseTypeWithAccessors(
            AbstractDataProviderType base_type,
            list<hash<auto>> accessors) {

        AbstractDataProviderType current = base_type;

        foreach hash<auto> acc in (accessors) {
            switch (acc.type) {
                case DP_FUNC_HASH_WILDCARD: {
                    # each value of a hash: the type of its values, if they all have the same type
                    *AbstractDataProviderType value_type = current.getFields() ? NOTHING : current.getElementType();
                    if (!value_type) {
                        return NOTHING;
                    }
                    current = value_type;
                    break;
                }

                case DP_FUNC_LIST_WILDCARD:
                case DP_FUNC_INDEX:
                case DP_FUNC_SLICE: {
                    *AbstractDataProviderType elem = current.getElementType();
                    if (!elem) {
                        return NOTHING;  # Not a list type
                    }
                    # a slice of a list (<tt>[0..2]</tt>) is a list of the same type
                    if (acc.type == DP_FUNC_SLICE || (acc.type == DP_FUNC_INDEX && acc.content =~ /\.\./)) {
                        if (!current.isList()) {
                            return NOTHING;
                        }
                        break;
                    }
                    # List index access, or each element: get element type
                    current = elem;
                    break;
                }

                case DP_FUNC_KEY:
                    # Key access: get the field's type for single key; a key of a list is the key of each of its
                    # items, through nested lists, and gives the list of their values, as at run time
                    if (acc.key_list && acc.key_list.size() == 1) {
                        int depth;
                        *AbstractDataField field = getKeyedType(current, \depth).getField(acc.key_list[0]);
                        if (!field) {
                            return NOTHING;  # Field doesn't exist
                        }
                        current = field.getType();
                        for (int i = 0; i < depth; ++i) {
                            current = new ListDataType(current);
                        }
                    }
                    # Multiple keys: result is still a hash type (subset)
                    # We keep current type for key suggestions
                    break;
            }
        }

        return current;
    }

    #! Returns the type of the values that a key accessor applies to: the items of a list, through nested lists
    /** At run time a key of a list is the key of each of its items, also of the items of a list in it
        (<tt>\@items{name}</tt> is the list of the names of the items); for a list of a known type, the type of its
        items is returned, through nested lists, and any other type as it is

        @param type the type of the value that the key accessor follows
        @param depth set to the number of nested lists

        @return the type of the values that the key applies to

        @since DataProvider 3.11
    */
    private static AbstractDataProviderType getKeyedType(AbstractDataProviderType type, reference<int> depth) {
        depth = 0;
        while (type.isList()) {
            *AbstractDataProviderType element_type = type.getElementType();
            if (!element_type) {
                break;
            }
            type = element_type;
            ++depth;
        }
        return type;
    }

    #! Gets key completions for a hash type
    /** @param type the type to get keys from; the keys of a list are the keys of its items, as a key of a list is the
        key of each item
        @param prefix filter prefix for matching
        @return list of completion items for valid keys
    */
    private list<hash<DpqlCompletionItem>> getKeyCompletions(
            AbstractDataProviderType type,
            string prefix) {

        list<hash<DpqlCompletionItem>> items = ();

        # Check if type has fields (is a hash-like type); a key of a list is the key of each item
        int depth;
        *hash<string, AbstractDataField> type_fields = getKeyedType(type, \depth).getFields();
        if (!type_fields) {
            return items;
        }

        string lwr_prefix = prefix.lwr();

        foreach string key_name in (keys type_fields) {
            if (!prefix || index(key_name.lwr(), lwr_prefix) == 0) {
                AbstractDataField field = type_fields{key_name};

                # a key that is not a bare name, or that is a keyword, is quoted
                # escape a backslash first, then a quote; in single quotes \\ is two backslashes, so '\"' is \"
                string insert_text = DpqlTokenizer::isBareKey(key_name)
                    ? key_name
                    : '"' + replace(replace(key_name, "\\", "\\\\"), '"', '\"') + '"';

                # Build documentation with type info
                *string doc = field.getDescription();
                string type_name = field.getType().getName();
                if (doc) {
                    doc += sprintf(" (%s)", type_name);
                } else {
                    doc = sprintf("Type: %s", type_name);
                }

                items += <DpqlCompletionItem>{
                    "label": key_name,
                    "kind": "field",
                    "insert_text": insert_text,
                    "documentation": doc,
                    "sort_priority": 10,
                };
            }
        }

        return items;
    }

    #! Unescapes the text between the quotes of a quoted field name to a field path, as the parser reads it
    /** An escaped dot stays escaped, as it is part of a key; the other escapes are decoded as in
        unescapeString(), which also takes an unknown escape as its character, as completion reads incomplete text

        @since DataProvider 3.11
    */
    private string unescapeFieldPath(string s) {
        return unescapeString(replace(s, "\\.", "\\\\."));
    }

    #! Unescapes DPQL escape sequences in a quoted string
    /** Handles \\\\, \\", \\', \\n, \\r, \\t
        @param s the escaped string content (without surrounding quotes)
        @return the unescaped string
    */
    private string unescapeString(string s) {
        string result = "";
        int i = 0;
        while (i < s.size()) {
            if (s[i] == "\\" && i + 1 < s.size()) {
                string next = s[i + 1];
                switch (next) {
                    case "n": result += "\n"; break;
                    case "r": result += "\r"; break;
                    case "t": result += "\t"; break;
                    case "\\": result += "\\"; break;
                    case '"': result += '"'; break;
                    case "'": result += "'"; break;
                    default: result += next; break;
                }
                i += 2;
            } else {
                result += s[i];
                ++i;
            }
        }
        return result;
    }

    #! Returns signature help for the function call at the given position
    /** Analyzes the text up to the cursor position to find the enclosing function call,
        then looks up the function in the expression map to provide parameter info.

        @param text the DPQL expression text
        @param position cursor position (0-based character offset)

        @return signature help info, or @ref nothing if the cursor is not inside a function call

        @since DataProvider 3.4
    */
    *hash<DpqlSignatureHelp> getSignatureHelp(string text, int position) {
        if (!expressions) {
            return NOTHING;
        }

        # Normalize to UTF-8; the byte-indexed scan below relies on stable
        # byte offsets that only line up under a single encoding.
        if (text.encoding() != "UTF-8") {
            text = convert_encoding(text, "UTF-8");
        }

        # Translate the caller's character-offset cursor to a byte offset so
        # the byte-indexed scan stays aligned even when multi-byte UTF-8
        # sequences appear before the cursor.  All structural tokens we
        # search for (parens, comma, identifier chars, hyphen) are pure ASCII,
        # so a byte-only scan is correct: every UTF-8 continuation byte has
        # bit 0x80 set and cannot match any of those literals.
        int prefix_byte_len = position >= text.length()
            ? text.size()
            : text.substr(0, position).size();

        # Scan backwards from the cursor to find the enclosing function call
        int depth = 0;
        int comma_count = 0;
        int paren_pos = -1;

        for (int i = prefix_byte_len - 1; i >= 0; --i) {
            int b = text.getByte(i);
            if (b == 41) {              # ')'
                ++depth;
            } else if (b == 40) {       # '('
                if (depth > 0) {
                    --depth;
                } else {
                    # Found the matching open paren at the top level
                    paren_pos = i;
                    break;
                }
            } else if (b == 44 && depth == 0) {     # ','
                ++comma_count;
            }
        }

        if (paren_pos < 0) {
            return NOTHING;
        }

        # Extract function name: scan backwards from paren_pos to find identifier.
        # Indices below are byte offsets into `text`.
        int name_end = paren_pos;
        int name_start = paren_pos - 1;
        # Skip whitespace before the paren (whitespace is ASCII)
        while (name_start >= 0 && chr(text.getByte(name_start)) =~ /\s/) {
            --name_start;
        }
        if (name_start < 0) {
            return NOTHING;
        }
        # Scan the identifier (supports hyphenated names like "custom-func").
        # Identifier and hyphen-allowed bytes are all ASCII, so the byte-level
        # scan correctly skips over any preceding multi-byte UTF-8 sequences
        # (continuation bytes don't match any of the patterns and end the scan).
        int id_end = name_start + 1;
        while (name_start >= 0) {
            int b = text.getByte(name_start);
            string sc = chr(b);
            if (sc =~ /[a-zA-Z0-9_]/) {
                --name_start;
            } else if (sc == "-" && name_start + 1 < id_end
                    && chr(text.getByte(name_start + 1)) =~ /[a-zA-Z]/) {
                # hyphenated identifier: '-' followed by a letter (looking right)
                --name_start;
            } else {
                break;
            }
        }
        ++name_start;
        if (name_start >= id_end) {
            return NOTHING;
        }

        # The identifier bytes [name_start, id_end) are guaranteed pure ASCII
        # by the loop above, so a byte-slice → string conversion is safe.
        # text.substr() is character-indexed and would mis-cut on multi-byte
        # input; go through the binary representation instead.
        string func_name = text.toBinary().substr(name_start, id_end - name_start).toString("UTF-8");

        # Resolve symbol to expression name, then look up in expressions
        *string resolved_name;
        # Try symbol→name first, then direct name lookup for backward compat
        foreach string ename in (keys name_to_symbol) {
            if (name_to_symbol{ename} == func_name) {
                resolved_name = ename;
                break;
            }
        }
        if (!resolved_name && expressions{func_name}) {
            resolved_name = func_name;
        }
        if (!resolved_name) {
            return NOTHING;
        }
        *hash<DataProviderExpressionInfo> info = expressions{resolved_name};
        if (!info) {
            return NOTHING;
        }

        # Build parameter info list
        list<hash<DpqlParameterInfo>> params = ();
        list<string> param_labels = ();
        if (info.arg_info) {
            foreach hash<DataProviderArgInfo> ai in (info.arg_info) {
                string plabel = ai.display_name;
                params += <DpqlParameterInfo>{
                    "label": plabel,
                    "documentation": ai.short_desc ?? ai.desc,
                };
                param_labels += plabel;
            }
        } else if (info.args) {
            for (int i = 0; i < info.args.size(); ++i) {
                string plabel = sprintf("arg%d", i + 1);
                params += <DpqlParameterInfo>{
                    "label": plabel,
                };
                param_labels += plabel;
            }
        }

        # Build the signature label
        string sig_label = func_name + "(" + (foldl $1 + ", " + $2, param_labels) + ")";
        if (info.return_type) {
            sig_label += " → " + info.return_type.getName();
        }

        # Clamp active parameter to valid range
        int active = comma_count;
        if (params.size() > 0 && active >= params.size()) {
            if (info.varargs) {
                active = params.size() - 1;
            } else {
                active = params.size() - 1;
            }
        }

        return <DpqlSignatureHelp>{
            "label": sig_label,
            "documentation": info.short_desc ?? info.desc,
            "parameters": params,
            "active_parameter": active,
        };
    }
}

} # namespace DataProvider
