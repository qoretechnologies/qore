# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%allow-opaque-references

our auto target;
our list<auto> observations;
our code<auto()> reader;
our bool throw_on_cleanup;
our bool write_on_cleanup;

class CleanupProbe {
    private {
        string id;
    }
    constructor(string id) { self.id = id; }
    destructor() {
        # Both a read and a write must be possible after the compound assignment finishes.
        push observations, (id, reader());
        if (write_on_cleanup) {
            target.mark = 99;
        }
        if (throw_on_cleanup) {
            throw "CLEANUP", id;
        }
    }
}

class Box {
    public {
        auto a;
        auto b;
        int mark = 0;
    }
}

class CycleProbe {
    public {
        Box root;
        *CycleProbe peer;
        static int destroyed = 0;
    }
    constructor(Box root) { self.root = root; }
    destructor() {
        ++destroyed;
        # The root stays live; its variable must be readable during collection of the removed cycle.
        push observations, ("cycle", reader());
    }
}

hashdecl TypedTarget {
    auto a;
    auto b;
    int bad = 0;
}

sub check(auto expected, auto actual, string label) {
    if (expected !== actual) {
        throw "TEST-FAIL", sprintf("%s: expected %y, got %y", label, expected, actual);
    }
}

auto sub snapshot_value(auto value) {
    # Observations must not retain a remaining probe and delay its destructor.
    return value instanceof CleanupProbe ? "probe" : value;
}

sub reset() {
    throw_on_cleanup = False;
    write_on_cleanup = False;
    target = NOTHING;
    observations = ();
    reader = auto sub () { return (snapshot_value(target.a), snapshot_value(target.b)); };
}

sub containers(string which) {
    reset();
    if (which.startsWith("object-")) {
        target = new Box();
    } else {
        target = {};
    }
    target.a = new CleanupProbe("a");
    target.b = new CleanupProbe("b");
    write_on_cleanup = True;
    if (which.endsWith("hash")) {
        target += {"a": 1, "b": 2};
    } else if (which.endsWith("object")) {
        Box source();
        source.a = 1;
        source.b = 2;
        target += source;
    } else if (which.endsWith("single")) {
        target -= "a";
        check(1, observations.size(), which + " first destructor");
        target -= "b";
    } else {
        # Repeated keys and absent hash keys must not release a value twice.
        target -= which.startsWith("object-") ? ("a", "b", "a") : ("a", "absent", "b", "a");
    }
    check(2, observations.size(), which + " destructor count");
    check("a", observations[0][0], which + " first destructor");
    check("b", observations[1][0], which + " second destructor");
    if (which.endsWith("hash") || which.endsWith("object")) {
        check((("a", (1, 2)), ("b", (1, 2))), observations, which + " completed merge");
    } else {
        check((NOTHING, NOTHING), observations[1][1], which + " completed removal");
        if (which.endsWith("list")) {
            check((NOTHING, NOTHING), observations[0][1], which + " removal before cleanup");
        }
    }
    check(99, target.mark, which + " reentrant write");
}

sub throwing(string which) {
    reset();
    target = {};
    target.a = new CleanupProbe("a");
    target.b = new CleanupProbe("b");
    throw_on_cleanup = True;
    bool caught = False;
    try {
        if (which == "throw-plus") {
            target += {"a": 1, "b": 2};
        } else {
            target -= ("a", "b");
        }
    } catch (hash<ExceptionInfo> e) {
        caught = True;
        check("CLEANUP", e.err, which + " propagated exception");
    }
    throw_on_cleanup = False;
    check(True, caught, which + " caught destructor exception");
    check(2, observations.size(), which + " remaining cleanup after exception");
    check(which == "throw-plus" ? (1, 2) : (NOTHING, NOTHING), reader(), which + " final value");
}

sub numeric(string op) {
    reset();
    # These operators replace a whole container, including its last reference to an object.
    target = {"a": new CleanupProbe("a")};
    reader = auto sub () { return target; };
    switch (op) {
        case "*=" : target *= 2; break;
        case "/=" : target /= 2; break;
        case "%=" : target %= 2; break;
        case "|=" : target |= 2; break;
        case "&=" : target &= 2; break;
        case "^=" : target ^= 2; break;
        case "<<=" : target <<= 2; break;
        case ">>=" : target >>= 2; break;
        case "++" : ++target; break;
        case "--" : --target; break;
        case "post++" : target++; break;
        case "post--" : target--; break;
        case "float*=" : target *= 2.0; break;
        case "float/=" : target /= 2.0; break;
        case "number*=" : target *= 2n; break;
        case "number/=" : target /= 2n; break;
    }
    check(1, observations.size(), op + " destructor count");
    check(target, observations[0][1], op + " result visible during cleanup");
}

