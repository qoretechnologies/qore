# AOT Phase 2B Step 5 — Exception-check Call-site Catalog

**Status:** Phase A complete 2026-04-18.
**Generator:** `/tmp/catalog_step5.py` (checked into PR branch during Step 5 work).
**Source:** scan of `lib/QoreIRToLLVM.cpp` at commit `9fafccd34`.
**Goal:** migrate every xsink-setting helper to throw (C++ EH) instead of setting
xsink and returning, so every `CreateCall + emitExceptionCheck` call site
becomes `emitMaybeInvoke` with a per-invoke LP. Expected outcome:
HttpServer.qm -O3 compile time ≤ 120s (currently 806s post Step 1-2; pathological
~100× slowdown vs -O0 due to 1,008 per-temp cleanup-flag allocas going quadratic
in LLVM SelectionDAG).

See continuation prompt `/tmp/phase2b_step5_helpers_throw_continuation.md` for
the full plan (Phases A–E), footguns, and measurement instructions.

## Attribution caveats

- Helper name for each site is extracted from the nearest preceding
  `getOrInsertFunction(...)` call, bounded by the enclosing
  `case QoreIROpcode::X:` label.
- When multiple `getOrInsertFunction` calls appear in the same case (AOT vs
  JIT branch, or fall-through to a shared helper), all are listed.
- When a `case` dispatches through a fast-path helper
  (`emitAnyCmpFastPath`, `emitAnyBitwiseFastPath`, `emitAnyUnaryFastPath`,
  `emitAnyCompoundAssignFastPath`, `emitAnyArithFastPath`,
  `emitAnyCmpSpaceshipFastPath`), the attribution points at the
  underlying slow-path helper inside that function.
- `aot_invoke_jit_createcall` pattern means AOT branch already uses
  `emitMaybeInvoke`; JIT branch still uses `CreateCall` — the post-call
  `emitExceptionCheck` handles both.

## Generated catalog

Total emitExceptionCheck call sites: 89
Distinct helpers referenced: 69
Distinct opcodes: 84

### Pattern breakdown
  - `createcall_only`: 74
  - `aot_invoke_jit_createcall`: 8
  - `unclear`: 4
  - `mixed_invoke_createcall`: 3

Existing _throwing wrappers (10):
  - qore_rt_call_closure_fast
  - qore_rt_call_direct_aot
  - qore_rt_call_method_direct_aot
  - qore_rt_call_method_fast_aot
  - qore_rt_call_static_method_direct_aot
  - qore_rt_call_static_method_fast_aot
  - qore_rt_call_with_args_aot
  - qore_rt_dot_eval_method_direct_aot
  - qore_rt_dot_eval_pseudo_method_direct_aot
  - qore_rt_new_object_nb_aot

## Migration buckets

### Bucket 1 — DONE (already invoke, throwing twin exists): 2
    - line 6517 (CallStatic) → `qore_rt_call_ref_fast / qore_rt_call_with_args / qore_rt_call_with_args_aot / qore_rt_invoke_expr / qore_rt_invoke_expr_aot` [aot_invoke_jit_createcall]
    - line 8465 (NewObject) → `qore_rt_new_object_nb / qore_rt_new_object_nb_aot` [aot_invoke_jit_createcall]

### Bucket 2 — EASY (has throwing twin, but some path uses CreateCall): 2
    - line 6702 (CallDirect) → `qore_rt_call_direct_aot / qore_rt_call_fast_with_target / qore_rt_call_self_recursive` → twin: `qore_rt_call_direct_aot_throwing` [mixed_invoke_createcall]
    - line 8058 (CallClosureDirect) → `qore_rt_call_closure_fast` → twin: `qore_rt_call_closure_fast_throwing` [mixed_invoke_createcall]

