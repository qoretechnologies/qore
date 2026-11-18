# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%allow-opaque-references

our list<int> target;
our hash<auto> nested;
our auto box;
our string scenario;
our list<auto> observations;
our bool write_on_cleanup;
our bool throw_on_cleanup;

class Box {
    public {
        list<int> values;
    }
}

list<int> sub read_target() {
    if (scenario == "nested-hash") {
        return nested.values;
    }
    if (scenario == "object-member") {
        return box.values;
    }
    return target;
}

class Probe {
    destructor() {
        # Read and replace the list whose append rejected this temporary.
        push observations, read_target();
        if (write_on_cleanup) {
            if (scenario == "nested-hash") {
                nested.values = (91,);
            } else if (scenario == "object-member") {
                box.values = (91,);
            } else {
                target = (91,);
            }
        }
        if (throw_on_cleanup) {
            throw "CLEANUP", "rejected append temporary";
        }
    }
}

auto sub make_value(string kind) {
    if (kind == "hash-owner") {
        return {"probe": new Probe()};
    }
    if (kind == "closure-owner") {
        Probe held();
        return auto sub () { return held; };
    }
    if (kind == "reference-owner") {
        Probe held();
        return \held;
    }
    if (kind == "partial-merge") {
        return (17, new Probe(), 19);
    }
    return new Probe();
}

sub check(auto expected, auto actual, string label) {
    if (expected !== actual) {
        throw "TEST-FAIL", sprintf("%s: expected %y, got %y", label, expected, actual);
    }
}

sub reject_with_handlers() {
    # No callee locals or catch: IR must release the rejected operand before unwinding this frame.
    on_exit {
        push observations, ("on-exit", target.size());
    }
    on_error {
        push observations, ("on-error", $1.err, $1.next.err);
    }
    target += make_value("probe");
}

sub run(string which) {
    scenario = which;
    observations = ();
    write_on_cleanup = which == "write" || which == "cow" || which == "nested-hash"
        || which == "object-member";
    throw_on_cleanup = which == "throw" || which == "handlers-throw";
    target = (3,);
    list<int> initial = (3,);
    nested = {"values": initial};
    box = new Box();
    box.values = (3,);
    list<int> alias = which == "cow" ? target : ();
    *hash<ExceptionInfo> caught;
    try {
        if (which == "handlers" || which == "handlers-throw") {
            reject_with_handlers();
        } else if (which == "nested-hash") {
            nested.values += make_value(which);
        } else if (which == "object-member") {
            box.values += make_value(which);
        } else {
            target += make_value(which);
        }
    } catch (hash<ExceptionInfo> e) {
        caught = e;
    }
    check("RUNTIME-TYPE-ERROR", caught.err, "append type error");
    check(throw_on_cleanup ? "CLEANUP" : NOTHING, caught.next.err, "cleanup exception chain");
    check(NOTHING, caught.next.next, "no extra exception");
    list<int> expected = which == "partial-merge" ? (3, 17) : (3,);
    list<auto> expected_observations = ();
    push expected_observations, expected;
    if (which == "handlers" || which == "handlers-throw") {
        expected_observations += (("on-error", "RUNTIME-TYPE-ERROR", throw_on_cleanup ? "CLEANUP" : NOTHING),
            ("on-exit", 1));
    }
    check(expected_observations, observations, "destructor runs once before callee handlers");
    check(write_on_cleanup ? (91,) : expected, read_target(), "destructor write is preserved");
    if (which == "cow") {
        check((3,), alias, "shared original is unchanged");
    }
    printf("PASS %s\n", which);
}

sub controls() {
    target = (3,);
    observations = ();
    scenario = "accepted";
    write_on_cleanup = False;
    throw_on_cleanup = False;
    list<Probe> accepted;
    accepted += make_value("probe");
    check(1, accepted.size(), "accepted value is appended");
    check((), observations, "successful append transfers ownership");
    accepted = ();
    check(1, observations.size(), "accepted value is released once");
    list<softfloat> converted = (1.0,);
    auto value = 7;
    converted += value;
    check((1.0, 7.0), converted, "successful element conversion");
    list<int> values = (1,);
    list<int> result = (values += 2);
    check((1, 2), values, "successful scalar append");
    check(values, result, "consumed expression result");
    values += values;
    check((1, 2, 1, 2), values, "self merge");
    printf("PASS controls\n");
}

if (ARGV) {
    ARGV[0] == "controls" ? controls() : run(ARGV[0]);
} else {
    foreach string which in ("read", "write", "throw", "cow", "hash-owner", "closure-owner",
            "reference-owner", "nested-hash", "object-member", "partial-merge", "handlers", "handlers-throw") {
        run(which);
    }
    controls();
}
