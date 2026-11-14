%modern
# named calls to overloaded methods and static methods whose variants name their parameters differently: the variant
# is resolved at run time by binding the arguments by name to each candidate variant; a variant to whose parameters
# the names cannot be bound is not a candidate (M4 phase 2); see compiled-overload-errors.qtest,
# AbstractMethodCallNode::execNamed() and StaticMethodCallNode::evalNamed()

class B {
    # the parameters in another order, one with another type
    string sw(int count, string label) { return sprintf("B::sw(int count, string label) count=%y label=%y", count,
        label); }
    string sw(string label, float count) { return sprintf("B::sw(string label, float count) label=%y count=%y",
        label, count); }

    # a reference parameter
    string rf(string b, reference<int> a) { a = a + 1; return sprintf("B::rf(string b, reference<int> a) b=%y", b); }
    string rf(int x) { return sprintf("B::rf(int x) x=%y", x); }

    # default and optional parameters
    string dflt(int a, string b = "db") { return sprintf("B::dflt(int a, string b) a=%y b=%y", a, b); }
    string dflt(string b, *int c) { return sprintf("B::dflt(string b, *int c) b=%y c=%y", b, c); }

    # one parameter, other names
    string one(int a) { return sprintf("B::one(int a) a=%y", a); }
    string one(string b) { return sprintf("B::one(string b) b=%y", b); }

    # a private variant: only a call in the class can select it
    string pv(int a) { return sprintf("B::pv(int a) a=%y", a); }
    private string pv(string b) { return sprintf("B::pv(string b) b=%y", b); }

    static string ss(int count, string label) { return sprintf("B::ss(int count, string label) count=%y label=%y",
        count, label); }
    static string ss(string label, float count) { return sprintf("B::ss(string label, float count) label=%y "
        "count=%y", label, count); }

    # calls of the methods of the object (self calls)
    string selfSw(auto i, auto s) { return sw(label: s, count: i); }
    string selfOne(auto v) { return v.typeCode() == NT_INT ? one(a: v) : one(b: v); }
    string selfPv(auto v) { return pv(b: v); }
    string selfStatic(auto i, auto s) { return ss(label: s, count: i); }
    string scopedSw(auto i, auto s) { return B::sw(label: s, count: i); }

    # calls made in a background thread whose arguments are taken when the thread starts
    nothing qsw(Queue q, int count, string label) { q.push(sprintf("B::qsw(Queue q, int count, string label) "
        "count=%y label=%y", count, label)); }
    nothing qsw(Queue q, string label, float count) { q.push(sprintf("B::qsw(Queue q, string label, float count) "
        "label=%y count=%y", label, count)); }
    nothing bgSelf(Queue q, auto i, auto s) { background qsw(q, label: s, count: i); }

    # a parameter list in another order than the arguments of a call bound at parse time
    string two(int a, string b) { return sprintf("B::two(int a, string b) a=%y b=%y", a, b); }

    # calls of methods of the object in default argument expressions (serialized as expression trees in AOT)
    string dsw(string s = sw(label: value_of_v(), count: value_of(1))) { return s; }
    string dtwo(string s = two(b: "s", a: 1)) { return s; }
}

# a child class that overrides a variant and adds a variant with other names
class C inherits B {
    string sw(int count, string label) { return sprintf("C::sw(int count, string label) count=%y label=%y", count,
        label); }
    string sw(bool flag) { return sprintf("C::sw(bool flag) flag=%y", flag); }

    # an explicit call of the base class's method from the child class
    string baseSw(auto i, auto s) { return B::sw(label: s, count: i); }
}

# an abstract method with variants that name their parameters differently
class A {
    abstract string am(int count, string label);
    abstract string am(string label, float count);
}

# a call of an abstract method of the class with an explicit class name
class AC inherits A {
    string callAm(auto i, auto s) { return A::am(label: s, count: i); }
}

class AI inherits AC {
    string am(int count, string label) { return sprintf("AI::am(int count, string label) count=%y label=%y", count,
        label); }
    string am(string label, float count) { return sprintf("AI::am(string label, float count) label=%y count=%y",
        label, count); }
}

final class F {
    string fs(int count, string label) { return sprintf("F::fs(int count, string label) count=%y label=%y", count,
        label); }
    string fs(string label, float count) { return sprintf("F::fs(string label, float count) label=%y count=%y",
        label, count); }
    string selfFs(auto i, auto s) { return fs(label: s, count: i); }
}

auto i = 1;
auto s = "x";
auto n;

