# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The cases of the InlineHandlerFlowLoop-<n>.qtest tests (see addPartCase()): the execution modes, and qcc executables
# compiled with -O0 and with -O3; together they run about 1,000 child processes one after another, which took more
# than the time limit of run_tests.sh under the load of a parallel run in one test (qore#7254)

#! a handler declared in the body of a handler that runs inline at an exit of a loop compiles with qcc (qore#6905)
/** The debug IR of a qcc executable has the paths of a debugger's break, continue and return requests at the step
    events of the statements of a handler's body; the paths of a handler declared in the body of a handler that the
    lowering runs inline at a return or a throw out of a loop took the loop, which the exit had left, for the target of
    a break or a continue, and qcc aborted on an assertion in QoreIRLowering::emitBlockCleanups() (an assertion-free
    build lowered the cleanups of blocks that the exit had already left).  A break or a continue that no loop or switch
    statement around the handler takes is a return, as in the AST interpreter.

    handler-flow.q gives the output of the AST interpreter in every execution mode and in qcc executables compiled with
    -O0 and -O3, without a debugger and with a debugger's request at a step event of a statement of such a handler.
    A request in a handler that a return runs ends the handler, and the return goes on with its value, also from a
    handler of a handler's body (qore#6949).  The paths after a block do not run the handlers of the block and of the
    blocks in it again, which have run (qore#6955).  A break or a continue request in a handler of a block of the body
    of a handler that is not the last one of its exit ends that handler, whose request the next handler's normal end
    replaces (qore#7040).  A return request in a handler that is not the last one runs the next handlers and ends the
    function (qore#6999).
*/
class InlineHandlerFlowLoopBase inherits QUnit::Test {
    public {
        #! the functions of handler-flow.q
        const Functions = ("loopThrow", "loopReturn", "switchBreak", "nestedLoops", "noLoop", "deepReturn", "nestIf",
            "nestLoop", "deepIf", "loopIf", "midReturn", "tryReturn", "tryNested", "switchContinue", "tryExitBreak",
            "tryExitContinue", "tryCutReturn", "loopHandlerTop", "scopeMidHandler", "scopeMid", "scopeMidLoop",
            "tryHandlerTry", "tryNestedHandlers", "nonLast", "nonLastLoop", "nonLastSwitch", "nonLastInnerLoop",
            "nonLastDeep", "nonLastInner", "nonLastTry", "nonLastSuccess", "retNonLast", "retLast", "retLastLoop",
            "retNonLastLoop", "retDeep", "retDeepLast",
            "twoHandlersLoop", "twoHandlers", "twoHandlersInner", "twoHandlersInnerReturn", "twoHandlersMid",
            "twoHandlersTry", "twoHandlersLaterReturn", "threeHandlers", "mixedHandlers",
            "mixedHandlersLoop", "mixedInHandler", "abandonedReturn", "abandonedObject", "outerPendingReturn", "eagerOuter",
            "innerLoopPending", "unchangedObject", "nrcLost", "dtorThrow", "abandonTiming", "abandonedNested",
            "abandonedNestedTry", "outerPendingTry", "dtorThrowLoop", "endsFunctionObject", "onErrorAfterExit",
            "tryInHandler");

        #! Expected return values and destructor order when a debugger request abandons a return (#7022).
        const AbandonedResults = {
            "abandonedReturn": "RESULT:null LOG:[]",
            "abandonedObject": "RESULT:3 LOG:[\"released\", \"after\"]",
            "innerLoopPending": "RESULT:-7 LOG:[\"after\"]",
            "unchangedObject": "RESULT:3 LOG:[\"released\"]",
            "outerPendingReturn": "RESULT:-7 LOG:[\"last\"]",
            "outerPendingReturn/string": "RESULT:restored LOG:[\"last\"]",
            "outerPendingReturn/list": "RESULT:restored LOG:[\"last\"]",
            "outerPendingReturn/hash": "RESULT:restored LOG:[\"last\"]",
            "outerPendingReturn/number": "RESULT:restored LOG:[\"last\"]",
            "outerPendingReturn/date": "RESULT:restored LOG:[\"last\"]",
            "outerPendingReturn/binary": "RESULT:restored LOG:[\"last\"]",
            "outerPendingReturn/object": "RESULT:null LOG:[\"released\", \"last\"]",
            "outerPendingReturn/object-list": "RESULT:null LOG:[\"released\", \"last\"]",
            "outerPendingReturn/object-opaque": "RESULT:null LOG:[\"released\", \"last\"]",
            # the return of an earlier handler resumes when a later handler takes the request
            "nrcLost": "RESULT:3 LOG:[\"last\"]",
            # the release of the abandoned object throws, and the next handler runs (bug 380)
            "dtorThrow": "RESULT:MARKER-ERROR: the destructor threw LOG:[\"throw\", \"last\"]",
            # the abandoned object is released when the request abandons its return, before the handler of the
            # enclosing block
            "abandonTiming": "RESULT:-7 LOG:[\"released\", \"between\", \"last\"]",
            "abandonedNested": "RESULT:null LOG:[\"outer\", \"inner\"]",
            "abandonedNestedTry": "RESULT:null LOG:[\"outer\", \"inner\"]",
            "abandonedNestedHit1": "RESULT:null LOG:[]",
            "dtorThrowLoop": "RESULT:MARKER-ERROR: the destructor threw LOG:[\"throw\"]",
            # a request that ends the function leaves the return, which goes on with its object (#6949)
            "endsFunctionObject": "RESULT:{<ReturnMarker object>: {}} LOG:[\"outer\", \"released\"]",
            # the on_exit handler is the last that runs: the on_error handler after it does not run at a normal exit
            "onErrorAfterExit": "RESULT:{<ReturnMarker object>: {}} LOG:[\"released\"]",
            # the request passes the try statement in the handler, and a later handler takes it
            "tryInHandler": "RESULT:-7 LOG:[\"released\", \"last\"]",
            "outerPendingTry": "RESULT:-7 LOG:[\"last\"]",
            "outerPendingTry/string": "RESULT:restored LOG:[\"last\"]",
            "outerPendingTry/object": "RESULT:null LOG:[\"released\", \"last\"]",
        };

        #! the function, the request, the tag of its statement and the hit of its step event, and optionally a second
        #! request, tag and hit
        const Requests = (
            ("abandonedReturn", "continue", "abandoned_step", 1, "return", "abandoned_step", 2),
            ("abandonedObject", "break", "abandoned_object", 1),
            ("nrcLost", "break", "nrc_mid", 1),
            ("dtorThrow", "break", "dtor_throw", 1),
            ("abandonTiming", "break", "abandon_timing", 1),
            ("abandonedNested", "continue", "abandoned_nested_inner", 1, "return", "abandoned_nested_outer", 2),
            ("abandonedNestedTry", "continue", "abandoned_try_inner", 1, "return", "abandoned_try_outer", 2),
            ("abandonedNestedHit1", "continue", "abandoned_nested_inner", 1, "return", "abandoned_nested_outer", 1),
            # (abandonedNestedTryHit1, the same requests with a try statement in the inner handler: a qcc executable
            # does not take the second request, in the outer handler on the path of the first one from the try
            # statement; a second request during the handlers of another request is qore#7008, which adds the case)
            ("dtorThrowLoop", "break", "dtor_throw_loop", 1),
            ("endsFunctionObject", "break", "ends_function_object", 1),
            ("onErrorAfterExit", "break", "on_error_after_exit", 1),
            ("tryInHandler", "break", "try_in_handler", 1),
            ("outerPendingTry", "break", "outer_pending_try", 1),
            ("outerPendingTry/string", "break", "outer_pending_try", 1),
            ("outerPendingTry/object", "break", "outer_pending_try", 1),
            ("innerLoopPending", "break", "inner_loop_pending", 1),
            ("innerLoopPending", "continue", "inner_loop_pending", 1),
            ("unchangedObject", "break", "keep_marker", 1),
            ("outerPendingReturn", "break", "outer_pending", 1),
            ("outerPendingReturn/string", "break", "outer_pending", 1),
            ("outerPendingReturn/list", "break", "outer_pending", 1),
            ("outerPendingReturn/hash", "break", "outer_pending", 1),
            ("outerPendingReturn/number", "break", "outer_pending", 1),
            ("outerPendingReturn/date", "break", "outer_pending", 1),
            ("outerPendingReturn/binary", "break", "outer_pending", 1),
            ("outerPendingReturn/object", "break", "outer_pending", 1),
            ("outerPendingReturn/object-list", "break", "outer_pending", 1),
            ("outerPendingReturn/object-opaque", "break", "outer_pending", 1),
            ("outerPendingReturn", "continue", "outer_pending", 1),
            ("loopThrow", "break", "throw_inner", 1),
            ("loopThrow", "continue", "throw_inner", 1),
            ("loopThrow", "return", "throw_inner", 1),
            ("loopThrow", "break", "throw_loop_inner", 2),
            ("loopThrow", "continue", "throw_loop_inner", 2),
            ("loopThrow", "return", "throw_loop_inner", 2),
            ("loopReturn", "continue", "return_loop_inner", 2),
            ("switchBreak", "break", "switch_case_inner", 1),
            # a loop around the handler is a target, and the inner loop that the return left is not
            ("nestedLoops", "break", "nested_inner", 2),
            ("nestedLoops", "continue", "nested_inner", 2),
            # requests in a handler that a return runs end the handler, and the return goes on with its value
            # (qore#6949: qcc executables returned no value; a build of qcc without assertions also ran handlers again
            # and resumed the loop that the return had left, qore#6905)
            ("loopReturn", "break", "return_inner", 1),
            ("switchBreak", "continue", "switch_inner", 1),
            ("nestedLoops", "return", "nested_inner", 2),
            ("noLoop", "break", "noloop_handler", 1),
            ("noLoop", "continue", "noloop_handler", 1),
            ("noLoop", "return", "noloop_handler", 1),
            ("deepReturn", "break", "deep_inner", 1),
            ("deepReturn", "return", "deep_inner", 1),
            # handlers of blocks of the body of such a handler; a break and a continue that a loop of the body takes
            # still jump
            ("nestIf", "break", "nest_if", 1),
            ("nestIf", "return", "nest_if", 1),
            ("nestLoop", "break", "nest_loop", 1),
            ("nestLoop", "continue", "nest_loop", 1),
            ("nestLoop", "return", "nest_loop", 1),
            ("deepIf", "return", "deep_if", 1),
            ("loopIf", "return", "loop_if", 2),
            # two requests: in the innermost handler, then in a handler that the code of the first lowers inline
            ("midReturn", "return", "mid_inner", 1, "break", "mid_handler", 1),
            ("midReturn", "return", "mid_inner", 1, "continue", "mid_handler", 1),
            ("midReturn", "return", "mid_inner", 1, "return", "mid_handler", 1),
            # in a try statement of the handler's body: a return request ends the try statement with its own value,
            # a break and a continue request go on with the return
            ("tryReturn", "return", "try_handler", 1),
            ("tryReturn", "break", "try_handler", 1),
            ("tryReturn", "continue", "try_handler", 1),
            ("tryNested", "return", "try_nested", 1),
            ("tryNested", "break", "try_nested", 1),
            # a switch statement of the body takes a break; a continue goes on with the return
            ("switchContinue", "break", "switch_continue", 1),
            ("switchContinue", "continue", "switch_continue", 1),
            # a handler that runs after the try statement that its block's exit left
            ("tryExitBreak", "return", "try_exit_break", 1),
            ("tryExitContinue", "return", "try_exit_continue", 2),
            # a handler lowered inline in the paths after a block left from a try statement (qore#6949); a handler
            # that can return ran again in those paths (qore#6955)
            ("tryCutReturn", "continue", "try_cut_inner", 1, "return", "try_cut_handler", 1),
            ("tryCutReturn", "continue", "try_cut_inner", 1),
            ("tryCutReturn", "continue", "try_cut_inner", 2),
            # the paths after a block run neither its handlers nor those of the blocks in it, which have run
            # (qore#6955: qcc executables ran them again)
            ("loopHandlerTop", "return", "loop_top_body", 1),
            ("loopHandlerTop", "return", "loop_top_body", 2),
            ("scopeMidHandler", "break", "scope_mid_handler_inner", 1),
            ("scopeMidHandler", "continue", "scope_mid_handler_inner", 1),
            ("scopeMidHandler", "return", "scope_mid_handler_inner", 1),
            ("scopeMidHandler", "return", "scope_mid_handler", 1, "return", "scope_mid_handler_inner", 1),
            ("scopeMid", "break", "scope_mid_inner", 1),
            ("scopeMid", "continue", "scope_mid_inner", 1),
            ("scopeMid", "return", "scope_mid_inner", 1),
            ("scopeMid", "continue", "scope_mid", 1, "return", "scope_mid_inner", 1),
            ("scopeMidLoop", "break", "scope_mid_loop_inner", 1),
            ("scopeMidLoop", "continue", "scope_mid_loop_inner", 1),
            ("scopeMidLoop", "continue", "scope_mid_loop_inner", 2),
            ("scopeMidLoop", "return", "scope_mid_loop_inner", 1),
            ("scopeMidLoop", "break", "scope_mid_loop", 1, "continue", "scope_mid_loop_inner", 1),
            ("tryHandlerTry", "break", "try_handler_try", 1),
            ("tryHandlerTry", "continue", "try_handler_try", 1),
            ("tryHandlerTry", "return", "try_handler_try", 1),
            ("tryHandlerTry", "return", "try_handler_try_inner", 1),
            ("tryHandlerTry", "continue", "try_handler_try_inner", 1, "return", "try_handler_try", 1),
            ("tryNestedHandlers", "return", "try_nested_handlers", 1),
            # a break or a continue request in a handler of a block of the body of a handler that is not the last of
            # its exit ends that handler, and the next one's normal end replaces it; a return request goes on
            # (qore#7040: qcc executables ended the function, or took a loop or a switch statement around the block)
            ("nonLast", "break", "non_last", 1),
            ("nonLast", "continue", "non_last", 1),
            ("nonLast", "return", "non_last", 1),
            ("nonLastLoop", "break", "non_last_loop", 1),
            ("nonLastLoop", "continue", "non_last_loop", 1),
            ("nonLastLoop", "break", "non_last_loop", 2),
            ("nonLastLoop", "return", "non_last_loop", 1),
            ("nonLastSwitch", "break", "non_last_switch", 1),
            ("nonLastSwitch", "continue", "non_last_switch", 1),
            # a loop of the handler's body takes the request
            ("nonLastInnerLoop", "break", "non_last_inner_loop", 1),
            ("nonLastInnerLoop", "continue", "non_last_inner_loop", 1),
            ("nonLastDeep", "break", "non_last_deep", 1),
            ("nonLastDeep", "continue", "non_last_deep", 2),
            ("nonLastDeep", "return", "non_last_deep", 1),
            ("nonLastInner", "break", "non_last_inner", 1),
            ("nonLastInner", "continue", "non_last_inner", 1),
            ("nonLastInner", "break", "non_last_inner", 2),
            ("nonLastTry", "break", "non_last_try", 1),
            ("nonLastTry", "continue", "non_last_try", 1),
            ("nonLastSuccess", "break", "non_last_success", 1),
            ("nonLastSuccess", "continue", "non_last_success", 1),
            # in the cleanups of a return statement of the handler's body: the request abandons the return unless it
            # ends the function, which then returns the return's value (qore#7040)
            ("retNonLast", "break", "ret_non_last", 1),
            ("retNonLast", "continue", "ret_non_last", 1),
            ("retLast", "break", "ret_last", 1),
            ("retLast", "continue", "ret_last", 1),
            ("retLastLoop", "break", "ret_last_loop", 1),
            ("retLastLoop", "continue", "ret_last_loop", 1),
            ("retNonLastLoop", "break", "ret_non_last_loop", 1),
            ("retNonLastLoop", "continue", "ret_non_last_loop", 1),
            ("retDeep", "break", "ret_deep", 1),
            ("retDeep", "continue", "ret_deep", 1),
            ("retDeepLast", "break", "ret_deep_last", 1),
            ("retDeepLast", "continue", "ret_deep_last", 1),
            # a handler that is not the last of its exit: the next handlers run, and keep a return request and replace
            # a break or a continue request (qore#6999)
            ("twoHandlersLoop", "return", "two_handlers_loop_a", 1),
            ("twoHandlersLoop", "break", "two_handlers_loop_a", 1),
            ("twoHandlersLoop", "continue", "two_handlers_loop_a", 1),
            ("twoHandlers", "return", "two_handlers_a", 1),
            ("twoHandlers", "break", "two_handlers_a", 1),
            ("twoHandlers", "continue", "two_handlers_a", 1),
            ("twoHandlersInner", "return", "two_handlers_inner_a", 1),
            ("twoHandlersInner", "break", "two_handlers_inner_a", 1),
            ("twoHandlersInner", "continue", "two_handlers_inner_a", 1),
            ("twoHandlersInnerReturn", "return", "two_handlers_inner_return_a", 1),
            ("twoHandlersInnerReturn", "break", "two_handlers_inner_return_a", 1),
            ("twoHandlersInnerReturn", "continue", "two_handlers_inner_return_a", 1),
            ("twoHandlersInnerReturn", "return", "two_handlers_inner_return_b", 1),
            ("twoHandlersInnerReturn", "break", "two_handlers_inner_return_b", 1),
            ("twoHandlersInnerReturn", "continue", "two_handlers_inner_return_b", 1),
            ("twoHandlersMid", "return", "two_handlers_mid_a", 1),
            ("twoHandlersMid", "break", "two_handlers_mid_a", 1),
            ("twoHandlersMid", "continue", "two_handlers_mid_a", 1),
            ("twoHandlersMid", "return", "two_handlers_mid_b", 1),
            ("twoHandlersMid", "break", "two_handlers_mid_b", 1),
            ("twoHandlersMid", "continue", "two_handlers_mid_b", 1),
            ("twoHandlersMid", "return", "two_handlers_mid_c", 1),
            ("twoHandlersMid", "break", "two_handlers_mid_c", 1),
            ("twoHandlersMid", "continue", "two_handlers_mid_c", 1),
            ("twoHandlersTry", "return", "two_handlers_try_a", 1),
            ("twoHandlersTry", "break", "two_handlers_try_a", 1),
            ("twoHandlersTry", "continue", "two_handlers_try_a", 1),
            ("twoHandlersLaterReturn", "return", "two_handlers_later_return_a", 1),
            ("twoHandlersLaterReturn", "break", "two_handlers_later_return_a", 1),
            ("twoHandlersLaterReturn", "continue", "two_handlers_later_return_a", 1),
            ("twoHandlersLaterReturn", "return", "two_handlers_later_return_b", 1),
            ("twoHandlersLaterReturn", "break", "two_handlers_later_return_b", 1),
            ("twoHandlersLaterReturn", "continue", "two_handlers_later_return_b", 1),
            ("threeHandlers", "return", "three_handlers_a", 1),
            ("threeHandlers", "break", "three_handlers_a", 1),
            ("threeHandlers", "continue", "three_handlers_a", 1),
            ("mixedHandlers", "return", "mixed_handlers", 1),
            ("mixedHandlers", "break", "mixed_handlers", 1),
            ("mixedHandlers", "continue", "mixed_handlers", 1),
            ("mixedHandlersLoop", "return", "mixed_handlers_loop_b", 1),
            ("mixedHandlersLoop", "return", "mixed_handlers_loop_b", 2),
            ("mixedHandlersLoop", "break", "mixed_handlers_loop_b", 1),
            ("mixedHandlersLoop", "break", "mixed_handlers_loop_b", 2),
            ("mixedHandlersLoop", "continue", "mixed_handlers_loop_b", 1),
            ("mixedHandlersLoop", "continue", "mixed_handlers_loop_b", 2),
            ("mixedInHandler", "return", "mixed_in_handler", 1),
            ("mixedInHandler", "break", "mixed_in_handler", 1),
            ("mixedInHandler", "continue", "mixed_in_handler", 1),
            ("mixedHandlersLoop", "break", "mixed_handlers_loop_x", 1, "return", "mixed_handlers_loop_b", 1),
        );
    }

