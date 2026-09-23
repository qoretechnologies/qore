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

if (ARGV[0] == "many-roots") {
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
            printf("%s scans=%d deferred=%d already=%d restarts=%d exclusive=%d nodes=%d\n", i.root, i.scans,
                i.deferred, i.already_scanned, i.restarts, i.exclusive_restarts, i.nodes);
        }
    }
}
