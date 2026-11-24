#!/usr/bin/env qore
# a module named HttpServer that sandboxed code loads by its path before the HttpServer module is loaded cannot claim
# the dispatch capability of the HTTP server (qore#6843): the module loader refuses it when the module path has a
# module named HttpServer, else its claim is refused; run by ../HttpServerDispatchCapability.qtest with the path of the
# module as its argument

%modern
%requires HttpServerUtil

Program p(PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
p.setSandboxManager(new SandboxManager());
try {
    p.loadModule(ARGV[0]);
    printf("loaded\n");
} catch (hash<ExceptionInfo> ex) {
    if (ex.err == "HTTP-SERVER-ERROR" && ex.desc =~ /^the dispatch capability of the HTTP server cannot be claimed by sandboxed code/) {
        printf("refused (claim)\n");
    } else if (ex.err == "LOAD-MODULE-ERROR" && ex.desc =~ /cannot load a module outside the module path under the name of module/) {
        printf("refused (module path)\n");
    } else {
        printf("other: %s: %s\n", ex.err, ex.desc);
    }
}
printf("HttpServer loaded: %y\n", exists get_module_hash().HttpServer);
