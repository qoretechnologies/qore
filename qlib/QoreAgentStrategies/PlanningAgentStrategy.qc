# -*- mode: qore; indent-tabs-mode: nil -*-

/*  PlanningAgentStrategy.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see PlanningAgentStrategy
public namespace QoreAgentStrategies {

#! Agent strategy that decomposes a query into sub-tasks, executes each, then synthesizes
/** The planning strategy operates in three phases:

    1. \b Planning: LLM decomposes the query into sub-tasks with tool specifications
    2. \b Execution: Each sub-task is executed with up to \c max_iterations_per_subtask
       tool dispatch iterations
    3. \b Synthesis: LLM combines sub-task results into a final response

    @since QoreAgentStrategies 1.0
*/
public class PlanningAgentStrategy inherits AbstractAgentStrategy {
    private {
        #! Maximum sub-tasks the planner can create
        int max_subtasks;
        #! Maximum tool dispatch iterations per sub-task
        int max_iterations_per_subtask;
    }

    #! Creates a PlanningAgentStrategy
    /** @param llm_backend the LLM backend to use
        @param options strategy options:
            - \c max_subtasks (int): max sub-tasks; default 5
            - \c max_iterations_per_subtask (int): tool iterations per sub-task; default 3
            - \c token_budget (int): total token budget; default 50000
    */
    constructor(QoreLlmUtils::LlmBackend llm_backend, *hash<auto> options)
            : AbstractAgentStrategy(llm_backend,
                {"token_budget": options.token_budget ?? 50000}) {
        max_subtasks = options.max_subtasks ?? 5;
        max_iterations_per_subtask = options.max_iterations_per_subtask ?? 3;
    }

    hash<AgentResult> execute(hash<AgentContext> ctx) {
        resetTokenCounter();
        list<hash<auto>> all_tool_calls = ();
        int llm_calls = 0;

        # Phase 1: Planning — decompose into sub-tasks
        string tools_desc = "";
        if (ctx.tools) {
            tools_desc = "\n\nAvailable tools:\n";
            foreach hash<auto> tool in (ctx.tools) {
                tools_desc += sprintf("- %s: %s\n", tool.name,
                    tool.description ?? "no description");
            }
        }

        string plan_prompt = sprintf(
            "Decompose the following query into concrete sub-tasks that can be "
            "executed sequentially. Each sub-task should specify which tool to use "
            "and what arguments to pass.%s\n\n"
            "Query: %s\n\n"
            "Respond with a JSON object: {\"subtasks\": [{\"description\": \"...\", "
            "\"tool\": \"tool_name\", \"args\": {...}}, ...]}\n"
            "Maximum %d sub-tasks.", tools_desc, ctx.message, max_subtasks);

        hash<auto> plan_schema = {
            "type": "object",
            "name": "execution_plan",
            "properties": {
                "subtasks": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "description": {"type": "string"},
                            "tool": {"type": "string"},
                            "args": {"type": "object"},
                        },
                        "required": ("description",),
                    },
                },
            },
            "required": ("subtasks",),
        };

        hash<QoreLlmUtils::LlmCompletionResult> plan_result = callLlm(
            "You are a task planning agent. Decompose complex queries into "
            "actionable sub-tasks.",
            plan_prompt, plan_schema);
        ++llm_calls;

        hash<auto> plan = parse_json(plan_result.content);
        list<hash<auto>> subtasks = plan.subtasks ?? ();

        # Limit to max_subtasks
        if (subtasks.size() > max_subtasks) {
            splice subtasks, max_subtasks;
        }

        # Phase 2: Execution — execute each sub-task
        list<hash<auto>> subtask_results = ();
        foreach hash<auto> subtask in (subtasks) {
            hash<auto> sub_result = {
                "description": subtask.description,
                "tool_calls": (),
                "result": NOTHING,
            };

            # Execute tool if specified and available
            if (subtask.tool.val() && ctx.tool_dispatch) {
                try {
                    auto tool_result = dispatchTool(ctx, subtask.tool,
                        subtask.args ?? {});
                    sub_result.result = tool_result;
                    push all_tool_calls, {
                        "tool": subtask.tool,
                        "args": subtask.args ?? {},
                        "result": tool_result,
                    };
                } catch (hash<ExceptionInfo> ex) {
                    sub_result.result = sprintf("Error: %s: %s", ex.err, ex.desc);
                }
            }

            push subtask_results, sub_result;
        }

        # Phase 3: Synthesis — combine results
        string synthesis_input = sprintf("Original query: %s\n\nSub-task results:\n",
            ctx.message);
        int idx = 1;
        foreach hash<auto> sr in (subtask_results) {
            synthesis_input += sprintf("%d. %s\n   Result: %y\n\n",
                idx, sr.description, sr.result);
            ++idx;
        }

        hash<QoreLlmUtils::LlmCompletionResult> synthesis_result = callLlm(
            "You are a synthesis agent. Given the original query and sub-task results, "
            "compose a comprehensive response that addresses the original query.",
            synthesis_input);
        ++llm_calls;

        return <AgentResult>{
            "response": synthesis_result.content,
            "tool_calls": all_tool_calls,
            "total_tokens": getTokensConsumed(),
            "llm_calls": llm_calls,
            "metadata": {
                "strategy": "planning",
                "subtask_count": subtasks.size(),
                "subtask_descriptions": (map $1.description, subtasks),
            },
        };
    }
}

} # namespace QoreAgentStrategies
