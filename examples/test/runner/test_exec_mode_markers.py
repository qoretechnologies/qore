#!/usr/bin/env python3
"""The tests that use examples/test/ExecModeHarness.qm declare whether the execution mode of the test itself matters.

A harness test runs child processes that each set their own execution mode (--exec-mode).  One whose checks are all
made in those child processes declares

    # test-exec-mode: children - <why>

and the CI jobs that give every test an execution mode (QORE_TEST_OPTS with --exec-mode) leave it out with
QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN=1 (run_tests.sh): the default-mode jobs run the same child processes, so it would
repeat their checks.  One that also makes checks in its own process (code run in its own Program, results computed
in its own process, its own options or mode given to a child process) declares

    # test-exec-mode: parent - <why>

and runs in every job.  A test that does not use the harness may also declare "children" when it makes its checks in
child processes of its own binary (QORE_ARGV[0]) that it starts without an execution mode: they run in the default
mode in every job, so the default-mode jobs make the same checks.  These tests fail when a harness test declares
neither, when a test that does not use the harness declares "parent" or declares "children" without starting such
child processes, when a test that declares "children" does what only a "parent" test may do, when the CI jobs set
QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN in a job whose tests get no execution mode, and they check that run_tests.sh
leaves out exactly the "children" tests.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_watchdog import STAND_IN_VERSION, stand_in_build  # noqa: E402
TESTS = ROOT / "examples/test"

MARKER = re.compile(r"(?m)^# test-exec-mode: (?P<kind>\S+) - (?P<why>\S.*)$")
ANY_MARKER = re.compile(r"(?m)^\s*#\s*test-exec-mode:")
HARNESS = re.compile(r"(?m)^\s*%requires\s+\S*ExecModeHarness(\.qm)?\s*$")
INCLUDE = re.compile(r"(?m)^\s*%include\s+(\S+)\s*$")

# what the code of a "children" test may not do in its own process (strings and comments do not count): read or set
# its own execution mode, read its own options (QORE_ARGV beyond the binary, QORE_TEST_OPTS), run code in a Program
# of its own, run the checks modules and include files whose results the tests compare in their own process, run
# the C++ unit tests or fork
FORBIDDEN = re.compile(r"\b(getExecMode|setExecMode|QORE_TEST_OPTS|callFunction|callMethod|callStaticMethod|"
                       r"run_debug_unit_tests|get_results|get_lines|fork)\b|\bQORE_ARGV(?!\[0\])")
# a Program of its own may only parse code: a parse error does not depend on the execution mode
PROGRAM = re.compile(r"\bProgram\b")
PROGRAM_RUN = re.compile(r"\.\s*run\s*\(")


def strip_code(source):
    """Returns Qore source without its comments and with its strings emptied"""
    out = []
    i = 0
    n = len(source)
    while i < n:
        c = source[i]
        if c == "#":
            j = source.find("\n", i)
            i = n if j < 0 else j
            continue
        if source.startswith("/*", i):
            j = source.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
            continue
        if c == '"':
            i += 1
            while i < n and source[i] != '"':
                i += 2 if source[i] == "\\" else 1
            i += 1
            out.append('""')
            continue
        if c == "'":
            j = source.find("'", i + 1)
            i = n if j < 0 else j + 1
            out.append("''")
            continue
        out.append(c)
        i += 1
    return "".join(out)


def sources(test):
    """Returns the test and the files it includes (recursively), which run in the test's process"""
    seen = []
    todo = [test]
    while todo:
        path = todo.pop()
        if path in seen or not path.is_file():
            continue
        seen.append(path)
        for match in INCLUDE.finditer(path.read_text(errors="replace")):
            todo.append((path.parent / match.group(1)).resolve())
    return seen


# a child process of the test's own binary: QORE_ARGV[0] given to a command it runs (strings and comments do not count)
# the test's own binary: QORE_ARGV[0], or the interpreter that examples/test/TestInterpreter.qm locates for it (which is
# QORE_ARGV[0] in an interpreted test, and never the executable of a test compiled with qcc)
CHILD_BINARY = re.compile(r"\bQORE_ARGV\[0\]|\bTestInterpreter::qore_interpreter\s*\(")


