# call-static-method-later-parent.q: call_static_method() calls the static method that a static method call in the
# caller's code calls (qore#7207): a caller that cannot call the private static method of the first parent class
# calls the public one of a later parent class, as the direct call does; each row prints both calls, which must be
# equal
%modern
string sub r(code c) { try { return sprintf("%y", c()); } catch (hash<ExceptionInfo> ex) { return ex.err; } }
sub row(string l, code direct, code byname) { string d = r(direct); string n = r(byname); printf("%s: direct %s by name %s%s\n", l, d, n, d == n ? "" : " DIFFERENT"); }
class P1 { static string sm() { return "P1::sm"; } }
class Cq { private static string sm() { return "Cq::sm"; }
    static run() { row("Cq context", any sub () { return X::sm(); }, any sub () { return call_static_method("X", "sm"); }); } }
class X inherits Cq, P1 {
    static run() { row("X context", any sub () { return X::sm(); }, any sub () { return call_static_method("X", "sm"); }); } }
class Z inherits Cq {
    static run() { row("other subclass of Cq", any sub () { return X::sm(); }, any sub () { return call_static_method("X", "sm"); }); } }
class K { static run() { row("unrelated class", any sub () { return X::sm(); }, any sub () { return call_static_method("X", "sm"); }); } }
class Q { private static string q() { return "Q::q"; } }
class Y inherits Q {}
class R inherits private P1 {}
row("outside", any sub () { return X::sm(); }, any sub () { return call_static_method("X", "sm"); });
Cq::run(); X::run(); Z::run(); K::run();
printf("call_static_method_args: %s\n", r(any sub () { return call_static_method_args("X", "sm"); }));
printf("Program::callStaticMethod: %s\n", r(any sub () { Program p(PO_NEW_STYLE); p.parse("class P1 { static string sm() { return \"P1::sm\"; } } class Cq { private static string sm() { return \"Cq::sm\"; } } class X inherits Cq, P1 {}", "pgm"); return p.callStaticMethod("X", "sm"); }));
printf("only private: %s\n", r(any sub () { return call_static_method("Y", "q"); }));
# a static method of a privately inherited class, which a direct call outside the class cannot call either
printf("private base: %s\n", r(any sub () { return call_static_method("R", "sm"); }));
printf("unknown: %s\n", r(any sub () { return call_static_method("X", "nope"); }));
