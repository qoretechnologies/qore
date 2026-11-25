#!/usr/bin/env qore
# the init closure of a module loaded at run time (qore#7289): run by ../module-init-frames.qtest
# ARGV[0]: the loader: "trusted" (the script) or "restricted" (a child Program with restrictions)
# ARGV[1]: where the load is made: "top" (top-level code) or "method" (a method)
# ARGV[2]: how the module is loaded: "name" or "path"; ARGV[3]: the directory of the modules

%modern
%no-child-restrictions
# the module that the init closure calls is loaded already, as a library module that the process uses is
%requires InitFramesProbe

string loader = ARGV[0];
string place = ARGV[1];
string how = ARGV[2];
string dir = ARGV[3];
string mod = how == "path" ? dir + "/InitFramesInit.qm" : "InitFramesInit";

#! the loading code, in the script and in the restricted Program
const LoaderCode = "class Loader {
    static load(string mod) {
        load_module(mod);
    }
}
string sub load_top(string mod) {
    load_module(mod);
    return call_static_method('InitFramesInit::InitFramesInit', 'report');
}
string sub load_method(string mod) {
    Loader::load(mod);
    return call_static_method('InitFramesInit::InitFramesInit', 'report');
}
";

switch (loader) {
    case "trusted": {
        if (place == "top") {
            load_module(mod);
        } else {
            Loader::load(mod);
        }
        printf("%s\n", call_static_method("InitFramesInit::InitFramesInit", "report"));
        break;
    }
    case "restricted": {
        Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_NO_PROCESS_CONTROL);
        p.parse(LoaderCode, "restricted-loader");
        printf("%s\n", p.callFunction(place == "top" ? "load_top" : "load_method", mod));
        break;
    }
    default:
        throw "PROBE-ERROR", sprintf("unknown loader %y", loader);
}

class Loader {
    static load(string mod) {
        load_module(mod);
    }
}
