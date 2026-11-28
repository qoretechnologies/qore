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

A test fails this check when it has a ``backquote()`` or ``system()`` whose command runs with ``2>&1``, keeps its
output (no ``> /dev/null``) and runs an executable, ``<exe> 2>&1`` or ``<exe> -b <args> 2>&1``, also with an
``LD_LIBRARY_PATH``, ``DYLD_LIBRARY_PATH`` or environment prefix: a command that is not a run of a tool or of qore or
qcc (nm, ldd, strings, codesign, install_name_tool, ln, cmake, command, qpp, qore, qcc, cat, find, cmp, strip,
objcopy, objdump, lipo, readelf, otool: a command word of the format), not a compile, dump or module build (an option
of the format: --compile, -m -o, --module, --dump, -std=), that has an argument (a command of literals only runs no
executable that the test built), and none of whose arguments is the qore or qcc binary (the whole argument, after a
quoting wrapper such as ``shellQuote()`` and a ``??`` default: qore_bin, qcc_bin, qccBin(),
TestInterpreter::qcc_compiler(), ...).  Only the format and these whole arguments decide: an executable whose name or
path has qcc, qore, compile or a tool's name in it is still found.

The command of a call is read as a format with its arguments (qore#7818):
- a ``sprintf()``: its format string and its arguments;
- a concatenation (``a + b + ...``, also in parentheses): its string literals in order, with ``%s`` for each other
  operand, which is an argument (``backquote(shellQuote(exe) + " 2>&1")`` is the format ``"%s 2>&1"`` of the argument
  ``shellQuote(exe)``); a ``sprintf()`` operand adds its format and arguments;
- a variable, as the whole command or as an operand of a concatenation (unless its name is the qore or qcc binary,
  which is an argument as it is): the values of its assignments (``=``, ``+=``) that come before the call, from its
  last declaration before the call (``string cmd = ...``, a parameter with its default); each ``=`` that does not use
  the variable is one more alternative (the branches of an ``if``), and a ``+=`` or an ``=`` that uses it
  (``cmd = cmd + " 2>&1"``) adds to each of them;
- a parameter of a named function or method (``string run(string cmd) { return backquote(cmd + " 2>&1"); }``): also
  the arguments that the calls of the function in the same text give it, one alternative each, so that a helper that
  its calls give only runs of qore and qcc is not flagged; a parameter without such calls is an argument.

The call is flagged when one of its alternatives is.  The variables of these values are resolved in the same way, up
to ``MAX_DEPTH`` levels; the arguments of a ``sprintf()`` are not resolved (they are the arguments of its format), nor
is an expression with a ``?:``, which is an argument as a whole.

Only tests that build executables (qcc or --compile in their code) and that have ``2>&1`` are checked, and comments
(from a ``#`` that starts a line or follows whitespace outside string literals to the end of its line, and ``/* */``
blocks that start a line) do not count; the calls, declarations and assignments in string literals are not read.  The
runs that stay as they are, as no loader warning can fail what they check, are in ``ALLOWED``, each with its reason
and an anchor: a part of the call, or of an assignment that it is built of, that only that call has, compared with
runs of whitespace as one space.

The tests that still have such runs are in ``PENDING_AOT3`` (converted on the AOT3 branch, not yet merged) and
``PENDING`` (to be converted), each with the number of its runs; these lists, and the allow-list, only shrink: their
sizes are pinned in ``SIZES``, and a test's number of runs must stay as recorded (lower it when a run is converted).

The check reads the text of the tests, not their code paths, so it does not see: a command whose ``2>&1`` comes from a
function's caller in another file, a call reference or a closure (``backquote(cmd)`` of a parameter without calls in
the text is not flagged, ``backquote(cmd + " 2>&1")`` is), or that is returned by a function or kept in a member or a
container (``cmds[i]``, ``self.cmd``); a ``2>&1`` in a ``?:`` or another expression that is not a literal, a
``sprintf()``, a variable or a concatenation of them; a ``2>&1`` of a ``sprintf()`` argument
(``sprintf("%s %s", env, cmd)`` with ``cmd`` ending in ``2>&1``); a variable assigned in a way other than ``=`` and
``+=`` (a reference argument, ``??=``, ``push``); the elements of a list that a command is joined from
(``(map quote($1), args).join(" ")`` is an argument, so a list that holds the qore or qcc binary or a tool is not
known as one: such runs are in ``ALLOWED``); a local variable of another function with the same name when the
variable of the call has no declaration of its own; and the helper modules that tests load.  A test author keeps to
the rule in those as well.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
TESTS = ROOT / "examples/test"

