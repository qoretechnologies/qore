# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DpqlSerializer.qc DPQL serializer implementation

/*  DpqlSerializer.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Operator precedence levels (lower = binds tighter)
const DPQL_PREC_PRIMARY = 0;
const DPQL_PREC_UNARY = 1;
const DPQL_PREC_COMPARISON = 2;
const DPQL_PREC_AND = 3;
const DPQL_PREC_OR = 4;

#! Serializer for DPQL (Data Provider Query Language) expressions
/** This class converts DataProviderExpression hashes back into human-readable
    DPQL query strings.

    @par Example:
    @code{.py}
DpqlSerializer ser();
hash<DataProviderExpression> exp = <DataProviderExpression>{
    "exp": "&&",
    "args": (
        <DataProviderExpression>{
            "exp": "==",
            "args": (<DataProviderFieldReference>{"field": "name"}, "John"),
        },
        <DataProviderExpression>{
            "exp": ">",
            "args": (<DataProviderFieldReference>{"field": "age"}, 18),
        },
    ),
};
string dpql = ser.serialize(exp);
# Result: @name == "John" && @age > 18
    @endcode
*/
public class DpqlSerializer {
    private {
        #! Expression map for rendering
        *hash<string, hash<DataProviderExpressionInfo>> expressions;
        #! Cached set of reserved identifiers
        *hash<string, bool> reserved_identifiers;

        #! Map of operators to their precedence
        const OperatorPrecedence = {
            DP_OP_OR: DPQL_PREC_OR,
            DP_OP_AND: DPQL_PREC_AND,
            DP_SEARCH_OP_EQ: DPQL_PREC_COMPARISON,
            DP_SEARCH_OP_NE: DPQL_PREC_COMPARISON,
            DP_SEARCH_OP_LT: DPQL_PREC_COMPARISON,
            DP_SEARCH_OP_LE: DPQL_PREC_COMPARISON,
            DP_SEARCH_OP_GT: DPQL_PREC_COMPARISON,
            DP_SEARCH_OP_GE: DPQL_PREC_COMPARISON,
            DP_SEARCH_OP_BETWEEN: DPQL_PREC_COMPARISON,
            DP_SEARCH_OP_IN: DPQL_PREC_COMPARISON,
            DP_SEARCH_OP_REGEX: DPQL_PREC_COMPARISON,
            DP_SEARCH_OP_NOT: DPQL_PREC_UNARY,
        };

        #! Map of operators to their DPQL symbols
        const OperatorSymbols = {
            DP_OP_OR: "||",
            DP_OP_AND: "&&",
            DP_SEARCH_OP_EQ: "==",
            DP_SEARCH_OP_NE: "!=",
            DP_SEARCH_OP_LT: "<",
            DP_SEARCH_OP_LE: "<=",
            DP_SEARCH_OP_GT: ">",
            DP_SEARCH_OP_GE: ">=",
            DP_SEARCH_OP_NOT: "!",
        };
    }

    #! Creates a serializer with optional expression map
    /** @param expressions optional map of expression names to their info
    */
    constructor(*hash<string, hash<DataProviderExpressionInfo>> expressions) {
        self.expressions = expressions;
    }

    #! Serializes an expression to DPQL string
    /** @param exp the expression to serialize
        @return the DPQL string representation
    */
    string serialize(hash<DataProviderExpression> exp) {
        return serializeExpr(exp, DPQL_PREC_OR + 1);
    }

    #! Serializes an expression with context precedence
    /** @param exp the expression to serialize
        @param context_prec the precedence of the surrounding context
        @return the DPQL string representation
    */
    private string serializeExpr(hash<DataProviderExpression> exp, int context_prec) {
        string op = exp.exp;
        int prec = OperatorPrecedence{op} ?? DPQL_PREC_PRIMARY;
        bool need_parens = prec > context_prec;

        string result;

        switch (op) {
            case DP_OP_OR:
                result = serializeBinaryOp(exp.args, "||", DPQL_PREC_OR);
                break;

            case DP_OP_AND:
                result = serializeBinaryOp(exp.args, "&&", DPQL_PREC_AND);
                break;

            case DP_SEARCH_OP_EQ:
            case DP_SEARCH_OP_NE:
            case DP_SEARCH_OP_LT:
            case DP_SEARCH_OP_LE:
            case DP_SEARCH_OP_GT:
            case DP_SEARCH_OP_GE:
                result = serializeComparisonOp(exp.args, OperatorSymbols{op});
                break;

            case DP_SEARCH_OP_NOT:
                result = serializeNotOp(exp.args);
                break;

            case DP_SEARCH_OP_BETWEEN:
                result = serializeBetweenOp(exp.args);
                break;

            case DP_SEARCH_OP_IN:
                result = serializeInOp(exp.args);
                break;

            case DP_SEARCH_OP_REGEX:
                result = serializeRegexOp(exp.args);
                break;

            case DP_EXPR_VALUE:
                # Value expression - just serialize the argument (field reference)
                result = serializeArg(exp.args[0], DPQL_PREC_PRIMARY);
                break;

            default:
                # Unknown operator - treat as function call
                result = serializeFunctionCall(op, exp.args);
        }

        if (need_parens) {
            return "(" + result + ")";
        }
        return result;
    }

