# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ExpressionTypes.qc the type rules of expressions

/*  ExpressionTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Two arithmetic operands that an operator cannot combine
/** @see ExpressionTypes::checkArithmetic()

    @since DataProvider 3.9
*/
public hashdecl ExpressionArithmeticMismatch {
    #! The operator
    string op;

    #! The left operand: the first operand, or the result of combining the operands before the right one
    auto left;

    #! The kind of value of the left operand: a key of @ref ExpressionTypes::OperandKindLabels
    string left_kind;

    #! True if the left operand is the result of combining earlier operands, not an operand of the expression
    bool left_combined = False;

    #! The right operand
    auto right;

    #! The kind of value of the right operand: a key of @ref ExpressionTypes::OperandKindLabels
    string right_kind;
}

#! The type rules of expressions: the type that an expression gives, worked out before it is evaluated
/** These are the one set of type rules for expressions: DPQL validation (@ref DpqlParser), expression verification
    (@ref AbstractDataProvider::verifyExpression()) and every other user of expression types work out the type of
    an expression with @ref getType(), so an expression has the same type wherever it is used.

    The type of an expression is:
    - for a literal, the type of its value; for a field reference, the type of the field, including a field of a
      field (ex: \c \@customer.name); for a template reference, the type that the registered template type
      callback gives (@ref AbstractDataProvider::resolveTemplateType())
    - for a comparison, a logical operator (\c &&, \c ||, \c !), \c in, \c between and a regular expression
      match: \c bool
    - for arithmetic (\c +, \c -, \c *, \c /, \c mod): the type that %Qore arithmetic gives for the operands'
      types (@ref getArithmeticResultKind()); an integer with a float is a float, and any number with an
      arbitrary-precision number is a number; operands that the operator cannot combine (ex: a hash plus an
      integer, which evaluates to the integer) have no type, and are reported by @ref checkArithmetic()
    - for a key or an index of a value whose type describes its fields or elements: the type of the field or of
      the elements
    - for any other expression, from its definition: the type of its first argument when it gives the type of its
      first argument (\c return_type_first_arg), the first of its \c return_type_arg_priority types that one of
      its arguments has, or its declared \c return_type

    A type that is only known when the expression is evaluated (\c any or \c auto, an unknown field, or an argument
    whose type is not known) is not given: @ref getType() returns @ref nothing.

    @since DataProvider 3.9
*/
public class ExpressionTypes {
    public {
        #! The arithmetic operators, with what each one accepts
        /** Arithmetic in expressions is %Qore arithmetic, which combines any two values: an operand that the
            operator cannot use is converted silently (a hash plus an integer is the integer), so the combinations
            that do not give a meaningful result have no type and are reported by validation
        */
        const ArithmeticAccepts = {
            DP_OP_PLUS: "it adds numbers, or a number and a date; joins a string with a string, number, date or "
                "boolean; appends to a list; and merges two hashes or two binary values",
            DP_OP_MINUS: "it subtracts numbers, or a date or number from a date; and removes keys (a string or a "
                "list of strings) from a hash",
            DP_OP_MULT: "it multiplies numbers",
            DP_OP_DIV: "it divides numbers",
            DP_OP_MOD: "it gives the remainder of the division of numbers",
        };

        #! How a diagnostic names each kind of operand value
        /** The numeric kinds are \c int, \c float, \c number and \c numeric (a value that can be more than one of
            them)
        */
        const OperandKindLabels = {
            "int": "a number",
            "float": "a number",
            "number": "a number",
            "numeric": "a number",
            "string": "a string",
            "date": "a date",
            "bool": "a boolean",
            "binary": "a binary value",
            "hash": "a hash",
            "list": "a list",
        };

        #! The operators that give \c bool: comparisons, logical operators and matches
        const ConditionOperators = {
            DP_SEARCH_OP_EQ: True,
            DP_SEARCH_OP_NE: True,
            DP_SEARCH_OP_LT: True,
            DP_SEARCH_OP_LE: True,
            DP_SEARCH_OP_GT: True,
            DP_SEARCH_OP_GE: True,
            DP_SEARCH_OP_IN: True,
            DP_OP_NOT_IN: True,
            DP_SEARCH_OP_BETWEEN: True,
            DP_SEARCH_OP_REGEX: True,
            DP_OP_REGEX_MATCH: True,
            DP_OP_REGEX_NMATCH: True,
            DP_SEARCH_OP_NOT: True,
            DP_OP_AND: True,
            DP_OP_OR: True,
        };
    }

