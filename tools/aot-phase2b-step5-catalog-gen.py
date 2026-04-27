#!/usr/bin/env python3
"""
Catalog every emitExceptionCheck() call site in lib/QoreIRToLLVM.cpp.

For each call site extract:
  - line number
  - nearest preceding `case QoreIROpcode::...:` label
  - nearest preceding helper name (via `getOrInsertFunction("qore_rt_XXX"`)
  - whether that helper already has a _throwing twin
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "lib/QoreIRToLLVM.cpp"
JIT = ROOT / "lib/JITRuntime.cpp"

# Existing throwing wrappers.
throwing = set()
for m in re.finditer(r'qore_rt_(\w+)_throwing\s*\(', JIT.read_text()):
    throwing.add("qore_rt_" + m.group(1))

lines = SRC.read_text().splitlines()
n = len(lines)

# All emitExceptionCheck sites (except the definition at 2121).
sites = []
for i, ln in enumerate(lines, 1):
    if 'emitExceptionCheck(' in ln and 'void QoreIRToLLVM::emitExceptionCheck' not in ln:
        sites.append(i)

RE_CASE = re.compile(r'\bcase\s+QoreIROpcode::(\w+)\s*:')
RE_HELPER = re.compile(r'getOrInsertFunction\(\s*"(qore_rt_\w+)"')
# Fast-path helpers that internally issue a runtime call on the slow path.
# Returns (fast_path_fn_name, slow_helper_name) — when one of these is seen,
# the slow helper is the real attribution.
FAST_PATH_HELPERS = {
    "emitAnyCmpFastPath": "qore_rt_comparison_op",
    "emitAnyCmpSpaceshipFastPath": "qore_rt_comparison_op",
    "emitAnyArithFastPath": "qore_rt_binary_op",
    "emitAnyBitwiseFastPath": "qore_rt_binary_op",
    "emitAnyUnaryFastPath": "qore_rt_unary_op",
    "emitAnyCompoundAssignFastPath": "qore_rt_binary_op",
}
RE_FASTPATH = re.compile(r'\b(' + '|'.join(FAST_PATH_HELPERS.keys()) + r')\(')
RE_DIRECT_CALL = re.compile(r'CreateCall\(\s*(?:module\.)?getOrInsertFunction\(\s*"(qore_rt_\w+)"')
# Also pattern like:
#   auto helper = module.getOrInsertFunction("qore_rt_XXX", ...);
#   ...
#   builder->CreateCall(helper, ...);

def nearest_preceding(line_idx, regex):
    """Search upward from line_idx for the first regex match. Returns group 1 or None."""
    for j in range(line_idx - 1, max(0, line_idx - 200), -1):
        m = regex.search(lines[j])
        if m:
            return m.group(1)
    return None

def nearest_preceding_helper(line_idx):
    """Prefer the most-recent getOrInsertFunction / fast-path helper.
       Bounded by the enclosing `case QoreIROpcode::X:` label — don't attribute
       helpers from a different opcode's emission."""
    helpers_in_case = []
    fp_in_case = None
    for j in range(line_idx - 1, max(0, line_idx - 200), -1):
        ln = lines[j]
        if RE_CASE.search(ln):
            break
        m_fp = RE_FASTPATH.search(ln)
        if m_fp and fp_in_case is None:
            fp_in_case = m_fp.group(1)
        m = RE_HELPER.search(ln)
        if m:
            helpers_in_case.append(m.group(1))
    if fp_in_case:
        return FAST_PATH_HELPERS[fp_in_case] + " (via " + fp_in_case + ")"
    if helpers_in_case:
        # Report all helpers in the case (since both AOT and JIT branches may declare).
        if len(helpers_in_case) == 1:
            return helpers_in_case[0]
        return " / ".join(sorted(set(helpers_in_case)))
    return None

RE_MAYBE_INVOKE = re.compile(r'\bemitMaybeInvoke\(')
RE_CREATE_CALL_RT = re.compile(r'\bbuilder->CreateCall\(\s*\w+\b')
RE_AOT_MODE = re.compile(r'\bif\s*\(\s*aot_mode\s*\)')

