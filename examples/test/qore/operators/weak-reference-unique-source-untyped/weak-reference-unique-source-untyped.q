%modern
%allow-weak-references

# a weak reference to a variable designates the variable's own container whatever the declared type of the variable
# and whatever kind of variable it is: an untyped (auto) local, a global, a class static variable, a variable that a
# closure captures; a container that the variable shares with another owner is made unique first (qore#5847)

class StaticSources {
    public {
        static hash<string, int> th = {"a": 1};
        static list<int> tl = (1, 2);
        static hash uh = {"a": 1};
        static list ul = (1, 2);
        static hash shared_h = {"a": 1};
    }

    static string fromStaticMethod() {
        auto w := StaticSources::uh;
        StaticSources::uh.b = 2;
        return sprintf("%y", w);
    }

    string fromMethod() {
        auto w := StaticSources::ul;
        StaticSources::ul[2] = 3;
        return sprintf("%y", w);
    }
}

our hash<string, int> gth = {"a": 1};
our hash gh = {"a": 1};
our list gl = (1, 2);
our list<int> gtl = (1, 2);
our hash gshared = {"a": 1};
our Mutex gm();
our int gi = 5;
our auto gn;

# an untyped local shares its container with the program (a literal)
sub literalLocal() {
    auto l = (1, 2);
    auto w := l;
    l[2] = 3;
    printf("literal list: %y\n", w);
    auto h = {"a": 1};
    auto hw := h;
    h.b = 2;
    printf("literal hash: %y\n", hw);
}

# an untyped local shares its container with another variable
sub sharedLocal() {
    auto a = (1, 2);
    auto b = a;
    auto w := b;
    b[2] = 3;
    printf("shared list: a=%y w=%y\n", a, w);
    auto ha = {"a": 1};
    auto hb = ha;
    auto hw := hb;
    hb.b = 2;
    printf("shared hash: ha=%y hw=%y\n", ha, hw);
    # the other owner changes: the variable keeps its own container
    auto c = (1, 2);
    auto d = c;
    auto cw := c;
    d[2] = 3;
    printf("other owner: c=%y d=%y w=%y\n", c, d, cw);
}

# a container that is already unique is not copied: two weak references designate the same container
sub twoWeakReferences() {
    auto l = (1, 2);
    auto w1 := l;
    auto w2 := l;
    l[2] = 3;
    printf("two weak references: %y %y\n", w1, w2);
    list<int> t = (1, 2);
    list<int> t1 := t;
    list<int> t2 := t;
    push t, 3;
    printf("two typed weak references: %y %y\n", t1, t2);
}

# the same variable is used after the weak assignment in a loop and by a function
sub loopLocal() {
    for (int i = 0; i < 3; ++i) {
        auto l = (10, 20);
        auto w := l;
        l[2] = i;
        printf("loop %d: %y\n", i, w);
    }
}

sub weakOfWeak() {
    auto x = (1, 2);
    auto w1 := x;
    auto w2 := w1;
    x[2] = 3;
    printf("weak of weak: %y %y\n", w1, w2);
}

# values that are not a hash or a list are not touched
sub otherValues() {
    auto n;
    auto wn := n;
    printf("nothing: %y\n", wn);
    auto i = 5;
    auto wi := i;
    printf("int: %y\n", wi);
    auto s = "text";
    auto ws := s;
    printf("string: %y\n", ws);
    auto o = new Mutex();
    auto wo := o;
    printf("object: %y\n", wo.typeCode());
    auto r = (1, 2);
    auto rr = \r;
    auto wr := r;
    r[2] = 3;
    printf("referenced: %y\n", wr);
    auto wm := gm;
    auto wgi := gi;
    auto wgn := gn;
    printf("global object %y int %y nothing %y\n", wm.typeCode(), wgi, wgn);
}

# a parameter and a closure-bound local
auto sub fromParameter(auto p) {
    auto w := p;
    p[2] = 3;
    return w;
}

sub closureBound() {
    auto cl = (1, 2);
    code c = auto sub () { return cl; };
    auto w := cl;
    cl[2] = 3;
    printf("closure-bound: %y %y\n", w, c());
}

# globals
sub globals() {
    hash<string, int> w1 := gth;
    gth.b = 2;
    printf("global typed hash: %y\n", w1);
    auto w2 := gh;
    gh.b = 2;
    printf("global untyped hash: %y\n", w2);
    auto w3 := gl;
    gl[2] = 3;
    printf("global untyped list: %y\n", w3);
    list<int> w4 := gtl;
    gtl[2] = 3;
    printf("global typed list: %y\n", w4);
    hash other = gshared;
    auto w5 := gshared;
    gshared.c = 3;
    printf("global shared hash: other=%y w5=%y\n", other, w5);
    auto w6 := gh;
    auto w7 := w6;
    gh.d = 4;
    printf("weak of weak global: %y %y\n", w6, w7);
}

# class static variables, read in a function, a method and a static method
sub statics() {
    hash<string, int> w1 := StaticSources::th;
    StaticSources::th.b = 2;
    printf("static typed hash: %y\n", w1);
    list<int> w2 := StaticSources::tl;
    StaticSources::tl[2] = 3;
    printf("static typed list: %y\n", w2);
    hash other = StaticSources::shared_h;
    auto w3 := StaticSources::shared_h;
    StaticSources::shared_h.c = 3;
    printf("static shared hash: other=%y w3=%y\n", other, w3);
    printf("static method: %s\n", StaticSources::fromStaticMethod());
    StaticSources o();
    printf("method: %s\n", o.fromMethod());
}

# a local of the top level, read by a function
auto top_list = (1, 2);
auto top_hash = {"a": 1};
sub topLevelLocal() {
    auto w := top_list;
    top_list[2] = 3;
    auto hw := top_hash;
    top_hash.b = 2;
    printf("top level local from a function: %y %y\n", w, hw);
}

literalLocal();
sharedLocal();
twoWeakReferences();
loopLocal();
weakOfWeak();
otherValues();
printf("parameter: %y\n", fromParameter((1, 2)));
printf("parameter shared: %y\n", fromParameter(gl));
closureBound();
globals();
statics();
topLevelLocal();

# the top level
auto tl = (1, 2);
auto tw := tl;
tl[2] = 3;
printf("top level local: %y\n", tw);
hash<string, int> tsw := StaticSources::th;
StaticSources::th.c = 3;
printf("top level static: %y\n", tsw);