### Bucket 3 — NEEDS NEW _throwing WRAPPER: 78
  - `qore_rt_comparison_op (via emitAnyCmpFastPath)`: 6 sites
      - line 7349 (EqAny) [createcall_only]
      - line 7377 (NeAny) [createcall_only]
      - line 7457 (LtAny) [createcall_only]
      - line 7471 (LeAny) [createcall_only]
      - line 7485 (GtAny) [createcall_only]
      - line 7499 (GeAny) [createcall_only]
  - `qore_rt_binary_op (via emitAnyBitwiseFastPath)`: 5 sites
      - line 7645 (AndAny) [createcall_only]
      - line 7661 (OrAny) [createcall_only]
      - line 7677 (XorAny) [createcall_only]
      - line 7693 (ShlAny) [unclear]
      - line 7709 (ShrAny) [unclear]
  - `qore_rt_binary_op`: 2 sites
      - line 10227 (RangeDate) [createcall_only]
      - line 11368 (ListIndexDynamic) [createcall_only]
  - `qore_rt_exec_statement`: 2 sites
      - line 10920 (Context) [createcall_only]
      - line 10939 (Summarize) [createcall_only]
  - `qore_rt_invoke_expr / qore_rt_invoke_expr_aot`: 2 sites
      - line 10428 (ListAssignAny) [createcall_only]
      - line 10815 (InvokeSimError) [createcall_only]
  - `qore_rt_lvalue_binary / qore_rt_lvalue_binary_aot`: 2 sites
      - line 9056 (UnshiftLValue) [createcall_only]
      - line 9179 (ModAssignLValue) [createcall_only]
  - `qore_rt_unary_op (via emitAnyUnaryFastPath)`: 2 sites
      - line 7723 (UnaryMinusAny) [unclear]
      - line 7735 (UnaryPlusAny) [unclear]
  - `qore_rt_assign_local / qore_rt_store_closure_aot`: 1 sites
      - line 8151 (StoreClosure) [createcall_only]
  - `qore_rt_background_self_call / qore_rt_background_self_call_aot / qore_rt_invoke_expr / qore_rt_invoke_expr_aot`: 1 sites
      - line 10646 (BackgroundInt) [createcall_only]
  - `qore_rt_binary_op (via emitAnyCompoundAssignFastPath)`: 1 sites
      - line 12054 ((unknown)) [createcall_only]
  - `qore_rt_call_method_direct / qore_rt_call_method_fast`: 1 sites
      - line 6787 (CallMethodDirect) [mixed_invoke_createcall]
  - `qore_rt_call_static_method_direct`: 1 sites
      - line 6957 (CallStaticDirect) [aot_invoke_jit_createcall]
  - `qore_rt_cast_by_type_path / qore_rt_cast_with_inner / qore_rt_cast_with_inner_aot`: 1 sites
      - line 10783 (CastAny) [createcall_only]
  - `qore_rt_coerce_value / qore_rt_coerce_value_aot`: 1 sites
      - line 3935 (StoreLocal) [createcall_only]
  - `qore_rt_coerce_value / qore_rt_coerce_value_aot / qore_rt_decref / qore_rt_instantiate_local / qore_rt_instantiate_local_aot`: 1 sites
      - line 4167 ((unknown)) [createcall_only]
  - `qore_rt_coerce_value / qore_rt_coerce_value_aot / qore_rt_decref / qore_rt_instantiate_local / qore_rt_strip_complex_type`: 1 sites
      - line 4258 ((unknown)) [createcall_only]
  - `qore_rt_comparison_op`: 1 sites
      - line 7574 (CmpFloat) [createcall_only]
  - `qore_rt_comparison_op (via emitAnyCmpSpaceshipFastPath)`: 1 sites
      - line 7601 (CmpAny) [createcall_only]
  - `qore_rt_create_call_ref / qore_rt_invoke_expr_aot`: 1 sites
      - line 8584 (CreateCallRef) [createcall_only]
  - `qore_rt_create_closure / qore_rt_invoke_expr_aot`: 1 sites
      - line 8557 (CreateClosure) [createcall_only]
  - `qore_rt_create_method_ref / qore_rt_invoke_expr_aot`: 1 sites
      - line 8611 (CreateMethodRef) [createcall_only]
  - `qore_rt_create_parse_ref / qore_rt_invoke_expr_aot`: 1 sites
      - line 8683 (CreateParseRef) [createcall_only]
  - `qore_rt_decref / qore_rt_iterator_next`: 1 sites
      - line 11155 (IteratorNext) [createcall_only]
  - `qore_rt_dot_eval_pseudo_method_direct / qore_rt_pseudo_empty / qore_rt_pseudo_size / qore_rt_pseudo_type / qore_rt_pseudo_typeCode / qore_rt_pseudo_val`: 1 sites
      - line 7111 (DotEvalMethodDirect) [aot_invoke_jit_createcall]
  - `qore_rt_dot_eval_with_base / qore_rt_dot_eval_with_base_aot`: 1 sites
      - line 10706 (DotEvalObject) [createcall_only]
  - `qore_rt_dot_eval_with_base / qore_rt_dot_eval_with_base_aot / qore_rt_invoke_expr / qore_rt_invoke_expr_aot`: 1 sites
      - line 10728 (DotEvalObject) [createcall_only]
  - `qore_rt_get_int64 / qore_rt_list_index_access`: 1 sites
      - line 8221 (ListIndexAccess) [createcall_only]
  - `qore_rt_get_int64 / qore_rt_list_index_store_cow / qore_rt_list_index_store_cow_aot`: 1 sites
      - line 8374 (ListIndexStore) [createcall_only]
  - `qore_rt_get_regex_case_aot / qore_rt_switch_regex_match`: 1 sites
      - line 10376 (SwitchRegexMatch) [createcall_only]
  - `qore_rt_hash_key_access`: 1 sites
      - line 8177 (HashKeyAccess) [createcall_only]
  - `qore_rt_hash_key_store_cow / qore_rt_hash_key_store_cow_aot`: 1 sites
      - line 8263 (HashKeyStore) [createcall_only]
  - `qore_rt_hash_key_store_dynamic_cow / qore_rt_hash_key_store_dynamic_cow_aot`: 1 sites
      - line 8318 (HashKeyStoreDynamic) [createcall_only]
  - `qore_rt_hash_set_key_value`: 1 sites
      - line 8867 (HashSetKeyValue) [createcall_only]
  - `qore_rt_instanceof / qore_rt_instanceof_by_type_path / qore_rt_invoke_expr_aot`: 1 sites
      - line 10517 (InstanceOfBool) [createcall_only]
  - `qore_rt_invoke_expr / qore_rt_invoke_expr_aot / qore_rt_regex_op_by_pattern / qore_rt_regex_op_with_operand / qore_rt_regex_op_with_operand_aot`: 1 sites
      - line 10336 (RegexExtractList) [createcall_only]
  - `qore_rt_invoke_expr / qore_rt_invoke_expr_aot / qore_rt_unary_op`: 1 sites
      - line 10561 (ElementsInt) [createcall_only]
  - `qore_rt_invoke_expr_aot / qore_rt_load_constant / qore_rt_load_constant_value`: 1 sites
      - line 8531 (LoadConstant) [aot_invoke_jit_createcall]
  - `qore_rt_invoke_expr_aot / qore_rt_load_static_var`: 1 sites
      - line 8495 (LoadStaticVar) [aot_invoke_jit_createcall]
  - `qore_rt_invoke_expr_aot / qore_rt_new_complex_hash`: 1 sites
      - line 8735 (NewComplexHash) [createcall_only]
  - `qore_rt_invoke_expr_aot / qore_rt_new_complex_list`: 1 sites
      - line 8761 (NewComplexList) [createcall_only]
  - `qore_rt_invoke_expr_aot / qore_rt_new_hash_decl`: 1 sites
      - line 8709 (NewHashDecl) [createcall_only]
  - `qore_rt_invoke_expr_aot / qore_rt_ref_foreach_init`: 1 sites
      - line 11210 (RefForeachInit) [createcall_only]
  - `qore_rt_iterator_create / qore_rt_iterator_create_aot`: 1 sites
      - line 11108 (IteratorCreate) [createcall_only]
  - `qore_rt_iterator_create_reverse`: 1 sites
      - line 8903 (IteratorCreateReverse) [createcall_only]
  - `qore_rt_list_push`: 1 sites
      - line 7884 (ListPush) [createcall_only]
  - `qore_rt_load_closure_aot / qore_rt_load_local`: 1 sites
      - line 8110 (LoadClosure) [aot_invoke_jit_createcall]
  - `qore_rt_lv_path_ternary / qore_rt_lv_path_ternary_aot`: 1 sites
      - line 11606 (LValuePathTernary) [createcall_only]
  - `qore_rt_lvalue_load / qore_rt_lvalue_load_aot`: 1 sites
      - line 8929 (LoadLValue) [createcall_only]
  - `qore_rt_lvalue_ternary / qore_rt_lvalue_ternary_aot`: 1 sites
      - line 9230 (SpliceLValue) [createcall_only]
  - `qore_rt_lvalue_unary / qore_rt_lvalue_unary_aot`: 1 sites
      - line 9015 (ShiftLValue) [createcall_only]
  - `qore_rt_make_hash`: 1 sites
      - line 9311 (MakeHash) [createcall_only]
  - `qore_rt_make_hash_const_keys`: 1 sites
      - line 9362 (MakeHashConstKeys) [createcall_only]
  - `qore_rt_make_list`: 1 sites
      - line 9271 (MakeList) [createcall_only]
  - `qore_rt_new_hash_decl_from_hash / qore_rt_new_hash_decl_from_hash_by_path`: 1 sites
      - line 8822 (NewHashDeclFromHash) [createcall_only]
  - `qore_rt_ref_foreach_cleanup`: 1 sites
      - line 11289 (RefForeachCleanup) [createcall_only]
  - `qore_rt_ref_foreach_finalize`: 1 sites
      - line 11279 (RefForeachFinalize) [createcall_only]
  - `qore_rt_ref_foreach_get_entry`: 1 sites
      - line 11241 (RefForeachGetEntry) [createcall_only]
  - `qore_rt_ref_foreach_record`: 1 sites
      - line 11267 (RefForeachRecord) [createcall_only]
  - `qore_rt_sprintf`: 1 sites
      - line 3642 (Sprintf) [createcall_only]
  - `qore_rt_string_eq_typed`: 1 sites
      - line 7363 (EqString) [createcall_only]
  - `qore_rt_string_ne_typed`: 1 sites
      - line 7391 (NeString) [createcall_only]
  - `qore_rt_switch_case_match / qore_rt_switch_case_match_value / qore_rt_switch_case_match_value_aot`: 1 sites
      - line 11345 (SwitchCaseMatch) [createcall_only]
  - `qore_rt_ternary_op`: 1 sites
      - line 10251 (RangeSliceFloat) [createcall_only]
  - `qore_rt_vrn_construct / qore_rt_vrn_construct_aot`: 1 sites
      - line 8788 (VrnConstruct) [createcall_only]

