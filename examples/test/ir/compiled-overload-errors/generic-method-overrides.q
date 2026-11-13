%modern
# the variants of generic methods: the type parameters of a generic method belong to its signature, so a method that
# overrides or implements a generic method has type parameters of its own, which correspond by position (qore#6018:
# comparing them crashed when the class had another variant of the method, and an abstract generic method could not
# be implemented); see compiled-overload-errors.qtest

# an override of a generic method in a class whose method has another variant (the issue's example)
class GM {
    string id<U>(U x) { return "GM::id"; }
    string id(string s, int k) { return "GM::id(string, int)"; }
    string opt<U>(*U x) { return "GM::opt"; }
    string lst<U>(list<U> x) { return "GM::lst"; }
    string two<U, V>(U a, V b) { return "GM::two"; }
}
class GMC inherits GM {
    string id<U>(U y) { return "GMC::id"; }
    string opt<U>(*U y) { return "GMC::opt"; }
    string lst<U>(list<U> y) { return "GMC::lst"; }
    string two<U, V>(U c, V d) { return "GMC::two"; }
}

# generic abstract methods implemented with type parameters of their own
class GA {
    abstract U ga<U>(U x);
    abstract string gl<U>(list<U> l);
    abstract *U gn<U>(U x);
}
class GAI inherits GA {
    V ga<V>(V x) { return x; }
    string gl<U>(list<U> l) { return sprintf("GAI::gl %d", l.size()); }
    # a narrower return type: U for *U
    U gn<U>(U x) { return x; }
}

# not overrides: other bounds, another number of type parameters, and a non-generic method
class GB {
    string b<U: int>(U x) { return "GB::b"; }
    string c<U, V>(U x) { return "GB::c"; }
    string g<U>(U x) { return "GB::g"; }
}
class GBC inherits GB {
    string b<U: string>(U x) { return "GBC::b"; }
    string c<U>(U x) { return "GBC::c"; }
    string g(auto x) { return "GBC::g"; }
}

auto i = 2;
GM gm = new GMC();
printf("override: %s %s %s\n", gm.id(2), gm.id(i), gm.id("s", 1));
printf("or nothing: %s %s\n", gm.opt(2), gm.opt(NOTHING));
printf("list: %s\n", gm.lst((1, 2)));
printf("two type parameters: %s\n", gm.two(1, "s"));

GA ga = new GAI();
printf("abstract: %y %y %s %y\n", ga.ga("a"), ga.ga(i), ga.gl((1, 2, 3)), ga.gn(3));

GBC gbc();
printf("not overrides: %s %s %s %s\n", gbc.b(1), gbc.b("s"), gbc.c(1), gbc.g(1));

# an implementation returning *U for U (a wider type) does not implement the abstract method
Program pw(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
try {
    pw.parse("class A { abstract U a<U>(U x); } class AI inherits A { *U a<U>(U x) { return x; } } "
        "A x = new AI();", "wider");
    printf("wider return: parsed\n");
} catch (hash<ExceptionInfo> ex) {
    printf("wider return: %s\n", ex.err);
}

# an implementation with another number of type parameters does not implement the abstract method
Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
try {
    p.parse("class A { abstract string a<U>(U x); } class AI inherits A { string a<U, V>(U x) { return \"\"; } } "
        "A x = new AI();", "count");
    printf("other count: parsed\n");
} catch (hash<ExceptionInfo> ex) {
    printf("other count: %s\n", ex.err);
}
