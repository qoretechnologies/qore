# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

# The cases of the CallStackGuard-<n>.qtest tests: each runs one part of them, so that each takes a short time (all of
# them took about 115 s alone, nearly all of it in their child processes, one at a time, and more than the 300 s limit
# of run_tests.sh beside other tests: qore#6856), and together they run every case (see addPartCase()); the parts are
# balanced by the time of their cases, most of which is the native compilation of the nested expressions

#! checks that code that recurses without bound raises STACK-LIMIT-EXCEEDED on every recursion path
/** Each script recurses through one kind of call or evaluation on a thread with a small stack; see
    StackGuardCaseTest
*/
class CallStackGuardBase inherits StackGuardCaseTest {
    public {
        #! nested expressions by kind: the body of a function that evaluates one, and the value it returns
        const NestedExpressions = {
            "binary operator": ("return " + strmul("x + ", 900) + "x;", "901"),
            "ternary operator": ("bool b = x == 1; return " + strmul("b ? ", 450) + "x" + strmul(" : 0", 450) + ";",
                "1"),
            "negation": ("return " + strmul("- ", 450) + "x;", "1"),
            "logical not": ("bool b = x == 1; return (" + strmul("!", 450) + "b) ? 1 : 0;", "1"),
            "complement": ("return " + strmul("~", 450) + "x;", "1"),
            "exists": ("return " + strmul("exists ", 450) + "x;", "True"),
            "assignment": ("int y; return " + strmul("y = ", 450) + "x;", "1"),
            "null coalescing": ("*int y; return " + strmul("y ?? ", 450) + "x;", "1"),
            "function call arguments": ("return " + strmul("id(", 450) + "x" + strmul(")", 450) + ";", "1"),
            # nested less deeply than the other calls: the native compilation of nested method calls takes much
            # longer per level (about 10 s of CPU at 450 levels and 2 s at 150), which a child process that gets
            # little CPU on a loaded host cannot do within its timeout (qore#7143); the AST interpreter, which
            # evaluates them recursively, exhausts the 128 KB stack of the case that evaluates them once at about 30
            # levels, so 150 still exhaust it several times over
            "method call arguments": ("return " + strmul("o.id(", 150) + "x" + strmul(")", 150) + ";", "1"),
            "static method call arguments": ("return " + strmul("C::sid(", 450) + "x" + strmul(")", 450) + ";",
                "1"),
            "closure call arguments": ("return " + strmul("cid(", 450) + "x" + strmul(")", 450) + ";", "1"),
            "constructor arguments": ("return " + strmul("new C(", 300) + "x" + strmul(")", 300) + " ? 1 : 0;",
                "1"),
            "list literal": ("return " + strmul("(x, ", 300) + "x" + strmul(")", 300) + ".size();", "2"),
            "hash literal": ("return " + strmul("{\"a\": ", 220) + "x" + strmul("}", 220) + ".size();", "1"),
            "hashdecl initializer": ("return " + strmul("<H>{\"a\": ", 220) + "x" + strmul("}", 220) + ".size();",
                "1"),
            "cast": ("return " + strmul("cast<int>(", 450) + "x" + strmul(")", 450) + ";", "1"),
        };

        #! the nested literals and the nested calls whose native compilation is short (part 2); the other nested
        #! expressions are operators (part 1)
        const NestedLiteralsAndCalls = ("list literal", "hash literal", "hashdecl initializer",
            "function call arguments", "static method call arguments", "constructor arguments");

        #! the nested calls whose native compilation takes the longest: evaluated once (part 3) and at every depth
        #! (part 4)
        const NestedLongCalls = ("method call arguments", "closure call arguments");

        #! the cases, by name
        const Cases = {
            "function": <StackCase>{
                "setup": "sub r() { r(); }",
                "op": "r()",
            },
            "method": <StackCase>{
                "setup": "class C { m() { m(); } }",
                "op": "(new C()).m()",
            },
            "static method": <StackCase>{
                "setup": "class C { static m() { C::m(); } }",
                "op": "C::m()",
            },
            "closure": <StackCase>{
                "setup": "our code c = sub () { c(); };",
                "op": "c()",
            },
            "call reference": <StackCase>{
                "setup": "sub r() { code c = \\r(); c(); }",
                "op": "r()",
            },
            "constructor": <StackCase>{
                "setup": "class C { constructor() { C c(); } }",
                "op": "new C()",
            },
            "base class constructor": <StackCase>{
                "setup": "class B { constructor() { new D(); } } class D inherits B {}",
                "op": "new D()",
            },
            "copy method": <StackCase>{
                "setup": "class C { copy() { C c(); c.copy(); } }",
                "op": "(new C()).copy()",
            },
            "memberGate()": <StackCase>{
                # memberGate() is not called again for the object whose memberGate() is running
                "setup": "class C { auto memberGate(string m) { return get(new C(), m); } }
auto sub get(C c, string m) { return c.(m); }",
                "op": "(new C()).a",
            },
            "methodGate()": <StackCase>{
                "setup": "class C { auto methodGate(string m) { return call_object_method(self, m + \"x\"); } }",
                "op": "(new C()).a()",
            },
            "member initializer": <StackCase>{
                "setup": "class C { public { int a = f(); } } int sub f() { C c(); return 1; }",
                "op": "new C()",
            },
            "hashdecl initializer": <StackCase>{
                "setup": "hashdecl H { int a = f(); } int sub f() { hash<H> h(); return 1; }",
                "op": "f()",
            },
            "map": <StackCase>{
                "setup": "list sub r(list l) { return map r(l), l; }",
                "op": "r((1,))",
            },
            "foldl": <StackCase>{
                "setup": "auto sub r(list l) { return foldl r(l), l; }",
                "op": "r((1, 2))",
            },
            "select": <StackCase>{
                "setup": "list sub r(list l) { return select l, r(l); }",
                "op": "r((1,))",
            },
            "sort callback": <StackCase>{
                "setup": "int sub cmp(int a, int b) { sort((1, 2), \\cmp()); return a <=> b; }",
                "op": "sort((1, 2), \\cmp())",
            },
            "call_function()": <StackCase>{
                "setup": "sub r() { call_function(\\r()); }",
                "op": "r()",
            },
            "call_function_args()": <StackCase>{
                "setup": "sub r() { call_function_args(\"r\"); }",
                "op": "r()",
            },
            "iterator": <StackCase>{
                "setup": "class I inherits AbstractIterator {
    bool next() { iterate(); return False; }
    auto getValue() {}
    bool valid() { return False; }
}
sub iterate() { foreach auto x in (new I()) {} }",
                "op": "iterate()",
            },
            "try/catch": <StackCase>{
                "setup": "sub r() { try { r(); } catch (hash<ExceptionInfo> ex) { rethrow; } }",
                "op": "r()",
            },
            "on_exit": <StackCase>{
                "setup": "sub r() { on_exit {} r(); }",
                "op": "r()",
            },
            "Program::callFunction()": <StackCase>{
                "setup": "our Program p(PO_NEW_STYLE); p.parse(\"sub r() { r(); }\", \"child\");",
                "op": "p.callFunction(\"r\")",
            },
            "callback across Programs": <StackCase>{
                "setup": "our Program p(PO_NEW_STYLE); p.parse(\"sub r(code c) { c(); }\", \"child\");
our code cb = sub () { p.callFunction(\"r\", cb); };",
                "op": "cb()",
            },
            "nested Programs": <StackCase>{
                "setup": "sub r() {
    Program p(PO_NEW_STYLE);
    p.parse(\"sub f(code c) { c(); }\", \"child\");
    p.callFunction(\"f\", \\r());
}",
                "op": "r()",
                # parsing the child Program with little stack left raises the parse stack error instead (see
                # parse_check_stack())
                "error": "STACK-LIMIT-EXCEEDED|PARSE-STACK-LIMIT-EXCEEDED",
            },
            "destructor": <StackCase>{
                "setup": "class C { destructor() { C c(); } } sub del() { C c(); delete c; }",
                "op": "del()",
            },
            "function on a 96KB stack": <StackCase>{
                "setup": "sub r() { r(); }",
                "op": "r()",
                "stack_size": 96 * 1024,
            },
        };
    }

    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : StackGuardCaseTest(name) {
        self.part = part;
        # the recursion paths and the nested operators
        map addPartCase(1, $1.key, \testCase(), ($1.key, $1.value)), Cases.pairIterator();
        map addPartCase(1, $1.key, \testCase(), ($1.key, $1.value)),
            getNestedCases(NestedExpressions - (NestedLiteralsAndCalls + NestedLongCalls)).pairIterator();
        map addPartCase(2, $1.key, \testCase(), ($1.key, $1.value)),
            getNestedCases(NestedExpressions{NestedLiteralsAndCalls}).pairIterator();
        map addPartCase(3, $1.key, \testCase(), ($1.key, $1.value)),
            getNestedCases(NestedExpressions{NestedLongCalls}, False).pairIterator();
        map addPartCase(4, $1.key, \testCase(), ($1.key, $1.value)),
            getNestedCases(NestedExpressions{NestedLongCalls}, True).pairIterator();
        set_return_value(main());
    }

    #! adds the case \a name if it is in the part \a case_part that this test runs
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! returns the cases of the nested expressions given: each evaluated once, and at every depth of a recursion
    /** The nested expression cases are parsed on the main thread and evaluated on a thread with a small stack: the
        AST interpreter evaluates an expression recursively, with the nesting of the expression, which the parser
        accepts up to 1000 nested elements whatever the stack of the thread that runs the code; IR and native code
        evaluate it iteratively and return its value

        @param nested_expressions the nested expressions by kind (see NestedExpressions)
        @param at_every_depth if set, only the cases that evaluate the expressions at every depth (True) or once
        (False)
    */
    static hash<string, hash<StackCase>> getNestedCases(hash<auto> nested_expressions, *bool at_every_depth) {
        hash<string, hash<StackCase>> rv();
        foreach hash<auto> i in (nested_expressions.pairIterator()) {
            rv{"nested " + i.key} = <StackCase>{
                "setup": sprintf("class C {
    constructor(auto x) {}
    auto id(auto x) { return x; }
    static auto sid(auto x) { return x; }
}
hashdecl H { auto a; }
auto sub id(auto x) { return x; }
our code cid = auto sub (auto x) { return x; };
our C o(1);
auto sub f(int x) { %s }", i.value[0]),
                "op": "f(1)",
                "stack_size": 128 * 1024,
                "value": i.value[1],
            };
            # the expression evaluated at every depth of a recursion until the thread's stack is exhausted, so that
            # it is evaluated with as little stack left as possible
            rv{"nested " + i.key + " at every depth"} = <StackCase>{
                "setup": rv{"nested " + i.key}.setup + "
sub r() {
    try {
        f(1);
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err != \"STACK-LIMIT-EXCEEDED\") {
            rethrow;
        }
    }
    r();
}",
                "op": "r()",
                "stack_size": 512 * 1024,
            };
        }
        if (exists at_every_depth) {
            foreach string name in (keys rv) {
                if ((name =~ / at every depth$/) != at_every_depth) {
                    remove rv{name};
                }
            }
        }
        return rv;
    }
}
