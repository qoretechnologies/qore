# AOT Binary Deserialization Memory Leaks

## Summary

The AOT binary deserializer (`QoreAOTBinaryDeserializer`) leaks `QoreValue` nodes
(QoreStringNode, QoreHashNode, QoreListNode, DateTimeNode) allocated during
`deserializeClasses()` and `deserializeHashDecls()`. On the Swagger test binary,
this amounts to **11,466 leaked objects / 577 KB**.

All other execution modes (AST, IR, Tiered, JIT) show **0 leaks** on the same test.

## Root Cause

`QoreValue` is a plain value type (not RAII) — it does not deref its contained node
pointer on destruction. Code that allocates nodes via `readValue()` must explicitly
call `val.discard(xsink)` when the value is no longer needed.

The deserializer has two leak patterns:

### Leak 1: Pre-existing class values not discarded (primary — ~11K objects)

**File**: `lib/QoreAOTBinary.cpp`, `deserializeClasses()`, lines 1910-1932 and 1961-1979

When a class already exists in the namespace (from a loaded module), the deserializer
sets `class_already_existed = true` and skips storing the values. But `readValue()` is
still called (to advance the binary read pointer), and the returned `QoreValue` is
never discarded:

```cpp
// Line 1916-1931: Instance member defaults
QoreValue default_val;
if (has_default) {
    default_val = reader.readValue(ptr, end, error);  // allocates node
    // ...
}
if (!class_already_existed && mname && *mname) {
    // ... stores default_val in PendingInstanceMember
}
// default_val goes out of scope here — node leaked if class_already_existed!
```

Same pattern for class constants at lines 1961-1979:
```cpp
QoreValue cval = reader.readValue(ptr, end, error);  // allocates node
// ...
if (!class_already_existed && cname && *cname) {
    // ... stores cval in PendingClassConstant
}
// cval goes out of scope here — node leaked if class_already_existed!
```

### Leak 2: Hashdecl member defaults on duplicate skip (minor)

**File**: `lib/QoreAOTBinary.cpp`, `deserializeHashDecls()`, lines 2262-2299

When a hashdecl already exists (line 2296-2299), the code does `hdp->deref()` and
`continue`, but the `members` vector with its `MemberInfo::default_val` QoreValues
goes out of scope without cleanup:

```cpp
if (ns_list[ns_idx]->hashDeclList.add(hd) != 0) {
    hdp->deref();
    continue;  // members vector with QoreValues destroyed without discard!
}
```

### Leak 3: Destructor doesn't clean pending values

**File**: `include/qore/intern/QoreAOTBinary.h`, lines 927-929

The `~QoreAOTBinaryDeserializer()` destructor only deletes `type_resolver`. If
deserialization fails partway through (early return from `deserializeIntoProgram()`),
any pending `QoreValue` objects in `pending_instance_members`,
`pending_class_constants`, and `pending_hashdecl_members` are leaked.

## Fix Locations

### Fix 1: Discard values for pre-existing classes

In `deserializeClasses()`, add `discard()` calls when skipping:

```cpp
// After line 1922, add else-if for class_already_existed:
if (!class_already_existed && mname && *mname) {
    PendingInstanceMember pim;
    // ... store values ...
} else if (default_val.hasNode()) {
    default_val.discard(nullptr);
}
```

Same for class constants:
```cpp
if (!class_already_existed && cname && *cname) {
    PendingClassConstant pcc;
    // ... store values ...
} else if (cval.hasNode()) {
    cval.discard(nullptr);
}
```

### Fix 2: Discard hashdecl member defaults on duplicate skip

In `deserializeHashDecls()`, before the `continue` on duplicate hashdecl:

```cpp
if (ns_list[ns_idx]->hashDeclList.add(hd) != 0) {
    hdp->deref();
    // Discard any QoreValue nodes in members
    for (auto& mi : members) {
        mi.default_val.discard(nullptr);
    }
    continue;
}
```

### Fix 3: Add cleanup to destructor for early-return safety

In `~QoreAOTBinaryDeserializer()`:

