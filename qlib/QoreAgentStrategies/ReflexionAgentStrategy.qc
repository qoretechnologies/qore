# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ReflexionAgentStrategy.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see ReflexionAgentStrategy
public namespace QoreAgentStrategies {

#! Agent strategy that executes, evaluates completeness, and corrects
/** The reflexion strategy operates in a loop:

    1. \b Execute: Generate an initial response (with optional tool calls)
    2. \b Reflect: LLM evaluates whether the response fully answers the query
    3. \b Correct: If incomplete, re-execute with the reflection feedback

    The loop runs for at most \c max_reflections iterations.

    The name "reflexion" comes from the AI research technique of having an
    agent reflect on its own output to identify gaps and improve quality,
    similar to how a human might review and revise their own work.

    @since QoreAgentStrategies 1.0
*/
public class ReflexionAgentStrategy inherits AbstractAgentStrategy {
    private {
        #! Maximum reflection cycles
        int max_reflections;
        #! Maximum tool iterations per execution
        int max_iterations;
    }

    #! Creates a ReflexionAgentStrategy
    /** @param llm_backend the LLM backend to use
        @param options strategy options:
            - \c max_reflections (int): maximum reflection cycles; default 2
            - \c max_iterations (int): tool iterations per execution; default 3
            - \c token_budget (int): total token budget; default 50000
    */
    constructor(QoreLlmUtils::LlmBackend llm_backend, *hash<auto> options)
            : AbstractAgentStrategy(llm_backend,
                {"token_budget": options.token_budget ?? 50000}) {
        max_reflections = options.max_reflections ?? 2;
        max_iterations = options.max_iterations ?? 3;
    }

    hash<AgentResult> execute(hash<AgentContext> ctx) {
        resetTokenCounter();
        list<hash<auto>> all_tool_calls = ();
        int llm_calls = 0;
        int reflections_done = 0;
        string current_response;

        # Build tools description
        string tools_desc = "";
        if (ctx.tools) {
            tools_desc = "\nAvailable tools: "
                + (map $1.name, ctx.tools).join(", ");
        }

        # Initial execution
        hash<QoreLlmUtils::LlmCompletionResult> exec_result = callLlm(
            "You are a helpful assistant." + tools_desc,
            ctx.message);
        ++llm_calls;
        current_response = exec_result.content;

        # Reflection loop
        while (reflections_done < max_reflections) {
            # Evaluate completeness
            hash<auto> reflection_schema = {
                "type": "object",
                "name": "reflection",
                "properties": {
                    "complete": {
                        "type": "boolean",
                        "description": "Whether the response fully answers the query",
                    },
                    "missing": {
                        "type": "string",
                        "description": "What is missing or could be improved",
                    },
                    "corrections": {
                        "type": "string",
                        "description": "Specific corrections or additions needed",
                    },
                },
                "required": ("complete", "missing", "corrections"),
            };

            string reflection_prompt = sprintf(
                "Original query: %s\n\nCurrent response:\n%s\n\n"
                "Evaluate: does this response fully and accurately answer the query? "
                "If not, what is missing or incorrect?",
                ctx.message, current_response);

            hash<QoreLlmUtils::LlmCompletionResult> reflection_result = callLlm(
                "You are a quality evaluator. Assess whether a response fully "
                "answers the original query. Be critical but fair.",
                reflection_prompt, reflection_schema);
            ++llm_calls;

            hash<auto> reflection = parse_json(reflection_result.content);

            if (reflection.complete) {
                break;
            }

            ++reflections_done;

            # Re-execute with corrections
            string correction_prompt = sprintf(
                "Original query: %s\n\nPrevious response: %s\n\n"
                "The response was incomplete. Missing: %s\n"
                "Corrections needed: %s\n\n"
                "Please provide an improved, complete response.",
                ctx.message, current_response,
                reflection.missing, reflection.corrections);

            hash<QoreLlmUtils::LlmCompletionResult> correction_result = callLlm(
                "You are a helpful assistant. Provide an improved response "
                "addressing the identified gaps." + tools_desc,
                correction_prompt);
            ++llm_calls;
            current_response = correction_result.content;
        }

        return <AgentResult>{
            "response": current_response,
            "tool_calls": all_tool_calls,
            "total_tokens": getTokensConsumed(),
            "llm_calls": llm_calls,
            "metadata": {
                "strategy": "reflexion",
                "reflections_done": reflections_done,
                "max_reflections": max_reflections,
            },
        };
    }
}

} # namespace QoreAgentStrategies
