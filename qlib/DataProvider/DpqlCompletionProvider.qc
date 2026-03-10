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
    #! Optional documentation or help text
    *string documentation;
    #! Sort priority (lower = higher priority)
    int sort_priority = 100;
}

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
}

#! Completion context types
const DPQL_CTX_START = "start";              # Start of expression
const DPQL_CTX_AFTER_FIELD = "after_field";  # After a field reference
const DPQL_CTX_AFTER_OP = "after_op";        # After an operator
const DPQL_CTX_AFTER_VALUE = "after_value";  # After a value
const DPQL_CTX_IN_FIELD = "in_field";        # Inside a field reference (after @)
const DPQL_CTX_IN_STRING = "in_string";      # Inside a string
const DPQL_CTX_IN_PAREN = "in_paren";        # Inside parentheses
const DPQL_CTX_IN_INDEX = "in_index";        # Inside index accessor [...]
const DPQL_CTX_IN_KEY = "in_key";            # Inside key accessor {...}
const DPQL_CTX_AFTER_ACCESSOR = "after_acc"; # After ] or } - can add more accessors
const DPQL_CTX_IN_TEMPLATE = "in_template"; # Inside a template reference (after $)

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

        #! Comparison operators with descriptions
        const ComparisonOperators = (
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
                "documentation": "Range comparison: between X and Y", "sort_priority": 20},
            {"label": "in", "kind": "keyword", "insert_text": "in (",
                "documentation": "Collection membership: in (val1, val2, ...)", "sort_priority": 21},
            {"label": "=~", "kind": "operator", "insert_text": "=~ /",
                "documentation": "Regex pattern match", "sort_priority": 22},
        );

        #! Logical operators with descriptions
        const LogicalOperators = (
            {"label": "&&", "kind": "operator", "insert_text": " && ",
                "documentation": "Logical AND", "sort_priority": 30},
            {"label": "||", "kind": "operator", "insert_text": " || ",
                "documentation": "Logical OR", "sort_priority": 31},
        );

        #! Keyword completions
        const Keywords = (
            {"label": "true", "kind": "keyword", "insert_text": "true",
                "documentation": "Boolean true value", "sort_priority": 50},
            {"label": "false", "kind": "keyword", "insert_text": "false",
                "documentation": "Boolean false value", "sort_priority": 51},
            {"label": "null", "kind": "keyword", "insert_text": "null",
                "documentation": "Null/nothing value", "sort_priority": 52},
            {"label": "and", "kind": "keyword", "insert_text": "and ",
                "documentation": "Used in 'between X and Y'", "sort_priority": 53},
        );

        #! Accessor completions
        const AccessorCompletions = (
            {"label": "[", "kind": "operator", "insert_text": "[",
                "documentation": "Index accessor: [0], [-1], [0..3]", "sort_priority": 5},
            {"label": "{", "kind": "operator", "insert_text": "{",
                "documentation": "Key accessor: {key}, {a, b, c}", "sort_priority": 6},
            {"label": ".", "kind": "operator", "insert_text": ".",
                "documentation": "Member access", "sort_priority": 7},
        );

        #! Well-known template contexts with descriptions
        const DefaultTemplateContexts = (
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
        const IndexSuggestions = (
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

        list<hash<DpqlCompletionItem>> items = ();

        switch (analysis.ctx) {
            case DPQL_CTX_START:
            case DPQL_CTX_AFTER_OP:
            case DPQL_CTX_IN_PAREN:
                # Can start with field, literal, function, template, or NOT
                items += getFieldCompletions(ctx.fields, analysis.prefix);
                items += getValueCompletions(analysis.prefix);
                items += getFunctionCompletions(analysis.prefix);
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
                break;

            case DPQL_CTX_IN_TEMPLATE:
                # Completing template context or value after $
                items += getTemplateCompletions(analysis.prefix, ctx.template_context);
                break;

            case DPQL_CTX_IN_FIELD:
                # Completing field name after @
                items += getFieldCompletions(ctx.fields, analysis.prefix);
                break;

            case DPQL_CTX_AFTER_FIELD:
                # After field, suggest accessors and comparison operators
                items += getAccessorCompletions(analysis.prefix);
                items += getOperatorCompletions(analysis.prefix, True);
                break;

            case DPQL_CTX_AFTER_VALUE:
                # After value, suggest logical operators or more comparisons
                items += getOperatorCompletions(analysis.prefix, False);
                break;

            case DPQL_CTX_IN_STRING:
                # Inside string - no completions
                break;

            case DPQL_CTX_IN_INDEX:
                # Inside index accessor - suggest integers and range operator
                items += getIndexCompletions(analysis.prefix);
                break;

            case DPQL_CTX_IN_KEY:
                # Inside key accessor - suggest keys from schema if available
                if (analysis.field_path) {
                    *AbstractDataField base_field = ctx.fields{analysis.field_path.field_name};
                    if (base_field) {
                        *AbstractDataProviderType target_type = traverseTypeWithAccessors(
                            base_field.getType(), analysis.field_path.accessors);
                        if (target_type) {
                            items += getKeyCompletions(target_type, analysis.prefix);
                        }
                    }
                }
                break;

            case DPQL_CTX_AFTER_ACCESSOR:
                # After ] or } - can add more accessors or operators
                items += getAccessorCompletions(analysis.prefix);
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

        # Check if we're inside a string
        int in_string = checkInString(before);
        if (in_string) {
            return {
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

        # After opening paren
        if (last_char == "(") {
            return {
                "ctx": DPQL_CTX_IN_PAREN,
                "prefix": prefix,
            };
        }

        # After comma
        if (last_char == ",") {
            return {
                "ctx": DPQL_CTX_AFTER_OP,
                "prefix": prefix,
            };
        }

        # Check if last token was a field reference
        if (trimmed =~ /@[a-zA-Z0-9_."]+\s*$/) {
            return {
                "ctx": DPQL_CTX_AFTER_FIELD,
                "prefix": prefix,
            };
        }

        # Check if last token was an operator
        if (trimmed =~ /(==|!=|<=|>=|<|>|&&|\|\||=~|between|in|and)\s*$/) {
            return {
                "ctx": DPQL_CTX_AFTER_OP,
                "prefix": prefix,
            };
        }

        # Check if last token was a value (string, number, boolean, null)
        if (trimmed =~ /("[^"]*"|'[^']*'|-?[0-9]+\.?[0-9]*|true|false|null|\))\s*$/) {
            return {
                "ctx": DPQL_CTX_AFTER_VALUE,
                "prefix": prefix,
            };
        }

        # Default to start context
        return {
            "ctx": DPQL_CTX_START,
            "prefix": prefix,
        };
    }

    #! Checks if position is inside brackets [...] (for index accessor)
    private bool checkInBrackets(string text) {
        int depth = 0;
        bool in_string = False;
        bool escaped = False;
        string quote_char;

        for (int i = 0; i < text.size(); ++i) {
            string c = text[i];
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
            string c = text[i];
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
        bool in_double = False;
        bool in_single = False;
        bool escaped = False;

        for (int i = 0; i < text.size(); ++i) {
            string c = text[i];
            if (escaped) {
                escaped = False;
                continue;
            }
            if (c == "\\") {
                escaped = True;
                continue;
            }
            if (c == '"' && !in_single) {
                in_double = !in_double;
            } else if (c == "'" && !in_double) {
                in_single = !in_single;
            }
        }

        return in_double || in_single ? 1 : 0;
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

                # Determine if field name needs quoting
                string insert_text;
                if (field_name =~ /[^a-zA-Z0-9_.]/ || field_name =~ /^[0-9]/) {
                    insert_text = '@"' + replace(field_name, '"', '\\"') + '"';
                } else {
                    insert_text = "@" + field_name;
                }

                items += <DpqlCompletionItem>{
                    "label": field_name,
                    "kind": "field",
                    "insert_text": insert_text,
                    "documentation": info.short_desc ?? info.desc,
                    "sort_priority": 1,
                };
            }
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

    #! Gets function completions
    /** Includes both \c DET_Function expressions and \c DET_Operator expressions that use
        function-call syntax (identifier-style names like \c ends-with, \c contains).
        Infix operators with symbolic names (\c ==, \c &&) are excluded as they are
        handled separately by \c getOperatorCompletions().
    */
    private list<hash<DpqlCompletionItem>> getFunctionCompletions(string prefix) {
        list<hash<DpqlCompletionItem>> items = ();

        if (!expressions) {
            return items;
        }

        string lwr_prefix = prefix.lwr();

        foreach string name in (keys expressions) {
            hash<DataProviderExpressionInfo> info = expressions{name};
            # Include functions and operators with function-call syntax
            # Operators with infix/prefix render templates (starting with $arg or !)
            # are handled separately by getOperatorCompletions()
            if (info.type == DET_Function
                    || (info.type == DET_Operator && name =~ /^[a-zA-Z]/
                        && (!info.render_template
                            || !info.render_template.regex("^(\\$arg|!)")))) {
                if (!prefix || index(name.lwr(), lwr_prefix) == 0) {
                    items += <DpqlCompletionItem>{
                        "label": name,
                        "kind": "function",
                        "insert_text": name + "(",
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
                string label = "$" + ctx.name + ":";
                if (!prefix || index(ctx.name.lwr(), lwr_prefix) == 0) {
                    items += <DpqlCompletionItem>{
                        "label": label,
                        "kind": "template",
                        "insert_text": label,
                        "documentation": ctx.description ?? sprintf("Template context: %s", ctx.name),
                        "sort_priority": ctx.sort_priority ?? 10,
                    };
                }
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
            items += <DpqlCompletionItem>{
                "label": "$" + ctx + ":" + val.name,
                "kind": "template",
                "insert_text": "$" + ctx + ":" + val.name,
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

    #! Extracts field path from text ending before/inside an opening brace
    /** @param text the text up to cursor position
        @return hash with "field_name" and "accessors" list, or NOTHING if not a valid path
    */
    private *hash<auto> extractFieldPath(string text) {
        # Find the last @ that starts a field reference
        int at_pos = -1;
        bool in_string = False;
        bool escaped = False;
        string quote_char;

        for (int i = text.size() - 1; i >= 0; --i) {
            string c = text[i];
            # When scanning backwards, we handle strings differently
            # Just look for @ not inside a string
            if (c == "@" && !in_string) {
                at_pos = i;
                break;
            }
            # Track string state (simplified backwards scan)
            if ((c == '"' || c == "'") && (i == 0 || text[i-1] != "\\")) {
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

        string field_name;
        if (text[pos] == '"') {
            # Quoted field name
            ++pos;  # Skip opening quote
            int start = pos;
            while (pos < text.size() && text[pos] != '"') {
                if (text[pos] == "\\" && pos + 1 < text.size()) {
                    pos += 2;
                } else {
                    ++pos;
                }
            }
            # Unescape the field name to match actual field names in schema
            field_name = unescapeString(substr(text, start, pos - start));
            ++pos;  # Skip closing quote
        } else {
            # Unquoted field name: stop at first '.' so nested segments are handled
            # as accessors (matches eval semantics where @record.address traverses
            # field "record" then key "address", not a single field "record.address")
            int start = pos;
            while (pos < text.size() && text[pos] =~ /[a-zA-Z0-9_]/) {
                ++pos;
            }
            field_name = substr(text, start, pos - start);
        }

        if (!field_name) {
            return NOTHING;
        }

        # Parse accessor chain from pos to end of text
        list<hash<auto>> accessors = ();

        while (pos < text.size()) {
            string c = text[pos];

            if (c == "[") {
                # Index/slice accessor
                ++pos;
                int bracket_start = pos;
                int depth = 1;
                while (pos < text.size() && depth > 0) {
                    if (text[pos] == "[") ++depth;
                    else if (text[pos] == "]") --depth;
                    ++pos;
                }
                # Only add accessor if it was properly closed
                if (depth == 0) {
                    # We've consumed the closing ], so content is bracket_start to pos-1
                    string content = substr(text, bracket_start, pos - 1 - bracket_start);
                    # Simplified: treat as INDEX for type traversal
                    accessors += {"type": DP_FUNC_INDEX, "content": content};
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
                    if (text[pos] == "{") ++depth;
                    else if (text[pos] == "}") --depth;
                    ++pos;
                }
                # Only add accessor if it was properly closed
                if (depth == 0) {
                    # Extract keys from content
                    string content = substr(text, brace_start, pos - 1 - brace_start);
                    list<string> key_list = parseKeyList(content);
                    accessors += {"type": DP_FUNC_KEY, "key_list": key_list};
                }
                # If not closed, we're still inside this accessor - stop parsing
                else {
                    break;
                }
            } else if (c == ".") {
                # Dot accessor - next identifier becomes a key accessor
                ++pos;
                int start = pos;
                while (pos < text.size() && text[pos] =~ /[a-zA-Z0-9_]/) {
                    ++pos;
                }
                if (pos > start) {
                    string key = substr(text, start, pos - start);
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
        };
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
            if (trimmed[pos] == '"') {
                # Quoted key - extract and unescape
                ++pos;
                int start = pos;
                while (pos < trimmed.size() && trimmed[pos] != '"') {
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
    /** @param base_type the starting field type
        @param accessors list of accessor specifications
        @return the resulting type after all accessors, or NOTHING if path is invalid
    */
    private *AbstractDataProviderType traverseTypeWithAccessors(
            AbstractDataProviderType base_type,
            list<hash<auto>> accessors) {

        AbstractDataProviderType current = base_type;

        foreach hash<auto> acc in (accessors) {
            switch (acc.type) {
                case DP_FUNC_INDEX:
                case DP_FUNC_SLICE:
                    # List index/slice access: get element type
                    *AbstractDataProviderType elem = current.getElementType();
                    if (!elem) {
                        return NOTHING;  # Not a list type
                    }
                    current = elem;
                    break;

                case DP_FUNC_KEY:
                    # Key access: get the field's type for single key
                    if (acc.key_list && acc.key_list.size() == 1) {
                        *AbstractDataField field = current.getField(acc.key_list[0]);
                        if (!field) {
                            return NOTHING;  # Field doesn't exist
                        }
                        current = field.getType();
                    }
                    # Multiple keys: result is still a hash type (subset)
                    # We keep current type for key suggestions
                    break;
            }
        }

        return current;
    }

    #! Gets key completions for a hash type
    /** @param type the type to get keys from
        @param prefix filter prefix for matching
        @return list of completion items for valid keys
    */
    private list<hash<DpqlCompletionItem>> getKeyCompletions(
            AbstractDataProviderType type,
            string prefix) {

        list<hash<DpqlCompletionItem>> items = ();

        # Check if type has fields (is a hash-like type)
        *hash<string, AbstractDataField> type_fields = type.getFields();
        if (!type_fields) {
            return items;
        }

        string lwr_prefix = prefix.lwr();

        foreach string key_name in (keys type_fields) {
            if (!prefix || index(key_name.lwr(), lwr_prefix) == 0) {
                AbstractDataField field = type_fields{key_name};

                # Determine if key needs quoting
                string insert_text;
                if (key_name =~ /[^a-zA-Z0-9_]/ || key_name =~ /^[0-9]/) {
                    insert_text = '"' + replace(key_name, '"', '\\"') + '"';
                } else {
                    insert_text = key_name;
                }

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
}

} # namespace DataProvider
