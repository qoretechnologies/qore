#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
#
# JIT Benchmark: numeric loop comparing --exec-mode=jit vs --exec-mode=ir vs --exec-mode=ast
#
# Run with: qore --exec-mode=<mode> JITBenchmark.q

%modern
%disable-warning unreferenced-variable

# Sum integers 1..N
int sub sum_int(int n) {
    int sum = 0;
    for (int i = 1; i <= n; i++) {
        sum += i;
    }
    return sum;
}

# Fibonacci iterative
int sub fib(int n) {
    if (n <= 1) {
        return n;
    }
    int a = 0;
    int b = 1;
    for (int i = 2; i <= n; i++) {
        int tmp = a + b;
        a = b;
        b = tmp;
    }
    return b;
}

# Float computation
float sub float_sum(int n) {
    float sum = 0.0;
    for (int i = 1; i <= n; i++) {
        sum += i * 1.0;
    }
    return sum;
}

# Integer comparison chain
int sub classify_many(int n) {
    int pos = 0;
    int neg = 0;
    int zero = 0;
    for (int i = -n; i <= n; i++) {
        if (i > 0) {
            pos++;
        } else if (i < 0) {
            neg++;
        } else {
            zero++;
        }
    }
    return pos + neg + zero;
}

# Warmup
sum_int(100);
fib(20);
float_sum(100);
classify_many(100);

int N = 10000;

# Benchmark: sum
date t0 = now_us();
int sum_result = sum_int(N);
date t1 = now_us();

# Benchmark: fib
date t2 = now_us();
int fib_result = fib(40);
date t3 = now_us();

# Benchmark: float
date t4 = now_us();
float float_result = float_sum(N);
date t5 = now_us();

# Benchmark: classify
date t6 = now_us();
int class_result = classify_many(N);
date t7 = now_us();

# Verify correctness
@assert(sum_result == N * (N + 1) / 2);
@assert(fib_result == 102334155);
@assert(class_result == 2 * N + 1);

int sum_us = get_duration_microseconds(t1 - t0);
int fib_us = get_duration_microseconds(t3 - t2);
int float_us = get_duration_microseconds(t5 - t4);
int class_us = get_duration_microseconds(t7 - t6);

printf("sum_int(%d):     %d us  (result: %d)\n", N, sum_us, sum_result);
printf("fib(40):         %d us  (result: %d)\n", fib_us, fib_result);
printf("float_sum(%d):   %d us  (result: %.0f)\n", N, float_us, float_result);
printf("classify(%d):    %d us  (result: %d)\n", N, class_us, class_result);
printf("total:           %d us\n", sum_us + fib_us + float_us + class_us);