    #! Serializes a binary operator with multiple arguments (&&, ||)
    private string serializeBinaryOp(list<auto> args, string symbol, int prec) {
        list<string> parts = ();
        foreach auto arg in (args) {
            parts += serializeArg(arg, prec);
        }
        return parts.join(" " + symbol + " ");
    }

    #! Serializes a comparison operator
    private string serializeComparisonOp(list<auto> args, string symbol) {
        if (args.size() < 2) {
            return "";
        }
        return serializeArg(args[0], DPQL_PREC_COMPARISON) + " " + symbol + " " +
            serializeArg(args[1], DPQL_PREC_COMPARISON);
    }

    #! Serializes the NOT operator
    private string serializeNotOp(list<auto> args) {
        if (!args) {
            return "!";
        }
        # serializeArg already adds parentheses based on precedence
        return "!" + serializeArg(args[0], DPQL_PREC_UNARY);
    }

    #! Serializes the BETWEEN operator
    private string serializeBetweenOp(list<auto> args) {
        if (args.size() < 3) {
            return "";
        }
        return serializeArg(args[0], DPQL_PREC_COMPARISON) + " between " +
            serializeArg(args[1], DPQL_PREC_COMPARISON) + " and " +
            serializeArg(args[2], DPQL_PREC_COMPARISON);
    }

    #! Serializes the IN operator
    private string serializeInOp(list<auto> args) {
        if (!args) {
            return "";
        }
        string field = serializeArg(args[0], DPQL_PREC_COMPARISON);
        list<string> values = ();
        for (int i = 1; i < args.size(); ++i) {
            values += serializeArg(args[i], DPQL_PREC_PRIMARY);
        }
        return field + " in (" + values.join(", ") + ")";
    }

    #! Serializes the REGEX operator
    private string serializeRegexOp(list<auto> args) {
        if (args.size() < 2) {
            return "";
        }
        string field = serializeArg(args[0], DPQL_PREC_COMPARISON);
        string pattern = args[1].type() == "string" ? args[1] : string(args[1]);

        # Escape forward slashes in pattern
        pattern = replace(pattern, "/", "\\/");

        # Build flags from remaining args
        string flags = "";
        for (int i = 2; i < args.size(); ++i) {
            if (args[i].typeCode() == NT_INT) {
                switch (args[i]) {
                    case RE_Caseless:
                        flags += "i";
                        break;
                    case RE_DotAll:
                        flags += "s";
                        break;
                    case RE_MultiLine:
                        flags += "m";
                        break;
                    case RE_Extended:
                        flags += "x";
                        break;
                    case RE_Unicode:
                        flags += "u";
                        break;
                }
            }
        }

        return field + " =~ /" + pattern + "/" + flags;
    }

    #! Serializes a function call
    private string serializeFunctionCall(string name, list<auto> args) {
        list<string> arg_strs = ();
        foreach auto arg in (args) {
            arg_strs += serializeArg(arg, DPQL_PREC_OR + 1);
        }
        return name + "(" + arg_strs.join(", ") + ")";
    }

    #! Serializes an argument (value, field ref, or expression)
    private string serializeArg(auto arg, int context_prec) {
        if (arg === NOTHING) {
            return "null";
        }

        switch (arg.typeCode()) {
            case NT_HASH:
                if (arg.hasKey("field")) {
                    return serializeFieldRef(arg);
                }
                if (arg.hasKey("exp")) {
                    return serializeExpr(arg, context_prec);
                }
                # Structured hash literal
                return serializeStructuredLiteral(arg);

            case NT_LIST:
                # Structured list literal
                return serializeStructuredLiteral(arg);

            default:
                return serializeLiteral(arg);
        }
    }

    #! Serializes a field reference with optional accessor chain
    private string serializeFieldRef(hash<auto> ref) {
        string field = ref.field;
        string result;

        # Check if field name needs quoting
        if (field =~ /[^a-zA-Z0-9_.]/ || field =~ /^[0-9]/) {
            # Escape special characters
            field = replace(replace(field, "\\", "\\\\"), '"', '\\"');
            result = '@"' + field + '"';
        } else {
            result = "@" + field;
        }

        # Serialize accessor chain if present
        if (ref.accessors) {
            foreach hash<DataProviderFieldAccessor> acc in (ref.accessors) {
                result += serializeAccessor(acc);
            }
        }

        return result;
    }

    #! Serializes a single accessor
    private string serializeAccessor(hash<DataProviderFieldAccessor> acc) {
        switch (acc.type) {
            case DPQL_ACCESSOR_INDEX:
                return "[" + string(acc.value) + "]";

            case DPQL_ACCESSOR_SLICE:
                return "[" + serializeSliceItems(acc.value) + "]";

            case DPQL_ACCESSOR_KEY:
                return "{" + serializeKeyList(acc.value) + "}";

            default:
                throw "DPQL-SERIALIZE-ERROR", sprintf("Unknown accessor type: %y", acc.type);
        }
    }

