%modern
# named calls to overloaded functions whose variants name their parameters differently: the variant is resolved at
# run time by binding the arguments by name to each candidate variant; a variant to whose parameters the names cannot
# be bound is not a candidate (M4); see compiled-overload-errors.qtest and QoreFunction::runtimeFindVariant()

# the parameters in another order, one with another type
string sub sw(int count, string label) { return sprintf("sw(int count, string label) count=%y label=%y", count,
    label); }
string sub sw(string label, float count) { return sprintf("sw(string label, float count) label=%y count=%y", label,
    count); }

# the same parameters in another order: a call that both match equally well is ambiguous
string sub tie(int a, string b) { return sprintf("tie(int a, string b) a=%y b=%y", a, b); }
string sub tie(string b, int a) { return sprintf("tie(string b, int a) b=%y a=%y", b, a); }

# a reference parameter
string sub rf(string b, reference<int> a) { a = a + 1; return sprintf("rf(string b, reference<int> a) b=%y", b); }
string sub rf(int x) { return sprintf("rf(int x) x=%y", x); }

# the order of the parameters of a call made in a background thread
string sub bg(Queue q, string b, string a) { q.push(sprintf("bg(Queue q, string b, string a) b=%y a=%y", b, a));
    return ""; }
string sub bg(Queue q, int x, string y) { q.push(sprintf("bg(Queue q, int x, string y) x=%y y=%y", x, y));
    return ""; }

# one parameter, other names
string sub one(int a) { return sprintf("one(int a) a=%y", a); }
string sub one(string b) { return sprintf("one(string b) b=%y", b); }

# default and optional parameters
string sub dflt(int a, string b = "db") { return sprintf("dflt(int a, string b) a=%y b=%y", a, b); }
string sub dflt(string b, *int c) { return sprintf("dflt(string b, *int c) b=%y c=%y", b, c); }

# variable arguments
string sub va(int a, ...) { return sprintf("va(int a, ...) a=%y argv=%y", a, argv); }
string sub va(string s) { return sprintf("va(string s) s=%y", s); }

# positional and named arguments
string sub mix(int a, int b, string c) { return sprintf("mix(int a, int b, string c) a=%y b=%y c=%y", a, b, c); }
string sub mix(int a, string c, float b) { return sprintf("mix(int a, string c, float b) a=%y c=%y b=%y", a, c, b); }

sub run(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

auto i = 1;
auto s = "x";
auto n;

# top-level calls
printf("sw label first: %s\n", sw(label: s, count: i));
printf("sw count first: %s\n", sw(count: i, label: s));
printf("sw float: %s\n", sw(count: 1.5, label: s));
try {
    printf("tie: %s\n", tie(a: i, b: s));
} catch (hash<ExceptionInfo> ex) {
    printf("tie: %s\n", ex.err);
}
int ri = 10;
printf("reference: %s ri=%d\n", rf(b: s, a: \ri), ri);
printf("one b: %s\n", one(b: s));
printf("one a: %s\n", one(a: i));
try {
    printf("one b int: %s\n", one(b: i));
} catch (hash<ExceptionInfo> ex) {
    printf("one b int: %s\n", ex.err);
}
printf("dflt a: %s\n", dflt(a: i));
printf("dflt b: %s\n", dflt(b: s));
printf("dflt b c nothing: %s\n", dflt(b: s, c: n));
printf("dflt b c: %s\n", dflt(b: s, c: i));
printf("va a: %s\n", va(a: i));
printf("va s: %s\n", va(s: s));
printf("mix: %s\n", mix(i, c: s, b: 2));
printf("mix float: %s\n", mix(i, c: s, b: 2.5));
try {
    printf("mix wrong: %s\n", mix(i, c: i, b: s));
} catch (hash<ExceptionInfo> ex) {
    printf("mix wrong: %s\n", ex.err);
}

# the same calls in a loop, with values of both types
list<string> l = ();
for (int k = 0; k < 4; ++k) {
    auto v = k % 2 ? k : "s" + k;
    l += k % 2 ? one(a: v) : one(b: v);
    l += sw(label: "l" + k, count: k);
}
printf("loop: %y\n", l);

# a call made in a background thread (a copy of the call node)
Queue q();
background q.push(sw(label: s, count: i));
printf("background: %s\n", q.get());
auto av = "A";
auto bv = "B";
background bg(q, a: av, b: bv);
printf("background call: %s\n", q.get());

# named calls in default argument expressions and member initializers (serialized as expression trees in AOT)
string sub g2(string a, string b) { return sprintf("g2(string a, string b) a=%y b=%y", a, b); }
auto sub value_of_v() { return "v"; }
string sub dflt2(string s = g2(b: "B3", a: "A3")) { return s; }
string sub dflt3(string s = one(b: value_of_v())) { return s; }
class MI {
    public {
        string m = g2(b: "B", a: "A");
        string r = one(b: value_of_v());
    }
}
printf("default committed: %s\n", dflt2());
printf("default runtime: %s\n", dflt3());
MI mi();
printf("member committed: %s\n", mi.m);
printf("member runtime: %s\n", mi.r);

# variants with the same parameter names that match equally well take the arguments at the same positions: the
# first one is called, as for positional arguments
string sub same(*int a) { return "same(*int a)"; }
string sub same(*string a) { return "same(*string a)"; }
auto nv;
printf("same names positional: %s\n", same(nv));
printf("same names: %s\n", same(a: nv));
printf("same names NOTHING: %s\n", same(a: NOTHING));

# builtin functions resolved at run time by parameter name
auto bs = "abc";
auto bb = <616263>;
printf("length bin: %y\n", length(bin: bb));
printf("length str: %y\n", length(str: bs));
printf("substr: %y\n", substr(str: bs, start: 1));
printf("substr len: %y\n", substr(str: bs, start: 0, len: 2));
