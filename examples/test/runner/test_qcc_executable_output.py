#!/usr/bin/env python3
"""A test runs a qcc executable through ExecModeHarness::run_command(), not with its stderr merged into its output
(qore#7799, qore#7805)

A test that runs a qcc executable with ``backquote(sprintf("... <exe> 2>&1", ...))`` and compares that output gets the
warnings of the module loader in it: when a module that the program loads has an AOT module that is out of date, the
loader loads its source and warns on stderr (``BINARY-MODULE-SOURCE-FALLBACK``), and the test fails although the
program's output is correct.  The tests run their executables through ``ExecModeHarness::run_command()`` instead, which
gives the output on stdout, the output on stderr apart from the loader's warnings and the native compilation trace,
and the exit status (``ModeRunResult``); the ir tests use a ``run_executable()`` helper around it, and a qore run that
the executable's output is compared with goes through it as well.

A test fails this check when it has a ``backquote()`` or ``system()`` of a ``sprintf()`` whose format runs a command
with ``2>&1``, keeps its output (no ``> /dev/null``) and runs an executable, ``<exe> 2>&1`` or ``<exe> -b <args> 2>&1``,
also with an ``LD_LIBRARY_PATH``, ``DYLD_LIBRARY_PATH`` or environment prefix: a command that is not a run of a tool
(nm, ldd, strings, codesign, install_name_tool, ln, cmake, command, qpp: a command word of the format), not a compile,
dump or module build (an option of the format: --compile, -m -o, --module, --dump, -std=), and none of whose
arguments is the qore or qcc binary (the whole argument, after a quoting wrapper such as ``shellQuote()`` and a
``??`` default: qore_bin, qcc_bin, qccBin(), TestInterpreter::qcc_compiler(), ...).  Only the format and these whole
arguments decide: an executable whose name or path has qcc, qore, compile or a tool's name in it is still found.  Only
tests that build executables (qcc or --compile in their code) are checked, and comments do not count.  The runs that
stay as they are, as no loader warning can fail what they check, are in ``ALLOWED``, each with its reason and an
anchor: a part of the call that only that call has, compared with runs of whitespace as one space.

The tests that still have such runs are in ``PENDING_AOT3`` (converted on the AOT3 branch, not yet merged) and
``PENDING`` (to be converted), each with the number of its runs; these lists, and the allow-list, only shrink: their
sizes are pinned in ``SIZES``, and a test's number of runs must stay as recorded (lower it when a run is converted).

The check reads the format strings of the calls: a command built in a variable (``cmd += " 2>&1"``) is not seen, nor
are the helper modules that tests load, so a test author keeps to the rule in those as well.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
TESTS = ROOT / "examples/test"

#: a backquote() or system() of a sprintf(): the start of a call that runs a command
CALL = re.compile(r"\b(?:backquote|system)\s*\(\s*sprintf\s*\(")
#: a string literal
STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')
#: a tool, as a command word of the format: not a run of a qcc executable
TOOL = re.compile(r"(?:^|[\s;&|({])(?:nm|ldd|strings|codesign|install_name_tool|ln|cmake|command|qpp)\s")
#: the options of a run of qcc or qore that is not a run of an executable, in the format: a compile, a dump, a module
#: build, a C++ compile
NOT_RUN = re.compile(r"--compile\b|-m -o|--module\b|--dump|-std=")
#: an argument that is the qore or qcc binary (or a command of it), as the whole argument after its quoting wrapper
QORE_OR_QCC = re.compile(r"(?:qore_bin|qore|qcc_bin|qcc|qcc_path|qore_path|compile|qpp|qccBin\(\)|qcc_compiler\(\)"
                         r"|(?:TestInterpreter::)?(?:qore_interpreter|qcc_compiler)\(\))")
#: a quoting wrapper of an argument: quote(x), shellQuote(x), q(x), Class::shellQuote(x)
WRAPPER = re.compile(r"^(?:\w+::)?(?:shellQuote|quote|q)\((.*)\)$", re.S)
#: a run whose output is not kept: its stdout goes to /dev/null
DISCARDED = re.compile(r">\s*/dev/null\s+2>&1")

#: a test that builds executables with qcc (or with qore --compile, which runs qcc's code generator)
USES_QCC = re.compile(r"qcc|--compile\b")

#: the runs of executables with 2>&1 that stay as they are: (test, an anchor of the call) -> reason; the anchor is a
#: part of the call, compared with runs of whitespace as one space, that only the call has in the test
ALLOWED = {
    ("examples/test/ir/AOTSmoke.qc", '%s 2>&1", libqore_dir, app_path)'):
        "the PO_ENABLE_DEBUG @assert probe: regex matches of ASSERTION-ERROR and NO-FIRE",
    ("examples/test/ir/AOTSmoke.qc", '%s 2>&1", libqore_dir, plain_app)'):
        "the PO_ENABLE_DEBUG @assert probe without the flag: regex matches of NO-FIRE and ASSERTION-ERROR",
    ("examples/test/qore/classes/type-retirement-aot-parent.qtest", '%s 2>&1", libqore_dir, libqore_dir, cmd)'):
        "runCmd() runs qcc -m and qore, not a qcc executable",
    ("examples/test/ir/AOTOutlinedClosure.qc", 'replace(nm, '):
        "runs nm, whose path is in a variable, on the executable: not the executable",
    ("examples/test/qore/misc/block-scope-unwind/block-scope-unwind.qtest",
        '%s 2>&1", shellQuote(libqore_dir), shellQuote(binary))'):
        "parseResults() reads the <name>=<value> lines of the output and ignores every other line",
    ("examples/test/ir/aot-standalone-format-floor/aot-standalone-format-floor.qtest",
        'shellQuote(libdir + "/qcc"), args)'):
        "runs the qcc of the v51 build (a compile), whose path is built in the argument",
    ("examples/test/ir/aot-standalone-format-floor/aot-standalone-format-floor.qtest",
        '%s %s -b 2>&1", env, shellQuote(exe))'):
        "a v51 image that the runtime refuses to load: exit status 2 and regex matches of the error",
    ("examples/test/module-cpp-api/fork-exit/fork-exit.qtest", '"%s %s 2>&1; echo EXIT=$?", env, cmd)'):
        "compile() runs qcc (the command in cmd), a compile: its exit status and the executable are checked",
    ("examples/test/module-cpp-api/plugin-order/plugin-order-identity.qtest", 'echo status=$?", i.variant, cmd)'):
        "checkMismatch() runs probes that fail: regex matches of the error and of a failing exit status, and no line "
        "of a call of the operation",
    ("examples/test/qore/parser/modern-option-consistency/modern-option-consistency.qtest",
        '"%s %s %s 2>&1", env, reseal_bin, obj)'):
        "runs the qore-aot-reseal tool on an object, not an executable: only its exit status is checked",
    ("examples/test/qore/security/module-public-var-readonly.qtest",
        'getModuleDir(tmp.path), tmp.path + DirSep + "writer")'):
        "aotWriterTest() reads the <name>=<value> lines of the output and ignores every other line",
}


#: the number of entries of each list, which only shrinks: when a test is converted, remove its entry and lower the
#: number
SIZES = {"ALLOWED": 11, "PENDING_AOT3": 14, "PENDING": 91}

#: the tests that still run qcc executables with 2>&1, which are to be converted: test -> the number of its flagged
#: calls; the lists only shrink (test_pending_counts fails when the number of a test's calls changes: when one is
#: converted, lower it, or remove the test when none is left)
PENDING_AOT3 = {
    # converted on the AOT3 branch (#7799 test/7799-aot3, #7805 test/7805-aot3-ir-3): on develop, not on BQ; they
    # leave this list when AOT3 is merged
    "examples/test/ir/AggregateProjectionCancellation.qtest": 1,
    "examples/test/qore/classes/abstract-redeclared-parent-order.qtest": 1,
    "examples/test/qore/classes/abstract-typed-call-created-method.qtest": 1,
    "examples/test/qore/classes/method-access-diagnostic-names.qtest": 1,
    "examples/test/qore/classes/own-private-internal-method-typed-call.qtest": 1,
    "examples/test/qore/classes/private-internal-subclass-calls.qtest": 1,
    "examples/test/qore/classes/same-name-class-type-identity.qtest": 1,
    "examples/test/qore/operators/weak-assignment-own-path.qtest": 1,
    "examples/test/qore/operators/weak-reference-holder-counts.qtest": 1,
    "examples/test/qore/operators/weak-reference-share-copy.qtest": 1,
    "examples/test/qore/operators/weak-source-untyped-path.qtest": 1,
    "examples/test/qore/values/hashdecl-member-reference-args.qtest": 1,
    "examples/test/qore/values/reference-element-read.qtest": 1,
    "examples/test/qore/vars/weak-store-destructor-exception.qtest": 1,
}
PENDING = {
    # runs with an environment prefix or other forms that the #7805 inventory did not find: to be converted (or, if a
    # loader warning cannot fail what they check, moved to ALLOWED with the reason)
    "examples/test/ir/AOTAggregateMethodProjection.qtest": 1,
    "examples/test/ir/AOTAggregateReturnProjection.qtest": 1,
    "examples/test/ir/AOTAssignedHashGuard.qtest": 2,
    "examples/test/ir/AOTAssignedStringConversionFacts.qtest": 3,
    "examples/test/ir/AOTBatchMemoryCgroup.qtest": 2,
    "examples/test/ir/AOTBoundedIntExpressionSummary.qtest": 2,
    "examples/test/ir/AOTBoxedReturnParam.qtest": 1,
    "examples/test/ir/AOTCallKindExpressionSummary.qtest": 2,
    "examples/test/ir/AOTClosureSerializationFailure.qtest": 2,
    "examples/test/ir/AOTCollectionPredicateFusion.qtest": 1,
    "examples/test/ir/AOTComplexHashTypePathCache.qtest": 3,
    "examples/test/ir/AOTConstantInitializerMemory.qtest": 1,
    "examples/test/ir/AOTContextIndependentBodyLocals.qtest": 1,
    "examples/test/ir/AOTCrossObjectAggregateProjection.qtest": 1,
    "examples/test/ir/AOTDataProviderExecutable.qtest": 1,
    "examples/test/ir/AOTDebugCatchTables.qtest": 1,
    "examples/test/ir/AOTDebugIRFlow.qtest": 1,
    "examples/test/ir/AOTExecutableOptions.qtest": 1,
    "examples/test/ir/AOTExitCleanupScaling.qtest": 1,
    "examples/test/ir/AOTExplicitObjectStringMeasure.qtest": 5,
    "examples/test/ir/AOTFreshContainerInit.qtest": 3,
    "examples/test/ir/AOTGenericCrossObjectFastEntry.qtest": 1,
    "examples/test/ir/AOTGenericFunctionFastEntry.qtest": 1,
    "examples/test/ir/AOTGenericInstanceMethodFastEntry.qtest": 1,
    "examples/test/ir/AOTGenericReturnParam.qtest": 1,
    "examples/test/ir/AOTGenericStaticMethodFastEntry.qtest": 1,
    "examples/test/ir/AOTHashDeclKeyProof.qtest": 4,
    "examples/test/ir/AOTHashdeclConstantDefaults.qtest": 1,
    "examples/test/ir/AOTImmediateNumericFacts.qtest": 1,
    "examples/test/ir/AOTInvokeCleanupArraySize.qtest": 2,
    "examples/test/ir/AOTLLVM23Terminator.qtest": 1,
    "examples/test/ir/AOTLateExceptionEdgeElision.qtest": 1,
    "examples/test/ir/AOTLinkDeploymentTarget.qtest": 1,
    "examples/test/ir/AOTLinkUnresolvedReference.qtest": 1,
    "examples/test/ir/AOTLinkedExitWaitsForThreads.qtest": 1,
    "examples/test/ir/AOTMemberInitNegativeOffsets.qtest": 1,
    "examples/test/ir/AOTModuleDependencyRestrictions.qtest": 1,
    "examples/test/ir/AOTModuleHeaderRequests.qtest": 1,
    "examples/test/ir/AOTModuleMetadataSection.qtest": 1,
    "examples/test/ir/AOTNamedCallRefModules.qtest": 1,
    "examples/test/ir/AOTNativeClosure.qtest": 8,
    "examples/test/ir/AOTNestedCfgSelectSummary.qtest": 2,
    "examples/test/ir/AOTNonescapingObjectScalarProjection.qtest": 1,
    "examples/test/ir/AOTNoopDecrefPrune.qtest": 2,
    "examples/test/ir/AOTObjectMemberCompoundGet.qtest": 2,
    "examples/test/ir/AOTObjectMemberCompoundGetCrossObject.qtest": 1,
    "examples/test/ir/AOTObjectMemberSetGet.qtest": 5,
    "examples/test/ir/AOTObjectMemberSetGetCrossObject.qtest": 1,
    "examples/test/ir/AOTOptionalScalarFastEntry.qtest": 2,
    "examples/test/ir/AOTOptionalScalarSpecialization.qtest": 1,
    "examples/test/ir/AOTReferenceFreeResult.qtest": 1,
    "examples/test/ir/AOTRegexCaseLoadFailure.qtest": 1,
    "examples/test/ir/AOTReturnTypeCoercion.qtest": 2,
    "examples/test/ir/AOTRuntimeLocationCache.qtest": 1,
    "examples/test/ir/AOTRuntimeNamedCallReturnType.qtest": 1,
    "examples/test/ir/AOTScalarObjectGetter.qtest": 3,
    "examples/test/ir/AOTScalarizedAggregateReturnProjection.qtest": 1,
    "examples/test/ir/AOTSelfCallPrivateInternal.qtest": 1,
    "examples/test/ir/AOTStaticModuleMetadata.qtest": 2,
    "examples/test/ir/AOTStoredCaptureCache.qtest": 2,
    "examples/test/ir/AOTStringCasePipeline.qtest": 1,
    "examples/test/ir/AOTStringOrdering.qtest": 2,
    "examples/test/ir/AOTStringSummaryCfg.qtest": 1,
    "examples/test/ir/AOTStringTransformCfg.qtest": 1,
    "examples/test/ir/AOTStringTransformConsumer.qtest": 3,
    "examples/test/ir/AOTStringTransformSwitch.qtest": 2,
    "examples/test/ir/AOTSummaryExceptionElision.qtest": 2,
    "examples/test/ir/AOTWideIntExpressionSummary.qtest": 2,
    "examples/test/ir/IRContextElision.qtest": 1,
    "examples/test/ir/IRNativeLocalSSA.qtest": 1,
    "examples/test/ir/IRTypedForeach.qtest": 1,
    "examples/test/ir/JITReloadChainFlush.qc": 1,
    "examples/test/ir/LoopInvariantHashProjection.qtest": 1,
    "examples/test/ir/LoopInvariantReadOnlyQuery.qtest": 1,
    "examples/test/ir/aot-closure-toplevel-local-update/aot-closure-toplevel-local-update.qtest": 1,
    "examples/test/ir/aot-ir-value-ids/AotIrValueIds.qc": 2,
    # its two runs have the same text: an image with a patched format version, which the runtime refuses (exit
    # status and matches of the error, which may stay), and compileAndRun()'s, which compares the output of a
    # successful run (to convert)
    "examples/test/ir/aot-standalone-format-floor/aot-standalone-format-floor.qtest": 2,
    "examples/test/ir/aot-runtime-identity/aot-runtime-identity.qtest": 4,
    "examples/test/ir/aot-section-bounds/AotSectionBounds.qc": 1,
    "examples/test/ir/aot-shadowed-locals/aot-shadowed-locals.qtest": 1,
    "examples/test/ir/batch-callee-closure-frame/batch-callee-closure-frame.qtest": 1,
    "examples/test/ir/catch-scope-rethrow/catch-scope-rethrow.qtest": 1,
    "examples/test/ir/cleared-captured-local/cleared-captured-local.qtest": 1,
    "examples/test/ir/closure-destroyed-self/closure-destroyed-self.qtest": 1,
    "examples/test/ir/image-ir-temp-drain/image-ir-temp-drain.qtest": 1,
    "examples/test/ir/inline-handler-flow-loop/InlineHandlerFlowLoop.qc": 2,
    "examples/test/ir/inline-handler-second-request/InlineHandlerSecondRequest.qtest": 2,
    "examples/test/ir/on-block-exit-outer-vars/on-block-exit-outer-vars.qtest": 1,
    "examples/test/ir/on-error-rethrow/on-error-rethrow.qtest": 1,
    "examples/test/ir/qcc-load-modules.qtest": 2,
    "examples/test/ir/static-closure-class-context/static-closure-class-context.qtest": 1,
}


def all_files():
    """The tests and the files that they include: those of examples/test and of the binary modules"""
    files = []
    for pattern in ("*.qtest", "*.qc"):
        files += TESTS.rglob(pattern)
        files += (ROOT / "modules").glob("*/test/**/" + pattern)
    return sorted(set(files))


def split_args(text):
    """Splits the arguments of a call (the text after its format) at its commas outside parentheses and strings"""
    args = []
    depth = 0
    cur = ""
    i = 0
    while i < len(text):
        c = text[i]
        if c == '"':
            m = STRING.match(text, i)
            if m:
                cur += m.group(0)
                i = m.end()
                continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                break
            depth -= 1
        if c == "," and depth == 0:
            args.append(cur.strip())
            cur = ""
        else:
            cur += c
        i += 1
    if cur.strip():
        args.append(cur.strip())
    return [a for a in args if a]


def unwrap(arg):
    """Returns an argument without its quoting wrappers and its default (x ?? "qcc")"""
    arg = arg.strip()
    while True:
        m = WRAPPER.match(arg)
        if not m:
            return arg.split("??")[0].strip()
        arg = m.group(1).split("??")[0].strip()


def runs_executable(fmt, args):
    """Returns True if the format runs a qcc executable: it runs no tool (a command word of the format), it is no
    compile, dump or module build (its options), and none of its arguments is the qore or qcc binary"""
    if NOT_RUN.search(fmt) or TOOL.search(fmt):
        return False
    return not any(QORE_OR_QCC.fullmatch(unwrap(arg)) for arg in args)


def calls(text):
    """Yields (line number, call text, format, arguments) for each backquote()/system() of a sprintf()"""
    for m in CALL.finditer(text):
        # the call text up to the closing parenthesis of the sprintf(), outside string literals
        depth = 1
        i = m.end()
        while i < len(text) and depth:
            c = text[i]
            if c == '"':
                s = STRING.match(text, i)
                i = s.end() if s else i + 1
                continue
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            i += 1
        call = text[m.start():i]
        # the format: the adjacent string literals that start the sprintf() arguments
        fmt = ""
        rest = text[m.end():i]
        while True:
            s = STRING.match(rest.lstrip())
            if not s:
                break
            fmt += s.group(1)
            rest = rest.lstrip()[s.end():]
        args = split_args(rest.lstrip()[1:]) if rest.lstrip().startswith(",") else []
        yield text.count("\n", 0, m.start()) + 1, call, fmt, args


def normalized(text):
    """Returns text with its runs of whitespace replaced by one space"""
    return " ".join(text.split())


def flagged_calls(rel, text):
    """Returns (line number, call) for each run of a qcc executable with 2>&1 of a test that is not allowed"""
    # comments do not count
    code = "\n".join("" if line.lstrip().startswith("#") else line for line in text.splitlines())
    # only a test that builds executables with qcc runs them
    if not USES_QCC.search(code):
        return []
    out = []
    for number, call, fmt, args in calls(code):
        if "2>&1" not in fmt or DISCARDED.search(fmt) or not runs_executable(fmt, args):
            continue
        if allowed(rel, call):
            continue
        out.append((number, call))
    return out


def allowed(rel, call):
    """Returns True if an ALLOWED entry covers the call: its anchor is in the call, whitespace normalized"""
    return any(rel == path and normalized(anchor) in normalized(call) for path, anchor in ALLOWED)


def check_text(rel, text):
    """Returns a list of the errors of the text of a test"""
    errors = []
    for number, _ in flagged_calls(rel, text):
        errors.append("%s:%d: a qcc executable runs with 2>&1, so a warning of the module loader on stderr is part of "
            "its output; run it with ExecModeHarness::run_command() and check its stdout and stderr apart from the "
            "warnings (qore#7805)" % (rel, number))
    return errors


def check(path):
    return check_text(str(path.relative_to(ROOT)), path.read_text(errors="replace"))


#: the start of a synthetic test that builds executables with qcc, for the self-tests
QCC = 'string qcc_bin = TestInterpreter::qcc_compiler();\n'


class QccExecutableOutputTest(unittest.TestCase):
    def test_tests(self):
        errors = []
        for path in all_files():
            if str(path.relative_to(ROOT)) not in PENDING_AOT3 and str(path.relative_to(ROOT)) not in PENDING:
                errors += check(path)
        self.assertEqual([], errors, "\n" + "\n".join(errors))

    def test_sizes(self):
        # the lists only shrink: lower the number when an entry is removed
        for name, size in SIZES.items():
            self.assertEqual(size, len(globals()[name]), "%s has %d entries; SIZES says %d: when a test is converted, "
                "remove its entry and lower the number in SIZES; never add an entry" % (name, len(globals()[name]),
                size))
        self.assertFalse(set(PENDING_AOT3) & set(PENDING), "a test in both pending lists")

    def test_pending_counts(self):
        # a pending test has as many flagged calls as recorded: none new, and a converted one lowers the number
        for rel, count in list(PENDING_AOT3.items()) + list(PENDING.items()):
            path = ROOT / rel
            n = len(flagged_calls(rel, path.read_text(errors="replace"))) if path.is_file() else 0
            self.assertEqual(count, n, "%s: %d runs of qcc executables with 2>&1, %d recorded: %s" % (rel, n, count,
                "a new run is not allowed; run it with ExecModeHarness::run_command()" if n > count else
                "lower the number, or remove the entry when none is left"))

    def test_allowed_entries(self):
        # every allow-list entry names one call, which the check would flag without the entry
        for (rel, anchor), why in ALLOWED.items():
            self.assertTrue(why, "%s: %r: no reason" % (rel, anchor))
            text = (ROOT / rel).read_text(errors="replace")
            code = "\n".join("" if line.lstrip().startswith("#") else line for line in text.splitlines())
            matching = [(fmt, args) for _, call, fmt, args in calls(code)
                if normalized(anchor) in normalized(call)]
            self.assertEqual(1, len(matching), "%s: %d calls with the anchor %r (%s); an anchor names one call"
                % (rel, len(matching), anchor, why))
            fmt, args = matching[0]
            self.assertTrue("2>&1" in fmt and not DISCARDED.search(fmt) and runs_executable(fmt, args),
                "%s: the call with the anchor %r is not flagged without its entry: remove the entry (%s)"
                % (rel, anchor, why))

    def test_detects(self):
        # the forms of the runs of executables that the tests converted are found
        for snippet in (
                'string out = backquote(sprintf("%s -b 2>&1", exe), \\rc);',
                'string out = backquote(sprintf("LD_LIBRARY_PATH=%s %s 2>&1", libqore_dir, exe));',
                'out = backquote(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s 2>&1",\n'
                '    libqore_dir, libqore_dir, exe));',
                'out = backquote(sprintf("%s %s 2>&1", env, exe));',
                'out = backquote(sprintf("%s%s -b 2>&1", kill_after, exe), \\rc);',
                'out = backquote(sprintf("LD_LIBRARY_PATH=%s %s 2>&1; echo EXIT=$?", libqore_dir, path));',
                'out = backquote(sprintf("LD_LIBRARY_PATH=%s "\n    "%s 2>&1", libqore_dir, exe));'):
            self.assertEqual(1, len(check_text("examples/test/x.qtest", QCC + snippet)), snippet)
        # an executable run whose arguments have names or paths with qcc, qore, compile or a tool's name in them is
        # found: only the whole argument that is the qore or qcc binary, and the format's command words and options,
        # make a run that is not one of an executable
        for snippet in (
                'out = backquote(sprintf("LD_LIBRARY_PATH=%s %s 2>&1", libqore_dir, qcc_exe));',
                'out = backquote(sprintf("LD_LIBRARY_PATH=%s %s -b 2>&1", libqore_dir, qore_app));',
                'out = backquote(sprintf("%s %s 2>&1", compile_out, exe));',
                'out = backquote(sprintf("LD_LIBRARY_PATH=%s %s 2>&1", libqore_dir, "/tmp/qore-test/exe"));',
                'out = backquote(sprintf("%s %s %s 2>&1", ln, strings, nm));',
                'out = backquote(sprintf("%s -b 2>&1", shellQuote(qcc_exe)));'):
            self.assertEqual(1, len(check_text("examples/test/x.qtest", QCC + snippet)), snippet)
        # runs of qore and qcc, compiles and tools are not, nor the converted runs, nor comments
        for snippet in (
                'out = backquote(sprintf("%s -b --exec-mode=ast %s 2>&1", qore_bin, src));',
                'out = backquote(sprintf("%s -O%s -o %s %s 2>&1", qcc_bin, opt, exe, src), \\rc);',
                'out = backquote(sprintf("%s -o %s %s 2>&1", shellQuote(TestInterpreter::qcc_compiler() ?? "qcc"), '
                'exe, src));',
                'out = backquote(sprintf("QORE_LIBDIR=%s %s -b --compile --output %s %s 2>&1", libqore_dir, q, o, s));',
                'out = backquote(sprintf("nm -u %s 2>&1", exe));',
                'out = backquote(sprintf("ldd %s 2>&1", exe));',
                'string out = run_executable(sprintf("LD_LIBRARY_PATH=%s %s", libqore_dir, exe));',
                'hash<auto> r = ExecModeHarness::run_command(sprintf("%s -b", exe));',
                '# out = backquote(sprintf("%s -b 2>&1", exe));',
                'string found = backquote("command -v qcc 2>/dev/null");'):
            self.assertEqual([], check_text("examples/test/x.qtest", QCC + snippet), snippet)
        # a run whose output is discarded is not, as only its exit status counts
        self.assertEqual([], check_text("examples/test/x.qtest", QCC +
            'int rc = system(sprintf("LD_LIBRARY_PATH=%s %s -b > /dev/null 2>&1", libqore_dir, exe));'))
        # an allow-list entry only covers its own test and call, whitespace normalized
        run = QCC + 'string run_out = backquote(sprintf("LD_LIBRARY_PATH=%s %s 2>&1", libqore_dir,\n    app_path));'
        self.assertEqual([], check_text("examples/test/ir/AOTSmoke.qc", run))
        self.assertEqual(1, len(check_text("examples/test/ir/Other.qtest", run)))
        self.assertEqual(1, len(check_text("examples/test/ir/AOTSmoke.qc", QCC +
            'string run_out = backquote(sprintf("LD_LIBRARY_PATH=%s %s 2>&1", libqore_dir, app_path2));')))
        # a test that builds no executable with qcc is not checked
        self.assertEqual([], check_text("examples/test/x.qtest",
            'string out = backquote(sprintf("LD_LIBRARY_PATH=%s %s 2>&1", libqore_dir, exe));'))


if __name__ == "__main__":
    unittest.main()
