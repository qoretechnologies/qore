# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%allow-opaque-references
%requires cppapiuser

our auto root;
our bool active = False;
our string trigger;
our string action;
our int callbacks = 0;
our bool recording = False;
our list<auto> notifications;
our int destroyed = 0;
our *code<auto()> writer;
our Queue entered();
our Queue done();

sub check(auto expected, auto actual, string label) {
    if (expected !== actual) {
        throw "TEST-FAIL", sprintf("%s: expected %y, got %y", label, expected, actual);
    }
}

int sub callback(string phase) {
    if (!active || phase != trigger) {
        return 0;
    }
    active = False;
    ++callbacks;
    # Reading the parent must be possible even when the compound assignment locks its global root.
    string parent_type = type(root);
    if (writer) {
        writer();
        return 0;
    }
    if (action == "thread") {
        entered.push(True);
        done.get(5000);
    } else if (action == "remove") {
        remove root;
    } else {
        root = {"replacement": 99};
    }
    if (action == "throw") {
        throw "NATIVE-CALLBACK", "parent replaced";
    }
    return 0;
}

class GateProbe inherits CppApiTest::MemberWriteProbe {
    public { auto normal; }
    int reenter() { return callback("gate"); }
    destructor() { ++destroyed; }
}

class AccessorProbe inherits CppApiTest::MemberAccessorProbe {
    int reenter(string phase) { return callback(phase); }
    destructor() { ++destroyed; }
}

class Box {
    public { auto item; }
    constructor(auto item) { self.item = item; }
    memberNotification(string member) {
        if (recording) {
            push notifications, (member, item.any);
        }
    }
}

auto sub operate(reference<auto> slot, string op) {
    switch (op) {
        case "minus": return slot -= "any";
        case "minus-many": return slot -= ("any", "declared");
        case "plus": return slot += {"any": 13, "count": 17};
        case "remove": return remove slot.any;
        case "remove-many": return remove slot{"any", "declared"};
        case "delete": delete slot.any; return NOTHING;
    }
    throw "TEST-FAIL", "unknown operation " + op;
}

auto sub apply(string shape, string op) {
    switch (shape) {
        case "global": return operate(\root, op);
        case "hash": return operate(\root.item, op);
        case "list": return operate(\root[0], op);
        case "member": return operate(\root.item, op);
        case "gate-parent": return operate(\root.any, op);
        case "accessor-parent": return operate(\root.any, op);
    }
    throw "TEST-FAIL", "unknown shape " + shape;
}

sub scenario(string kind, string shape, string op, string phase, string mutation) {
    active = False;
    callbacks = 0;
    notifications = ();
    recording = False;
    trigger = phase;
    action = mutation;
    object original = kind == "gate" ? new GateProbe() : new AccessorProbe();
    original.any = 7;
    switch (shape) {
        case "global": root = original; break;
        case "hash": root = {"item": original}; break;
        case "list": root = (original,); break;
        case "member": root = new Box(original); break;
        case "gate-parent": root = new CppApiTest::MemberWriteProbe(); root.any = original; break;
        case "accessor-parent": root = new CppApiTest::MemberAccessorProbe(); root.any = original; break;
    }
    int tid = 0;
    if (action == "thread") {
        tid = background sub () {
            entered.get(5000);
            root = {"replacement": 99};
            done.push(True);
        }();
    }
    on_exit {
        active = False;
        if (tid) {
            thread_join(tid);
        }
    }
    active = True;
    recording = True;
    auto result;
    bool raised = False;
    try {
        result = apply(shape, op);
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err != "NATIVE-CALLBACK" || action != "throw") {
            rethrow;
        }
        check("parent replaced", ex.desc, "callback exception preserved");
        raised = True;
    }
    active = False;
    recording = False;
    check(1, callbacks, "native callback ran");
    check(action == "throw", raised, "callback exception propagated");
    check(action == "remove" ? NOTHING : {"replacement": 99}, root, "callback's parent write survived");
    if (shape == "member") {
        check((("item", raised ? 7 : op == "plus" ? 13 : NOTHING),), notifications,
            "parent notification runs after the native mutation");
    }
    if (!raised) {
        if (op == "minus" || op == "minus-many" || op == "plus") {
            check(True, result === original, "compound result is the original target");
        } else if (op == "remove") {
            check(7, result, "removed value");
        } else if (op == "remove-many") {
            check({"any": 7, "declared": 7}, result, "slice values");
        }
        check(op == "plus" ? 13 : NOTHING, original.any, "original object was updated");
        if (op == "minus-many" || op == "remove-many") {
            check(NOTHING, original.declared, "ordinary member was removed too");
        }
    }
    printf("PASS %s/%s/%s/%s/%s\n", kind, shape, op, phase, mutation);
}

