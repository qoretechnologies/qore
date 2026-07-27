# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OnnxProfileTools.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see QoreOnnxTools
public namespace QoreOnnxTools {

#! Aggregated ONNX Runtime profile timing information
public hashdecl OnnxProfileAggregateInfo {
    #! Group kind: \c operator, \c provider, \c node, or \c session
    string kind;
    #! Group display name
    string name;
    #! Event count in this group
    int count = 0;
    #! Total duration in microseconds
    int total_us = 0;
    #! Total duration in milliseconds
    float total_ms = 0.0;
    #! Minimum event duration in microseconds
    int min_us = 0;
    #! Maximum event duration in microseconds
    int max_us = 0;
    #! Average event duration in microseconds
    float avg_us = 0.0;
    #! ONNX Runtime profile category
    *string category;
    #! ONNX operator name, when available
    *string op_name;
    #! ONNX Runtime execution provider, when available
    *string provider;
    #! Timestamp of the first event in this group
    *int first_ts;
    #! Timestamp of the last event in this group
    *int last_ts;
}

#! Structured summary of an ONNX Runtime profile file
public hashdecl OnnxProfileSummary {
    #! Profile JSON path
    string profile_path;
    #! Total event count in the profile
    int event_count = 0;
    #! Number of \c Node events
    int node_event_count = 0;
    #! Number of \c Session events
    int session_event_count = 0;
    #! Total node execution duration in microseconds
    int total_node_us = 0;
    #! Total node execution duration in milliseconds
    float total_node_ms = 0.0;
    #! Total session event duration in microseconds
    int total_session_us = 0;
    #! Total session event duration in milliseconds
    float total_session_ms = 0.0;
    #! Operators sorted by descending total duration
    list<hash<OnnxProfileAggregateInfo>> operators = ();
    #! Execution providers sorted by descending total node duration
    list<hash<OnnxProfileAggregateInfo>> providers = ();
    #! Slowest nodes sorted by descending total duration
    list<hash<OnnxProfileAggregateInfo>> nodes = ();
    #! Session events sorted by descending total duration
    list<hash<OnnxProfileAggregateInfo>> sessions = ();
    #! Distinct operator names in the profile
    list<string> operator_names = ();
    #! Distinct provider names in the profile
    list<string> provider_names = ();
}

/** Utilities for summarizing ONNX Runtime profile traces.

    ONNX Runtime writes profile files as Chrome trace JSON. This class turns
    those raw trace events into compact Qore records that can be attached to
    model registry metadata, processor diagnostics, and benchmark reports.

    @since QoreOnnxTools 1.0
*/
public class OnnxProfileTools {
    /** Summarizes an ONNX Runtime profile JSON file.

        @param profile_path profile JSON file returned by
        @ref Qore::ML::OnnxModel::endProfiling()
        @param top_limit maximum number of node and session rows to return;
        use \c 0 for no limit

        @return profile summary with operator, provider, node, and session
        aggregates

        @throw QORE-ONNX-TOOLS-ERROR if the file is missing, JSON support is not
        available, or the trace cannot be parsed
    */
    static hash<OnnxProfileSummary> summarizeProfile(string profile_path, int top_limit = 20) {
        if (!profile_path.val()) {
            throw "QORE-ONNX-TOOLS-ERROR", "profile_path must not be empty";
        }
        if (!is_file(profile_path)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("profile file %y does not exist or is not a regular file",
                    profile_path);
        }
        if (top_limit < 0) {
            throw "QORE-ONNX-TOOLS-ERROR", "top_limit must be >= 0";
        }

        list<hash<auto>> events = cast<list<hash<auto>>>(
            parse_json(ReadOnlyFile::readTextFile(profile_path)));
        hash<auto!> by_operator;
        hash<auto!> by_provider;
        hash<auto!> by_node;
        hash<auto!> by_session;

        hash<OnnxProfileSummary> result = <OnnxProfileSummary>{
            "profile_path": profile_path,
            "event_count": events.size(),
        };

        foreach hash<auto> event in (events) {
            string category = event.cat ?? "";
            string name = event.name ?? "";
            int duration_us = event.dur ? int(event.dur) : 0;
            *int ts = exists event.ts ? int(event.ts) : NOTHING;

            if (category == "Node") {
                ++result.node_event_count;
                result.total_node_us += duration_us;
                hash<auto> args = event.args ?? {};
                string op_name = args.op_name ?? normalizeNodeName(name);
                string provider = args.provider ?? "unknown";
                recordAggregate(\by_operator, op_name, "operator", duration_us, ts,
                    category, op_name, provider);
                recordAggregate(\by_provider, provider, "provider", duration_us, ts,
                    category, op_name, provider);
                recordAggregate(\by_node, normalizeNodeName(name), "node", duration_us,
                    ts, category, op_name, provider);
            } else if (category == "Session") {
                ++result.session_event_count;
                result.total_session_us += duration_us;
                recordAggregate(\by_session, name, "session", duration_us, ts,
                    category);
            }
        }

        result.total_node_ms = result.total_node_us / 1000.0;
        result.total_session_ms = result.total_session_us / 1000.0;
        result.operators = sortAggregates(toAggregateList(by_operator), 0);
        result.providers = sortAggregates(toAggregateList(by_provider), 0);
        result.nodes = sortAggregates(toAggregateList(by_node), top_limit);
        result.sessions = sortAggregates(toAggregateList(by_session), top_limit);
        result.operator_names = sort(map $1.name, result.operators);
        result.provider_names = sort(map $1.name, result.providers);
        return result;
    }