sub partial_error() {
    reset();
    hash<TypedTarget> typed = <TypedTarget>{"a": new CleanupProbe("a")};
    # No extra reference to the hash: exercise the lock of a closure-captured local.
    reader = auto sub () { return (typed.a, typed.bad); };
    bool caught = False;
    try {
        typed += {"a": 1, "bad": "invalid"};
    } catch (hash<ExceptionInfo> e) {
        caught = True;
        check("RUNTIME-TYPE-ERROR", e.err, "merge failure");
    }
    check(True, caught, "type failure propagated");
    check((("a", (1, 0)),), observations, "cleanup after partial merge failure");
    reader = auto sub () { return NOTHING; };
}

sub nested_object(string which) {
    reset();
    if (which == "nested-hash") {
        hash<auto!> parent = {"obj": new Box()};
        parent.obj.a = new CleanupProbe("a");
        reader = auto sub () { return snapshot_value(parent.obj.a); };
        parent.obj -= "a";
        check((("a", NOTHING),), observations, which + " cleanup");
        # The remaining object still contributes one scannable value to its parent.
        parent.obj = 0;
        check(0, parent.obj, which + " parent scan count");
    } else if (which == "nested-list") {
        list<auto!> parent = (new Box(),);
        parent[0].a = new CleanupProbe("a");
        reader = auto sub () { return snapshot_value(parent[0].a); };
        parent[0] -= "a";
        check((("a", NOTHING),), observations, which + " cleanup");
        parent[0] = 0;
        check(0, parent[0], which + " parent scan count");
    } else {
        Box parent();
        parent.a = new Box();
        parent.a.a = new CleanupProbe("a");
        reader = auto sub () { return snapshot_value(parent.a.a); };
        parent.a -= "a";
        check((("a", NOTHING),), observations, which + " cleanup");
        parent.a = 0;
        check(0, parent.a, which + " parent scan count");
    }
    reader = auto sub () { return NOTHING; };
}

sub partial_remove() {
    reset();
    target = new Box();
    target.a = new CleanupProbe("a");
    target.b = new CleanupProbe("b");
    bool caught = False;
    try {
        target -= ("a", "absent");
    } catch (hash<ExceptionInfo> e) {
        caught = True;
        check("INVALID-MEMBER", e.err, "partial removal failure");
    }
    check(True, caught, "partial removal exception propagated");
    check((("a", (NOTHING, "probe")),), observations, "removed value released after failure");
    target -= "b";
    check(2, observations.size(), "remaining member can still be removed");
}

sub cow(string which) {
    reset();
    hash<auto!> kept = {"a": new CleanupProbe("a")};
    target = kept;
    if (which == "cow-plus") {
        target += {"a": 1};
    } else {
        target -= "a";
    }
    check(True, kept.a instanceof CleanupProbe, "original COW container retains the probe");
    check(0, observations.size(), "retained probe remains live");
    kept = {};
    check((("a", (which == "cow-plus" ? 1 : NOTHING, NOTHING)),), observations,
        "only releasing the original container destroys the probe");
}

sub cycle(string which) {
    reset();
    int before = CycleProbe::destroyed;
    target = new Box();
    {
        CycleProbe first(target);
        first.peer = new CycleProbe(target);
        first.peer.peer = first;
        if (which.startsWith("cycle-hash-")) {
            target.a = {};
            target.a.entry = first;
        } else {
            target.a = first;
        }
    }
    check(before, CycleProbe::destroyed, "the root still holds the cycle");
    reader = auto sub () { return type(target); };
    if (which == "cycle-hash-plus") {
        target.a += {"entry": 1};
    } else if (which == "cycle-hash-minus") {
        target.a -= "entry";
    } else if (which == "cycle-object-plus") {
        target += {"a": 1};
    } else {
        target -= "a";
    }
    check(before + 2, CycleProbe::destroyed, which + " removed cycle collected");
    check(2, observations.size(), which + " destructors read the unlocked root");
}

sub expression(string which) {
    reset();
    target = {};
    target.a = new CleanupProbe("a");
    auto result;
    if (which == "expression-plus") {
        result = (target += {"a": 1});
    } else {
        result = (target -= "a");
    }
    check(target, result, "compound expression result");
    check(1, observations.size(), "consumed result does not postpone cleanup");
}

sub encoded_key() {
    reset();
    target = {"café": new CleanupProbe("a")};
    reader = auto sub () { return target.size(); };
    target -= convert_encoding("café", "ISO-8859-1");
    check(0, target.size(), "encoded removal key");
    check((("a", 0),), observations, "encoded-key cleanup after removal");
}