### Bucket NONE — could not identify preceding helper (inspect manually): 7
    - line 7767 (LoadThreadLocal) [createcall_only]
    - line 8086 (StoreThreadLocal) [aot_invoke_jit_createcall]
    - line 8403 (LoadSelfMember) [createcall_only]
    - line 8974 (StoreLValue) [createcall_only]
    - line 9137 (ShrAssignLValue) [createcall_only]
    - line 10665 (DotEvalObject) [createcall_only]
    - line 10673 (DotEvalObject) [createcall_only]

## Phase B plan — unique BASE helpers needing _throwing wrappers

Note: AOT Step 5 targets AOT-mode compile speed. Wrappers named
`qore_rt_X_aot_throwing` are the critical ones; JIT-mode `qore_rt_X_throwing`
are nice-to-have for consistency but won't affect the HttpServer.qm compile
cliff (that code runs in AOT mode).

### Local access
  - `qore_rt_assign_local` (1 sites): 8151(StoreClosure)
  - `qore_rt_instantiate_local` (2 sites): 4167((unknown)), 4258((unknown))
  - `qore_rt_instantiate_local_aot` (1 sites): 4167((unknown))
  - `qore_rt_load_closure_aot` (1 sites): 8110(LoadClosure)
  - `qore_rt_load_constant` (1 sites): 8531(LoadConstant)
  - `qore_rt_load_constant_value` (1 sites): 8531(LoadConstant)
  - `qore_rt_load_local` (1 sites): 8110(LoadClosure)
  - `qore_rt_load_static_var` (1 sites): 8495(LoadStaticVar)
  - `qore_rt_store_closure_aot` (1 sites): 8151(StoreClosure)

### Expression / value
  - `qore_rt_background_self_call` (1 sites): 10646(BackgroundInt)
  - `qore_rt_background_self_call_aot` (1 sites): 10646(BackgroundInt)
  - `qore_rt_call_method_direct` (1 sites): 6787(CallMethodDirect)
  - `qore_rt_call_method_fast` (1 sites): 6787(CallMethodDirect)
  - `qore_rt_call_static_method_direct` (1 sites): 6957(CallStaticDirect)
  - `qore_rt_cast_by_type_path` (1 sites): 10783(CastAny)
  - `qore_rt_cast_with_inner` (1 sites): 10783(CastAny)
  - `qore_rt_cast_with_inner_aot` (1 sites): 10783(CastAny)
  - `qore_rt_coerce_value` (3 sites): 3935(StoreLocal), 4167((unknown)), 4258((unknown))
  - `qore_rt_coerce_value_aot` (3 sites): 3935(StoreLocal), 4167((unknown)), 4258((unknown))
  - `qore_rt_create_call_ref` (1 sites): 8584(CreateCallRef)
  - `qore_rt_create_closure` (1 sites): 8557(CreateClosure)
  - `qore_rt_create_method_ref` (1 sites): 8611(CreateMethodRef)
  - `qore_rt_create_parse_ref` (1 sites): 8683(CreateParseRef)
  - `qore_rt_dot_eval_pseudo_method_direct` (1 sites): 7111(DotEvalMethodDirect)
  - `qore_rt_dot_eval_with_base` (2 sites): 10706(DotEvalObject), 10728(DotEvalObject)
  - `qore_rt_dot_eval_with_base_aot` (2 sites): 10706(DotEvalObject), 10728(DotEvalObject)
  - `qore_rt_get_regex_case_aot` (1 sites): 10376(SwitchRegexMatch)
  - `qore_rt_hash_key_access` (1 sites): 8177(HashKeyAccess)
  - `qore_rt_hash_key_store_cow` (1 sites): 8263(HashKeyStore)
  - `qore_rt_hash_key_store_cow_aot` (1 sites): 8263(HashKeyStore)
  - `qore_rt_hash_key_store_dynamic_cow` (1 sites): 8318(HashKeyStoreDynamic)
  - `qore_rt_hash_key_store_dynamic_cow_aot` (1 sites): 8318(HashKeyStoreDynamic)
  - `qore_rt_hash_set_key_value` (1 sites): 8867(HashSetKeyValue)
  - `qore_rt_instanceof` (1 sites): 10517(InstanceOfBool)
  - `qore_rt_instanceof_by_type_path` (1 sites): 10517(InstanceOfBool)
  - `qore_rt_invoke_expr` (6 sites): 10336(RegexExtractList), 10428(ListAssignAny), 10561(ElementsInt), 10646(BackgroundInt), 10728(DotEvalObject), 10815(InvokeSimError)
  - `qore_rt_invoke_expr_aot` (17 sites): 8495(LoadStaticVar), 8531(LoadConstant), 8557(CreateClosure), 8584(CreateCallRef), 8611(CreateMethodRef), 8683(CreateParseRef) +11 more
  - `qore_rt_list_index_access` (1 sites): 8221(ListIndexAccess)
  - `qore_rt_list_index_store_cow` (1 sites): 8374(ListIndexStore)
  - `qore_rt_list_index_store_cow_aot` (1 sites): 8374(ListIndexStore)
  - `qore_rt_list_push` (1 sites): 7884(ListPush)
  - `qore_rt_make_hash` (1 sites): 9311(MakeHash)
  - `qore_rt_make_hash_const_keys` (1 sites): 9362(MakeHashConstKeys)
  - `qore_rt_make_list` (1 sites): 9271(MakeList)
  - `qore_rt_new_complex_hash` (1 sites): 8735(NewComplexHash)
  - `qore_rt_new_complex_list` (1 sites): 8761(NewComplexList)
  - `qore_rt_new_hash_decl` (1 sites): 8709(NewHashDecl)
  - `qore_rt_new_hash_decl_from_hash` (1 sites): 8822(NewHashDeclFromHash)
  - `qore_rt_new_hash_decl_from_hash_by_path` (1 sites): 8822(NewHashDeclFromHash)
  - `qore_rt_regex_op_by_pattern` (1 sites): 10336(RegexExtractList)
  - `qore_rt_regex_op_with_operand` (1 sites): 10336(RegexExtractList)
  - `qore_rt_regex_op_with_operand_aot` (1 sites): 10336(RegexExtractList)
  - `qore_rt_sprintf` (1 sites): 3642(Sprintf)
  - `qore_rt_strip_complex_type` (1 sites): 4258((unknown))
  - `qore_rt_switch_case_match` (1 sites): 11345(SwitchCaseMatch)
  - `qore_rt_switch_regex_match` (1 sites): 10376(SwitchRegexMatch)
  - `qore_rt_vrn_construct` (1 sites): 8788(VrnConstruct)

