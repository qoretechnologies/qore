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
"""The user modules of the build are compiled with qcc -g only when QORE_QMOD_DEBUGGING is set (qore#7676)

Debug support that costs performance is never on by default, in any CMake build type: the option is OFF, the qcc
flags of the qmod rules get -g only under it, and a change of the option compiles the qmods again also in a Makefile
build, whose rules do not depend on their command lines (the stamp is a dependency of every qmod rule).
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
CMAKE_LISTS = ROOT / "CMakeLists.txt"
MACROS = ROOT / "cmake" / "QoreMacros.cmake"


class QmodDebuggingOptionTest(unittest.TestCase):
    def test_option_is_off_by_default(self):
        text = CMAKE_LISTS.read_text()
        m = re.search(r"option\(QORE_QMOD_DEBUGGING\s+\"[^\"]*\"\s+(\w+)\)", text)
        self.assertTrue(m, "QORE_QMOD_DEBUGGING is a CMake option")
        self.assertEqual("OFF", m.group(1))
        # no build type turns it on
        self.assertIsNone(re.search(r"set\(QORE_QMOD_DEBUGGING\b", text))

    def test_g_only_under_the_option(self):
        text = MACROS.read_text()
        flags = [m.start() for m in re.finditer(r'list\(APPEND _qmod_opt_flags "-g"\)', text)]
        self.assertEqual(1, len(flags), "the qmod rules add -g in one place")
        guard = text.rfind("if (QORE_QMOD_DEBUGGING)", 0, flags[0])
        self.assertGreaterEqual(guard, 0, "-g is added under QORE_QMOD_DEBUGGING")
        self.assertEqual(-1, text.find("endif", guard, flags[0]), "-g is inside the QORE_QMOD_DEBUGGING block")
        # -g is given nowhere else to qcc for the qmods, nor by a build type generator expression
        self.assertIsNone(re.search(r"CONFIG:[A-Za-z]+>:-g\b", text))

    def test_a_change_rebuilds_the_qmods(self):
        text = MACROS.read_text()
        m = re.search(r'set\(_qmod_debugging_stamp "([^"]+)"\)', text)
        self.assertTrue(m, "the debugging choice has a stamp")
        self.assertIn('QORE_WRITE_IF_CHANGED("${_qmod_debugging_stamp}" "debugging=ON\\n")', text)
        self.assertIn('QORE_WRITE_IF_CHANGED("${_qmod_debugging_stamp}" "debugging=OFF\\n")', text)
        self.assertIn('list(APPEND _qmod_probe_dep "${_qmod_debugging_stamp}")', text)


if __name__ == "__main__":
    unittest.main()
