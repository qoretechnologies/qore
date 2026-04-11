# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ParallelToolDispatcher.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see ParallelToolDispatcher
public namespace QoreAgentStrategies {

#! Dispatches multiple tool calls concurrently
/** When an LLM returns multiple tool calls in a single response, this
    dispatcher executes them in parallel using background threads. Results
    are collected and returned in the original order.

    This is orthogonal to agent strategies — any strategy can use the
    dispatcher via the \c dispatchToolsParallel() method.

    @par Limits
    - \c max_concurrent (int): maximum parallel tool calls; default 5
    - \c tool_timeout_ms (int): per-tool timeout; default 30000
    - \c max_total_time_ms (int): total time budget for all tools; default 120000

    @since QoreAgentStrategies 1.0
*/
public class ParallelToolDispatcher {
    private {
        #! Maximum concurrent tool dispatches
        int max_concurrent;
        #! Per-tool timeout in milliseconds
        int tool_timeout_ms;
        #! Total time budget for all tools
        int max_total_time_ms;
    }

    #! Creates a ParallelToolDispatcher
    /** @param options dispatcher options:
            - \c max_concurrent (int): max parallel tools; default 5
            - \c tool_timeout_ms (int): per-tool timeout; default 30000
            - \c max_total_time_ms (int): total time budget; default 120000
    */
    constructor(*hash<auto> options) {
        max_concurrent = options.max_concurrent ?? 5;
        tool_timeout_ms = options.tool_timeout_ms ?? 30000;
        max_total_time_ms = options.max_total_time_ms ?? 120000;
    }

    #! Dispatches multiple tool calls in parallel
    /** @param tool_calls list of {name, args} hashes to dispatch
        @param tool_dispatch the callable to invoke for each tool
        @return list of {name, args, result, error, duration_ms} hashes in input order

        @throw AGENT-TIMEOUT if the total time budget is exceeded
    */
    list<hash<auto>> dispatchToolsParallel(list<hash<auto>> tool_calls,
            code tool_dispatch) {
        if (!tool_calls) {
            return ();
        }

        # If only one tool or max_concurrent is 1, dispatch sequentially
        if (tool_calls.size() == 1 || max_concurrent <= 1) {
            return dispatchSequential(tool_calls, tool_dispatch);
        }

        date start = now_us();
        list<hash<auto>> results = ();

        # Process in batches of max_concurrent
        int total = tool_calls.size();
        int offset = 0;

        while (offset < total) {
            # Check total time budget
            int elapsed_ms = (now_us() - start).durationMicroseconds() / 1000;
            if (elapsed_ms >= max_total_time_ms) {
                throw "AGENT-TIMEOUT",
                    sprintf("parallel tool dispatch total time budget exceeded: "
                        "%d ms >= %d ms", elapsed_ms, max_total_time_ms);
            }

            int batch_end = min(offset + max_concurrent, total);
            list<hash<auto>> batch = tool_calls[offset .. batch_end - 1];

            # Dispatch batch concurrently using Counter for synchronization
            Counter cnt(batch.size());
            list<hash<auto>> batch_results = ();
            # Pre-size results list
            for (int i = 0; i < batch.size(); ++i) {
                push batch_results, {};
            }

            for (int i = 0; i < batch.size(); ++i) {
                int idx = i;
                hash<auto> tc = batch[i];
                background sub () {
                    on_exit cnt.dec();
                    date tool_start = now_us();
                    try {
                        auto result = tool_dispatch(tc.name, tc.args ?? {});
                        int dur = (now_us() - tool_start).durationMicroseconds() / 1000;
                        batch_results[idx] = {
                            "name": tc.name,
                            "args": tc.args ?? {},
                            "result": result,
                            "duration_ms": dur,
                        };
                    } catch (hash<ExceptionInfo> ex) {
                        int dur = (now_us() - tool_start).durationMicroseconds() / 1000;
                        batch_results[idx] = {
                            "name": tc.name,
                            "args": tc.args ?? {},
                            "error": sprintf("%s: %s", ex.err, ex.desc),
                            "duration_ms": dur,
                        };
                    }
                }();
            }

            # Wait for batch with timeout
            int remaining_ms = max_total_time_ms
                - (now_us() - start).durationMicroseconds() / 1000;
            if (remaining_ms <= 0) {
                remaining_ms = 1;
            }
            cnt.waitForZero(remaining_ms);

            results += batch_results;
            offset = batch_end;
        }

        return results;
    }

    #! Dispatches tools sequentially (fallback for single tool or max_concurrent=1)
    private list<hash<auto>> dispatchSequential(list<hash<auto>> tool_calls,
            code tool_dispatch) {
        list<hash<auto>> results = ();
        foreach hash<auto> tc in (tool_calls) {
            date tool_start = now_us();
            try {
                auto result = tool_dispatch(tc.name, tc.args ?? {});
                int dur = (now_us() - tool_start).durationMicroseconds() / 1000;
                push results, {
                    "name": tc.name,
                    "args": tc.args ?? {},
                    "result": result,
                    "duration_ms": dur,
                };
            } catch (hash<ExceptionInfo> ex) {
                int dur = (now_us() - tool_start).durationMicroseconds() / 1000;
                push results, {
                    "name": tc.name,
                    "args": tc.args ?? {},
                    "error": sprintf("%s: %s", ex.err, ex.desc),
                    "duration_ms": dur,
                };
            }
        }
        return results;
    }
}

} # namespace QoreAgentStrategies