### Lvalue ops
  - `qore_rt_lv_path_ternary` (1 sites): 11606(LValuePathTernary)
  - `qore_rt_lv_path_ternary_aot` (1 sites): 11606(LValuePathTernary)
  - `qore_rt_lvalue_binary` (2 sites): 9056(UnshiftLValue), 9179(ModAssignLValue)
  - `qore_rt_lvalue_binary_aot` (2 sites): 9056(UnshiftLValue), 9179(ModAssignLValue)
  - `qore_rt_lvalue_load` (1 sites): 8929(LoadLValue)
  - `qore_rt_lvalue_load_aot` (1 sites): 8929(LoadLValue)
  - `qore_rt_lvalue_ternary` (1 sites): 9230(SpliceLValue)
  - `qore_rt_lvalue_ternary_aot` (1 sites): 9230(SpliceLValue)
  - `qore_rt_lvalue_unary` (1 sites): 9015(ShiftLValue)
  - `qore_rt_lvalue_unary_aot` (1 sites): 9015(ShiftLValue)

### Iterator
  - `qore_rt_iterator_create` (1 sites): 11108(IteratorCreate)
  - `qore_rt_iterator_create_aot` (1 sites): 11108(IteratorCreate)
  - `qore_rt_iterator_create_reverse` (1 sites): 8903(IteratorCreateReverse)
  - `qore_rt_iterator_next` (1 sites): 11155(IteratorNext)
  - `qore_rt_ref_foreach_cleanup` (1 sites): 11289(RefForeachCleanup)
  - `qore_rt_ref_foreach_finalize` (1 sites): 11279(RefForeachFinalize)
  - `qore_rt_ref_foreach_get_entry` (1 sites): 11241(RefForeachGetEntry)
  - `qore_rt_ref_foreach_init` (1 sites): 11210(RefForeachInit)
  - `qore_rt_ref_foreach_record` (1 sites): 11267(RefForeachRecord)

### Fast-path slow-route helpers
  - `qore_rt_binary_op` (8 sites): 7645(AndAny), 7661(OrAny), 7677(XorAny), 7693(ShlAny), 7709(ShrAny), 10227(RangeDate) +2 more
  - `qore_rt_comparison_op` (8 sites): 7349(EqAny), 7377(NeAny), 7457(LtAny), 7471(LeAny), 7485(GtAny), 7499(GeAny) +2 more
  - `qore_rt_unary_op` (3 sites): 7723(UnaryMinusAny), 7735(UnaryPlusAny), 10561(ElementsInt)

### Other
  - `qore_rt_decref` (3 sites): 4167((unknown)), 4258((unknown)), 11155(IteratorNext)
  - `qore_rt_exec_statement` (2 sites): 10920(Context), 10939(Summarize)
  - `qore_rt_get_int64` (2 sites): 8221(ListIndexAccess), 8374(ListIndexStore)

### Uncategorized (needs review)
  - `qore_rt_pseudo_empty` (1 sites): 7111(DotEvalMethodDirect)
  - `qore_rt_pseudo_size` (1 sites): 7111(DotEvalMethodDirect)
  - `qore_rt_pseudo_type` (1 sites): 7111(DotEvalMethodDirect)
  - `qore_rt_pseudo_typeCode` (1 sites): 7111(DotEvalMethodDirect)
  - `qore_rt_pseudo_val` (1 sites): 7111(DotEvalMethodDirect)
  - `qore_rt_string_eq_typed` (1 sites): 7363(EqString)
  - `qore_rt_string_ne_typed` (1 sites): 7391(NeString)
  - `qore_rt_switch_case_match_value` (1 sites): 11345(SwitchCaseMatch)
  - `qore_rt_switch_case_match_value_aot` (1 sites): 11345(SwitchCaseMatch)
  - `qore_rt_ternary_op` (1 sites): 10251(RangeSliceFloat)
  - `qore_rt_vrn_construct_aot` (1 sites): 8788(VrnConstruct)


## Sites grouped by helper

- `(none)`: 7 sites [no helper — likely post-call cleanup or non-helper site]
    - line 7767 (LoadThreadLocal) [createcall_only]
    - line 8086 (StoreThreadLocal) [aot_invoke_jit_createcall]
    - line 8403 (LoadSelfMember) [createcall_only]
    - line 8974 (StoreLValue) [createcall_only]
    - line 9137 (ShrAssignLValue) [createcall_only]
    - line 10665 (DotEvalObject) [createcall_only]
    - line 10673 (DotEvalObject) [createcall_only]
- `qore_rt_comparison_op (via emitAnyCmpFastPath)`: 6 sites [no throwing twin]
    - line 7349 (EqAny) [createcall_only]
    - line 7377 (NeAny) [createcall_only]
    - line 7457 (LtAny) [createcall_only]
    - line 7471 (LeAny) [createcall_only]
    - line 7485 (GtAny) [createcall_only]
    - line 7499 (GeAny) [createcall_only]
