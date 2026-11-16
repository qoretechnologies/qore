# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of generic-function-local-object-type.qtest: the types that name the type parameters of generic functions
# with a debugger attached, which makes qcc programs read the IR of their functions from the AOT image (qore#6261)

%modern
%allow-debugger

class Stepper inherits DebugProgram {
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl pgm, reference rs, reference rts) {}
    onStep(ProgramControl pgm, int block, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) { rs = DebugStep; }
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        rs = DebugStep;
    }
    onExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) { rs = DebugStep; }
}
class B<T> {
    public { list<T> l; }
    constructor(list<T> x) { l = x; }
    string desc() { return sprintf("%s %y", (new list<T>()).fullType(), l); }
}
hashdecl H<T> { T v; }
sub show(string label, code c) {
    try { printf("%s => %y\n", label, c()); } catch (hash<ExceptionInfo> ex) { printf("%s => %s: %s\n", label, ex.err, ex.desc); }
}
auto sub castU<U>(auto v) { return cast<B<U>>(v).desc(); }
auto sub instOf<U>(auto v) { return v instanceof B<U>; }
auto sub instOfL<U>(auto v) { return v instanceof list<U>; }
auto sub typedHash<U>(U v) { hash<string, U> h(); h.a = v; return h.fullType(); }
auto sub typedList<U>(U v) { list<U> l(); l += v; return l.fullType(); }
auto sub newList<U>(U v) { return (new list<U>()).fullType(); }
auto sub newHash<U>(U v) { return (new hash<string, U>()).fullType(); }
auto sub hdNew<U>(U v) { return (new hash<H<U>>({"v": v})).fullType(); }
auto sub localList<U>(U v) { list<B<U>> lb = (new B<U>((v,)),); return lb.fullType(); }
auto sub localHash<U>(U v) { hash<string, B<U>> hb = {"a": new B<U>((v,))}; return hb.fullType(); }
auto sub listOfU<U>(U v) { list<U> l = (v, v); return l.fullType(); }
auto sub hashOfU<U>(U v) { hash<string, U> h = {"a": v, "b": v}; return h.fullType(); }
auto sub listOfUWrong<U>(U v) { list<U> l = (v,); l += "x"; return l; }
auto sub typedClosureRet<U>(U v) { code c = list<U> sub (U x) { list<U> l = (x,); return l; }; return c(v).fullType(); }
auto sub callRef<U>(list<U> l) { code c = \castU<U>(); return c(new B<U>(l)); }
auto sub castList<U>(auto v) { return cast<list<U>>(v).fullType(); }
auto sub defInst<U>(auto v, bool b = (new B<int>((1,))) instanceof B<U>) { return b; }
auto sub defList<U>(U v, list<U> l = new list<U>()) { return l.fullType(); }
auto sub defHash<U>(U v, hash<string, U> h = new hash<string, U>()) { return h.fullType(); }
auto sub defCast<U>(auto v, auto x = cast<list<U>>((1,))) { return x.fullType(); }
string sub viaDecl<U>(list<U> l) { B<U> b(l); return b.desc(); }
string sub viaNew<U>(list<U> l) { return (new B<U>(l)).desc(); }
string sub viaClosure<U>(list<U> l) { code c = string sub () { B<U> b(l); return b.desc(); }; return c(); }
class M {
    static string s<U>(list<U> l) { B<U> b(l); return b.desc(); }
    string m<U>(list<U> l) { return (new B<U>(l)).desc(); }
}
sub runCases() {
    show("declaration", auto sub () { return viaDecl<int>((1,)); });
    show("new", auto sub () { return viaNew<string>(("a",)); });
    show("declaration in a closure", auto sub () { return viaClosure<int>((1,)); });
    show("static method", auto sub () { return M::s<int>((1,)); });
    show("method", auto sub () { M m(); return m.m<int>((1,)); });
    show("cast", auto sub () { return castU<int>(new B<int>((1,))); });
    show("instanceof", auto sub () { return instOf<int>(new B<int>((1,))); });
    show("instanceof neg", auto sub () { return instOf<string>(new B<int>((1,))); });
    show("instanceof list", auto sub () { return instOfL<int>(cast<list<int>>((1,))); });
    show("typed hash", auto sub () { return typedHash<int>(1); });
    show("typed list", auto sub () { return typedList<int>(1); });
    show("new list", auto sub () { return newList<int>(1); });
    show("new hash", auto sub () { return newHash<int>(1); });
    show("hashdecl new", auto sub () { return hdNew<int>(1); });
    show("local list<B<U>>", auto sub () { return localList<int>(1); });
    show("local hash<B<U>>", auto sub () { return localHash<int>(1); });
    show("list<U>", auto sub () { return listOfU<int>(1); });
    show("hash<U>", auto sub () { return hashOfU<int>(1); });
    show("list<U> wrong", auto sub () { return listOfUWrong<int>(1); });
    show("typed closure ret", auto sub () { return typedClosureRet<int>(1); });
    show("call ref", auto sub () { return callRef<int>((1,)); });
    show("cast list", auto sub () { return castList<int>((1,)); });
    show("def instanceof", auto sub () { return defInst<int>(new B<int>((1,))); });
    show("def instanceof neg", auto sub () { return defInst<string>(new B<int>((1,))); });
    show("def list", auto sub () { return defList<int>(1); });
    show("def hash", auto sub () { return defHash<int>(1); });
    show("def cast", auto sub () { return defCast<int>((1,)); });
}
Stepper stepper();
ProgramControl pc = ProgramControl::getProgram();
Counter done(1);
background sub () {
    on_exit done.dec();
    stepper.addProgram(pc);
    on_exit stepper.removeProgram(pc);
    call_function("runCases");
}();
done.waitForZero();
