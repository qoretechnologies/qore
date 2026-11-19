#!/usr/bin/env python3
"""Every invocation path of a call reference or closure applies its creation provenance (issues #6520, sandboxing).

An object or call reference records the sandbox and the functional-domain restrictions of the code that creates it
(QoreSandboxContext; the origin restrictions of library module code that restricted code creates, issue #6520), and
QoreSandboxProvenanceHelper applies them while its code runs.  A dispatch method that does not construct the helper
runs the code without them: for the functional-domain checks, a library registry that a library module's
initialization fires would run what restricted code put there without its restrictions.  This test requires the
helper in every dispatchValue(), dispatchNamed(), dispatchLexicalValue() and execValueTmpArgs() implementation of the
call reference and closure classes, and in the direct closure paths of the IR interpreter and the JIT runtime, except
for the implementations named in EXEMPT, each with its reason.

Copyright (C) 2026 Qore Technologies, s.r.o.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]

#! the sources of the call reference and closure classes
CLASS_SOURCES = (
    "lib/CallReferenceNode.cpp",
    "lib/QoreClosureNode.cpp",
    "include/qore/intern/CallReferenceNode.h",
    "include/qore/intern/QoreClosureNode.h",
)

#! the invocation methods that run the code of a call reference
METHODS = ("dispatchValue", "dispatchNamed", "dispatchLexicalValue", "execValueTmpArgs")

#! implementations that need no helper: "Class::method" -> reason
EXEMPT = {
    "ResolvedCallReferenceNode::dispatchValue": "the base implementation calls execValue(), whose implementations "
        "apply the provenance (or are native callbacks of modules, which run no Qore code of a reference)",
    "ResolvedCallReferenceNode::dispatchLexicalValue": "the base implementation calls execValue(), see above",
    "ResolvedCallReferenceNodeIntern::dispatchNamed": "the base implementation only raises NAMED-CALL-NOT-SUPPORTED",
    "SystemThreadCallReference::dispatchValue": "starts its fixed callback in a new system thread that no sandbox "
        "governs (SandboxManager::createSystemThreadCall()); it runs no code of the reference in the calling thread",
}

#! the direct closure paths of the compiled modes, which run the closure's code without a dispatch method:
#! (source, function name); execClosureDirect() and execClosureDirectConsume() of the JIT runtime forward to
#! execClosureDirect_t().  The IR interpreter's native leaf shortcuts (tryExecuteSimpleClosureIncrement() and the
#! others for closures whose body only changes a captured variable) make no call, so no check can depend on the
#! provenance there
DIRECT_PATHS = (
    ("lib/JITRuntime.cpp", "execClosureDirect_t"),
    ("lib/QoreIRInterpreter.cpp", "tryExecuteInterpreterInlineIRClosure"),
)

HELPER = "QoreSandboxProvenanceHelper"


def body_at(text, open_brace):
    """Returns the text of the block that starts at the '{' at index open_brace"""
    depth = 0
    i = open_brace
    while i < len(text):
        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[open_brace:i + 1]
        i += 1
    return text[open_brace:]


def class_at(text, pos):
    """Returns the name of the innermost class whose definition contains the position, or None"""
    best = None
    for m in re.finditer(r"\bclass\s+(?:DLLLOCAL\s+|DLLEXPORT\s+)?(\w+)[^;{]*\{", text):
        start = m.end() - 1
        if start > pos:
            break
        body = body_at(text, start)
        if start + len(body) > pos:
            best = m.group(1)
    return best


def find_implementations(path):
    """Returns the implementations of METHODS in the source as (name, body)"""
    text = (ROOT / path).read_text()
    found = []
    # out-of-line definitions: QoreValue Class::method(...) ... {
    for m in re.finditer(r"^QoreValue\s+(\w+)::(%s)\([^;{]*\)\s*const\s*\{" % "|".join(METHODS), text, re.M):
        found.append(("%s::%s" % (m.group(1), m.group(2)), body_at(text, m.end() - 1)))
    # definitions inside a class: QoreValue method(...) const [override] {
    pattern = r"^\s+(?:DLLLOCAL\s+)?(?:virtual\s+)?QoreValue\s+(%s)\([^;{]*\)\s*const\s*(?:override\s*)?\{" % \
        "|".join(METHODS)
    for m in re.finditer(pattern, text, re.M):
        cls = class_at(text, m.start())
        found.append(("%s::%s" % (cls, m.group(1)), body_at(text, m.end() - 1)))
    return found


class CallReferenceProvenanceTest(unittest.TestCase):
    def test_dispatch_methods_apply_provenance(self):
        missing = []
        names = []
        for path in CLASS_SOURCES:
            for name, body in find_implementations(path):
                names.append(name)
                if name in EXEMPT:
                    continue
                # a body that only forwards to another invocation method of the same object is covered by that one
                if re.fullmatch(r"\{\s*return\s+(dispatchValue|execValue)\(args,\s*xsink\);\s*\}", body.strip()):
                    continue
                if HELPER not in body:
                    missing.append("%s (%s)" % (name, path))
        self.assertEqual([], missing, "invocation methods that do not apply the creation provenance with "
            "QoreSandboxProvenanceHelper; add the helper, or an entry with its reason to EXEMPT")
        # the scan finds the implementations: a changed declaration style would otherwise pass silently
        self.assertGreaterEqual(len(names), 40, names)
        for name in EXEMPT:
            self.assertIn(name, names, "an exemption for an implementation that no longer exists: " + name)

    def test_direct_closure_paths_apply_provenance(self):
        for path, func in DIRECT_PATHS:
            text = (ROOT / path).read_text()
            m = re.search(r"\b%s\([^;{]*\)\s*\{" % re.escape(func), text)
            self.assertIsNotNone(m, "%s not found in %s" % (func, path))
            self.assertIn(HELPER, body_at(text, m.end() - 1), "%s in %s does not apply the provenance" % (func, path))


if __name__ == "__main__":
    unittest.main()
