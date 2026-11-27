%modern
%disable-warning unreferenced-variable
# the code that reads the thread's $# after a call that an exception from the body of a foreach in the callee left
# sees the $# of the caller; see foreach-implicit-element-exception-exit.qtest (qore#7379)
# Copyright (C) 2026 Qore Technologies, s.r.o.

our list<string> log;

const Data = {"a": (0, 1, 2, 3, 4, 5, 6, 7, 8, 9)};
const Src = (10, 11, 12, 13, 14, 15, 16, 17);

sub thrower() {
    throw "X";
}

sub nop() {
}

int sub id(int i) {
    return i;
}

# throws from a call in the body of a foreach whose body reads $#
sub list_call_reads() {
    foreach auto a in (Src) {
        if ($# == 5) {
            thrower();
        }
    }
}

# throws from a call in the body of a foreach whose body does not read $#
sub list_call_no_read() {
    foreach auto a in (Src) {
        if (a == 15) {
            thrower();
        }
    }
}

# throws directly in a body that also makes a call
sub list_direct_with_call() {
    foreach auto a in (Src) {
        nop();
        if ($# == 5) {
            throw "X";
        }
    }
}

# throws directly in a body without a call
sub list_direct() {
    foreach auto a in (Src) {
        if ($# == 5) {
            throw "X";
        }
    }
}