def site_pattern(line_idx):
    """Classify the preceding ~100 lines:
       - 'aot_invoke_only': AOT branch uses emitMaybeInvoke
       - 'aot_createcall_only': AOT branch uses CreateCall
       - 'invoke_both': emitMaybeInvoke with no clear branching
       - 'createcall_only': CreateCall with no emitMaybeInvoke
       - 'unclear'
    """
    has_mi = False
    has_cc = False
    has_aot_mode = False
    for j in range(line_idx - 1, max(0, line_idx - 100), -1):
        ln = lines[j]
        if RE_MAYBE_INVOKE.search(ln):
            has_mi = True
        if RE_CREATE_CALL_RT.search(ln):
            has_cc = True
        if RE_AOT_MODE.search(ln):
            has_aot_mode = True
    if has_mi and not has_cc:
        return "invoke_only"
    if has_mi and has_cc and has_aot_mode:
        return "aot_invoke_jit_createcall"
    if has_mi and has_cc:
        return "mixed_invoke_createcall"
    if has_cc:
        return "createcall_only"
    return "unclear"

rows = []
for site in sites:
    opcode = nearest_preceding(site, RE_CASE) or "(unknown)"
    helper = nearest_preceding_helper(site) or "(none)"
    # For "(via ...)" or "A / B" forms, consider each base; throwing twin present
    # if ANY underlying helper has one.
    helper_base_list = []
    if helper == "(none)":
        helper_base_list = []
    elif " (via " in helper:
        helper_base_list = [helper.split(" ")[0]]
    elif " / " in helper:
        helper_base_list = [h.strip() for h in helper.split(" / ")]
    else:
        helper_base_list = [helper]
    tw_hits = [h + "_throwing" for h in helper_base_list if h in throwing]
    has_throwing = tw_hits[0] if tw_hits else ""
    pattern = site_pattern(site)
    rows.append((site, opcode, helper, bool(has_throwing), has_throwing, pattern))

# Group.
from collections import defaultdict
by_helper = defaultdict(list)
for (site, opcode, helper, has_tw, tw, pattern) in rows:
    by_helper[helper].append((site, opcode, has_tw, tw, pattern))

# Group by opcode too.
by_opcode = defaultdict(list)
for (site, opcode, helper, has_tw, tw, pattern) in rows:
    by_opcode[opcode].append((site, helper, has_tw, tw, pattern))

by_pattern = defaultdict(list)
for (site, opcode, helper, has_tw, tw, pattern) in rows:
    by_pattern[pattern].append((site, opcode, helper, has_tw, tw))

print(f"Total emitExceptionCheck call sites: {len(sites)}")
print(f"Distinct helpers referenced: {len(by_helper)}")
print(f"Distinct opcodes: {len(by_opcode)}")
print()
print("### Pattern breakdown")
for p, rs in sorted(by_pattern.items(), key=lambda x: (-len(x[1]), x[0])):
    print(f"  - `{p}`: {len(rs)}")
print()
print(f"Existing _throwing wrappers ({len(throwing)}):")
for t in sorted(throwing):
    print(f"  - {t}")
print()
print("## Migration buckets")
print()
# Bucket 1: helper already has throwing twin + site uses emitMaybeInvoke → DONE.
# Bucket 2: helper has throwing twin + site uses CreateCall → EASY (swap call).
# Bucket 3: helper has NO throwing twin + site uses CreateCall → NEEDS NEW WRAPPER.
b1, b2, b3, bnone = [], [], [], []
for r in rows:
    site, opcode, helper, has_tw, tw, pattern = r
    if helper == "(none)":
        bnone.append(r)
    elif has_tw and pattern in ("invoke_only", "aot_invoke_jit_createcall"):
        b1.append(r)
    elif has_tw:
        b2.append(r)
    else:
        b3.append(r)

print(f"### Bucket 1 — DONE (already invoke, throwing twin exists): {len(b1)}")
for (site, opcode, helper, ht, tw, pat) in sorted(b1):
    print(f"    - line {site} ({opcode}) → `{helper}` [{pat}]")
print()
print(f"### Bucket 2 — EASY (has throwing twin, but some path uses CreateCall): {len(b2)}")
for (site, opcode, helper, ht, tw, pat) in sorted(b2):
    print(f"    - line {site} ({opcode}) → `{helper}` → twin: `{tw}` [{pat}]")