    private {
        #! The numeric kinds of value
        const NumericKinds = {
            "int": True,
            "float": True,
            "number": True,
            "numeric": True,
        };

        #! The kinds of value that \c + joins with a string
        const StringJoinKinds = NumericKinds + {
            "string": True,
            "date": True,
            "bool": True,
        };
    }

    #! Returns the type that an expression, a field reference, a template reference or a literal value gives
    /** @param arg the expression (@ref DataProviderExpression), field reference
        (@ref DataProviderFieldReference), template reference (@ref DataProviderTemplateReference) or literal value
        @param fields the fields of the record that the expression is evaluated on, if known, for the types of
        field references
        @param expmap the definitions of the expressions that can be used, if any; an expression that it does not
        define is looked up in the custom expressions registered for generic evaluation and then in the generic
        expressions (@ref DataProviderGenericExpressions), also by its operator alias

        @return the type, or @ref nothing if the type is only known when the expression is evaluated

        @par Example:
        @code{.py}
# float
string name = ExpressionTypes::getType(DataProvider::parseDpqlExpression("1 + 2.5")).getName();
        @endcode
    */
    static *AbstractDataProviderType getType(auto arg, *hash<string, AbstractDataField> fields,
            *hash<string, hash<DataProviderExpressionInfo>> expmap) {
        switch (arg.typeCode()) {
            case NT_STRING:
                return AbstractDataProviderTypeMap."string";
            case NT_INT:
                return AbstractDataProviderTypeMap."int";
            case NT_FLOAT:
                return AbstractDataProviderTypeMap."float";
            case NT_NUMBER:
                return AbstractDataProviderTypeMap."number";
            case NT_BOOLEAN:
                return AbstractDataProviderTypeMap."bool";
            case NT_DATE:
                return AbstractDataProviderTypeMap."date";
            case NT_BINARY:
                return AbstractDataProviderTypeMap."binary";
            case NT_LIST:
                return AbstractDataProviderTypeMap."list";
            case NT_NULL:
            case NT_NOTHING:
                return AbstractDataProviderTypeMap."nothing";
            case NT_HASH:
                break;
            default:
                return;
        }
        if (arg instanceof hash<DataProviderExpression>) {
            return ExpressionTypes::getExpressionType(cast<hash<DataProviderExpression>>(arg), fields, expmap);
        }
        if (arg instanceof hash<DataProviderFieldReference>) {
            return ExpressionTypes::known(AbstractDataProvider::getFieldReferenceType(
                cast<hash<DataProviderFieldReference>>(arg), fields));
        }
        if (arg instanceof hash<DataProviderTemplateReference>) {
            return ExpressionTypes::known(AbstractDataProvider::resolveTemplateType(arg.tmpl_context,
                arg.tmpl_value));
        }
        # a hash literal
        return AbstractDataProviderTypeMap."hash";
    }

    #! Returns the definition of an expression
    /** @param name the name of the expression or an alias of an operator (ex: \c "PLUS")
        @param expmap the definitions of the expressions that can be used, if any; an expression that it does not
        define is looked up in the custom expressions registered for generic evaluation and then in the generic
        expressions (@ref DataProviderGenericExpressions), also by its operator alias

        @return the definition of the expression, or @ref nothing if it is not known
    */
    static *hash<DataProviderExpressionInfo> getExpressionInfo(string name,
            *hash<string, hash<DataProviderExpressionInfo>> expmap) {
        *string alias = OperatorAliasMap{name};
        if (*hash<DataProviderExpressionInfo> info = expmap{name} ?? (alias ? expmap{alias} : NOTHING)) {
            return info;
        }
        if (*hash<auto> custom = get_app_registry("dp-custom-expressions")) {
            if (*hash<auto> impl = custom{name} ?? (alias ? custom{alias} : NOTHING)) {
                if (impl.exp) {
                    return impl.exp;
                }
            }
        }
        return DataProviderGenericExpressions{name} ?? (alias ? DataProviderGenericExpressions{alias} : NOTHING);
    }

