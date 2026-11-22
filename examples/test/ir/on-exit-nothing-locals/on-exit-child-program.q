%modern
# on_exit in a function of a child Program that a script calls with Program::callFunction(): the handler restores the
# local that holds a saved value, or NOTHING, when an exception leaves the function, called directly, through
# call_function_args() and through a method; see on-exit-nothing-locals.qtest (#7259, #7244)

#! the code of the child Program: a global, and wrappers that save it in a local, set it and restore it with on_exit
const ChildCode = "
our hash<auto> st = {'g': -1};
sub fail() { throw 'X', 'boom'; }
*int sub get() { return st.g; }
sub setg(*int g) { st.g = g; }
auto sub wrapArgs(string f, auto a, *int g) {
    *int s = st.g;
    on_exit st.g = s;
    st.g = g;
    return call_function_args(f, a);
}
auto sub wrapDirect(*int g) {
    *int s = st.g;
    on_exit st.g = s;
    st.g = g;
    fail();
}
auto sub wrapOk(*int g) {
    *int s = st.g;
    on_exit st.g = s;
    st.g = g;
    return st.g;
}
class W {
    private { *int saved; }
    auto run(*int g) {
        *int s = st.g;
        on_exit st.g = s;
        st.g = g;
        fail();
    }
}
auto sub wrapMethod(*int g) {
    W w();
    return w.run(g);
}
auto sub wrapTry(*int g) {
    try {
        *int s = st.g;
        on_exit st.g = s;
        st.g = g;
        fail();
    } catch (hash<ExceptionInfo> ex) {
        return st.g;
    }
}
";

#! a child Program with the code, the code of the wrapper parsed in a pending parse of its own as Qorus injects it
Program sub make() {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    p.parsePending(ChildCode, "child");
    p.parseCommit();
    return p;
}

#! calls a function of the Program that throws and reports the value of the global afterwards
string sub check(Program p, string name, *int initial, list<auto> args) {
    p.callFunction("setg", initial);
    string r;
    try {
        r = sprintf("returned %y", p.callFunctionArgs(name, args));
    } catch (hash<ExceptionInfo> ex) {
        r = "threw " + ex.err;
    }
    return sprintf("%s: %s, then %y", name, r, p.callFunction("get"));
}

for (int i = 0; i < 3; ++i) {
    Program p = make();
    foreach *int initial in (-1, NOTHING, 0) {
        printf("initial %y\n", initial);
        printf("  %s\n", check(p, "wrapDirect", initial, (42,)));
        printf("  %s\n", check(p, "wrapArgs", initial, ("fail", (), 42)));
        printf("  %s\n", check(p, "wrapMethod", initial, (42,)));
        printf("  %s\n", check(p, "wrapTry", initial, (42,)));
        printf("  %s\n", check(p, "wrapOk", initial, (42,)));
    }
}