print()
print(f"### Bucket 3 — NEEDS NEW _throwing WRAPPER: {len(b3)}")
# Group bucket 3 by helper for Phase B.
b3_by_helper = defaultdict(list)
for r in b3:
    b3_by_helper[r[2]].append(r)
for h, rs in sorted(b3_by_helper.items(), key=lambda x: (-len(x[1]), x[0])):
    print(f"  - `{h}`: {len(rs)} sites")
    for (site, opcode, helper, ht, tw, pat) in sorted(rs):
        print(f"      - line {site} ({opcode}) [{pat}]")
print()
print(f"### Bucket NONE — could not identify preceding helper (inspect manually): {len(bnone)}")
for (site, opcode, helper, ht, tw, pat) in sorted(bnone):
    print(f"    - line {site} ({opcode}) [{pat}]")
print()
print("## Phase B plan — unique BASE helpers needing _throwing wrappers")
print()
print("Note: AOT Step 5 targets AOT-mode compile speed. Wrappers named")
print("`qore_rt_X_aot_throwing` are the critical ones; JIT-mode `qore_rt_X_throwing`")
print("are nice-to-have for consistency but won't affect the HttpServer.qm compile")
print("cliff (that code runs in AOT mode).")
print()

# Collect unique base helpers from bucket 3.
needed = defaultdict(list)  # base helper name -> list of (site, opcode)
for (site, opcode, helper, has_tw, tw, pattern) in b3:
    if helper == "(none)":
        continue
    bases = []
    if " (via " in helper:
        bases = [helper.split(" ")[0]]
    else:
        bases = [h.strip() for h in helper.split(" / ")]
    for b in bases:
        needed[b].append((site, opcode))

