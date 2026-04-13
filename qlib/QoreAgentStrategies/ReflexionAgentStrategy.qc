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

    1. \b Execute: Run a @ref SimpleAgentStrategy to generate a response
       (with tool calling if tools are available)
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
        #! Whether to use parallel tool dispatch
        bool parallel_dispatch;
    }

    #! Creates a ReflexionAgentStrategy
    /** @param llm_backend the LLM backend to use
        @param options strategy options:
            - \c max_reflections (int): maximum reflection cycles; default 2
            - \c max_iterations (int): tool iterations per execution; default 3
            - \c parallel_dispatch (bool): parallel tool dispatch; default False
            - \c token_budget (int): total token budget; default 50000
            - \c event_callback (code): event callback for observability
    */
    constructor(QoreLlmUtils::LlmBackend llm_backend, *hash<auto> options)
            : AbstractAgentStrategy(llm_backend,
                {"token_budget": options.token_budget ?? 50000,
                 "event_callback": options.event_callback}) {
        max_reflections = options.max_reflections ?? 2;
        max_iterations = options.max_iterations ?? 3;
        parallel_dispatch = options.parallel_dispatch ?? False;
    }

    hash<AgentResult> execute(hash<AgentContext> ctx) {
        resetTokenCounter();
        list<hash<auto>> all_tool_calls = ();
        int llm_calls = 0;
        int reflections_done = 0;
        string current_response;

        emitEvent(<AgentEvent>{
            "event_type": "strategy_started",
            "strategy": "reflexion",
            "timestamp": now_us(),
        });

        # Initial execution using SimpleAgentStrategy (with tool calling)
        SimpleAgentStrategy executor(llm, {
            "max_iterations": max_iterations,
            "parallel_dispatch": parallel_dispatch,
            "token_budget": 0,
            "event_callback": event_callback,
        });

        hash<AgentResult> exec_result = executor.execute(ctx);
        current_response = exec_result.response;
        all_tool_calls += exec_result.tool_calls;
        llm_calls += exec_result.llm_calls;
        tokens_consumed += exec_result.total_tokens;

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

            emitEvent(<AgentEvent>{
                "event_type": "iteration_completed",
                "strategy": "reflexion",
                "iteration": reflections_done + 1,
                "metadata": {
                    "complete": reflection.complete,
                    "missing": reflection.missing,
                },
                "timestamp": now_us(),
            });

            if (reflection.complete) {
                break;
            }

            ++reflections_done;

            # Re-execute with corrections using SimpleAgentStrategy
            SimpleAgentStrategy corrector(llm, {
                "max_iterations": max_iterations,
                "parallel_dispatch": parallel_dispatch,
                "token_budget": 0,
                "event_callback": event_callback,
            });

            hash<AgentResult> correction_result = corrector.execute(<AgentContext>{
                "message": sprintf(
                    "Original query: %s\n\nPrevious response: %s\n\n"
                    "The response was incomplete. Missing: %s\n"
                    "Corrections needed: %s\n\n"
                    "Please provide an improved, complete response.",
                    ctx.message, current_response,
                    reflection.missing, reflection.corrections),
                "tools": ctx.tools,
                "tool_dispatch": ctx.tool_dispatch,
                "system_prompt": "You are a helpful assistant. Provide an improved "
                    "response addressing the identified gaps.",
            });

            current_response = correction_result.response;
            all_tool_calls += correction_result.tool_calls;
            llm_calls += correction_result.llm_calls;
            tokens_consumed += correction_result.total_tokens;
        }

        emitEvent(<AgentEvent>{
            "event_type": "strategy_completed",
            "strategy": "reflexion",
            "timestamp": now_us(),
            "metadata": {"reflections_done": reflections_done},
        });

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
