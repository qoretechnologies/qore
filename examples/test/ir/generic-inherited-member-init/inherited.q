%modern

# members with an initializer of a generic class, inherited by classes that give the type arguments (qore#6027): the
# type parameter of the member's type is the type argument of the parent class, as it is for the same member of an
# object of the generic class itself

class Box<T> {
    public {
        T v;
        hash<string, T> mi = {};
        list<T> ml = ();
        hash<string, list<T>> nested = {};
        hash<string, T> h;
    }

    constructor(T v) {
        self.v = v;
    }

    setEmpty() {
        h = {};
    }
}

class Pair<K, V> {
    public {
        hash<string, V> vals = {};
        list<K> ids = ();
    }
}

# a non-generic class that gives the type argument, and one that inherits it
class IntBox inherits Box<int> {
    constructor() : Box(5) {
    }
}

class StringLeaf inherits Box<string> {
    constructor() : Box("s") {
    }
}

class LeafOfIntBox inherits IntBox {
}

# a generic class that passes its parameter on, and one of its instantiations
class GBox<U> inherits Box<U> {
    constructor(U v) : Box(v) {
    }
}

# two generic parents
class TwoParents inherits Box<float>, Pair<string, int> {
    constructor() : Box(1.5) {
    }
}

sub show(string label, object o) {
    printf("%s: mi %s ml %s nested %s\n", label, o.mi.fullType(), o.ml.fullType(), o.nested.fullType());
}

Box<int> b(1);
show("direct", b);
show("IntBox", new IntBox());
show("StringLeaf", new StringLeaf());
show("LeafOfIntBox", new LeafOfIntBox());
show("GBox<int>", new GBox<int>(3));
show("GBox<string>", new GBox<string>("x"));
TwoParents tp();
show("TwoParents", tp);
printf("TwoParents: vals %s ids %s\n", tp.vals.fullType(), tp.ids.fullType());

# the members are usable: the elements are checked against the type argument
IntBox ib();
ib.mi.x = 1;
ib.ml += 2;
ib.nested.k = (1, 2);
printf("%y %y %y\n", ib.mi, ib.ml, ib.nested);

# a value of another type is refused by the member of the inherited instantiation
try {
    ib.mi.y = "no";
} catch (hash<ExceptionInfo> ex) {
    printf("refused: %s\n", ex.err);
}

GBox<string> gb("a");
gb.mi.x = "v";
printf("%y\n", gb.mi);
try {
    gb.ml += 7;
} catch (hash<ExceptionInfo> ex) {
    printf("refused: %s\n", ex.err);
}