#: a backquote() or system() call: the start of a call that runs a command
CALL = re.compile(r"(?<![\w.:>])(?:backquote|system)\s*\(")
#: a string literal
STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')
#: a single-quoted string literal
SQ_STRING = re.compile(r"'(?:[^'\\]|\\.)*'")
#: one or more adjacent string literals: a whole operand
LITERALS = re.compile(r'\s*(?:"(?:[^"\\]|\\.)*"\s*)+')
#: a sprintf() call
SPRINTF = re.compile(r"sprintf\s*\(")
#: an identifier: a variable operand, resolved through its assignments
IDENT = re.compile(r"[A-Za-z_]\w*")
#: a word that precedes a variable without declaring it (return cmd;, else cmd = ...)
KEYWORDS = {"return", "else", "throw", "case", "delete", "remove", "exists", "not", "new", "cast", "background", "do",
    "in", "elements", "keys", "our", "my"}
#: the name of a function or method, as the end of the text before its parameter list
FUNC_NAME = re.compile(r"(\w+)\s*$")
#: how deep the variables that a command is built of are resolved (a variable of a variable ...)
MAX_DEPTH = 4
#: the most alternatives of a command that are kept (one per assignment of a variable that is assigned in branches)
MAX_ALTERNATIVES = 32
#: a tool or qore or qcc, as a command word of the format: not a run of a qcc executable
TOOL = re.compile(r"(?:^|[\s;&|({])(?:nm|ldd|strings|codesign|install_name_tool|ln|cmake|command|qpp|qore|qcc|cat|find"
                  r"|cmp|strip|objcopy|objdump|lipo|readelf|otool)\s")
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
    ("examples/test/ir/AOTBatchMemoryCgroup.qtest",
        'QORE_AOT_BATCH_JOBS=1 %s 2>&1", env, compileCommand(ref, groups[g]))'):
        "the serial reference compiles of qcc (the command of compileCommand()): only their exit status is checked",
    ("examples/test/ir/AOTBatchMemoryCgroup.qtest", '"sh %s 2>&1", MemoryMax, q(script))'):
        "the cgroup script runs the compiles in the background with their output in logs; the test reads only its "
        "key=<number> pairs",
    ("examples/test/ir/AOTDataProviderExecutable.qtest", 'echo $? > %s", libqore_dir, exe_path, status_path)'):
        "the executable runs a QUnit test: its exit status and a regex match of the QUnit summary line",
    ("examples/test/ir/AOTModuleMetadataSection.qtest", '"%s %s 2>&1", reseal_bin, qmod)'):
        "runs the qore-aot-reseal tool on a module, not an executable: only its exit status is checked",
    ("examples/test/ir/AOTLinkDeploymentTarget.qtest", 'shellQuote(lib_dir), cmd)'):
        "run() runs qcc links (the command in cmd), checked for no linker warning and by the files they write; the "
        "executable runs through checkExecutable()",
    ("examples/test/ir/AOTStaticModuleMetadata.qtest", '"%s %s --min-stack 2>&1", env, exe)'):
        "the C++ host prints its minimum stack size and returns before qore_init(): no module is loaded",
    ("examples/test/ir/aot-ir-value-ids/AotIrValueIds.qc", '"%s %s %d %d 2>&1", reseal_bin, path, meta_pos, meta_len)'):
        "runs the qore-aot-reseal tool on a module, not an executable: only its exit status is checked",
    ("examples/test/ir/aot-section-bounds/AotSectionBounds.qc",
        '"%s %s %d %d 2>&1", reseal_bin, path, meta_pos, meta_len)'):
        "runs the qore-aot-reseal tool on a module, not an executable: only its exit status is checked",
    ("examples/test/ir/AOTStaticModuleMetadata.qtest", 'env, tmp.path, exe, RegisterStackSize, kb)'):
        "the C++ host's registration runs: its exit status, and finds and regex matches in the output, whose order of "
        "stdout and stderr (the errors before \"registered\") the checks read",
    # the commands built by concatenation or in a variable (qore#7818)
    ("examples/test/ir/aot-standalone-format-floor/aot-standalone-format-floor.qtest",
        'shellQuote(libdir), shellQuote(libdir), shellQuote(exe))'):
        "oldRuntimeTest(): the runtime of the v51 build refuses the executable: exit status 2 and a regex match and "
        "finds of the error",
    ("examples/test/qore/classes/Program/class-injection-ownership.qtest",
        'backquote(shellQuote(exe) + " 2>&1", \\rc)'):
        "the executable's exit status only",
    ("examples/test/qore/classes/imported-internal-bareword/imported-internal-bareword.qtest",
        'backquote(command + " 2>&1", \\rc)'):
        "checkRefusal() runs refused programs: a failing exit status and a find of the error",
    ("examples/test/ir/AOTModuleMetadataSection.qtest", 'backquote(cmd + " 2>&1")'):
        "run() runs qcc, qore and tools; the runs that are not of qcc or qore are of otool and readelf, whose paths "
        "are in variables, on a module: not an executable",
    ("examples/test/ir/AOTModuleMetadataSection.qtest", 'backquote(cmd + " 2>&1", \\rc)'):
        "run(cmd, rc) runs qcc, qore and tools; the runs that are not of qcc or qore are of otool and readelf, whose "
        "paths are in variables, on a module: not an executable",
    ("examples/test/ir/AOTStaticModuleMetadata.qtest",
        '(map TestBuildTools::shell_quote($1), args).join(" ") + " 2>&1", \\status)'):
        "links the C++ host with the C++ compiler (the command in args): its exit status and the file it writes",
    ("examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary.qc",
        'backquote((map quote($1), args).join(" ") + " 2>&1", \\status)'):
        "compiles the C++ fixture with the C++ compiler (the command in args): its exit status only",
    ("examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary.qc", 'quote(exe) + " --min-stack 2>&1"'):
        "the C++ fixture prints its minimum stack size and returns before qore_init(): no module is loaded",
    ("examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary.qc", '" --source " + quote(source) + " " + kb'):
        "runSource() reads only the last line of the output (diagnostics of the library can come before it)",
    ("examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary.qc", '" " + kb + " " + default_kb + " 2>&1"'):
        "reads only the result lines of the output (a regex filter of its lines) and the exit status",
    ("examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary.qc", '(plain_exit ? " --plain-exit" : "")'):
        "runHeldCompileFixture(): finds and a multiline regex match of lines of the output, and the exit status",
    ("examples/test/qore/threads/frame-check-stack.qtest",
        'backquote((map quote($1), args).join(" ") + " 2>&1", \\status)'):
        "compiles the C++ fixture with the C++ compiler (the command in args): its exit status only",
    ("examples/test/ir/AOTCrossModuleFastCall.qtest", '" QORE_AOT_TRACE_LINK_RELOCS=1") + " 2>&1")'):
        "a line of the output and a count of the trace lines that have a helper name",
    ("examples/test/ir/AOTExampleScripts.qtest", 'backquote("{ " + command + "; } 2>&1; echo \\"rc=$?\\"")'):
        "runs the example scripts with bash: their exit status only (the output is shown when it fails)",
    ("examples/test/ir/AOTMetadataDecompressOnce.qtest", '" QORE_AOT_TRACE_METADATA_DECOMPRESS=1 " + command'):
        "finds of the result lines and regex matches of the decompression trace lines",
    ("examples/test/ir/AOTRetainedMetadata.qtest", 'backquote(env + " " + command + " 2>&1")'):
        "checkRetention(): finds of the result and trace lines and regex matches of the retention trace",
    ("examples/test/ir/DebugFlowPathCode.qtest", 'command = "env LD_LIBRARY_PATH=" + quote(lib_dir)'):
        "runs qore (an element of the list of the command) with an LLVM IR dump, not a qcc executable; it reads the "
        "lines of the IR",
    ("examples/test/ir/AOTSccGeneration.qc", 'backquote("{ " + join(" ", command)'):
        "runTool() runs the source-order tool with qore (the first element of the command)",
    ("examples/test/ir/AOTSccGraphTransition.qtest", 'backquote("{ " + join(" ", command)'):
        "runTool() runs the source-order tool with qore (an element of the command)",
    ("examples/test/ir/AOTSccIncrementalDriver.qc", 'backquote("{ " + join(" ", command)'):
        "runTool() runs the source-order tool with qore (the first element of the command)",
    ("examples/test/qlib/ProviderIndexUtil/ProviderIndexCallerPermissions.qtest",
        'backquote(env + shellQuote(exe) + " 2>&1", \\rc)'):
        "the executable's exit status only",
}