sub uniqueTarget(string kind, string op, bool throwing = False) {
    active = recording = False;
    destroyed = callbacks = 0;
    trigger = kind == "gate" ? "gate" : op == "plus" ? "set" : "get";
    action = throwing ? "throw" : "replace";
    root = {"item": kind == "gate" ? new GateProbe() : new AccessorProbe()};
    root.item.any = 7;
    active = True;
    auto result;
    bool raised = False;
    try {
        result = apply("hash", op);
    } catch (hash<ExceptionInfo> ex) {
        if (!throwing || ex.err != "NATIVE-CALLBACK") { rethrow; }
        raised = True;
    }
    active = False;
    check(throwing, raised, "unique target exception");
    check(1, callbacks, "unique target callback");
    check({"replacement": 99}, root, "unique target parent replacement");
    if (!throwing && (op == "minus" || op == "minus-many" || op == "plus")) {
        check(0, destroyed, "target stays live until the returned reference is released");
        check(op == "plus" ? 13 : NOTHING, result.any, "unique original target was updated");
        remove result;
    }
    check(1, destroyed, "target destroyed exactly once");
    printf("PASS %s/unique-target/%s/%s\n", kind, op, throwing ? "throw" : "success");
}

our int cycles_destroyed = 0;
class Cycle {
    public { auto target; }
    constructor(auto target) { self.target = target; }
    destructor() {
        ++cycles_destroyed;
        check({"replacement": 99}, root, "cycle destructor sees callback's parent replacement");
    }
}

sub cycleTarget(bool slice, bool throwing) {
    active = recording = False;
    destroyed = callbacks = cycles_destroyed = 0;
    trigger = "gate";
    action = throwing ? "throw" : "replace";
    root = {"item": new GateProbe()};
    # The same cycle has an ordinary member edge and a native private-data edge. No independent reference keeps
    # it alive; removed values must survive graph repair, including a callback that throws after the first removal.
    root.item.normal = new Cycle(root.item);
    root.item.any = root.item.normal;
    active = True;
    bool raised = False;
    auto result;
    try {
        if (slice) {
            result = remove root.item{"normal", "normal", "any", "any"};
        } else {
            result = root.item -= ("normal", "any");
        }
    } catch (hash<ExceptionInfo> ex) {
        if (!throwing || ex.err != "NATIVE-CALLBACK") { rethrow; }
        raised = True;
    }
    active = False;
    check(throwing, raised, "cycle callback exception");
    check(1, callbacks, "cycle callback");
    check({"replacement": 99}, root, "cycle parent replacement");
    if (!throwing) {
        if (!slice) {
            check(NOTHING, result.normal, "ordinary cycle edge removed");
            check(NOTHING, result.any, "native cycle edge removed");
        }
        remove result;
    }
    check(1, cycles_destroyed, "cycle collected exactly once");
    check(1, destroyed, "cycle target collected exactly once");
    printf("PASS cycle/%s/%s\n", slice ? "slice" : "minus", throwing ? "throw" : "success");
}

