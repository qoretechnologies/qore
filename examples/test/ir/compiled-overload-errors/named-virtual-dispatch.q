%modern
# a named call is resolved as the call with positional arguments that it spells out: the classes of the object are
# searched from the class of the object, and the first class with a variant that accepts the names of the arguments
# and the arguments is used; a variant whose parameters do not have the names is not a candidate, so the call differs
# from the positional call only where the names rule a variant out (qore#5990); see compiled-overload-errors.qtest

auto sub value_of(auto v) { return v; }

class B {
    string sw(int count, string label) { return sprintf("B::sw(int count, string label) count=%y label=%y", count,
        label); }
    string sw(string label, float count) { return sprintf("B::sw(string label, float count) label=%y count=%y",
        label, count); }

    # calls of the methods of the object (self calls)
    string selfSw(auto i, auto s) { return sw(label: s, count: i); }
    string selfSwLiteral() { return sw(label: "x", count: 1); }
    string selfSwPositional(auto i, auto s) { return sw(i, s); }
    # an explicit call of the method of the class is resolved on the class
    string scopedSw(auto i, auto s) { return B::sw(label: s, count: i); }
    # a default argument expression
    string dsw(string r = sw(label: value_of("v"), count: value_of(1))) { return r; }
}

# an override with the same parameter names: both forms call it
class CS inherits B {
    string sw(int count, string label) { return sprintf("CS::sw(int count, string label) count=%y label=%y", count,
        label); }
}

# an override with other parameter names: the names rule it out, and the variant of the parent class that has them is
# called
class CR inherits B {
    string sw(int n, string l) { return sprintf("CR::sw(int n, string l) n=%y l=%y", n, l); }
}

# a child class method that is another overload, which the positional call selects first (the issue's example)
class M {
    string m(string x) { return "M::m(string x)"; }
}
class MY inherits M {
    string m(*string y) { return "MY::m(*string y)"; }
}
class MX inherits M {
    string m(*string x) { return "MX::m(*string x)"; }
}

# abstract methods
class A {
    abstract string am(int count, string label);
    string selfAm(auto i, auto s) { return am(label: s, count: i); }
}
# implemented with the same names
class AS inherits A {
    string am(int count, string label) { return sprintf("AS::am(int count, string label) count=%y label=%y", count,
        label); }
}
# implemented with other names: no variant that can be called has the names
class AR inherits A {
    string am(int n, string l) { return sprintf("AR::am(int n, string l) n=%y l=%y", n, l); }
}

# a name that only one variant has selects that variant also for an argument that another variant matches better by
# position (an int for an int parameter rather than a float parameter)
class P {
    string m(float x) { return "P::m(float x)"; }
    string m(int n) { return "P::m(int n)"; }
}
class Q inherits P {
    string m(float x) { return "Q::m(float x)"; }
    string m(int n) { return "Q::m(int n)"; }
}

# a single variant called by an unqualified named call on an object of a subclass, and arguments of the same type in
# another order
class SV {
    string r(int x, int y) { return sprintf("SV::r x=%y y=%y", x, y); }
    string one(int x) { return sprintf("SV::one x=%y", x); }
    string self1() { return r(y: 2, x: 1); }
    string self2() { return one(x: 3); }
}
class SVC inherits SV {}

# the return type of a named call of a generic method
class GM {
    U id<U>(U x) { return x; }
    string id(string s, int k) { return s + k; }
}
class GMC inherits GM {}

# the overrides of generic methods
class GO {
    string id<U>(U x) { return "GO::id"; }
    string opt<U>(*U x) { return "GO::opt"; }
}
class GOS inherits GO {
    string id<U>(U x) { return "GOS::id"; }
    string opt<U>(*U x) { return "GOS::opt"; }
}
class GOR inherits GO {
    string id<U>(U y) { return "GOR::id"; }
}

# a named call in a const method
class KC {
    string cm(int x) const { return sprintf("KC::cm(int x) const x=%y", x); }
    string cm(string s) { return sprintf("KC::cm(string s) s=%y", s); }
    string callConst() const { return cm(x: 1) + " / " + self.cm(x: 2); }
}

# static methods: called through an object, they are resolved on the class of the object; called in the class, on
# the class of the call
class SA {
    static string sm(int x) { return "SA::sm(int x)"; }
    static string sm(float f) { return "SA::sm(float f)"; }
    string selfStatic(auto v) { return sm(f: v); }
    string selfStaticPositional(auto v) { return sm(v); }
}
class SBS inherits SA {
    static string sm(float f) { return "SBS::sm(float f)"; }
}
class SBR inherits SA {
    static string sm(float g) { return "SBR::sm(float g)"; }
}

# typed results: an overload of a child class that a named call selects by the names of the parameters of the variant
# of the parent class returns a type that the parent's accepts (refused otherwise; see override-return-types.qtest),
# so the compiled modes can trust the type of the result
class TB {
    int m(int a, string b) { return 1; }
}
class TC inherits TB {
    int m(string b, int a) { return 2; }
}

# a variant of a specialization of a generic class and its override
class GB<T> {
    string m(T x) { return "GB::m(T x)"; }
}
class GC inherits GB<int> {
    string m(int x) { return "GC::m(int x)"; }
}

auto i = 1;
auto s = "x";

# arguments whose types are known select the variant that matches them best (an int matches an int parameter
# better than a float parameter), as for positional arguments
B b();
printf("base literal: %s\n", b.sw(count: 1, label: "x"));
printf("base literal float: %s\n", b.sw(count: 1.5, label: "x"));

