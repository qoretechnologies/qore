%modern

class Box<T> {
    public {
        *T v;
    }

    constructor(*T v) {
        self.v = v;
    }
}

#! returns the result of the given closure, or the exception it raises
string sub probe(string label, code c) {
    try {
        return sprintf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

#! returns the value of the object in the list and the type of the list
string sub describe(list<auto> l, int i) {
    return sprintf("%y %s", l[i].v, l.fullType());
}

#! returns a list of NOTHING and an object of the class specialized with the type argument
list<auto> sub generic_list<T>(T v) {
    return (NOTHING, new Box<T>(v));
}

#! returns the list given as argument after the first argument
string sub call_args(code f, list<auto> l) {
    return describe(l, 1);
}

string sub probes() {
    string rv;
    # NOTHING first and second: the common type of NOTHING and object<Box<int>> is *object<Box<int>>
    rv += probe("nothing first", string sub () { return describe((NOTHING, new Box<int>(1)), 1); });
    rv += probe("nothing second", string sub () { return describe((new Box<int>(2), NOTHING), 0); });
    rv += probe("list<auto> local", string sub () {
        list<auto> l = (NOTHING, new Box<int>(3));
        return describe(l, 1);
    });
    rv += probe("auto local", string sub () {
        auto l = (NOTHING, new Box<int>(4));
        return describe(l, 1);
    });
    # a nullable local of the specialized type without a value
    rv += probe("*Box<int> local", string sub () {
        *Box<int> b;
        return describe((b, new Box<int>(5)), 1);
    });
    rv += probe("string type argument", string sub () {
        return describe((NOTHING, new Box<string>("s")), 1);
    });
    # a value of another specialization of the same class is not accepted by *object<Box<int>>: no common type
    rv += probe("other specialization", string sub () {
        return describe((NOTHING, new Box<int>(6), new Box<string>("t")), 2);
    });
    rv += probe("call argument", string sub () { return call_args(sub () {}, (NOTHING, new Box<int>(7))); });
    rv += probe("generic function", string sub () { return describe(generic_list<int>(8), 1); });
    rv += probe("hash literal", string sub () {
        auto h = {"a": NOTHING, "b": new Box<int>(9)};
        return sprintf("%y %s", h.b.v, h.fullType());
    });
    return rv;
}

# the same at the top level, which the AST interpreter folds at parse time
list<auto> top_level = (NOTHING, new Box<int>(10));
string sub probes_top() {
    return sprintf("top level: %s\n", describe(top_level, 1));
}
