#!/usr/bin/env python3
"""Measure same-program and cross-program typed calls in Qore IR (ns/call)."""
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
import tempfile

from run import HERE, ROOT, digest, parse_qore, reference, summarize

NAMES = ("same_program", "cross_program")


def validate(output):
    expected = {(name, count, seed): reference("direct_call", count, seed)
                for name in NAMES for count in (0, 1, 17, 1024, 4096) for seed in (0, 7, 65535)}
    actual = {}
    for line in output.splitlines():
        tag, name, count, seed, value = line.split("\t")
        key = (name, int(count), int(seed))
        if tag != "CHECK" or key in actual:
            raise ValueError(f"invalid/duplicate check: {line}")
        actual[key] = int(value)
    if actual != expected:
        raise ValueError("program-call checks do not match the independent reference")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qore", type=Path, default=ROOT / "build/qore")
    parser.add_argument("--baseline-libdir", type=Path)
    parser.add_argument("--output-root", type=Path, default=ROOT / "build/bench-runtime-comparison")
    parser.add_argument("--forks", type=int, default=3)
    parser.add_argument("--warmup", type=int, default=3)
    parser.add_argument("--samples", type=int, default=5)
    parser.add_argument("--time-ms", type=int, default=300)
    parser.add_argument("--count", type=int, default=1024)
    parser.add_argument("--order-seed", type=int, default=20261006)
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    for name, low, high in (("forks", 1, 1000), ("warmup", 0, 1000), ("samples", 1, 1000),
                           ("time_ms", 1, 60000), ("count", 1, 1000000)):
        if not low <= getattr(args, name) <= high:
            parser.error(f"{name} must be in {low}..{high}")
    qore = args.qore.resolve()
    loader = "DYLD_LIBRARY_PATH" if platform.system() == "Darwin" else "LD_LIBRARY_PATH"
    libname = "libqore.dylib" if platform.system() == "Darwin" else "libqore.so"
    libdirs = {"after": qore.parent}
    if args.baseline_libdir:
        libdirs["before"] = args.baseline_libdir.resolve()
    if not qore.is_file() or not os.access(qore, os.X_OK):
        parser.error(f"qore is not executable: {qore}")
    for directory in libdirs.values():
        if not (directory / libname).is_file():
            parser.error(f"missing {libname} in {directory}")
    args.output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="calls-", dir=args.output_root.resolve()))
    print(f"Results: {output}", flush=True)
    for name in ("program_calls.py", "program_calls.qr", "run.py"):
        shutil.copy2(HERE / name, output / name)
    metadata = {"settings": {key: str(value) if isinstance(value, Path) else value
                             for key, value in vars(args).items()},
                "platform": platform.platform(), "qore": str(qore), "qore_sha256": digest(qore),
                "sources": {name: digest(output / name)
                            for name in ("program_calls.py", "program_calls.qr", "run.py")},
                "libraries": {}, "commands": []}
    environments = {}
    for mode, directory in libdirs.items():
        library = (directory / libname).resolve(strict=True)
        env = os.environ.copy()
        env[loader] = str(directory) + (os.pathsep + env[loader] if env.get(loader) else "")
        environments[mode] = env
        metadata["libraries"][mode] = {"path": str(library), "sha256": digest(library),
            "environment": {k: v for k, v in env.items()
                            if k.startswith("QORE_") or k in ("LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH")}}
    timeout = max(120, (args.warmup + args.samples) * args.time_ms / 1000 * 3 + 60)

    def run(mode, label, arguments):
        command = [str(qore), "--exec-mode=ir", str(output / "program_calls.qr"), *map(str, arguments)]
        metadata["commands"].append({"mode": mode, "label": label, "command": command})
        (output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
        result = subprocess.run(command, env=environments[mode], capture_output=True, text=True, timeout=timeout)
        (output / (label + ".stdout")).write_text(result.stdout)
        (output / (label + ".stderr")).write_text(result.stderr)
        result.check_returncode()
        return result.stdout

    for mode in libdirs:
        validate(run(mode, "check-" + mode, ["check"]))
        print(f"{mode}: 30 reference checks passed", flush=True)
    if args.check_only:
        return
    jobs = [(mode, name, fork) for mode in libdirs for name in NAMES for fork in range(args.forks)]
    random.Random(args.order_seed).shuffle(jobs)
    samples = []
    for mode, name, fork in jobs:
        label = f"{mode}-{name}-{fork}"
        print(label, flush=True)
        raw = run(mode, label, [name, args.count, args.warmup, args.samples, args.time_ms])
        values = parse_qore(raw, name, args.count, args.warmup, args.samples, args.time_ms)
        samples.append({"mode": mode, "kernel": name, "fork": fork, "ns_per_element": values})
        (output / "samples.json").write_text(json.dumps(samples, indent=2) + "\n")
    summary = []
    for mode in libdirs:
        for name in NAMES:
            means = [statistics.mean(r["ns_per_element"]) for r in samples
                     if r["mode"] == mode and r["kernel"] == name]
            row = {"mode": mode, "kernel": name, "fork_means_ns": means, **summarize(means)}
            summary.append(row)
            print(f"{mode} {name}: {row['median']:.2f} ns/call")
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")


if __name__ == "__main__":
    main()
