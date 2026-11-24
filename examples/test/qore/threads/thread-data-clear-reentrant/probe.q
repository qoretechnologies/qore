#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# destructors that run while thread-local data is cleared read, add, delete and remove its keys (qore#7265); the
# output is the same in every execution mode

%modern

# the events of the destructors, in order; the objects refer to it, as the destructors that run at the end of a
# thread do not use global variables
Queue events();

# saves a value in the thread data; a save that is refused is an event
sub save(Queue events, string key, auto value) {
    try {
        save_thread_data(key, value);
    } catch (hash<ExceptionInfo> ex) {
        events.push(sprintf("save %s: %s", key, ex.err));
    }
}

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
                save(events, "p0", new D("D1", events));
                break;
            case "D1":
                save(events, "Q", new D("noop", events));
                for (int i = 0; i < 40; ++i) {
                    save(events, "q" + i, i);
                }
                save(events, "R", new D("readQ", events));
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
                save(events, "late", 1);
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
                    save(events, "new-" + i, i);
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

# a value whose destructor saves a new value of its class to its own key (mode "same") or to a new key (mode "new")
class Resave {
    private {
        string mode;
        int n;
        Queue events;
    }

    constructor(string mode, int n, Queue events) {
        self.mode = mode;
        self.n = n;
        self.events = events;
    }

    destructor() {
        events.push(sprintf("resave %d freed", n));
        # bounded, so that a clearing that does not end shows as the bound, not as a hang: a refused save frees the new
        # value at once, and its destructor saves another one, which is refused in turn, so the chain is recursive
        if (n < 10) {
            save(events, mode == "same" ? "r" : "r" + (n + 1), new Resave(mode, n + 1, events));
        }
    }
}

# a value whose destructor saves one value to a new key (if i >= 0); counts the values freed
class Spawn {
    private {
        int i;
        Counter freed;
    }

    constructor(int i, Counter freed) {
        self.i = i;
        self.freed = freed;
    }

    destructor() {
        freed.inc();
        if (i >= 0) {
            save_thread_data("n" + i, new Spawn(-1, freed));
        }
    }
}

# a value whose destructor clears all thread data
class ClearAll {
    private {
        Queue events;
    }

    constructor(Queue events) {
        self.events = events;
    }

    destructor() {
        delete_all_thread_data();
        events.push(sprintf("clear-all: keys %y", keys get_all_thread_data()));
    }
}

# a value whose destructor deletes (mode "delete") or removes (mode "remove") the key "x"
class DeleteX {
    private {
        string mode;
        Queue events;
    }

    constructor(string mode, Queue events) {
        self.mode = mode;
        self.events = events;
    }

    destructor() {
        if (mode == "delete") {
            delete_thread_data("x");
        } else {
            remove_thread_data("x");
        }
        events.push(sprintf("%s x: keys %y", mode, keys get_all_thread_data()));
    }
}

# a value whose destructor saves a DeleteX value and then the ClearAll value "x", which the second phase frees
class SaveLate {
    private {
        string mode;
        Queue events;
    }

    constructor(string mode, Queue events) {
        self.mode = mode;
        self.events = events;
    }

    destructor() {
        save_thread_data("late", new DeleteX(mode, events));
        save_thread_data("x", new ClearAll(events));
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
# destructors that save values again and again: the clearing ends, as nothing can be saved while the values saved
# during the release are freed
foreach string how in ("direct", "pool") {
    foreach string mode in ("same", "new") {
        run(how, sub () {
            save_thread_data("r", new Resave(mode, 0, events));
        });
        show("resave " + mode + " " + how);
    }
}
# many values whose destructors each save one value: all are freed (each member is visited once in the second phase)
foreach string how in ("direct", "pool") {
    Counter freed();
    run(how, sub () {
        for (int i = 0; i < 2000; ++i) {
            save_thread_data("k" + i, new Spawn(i, freed));
        }
    });
    printf("spawn %s: freed %d\n", how, freed.getCount());
}
# a key is deleted or removed whose value's destructor clears all thread data: from plain code, and from the
# destructors of values freed in the first and in the second phase of a release
foreach string mode in ("delete", "remove") {
    # plain code
    for (int i = 0; i < 3; ++i) {
        save_thread_data("a" + i, i);
    }
    save_thread_data("x", new ClearAll(events));
    save_thread_data("z", 1);
    if (mode == "delete") {
        delete_thread_data("x");
    } else {
        remove_thread_data("x");
    }
    save_thread_data("w", 1);
    show(sprintf("%s plain (keys after: %y)", mode, keys get_all_thread_data()));
    delete_all_thread_data();
    foreach string how in ("direct", "pool") {
        # the first phase: the value of "d", freed before "x", deletes or removes "x"
        run(how, sub () {
            save_thread_data("p0", 0);
            save_thread_data("d", new DeleteX(mode, events));
            save_thread_data("x", new ClearAll(events));
            save_thread_data("p1", 1);
        });
        show(sprintf("%s phase 1 %s", mode, how));
        # the second phase: values saved in the first phase delete or remove "x"
        run(how, sub () {
            save_thread_data("s", new SaveLate(mode, events));
            save_thread_data("p0", 0);
        });
        show(sprintf("%s phase 2 %s", mode, how));
    }
}
# the thread data of this thread is empty after the clearing
printf("main keys: %y\n", keys get_all_thread_data());
