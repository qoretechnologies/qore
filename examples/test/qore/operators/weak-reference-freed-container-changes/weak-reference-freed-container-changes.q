%modern
%allow-weak-references
# a list or hash freed under a weak reference cannot be changed: every change raises LIST-ERROR or HASH-ERROR, as the
# documentation of the weak assignment operator says (qore#7199); a value computed from it is a new container that can
# be changed; every mode must print the output of the AST interpreter

class C {
    public {
        int m = 7;
    }
}

hashdecl HD {
    int a;
    int b;
    int m;
    int c;
}

#! holds a list and a hash as members
class Holder {
    public {
        list<auto> l;
        hash<auto> h;
    }
}

list<auto> gl;
hash<auto> gh;

hash<auto> sub mk() {
    hash<auto> r = {};
    r.a = 1;
    r.b = 2;
    return r;
}

sub t(string name, code c) {
    try {
        printf("%s: %y\n", name, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", name, ex.err);
    }
}

sub freed_list() {
    list<auto> w;
    w := list(1, 2, 3);
    t("list splice", auto sub () { auto s = w; splice s, 0, 1; return s; });
    t("list splice with value", auto sub () { auto s = w; splice s, 0, 0, (7, 8); return s; });
    t("list extract", auto sub () { auto s = w; return extract s, 0, 1; });
    t("list extract with value", auto sub () { auto s = w; extract s, 0, 0, (7,); return s; });
    t("list += list", auto sub () { auto s = w; s += (7, 8); return s; });
    t("list += element", auto sub () { auto s = w; s += 7; return s; });
    t("list push", auto sub () { auto s = w; push s, 7; return s; });
    t("list element", auto sub () { auto s = w; s[0] = 7; return s; });
    # values computed from it are new lists
    t("list + element then change", auto sub () { list<auto> m = w + 7; m += 8; return m; });
    t("splice() then change", auto sub () { list<auto> m = splice(w, 0, 0); m += 1; return m; });
    t("splice() with a value then change", auto sub () { list<auto> m = splice(w, 0, 0, (7,)); m += 8; return m; });
}

sub freed_hash() {
    hash<auto> h;
    h := mk();
    t("hash += hash", auto sub () { auto s = h; s += {"c": 3}; return s; });
    t("hash += object", auto sub () { auto s = h; s += new C(); return s; });
    t("hash key", auto sub () { auto s = h; s.c = 3; return s; });
    # values computed from it are new hashes
    t("hash - key then change", auto sub () { hash<auto> m = h - "a"; m.x = 1; return m; });
    t("hash - list then change", auto sub () { hash<auto> m = h - ("a", "b"); m.x = 1; return m; });
    t("hash + hash then change", auto sub () { hash<auto> m = h + {}; m.x = 1; return m; });
    t("hash + object then change", auto sub () { hash<auto> m = h + new C(); m.x = 1; return m; });
}

#! the changes outside of a closure: of a local, a member, a global and a list element, and of typed and hashdecl
#! containers
sub freed_direct() {
    list<auto> w;
    w := list(1, 2, 3);
    hash<auto> h;
    h := mk();
    list<auto> s = w;
    try { splice s, 0, 1; printf("direct splice: %y\n", s); } catch (hash<ExceptionInfo> ex) {
        printf("direct splice: %s\n", ex.err); }
    s = w;
    try { extract s, 0, 0, (7,); printf("direct extract: %y\n", s); } catch (hash<ExceptionInfo> ex) {
        printf("direct extract: %s\n", ex.err); }
    s = w;
    try { unshift s, 7; printf("direct unshift: %y\n", s); } catch (hash<ExceptionInfo> ex) {
        printf("direct unshift: %s\n", ex.err); }
    Holder o();
    o.l = w;
    try { splice o.l, 0, 1; printf("member splice: %y\n", o.l); } catch (hash<ExceptionInfo> ex) {
        printf("member splice: %s\n", ex.err); }
    o.h = h;
    try { o.h += {"c": 3}; printf("member += hash: %y\n", o.h); } catch (hash<ExceptionInfo> ex) {
        printf("member += hash: %s\n", ex.err); }
    gl = w;
    try { gl += (7,); printf("global += list: %y\n", gl); } catch (hash<ExceptionInfo> ex) {
        printf("global += list: %s\n", ex.err); }
    gh = h;
    try { gh += new C(); printf("global += object: %y\n", gh); } catch (hash<ExceptionInfo> ex) {
        printf("global += object: %s\n", ex.err); }
    list<auto> ll = (w,);
    try { splice ll[0], 0, 1; printf("element splice: %y\n", ll[0]); } catch (hash<ExceptionInfo> ex) {
        printf("element splice: %s\n", ex.err); }

    list<int> wi;
    # a container of the lvalue's type: a weak assignment that converts the container raises (#7237)
    wi := cast<list<int>>(list(1, 2));
    list<int> si = wi;
    try { si += (3, 4); printf("list<int> += list: %y\n", si); } catch (hash<ExceptionInfo> ex) {
        printf("list<int> += list: %s\n", ex.err); }
    hash<string, int> wt;
    wt := cast<hash<string, int>>(mk());
    hash<string, int> st = wt;
    try { st += {"c": 3}; printf("hash<string, int> += hash: %y\n", st); } catch (hash<ExceptionInfo> ex) {
        printf("hash<string, int> += hash: %s\n", ex.err); }
    hash<HD> wd;
    wd := cast<hash<HD>>(mk());
    hash<HD> sd = wd;
    try { sd += new C(); printf("hash<HD> += object: %y\n", sd); } catch (hash<ExceptionInfo> ex) {
        printf("hash<HD> += object: %s\n", ex.err); }
    try { hash<HD> md = wd - "a"; md.c = 1; printf("hash<HD> - key then change: %y\n", md); }
        catch (hash<ExceptionInfo> ex) { printf("hash<HD> - key then change: %s\n", ex.err); }
}

sub live() {
    # a container that weak references designate and that is not freed is changed as before
    list<auto> l = (1, 2, 3);
    list<auto> w;
    w := l;
    splice l, 0, 1;
    extract l, 0, 0, (9,);
    l += (4, 5);
    printf("live list: %y weak %y\n", l, w);
    hash<auto> h = mk();
    hash<auto> wh;
    wh := h;
    h += {"c": 3};
    h += new C();
    printf("live hash: %y weak %y\n", h, wh);
    hash<auto> m = h - "a";
    m.y = 2;
    printf("live hash - key: %y\n", m);
}

freed_list();
freed_hash();
freed_direct();
live();