sub opaque(string which) {
    reset();
    target = which.startsWith("opaque-hash-") ? {} : new Box();
    target.a @= new CleanupProbe("a");
    if (which.endsWith("plus")) {
        target += {"a": 1};
    } else {
        target -= "a";
    }
    check((("a", (which.endsWith("plus") ? 1 : NOTHING, NOTHING)),), observations,
        "opaque value released after unlocking and unregistering its reference");
}

sub shared_containers() {
    reset();
    target = {};
    auto nested = new CleanupProbe("a");
    # Only 31 distinct containers, but over a billion paths to the object. Removing the outer reference must
    # not enumerate those paths, and the separately held reference must keep the object alive.
    for (int i = 0; i < 31; ++i) {
        nested = (nested, nested);
    }
    target.a = nested;
    target -= "a";
    check(0, observations.size(), "shared container remains held");
    nested = NOTHING;
    check((("a", (NOTHING, NOTHING)),), observations, "shared container released once");
}

sub fill_indirect(string which) {
    auto local = new CleanupProbe("a");
    auto held;
    if (which.contains("variable")) {
        # Preserve the reference itself in the hash; it is the last owner of the captured local after return.
        hash<auto!> h = {"a": \local};
        target = h;
        return;
    } else if (which.contains("bound")) {
        held = SandboxManager::bindCall(auto sub () { return local; });
    } else {
        held = auto sub () { return local; };
    }
    if (which.contains("object")) {
        target = new Box();
        target.a = held;
    } else {
        hash<auto!> h = {"a": held};
        target = h;
    }
}

sub indirect(string which) {
    reset();
    fill_indirect(which);
    check(0, observations.size(), "the indirect value retains the probe");
    bool throwing = which.contains("throw");
    throw_on_cleanup = throwing;
    bool caught = False;
    try {
        if (which.endsWith("assign")) {
            target = {};
        } else if (which.endsWith("plus")) {
            target += {"a": 1};
        } else {
            target -= "a";
        }
    } catch (hash<ExceptionInfo> e) {
        caught = True;
        check("CLEANUP", e.err, "indirect cleanup exception");
    }
    throw_on_cleanup = False;
    check(throwing, caught, "indirect cleanup exception propagated");
    check((("a", (which.endsWith("plus") ? 1 : NOTHING, NOTHING)),), observations,
        "indirect owner releases its probe after unlocking");
}

sub run(string which) {
    if (which.startsWith("hash-") || which.startsWith("object-")) {
        containers(which);
    } else if (which.startsWith("throw-")) {
        throwing(which);
    } else if (which == "partial-error") {
        partial_error();
    } else if (which.startsWith("nested-")) {
        nested_object(which);
    } else if (which == "partial-remove") {
        partial_remove();
    } else if (which.startsWith("cow-")) {
        cow(which);
    } else if (which.startsWith("cycle-")) {
        cycle(which);
    } else if (which.startsWith("expression-")) {
        expression(which);
    } else if (which == "encoded-key") {
        encoded_key();
    } else if (which.startsWith("opaque-")) {
        opaque(which);
    } else if (which == "shared-containers") {
        shared_containers();
    } else if (which.startsWith("indirect-")) {
        indirect(which);
    } else {
        numeric(which);
    }
    reset();
    printf("PASS %s\n", which);
}

list<string> cases = ("hash-plus-hash", "hash-plus-object", "object-plus-hash", "object-plus-object",
    "hash-minus-single", "hash-minus-list", "object-minus-single", "object-minus-list",
    "throw-plus", "throw-minus", "partial-error", "*=", "/=", "%=", "|=", "&=", "^=", "<<=", ">>=",
    "++", "--", "float*=", "float/=", "number*=", "number/=",
    "nested-hash", "nested-list", "nested-object", "partial-remove", "cow-plus", "cow-minus",
    "cycle-hash-plus", "cycle-hash-minus", "cycle-object-plus", "cycle-object-minus",
    "expression-plus", "expression-minus", "encoded-key", "post++", "post--",
    "opaque-hash-plus", "opaque-hash-minus", "opaque-object-plus", "opaque-object-minus", "shared-containers",
    "indirect-variable-plus", "indirect-variable-minus", "indirect-bound-hash-plus", "indirect-bound-hash-minus",
    "indirect-bound-object-plus", "indirect-bound-object-minus", "indirect-closure-hash-plus",
    "indirect-closure-hash-minus", "indirect-closure-object-plus", "indirect-closure-object-minus",
    "indirect-throw-variable-plus", "indirect-throw-bound-plus", "indirect-throw-variable-assign");
if (ARGV) {
    run(ARGV[0]);
} else {
    foreach string which in (cases) {
        run(which);
    }
}