#: the number of entries of each list, which only shrinks: when a test is converted, remove its entry and lower the
#: number
SIZES = {"ALLOWED": 41, "PENDING_AOT3": 14, "PENDING": 87}

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
    # the runs of qcc executables with 2>&1 whose commands are built by concatenation or in a variable, which the check
    # sees since qore#7818: to be converted like those of #7805 (a test cannot be added here: the lists only shrink)
    "examples/test/ir/AOTAliasedConstantPathOwnership.qtest": 1,
    "examples/test/ir/AOTBackgroundStaticMethodSlot.qtest": 1,
    "examples/test/ir/AOTBareReturnNothing.qtest": 1,
    "examples/test/ir/AOTBatchSharedPendingConstant.qtest": 1,
    "examples/test/ir/AOTCachedMethodBinding.qtest": 1,
    "examples/test/ir/AOTCachedMethodNames.qtest": 1,
    "examples/test/ir/AOTClassBodyLocals.qtest": 1,
    "examples/test/ir/AOTClassLocalRelease.qtest": 1,
    "examples/test/ir/AOTClosureUnanchoredSlots.qtest": 1,
    "examples/test/ir/AOTConstructorCallRelocations.qtest": 1,
    "examples/test/ir/AOTConstructorParameterReceiverLifetime.qtest": 1,
    "examples/test/ir/AOTContextExpressionSlots.qtest": 1,
    "examples/test/ir/AOTCrossModuleFastCall.qtest": 1,
    "examples/test/ir/AOTDebuggerIntStringMeasure.qtest": 1,
    "examples/test/ir/AOTDebuggerReflectionFreeSlots.qtest": 1,
    "examples/test/ir/AOTDeferredNamespaceConstantPath.qtest": 1,
    "examples/test/ir/AOTEmptyRegex.qtest": 1,
    "examples/test/ir/AOTFindContextCallArgs.qtest": 1,
    "examples/test/ir/AOTInitFixpointTransientException.qtest": 1,
    "examples/test/ir/AOTLValuePathRefRv.qtest": 1,
    "examples/test/ir/AOTLargeLValuePathCount.qtest": 1,
    "examples/test/ir/AOTLargeSourceLineLocations.qtest": 1,
    "examples/test/ir/AOTLocalReleaseOrder.qtest": 1,
    "examples/test/ir/AOTMethodDefaultNewline.qtest": 1,
    "examples/test/ir/AOTModuleDelClosure.qtest": 1,
    "examples/test/ir/AOTModuleInitException.qtest": 1,
    "examples/test/ir/AOTNestedFind.qtest": 1,
    "examples/test/ir/AOTOutlinedLocalRelease.qtest": 1,
    "examples/test/ir/AOTOverloadSelfRecursion.qtest": 1,
    "examples/test/ir/AOTPathPushResult.qtest": 1,
    "examples/test/ir/AOTPlainFunctionClassContext.qtest": 1,
    "examples/test/ir/AOTQoLinkNativeSymbols.qtest": 1,
    "examples/test/ir/AOTScopedFunctionNoFallback.qtest": 1,
    "examples/test/ir/AOTScriptAggregateNativeRegister.qtest": 1,
    "examples/test/ir/AOTScriptPendingConstantRecovery.qtest": 1,
    "examples/test/ir/AOTSelfMemberIntUpdate.qtest": 1,
    "examples/test/ir/AOTSerializedDebugSteps.qtest": 1,
    "examples/test/ir/AOTSiblingNameCollision.qtest": 1,
    "examples/test/ir/AOTSplitStaticCallSlot.qtest": 1,
    "examples/test/ir/AOTStaticModuleShadowing.qtest": 1,
    "examples/test/ir/AOTStoredCapturedClosure.qtest": 1,
    "examples/test/ir/AOTSymbolIndex.qc": 1,
    "examples/test/ir/AOTTopLevelSourceLocations.qtest": 1,
    "examples/test/ir/AOTTryLocalReload.qtest": 1,
    "examples/test/ir/GenericSpecializationPrograms.qc": 1,
    "examples/test/ir/IRForeachHeldSource.qtest": 1,
    "examples/test/ir/IRHandlerReplacedValues.qtest": 1,
    "examples/test/ir/IRInPlaceListPushTypeError.qtest": 1,
    "examples/test/ir/IRLocalUnwindOrder.qtest": 1,
    "examples/test/ir/IRScalarSnapshot.qtest": 1,
    "examples/test/ir/IRThreadExitUnwind.qtest": 1,
    "examples/test/ir/JITStaticMethodDirectCalls.qtest": 1,
    "examples/test/ir/PlainFunctionCallerContexts.qtest": 1,
    "examples/test/ir/SwitchStringEncoding.qtest": 1,
    "examples/test/ir/ThrowingTempCleanup.qtest": 1,
    "examples/test/ir/qcc-test-libqore.qtest": 1,
    "examples/test/qore/classes/Program/reparse-toplevel-run.qtest": 1,
    "examples/test/qore/classes/abstract-mixed-variants/abstract-mixed-variants.qtest": 1,
    "examples/test/qore/classes/abstract-second-parent/abstract-second-parent.qtest": 1,
    "examples/test/qore/classes/call-reference-deleted-object/call-reference-deleted-object.qtest": 1,
    "examples/test/qore/classes/call-reference-hash-value-deleted/call-reference-hash-value-deleted.qtest": 1,
    "examples/test/qore/classes/call-target-before-args/call-target-before-args.qtest": 1,
    "examples/test/qore/classes/child-program-private-internal-member/child-program-private-internal-member.qtest": 1,
    "examples/test/qore/classes/class-identity-private-access/class-identity-private-access.qtest": 1,
    "examples/test/qore/classes/declaring-class-internal-member/declaring-class-internal-member.qtest": 1,
    "examples/test/qore/classes/deep-inheritance-chain/deep-inheritance-chain.qtest": 1,
    "examples/test/qore/classes/deleted-by-args-call-targets/deleted-by-args-call-targets.qtest": 1,
    "examples/test/qore/classes/deleted-by-args-variant-class/deleted-by-args-variant-class.qtest": 1,
    "examples/test/qore/classes/deleted-object-by-name/deleted-object-by-name.qtest": 1,
    "examples/test/qore/classes/imported-private-internal-paths/imported-private-internal-paths.qtest": 1,
    "examples/test/qore/classes/internal-member-base-typed/internal-member-base-typed.qtest": 1,
    "examples/test/qore/classes/member-import-order/member-import-order.qtest": 1,
    "examples/test/qore/classes/method-lookup-before-args/method-lookup-before-args.qtest": 1,
    "examples/test/qore/classes/named-call-member-code/named-call-member-code.qtest": 1,
    "examples/test/qore/classes/nested-call-targets/nested-call-locals.qtest": 1,
    "examples/test/qore/classes/nested-call-targets/nested-call-targets.qtest": 1,
    "examples/test/qore/classes/object-member-enumeration/object-member-enumeration.qtest": 1,
    "examples/test/qore/classes/object-val-visibility/object-val-visibility.qtest": 1,
    "examples/test/qore/classes/private-internal-dynamic-calls/private-internal-dynamic-calls.qtest": 1,
    "examples/test/qore/classes/private-internal-member-init/private-internal-member-init.qtest": 1,
    "examples/test/qore/classes/private-internal-member-type/private-internal-member-type.qtest": 1,
    "examples/test/qore/classes/private-internal-self-member/private-internal-self-member.qtest": 1,
    "examples/test/qore/classes/pseudo-method-receiver-type/pseudo-method-receiver-type.qtest": 1,
    "examples/test/qore/classes/reflection-private-internal/reflection-private-internal.qtest": 1,
    "examples/test/qore/classes/self-call-dispatch/self-call-dispatch.qtest": 1,
    "examples/test/qore/classes/sibling-private-member/sibling-private-member.qtest": 1,
    "examples/test/qore/classes/static-method-ref-deleted-program/static-method-ref-deleted-program.qtest": 1,
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
    """Returns True if the format runs a qcc executable: it has an argument (a command of literals only names no
    executable that the test built), it runs no tool (a command word of the format), it is no compile, dump or module
    build (its options), and none of its arguments is the qore or qcc binary"""
    if not args or NOT_RUN.search(fmt) or TOOL.search(fmt):
        return False
    return not any(QORE_OR_QCC.fullmatch(unwrap(arg)) for arg in args)


