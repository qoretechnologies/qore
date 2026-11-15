#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# changes its working directory to the directory given as its argument, then prints what QoreBuildTree finds; run by
# qore-build-tree.qtest

%modern

%requires ../../../QoreBuildTree.qm

if (chdir(ARGV[0])) {
    stderr.printf("cannot change to %y: %s\n", ARGV[0], strerror());
    exit(1);
}
printf("qore=%s\n", QoreBuildTree::qore_executable());
printf("build=%s\n", QoreBuildTree::build_dir() ?? "NOTHING");
printf("candidates=%s\n", QoreBuildTree::candidates().join(":"));
