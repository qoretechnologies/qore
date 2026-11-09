%modern
# usage: module-api-probe.q <path of apiold-api-2.0.qmod>
# loads the apiold and apicur test modules (see apiprobe-module.cpp) and prints the results; see module-api-version.qtest

# by its feature name, as %requires or load_module() finds a module: the loader looks only for the supported module
# API, so apiold-api-2.0.qmod is not found, and the error names it
try {
    load_module("apiold");
    print("feature apiold: loaded\n");
} catch (hash<ExceptionInfo> ex) {
    printf("feature apiold: %s: %s\n", ex.err, ex.desc);
}

# by its path: the module is loaded and refused because of the module API that it declares
try {
    load_module(ARGV[0]);
    print("path apiold: loaded\n");
} catch (hash<ExceptionInfo> ex) {
    printf("path apiold: %s: %s\n", ex.err, ex.desc);
}

load_module("apicur");
hash<auto> info = get_module_hash().apicur;
printf("apicur: loaded API %d.%d\n", info.api_major, info.api_minor);
