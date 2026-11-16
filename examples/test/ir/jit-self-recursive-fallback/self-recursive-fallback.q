%modern
# the script of jit-self-recursive-fallback.qtest: a recursive function whose native code is removed while the
# recursion runs, as a recompilation removes it, so that its next recursive call takes the fallback of the native
# self-recursive call; the output must be that of the AST interpreter (qore#6317)

# Copyright (C) 2026 Qore Technologies, s.r.o.

# True when the calls of the test run (False during the warm-up)
our bool testing = False;
our *bool dropped;

# the function has no parameter with a default value and does not read argv, so that its calls can be made with
# the native fast call path, which the self-recursive call helper is part of
int sub rec(int n, int acc, string tag) {
    if (testing && n == 3 && !exists dropped) {
        # removes the function's native code (with JIT and tiered execution; nothing is removed otherwise)
        dropped = dbg_jit_drop_native_code(\rec());
    }
    # the parameters are bound with the arguments of the call
    if (testing) {
        printf("n=%y acc=%y tag=%y\n", n, acc, tag);
    }
    if (!n) {
        return acc;
    }
    return rec(n - 1, acc + n, tag + n);
}

# the function is compiled to native code (with JIT and tiered execution) after its first calls
rec(2, 0, "w");
dbg_wait_bg_compile_idle();
# it is compiled again alone, as a recompilation after deoptimizations compiles it, so that its recursive calls are
# made with the native self-recursive call helper, which has the fallback; a batch compile calls itself directly
bool recompiled = dbg_jit_recompile_native_code(\rec());
testing = True;
# called through the function's installed code, not a copy in the batch of the top-level code
printf("result: %d\n", call_function("rec", 6, 0, "t"));
# the native code is removed only with JIT and tiered execution
printf("recompiled: %y dropped: %y\n", recompiled, dropped);
