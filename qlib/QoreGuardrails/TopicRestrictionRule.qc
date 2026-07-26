# -*- mode: qore; indent-tabs-mode: nil -*-

/*  TopicRestrictionRule.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see TopicRestrictionRule
public namespace QoreGuardrails {

#! LLM-based topic boundary enforcement
/** Ensures that user queries stay within allowed topics using an LLM classifier.
    For example, a customer support chatbot for a software company can be
    restricted to topics like "billing", "technical support", and "account
    management" -- any questions about competitors or unrelated subjects would be
    blocked.

    Requires a QoreLlmUtils backend.
*/
public class TopicRestrictionRule inherits AbstractGuardrailRule {
    private {
        #! Allowed topics list
        list<string> allowed_topics;
        #! LLM backend for classification
        *QoreLlmUtils::LlmBackend llm;
    }

    #! Creates a topic restriction rule
    /** @param severity violation severity level
        @param params required parameters:
            - \c allowed_topics (list<string>): list of allowed topic names
        @param llm_config LlmBackendConfig for LLM classification

        @throw GUARDRAIL-CONFIG-ERROR if allowed_topics is missing
    */
    constructor(string severity = GS_HIGH, *hash<auto> params,
            *hash<auto> llm_config) : AbstractGuardrailRule(severity) {
        if (!params.allowed_topics) {
            throw "GUARDRAIL-CONFIG-ERROR",
                "TopicRestrictionRule requires 'allowed_topics' parameter";
        }
        allowed_topics = params.allowed_topics;

        if (llm_config) {
            llm = new QoreLlmUtils::LlmBackend(
                cast<hash<QoreLlmUtils::LlmBackendConfig>>(llm_config));
        }
    }

    string getRuleName() {
        return "topic_restriction";
    }

    #! Topic restriction is an input-side concern by default
    string getDirection() {
        return direction_override ?? GD_INPUT;
    }

    #! Returns the DataProvider-option schema for this rule's \c params hash
    /** @since QoreGuardrails 1.1 */
    public static hash<string, hash<DataProviderOptionInfo>> getConfigSchema() {
        return {
            "allowed_topics": <DataProviderOptionInfo>{
                "type": (AbstractDataProviderType::get(AutoListType),),
                "display_name": "Allowed Topics",
                "short_desc": "List of topic names that user messages are allowed "
                    "to discuss",
                "desc": "The list of topic labels an LLM classifier compares each "
                    "incoming message against.  A message that doesn't match "
                    "any of the listed topics is classified as off-topic and "
                    "blocked.  Use short, concrete topic names the "
                    "classifier can recognize: `customer support`, `order "
                    "tracking`, `product information`, `returns and refunds`.  "
                    "Avoid meta-topics like `anything related to Acme` — they "
                    "confuse the classifier.  Typical lists are 2-6 topics.",
                "required": True,
                "preselected": True,
                "example_value": (
                    "customer support",
                    "order tracking",
                    "product information",
                ),
            },
        };
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();

        if (llm) {
            return classifyTopic(text, start);
        }

        # Without LLM, cannot classify topics
        if (getFailClosed()) {
            hash<GuardrailResult> result = makeFailResult(
                "topic classification unavailable without LLM (fail-closed)");
            result.duration_ms = getElapsedMs(start);
            return result;
        }
        hash<GuardrailResult> result = makePassResult();
        result.duration_ms = getElapsedMs(start);
        return result;
    }

    #! Uses LLM to classify whether the query is within allowed topics
    private hash<GuardrailResult> classifyTopic(string text, date start) {
        string topics_str = (map sprintf("\"%s\"", $1), allowed_topics).join(", ");
        string system_prompt = sprintf(
            "You are a topic classifier. Determine if the user's query falls within "
            "the following allowed topics: %s.\n\n"
            "Respond with a JSON object: {\"on_topic\": true/false, "
            "\"detected_topic\": \"the topic of the query\", "
            "\"reason\": \"brief explanation\"}", topics_str);

        hash<auto> output_schema = {
            "type": "object",
            "name": "topic_classification",
            "properties": {
                "on_topic": {"type": "boolean"},
                "detected_topic": {"type": "string"},
                "reason": {"type": "string"},
            },
            "required": ("on_topic", "detected_topic", "reason"),
        };

        try {
            hash<QoreLlmUtils::LlmCompletionResult> llm_result =
                llm.complete(system_prompt, text, output_schema);
            hash<auto> parsed = parse_json(llm_result.content);

            if (!parsed.on_topic) {
                hash<GuardrailResult> result = makeFailResult(
                    sprintf("query topic \"%s\" is outside allowed topics (%s): %s",
                        parsed.detected_topic, topics_str, parsed.reason),
                    NOTHING,
                    {
                        "detected_topic": parsed.detected_topic,
                        "allowed_topics": allowed_topics,
                        "reason": parsed.reason,
                    },
                );
                result.duration_ms = getElapsedMs(start);
                return result;
            }
        } catch (hash<ExceptionInfo> ex) {
            if (getFailClosed()) {
                hash<GuardrailResult> result = makeFailResult(
                    sprintf("topic classification unavailable (fail-closed): %s",
                        ex.desc));
                result.duration_ms = getElapsedMs(start);
                return result;
            }
            # LLM failure — pass through (fail open)
        }

        hash<GuardrailResult> result = makePassResult();
        result.duration_ms = getElapsedMs(start);
        return result;
    }
}

} # namespace QoreGuardrails