- `qore_rt_binary_op (via emitAnyBitwiseFastPath)`: 5 sites [no throwing twin]
    - line 7645 (AndAny) [createcall_only]
    - line 7661 (OrAny) [createcall_only]
    - line 7677 (XorAny) [createcall_only]
    - line 7693 (ShlAny) [unclear]
    - line 7709 (ShrAny) [unclear]
- `qore_rt_binary_op`: 2 sites [no throwing twin]
    - line 10227 (RangeDate) [createcall_only]
    - line 11368 (ListIndexDynamic) [createcall_only]
- `qore_rt_exec_statement`: 2 sites [no throwing twin]
    - line 10920 (Context) [createcall_only]
    - line 10939 (Summarize) [createcall_only]
- `qore_rt_invoke_expr / qore_rt_invoke_expr_aot`: 2 sites [no throwing twin]
    - line 10428 (ListAssignAny) [createcall_only]
    - line 10815 (InvokeSimError) [createcall_only]
- `qore_rt_lvalue_binary / qore_rt_lvalue_binary_aot`: 2 sites [no throwing twin]
    - line 9056 (UnshiftLValue) [createcall_only]
    - line 9179 (ModAssignLValue) [createcall_only]
- `qore_rt_unary_op (via emitAnyUnaryFastPath)`: 2 sites [no throwing twin]
    - line 7723 (UnaryMinusAny) [unclear]
    - line 7735 (UnaryPlusAny) [unclear]
- `qore_rt_assign_local / qore_rt_store_closure_aot`: 1 sites [no throwing twin]
    - line 8151 (StoreClosure) [createcall_only]
- `qore_rt_background_self_call / qore_rt_background_self_call_aot / qore_rt_invoke_expr / qore_rt_invoke_expr_aot`: 1 sites [no throwing twin]
    - line 10646 (BackgroundInt) [createcall_only]
- `qore_rt_binary_op (via emitAnyCompoundAssignFastPath)`: 1 sites [no throwing twin]
    - line 12054 ((unknown)) [createcall_only]
- `qore_rt_call_closure_fast`: 1 sites [THROWING TWIN: qore_rt_call_closure_fast_throwing]
    - line 8058 (CallClosureDirect) [mixed_invoke_createcall]
- `qore_rt_call_direct_aot / qore_rt_call_fast_with_target / qore_rt_call_self_recursive`: 1 sites [THROWING TWIN: qore_rt_call_direct_aot_throwing]
    - line 6702 (CallDirect) [mixed_invoke_createcall]
- `qore_rt_call_method_direct / qore_rt_call_method_fast`: 1 sites [no throwing twin]
    - line 6787 (CallMethodDirect) [mixed_invoke_createcall]
- `qore_rt_call_ref_fast / qore_rt_call_with_args / qore_rt_call_with_args_aot / qore_rt_invoke_expr / qore_rt_invoke_expr_aot`: 1 sites [THROWING TWIN: qore_rt_call_with_args_aot_throwing]
    - line 6517 (CallStatic) [aot_invoke_jit_createcall]
- `qore_rt_call_static_method_direct`: 1 sites [no throwing twin]
    - line 6957 (CallStaticDirect) [aot_invoke_jit_createcall]
- `qore_rt_cast_by_type_path / qore_rt_cast_with_inner / qore_rt_cast_with_inner_aot`: 1 sites [no throwing twin]
    - line 10783 (CastAny) [createcall_only]
- `qore_rt_coerce_value / qore_rt_coerce_value_aot`: 1 sites [no throwing twin]
    - line 3935 (StoreLocal) [createcall_only]
- `qore_rt_coerce_value / qore_rt_coerce_value_aot / qore_rt_decref / qore_rt_instantiate_local / qore_rt_instantiate_local_aot`: 1 sites [no throwing twin]
    - line 4167 ((unknown)) [createcall_only]
- `qore_rt_coerce_value / qore_rt_coerce_value_aot / qore_rt_decref / qore_rt_instantiate_local / qore_rt_strip_complex_type`: 1 sites [no throwing twin]
    - line 4258 ((unknown)) [createcall_only]
- `qore_rt_comparison_op`: 1 sites [no throwing twin]
    - line 7574 (CmpFloat) [createcall_only]
- `qore_rt_comparison_op (via emitAnyCmpSpaceshipFastPath)`: 1 sites [no throwing twin]
    - line 7601 (CmpAny) [createcall_only]
- `qore_rt_create_call_ref / qore_rt_invoke_expr_aot`: 1 sites [no throwing twin]
    - line 8584 (CreateCallRef) [createcall_only]
- `qore_rt_create_closure / qore_rt_invoke_expr_aot`: 1 sites [no throwing twin]
    - line 8557 (CreateClosure) [createcall_only]
- `qore_rt_create_method_ref / qore_rt_invoke_expr_aot`: 1 sites [no throwing twin]
    - line 8611 (CreateMethodRef) [createcall_only]
- `qore_rt_create_parse_ref / qore_rt_invoke_expr_aot`: 1 sites [no throwing twin]
    - line 8683 (CreateParseRef) [createcall_only]
- `qore_rt_decref / qore_rt_iterator_next`: 1 sites [no throwing twin]
    - line 11155 (IteratorNext) [createcall_only]
- `qore_rt_dot_eval_pseudo_method_direct / qore_rt_pseudo_empty / qore_rt_pseudo_size / qore_rt_pseudo_type / qore_rt_pseudo_typeCode / qore_rt_pseudo_val`: 1 sites [no throwing twin]
    - line 7111 (DotEvalMethodDirect) [aot_invoke_jit_createcall]
- `qore_rt_dot_eval_with_base / qore_rt_dot_eval_with_base_aot`: 1 sites [no throwing twin]
    - line 10706 (DotEvalObject) [createcall_only]
- `qore_rt_dot_eval_with_base / qore_rt_dot_eval_with_base_aot / qore_rt_invoke_expr / qore_rt_invoke_expr_aot`: 1 sites [no throwing twin]
    - line 10728 (DotEvalObject) [createcall_only]
- `qore_rt_get_int64 / qore_rt_list_index_access`: 1 sites [no throwing twin]
    - line 8221 (ListIndexAccess) [createcall_only]
- `qore_rt_get_int64 / qore_rt_list_index_store_cow / qore_rt_list_index_store_cow_aot`: 1 sites [no throwing twin]
    - line 8374 (ListIndexStore) [createcall_only]
- `qore_rt_get_regex_case_aot / qore_rt_switch_regex_match`: 1 sites [no throwing twin]
    - line 10376 (SwitchRegexMatch) [createcall_only]
- `qore_rt_hash_key_access`: 1 sites [no throwing twin]
    - line 8177 (HashKeyAccess) [createcall_only]