B b();
printf("object label first: %s\n", b.sw(label: s, count: i));
printf("object count first: %s\n", b.sw(count: i, label: s));
printf("object float: %s\n", b.sw(count: 1.5, label: s));
int ri = 10;
printf("object reference: %s ri=%d\n", b.rf(b: s, a: \ri), ri);
printf("object dflt a: %s\n", b.dflt(a: i));
printf("object dflt b: %s\n", b.dflt(b: s));
printf("object dflt b c nothing: %s\n", b.dflt(b: s, c: n));
printf("object one a: %s\n", b.one(a: i));
printf("object one b: %s\n", b.one(b: s));
try {
    printf("object one b int: %s\n", b.one(b: i));
} catch (hash<ExceptionInfo> ex) {
    printf("object one b int: %s\n", ex.err);
}
# a private variant is not accessible outside the class (a parse error; see compiled-overload-errors.qtest)
printf("object public: %s\n", b.pv(a: i));

# calls in the class
printf("self: %s\n", b.selfSw(i, s));
printf("self one a: %s\n", b.selfOne(i));
printf("self one b: %s\n", b.selfOne(s));
printf("self private: %s\n", b.selfPv(s));
printf("self static: %s\n", b.selfStatic(i, s));
printf("scoped self: %s\n", b.scopedSw(i, s));

# static methods
printf("static label first: %s\n", B::ss(label: s, count: i));
printf("static float: %s\n", B::ss(count: 1.5, label: s));

# a receiver of a child class: its method is called, which overrides or inherits the variants of the base class
B bc = new C();
printf("child override: %s\n", bc.sw(label: s, count: i));
printf("child inherited: %s\n", bc.sw(count: 1.5, label: s));
C c();
printf("child self: %s\n", c.selfSw(i, s));
printf("child base call: %s\n", c.baseSw(i, s));
printf("child scoped self: %s\n", c.scopedSw(i, s));

# an abstract method called through the base class
A a = new AI();
printf("abstract: %s\n", a.am(label: s, count: i));
printf("abstract float: %s\n", a.am(count: 1.5, label: s));
AI ai();
printf("abstract scoped: %s\n", ai.callAm(i, s));
printf("abstract scoped float: %s\n", ai.callAm(1.5, s));

# a final class
F f();
printf("final: %s\n", f.fs(label: s, count: i));
printf("final self: %s\n", f.selfFs(i, s));

# a receiver that may have no value
*B ob = b;
printf("optional receiver: %s\n", ob.sw(label: s, count: i));
ob = NOTHING;
try {
    printf("no receiver: %s\n", ob.sw(label: s, count: i));
} catch (hash<ExceptionInfo> ex) {
    printf("no receiver: %s\n", ex.err);
}

# a deleted object
B db();
B dref = db;
delete db;
try {
    printf("deleted: %s\n", dref.sw(label: s, count: i));
} catch (hash<ExceptionInfo> ex) {
    printf("deleted: %s\n", ex.err);
}

# the same calls in a loop, with values of both types
list<string> l = ();
for (int k = 0; k < 4; ++k) {
    auto v = k % 2 ? k : "s" + k;
    l += k % 2 ? b.one(a: v) : b.one(b: v);
    l += bc.sw(label: "l" + k, count: k);
    l += B::ss(label: "s" + k, count: k);
}
printf("loop: %y\n", l);

# calls made in a background thread (copies of the call nodes)
Queue q();
background q.push(b.sw(label: s, count: i));
printf("background object: %s\n", q.get());
background q.push(B::ss(label: s, count: i));
printf("background static: %s\n", q.get());
background b.qsw(q, label: s, count: i);
printf("background object call: %s\n", q.get());
b.bgSelf(q, i, s);
printf("background self call: %s\n", q.get());

# named calls in default argument expressions and member initializers (serialized as expression trees in AOT)
auto sub value_of_v() { return "v"; }
auto sub value_of(auto v) { return v; }
B sub new_b() { return new B(); }
string sub dflt_static(string s = B::ss(label: value_of_v(), count: value_of(1))) { return s; }
string sub dflt_object(string s = new_b().sw(label: value_of_v(), count: value_of(2))) { return s; }
class MI {
    public {
        string st = B::ss(label: value_of_v(), count: value_of(3));
        string ob = new_b().sw(label: value_of_v(), count: value_of(4));
    }
}
printf("default static: %s\n", dflt_static());
printf("default object: %s\n", dflt_object());
MI mi();
printf("member static: %s\n", mi.st);
printf("member object: %s\n", mi.ob);
printf("default self: %s\n", b.dsw());
printf("default self child: %s\n", c.dsw());
printf("default self bound: %s\n", b.dtwo());
