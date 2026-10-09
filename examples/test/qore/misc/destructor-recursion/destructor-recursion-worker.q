#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# a destructor that deletes a new object of its own class recurses without end; it must raise STACK-LIMIT-EXCEEDED
# as the stack guard makes any recursion without end do, also when the deletions are deferred beyond a fixed depth

%modern

#! each object's destructor deletes a new one
class Runaway {
    destructor() {
        Runaway r();
    }
}

#! a link of a chain of objects
class Link {
    public {
        *Link next;
        *Runaway runaway;
    }
}

#! returns the error raised by deleting the object, or "none"
string sub delete_runaway() {
    try {
        Runaway r();
        delete r;
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "none";
}

#! as delete_runaway(), with the object at the end of a chain, so that its deletion is made by a deletion loop
string sub delete_chain_runaway(int length) {
    try {
        Link head();
        Link tail = head;
        for (int i = 0; i < length; ++i) {
            tail.next = new Link();
            tail = tail.next;
        }
        tail.runaway = new Runaway();
        remove tail;
        delete head;
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return "none";
}

#! runs the code in a thread with the given stack size and returns its result
string sub in_thread(code test, int stack_size) {
    int default_size = get_default_thread_stack_size();
    set_default_thread_stack_size(stack_size);
    on_exit set_default_thread_stack_size(default_size);
    Counter c(1);
    string rv;
    background sub () {
        on_exit c.dec();
        rv = test();
    }();
    c.waitForZero();
    return rv;
}

printf("direct: %s\n", delete_runaway());
printf("chain: %s\n", delete_chain_runaway(100));
printf("small stack: %s\n", in_thread(\delete_runaway(), 256 * 1024));

# the process is healthy afterwards: a long chain is still built and freed
Link head();
Link tail = head;
for (int i = 0; i < 10000; ++i) {
    tail.next = new Link();
    tail = tail.next;
}
remove tail;
remove head;
printf("done\n");
