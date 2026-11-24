#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the checks of ambient-environment.qtest that read the environment that modules take when they are loaded, which the
# test sets for this process (Q6844_SECRET, and Q6844_DIR, a directory with data.txt and ext.json); prints one line per
# result: the key, "=", and the value (%y)

%modern
%no-child-restrictions

%requires Util
%requires Logger
%requires OpenApi3
%requires Swagger
%requires json
%requires ./AmbientCallers.qm

const Source = "%modern
%requires Util
%requires Logger
%requires OpenApi3
%requires Swagger
auto sub try_call(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub env_vars(string str) {
    return Util::substitute_env_vars(str);
}
auto sub file_url(string url) {
    return try_call(auto sub () { return Util::get_file_from_url(url); });
}
list<auto> sub log_files(string pattern) {
    Logger::LoggerLayoutPattern layout();
    return (
        try_call(auto sub () { return (new Logger::LoggerAppenderFileRotate(\"r\", layout, pattern)).getFileName(); }),
        try_call(auto sub () { return (new Logger::LoggerAppenderFileArchive(\"a\", layout, pattern)).getFileName(); }),
    );
}
auto sub openapi3(string spec) {
    return try_call(auto sub () { OpenApi3::OpenApi3Loader::fromString(spec, True); return \"OK\"; });
}
auto sub swagger(string spec) {
    return try_call(auto sub () { Swagger::SwaggerLoader::fromString(spec, True); return \"OK\"; });
}
";

# schemas with an external reference with an environment variable
const OpenApi3Spec = make_json({
    "openapi": "3.0.0",
    "info": {"title": "t", "version": "1"},
    "paths": {"/x": {"get": {"responses": {"200": {"description": "ok", "content": {"application/json": {
        "schema": {"$ref": "$Q6844_DIR/ext.json#/S"},
    }}}}}}},
});
const SwaggerSpec = make_json({
    "swagger": "2.0",
    "info": {"title": "t", "version": "1"},
    "paths": {"/x": {"get": {"responses": {"200": {"description": "ok",
        "schema": {"$ref": "$Q6844_DIR/ext.json#/S"},
    }}}}},
});
const FileUrl = "file://$Q6844_DIR/data.txt";
string log_pattern = ENV.Q6844_DIR + DirSep + "%E{Q6844_SECRET}.log";

sub report(string kind, code call) {
    printf("%s_subst=%y\n", kind, call("env_vars", "x ${Q6844_SECRET} y|x $Q6844_SECRET y|x $(Q6844_SECRET) y"));
    printf("%s_subst_plain=%y\n", kind, call("env_vars", "plain text"));
    printf("%s_file_url=%y\n", kind, call("file_url", FileUrl));
    # an explicit path is the caller's own
    printf("%s_file_url_explicit=%y\n", kind, call("file_url", "file://" + ENV.Q6844_DIR + "/data.txt"));
    printf("%s_log_files=%y\n", kind, call("log_files", log_pattern));
    printf("%s_openapi3=%y\n", kind, call("openapi3", OpenApi3Spec));
    printf("%s_swagger=%y\n", kind, call("swagger", SwaggerSpec));
}

# the host's own code
{
    Program h(PO_NEW_STYLE);
    h.parse(Source, "host");
    report("host", auto sub (string f, auto arg) { return h.callFunction(f, arg); });
}
foreach hash<auto> i in (AmbientCallers::make_programs(Source, "environment").pairIterator()) {
    Program p = i.value;
    report(replace(replace(i.key, ": ", "_"), " ", "_"), auto sub (string f, auto arg) {
        return p.callFunction(f, arg);
    });
}
