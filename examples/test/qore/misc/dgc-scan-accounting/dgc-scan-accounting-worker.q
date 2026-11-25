#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# makes scans with known outcomes and prints this process's scan accounting as "name key=value..." lines; run by
# dgc-scan-accounting.qtest with and without QORE_SCAN_STATS

%modern

#! the root of a cycle held only by a list: every write to it is scanned
class AcctListRoot {
    public {
        auto x;
        auto peer;
    }
}

#! the root of a cycle held by a local variable: every write to it is deferred
class AcctLocalRoot {
    public {
        auto x;
        auto peer;
    }
}

class AcctLeaf {
}

#! the root of a cycle held only by a list that refers to a graph of leaves that cannot reach it
class AcctRegionRoot {
    public {
        auto x;
        auto peer;
        auto shared;
    }
}

sub list_writes() {
    list<AcctListRoot> l = (new AcctListRoot(),);
    l[0].peer = new AcctListRoot();
    l[0].peer.peer = l[0];
    for (int i = 0; i < 100; ++i) {
        l[0].x = new AcctLeaf();
    }
}

sub local_writes() {
    AcctLocalRoot r();
    r.peer = new AcctLocalRoot();
    r.peer.peer = r;
    for (int i = 0; i < 100; ++i) {
        r.x = new AcctLeaf();
    }
}

#! returns the accounting of the scans rooted at AcctRegionRoot
hash<ScanStatsInfo> sub region_stats() {
    return (map $1, get_scan_stats(), $1.root == "AcctRegionRoot")[0] ?? <ScanStatsInfo>{
        "root": "AcctRegionRoot",
        "scans": 0,
        "deferred": 0,
        "already_scanned": 0,
        "restarts": 0,
        "exclusive_restarts": 0,
        "nodes": 0,
        "total_ms": 0.0,
        "wait_ms": 0.0,
        "max_ms": 0.0,
        "unsealed_nodes": 0,
    };
}

#! prints the scans made by a write to a cycle's root that refers to leaves recorded in a closed region
/** The first scan enters the leaves and records them in a closed region; the second does not enter them, as the
    region is current.  Both walk the same graph, so their unsealed nodes are the same: the root and its peer, which
    are in a cycle.
*/
sub region_writes() {
    list<AcctRegionRoot> l = (new AcctRegionRoot(),);
    l[0].peer = new AcctRegionRoot();
    l[0].peer.peer = l[0];
    hash<ScanStatsInfo> s0 = region_stats();
    l[0].shared = map new AcctLeaf(), xrange(10);
    hash<ScanStatsInfo> s1 = region_stats();
    l[0].x = new AcctLeaf();
    hash<ScanStatsInfo> s2 = region_stats();
    printf("AcctRegionFirst scans=%d nodes=%d unsealed=%d\n", s1.scans - s0.scans, s1.nodes - s0.nodes,
        s1.unsealed_nodes - s0.unsealed_nodes);
    printf("AcctRegionSecond scans=%d nodes=%d unsealed=%d\n", s2.scans - s1.scans, s2.nodes - s1.nodes,
        s2.unsealed_nodes - s1.unsealed_nodes);
}

#! makes one scan rooted at each of \a n distinct classes, more than the accounting table has rows for
sub many_roots(int n) {
    string src;
    for (int i = 0; i < n; ++i) {
        src += sprintf("class AcctMany%d { public { auto x; auto peer; } }\n", i);
    }
    src += "sub scan(int n) {\n"
        "    for (int i = 0; i < n; ++i) {\n"
        "        list<object> l = (create_object(\"AcctMany\" + i),);\n"
        "        l[0].peer = l[0];\n"
        "    }\n"
        "}\n";
    Program p(PO_NEW_STYLE);
    p.parse(src, "many-roots");
    p.callFunction("scan", n);
}

if (ARGV[0] == "regions") {
    region_writes();
    printf("pid %d\n", getpid());
} else if (ARGV[0] == "many-roots") {
    many_roots(ARGV[1].toInt());
    printf("pid %d\n", getpid());
    hash<string, int> rows = {};
    foreach hash<ScanStatsInfo> i in (get_scan_stats()) {
        if (i.root =~ /^AcctMany/) {
            ++rows.named;
            rows.named_scans += i.scans;
        } else if (i.root == "<other>") {
            rows.other_scans += i.scans;
        }
        ++rows.total;
    }
    printf("AcctManyRoots named=%d named_scans=%d other_scans=%d total=%d\n", rows.named ?? 0, rows.named_scans ?? 0,
        rows.other_scans ?? 0, rows.total ?? 0);
} else {
    list_writes();
    local_writes();
    printf("pid %d\n", getpid());
    foreach hash<ScanStatsInfo> i in (get_scan_stats()) {
        if (i.root =~ /^Acct/) {
            printf("%s scans=%d deferred=%d already=%d restarts=%d exclusive=%d nodes=%d unsealed=%d\n", i.root,
                i.scans, i.deferred, i.already_scanned, i.restarts, i.exclusive_restarts, i.nodes, i.unsealed_nodes);
        }
    }
}
