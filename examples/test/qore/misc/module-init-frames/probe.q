#!/usr/bin/env qore
# the initialization of a module loaded at run time (qore#7289): run by ../module-init-frames.qtest
# ARGV[0]: how the module is loaded; ARGV[1]: the directory of the modules; ARGV[2]: the extension of the modules

%modern
%no-child-restrictions
# the module that the initializations call is loaded already, as a library module that the process uses is
%requires InitFramesProbe

string how = ARGV[0];
string dir = ARGV[1];
string ext = ARGV[2] ?? ".qm";
set_module_option("InitFramesA", "opt", "set");
set_module_option("InitFramesA", "b-path", dir + "/InitFramesB" + ext);

const Opts = PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS;

switch (how) {
    case "program-path": {
        Program p(Opts);
        p.loadModule(dir + "/InitFramesA" + ext);
        p.parse("string sub report() { return InitFramesA::InitFramesA::report(); }", "loader");
        printf("%s\n", p.callFunction("report"));
        break;
    }
    case "independent-program-path": {
        Program p(Opts, True);
        p.loadModule(dir + "/InitFramesA" + ext);
        p.parse("string sub report() { return InitFramesA::InitFramesA::report(); }", "loader");
        printf("%s\n", p.callFunction("report"));
        break;
    }
    case "load-path": {
        load_module(dir + "/InitFramesA" + ext);
        printf("%s\n", call_static_method("InitFramesA::InitFramesA", "report"));
        break;
    }
    case "load-name": {
        load_module("InitFramesA");
        printf("%s\n", call_static_method("InitFramesA::InitFramesA", "report"));
        break;
    }
    case "sandboxed-path": {
        # a sandboxed Program loads the module by its path: its restrictions apply to the initialization
        Program p(Opts);
        p.setSandboxManager(new SandboxManager());
        p.parse("string sub report(string path) { load_module(path);
    return call_static_method('InitFramesA::InitFramesA', 'report'); }", "sandboxed-loader");
        printf("%s\n", p.callFunction("report", dir + "/InitFramesA" + ext));
        break;
    }
    case "after-failed-init": {
        # an initialization that fails; the frames of the next one name its module
        try {
            load_module(dir + "/InitFramesThrow" + ext);
            printf("the initialization did not fail\n");
        } catch (hash<ExceptionInfo> ex) {
            printf("failed: %s\n", ex.err);
        }
        load_module(dir + "/InitFramesA" + ext);
        printf("%s\n", call_static_method("InitFramesA::InitFramesA", "report"));
        # the frames of the script's own calls name its Program again
        printf("script frames: %s\n", call_function("InitFramesProbe::caller_pgm")
            == call_function("InitFramesProbe::caller_pgm") ? "consistent" : "OTHER");
        break;
    }
    default:
        throw "PROBE-ERROR", sprintf("unknown mode %y", how);
}
