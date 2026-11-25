%modern
%allow-debugger
%allow-opaque-references
# a handler declared in the root block of a handler that runs inline at an exit of a loop (qore#6905): with no
# debugger attached, ARGV[0] is a function to call; with a request, the debugger requests a flow at the hit-th step
# event of the statement on a line in that function (the lines of the statements tagged "### <tag>"), and optionally
# a second one (ARGV[4..6])
class FlowDebugger inherits DebugProgram {
    public {
        list<auto> steps = ();
        int flow;
        int line;
        int hit;
        int hits = 0;
        #! the second request, if any
        int flow2 = -1;
        int line2;
        int hit2;
        int hits2 = 0;
    }
    constructor(int flow, int line, int hit) {
        self.flow = flow;
        self.line = line;
        self.hit = hit;
    }
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl pgm, reference rs, reference rts) { rs = DebugDetach; }
    onStep(ProgramControl pgm, int blk, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        if (!sid) {
            return;
        }
        int l = pgm.getStatementIdInfo(sid).start_line;
        push steps, l;
        if (l == line && ++hits == hit) {
            flow = self.flow;
        }
        if (flow2 >= 0 && l == line2 && ++hits2 == hit2) {
            flow = flow2;
        }
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {}
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {}
    onExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {}
}

our list<string> log = ();

# the shape of qore#6905: a loop body leaves by an exception
auto sub loopThrow(int n) {
    on_exit {
        on_exit {
            push log, "inner";                      ### throw_inner
            push log, "inner2";
        }
        push log, "outer";
    }
    int total = 0;
    while (True) {
        on_exit {
            on_exit {
                push log, "loop-inner" + total;     ### throw_loop_inner
            }
        }
        if (++total == n) {
            throw "X", "loop";
        }
    }
}

# a return in a loop body
auto sub loopReturn(int n) {
    on_exit {
        on_exit {
            push log, "inner";                      ### return_inner
        }
        push log, "outer";
    }
    int total = 0;
    for (int i = 0; i < 10; ++i) {
        on_exit {
            on_exit {
                push log, "loop-inner" + i;         ### return_loop_inner
            }
        }
        if (++total == n) {
            return total;
        }
    }
    return -1;
}

# a return from a case of a switch statement in a loop body
auto sub switchBreak(int n) {
    on_exit {
        on_exit {
            push log, "inner";                      ### switch_inner
        }
    }
    int total = 0;
    foreach int i in (xrange(10)) {
        switch (i) {
            case 2:
                on_exit {
                    on_exit {
                        push log, "case-inner" + i; ### switch_case_inner
                    }
                }
                return i;
            default:
                ++total;
        }
    }
    return total;
}

# a handler declared in the body of an outer loop, run inline at a return out of an inner loop: a request in the
# handler of its body does not take the inner loop, which the return left, but takes the outer loop, which encloses
# the handler
auto sub nestedLoops(int n) {
    int total = 0;
    for (int o = 0; o < 3; ++o) {
        on_exit {
            on_exit {
                push log, "outer-inner" + o;        ### nested_inner
            }
            push log, "outer" + o;
        }
        for (int i = 0; i < 3; ++i) {
            if (o == 1 && i == n - 2) {
                return total;
            }
            ++total;
        }
    }
    return -1;
}

# a handler of the function's root block that a return runs (qore#6949): a request in it ends the handler, and the
# return goes on with its value
auto sub noLoop(int n) {
    on_exit {
        push log, "handler";                        ### noloop_handler
        push log, "handler2";
    }
    push log, "body";
    return n;
}

# three levels of handlers of handler bodies that a return runs: a request in the innermost ends it, and the
# requests of the handlers around it go on with the return (qore#6949)
auto sub deepReturn(int n) {
    on_exit {
        on_exit {
            on_exit {
                push log, "deep";                   ### deep_inner
            }
            push log, "middle";
        }
        push log, "outer";
    }
    push log, "body";
    return n;
}

# handlers declared in blocks of the body of a handler that a return runs (an if block, a loop body, at two levels,
# and in a loop body of the function): a request in them goes on with the return (qore#6949)
auto sub nestIf(int n) {
    on_exit {
        if (n) {
            on_exit {
                push log, "ifh";                    ### nest_if
            }
            push log, "ifbody";
        }
        push log, "outer";
    }
    return n;
}

