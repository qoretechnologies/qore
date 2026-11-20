# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%allow-weak-references
%disable-warning duplicate-local-vars

int sub privateOwner(int value) {
    auto alias = {"a": value};
    auto owner = alias;
    auto observer := owner;
    return observer.a;
}

int sub capturedOwner(int value) {
    auto owner = {"a": value};
    code assign = sub (int i) { owner.a = i; };
    auto observer := owner;
    assign(value + 1);
    return observer.a;
}

# The private owner and the captured owner share a frame. The closure must
# update the observed value without replacing the private owner's storage.
int sub mixedOwner(int value) {
    auto alias = {"a": value};
    auto owner = alias;
    auto seen := owner;
    auto captured = {"a": value};
    auto other := captured;
    code change = sub () { captured.a += 1; };
    change();
    return 10 * seen.a + other.a;
}

sub replaceOwner(reference r, int value) {
    r = {"a": value};
}

class Owners {
    int countControl(hash<auto> input) {
        return qore_test_local_var_stack_size() + input.a;
    }

    int countPrivate(hash<auto> input) {
        auto copy = input;
        return qore_test_local_var_stack_size() + copy.a;
    }

    int byReference(int value) {
        auto owner = {"a": value};
        replaceOwner(\owner, value + 2);
        return owner.a;
    }
}

int sub handlerOwner(int value) {
    auto owner = {"a": value};
    {
        on_exit owner.a += 3;
        auto owner = {"a": value + 10};
        auto observer := owner;
        if (observer.a != value + 10) {
            return -1;
        }
    }
    return owner.a;
}

# A regular call and a call through call_function enter through different wrappers.
int sub countControl(hash<auto> input) {
    return qore_test_local_var_stack_size() + input.a;
}

int sub countPrivate(hash<auto> input) {
    auto copy = input;
    return qore_test_local_var_stack_size() + copy.a;
}

# The capture must stay live in both frames. Adding a private copy alongside
# it should have the same stack cost as it does in a function without captures.
int sub mixedCountControl(hash<auto> input) {
    int count = 1;
    code advance = sub () { ++count; };
    advance();
    return qore_test_local_var_stack_size() + input.a + count;
}

int sub mixedCountPrivate(hash<auto> input) {
    int count = 1;
    code advance = sub () { ++count; };
    auto copy = input;
    advance();
    return qore_test_local_var_stack_size() + copy.a + count;
}

int sub recursiveControl(int depth, hash<auto> input) {
    auto observer := input;
    if (depth) {
        return 1 + recursiveControl(depth - 1, input) + observer.a;
    }
    return qore_test_local_var_stack_size() + input.a;
}

int sub recursivePrivate(int depth, hash<auto> input) {
    auto observer := input;
    auto copy = input;
    if (depth) {
        return 1 + recursivePrivate(depth - 1, input) + observer.a;
    }
    return qore_test_local_var_stack_size() + copy.a;
}

our string released = "";

class Marker {
    public { string name; }
    constructor(string n) { name = n; }
    destructor() { released += name; }
}

sub weakLifetime(bool unwind) {
    auto alias = {"node": new Marker("a")};
    auto owner = alias;
    auto observer := owner;
    alias = NOTHING;
    printf("after alias %s [%s]\n", observer.node.name, released);
    owner = {"node": new Marker("b")};
    printf("after replace %y [%s]\n", exists observer.node, released);
    {
        auto local = {"node": new Marker("c")};
        auto weak := local;
        printf("in block %s [%s]\n", weak.node.name, released);
        if (unwind) {
            throw "UNWIND";
        }
    }
    printf("after block [%s]\n", released);
}

Owners sample();
printf("%d %d %d %d %d %d\n", privateOwner(7), capturedOwner(7), mixedOwner(7),
    call_function("mixedOwner", 8), sample.byReference(7), handlerOwner(7));

if (ARGV[0] == "count") {
    hash<auto> input = {"a": 17};
    code privateRef = \countPrivate();
    code controlRef = \countControl();
    code closureControl = int sub (hash<auto> h) {
        return qore_test_local_var_stack_size() + h.a;
    };
    code closurePrivate = int sub (hash<auto> h) {
        auto copy = h;
        return qore_test_local_var_stack_size() + copy.a;
    };
    printf("%d %d %d %d %d %d %d\n", countPrivate(input) - countControl(input),
        call_function("countPrivate", input) - call_function("countControl", input),
        sample.countPrivate(input) - sample.countControl(input), privateRef(input) - controlRef(input),
        call_function("mixedCountPrivate", input) - call_function("mixedCountControl", input),
        recursivePrivate(2, input) - recursiveControl(2, input), closurePrivate(input) - closureControl(input));
}

if (ARGV[0] == "lifetimes") {
    weakLifetime(False);
    printf("after return [%s]\n", released);
    released = "";
    try {
        weakLifetime(True);
    } catch (hash<ExceptionInfo> ex) {
        printf("after unwind %s [%s]\n", ex.err, released);
    }
}
