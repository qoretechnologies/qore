#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# Starts and stops a watch data provider the given number of times; when polling is stopped, the polling thread is
# woken and cancelled, and the cancellation can be raised anywhere in its polling loop, including where the loop itself is
# checked for cancellation; prints "done <n>" when every provider has stopped, and
# an unhandled exception in a polling thread is printed on stderr

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires DataProvider

#! A watch data provider that signals its first poll
class StartStopProvider inherits DataProvider::AbstractWatchDataProviderBase {
    public {
        #! counts down when the first poll has run
        Counter polled(1);
    }

    constructor() : AbstractWatchDataProviderBase(1) {
    }

    private pollOnce() {
        if (polled.getCount()) {
            polled.dec();
        }
    }
}

#! An observer that ignores events
class NullObserver inherits DataProvider::Observer {
    update(string event_id, hash<auto> data_) {
    }
}

sub main() {
    int n = ARGV[0] ? ARGV[0].toInt() : 500;
    for (int i = 0; i < n; ++i) {
        StartStopProvider p();
        p.registerObserver(new NullObserver());
        p.observersReady();
        # the polling thread waits for the next poll interval when stopped
        p.polled.waitForZero();
        p.stopEvents();
    }
    printf("done %d\n", n);
}

main();
