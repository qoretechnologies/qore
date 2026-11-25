# Runtime comparison: target numbers

Copyright (c) 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT.

This file holds the final per-kernel AOT numbers of the performance branch of qore#5578 (`feature/performance`, tip
8d01fe1cf8) as the **target** for the work ported from it.  `BASELINE-*.md` holds the numbers of the revision the port
starts from.  The branch's numbers were measured on an Apple M4 Pro (macOS arm64) with Release `-O3 -DNDEBUG` builds,
LLVM 22.1.8 and `qcc -O3 --strip-source` executables, at 4096 items per invocation; they are whole-kernel ns/item
(lower is better).  They are measurements of another revision on another host at another time: compare the change
that a port makes against `BASELINE-*.md` on the same host, not absolute numbers against this table.

The pull request summarizes its final run as: AOT 12.6x geometric-mean speedup over develop b27e53d5b9 (range
2.8x-93x, no kernel slower); IR 1.22x (0.98x-2.9x); AOT at 6.5x of warmed HotSpot's time.  That run's per-kernel
table is not in the branch (its raw result files were removed before pushing), and the branch records no per-kernel
IR, JIT or AST numbers after its IR caveat (before develop 5243c8aef, `--exec-mode=ir` could promote to JIT).  The
target column is therefore the latest per-kernel AOT number that the branch's `RESULTS.md` records for the
same-Program `kernels.qr` layout:

- **C**: the cumulative checkpoint "Optimized checkpoint after receiver-local optimization (2026-10-31)", branch
  f1adb09eb9 against develop 3955cf3ac3 (added to `RESULTS.md` by 29a73b9091), its `local:` (standalone, one Program)
  rows; its develop column is given for the change it measured.
- **M**: for the kernels that checkpoint does not list, the last complete 37-kernel matrix, "Shared private AOT
  callback entry state (2026-10-25)", broad default-layout screen, candidate column (ad48cf8675).

The last column is the 10-31 checkpoint's row for the same kernel imported from a module and called from an IR host
(`micro:`), which is a different layout: it shows later work on kernels whose same-Program number is older (for
example `temporary_hash`, where the branch's finite-key hash scalarization of 10-26, 6c87c983d0, is not in the
same-Program number).

|!Kernel|!PR target AOT ns/item|!Source|!PR-era develop AOT ns/item|!10-31 imported-module row ns/item
|arithmetic|1.56|M|-|-
|direct_call|1.48|C|11.02|-
|method_call|1.57|C|11.07|-
|indirect_call|8.12|M|-|7.72
|polymorphic_call|14.90|C|305.19|15.15
|polymorphic_expression_call|4.76|C|132.75|4.68
|monomorphic_call|3.70|C|16.85|-
|optional_method_call|11.43|M|-|-
|polymorphic_float_call|14.75|C|320.86|14.72
|polymorphic_float_expression_call|4.48|C|141.52|4.53
|monomorphic_float_call|3.66|C|16.82|-
|optional_float_method_call|11.94|M|-|-
|boxed_float_method_call|20.66|M|-|106.74
|dynamic_int_result_call|11.32|M|-|-
|dynamic_int_result_fallback|22.02|M|-|-
|boxed_int_method_call|17.24|M|-|100.74
|boxed_int_method_fallback|31.42|C|258.59|114.24
|boxed_float_method_fallback|34.37|C|292.86|119.27
|boxed_int_method_effect|89.05|C|359.36|170.67
|dynamic_float_result_call|12.91|M|-|-
|indirect_wide|7.71|M|-|-
|indirect_float|8.26|M|-|-
|indirect_int_result|7.78|M|-|-
|indirect_float_result|8.31|M|-|-
|indirect_tagged_float|8.33|M|-|-
|indirect_branch_float|8.57|M|-|-
|indirect_affine|8.15|M|-|-
|indirect_affine_wide|7.54|M|-|-
|indirect_branch_int|7.89|M|-|-
|dispatch_control|1.91|M|-|-
|list_read|3.00|M|-|2.82
|list_build|27.46|M|-|22.15
|list_mutate|116.82|C|235.98|117.89
|temporary_list|1.59|C|145.31|-
|temporary_hash|337.02|M|-|1.59
|closure_call|20.81|M|-|17.80
|closure_capture|25.97|M|-|23.36
