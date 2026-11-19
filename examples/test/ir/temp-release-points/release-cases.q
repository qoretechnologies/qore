%modern
%allow-weak-references
%disable-warning invalid-operation
# the cases of temp-release-points.qtest: the point at which a value read from a local, or a temporary that holds it,
# is released, when the statement writes the local later (qore#5712); see design/temporary-lifetime.md

# Copyright (C) 2026 Qore Technologies, s.r.o.

class E {
    public {
        string n;
        static string s;
    }

    constructor(string n) {
        self.n = n;
    }

    destructor() {
        printf("  dtor %s\n", n);
    }

    string m(auto v) {
        return "m";
    }

    static string sm(auto v) {
        return "sm";
    }
}

class It inherits AbstractIterator {
    public {
        list<E> l;
        int i = -1;
    }

    constructor(list<E> l) {
        self.l = l;
    }

    bool next() {
        return ++i < l.size();
    }

    auto getValue() {
        return l[i];
    }

    bool valid() {
        return i >= 0 && i < l.size();
    }
}

hashdecl HE {
    auto a;
}

enum EnR { One = 1 }

class Other {
}

#! records a point of the statement
string sub mark(string m) {
    printf("  mark %s\n", m);
    return "";
}

string sub f1(auto v) {
    return "f1";
}

auto sub id(auto v) {
    return v;
}

string sub raise() {
    throw "X";
}

string sub g3(auto a, auto b, auto c) {
    return "g3";
}

hash<auto> sub mkh(string n) {
    return {"a": new E(n)};
}

E sub mke(string n) {
    return new E(n);
}

list<E> sub mkl(string n) {
    return (new E(n),);
}

#! an int outside the inline range, computed at runtime: a node
auto sub bigi(int i) {
    return (1 << 62) + i;
}

#! a class cast in a default argument (qore#5712)
string sub cast_default_arg(E e = cast<E>(mke("cda"))) {
    return e.n;
}

#! a class cast in a member initializer (qore#5712)
class CastMember {
    public {
        E e = cast<E>(mke("cmi"));
    }
}

# the cases of qore#5801 are functions, not closures, so that each execution mode compiles them
auto sub case_map_iterator() {
    *It it = new It((new E("mapit"),));
    string s = f1(map $1, it) + ((it = NOTHING) ?? "") + mark("mapit");
}

auto sub case_map_single() {
    *E e = new E("maps");
    string s = f1(map $1, e) + ((e = NOTHING) ?? "") + mark("maps");
}

auto sub case_map_exception() {
    *list<E> l = (new E("mapex"),);
    try {
        string s = f1(map raise(), l) + ((l = NOTHING) ?? "") + mark("mapex");
    } catch (hash<ExceptionInfo> ex) {
        mark("caught mapex");
    }
    l = NOTHING;
    mark("mapex end");
}

auto sub case_map_loop() {
    *list<E> l = (new E("mapl1"), new E("mapl2"));
    foreach E e in (l) {
        *list<E> x = (e,);
        try {
            string s = f1(map $1, x) + ((x = NOTHING) ?? "") + mark("mapl " + e.n);
        } catch (hash<ExceptionInfo> ex) {
            mark("caught mapl");
        }
    }
    l = NOTHING;
    mark("mapl end");
}

auto sub case_cast_list() {
    *list<auto> l = (new E("castl"),);
    string s = f1(cast<list<E>>(l)) + ((l = NOTHING) ?? "") + mark("castl");
}

auto sub case_cast_hashdecl() {
    *hash<auto> h = {"a": new E("casth")};
    string s = f1(cast<hash<HE>>(h)) + ((h = NOTHING) ?? "") + mark("casth");
}

auto sub case_hash_minus_key() {
    *hash<auto> h = {"a": new E("hmk"), "b": 1};
    string s = f1(h - "b") + ((h = NOTHING) ?? "") + mark("hmk");
}

auto sub case_map() {
    *list<auto> l = (new E("map"),);
    string s = f1(map $1, l) + ((l = NOTHING) ?? "") + mark("map");
}

