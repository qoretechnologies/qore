// Copyright (c) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: MIT
package org.qore.bench;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.TimeUnit;
import java.util.function.LongUnaryOperator;
import java.util.function.DoubleUnaryOperator;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.CompilerControl;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;

// One JMH operation is one count-element kernel; run.py normalizes ns/element.
@State(Scope.Thread)
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
public class RuntimeBench {
    @Param({"1024"})
    public int count;
    @Param({"7"})
    public long seed;
    private ArrayList<Long> data;
    private Stepper receiver;
    private LongUnaryOperator callback;
    private LongUnaryOperator closureCallback, capturedCallback;
    private LongUnaryOperator wideCallback;
    private LongUnaryOperator affineCallback, branchIntCallback;
    private DoubleUnaryOperator floatCallback, taggedCallback, branchCallback;
    private DynamicStep a, b, c;

    @Setup
    public void setup() {
        data = makeData(count, 7);
        receiver = new Stepper();
        callback = RuntimeBench::boundaryStep;
        closureCallback = new ClosureStep();
        capturedCallback = new CapturedStep(17);
        wideCallback = RuntimeBench::wideStep;
        affineCallback = RuntimeBench::affineStep;
        branchIntCallback = RuntimeBench::branchIntStep;
        floatCallback = RuntimeBench::floatStep;
        taggedCallback = RuntimeBench::taggedFloatStep;
        branchCallback = RuntimeBench::branchFloatStep;
        a = new DynamicStep();
        b = new DynamicStepB();
        c = new DynamicStepC();
    }

    static long step(long value) {
        return (value * 33 + 17) & 65535;
    }

    static final class ClosureStep implements LongUnaryOperator {
        @Override @CompilerControl(CompilerControl.Mode.DONT_INLINE)
        public long applyAsLong(long value) { return (value * 33 + 17) & 65535; }
    }

    // Non-final storage models Qore's captured variable, not a constant folded
    // into the target body. Neither workload mutates this capture while timed.
    static final class CapturedStep implements LongUnaryOperator {
        private long increment;
        CapturedStep(long increment) { this.increment = increment; }
        @Override @CompilerControl(CompilerControl.Mode.DONT_INLINE)
        public long applyAsLong(long value) { return (value * 33 + increment) & 65535; }
    }

    static final class Stepper {
        long next(long value) {
            return (value * 33 + 17) & 65535;
        }
    }

    @CompilerControl(CompilerControl.Mode.DONT_INLINE)
    static long boundaryStep(long value) {
        return (value * 33 + 17) & 65535;
    }

    @CompilerControl(CompilerControl.Mode.DONT_INLINE)
    static long wideStep(long value) {
        return ((value * 33 + 17) & 65535) | 1125899906842624L;
    }
    @CompilerControl(CompilerControl.Mode.DONT_INLINE)
    static long affineStep(long value) {
        return value + 17;
    }
    @CompilerControl(CompilerControl.Mode.DONT_INLINE)
    static long branchIntStep(long value) {
        if (value > 32767) { return value - 32767; }
        return value + 17;
    }
    @CompilerControl(CompilerControl.Mode.DONT_INLINE)
    static double floatStep(double value) {
        return 65535.0 - value;
    }
    @CompilerControl(CompilerControl.Mode.DONT_INLINE)
    static double branchFloatStep(double value) {
        return value > 32767.0 ? value - 32767.0 : value + 17.0;
    }
    @CompilerControl(CompilerControl.Mode.DONT_INLINE)
    static double taggedFloatStep(double value) {
        return -9.0e307 - value;
    }

    static class DynamicStep {
        @CompilerControl(CompilerControl.Mode.DONT_INLINE)
        long next(long value) { return (value * 33 + 17) & 65535; }
    }
    static class DynamicStepB extends DynamicStep {
        @Override @CompilerControl(CompilerControl.Mode.DONT_INLINE)
        long next(long value) { return (value * 33 + 19) & 65535; }
    }
    static class DynamicStepC extends DynamicStep {
        @Override @CompilerControl(CompilerControl.Mode.DONT_INLINE)
        long next(long value) { return (value * 33 + 23) & 65535; }
    }

