%modern

# a Program whose runtime parse options restrict the network domain after its code was parsed: the implicit Socket
# constructor of its classes with constructors is refused, also when the user constructor is called directly and when
# it is the implicit constructor of a base class (variant resolved at run time)
string sub run(string src, string f) {
    Program p(PO_MODERN);
    p.parse(src, "nonet");
    p.setParseOptions(PO_NO_NETWORK);
    try {
        return p.callFunction(f);
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s", ex.err, ex.desc);
    }
}

const Src = "class WC inherits Socket { constructor() {} }
class B inherits Socket { constructor() {} }
class C inherits B {}
class D inherits B { constructor() {} }
string sub wc() { WC w(); return \"WC ok\"; }
string sub c() { C c(); return \"C ok\"; }
string sub d() { D d(); return \"D ok\"; }
";

foreach string f in (("wc", "c", "d")) {
    printf("%s: %s\n", f, run(Src, f));
}