auto sub case_map_typed() {
    *list<E> l = (new E("mapt"),);
    string s = f1(map $1, l) + ((l = NOTHING) ?? "") + mark("mapt");
}

auto sub case_select() {
    *list<auto> l = (new E("sel"),);
    string s = f1(select l, True) + ((l = NOTHING) ?? "") + mark("sel");
}

auto sub case_select_typed() {
    *list<E> l = (new E("selt"),);
    string s = f1(select l, True) + ((l = NOTHING) ?? "") + mark("selt");
}

auto sub case_foldl() {
    *list<auto> l = ((new E("fold1"),), (new E("fold2"),));
    string s = f1(foldl $1 + $2, l) + ((l = NOTHING) ?? "") + mark("fold");
}

auto sub case_foldl_single_element() {
    *list<auto> l = (new E("fold0"),);
    string s = f1(foldl $1 + $2, l) + ((l = NOTHING) ?? "") + mark("fold0");
}

# a fold over a pipeline (a fused fold) given to a call: the Phi that takes the fold's value from the loop (qore#5801)
# owns the accumulator, which the fold takes over from the pipeline's elements (qore#5883)
auto sub case_foldl_pipeline() {
    *list<E> l = (new E("fp1"), new E("fp2"));
    string s = f1(foldl $2, (map $1, l)) + ((l = NOTHING) ?? "") + mark("fp");
}

auto sub case_foldl_pipeline_iterator() {
    *It it = new It((new E("fpi1"), new E("fpi2")));
    string s = f1(foldl $2, (map $1, it)) + ((it = NOTHING) ?? "") + mark("fpi");
}

auto sub case_foldl_pipeline_concat() {
    *list<E> l = (new E("fpc1"), new E("fpc2"));
    string s = f1(foldl $1 + $2, (map ($1,), l)) + ((l = NOTHING) ?? "") + mark("fpc");
}

auto sub case_foldl_pipeline_single_element() {
    *list<E> l = (new E("fps"),);
    string s = f1(foldl $1 + $2, (map ($1,), l)) + ((l = NOTHING) ?? "") + mark("fps");
}

#! runs a case
sub t(string name, code<auto()> c) {
    printf("%s:\n", name);
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("  ! %s\n", ex.err);
    }
    printf("  end\n");
}

