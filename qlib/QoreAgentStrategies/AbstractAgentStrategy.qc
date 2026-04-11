# -*- mode: qore; indent-tabs-mode: nil -*-

/*  AbstractAgentStrategy.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see AbstractAgentStrategy
public namespace QoreAgentStrategies {

#! Context hash passed to agent strategies
public hashdecl AgentContext {
    #! The user message to process
    string message;
    #! Conversation history (list of {role, content} hashes)
    *list<hash<auto>> history;
    #! Available tools (list of {name, description, parameters} hashes)
    *list<hash<auto>> tools;
    #! Tool dispatch callable: code<auto(string tool_name, hash<auto> args)>
    *code tool_dispatch;
    #! Additional user context (e.g., user_id, session_id)
    *hash<auto> user_context;
}

#! Result from an agent strategy execution
public hashdecl AgentResult {
    #! The final response text
    string response;
    #! Tool calls made during execution (list of {tool, args, result} hashes)
    list<hash<auto>> tool_calls;
    #! Total tokens consumed
    int total_tokens;
    #! Number of LLM calls made
    int llm_calls;
    #! Strategy-specific metadata
    *hash<auto> metadata;
}

#! Abstract base class for agent execution strategies
/** Subclasses define how the agent's reasoning loop operates — how tools
    are dispatched, whether to plan first, whether to reflect on results, etc.

    @since QoreAgentStrategies 1.0
*/
public class AbstractAgentStrategy {
    private {
        #! The LLM backend for making completion calls
        QoreLlmUtils::LlmBackend llm;
        #! Total token budget (0 = unlimited)
        int token_budget;
        #! Tokens consumed so far
        int tokens_consumed = 0;
    }

    #! Creates the strategy with an LLM backend
    /** @param llm_backend the LLM backend to use
        @param options strategy options:
            - \c token_budget (int): total token budget; default 0 (unlimited)
    */
    constructor(QoreLlmUtils::LlmBackend llm_backend, *hash<auto> options) {
        llm = llm_backend;
        token_budget = options.token_budget ?? 0;
    }

    #! Executes the strategy
    /** @param ctx the agent context with message, tools, and dispatch callable
        @return an @ref AgentResult hash

        @throw AGENT-ERROR if execution fails
        @throw AGENT-BUDGET-EXCEEDED if the token budget is exhausted
    */
    abstract hash<AgentResult> execute(hash<AgentContext> ctx);

    #! Makes an LLM call and tracks token usage
    /** @throw AGENT-BUDGET-EXCEEDED if calling would exceed the budget
    */
    hash<QoreLlmUtils::LlmCompletionResult> callLlm(string system_prompt,
            string user_prompt, *hash<auto> output_schema) {
        if (token_budget > 0 && tokens_consumed >= token_budget) {
            throw "AGENT-BUDGET-EXCEEDED",
                sprintf("token budget exhausted: %d/%d tokens consumed",
                    tokens_consumed, token_budget);
        }

        hash<QoreLlmUtils::LlmCompletionResult> result =
            llm.complete(system_prompt, user_prompt, output_schema);

        int used = result.usage.total_tokens ?? 0;
        tokens_consumed += used;

        return result;
    }

    #! Returns the total tokens consumed
    int getTokensConsumed() {
        return tokens_consumed;
    }

    #! Resets the token counter
    resetTokenCounter() {
        tokens_consumed = 0;
    }

    #! Dispatches a tool call via the context's tool_dispatch callable
    auto dispatchTool(hash<AgentContext> ctx, string tool_name, hash<auto> args) {
        if (!ctx.tool_dispatch) {
            throw "AGENT-ERROR",
                sprintf("no tool_dispatch callable provided for tool %y", tool_name);
        }
        return ctx.tool_dispatch(tool_name, args);
    }
}

} # namespace QoreAgentStrategies