- `qore_rt_hash_key_store_cow / qore_rt_hash_key_store_cow_aot`: 1 sites [no throwing twin]
    - line 8263 (HashKeyStore) [createcall_only]
- `qore_rt_hash_key_store_dynamic_cow / qore_rt_hash_key_store_dynamic_cow_aot`: 1 sites [no throwing twin]
    - line 8318 (HashKeyStoreDynamic) [createcall_only]
- `qore_rt_hash_set_key_value`: 1 sites [no throwing twin]
    - line 8867 (HashSetKeyValue) [createcall_only]
- `qore_rt_instanceof / qore_rt_instanceof_by_type_path / qore_rt_invoke_expr_aot`: 1 sites [no throwing twin]
    - line 10517 (InstanceOfBool) [createcall_only]
- `qore_rt_invoke_expr / qore_rt_invoke_expr_aot / qore_rt_regex_op_by_pattern / qore_rt_regex_op_with_operand / qore_rt_regex_op_with_operand_aot`: 1 sites [no throwing twin]
    - line 10336 (RegexExtractList) [createcall_only]
- `qore_rt_invoke_expr / qore_rt_invoke_expr_aot / qore_rt_unary_op`: 1 sites [no throwing twin]
    - line 10561 (ElementsInt) [createcall_only]
- `qore_rt_invoke_expr_aot / qore_rt_load_constant / qore_rt_load_constant_value`: 1 sites [no throwing twin]
    - line 8531 (LoadConstant) [aot_invoke_jit_createcall]
- `qore_rt_invoke_expr_aot / qore_rt_load_static_var`: 1 sites [no throwing twin]
    - line 8495 (LoadStaticVar) [aot_invoke_jit_createcall]
- `qore_rt_invoke_expr_aot / qore_rt_new_complex_hash`: 1 sites [no throwing twin]
    - line 8735 (NewComplexHash) [createcall_only]
- `qore_rt_invoke_expr_aot / qore_rt_new_complex_list`: 1 sites [no throwing twin]
    - line 8761 (NewComplexList) [createcall_only]
- `qore_rt_invoke_expr_aot / qore_rt_new_hash_decl`: 1 sites [no throwing twin]
    - line 8709 (NewHashDecl) [createcall_only]
- `qore_rt_invoke_expr_aot / qore_rt_ref_foreach_init`: 1 sites [no throwing twin]
    - line 11210 (RefForeachInit) [createcall_only]
- `qore_rt_iterator_create / qore_rt_iterator_create_aot`: 1 sites [no throwing twin]
    - line 11108 (IteratorCreate) [createcall_only]
- `qore_rt_iterator_create_reverse`: 1 sites [no throwing twin]
    - line 8903 (IteratorCreateReverse) [createcall_only]
- `qore_rt_list_push`: 1 sites [no throwing twin]
    - line 7884 (ListPush) [createcall_only]
- `qore_rt_load_closure_aot / qore_rt_load_local`: 1 sites [no throwing twin]
    - line 8110 (LoadClosure) [aot_invoke_jit_createcall]
- `qore_rt_lv_path_ternary / qore_rt_lv_path_ternary_aot`: 1 sites [no throwing twin]
    - line 11606 (LValuePathTernary) [createcall_only]
- `qore_rt_lvalue_load / qore_rt_lvalue_load_aot`: 1 sites [no throwing twin]
    - line 8929 (LoadLValue) [createcall_only]
- `qore_rt_lvalue_ternary / qore_rt_lvalue_ternary_aot`: 1 sites [no throwing twin]
    - line 9230 (SpliceLValue) [createcall_only]
- `qore_rt_lvalue_unary / qore_rt_lvalue_unary_aot`: 1 sites [no throwing twin]
    - line 9015 (ShiftLValue) [createcall_only]
- `qore_rt_make_hash`: 1 sites [no throwing twin]
    - line 9311 (MakeHash) [createcall_only]
- `qore_rt_make_hash_const_keys`: 1 sites [no throwing twin]
    - line 9362 (MakeHashConstKeys) [createcall_only]
- `qore_rt_make_list`: 1 sites [no throwing twin]
    - line 9271 (MakeList) [createcall_only]
- `qore_rt_new_hash_decl_from_hash / qore_rt_new_hash_decl_from_hash_by_path`: 1 sites [no throwing twin]
    - line 8822 (NewHashDeclFromHash) [createcall_only]
- `qore_rt_new_object_nb / qore_rt_new_object_nb_aot`: 1 sites [THROWING TWIN: qore_rt_new_object_nb_aot_throwing]
    - line 8465 (NewObject) [aot_invoke_jit_createcall]
- `qore_rt_ref_foreach_cleanup`: 1 sites [no throwing twin]
    - line 11289 (RefForeachCleanup) [createcall_only]
- `qore_rt_ref_foreach_finalize`: 1 sites [no throwing twin]
    - line 11279 (RefForeachFinalize) [createcall_only]
- `qore_rt_ref_foreach_get_entry`: 1 sites [no throwing twin]
    - line 11241 (RefForeachGetEntry) [createcall_only]
- `qore_rt_ref_foreach_record`: 1 sites [no throwing twin]
    - line 11267 (RefForeachRecord) [createcall_only]
- `qore_rt_sprintf`: 1 sites [no throwing twin]
    - line 3642 (Sprintf) [createcall_only]
- `qore_rt_string_eq_typed`: 1 sites [no throwing twin]
    - line 7363 (EqString) [createcall_only]
- `qore_rt_string_ne_typed`: 1 sites [no throwing twin]
    - line 7391 (NeString) [createcall_only]
- `qore_rt_switch_case_match / qore_rt_switch_case_match_value / qore_rt_switch_case_match_value_aot`: 1 sites [no throwing twin]
    - line 11345 (SwitchCaseMatch) [createcall_only]
- `qore_rt_ternary_op`: 1 sites [no throwing twin]
    - line 10251 (RangeSliceFloat) [createcall_only]
- `qore_rt_vrn_construct / qore_rt_vrn_construct_aot`: 1 sites [no throwing twin]
    - line 8788 (VrnConstruct) [createcall_only]

## Sites grouped by opcode

- `DotEvalObject`: 4 sites
    - line 10665 → `(none)` [createcall_only]
    - line 10673 → `(none)` [createcall_only]
    - line 10706 → `qore_rt_dot_eval_with_base / qore_rt_dot_eval_with_base_aot` [createcall_only]
    - line 10728 → `qore_rt_dot_eval_with_base / qore_rt_dot_eval_with_base_aot / qore_rt_invoke_expr / qore_rt_invoke_expr_aot` [createcall_only]