auto sub nestLoop(int n) {
    on_exit {
        for (int k = 0; k < 2; ++k) {
            on_exit {
                push log, "lh" + k;                 ### nest_loop
            }
            push log, "lb" + k;
        }
        push log, "outer";
    }
    return n;
}

auto sub deepIf(int n) {
    on_exit {
        if (n) {
            on_exit {
                on_exit {
                    push log, "deep";               ### deep_if
                }
                push log, "middle";
            }
            push log, "if";
        }
        push log, "outer";
    }
    return n;
}

auto sub loopIf(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            if (n) {
                on_exit {
                    push log, "lif" + i;            ### loop_if
                }
            }
        }
        if (i == 1) {
            return i + 10;
        }
    }
    return -1;
}

# a handler that can return, in a block of the body of a handler that a return runs, which the code of a request in a
# handler of an inner block of the body lowers inline: a request in it goes on with the return (qore#6949)
auto sub midReturn(int n) {
    on_exit {
        {
            on_exit {
                push log, "mid";                    ### mid_handler
                if (n > 100) {
                    return -5;
                }
                push log, "mid2";
            }
            {
                on_exit {
                    push log, "in";                 ### mid_inner
                }
                push log, "x";
            }
            push log, "y";
        }
        push log, "outer";
    }
    return n;
}

# a return request in a try statement of a handler that a return runs ends the try statement, whose return value
# (none) replaces that of the return; a break or a continue request goes on with the return (qore#6949)
auto sub tryReturn(int n) {
    on_exit {
        try {
            push log, "h";                          ### try_handler
        } catch (hash<ExceptionInfo> ex) {
            push log, "c";
        }
        push log, "h2";
    }
    return n;
}

# the same in a handler of a block in a try statement of such a handler's body
auto sub tryNested(int n) {
    on_exit {
        try {
            {
                on_exit {
                    push log, "in";                 ### try_nested
                }
                push log, "x";
            }
        } catch (hash<ExceptionInfo> ex) {
            push log, "c";
        }
        push log, "h2";
    }
    return n;
}

# a handler of a case block of a switch statement in such a handler's body: a break request is taken by the switch
# statement, a continue request goes on with the return (qore#6949)
auto sub switchContinue(int n) {
    on_exit {
        switch (n) {
            case 3: {
                on_exit {
                    push log, "sw";                 ### switch_continue
                }
                push log, "c3";
                break;
            }
        }
        push log, "outer";
    }
    return n;
}

# a handler of a block in a loop of such a handler's body, whose block a break or a continue leaves from a try
# statement: the handler runs after the try statement, and a request in it goes on with the return (qore#6949)
auto sub tryExitBreak(int n) {
    on_exit {
        int i = 0;
        while (i++ < 2) {
            {
                on_exit {
                    push log, "in";                 ### try_exit_break
                }
                try {
                    push log, "x";
                    break;
                } catch (hash<ExceptionInfo> ex) {
                }
            }
        }
        push log, "after";
    }
    return n;
}

auto sub tryExitContinue(int n) {
    on_exit {
        int i = 0;
        while (i++ < 2) {
            {
                on_exit {
                    push log, "in" + i;             ### try_exit_continue
                }
                try {
                    push log, "x" + i;
                    continue;
                } catch (hash<ExceptionInfo> ex) {
                }
            }
        }
        push log, "after";
    }
    return n;
}

# a handler that can return, which the paths of a switch statement's case block in a loop of such a handler's body
# lower inline, after a break out of a try statement of that block: a return request in it goes on with the return
# (qore#6949)
auto sub tryCutReturn(int n) {
    on_exit {
        int i = 0;
        while (i++ < 2) {
            on_exit {
                if (!n) {
                    return 9;
                }
                push log, "w" + i;                  ### try_cut_handler
            }
            switch (1) {
                case 1:
                    {
                        on_exit {
                            push log, "in" + i;     ### try_cut_inner
                        }
                        try {
                            push log, "x" + i;
                            break;
                        } catch (hash<ExceptionInfo> ex) {
                        }
                    }
            }
            push log, "s" + i;
        }
        push log, "after";
    }
    return n;
}