# an override with the same names: both forms call it
B bs = new CS();
printf("same names: %s / %s\n", bs.sw(count: i, label: s), bs.sw(i, s));
printf("same names literal: %s\n", bs.sw(label: "x", count: 1));
printf("same names inherited: %s\n", bs.sw(count: 1.5, label: s));

# an override with other names: the names select the variant of the parent class that has them
B br = new CR();
printf("other names: %s / %s\n", br.sw(count: i, label: s), br.sw(i, s));
printf("other names literal: %s\n", br.sw(label: "x", count: 1));
try {
    printf("wrong types: %s\n", br.sw(count: s, label: i));
} catch (hash<ExceptionInfo> ex) {
    printf("wrong types: %s\n", ex.err);
}
# the receiver declared with the child class can use the names of either variant
CR cr();
printf("child class names: %s / %s\n", cr.sw(n: i, l: s), cr.sw(count: i, label: s));

# the issue's example: the overload of the child class is selected first by the positional call, and by the named call
# only if its parameter has the name
M omy = new MY();
M omx = new MX();
printf("overload other name: %s / %s\n", omy.m(x: "a"), omy.m("a"));
printf("overload same name: %s / %s\n", omx.m(x: "a"), omx.m("a"));

# calls in the class: resolved on the class of the object
printf("self same names: %s / %s\n", bs.selfSw(i, s), bs.selfSwPositional(i, s));
printf("self other names: %s / %s\n", br.selfSw(i, s), br.selfSwPositional(i, s));
printf("self literal: %s\n", bs.selfSwLiteral());
printf("scoped: %s\n", bs.scopedSw(i, s));
printf("default: %s / %s\n", bs.dsw(), br.dsw());

# abstract methods: an implementation with other names cannot be called by the names of the abstract method
A oas = new AS();
printf("abstract same names: %s / %s\n", oas.am(count: i, label: s), oas.am(i, s));
printf("abstract same names literal: %s\n", oas.am(label: "x", count: 1));
printf("abstract self: %s\n", oas.selfAm(i, s));
A ar = new AR();
printf("abstract other names positional: %s\n", ar.am(i, s));
try {
    printf("abstract other names: %s\n", ar.am(count: i, label: s));
} catch (hash<ExceptionInfo> ex) {
    printf("abstract other names: %s\n", ex.err);
}
try {
    printf("abstract other names self: %s\n", ar.selfAm(i, s));
} catch (hash<ExceptionInfo> ex) {
    printf("abstract other names self: %s\n", ex.err);
}
AR arr();
printf("implementation names: %s\n", arr.am(n: i, l: s));

# the same calls in a loop
list<string> l = ();
for (int k = 0; k < 3; ++k) {
    l += bs.sw(label: "s" + k, count: k);
    l += br.sw(label: "r" + k, count: k);
    l += oas.am(label: "a" + k, count: k);
}
printf("loop: %y\n", l);

# the variant that the names select
P pq = new Q();
P pp = new P();
printf("names select: %s / %s\n", pq.m(x: 1), pq.m(x: i));
printf("names select base: %s / %s\n", pp.m(x: 1), pp.m(x: i));
printf("names select other: %s / %s\n", pq.m(n: i), pq.m(i));

# a single variant by an unqualified named call on an object of a subclass
SVC svc();
printf("single variant swapped: %s\n", svc.self1());
printf("single variant: %s\n", svc.self2());

# the return type of a named call of a generic method is that of the variant with its type arguments
GM gm = new GMC();
int gi = gm.id(x: 2);
string gs = gm.id(x: "g");
printf("generic method: %y %y\n", gi, gs);

# the overrides of generic methods
GO gos = new GOS();
GO gor = new GOR();
printf("generic same names: %s / %s\n", gos.id(x: 2), gos.id(2));
printf("generic same names runtime: %s\n", gos.id(x: i));
printf("generic or nothing: %s / %s\n", gos.opt(x: value_of(NOTHING)), gos.opt(value_of(NOTHING)));
printf("generic other names: %s / %s\n", gor.id(x: 2), gor.id(2));

# a named call in a const method calls the const variant that the names select
printf("const method: %s\n", (new KC()).callConst());

# static methods
SA sas = new SBS();
SA sar = new SBR();
printf("static through object same names: %s / %s\n", sas.sm(f: 1.5), sas.sm(1.5));
printf("static through object other names: %s / %s\n", sar.sm(f: 1.5), sar.sm(1.5));
printf("static through object runtime: %s\n", sas.sm(f: value_of(1.5)));
printf("static self: %s / %s\n", sas.selfStatic(1.5), sas.selfStaticPositional(1.5));
printf("static scoped: %s\n", SA::sm(f: i));

# the override of a variant of a specialization of a generic class
GB<int> gb = new GC();
printf("generic class override: %s / %s\n", gb.m(x: 1), gb.m(1));
printf("generic class override runtime: %s\n", gb.m(x: i));

# typed results of calls resolved on the class of the object
TB tb = new TC();
int tn = tb.m(a: 1, b: "x") + 40;
int tp = tb.m(1, "x") + 40;
printf("typed results: %y %y\n", tn, tp);

# a call made in a background thread
Queue q();
background q.push(bs.sw(label: s, count: i));
printf("background: %s\n", q.get());
background q.push(br.sw(label: s, count: i));
printf("background other names: %s\n", q.get());
