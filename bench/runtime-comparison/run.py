#!/usr/bin/env python3
"""Build, validate, and measure Qore IR/AOT and Java interpreted/JIT kernels."""
# Copyright (c) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
import urllib.request

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
KERNELS = ("arithmetic", "direct_call", "method_call", "list_read", "list_build",
           "list_mutate", "temporary_list", "temporary_hash")
MODES = ("qore-ir", "qore-aot", "java-int", "java-jit")
MAIN = "org.qore.bench.RuntimeBench"
# JMH 1.37 and its runtime/annotation-processor dependencies, from Maven Central.
ARTIFACTS = {
    "org/openjdk/jmh/jmh-core/1.37/jmh-core-1.37.jar":
        "dc0eaf2bbf0036a70b60798c785d6e03a9daf06b68b8edb0f1ba9eb3421baeb3",
    "org/openjdk/jmh/jmh-generator-annprocess/1.37/jmh-generator-annprocess-1.37.jar":
        "6a5604b5b804e0daca1145df1077609321687734a8b49387e49f10557c186c77",
    "net/sf/jopt-simple/jopt-simple/5.0.4/jopt-simple-5.0.4.jar":
        "df26cc58f235f477db07f753ba5a3ab243ebe5789d9f89ecf68dd62ea9a66c28",
    "org/apache/commons/commons-math3/3.6.1/commons-math3-3.6.1.jar":
        "1e56d7b058d28b65abd256b8458e3885b674c1d588fa43cd7d1cbb9c7ef2b308",
}


def digest(path):
    checksum = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def dependencies(directory):
    directory.mkdir(parents=True, exist_ok=True)
    result = []
    for artifact, expected in ARTIFACTS.items():
        path = directory / Path(artifact).name
        if not path.exists():
            print(f"Downloading {path.name}", flush=True)
            with urllib.request.urlopen("https://repo.maven.apache.org/maven2/" + artifact,
                                        timeout=60) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != expected:
                raise ValueError(f"dependency checksum mismatch: {artifact}")
            path.write_bytes(data)
        if digest(path) != expected:
            raise ValueError(f"cached dependency checksum mismatch: {path}")
        result.append(str(path))
    return result


def reference(name, count, seed):
    """Independent integer reference for pre-measurement correctness checks."""
    if name == "list_read":
        value = 7
        result = 0
        for _ in range(count):
            value = (value * 33 + 17) & 65535
            result += value ^ seed
        return result
    result = 0
    for _ in range(count):
        seed = (seed * 33 + 17) & 65535
        result += seed
        if name == "list_mutate":
            result += 1
        elif name in ("temporary_list", "temporary_hash"):
            result += seed & 1
    return result


def validate_checks(output):
    expected = {(name, count, seed): reference(name, count, seed)
                for name in KERNELS for count in (0, 1, 17, 1024, 4096)
                for seed in (0, 7, 65535)}
    actual = {}
    for line in output.splitlines():
        tag, name, count, seed, value = line.split("\t")
        key = (name, int(count), int(seed))
        if tag != "CHECK" or key in actual:
            raise ValueError(f"invalid/duplicate checksum record: {line}")
        actual[key] = int(value)
    if actual != expected:
        raise ValueError("kernel checksums do not match independent reference")


def parse_qore(output, name, count, warmup, samples, duration_ms):
    timings = []
    records = output.splitlines()
    if len(records) != warmup + samples:
        raise ValueError("missing or extra Qore timing records")
    for index, line in enumerate(records):
        tag, actual_name, elapsed, calls, actual_count, checksum = line.split("\t")
        elapsed, calls, actual_count = int(elapsed), int(calls), int(actual_count)
        if (tag != ("WARMUP" if index < warmup else "SAMPLE") or actual_name != name
                or actual_count != count or calls <= 0 or elapsed < duration_ms * 1000):
            raise ValueError(f"invalid timing record: {line}")
        # Parsing the checksum ensures it remains part of the observable output.
        # Full semantic equivalence is checked separately before measurements.
        int(checksum)
        if tag == "SAMPLE":
            timings.append(elapsed * 1000 / (calls * count))
    return timings