# a handler of a loop body run at an exception out of the loop, with a handler of a foreach loop in its body: a request
# in the foreach loop's handler does not run the handlers of blocks that the paths after the loop's block have left
# (qore#6955)
auto sub loopHandlerTop(int n) {
    on_exit {
        push log, "top";
    }
    int i = 0;
    while (True) {
        on_exit {
            foreach int k in (xrange(3)) {
                on_exit {
                    push log, "lh" + k;
                }
                push log, "lb" + k;                 ### loop_top_body
            }
        }
        if (++i == n) {
            throw "X", "loopHandlerTop";
        }
    }
}

# a handler that can return of a block in a handler's body, around a block with a handler (qore#6955)
auto sub scopeMidHandler(int n) {
    on_exit {
        {
            on_exit {
                push log, "mid";                    ### scope_mid_handler
                if (n > 100) {
                    return -5;
                }
            }
            {
                on_exit {
                    push log, "in";                 ### scope_mid_handler_inner
                }
                push log, "x";
            }
            push log, "y";
        }
        push log, "outer";
    }
    push log, "body";
}

# the same in the function's body (qore#6955)
auto sub scopeMid(int n) {
    {
        on_exit {
            push log, "mid";                        ### scope_mid
            if (n > 100) {
                return -5;
            }
        }
        {
            on_exit {
                push log, "in";                     ### scope_mid_inner
            }
            push log, "x";
        }
        push log, "y";
    }
    push log, "body";
}

# the same in a loop: a break and a continue take the loop (qore#6955)
auto sub scopeMidLoop(int n) {
    for (int j = 0; j < 2; ++j) {
        {
            on_exit {
                push log, "mid";                    ### scope_mid_loop
                if (n > 100) {
                    return -5;
                }
            }
            {
                on_exit {
                    push log, "in";                 ### scope_mid_loop_inner
                }
                push log, "x";
            }
        }
        push log, "j" + j;
    }
    return -9;
}

# a handler of a try block with a try statement in its body (qore#6955)
auto sub tryHandlerTry(int n) {
    try {
        on_exit {
            push log, "h";                          ### try_handler_try
            try {
                push log, "t";                      ### try_handler_try_inner
            } catch (hash<ExceptionInfo> ex) {
            }
        }
        push log, "x";
    } catch (hash<ExceptionInfo> ex) {
    }
    return n;
}

