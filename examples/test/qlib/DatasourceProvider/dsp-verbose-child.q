#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
#
# the child process of DatasourceProvider.qtest that calls get_ds_string() with verbose output, whose stdout the test
# checks: ARGV[0] is the string; QORE_DATASOURCE_PROVIDERS names the provider modules

%modern

%requires DatasourceProvider

string str = shift ARGV;
string rv = get_ds_string(str, True);
printf("result: %s\n", rv == str ? "the string given" : "another string");
