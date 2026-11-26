// Copyright 2026 Qore Technologies, s.r.o. -- SPDX-License-Identifier: MIT
package org.qore.bench;

import java.util.concurrent.TimeUnit;
import java.util.function.DoubleSupplier;
import java.util.function.DoubleUnaryOperator;
import java.util.function.LongSupplier;
import java.util.function.LongUnaryOperator;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.State;

// Exact counterparts of opaque_callbacks.qr. Select one of two known references
// once per invocation, then invoke it inside the loop. Normal HotSpot inlining
// and constant reduction are allowed; no DONT_INLINE or interpretation controls.
// Results are whole-kernel ns/item, not isolated callback latency.
@State(Scope.Thread)
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
public class OpaqueCallbackBench {
    @Param({"4096"}) public int count;
    @Param({"7"}) public long seed;

    static long intZeroA() { return 17; }
    static long intZeroB() { return 19; }
    static long intUnaryA(long value) { return (value * 33 + 17) & 65535; }
    static long intUnaryB(long value) { return (value * 33 + 19) & 65535; }
    static double floatZeroA() { return 0.25; }
    static double floatZeroB() { return 0.5; }
    static double floatUnaryA(double value) { return 65535.5 - value; }
    static double floatUnaryB(double value) { return 65534.5 - value; }

    static long intZero(long count, long seed) {
        LongSupplier fn = (seed & 1) != 0
            ? OpaqueCallbackBench::intZeroA : OpaqueCallbackBench::intZeroB;
        long sum = 0;
        for (long i = 0; i < count; ++i) { sum += fn.getAsLong(); }
        return sum;
    }
    static long intUnary(long count, long seed) {
        LongUnaryOperator fn = (seed & 1) != 0
            ? OpaqueCallbackBench::intUnaryA : OpaqueCallbackBench::intUnaryB;
        long value = seed;
        long sum = 0;
        for (long i = 0; i < count; ++i) { value = fn.applyAsLong(value); sum += value; }
        return sum;
    }
    static long floatZero(long count, long seed) {
        DoubleSupplier fn = (seed & 1) != 0
            ? OpaqueCallbackBench::floatZeroA : OpaqueCallbackBench::floatZeroB;
        double sum = 0.0;
        for (long i = 0; i < count; ++i) { sum += fn.getAsDouble(); }
        return (long) (sum * 2.0);
    }
    static long floatUnary(long count, long seed) {
        DoubleUnaryOperator fn = (seed & 1) != 0
            ? OpaqueCallbackBench::floatUnaryA : OpaqueCallbackBench::floatUnaryB;
        double value = (double) seed;
        double sum = 0.0;
        for (long i = 0; i < count; ++i) { value = fn.applyAsDouble(value); sum += value; }
        return (long) (sum * 2.0);
    }
    private long nextSeed() { seed = (seed + 1) & 65535; return seed; }
    @Benchmark public long int_zero() { return intZero(count, nextSeed()); }
    @Benchmark public long int_unary() { return intUnary(count, nextSeed()); }
    @Benchmark public long float_zero() { return floatZero(count, nextSeed()); }
    @Benchmark public long float_unary() { return floatUnary(count, nextSeed()); }

    public static void main(String[] args) {
        if (args.length != 1 || !args[0].equals("check")) {
            throw new IllegalArgumentException("expected check");
        }
        for (long count : new long[] {0, 1, 17, 257, 4096}) {
            for (long seed : new long[] {0, 7, 65535}) {
                System.out.printf("CHECK\tint_zero\t%d\t%d\t%d%n", count, seed, intZero(count, seed));
                System.out.printf("CHECK\tint_unary\t%d\t%d\t%d%n", count, seed, intUnary(count, seed));
                System.out.printf("CHECK\tfloat_zero\t%d\t%d\t%d%n", count, seed, floatZero(count, seed));
                System.out.printf("CHECK\tfloat_unary\t%d\t%d\t%d%n", count, seed, floatUnary(count, seed));
            }
        }
    }
}