def starts_children(test):
    """Returns True if the code of a test or of the files it includes runs its own binary in a child process"""
    for path in sources(test):
        code = strip_code(path.read_text(errors="replace"))
        if CHILD_BINARY.search(code) and re.search(r"\b(backquote|system|exec)\s*\(|\bProcess\b", code):
            return True
    return False


def uses_harness(test):
    return any(HARNESS.search(path.read_text(errors="replace")) for path in sources(test))


def all_tests():
    tests = sorted(TESTS.rglob("*.qtest"))
    tests += sorted((ROOT / "modules").glob("*/test/**/*.qtest"))
    return tests


def parent_only_code(test):
    """Returns what the code of a test or of the files it includes does that only a "parent" test may do"""
    found = []
    for path in sources(test):
        code = strip_code(path.read_text(errors="replace"))
        found += sorted({m.group(0) for m in FORBIDDEN.finditer(code)})
        if PROGRAM.search(code) and PROGRAM_RUN.search(code):
            found.append("Program ... .run()")
    return found


class ExecModeMarkerTest(unittest.TestCase):
    def test_harness_tests_declare_marker(self):
        harness_tests = 0
        kinds = {"children": 0, "parent": 0}
        non_harness_children = []
        for test in all_tests():
            text = test.read_text(errors="replace")
            name = test.relative_to(ROOT)
            markers = MARKER.findall(text)
            self.assertEqual(len(ANY_MARKER.findall(text)), len(markers),
                             "%s: a test-exec-mode declaration has the form '# test-exec-mode: <kind> - <why>'" % name)
            if not uses_harness(test):
                if markers:
                    self.assertEqual([("children", markers[0][1])], markers, "%s does not use ExecModeHarness.qm: "
                                     "it declares no test-exec-mode, or 'children' once" % name)
                    self.assertTrue(starts_children(test), "%s does not use ExecModeHarness.qm: it declares "
                                    "'children' only if it runs its own binary (QORE_ARGV[0]) in child processes"
                                    % name)
                    non_harness_children.append(str(name))
                continue
            harness_tests += 1
            self.assertEqual(1, len(markers), "%s uses ExecModeHarness.qm: it declares '# test-exec-mode: children "
                             "- <why>' or '# test-exec-mode: parent - <why>' once" % name)
            kind = markers[0][0]
            self.assertIn(kind, kinds, "%s: the kind is 'children' or 'parent'" % name)
            kinds[kind] += 1
        # the harness tests are found, and most of them are child-process tests
        self.assertGreater(harness_tests, 200)
        self.assertGreater(kinds["children"], kinds["parent"])
        self.assertGreater(kinds["parent"], 0)
        # the tests that run their checks in default-mode child processes without the harness:
        # - AOTConstantImportStale (#5798) and AOTDumpInfoPendingConstants (#5799): their checks run qcc and the
        #   programs and modules it compiles in child processes
        # - AOTDumpInfoFindCallRef (#6235): its checks run qcc --dump-info in child processes
        # - AOTInstanceOfTypePath (#5976): its checks run an AOT-compiled module and the same module's source in the
        #   AST interpreter in child processes
        # - AOTModuleDelClosure (#6015): its checks compile modules and scripts with qcc and run the interpreter and the
        #   compiled programs in child processes
        # - AOTNestedFind (#5974): its checks run the interpreter, qcc and the compiled programs in child processes
        # - AOTModuleMissingDependencyFallback (#7004): its checks compile modules and run drivers that load them, in
        #   child processes that set their own --exec-mode, and compiled with qcc
        # - GenericCacheRelease: GenericCacheRelease.qr runs in child processes that set their own --exec-mode
        # - JitTeardownAtExit-<n> (#6390): JitTeardownAtExit.qr runs in child processes that set their own --exec-mode
        # - index-writer-filesystem (#6500): each execution mode runs in a child process of its own, which loads the
        #   modules once
        # - qlib-module-domains: each module is loaded first by a restricted Program in a child process, as a loaded
        #   module is not checked again for the next Program (#5803)
        # - QlibAllModulesLoad-<n>: each qlib module is loaded by a restricted Program in a child process of its own,
        #   as a loaded module is not checked again for the next Program (#5803, #6438)
        # - qlib-restricted-callers: each case loads the modules in a restricted Program in a child process of its own
        #   (#6337)
        # - nested-module-parse: the module is loaded in a child process whose module path holds it (#5803, #5938)
        # - RestSchemaModuleLazyTypes-<n>: the modules are loaded and compared in child processes
        # - WebSocketProgramEnd: the server whose Program ends with an open connection is a child process; the
        #   WebSocket client in the test's own process only drives it
        # - TerminalIoRestrictedLoad (#6438): the modules are loaded first in child processes that set their own
        #   --exec-mode and in a qcc executable
        # - thread-data-clear-reentrant (#7265): its probe runs in child processes that set their own --exec-mode
        # - AOTModuleForeignThread (#6045 follow-up): it compiles a module with qcc and runs its probes in child
        #   processes that set their own --exec-mode
        # - dataframe-default-encoding (#6092): its checks run in a child process whose default character encoding is
        #   ISO-8859-1
        # - dataframe-default-encoding-names (#6100): its checks run in child processes whose default character
        #   encodings differ
        # - default-encoding-tokens (#6604): its checks run in child processes whose default character encodings
        #   differ
        # - default-encoding-keys (#6604): its checks run in child processes whose default character encodings
        #   differ
        # - ParseStackBoundary-2 (#6047): the module loads that it checks in the primary thread run in child processes
        # - ParseStackBoundary-5 (#6420): the AOT module loads that it checks run in child processes (the fixture)
        # - parse-load-module-location (#6047): its module loads run in child processes, one for each stack size
        # - dataframe-aot-plugin-helpers (#6130): its probes run in child processes that set their own --exec-mode,
        #   in qcc executables and with a module compiled by qcc
        # - dataframe-plugin-exception-handler (#6135): its probes run in child processes that set their own
        #   --exec-mode
        # - plugin-order-identity (#6136): its probes run in child processes that set their own --exec-mode, in qcc
        #   executables and with a module compiled by qcc
        # - dataframe-inline-mask-subscript (#6141): its probes run in child processes that set their own --exec-mode
        # - declaration-initializer-scope (#6268): its scripts run in child processes that set their own --exec-mode
        #   and in qcc executables
        # - parquet-memory-pool (#6171): its checks run in child processes without an execution mode, natively and
        #   under valgrind
        # - broken-list-range-past-end (#5757): its check runs a legacy script, which only the AST interpreter runs,
        #   in a child process with --exec-mode=ast
        # - AOTModuleInitException (#6217): its checks run in child processes of the interpreter and of qcc
        #   executables
        # - deferred-callback-runtime-parse-options (#6208): its checks run in child processes that set their own
        #   execution modes
        # - thread-reaper (#6433): its checks run in child processes without an execution mode, whose native thread
        #   reapers it inspects
        # - unhandled-exception-report: its checks run in child processes without an execution mode, whose threads
        #   raise unhandled exceptions at the same time
        # - asyncio_callback_exception_stderr (#6275): its checks run in child processes without an execution mode,
        #   whose standard output and error output it reads
        # - asyncio_poll_complete_notify (#6804): its checks run in child processes without an execution mode, whose
        #   standard output and error output it reads
        # - QUnit result-counts (#2297): the test whose results it counts runs in child processes without an
        #   execution mode
        # - JITFinalizeCancellation (#6305): its checks run in child processes that set their own --exec-mode and arm
        #   a debug hook in them
        # - JITFinalizeCancellation (#6305, #6313): its checks run in child processes that set their own --exec-mode
        #   and arm a debug hook in them
        # - entry-cpp-exception (#6255): its checks run in child processes that set their own --exec-mode and in qcc
        #   executables, which arm a debug hook
        # - frame-check-stack (#6124, #6414): its checks run a fixture that sets its own execution mode, in child
        #   processes and in qcc executables
        # - RestSchemaImportTool (#6190): its checks run tools/rest-schema-import.qr in child processes
        # - ir-guard-profile-types (#6421): its checks run in child processes that set their own --exec-mode
        # - ModuleCleanupCancel (#6510): its checks run in child processes that set their own --exec-mode
        # - include-failure-diagnostics (#6509): its checks run the command line in child processes started without an
        #   execution mode
        # - DeserializationSizes (#6502): each case is deserialized in a child process started without an execution mode
        # - FtpPortAcceptUnreferenced (#6525): each case runs in a child process started without an execution mode
        # - library-registry-origin (#6520): each case runs in a child process whose registering Program sets its own
        #   execution mode
        # - aot-dependency-domains (#6351): its checks load modules from source and compiled with qcc in child
        #   processes, as a module is loaded once in a process
        # - WebSocketAsyncTextEncoding (#6619): each case runs in a child process with a default encoding of its own
        # - WebSocketHandlerRestrictedTimers (#6836, batch AO): a sandboxed restricted Program loads the modules first
        #   in child processes that set their own execution modes and in a qcc executable, as a module is loaded once
        #   in a process
        # - globalsetup-stop (#6659): the tests whose global setup stops run in child processes of its own binary
        # - global-error-label (#6925): the test whose global setup or teardown raises an exception runs in child
        #   processes of its own binary started without an execution mode
        # - TabularProfilerEncoding (#6663): each case runs in a child process with a default encoding of its own
        # - QdpNoTty (#7125): its checks run qdp in child processes in a new session, without a controlling terminal
        # - QdpAppActionOptions (#7126): its checks run qdp in child processes of its own binary
        # - QdpListenNoTty (#7130): its checks run qdp in child processes in a new session, with stdin not a terminal
        # - QdpInfo (#7386): its checks run qdp in child processes of its own binary
        # - FileForegroundTty (#7387): its checks run in child processes of its own binary, on a new terminal
        # - type-name-encoding (#6814): each case runs in a child process with a default encoding of its own
        # - AOTFusedFoldVerify (#6549): its checks compile with qcc and run the module in a child process started
        #   without an execution mode
        # - aot-cross-module-caller-mask (#7101): its checks run in child processes that set their own --exec-mode
        # - stdio-sandbox-change (#7670): its checks run in child interpreters that set their own --exec-mode, as the
        #   calls can close or redirect the standard streams of the process that makes them
        # - short-string-native-access (#7038): its probes run in child processes that set their own --exec-mode and
        #   as qcc executables
        # - short-string-access of the dataframe, i18n, logger_bin and tokenizer modules (#7062): their probes run in
        #   child processes that set their own --exec-mode and as qcc executables
        # - reflection-module-init-barrier-oom: its script runs in child processes that set their own execution
        #   modes and as a qcc executable, which arm a debug hook and whose failed module load ends the process
        # - AOTModuleBuiltinApiIdentity (#7183): its checks compile modules and load them in child processes of its own
        #   binary started without an execution mode
        # - AOTGuardedMethodEntryReturnKind (#6860): its checks run qcc in child processes started without an execution
        #   mode
        # - HttpServerRecvGrowFailure (#7417): the server whose read fails runs in a child process of the qore
        #   interpreter in the default mode, under a limit of its address space
        # - HttpServerControllerCallbackError (#7698): each server whose controller callback raises runs in a child
        #   process of the qore interpreter started without an execution mode, whose output the test checks
        # - module-init-frames (#7289): its probes run in child processes that set their own --exec-mode, with the
        #   modules loaded from source and compiled with qcc, as a module is initialized once in a process
        # - type-retirement-aot-parent (#7464): its script runs in child processes without an execution mode, with the
        #   modules loaded from source and compiled with qcc
        # - self-held-program-module-decls-exit (#7533): its scripts run in child processes without an execution mode,
        #   as the checks are of the exit of the process that holds the Program
        # - AOTEmptyRegex (#7623): its checks run in child processes that set their own execution modes and in qcc
        #   executables
        # - sandbox-timezone-paths-library (#7566): its checks run in child interpreters without an execution mode,
        #   started with a module directory of library code, as a module's initialization runs once in a process
        self.assertEqual(sorted(["examples/test/ir/AOTConstantImportStale.qtest",
                                 "examples/test/ir/AOTEmptyRegex.qtest",
                                 "examples/test/ir/AOTGuardedMethodEntryReturnKind.qtest",
                                 "examples/test/qore/classes/type-retirement-aot-parent.qtest",
                                 "examples/test/ir/AOTModuleBuiltinApiIdentity.qtest",
                                 "examples/test/qore/security/aot-cross-module-caller-mask.qtest",
                                 "examples/test/qore/misc/alloc-failure/alloc-failure.qtest",
                                 "examples/test/qore/misc/aot-dependency-domains/aot-dependency-domains.qtest",
                                 "examples/test/ir/AOTFusedFoldVerify.qtest",
                                 "examples/test/ir/JITFinalizeCancellation.qtest",
                                 "examples/test/ir/ir-guard-profile-types/ir-guard-profile-types.qtest",
                                 "examples/test/ir/AOTInstanceOfTypePath.qtest",
                                 "examples/test/ir/AOTDumpInfoPendingConstants.qtest",
                                 "examples/test/ir/AOTDumpInfoFindCallRef.qtest",
                                 "examples/test/ir/AOTModuleInitException.qtest",
                                 "examples/test/qore/misc/process-end-callbacks/process-end-callbacks.qtest",
                                 "examples/test/qore/classes/Serializable/DeserializationSizes.qtest",
                                 "examples/test/qore/security/method-call-program-context.qtest",
                                 "examples/test/qore/security/stdio-sandbox-change.qtest",
                                 "examples/test/qore/classes/FtpClient/FtpPortAcceptUnreferenced.qtest",
                                 "examples/test/qore/functions/user-functional-domains/library-registry-origin.qtest",
                                 "examples/test/qore/lists/broken-list-range-past-end.qtest",
                                 "examples/test/qore/threads/thread-reaper/thread-reaper.qtest",
                                 "examples/test/qore/diagnostics/exception-report-limit.qtest",
                                 "examples/test/qore/threads/unhandled-exception-report.qtest",
                                 "examples/test/qore/threads/frame-check-stack.qtest",
                                 "examples/test/qore/threads/fork-child-signal-thread.qtest",
                                 "examples/test/qore/classes/Program/self-held-program-module-decls-exit.qtest",
                                 "examples/test/qore/classes/AsyncIoController/asyncio_callback_exception_stderr.qtest",
                                 "examples/test/qore/classes/AsyncIoController/asyncio_poll_complete_notify.qtest",
                                 "examples/test/qore/classes/AsyncIoController/asyncio_cancel_fork.qtest",
                                 "examples/test/qore/classes/AsyncIoController/asyncio_worker_fork.qtest",
                                 "examples/test/ir/AOTModuleForeignThread.qtest",
                                 "examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary-2.qtest",
                                 "examples/test/qore/parser/parse-stack-boundary/ParseStackBoundary-5.qtest",
                                 "examples/test/module-cpp-api/parse-load-module-location/"
                                 "parse-load-module-location.qtest",
                                 "examples/test/qore/strings/short-string-native-access.qtest",
                                 "examples/test/qore/security/sandbox-module-source-policy-library.qtest",
                                 "examples/test/qore/threads/deferred-callback-runtime-parse-options.qtest",
                                 "examples/test/qore/misc/declaration-initializer-scope.qtest",
                                 "examples/test/qore/misc/reflection-module-init-barrier-oom.qtest",
                                 "examples/test/qore/threads/thread-data-clear-reentrant.qtest",
                                 "examples/test/qore/vars/timezone-spelling-casefold.qtest",
                                 "examples/test/ir/AOTModuleDelClosure.qtest",
                                 "examples/test/ir/AOTNestedFind.qtest",
                                 "examples/test/ir/AOTModuleMissingDependencyFallback.qtest",
                                 "examples/test/module-cpp-api/plugin-order/plugin-order-identity.qtest",
                                 "examples/test/ir/GenericCacheRelease.qtest",
                                 "examples/test/ir/IrEnvironmentLookups.qtest",
                                 "examples/test/qore/threads/restricted-first-modules.qtest",
                                 "examples/test/qore/security/sandbox-timezone-paths-library.qtest",
                                 "examples/test/qore/parser/include-failure-diagnostics.qtest",
                                 "examples/test/ir/jit-teardown-at-exit/JitTeardownAtExit-1.qtest",
                                 "examples/test/ir/jit-teardown-at-exit/JitTeardownAtExit-2.qtest",
                                 "examples/test/ir/jit-teardown-at-exit/JitTeardownAtExit-3.qtest",
                                 "examples/test/qore/functions/user-functional-domains/nested-module-parse.qtest",
                                 "examples/test/qore/functions/user-functional-domains/qlib-module-domains.qtest",
                                 "examples/test/qore/functions/user-functional-domains/index-writer-filesystem.qtest",
                                 "examples/test/qore/functions/user-functional-domains/QlibAllModulesLoad-1.qtest",
                                 "examples/test/qore/functions/user-functional-domains/QlibAllModulesLoad-2.qtest",
                                 "examples/test/qore/functions/user-functional-domains/qlib-restricted-callers.qtest",
                                 "examples/test/qlib/DataProvider/TabularProfilerEncoding.qtest",
                                 "examples/test/qlib/DataProvider/QdpNoTty.qtest",
                                 "examples/test/qlib/DataProvider/QdpAppActionOptions.qtest",
                                 "examples/test/qlib/DataProvider/QdpListenNoTty.qtest",
                                 "examples/test/qlib/DataProvider/QdpInfo.qtest",
                                 "examples/test/qore/classes/File/FileForegroundTty.qtest",
                                 "examples/test/qore/parser/type-name-encoding.qtest",
                                 "examples/test/qlib/HttpClientIo/ConnectionManagerTeardown.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaImportTool.qtest",
                                 "examples/test/qlib/HttpClientIo/ModuleCleanupCancel.qtest",
                                 "examples/test/qlib/HttpServer/HttpServerRecvGrowFailure.qtest",
                                 "examples/test/qlib/HttpServer/HttpServerControllerCallbackError.qtest",
                                 "examples/test/qlib/ConnectionProvider/NamedConnectionCreation.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-1.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-2.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-3.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-4.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-5.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-6.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-7.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-8.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-9.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-10.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-11.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-12.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-13.qtest",
                                 "examples/test/qlib/RestSchemaActions/RestSchemaModuleLazyTypes-14.qtest",
                                 "examples/test/qlib/WebSocketClient/WebSocketAsyncTextEncoding.qtest",
                                 "examples/test/qlib/WebSocketHandler/WebSocketHandlerRestrictedTimers.qtest",
                                 "examples/test/qlib/WebSocketHandler/WebSocketProgramEnd.qtest",
                                 "examples/test/qlib/TerminalIoRestrictedLoad/TerminalIoRestrictedLoad.qtest",
                                 "examples/test/qore/classes/Program/entry-cpp-exception/entry-cpp-exception.qtest",
                                 "examples/test/qlib/QUnit/global-error-label.qtest",
                                 "examples/test/qlib/QUnit/globalsetup-stop.qtest",
                                 "examples/test/qlib/QUnit/result-counts.qtest",
                                 "modules/dataframe/test/dataframe-default-encoding.qtest",
                                 "modules/dataframe/test/dataframe-default-encoding-names.qtest",
                                 "modules/tokenizer/test/default-encoding-tokens.qtest",
                                 "modules/protobuf/test/default-encoding-keys.qtest",
                                 "modules/dataframe/test/dataframe-aot-plugin-helpers.qtest",
                                 "modules/dataframe/test/dataframe-plugin-exception-handler.qtest",
                                 "modules/dataframe/test/dataframe-inline-mask-subscript.qtest",
                                 "modules/dataframe/test/parquet-memory-pool.qtest",
                                 "modules/dataframe/test/short-string-access.qtest",
                                 "modules/i18n/test/short-string-access.qtest",
                                 "modules/logger_bin/test/short-string-access.qtest",
                                 "modules/tokenizer/test/short-string-access.qtest",
                                 "examples/test/qore/misc/module-init-frames.qtest"]),
                         sorted(non_harness_children))

    def test_children_tests_check_nothing_in_their_own_process(self):
        for test in all_tests():
            match = MARKER.search(test.read_text(errors="replace"))
            if not match or match.group("kind") != "children":
                continue
            with self.subTest(test=str(test.relative_to(ROOT))):
                self.assertEqual([], parent_only_code(test),
                                 "a test that declares 'test-exec-mode: children' makes all its checks in child "
                                 "processes; this code runs in its own process, so it declares 'parent'")

    def test_scan_finds_parent_checks(self):
        # the scan finds each kind of check in the test's own process, and not the same text in a string or comment
        with tempfile.TemporaryDirectory(prefix="qore-exec-mode-markers-") as directory:
            root = Path(directory)
            (root / "checks.qi").write_text("sub f() { ChecksModule::get_results(); }\n")
            cases = {
                "string sub mode() { return getExecMode(); }": ["getExecMode"],
                "string opts = ENV.QORE_TEST_OPTS;": ["QORE_TEST_OPTS"],
                "string cmd = QORE_ARGV.join(\" \");": ["QORE_ARGV"],
                "string arg = QORE_ARGV[1];": ["QORE_ARGV"],
                "Program p(); p.parse(src, \"x\"); p.callFunction(\"f\");": ["callFunction"],
                "Program p(); p.parse(src, \"x\"); p.run();": ["Program ... .run()"],
                "%include checks.qi\n": ["get_results"],
                # only the binary, a parse in a Program of its own, and text in strings and comments are allowed
                "string qore = QORE_ARGV[0];": [],
                "Program p(); p.parse(src, \"x\");": [],
                "string src = \"Program p(); p.callFunction('f'); getExecMode();\";": [],
                "# getExecMode() and QORE_ARGV[1] in a comment\n/* p.callFunction() */": [],
                "string s = 'getExecMode()';": [],
            }
            for code, expected in cases.items():
                with self.subTest(code=code):
                    test = root / "case.qtest"
                    test.write_text("%modern\n" + code + "\n")
                    self.assertEqual(expected, parent_only_code(test))

    def test_scan_finds_child_processes(self):
        # a test that runs its own binary in a child process is found, and not the same text in a string or comment
        with tempfile.TemporaryDirectory(prefix="qore-exec-mode-children-") as directory:
            root = Path(directory)
            cases = {
                "string out = backquote(QORE_ARGV[0] + \" child.q\");": True,
                "int rc = system(sprintf(\"%s child.q\", QORE_ARGV[0]));": True,
                "string qore = QORE_ARGV[0];": False,
                "string out = backquote(\"ls\");": False,
                "# backquote(QORE_ARGV[0])\nstring s = \"system(QORE_ARGV[0])\";": False,
            }
            for code, expected in cases.items():
                with self.subTest(code=code):
                    test = root / "case.qtest"
                    test.write_text("%modern\n" + code + "\n")
                    self.assertEqual(expected, starts_children(test))

    def test_ci_excludes_only_in_explicit_mode_jobs(self):
        text = (ROOT / ".gitlab-ci.yml").read_text()
        jobs = re.split(r"(?m)^(?=[a-z][\w-]*:\s*$)", text)
        excluding = []
        for job in jobs:
            name = job.split(":", 1)[0]
            opts = re.search(r"(?m)^\s+QORE_TEST_OPTS:\s*'([^']*)'", job)
            exclude = re.search(r'(?m)^\s+QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN:\s*"?1"?\s*$', job)
            explicit = bool(opts and re.search(r"--exec-mode[= ]", opts.group(1)))
            if explicit or exclude:
                with self.subTest(job=name):
                    self.assertEqual(explicit, bool(exclude), "a job leaves out the child-process tests exactly "
                                     "when its tests get an execution mode")
            if exclude:
                excluding.append(name)
        self.assertEqual(["test-ast-ubuntu-amd64", "test-ir-ubuntu-amd64", "test-jit-ubuntu-amd64",
                          "test-tiered-ubuntu-amd64"], excluding)
        # the global variables give no execution mode, so the default-mode jobs run every test
        global_variables = re.search(r"(?ms)^variables:\n.*?^\S", text)
        self.assertIsNotNone(global_variables)
        self.assertNotRegex(global_variables.group(0), r"QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN|--exec-mode")


