# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The cases of the HandlerFunctionRequest-<n>.qtest tests (see addPartCase()): the functions of a group, in a qcc
# executable compiled with -O0 or -O3, each compared with the AST interpreter

#! a debugger's request in a handler compiled as a function of its own (qore#7625)
/** A handler that runs out of line is a function of its own, whose return statements hand their values over to the
    function that it runs for when they execute (QoreIROpcode::HandlerReturn): an on_error handler, and a handler of a
    block in the body of a handler past the budget of the handlers that the lowering runs inline.  A break or a continue
    requested in the handler of a nested block of the handler, after the nested block's return, abandons that return
    when the function takes the request: the AST interpreter releases its value at once, when the handlers of the
    nested block have run, before its locals and the blocks around it are left
    (StatementBlock::requestTakenInFunction()): a handler that is not the last of its exit (always at an error exit)
    takes it, and so does a loop or a switch statement around the nested block.  A qcc executable kept the value handed
    over until its function ended.  A return request, and a request that nothing takes, go on with the nested return.

    Each function is called with an argument that makes the err* ones raise an exception; each nested return gives an
    object (InnerMarker, whose destructor logs its release) or a string.  The test requires the results of a qcc
    executable to be those of the AST interpreter.
*/
class HandlerFunctionRequestBase inherits QUnit::Test {
    public {
        #! the number of extra handlers that make the handlers after them run out of line
        const ExtraHandlers = 67;

        #! the nesting of handlers past the nesting limit of the handlers that the lowering runs inline
        const DeepHandlers = 17;

        #! the kinds of the value of the nested return: the suffix of the function name, and the expression
        const Kinds = {
            "Object": "new InnerMarker()",
            "Data": "sprintf(\"inner value %d\", n)",
        };

        const Flows = ("none", "break", "continue", "return");

        #! the functions of each group: the name, the body (%NESTED% is the nested block, %EXTRA% the extra handlers)
        const Groups = (
            (
                ("errObject", "    on_exit { push log, \"last\"; }\n    on_error {\n%NESTED%    }\n%THROW%"),
                # a local of a block of the handler around the nested block: released after the nested return
                ("errLocal", "    on_exit { push log, \"last\"; }\n    on_error {\n        OuterMarker o();\n"
                    "%NESTED%    }\n%THROW%"),
                # the nested block's handlers run out of line past the budget, in the handler's function
                ("errPastCap", "    on_exit { push log, \"last\"; }\n    on_error {\n%NESTED_EXTRA%    }\n%THROW%"),
                ("errFor", "    on_exit { push log, \"last\"; }\n    on_error {\n"
                    "        for (int j = 0; j < 1; ++j) {\n%NESTED%        }\n        push log, \"after loop\";\n"
                    "    }\n%THROW%"),
                # a block with a local and a handler between the nested block and the loop
                ("errForLocal", "    on_exit { push log, \"last\"; }\n    on_error {\n"
                    "        for (int j = 0; j < 1; ++j) {\n            OuterMarker o();\n"
                    "            on_exit { push log, \"mid\"; }\n%NESTED%        }\n        push log, \"after loop\";\n"
                    "    }\n%THROW%"),
                ("errWhile", "    on_exit { push log, \"last\"; }\n    on_error {\n        int j = 0;\n"
                    "        while (j++ < 1) {\n%NESTED%        }\n        push log, \"after loop\";\n    }\n%THROW%"),
                ("errDo", "    on_exit { push log, \"last\"; }\n    on_error {\n        int j = 0;\n        do {\n"
                    "%NESTED%        } while (++j < 1);\n        push log, \"after loop\";\n    }\n%THROW%"),
                ("errForeach", "    on_exit { push log, \"last\"; }\n    on_error {\n"
                    "        foreach int j in ((1,)) {\n%NESTED%        }\n        push log, \"after loop\";\n"
                    "    }\n%THROW%"),
            ),
            (
                ("errForeachRef", "    on_exit { push log, \"last\"; }\n    on_error {\n        list<int> l = (1,);\n"
                    "        foreach int j in (\\l) {\n%NESTED%        }\n        push log, \"after loop\";\n"
                    "    }\n%THROW%"),
                ("errContext", "    on_exit { push log, \"last\"; }\n    on_error {\n"
                    "        context ({\"a\": (1,)}) {\n%NESTED%        }\n        push log, \"after loop\";\n"
                    "    }\n%THROW%"),
                # a loop over a lazy pipeline (the fused lowering)
                ("errPipeline", "    on_exit { push log, \"last\"; }\n    on_error {\n"
                    "        foreach int j in (select (1, 2), $1 > 1) {\n%NESTED%        }\n"
                    "        push log, \"after loop\";\n    }\n%THROW%"),
                ("errSwitch", "    on_exit { push log, \"last\"; }\n    on_error {\n        switch (n) {\n"
                    "        case 3: {\n%NESTED%        }\n        }\n        push log, \"after switch\";\n"
                    "    }\n%THROW%"),
                # the inner loop takes the request
                ("errNested", "    on_exit { push log, \"last\"; }\n    on_error {\n"
                    "        for (int i = 0; i < 1; ++i) {\n            for (int j = 0; j < 1; ++j) {\n%NESTED%"
                    "            }\n            push log, \"after inner loop\";\n        }\n"
                    "        push log, \"after loop\";\n    }\n%THROW%"),
                # a continue: the next iteration hands its value over again
                ("errAgain", "    on_exit { push log, \"last\"; }\n    on_error {\n"
                    "        for (int j = 0; j < 2; ++j) {\n%NESTED%        }\n        push log, \"after loop\";\n"
                    "    }\n%THROW%"),
                # a try statement of the handler around the nested block
                ("errTry", "    on_exit { push log, \"last\"; }\n    on_error {\n        try {\n%NESTED%"
                    "        } catch (hash<ExceptionInfo> ex) {\n            push log, \"caught\";\n        }\n"
                    "        push log, \"after try\";\n    }\n%THROW%"),
                # a try statement of the function catches the exception
                ("errCaughtLoop", "    on_exit { push log, \"last\"; }\n    try {\n        on_error {\n"
                    "        for (int j = 0; j < 1; ++j) {\n%NESTED%        }\n        push log, \"after loop\";\n"
                    "        }\n        if (n) { throw \"ERR\", \"err\"; }\n"
                    "    } catch (hash<ExceptionInfo> ex) {\n        push log, \"caught\";\n    }\n"
                    "    return n - 10;\n"),
                # a handler of a block of a handler past the budget, in the cleanups of the function's return: the
                # last handler of its exit, one that is not, and one whose loop takes the request
                ("retLast", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n            on_exit {\n"
                    "%NESTED%            }\n        }\n    }\n%EXTRA%    return n - 10;\n"),
                ("retNotLast", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
                    "            on_exit { push log, \"next\"; }\n            on_exit {\n%NESTED%            }\n"
                    "        }\n    }\n%EXTRA%    return n - 10;\n"),
                ("retLoop", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n            on_exit {\n"
                    "                for (int j = 0; j < 1; ++j) {\n%NESTED%                }\n"
                    "                push log, \"after loop\";\n            }\n        }\n    }\n%EXTRA%"
                    "    return n - 10;\n"),
            ),
            (
                # the last handler of its exit at a normal exit, where nothing takes the request: the nested return
                # goes on with its value
                ("retNoLast", "    on_exit {\n        {\n            on_exit {\n%NESTED%            }\n        }\n"
                    "    }\n%EXTRA%    return n - 10;\n"),
                # the function's own pending return is an object, which the nested return replaces
                ("retNotLastPending", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
                    "            on_exit { push log, \"next\"; }\n            on_exit {\n%NESTED%            }\n"
                    "        }\n    }\n%EXTRA%    return new OuterMarker();\n"),
                # a loop of the function around the block that declares the handler past the budget: at the normal
                # end of the loop's body, and at an error exit
                ("retParentLoop", "    for (int j = 0; j < 1; ++j) {\n        on_exit {\n            {\n"
                    "                on_exit {\n%NESTED%                }\n            }\n        }\n%EXTRA%"
                    "        push log, \"body\";\n    }\n    push log, \"after loop\";\n    return n - 10;\n"),
                ("errParentLoop", "    on_exit { push log, \"last\"; }\n    for (int j = 0; j < 1; ++j) {\n"
                    "        try {\n            on_error {\n%NESTED%            }\n"
                    "            if (n) { throw \"ERR\", \"err\"; }\n"
                    "        } catch (hash<ExceptionInfo> ex) {\n            push log, \"caught\";\n        }\n"
                    "    }\n    push log, \"after loop\";\n    return n - 10;\n"),
                # a handler past the budget in the body of a handler past the budget (a run that forwards the values
                # of its handlers), at a normal exit and at an error exit
                ("retForwarded", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
                    "            on_exit {\n                {\n                    on_exit {\n"
                    "                        {\n                            on_exit {\n%NESTED%"
                    "                            }\n                        }\n                    }\n"
                    "%EXTRA_IN%                }\n            }\n        }\n    }\n%EXTRA%    return n - 10;\n"),
                ("errForwarded", "    on_exit { push log, \"last\"; }\n    on_error {\n        {\n"
                    "            on_exit {\n                {\n                    on_exit {\n%NESTED%"
                    "                    }\n                }\n            }\n%EXTRA_IN%        }\n    }\n%THROW%"),
                # a handler of a block that the request leaves returns: its return replaces the request
                ("errHandOverAfter", "    on_exit { push log, \"last\"; }\n    on_error {\n        {\n"
                    "            on_exit { return new OuterMarker(); }\n%NESTED%        }\n    }\n%THROW%"),
                # the nested return in a try statement of the nested block, which hands it over when it ends; with a
                # local of the block around the nested block, and with a loop of the handler's body around it
                ("errTryReturn", "    on_exit { push log, \"last\"; }\n    on_error {\n        OuterMarker o();\n"
                    "%NESTED_TRY%    }\n%THROW%"),
                ("errTryReturnLoop", "    on_exit { push log, \"last\"; }\n    on_error {\n"
                    "        for (int j = 0; j < 1; ++j) {\n            OuterMarker o();\n%NESTED_TRY%        }\n"
                    "        push log, \"after loop\";\n    }\n%THROW%"),
            ),
            (
                # a handler past the budget at the normal end of a block in the body of a handler compiled as a
                # function of its own: the request of its handler is that of the block, which ends the handler's body
                # (qore#7753)
                ("errPastCapNormal", "    on_exit { push log, \"last\"; }\n    on_error {\n        {\n"
                    "            on_exit {\n                {\n                    on_exit {\n%NESTED%"
                    "                    }\n                }\n            }\n%EXTRA_IN%        }\n"
                    "        push log, \"after\";\n    }\n%THROW%"),
                # ... and a loop of the handler's body takes it
                ("errPastCapNormalLoop", "    on_exit { push log, \"last\"; }\n    on_error {\n"
                    "        for (int j = 0; j < 1; ++j) {\n            {\n                on_exit {\n"
                    "                    {\n                        on_exit {\n%NESTED%                        }\n"
                    "                    }\n                }\n%EXTRA_IN%            }\n"
                    "            push log, \"body\";\n        }\n        push log, \"after loop\";\n    }\n%THROW%"),
                # a request in a handler past the budget in the cleanups of a return in a try statement of a
                # handler, whose value replaces a value that a handler past the nesting limit handed over to the
                # return of the function (try_replaced_handed_over, qore#7753)
                ("retTryHandOver", "    on_exit { push log, \"last\"; }\n    on_exit {\n        try {\n"
                    "            {\n                on_exit {\n"
                    "                    push log, \"inner\";      ### %TAG%\n"
                    "                }\n%EXTRA_IN%                return %VALUE%;\n            }\n"
                    "        } catch (hash<ExceptionInfo> ex) {\n            push log, \"caught\";\n        }\n    }\n"
                    "%DEEP%    return n - 10;\n"),
                # a handler that hands a value over runs before the handler whose nested return a request abandons:
                # the next handler takes the request, and the value handed over before resumes (qore#7753)
                ("retRestore", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
                    "            on_exit { push log, \"next\"; }\n            on_exit {\n%NESTED%            }\n"
                    "            on_exit { return \"x value\"; }\n        }\n    }\n%EXTRA%    return n - 10;\n"),
                ("retRestoreObject", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
                    "            on_exit { push log, \"next\"; }\n            on_exit {\n%NESTED%            }\n"
                    "            on_exit { return new OuterMarker(); }\n        }\n    }\n%EXTRA%    return n - 10;\n"),
                # ... the handler's value comes from a handler of a block of its body, and the request is in the
                # handler's own handler, which runs past the budget in the cleanups of that return (qore#7753)
                ("retRestoreHandlerCap", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
                    "            on_exit { push log, \"next\"; }\n            on_exit {\n"
                    "                on_exit {\n                    push log, \"inner\";      ### %TAG%\n"
                    "                }\n                {\n                    on_exit { return %VALUE%; }\n"
                    "%EXTRA_IN%                }\n            }\n"
                    "            on_exit { return \"x value\"; }\n        }\n    }\n%EXTRA%    return n - 10;\n"),
                # ... the handler's value comes from a handler past the budget in its body, a run that forwards to the
                # handler's run and whose first hand-over is that of the handler's run (qore#7753)
                ("retRestoreForwarded", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
                    "            on_exit { push log, \"next\"; }\n            on_exit {\n"
                    "                on_exit {\n                    push log, \"inner\";      ### %TAG%\n"
                    "                }\n                {\n                    on_exit {\n"
                    "                        {\n                            on_exit { return %VALUE%; }\n"
                    "                        }\n                    }\n%EXTRA_IN%                }\n            }\n"
                    "            on_exit { return \"x value\"; }\n        }\n    }\n%EXTRA%    return n - 10;\n"),
                # ... a loop of the handler's body takes the request (the release of the code of the request)
                ("retRestoreLoop", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
                    "            on_exit { push log, \"next\"; }\n            on_exit {\n"
                    "                for (int j = 0; j < 1; ++j) {\n%NESTED%                }\n"
                    "                push log, \"after loop\";\n            }\n"
                    "            on_exit { return \"x value\"; }\n        }\n    }\n%EXTRA%    return n - 10;\n"),
                # ... in the cleanups of a return whose value is an integer, which the first hand-over does not
                # release: an earlier object handed over leaves no value, as with the AST interpreter (qore#7753)
                ("retRestoreReturn", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
                    "            on_exit { push log, \"next\"; }\n            on_exit {\n%NESTED%            }\n"
                    "            on_exit { return \"x value\"; }\n%EXTRA_IN%            return n - 10;\n        }\n"
                    "    }\n    return 5;\n"),
                ("retRestoreReturnObject", "    on_exit { push log, \"last\"; }\n    on_exit {\n        {\n"
                    "            on_exit { push log, \"next\"; }\n            on_exit {\n%NESTED%            }\n"
                    "            on_exit { return new OuterMarker(); }\n%EXTRA_IN%            return n - 10;\n"
                    "        }\n    }\n    return 5;\n"),
            ),
        );

        #! the results of the AST interpreter of the issue (qore#7625)
        const IssueResults = {
            "errObjectObject break": "RESULT:ERR: err LOG:[\"inner released\", \"last\"]",
            "errObjectObject return": "RESULT:ERR: err LOG:[\"last\", \"inner released\"]",
            "errObjectObject none": "RESULT:ERR: err LOG:[\"inner\", \"last\", \"inner released\"]",
            "errLocalObject break": "RESULT:ERR: err LOG:[\"inner released\", \"outer released\", \"last\"]",
            "errPastCapObject break": "RESULT:ERR: err LOG:[\"inner released\", \"last\"]",
            "errForObject break": "RESULT:ERR: err LOG:[\"inner released\", \"after loop\", \"last\"]",
            "errForLocalObject break":
                "RESULT:ERR: err LOG:[\"inner released\", \"mid\", \"outer released\", \"after loop\", \"last\"]",
            "errSwitchObject break": "RESULT:ERR: err LOG:[\"inner released\", \"after switch\", \"last\"]",
            "errCaughtLoopObject break": "RESULT:-7 LOG:[\"inner released\", \"after loop\", \"caught\", \"last\"]",
            # nothing takes the request: the nested return goes on with its value
            "retNoLastObject break": "RESULT:{<InnerMarker object>: {}} LOG:[\"inner released\"]",
            # the loop of the function takes the request
            "retParentLoopObject break": "RESULT:-7 LOG:[\"body\", \"inner released\", \"after loop\"]",
            "retParentLoopObject return": "RESULT:{<InnerMarker object>: {}} LOG:[\"body\", \"inner released\"]",
            # the return of a handler of a block that the request leaves replaces the request
            "errHandOverAfterObject break": "RESULT:ERR: err LOG:[\"inner released\", \"last\", \"outer released\"]",
            # a handler past the budget at a normal exit in a handler body: its request ends the handler's body, or a
            # loop of the body takes it (qore#7753)
            "errPastCapNormalObject break": "RESULT:ERR: err LOG:[\"inner released\", \"last\"]",
            "errPastCapNormalLoopObject break":
                "RESULT:ERR: err LOG:[\"inner released\", \"after loop\", \"last\"]",
            # the value that an earlier handler handed over resumes when the next handler takes the request (qore#7753)
            "retRestoreData break": "RESULT:\"x value\" LOG:[\"next\", \"last\"]",
            "retRestoreObject break": "RESULT:\"x value\" LOG:[\"inner released\", \"next\", \"last\"]",
            # ... an object that it handed over was released when the later return replaced it (qore#7022)
            "retRestoreObjectObject break":
                "RESULT:null LOG:[\"outer released\", \"inner released\", \"next\", \"last\"]",
            "retTryHandOverData break": "RESULT:\"x value\" LOG:[\"last\"]",
            # ... from a run that forwards to the handler's run, and through a loop of the handler's body
            "retRestoreForwardedData break": "RESULT:\"x value\" LOG:[\"next\", \"last\"]",
            "retRestoreLoopData break": "RESULT:\"x value\" LOG:[\"after loop\", \"next\", \"last\"]",
            # an object handed over before leaves no value, also where the return's own value is an integer
            "retRestoreReturnObjectData break": "RESULT:null LOG:[\"outer released\", \"next\", \"last\"]",
            # the return of a try statement of the nested block is released before the local of the block around it
            "errTryReturnObject break": "RESULT:ERR: err LOG:[\"inner released\", \"outer released\", \"last\"]",
        };
    }

    private {
        string qore_bin;
        string libqore_dir;
        string qcc_bin;
        string env;
        int part;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        qore_bin = TestInterpreter::qore_interpreter();
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = TestInterpreter::qcc_compiler() ?? "qcc";
        env = sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s", libqore_dir, libqore_dir);
        addPartCase(1, "requests in handlers compiled as functions of their own (group 1), qcc -O0",
            \testQcc(), (0, 0));
        addPartCase(2, "requests in handlers compiled as functions of their own (group 1), qcc -O3",
            \testQcc(), (0, 3));
        addPartCase(3, "requests in handlers compiled as functions of their own (group 2), qcc -O0",
            \testQcc(), (1, 0));
        addPartCase(4, "requests in handlers compiled as functions of their own (group 2), qcc -O3",
            \testQcc(), (1, 3));
        addPartCase(5, "requests in handlers compiled as functions of their own (group 3), qcc -O0",
            \testQcc(), (2, 0));
        addPartCase(6, "requests in handlers compiled as functions of their own (group 3), qcc -O3",
            \testQcc(), (2, 3));
        addPartCase(7, "requests in handlers compiled as functions of their own (group 4), qcc -O0",
            \testQcc(), (3, 0));
        addPartCase(8, "requests in handlers compiled as functions of their own (group 4), qcc -O3",
            \testQcc(), (3, 3));
        set_return_value(main());
    }

    #! adds a test case that runs only in the given part
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! returns the source of the functions of a group; the tags of their requests are added to \a tags
    static string functions(int group, reference<hash<string, string>> tags) {
        string src = "class InnerMarker {\n    destructor() { push log, \"inner released\"; }\n}\n"
            "class OuterMarker {\n    destructor() { push log, \"outer released\"; }\n}\n";
        string extra = strmul("    on_exit { n += 0; }\n", ExtraHandlers);
        string nested_extra = strmul("            on_exit { n += 0; }\n", ExtraHandlers);
        string extra_in = strmul("                    on_exit { n += 0; }\n", ExtraHandlers);
        # handlers nested past the nesting limit of the handlers lowered inline, the innermost of which returns
        string deep = strmul("    on_exit {\n", DeepHandlers) + "        return \"x value\";\n"
            + strmul("    }\n", DeepHandlers);
        string throw_tail = "    if (n) { throw \"ERR\", \"err\"; }\n    return n - 10;\n";
        foreach list<auto> f in (Groups[group]) {
            foreach hash<auto> k in (Kinds.pairIterator()) {
                string name = f[0] + k.key;
                string tag = "req_" + name.lwr();
                tags{name} = tag;
                string nested = "        {\n            on_exit {\n                push log, \"inner\";      ### " + tag
                    + "\n            }\n%MORE%            return " + k.value + ";\n        }\n";
                string nested_try = "        {\n            on_exit {\n                push log, \"inner\";      ### "
                    + tag + "\n            }\n            try {\n                return " + k.value + ";\n"
                    "            } catch (hash<ExceptionInfo> ex) {\n                push log, \"caught\";\n"
                    "            }\n        }\n";
                string body = f[1];
                body = replace(body, "%NESTED_TRY%", nested_try);
                body = replace(body, "%NESTED_EXTRA%", replace(nested, "%MORE%", nested_extra));
                body = replace(body, "%NESTED%", replace(nested, "%MORE%", ""));
                body = replace(body, "%DEEP%", deep);
                body = replace(body, "%EXTRA_IN%", extra_in);
                body = replace(body, "%EXTRA%", extra);
                body = replace(body, "%THROW%", throw_tail);
                body = replace(body, "%TAG%", tag);
                body = replace(body, "%VALUE%", k.value);
                src += sprintf("auto sub %s(int n) {\n%s}\n", name, body);
            }
        }
        return src;
    }

    #! writes the program of a group, and returns its path and the requests: "<function> <flow>" -> the arguments
    private hash<auto> program(string dir, int group) {
        hash<string, string> tags_of = {};
        string functions = HandlerFunctionRequestBase::functions(group, \tags_of);
        string src = ReadOnlyFile::readTextFile(get_script_dir() + "/handler-function-requests.q.in");
        src = replace(src, "%%FUNCTIONS%%", functions);
        string path = join_paths(dir, sprintf("handler-function-requests-%d.q", group));
        File f();
        f.open2(path, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(src);
        f.close();
        hash<string, int> lines = {};
        list<string> l = src.split("\n");
        for (int i = 0; i < l.size(); ++i) {
            if (*string tag = (l[i] =~ x/### (\w+)$/)[0]) {
                lines{tag} = i + 1;
            }
        }
        hash<string, string> requests = {};
        list<string> names = keys tags_of;
        foreach hash<auto> fn in (tags_of.pairIterator()) {
            assertTrue(exists lines{fn.value}, "the tag of " + fn.key);
            foreach string flow in (Flows) {
                requests{sprintf("%s %s", fn.key, flow)} = flow == "none"
                    ? fn.key
                    : sprintf("%s %s %d 1", fn.key, flow, lines{fn.value});
            }
        }
        return {"path": path, "requests": requests, "names": names};
    }

    #! returns true if the IR of a function (QORE_AOT_DUMP_IR) runs a handler as a function of its own outside the
    #! code of a debugger's request: an on_error handler is registered (on.block.exit) and runs on the exception path,
    #! and a handler past the budget runs through a ScopeExit that is not lowered inline
    static bool runsHandlerOutOfLine(string dump, string name) {
        *int start = dump.find("=== IR for " + name + "(int) ===");
        if (start < 0) {
            return False;
        }
        int end = dump.find("=== IR for ", start + 1);
        string section = end < 0 ? dump.substr(start) : dump.substr(start, end - start);
        bool error_handler = name =~ /^err/;
        string label = "";
        foreach string line in (section.split("\n")) {
            if (line =~ /^[^ ].*:$/) {
                label = line;
                continue;
            }
            if (label =~ /^debug\.flow/) {
                continue;
            }
            if (error_handler
                    ? line =~ /^  on\.block\.exit/
                    : line =~ /^  scope\.exit( -> %[0-9]+)? scope=[0-9]+( hand_over_return)?( exception:.*)?$/) {
                return True;
            }
        }
        return False;
    }

    testQcc(int group, int opt) {
        TmpDir dir("qore-handler-function-requests-");
        hash<auto> prog = program(dir.path, group);
        hash<string, string> ast = {};
        foreach hash<auto> r in (prog.requests.pairIterator()) {
            ast{r.key} = ExecModeHarness::run(sprintf("%s %s -b --exec-mode=ast %s %s", env, qore_bin, prog.path,
                r.value), ExecModeHarness::NativeCheck::None).split(" STEPS:")[0];
        }
        foreach hash<auto> r in (IssueResults.pairIterator()) {
            if (exists ast{r.key}) {
                assertEq(r.value, ast{r.key}, "AST: " + r.key);
            }
        }
        string exe = join_paths(dir.path, sprintf("handler-function-requests-%d-O%d", group, opt));
        int rc;
        string out = backquote(sprintf("%s QORE_AOT_DUMP_IR=1 %s -O%d -o %s %s 2>&1", env, qcc_bin, opt, exe,
            prog.path), \rc);
        assertEq(0, rc, sprintf("qcc -O%d: %s", opt, out.substr(-2000)));
        if (rc) {
            return;
        }
        # the handlers of the cases are compiled as functions of their own, which the debug IR of the function runs
        foreach string name in (prog.names) {
            assertTrue(runsHandlerOutOfLine(out, name), "a handler of " + name + " runs out of line");
        }
        foreach hash<auto> r in (prog.requests.pairIterator()) {
            string q = ExecModeHarness::run_any(sprintf("%s %s -b %s", env, exe, r.value)).split(" STEPS:")[0];
            assertEq(ast{r.key}, q, sprintf("qcc -O%d: %s", opt, r.key));
        }
    }
}