sub localTarget(string kind) {
    active = recording = False;
    destroyed = callbacks = 0;
    trigger = kind == "gate" ? "gate" : "set";
    action = "replace";
    root = NOTHING;
    auto local = kind == "gate" ? new GateProbe() : new AccessorProbe();
    local.any = 7;
    active = True;
    auto result = kind == "gate" ? (local -= "any") : (local += {"any": 13});
    active = False;
    check(1, callbacks, "local callback");
    check(True, result === local, "local target identity");
    check(kind == "gate" ? NOTHING : 13, local.any, "local operation");
    remove result;
    remove local;
    check(1, destroyed, "local target released");
    printf("PASS %s/local-target\n", kind);
}

sub closureTarget(string kind) {
    active = recording = False;
    destroyed = callbacks = 0;
    trigger = kind == "gate" ? "gate" : "set";
    action = "replace";
    root = NOTHING;
    auto local = kind == "gate" ? new GateProbe() : new AccessorProbe();
    local.any = 7;
    writer = sub () { local = {"replacement": 99}; root = local; };
    on_exit { writer = NOTHING; active = False; }
    active = True;
    auto result = kind == "gate" ? (local -= "any") : (local += {"any": 13});
    active = False;
    check(1, callbacks, "closure callback");
    check({"replacement": 99}, local, "callback replaced closure-bound target");
    check(kind == "gate" ? NOTHING : 13, result.any, "original closure target operation");
    remove result;
    check(1, destroyed, "closure target released");
    printf("PASS %s/closure-target\n", kind);
}

sub externalParentWrite(string kind, string parent_kind) {
    active = recording = False;
    destroyed = callbacks = 0;
    trigger = kind == "gate" ? "gate" : "set";
    root = parent_kind == "gate" ? new CppApiTest::MemberWriteProbe() : new CppApiTest::MemberAccessorProbe();
    root.any = kind == "gate" ? new GateProbe() : new AccessorProbe();
    root.any.any = 7;
    writer = sub () { root.any = 99; };
    on_exit { writer = NOTHING; active = False; }
    active = True;
    auto result = kind == "gate" ? (root.any -= "any") : (root.any += {"any": 13});
    active = False;
    check(1, callbacks, "external parent callback");
    check(99, root.any, "external member writeback did not overwrite callback's write");
    check(kind == "gate" ? NOTHING : 13, result.any, "original external-member target operation");
    remove result;
    check(1, destroyed, "external-member target released");
    printf("PASS %s/%s-parent-write\n", kind, parent_kind);
}

sub concurrentTargetMembers() {
    active = recording = False;
    callbacks = 0;
    trigger = "gate";
    action = "replace";
    GateProbe target();
    bool stop = False;
    Queue started();
    int tid = background sub () {
        int iterations = 0;
        while (!stop) {
            target.normal = new Box(NOTHING);
            remove target.normal;
            if (++iterations == 1) { started.push(True); }
        }
    }();
    on_exit { active = False; stop = True; thread_join(tid); }
    started.get(5000);
    for (int i = 0; i < 64; ++i) {
        active = False;
        target.any = i;
        root = target;
        active = True;
        auto result = root -= "any";
        active = False;
        check(True, result === target, "concurrent target identity");
        check(NOTHING, result.any, "concurrent native removal");
    }
    check(64, callbacks, "callbacks with concurrent ordinary target writes");
    printf("PASS concurrent-target-members\n");
}

