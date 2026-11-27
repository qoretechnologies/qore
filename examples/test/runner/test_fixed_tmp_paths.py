#!/usr/bin/env python3
"""Tests write their files under a temporary directory of their own, not at fixed paths in /tmp (qore#7817)

With ``QORE_TEST_TMP_LEAKS``, run_tests.sh gives each test a temporary directory of its own as ``TMPDIR`` and
reports what the test leaves there.  A test that writes to a fixed path in ``/tmp`` bypasses ``TMPDIR``: it leaves
its files in the shared ``/tmp`` when it fails (or always), where nothing reports or removes them, and its names can
collide with a test running beside it.  A test makes its files under ``tmp_location()``, in a ``TmpDir`` or with
``make_tmp_dir()``, which are removed also when it fails.

A test fails this check when its source has a string literal that starts with ``/tmp/``.  The tests that have one
today are listed in ``KNOWN``, which only shrinks: a test in it that no longer has one must be removed from it.  Some
of them use such a path only as data (URL and path parsing tests) and stay listed.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]

#: a string literal that starts with /tmp/
FIXED_TMP = re.compile(r"""["']/tmp/""")

#: the sources of tests that have a fixed path in /tmp (qore#7817); remove an entry when its test is converted
KNOWN = frozenset((
    "examples/test/ir/AOTAggregateMethodProjection.qtest",
    "examples/test/ir/AOTAggregateNoEscape.qtest",
    "examples/test/ir/AOTAggregateReturnProjection.qtest",
    "examples/test/ir/AOTAssignedHashGuard.qtest",
    "examples/test/ir/AOTAssignedStringConversionFacts.qtest",
    "examples/test/ir/AOTBatchCalleeFastPath.qtest",
    "examples/test/ir/AOTBorrowedAggregateProjection.qtest",
    "examples/test/ir/AOTBoundedIntExpressionSummary.qtest",
    "examples/test/ir/AOTBoxedReturnParam.qtest",
    "examples/test/ir/AOTCallKindExpressionSummary.qtest",
    "examples/test/ir/AOTClosureAggregateProjection.qtest",
    "examples/test/ir/AOTCollectionPredicateFusion.qtest",
    "examples/test/ir/AOTComplexHashTypePathCache.qtest",
    "examples/test/ir/AOTConstHashLiteralInsert.qtest",
    "examples/test/ir/AOTContextIndependentBodyLocals.qtest",
    "examples/test/ir/AOTCrossObjectAggregateProjection.qtest",
    "examples/test/ir/AOTDataProviderExecutable.qtest",
    "examples/test/ir/AOTDebuggableClosureSerialization.qtest",
    "examples/test/ir/AOTDirectIntToString.qtest",
    "examples/test/ir/AOTDirectSelfMemberMutation.qtest",
    "examples/test/ir/AOTDynamicStringConsumer.qtest",
    "examples/test/ir/AOTExactBoxedCollectionFacts.qtest",
    "examples/test/ir/AOTExactBoxedCrossBlockFacts.qtest",
    "examples/test/ir/AOTExactBoxedReturnFacts.qtest",
    "examples/test/ir/AOTExplicitObjectStringMeasure.qtest",
    "examples/test/ir/AOTFixedSprintf.qtest",
    "examples/test/ir/AOTFreshContainerInit.qtest",
    "examples/test/ir/AOTFreshHashQueryFold.qtest",
    "examples/test/ir/AOTFunctionEffectSummary.qtest",
    "examples/test/ir/AOTFusedMapFold.qtest",
    "examples/test/ir/AOTGenericCrossObjectFastEntry.qtest",
    "examples/test/ir/AOTGenericFunctionFastEntry.qtest",
    "examples/test/ir/AOTGenericInstanceMethodFastEntry.qtest",
    "examples/test/ir/AOTGenericReturnParam.qtest",
    "examples/test/ir/AOTGenericStaticMethodFastEntry.qtest",
    "examples/test/ir/AOTGlobalIntSummary.qtest",
    "examples/test/ir/AOTHashDeclKeyProof.qtest",
    "examples/test/ir/AOTImmediateCapturedClosure.qtest",
    "examples/test/ir/AOTImmediateNumericFacts.qtest",
    "examples/test/ir/AOTImplicitSelfGetterInlining.qtest",
    "examples/test/ir/AOTImplicitSelfHashStringConsumer.qtest",
    "examples/test/ir/AOTIntExpressionSummary.qtest",
    "examples/test/ir/AOTKnownCallRefFastEntry.qtest",
    "examples/test/ir/AOTLLVM23Terminator.qtest",
    "examples/test/ir/AOTLateExceptionEdgeElision.qtest",
    "examples/test/ir/AOTLateNativeOperations.qtest",
    "examples/test/ir/AOTLateNativeRemainingOperations.qtest",
    "examples/test/ir/AOTLazyMethodFastArgs.qtest",
    "examples/test/ir/AOTListStringJoin.qtest",
    "examples/test/ir/AOTListStringJoinMethod.qtest",
    "examples/test/ir/AOTMetadataCompression.qtest",
    "examples/test/ir/AOTMethodMultiStringConsumer.qtest",
    "examples/test/ir/AOTNativeClosure.qtest",
    "examples/test/ir/AOTNestedCfgSelectSummary.qtest",
    "examples/test/ir/AOTNonescapingObjectScalarProjection.qtest",
    "examples/test/ir/AOTNoopDecrefPrune.qtest",
    "examples/test/ir/AOTObjectMemberCompoundGet.qtest",
    "examples/test/ir/AOTObjectMemberCompoundGetCrossObject.qtest",
    "examples/test/ir/AOTObjectMemberSetGet.qtest",
    "examples/test/ir/AOTObjectMemberSetGetCrossObject.qtest",
    "examples/test/ir/AOTOptionalScalarFastEntry.qtest",
    "examples/test/ir/AOTOptionalScalarSpecialization.qtest",
    "examples/test/ir/AOTOverloadSelfRecursion.qtest",
    "examples/test/ir/AOTPostRewriteFixedPoint.qtest",
    "examples/test/ir/AOTPrivateStaticClosureContext.qtest",
    "examples/test/ir/AOTReferenceFreeResult.qtest",
    "examples/test/ir/AOTReturnTypeCoercion.qtest",
    "examples/test/ir/AOTRuntimeLocationCache.qtest",
    "examples/test/ir/AOTScalarObjectGetter.qtest",
    "examples/test/ir/AOTScalarizedAggregateReturnProjection.qtest",
    "examples/test/ir/AOTSharedMapArgs.qtest",
    "examples/test/ir/AOTSmoke.qc",
    "examples/test/ir/AOTStoredCaptureCache.qtest",
    "examples/test/ir/AOTStringAppendCow.qtest",
    "examples/test/ir/AOTStringCasePipeline.qtest",
    "examples/test/ir/AOTStringFoldJoin.qtest",
    "examples/test/ir/AOTStringFoldrJoin.qtest",
    "examples/test/ir/AOTStringOrdering.qtest",
    "examples/test/ir/AOTStringProducingSummary.qtest",
    "examples/test/ir/AOTStringSearchFusion.qtest",
    "examples/test/ir/AOTStringSummaryCfg.qtest",
    "examples/test/ir/AOTStringTransformCfg.qtest",
    "examples/test/ir/AOTStringTransformConsumer.qtest",
    "examples/test/ir/AOTStringTransformSwitch.qtest",
    "examples/test/ir/AOTSummaryExceptionElision.qtest",
    "examples/test/ir/AOTWarmedClosureSlotLayout.qtest",
    "examples/test/ir/AOTWideIntExpressionSummary.qtest",
    "examples/test/ir/BgCompileShutdown.qtest",
    "examples/test/ir/CommonPseudoFastPath.qtest",
    "examples/test/ir/CompoundAssignmentAssignedState.qtest",
    "examples/test/ir/ConversionPseudoFastPath.qtest",
    "examples/test/ir/IRAutoLocalReference.qtest",
    "examples/test/ir/IRBorrowedListRead.qtest",
    "examples/test/ir/IRBoundedTypedListRead.qtest",
    "examples/test/ir/IRCallEffectSummary.qtest",
    "examples/test/ir/IRConstantBranchFold.qtest",
    "examples/test/ir/IRContextElision.qtest",
    "examples/test/ir/IRExecModeSmoke.qtest",
    "examples/test/ir/IRFixedBoxedAggregateScalarReplacement.qtest",
    "examples/test/ir/IRFixedHashDeclScalarReplacement.qtest",
    "examples/test/ir/IRFixedHashScalarReplacement.qtest",
    "examples/test/ir/IRFixedListScalarReplacement.qtest",
    "examples/test/ir/IRFoldContextElision.qtest",
    "examples/test/ir/IRInPlaceListPush.qtest",
    "examples/test/ir/IRListPushCow.qtest",
    "examples/test/ir/IRListPushLocalCow.qtest",
    "examples/test/ir/IRLocalListPushLinear.qtest",
    "examples/test/ir/IRLoopCarriedAggregateScalarReplacement.qtest",
    "examples/test/ir/IRNativeIntLeafInline.qtest",
    "examples/test/ir/IRNativeLocalSSA.qtest",
    "examples/test/ir/IRReferencedContainerCow.qtest",
    "examples/test/ir/IRScalarCSE.qtest",
    "examples/test/ir/IRScalarCSETemporaryLifetime.qtest",
    "examples/test/ir/IRScalarListQueryFolding.qtest",
    "examples/test/ir/IRTypedForeach.qtest",
    "examples/test/ir/JITAggregateReturnProjection.qtest",
    "examples/test/ir/JITTransitiveBatch.qtest",
    "examples/test/ir/JitCompileThreadStack.qtest",
    "examples/test/ir/LoopInvariantHashProjection.qtest",
    "examples/test/ir/LoopInvariantListSize.qtest",
    "examples/test/ir/LoopInvariantReadOnlyQuery.qtest",
    "examples/test/ir/LoopInvariantScalar.qtest",
    "examples/test/ir/LoopInvariantStringLength.qtest",
    "examples/test/ir/StringPseudoFastPath.qtest",
    "examples/test/ir/benchmark_exec_modes.qr",
    "examples/test/ir/benchmark_execution_speed.qr",
    "examples/test/ir/exec-mode-harness/exec-mode-harness.qtest",
    "examples/test/modules/sshutil/SshUtilRegistrySandbox.qtest",
    "examples/test/modules/sshutil/sshutil.qtest",
    "examples/test/qlib/ConnectionProvider/ConnectionUpStatus.qtest",
    "examples/test/qlib/DataProvider/DebugCollector.qtest",
    "examples/test/qlib/DbDataProvider/DatasourceConnection.qtest",
    "examples/test/qlib/HttpUtil/HttpRedirectFollower.qtest",
    "examples/test/qlib/ImapClient/ImapClient.qtest",
    "examples/test/qlib/JsonFileDataProvider/JsonFileDataProvider.qtest",
    "examples/test/qlib/LlamaParseDataProvider/LlamaParseDataProvider.qtest",
    "examples/test/qlib/Pop3Client/Pop3Client.qtest",
    "examples/test/qlib/QLS/Diagnostics.qtest",
    "examples/test/qlib/QLS/Document.qtest",
    "examples/test/qlib/QLS/WorkspaceIndex.qtest",
    "examples/test/qlib/QoreApiMetadata/QoreApiMetadata.qtest",
    "examples/test/qlib/QoreCodeFormat/qfmt.qtest",
    "examples/test/qlib/QoreHistory/QoreHistory.qtest",
    "examples/test/qlib/RestClientIo/RestClientIo.qtest",
    "examples/test/qlib/SmtpClient/SmtpClient.qtest",
    "examples/test/qlib/SqlUtil/FirebirdTestIsolation.qtest",
    "examples/test/qlib/SqlUtil/TestRunPrecondition.qtest",
    "examples/test/qlib/TelnetClient/TelnetClient.qtest",
    "examples/test/qlib/UnstructuredDataProvider/UnstructuredDataProvider.qtest",
    "examples/test/qlib/UrlDownloader/UrlDownloader.qtest",
    "examples/test/qore/classes/HTTPClient/HttpClientIpv6Url.qtest",
    "examples/test/qore/classes/Program/parse-options.qtest",
    "examples/test/qore/closures/const-deferred-closure-init.qtest",
    "examples/test/qore/functions/parse_datasource.qtest",
    "examples/test/qore/functions/parse_url.qtest",
    "examples/test/qore/parser/module-path-directive.qtest",
    "examples/test/qore/security/path-nul.qtest",
    "examples/test/qore/security/sandbox-entry-points.qtest",
    "examples/test/qore/security/sandbox-manager.qtest",
    "examples/test/qore/vars/named-arguments.qtest",
    "examples/test/qore/vars/timezone-spelling-cache.qtest",
    "modules/krb5/test/krb5.qtest",
    "modules/linenoise/test/linenoise-sandbox.qtest",
))


