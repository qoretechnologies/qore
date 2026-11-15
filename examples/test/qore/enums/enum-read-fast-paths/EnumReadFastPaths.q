#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of enum-read-fast-paths.qtest: prints the type and the value of enum-typed values read through the fast
# paths of the compiled modes (qore#6236)

%modern
%enable-all-warnings

enum EI : int { One = 1, Seven = 7, Eight = 8 }
enum EF : float { H = 0.5, Q = 0.25 }
enum ES : string { X = "x", Y = "y" }
enum EN : number { N1 = 1.5n, N2 = 2.5n }

sub show(string label, auto v) {
    if (v.typeCode() == NT_LIST) {
        printf("%s: %y %y\n", label, (map $1.fullType(), v), v);
    } else if (v.typeCode() == NT_HASH) {
        printf("%s: %y %y\n", label, (map {$1.key: $1.value.fullType()}, v.pairIterator()), v);
    } else {
        printf("%s: %s %y\n", label, v.fullType(), v);
    }
}

sub run(string label, code c) {
    try {
        show(label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

EI sub retEI(EI e) {
    return e;
}

EF sub retEF(EF e) {
    return e;
}

EF sub localEF(EF e) {
    EF c = e;
    EF d = c;
    return d;
}

auto sub loopEF(list<EF> l) {
    EF last = EF::Q;
    foreach EF x in (l) {
        last = x;
    }
    return last;
}

auto sub loopEI(list<EI> l) {
    EI last = EI::One;
    foreach EI x in (l) {
        last = x;
    }
    return last;
}

auto sub indexEF(list<EF> l) {
    EF r = EF::Q;
    for (int i = 0; i < l.size(); ++i) {
        r = l[i];
    }
    return r;
}

auto sub indexEI(list<EI> l) {
    EI r = EI::One;
    for (int i = 0; i < l.size(); ++i) {
        r = l[i];
    }
    return r;
}

EF sub ternaryEF(bool b, EF x, EF y) {
    return b ? x : y;
}

auto sub whileEF(list<EF> l) {
    EF r = EF::Q;
    int i = 0;
    while (i < l.size()) {
        r = l[i];
        ++i;
    }
    return r;
}

list<EF> sub pushEF(list<EF> l) {
    list<EF> r = ();
    foreach EF x in (l) {
        push r, x;
    }
    return r;
}

list<EI> sub pushEI(list<EI> l) {
    list<EI> r = ();
    foreach EI x in (l) {
        push r, x;
    }
    return r;
}

# the values of a type parameter are enum values when the type argument is an enum, also when its bound is int or
# float
class G<T: int> {
    public {
        T v;
        list<T> l;
        hash<string, T> h;
    }

    constructor(T x, T y) {
        v = x;
        l = (x, y);
        h = {"a": x};
    }

    auto mapList() { list<T> l2 = l; return map $1, l2; }
    auto mapSelect() { list<T> l2 = l; return map $1, l2, $1 > 0; }
    auto selectList() { list<T> l2 = l; return select l2, $1 > 0; }
    auto foldlMax() { return foldl max($1, $2), l; }
    auto foldrMin() { list<T> l2 = l; return foldr min($1, $2), l2; }
    auto hashKey() { hash<string, T> h2 = h; return h2.a; }
    auto local() { T y = v; T z = y; return z; }
    auto pushList() { list<T> l2 = (); foreach T x in (l) { push l2, x; } return l2; }
    T ret() { return v; }
    auto callRet() { return ret(); }
}

class GE<T: EI> {
    public {
        list<T> l;
        hash<string, T> h;
    }

    constructor(T x, T y) {
        l = (x, y);
        h = {"a": x};
    }

    auto mapList() { list<T> l2 = l; return map $1, l2; }
    auto selectList() { list<T> l2 = l; return select l2, $1 > 0; }
    auto foldlMax() { return foldl max($1, $2), l; }
    auto hashKey() { hash<string, T> h2 = h; return h2.a; }
}

class GF<T: EF> {
    public {
        T v;
        list<T> l;
        hash<string, T> h;
    }

    constructor(T x, T y) {
        v = x;
        l = (x, y);
        h = {"a": x};
    }

    auto mapList() { list<T> l2 = l; return map $1, l2; }
    auto selectList() { list<T> l2 = l; return select l2, $1 > 0; }
    auto foldrMin() { list<T> l2 = l; return foldr min($1, $2), l2; }
    auto hashKey() { hash<string, T> h2 = h; return h2.a; }
    auto local() { T y = v; T z = y; return z; }
    T ret() { return v; }
    auto callRet() { return ret(); }
}

# generic functions, whose type arguments are enums or plain types
auto sub gmap<T: int>(list<T> l) { return map $1, l; }
auto sub gselect<T: int>(list<T> l) { return select l, $1 > 0; }
auto sub gselectAnd<T: int>(list<T> l) { return select l, $1 > 0 && $1 < 8; }
auto sub gfoldlMax<T: int>(list<T> l) { return foldl max($1, $2), l; }
auto sub gfoldrMin<T: float>(list<T> l) { return foldr min($1, $2), l; }
auto sub gfoldlTernary<T: int>(list<T> l) { return foldl ($1 > $2 ? $1 : $2), l; }
auto sub gfoldrOfSelect<T: int>(list<T> l) { return foldr ($1 < $2 ? $1 : $2), (select l, $1 > 0); }
auto sub gmapSelect<T: int>(list<T> l) { return map $1, l, $1 > 1; }
auto sub gfirstOfMap<T: int>(list<T> l) { return (map $1, l)[0]; }
auto sub ghashKey<T: int>(hash<string, T> h) { return h.a; }
auto sub ghashOfMap<T: int>(list<T> l) { hash<string, T> h = map {$1.toString(): $1}, l; return h."7"; }
auto sub gmapHashKey<T: int>(list<hash<string, T>> l) { return map $1.a, l; }
T sub greturn<T: float>(T v) { return v; }

class GM {
    auto mapList<U: int>(list<U> l) { return map $1, l; }
    static auto selectList<U: int>(list<U> l) { return select l, $1 > 0; }
}

sub genericFunctions(list<EI> l, list<EF> lf, EF ef) {
    list<int> li = (7, 1, 8);
    GM gm();
    run("generic function map", auto sub () { return gmap<EI>(l); });
    run("generic function select", auto sub () { return gselect<EI>(l); });
    run("generic function select with &&", auto sub () { return gselectAnd<EI>(l); });
    run("generic function foldl max", auto sub () { return gfoldlMax<EI>(l); });
    run("generic function foldr min float", auto sub () { return gfoldrMin<EF>(lf); });
    run("generic function foldl ternary", auto sub () { return gfoldlTernary<EI>(l); });
    run("generic function foldr of select", auto sub () { return gfoldrOfSelect<EI>(l); });
    run("generic function map with select", auto sub () { return gmapSelect<EI>(l); });
    run("generic function element of map", auto sub () { return gfirstOfMap<EI>(l); });
    run("generic function hash key", auto sub () { return ghashKey<EI>({"a": EI::Seven}); });
    run("generic function hash of map", auto sub () { return ghashOfMap<EI>(l); });
    run("generic function map hash key", auto sub () { return gmapHashKey<EI>(({"a": EI::Seven}, {"a": EI::One})); });
    run("generic function return float", auto sub () { return greturn<EF>(ef); });
    run("generic method map", auto sub () { return gm.mapList<EI>(l); });
    run("generic static method select", auto sub () { return GM::selectList<EI>(l); });
    # plain type arguments keep plain values
    run("generic function map int", auto sub () { return gmap<int>(li); });
    run("generic function select int", auto sub () { return gselect<int>(li); });
    run("generic function foldl max int", auto sub () { return gfoldlMax<int>(li); });
    run("generic function hash key int", auto sub () { return ghashKey<int>({"a": 3}); });
}

sub generics(EI e, EF ef) {
    G<EI> gi(e, EI::One);
    GE<EI> ge(e, EI::Eight);
    GF<EF> gf(ef, EF::H);
    G<int> gint(3, 4);
    run("generic int bound map", auto sub () { return gi.mapList(); });
    run("generic int bound map with select", auto sub () { return gi.mapSelect(); });
    run("generic int bound select", auto sub () { return gi.selectList(); });
    run("generic int bound foldl max", auto sub () { return gi.foldlMax(); });
    run("generic int bound foldr min", auto sub () { return gi.foldrMin(); });
    run("generic int bound hash key", auto sub () { return gi.hashKey(); });
    run("generic int bound local", auto sub () { return gi.local(); });
    run("generic int bound push", auto sub () { return gi.pushList(); });
    run("generic int bound return", auto sub () { return gi.callRet(); });
    run("generic enum bound map", auto sub () { return ge.mapList(); });
    run("generic enum bound select", auto sub () { return ge.selectList(); });
    run("generic enum bound foldl max", auto sub () { return ge.foldlMax(); });
    run("generic enum bound hash key", auto sub () { return ge.hashKey(); });
    run("generic float enum bound map", auto sub () { return gf.mapList(); });
    run("generic float enum bound select", auto sub () { return gf.selectList(); });
    run("generic float enum bound foldr min", auto sub () { return gf.foldrMin(); });
    run("generic float enum bound hash key", auto sub () { return gf.hashKey(); });
    run("generic float enum bound local", auto sub () { return gf.local(); });
    run("generic float enum bound return", auto sub () { return gf.callRet(); });
    # plain values of a generic class keep their plain types
    run("generic int map", auto sub () { return gint.mapList(); });
    run("generic int select", auto sub () { return gint.selectList(); });
    run("generic int foldl max", auto sub () { return gint.foldlMax(); });
    run("generic int hash key", auto sub () { return gint.hashKey(); });
    run("generic int local", auto sub () { return gint.local(); });
}

sub cases(list<EI> l, list<EF> lf, list<ES> ls, list<EN> ln, EI e, EF ef, ES es, EN en) {
    hash<string, EI> hh = {"a": EI::Seven};
    hash<string, EI> h2();
    h2.a = EI::Eight;
    list<hash<string, EI>> lh = ({"a": EI::Seven}, {"a": EI::One});

    # hash values read with a constant key
    run("hash constant key", auto sub () { return hh.a; });
    run("hash assigned key", auto sub () { return h2.a; });
    run("hash constant key to int", auto sub () { int i = hh.a; return i; });
    run("hash constant key to enum", auto sub () { EI x = hh.a; return x; });
    run("hash float", auto sub () { hash<string, EF> h = {"a": EF::Q}; return h.a; });
    run("hash string", auto sub () { hash<string, ES> h = {"a": ES::Y}; return h.a; });
    run("hash number", auto sub () { hash<string, EN> h = {"a": EN::N2}; return h.a; });
    run("hash values", auto sub () { return hh.values(); });

    # map
    run("map", auto sub () { return map $1, l; });
    run("map float", auto sub () { return map $1, lf; });
    run("map string", auto sub () { return map $1, ls; });
    run("map number", auto sub () { return map $1, ln; });
    run("map with select", auto sub () { return map $1, l, $1 > EI::One; });
    run("map with select float", auto sub () { return map $1, lf, $1 > 0.3; });
    run("map with select string", auto sub () { return map $1, ls, $1 == "x"; });
    run("map hash key", auto sub () { return map $1.a, lh; });
    run("map hash key float", auto sub () { list<hash<string, EF>> l2 = ({"a": EF::H},); return map $1.a, l2; });
    run("map hash key string", auto sub () { list<hash<string, ES>> l2 = ({"a": ES::X},); return map $1.a, l2; });
    run("map of map", auto sub () { return map $1, (map $1, l); });
    run("map of select", auto sub () { return map $1, (select l, $1 > 1); });

    # select
    run("select", auto sub () { return select l, $1 > 0; });
    run("select float", auto sub () { return select lf, $1 > 0.3; });
    run("select string", auto sub () { return select ls, $1 == "y"; });
    run("select number", auto sub () { return select ln, $1 > 2n; });
    run("select hash key", auto sub () { return select lh, $1.a > 1; });
    run("select of map", auto sub () { return select (map $1, l), $1 > 1; });
    run("select != 0", auto sub () { return select l, $1 != 0; });
    run("select float != 0", auto sub () { return select lf, $1 != 0; });
    run("select float > 0", auto sub () { return select lf, $1 > 0; });
    run("map with select toInt()", auto sub () { return map $1, l, $1.toInt() > 1; });

    # folds that give one of the elements
    run("foldl min", auto sub () { return foldl min($1, $2), l; });
    run("foldl max", auto sub () { return foldl max($1, $2), l; });
    run("foldr max", auto sub () { return foldr max($1, $2), l; });
    run("foldl min float", auto sub () { return foldl min($1, $2), lf; });
    run("foldr min float", auto sub () { return foldr min($1, $2), lf; });
    run("min", auto sub () { return min(l); });
    run("max", auto sub () { return max(l); });
    run("min float", auto sub () { return min(lf); });
    run("foldl ternary", auto sub () { return foldl ($1 > $2 ? $1 : $2), l; });

    # locals, returns and captures
    run("return", auto sub () { return retEI(e); });
    run("return float", auto sub () { return retEF(ef); });
    run("local float", auto sub () { return localEF(ef); });
    run("loop", auto sub () { return loopEI(l); });
    run("loop float", auto sub () { return loopEF(lf); });
    run("index", auto sub () { return indexEI(l); });
    run("index float", auto sub () { return indexEF(lf); });
    run("while float", auto sub () { return whileEF(lf); });
    run("ternary float function", auto sub () { return ternaryEF(True, ef, EF::H); });
    run("push", auto sub () { return pushEI(l); });
    run("push float", auto sub () { return pushEF(lf); });
    run("capture", auto sub () { code c = auto sub () { return e; }; return c(); });
    run("capture float", auto sub () { code c = auto sub () { return ef; }; return c(); });
    run("capture local float", auto sub () { EF x = ef; code c = auto sub () { return x; }; return c(); });
    run("copy", auto sub () { EI c = e; return c; });
    run("ternary", auto sub () { return e ? e : EI::One; });
    run("ternary float", auto sub () { return ef ? ef : EF::H; });
    run("list element", auto sub () { return l[1]; });
    run("list element float", auto sub () { return lf[0]; });
    run("list store", auto sub () { list<EI> l2 = l; l2[0] = EI::Eight; return l2; });
    run("list store float", auto sub () { list<EF> l2 = lf; l2[1] = EF::H; return l2; });
    run("list literal", auto sub () { list<EI> l2 = (e, EI::One); return l2; });
    run("list literal float", auto sub () { list<EF> l2 = (ef, EF::H); return l2; });
    run("hash literal float", auto sub () { hash<string, EF> h = {"a": ef}; return h; });
    run("string", auto sub () { return es; });
    run("number", auto sub () { return en; });

    # operators on enum values give plain values
    run("foldl +", auto sub () { return foldl $1 + $2, l; });
    run("foldl *", auto sub () { return foldl $1 * $2, l; });
    run("foldl -", auto sub () { return foldl $1 - $2, l; });
    run("foldl + string", auto sub () { return foldl $1 + $2, ls; });
    run("map + 1", auto sub () { return map $1 + 1, l; });
    run("map * 2", auto sub () { return map $1 * 2, l; });
    run("map * 2 float", auto sub () { return map $1 * 2, lf; });
    run("map hash key + 1", auto sub () { return map $1.a + 1, lh; });
    run("map hash key * 2", auto sub () { return map $1.a * 2, lh; });
    run("map sprintf", auto sub () { return map sprintf("%d", $1), l; });
    run("join", auto sub () { return join(",", ls); });
    run("+ 1", auto sub () { return e + 1; });
    run("* 3", auto sub () { return e * 3; });
    run("float * 2", auto sub () { return ef * 2; });
    run("== 7", auto sub () { return e == 7; });
    run("< 8", auto sub () { return e < 8; });
    run("to int", auto sub () { int i = e; return i; });
    run("to float", auto sub () { float f = ef; return f; });
    run("list element + 1", auto sub () { return l[1] + 1; });
    run("sum in a loop", auto sub () { int s = 0; foreach EI x in (l) { s += x; } return s; });
}

cases((EI::Seven, EI::One, EI::Eight), (EF::H, EF::Q), (ES::X, ES::Y), (EN::N1, EN::N2), EI::Seven, EF::Q, ES::Y,
    EN::N2);
generics(EI::Seven, EF::Q);
genericFunctions((EI::Seven, EI::One, EI::Eight), (EF::H, EF::Q), EF::Q);