    static long indirectCall(int count, long seed, LongUnaryOperator callback) {
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            seed = callback.applyAsLong(seed);
            sum += seed;
        }
        return sum;
    }

    static long wideIndirectCall(int count, long seed, LongUnaryOperator callback) {
        long value = seed | 1125899906842624L;
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            value = callback.applyAsLong(value);
            sum += value & 65535;
        }
        return sum;
    }

    static long floatIndirectCall(int count, long seed, DoubleUnaryOperator callback) {
        double value = seed;
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            value = callback.applyAsDouble(value);
            sum += (long) value;
        }
        return sum;
    }

    static long taggedFloatIndirectCall(int count, long seed, DoubleUnaryOperator callback) {
        double value = (seed & 1) != 0 ? -3.0e307 : -6.0e307;
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            value = callback.applyAsDouble(value);
            sum += value < -4.5e307 ? 6 : 3;
        }
        return sum;
    }

    static long polymorphicCall(int count, long seed, DynamicStep a, DynamicStep b, DynamicStep c) {
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            DynamicStep receiver = i % 3 == 0 ? a : i % 3 == 1 ? b : c;
            seed = receiver.next(seed);
            sum += seed;
        }
        return sum;
    }

    static long dispatchControl(int count, long seed) {
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            long increment = i % 3 == 0 ? 17 : i % 3 == 1 ? 19 : 23;
            seed = (seed * 33 + increment) & 65535;
            sum += seed;
        }
        return sum;
    }

    static long arithmetic(int count, long seed) {
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            seed = (seed * 33 + 17) & 65535;
            sum += seed;
        }
        return sum;
    }

    static long directCall(int count, long seed) {
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            seed = step(seed);
            sum += seed;
        }
        return sum;
    }

    static long methodCall(int count, long seed, Stepper receiver) {
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            seed = receiver.next(seed);
            sum += seed;
        }
        return sum;
    }

    static ArrayList<Long> makeData(int count, long seed) {
        ArrayList<Long> values = new ArrayList<>();
        for (int i = 0; i < count; ++i) {
            seed = (seed * 33 + 17) & 65535;
            values.add(seed);
        }
        return values;
    }

    static long listRead(ArrayList<Long> values, long seed) {
        long sum = 0;
        for (int i = 0; i < values.size(); ++i) {
            sum += values.get(i) ^ seed;
        }
        return sum;
    }

    static long listBuild(int count, long seed) {
        ArrayList<Long> values = new ArrayList<>();
        for (int i = 0; i < count; ++i) {
            seed = (seed * 33 + 17) & 65535;
            values.add(seed);
        }
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            sum += values.get(i);
        }
        return sum;
    }

    static long listMutate(int count, long seed) {
        ArrayList<Long> values = new ArrayList<>();
        for (int i = 0; i < count; ++i) {
            seed = (seed * 33 + 17) & 65535;
            values.add(seed);
        }
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            values.set(i, values.get(i) + 1);
            sum += values.get(i);
        }
        return sum;
    }

    static long temporaryList(int count, long seed) {
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            seed = (seed * 33 + 17) & 65535;
            long[] pair = {seed, seed + 1};
            sum += pair[(int) (seed & 1)];
        }
        return sum;
    }

    static long temporaryHash(int count, long seed) {
        long sum = 0;
        for (int i = 0; i < count; ++i) {
            seed = (seed * 33 + 17) & 65535;
            HashMap<String, Long> pair = new HashMap<>();
            pair.put("left", seed);
            pair.put("right", seed + 1);
            String key = (seed & 1) != 0 ? "right" : "left";
            sum += pair.get(key);
        }
        return sum;
    }

    private long nextSeed() {
        seed = (seed + 1) & 65535;
        return seed;
    }

    @Benchmark public long arithmetic() { return arithmetic(count, nextSeed()); }
    @Benchmark public long direct_call() { return directCall(count, nextSeed()); }
    @Benchmark public long method_call() { return methodCall(count, nextSeed(), receiver); }
    @Benchmark public long indirect_call() { return indirectCall(count, nextSeed(), callback); }
    @Benchmark public long closure_call() { return indirectCall(count, nextSeed(), closureCallback); }
    @Benchmark public long closure_capture() { return indirectCall(count, nextSeed(), capturedCallback); }
    @Benchmark public long indirect_wide() { return wideIndirectCall(count, nextSeed(), wideCallback); }
    @Benchmark public long indirect_affine() { return indirectCall(count, nextSeed(), affineCallback); }
    @Benchmark public long indirect_affine_wide() { return wideIndirectCall(count, nextSeed(), affineCallback); }
    @Benchmark public long indirect_branch_int() { return indirectCall(count, nextSeed(), branchIntCallback); }
    @Benchmark public long indirect_float() { return floatIndirectCall(count, nextSeed(), floatCallback); }
    @Benchmark public long indirect_branch_float() { return floatIndirectCall(count, nextSeed(), branchCallback); }
    @Benchmark public long indirect_tagged_float() { return taggedFloatIndirectCall(count, nextSeed(), taggedCallback); }
    @Benchmark public long polymorphic_call() { return polymorphicCall(count, nextSeed(), a, b, c); }
    @Benchmark public long dispatch_control() { return dispatchControl(count, nextSeed()); }
    @Benchmark public long list_read() { return listRead(data, nextSeed()); }
    @Benchmark public long list_build() { return listBuild(count, nextSeed()); }
    @Benchmark public long list_mutate() { return listMutate(count, nextSeed()); }
    @Benchmark public long temporary_list() { return temporaryList(count, nextSeed()); }
    @Benchmark public long temporary_hash() { return temporaryHash(count, nextSeed()); }

    public static void main(String[] args) {
        if (args.length == 1 && args[0].equals("startup")) {
            System.out.println("ready");
            return;
        }
        if (args.length != 1 || !args[0].equals("check")) {
            throw new IllegalArgumentException("expected startup or check; use JMH for measurements");
        }
        Stepper receiver = new Stepper();
        LongUnaryOperator callback = RuntimeBench::boundaryStep;
        LongUnaryOperator closureCallback = new ClosureStep(), capturedCallback = new CapturedStep(17);
        LongUnaryOperator wideCallback = RuntimeBench::wideStep;
        LongUnaryOperator affineCallback = RuntimeBench::affineStep, branchIntCallback = RuntimeBench::branchIntStep;
        DoubleUnaryOperator floatCallback = RuntimeBench::floatStep, taggedCallback = RuntimeBench::taggedFloatStep;
        DoubleUnaryOperator branchCallback = RuntimeBench::branchFloatStep;
        DynamicStep a = new DynamicStep(), b = new DynamicStepB(), c = new DynamicStepC();
        for (int count : new int[] {0, 1, 17, 1024, 4096}) {
            ArrayList<Long> values = makeData(count, 7);
            for (long seed : new long[] {0, 7, 65535}) {
                String[] names = {"arithmetic", "direct_call", "method_call", "indirect_call",
                    "indirect_wide", "indirect_float", "indirect_tagged_float", "indirect_branch_float",
                    "indirect_affine", "indirect_affine_wide", "indirect_branch_int",
                    "polymorphic_call", "dispatch_control", "list_read",
                    "list_build", "list_mutate", "temporary_list", "temporary_hash",
                    "closure_call", "closure_capture"};
                long[] results = {arithmetic(count, seed), directCall(count, seed),
                    methodCall(count, seed, receiver), indirectCall(count, seed, callback),
                    wideIndirectCall(count, seed, wideCallback), floatIndirectCall(count, seed, floatCallback),
                    taggedFloatIndirectCall(count, seed, taggedCallback),
                    floatIndirectCall(count, seed, branchCallback),
                    indirectCall(count, seed, affineCallback), wideIndirectCall(count, seed, affineCallback),
                    indirectCall(count, seed, branchIntCallback),
                    polymorphicCall(count, seed, a, b, c), dispatchControl(count, seed),
                    listRead(values, seed), listBuild(count, seed),
                    listMutate(count, seed), temporaryList(count, seed), temporaryHash(count, seed),
                    indirectCall(count, seed, closureCallback), indirectCall(count, seed, capturedCallback)};
                for (int i = 0; i < names.length; ++i) {
                    System.out.printf("CHECK\t%s\t%d\t%d\t%d%n", names[i], count, seed, results[i]);
                }
            }
        }
    }
}
