# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreFilterRecordsProcessor class definition

/** QoreFilterRecordsProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that filters records based on expressions or simple field comparisons
public class QoreFilterRecordsProcessor inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreFilterRecordsProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_FILTER,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "where_cond": <DataProviderOptionInfo>{
                "display_name": "Filter Expression",
                "short_desc": "A structured filter expression",
                "desc": "A structured filter expression (hash<DataProviderExpression>) built by the "
                    "expression builder; mutually exclusive with the DPQL expression and "
                    "field/operator/value options",
                "type": AbstractDataProviderTypeMap."*hash",
                "required": False,
                "required_groups": ("filter-mode",),
                "exclusive_with": ("expression", "field", "operator", "value"),
            },
            "expression": <DataProviderOptionInfo>{
                "display_name": "DPQL Expression",
                "short_desc": "A DPQL filter expression",
                "desc": "A DPQL expression string for filtering records (e.g. "
                    "`@status == \"active\" && @amount > 100`); mutually exclusive with the "
                    "field/operator/value and where_cond options",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "required_groups": ("filter-mode",),
                "exclusive_with": ("where_cond", "field", "operator", "value"),
            },
            "field": <DataProviderOptionInfo>{
                "display_name": "Field Name",
                "short_desc": "Field name for simple single-field filtering",
                "desc": "The name of the record field to compare; used with `operator` and `value` "
                    "for simple filtering; mutually exclusive with `expression` and `where_cond`",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "required_groups": ("filter-mode",),
                "exclusive_with": ("where_cond", "expression"),
            },
            "operator": <DataProviderOptionInfo>{
                "display_name": "Comparison Operator",
                "short_desc": "Comparison operator for field filtering",
                "desc": "The comparison operator to use when filtering by field; one of: "
                    "`==`, `!=`, `<`, `<=`, `>`, `>=`, `in`, `between`, `regex`",
                "type": AbstractDataProviderTypeMap."string",
                "required": False,
                "required_groups": ("filter-mode",),
                "exclusive_with": ("where_cond", "expression"),
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "==",
                        "desc": "Equal to",
                    },
                    <AllowedValueInfo>{
                        "value": "!=",
                        "desc": "Not equal to",
                    },
                    <AllowedValueInfo>{
                        "value": "<",
                        "desc": "Less than",
                    },
                    <AllowedValueInfo>{
                        "value": "<=",
                        "desc": "Less than or equal to",
                    },
                    <AllowedValueInfo>{
                        "value": ">",
                        "desc": "Greater than",
                    },
                    <AllowedValueInfo>{
                        "value": ">=",
                        "desc": "Greater than or equal to",
                    },
                    <AllowedValueInfo>{
                        "value": "in",
                        "desc": "Value is in a set of values",
                    },
                    <AllowedValueInfo>{
                        "value": "between",
                        "desc": "Value is between two values (exclusive)",
                    },
                    <AllowedValueInfo>{
                        "value": "regex",
                        "desc": "Value matches a regular expression",
                    },
                ),
            },
            "value": <DataProviderOptionInfo>{
                "display_name": "Comparison Value",
                "short_desc": "Value to compare against",
                "desc": "The value to compare the field against; for the `in` operator, provide a list; "
                    "for `between`, provide a list with two elements",
                "type": AbstractDataProviderTypeMap."auto",
                "required": False,
                "required_groups": ("filter-mode",),
                "exclusive_with": ("where_cond", "expression"),
            },
        };
    }

    private {
        #! The parsed where expression
        hash<DataProviderExpression> where_expr;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.where_cond) {
            where_expr = cast<hash<DataProviderExpression>>(copts.where_cond);
        } else if (copts.expression.val()) {
            DpqlParser parser();
            where_expr = parser.parse(copts.expression);
        } else if (copts.field.val() && copts.operator.val() && exists copts.value) {
            if (copts.operator == "between") {
                if (copts.value.typeCode() != NT_LIST || copts.value.size() != 2) {
                    throw "CONSTRUCTOR-ERROR", "'between' operator requires a list of exactly 2 values "
                        "[lower_bound, upper_bound]";
                }
                where_expr = <DataProviderExpression>{
                    "exp": copts.operator,
                    "args": (<DataProviderFieldReference>{"field": copts.field},
                        copts.value[0], copts.value[1]),
                };
            } else {
                where_expr = <DataProviderExpression>{
                    "exp": copts.operator,
                    "args": (<DataProviderFieldReference>{"field": copts.field}, copts.value),
                };
            }
        } else if (copts.field.val() || copts.operator.val() || exists copts.value) {
            throw "CONSTRUCTOR-ERROR", "when using simple filtering, all of 'field', 'operator', and 'value' "
                "must be provided";
        }
        # if no options at all, the processor is in navigation mode
    }

    #! Returns the name of this data provider
    string getName() {
        return "filter";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Filters records based on a DPQL expression or a simple field/operator/value comparison; "
            "records matching the condition are passed through, non-matching records are dropped";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns action options for the pipeline processor action with dynamic \c where_cond option
    /** The \c where_cond option is added with \c supports_expressions set to @ref True so the expression builder
        UI can render the expression builder widget.

        @param action action info for the pipeline processor action
        @param options sample option data that can be used to determine the final type

        @return action options including the dynamic \c where_cond option for the expression builder

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForPipelineProcessorWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        return action.options + {
            "where_cond": <ActionOptionInfo>{
                "display_name": "Filter Expression",
                "desc": "A boolean expression to filter records; matching records pass through, "
                    "non-matching records are dropped",
                "type": AbstractDataProviderTypeMap."*bool",
                "supports_templates": False,
                "supports_custom_values": False,
                "supports_expressions": True,
                "server_expression_handling": False,
                "preselected": True,
                "loc": "constructor",
            },
        };
    }

    #! Processes a single record and returns the result
    private *auto processRecordImpl(auto record) {
        if (!where_expr) {
            throw "PROCESSOR-ERROR", "QoreFilterRecordsProcessor: no filter expression configured; "
                "provide 'expression' or 'field'/'operator'/'value' options";
        }
        return AbstractDataProvider::evalGenericExpression(record, where_expr) ? record : NOTHING;
    }

    #! Indicates that this processor does not support the bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_FILTER;
    }
}
}
