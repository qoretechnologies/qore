#!/usr/bin/env qore
# the values of typed hashes and of typed TreeMaps holding references, objects and closures (qore#6558)

%modern
%enable-all-warnings

class T {
    public {
        string n;
    }

    constructor(string n) {
        self.n = n;
    }
}

our *T g_o = new T("x");
our *T g_p;

# describes a value without the address that a reference or closure is printed with
string sub desc(auto v) {
    switch (v.type()) {
        case "reference": return "reference";
        case "object": return "object<" + v.className() + "> " + v.n;
        case "closure": return "closure";
        case "list": return "[" + (foldl $1 + ", " + $2, (map desc($1), v)) + "]";
        case "hash": return "{" + (foldl $1 + ", " + $2, (map $1.key + ": " + desc($1.value), v.pairIterator())) + "}";
    }
    return sprintf("%y", v);
}

# reads and writes the variable that a reference refers to
string sub through(reference<*T> r, string n) {
    string old = r ? r.n : "NOTHING";
    r = new T(n);
    return old;
}

sub show(string label, auto v) {
    printf("%s: %s %s\n", label, v.fullType(), desc(v));
}

# the literal hash is typed hash<string, reference<*object<T>>>
sub values_param(hash<auto> h) {
    show("values param", h.values());
    show("hash_values param", hash_values(h));
}

sub values_declared(hash<string, reference<*T>> h) {
    show("values declared", h.values());
}

sub values_nested(hash<auto> h) {
    show("values nested", h.a.values());
}

sub values_list_elem(list<auto> l) {
    show("values list element", l[0].values());
}

sub values_closure(hash<auto> h) {
    code c = sub () { show("values closure", h.values()); };
    c();
}

sub values_background(hash<auto> h) {
    Counter cnt(1);
    background sub () {
        on_exit cnt.dec();
        show("values background", h.values());
    }();
    cnt.waitForZero();
}

sub values_write_through(hash<auto> h) {
    list<auto> l = h.values();
    printf("write through: old %s\n", through(l[0], "w"));
    printf("write through: g_p %s\n", g_p.n);
}

sub values_untyped(hash<auto> h) {
    # the sum of the hashes has no declared value type
    hash<auto> u = h + {};
    show("values untyped", u.values());
}

sub other_ops(hash<auto> h) {
    show("plus", h + {"b": 1});
    show("slice", h{"a",});
    show("firstValue", h.firstValue());
    show("lastValue", h.lastValue());
    show("map pairs", map $1.value, h.pairIterator());
    HashIterator i(h);
    while (i.next()) {
        show("HashIterator", i.getValue());
    }
    HashPairIterator pi(h);
    while (pi.next()) {
        show("HashPairIterator", pi.getValue().value);
    }
    HashReverseIterator ri(h);
    while (ri.next()) {
        show("HashReverseIterator", ri.getValue());
    }
}

sub list_ops(list<auto> l) {
    show("list", l);
    show("list plus", l + l);
    show("list range", l[0..0]);
    show("list reverse", reverse(l));
    show("list map", map $1, l);
    show("list select", select l, True);
    ListIterator i(l);
    while (i.next()) {
        show("ListIterator", i.getValue());
    }
    splice l, 0, 0, (\g_o,);
    push l, \g_o;
    show("list splice push", l);
    show("list extract", extract l, 0, 1);
}

values_param({"a": \g_o, "b": \g_o});
values_declared({"a": \g_o});
values_nested({"a": {"b": \g_o}});
values_list_elem(({"b": \g_o},));
values_closure({"a": \g_o});
values_background({"a": \g_o});
values_write_through({"a": \g_p});
values_untyped({"a": \g_o});
other_ops({"a": \g_o});
list_ops((\g_o,));

{
    hash<auto> h = {"a": \g_o};
    show("values variable", h.values());
}

# objects and closures in typed hashes
{
    code c = sub () {};
    hash<string, T> ho = {"a": new T("y")};
    show("values objects", ho.values());
    hash<string, code> hc = {"a": c};
    show("values closures", hc.values());
    hash<string, list<int>> hl = {"a": (1, 2)};
    show("values lists", hl.values());
    hash<string, softint> hs = {"a": "1"};
    show("values softint", hs.values());
}

# typed TreeMaps
{
    TreeMap<reference<*T>> tm();
    tm.put("a", \g_o);
    show("TreeMap reference getAll", tm.getAll());
    TreeMap<T> to();
    to.put("a", new T("z"));
    show("TreeMap object getAll", to.getAll());
    TreeMap<code> tc();
    tc.put("a", sub () {});
    show("TreeMap closure getAll", tc.getAll());
    TreeMap tu();
    tu.put("a", {"x": 1});
    show("TreeMap untyped getAll", tu.getAll());
}
