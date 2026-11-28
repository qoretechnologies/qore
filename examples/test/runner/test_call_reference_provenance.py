#!/usr/bin/env python3
"""Every invocation path of a call reference or closure applies its creation provenance (issues #6520, sandboxing).

An object or call reference records the sandbox and the functional-domain restrictions of the code that creates it
(QoreSandboxContext; the origin restrictions of library module code that restricted code creates, issue #6520), and
QoreSandboxProvenanceHelper applies them while its code runs.  A dispatch method that does not construct the helper
runs the code without them: for the functional-domain checks, a library registry that a library module's
initialization fires would run what restricted code put there without its restrictions.  This test requires the
helper in every dispatchValue(), dispatchNamed(), dispatchLexicalValue() and execValueTmpArgs() implementation of the
call reference and closure classes, and in the direct closure paths of the IR interpreter and the JIT runtime, except
for the implementations named in EXEMPT, each with its reason.  An implementation that delegates to other methods of
its class (a wrapper that sends some references to a path of their own, as the references that a Program object
lends, qore#7678) is covered when each of those methods is: every return of its code that runs code returns a call of
a method of its class whose name starts with the invocation method's name (dispatchValueImpl(), dispatchValueLent()),
and every such method constructs the helper or is covered in turn.

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


#! an inlining attribute before the return type of a definition
ATTRS = r"(?:QORE_INTERN_\w+\s+)?"


def find_implementations(path, suffixed=False):
    """Returns the implementations of METHODS in the source as (name, body); with suffixed, also the methods whose
    names start with one of them (the paths that an implementation delegates to)"""
    text = (ROOT / path).read_text()
    found = []
    names = "(?:%s)%s" % ("|".join(METHODS), r"\w*" if suffixed else "")
    # out-of-line definitions: QoreValue Class::method(...) ... {
    for m in re.finditer(r"^%sQoreValue\s+(\w+)::(%s)\([^;{]*\)\s*const\s*\{" % (ATTRS, names), text, re.M):
        found.append(("%s::%s" % (m.group(1), m.group(2)), body_at(text, m.end() - 1)))
    # definitions inside a class: QoreValue method(...) const [override] {
    pattern = r"^\s+(?:DLLLOCAL\s+)?(?:virtual\s+)?%sQoreValue\s+(%s)\([^;{]*\)\s*const\s*(?:override\s*)?\{" % \
        (ATTRS, names)
    for m in re.finditer(pattern, text, re.M):
        cls = class_at(text, m.start())
        found.append(("%s::%s" % (cls, m.group(1)), body_at(text, m.end() - 1)))
    return found


def delegates(cls, body):
    """Returns the methods of the class that the body delegates to, if every return of the body that runs code returns
    a call of a method whose name starts with an invocation method's name; otherwise None"""
    returns = re.findall(r"\breturn\b([^;]*);", body)
    targets = []
    for r in returns:
        r = r.strip()
        # an error return: the code of the reference does not run
        if r in ("QoreValue()", ""):
            continue
        m = re.fullmatch(r"((?:%s)\w*)\([^;]*\)" % "|".join(METHODS), r, re.S)
        if not m:
            return None
        targets.append("%s::%s" % (cls, m.group(1)))
    return targets or None


def covered(name, impls, path=()):
    """True if the implementation constructs the helper, or delegates only to implementations that are covered; a
    delegation that comes back to a method on its own path is not"""
    if name in path:
        return False
    body = impls.get(name)
    if body is None:
        return False
    if HELPER in body:
        return True
    targets = delegates(name.split("::")[0], body)
    return targets is not None and all(covered(t, impls, path + (name,)) for t in targets)


class CallReferenceProvenanceTest(unittest.TestCase):
    def test_dispatch_methods_apply_provenance(self):
        missing = []
        names = []
        # every implementation, also the paths that the invocation methods delegate to
        impls = {}
        for path in CLASS_SOURCES:
            impls.update(dict(find_implementations(path, True)))
        for path in CLASS_SOURCES:
            for name, body in find_implementations(path):
                names.append(name)
                if name in EXEMPT:
                    continue
                # a body that only forwards to another invocation method of the same object is covered by that one
                if re.fullmatch(r"\{\s*return\s+(dispatchValue|execValue)\(args,\s*xsink\);\s*\}", body.strip()):
                    continue
                if not covered(name, impls):
                    missing.append("%s (%s)" % (name, path))
        self.assertEqual([], missing, "invocation methods that do not apply the creation provenance with "
            "QoreSandboxProvenanceHelper; add the helper, or an entry with its reason to EXEMPT")
        # the scan finds the implementations: a changed declaration style would otherwise pass silently
        self.assertGreaterEqual(len(names), 40, names)
        for name in EXEMPT:
            self.assertIn(name, names, "an exemption for an implementation that no longer exists: " + name)

    def test_delegation(self):
        """A wrapper is covered only through paths that are covered; a path that runs code without the helper is not"""
        helper = "{ QoreSandboxProvenanceHelper provenance(this); return run(); }"
        impls = {
            "C::dispatchValue": "{ if (lent) { return dispatchValueLent(args, xsink); } return dispatchValueImpl(args, xsink); }",
            "C::dispatchValueLent": "{ Lent l(pgm, xsink); if (*xsink) { return QoreValue(); } return dispatchValueImpl(args, xsink); }",
            "C::dispatchValueImpl": helper,
            "D::dispatchValue": "{ if (lent) { return dispatchValueLent(args, xsink); } return dispatchValueImpl(args, xsink); }",
            "D::dispatchValueLent": "{ return pgm->run(args); }",
            "D::dispatchValueImpl": helper,
            "E::dispatchValue": "{ if (lent) { return pgm->run(args); } return dispatchValueImpl(args, xsink); }",
            "E::dispatchValueImpl": helper,
            "F::dispatchValue": "{ return dispatchValueMissing(args, xsink); }",
            "G::dispatchValue": "{ return dispatchValueLoop(args, xsink); }",
            "G::dispatchValueLoop": "{ return dispatchValue(args, xsink); }",
        }
        self.assertTrue(covered("C::dispatchValue", impls))
        # a delegated path that runs code without the helper
        self.assertFalse(covered("D::dispatchValue", impls))
        # a return that runs code itself
        self.assertFalse(covered("E::dispatchValue", impls))
        # a path that is not found
        self.assertFalse(covered("F::dispatchValue", impls))
        # a delegation that comes back to itself
        self.assertFalse(covered("G::dispatchValue", impls))

    def test_direct_closure_paths_apply_provenance(self):
        for path, func in DIRECT_PATHS:
            text = (ROOT / path).read_text()
            m = re.search(r"\b%s\([^;{]*\)\s*\{" % re.escape(func), text)
            self.assertIsNotNone(m, "%s not found in %s" % (func, path))
            self.assertIn(HELPER, body_at(text, m.end() - 1), "%s in %s does not apply the provenance" % (func, path))


if __name__ == "__main__":
    unittest.main()