def sources():
    roots = [ROOT / "examples/test"] + sorted((ROOT / "modules").glob("*/test"))
    for root in roots:
        for path in sorted(root.rglob("*")):
            if path.is_file() and path.suffix in (".qtest", ".q", ".qm", ".qc", ".qr", ".ql"):
                yield path


def offenders():
    found = set()
    for path in sources():
        if FIXED_TMP.search(path.read_text(errors="replace")):
            found.add(path.relative_to(ROOT).as_posix())
    return found


class FixedTmpPathTest(unittest.TestCase):
    def test_no_new_fixed_tmp_paths(self):
        new = sorted(offenders() - KNOWN)
        self.assertEqual([], new, "a test writes to a fixed path in /tmp: use tmp_location(), TmpDir or "
                         "make_tmp_dir() (qore#7817)")

    def test_known_list_shrinks(self):
        fixed = sorted(KNOWN - offenders())
        self.assertEqual([], fixed, "remove these tests from KNOWN: they no longer have a fixed path in /tmp")

    def test_runner_gives_tests_their_own_tmpdir(self):
        runner = (ROOT / "run_tests.sh").read_text()
        self.assertIn('TEST_ENV="$TEST_ENV TMPDIR=$TEST_TMPDIR"', runner)
        self.assertIn("check_test_tmpdir", runner)


if __name__ == "__main__":
    unittest.main()
