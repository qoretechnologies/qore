%modern
# loads the apiold and apicur test modules (see apiprobe-module.cpp) and prints the result; see module-api-version.qtest

try {
    load_module("apiold");
    print("apiold: loaded\n");
} catch (hash<ExceptionInfo> ex) {
    printf("apiold: %s: %s\n", ex.err, ex.desc);
}

load_module("apicur");
hash<auto> info = get_module_hash().apicur;
printf("apicur: loaded API %d.%d\n", info.api_major, info.api_minor);