# handlers of blocks in a try statement in a handler's body (qore#6955)
auto sub tryNestedHandlers(int n) {
    on_exit {
        try {
            {
                on_exit {
                    {
                        on_exit {
                            push log, "inner";      ### try_nested_handlers_inner
                        }
                        push log, "x3";
                    }
                    push log, "mid";                ### try_nested_handlers
                }
                push log, "x2";
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        push log, "h2";
    }
    return n;
}

# a handler of a block of the body of a handler that is not the last one that its exit runs: a break or a continue
# request that no loop or switch statement of that body takes ends that handler, and the normal end of the next handler
# replaces it (StatementBlock::execIntern()), so the code after the block goes on (qore#7040: qcc executables ended
# the function, or took a loop or a switch statement around the block)
auto sub nonLast(int n) {
    {
        on_exit {
            push log, "m1";
        }
        on_exit {
            {
                on_exit {
                    push log, "m2i";                ### non_last
                }
                push log, "m2";
            }
        }
        push log, "x";
    }
    push log, "y";
    return n;
}

# the same in a loop body: the loop does not take the request (qore#7040)
auto sub nonLastLoop(int n) {
    for (int j = 0; j < 2; ++j) {
        on_exit {
            push log, "m1" + j;
        }
        on_exit {
            {
                on_exit {
                    push log, "m2i" + j;            ### non_last_loop
                }
                push log, "m2" + j;
            }
            push log, "m2e" + j;
        }
        push log, "x" + j;
    }
    push log, "y";
    return n;
}

# the same in a case block: the switch statement does not take a break (qore#7040)
auto sub nonLastSwitch(int n) {
    switch (n) {
        case 3: {
            on_exit {
                push log, "m1";
            }
            on_exit {
                {
                    on_exit {
                        push log, "m2i";            ### non_last_switch
                    }
                    push log, "m2";
                }
            }
            push log, "x";
        }
    }
    push log, "y";
    return n;
}

# a loop of the body of the handler takes the request (qore#7040)
auto sub nonLastInnerLoop(int n) {
    {
        on_exit {
            push log, "m1";
        }
        on_exit {
            for (int k = 0; k < 2; ++k) {
                {
                    on_exit {
                        push log, "m2i" + k;        ### non_last_inner_loop
                    }
                    push log, "m2" + k;
                }
            }
            push log, "m2e";
        }
        push log, "x";
    }
    push log, "y";
    return n;
}

# three levels: the request ends the handler in the middle, which is the last of its exit, and then the outer one,
# which is not (qore#7040)
auto sub nonLastDeep(int n) {
    for (int j = 0; j < 2; ++j) {
        on_exit {
            push log, "a" + j;
        }
        on_exit {
            {
                on_exit {
                    {
                        on_exit {
                            push log, "c" + j;      ### non_last_deep
                        }
                        push log, "cb" + j;
                    }
                }
                on_exit {
                    push log, "b2" + j;
                }
                push log, "bb" + j;
            }
            push log, "be" + j;
        }
        push log, "x" + j;
    }
    push log, "y";
    return n;
}

# a handler that is not the last one in the body of the last one of a loop body's exit: its body goes on after the
# block (qore#7040)
auto sub nonLastInner(int n) {
    for (int j = 0; j < 2; ++j) {
        on_exit {
            {
                on_exit {
                    push log, "i1" + j;
                }
                on_exit {
                    {
                        on_exit {
                            push log, "i2" + j;     ### non_last_inner
                        }
                        push log, "ib" + j;
                    }
                }
                push log, "ob" + j;
            }
            push log, "oe" + j;
        }
        push log, "x" + j;
    }
    push log, "y";
    return n;
}

# a try statement of the body passes the request (qore#7040)
auto sub nonLastTry(int n) {
    for (int j = 0; j < 2; ++j) {
        on_exit {
            push log, "m1" + j;
        }
        on_exit {
            try {
                {
                    on_exit {
                        push log, "m2i" + j;        ### non_last_try
                    }
                    push log, "m2" + j;
                }
            } catch (hash<ExceptionInfo> ex) {
            }
            push log, "m2e" + j;
        }
        push log, "x" + j;
    }
    push log, "y";
    return n;
}

# an on_success handler (qore#7040)
auto sub nonLastSuccess(int n) {
    for (int j = 0; j < 2; ++j) {
        on_exit {
            push log, "m1" + j;
        }
        on_success {
            {
                on_exit {
                    push log, "m2i" + j;            ### non_last_success
                }
                push log, "m2" + j;
            }
        }
        push log, "x" + j;
    }
    push log, "y";
    return n;
}

# a return statement of the body of a handler that is not the last one of its exit runs a handler of a block of that
# body: a break or a continue request in it replaces the return, ends the handler, and the next handler's normal end
# replaces it in turn (qore#7040)
auto sub retNonLast(int n) {
    {
        on_exit {
            push log, "m1";
        }
        on_exit {
            {
                on_exit {
                    push log, "m2i";    ### ret_non_last
                }
                push log, "m2";
                if (n == 3) {
                    return -5;
                }
            }
        }
        push log, "x";
    }
    push log, "y";
    return n;
}

# the same in the last handler: the request ends the function, which returns the return's value (qore#7040)
auto sub retLast(int n) {
    {
        on_exit {
            {
                on_exit {
                    push log, "m2i";    ### ret_last
                }
                push log, "m2";
                if (n == 3) {
                    return -5;
                }
            }
        }
        push log, "x";
    }
    push log, "y";
    return n;
}

# the same in the last handler of a loop body: the loop takes the request, which abandons the return (qore#7040)
auto sub retLastLoop(int n) {
    for (int j = 0; j < 2; ++j) {
        on_exit {
            {
                on_exit {
                    push log, "m2i" + j;    ### ret_last_loop
                }
                push log, "m2" + j;
                if (n == 3) {
                    return -5;
                }
            }
        }
        push log, "x" + j;
    }
    push log, "y";
    return n;
}

# a handler that is not the last one in a loop body (qore#7040)
auto sub retNonLastLoop(int n) {
    for (int j = 0; j < 2; ++j) {
        on_exit {
            push log, "m1" + j;
        }
        on_exit {
            {
                on_exit {
                    push log, "m2i" + j;    ### ret_non_last_loop
                }
                push log, "m2" + j;
                if (n == 3) {
                    return -5;
                }
            }
        }
        push log, "x" + j;
    }
    push log, "y";
    return n;
}

# three levels, the outer handler not the last one (qore#7040)
auto sub retDeep(int n) {
    {
        on_exit {
            push log, "a";
        }
        on_exit {
            {
                on_exit {
                    {
                        on_exit {
                            push log, "c";      ### ret_deep
                        }
                        push log, "cb";
                        if (n == 3) {
                            return -5;
                        }
                    }
                }
                push log, "bb";
            }
            push log, "be";
        }
        push log, "x";
    }
    push log, "y";
    return n;
}

# three levels, the outer handler the last one (qore#7040)
auto sub retDeepLast(int n) {
    {
        on_exit {
            {
                on_exit {
                    {
                        on_exit {
                            push log, "c";      ### ret_deep_last
                        }
                        push log, "cb";
                        if (n == 3) {
                            return -5;
                        }
                    }
                }
                push log, "bb";
            }
            push log, "be";
        }
        push log, "x";
    }
    push log, "y";
    return n;
}

# a request in a handler that is not the last one of its exit: a return request is kept by the normal end of the next
# handlers, which run, and a break or a continue request is replaced by it (qore#6999: qcc executables dropped the
# return request)
auto sub twoHandlersLoop(int n) {
    for (int i = 0; i < 2; ++i) {
        on_exit {
            push log, "a" + i;
        }
        on_exit {
            push log, "b" + i;                      ### two_handlers_loop_a
        }
        push log, "body" + i;
    }
    push log, "after";
    return n;
}

auto sub twoHandlers(int n) {
    {
        on_exit {
            push log, "a";
        }
        on_exit {
            push log, "b";                          ### two_handlers_a
        }
        push log, "blk";
    }
    push log, "after";
    return n;
}

auto sub twoHandlersInner(int n) {
    {
        on_exit {
            {
                on_exit {
                    push log, "a";
                }
                on_exit {
                    push log, "b";                  ### two_handlers_inner_a
                }
            }
            push log, "outer";
        }
        push log, "blk";
    }
    push log, "after";
    return n;
}

auto sub twoHandlersInnerReturn(int n) {
    on_exit {
        {
            on_exit {
                push log, "a";                      ### two_handlers_inner_return_a
            }
            on_exit {
                push log, "b";                      ### two_handlers_inner_return_b
            }
            push log, "blk";
        }
        push log, "outer";
    }
    return n;
}

auto sub twoHandlersMid(int n) {
    on_exit {
        {
            on_exit {
                push log, "m1";                     ### two_handlers_mid_a
                if (n > 100) {
                    return -5;
                }
            }
            on_exit {
                push log, "m2";                     ### two_handlers_mid_b
                if (n > 100) {
                    return -6;
                }
            }
            {
                on_exit {
                    push log, "in";                 ### two_handlers_mid_c
                }
                push log, "x";
            }
            push log, "y";
        }
        push log, "outer";
    }
    return n;
}

# a return request in a try statement of a handler that is not the last one of the cleanups of a return statement: the
# try statement ends with its own value, and the other handler runs (qore#6999)
auto sub twoHandlersTry(int n) {
    on_exit {
        push log, "a";
    }
    on_exit {
        try {
            push log, "t";                          ### two_handlers_try_a
        } catch (hash<ExceptionInfo> ex) {
        }
        push log, "b";
    }
    return n;
}

# the handler that runs after the request can return: its return replaces the request's (qore#6999)
auto sub twoHandlersLaterReturn(int n) {
    {
        on_exit {
            push log, "a";                          ### two_handlers_later_return_b
            if (n == 3) {
                return -5;
            }
        }
        on_exit {
            push log, "b";                          ### two_handlers_later_return_a
        }
        push log, "x";
    }
    push log, "y";
    return n;
}

# three handlers: the two after the request run (qore#6999)
auto sub threeHandlers(int n) {
    for (int j = 0; j < 2; ++j) {
        on_exit {
            push log, "a" + j;
        }
        on_exit {
            push log, "b" + j;
        }
        on_exit {
            push log, "c" + j;                      ### three_handlers_a
        }
        push log, "x" + j;
    }
    push log, "y";
    return n;
}

# a block with on_exit and on_error handlers runs them at runtime: the normal end of the next handler keeps a return
# request (qore#6999)
auto sub mixedHandlers(int n) {
    {
        on_error {
            push log, "e";
        }
        on_exit {
            push log, "a";
        }
        on_exit {
            push log, "b";                          ### mixed_handlers
        }
        push log, "x";
    }
    push log, "y";
    return n;
}

# the same in a loop body, where a break request's path runs the handlers, and one of them makes a request (qore#6999)
auto sub mixedHandlersLoop(int n) {
    for (int j = 0; j < 2; ++j) {
        on_error {
            push log, "e" + j;
        }
        on_exit {
            push log, "a" + j;
        }
        on_exit {
            push log, "b" + j;                      ### mixed_handlers_loop_b
        }
        push log, "x" + j;                          ### mixed_handlers_loop_x
        push log, "w" + j;
    }
    push log, "after";
    return n;
}

# a block with on_exit and on_error handlers in the body of a handler: a request in them goes on in that handler
# (qore#6999)
auto sub mixedInHandler(int n) {
    {
        on_exit {
            {
                on_error {
                    push log, "e";
                }
                on_exit {
                    push log, "a";                  ### mixed_in_handler
                }
                push log, "x";
            }
            push log, "h_end";
        }
        push log, "blk";
    }
    push log, "after";
    return n;
}

# A second debugger return sees the pending value of its own iteration (#7022).
auto sub abandonedReturn(int n) {
    int i = 0;
    while (i++ < 2) {
        on_exit {
            push log, "h";                          ### abandoned_step
        }
        if (i == 1) {
            return n;
        }
    }
    return -1;
}

class ReturnMarker {
    destructor() {
        push log, "released";
    }
}

auto sub abandonedObject(int n) {
    for (int j = 0; j < 2; ++j) {
        on_exit {
            {
                on_exit {
                    push log, "inner";              ### abandoned_object
                }
                return new ReturnMarker();
            }
        }
    }
    push log, "after";
    return n;
}

# Construct values at run time so the debug IR has to preserve the slot of the outer return.
auto sub pendingData(string kind, int n) {
    switch (kind) {
        case "string": return sprintf("outer value %d", n);
        case "list": return (n, n + 1);
        case "hash": return {"outer": n};
        case "number": return number(n) + 0.25n;
        case "date": return date(1700000000 + n);
        case "binary": return sprintf("outer value %d", n).toBinary();
        case "object": return new ReturnMarker();
        case "object-list": return (new ReturnMarker(), n);
        case "object-opaque": {
            hash<auto> holder;
            ReturnMarker marker();
            holder.marker @= marker;
            return (holder, n);
        }
    }
    throw "TEST-ERROR", "unknown pending data kind: " + kind;
}

auto sub outerPendingReturn(int n) {
    on_exit {
        push log, "last";
    }
    on_exit {
        {
            on_exit {
                push log, "inner";                  ### outer_pending
            }
            return -5;
        }
    }
    list<string> parts = ARGV[0].split("/");
    if (parts.size() == 2) {
        return pendingData(parts[1], n);
    }
    return -7;
}

# No debugger request: an inner source return releases the former value before its exit handler (#5992).
auto sub eagerOuter(int n) {
    on_exit {
        push log, "last";
    }
    on_exit {
        while (True) {
            on_exit { push log, "cleanup"; }
            return n;
        }
    }
    return (new ReturnMarker(), n);
}

# A loop inside a handler consumes a request and resumes the handler's pending return.
auto sub innerLoopPending(int n) {
    on_exit {
        for (int i = 0; i < 1; ++i) {
            on_exit {
                push log, "inner";                  ### inner_loop_pending
            }
            return -5;
        }
        push log, "after";
    }
    return -7;
}

auto sub keepMarker() {
    on_exit {
        while (True) {
            push log, "body";                       ### keep_marker
            break;
        }
    }
    return new ReturnMarker();
}

auto sub unchangedObject(int n) {
    auto v = keepMarker();
    return v instanceof ReturnMarker ? n : -1;
}

# A handler's own return, a request in a later handler, and a last handler that completes normally: the return
# of the first handler resumes, also its request (the block does not end normally; #7022)
auto sub nrcLost(int n) {
    {
        on_exit {
            push log, "last";
        }
        on_exit {
            push log, "mid";                        ### nrc_mid
        }
        on_exit {
            return n;
        }
    }
    push log, "after";
    return -7;
}

class ThrowingMarker {
    destructor() {
        push log, "throw";
        throw "MARKER-ERROR", "the destructor threw";
    }
}

# The release of an abandoned return runs a destructor that throws: the next handler still runs (bug 380; #7022)
auto sub dtorThrow(int n) {
    on_exit {
        push log, "last";
    }
    on_exit {
        {
            on_exit {
                push log, "inner";                  ### dtor_throw
            }
            return new ThrowingMarker();
        }
    }
    return -7;
}

# A handler of an enclosing block in the handler runs after the request abandoned the return and before a later
# handler of the function takes the request: the order of the abandoned object's release (#7022)
auto sub abandonTiming(int n) {
    on_exit {
        push log, "last";
    }
    on_exit {
        {
            on_exit {
                push log, "between";
            }
            {
                on_exit {
                    push log, "inner";              ### abandon_timing
                }
                return new ReturnMarker();
            }
        }
    }
    return -7;
}

# The issue's nested block handlers: a continue at the inner handler, then a return at the outer one (#7022)
auto sub abandonedNested(int n) {
    int i = 0;
    while (i++ < 2) {
        on_exit {
            push log, "outer";                      ### abandoned_nested_outer
        }
        {
            on_exit {
                push log, "inner";                  ### abandoned_nested_inner
            }
            if (i == 1) {
                return n;
            }
        }
    }
    return -1;
}

# ... with a try statement in the inner handler
auto sub abandonedNestedTry(int n) {
    int i = 0;
    while (i++ < 2) {
        on_exit {
            push log, "outer";                      ### abandoned_try_outer
        }
        {
            on_exit {
                try {
                    push log, "inner";              ### abandoned_try_inner
                } catch (hash<ExceptionInfo> ex) {
                    push log, ex.err;
                }
            }
            if (i == 1) {
                return n;
            }
        }
    }
    return -1;
}

# The return in the handler is in a try statement, which gives its value to the block (qore_try_give_return())
auto sub outerPendingTry(int n) {
    on_exit {
        push log, "last";
    }
    on_exit {
        {
            on_exit {
                push log, "inner";                  ### outer_pending_try
            }
            try {
                return -5;
            } catch (hash<ExceptionInfo> ex) {
                push log, ex.err;
            }
        }
    }
    list<string> parts = ARGV[0].split("/");
    if (parts.size() == 2) {
        return pendingData(parts[1], n);
    }
    return -7;
}

const Flows = {"return": DebugFlowReturn, "break": DebugFlowBreak, "continue": DebugFlowContinue};
string result;
*FlowDebugger dbg;
if (ARGV.size() > 1) {
    dbg = new FlowDebugger(Flows{ARGV[1]}, ARGV[2].toInt(), ARGV[3].toInt());
    if (ARGV.size() > 4) {
        dbg.flow2 = Flows{ARGV[4]};
        dbg.line2 = ARGV[5].toInt();
        dbg.hit2 = ARGV[6].toInt();
    }
}
ProgramControl pc = ProgramControl::getProgram();
Counter done(1);
background sub () {
    on_exit done.dec();
    if (dbg) {
        dbg.addProgram(pc);
    }
    on_exit if (dbg) {
        dbg.removeProgram(pc);
    }
    try {
        list<string> parts = ARGV[0].split("/");
        auto v = call_function(parts[0], 3);
        if (parts.size() == 2 && parts[1] !~ /^object/) {
            result = v === pendingData(parts[1], 3) ? "restored" : sprintf("%y", v);
        } else {
            result = sprintf("%y", v);
        }
    } catch (hash<ExceptionInfo> ex) {
        result = sprintf("%s: %s", ex.err, ex.desc);
    }
}();
done.waitForZero();
# the steps of the functions (the lines from the first function to the top-level code); the top-level code of AOT code
# is native code without step events
list<string> src = ReadOnlyFile::readTextFile(get_script_path()).split("\n");
int first_line = 0;
int last_line = 0;
for (int i = 0; i < src.size(); ++i) {
    if (!first_line && src[i] =~ /^auto sub /) {
        first_line = i + 1;
    } else if (src[i] =~ /^const Flows /) {
        last_line = i + 1;
        break;
    }
}
if (!first_line || !last_line) {
    throw "TEST-ERROR", sprintf("the lines of the functions were not found (%d, %d)", first_line, last_line);
}
printf("RESULT:%s LOG:%y STEPS:%y\n", result, log, dbg
    ? (select dbg.steps, $1 >= first_line && $1 < last_line) : ());
