# Expression Handler Integration Guide - Phase 3.2

## Handlers with Recursive Dependencies

The following handlers call `classifyAndWriteExpr()` or `readOneExpr()` and therefore require these functions to be accessible as extern symbols:

### Write-Side Recursive Handlers
(Call `classifyAndWriteExpr()` internally)

1. **NEW_OBJECT** (4) - Line 183
   - Calls `classifyAndWriteExpr()` for each argument
2. **SCOPED_NEW_OBJECT** (15) - Line 515
   - Calls `classifyAndWriteExpr()` for each argument
3. **HASH_LITERAL** (21) - Lines 721, 735
   - Calls `classifyAndWriteExpr()` for each key string and value
4. **HASH_DEREF** (22) - Lines 770-771
   - Calls `classifyAndWriteExpr()` for left and right sub-expressions
5. **PARSE_REF** (23) - Line 802
   - Calls `classifyAndWriteExpr()` for inner lvalue expression
6. **LIST_LITERAL** (33) - Line 1063
   - Calls `classifyAndWriteExpr()` for each element

### Read-Side Recursive Handlers
(Call `readOneExpr()` internally)

1. **NEW_OBJECT** (4) - Line 210
   - Calls `readOneExpr()` for each argument
2. **SCOPED_NEW_OBJECT** (15) - Line 531
   - Calls `readOneExpr()` for each argument
3. **HASH_LITERAL** (21) - Line 749
   - Calls `readOneExpr()` for each value
4. **HASH_DEREF** (22) - Lines 779, 785
   - Calls `readOneExpr()` for left and right sub-expressions
5. **PARSE_REF** (23) - Line 810
   - Calls `readOneExpr()` for inner lvalue expression
6. **LIST_LITERAL** (33) - Line 1076
   - Calls `readOneExpr()` for each element

**Total**: 6 write handlers + 6 read handlers = 12 handlers with recursive dependencies

## Integration Steps

### Step 1: Promote Static Helper Functions

#### In `lib/QoreAOTBinary.cpp`
**Line 1844**: Change from `static bool classifyAndWriteExpr(...)` to:
```cpp
// Public declaration for use by expression registry handlers
bool classifyAndWriteExpr(QoreAOTBinaryWriter& writer, const QoreValue& expr,
        const std::vector<AOTLocalSlotId>& parent_locals,
        const std::vector<AOTGlobalSlotId>& parent_globals,
        const AOTConstantReverseMap* const_reverse_map = nullptr) {
    // existing implementation unchanged
}
```

**Action**: Remove `static` keyword (keep function as is otherwise)

#### In `lib/QoreAOTRuntime.cpp`
**Line 690**: Change from `static QoreValue readOneExpr(...)` to:
```cpp
// Public declaration for use by expression registry handlers
QoreValue readOneExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals) {
    // existing implementation unchanged
}
```

**Action**: Remove `static` keyword (keep function as is otherwise)

### Step 2: Add Extern Declarations

Create a new internal header or add to existing one:

#### Option A: Add to `include/qore/intern/QoreAOTBinary.h`

At the end of the file, after the `QoreAOTExprRegistry.h` include section, add:

```cpp
// ============================================================================
// Internal Expression Handler Helpers (for registry implementation)
// ============================================================================
// These functions are used by expression registry handlers for recursive
// serialization/deserialization of nested expressions.

//! Classify and write a QoreValue expression in AOTExprKind format
//! @param writer binary writer to write to
//! @param expr the expression to serialize
//! @param parent_locals parent function's local slot metadata
//! @param parent_globals parent function's global slot metadata
//! @param const_reverse_map reverse map for constant lookup (optional)
//! @return true if expression was successfully serialized, false otherwise
bool classifyAndWriteExpr(QoreAOTBinaryWriter& writer, const QoreValue& expr,
        const std::vector<AOTLocalSlotId>& parent_locals,
        const std::vector<AOTGlobalSlotId>& parent_globals,
        const AOTConstantReverseMap* const_reverse_map = nullptr);

//! Read one expression from inline closure/handler IR binary data
//! @param rdr binary reader for reading data
//! @param p current read pointer (advanced by reading)
//! @param e end of valid data
//! @param err set to error message on failure
//! @param pgm the Qore program for symbol resolution
//! @param locals LocalVar* array for LOCAL_VARREF resolution (may be null)
//! @param num_locals number of entries in locals
//! @param globals Var* array for GLOBAL_VARREF resolution (may be null)
//! @param num_globals number of entries in globals
//! @return reconstructed expression, or NOTHING on failure
QoreValue readOneExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals);
```

#### Option B: Create new header `include/qore/intern/QoreAOTInternalHelpers.h`

