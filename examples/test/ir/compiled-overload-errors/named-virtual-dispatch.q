%modern
# a named call binds the argument names to the parameters of the variants of the receiver's static (declared) type
# and is then dispatched by position to the class of the object, as the same call with positional arguments is, so
# that an override is called whatever the names of its parameters (qore#5990); see compiled-overload-errors.qtest

auto sub value_of(auto v) { return v; }

class B {
    string sw(int count, string label) { return sprintf("B::sw(int count, string label) count=%y label=%y", count,
        label); }
    string sw(string label, float count) { return sprintf("B::sw(string label, float count) label=%y count=%y",
        label, count); }
    string one(int count, string label) { return sprintf("B::one(int count, string label) count=%y label=%y",
        count, label); }

    # calls of the methods of the object (self calls)
    string selfSw(auto i, auto s) { return sw(label: s, count: i); }
    string selfSwLiteral() { return sw(label: "x", count: 1); }
    # an explicit call of the method of the class is not dispatched on the class of the object
    string scopedSw(auto i, auto s) { return B::sw(label: s, count: i); }
    # a default argument expression
    string dsw(string r = sw(label: value_of("v"), count: value_of(1))) { return r; }
}

# overrides with other parameter names
class C inherits B {
    string sw(int n, string l) { return sprintf("C::sw(int n, string l) n=%y l=%y", n, l); }
    string one(int n, string l) { return sprintf("C::one(int n, string l) n=%y l=%y", n, l); }
}

# abstract methods implemented with other parameter names
class A {
    abstract string am(int count, string label);
    abstract string am(string label, float count);

    string selfAm(auto i, auto s) { return am(label: s, count: i); }
}

class AI inherits A {
    string am(int n, string l) { return sprintf("AI::am(int n, string l) n=%y l=%y", n, l); }
    string am(string l, float n) { return sprintf("AI::am(string l, float n) l=%y n=%y", l, n); }
}

# a name that only one variant has selects that variant also for an argument that another variant matches better by
# position (an int for an int parameter rather than a float parameter): its override is called (as in C#)
class P {
    string m(float x) { return "P::m(float x)"; }
    string m(int n) { return "P::m(int n)"; }
}
class Q inherits P {
    string m(float x) { return "Q::m(float x)"; }
    string m(int n) { return "Q::m(int n)"; }
}

# a single variant called by an unqualified named call bound at parse time, on an object of a subclass, and arguments
# of the same type in another order
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

# a named call in a const method
class KC {
    string cm(int x) const { return sprintf("KC::cm(int x) const x=%y", x); }
    string cm(string s) { return sprintf("KC::cm(string s) s=%y", s); }
    string callConst() const { return cm(x: 1) + " / " + self.cm(x: 2); }
}

# static methods called through an object
class SA {
    static string sm(int x) { return "SA::sm(int x)"; }
    static string sm(float f) { return "SA::sm(float f)"; }
}
class SB inherits SA {
    static string sm(int x) { return "SB::sm(int x)"; }
    static string sm(float g) { return "SB::sm(float g)"; }
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

# a receiver of the child class declared with the base class: the override is called
B bc = new C();
printf("virtual literal: %s\n", bc.sw(count: 1, label: "x"));
printf("virtual runtime: %s\n", bc.sw(count: i, label: s));
printf("virtual runtime label first: %s\n", bc.sw(label: s, count: i));
printf("positional: %s\n", bc.sw(i, s));
printf("inherited: %s\n", bc.sw(count: 1.5, label: s));
printf("virtual one: %s\n", bc.one(count: i, label: s));
try {
    printf("virtual wrong types: %s\n", bc.sw(count: s, label: i));
} catch (hash<ExceptionInfo> ex) {
    printf("virtual wrong types: %s\n", ex.err);
}

# the receiver declared with the child class takes the names of the override
C c();
printf("child names: %s\n", c.one(n: i, l: s));
printf("child names literal: %s\n", c.one(n: 1, l: "x"));

# calls in the class
printf("self: %s\n", bc.selfSw(i, s));
printf("self literal: %s\n", bc.selfSwLiteral());
printf("scoped: %s\n", bc.scopedSw(i, s));
printf("default: %s\n", bc.dsw());

# abstract methods
A a = new AI();
printf("abstract literal: %s\n", a.am(count: 1, label: "x"));
printf("abstract runtime: %s\n", a.am(count: i, label: s));
printf("abstract runtime float: %s\n", a.am(count: 1.5, label: s));
printf("abstract self: %s\n", a.selfAm(i, s));
AI ai();
printf("implementation names: %s\n", ai.am(n: i, l: s));

# the same calls in a loop
list<string> l = ();
for (int k = 0; k < 3; ++k) {
    l += bc.sw(label: "l" + k, count: k);
    l += a.am(label: "a" + k, count: k);
}
printf("loop: %y\n", l);

# the variant that the names select
P pq = new Q();
P pp = new P();
Q qq = new Q();
printf("names select literal: %s\n", pq.m(x: 1));
printf("names select runtime: %s\n", pq.m(x: i));
printf("names select base literal: %s\n", pp.m(x: 1));
printf("names select base runtime: %s\n", pp.m(x: i));
printf("names select child literal: %s\n", qq.m(x: 1));
printf("names select child runtime: %s\n", qq.m(x: i));
printf("names select other: %s\n", pq.m(n: i));
printf("positional: %s\n", pq.m(i));

# a single variant by an unqualified named call (bound at parse time) on an object of a subclass
SVC svc();
printf("single variant swapped: %s\n", svc.self1());
printf("single variant: %s\n", svc.self2());

# the return type of a named call of a generic method is that of the variant with its type arguments
GM gm = new GMC();
int gi = gm.id(x: 2);
string gs = gm.id(x: "g");
printf("generic method: %y %y\n", gi, gs);

# a named call in a const method calls the const variant that the names select
printf("const method: %s\n", (new KC()).callConst());

# a static method called through an object: the override of the variant that the names select
SA sa = new SB();
printf("static through object literal: %s\n", sa.sm(f: 1));
printf("static through object runtime: %s\n", sa.sm(f: i));
printf("static scoped: %s\n", SA::sm(f: i));

# the override of a variant of a specialization of a generic class
GB<int> gb = new GC();
printf("generic override literal: %s\n", gb.m(x: 1));
printf("generic override runtime: %s\n", gb.m(x: i));

# a call made in a background thread
Queue q();
background q.push(bc.sw(label: s, count: i));
printf("background: %s\n", q.get());
