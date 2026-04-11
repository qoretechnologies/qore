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

    @since QoreGuardrails 1.0
*/
public class TopicRestrictionRule inherits AbstractGuardrailRule {
    private {
        #! Allowed topics list
        list<string> allowed_topics;
%ifndef NoLlmUtils
        #! LLM backend for classification
        *QoreLlmUtils::LlmBackend llm;
%endif
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

%ifndef NoLlmUtils
        if (llm_config) {
            llm = new QoreLlmUtils::LlmBackend(
                cast<hash<QoreLlmUtils::LlmBackendConfig>>(llm_config));
        }
%endif
    }

    string getRuleName() {
        return "topic_restriction";
    }

    hash<GuardrailResult> evaluate(string text, *hash<auto> ctx) {
        date start = now_us();

%ifndef NoLlmUtils
        if (llm) {
            return classifyTopic(text, start);
        }
%endif

        # Without LLM, pass (cannot classify topics without one)
        hash<GuardrailResult> result = makePassResult();
        result.duration_ms = getElapsedMs(start);
        return result;
    }

%ifndef NoLlmUtils
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
            # LLM failure — pass through (fail open)
        }

        hash<GuardrailResult> result = makePassResult();
        result.duration_ms = getElapsedMs(start);
        return result;
    }
%endif

    #! Calculates elapsed milliseconds since start
    private int getElapsedMs(date start) {
        return (now_us() - start).durationMicroseconds() / 1000;
    }
}

} # namespace QoreGuardrails
