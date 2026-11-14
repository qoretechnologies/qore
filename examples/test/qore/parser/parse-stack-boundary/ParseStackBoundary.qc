# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (c) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

#! The cases of the ParseStackBoundary-<n>.qtest tests: each runs one part of them, so that each takes a short time,
#! and together they run every case (see addPartCase()); parts 1 and 3 parse code in a single Program, part 2 loads
#! modules
#! Parsing that runs short of stack in a foreign thread fails with a parse exception at every stack size (issue #6047)
/** The fixture parses nested code in foreign threads with stack sizes from 160KB to 640KB in 4KB steps; each level of
    the code is an expression that inspects the expression it contains after parse-initializing it (a list assignment
    checks its lvalues, a cast to a hashdecl checks the keys of its hash).  Where the stack runs short, parse
    initialization stops, and the expression containing the one that it did not initialize used to inspect it as if
    it were initialized and crash; each parse must instead succeed or fail with the parse exception for code nested
    too deeply for the thread's stack, and each kind of code must do both within the range of sizes.
*/
public class ParseStackBoundaryBase inherits QUnit::Test {
    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : Test(name, "1.0") {
        self.part = part;
        addPartCase(1, "parse-time stack boundary", \check());
        addPartCase(1, "class inheritance chain", \classChainCheck());
        addPartCase(3, "class inheritance chain with methods", \classMethodChainCheck());
        addPartCase(3, "class inheritance chain with methods, most derived class first", \classMethodChainCheck(),
            True);
        addPartCase(1, "hashdecl inheritance chain", \hashdeclChainCheck());
        addPartCase(2, "nested module loads", \moduleCheck());
        addPartCase(2, "nested optional module loads", \tryModuleCheck());
        addPartCase(2, "nested optional AOT module loads", \aotModuleCheck());
        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! the number of modules in the chain of modules that require each other
    const ChainLength = 40;

    private static string quote(string value) {
        return "'" + replace(value, "'", "'\"'\"'") + "'";
    }

    #! builds the fixture in the given directory and returns the path of the executable
    private string build(string dir, string root, string libdir) {
        string exe = dir + "/parse-stack-boundary";
        list<string> args = (ENV.CXX ?? "c++", "-std=c++17", "-pthread", "-g", "-O2", "-I" + root + "/include",
            "-I" + libdir + "/include", get_script_dir() + "/parse_stack_boundary.cpp", "-L" + libdir,
            "-Wl,-rpath," + libdir, "-lqore", "-o", exe);
        *list<*string> target = (Qore::CFLAGS =~ x/(-mmacosx-version-min=\S+)/);
        if (target) {
            push args, target[0];
        }
        int status;
        string output = backquote((map quote($1), args).join(" ") + " 2>&1", \status);
        assertEq(0, status, output);
        return exe;
    }

    private check() {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("runtime built without native stack protection");
        }
        string root = realpath(get_script_dir() + "/../../../../..");
        string libdir = realpath(ENV.QORE_LIBDIR ?? root + "/build");
        TmpDir dir("qore-parse-stack-boundary-");
        string exe = build(dir.path, root, libdir);
        int status;
        string output = backquote("LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir) + " "
            + quote(exe) + " 2>&1", \status);
        assertEq(0, status, output);
        assertEq("parse-stack-boundary: ok\n", output);
    }