    private {
        string qore_bin;
        string libqore_dir;
        string qcc_bin;
        string script;
        #! the lines of the tagged statements of the script
        hash<string, int> tags = {};
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = TestInterpreter::qcc_compiler() ?? "qcc";
        script = get_script_dir() + "/handler-flow.q";
        list<string> lines = ReadOnlyFile::readTextFile(script).split("\n");
        for (int i = 0; i < lines.size(); ++i) {
            *list<*string> tag = lines[i] =~ x/### ([a-z_]+)$/;
            if (tag) {
                tags{tag[0]} = i + 1;
            }
        }
        # each part runs about a third of the child processes, which run one after another (qore#7254)
        addPartCase(1, "the handlers run alike in every execution mode", \testModes());
        addPartCase(2, "qcc -O0 compiles the handlers, which run as with the AST interpreter", \testQcc(), 0);
        addPartCase(3, "qcc -O3 compiles the handlers, which run as with the AST interpreter", \testQcc(), 3);
        set_return_value(main());
    }

    #! adds a case if it is in the part that this test runs: each case is in one part, and each part is a test
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    testModes() {
        string env = sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s", libqore_dir, libqore_dir);
        foreach string f in (Functions) {
            string expected = ExecModeHarness::run(sprintf("%s %s -b --exec-mode=ast %s %s", env, qore_bin, script,
                f));
            assertRegex("^RESULT:", expected, f + ": AST");
            if (f == "eagerOuter") {
                assertEq("RESULT:3 LOG:[\"released\", \"cleanup\", \"last\"]", expected.split(" STEPS:")[0], f);
            }
            foreach string mode in (ExecModeHarness::ExecModes) {
                if (mode == "ast") {
                    continue;
                }
                assertEq(expected, ExecModeHarness::run(sprintf("%s %s -b --exec-mode=%s %s %s", env, qore_bin, mode,
                    script, f)), sprintf("%s: %s", f, mode));
            }
            assertEq(expected, ExecModeHarness::run_ir_tier(sprintf("%s %s -b --exec-mode=tiered %s %s", env,
                qore_bin, script, f)), f + ": IR tier");
        }
        # A source program with a debugger attached must give the same specific result in each execution mode.
        foreach list<auto> r in (Requests) {
            if (AbandonedResults.hasKey(r[0])) {
                string args = requestArgs(r);
                foreach string mode in (ExecModeHarness::ExecModes) {
                    string out = ExecModeHarness::run(sprintf("%s %s -b --exec-mode=%s %s %s", env, qore_bin,
                        mode, script, args));
                    assertEq(AbandonedResults{r[0]}, out.split(" STEPS:")[0], sprintf("%s: %s", args, mode));
                }
            }
        }
    }