    #! Returns the kind of value of a type: one of the keys of @ref OperandKindLabels, or @ref nothing
    /** A type that also accepts no value has the kind of its value (ex: \c *int is \c int).

        @return @ref nothing if the type holds values of more than one kind (ex: \c data is a string or a binary
        value) or of another kind (ex: any value); a type that holds more than one kind of number is \c numeric
    */
    static *string getKind(*AbstractDataProviderType type) {
        if (!type) {
            return;
        }
        *string rv;
        foreach string name in (keys type.getReturnTypeHash()) {
            *string kind;
            switch (name) {
                case "int":
                case "integer":
                    kind = "int";
                    break;
                case "float":
                case "number":
                case "string":
                case "date":
                case "bool":
                case "binary":
                    kind = name;
                    break;
                case "nothing":
                case "null":
                    # an optional value
                    continue;
                default:
                    if (name =~ /^(soft)?list\b/) {
                        kind = "list";
                    } else if (name =~ /^hash\b/) {
                        kind = "hash";
                    }
                    break;
            }
            if (!kind) {
                return;
            }
            if (rv && rv != kind) {
                if (!NumericKinds{rv} || !NumericKinds{kind}) {
                    return;
                }
                kind = "numeric";
            }
            rv = kind;
        }
        return rv;
    }

    #! Returns the kind of value that an arithmetic operator gives for two kinds of operand
    /** These are the results of %Qore arithmetic:
        - numbers: an integer with an integer is an integer, a float with an integer or a float is a float, and
          any number with an arbitrary-precision number is a number; \c mod gives an integer
        - \c + appends to (or prepends to) a list; joins a string with a string, a number, a date or a boolean;
          adds a number to a date; and merges two hashes or two binary values
        - \c - subtracts a date or a number from a date, and removes keys (a string or a list) from a hash

        @param op the operator: a key of @ref ArithmeticAccepts
        @param left the kind of the left operand: a key of @ref OperandKindLabels
        @param right the kind of the right operand: a key of @ref OperandKindLabels

        @return the kind of the result, or @ref nothing if the operator cannot combine the operands: %Qore would
        convert one of them silently (ex: a hash plus an integer is the integer)
    */
    static *string getArithmeticResultKind(string op, string left, string right) {
        bool numeric = NumericKinds{left} && NumericKinds{right};
        switch (op) {
            case DP_OP_PLUS:
                if (left == "list" || right == "list") {
                    return "list";
                }
                if (numeric) {
                    return ExpressionTypes::getNumericResultKind(left, right);
                }
                if (left == "string" || right == "string") {
                    return StringJoinKinds{left == "string" ? right : left} ? "string" : NOTHING;
                }
                if ((left == "date" && (right == "date" || NumericKinds{right}))
                        || (NumericKinds{left} && right == "date")) {
                    return "date";
                }
                if (left == right && (left == "hash" || left == "binary")) {
                    return left;
                }
                return;

            case DP_OP_MINUS:
                if (numeric) {
                    return ExpressionTypes::getNumericResultKind(left, right);
                }
                if (left == "date" && (right == "date" || NumericKinds{right})) {
                    return "date";
                }
                if (left == "hash" && (right == "string" || right == "list")) {
                    return "hash";
                }
                return;

            case DP_OP_MOD:
                return numeric ? "int" : NOTHING;
        }
        # multiplication and division
        return numeric ? ExpressionTypes::getNumericResultKind(left, right) : NOTHING;
    }