def strip_comments(text):
    """Returns the text with its comments blanked, its lines kept: a line that starts with # or is in a /* */ block
    that starts a line, and the rest of a line from a # that follows whitespace outside string literals"""
    out = []
    in_block = False
    for line in text.splitlines():
        s = line.lstrip()
        if in_block or s.startswith("/*"):
            in_block = "*/" not in (s if in_block else s[2:])
            out.append("")
            continue
        if s.startswith("#"):
            out.append("")
            continue
        i = 0
        while i < len(line):
            c = line[i]
            if c in "\"'":
                m = (STRING if c == '"' else SQ_STRING).match(line, i)
                if not m:
                    # a string that goes on to the next line (or a quote in a regex): the rest of the line stays
                    i = len(line)
                    break
                i = m.end()
                continue
            if c == "#" and i and line[i - 1] in " \t":
                break
            i += 1
        out.append(line[:i])
    return "\n".join(out)


def close_paren(text, i):
    """Returns the index of the bracket that closes the one before i, outside string literals (len(text) if none)"""
    depth = 1
    while i < len(text):
        c = text[i]
        if c == '"':
            s = STRING.match(text, i)
            i = s.end() if s else i + 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if not depth:
                return i
        i += 1
    return i


def split_top(text, sep):
    """Splits the text at its separators (, or +) outside brackets and string literals; a + of += or ++ does not
    split"""
    parts = []
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
        if c == "+" and text[i + 1:i + 2] in ("+", "="):
            cur += text[i:i + 2]
            i += 2
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == sep and not depth:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += c
        i += 1
    parts.append(cur.strip())
    return parts