# Phase B categories per the continuation prompt.
categories = [
    ("Local access", {
        "qore_rt_load_local", "qore_rt_load_local_aot",
        "qore_rt_assign_local", "qore_rt_assign_local_aot",
        "qore_rt_clear_local", "qore_rt_clear_local_aot",
        "qore_rt_instantiate_local", "qore_rt_instantiate_local_aot",
        "qore_rt_uninstantiate_local",
        "qore_rt_load_closure_aot", "qore_rt_store_closure_aot",
        "qore_rt_pop_closure_var_aot",
        "qore_rt_load_thread_local", "qore_rt_load_thread_local_aot",
        "qore_rt_store_thread_local", "qore_rt_store_thread_local_aot",
        "qore_rt_load_global", "qore_rt_load_global_aot",
        "qore_rt_store_global", "qore_rt_store_global_aot",
        "qore_rt_load_static_var",
        "qore_rt_load_constant", "qore_rt_load_constant_value",
    }),
    ("Expression / value", {
        "qore_rt_invoke_expr", "qore_rt_invoke_expr_aot",
        "qore_rt_make_string", "qore_rt_make_list", "qore_rt_make_hash",
        "qore_rt_make_hash_const_keys", "qore_rt_make_enum",
        "qore_rt_new_complex_hash", "qore_rt_new_complex_list",
        "qore_rt_new_hash_decl", "qore_rt_new_hash_decl_from_hash",
        "qore_rt_new_hash_decl_from_hash_by_path",
        "qore_rt_hash_key_access", "qore_rt_hash_key_store_cow",
        "qore_rt_hash_key_store_cow_aot",
        "qore_rt_hash_key_store_dynamic_cow",
        "qore_rt_hash_key_store_dynamic_cow_aot",
        "qore_rt_hash_set_key_value",
        "qore_rt_list_index_access", "qore_rt_list_push",
        "qore_rt_list_index_store_cow", "qore_rt_list_index_store_cow_aot",
        "qore_rt_sprintf", "qore_rt_cast_with_inner",
        "qore_rt_cast_with_inner_aot", "qore_rt_cast_by_type_path",
        "qore_rt_coerce_value", "qore_rt_coerce_value_aot",
        "qore_rt_strip_complex_type",
        "qore_rt_create_call_ref", "qore_rt_create_closure",
        "qore_rt_create_method_ref", "qore_rt_create_parse_ref",
        "qore_rt_vrn_construct",
        "qore_rt_dot_eval_with_base", "qore_rt_dot_eval_with_base_aot",
        "qore_rt_dot_eval_pseudo_method_direct",
        "qore_rt_call_method_direct", "qore_rt_call_method_fast",
        "qore_rt_call_static_method_direct",
        "qore_rt_call_self_recursive",
        "qore_rt_call_fast_with_target",
        "qore_rt_call_with_args",
        "qore_rt_call_ref_fast",
        "qore_rt_instanceof", "qore_rt_instanceof_by_type_path",
        "qore_rt_background_self_call",
        "qore_rt_background_self_call_aot",
        "qore_rt_regex_op_by_pattern", "qore_rt_regex_op_with_operand",
        "qore_rt_regex_op_with_operand_aot",
        "qore_rt_get_regex_case_aot",
        "qore_rt_switch_regex_match", "qore_rt_switch_case_match",
    }),
    ("Lvalue ops", {
        "qore_rt_lvalue_unary", "qore_rt_lvalue_unary_aot",
        "qore_rt_lvalue_binary", "qore_rt_lvalue_binary_aot",
        "qore_rt_lvalue_ternary", "qore_rt_lvalue_ternary_aot",
        "qore_rt_lvalue_load", "qore_rt_lvalue_load_aot",
        "qore_rt_lv_path_ternary", "qore_rt_lv_path_ternary_aot",
    }),
    ("Iterator", {
        "qore_rt_iterator_create", "qore_rt_iterator_create_aot",
        "qore_rt_iterator_create_reverse",
        "qore_rt_iterator_next",
        "qore_rt_ref_foreach_init", "qore_rt_ref_foreach_get_entry",
        "qore_rt_ref_foreach_record", "qore_rt_ref_foreach_finalize",
        "qore_rt_ref_foreach_cleanup",
    }),
    ("Fast-path slow-route helpers", {
        "qore_rt_comparison_op", "qore_rt_binary_op", "qore_rt_unary_op",
    }),
    ("Other", {
        "qore_rt_exec_statement", "qore_rt_get_int64",
        "qore_rt_decref",
    }),
]
classified = set()
for (name, members) in categories:
    print(f"### {name}")
    hits = []
    for b in sorted(members):
        if b in needed:
            hits.append((b, needed[b]))
            classified.add(b)
    if not hits:
        print("  (no sites in this category)")
    for (b, sites_b) in hits:
        site_strs = [f"{s}({op})" for (s, op) in sites_b]
        # Truncate long lists.
        more = ""
        if len(site_strs) > 6:
            more = f" +{len(site_strs)-6} more"
            site_strs = site_strs[:6]
        print(f"  - `{b}` ({len(sites_b)} sites): {', '.join(site_strs)}{more}")
    print()

# Uncategorized.
leftover = sorted(set(needed.keys()) - classified)
if leftover:
    print("### Uncategorized (needs review)")
    for b in leftover:
        sites_b = needed[b]
        site_strs = [f"{s}({op})" for (s, op) in sites_b]
        print(f"  - `{b}` ({len(sites_b)} sites): {', '.join(site_strs[:6])}")
    print()
print()
print("## Sites grouped by helper")
print()
for h, rows_h in sorted(by_helper.items(), key=lambda x: (-len(x[1]), x[0])):
    has_tw = any(r[2] for r in rows_h)
    tw = rows_h[0][3] if has_tw else ""
    tag = f" [THROWING TWIN: {tw}]" if has_tw else " [no throwing twin]"
    if h == "(none)":
        tag = " [no helper — likely post-call cleanup or non-helper site]"
    print(f"- `{h}`: {len(rows_h)} sites{tag}")
    for (site, opcode, ht, t, pat) in rows_h:
        print(f"    - line {site} ({opcode}) [{pat}]")
print()
print("## Sites grouped by opcode")
print()
for opc, rows_o in sorted(by_opcode.items(), key=lambda x: (-len(x[1]), x[0])):
    print(f"- `{opc}`: {len(rows_o)} sites")
    for (site, helper, ht, t, pat) in rows_o:
        tmark = " ✓twin" if ht else ""
        print(f"    - line {site} → `{helper}`{tmark} [{pat}]")