sub opaqueCompoundResult(string kind, string op, bool reentrant) {
    active = recording = False;
    callbacks = 0;
    trigger = kind == "gate" ? "gate" : op == "plus" ? "set" : "get";
    action = "replace";
    root = {};
    if (kind == "gate") {
        root.item @= new GateProbe();
    } else {
        root.item @= new AccessorProbe();
    }
    root.item.any = 7;
    # Compare with an ordinary read: AST currently normalizes opaque call arguments through ValueHolder.
    bool expected_opaque = CppApiTest::MemberAccessorProbe::isOpaqueValue(root.item);
    active = reentrant;
    bool opaque;
    if (op == "plus") {
        opaque = CppApiTest::MemberAccessorProbe::isOpaqueValue(root.item += {"any": 13});
    } else {
        opaque = CppApiTest::MemberAccessorProbe::isOpaqueValue(root.item -= "any");
    }
    active = False;
    check(expected_opaque, opaque, "compound result retains opaque representation");
    if (reentrant) {
        check(1, callbacks, "opaque target callback");
        check({"replacement": 99}, root, "opaque target parent replacement");
    }
    printf("PASS opaque/%s/%s/%s\n", kind, op, reentrant ? "callback" : "plain");
}

sub nativeApiContract(string kind, bool reentrant = False) {
    active = recording = False;
    callbacks = 0;
    trigger = kind == "gate" ? "gate" : "get";
    action = "replace";
    if (kind == "gate") {
        root = new GateProbe();
    } else if (kind == "accessor") {
        root = new AccessorProbe();
    } else if (kind == "object") {
        root = new Box(NOTHING);
    } else {
        root = {};
    }
    string member = kind == "object" ? "item" : "any";
    root{member} = 7;
    int reuse = 41;
    active = reentrant;
    auto result = CppApiTest::MemberAccessorProbe::referenceRemove(\root, \reuse, member);
    active = False;
    bool ordinary = kind == "hash" || kind == "object";
    check({"removed": 7, "valid": ordinary, "locked": ordinary, "before": 41, "after": 42}, result,
        "native reference API removal, invalidation and reset/reuse");
    check(42, reuse, "reset helper assigned the new reference");
    if (reentrant) {
        check(1, callbacks, "native reference API callback");
        check({"replacement": 99}, root, "native API does not reacquire or overwrite parent");
    }
    printf("PASS native-api/%s/%s\n", kind, reentrant ? "callback" : "plain");
}

if (ARGV && ARGV[0] == "opaque") {
    opaqueCompoundResult(ARGV[1], ARGV[2], False);
} else if (ARGV && ARGV[0] == "api-contract") {
    nativeApiContract(ARGV[1]);
} else if (ARGV) {
    scenario(ARGV[0], ARGV[1], ARGV[2], ARGV[3], ARGV[4]);
} else {
    foreach string kind in ("gate", "accessor") {
        foreach string op in ("minus", "minus-many", "remove", "remove-many", "delete", "plus") {
            if (kind != "gate" || op != "plus") {
                uniqueTarget(kind, op);
            }
        }
        localTarget(kind);
        closureTarget(kind);
        externalParentWrite(kind, "gate");
        externalParentWrite(kind, "accessor");
        uniqueTarget(kind, "minus", True);
        if (kind == "accessor") { uniqueTarget(kind, "plus", True); }
        foreach string shape in ("global", "hash", "list", "member", "gate-parent", "accessor-parent") {
            foreach string op in ("minus", "minus-many", "remove", "remove-many", "delete", "plus") {
                if (kind == "gate" && op == "plus") {
                    continue;
                }
                foreach string phase in (kind == "gate" ? ("gate",) : op == "plus" ? ("set",) : ("get", "set")) {
                    foreach string mutation in ("replace", "remove", "throw", "thread") {
                        scenario(kind, shape, op, phase, mutation);
                    }
                }
            }
        }
    }
    foreach string kind in ("gate", "accessor", "hash", "object") { nativeApiContract(kind); }
    nativeApiContract("gate", True);
    nativeApiContract("accessor", True);
    foreach bool reentrant in (False, True) {
        opaqueCompoundResult("gate", "minus", reentrant);
        opaqueCompoundResult("accessor", "minus", reentrant);
        opaqueCompoundResult("accessor", "plus", reentrant);
    }
    concurrentTargetMembers();
    cycleTarget(False, False);
    cycleTarget(False, True);
    cycleTarget(True, False);
    cycleTarget(True, True);
}