def statement_end(text, i):
    """Returns the index of the ; that ends the statement at i (or of the , or ) that ends a default argument), outside
    brackets and string literals"""
    depth = 0
    while i < len(text):
        c = text[i]
        if c == '"':
            s = STRING.match(text, i)
            i = s.end() if s else i + 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if not depth:
                return i
            depth -= 1
        elif c in ";," and not depth:
            return i
        i += 1
    return i


def sprintf_parts(op):
    """Returns (format, arguments) of an operand that is a sprintf() call, or None"""
    m = SPRINTF.match(op)
    if not m:
        return None
    end = close_paren(op, m.end())
    if op[end + 1:].strip():
        return None
    # the format: the adjacent string literals that start the sprintf() arguments
    fmt = ""
    rest = op[m.end():end]
    while True:
        s = STRING.match(rest.lstrip())
        if not s:
            break
        fmt += s.group(1)
        rest = rest.lstrip()[s.end():]
    args = split_args(rest.lstrip()[1:]) if rest.lstrip().startswith(",") else []
    return fmt, args


def evaluate(expr, code, pos, depth, bindings=None):
    """Returns the alternatives of the command of an expression at pos: a list of (format, arguments, texts of the
    assignments that it is built of)"""
    # a ?: is not read: the whole expression is an argument
    if len(split_top(expr.replace("??", "  "), "?")) > 1:
        return [("%s", [expr.strip()], [])]
    alts = [("", [], [])]
    for op in split_top(expr, "+"):
        if not op:
            continue
        new = operand(op, code, pos, depth, bindings or {})
        alts = [(f1 + f2, a1 + a2, t1 + t2) for f1, a1, t1 in alts for f2, a2, t2 in new][:MAX_ALTERNATIVES]
    return alts


