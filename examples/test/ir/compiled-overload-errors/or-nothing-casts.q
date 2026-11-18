%modern
# a cast to an or-nothing type that fails names the or-nothing type in all execution modes, as the AST interpreter does
# (issue #5717: AOT named 'int' for cast<*int>); other cast targets and successful casts are compared too; see
# compiled-overload-errors.qtest
class O {}
class K {}
hashdecl H { int a; }
enum E { A = 1 }
sub t(string label, code f) {
    string r;
    try { r = sprintf("%y", f()); } catch (hash<ExceptionInfo> ex) { r = ex.err + ": " + ex.desc; }
    printf("%s=%s\n", label, r);
}
sub main() {
    auto o = new O();
    auto n;
    auto s = "12";
    auto i = 1;
    auto e = 7;
    for (int x = 0; x < 3; ++x) {
        t("int-obj", auto sub () { return cast<*int>(o); });
        t("float-obj", auto sub () { return cast<*float>(o); });
        t("number-obj", auto sub () { return cast<*number>(o); });
        t("string-obj", auto sub () { return cast<*string>(o); });
        t("bool-obj", auto sub () { return cast<*bool>(o); });
        t("date-obj", auto sub () { return cast<*date>(o); });
        t("binary-obj", auto sub () { return cast<*binary>(o); });
        t("int-plain-obj", auto sub () { return cast<int>(o); });
        t("int-str", auto sub () { return cast<*int>(s); });
        t("float-str", auto sub () { return cast<*float>(s); });
        t("int-nothing", auto sub () { return cast<*int>(n); });
        t("string-nothing", auto sub () { return cast<*string>(n); });
        t("class-int", auto sub () { return cast<*K>(i); });
        t("class-obj", auto sub () { return cast<*K>(o); });
        t("class-nothing", auto sub () { return cast<*K>(n); });
        t("hd-int", auto sub () { return cast<*hash<H>>(i); });
        t("chash-int", auto sub () { return cast<*hash<string, int>>(i); });
        t("list-int", auto sub () { return cast<*list<int>>(i); });
        t("enum-str", auto sub () { return cast<*E>(s); });
        t("enum-int", auto sub () { return cast<*E>(e); });
    }
}
main();
