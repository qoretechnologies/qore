#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# runs a scenario of primary-thread-stack.qtest in the primary thread of its own process and prints its results as
# "<name>: <value>" lines, ending with "STATUS: done"
#
# Each scenario sets the stack of the primary thread as a runtime that enforces the thread's stack size, such as a
# JVM, does (see cpp_api_enforce_primary_thread_stack()).  A wrong stack limit leaves the primary thread unable to
# run any more code, so each scenario runs in a process of its own.
#
# usage: primary-thread-stack-worker.q <scenario> <args...>
#   sizes <max> <protected> [<max> <protected> ...]   sets each maximum and protected stack size in turn and prints
#                                                      "SIZE <n>" with the stack size of the primary thread after
#                                                      the nth
#   limit <max> <protected>                           prints the stack size and "TO-LIMIT": the distance from where
#                                                      Qore started managing the stack to the stack limit
#   recursion <max> <protected> <use>                 recurses until the thread has used <use> more bytes of stack
#                                                      and prints "REMAINING": the stack remaining at that point
#   unbounded <max>                                   recurses without bound with <max> enforced, and prints the
#                                                      "ERROR" that it raised and the "DEEPEST" stack use
#   parse <max> <protected> <depth> <mode>            parses and runs code nested <depth> deep of each kind in a
#                                                      Program with the given execution mode and prints its
#                                                      "RESULT <kind>"

%modern
%requires cppapiuser

#! the recursion depth at which a recursion stops if its calls use no stack, so that it cannot run for ever
const MaxDepth = 1000000;

#! the execution modes by name
const ExecModes = {
    "ast": EM_AST,
    "ir": EM_IR,
    "jit": EM_JIT,
    "tiered": EM_TIERED,
};

#! recurses until the thread has used the given amount of stack; returns the stack remaining at that point
/** @return the stack remaining when the thread has used \a target bytes of stack, or -1 if the recursion reached
    MaxDepth
*/
int sub recurse(int target, int depth) {
    if (cpp_api_thread_stack_used() >= target) {
        return cpp_api_thread_stack_remaining();
    }
    if (depth == MaxDepth) {
        return -1;
    }
    return recurse(target, depth + 1);
}

#! recurses until the stack limit is reached, recording the stack used at each step
/** The stack used is recorded in a member: a reference passed down the recursion would be a chain of references as
    deep as the recursion, which each assignment follows through native recursion
*/
class UnboundedRecursion {
    public {
        #! the stack used at the deepest step of the recursion
        int deepest = 0;
    }

    #! recurses until the stack limit is reached
    /** @throw RECURSION-DEPTH-ERROR the recursion reached MaxDepth without reaching the stack limit
    */
    recurse(int depth) {
        deepest = cpp_api_thread_stack_used();
        if (depth == MaxDepth) {
            throw "RECURSION-DEPTH-ERROR", sprintf("the recursion reached depth %d without reaching the stack limit",
                depth);
        }
        recurse(depth + 1);
    }
}

#! returns the body of a function that returns 1 from code nested to the given depth
string sub get_nested_body(string kind, int depth) {
    switch (kind) {
        case "call": return "return " + strmul("id(", depth) + "1" + strmul(")", depth) + ";";
        case "paren": return "return " + strmul("(", depth) + "1" + strmul(")", depth) + ";";
        case "block": return strmul("{ ", depth) + "return 1;" + strmul(" }", depth);
    }
    throw "WORKER-ERROR", sprintf("unknown kind of nesting %y", kind);
}

#! returns the integer argument at the given position
int sub get_arg(int i) {
    *string arg = ARGV[i];
    if (!exists arg || arg !~ /^-?[0-9]+$/) {
        throw "WORKER-ERROR", sprintf("argument %d: expecting an integer; got %y", i + 1, arg);
    }
    return arg.toInt();
}

*string scenario = shift ARGV;
printf("TID: %d\n", gettid());
printf("INITIAL: %d\n", get_stack_size());
switch (scenario) {
    case "sizes": {
        if (!ARGV || (ARGV.size() % 2)) {
            throw "WORKER-ERROR", "expecting pairs of maximum and protected sizes";
        }
        for (int i = 0; i < ARGV.size(); i += 2) {
            cpp_api_enforce_primary_thread_stack(get_arg(i), get_arg(i + 1));
            printf("SIZE %d: %d\n", i / 2, get_stack_size());
        }
        break;
    }
    case "limit": {
        cpp_api_enforce_primary_thread_stack(get_arg(0), get_arg(1));
        int used = cpp_api_thread_stack_used();
        int remaining = cpp_api_thread_stack_remaining();
        printf("SIZE: %d\n", get_stack_size());
        printf("TO-LIMIT: %d\n", used + remaining);
        break;
    }
    case "recursion": {
        cpp_api_enforce_primary_thread_stack(get_arg(0), get_arg(1));
        int target = cpp_api_thread_stack_used() + get_arg(2);
        printf("REMAINING: %d\n", recurse(target, 0));
        break;
    }
    case "unbounded": {
        cpp_api_enforce_primary_thread_stack(get_arg(0), 0);
        UnboundedRecursion rec();
        string err = "none";
        try {
            rec.recurse(0);
        } catch (hash<ExceptionInfo> ex) {
            err = ex.err;
        }
        printf("ERROR: %s\n", err);
        printf("DEEPEST: %d\n", rec.deepest);
        break;
    }
    case "parse": {
        cpp_api_enforce_primary_thread_stack(get_arg(0), get_arg(1));
        int depth = get_arg(2);
        *ExecMode mode = ExecModes{ARGV[3]};
        if (!exists mode) {
            throw "WORKER-ERROR", sprintf("unknown execution mode %y", ARGV[3]);
        }
        foreach string kind in (("call", "paren", "block")) {
            Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
            p.setExecMode(mode);
            p.parse("auto sub id(auto x) { return x; }\nauto sub run() { " + get_nested_body(kind, depth) + " }\n",
                kind);
            printf("RESULT %s: %y\n", kind, p.callFunction("run"));
        }
        break;
    }
    default:
        throw "WORKER-ERROR", sprintf("unknown scenario %y", scenario);
}
printf("STATUS: done\n");