    #! Returns the kind of number that arithmetic on two kinds of number gives
    /** Any number with an arbitrary-precision number is a number; otherwise a float with an integer or a float is a
        float, and an integer with an integer is an integer.  An operand that can be more than one kind of number
        (\c numeric) gives a number with a number, and otherwise more than one kind of number.
    */
    private static string getNumericResultKind(string left, string right) {
        if (left == "number" || right == "number") {
            return "number";
        }
        if (left == "numeric" || right == "numeric") {
            return "numeric";
        }
        return (left == "float" || right == "float") ? "float" : "int";
    }

    #! Returns the type of a kind of value
    /** @param kind a kind of value: a key of @ref OperandKindLabels

        @return the type, or @ref nothing for \c numeric, which is more than one type
    */
    static *AbstractDataProviderType getKindType(string kind) {
        return kind == "numeric" ? NOTHING : AbstractDataProviderTypeMap{kind};
    }

    #! Returns the first two operands of an arithmetic expression that its operator cannot combine
    /** The operands whose kind of value is known are checked from left to right, as the operator combines them;
        an operand whose kind is not known cannot be checked, nor can what the operator gives with it.  Nested
        arithmetic gives the kind that its operands give; operands of nested arithmetic that cannot be combined are
        not returned here, but when the nested arithmetic itself is checked, and give no kind.

        @param exp the arithmetic expression: its operator is a key of @ref ArithmeticAccepts
        @param fields the fields of the record that the expression is evaluated on, if known
        @param expmap the definitions of the expressions that can be used, if any (see @ref getType())

        @return the operands that cannot be combined, or @ref nothing if all known operands can be combined
    */
    static *hash<ExpressionArithmeticMismatch> checkArithmetic(hash<DataProviderExpression> exp,
            *hash<string, AbstractDataField> fields, *hash<string, hash<DataProviderExpressionInfo>> expmap) {
        *hash<ExpressionArithmeticMismatch> mismatch;
        ExpressionTypes::combine(exp, fields, expmap, \mismatch);
        return mismatch;
    }

    #! Returns the kind of value that an arithmetic operand gives, or @ref nothing if it is not known
    /** @param arg an operand of an arithmetic expression
        @param fields the fields of the record that the expression is evaluated on, if known
        @param expmap the definitions of the expressions that can be used, if any (see @ref getType())

        @return the kind: a key of @ref OperandKindLabels, or @ref nothing if it is not known or if the operand is
        arithmetic whose operands cannot be combined
    */
    static *string getOperandKind(auto arg, *hash<string, AbstractDataField> fields,
            *hash<string, hash<DataProviderExpressionInfo>> expmap) {
        return ExpressionTypes::getOperandKindIntern(arg, fields, expmap);
    }

    #! Returns the type of an expression
    private static *AbstractDataProviderType getExpressionType(hash<DataProviderExpression> exp,
            *hash<string, AbstractDataField> fields, *hash<string, hash<DataProviderExpressionInfo>> expmap) {
        if (exp.exp == DP_EXPR_VALUE) {
            return exp.args.lsize() == 1 ? ExpressionTypes::getType(exp.args[0], fields, expmap) : NOTHING;
        }
        if (ConditionOperators{exp.exp}) {
            return AbstractDataProviderTypeMap."bool";
        }
        string op = OperatorAliasMap{exp.exp} ?? exp.exp;
        if (ArithmeticAccepts{op}) {
            *hash<ExpressionArithmeticMismatch> mismatch;
            *string kind = ExpressionTypes::combine(exp, fields, expmap, \mismatch);
            return kind ? ExpressionTypes::getKindType(kind) : NOTHING;
        }
        switch (op) {
            case DP_FUNC_KEY:
                return ExpressionTypes::getKeyType(exp, fields, expmap);
            case DP_FUNC_INDEX:
                return ExpressionTypes::getIndexType(exp, fields, expmap);
        }
        *hash<DataProviderExpressionInfo> info = ExpressionTypes::getExpressionInfo(exp.exp, expmap);
        if (!info) {
            return;
        }
        if (info.return_type_first_arg) {
            return exp.args ? ExpressionTypes::getType(exp.args[0], fields, expmap) : NOTHING;
        }
        if (info.return_type_arg_priority && exp.args) {
            hash<string, bool> given = {};
            foreach auto arg in (exp.args) {
                *string kind = ExpressionTypes::getKind(ExpressionTypes::getType(arg, fields, expmap));
                if (!kind || kind == "numeric") {
                    # the type is known only when the argument is evaluated
                    return;
                }
                given{kind} = True;
            }
            foreach string type in (info.return_type_arg_priority) {
                if (given{type == "integer" ? "int" : type}) {
                    return AbstractDataProviderTypeMap{type};
                }
            }
        }
        return ExpressionTypes::known(info.return_type);
    }

