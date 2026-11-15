#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# ExtractionPipeline (QoreStructuredExtraction) loaded first by a Program that may not control threads (qorus#867):
# its threads start, governed by the caller's sandbox (its thread limit); this Program does not load the module itself

# Copyright 2026 Qore Technologies, s.r.o.

%new-style
%require-types
%strict-args

const Source = "
# an LLM backend that returns one entity for every text
class MockBackend inherits QoreLlmUtils::LlmBackend {
    constructor() : LlmBackend(<LlmBackendConfig>{\"mode\": \"callback\",
        \"api_callback\": hash<auto> sub (string m, string p, hash<auto> b) { return {}; }}) {
    }

    hash<ChatResponse> chat(list<hash<ChatMessage>> messages, *list<hash<ToolDefinition>> tools,
            *hash<LlmRequestOptions> opts) {
        hash<auto> args = {\"entities\": ({\"entity_type\": \"PERSON\", \"text\": \"A\", \"start\": 0, \"end\": 1,
            \"confidence\": 0.9},)};
        return <ChatResponse>{
            \"stop_reason\": \"tool_use\",
            \"tool_calls\": (<ChatToolCall>{\"id\": \"tc\", \"name\": tools[0].name, \"arguments\": args},),
            \"usage\": {\"total_tokens\": 1},
        };
    }
}

# extracts n texts in parallel; returns whether each result is valid, or the error
list<string> sub run(int n) {
    ExtractionPipeline pipeline(new MockBackend(), ExtractionSchema::NerSchema,
        <ExtractionPipelineOptions>{\"batch_parallel\": n});
    list<string> texts = ();
    for (int i = 0; i < n; ++i) {
        texts += \"text \" + i;
    }
    try {
        return map $1.valid ? \"ok\" : ($1.validation_errors ? $1.validation_errors[0] : \"invalid\"),
            pipeline.extractBatch(texts);
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err,);
    }
}
";

# a restricted Program that loads the module first, with a sandbox allowing max_threads threads
Program sub restricted(int max_threads) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_NO_THREAD_CONTROL);
    p.loadModule("QoreStructuredExtraction");
    p.parse(Source, "restricted");
    SandboxManager sm();
    sm.setMaxThreads(max_threads);
    p.setSandboxManager(sm);
    return p;
}

# the Programs are kept until the end, so that the threads of their objects end first
Program limited = restricted(1);
Program free = restricted(5);
printf("limited: %s\n", (foldl $1 + "," + $2, limited.callFunction("run", 2)));
printf("started: %s\n", (foldl $1 + "," + $2, free.callFunction("run", 2)));