sub main() {
    # a list or hash literal, or the value of an operator, passed to a call is released when the call returns, and the
    # read of the local in it does not keep the value: the write destroys it
    t("list literal", auto sub () { auto o = new E("lit"); string s = f1((o,)) + (o = NOTHING) + mark("lit"); });
    t("list literal written", auto sub () { auto o = new E("litw"); string s = f1((o, o = NOTHING)) + mark("litw"); });
    t("hash literal", auto sub () { auto o = new E("hash"); string s = f1({"a": o}) + (o = NOTHING) + mark("hash"); });
    t("list plus", auto sub () {
        auto o = new E("plus");
        list<auto> l = ();
        string s = f1(l + o) + (o = NOTHING) + mark("plus");
    });
    t("two calls", auto sub () { auto o = new E("two"); string s = f1((o, 1)) + f1(2) + (o = NOTHING) + mark("two"); });
    t("method literal", auto sub () {
        E c("callee");
        auto o = new E("meth");
        string s = c.m((o,)) + (o = NOTHING) + mark("meth");
    });
    t("static literal", auto sub () { auto o = new E("st"); string s = E::sm((o,)) + (o = NOTHING) + mark("st"); });
    t("closure literal", auto sub () {
        code c = string sub (auto v) { return "c"; };
        auto o = new E("clo");
        string s = c((o,)) + (o = NOTHING) + mark("clo");
    });
    # a member read from a local does not keep the object
    t("member", auto sub () { auto o = new E("mem"); string s = o.n + (o = NOTHING) + mark("mem"); });
    t("member twice", auto sub () { auto o = new E("mem2"); string s = o.n + o.n + (o = NOTHING) + mark("mem2"); });
    # without a later write nothing changes: the local keeps the value to the end
    t("no write", auto sub () { auto o = new E("nw"); string s = f1((o,)) + mark("nw"); });
    # the result of an operator that is the receiver of a chain lives until the end of the statement
    t("remove receiver", auto sub () { auto o = new E("rr"); string s = f1(1) + (remove o).n + mark("rr"); });
    t("pop receiver", auto sub () { list<auto> l = (new E("pr"),); string s = f1(1) + (pop l).n + mark("pr"); });
    t("shift receiver", auto sub () { list<auto> l = (new E("sr"),); string s = f1(1) + (shift l).n + mark("sr"); });
    t("extract receiver", auto sub () {
        list<auto> l = (new E("xr"),);
        string s = f1(1) + (extract l, 0, 1).size() + mark("xr");
    });
    t("member remove receiver", auto sub () {
        hash<auto> h = {"e": new E("mr")};
        string s = f1(1) + (remove h.e).n + mark("mr");
    });
    # an argument created by a call
    t("new argument", auto sub () { string s = f1(new E("na")) + mark("na"); });
    t("call argument", auto sub () { auto o = new E("ca"); string s = f1(id(o)) + (o = NOTHING) + mark("ca"); });
    t("remove argument", auto sub () {
        auto o = new E("ra");
        list<auto> l = (o,);
        o = NOTHING;
        string s = f1(remove l[0]) + mark("ra");
    });
    t("pop argument", auto sub () { list<auto> l = (new E("pa"),); string s = f1(pop l) + mark("pa"); });
    t("new in literal", auto sub () { string s = f1((new E("nl"), 1)) + mark("nl"); });
    # the operand of an operator is a temporary of the statement: it lives until the end of the statement
    t("operand of plus", auto sub () {
        auto o = new E("op");
        hash<auto> h = {};
        string s = f1(h + {"a": o}) + (o = NOTHING) + mark("op");
    });
    t("operand of equals", auto sub () { string s = f1(new E("eq") == NOTHING) + mark("eq"); });
    t("operand of and", auto sub () { string s = f1(new E("and") && True) + mark("and"); });
    # two temporary operands are released in the reverse order of their creation
    t("two operands", auto sub () { string s = f1(new E("l") == new E("r")) + mark("two"); });
    # the operand that an operator created for an earlier element is released when it is created again
    t("operand in map", auto sub () { list<auto> r = map (new E("m" + $1) == mark("m" + $1)), (1, 2); });
    # an exception after the operator: the interrupted argument list is released first
    t("operand exception", auto sub () { string s = g3(new E("arg"), new E("op") == NOTHING, raise()); });
    # keys, and a comparison with a float, whose AST node takes a typed path
    t("operand of keys", auto sub () { string s = f1(keys mkh("k")) + mark("k"); });
    t("operand of float compare", auto sub () { string s = f1(mke("lt") < 1.0) + mark("lt"); });
    # keys as the source of a functional operator, of a hash and of an object
    t("keys source hash", auto sub () { string s = f1(map $1, keys mkh("ksh")) + mark("ksh"); });
    t("keys source object", auto sub () { string s = f1(map $1, keys mke("kso")) + mark("kso"); });
    # an index and a key
    t("operand of index", auto sub () { list<auto> l = (1, 2); string s = f1(l[mke("ix")]) + mark("ix"); });
    t("operand of key", auto sub () { hash<auto> h = {"a": 1}; string s = f1(h{mke("key").n}) + mark("key"); });
    # the key itself, not the receiver of a member access (qore#5712)
    t("operand of key value", auto sub () { hash<auto> h = {"a": 1}; string s = f1(h{id(mke("keyv"))}) + mark("keyv"); });
    # [] as the source of a functional operator: its index and its container
    t("index source", auto sub () { list<auto> l = (1, 2); string s = f1(map $1, l[mke("ixs")]) + mark("ixs"); });
    t("index source container", auto sub () { string s = f1(map $1, mkl("ixc")[(0,)]) + mark("ixc"); });
    # the operand of a class cast and of an enum cast
    t("operand of class cast", auto sub () { string s = f1(cast<E>(mke("cc"))) + mark("cc"); });
    t("class cast exception", auto sub () { string s = g3(new E("arg"), cast<Other>(mke("ccx")), raise()); });
    t("enum cast exception", auto sub () { string s = g3(new E("arg"), cast<EnR>(id(mke("enx"))), raise()); });
    # a class cast whose value is used after the scope of its expression ends: an assignment to an element, a default
    # argument and a member initializer evaluate in a scope of their own
    t("class cast element", auto sub () { hash<auto> h = {}; h.x = cast<E>(mke("cel")); string s = h.x.n + mark("cel"); });
    t("class cast default argument", auto sub () { string s = cast_default_arg() + mark("cda"); });
    t("class cast member", auto sub () { CastMember c(); string s = c.e.n + mark("cmi"); });
    # unary plus of an int node, assigned to an element and pushed to a list
    t("unary plus element", auto sub () { hash<auto> h = {}; h.x = +bigi(12345); printf("  value %d\n", h.x); });
    t("unary plus push", auto sub () { list<auto> l = (); push l, +bigi(7); printf("  value %d\n", l[0]); });
    t("unary plus variable", auto sub () {
        number n = 1.5n;
        date d = 3h;
        hash<auto> h = {};
        h.x = +n;
        h.y = +d;
        printf("  value %y %y\n", h.x, h.y);
    });
    t("float compare exception", auto sub () { string s = g3(new E("arg"), mke("gt") > 1.0, raise()); });
    # the condition of ?:
    t("condition", auto sub () { string s = f1(1) + ((new E("q")) ? mark("q") : ""); });
    # a statement in a try block, or with a branch, continues in other blocks after a call
    t("try block", auto sub () {
        auto o = new E("try");
        try {
            string s = f1((o,)) + (o = NOTHING) + mark("try");
        } catch (hash<ExceptionInfo> ex) {
            printf("  ! %s\n", ex.err);
        }
    });
    t("branch", auto sub () {
        auto o = new E("br");
        bool c = True;
        string s = f1((o,)) + (c ? (o = NOTHING) : "") + mark("br");
    });
    # the value of a cast, of hash - key and of map, select and foldl, passed to a call, is released when the call
    # returns, and the operator's source read from a local does not keep its value: the write destroys it (qore#5801)
    t("cast list", \case_cast_list());
    t("cast hashdecl", \case_cast_hashdecl());
    t("hash minus key", \case_hash_minus_key());
    t("map", \case_map());
    t("map typed", \case_map_typed());
    t("select", \case_select());
    t("select typed", \case_select_typed());
    t("foldl", \case_foldl());
    t("foldl single element", \case_foldl_single_element());
    t("foldl pipeline", \case_foldl_pipeline());
    t("foldl pipeline iterator", \case_foldl_pipeline_iterator());
    t("foldl pipeline concat", \case_foldl_pipeline_concat());
    t("foldl pipeline single element", \case_foldl_pipeline_single_element());
    t("map iterator", \case_map_iterator());
    t("map single value", \case_map_single());
    t("map exception", \case_map_exception());
    t("map loop", \case_map_loop());
    # an exception after the release
    t("exception", auto sub () {
        auto o = new E("ex");
        string s = f1((o,)) + (o = NOTHING) + mark("ex") + raise();
    });
    # in a loop
    t("loop", auto sub () {
        for (int i = 0; i < 2; ++i) {
            auto o = new E("loop" + i);
            string s = f1((o,)) + (o = NOTHING) + mark("loop" + i);
        }
    });
}