# throws from the inner one of two nested loops
sub nested_call() {
    foreach auto a in (Src) {
        if ($# == 4) {
            foreach auto b in (Src) {
                if (id($#) == 6) {
                    thrower();
                }
            }
        }
    }
}

# throws from a foreach over a reference
sub ref_call() {
    list<auto> l = Src;
    foreach auto a in (\l) {
        if ($# == 5) {
            thrower();
        }
    }
}

# throws from a foreach over a lazy pipeline (the fused lowering)
sub fused_call() {
    foreach auto a in (select Src, $1 > 0) {
        if ($# == 5) {
            thrower();
        }
    }
}

# throws from a map whose expression calls a function in a foreach body
sub map_call() {
    foreach auto a in (Src) {
        if ($# == 3) {
            list<auto> l = map ($# == 5 ? thrower() : $1), Src;
        }
    }
}

# throws from a select in a foreach body
sub select_call() {
    foreach auto a in (Src) {
        if ($# == 3) {
            list<auto> l = select Src, ($# == 5 ? thrower() : True);
        }
    }
}

# throws from a hash map in a foreach body
sub hash_map_call() {
    foreach auto a in (Src) {
        if ($# == 3) {
            hash<auto> h = map {$1.toString(): $# == 5 ? thrower() : $1}, Src;
        }
    }
}

# catches the exception in the body and rethrows it
sub rethrow_in_body() {
    foreach auto a in (Src) {
        try {
            if ($# == 5) {
                thrower();
            }
        } catch (hash<ExceptionInfo> ex) {
            rethrow;
        }
    }
}

# catches the exception in the body: the loop goes on and ends normally
sub caught_in_body() {
    foreach auto a in (Src) {
        try {
            if ($# == 5) {
                thrower();
            }
        } catch () {
        }
    }
}

# catches the exception around the loop in the callee, then throws again outside of it
sub caught_around_loop() {
    try {
        foreach auto a in (Src) {
            if ($# == 5) {
                thrower();
            }
        }
    } catch () {
    }
    thrower();
}

# an on_error handler in the body, which runs on the way out
sub on_error_in_body() {
    foreach auto a in (Src) {
        on_error nop();
        if ($# == 5) {
            thrower();
        }
    }
}

# three levels of calls with foreach loops: the innermost throws
sub level3() {
    foreach auto a in (Src) {
        if ($# == 7) {
            thrower();
        }
    }
}

sub level2() {
    foreach auto a in (Src) {
        if ($# == 6) {
            level3();
        }
    }
}

sub level1() {
    foreach auto a in (Src) {
        if ($# == 5) {
            level2();
        }
    }
}

# handlers in the body read the loop's $#, directly and in a find expression, as the exception leaves the function
sub handlers_in_body() {
    foreach auto a in (Src) {
        on_error log += sprintf("handlers on_error %d %d", $#, (find %a in Data where (%a == $#)));
        on_exit log += sprintf("handlers on_exit %d", $#);
        if ($# == 5) {
            thrower();
        }
    }
}

# handlers in the bodies of nested loops
sub handlers_nested() {
    foreach auto a in (Src) {
        on_error log += sprintf("nested outer on_error %d %d", $#, (find %a in Data where (%a == $#)));
        if ($# == 2) {
            foreach auto b in (Src) {
                on_error log += sprintf("nested inner on_error %d %d", $#, (find %a in Data where (%a == $#)));
                if (id($#) == 6) {
                    thrower();
                }
            }
        }
    }
}

# a handler in the body of a loop whose map raises the exception
sub handler_map() {
    foreach auto a in (Src) {
        on_error log += sprintf("map on_error %d %d", $#, (find %a in Data where (%a == $#)));
        if ($# == 3) {
            list<auto> l = map ($# == 5 ? thrower() : $1), Src;
        }
    }
}

# a handler in a block in the body
sub handler_in_block() {
    foreach auto a in (Src) {
        if ($# == 4) {
            on_error log += sprintf("block on_error %d %d", $#, (find %a in Data where (%a == $#)));
            thrower();
        }
    }
}

# an iterator whose next() raises: the exception leaves the loop from its header
class ThrowingIterator inherits AbstractIterator {
    private {
        int i = -1;
    }

    bool next() {
        if (++i == 3) {
            throw "X";
        }
        return i < 6;
    }

    auto getValue() {
        return i;
    }

    bool valid() {
        return i >= 0 && i < 6;
    }
}

sub iterator_next_throws() {
    foreach auto a in (new ThrowingIterator()) {
        if ($# == 1) {
            nop();
        }
    }
}

# the same with a handler of the function around the loop, which reads the enclosing element
sub iterator_next_throws_handler() {
    foreach auto o in ((40, 41)) {
        on_error log += sprintf("iterator handler on_error %d %d", $#, (find %a in Data where (%a == $#)));
        if ($# == 1) {
            foreach auto a in (new ThrowingIterator()) {
                if ($# == 1) {
                    nop();
                }
            }
        }
    }
}

# the caller: the thread's $# after the exception is read by a find expression, a context statement and the catch
# block, in a loop of the caller
sub check(string label, code c) {
    list<string> out = ();
    foreach auto o in ((20, 21, 22)) {
        *int caught;
        try {
            c();
        } catch () {
            caught = (find %a in Data where (%a == $#));
        }
        int found = (find %a in Data where (%a == $#));
        list<int> ctx = ();
        context (Data) where (%a == $#) {
            ctx += %a;
        }
        out += sprintf("%d:%y:%d:%y", $#, caught, found, ctx);
    }
    log += label + " " + (foldl $1 + " " + $2, out);
}

check("list call reads", \list_call_reads());
check("list call no read", \list_call_no_read());
check("list direct with call", \list_direct_with_call());
check("list direct", \list_direct());
check("nested call", \nested_call());
check("ref call", \ref_call());
check("fused call", \fused_call());
check("map call", \map_call());
check("select call", \select_call());
check("hash map call", \hash_map_call());
check("rethrow in body", \rethrow_in_body());
check("caught in body", \caught_in_body());
check("caught around loop", \caught_around_loop());
check("on_error in body", \on_error_in_body());
check("three levels", \level1());
# a closure in the caller's loop that calls the callee
check("closure", sub () { list_call_reads(); });
check("handlers in body", \handlers_in_body());
check("handlers nested", \handlers_nested());
check("handler map", \handler_map());
check("handler in block", \handler_in_block());
check("iterator next throws", \iterator_next_throws());
check("iterator next throws handler", \iterator_next_throws_handler());

# the caller's foreach is itself in a callee of a loop: the outer caller's $# after the exception
sub outer_check() {
    foreach auto o in ((30, 31)) {
        try {
            foreach auto i in ((40, 41)) {
                list_call_reads();
            }
        } catch () {
        }
        log += sprintf("outer %d %d", $#, (find %a in Data where (%a == $#)));
    }
}
outer_check();

# normal ends of the loops of a callee: a return and a break from the body, skipped elements, an empty list and an
# unassigned typed list; the body reads its $# in a find expression, and the caller then reads its own
sub return_plain() {
    foreach auto a in (Src) {
        if ((find %a in Data where (%a == $#)) == 2) {
            return;
        }
    }
}

sub return_fused() {
    foreach auto a in (select Src, $1 % 2) {
        if ((find %a in Data where (%a == $#)) == 2) {
            return;
        }
    }
}

sub break_plain() {
    foreach auto a in (Src) {
        if ((find %a in Data where (%a == $#)) == 2) {
            break;
        }
    }
}

sub break_fused() {
    foreach auto a in (select Src, $1 % 2) {
        if ((find %a in Data where (%a == $#)) == 2) {
            break;
        }
    }
}

# the elements that the select skips take no index: the body sees 0, 1, 2, 3
sub skipped_fused() {
    list<int> seen = ();
    foreach auto a in (select Src, $1 % 2) {
        seen += (find %a in Data where (%a == $#));
    }
    log += sprintf("skipped fused %y", seen);
}

sub empty_list() {
    list<auto> l = ();
    foreach auto a in (l) {
        log += sprintf("empty %d", $#);
    }
}

sub unassigned_typed_list() {
    list<int> l;
    foreach int a in (l) {
        log += sprintf("unassigned %d", $#);
    }
}

sub ends_caller() {
    foreach auto o in ((60, 61, 62)) {
        return_plain();
        return_fused();
        break_plain();
        break_fused();
        skipped_fused();
        empty_list();
        unassigned_typed_list();
        log += sprintf("ends %d %d", $#, (find %a in Data where (%a == $#)));
    }
}
ends_caller();

# a thread_exit from the body of a foreach in a callee: the handlers of the caller's loop that it runs see the caller's
# $#
sub exit_thread() {
    foreach auto a in (Src) {
        if ($# == 5) {
            thread_exit;
        }
    }
}

sub thread_exit_caller() {
    foreach auto o in ((50, 51)) {
        on_exit log += sprintf("thread_exit on_exit %d %d", $#, (find %a in Data where (%a == $#)));
        if ($# == 1) {
            exit_thread();
        }
    }
}
{
    Counter c(1);
    background sub () {
        on_exit c.dec();
        thread_exit_caller();
    }();
    c.waitForZero();
}

# a cancellation that the check at the start of the next iteration raises: the latch has set the index of the next
# element, and the exception restores the enclosing element before the catch block runs; a loop over a list of ints
# has no exception cleanup of its own (qore#7379)
our Queue cancel_tid();
our Queue cancel_go();

class CancelPoint {
    destructor() {
        cancel_tid.push(gettid());
        cancel_go.get();
    }
}

sub cancel_fused() {
    foreach auto o in ((20, 21, 22)) {
        if ($# == 1) {
            try {
                foreach int a in (select Src, $1 > 0) {
                    if ((find %a in Data where (%a == $#)) == 3) {
                        CancelPoint c();
                    }
                }
            } catch (hash<ExceptionInfo> ex) {
                log += sprintf("cancel fused %s %d %d", ex.err, $#, (find %a in Data where (%a == $#)));
            }
        }
    }
}

sub cancel_plain() {
    foreach auto o in ((20, 21, 22)) {
        if ($# == 1) {
            try {
                foreach int a in (Src) {
                    if ((find %a in Data where (%a == $#)) == 3) {
                        CancelPoint c();
                    }
                }
            } catch (hash<ExceptionInfo> ex) {
                log += sprintf("cancel plain %s %d %d", ex.err, $#, (find %a in Data where (%a == $#)));
            }
        }
    }
}

foreach code c in ((\cancel_fused(), \cancel_plain())) {
    Counter done(1);
    background sub () {
        on_exit done.dec();
        # the cancellation is raised again in the enclosing loop
        try {
            c();
        } catch () {
        }
    }();
    cancel_thread(cancel_tid.get());
    cancel_go.push(True);
    done.waitForZero();
}

map printf("%s\n", $1), log;
