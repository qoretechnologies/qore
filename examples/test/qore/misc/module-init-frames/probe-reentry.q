#!/usr/bin/env qore
# the code that loads a module, called back by the module's initialization (qore#7289): run by ../module-init-frames.qtest
# ARGV[0]: how the module is loaded: "name" or "path"; ARGV[1]: the directory of the modules
# The code called back makes calls into another Program (the module InitFramesProbe), which the IR interpreter can run
# inline: their frames name the loading Program, whose code makes them

%modern
# the module that the code called back calls is loaded already, as a library module that the process uses is
%requires InitFramesProbe

#! the code that the initialization calls back: calls into another Program, of a function and of a static method
hash<auto> sub called_back() {
    return {
        "fn": InitFramesProbe::caller_pgm(),
        "static": InitFramesProbe::Probe::callerPgm(),
    };
}

#! the same calls outside the initialization, many times, so that the calls run as they do when they are hot
int sub warm_up() {
    int rv;
    for (int i = 0; i < 100; ++i) {
        rv = called_back().fn;
    }
    return rv;
}

string how = ARGV[0];
string dir = ARGV[1];
# the loading Program, as the frames of its calls name it
int own = warm_up();
set_module_option("InitFramesReentry", "cb", \called_back());
load_module(how == "path" ? dir + "/InitFramesReentry.qm" : "InitFramesReentry");
hash<auto> seen = call_static_method("InitFramesReentry::InitFramesReentry", "report");
printf("reentry-fn=%s reentry-static=%s\n", seen.fn == own ? "loader" : "OTHER",
    seen."static" == own ? "loader" : "OTHER");