    #! Modules that require each other are parsed from their sources in a foreign thread with each stack size
    /** Each module is parsed while the parse that requires it waits, so the parses nest in the thread's stack with the
        parser's frames; the parse of the code that requires the first module must succeed or fail with the parse
        exception for code nested too deeply for the thread's stack, and must do both within the range of sizes.  A
        process loads a module once, so each size runs in a process of its own.
    */
    private moduleCheck() {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("runtime built without native stack protection");
        }
        string root = realpath(get_script_dir() + "/../../../../..");
        string libdir = realpath(ENV.QORE_LIBDIR ?? root + "/build");
        TmpDir dir("qore-parse-stack-modules-");
        string exe = build(dir.path, root, libdir);
        string mdir = dir.path + "/modules";
        mkdir(mdir);
        for (int i = 0; i < ChainLength; ++i) {
            File f();
            f.open2(sprintf("%s/StackChain%d.qm", mdir, i), O_WRONLY | O_CREAT | O_TRUNC);
            f.write(sprintf("%%modern\n%s"
                "module StackChain%d { version = \"1.0\"; desc = \"test\"; author = \"Qore\"; license = \"MIT\"; }\n"
                "public namespace StackChain%d {\n"
                "public bool sub matches(string s) { return s =~ /^a(b+)c[0-9]{2,}(x|y|z)*$/; }\n"
                "public hash<auto> sub make(int n) { return {\"n\": n, \"l\": (n, n + 1), \"s\": sprintf(\"%%d\", n)}; }\n"
                "}\n",
                i + 1 < ChainLength ? sprintf("%%requires StackChain%d\n", i + 1) : "", i, i));
        }
        int parsed;
        int stack_errors;
        for (int kb = 128; kb <= 1024; kb += 32) {
            int status;
            string output = backquote("LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir)
                + " QORE_MODULE_DIR=" + quote(mdir) + " " + quote(exe) + " StackChain0 " + kb + " 2>&1", \status);
            string label = sprintf("%dK", kb);
            assertEq(0, status, label + ": " + output);
            if (output == "parsed\n") {
                ++parsed;
            } else {
                assertEq("stack error\n", output, label);
                ++stack_errors;
            }
        }
        assertGt(0, parsed, "the chain is parsed with a large stack");
        assertGt(0, stack_errors, "the chain runs short of a small stack");
    }
    #! runs the fixture on a source file in a foreign thread with each stack size from 96KB to 1MB in 16KB steps
    /** Each run must parse the source and return the expected value from its run() function, or fail with the parse
        stack error; both must occur within the range of sizes.  Code that was parsed can still run short of stack
        when it runs (\c STACK-LIMIT-EXCEEDED).  A process loads a module once, so each size runs in a process of its
        own.  A smaller stack than 96KB leaves a foreign thread too little stack to run at all on glibc, whose threads
        hold libqore's static TLS (about 60KB) in their stacks.

        @param exe the fixture
        @param libdir the directory of libqore
        @param source the file to parse
        @param expected the value that run() must return
        @param mdir the module directory, if any; the fixture inherits the module path of this process otherwise
        @param max_kb the largest stack size
        @param require_parse if the code must be parsed with the largest stack size
    */
    private runSource(string exe, string libdir, string source, int expected, *string mdir, int max_kb = 1024,
            bool require_parse = True) {
        int parsed;
        int stack_errors;
        int run_stack_errors;
        for (int kb = 96; kb <= max_kb; kb += 16) {
            int status;
            string output = backquote("LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir)
                + (mdir ? " QORE_MODULE_DIR=" + quote(mdir) : "") + " " + quote(exe) + " --source " + quote(source)
                + " " + kb + " 2>&1", \status);
            string label = sprintf("%s %dK", basename(source), kb);
            assertEq(0, status, label + ": " + output);
            # the result is the last line: a debug build of the library can write diagnostics before it, such as a
            # constant initializer of an AOT module that could not run
            *string result = (output =~ x/([^\n]*)\n$/)[0];
            if (result == "stack error") {
                ++stack_errors;
            } else if (result == "run stack error") {
                ++run_stack_errors;
            } else {
                # a load that leaves out a module, or code that is parsed with only part of its classes, is never
                # the result of a parse in a small stack
                assertEq(sprintf("parsed %d", expected), result, label + ": " + output);
                ++parsed;
            }
        }
        if (require_parse) {
            assertGt(0, parsed, basename(source) + ": the code is parsed with a large stack");
        }
        assertGt(0, stack_errors, basename(source) + ": the code runs short of a small stack");
    }

    #! writes a file
    private static writeFile(string path, string data) {
        File f();
        f.open2(path, O_WRONLY | O_CREAT | O_TRUNC);
        f.write(data);
    }

    #! Modules that load each other with %try-module, each with a fallback for the module that it cannot load
    /** A module that is not available is replaced by its fallback, but one that cannot be parsed in the thread's stack
        is not unavailable: the parse stack error of the module at any depth of the chain must be the error of the
        parse of the code that loads the first module, whether it loads it with %try-module or with load_module() at
        run time, and the chain must never be loaded in part.  Each module's count() returns the number of modules
        loaded from it to the end of the chain.
    */
    private tryModuleCheck() {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("runtime built without native stack protection");
        }
        string root = realpath(get_script_dir() + "/../../../../..");
        string libdir = realpath(ENV.QORE_LIBDIR ?? root + "/build");
        TmpDir dir("qore-parse-stack-try-");
        string exe = build(dir.path, root, libdir);
        string mdir = dir.path + "/modules";
        mkdir(mdir);
        for (int i = 0; i < ChainLength; ++i) {
            string next = sprintf("StackTry%d", i + 1);
            writeFile(sprintf("%s/StackTry%d.qm", mdir, i), sprintf("%%modern\n%s"
                "module StackTry%d { version = \"1.0\"; desc = \"test\"; author = \"Qore\"; license = \"MIT\"; }\n"
                "public namespace StackTry%d {\n"
                "public int sub count() { return %s + 1; }\n"
                "}\n",
                i + 1 < ChainLength
                    ? sprintf("%%try-module %s\npublic namespace %s { public int sub count() { return 0; } }\n"
                        "%%endtry\n", next, next)
                    : "",
                i, i, i + 1 < ChainLength ? next + "::count()" : "0"));
        }
        string source = dir.path + "/try-module.q";
        writeFile(source, "%try-module StackTry0\n"
            "public namespace StackTry0 { public int sub count() { return 0; } }\n"
            "%endtry\n"
            "int sub run() { return StackTry0::count(); }\n");
        runSource(exe, libdir, source, ChainLength, mdir);

        source = dir.path + "/load-module.q";
        writeFile(source, "int sub run() { load_module(\"StackTry0\"); return call_function(\"StackTry0::count\"); }\n");
        runSource(exe, libdir, source, ChainLength, mdir);
    }

    #! the number of classes or hashdecls in an inheritance chain
    const InheritanceLength = 300;

    #! A chain of classes each inheriting the previous one is parse-initialized from the last class to the first
    private classChainCheck() {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("runtime built without native stack protection");
        }
        string root = realpath(get_script_dir() + "/../../../../..");
        string libdir = realpath(ENV.QORE_LIBDIR ?? root + "/build");
        TmpDir dir("qore-parse-stack-classes-");
        string exe = build(dir.path, root, libdir);
        string source = dir.path + "/class-chain.q";
        string code = "class C0 { int depth() { return 0; } }\n";
        for (int i = 1; i < InheritanceLength; ++i) {
            code += sprintf("class C%d inherits C%d { int depth() { return %d; } }\n", i, i - 1, i);
        }
        code += sprintf("int sub run() { C0 c = new C%d(); return c.depth(); }\n", InheritanceLength - 1);
        writeFile(source, code);
        runSource(exe, libdir, source, InheritanceLength - 1);
    }

    #! the number of classes in the chain of classes with methods
    const MethodChainLength = 300;

    #! the number of classes in the chain of classes with methods whose most derived class is initialized first
    const ReverseMethodChainLength = 1000;

    #! A chain of classes with members, a constructor, a destructor and methods, each inheriting the previous one
    /** Each class overrides a method of its base class, and the abstract method of the first class is implemented only
        by the last one, so the parse initialization of each class reaches every class that it inherits
        (qore_class_private::parseInitPartial()) to resolve the abstract method.

        @param reverse if the names of the classes are in the reverse order of the chain: the classes are
        parse-initialized in the order of their names, so the most derived class is initialized first, and its
        initialization recurses through every class of the chain that is not yet initialized; with the chain's
        hierarchy initialized first, the recursion of qore_class_private::parseInitPartial() is the deepest one.  A
        parse of the whole chain takes too long for a test, so the stack sizes stop at 256KB, and only the stack error
        is required
    */
    private classMethodChainCheck(*bool reverse) {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("runtime built without native stack protection");
        }
        string root = realpath(get_script_dir() + "/../../../../..");
        string libdir = realpath(ENV.QORE_LIBDIR ?? root + "/build");
        TmpDir dir("qore-parse-stack-method-classes-");
        string exe = build(dir.path, root, libdir);
        string source = dir.path + "/method-class-chain.q";
        int len = reverse ? ReverseMethodChainLength : MethodChainLength;
        code name = string sub (int i) { return sprintf("A%04d", reverse ? len - 1 - i : i); };
        string code = sprintf("class %s { public { int m0 = 0; } constructor() {} destructor() {} abstract int f(); "
            "int g() { return 0; } }\n", name(0));
        for (int i = 1; i < len; ++i) {
            code += sprintf("class %s inherits %s { public { int m%d = %d; } constructor() {} destructor() {} "
                "int g() { return %s::g() + 1; }%s }\n", name(i), name(i - 1), i, i, name(i - 1),
                i + 1 == len ? sprintf(" int f() { return %d; } static int depth() { return %d; }", i, i) : "");
        }
        # no object is created: its construction would recurse through every constructor at run time
        code += sprintf("int sub run() { return %s::depth(); }\n", name(len - 1));
        writeFile(source, code);
        if (reverse) {
            runSource(exe, libdir, source, len - 1, NOTHING, 256, False);
        } else {
            runSource(exe, libdir, source, len - 1);
        }
    }

    #! the number of AOT modules (.qmod) in the chain of AOT modules that load each other with %try-module
    const AotChainLength = 12;

    #! AOT modules (.qmod) that load each other with %try-module, each with a fallback for the module that it loads
    /** As tryModuleCheck() for modules compiled with \c qcc: an AOT module loads the modules that it depends on when
        it is initialized, and a module that the chain cannot load within the stack of the thread is not a module that
        is not available, so the code that loads the first module fails with the stack error, and never parses its
        fallback code.  The constant of each module is initialized with a recursion when the module is loaded, which
        runs short of the stack (\c STACK-LIMIT-EXCEEDED) at sizes where the parse does not.  The modules shipped with %Qore are not used: a constant initializer of DataProvider has a
        compiled frame larger than the stack guard, which crashes the thread where the stack runs out in it (issue
        #6124)
    */
    private aotModuleCheck() {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("runtime built without native stack protection");
        }
        string root = realpath(get_script_dir() + "/../../../../..");
        string libdir = realpath(ENV.QORE_LIBDIR ?? root + "/build");
        string qcc = ENV.QCC ?? ENV.QORE_QCC ?? (is_file(libdir + "/qcc") ? libdir + "/qcc" : "qcc");
        TmpDir dir("qore-parse-stack-aot-");
        string exe = build(dir.path, root, libdir);
        string src = dir.path + "/src";
        string mdir = dir.path + "/modules";
        mkdir(src);
        mkdir(mdir);
        # the modules are compiled from the end of the chain, each with the modules that it loads available
        for (int i = AotChainLength - 1; i >= 0; --i) {
            string next = sprintf("StackAot%d", i + 1);
            string path = sprintf("%s/StackAot%d.qm", src, i);
            writeFile(path, sprintf("%%modern\n%s"
                "module StackAot%d { version = \"1.0\"; desc = \"test\"; author = \"Qore\"; license = \"MIT\"; }\n"
                "public namespace StackAot%d {\n"
                "public int sub deep(int n) { return n ? deep(n - 1) + 1 : 0; }\n"
                "public const Depth = deep(1500);\n"
                "public int sub count() { return %s + 1; }\n"
                "}\n",
                i + 1 < AotChainLength
                    ? sprintf("%%try-module %s\npublic namespace %s { public int sub count() { return 0; } }\n"
                        "%%endtry\n", next, next)
                    : "",
                i, i, i + 1 < AotChainLength ? next + "::count()" : "0"));
            string qmod = sprintf("%s/StackAot%d.qmod", mdir, i);
            int status;
            string output = backquote("LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir)
                + " QORE_MODULE_DIR=" + quote(mdir) + " " + quote(qcc) + " -m " + quote(path) + " -o " + quote(qmod)
                + " 2>&1", \status);
            if (status || !is_file(qmod)) {
                testSkip("qcc is not available or cannot compile the modules: " + output);
            }
        }
        string source = dir.path + "/try-aot.q";
        writeFile(source, "%try-module StackAot0\n"
            "public namespace StackAot0 { public int sub count() { return 0; } }\n"
            "%endtry\n"
            "int sub run() { return StackAot0::count(); }\n");
        # only the AOT modules are on the module path
        runSource(exe, libdir, source, AotChainLength, mdir);
    }

    #! A chain of hashdecls each inheriting the previous one is parse-initialized from the last hashdecl to the first
    private hashdeclChainCheck() {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("runtime built without native stack protection");
        }
        string root = realpath(get_script_dir() + "/../../../../..");
        string libdir = realpath(ENV.QORE_LIBDIR ?? root + "/build");
        TmpDir dir("qore-parse-stack-hashdecls-");
        string exe = build(dir.path, root, libdir);
        string source = dir.path + "/hashdecl-chain.q";
        string code = "hashdecl H0 { int k0 = 0; }\n";
        for (int i = 1; i < InheritanceLength; ++i) {
            code += sprintf("hashdecl H%d inherits H%d { int k%d = %d; }\n", i, i - 1, i, i);
        }
        code += sprintf("int sub run() { hash<H%d> h(); return h.size() - 1; }\n", InheritanceLength - 1);
        writeFile(source, code);
        runSource(exe, libdir, source, InheritanceLength - 1);
    }
}
