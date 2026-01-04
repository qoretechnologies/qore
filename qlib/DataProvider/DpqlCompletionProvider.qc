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
}

#! Completion context types
const DPQL_CTX_START = "start";              # Start of expression
const DPQL_CTX_AFTER_FIELD = "after_field";  # After a field reference
const DPQL_CTX_AFTER_OP = "after_op";        # After an operator
const DPQL_CTX_AFTER_VALUE = "after_value";  # After a value
const DPQL_CTX_IN_FIELD = "in_field";        # Inside a field reference (after @)
const DPQL_CTX_IN_STRING = "in_string";      # Inside a string
const DPQL_CTX_IN_PAREN = "in_paren";        # Inside parentheses

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
        @return a list of completion items
    */
    list<hash<DpqlCompletionItem>> getCompletions(string text, int position,
            hash<string, AbstractDataField> fields) {
        return getCompletionsWithContext(<DpqlCompletionContext>{
            "text": text,
            "position": position,
            "fields": fields,
            "expressions": expressions,
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
                # Can start with field, literal, function, or NOT
                items += getFieldCompletions(ctx.fields, analysis.prefix);
                items += getValueCompletions(analysis.prefix);
                items += getFunctionCompletions(analysis.prefix);
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

            case DPQL_CTX_IN_FIELD:
                # Completing field name after @
                items += getFieldCompletions(ctx.fields, analysis.prefix);
                break;

            case DPQL_CTX_AFTER_FIELD:
                # After field, suggest comparison operators
                items += getOperatorCompletions(analysis.prefix, True);
                break;

            case DPQL_CTX_AFTER_VALUE:
                # After value, suggest logical operators or more comparisons
                items += getOperatorCompletions(analysis.prefix, False);
                break;

            case DPQL_CTX_IN_STRING:
                # Inside string - no completions
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
        int token_start = position;
        while (token_start > 0 && before[token_start - 1] !~ /[\s(),]/) {
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
    private list<hash<DpqlCompletionItem>> getFunctionCompletions(string prefix) {
        list<hash<DpqlCompletionItem>> items = ();

        if (!expressions) {
            return items;
        }

        string lwr_prefix = prefix.lwr();

        foreach string name in (keys expressions) {
            hash<DataProviderExpressionInfo> info = expressions{name};
            # Only include function-type expressions
            if (info.type == DET_Function) {
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
}

} # namespace DataProvider
