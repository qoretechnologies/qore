#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# destructors that run while thread-local data is cleared read, add, delete and remove its keys (qore#7265); the
# output is the same in every execution mode

%modern

# the events of the destructors, in order; the objects refer to it, as the destructors that run at the end of a
# thread do not use global variables
Queue events();

class D {
    private {
        string mode;
        Queue events;
    }

    constructor(string mode, Queue events) {
        self.mode = mode;
        self.events = events;
    }

    destructor() {
        switch (mode) {
            # a value saved to a key that the clearing has passed, whose destructor adds members
            case "A":
                save_thread_data("p0", new D("D1", events));
                break;
            case "D1":
                save_thread_data("Q", new D("noop", events));
                for (int i = 0; i < 40; ++i) {
                    save_thread_data("q" + i, i);
                }
                save_thread_data("R", new D("readQ", events));
                break;
            # a later destructor looks up the members that an earlier one added
            case "readQ": {
                int n = 0;
                if (exists get_thread_data("Q")) {
                    ++n;
                }
                for (int i = 0; i < 40; ++i) {
                    if (exists get_thread_data("q" + i)) {
                        ++n;
                    }
                }
                events.push(sprintf("readQ: %d", n));
                save_thread_data("late", 1);
                break;
            }
        }
    }
}

class T {
    private {
        string k;
        string mode;
        Queue events;
    }

    constructor(string k, string mode, Queue events) {
        self.k = k;
        self.mode = mode;
        self.events = events;
    }

    destructor() {
        switch (mode) {
            case "self-delete":
                delete_thread_data(k);
                break;
            case "self-remove":
                remove_thread_data(k);
                break;
            case "next-delete":
                delete_thread_data("after");
                break;
            case "all":
                delete_all_thread_data();
                break;
            case "add":
                for (int i = 0; i < 50; ++i) {
                    save_thread_data("new-" + i, i);
                }
                break;
        }
        int n = 0;
        for (int i = 0; i < 60; ++i) {
            if (exists get_thread_data("p" + i)) {
                ++n;
            }
        }
        hash<auto> all = get_all_thread_data();
        # the values of the 60 keys "p<n>" that are not freed yet, the keys and the value of the key after its own
        events.push(sprintf("%s: values %d, keys %d, after %y", mode, n, all.size(), all.after));
    }
}

# runs the code that fills the thread data in the way given, and clears the thread data there: with
# delete_all_thread_data(), or at the end of a pool task (the data of a thread that ends is freed after the thread's
# Program context, so its destructors do not use it)
sub run(string how, code fill) {
    switch (how) {
        case "direct":
            fill();
            delete_all_thread_data();
            break;
        case "pool": {
            ThreadPool pool(1, 1);
            pool.submit(fill);
            pool.submit(sub () {});
            pool.stopWait();
            break;
        }
    }
}

sub show(string label) {
    list<string> l = ();
    while (events.size()) {
        push l, events.get();
    }
    printf("%s: %y\n", label, l);
}

foreach string how in ("direct", "pool") {
    run(how, sub () {
        save_thread_data("p0", 0);
        for (int i = 1; i < 30; ++i) {
            save_thread_data("p" + i, i);
        }
        save_thread_data("a", new D("A", events));
    });
    show("phase2 " + how);
    foreach string mode in ("self-delete", "self-remove", "next-delete", "all", "add", "none") {
        run(how, sub () {
            for (int i = 0; i < 30; ++i) {
                save_thread_data("p" + i, i);
            }
            save_thread_data("obj", new T("obj", mode, events));
            save_thread_data("after", "x");
            for (int i = 30; i < 60; ++i) {
                save_thread_data("p" + i, i);
            }
        });
        show(mode + " " + how);
    }
}
# the thread data of this thread is empty after the clearing
printf("main keys: %y\n", keys get_all_thread_data());
