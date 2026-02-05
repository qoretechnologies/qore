#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%strict-args
%require-types

# Benchmark script to compare AST, IR, and JIT execution modes

const ITERATIONS = 1000000;
const WARMUP = 100000;

# Test 1: Simple loop computation
string loop_code = sprintf("%%modern
int sum = 0;
for (int i = 0; i < %d; ++i) {
    sum += i;
}
printf(\"%%d\\n\", sum);
", ITERATIONS);

# Test 2: Final class method calls (tests InvokeMethodDirect in try/catch)
string method_code = sprintf("%%modern
%%disable-warning non-existent-method-call

final class Accumulator {
    public {
        int total = 0;
    }

    int add(int x) {
        total += x;
        return total;
    }

    int compute() {
        int result = 0;
        for (int i = 0; i < %d; ++i) {
            try {
                result = self.add(1);
            } catch (hash<auto> ex) {
                # won't happen
            }
        }
        return result;
    }
}

Accumulator a();
printf(\"%%d\\n\", a.compute());
", ITERATIONS / 10);

# Test 3: Final class method calls without try/catch (tests CallMethodDirect)
string method_direct_code = sprintf("%%modern
%%disable-warning non-existent-method-call

final class DirectAccum {
    public {
        int total = 0;
    }

    int add(int x) {
        total += x;
        return total;
    }

    int compute() {
        int result = 0;
        for (int i = 0; i < %d; ++i) {
            result = self.add(1);
        }
        return result;
    }
}

DirectAccum a();
printf(\"%%d\\n\", a.compute());
", ITERATIONS / 10);

# Test 4: Functional operator - foldl sum (using range() for fair comparison)
string foldl_code = sprintf("%%modern
list<int> nums = range(%d);
int sum = (foldl $1 + $2, nums, 0);
printf(\"%%d\\n\", sum);
", ITERATIONS / 100);

# Test 5: Functional operator - map (using range() for fair comparison)
string map_code = sprintf("%%modern
list<int> nums = range(%d);
list<int> doubled = (map $1 * 2, nums);
printf(\"%%d\\n\", doubled.size());
", ITERATIONS / 100);

# Test 6: Recursive fibonacci (tests deep call stack)
string fib_code = "%modern
int fib(int n) {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
printf(\"%d\\n\", fib(30));
";

hash<string, string> tests = {
    "loop": loop_code,
    "method_try": method_code,
    "method_direct": method_direct_code,
    "foldl": foldl_code,
    "map": map_code,
    "fibonacci": fib_code,
};

string qore_bin = ENV.QORE_BIN ?? QORE_ARGV[0] ?? "qore";
string ld_path = ENV.LD_LIBRARY_PATH ?? "";

hash<auto> sub run_benchmark(string name, string code, string mode) {
    # Write code to temp file
    string tmp = sprintf("/tmp/bench_%s.q", name);
    File f();
    f.open(tmp, O_CREAT | O_WRONLY | O_TRUNC);
    f.write(code);
    f.close();

    string cmd;
    if (mode == "aot") {
        # For AOT, we need to compile first, then run
        string aot_exe = sprintf("/tmp/bench_%s_aot", name);
        string compile_cmd = sprintf("LD_LIBRARY_PATH=%s %s --compile --output=%s %s 2>/dev/null",
            ld_path, qore_bin, aot_exe, tmp);
        system(compile_cmd);
        cmd = sprintf("LD_LIBRARY_PATH=%s %s 2>/dev/null", ld_path, aot_exe);
    } else {
        cmd = sprintf("LD_LIBRARY_PATH=%s %s --exec-mode=%s %s 2>/dev/null", ld_path, qore_bin, mode, tmp);
    }

    date start = now_us();
    string output = backquote(cmd);
    date end = now_us();

    int elapsed_us = (end - start).durationMicroseconds();
    float elapsed_ms = elapsed_us / 1000.0;

    return {
        "elapsed_ms": elapsed_ms,
        "output": trim(output),
    };
}

sub main() {
    printf("Performance Benchmark: AST vs IR vs JIT\n");
    printf("======================================================================\n\n");

    list<string> modes = ("ast", "ir", "jit");  # AOT excluded - has LLVM terminator bug

    # Results table
    hash<string, hash<string, float>> results;

    foreach string test_name in (keys tests) {
        string code = tests{test_name};
        printf("Test: %s\n", test_name);
        printf("--------------------------------------------------\n");

        hash<string, float> test_results;
        string expected_output;
        bool have_expected = False;

        foreach string mode in (modes) {
            hash<auto> r = run_benchmark(test_name, code, mode);
            test_results{mode} = r.elapsed_ms;
            if (!have_expected) {
                expected_output = r.output;
                have_expected = True;
            }
            string status = r.output == expected_output ? "OK" : "MISMATCH";
            printf("  %-4s: %8.2f ms  [%s]\n", mode, r.elapsed_ms, status);
        }

        # Calculate speedups relative to AST
        float ast_time = test_results.ast;
        printf("  Speedup vs AST:\n");
        foreach string mode in (modes) {
            if (mode != "ast") {
                float speedup = ast_time / test_results{mode};
                printf("    %-4s: %.2fx\n", mode, speedup);
            }
        }

        results{test_name} = test_results;
        printf("\n");
    }

    # Summary table
    printf("======================================================================\n");
    printf("SUMMARY (times in ms)\n");
    printf("======================================================================\n");
    printf("%-15s %10s %10s %10s\n", "Test", "AST", "IR", "JIT");
    printf("----------------------------------------------------------------------\n");
    foreach string test_name in (keys results) {
        hash<string, float> r = results{test_name};
        printf("%-15s %10.2f %10.2f %10.2f\n",
            test_name, r.ast, r.ir, r.jit);
    }
    printf("----------------------------------------------------------------------\n");

    # Average speedups
    printf("\nAverage Speedup vs AST:\n");
    foreach string mode in (modes) {
        if (mode == "ast") {
            continue;
        }
        float total_speedup = 0.0;
        int count = 0;
        foreach string test_name in (keys results) {
            hash<string, float> r = results{test_name};
            if (r{mode} > 0) {
                total_speedup += r.ast / r{mode};
                ++count;
            }
        }
        if (count > 0) {
            printf("  %-4s: %.2fx\n", mode, total_speedup / count);
        }
    }
}

main();