def operand(op, code, pos, depth, bindings):
    """Returns the alternatives of an operand of a concatenation (see evaluate())"""
    if LITERALS.fullmatch(op):
        return [("".join(STRING.findall(op)), [], [])]
    parts = sprintf_parts(op)
    if parts:
        return [(parts[0], parts[1], [])]
    if op.startswith("(") and close_paren(op, 1) == len(op) - 1:
        return evaluate(op[1:-1], code, pos, depth, bindings)
    # a variable, unless its name is the qore or qcc binary
    if IDENT.fullmatch(op) and not QORE_OR_QCC.fullmatch(op):
        if op in bindings:
            return bindings[op]
        if depth < MAX_DEPTH:
            values = resolve(op, code, pos, depth + 1)
            if values:
                return values
    return [("%s", [op], [])]


def parameter(code, decl):
    """Returns (function name, index of the parameter, position of the ( of the parameter list) if the declaration at
    decl is a parameter of a named function or method (name(... type var ...) {), else None"""
    depth = 0
    i = decl - 1
    while i >= 0:
        c = code.mask[i]
        if c in ")]}":
            depth += 1
        elif c in "([{":
            if not depth:
                break
            depth -= 1
        i -= 1
    if i < 0 or code[i] != "(" or not code[close_paren(code, i + 1) + 1:].lstrip().startswith("{"):
        return None
    m = FUNC_NAME.search(code.mask[max(0, i - 200):i])
    if not m or m.group(1) in KEYWORDS or m.group(1) == "sub":
        return None
    return m.group(1), len(split_top(code[i + 1:decl], ",")) - 1, i


def resolve(name, code, pos, depth):
    """Returns the alternatives of the value of a variable at pos (see evaluate()), from its assignments since its last
    declaration before pos, and for a parameter of a named function or method, from the arguments of the calls of the
    function in the text; an empty list if it has none of them"""
    start = 0
    for m in re.finditer(r"(?<![\w.$])(\*?[\w:]+(?:<[^;=()]*>)?)\s+" + name + r"\b(?=\s*(?:=(?![=~>])|;|,|\)|in\b))",
            code.mask[:pos]):
        if m.group(1) not in KEYWORDS:
            start = m.start()
    values = []
    param = parameter(code, start) if start else None
    if param:
        func, index, paren = param
        for m in re.finditer(r"(?<![\w$\\])" + func + r"\s*\(", code.mask):
            close = close_paren(code, m.end())
            # a definition of the function (this one or an overload) is not a call
            if m.end() - 1 == paren or code[close + 1:].lstrip().startswith("{"):
                continue
            args = split_top(code[m.end():close], ",")
            if index < len(args) and args[index]:
                values += evaluate(args[index], code, m.start(), depth)
    for m in re.finditer(r"(?<![\w.$>])" + name + r"\s*(\+=|=(?![=~>]))", code.mask[start:pos]):
        begin = start + m.end()
        end = statement_end(code, begin)
        if end > pos:
            # the statement of the call itself (string out = backquote(out ...))
            continue
        text = code[start + m.start():end]
        expr = code[begin:end]
        if m.group(1) == "+=":
            new = evaluate(expr, code, start + m.start(), depth)
            base = values or [("%s", [name], [])]
            values = [(f1 + f2, a1 + a2, t1 + t2 + [text]) for f1, a1, t1 in base for f2, a2, t2 in new]
        else:
            uses = values and name in split_top(expr, "+")
            new = evaluate(expr, code, start + m.start(), depth, {name: values} if uses else None)
            new = [(f, a, t + [text]) for f, a, t in new]
            # an assignment that uses the variable builds on each of its values; another one is an alternative
            values = new if uses else values + new
        values = values[:MAX_ALTERNATIVES]
    return values


