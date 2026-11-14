%modern
%disable-warning unreferenced-variable
%disable-warning invalid-operation

# $$ (the list of the implicit arguments) in the contexts that set it, in every execution mode: the single element of
# map, select and hash map, the two values of foldl and foldr, and the arguments of the function in a foreach body
# (qore#5804, #5807, #5808); the order in which a fold and a hash map release their values (qore#5806)

class Tracked {
    public {
        string n;
    }

    constructor(string n) {
        self.n = n;
    }

    destructor() {
        printf("release %s\n", n);
    }
}

string sub thrower(string s) {
    throw "E", s;
}

# an element with no value gives no list: $$ is NOTHING (qore#5804)
sub nothingElement() {
    list ul = (NOTHING, "x", NULL, 0, "", False);
    printf("map hash: %y\n", (map {($1 ?? "n") + $#: $$}, ul));
    printf("map list: %y\n", (map $$, ul));
    printf("select: %y\n", (select ul, $$.size() == 1));
    printf("select not nothing: %y\n", (select ul, exists $$));
    printf("size: %y\n", (map $$.size(), ul));
    printf("fold: %y\n", (foldl $$.size(), (1, 2, 3)));
    printf("foldr: %y\n", (foldr $$, (NOTHING, 2)));
    printf("fold first: %y\n", (foldl $$[0], (NOTHING, 2, 3)));
}

# $$ in a nested context, in the operands of logical operators, in a method call and with an exception
class ArgvHolder {
    string describe(auto a) {
        return sprintf("%y", a);
    }
}

sub nestedAndLogical() {
    list ul = (NOTHING, "x", 0);
    printf("map in map: %y\n", (map (map $$, ($1, NOTHING)), ul));
    printf("select and: %y\n", (select ul, $$ && $$.size() == 1));
    printf("select or: %y\n", (select ul, !$$ || $$[0] == "x"));
    printf("coalesce: %y\n", (map ($$ ?? "none"), ul));
    printf("ternary: %y\n", (map ($$ ? "list" : "nothing"), ul));
    ArgvHolder h();
    printf("method: %y\n", (map h.describe($$), ul));
    list<int> li = (1, 2);
    printf("typed: %y\n", (map $$, li));
    printf("typed select: %y\n", (select li, $$[0] == 2));
    printf("single value: %y\n", (map $$, NOTHING));
    printf("fold nothing first: %y\n", (foldl $$.size(), (NOTHING, 2)));
    printf("fold nothing last: %y\n", (foldl $$[1], (1, NOTHING)));
    try {
        auto r = (map (new Tracked("t" + $#)).n + ($$ ? "" : thrower("none")), (1, NOTHING, 3));
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.desc);
    }
}

# $$ in a foreach or a for loop is the list of the function's arguments (qore#5807)
string sub foreachArguments(...) {
    foreach string m in ("x") {
        return sprintf("%y", $$);
    }
    return "none";
}

list<auto> sub loopArguments(...) {
    list<auto> l = ();
    for (int i = 0; i < 2; ++i) {
        push l, $$.size();
    }
    foreach string m in ("a", "b") {
        push l, $$;
    }
    int j = 0;
    while (j++ < 2) {
        push l, $$[0];
    }
    return l;
}

# a map in the body of a loop has its own $$: the element, not the arguments
list<auto> sub mapInLoop(...) {
    list<auto> l = ();
    foreach string m in ("a") {
        push l, (map $$, (1, NOTHING, 3));
        push l, $$;
    }
    return l;
}

# $$ has no element type (qore#5808)
sub typeOfArgv() {
    printf("%y\n", map {$1: $#}, ("a", "b"), $$.size() == 1);
    list<int> li = (1, 2, 3);
    printf("%y\n", (foldl $1 + $2 + elements $$, li));
    printf("%y\n", (map elements $$, li));
}

# the values of a fold are released in the order of the AST interpreter (qore#5806)
sub foldRelease() {
    printf("fold literal:\n");
    {
        auto r = (foldl new Tracked($1.n + $2.n), (new Tracked("f1"), new Tracked("f2"), new Tracked("f3")));
    }
    printf("foldr literal:\n");
    {
        auto r = (foldr new Tracked($1.n + $2.n), (new Tracked("r1"), new Tracked("r2"), new Tracked("r3")));
    }
}

sub hashMapKeyRelease() {
    printf("hash map key:\n");
    try {
        hash h = (map {(new Tracked("e" + $1)).n + ($1 == 2 ? thrower("x") : ""): 1}, (1, 2, 3));
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.desc);
    }
    printf("hash map key, tracked elements:\n");
    try {
        hash h = (map {$1.n + ($1.n == "k2" ? thrower("y") : ""): 1},
            (new Tracked("k1"), new Tracked("k2"), new Tracked("k3")));
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.desc);
    }
    printf("hash map value:\n");
    try {
        hash h = (map {$1.n: ($1.n == "w2" ? thrower("z") : 1)},
            (new Tracked("w1"), new Tracked("w2"), new Tracked("w3")));
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.desc);
    }
}

nothingElement();
nestedAndLogical();
printf("foreach: %s\n", foreachArguments("A", "B"));
printf("foreach no arguments: %s\n", foreachArguments());
printf("loops: %y\n", loopArguments(7, 8, 9));
printf("map in loop: %y\n", mapInLoop("p", "q"));
typeOfArgv();
foldRelease();
hashMapKeyRelease();