    #! Serializes slice items (indices and ranges)
    private string serializeSliceItems(list<auto> items) {
        list<string> parts = ();
        foreach auto item in (items) {
            if (item.typeCode() == NT_HASH && (item.hasKey("start") || item.hasKey("end"))) {
                # Range spec
                parts += serializeRange(item);
            } else {
                # Single index
                parts += string(item);
            }
        }
        return parts.join(",");
    }

    #! Serializes a range: start..end, ..end, start..
    private string serializeRange(hash<auto> range) {
        string result = "";
        if (exists range.start) {
            result += string(range.start);
        }
        result += "..";
        if (exists range.end) {
            result += string(range.end);
        }
        return result;
    }

    #! Serializes key list for key accessor
    private string serializeKeyList(list<string> key_list) {
        list<string> parts = ();
        foreach string key in (key_list) {
            # Check if key needs quoting
            if (key =~ /[^a-zA-Z0-9_]/ || key =~ /^[0-9]/) {
                parts += serializeString(key);
            } else {
                parts += key;
            }
        }
        return parts.join(", ");
    }

    #! Serializes a literal value
    private string serializeLiteral(auto value) {
        if (value === NOTHING) {
            return "null";
        }

        switch (value.typeCode()) {
            case NT_STRING:
                return serializeString(value);

            case NT_INT:
            case NT_FLOAT:
            case NT_NUMBER:
                return string(value);

            case NT_BOOLEAN:
                return value ? "true" : "false";

            case NT_DATE:
                # Serialize dates as ISO-8601 literal
                return value.format("IF");

            case NT_BINARY:
                # Serialize binary as hex literal
                return "<" + value.toHex() + ">";

            case NT_LIST:
            case NT_HASH:
                return serializeStructuredLiteral(value);

            default:
                # For other types, try to convert to string
                return serializeString(string(value));
        }
    }

    #! Serializes list/hash literals using parse_to_qore_value-compatible syntax
    private string serializeStructuredLiteral(auto value) {
        switch (value.typeCode()) {
            case NT_LIST:
                return serializeStructuredList(value);
            case NT_HASH:
                return serializeStructuredHash(value);
            default:
                return serializeStructuredValue(value);
        }
    }

    private string serializeStructuredList(list<auto> value) {
        list<string> items = ();
        foreach auto item in (value) {
            items += serializeStructuredValue(item);
        }
        return "(" + items.join(",") + ")";
    }

    private string serializeStructuredHash(hash<auto> value) {
        list<string> parts = ();
        foreach hash<auto> i in (value.pairIterator()) {
            parts += serializeStructuredKey(i.key) + "=" + serializeStructuredValue(i.value);
        }
        return "{" + parts.join(", ") + "}";
    }

    private string serializeStructuredKey(string key) {
        if (canEmitBareIdentifier(key)) {
            return key;
        }
        return serializeString(key);
    }

    private string serializeStructuredValue(auto value) {
        if (value === NOTHING) {
            return "null";
        }

        if (value.typeCode() == NT_STRING) {
            if (canEmitBareIdentifier(value)) {
                return value;
            }
            return serializeString(value);
        }

        if (value.typeCode() == NT_LIST || value.typeCode() == NT_HASH) {
            return serializeStructuredLiteral(value);
        }

        return serializeLiteral(value);
    }

    #! Returns True if the string can be emitted without quoting
    private bool canEmitBareIdentifier(string value) {
        if (!(value =~ /^[a-zA-Z_][a-zA-Z0-9_]*$/)) {
            return False;
        }
        return !getReservedIdentifiers(){value.lwr()};
    }

    private hash<string, bool> getReservedIdentifiers() {
        if (reserved_identifiers) {
            return reserved_identifiers;
        }

        hash<string, bool> reserved = {
            "true": True,
            "false": True,
            "null": True,
        };

        if (expressions) {
            foreach string key in (keys expressions) {
                if (key =~ /^[a-zA-Z_][a-zA-Z0-9_]*$/) {
                    reserved{key.lwr()} = True;
                }
                hash<DataProviderExpressionInfo> info = expressions{key};
                if (info.name && info.name =~ /^[a-zA-Z_][a-zA-Z0-9_]*$/) {
                    reserved{info.name.lwr()} = True;
                }
                if (info.symbol && info.symbol =~ /^[a-zA-Z_][a-zA-Z0-9_]*$/) {
                    reserved{info.symbol.lwr()} = True;
                }
            }
        }

        reserved_identifiers = reserved;
        return reserved_identifiers;
    }

    #! Serializes a string with proper escaping
    private string serializeString(string value) {
        string result = '"';
        for (int i = 0; i < value.size(); ++i) {
            string c = value[i];
            switch (c) {
                case '"':
                    result += '\\"';
                    break;
                case "\\":
                    result += "\\\\";
                    break;
                case "\n":
                    result += "\\n";
                    break;
                case "\r":
                    result += "\\r";
                    break;
                case "\t":
                    result += "\\t";
                    break;
                default:
                    result += c;
            }
        }
        return result + '"';
    }
}

} # namespace DataProvider
