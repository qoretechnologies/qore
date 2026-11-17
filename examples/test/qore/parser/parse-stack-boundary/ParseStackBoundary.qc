# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (c) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

%requires ../../../TestBuildTools.qm
%requires ../../../TestInterpreter.qm

#! The cases of the ParseStackBoundary-<n>.qtest tests: each runs one part of them, so that each takes a short time,
#! and together they run every case (see addPartCase()); parts 1, 3 and 4 parse code in a single Program, parts 2 and 5
#! load modules
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
        addPartCase(4, "class inheritance chain", \classChainCheck());
        addPartCase(3, "class inheritance chain with methods", \classMethodChainCheck());
        addPartCase(4, "class inheritance chain with methods, most derived class first", \classMethodChainCheck(),
            True);
        addPartCase(1, "hashdecl inheritance chain", \hashdeclChainCheck());
        addPartCase(2, "nested module loads", \moduleCheck());
        addPartCase(2, "nested optional module loads", \tryModuleCheck());
        addPartCase(5, "nested optional AOT module loads", \aotModuleCheck());
        addPartCase(5, "AOT module compiled without an optional module that is available now",
            \aotOptionalAvailableCheck());
        addPartCase(2, "module initialization that recurses without end", \runawayInitCheck());
        addPartCase(2, "module initialization deeper than a small stack", \deepInitCheck());
        addPartCase(2, "the end of the fixture with a background JIT compile in LLVM", \abandonedCompileExitCheck());
        addPartCase(2, "a return from main() after qore_cleanup() with a JIT compile held in LLVM",
            \abandonedCompilePlainExitCheck());
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

    #! Returns the build tree of the qore under test, if any: that of LIBQORE_BINARY (set by run_tests.sh), else that
    #! of the qore binary (QORE_BINARY or QORE_ARGV[0], a bare name found in the PATH, symbolic links resolved)
    private static *string findBuildTree() {
        list<string> candidates;
        if (ENV.LIBQORE_BINARY && is_file(ENV.LIBQORE_BINARY)) {
            push candidates, dirname(realpath(ENV.LIBQORE_BINARY));
        }
        string qore = TestInterpreter::qore_interpreter();
        if (qore !~ /\//) {
            *string found;
            foreach string dir in ((ENV.PATH ?? "").split(":")) {
                if (dir && is_executable(dir + "/" + qore)) {
                    found = dir + "/" + qore;
                    break;
                }
            }
            qore = found ?? "";
        }
        if (qore && is_file(qore)) {
            push candidates, dirname(realpath(qore));
        }
        foreach string dir in (candidates) {
            if ((is_file(dir + "/libqore.so") || is_file(dir + "/libqore.dylib"))
                    && is_file(dir + "/include/qore/qore-version.h")) {
                return dir;
            }
        }
    }

    #! Returns the build tree that the fixture is built with: QORE_LIBDIR, or that of the qore under test
    private string getLibDir() {
        if (ENV.QORE_LIBDIR) {
            # as in run_tests.sh, QORE_LIBDIR names the build tree
            return realpath(ENV.QORE_LIBDIR);
        }
        *string dir = findBuildTree();
        if (!dir) {
            testSkip("the qore under test is not in a build tree (the fixture is compiled with the headers of this "
                "checkout and of its build tree): set QORE_LIBDIR to the build tree, or run the test with the qore "
                "of a build tree");
        }
        return dir;
    }

    #! builds the fixture in the given directory and returns the path of the executable
    private string build(string dir, string root, string libdir) {
        # CXX can be a command line, such as "ccache c++": its words are the first arguments
        list<string> args = TestBuildTools::command_words(ENV.CXX, "c++");
        # only a missing compiler skips the test: a compiler that cannot build the fixture is a failure
        if (!TestBuildTools::find_command(args[0])) {
            testSkip(sprintf("no C++ compiler to build the fixture: %y is not available", args[0]));
        }
        string exe = dir + "/parse-stack-boundary";
        args += ("-std=c++17", "-pthread", "-g", "-O2", "-I" + root + "/include",
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
        string libdir = getLibDir();
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
        string libdir = getLibDir();
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
                "public hash<auto> sub make(int n) {\n"
                "    return {\"n\": n, \"l\": (n, n + 1), \"s\": sprintf(\"%%d\", n)};\n"
                "}\n"
                "}\n",
                i + 1 < ChainLength ? sprintf("%%requires StackChain%d\n", i + 1) : "", i, i));
        }
        # from 128KB, or the platform's minimum stack size if it is larger (see getSweepStartKb())
        int start_kb = max(128, getSweepStartKb(exe, libdir, 32));
        hash<string, string> results = sweep(start_kb, 1024, 32, string sub (int kb) {
            int status;
            string output = backquote("LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir)
                + " QORE_MODULE_DIR=" + quote(mdir) + " " + quote(exe) + " StackChain0 " + kb + " 2>&1", \status);
            string label = sprintf("%dK", kb);
            assertEq(0, status, label + ": " + output);
            if (output == "parsed\n") {
                return "parsed";
            }
            assertEq("stack error\n", output, label);
            return "stack error";
        });
        assertGt(0, count(results, "parsed"), "the chain is parsed with a large stack");
        assertGt(0, count(results, "stack error"), "the chain runs short of a small stack");
    }

    #! the smallest stack size of the sweeps of runSource() and deepInitCheck() in KB, where the platform allows it
    /** A smaller stack than 96KB leaves a foreign thread too little stack to run at all on glibc, whose threads hold
        libqore's static TLS (about 60KB) in their stacks
    */
    const MinSweepKb = 96;

    #! Returns the smallest stack size of a sweep in KB with the given step: MinSweepKb, or the smallest stack size
    #! that a thread can be created with on this platform rounded up to a multiple of the step, if it is larger
    /** The fixture reports the platform's minimum (\c PTHREAD_STACK_MIN, 128KB with glibc on aarch64), where
        \c pthread_attr_setstacksize() refuses a smaller size, and the fixture cannot run a thread with it
    */
    private static int getSweepStartKb(string exe, string libdir, int step_kb) {
        int status;
        string output = backquote("LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir) + " "
            + quote(exe) + " --min-stack 2>&1", \status);
        if (status || output !~ /^[0-9]+\n$/) {
            throw "FIXTURE-ERROR", sprintf("the fixture cannot report the minimum stack size (exit code %d): %s",
                status, output);
        }
        int min_kb = (output.toInt() + 1023) / 1024;
        @assert(step_kb > 0);
        int rv = (min_kb + step_kb - 1) / step_kb * step_kb;
        return max(MinSweepKb, rv);
    }

    #! the ratio of the coarse step of a sweep to its step (see sweep())
    const SweepCoarseFactor = 4;

    #! Runs a check at the stack sizes of a sweep, and returns the result of each size that it ran, by size in KB
    /** Each size starts a process, so a sweep runs its check at every \a SweepCoarseFactor-th size of its range and at
        its largest size, then at every size between two of these whose results differ (qore#6420): each change of
        the result (from the stack error to code that runs short of stack when it runs, or to code that is parsed),
        where the parse runs short of stack at different points of the code, is run at the step of the sweep, and
        the sizes between two sizes with the same result are left out

        @param start_kb the smallest size
        @param max_kb the largest size; the sizes are those from \a start_kb up to it in steps of \a step_kb
        @param step_kb the step
        @param check the check of a size: runs the size, checks its output and returns its result
    */
    private static hash<string, string> sweep(int start_kb, int max_kb, int step_kb, code check) {
        list<int> sizes = ();
        for (int kb = start_kb; kb <= max_kb; kb += step_kb * SweepCoarseFactor) {
            sizes += kb;
        }
        # the largest size of the sweep
        int last_kb = start_kb + (max_kb - start_kb) / step_kb * step_kb;
        if (sizes.last() != last_kb) {
            sizes += last_kb;
        }
        hash<string, string> results = map {$1: check($1)}, sizes;
        for (int i = 0; i + 1 < sizes.size(); ++i) {
            if (results{sizes[i]} != results{sizes[i + 1]}) {
                for (int kb = sizes[i] + step_kb; kb < sizes[i + 1]; kb += step_kb) {
                    results{kb} = check(kb);
                }
            }
        }
        return results;
    }

    #! returns the number of sizes of a sweep with the given result
    private static int count(hash<string, string> results, string result) {
        return (select results.values(), $1 == result).size();
    }

    #! runs the fixture on a source file in a foreign thread with stack sizes from 96KB (or the platform's minimum) to
    #! max_kb (1MB by default) in 16KB steps: every 64KB, and every 16KB where the result changes (see sweep())
    /** Each run must parse the source and return the expected value from its run() function, or fail with the parse
        stack error; both must occur within the range of sizes.  Code that was parsed can still run short of stack
        when it runs (\c STACK-LIMIT-EXCEEDED).  A process loads a module once, so each size runs in a process of its
        own.  The sweep starts at the platform's minimum stack size where it is larger than 96KB (see
        getSweepStartKb()).

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
        int start_kb = getSweepStartKb(exe, libdir, 16);
        assertGt(start_kb, max_kb, basename(source) + ": the sweep has more than one size");
        hash<string, string> results = sweep(start_kb, max_kb, 16, string sub (int kb) {
            int status;
            string output = backquote("LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir)
                + (mdir ? " QORE_MODULE_DIR=" + quote(mdir) : "") + " " + quote(exe) + " --source " + quote(source)
                + " " + kb + " 2>&1", \status);
            string label = sprintf("%s %dK", basename(source), kb);
            assertEq(0, status, label + ": " + output);
            # the result is the last line: a debug build of the library can write diagnostics before it, such as a
            # constant initializer of an AOT module that could not run
            *string result = (output =~ x/([^\n]*)\n$/)[0];
            if (result == "stack error" || result == "run stack error") {
                return result;
            }
            # a load that leaves out a module, or code that is parsed with only part of its classes, is never the
            # result of a parse in a small stack
            assertEq(sprintf("parsed %d", expected), result, label + ": " + output);
            return "parsed";
        });
        if (require_parse) {
            assertGt(0, count(results, "parsed"), basename(source) + ": the code is parsed with a large stack");
        }
        assertGt(0, count(results, "stack error"), basename(source) + ": the code runs short of a small stack");
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
        string libdir = getLibDir();
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
        writeFile(source, "int sub run() { load_module(\"StackTry0\"); "
            "return call_function(\"StackTry0::count\"); }\n");
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
        string libdir = getLibDir();
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
        string libdir = getLibDir();
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

    #! the depth of the recursion of the constant initializer of each AOT module of aotModuleCheck()
    /** \c qcc initializes the constant when it compiles the module, in its primary thread (8MB with the usual resource
        limit), where the recursion must fit: a level takes up to about 6KB of stack in a debug build (5.7KB with GCC
        16 at -O0 on x86_64, where 1500 levels exceeded it), and it still runs short of the small stacks of the
        fixture's threads
    */
    const AotInitDepth = 1000;

    #! AOT modules (.qmod) that load each other with %try-module, each with a fallback for the module that it loads
    /** As tryModuleCheck() for modules compiled with \c qcc: an AOT module loads the modules that it depends on when
        it is initialized, and a module that the chain cannot load within the stack of the thread is not a module that
        is not available, so the code that loads the first module fails with the stack error, and never parses its
        fallback code.  The constant of each module is initialized with a recursion when the module is loaded, which
        runs short of the stack (\c STACK-LIMIT-EXCEEDED) at sizes where the parse does not; the stack sizes are all
        smaller than the default thread stack size, where this error depends on the thread.  The modules shipped with
        %Qore are not used: a constant initializer of DataProvider has a compiled frame larger than the stack guard,
        which crashes the thread where the stack runs out in it (issue #6124)
    */
    private aotModuleCheck() {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("runtime built without native stack protection");
        }
        string root = realpath(get_script_dir() + "/../../../../..");
        string libdir = getLibDir();
        string qcc = getQcc(libdir);
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
                "public const Depth = deep(%d);\n"
                "public int sub count() { return %s + 1; }\n"
                "}\n",
                i + 1 < AotChainLength
                    ? sprintf("%%try-module %s\npublic namespace %s { public int sub count() { return 0; } }\n"
                        "%%endtry\n", next, next)
                    : "",
                i, i, AotInitDepth, i + 1 < AotChainLength ? next + "::count()" : "0"));
            compileAotModule(qcc, libdir, mdir, path, sprintf("%s/StackAot%d.qmod", mdir, i));
        }
        string source = dir.path + "/try-aot.q";
        writeFile(source, "%try-module StackAot0\n"
            "public namespace StackAot0 { public int sub count() { return 0; } }\n"
            "%endtry\n"
            "int sub run() { return StackAot0::count(); }\n");
        # only the AOT modules are on the module path
        runSource(exe, libdir, source, AotChainLength, mdir);

        # a long chain of AOT modules that require each other: each AOT module loads the binary module of the next one
        # when it is loaded, before it is initialized (QoreModuleManager::loadAOTBinaryModuleDependencies()), so the
        # loads nest in the thread's stack with no parse between them
        string req_src = dir.path + "/req-src";
        string req_mdir = dir.path + "/req-modules";
        mkdir(req_src);
        mkdir(req_mdir);
        for (int i = AotRequiresChainLength - 1; i >= 0; --i) {
            string path = sprintf("%s/AotReq%d.qm", req_src, i);
            writeFile(path, sprintf("%%modern\n%s"
                "module AotReq%d { version = \"1.0\"; desc = \"test\"; author = \"Qore\"; license = \"MIT\"; }\n"
                "public namespace AotReq%d {\n"
                "public int sub count() { return %s + 1; }\n"
                "}\n",
                i + 1 < AotRequiresChainLength ? sprintf("%%requires AotReq%d\n", i + 1) : "", i, i,
                i + 1 < AotRequiresChainLength ? sprintf("AotReq%d::count()", i + 1) : "0"));
            compileAotModule(qcc, libdir, req_mdir, path, sprintf("%s/AotReq%d.qmod", req_mdir, i));
        }
        source = dir.path + "/try-aot-requires.q";
        writeFile(source, "%try-module AotReq0\n"
            "public namespace AotReq0 { public int sub count() { return 0; } }\n"
            "%endtry\n"
            "int sub run() { return AotReq0::count(); }\n");
        runSource(exe, libdir, source, AotRequiresChainLength, req_mdir);
    }

    #! the number of AOT modules in the chain of AOT modules that require each other
    const AotRequiresChainLength = 100;

    #! Returns the qcc that compiles the AOT modules: QCC, QORE_QCC, that of the build tree or that of the PATH
    private string getQcc(string libdir) {
        *string qcc = ENV.QCC ?: ENV.QORE_QCC;
        if (!qcc && is_file(libdir + "/qcc")) {
            qcc = libdir + "/qcc";
        }
        if (!qcc) {
            # the qcc of the shell's search path, if any
            qcc = TestBuildTools::find_command("qcc");
        }
        if (!qcc) {
            testSkip("qcc is not available");
        }
        return qcc;
    }

    #! compiles an AOT module with qcc, with only the modules of the given directory available
    private compileAotModule(string qcc, string libdir, string mdir, string path, string qmod) {
        int status;
        string output = backquote("LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir)
            + " QORE_MODULE_DIR=" + quote(mdir) + " " + quote(qcc) + " -m " + quote(path) + " -o " + quote(qmod)
            + " 2>&1", \status);
        # qcc is available, so a module that it cannot compile is a failure
        assertEq(0, status, sprintf("qcc %s: %s", basename(path), output));
        assertTrue(is_file(qmod), sprintf("qcc %s: %s", basename(path), output));
    }

    #! An AOT module compiled when the module of its %try-module was missing, which is available now
    /** The AOT module took the branch for the optional module being unavailable, so it is stale now: its source module
        is loaded instead, which loads the optional module.  The optional module is the first of a chain of source
        modules that require each other, so whether it is available is checked (by loading it) with much more stack than
        the AOT module itself needs; where the thread's stack runs short of it, the check cannot tell, and the load must
        fail with the parse stack error: it must never load the AOT module as it was compiled, without the optional
        module (issue #6047).  OptChain0::value() returns 1000 and the fallback for it 0; AotOpt::count() returns that
        value plus one.
    */
    private aotOptionalAvailableCheck() {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("runtime built without native stack protection");
        }
        string root = realpath(get_script_dir() + "/../../../../..");
        string libdir = getLibDir();
        string qcc = getQcc(libdir);
        TmpDir dir("qore-parse-stack-aot-opt-");
        string exe = build(dir.path, root, libdir);
        string mdir = dir.path + "/modules";
        mkdir(mdir);
        # the source module is next to the AOT module, which is compiled after it, so the AOT module is only stale
        # because of the optional module
        string path = mdir + "/AotOpt.qm";
        writeFile(path, "%modern\n"
            "%try-module OptChain0\n"
            "public namespace OptChain0 { public int sub value() { return 0; } }\n"
            "%endtry\n"
            "module AotOpt { version = \"1.0\"; desc = \"test\"; author = \"Qore\"; license = \"MIT\"; }\n"
            "public namespace AotOpt {\n"
            "public int sub count() { return OptChain0::value() + 1; }\n"
            "}\n");
        # OptChain0 does not exist yet
        compileAotModule(qcc, libdir, mdir, path, mdir + "/AotOpt.qmod");
        for (int i = 0; i < ChainLength; ++i) {
            writeFile(sprintf("%s/OptChain%d.qm", mdir, i), sprintf("%%modern\n%s"
                "module OptChain%d { version = \"1.0\"; desc = \"test\"; author = \"Qore\"; license = \"MIT\"; }\n"
                "public namespace OptChain%d {\n"
                "public bool sub matches(string s) { return s =~ /^a(b+)c[0-9]{2,}(x|y|z)*$/; }\n"
                "%s"
                "}\n",
                i + 1 < ChainLength ? sprintf("%%requires OptChain%d\n", i + 1) : "", i, i,
                i ? "" : "public int sub value() { return 1000; }\n"));
        }
        string source = dir.path + "/try-aot-optional.q";
        writeFile(source, "%try-module AotOpt\n"
            "public namespace AotOpt { public int sub count() { return 0; } }\n"
            "%endtry\n"
            "int sub run() { return AotOpt::count(); }\n");
        runSource(exe, libdir, source, 1001, mdir);
    }

    #! writes a user module with a constant initialized by a recursion
    /** @param dir the module directory
        @param name the module's name
        @param depth the depth of the recursion; 0 for a recursion without end
        @param directives directives before the module's declaration
    */
    private static writeRecursiveModule(string dir, string name, int depth, string directives = "") {
        writeFile(sprintf("%s/%s.qm", dir, name), sprintf("%%modern\n%s"
            "module %s { version = \"1.0\"; desc = \"test\"; author = \"Qore\"; license = \"MIT\"; }\n"
            "public namespace %s {\n"
            "public int sub deep(int n) { return %s; }\n"
            "public const Depth = deep(%d);\n"
            "}\n", directives, name, name, depth ? "n ? deep(n - 1) + 1 : 0" : "deep(n + 1) + 1", depth));
    }

    #! runs a script with the qore under test in a process of its own; returns its output
    private string runScript(string script, string mdir) {
        string qore = TestInterpreter::qore_interpreter();
        int status;
        string output = backquote("QORE_MODULE_DIR=" + quote(mdir) + " " + quote(qore) + " " + quote(script)
            + " 2>&1", \status);
        assertEq(0, status, output);
        return output;
    }

    #! A module whose constant initializer recurses without end fails to load in a thread with enough stack
    /** The primary thread and a thread with the default thread stack size have enough stack: the module's
        \c STACK-LIMIT-EXCEEDED is the module's own bug, not an error that depends on the thread, so %try-module
        handles the module as one that cannot be loaded and parses its fallback code, and an optional child module
        with the same bug fails (\c MODULE-CHILD-FAILED) while the module that declares it is loaded
    */
    private runawayInitCheck() {
        if (!Option::HAVE_STACK_GUARD || !Option::HAVE_MANAGE_STACK) {
            testSkip("runtime built without native stack protection");
        }
        TmpDir dir("qore-parse-stack-runaway-");
        string mdir = dir.path + "/modules";
        mkdir(mdir);
        writeRecursiveModule(mdir, "RunawayInit", 0);
        writeRecursiveModule(mdir, "RunawayChild", 0);
        writeFile(mdir + "/RunawayParent.qm", "%modern\n%try-child-module RunawayChild\n"
            "module RunawayParent { version = \"1.0\"; desc = \"test\"; author = \"Qore\"; license = \"MIT\"; }\n"
            "public namespace RunawayParent { public const Loaded = True; }\n");

        # the primary thread
        string script = dir.path + "/try.q";
        writeFile(script, "%new-style\n%try-module (ex) RunawayInit\n%define Missing\nprintf(\"fallback %s\\n\", "
            "ex.err);\n%endtry\n%ifndef Missing\nprintf(\"loaded\\n\");\n%endif\n");
        assertEq("fallback STACK-LIMIT-EXCEEDED\n", runScript(script, mdir), "primary thread");

        # a thread with the default thread stack size
        script = dir.path + "/try-thread.q";
        writeFile(script, "%new-style\n%require-types\nQueue q();\nbackground sub () {\n"
            "    Program p(PO_NEW_STYLE);\n"
            "    p.parse(\"%try-module (ex) RunawayInit\\nour string err = ex.err;\\n%endtry\\n\", \"t\");\n"
            "    p.run();\n"
            "    q.push(p.getGlobalVariable(\"err\"));\n"
            "}();\n"
            "printf(\"fallback %s\\n\", q.get());\n");
        assertEq("fallback STACK-LIMIT-EXCEEDED\n", runScript(script, mdir), "default-size thread");

        # the optional child fails, and the module that declares it is loaded without it
        script = dir.path + "/child.q";
        writeFile(script, "%new-style\n%requires RunawayParent\nprintf(\"%y %s\\n\", RunawayParent::Loaded, "
            "get_module_hash().RunawayParent.\"child-modules\".RunawayChild.status);\n");
        string output = runScript(script, mdir);
        assertRegex("True failed\n$", output);
        assertRegex("MODULE-CHILD-FAILED", output);
    }

    #! A module whose constant initializer recurses deeply is loaded with enough stack, and never left out with less
    /** In a thread whose stack is smaller than the default thread stack size, the module's \c STACK-LIMIT-EXCEEDED
        depends on the thread: the code that loads it (with %try-module) fails with it and never parses its fallback
        code, as does the code that loads a module whose optional child fails with it.  After that, a thread with the
        default thread stack size in the same process loads the module, and attaches the child that did not fail for
        good
    */
    private deepInitCheck() {
        if (!Option::HAVE_STACK_GUARD || !Option::HAVE_MANAGE_STACK) {
            testSkip("runtime built without native stack protection");
        }
        string libdir = getLibDir();
        TmpDir dir("qore-parse-stack-deep-init-");
        string exe = build(dir.path, realpath(get_script_dir() + "/../../../../.."), libdir);
        string mdir = dir.path + "/modules";
        mkdir(mdir);
        writeRecursiveModule(mdir, "DeepInit", DeepInitDepth);
        writeRecursiveModule(mdir, "DeepChild", DeepInitDepth);
        writeFile(mdir + "/DeepParent.qm", "%modern\n%try-child-module DeepChild\n"
            "module DeepParent { version = \"1.0\"; desc = \"test\"; author = \"Qore\"; license = \"MIT\"; }\n"
            "public namespace DeepParent { public const Loaded = 1; }\n");
        string try_source = dir.path + "/try-deep.q";
        writeFile(try_source, "%try-module DeepInit\npublic namespace DeepInit { public const Depth = 0; }\n%endtry\n"
            "int sub run() { return DeepInit::Depth; }\n");
        string child_source = dir.path + "/deep-child.q";
        writeFile(child_source, "%requires DeepParent\nint sub run() { return get_module_hash().DeepParent."
            "\"child-modules\".DeepChild.status == \"attached\" ? DeepParent::Loaded : 0; }\n");
        int default_kb = get_default_thread_stack_size() / 1024;
        int start_kb = getSweepStartKb(exe, libdir, 32);
        foreach hash<auto> i in ({"source": try_source, "expected": DeepInitDepth},
                {"source": child_source, "expected": 1}) {
            hash<string, string> results = sweep(start_kb, 512, 32, string sub (int kb) {
                # a small stack, then the default size in the same process
                int status;
                string output = backquote("LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir)
                    + " QORE_MODULE_DIR=" + quote(mdir) + " " + quote(exe) + " --source " + quote(i.source) + " "
                    + kb + " " + default_kb + " 2>&1", \status);
                string label = sprintf("%s %dK", basename(i.source), kb);
                assertEq(0, status, label + ": " + output);
                # the results are the last two lines (see runSource())
                list<string> lines = (map $1, output.split("\n"), $1 =~ /^(parsed|stack error|run stack error)/);
                assertEq(2, lines.size(), label + ": " + output);
                if (lines[0] != "stack error") {
                    assertEq(sprintf("parsed %d", i.expected), lines[0], label + ": " + output);
                }
                assertEq(sprintf("parsed %d", i.expected), lines[1], label + " then the default stack size: "
                    + output);
                return lines[0] ?? "";
            });
            assertGt(0, count(results, "stack error"), basename(i.source)
                + ": the module runs short of a small stack");
        }
    }

    #! The fixture ends cleanly with qore_fork_child_quick_exit() after a background JIT compile was held in LLVM
    /** The module code that the fixture loads is %modern, so its functions are compiled in the background.  A test hook
        of a debug library holds the compile of a module function in LLVM until the end; qore_cleanup() lets it go and
        waits for it (issue #6296), and the fixture then ends with qore_fork_child_quick_exit(), as the qore program
        does.
    */
    private abandonedCompileExitCheck() {
        int status;
        string output = runHeldCompileFixture("qore-parse-stack-jit-exit-", False, \status);
        assertEq(-1, output.find("abandoned compile held at debug hold point"), output);
        assertEq(0, status, output);
        # the output is written before the process ends; the compile was held (the sum of i * 4 for i < 10, twice)
        assertTrue(output =~ /^parsed 360$/m, output);
    }

    #! A process returns from main() after qore_cleanup() while a background JIT compile was held in LLVM (issue #6296)
    /** The fixture returns from main() after qore_cleanup() without qore_fork_child_quick_exit() (--plain-exit).  Before
        the fix, qore_cleanup() left the compile in LLVM, and exit() destroyed LLVM's static objects under it (a crash
        in LLVM with the output of the process lost); with the test hook, the join of the compile thread in the atexit()
        handler of the library never returned, and the debug library ended the process with status 86
        (QoreJIT::dbgExitIfAbandonedCompileHeld()).  qore_cleanup() now lets the held compile go and waits for it, so
        the process ends normally with its output.
    */
    private abandonedCompilePlainExitCheck() {
        int status;
        string output = runHeldCompileFixture("qore-parse-stack-jit-plain-exit-", True, \status);
        assertEq(-1, output.find("abandoned compile held at debug hold point"), output);
        assertEq(0, status, output);
        # qore_cleanup() waited for the compile before it released the JIT
        assertTrue(output.find("[BG-JIT] released the JIT in shutdown() after the abandoned compile ended") >= 0,
            output);
        # the output is written before the process ends; the compile was held (the sum of i * 4 for i < 10, twice)
        assertTrue(output =~ /^parsed 360$/m, output);
    }

    #! Runs the fixture on code whose module function's compile a test hook holds in LLVM until the end of the process
    /** @param prefix the prefix of the temporary directory
        @param plain_exit if the fixture ends with exit() after qore_cleanup() (--plain-exit)
        @param status the exit status of the fixture

        @return the output of the fixture, stdout and stderr
    */
    private string runHeldCompileFixture(string prefix, bool plain_exit, reference<int> status) {
        if (!Option::HAVE_LIBRARY_DEBUGGING) {
            testSkip("dbg_hold_bg_compile() requires library debugging");
        }
        string libdir = getLibDir();
        TmpDir dir(prefix);
        string exe = build(dir.path, realpath(get_script_dir() + "/../../../../.."), libdir);
        string mdir = dir.path + "/modules";
        mkdir(mdir);
        writeFile(mdir + "/HoldJit.qm", "%modern\n"
            "module HoldJit { version = \"1.0\"; desc = \"test\"; author = \"Qore\"; license = \"MIT\"; }\n"
            "public namespace HoldJit {\n"
            "public int sub held(int x) { int s = 0; for (int i = 0; i < x; ++i) { s += i * 4; } return s; }\n"
            # the compile of held() stops in LLVM, and is still there when the fixture ends
            "public int sub start() {\n"
            "    call_function(\"dbg_hold_bg_compile\", \"llvm\", \"held\");\n"
            "    int t = held(10) + held(10);\n"
            "    return call_function(\"dbg_wait_bg_compile_held\", \"llvm\", 60000) ? t : -1;\n"
            "}\n"
            "}\n");
        string source = dir.path + "/hold-jit.q";
        writeFile(source, "%requires HoldJit\nint sub run() { return HoldJit::start(); }\n");
        # functions are compiled in the background at their first call
        int rc;
        # QORE_JIT_TIMING reports where the JIT is released
        string output = backquote("env -u QORE_JIT_SYNC_COMPILE QORE_IR_THRESHOLD=1 QORE_JIT_THRESHOLD=1 "
            "QORE_JIT_TIMING=1 "
            "LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir) + " QORE_MODULE_DIR="
            + quote(mdir) + " " + quote(exe) + (plain_exit ? " --plain-exit" : "") + " --source " + quote(source)
            + " 8192 2>&1", \rc);
        status = rc;
        return output;
    }

    #! the depth of the recursion of the constant initializer of deepInitCheck()
    const DeepInitDepth = 1000;

    #! A chain of hashdecls each inheriting the previous one is parse-initialized from the last hashdecl to the first
    private hashdeclChainCheck() {
        if (!Option::HAVE_STACK_GUARD) {
            testSkip("runtime built without native stack protection");
        }
        string root = realpath(get_script_dir() + "/../../../../..");
        string libdir = getLibDir();
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
