#!/usr/bin/env python3
"""Fast harness checks; no Qore, Java, or network access required."""
# Copyright (c) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import run
import thread_pool
import program_calls


class RunnerTest(unittest.TestCase):
    def checks(self):
        return "".join(f"CHECK\t{name}\t{count}\t{seed}\t{run.reference(name, count, seed)}\n"
                       for name in run.KERNELS for count in (0, 1, 17, 1024, 4096)
                       for seed in (0, 7, 65535))

    def test_reference(self):
        for name in run.KERNELS:
            self.assertEqual(0, run.reference(name, 0, 7))
        self.assertEqual(8449, run.reference("arithmetic", 2, 7))
        self.assertEqual(8461, run.reference("list_read", 2, 7))
        self.assertEqual(8449, run.reference("list_build", 2, 7))
        self.assertEqual(8451, run.reference("list_mutate", 2, 7))
        self.assertEqual(8450, run.reference("temporary_list", 2, 7))
        self.assertEqual(8450, run.reference("temporary_hash", 2, 7))
        self.assertEqual(8449, run.reference("indirect_call", 2, 7))
        self.assertEqual(8449, run.reference("closure_call", 2, 7))
        self.assertEqual(8449, run.reference("closure_capture", 2, 7))
        self.assertEqual(8449, run.reference("indirect_wide", 2, 7))
        self.assertEqual(65535, run.reference("indirect_float", 2, 7))
        self.assertEqual(0, run.reference("indirect_float", 1, 65535))
        self.assertEqual(65, run.reference("indirect_branch_float", 2, 7))
        self.assertEqual(32769, run.reference("indirect_branch_float", 2, 65535))
        self.assertEqual(65, run.reference("indirect_affine", 2, 7))
        self.assertEqual(196707, run.reference("indirect_affine", 3, 65535))
        self.assertEqual(99, run.reference("indirect_affine_wide", 3, 65535))
        self.assertEqual(65, run.reference("indirect_branch_int", 2, 7))
        self.assertEqual(32769, run.reference("indirect_branch_int", 2, 65535))
        self.assertEqual(6, run.reference("indirect_tagged_float", 1, 7))
        self.assertEqual(12, run.reference("indirect_tagged_float", 3, 0))
        self.assertEqual(8451, run.reference("polymorphic_call", 2, 7))
        self.assertEqual(8451, run.reference("dispatch_control", 2, 7))

    def test_checks(self):
        run.validate_checks(self.checks())

    def test_invalid_checks(self):
        valid = self.checks()
        for invalid in ("", valid + valid.splitlines()[0],
                        "\n".join(valid.splitlines()[1:]),
                        valid.replace("CHECK", "WRONG", 1),
                        valid.replace("\t0\t0\t0\n", "\t0\t0\t1\n", 1)):
            with self.subTest(invalid=invalid[:60]), self.assertRaises(ValueError):
                run.validate_checks(invalid)

    def test_timings(self):
        output = "WARMUP\tarithmetic\t1000\t10\t100\t42\nSAMPLE\tarithmetic\t2000\t20\t100\t43\n"
        self.assertEqual([1000.0], run.parse_qore(output, "arithmetic", 100, 1, 1, 1))
        for invalid in ("", output + output, output.replace("SAMPLE", "WARMUP"),
                        output.replace("\t20\t", "\t0\t"),
                        output.replace("\t2000\t", "\t999\t"),
                        output.replace("\t100\t", "\t101\t"),
                        output.replace("\t43\n", "\tnan\n")):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                run.parse_qore(invalid, "arithmetic", 100, 1, 1, 1)

    def test_summary(self):
        self.assertEqual(2, run.summarize([3, 1, 2])["median"])
        self.assertEqual(0, run.summarize([3])["stdev"])

    def test_aot_build_modes(self):
        for have_baseline, reuse in ((False, False), (True, False), (True, True)):
            with self.subTest(baseline=have_baseline, reuse=reuse), tempfile.TemporaryDirectory() as directory:
                output = Path(directory)
                qcc = output / "qcc"
                qcc.write_bytes(b"mock compiler")
                calls = []
                baseline_env = {"library-path": "baseline"} if have_baseline else None

                def compile_mock(command, label, timeout, environment=None):
                    calls.append((label, environment, timeout))
                    Path(command[3]).write_bytes(label.encode("ascii"))
                    return "", 12.5

                commands, metadata = run.build_aot_pair(qcc, output / "kernels.qr", output,
                                                        compile_mock, baseline_env, reuse)
                self.assertEqual(("qcc", None, 600), calls[0])
                self.assertEqual(run.digest(qcc), metadata["qcc_sha256"])
                self.assertEqual(12.5, metadata["aot_compile_ms"])
                self.assertEqual([str(output / "kernels-aot")], commands["qore-aot"])
                self.assertEqual(2 if have_baseline and not reuse else 1, len(calls))
                if have_baseline:
                    self.assertEqual(reuse, metadata["baseline_aot_reused"])
                    self.assertEqual(reuse, commands["qore-aot"] == commands["qore-aot-before"])
                    self.assertEqual(reuse, metadata["aot_sha256"] == metadata["baseline_aot_sha256"])
                    if reuse:
                        self.assertNotIn("baseline_aot_compile_ms", metadata)
                        self.assertFalse((output / "kernels-aot-before").exists())
                    else:
                        self.assertEqual(("qcc-before", baseline_env, 600), calls[1])
                        self.assertEqual(12.5, metadata["baseline_aot_compile_ms"])
                else:
                    self.assertNotIn("qore-aot-before", commands)
                    self.assertNotIn("baseline_aot_sha256", metadata)
        with self.assertRaises(ValueError):
            run.build_aot_pair(None, None, None, None, reuse_aot=True)

    def test_pool_checks(self):
        checks = "".join(f"CHECK\t{name}\t{batch * sum(range(2, count + 2))}\n"
                         for name, _, count, batch in thread_pool.SCENARIOS)
        thread_pool.validate_checks(checks)
        for invalid in ("", checks + checks.splitlines()[0],
                        "\n".join(checks.splitlines()[1:]),
                        checks.replace("CHECK", "WRONG", 1),
                        checks.replace("\t0\n", "\t1\n", 1)):
            with self.subTest(invalid=invalid[:60]), self.assertRaises(ValueError):
                thread_pool.validate_checks(invalid)

    def test_pool_invalid_arguments(self):
        for arguments in (("--samples", "0"), ("--forks", "0"), ("--time-ms", "0"),
                          ("--warmup", "-1"), ("--baseline-libdir", str(run.HERE))):
            with self.subTest(arguments=arguments):
                result = subprocess.run([sys.executable, str(run.HERE / "thread_pool.py"), *arguments],
                                        text=True, capture_output=True, timeout=10)
                self.assertEqual(2, result.returncode, result.stdout + result.stderr)
                self.assertIn("error:", result.stderr)

    def test_program_checks(self):
        checks = "".join(f"CHECK\t{name}\t{count}\t{seed}\t{run.reference('direct_call', count, seed)}\n"
                         for name in program_calls.NAMES for count in (0, 1, 17, 1024, 4096)
                         for seed in (0, 7, 65535))
        program_calls.validate(checks)
        for invalid in ("", checks + checks.splitlines()[0],
                        "\n".join(checks.splitlines()[1:]),
                        checks.replace("CHECK", "WRONG", 1),
                        checks.replace("\t0\t0\t0\n", "\t0\t0\t1\n", 1)):
            with self.subTest(invalid=invalid[:60]), self.assertRaises(ValueError):
                program_calls.validate(invalid)

    def test_program_invalid_arguments(self):
        for arguments in (("--samples", "0"), ("--forks", "0"), ("--time-ms", "0"),
                          ("--warmup", "-1"), ("--count", "1000001"),
                          ("--baseline-libdir", str(run.HERE)), ("--qore", str(run.HERE))):
            with self.subTest(arguments=arguments):
                result = subprocess.run([sys.executable, str(run.HERE / "program_calls.py"), *arguments],
                                        text=True, capture_output=True, timeout=10)
                self.assertEqual(2, result.returncode, result.stdout + result.stderr)
                self.assertIn("error:", result.stderr)

    def test_invalid_arguments(self):
        for arguments in (("--samples", "0"), ("--count", "0"), ("--count", "1000001"),
                          ("--warmup", "-1"), ("--time-ms", "0"),
                          ("--modes", "qore-ir", "qore-ir"),
                          ("--reuse-aot-for-baseline",),
                          ("--reuse-aot-for-baseline", "--modes", "qore-ir",
                           "--baseline-libdir", str(run.HERE)),
                          ("--baseline-libdir", str(run.HERE))):
            with self.subTest(arguments=arguments):
                result = subprocess.run([sys.executable, str(run.HERE / "run.py"), *arguments],
                                        text=True, capture_output=True, timeout=10)
                self.assertEqual(2, result.returncode, result.stdout + result.stderr)
                self.assertIn("error:", result.stderr)


if __name__ == "__main__":
    unittest.main()