class Code(str):
    """The text of a test without its comments, and its mask: the same text with the contents of its string literals
    blanked, in which calls, declarations and assignments are searched (not in the program text of a literal)"""
    def __new__(cls, text):
        code = super().__new__(cls, text)
        code.mask = STRING.sub(lambda m: '"' + " " * (len(m.group(0)) - 2) + '"', text)
        return code


def calls(code):
    """Yields (line number, call text, alternatives) for each backquote()/system() of the text of a test without its
    comments: the call text is the call and the assignments that its command is built of, and the alternatives are the
    (format, arguments) of its command"""
    code = Code(code)
    for m in CALL.finditer(code.mask):
        end = close_paren(code, m.end())
        alts = evaluate(split_top(code[m.end():end], ",")[0], code, m.start(), 0)
        texts = []
        for _, _, t in alts:
            texts += [x for x in t if x not in texts]
        yield code.count("\n", 0, m.start()) + 1, "\n".join([code[m.start():end + 1]] + texts), \
            [(f, a) for f, a, _ in alts]


def flags(fmt, args):
    """Returns True if a command of the format and arguments runs a qcc executable with 2>&1 and keeps its output"""
    return "2>&1" in fmt and not DISCARDED.search(fmt) and runs_executable(fmt, args)


def normalized(text):
    """Returns text with its runs of whitespace replaced by one space"""
    return " ".join(text.split())


