%modern
# usage: module-api-hint-probe.q <feature>
# loads a feature that is not found and prints the error; see module-api-version.qtest

try {
    load_module(ARGV[0]);
    printf("%s: loaded\n", ARGV[0]);
} catch (hash<ExceptionInfo> ex) {
    printf("%s: %s: %s\n", ARGV[0], ex.err, ex.desc);
}