class RunnerExclusionTest(unittest.TestCase):
    """run_tests.sh with a stand-in for qore that records the tests it runs"""

    def run_runner(self, env_extra):
        with tempfile.TemporaryDirectory(prefix="qore-runner-exec-mode-") as directory:
            root = Path(directory)
            tests = root / "examples/test"
            (tests / "docker_test").mkdir(parents=True)
            (tests / "docker_test/ci-job-processes.sh").symlink_to(
                ROOT / "examples/test/docker_test/ci-job-processes.sh")
            (tests / "children.qtest").write_text("#!/usr/bin/env qore\n# test-exec-mode: children - child processes\n")
            (tests / "parent.qtest").write_text("#!/usr/bin/env qore\n# test-exec-mode: parent - own process\n")
            (tests / "plain.qtest").write_text("#!/usr/bin/env qore\n# says test-exec-mode: children in a comment\n")
            binaries = root / "bin"
            binaries.mkdir()
            (binaries / "libqore.so").touch()
            log = root / "ran"
            qore = binaries / "qore"
            qore.write_text('#!/bin/sh\n' + STAND_IN_VERSION + 'for a in "$@"; do case "$a" in *.qtest) basename "$a" '
                            '>> "$RAN_LOG";; esac; done\nexit 0\n')
            qore.chmod(0o755)
            stand_in_build(root, binaries)
            env = {
                "PATH": os.defpath,
                "CI": "1",
                "QORE_BINARY": str(qore),
                "LIBQORE_BINARY": str(binaries / "libqore.so"),
                "CORE_DIR": str(root / "cores"),
                "RAN_LOG": str(log),
            }
            env.update(env_extra)
            result = subprocess.run(["sh", str(ROOT / "run_tests.sh")], cwd=root, env=env, capture_output=True,
                                    text=True, timeout=120)
            ran = sorted(log.read_text().split()) if log.exists() else []
            return result, ran

    def test_default_mode_runs_every_test(self):
        result, ran = self.run_runner({"QORE_TEST_OPTS": "-penable-debug"})
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual(["children.qtest", "parent.qtest", "plain.qtest"], ran)

    def test_explicit_mode_without_exclusion_runs_every_test(self):
        result, ran = self.run_runner({"QORE_TEST_OPTS": "-penable-debug --exec-mode=jit"})
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual(["children.qtest", "parent.qtest", "plain.qtest"], ran)

    def test_exclusion_leaves_out_children_tests(self):
        for opts in ("-penable-debug --exec-mode=ir --ir-fallback-warn", "--exec-mode tiered"):
            with self.subTest(opts=opts):
                result, ran = self.run_runner({"QORE_TEST_OPTS": opts, "QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN": "1"})
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertEqual(["parent.qtest", "plain.qtest"], ran)
                self.assertIn("Left out 1 tests that declare 'test-exec-mode: children'", result.stdout)

    def test_exclusion_needs_an_execution_mode(self):
        for opts in ("-penable-debug", "", "--exec-modes=jit"):
            with self.subTest(opts=opts):
                result, ran = self.run_runner({"QORE_TEST_OPTS": opts, "QORE_TEST_EXCLUDE_EXEC_MODE_CHILDREN": "1"})
                self.assertEqual(1, result.returncode, result.stdout + result.stderr)
                self.assertIn("needs an --exec-mode option in QORE_TEST_OPTS", result.stderr)
                self.assertEqual([], ran)


if __name__ == "__main__":
    unittest.main()