    private string requestArgs(list<auto> r) {
        string args = sprintf("%s %s %d %d", r[0], r[1], tags{r[2]}, r[3]);
        if (r.size() > 4) {
            args += sprintf(" %s %d %d", r[4], tags{r[5]}, r[6]);
        }
        return args;
    }

    testQcc(int opt) {
        TmpDir dir("qore-inline-handler-flow-loop-");
        string env = sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s", libqore_dir, libqore_dir);
        {
            string exe = join_paths(dir.path, sprintf("handler-flow-O%d", opt));
            int rc;
            string out = backquote(sprintf("%s %s -O%d -o %s %s 2>&1", env, qcc_bin, opt, exe, script), \rc);
            # the compile aborted on an assertion (qore#6905)
            assertEq(0, rc, sprintf("qcc -O%d: exit status (output: %s)", opt, out.substr(-2000)));
            if (!is_file(exe)) {
                return;
            }
            # without a debugger
            foreach string f in (Functions) {
                string expected = backquote(sprintf("%s %s -b --exec-mode=ast %s %s 2>&1", env, qore_bin, script, f));
                assertEq(expected, backquote(sprintf("%s %s -b %s 2>&1", env, exe, f)), sprintf("qcc -O%d: %s", opt,
                    f));
            }
            # with a debugger's request in a handler: the AST interpreter is the reference
            foreach list<auto> r in (Requests) {
                string args = requestArgs(r);
                string expected = backquote(sprintf("%s %s -b --exec-mode=ast %s %s 2>&1", env, qore_bin, script,
                    args));
                assertRegex("^RESULT:", expected, args + ": AST");
                assertEq(expected, backquote(sprintf("%s %s -b %s 2>&1", env, exe, args)), sprintf("qcc -O%d: %s", opt,
                    args));
            }
        }
    }
}
