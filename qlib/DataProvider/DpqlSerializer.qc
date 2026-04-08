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
const DPQL_PREC_MULTIPLICATIVE = 1;
const DPQL_PREC_ADDITIVE = 2;
const DPQL_PREC_UNARY = 3;
const DPQL_PREC_COMPARISON = 4;
const DPQL_PREC_AND = 5;
const DPQL_PREC_OR = 6;

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
        #! Map of expression name to DPQL symbol (stripped of trailing "()")
        /** @since DataProvider 3.4
        */
        hash<string, string> name_to_symbol;
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
            DP_OP_PLUS: DPQL_PREC_ADDITIVE,
            DP_OP_MINUS: DPQL_PREC_ADDITIVE,
            DP_OP_MULT: DPQL_PREC_MULTIPLICATIVE,
            DP_OP_DIV: DPQL_PREC_MULTIPLICATIVE,
            DP_OP_MOD: DPQL_PREC_MULTIPLICATIVE,
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
            DP_OP_PLUS: "+",
            DP_OP_MINUS: "-",
            DP_OP_MULT: "*",
            DP_OP_DIV: "/",
            DP_OP_MOD: "%",
        };
    }

    #! Creates a serializer with optional expression map
    /** @param expressions optional map of expression names to their info
    */
    constructor(*hash<string, hash<DataProviderExpressionInfo>> expressions) {
        self.expressions = expressions;
        if (expressions) {
            buildNameToSymbolMap();
        }
    }

    #! Builds a name-to-symbol lookup map from the expression map
    /** @since DataProvider 3.4
    */
    private buildNameToSymbolMap() {
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
        int prec = OperatorPrecedence{op}
            ?? (expressions{op} && inlist("Comparison", expressions{op}.groups)
                ? DPQL_PREC_COMPARISON : DPQL_PREC_PRIMARY);
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

            case DP_OP_PLUS:
            case DP_OP_MINUS:
            case DP_OP_MULT:
            case DP_OP_DIV:
            case DP_OP_MOD:
                result = serializeArithmeticOp(exp.args, OperatorSymbols{op}, prec);
                break;

            case DP_EXPR_VALUE:
                # Value expression - just serialize the argument (field reference)
                result = serializeArg(exp.args[0], DPQL_PREC_PRIMARY);
                break;

            case DP_EXPR_TEMPLATE:
                # Template expression - serialize the template reference
                result = serializeArg(exp.args[0], DPQL_PREC_PRIMARY);
                break;

            case DP_FUNC_INDEX:
                # Render as sugar: base[n] only if index arg is a literal
                if (isAccessorArgLiteral(exp.args[1])) {
                    result = serializeArg(exp.args[0], DPQL_PREC_PRIMARY)
                        + "[" + string(exp.args[1]) + "]";
                } else {
                    result = serializeFunctionCall(op, exp.args);
                }
                break;

            case DP_FUNC_SLICE:
                # Render as sugar: base[items...] only if all args are literals/ranges
                if (areSliceArgsLiteral(exp.args)) {
                    result = serializeArg(exp.args[0], DPQL_PREC_PRIMARY)
                        + "[" + serializeSliceArgs(exp.args) + "]";
                } else {
                    result = serializeFunctionCall(op, exp.args);
                }
                break;

            case DP_FUNC_KEY:
                # Render as sugar: base{keys...} only if all key args are literals
                if (areKeyArgsLiteral(exp.args)) {
                    result = serializeArg(exp.args[0], DPQL_PREC_PRIMARY)
                        + "{" + serializeKeyArgs(exp.args) + "}";
                } else {
                    result = serializeFunctionCall(op, exp.args);
                }
                break;

            default:
                # Check for provider-specific infix comparison operators (e.g. "like")
                if (expressions{op} && expressions{op}.type == DET_Operator
                        && inlist("Comparison", expressions{op}.groups)
                        && exp.args.size() == 2) {
                    string sym = name_to_symbol{op} ?? op;
                    result = serializeComparisonOp(exp.args, sym);
                } else {
                    # Unknown operator - treat as function call
                    result = serializeFunctionCall(op, exp.args);
                }
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

    #! Serializes an arithmetic binary operator with correct precedence
    private string serializeArithmeticOp(list<auto> args, string symbol, int prec) {
        if (args.size() < 2) {
            return "";
        }
        return serializeArg(args[0], prec) + " " + symbol + " " +
            serializeArg(args[1], prec);
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

        # Build flags from args[2] (list of flag name strings) or legacy individual int args
        string flags = "";
        if (args.size() > 2) {
            if (args[2].typeCode() == NT_LIST) {
                # New format: args[2] is a list of flag name strings
                foreach string flag in (args[2]) {
                    # Accept both short ("i") and long ("RE_Caseless") names
                    switch (flag) {
                        case "i":
                        case "RE_Caseless":
                            flags += "i";
                            break;
                        case "s":
                        case "RE_DotAll":
                            flags += "s";
                            break;
                        case "m":
                        case "RE_MultiLine":
                            flags += "m";
                            break;
                        case "x":
                        case "RE_Extended":
                            flags += "x";
                            break;
                        case "u":
                        case "RE_Unicode":
                            flags += "u";
                            break;
                    }
                }
            } else {
                # Legacy format: individual integer constants at args[2..]
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
            }
        }

        return field + " =~ /" + pattern + "/" + flags;
    }

    #! Serializes a function call using the DPQL symbol
    /** Uses the symbol from the expression map when available, falling back to the
        internal name for backward compatibility.

        @since DataProvider 3.4 symbol-based output
    */
    private string serializeFunctionCall(string name, list<auto> args) {
        # Use symbol for DPQL output when available
        string sym = name_to_symbol{name} ?? name;
        list<string> arg_strs = ();
        foreach auto arg in (args) {
            arg_strs += serializeArg(arg, DPQL_PREC_OR + 1);
        }
        return sym + "(" + arg_strs.join(", ") + ")";
    }

    #! Serializes an argument (value, field ref, or expression)
    private string serializeArg(auto arg, int context_prec) {
        if (arg === NOTHING) {
            return "null";
        }

        switch (arg.typeCode()) {
            case NT_HASH:
                if (arg instanceof hash<DataProviderTemplateReference>) {
                    return serializeTemplateRef(arg);
                }
                if (arg instanceof hash<DataProviderFieldReference>) {
                    return serializeFieldRef(arg);
                }
                if (arg instanceof hash<DataProviderExpression>) {
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

    #! Serializes a template reference
    /** Uses the \c raw field for perfect round-trip fidelity. Falls back to reconstruction
        from parts if \c raw is empty.

        @since DataProvider 3.3
    */
    private string serializeTemplateRef(hash<auto> ref) {
        if (ref.raw) {
            return ref.raw;
        }
        # Reconstruct from parts
        string result = "$" + ref.tmpl_context + ":";
        if (ref.tmpl_value =~ /[^a-zA-Z0-9_.*]/) {
            result += "{" + ref.tmpl_value + "}";
        } else {
            result += ref.tmpl_value;
        }
        return result;
    }

    #! Serializes a field reference
    private string serializeFieldRef(hash<auto> ref) {
        string field = ref.field;

        # Check if field name needs quoting
        if (field =~ /[^a-zA-Z0-9_.]/ || field =~ /^[0-9]/) {
            # Escape special characters: backslash first, then quote
            # Note: use '\"' not '\\"' - in single quotes, \\ is two backslashes
            field = replace(replace(field, "\\", "\\\\"), '"', '\"');
            return '@"' + field + '"';
        }
        return "@" + field;
    }

    #! Returns True if an accessor arg is a literal (not an expression)
    private bool isAccessorArgLiteral(auto arg) {
        if (arg.typeCode() == NT_HASH) {
            # Range specs (start/end keys) are literals; expression hashes have "exp" key
            return !arg.hasKey("exp");
        }
        return True;
    }

    #! Returns True if all slice args (args[1..]) are literals or range specs
    private bool areSliceArgsLiteral(list<auto> args) {
        for (int i = 1; i < args.size(); ++i) {
            if (!isAccessorArgLiteral(args[i])) {
                return False;
            }
        }
        return True;
    }

    #! Returns True if all key args (args[1..]) are literals
    private bool areKeyArgsLiteral(list<auto> args) {
        for (int i = 1; i < args.size(); ++i) {
            if (!isAccessorArgLiteral(args[i])) {
                return False;
            }
        }
        return True;
    }

    #! Serializes slice arguments from a DP_FUNC_SLICE expression (args[1..])
    private string serializeSliceArgs(list<auto> args) {
        list<string> parts = ();
        for (int i = 1; i < args.size(); ++i) {
            auto item = args[i];
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

    #! Serializes key arguments from a DP_FUNC_KEY expression (args[1..])
    private string serializeKeyArgs(list<auto> args) {
        list<string> parts = ();
        for (int i = 1; i < args.size(); ++i) {
            string skey = string(args[i]);
            # Check if key needs quoting
            if (skey =~ /[^a-zA-Z0-9_]/ || skey =~ /^[0-9]/) {
                parts += serializeString(skey);
            } else {
                parts += skey;
            }
        }
        return parts.join(", ");
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