# Operator source, element and result lifetimes (qore#5959).
sub case_field() {
    list<hash<auto>> l = ({"a": new E("a")}, {"a": new E("b")});
    string s = f1(map $1.a, l) + f1(l = ()) + mark("field");
}

sub case_nested() {
    list<list<auto>> l = ((new E("a"),), (new E("b"),));
    string s = f1(map (foldl $2, $1), l) + f1(l = ()) + mark("nested");
}

sub case_write() {
    list<E> l = (new E("a"), new E("b"));
    string s = f1(map ((l = ()) ? NOTHING : $1), l) + mark("write");
}

sub case_select_single() {
    *E l = new E("a");
    string s = f1(select l, True) + ((l = NOTHING) ?? "") + mark("single");
}

sub case_select_hash() {
    *hash<auto> l = {"x": new E("a"), "y": new E("b")};
    string s = f1(select l, True) + ((l = NOTHING) ?? "") + mark("hash");
}

sub case_coalesce() {
    *list<E> l = (new E("a"), new E("b"));
    string s = f1((map $1, l) ?? l) + ((l = NOTHING) ?? "") + mark("coalesce");
}

sub case_hashmap() {
    *list<E> l = (new E("a"), new E("b"));
    string s = f1(map {$1.n: $1}, l) + ((l = NOTHING) ?? "") + mark("hashmap");
}

