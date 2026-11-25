#!/usr/bin/env python3
"""Compare Qore IR/AOT end-to-end task costs with and without a submitting sandbox."""
# Copyright (c) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

import argparse
import json
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import sys
import tempfile

from run import HERE, ROOT, digest, parse_qore, summarize


SCENARIOS = [(f"{context}-{count}-{batch}", int(context == "sandbox"), count, batch)
             for context in ("plain", "sandbox") for count in (0, 1024) for batch in (1, 64)]


def validate_checks(output):
    expected = {name: batch * count * (count + 3) // 2 for name, _, count, batch in SCENARIOS}
    actual = {}
    for line in output.splitlines():
        tag, name, value = line.split("\t")
        if tag != "CHECK" or name in actual:
            raise ValueError("invalid or duplicate task checksum")
        actual[name] = int(value)
    if actual != expected:
        raise ValueError("task checksums do not match independent reference")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qore", type=Path, default=ROOT / "build/qore")
    parser.add_argument("--qcc", type=Path, default=ROOT / "build/qcc")
    parser.add_argument("--baseline-libdir", type=Path)
    parser.add_argument("--output-root", type=Path, default=ROOT / "build/bench-runtime-comparison")
    parser.add_argument("--forks", type=int, default=3)
    parser.add_argument("--warmup", type=int, default=3)
    parser.add_argument("--samples", type=int, default=5)
    parser.add_argument("--time-ms", type=int, default=300)
    parser.add_argument("--order-seed", type=int, default=20261006)
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    for name in ("forks", "samples"):
        if not 1 <= getattr(args, name) <= 1000:
            parser.error(f"{name} must be in 1..1000")
    if not 0 <= args.warmup <= 1000 or not 1 <= args.time_ms <= 60000:
        parser.error("warmup must be in 0..1000; time-ms must be in 1..60000")
    library_name = "libqore.dylib" if sys.platform == "darwin" else "libqore.so"
    if args.baseline_libdir and not (args.baseline_libdir / library_name).is_file():
        parser.error(f"baseline-libdir must contain {library_name}")

    args.output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="pool-", dir=args.output_root)).resolve()
    print(f"Results: {output}", flush=True)
    sources = ("thread_pool.py", "thread_pool.qr", "run.py")
    for name in sources:
        shutil.copyfile(HERE / name, output / name)
    qore, qcc = args.qore.resolve(), args.qcc.resolve()
    loader = "DYLD_LIBRARY_PATH" if sys.platform == "darwin" else "LD_LIBRARY_PATH"
    commands = []

    def execute(command, label, env, timeout=600):
        command = list(map(str, command))
        commands.append({"label": label, "argv": command, "library_path": env[loader]})
        (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        result = subprocess.run(command, text=True, capture_output=True, env=env, timeout=timeout)
        (output / (label + ".stdout")).write_text(result.stdout)
        (output / (label + ".stderr")).write_text(result.stderr)
        if result.returncode:
            raise RuntimeError(f"{label} exited {result.returncode}; see {output}")
        return result.stdout

    variants = [("", qore.parent)]
    if args.baseline_libdir:
        variants.append(("-before", args.baseline_libdir.resolve()))
    bases, environments = {}, {}
    metadata = {"platform": platform.platform(), "machine": platform.machine(),
                "settings": {k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
                "sources": {name: digest(output / name) for name in sources},
                "qore_sha256": digest(qore), "qcc_sha256": digest(qcc), "variants": {}}
    for suffix, libdir in variants:
        env = os.environ.copy()
        env[loader] = str(libdir) + os.pathsep + env.get(loader, "")
        executable = output / ("thread-pool-aot" + suffix)
        execute([qcc, "-O3", "--strip-source", "-o", executable, output / "thread_pool.qr"],
                "compile" + suffix, env)
        metadata["variants"][suffix or "after"] = {
            "library_sha256": digest(libdir / library_name), "aot_sha256": digest(executable),
            "qore_version": execute([qore, "--version"], "version" + suffix, env),
            "environment": {k: v for k, v in env.items() if k.startswith((
                "QORE_DISABLE_", "QORE_IR_", "QORE_JIT_", "QORE_AOT_"))}}
        for mode, base in (("qore-ir", [qore, "--exec-mode=ir", output / "thread_pool.qr"]),
                           ("qore-aot", [executable])):
            bases[mode + suffix] = base
            environments[mode + suffix] = env
    metadata["revision"] = execute(["git", "rev-parse", "HEAD"], "revision", env).strip()
    metadata["worktree"] = execute(["git", "status", "--short"], "worktree", env)
    (output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    for mode, base in bases.items():
        validate_checks(execute(base + ["check"], "check-" + mode, environments[mode]))
        print(f"{mode}: 8 scenario checks passed", flush=True)
    if args.check_only:
        return

    jobs = [(mode, scenario, fork) for mode in bases for scenario in SCENARIOS for fork in range(args.forks)]
    random.Random(args.order_seed).shuffle(jobs)
    results = []
    timeout = max(120, (args.warmup + args.samples) * args.time_ms / 1000 * 3 + 60)
    for index, (mode, (name, sandboxed, count, batch), fork) in enumerate(jobs, 1):
        label = f"{mode}-{name}-{fork}"
        print(f"[{index}/{len(jobs)}] {label}", flush=True)
        stdout = execute(bases[mode] + [name, sandboxed, count, batch, args.warmup, args.samples, args.time_ms],
                         label, environments[mode], timeout)
        samples = parse_qore(stdout, name, 1, args.warmup, args.samples, args.time_ms)
        results.append({"mode": mode, "scenario": name, "fork": fork, "ns_per_task": samples})
        (output / "samples.json").write_text(json.dumps(results, indent=2) + "\n")
    summary = []
    for name, _, _, _ in SCENARIOS:
        for mode in bases:
            means = [statistics.mean(row["ns_per_task"]) for row in results
                     if row["mode"] == mode and row["scenario"] == name]
            row = {"mode": mode, "scenario": name, "fork_means_ns": means, **summarize(means)}
            summary.append(row)
            print(f"{name:20s} {mode:17s} {row['median']:10.2f} ns/task")
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError, subprocess.SubprocessError) as error:
        sys.exit(str(error))
