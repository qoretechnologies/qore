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

# a call made in a background thread
Queue q();
background q.push(bc.sw(label: s, count: i));
printf("background: %s\n", q.get());