def flagged_calls(rel, text):
    """Returns (line number, call) for each run of a qcc executable with 2>&1 of a test that is not allowed"""
    # only a test that builds executables with qcc and has 2>&1 runs them so
    if "2>&1" not in text:
        return []
    # comments do not count
    code = strip_comments(text)
    if not USES_QCC.search(code):
        return []
    out = []
    for number, call, alts in calls(code):
        if not any(flags(fmt, args) for fmt, args in alts):
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
            code = strip_comments((ROOT / rel).read_text(errors="replace"))
            matching = [alts for _, call, alts in calls(code) if normalized(anchor) in normalized(call)]
            self.assertEqual(1, len(matching), "%s: %d calls with the anchor %r (%s); an anchor names one call"
                % (rel, len(matching), anchor, why))
            self.assertTrue(any(flags(fmt, args) for fmt, args in matching[0]),
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

    def test_detects_built_commands(self):
        # the commands built by concatenation or in a variable are found (qore#7818)
        for snippet in (
                # concatenation, also over lines, in parentheses and of a sprintf()
                'assertEq(expected, backquote(shellQuote(program) + " 2>&1"));',
                'out = backquote(shellQuote(exe) + " 2>&1", \\rc);',
                'int rc = system(exe + " 2>&1");',
                'out = backquote("LD_LIBRARY_PATH=" + quote(libdir) + " DYLD_LIBRARY_PATH=" + quote(libdir) + " "\n'
                '    + quote(exe) + " 2>&1", \\status);',
                'out = backquote((shellQuote(exe) + " 2>&1"));',
                'out = backquote(sprintf("LD_LIBRARY_PATH=%s %s", libqore_dir, exe) + " 2>&1");',
                'out = backquote("exec " + shellQuote(exe) + " </dev/null 2>&1", \\rc);',
                # a parameter, which is an argument when the calls of its function do not say what it is
                'private string run(string cmd) {\n    return backquote(cmd + " 2>&1");\n}',
                # a variable: a sprintf(), a concatenation, a +=, an = that uses it
                'string cmd = sprintf("LD_LIBRARY_PATH=%s %s -b 2>&1", libqore_dir, exe);\n'
                'string out = backquote(cmd, \\rc);',
                'string cmd = sprintf("%s -b", shellQuote(exe));\ncmd += " 2>&1";\nout = backquote(cmd);',
                'string cmd = shellQuote(exe);\ncmd = cmd + " 2>&1";\nout = backquote(cmd);',
                'string env = sprintf("LD_LIBRARY_PATH=%s", libqore_dir);\n'
                'out = backquote(env + " " + shellQuote(exe) + " 2>&1");',
                'string full = "env LD_LIBRARY_PATH=" + quote(lib_dir) + " " + exe + " 2>&1";\nout = backquote(full);',
                # a variable of a variable
                'string bin = tmp.path + "/app";\nstring cmd = bin + " 2>&1";\nout = backquote(cmd);',
                # a variable assigned in branches: one alternative runs an executable
                'string cmd;\nif (ast) {\n    cmd = sprintf("%s -b %s", qore_bin, src);\n} else {\n'
                '    cmd = shellQuote(exe);\n}\nout = backquote(cmd + " 2>&1");',
                # a parameter of a function that one of its calls runs an executable with
                'private string run(string cmd) {\n    return backquote(cmd + " 2>&1");\n}\n'
                'run(sprintf("%s -o %s %s", qcc_bin, exe, src));\nassertEq(expected, run(sprintf("%s %s", env, exe)));',
                # a default argument
                'private string run(string exe, string prefix = "") {\n'
                '    return backquote(prefix + shellQuote(exe) + " 2>&1");\n}'):
            self.assertEqual(1, len(check_text("examples/test/x.qtest", QCC + snippet)), snippet)
        # not runs of qore or qcc, compiles and tools built so, nor the converted runs, nor comments, nor runs whose
        # output is discarded
        for snippet in (
                'out = backquote(shellQuote(qore_bin) + " -b --exec-mode=ast " + src + " 2>&1");',
                'out = backquote(qcc_bin + " -o " + exe + " " + src + " 2>&1", \\rc);',
                'out = backquote(shellQuote(TestInterpreter::qcc_compiler() ?? "qcc") + " -o " + exe + " 2>&1");',
                'out = backquote(shellQuote(q) + " -b --compile --output " + o + " " + s + " 2>&1");',
                'out = backquote(cc + " -m -o " + qmod + " " + src + " 2>&1");',
                'out = backquote("nm -u " + shellQuote(exe) + " 2>&1");',
                'out = backquote("ldd " + exe + " 2>&1");',
                'out = backquote("cmake --build " + dir + " 2>&1");',
                'string cmd = "codesign -f -s - " + shellQuote(exe);\ncmd += " 2>&1";\nout = backquote(cmd);',
                'string cmd = sprintf("ln -s %s %s", a, b);\nout = backquote(cmd + " 2>&1");',
                'out = backquote("qore -ne \'print(1);\' 2>&1");',
                'out = backquote("systemd-run --user --scope -- true 2>&1", \\rc);',
                # a variable whose name is the qore or qcc binary is not resolved (an argument as it is)
                'string qore = QORE_ARGV[0];\nout = backquote(qore + " -b " + src + " 2>&1");',
                # a variable that runs qore or qcc
                'string cmd = sprintf("%s -b --exec-mode=ast %s", qore_bin, src);\nout = backquote(cmd + " 2>&1");',
                'string cmd = qcc_bin;\ncmd += " -o " + exe + " " + src;\nout = backquote(cmd + " 2>&1");',
                # the last declaration of a variable before the call counts
                'string cmd = shellQuote(exe);\nstring cmd = sprintf("%s %s", qore_bin, src);\n'
                'out = backquote(cmd + " 2>&1");',
                # a parameter of a function that its calls give only qore and qcc runs
                'private string run(string cmd) {\n    return backquote(cmd + " 2>&1");\n}\n'
                'run(sprintf("%s -o %s %s", qcc_bin, exe, src));\nassertEq(expected, run(sprintf("%s %s", qore_bin, '
                'src)));',
                # converted runs
                'hash<auto> r = ExecModeHarness::run_command(shellQuote(exe) + " -b");',
                'string cmd = sprintf("%s -b", exe);\nhash<auto> r = ExecModeHarness::run_command(cmd);',
                # comments, also at the end of a line and in a block, and a call in a string literal
                '# out = backquote(shellQuote(exe) + " 2>&1");',
                'int x = 1; # out = backquote(shellQuote(exe) + " 2>&1");',
                '/*\n    out = backquote(shellQuote(exe) + " 2>&1");\n*/',
                'string prog = "out = backquote(exe + \\" 2>&1\\");";',
                # a run whose output is discarded
                'int rc = system(shellQuote(exe) + " > /dev/null 2>&1");',
                'string cmd = shellQuote(exe);\ncmd += " > /dev/null 2>&1";\nint rc = system(cmd);'):
            self.assertEqual([], check_text("examples/test/x.qtest", QCC + snippet), snippet)
        # an allow-list anchor can be in an assignment that the command is built of
        run = QCC + ('string cmd = sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s -b 2>&1",\n'
            '    shellQuote(libdir), shellQuote(libdir), shellQuote(exe));\nstring out = backquote(cmd, \\rc);')
        self.assertEqual([], check_text(
            "examples/test/ir/aot-standalone-format-floor/aot-standalone-format-floor.qtest", run))
        self.assertEqual(1, len(check_text("examples/test/ir/Other.qtest", run)))


if __name__ == "__main__":
    unittest.main()