- `(unknown)`: 3 sites
    - line 4167 → `qore_rt_coerce_value / qore_rt_coerce_value_aot / qore_rt_decref / qore_rt_instantiate_local / qore_rt_instantiate_local_aot` [createcall_only]
    - line 4258 → `qore_rt_coerce_value / qore_rt_coerce_value_aot / qore_rt_decref / qore_rt_instantiate_local / qore_rt_strip_complex_type` [createcall_only]
    - line 12054 → `qore_rt_binary_op (via emitAnyCompoundAssignFastPath)` [createcall_only]
- `AndAny`: 1 sites
    - line 7645 → `qore_rt_binary_op (via emitAnyBitwiseFastPath)` [createcall_only]
- `BackgroundInt`: 1 sites
    - line 10646 → `qore_rt_background_self_call / qore_rt_background_self_call_aot / qore_rt_invoke_expr / qore_rt_invoke_expr_aot` [createcall_only]
- `CallClosureDirect`: 1 sites
    - line 8058 → `qore_rt_call_closure_fast` ✓twin [mixed_invoke_createcall]
- `CallDirect`: 1 sites
    - line 6702 → `qore_rt_call_direct_aot / qore_rt_call_fast_with_target / qore_rt_call_self_recursive` ✓twin [mixed_invoke_createcall]
- `CallMethodDirect`: 1 sites
    - line 6787 → `qore_rt_call_method_direct / qore_rt_call_method_fast` [mixed_invoke_createcall]
- `CallStatic`: 1 sites
    - line 6517 → `qore_rt_call_ref_fast / qore_rt_call_with_args / qore_rt_call_with_args_aot / qore_rt_invoke_expr / qore_rt_invoke_expr_aot` ✓twin [aot_invoke_jit_createcall]
- `CallStaticDirect`: 1 sites
    - line 6957 → `qore_rt_call_static_method_direct` [aot_invoke_jit_createcall]
- `CastAny`: 1 sites
    - line 10783 → `qore_rt_cast_by_type_path / qore_rt_cast_with_inner / qore_rt_cast_with_inner_aot` [createcall_only]
- `CmpAny`: 1 sites
    - line 7601 → `qore_rt_comparison_op (via emitAnyCmpSpaceshipFastPath)` [createcall_only]
- `CmpFloat`: 1 sites
    - line 7574 → `qore_rt_comparison_op` [createcall_only]
- `Context`: 1 sites
    - line 10920 → `qore_rt_exec_statement` [createcall_only]
- `CreateCallRef`: 1 sites
    - line 8584 → `qore_rt_create_call_ref / qore_rt_invoke_expr_aot` [createcall_only]
- `CreateClosure`: 1 sites
    - line 8557 → `qore_rt_create_closure / qore_rt_invoke_expr_aot` [createcall_only]
- `CreateMethodRef`: 1 sites
    - line 8611 → `qore_rt_create_method_ref / qore_rt_invoke_expr_aot` [createcall_only]
- `CreateParseRef`: 1 sites
    - line 8683 → `qore_rt_create_parse_ref / qore_rt_invoke_expr_aot` [createcall_only]
- `DotEvalMethodDirect`: 1 sites
    - line 7111 → `qore_rt_dot_eval_pseudo_method_direct / qore_rt_pseudo_empty / qore_rt_pseudo_size / qore_rt_pseudo_type / qore_rt_pseudo_typeCode / qore_rt_pseudo_val` [aot_invoke_jit_createcall]
- `ElementsInt`: 1 sites
    - line 10561 → `qore_rt_invoke_expr / qore_rt_invoke_expr_aot / qore_rt_unary_op` [createcall_only]
- `EqAny`: 1 sites
    - line 7349 → `qore_rt_comparison_op (via emitAnyCmpFastPath)` [createcall_only]
- `EqString`: 1 sites
    - line 7363 → `qore_rt_string_eq_typed` [createcall_only]
- `GeAny`: 1 sites
    - line 7499 → `qore_rt_comparison_op (via emitAnyCmpFastPath)` [createcall_only]
- `GtAny`: 1 sites
    - line 7485 → `qore_rt_comparison_op (via emitAnyCmpFastPath)` [createcall_only]
- `HashKeyAccess`: 1 sites
    - line 8177 → `qore_rt_hash_key_access` [createcall_only]
- `HashKeyStore`: 1 sites
    - line 8263 → `qore_rt_hash_key_store_cow / qore_rt_hash_key_store_cow_aot` [createcall_only]
- `HashKeyStoreDynamic`: 1 sites
    - line 8318 → `qore_rt_hash_key_store_dynamic_cow / qore_rt_hash_key_store_dynamic_cow_aot` [createcall_only]
- `HashSetKeyValue`: 1 sites
    - line 8867 → `qore_rt_hash_set_key_value` [createcall_only]
- `InstanceOfBool`: 1 sites
    - line 10517 → `qore_rt_instanceof / qore_rt_instanceof_by_type_path / qore_rt_invoke_expr_aot` [createcall_only]
- `InvokeSimError`: 1 sites
    - line 10815 → `qore_rt_invoke_expr / qore_rt_invoke_expr_aot` [createcall_only]
- `IteratorCreate`: 1 sites
    - line 11108 → `qore_rt_iterator_create / qore_rt_iterator_create_aot` [createcall_only]
- `IteratorCreateReverse`: 1 sites
    - line 8903 → `qore_rt_iterator_create_reverse` [createcall_only]
- `IteratorNext`: 1 sites
    - line 11155 → `qore_rt_decref / qore_rt_iterator_next` [createcall_only]
- `LValuePathTernary`: 1 sites
    - line 11606 → `qore_rt_lv_path_ternary / qore_rt_lv_path_ternary_aot` [createcall_only]
- `LeAny`: 1 sites
    - line 7471 → `qore_rt_comparison_op (via emitAnyCmpFastPath)` [createcall_only]
- `ListAssignAny`: 1 sites
    - line 10428 → `qore_rt_invoke_expr / qore_rt_invoke_expr_aot` [createcall_only]
- `ListIndexAccess`: 1 sites
    - line 8221 → `qore_rt_get_int64 / qore_rt_list_index_access` [createcall_only]
- `ListIndexDynamic`: 1 sites
    - line 11368 → `qore_rt_binary_op` [createcall_only]
- `ListIndexStore`: 1 sites
    - line 8374 → `qore_rt_get_int64 / qore_rt_list_index_store_cow / qore_rt_list_index_store_cow_aot` [createcall_only]
- `ListPush`: 1 sites
    - line 7884 → `qore_rt_list_push` [createcall_only]
