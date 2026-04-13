# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreGuardrailTransformProcessor class definition

/** QoreGuardrailTransformProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that enriches records with guardrail evaluation results
/** Evaluates a text field against configured guardrail rules and enriches each
    record with `_guardrail_passed`, `_guardrail_violations`, and optionally
    redacted text. Records always pass through (unlike the filter processor).

    This is a @ref DPC_TRANSFORM processor — records are enriched, never dropped.

    @par Example:
    @code{.py}
QoreGuardrailTransformProcessor proc({
    "text_field": "body",
    "rules": (
        {"type": "pii", "severity": "high", "params": {"action": "redact"}},
    ),
});
    @endcode

*/
public class QoreGuardrailTransformProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreGuardrailTransformProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
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
                    "Each record is enriched with `_guardrail_passed` (bool) and "
                    "`_guardrail_violations` (list). If PII redaction is configured, "
                    "the text field is replaced with the redacted version.",
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
        return "guardrail-transform";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Enriches records with guardrail check results and optionally redacts PII — "
            "use after an LLM call to catch hallucinations, PII leakage, or policy violations "
            "in the output";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Enriches records with guardrail evaluation results
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreGuardrailTransformProcessor: not configured; provide constructor options";
        }

        hash<auto> result = {} + record;

        string text = DataProvider::AbstractDataProvider::getFieldValue(
            record, text_field).toString();

        hash<QoreGuardrails::GuardrailResult> gr = engine.evaluateInput(text);

        result._guardrail_passed = gr.passed;
        result._guardrail_violations = gr.violations;

        # If text was modified (e.g., PII redacted), update the text field
        if (gr.modified_text.val()) {
            result{text_field} = gr.modified_text;
        }

        return result;
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }

    #! Returns NOTHING — output type is dynamic
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