```cpp
/* -*- mode: c++ -*- */
/*
  QoreAOTInternalHelpers.h

  Internal helper declarations for AOT expression handling (Phase 3.2+)

  Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#ifndef _QORE_AOTINTERNALHELPERS_H
#define _QORE_AOTINTERNALHELPERS_H

// Forward declarations
class QoreAOTBinaryWriter;
class QoreAOTBinaryReader;
class QoreValue;
class QoreProgram;
struct AOTLocalSlotId;
struct AOTGlobalSlotId;
typedef std::unordered_map<const AbstractQoreNode*, std::string> AOTConstantReverseMap;

// Internal helpers for expression registry handlers
bool classifyAndWriteExpr(QoreAOTBinaryWriter& writer, const QoreValue& expr,
        const std::vector<AOTLocalSlotId>& parent_locals,
        const std::vector<AOTGlobalSlotId>& parent_globals,
        const AOTConstantReverseMap* const_reverse_map = nullptr);

QoreValue readOneExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals);

#endif // _QORE_AOTINTERNALHELPERS_H
```

### Step 3: Include Declarations in Handler File

Add to `lib/QoreAOTExprHandlers.cpp` at line 35 (after existing includes):

```cpp
// For recursive expression handling in handlers
#include "qore/intern/QoreAOTBinary.h"  // declares classifyAndWriteExpr
// OR if using separate header:
// #include "qore/intern/QoreAOTInternalHelpers.h"
```

Or inline forward declarations if simpler:
```cpp
// Forward declarations for internal recursion
extern bool classifyAndWriteExpr(QoreAOTBinaryWriter& writer, const QoreValue& expr,
        const std::vector<AOTLocalSlotId>& parent_locals,
        const std::vector<AOTGlobalSlotId>& parent_globals,
        const AOTConstantReverseMap* const_reverse_map);

extern QoreValue readOneExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals);
```

### Step 4: Replace Registry Stubs

Replace placeholder handlers in `lib/QoreAOTExprRegistry.cpp` with real implementations from `lib/QoreAOTExprHandlers.cpp`.

#### Option A: Include handler file
Add at line 32 (after `#include "qore/intern/QoreAOTExprRegistry.h"`):
```cpp
// Include handler implementations
#include "QoreAOTExprHandlers.cpp"
```

#### Option B: Direct replacement
Copy all handler functions from `lib/QoreAOTExprHandlers.cpp` into `lib/QoreAOTExprRegistry.cpp`, replacing the placeholder handlers (lines 41-321).

**Recommendation**: Use Option A (include) to keep implementation separate and maintainable.

### Step 5: Update CMakeLists.txt (if needed)

Ensure both source files are in the build:

```cmake
# In CMakeLists.txt where qorelib sources are defined
set(QORE_LIB_SOURCES
    # ... existing sources ...
    lib/QoreAOTBinary.cpp         # provides classifyAndWriteExpr
    lib/QoreAOTRuntime.cpp        # provides readOneExpr
    lib/QoreAOTExprRegistry.cpp   # uses handlers
    # QoreAOTExprHandlers.cpp is included by QoreAOTExprRegistry.cpp
)
```

### Step 6: Compilation & Verification

```bash
cd /home/david/src/qore/git/qore
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug 2>&1 | grep -i "error\|warning"
```

### Step 7: Test Execution

```bash
cd /home/david/src/qore/git/qore
# Run IR tests
./run_tests.sh -d ir

# Or individual test file
build-debug/qore examples/test/irtests/ir-exec-mode.qtest --enable-debug
```

## Summary of Changes

| File | Change | Type |
|------|--------|------|
| `lib/QoreAOTBinary.cpp` | Remove `static` from `classifyAndWriteExpr()` | Modification |
| `lib/QoreAOTRuntime.cpp` | Remove `static` from `readOneExpr()` | Modification |
| `include/qore/intern/QoreAOTBinary.h` | Add extern declarations | Addition |
| `lib/QoreAOTExprRegistry.cpp` | Replace stubs with real handlers | Replacement |
| `lib/QoreAOTExprHandlers.cpp` | (New file with 70 handler functions) | New |

## Handler Files Included

| Handlers | Count | Type |
|----------|-------|------|
| Self-contained (no recursion) | 23 | Simple value/reference |
| With recursive calls | 12 | Collections/constructors |
| Unsupported/stub | 2 | EXPR_TREE, GENERIC_EVAL |
| **Total** | **37** | **35 kinds + 2 special** |

## Verification Checklist

- [ ] Functions de-static'd successfully compile
- [ ] No linking errors from circular dependencies
- [ ] All 35 handler pairs accessible via registry
- [ ] All 9/9 IR test files still pass
- [ ] No memory leaks detected (valgrind clean)
- [ ] Error handling paths work correctly

## Known Issues & Workarounds

### Issue: Circular includes
If including `QoreAOTExprHandlers.cpp` causes circular includes:
- Use forward declarations instead of full includes
- Ensure types are fully defined before use
- Check include guard ordering

### Issue: LOCAL_VARREF and GLOBAL_VARREF read handlers
These handlers are noted as handled specially in `readOneExpr()` with the passed-in locals/globals arrays.
- Read handlers for these types return `QoreValue()` stub
- Actual handling happens in the switch statement of `readOneExpr()` when locals/globals are available

## Testing Success Criteria

✅ **Phase 3.2 Complete**: All 35 handler pairs extracted and ready for integration
✅ **Code Quality**: Exact extraction from source, no invented logic
✅ **Dependencies**: Properly documented and managed
✅ **Integration**: Clear steps for compilation and testing

**Status**: Ready for integration into Phase 3.2 implementation
