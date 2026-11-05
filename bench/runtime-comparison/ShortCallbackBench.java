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

// Match short_callbacks.qr: batch short scalar invocations with changing data
// and a dependent checksum. Normal HotSpot inlining is allowed. Results are
// whole-batch ns/invocation; inline controls are not subtracted as call latency.
@State(Scope.Thread)
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
public class ShortCallbackBench {
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
            ? ShortCallbackBench::intZeroA : ShortCallbackBench::intZeroB;
        long sum = 0;
        for (long i = 0; i < count; ++i) { sum += fn.getAsLong(); }
        return sum;
    }
    static long intUnary(long count, long seed) {
        LongUnaryOperator fn = (seed & 1) != 0
            ? ShortCallbackBench::intUnaryA : ShortCallbackBench::intUnaryB;
        long value = seed;
        long sum = 0;
        for (long i = 0; i < count; ++i) { value = fn.applyAsLong(value); sum += value; }
        return sum;
    }
    static long floatZero(long count, long seed) {
        DoubleSupplier fn = (seed & 1) != 0
            ? ShortCallbackBench::floatZeroA : ShortCallbackBench::floatZeroB;
        double sum = 0.0;
        for (long i = 0; i < count; ++i) { sum += fn.getAsDouble(); }
        return (long) (sum * 2.0);
    }
    static long floatUnary(long count, long seed) {
        DoubleUnaryOperator fn = (seed & 1) != 0
            ? ShortCallbackBench::floatUnaryA : ShortCallbackBench::floatUnaryB;
        double value = (double) seed;
        double sum = 0.0;
        for (long i = 0; i < count; ++i) { value = fn.applyAsDouble(value); sum += value; }
        return (long) (sum * 2.0);
    }
    static long intZero1Batch(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + intZero(1, seed)) & 65535;
        }
        return checksum;
    }
    static long intZero17Batch(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + intZero(17, seed)) & 65535;
        }
        return checksum;
    }
    static long intUnary1Batch(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + intUnary(1, seed)) & 65535;
        }
        return checksum;
    }
    static long intUnary17Batch(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + intUnary(17, seed)) & 65535;
        }
        return checksum;
    }
    static long floatZero1Batch(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + floatZero(1, seed)) & 65535;
        }
        return checksum;
    }
    static long floatZero17Batch(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + floatZero(17, seed)) & 65535;
        }
        return checksum;
    }
    static long floatUnary1Batch(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + floatUnary(1, seed)) & 65535;
        }
        return checksum;
    }
    static long floatUnary17Batch(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + floatUnary(17, seed)) & 65535;
        }
        return checksum;
    }
    static long inlineIntZero(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + (((seed & 1) != 0) ? 17 : 19)) & 65535;
        }
        return checksum;
    }
    static long inlineIntUnary(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + ((seed * 33 + (((seed & 1) != 0) ? 17 : 19)) & 65535)) & 65535;
        }
        return checksum;
    }
    static long inlineFloatZero(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + (((seed & 1) != 0) ? 0 : 1)) & 65535;
        }
        return checksum;
    }
    static long inlineFloatUnary(long count, long seed) {
        long checksum = 0;
        for (long i = 0; i < count; ++i) {
            seed = (seed + 1) & 65535;
            checksum = (checksum * 33 + ((((seed & 1) != 0) ? 131071 : 131069) - 2 * seed)) & 65535;
        }
        return checksum;
    }
    private long nextSeed() { seed = (seed + 1) & 65535; return seed; }
    @Benchmark public long int_zero_1() { return intZero1Batch(count, nextSeed()); }
    @Benchmark public long int_zero_17() { return intZero17Batch(count, nextSeed()); }
    @Benchmark public long int_unary_1() { return intUnary1Batch(count, nextSeed()); }
    @Benchmark public long int_unary_17() { return intUnary17Batch(count, nextSeed()); }
    @Benchmark public long float_zero_1() { return floatZero1Batch(count, nextSeed()); }
    @Benchmark public long float_zero_17() { return floatZero17Batch(count, nextSeed()); }
    @Benchmark public long float_unary_1() { return floatUnary1Batch(count, nextSeed()); }
    @Benchmark public long float_unary_17() { return floatUnary17Batch(count, nextSeed()); }
    @Benchmark public long int_zero_inline() { return inlineIntZero(count, nextSeed()); }
    @Benchmark public long int_unary_inline() { return inlineIntUnary(count, nextSeed()); }
    @Benchmark public long float_zero_inline() { return inlineFloatZero(count, nextSeed()); }
    @Benchmark public long float_unary_inline() { return inlineFloatUnary(count, nextSeed()); }
    public static void main(String[] args) {
        if (args.length != 1 || !args[0].equals("check")) { throw new IllegalArgumentException("expected check"); }
        for (long count : new long[] {0, 1, 17, 257, 4096}) {
            for (long seed : new long[] {0, 7, 65535}) {
                System.out.printf("CHECK\tint_zero_1\t%d\t%d\t%d%n", count, seed, intZero1Batch(count, seed));
                System.out.printf("CHECK\tint_zero_17\t%d\t%d\t%d%n", count, seed, intZero17Batch(count, seed));
                System.out.printf("CHECK\tint_unary_1\t%d\t%d\t%d%n", count, seed, intUnary1Batch(count, seed));
                System.out.printf("CHECK\tint_unary_17\t%d\t%d\t%d%n", count, seed, intUnary17Batch(count, seed));
                System.out.printf("CHECK\tfloat_zero_1\t%d\t%d\t%d%n", count, seed, floatZero1Batch(count, seed));
                System.out.printf("CHECK\tfloat_zero_17\t%d\t%d\t%d%n", count, seed, floatZero17Batch(count, seed));
                System.out.printf("CHECK\tfloat_unary_1\t%d\t%d\t%d%n", count, seed, floatUnary1Batch(count, seed));
                System.out.printf("CHECK\tfloat_unary_17\t%d\t%d\t%d%n", count, seed, floatUnary17Batch(count, seed));
                System.out.printf("CHECK\tint_zero_inline\t%d\t%d\t%d%n", count, seed, inlineIntZero(count, seed));
                System.out.printf("CHECK\tint_unary_inline\t%d\t%d\t%d%n", count, seed, inlineIntUnary(count, seed));
                System.out.printf("CHECK\tfloat_zero_inline\t%d\t%d\t%d%n", count, seed, inlineFloatZero(count, seed));
                System.out.printf("CHECK\tfloat_unary_inline\t%d\t%d\t%d%n", count, seed, inlineFloatUnary(count, seed));
            }
        }
    }
}