    #! Returns the kind of value that an arithmetic expression gives, recording the first operands it cannot combine
    private static *string combine(hash<DataProviderExpression> exp, *hash<string, AbstractDataField> fields,
            *hash<string, hash<DataProviderExpressionInfo>> expmap,
            reference<*hash<ExpressionArithmeticMismatch>> mismatch) {
        if (exp.args.lsize() < 2) {
            return;
        }
        string op = OperatorAliasMap{exp.exp} ?? exp.exp;
        *string left = ExpressionTypes::getOperandKindIntern(exp.args[0], fields, expmap);
        auto left_arg = exp.args[0];
        bool left_combined = False;
        for (int i = 1; i < exp.args.size(); ++i) {
            *string right = ExpressionTypes::getOperandKindIntern(exp.args[i], fields, expmap);
            if (!left || !right) {
                # an operand whose kind is not known cannot be checked, nor can what the operator gives with it
                left = NOTHING;
                continue;
            }
            *string result = ExpressionTypes::getArithmeticResultKind(op, left, right);
            if (!result) {
                if (!mismatch) {
                    mismatch = <ExpressionArithmeticMismatch>{
                        "op": op,
                        "left": left_arg,
                        "left_kind": left,
                        "left_combined": left_combined,
                        "right": exp.args[i],
                        "right_kind": right,
                    };
                }
                return;
            }
            left = result;
            left_combined = True;
        }
        return left;
    }

    #! Returns the kind of value of an arithmetic operand
    private static *string getOperandKindIntern(auto arg, *hash<string, AbstractDataField> fields,
            *hash<string, hash<DataProviderExpressionInfo>> expmap) {
        if (arg instanceof hash<DataProviderExpression>
                && ArithmeticAccepts{OperatorAliasMap{arg.exp} ?? arg.exp}) {
            # the operands of nested arithmetic are reported when it is checked itself
            *hash<ExpressionArithmeticMismatch> nested_mismatch;
            return ExpressionTypes::combine(cast<hash<DataProviderExpression>>(arg), fields, expmap,
                \nested_mismatch);
        }
        return ExpressionTypes::getKind(ExpressionTypes::getType(arg, fields, expmap));
    }

    #! Returns the type of a key of a value, if the type of the value describes the field
    private static *AbstractDataProviderType getKeyType(hash<DataProviderExpression> exp,
            *hash<string, AbstractDataField> fields, *hash<string, hash<DataProviderExpressionInfo>> expmap) {
        if (exp.args.lsize() != 2 || exp.args[1].typeCode() != NT_STRING) {
            return;
        }
        *AbstractDataProviderType type = ExpressionTypes::getType(exp.args[0], fields, expmap);
        return type ? ExpressionTypes::known(type.getFieldType(exp.args[1])) : NOTHING;
    }

    #! Returns the type of an element of a list, if the type of the list describes its elements
    private static *AbstractDataProviderType getIndexType(hash<DataProviderExpression> exp,
            *hash<string, AbstractDataField> fields, *hash<string, hash<DataProviderExpressionInfo>> expmap) {
        if (exp.args.lsize() != 2 || exp.args[1].typeCode() != NT_INT) {
            return;
        }
        *AbstractDataProviderType type = ExpressionTypes::getType(exp.args[0], fields, expmap);
        return type && type.isList() ? ExpressionTypes::known(type.getElementType()) : NOTHING;
    }

    #! Returns the type if it is known before the expression is evaluated: not \c any or \c auto
    private static *AbstractDataProviderType known(*AbstractDataProviderType type) {
        return type && type.hasType() ? type : NOTHING;
    }
}
}
