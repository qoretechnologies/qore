#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
# documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
# persons to whom the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""The LLVM lowering tests a block for a terminator only with qoreGetTerminator()

LLVM 23 changed BasicBlock::getTerminator(): it no longer returns nullptr for a block whose last instruction is not a
terminator (it returns the last instruction in a release build of LLVM).  A lowering that tested a block it was still
building with it took the block for terminated and left it without one: on macOS with LLVM 23.1.6 the DataProvider
qmod failed with "missing terminator in LLVM block reference_slot_done" (AOT2 pipeline 71092), from
QoreIRToLLVM::lowerPairedAutoAddStore().  The other LLVM versions of the CI cannot show it, so this checks the code: a
raw getTerminator() whose result is tested for null is an error; qoreGetTerminator() (QoreIRToLLVM.h) is the nullable
query.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]

# the result of a raw getTerminator() call tested for null in the same expression
NULL_TESTS = (
    re.compile(r"!\s*[\w\->\.\(\)\*&]*\bgetTerminator\(\)"),
    re.compile(r"\bgetTerminator\(\)\s*[!=]=\s*nullptr"),
    re.compile(r"\bdyn_cast_or_null<[^>]+>\([^;]*\bgetTerminator\(\)"),
    re.compile(r"\?[^;:]*\bgetTerminator\(\)[^;]*:"),
    re.compile(r"\bif\s*\([^;]*\bgetTerminator\(\)\s*\)"),
)
# a raw getTerminator() assigned to a variable, which a following line tests for null
ASSIGNED = re.compile(r"\b(\w+)\s*=\s*[^;]*\bgetTerminator\(\);")


def sources():
    for pattern in ("lib/*.cpp", "lib/*.h", "include/qore/intern/*.h"):
        yield from sorted(ROOT.glob(pattern))


def raw_null_tests(text):
    """Returns the lines (1-based) where a raw getTerminator() result is tested for null"""
    lines = text.splitlines()
    found = []
    for n, line in enumerate(lines, 1):
        code = line.split("//", 1)[0]
        if "getTerminator()" not in code:
            continue
        if any(p.search(code) for p in NULL_TESTS):
            found.append(n)
            continue
        m = ASSIGNED.search(code)
        if m:
            var = re.escape(m.group(1))
            following = "\n".join(lines[n:n + 3])
            if re.search(rf"(!\s*{var}\b|\b{var}\s*[!=]=\s*nullptr|\bif\s*\(\s*{var}\s*\))", following):
                found.append(n)
    return found


class LlvmTerminatorQueriesTest(unittest.TestCase):
    def test_no_raw_terminator_null_test(self):
        offenders = []
        for path in sources():
            text = path.read_text(errors="replace")
            if "getTerminator()" not in text:
                continue
            for n in raw_null_tests(text):
                offenders.append(f"{path.relative_to(ROOT)}:{n}")
        self.assertEqual([], offenders, "use qoreGetTerminator() to test a block for a terminator")

    def test_detector(self):
        # the forms that the check finds, and the uses of a block's terminator that it leaves
        for code in ("if (!builder->GetInsertBlock()->getTerminator()) {",
                     "auto* br = llvm::dyn_cast_or_null<llvm::BranchInst>(pred->getTerminator());",
                     "llvm::Instruction* exit = bb ? bb->getTerminator() : nullptr;",
                     "if (bb->getTerminator() == nullptr) {",
                     "llvm::Instruction* t = bb->getTerminator();\nif (!t) {"):
            self.assertTrue(raw_null_tests(code), code)
        for code in ("llvm::IRBuilder<> init(locals_entry->getTerminator());",
                     "bb->getTerminator()->eraseFromParent();",
                     "if (!qoreGetTerminator(builder->GetInsertBlock())) {",
                     "llvm::Instruction* head_br = head->getTerminator();\nllvm::IRBuilder<> b(head_br);"):
            self.assertFalse(raw_null_tests(code), code)

    def test_wrapper_uses_the_nullable_query(self):
        text = (ROOT / "include/qore/intern/QoreIRToLLVM.h").read_text()
        m = re.search(r"inline llvm::Instruction\* qoreGetTerminator\(llvm::BasicBlock\* block\) \{(?P<body>.*?)\n\}",
                      text, re.S)
        self.assertTrue(m, "qoreGetTerminator() is in QoreIRToLLVM.h")
        self.assertIn("#if LLVM_VERSION_MAJOR >= 23\n    return block->getTerminatorOrNull();", m.group("body"))
        self.assertIn("#include <llvm/Config/llvm-config.h>", text)


if __name__ == "__main__":
    unittest.main()