    private static nothing recordAggregate(reference<hash<auto!>> groups, string name,
            string kind, int duration_us, *int ts, string category, *string op_name = NOTHING,
            *string provider = NOTHING) {
        if (!name.val()) {
            name = "unknown";
        }
        hash<auto!> info = exists groups{name} ? groups{name} : {};
        if (!info) {
            info.kind = kind;
            info.name = name;
            info.count = 0;
            info.total_us = 0;
            info.min_us = duration_us;
            info.max_us = duration_us;
            info.category = category;
            if (op_name.val()) {
                info.op_name = op_name;
            }
            if (provider.val()) {
                info.provider = provider;
            }
            if (ts) {
                info.first_ts = ts;
                info.last_ts = ts;
            }
        }
        ++info.count;
        info.total_us += duration_us;
        if (duration_us < info.min_us) {
            info.min_us = duration_us;
        }
        if (duration_us > info.max_us) {
            info.max_us = duration_us;
        }
        if (ts) {
            if (!exists info.first_ts || ts < info.first_ts) {
                info.first_ts = ts;
            }
            if (!exists info.last_ts || ts > info.last_ts) {
                info.last_ts = ts;
            }
        }
        groups{name} = info;
    }

    private static list<hash<OnnxProfileAggregateInfo>> toAggregateList(
            hash<auto!> groups) {
        list<hash<OnnxProfileAggregateInfo>> result();
        foreach string name in (keys groups) {
            hash<auto!> info = groups{name};
            info.avg_us = info.count ? info.total_us / info.count : 0.0;
            info.total_ms = info.total_us / 1000.0;
            push result, cast<hash<OnnxProfileAggregateInfo>>(info);
        }
        return result;
    }

    private static list<hash<OnnxProfileAggregateInfo>> sortAggregates(
            list<hash<OnnxProfileAggregateInfo>> items, int limit) {
        list<hash<OnnxProfileAggregateInfo>> sorted = sort(items,
            int sub(hash<OnnxProfileAggregateInfo> left,
                    hash<OnnxProfileAggregateInfo> right) {
                if (left.total_us != right.total_us) {
                    return right.total_us <=> left.total_us;
                }
                return left.name <=> right.name;
            });
        if (!limit || sorted.size() <= limit) {
            return sorted;
        }
        list<hash<OnnxProfileAggregateInfo>> trimmed();
        for (int i = 0; i < limit; ++i) {
            push trimmed, sorted[i];
        }
        return trimmed;
    }

    private static string normalizeNodeName(string name) {
        name =~ s/_kernel_time$//;
        return name.val() ? name : "unknown";
    }
}

} # namespace QoreOnnxTools