sub case_foldmap() {
    *list<E> l = (new E("a"), new E("b"));
    string s = f1(foldl $2, (map $1, l)) + ((l = NOTHING) ?? "") + mark("foldmap");
}

sub case_ternmap(bool c) {
    *list<E> l = (new E("a"), new E("b"));
    string s = f1(c ? (map $1, l) : (map $1, l)) + ((l = NOTHING) ?? "") + mark("ternmap");
}

sub case_ternfold(bool c) {
    *list<E> l = (new E("a"), new E("b"));
    string s = f1(c ? (foldl $2, l) : (foldl $1, l)) + ((l = NOTHING) ?? "") + mark("ternfold");
}

sub case_automap() {
    auto l = (new E("a"), new E("b"));
    string s = f1(map $1, l) + ((l = NOTHING) ?? "") + mark("automap");
}

sub case_autosel() {
    auto l = (new E("a"), new E("b"));
    string s = f1(select l, True) + ((l = NOTHING) ?? "") + mark("autosel");
}

sub case_autofold() {
    auto l = (new E("a"), new E("b"));
    string s = f1(foldl $2, l) + ((l = NOTHING) ?? "") + mark("autofold");
}

sub map_parameter(auto l) {
    string s = f1(map $1, l) + ((l = NOTHING) ?? "") + mark("param");
}

sub map_reference_or_value(bool choose) {
    auto target = (new E("ref"),);
    auto l = choose ? \target : (new E("plain"),);
    string s = f1(map $1, l) + ((l = NOTHING) ?? "") + mark("local");
}

class ThrowingElement inherits E {
    constructor(string name) : E(name) {
    }

    destructor() {
        throw "ELEMENT-RELEASE";
    }
}

sub map_weak_element(bool catch_here) {
    *E owner = new ThrowingElement("weak");
    list<E> source = ();
    source[0] := owner;
    source[1] = new E("next");
    if (catch_here) {
        try {
            string s = f1(map ((owner = NOTHING) ?? 1), source) + mark("unreachable");
        } catch (hash<ExceptionInfo> ex) {
            mark(ex.err);
        }
    } else {
        string s = f1(map ((owner = NOTHING) ?? 1), source) + mark("unreachable");
    }
}

main();

case_field();
case_nested();
case_write();
case_select_single();
case_select_hash();
case_coalesce();
case_hashmap();
case_foldmap();
case_ternmap(True);
case_ternmap(False);
case_ternfold(True);
case_ternfold(False);
case_automap();
case_autosel();
case_autofold();
string typed_source = f1(map $1, mkl("top")) + mark("top");
map_parameter((new E("value"),));
auto ref_list = (new E("ref-param"),);
map_parameter(\ref_list);
map_reference_or_value(True);
map_reference_or_value(False);
map_weak_element(True);
try {
    map_weak_element(False);
} catch (hash<ExceptionInfo> ex) {
    mark(ex.err);
}