def summarize(values):
    return {"median": statistics.median(values), "min": min(values), "max": max(values),
            "mean": statistics.mean(values),
            "stdev": statistics.stdev(values) if len(values) > 1 else 0.0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qore", default=str(ROOT / "build/qore"))
    parser.add_argument("--qcc", default=str(ROOT / "build/qcc"))
    parser.add_argument("--java", default="java")
    parser.add_argument("--javac", default="javac")
    parser.add_argument("--baseline-libdir", help="also run selected Qore modes against a saved compatible libqore")
    parser.add_argument("--output-root", type=Path, default=ROOT / "build/bench-runtime-comparison")
    parser.add_argument("--modes", nargs="+", choices=MODES, default=list(MODES))
    parser.add_argument("--kernels", nargs="+", choices=KERNELS, default=list(KERNELS))
    parser.add_argument("--forks", type=int, default=3)
    parser.add_argument("--warmup", type=int, default=3)
    parser.add_argument("--samples", type=int, default=5)
    parser.add_argument("--time-ms", type=int, default=500)
    parser.add_argument("--count", type=int, default=1024)
    parser.add_argument("--startup-samples", type=int, default=5)
    parser.add_argument("--order-seed", type=int, default=20261006)
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    for name in ("forks", "samples", "startup_samples"):
        if not 1 <= getattr(args, name) <= 1000:
            parser.error(f"--{name.replace('_', '-')} must be in 1..1000")
    if not 0 <= args.warmup <= 1000 or not 1 <= args.time_ms <= 60000:
        parser.error("warmup must be in 0..1000; time-ms must be in 1..60000")
    if not 1 <= args.count <= 1000000:
        parser.error("count must be in 1..1000000")
    if len(set(args.modes)) != len(args.modes) or len(set(args.kernels)) != len(args.kernels):
        parser.error("duplicate modes/kernels")
    if args.baseline_libdir:
        args.baseline_libdir = str(Path(args.baseline_libdir).resolve())
        qore_modes = [mode for mode in args.modes if mode.startswith("qore-")]
        if not qore_modes or not Path(args.baseline_libdir).is_dir():
            parser.error("baseline-libdir requires a Qore mode and an existing library directory")
        library_name = "libqore.dylib" if sys.platform == "darwin" else "libqore.so"
        if not (Path(args.baseline_libdir) / library_name).is_file():
            parser.error(f"baseline-libdir must contain {library_name}")
        args.modes.extend(mode + "-before" for mode in qore_modes)

    args.output_root = args.output_root.resolve()
    args.output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output_root))
    print(f"Results: {output}", flush=True)
    sources = [p for p in HERE.iterdir() if p.suffix in (".py", ".qr", ".java")]
    for source in sources:
        shutil.copyfile(source, output / source.name)
    env = os.environ.copy()
    qore = str(Path(args.qore).resolve())
    qcc = str(Path(args.qcc).resolve())
    library_var = "DYLD_LIBRARY_PATH" if sys.platform == "darwin" else "LD_LIBRARY_PATH"
    env[library_var] = str(Path(qore).parent) + os.pathsep + env.get(library_var, "")
    mode_environments = {mode: env for mode in args.modes}
    if args.baseline_libdir:
        baseline_env = env.copy()
        baseline_env[library_var] = args.baseline_libdir + os.pathsep + env[library_var]
        for mode in args.modes:
            if mode.endswith("-before"):
                mode_environments[mode] = baseline_env
    commands = []

    def run(command, label, timeout=300, environment=None):
        command = list(map(str, command))
        commands.append({"label": label, "argv": command})
        (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        start = time.perf_counter_ns()
        result = subprocess.run(command, env=environment or env, text=True, capture_output=True, timeout=timeout)
        elapsed = (time.perf_counter_ns() - start) / 1e6
        (output / (label + ".stdout")).write_text(result.stdout)
        (output / (label + ".stderr")).write_text(result.stderr)
        if result.returncode:
            raise RuntimeError(f"{label} exited {result.returncode}; see {output / (label + '.stderr')}")
        return result.stdout, elapsed

    metadata = {"platform": platform.platform(), "machine": platform.machine(),
                "python": platform.python_version(), "settings": vars(args).copy(),
                "sources": {p.name: digest(output / p.name) for p in sources},
                "environment": {k: v for k, v in env.items() if k.startswith((
                    "QORE_DISABLE_", "QORE_IR_", "QORE_JIT_", "QORE_AOT_")) or
                                k in (library_var, "JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS")}}
    metadata["settings"]["output_root"] = str(args.output_root)
    metadata["revision"] = run(["git", "rev-parse", "HEAD"], "revision")[0].strip()
    metadata["worktree"] = run(["git", "status", "--short"], "worktree")[0]
    base = {}
    if any(mode.startswith("qore-") for mode in args.modes):
        metadata["qore_version"] = run([qore, "--version"], "qore-version")[0]
        metadata["qore_sha256"] = digest(qore)
        libraries = sorted(Path(qore).parent.glob("libqore*.dylib"))
        libraries += sorted(Path(qore).parent.glob("libqore.so*"))
        metadata["qore_libraries"] = {str(p.resolve()): digest(p) for p in libraries}
        base["qore-ir"] = [qore, "--exec-mode=ir", str(output / "kernels.qr")]
        if args.baseline_libdir:
            base["qore-ir-before"] = base["qore-ir"]
            libraries = sorted(Path(args.baseline_libdir).glob("libqore*.dylib"))
            libraries += sorted(Path(args.baseline_libdir).glob("libqore.so*"))
            metadata["baseline_libraries"] = {str(p.resolve()): digest(p) for p in libraries}
            metadata["baseline_library_path"] = baseline_env[library_var]
    if "qore-aot" in args.modes:
        executable = output / "kernels-aot"
        _, compile_ms = run([qcc, "-O3", "-o", executable, output / "kernels.qr"], "qcc", timeout=600)
        metadata["aot_compile_ms"] = compile_ms
        metadata["qcc_sha256"] = digest(qcc)
        metadata["aot_sha256"] = digest(executable)
        base["qore-aot"] = [str(executable)]
        if args.baseline_libdir:
            baseline_executable = output / "kernels-aot-before"
            _, compile_ms = run([qcc, "-O3", "-o", baseline_executable, output / "kernels.qr"],
                                "qcc-before", timeout=600, environment=baseline_env)
            metadata["baseline_aot_compile_ms"] = compile_ms
            metadata["baseline_aot_sha256"] = digest(baseline_executable)
            base["qore-aot-before"] = [str(baseline_executable)]
    classpath = ""
    java = shutil.which(args.java)
    if any(mode.startswith("java-") for mode in args.modes):
        javac = shutil.which(args.javac)
        if not java or not javac:
            raise ValueError("java and javac must be available (or select only Qore modes)")
        run([java, "-version"], "java-version")
        metadata["java_version"] = (output / "java-version.stderr").read_text()
        jars = dependencies(args.output_root / "deps")
        classes = output / "classes"
        classes.mkdir()
        jar_path = os.pathsep.join(jars)
        run([javac, "-cp", jar_path, "-processorpath", jar_path,
             "-processor", "org.openjdk.jmh.generators.BenchmarkProcessor", "-d", classes,
             output / "RuntimeBench.java"], "javac")
        classpath = os.pathsep.join([str(classes)] + jars)
        base["java-int"] = [java, "-Xint", "-cp", classpath, MAIN]
        base["java-jit"] = [java, "-cp", classpath, MAIN]
    (output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")

    for mode in args.modes:
        validate_checks(run(base[mode] + ["check"], "check-" + mode,
                            environment=mode_environments[mode])[0])
        print(f"{mode}: 120 reference checks passed", flush=True)
    if args.check_only:
        return

    rng = random.Random(args.order_seed)
    startup = {mode: [] for mode in args.modes}
    startup_jobs = [(mode, sample) for mode in args.modes for sample in range(args.startup_samples)]
    rng.shuffle(startup_jobs)
    for mode, sample in startup_jobs:
        result, elapsed = run(base[mode] + ["startup"], f"startup-{mode}-{sample}",
                              environment=mode_environments[mode])
        if result != "ready\n":
            raise ValueError(f"invalid startup result for {mode}: {result!r}")
        startup[mode].append(elapsed)

    results = []
    jobs = [(mode, kernel, fork) for fork in range(args.forks)
            for mode in args.modes for kernel in args.kernels]
    rng.shuffle(jobs)
    timeout = max(120, (args.warmup + args.samples) * args.time_ms / 1000 * 3 + 60)
    for position, (mode, kernel, fork) in enumerate(jobs, 1):
        label = f"{mode}-{kernel}-{fork}"
        print(f"[{position}/{len(jobs)}] {label}", flush=True)
        allocation = None
        if mode.startswith("qore-"):
            stdout, _ = run(base[mode] + [kernel, args.count, args.warmup, args.samples,
                                          args.time_ms, 7], label, timeout, mode_environments[mode])
            values = parse_qore(stdout, kernel, args.count, args.warmup, args.samples, args.time_ms)
        else:
            result_path = output / (label + ".json")
            jvm_args = "-Xms256m -Xmx256m" + (" -Xint" if mode == "java-int" else "")
            run([java, "-cp", classpath, "org.openjdk.jmh.Main", MAIN + "." + kernel + "$",
                 "-f", 1, "-t", 1, "-wi", args.warmup, "-i", args.samples,
                 "-w", f"{args.time_ms}ms", "-r", f"{args.time_ms}ms",
                 "-p", f"count={args.count}", "-p", "seed=7", "-bm", "avgt", "-tu", "ns",
                 "-jvmArgs", jvm_args, "-prof", "gc", "-foe", "true",
                 "-rf", "json", "-rff", result_path], label, timeout)
            data = json.loads(result_path.read_text())
            if len(data) != 1 or data[0]["benchmark"] != MAIN + "." + kernel:
                raise ValueError(f"missing/unexpected JMH benchmark: {label}")
            metric = data[0]["primaryMetric"]
            if metric["scoreUnit"] != "ns/op":
                raise ValueError(f"unexpected JMH unit: {metric['scoreUnit']}")
            values = [v / args.count for trial in metric["rawData"] for v in trial]
            if len(values) != args.samples or not all(math.isfinite(v) and v > 0 for v in values):
                raise ValueError(f"invalid JMH sample data: {label}")
            metric = data[0]["secondaryMetrics"].get("gc.alloc.rate.norm")
            if metric and isinstance(metric["score"], (int, float)):
                allocation = metric["score"] / args.count
        results.append({"mode": mode, "kernel": kernel, "fork": fork,
                        "ns_per_element": values, "java_bytes_per_element": allocation})
        (output / "samples.json").write_text(json.dumps(results, indent=2) + "\n")

    summary = {"startup_ms": {mode: {"samples": values, **summarize(values)}
                              for mode, values in startup.items()}, "runtime": []}
    print("\nMedian of fork means, ns/element (startup excluded):")
    width = max(14, max(map(len, args.modes)) + 2)
    print(f"{'kernel':20s}" + "".join(f"{mode:>{width}s}" for mode in args.modes))
    for kernel in args.kernels:
        row = f"{kernel:20s}"
        for mode in args.modes:
            group = [r for r in results if r["mode"] == mode and r["kernel"] == kernel]
            means = [statistics.mean(r["ns_per_element"]) for r in group]
            item = {"mode": mode, "kernel": kernel, "fork_means_ns": means, **summarize(means)}
            allocations = [r["java_bytes_per_element"] for r in group
                           if r["java_bytes_per_element"] is not None]
            if allocations:
                item["java_bytes_per_element"] = statistics.median(allocations)
            summary["runtime"].append(item)
            row += f"{item['median']:{width}.2f}"
        print(row)
    print("\nProcess launch-to-exit medians (filesystem caches uncontrolled):")
    for mode, values in startup.items():
        print(f"  {mode}: {statistics.median(values):.2f} ms")
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Raw samples, commands, build logs, and metadata: {output}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError, subprocess.SubprocessError) as error:
        sys.exit(str(error))
