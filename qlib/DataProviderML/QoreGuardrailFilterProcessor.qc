# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreGuardrailFilterProcessor class definition

/** QoreGuardrailFilterProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Pipeline processor that filters records failing guardrail checks
/** Evaluates a text field against configured guardrail rules. Records that
    fail are dropped (return NOTHING). Records that pass are forwarded unchanged.

    This is a @ref DPC_FILTER processor because it either passes or drops records.

    @par Example:
    @code{.py}
QoreGuardrailFilterProcessor proc({
    "text_field": "body",
    "rules": (
        {"type": "pii", "severity": "critical", "params": {"action": "block"}},
    ),
});
    @endcode

*/
public class QoreGuardrailFilterProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreGuardrailFilterProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_FILTER,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "text_field": <DataProviderOptionInfo>{
                "display_name": "Text Field",
                "short_desc": "Which text field to check",
                "desc": "The name of the text field in each record to evaluate "
                    "against guardrail rules.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "rules": <DataProviderOptionInfo>{
                "display_name": "Guardrail Rules",
                "short_desc": "List of guardrail rule configurations",
                "desc": "A list of guardrail rule configurations. Each rule has:\n"
                    "- `type` — rule type (`pii`, `prompt_injection`, "
                    "`output_format`, `topic_restriction`)\n"
                    "- `severity` — `warning`, `high`, or `critical`\n"
                    "- `params` — rule-specific parameters\n\n"
                    "Records that fail any rule are dropped from the pipeline.",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."hash", True),
                "required": True,
            },
        };
    }

    private {
        #! Text field name
        string text_field;

        #! Guardrail engine
        QoreGuardrails::GuardrailEngine engine;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        text_field = copts.text_field;
        if (!text_field.val()) {
            throw "CONSTRUCTOR-ERROR", "text_field option is required";
        }
        if (!copts.rules) {
            throw "CONSTRUCTOR-ERROR", "rules option is required";
        }

        list<hash<QoreGuardrails::GuardrailRuleConfig>> rule_configs = ();
        foreach hash<auto> rule in (copts.rules) {
            push rule_configs, <QoreGuardrails::GuardrailRuleConfig>{
                "type": rule.type,
                "enabled": rule.enabled ?? True,
                "severity": rule.severity ?? QoreGuardrails::GS_HIGH,
                "params": rule.params,
            };
        }

        engine = new QoreGuardrails::GuardrailEngine(rule_configs);
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "guardrail-filter";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Drops records that fail guardrail checks — use before an LLM call to block "
            "PII, prompt injections, or off-topic queries from reaching the model";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Filters records: returns the record if it passes, NOTHING if it fails
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreGuardrailFilterProcessor: not configured; provide constructor options";
        }

        string text = DataProvider::AbstractDataProvider::getFieldValue(
            record, text_field).toString();

        hash<QoreGuardrails::GuardrailResult> result = engine.evaluateInput(text);

        if (result.passed) {
            return record;
        }

        # Record failed — drop it (return NOTHING)
        return NOTHING;
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_FILTER;
    }

    #! Returns NOTHING — output type is same as input
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