```cpp
~QoreAOTBinaryDeserializer() {
    delete type_resolver;
    // Clean up any pending QoreValues that weren't transferred to the namespace tree
    for (auto& class_members : pending_instance_members) {
        for (auto& pim : class_members) {
            pim.default_val.discard(nullptr);
        }
    }
    for (auto& class_consts : pending_class_constants) {
        for (auto& pcc : class_consts) {
            pcc.value.discard(nullptr);
        }
    }
    for (auto& [hd, members] : pending_hashdecl_members) {
        for (auto& phm : members) {
            phm.default_val.discard(nullptr);
        }
    }
}
```

Note: For the normal (non-error) path, `resolveInstanceMembers()` clears
`pim.default_val` after transferring to the class (line 2047:
`pim.default_val = QoreValue()`), and `resolveClassConstants()` passes
`pcc.value` to `addUserConstant()` which takes ownership. So the destructor
cleanup only fires on early-return error paths.

### Comparison: `deserializeConstants()` is already correct

`deserializeConstants()` (line 2464) properly calls `val.discard(nullptr)` on all
skip/error paths (lines 2492, 2498, 2510, 2517). The class and hashdecl paths
should follow this same pattern.

## Verification

### Reproduce the leak
```bash
# Build
cmake --build build --target libqore -j8 && cmake --build build --target qore -j8

# AOT compile (creates source-stripped binary)
DYLD_LIBRARY_PATH=build QORE_MODULE_DIR=qlib:build/modules/reflection:build/modules/astparser \
  build/qore --compile --opt-level=3 examples/test/qlib/Swagger/Swagger.qtest

# Run with leaks tool (macOS)
DYLD_LIBRARY_PATH=build QORE_MODULE_DIR=qlib:build/modules/reflection:build/modules/astparser \
  MallocStackLogging=1 leaks --atExit -- examples/test/qlib/Swagger/Swagger -b --enable-debug -v

# Run with valgrind (Linux)
LD_LIBRARY_PATH=build QORE_MODULE_DIR=qlib:build/modules/reflection:build/modules/astparser \
  valgrind --leak-check=full --show-leak-kinds=definite \
  examples/test/qlib/Swagger/Swagger -b --enable-debug -v
```

### Verify the fix
After applying all three fixes, the AOT binary should show 0 leaks:
```bash
# macOS
leaks --atExit -- examples/test/qlib/Swagger/Swagger -b --enable-debug -v 2>&1 | grep "leaks for"
# Expected: Process XXXXX: 0 leaks for 0 total leaked bytes.

# Linux
valgrind --leak-check=full examples/test/qlib/Swagger/Swagger -b --enable-debug -v 2>&1 | grep "definitely lost"
# Expected: definitely lost: 0 bytes in 0 blocks
```

### Verify no regressions
```bash
# All exec modes should still pass 22/22 tests, 758/758 assertions
for mode in ast ir tiered jit; do
  build/qore --enable-debug --exec-mode=$mode examples/test/qlib/Swagger/Swagger.qtest -v
done
# AOT binary
examples/test/qlib/Swagger/Swagger --enable-debug -v
```

## Leak Breakdown by Type (macOS `leaks` tool output)

| Type | Instances | Notes |
|------|-----------|-------|
| QoreStringNode | 964 | Hash member keys, string constant values |
| QoreHashNode | 262 | Default member values (nested hashes) |
| QoreListNode | 27 | Default member values (arrays) |
| DateTimeNode | 21 | Date constant values |
| **Total** | **11,466** | Includes child nodes of leaked containers |

The 11,466 count includes all transitively-reachable child nodes of the root
leaked objects (e.g., strings inside leaked hashes).

## Files to Modify

| File | Change |
|------|--------|
| `lib/QoreAOTBinary.cpp` | Add `discard()` calls in `deserializeClasses()` for pre-existing class values |
| `lib/QoreAOTBinary.cpp` | Add `discard()` loop in `deserializeHashDecls()` before `continue` on duplicate |
| `include/qore/intern/QoreAOTBinary.h` | Add pending QoreValue cleanup in `~QoreAOTBinaryDeserializer()` |