- `LoadClosure`: 1 sites
    - line 8110 → `qore_rt_load_closure_aot / qore_rt_load_local` [aot_invoke_jit_createcall]
- `LoadConstant`: 1 sites
    - line 8531 → `qore_rt_invoke_expr_aot / qore_rt_load_constant / qore_rt_load_constant_value` [aot_invoke_jit_createcall]
- `LoadLValue`: 1 sites
    - line 8929 → `qore_rt_lvalue_load / qore_rt_lvalue_load_aot` [createcall_only]
- `LoadSelfMember`: 1 sites
    - line 8403 → `(none)` [createcall_only]
- `LoadStaticVar`: 1 sites
    - line 8495 → `qore_rt_invoke_expr_aot / qore_rt_load_static_var` [aot_invoke_jit_createcall]
- `LoadThreadLocal`: 1 sites
    - line 7767 → `(none)` [createcall_only]
- `LtAny`: 1 sites
    - line 7457 → `qore_rt_comparison_op (via emitAnyCmpFastPath)` [createcall_only]
- `MakeHash`: 1 sites
    - line 9311 → `qore_rt_make_hash` [createcall_only]
- `MakeHashConstKeys`: 1 sites
    - line 9362 → `qore_rt_make_hash_const_keys` [createcall_only]
- `MakeList`: 1 sites
    - line 9271 → `qore_rt_make_list` [createcall_only]
- `ModAssignLValue`: 1 sites
    - line 9179 → `qore_rt_lvalue_binary / qore_rt_lvalue_binary_aot` [createcall_only]
- `NeAny`: 1 sites
    - line 7377 → `qore_rt_comparison_op (via emitAnyCmpFastPath)` [createcall_only]
- `NeString`: 1 sites
    - line 7391 → `qore_rt_string_ne_typed` [createcall_only]
- `NewComplexHash`: 1 sites
    - line 8735 → `qore_rt_invoke_expr_aot / qore_rt_new_complex_hash` [createcall_only]
- `NewComplexList`: 1 sites
    - line 8761 → `qore_rt_invoke_expr_aot / qore_rt_new_complex_list` [createcall_only]
- `NewHashDecl`: 1 sites
    - line 8709 → `qore_rt_invoke_expr_aot / qore_rt_new_hash_decl` [createcall_only]
- `NewHashDeclFromHash`: 1 sites
    - line 8822 → `qore_rt_new_hash_decl_from_hash / qore_rt_new_hash_decl_from_hash_by_path` [createcall_only]
- `NewObject`: 1 sites
    - line 8465 → `qore_rt_new_object_nb / qore_rt_new_object_nb_aot` ✓twin [aot_invoke_jit_createcall]
- `OrAny`: 1 sites
    - line 7661 → `qore_rt_binary_op (via emitAnyBitwiseFastPath)` [createcall_only]
- `RangeDate`: 1 sites
    - line 10227 → `qore_rt_binary_op` [createcall_only]
- `RangeSliceFloat`: 1 sites
    - line 10251 → `qore_rt_ternary_op` [createcall_only]
- `RefForeachCleanup`: 1 sites
    - line 11289 → `qore_rt_ref_foreach_cleanup` [createcall_only]
- `RefForeachFinalize`: 1 sites
    - line 11279 → `qore_rt_ref_foreach_finalize` [createcall_only]
- `RefForeachGetEntry`: 1 sites
    - line 11241 → `qore_rt_ref_foreach_get_entry` [createcall_only]
- `RefForeachInit`: 1 sites
    - line 11210 → `qore_rt_invoke_expr_aot / qore_rt_ref_foreach_init` [createcall_only]
- `RefForeachRecord`: 1 sites
    - line 11267 → `qore_rt_ref_foreach_record` [createcall_only]
- `RegexExtractList`: 1 sites
    - line 10336 → `qore_rt_invoke_expr / qore_rt_invoke_expr_aot / qore_rt_regex_op_by_pattern / qore_rt_regex_op_with_operand / qore_rt_regex_op_with_operand_aot` [createcall_only]
- `ShiftLValue`: 1 sites
    - line 9015 → `qore_rt_lvalue_unary / qore_rt_lvalue_unary_aot` [createcall_only]
- `ShlAny`: 1 sites
    - line 7693 → `qore_rt_binary_op (via emitAnyBitwiseFastPath)` [unclear]
- `ShrAny`: 1 sites
    - line 7709 → `qore_rt_binary_op (via emitAnyBitwiseFastPath)` [unclear]
- `ShrAssignLValue`: 1 sites
    - line 9137 → `(none)` [createcall_only]
- `SpliceLValue`: 1 sites
    - line 9230 → `qore_rt_lvalue_ternary / qore_rt_lvalue_ternary_aot` [createcall_only]
- `Sprintf`: 1 sites
    - line 3642 → `qore_rt_sprintf` [createcall_only]
- `StoreClosure`: 1 sites
    - line 8151 → `qore_rt_assign_local / qore_rt_store_closure_aot` [createcall_only]
- `StoreLValue`: 1 sites
    - line 8974 → `(none)` [createcall_only]
- `StoreLocal`: 1 sites
    - line 3935 → `qore_rt_coerce_value / qore_rt_coerce_value_aot` [createcall_only]
- `StoreThreadLocal`: 1 sites
    - line 8086 → `(none)` [aot_invoke_jit_createcall]
- `Summarize`: 1 sites
    - line 10939 → `qore_rt_exec_statement` [createcall_only]
- `SwitchCaseMatch`: 1 sites
    - line 11345 → `qore_rt_switch_case_match / qore_rt_switch_case_match_value / qore_rt_switch_case_match_value_aot` [createcall_only]
- `SwitchRegexMatch`: 1 sites
    - line 10376 → `qore_rt_get_regex_case_aot / qore_rt_switch_regex_match` [createcall_only]
- `UnaryMinusAny`: 1 sites
    - line 7723 → `qore_rt_unary_op (via emitAnyUnaryFastPath)` [unclear]
- `UnaryPlusAny`: 1 sites
    - line 7735 → `qore_rt_unary_op (via emitAnyUnaryFastPath)` [unclear]
- `UnshiftLValue`: 1 sites
    - line 9056 → `qore_rt_lvalue_binary / qore_rt_lvalue_binary_aot` [createcall_only]
- `VrnConstruct`: 1 sites
    - line 8788 → `qore_rt_vrn_construct / qore_rt_vrn_construct_aot` [createcall_only]
- `XorAny`: 1 sites
    - line 7677 → `qore_rt_binary_op (via emitAnyBitwiseFastPath)` [createcall_only]
