# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern

our int destroyed = 0;
class Box { public { auto payload; } }
class Leaf {
    public { *Box owner; }
    destructor() { ++destroyed; }
}

sub check(auto expected, auto actual, string label) {
    if (expected !== actual) {
        throw "TEST-FAIL", sprintf("%s: expected %y, got %y", label, expected, actual);
    }
}

int sub visits() {
    return Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_lvalue_removal_visit_count") : 0;
}
int sub probes() {
    return Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_lvalue_removal_probe_count") : 0;
}
int sub allocations() {
    return Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_lvalue_removal_allocation_count") : 0;
}
sub checkWork(string name, int start_visits, int start_probes, int visit_bound, int probe_bound) {
    int used_visits = visits() - start_visits;
    int used_probes = probes() - start_probes;
    printf("STATS %s visits=%d probes=%d\n", name, used_visits, used_probes);
    if (Option::HAVE_LIBRARY_DEBUGGING) {
        check(True, used_visits <= visit_bound, name + " linear graph visits");
        check(True, used_probes <= probe_bound, name + " linear identity probes");
    }
}

sub dag(string shape, string parent, string name) {
    const int Depth = 12;
    auto graph = new Leaf();
    for (int depth = 0; depth < Depth; ++depth) {
        if (shape == "hash" || (shape == "mixed" && depth % 2)) {
            graph = {"left": graph, "right": graph};
        } else {
            graph = (graph, graph);
        }
    }
    # Keep graph alive: only removal bookkeeping is measured, not recursive destruction.
    auto holder = parent == "hash" ? {"payload": graph} : (graph,);
    int d0 = destroyed;
    int v0 = visits();
    int p0 = probes();
    auto result;
    if (parent == "hash") {
        result = remove holder.payload;
    } else {
        result = remove holder[0];
    }
    checkWork(name, v0, p0, 4 * Depth + 12, 4 * Depth + 12);
    check(d0, destroyed, name + " retained graph alive");
    auto cursor = result;
    for (int depth = Depth - 1; depth >= 0; --depth) {
        cursor = shape == "hash" || (shape == "mixed" && depth % 2) ? cursor.left : cursor[0];
    }
    check(True, cursor instanceof Leaf, name + " result reaches original leaf");
}

Box sub makeCycle(int count) {
    Box owner();
    owner.payload = ();
    for (int i = 0; i < count; ++i) {
        Leaf leaf();
        leaf.owner = owner;
        push owner.payload, leaf;
    }
    # Leaving this frame releases its real reference and completes the deferred recursive-set scan.
    return owner;
}

sub flat(int count, bool cycle, string name, bool duplicate = False, bool repeat = False) {
    # A container holds the owner without a frame's real reference, so the removal scans its existing set.
    auto outer = {"root": cycle ? makeCycle(count) : new Box()};
    auto graph = cycle ? outer.root.payload : ();
    if (duplicate) {
        Leaf leaf();
        for (int i = 0; i < count; ++i) { push graph, leaf; }
    } else if (!cycle) {
        for (int i = 0; i < count; ++i) {
            push graph, new Leaf();
        }
    }
    if (repeat) {
        auto originals = graph;
        graph += originals;
    }
    if (!cycle) { outer.root.payload = graph; }
    int d0 = destroyed;
    int v0 = visits();
    int p0 = probes();
    int a0 = allocations();
    auto result = remove outer.root.payload;
    if (!cycle && (count <= 2 || duplicate)) {
        check(a0, allocations(), name + " no tracker allocation");
    }
    checkWork(name, v0, p0, 2 * count + 8, 16 * count + 16);
    check(count * (repeat ? 2 : 1), result.size(), name + " removed list size");
    check(d0, destroyed, name + " retained objects alive");
    check(NOTHING, outer.root.payload, name + " parent cleared");
}

sub smallHash(int count, string name) {
    auto graph = {};
    for (int i = 0; i < count; ++i) { graph{i.toString()} = new Leaf(); }
    auto holder = {"payload": graph};
    int v0 = visits();
    int p0 = probes();
    int a0 = allocations();
    int d0 = destroyed;
    auto result = remove holder.payload;
    check(a0, allocations(), name + " no tracker allocation");
    checkWork(name, v0, p0, count + 1, 2);
    check(count, result.size(), name + " size");
    check(d0, destroyed, name + " retained targets alive");
}

sub scalars() {
    list<int> graph = ();
    for (int i = 0; i < 256; ++i) { push graph, i; }
    hash<auto!> holder = {"payload": graph};
    int v0 = visits();
    int p0 = probes();
    int a0 = allocations();
    auto result = remove holder.payload;
    check(a0, allocations(), "scalars no tracker allocation");
    checkWork("scalars", v0, p0, 258, 0);
    check(256, result.size(), "scalar list size");
    check(255, result[255], "scalar list last value");
}

sub runCase(string name) {
    int d0 = destroyed;
    int expected = 1;
    switch (name) {
        case "dag-list-hash": dag("list", "hash", name); break;
        case "dag-list-list": dag("list", "list", name); break;
        case "dag-hash-hash": dag("hash", "hash", name); break;
        case "dag-hash-list": dag("hash", "list", name); break;
        case "dag-mixed-hash": dag("mixed", "hash", name); break;
        case "dag-mixed-list": dag("mixed", "list", name); break;
        case "flat-0": expected = 0; flat(expected, False, name); break;
        case "flat-1": expected = 1; flat(expected, False, name); break;
        case "flat-2": expected = 2; flat(expected, False, name); break;
        case "hash-0": expected = 0; smallHash(expected, name); break;
        case "hash-1": expected = 1; smallHash(expected, name); break;
        case "hash-2": expected = 2; smallHash(expected, name); break;
        case "flat-64": expected = 64; flat(expected, False, name); break;
        case "flat-256": expected = 256; flat(expected, False, name); break;
        case "cycle-64": expected = 64; flat(expected, True, name); break;
        case "cycle-256": expected = 256; flat(expected, True, name); break;
        case "repeat-66": expected = 66; flat(expected, False, name, False, True); break;
        case "duplicate": flat(256, False, name, True); break;
        case "scalars": expected = 0; scalars(); break;
        default: throw "TEST-FAIL", "unknown case " + name;
    }
    check(d0 + expected, destroyed, name + " all targets released");
    printf("PASS %s\n", name);
}

if (ARGV) {
    runCase(ARGV[0]);
} else {
    foreach string name in ("dag-list-hash", "dag-list-list", "dag-hash-hash", "dag-hash-list",
            "dag-mixed-hash", "dag-mixed-list", "flat-64", "flat-256", "cycle-64", "cycle-256",
            "duplicate", "scalars", "flat-0", "flat-1", "flat-2", "hash-0", "hash-1", "hash-2", "repeat-66") {
        runCase(name);
    }
}
