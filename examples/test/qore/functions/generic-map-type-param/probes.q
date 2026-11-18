# probes of generic-map-type-param.qtest (qore#5988): the map, map select, hash map and hash map select operators
# in a generic function or a method of a generic class create their result with the type argument of the call in
# place of the type parameter, as select and take do; probes() returns one line per pass, so that the code runs on
# every tier with tiered execution

%modern

auto sub mapped<T>(list<T> values) { return map $1, values; }
auto sub mapsel<T>(list<T> values) { return map $1, values, True; }
auto sub hmapped<T>(list<T> values) { return map {"k" + $#: $1}, values; }
auto sub hmapsel<T>(list<T> values) { return map {"k" + $#: $1}, values, True; }
auto sub selected<T>(list<T> values) { return select values, True; }
auto sub taken<T>(list<T> values) { return take 2, values; }
auto sub mapit<T>(list<T> values) { return map $1, values.iterator(); }
list<T> sub typed<T>(list<T> values) { return map $1, values; }
auto sub selmap<T>(list<T> values) { return select (map $1, values), True; }
auto sub selmapsel<T>(list<T> values) { return select (map $1, values, True), True; }
auto sub takemap<T>(list<T> values) { return take 1, (map $1, values); }
auto sub dropmap<T>(list<T> values) { return drop 1, (map $1, values); }
auto sub selmapit<T>(list<T> values) { return select (map $1, values.iterator()), True; }
# the operators copied for a background expression in a generic function
auto sub bghmapped<T>(list<T> values) { Queue q(); background q.push(map {"k" + $#: $1}, values); return q.get(10s); }

class Box<T> {
    public {
        list<T> values;
    }
    constructor(list<T> v) { values = v; }
    auto mapped() { return map $1, values; }
    auto hmapped() { return map {"k" + $#: $1}, values; }
    # the operators copied for a background expression keep their class
    auto bgMapped() { Queue q(); background q.push(map $1, values); return q.get(10s); }
    auto bgMapsel() { Queue q(); background q.push(map $1, values, True); return q.get(10s); }
    auto bgHmapped() { Queue q(); background q.push(map {"k" + $#: $1}, values); return q.get(10s); }
    auto bgHmapsel() { Queue q(); background q.push(map {"k" + $#: $1}, values, True); return q.get(10s); }
}

auto sub show(code c) {
    try {
        auto v = c();
        return sprintf("%y (%s)", v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s "
            "| %s | %s | %s | %s\n", i,
            show(auto sub () { return mapped<int>((1, 2)); }),
            show(auto sub () { return mapsel<int>((1, 2)); }),
            show(auto sub () { return hmapped<int>((1, 2)); }),
            show(auto sub () { return hmapsel<int>((1, 2)); }),
            show(auto sub () { return selected<int>((1, 2)); }),
            show(auto sub () { return taken<int>((1, 2)); }),
            show(auto sub () { return mapped<string>(("a", "b")); }),
            show(auto sub () { return mapit<int>((1, 2)); }),
            show(auto sub () { return typed<int>((1, 2)); }),
            show(auto sub () { return new Box<int>((1, 2)).mapped(); }),
            show(auto sub () { return new Box<int>((1, 2)).hmapped(); }),
            show(auto sub () { return selmap<int>((1, 2)); }),
            show(auto sub () { return selmapsel<int>((1, 2)); }),
            show(auto sub () { return takemap<int>((1, 2)); }),
            show(auto sub () { return dropmap<int>((1, 2)); }),
            show(auto sub () { return selmapit<int>((1, 2)); }),
            show(auto sub () { return new Box<int>((1, 2)).bgMapped(); }),
            show(auto sub () { return new Box<int>((1, 2)).bgMapsel(); }),
            show(auto sub () { return new Box<int>((1, 2)).bgHmapped(); }),
            show(auto sub () { return new Box<int>((1, 2)).bgHmapsel(); }),
            show(auto sub () { return bghmapped<int>((1, 2)); }),
            # type arguments inferred from the arguments
            show(auto sub () { return mapped((1, 2)); }));
    }
    return rv;
}

# the select expression of a map select in generic code follows the lexical semantic policy of the code: the string
# "x" is False with %strict-bool-eval (strict mathematic evaluation: it converts to 0), True otherwise
auto sub lexmapsel<T>(list<T> values, list<string> conds) { return map $1, values, conds[$#]; }
auto sub lexhmapsel<T>(list<T> values, list<string> conds) { return map {"k" + $#: $1}, values, conds[$#]; }
# ... also in background expressions
auto sub lexbgmapsel<T>(list<T> values, list<string> conds) {
    Queue q();
    background q.push(map $1, values, conds[$#]);
    return q.get(10s);
}
auto sub lexbghmapsel<T>(list<T> values, list<string> conds) {
    Queue q();
    background q.push(map {"k" + $#: $1}, values, conds[$#]);
    return q.get(10s);
}

string sub probes_lexical() {
    return sprintf("%s | %s | %s | %s", show(auto sub () { return lexmapsel<int>((1, 2), ("x", "1")); }),
        show(auto sub () { return lexhmapsel<int>((1, 2), ("x", "1")); }),
        show(auto sub () { return lexbgmapsel<int>((1, 2), ("x", "1")); }),
        show(auto sub () { return lexbghmapsel<int>((1, 2), ("x", "1")); }));
}
