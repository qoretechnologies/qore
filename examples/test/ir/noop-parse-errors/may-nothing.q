%modern
# calls of builtins whose arguments may have no value at run time: they parse, and raise RUNTIME-OVERLOAD-ERROR when
# the value is NOTHING, as only a no-op variant accepts it (issue #5686); see noop-parse-errors.qtest
*string sub s() { return; }
*int sub i() { return; }
*list<auto> sub l() { return; }
*date sub d() { return; }
*binary sub b() { return; }
sub run(string label, code<auto()> f) {
    try {
        printf("%s: %y\n", label, f());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}
sub main() {
    for (int n = 0; n < 3; ++n) {
        run("substr", auto sub () { return substr(s(), 1); });
        run("sprintf", auto sub () { return sprintf(s()); });
        run("vsprintf", auto sub () { return vsprintf(s(), ()); });
        run("chr", auto sub () { return chr(i()); });
        run("setenv", auto sub () { return setenv(s(), "x"); });
        run("unsetenv", auto sub () { return unsetenv(s()); });
        run("getenv", auto sub () { return getenv(s()); });
        run("reverse", auto sub () { return reverse(l()); });
        run("exists_function", auto sub () { return exists_function(s()); });
        run("is_date_absolute", auto sub () { return is_date_absolute(d()); });
        # regular variants that accept NOTHING
        run("length", auto sub () { return length(s()); });
        run("strlen", auto sub () { return strlen(s()); });
        # the regular variants for NOTHING have the parameter names of the regular variants, so named calls bind
        run("length str:", auto sub () { return length(str: s()); });
        run("strlen str:", auto sub () { return strlen(str: s()); });
        run("length bin:", auto sub () { return length(bin: b()); });
        run("length str: NOTHING", auto sub () { return length(str: NOTHING); });
        run("length str: value", auto sub () { return length(str: "abc"); });
        run("length bin: value", auto sub () { return length(bin: <0102>); });
        # the same calls with values
        run("substr value", auto sub () { return substr("abc", 1); });
        run("chr value", auto sub () { return chr(65); });
    }
}
main();
