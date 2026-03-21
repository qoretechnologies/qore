# Opcode Registry Implementation Plan

## Overview

Implement a centralized opcode registry to eliminate silent bugs and simplify opcode property management across the codebase.

**Total Effort**: ~4-6 hours across 4 phases
**Risk Level**: Low (registry is additive, existing functions unchanged until phase 3)
**Rollback**: Easy at any phase (just delete registry code)

---

## Phase 1: Create Registry Structure (1-1.5 hours)

**Goal**: Define the registry data structure and populate it with all 349 opcodes.
**Files**:
- `include/qore/intern/QoreOpcodeRegistry.h` (NEW)
- `include/qore/intern/QoreIR.h` (MODIFY - add include)

### Step 1a: Create QoreOpcodeRegistry.h

```cpp
// include/qore/intern/QoreOpcodeRegistry.h
#ifndef _QORE_OPCODE_REGISTRY_H
#define _QORE_OPCODE_REGISTRY_H

#include "qore/intern/QoreIR.h"

//! Metadata for a single IR opcode
struct OpcodeInfo {
    QoreIROpcode opcode;
    const char* name;           //! Human-readable name for debugging/printing
    bool can_return_nothing;    //! True if opcode can legitimately return NOTHING
    bool never_returns_nothing; //! True if opcode always returns non-NOTHING
    bool is_terminator;         //! True if opcode ends a basic block (jumps/returns/throws)
    int expected_operands;      //! Number of operands (-1 = variable/context-dependent)
};

//! Central registry of all IR opcodes
//! This is the single source of truth for opcode properties.
//! When adding a new opcode:
//!   1. Add entry to QoreIROpcode enum in QoreIR.h
//!   2. Update QORE_IR_MAX_OPCODE
//!   3. Add entry to this registry
//!   4. static_assert will verify completeness
constexpr OpcodeInfo OPCODE_REGISTRY[] = {
    // Note: entries MUST be in order matching the enum (0, 1, 2, ...)
    // Format: {Opcode::Name, "name", can_return_nothing, never_returns_nothing, is_terminator, expected_operands}

    // Load/store operations
    { QoreIROpcode::LoadLocal,        "LoadLocal",        true,  false, false, 2 },  // id + extra output
    { QoreIROpcode::LoadClosure,      "LoadClosure",      true,  false, false, 2 },
    { QoreIROpcode::LoadGlobal,       "LoadGlobal",       true,  false, false, 2 },
    { QoreIROpcode::LoadThreadLocal,  "LoadThreadLocal",  true,  false, false, 2 },
    { QoreIROpcode::StoreLocal,       "StoreLocal",       false, true,  false, 1 },
    { QoreIROpcode::StoreClosure,     "StoreClosure",     false, true,  false, 1 },
    { QoreIROpcode::StoreGlobal,      "StoreGlobal",      false, true,  false, 1 },
    { QoreIROpcode::StoreThreadLocal, "StoreThreadLocal", false, true,  false, 1 },

    // Constants
    { QoreIROpcode::ConstInt,         "ConstInt",         false, true,  false, 1 },
    { QoreIROpcode::ConstFloat,       "ConstFloat",       false, true,  false, 1 },
    { QoreIROpcode::ConstString,      "ConstString",      false, true,  false, 1 },
    { QoreIROpcode::ConstNothing,     "ConstNothing",     true,  false, false, 0 },
    // ... continue for all opcodes up to Sprintf

    // Control flow
    { QoreIROpcode::Jump,             "Jump",             false, false, true,  0 },
    { QoreIROpcode::CondJump,         "CondJump",         false, false, true,  1 },
    { QoreIROpcode::Return,           "Return",           false, false, true,  -1 },
    { QoreIROpcode::Throw,            "Throw",            false, false, true,  1 },
};

//! Verify registry completeness at compile time
static_assert(
    sizeof(OPCODE_REGISTRY) / sizeof(OPCODE_REGISTRY[0]) == static_cast<int>(QoreIROpcode::Sprintf) + 1,
    "OPCODE_REGISTRY incomplete: missing opcode entries"
);

//! Look up opcode info by opcode ID
//! Returns nullptr if opcode not found (which should never happen at compile time)
constexpr const OpcodeInfo* getOpcodeInfo(QoreIROpcode op) {
    int op_id = static_cast<int>(op);
    if (op_id >= 0 && op_id < static_cast<int>(QoreIROpcode::Sprintf) + 1) {
        return &OPCODE_REGISTRY[op_id];
    }
    return nullptr;
}

#endif // _QORE_OPCODE_REGISTRY_H
```

### Step 1b: Populate All 349 Opcodes

**Task**: Fill in all opcode entries for QoreIROpcode enum values 0-348.

**Source**: Extract from `QoreIROpcode` enum in `include/qore/intern/QoreIR.h` (lines 81-540)

**Process**:
1. For each opcode in the enum, create a registry entry
2. Use opcode names directly from the enum
3. For initial values, use conservative defaults:
   - `can_return_nothing`: `false` (assume safe)
   - `never_returns_nothing`: `false` (assume safe)
   - `is_terminator`: `false` (assume not terminal)
   - `expected_operands`: `-1` (variable)

4. Then refine based on opcode semantics:

```
LOAD opcodes      → can_return_nothing=true
CONST opcodes     → never_returns_nothing=true, expected_operands=0 or 1
CALL opcodes      → can_return_nothing=true
RETURN/THROW/JUMP → is_terminator=true
STORE opcodes     → never_returns_nothing=true, expected_operands=1
Arithmetic opcodes → never_returns_nothing=true
Hash/List access  → can_return_nothing=true
```

**Verification**:
- Verify count: `sizeof(OPCODE_REGISTRY) / sizeof(OPCODE_REGISTRY[0]) == 349`
- Ensure no gaps in opcode IDs
- Add `#include "qore/intern/QoreOpcodeRegistry.h"` to `QoreIR.h`

**Testing**: Compile and verify no static_assert failures

---

## Phase 2: Create Lookup Functions (1-1.5 hours)

**Goal**: Implement property query functions that use the registry.
**Files**:
- `lib/QoreOpcodeRegistry.cpp` (NEW)
- `include/qore/intern/QoreOpcodeRegistry.h` (MODIFY - add function declarations)

### Step 2a: Implement Fast Lookup Functions

Add to `QoreOpcodeRegistry.h`:

```cpp
//! Query functions that delegate to registry
//! These are the new preferred way to check opcode properties

inline bool getOpcodeCanReturnNothing(QoreIROpcode op) {
    const OpcodeInfo* info = getOpcodeInfo(op);
    return info ? info->can_return_nothing : false;  // conservative default
}

inline bool getOpcodeNeverReturnsNothing(QoreIROpcode op) {
    const OpcodeInfo* info = getOpcodeInfo(op);
    return info ? info->never_returns_nothing : false;
}

inline bool getOpcodeIsTerminator(QoreIROpcode op) {
    const OpcodeInfo* info = getOpcodeInfo(op);
    return info ? info->is_terminator : false;
}

inline const char* getOpcodeName(QoreIROpcode op) {
    const OpcodeInfo* info = getOpcodeInfo(op);
    return info ? info->name : "<UNKNOWN>";
}

inline int getOpcodeExpectedOperands(QoreIROpcode op) {
    const OpcodeInfo* info = getOpcodeInfo(op);
    return info ? info->expected_operands : -1;
}
```

### Step 2b: Add Debug/Analysis Functions

Add to `lib/QoreOpcodeRegistry.cpp`:

```cpp
#include "qore/intern/QoreOpcodeRegistry.h"
#include <cstdio>

// Runtime validation function (useful for diagnostics)
void validateOpcodeRegistry() {
    for (int i = 0; i <= static_cast<int>(QoreIROpcode::Sprintf); i++) {
        const OpcodeInfo& info = OPCODE_REGISTRY[i];

        // Sanity checks
        if (info.opcode != static_cast<QoreIROpcode>(i)) {
            fprintf(stderr, "WARNING: Opcode registry mismatch at index %d\n", i);
        }

        // Logical consistency checks
        if (info.never_returns_nothing && info.can_return_nothing) {
            fprintf(stderr, "WARNING: Opcode %s (%d): "
                "cannot be both 'never returns nothing' and 'can return nothing'\n",
                info.name, i);
        }
    }
}

// Analysis function: print all opcodes with a property
void printOpcodesWithProperty(const char* property_name) {
    if (strcmp(property_name, "can_return_nothing") == 0) {
        printf("Opcodes that CAN return nothing:\n");
        for (const auto& info : OPCODE_REGISTRY) {
            if (info.can_return_nothing) {
                printf("  %3d: %s\n", static_cast<int>(info.opcode), info.name);
            }
        }
    }
    // ... similar for other properties
}
```

**Testing**:
- Compile and verify functions work
- Run `validateOpcodeRegistry()` at startup (can be conditional with env var)
- Test that queries return correct values

---

## Phase 3: Refactor Existing Property Functions (1.5-2 hours)

**Goal**: Make existing functions use the registry instead of switch statements.
**Files** (in order):
- `lib/QoreIRLowering.cpp`
- `lib/QoreIRVerifier.cpp`
- `lib/QoreIRPrinter.cpp`
- `include/qore/intern/QoreIR.h`

**Safety**: Keep old functions during transition, add new ones alongside

### Step 3a: Replace opcodeCanReturnNothing()

**Current** (in `lib/QoreIRLowering.cpp` ~line 3273):
```cpp
static bool opcodeCanReturnNothing(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::LoadLocal:
        case QoreIROpcode::LoadClosure:
        case QoreIROpcode::LoadGlobal:
        // ... 20+ cases
        return true;
        default:
            return false;
    }
}
```

**Replacement**:
```cpp
// In lib/QoreIRLowering.cpp, add:
#include "qore/intern/QoreOpcodeRegistry.h"

// OLD FUNCTION - for comparison during transition
static bool opcodeCanReturnNothing_OLD(QoreIROpcode op) {
    // ... keep old implementation
}

// NEW FUNCTION - uses registry
static bool opcodeCanReturnNothing(QoreIROpcode op) {
    return getOpcodeCanReturnNothing(op);
}
```

Then verify both functions return same result on all opcodes:
```cpp
// Add temporary verification at startup
#ifdef QORE_DEBUG_OPCODE_REGISTRY
void verifyOpcodeCanReturnNothing() {
    for (int i = 0; i <= 348; i++) {
        auto op = static_cast<QoreIROpcode>(i);
        bool old_result = opcodeCanReturnNothing_OLD(op);
        bool new_result = opcodeCanReturnNothing(op);
        if (old_result != new_result) {
            fprintf(stderr, "MISMATCH at opcode %d: old=%d, new=%d\n",
                i, old_result, new_result);
        }
    }
}
#endif
```

**Process**:
1. Replace `opcodeCanReturnNothing()` in QoreIRLowering.cpp
2. Replace `opcodeNeverReturnsNothing()` in QoreIRLowering.cpp
3. Replace `opcodeName()` in QoreIRPrinter.cpp
4. Replace `isTerminator()` in QoreIR.h
5. Add any missing opcode property functions from QoreIRVerifier.cpp
6. Compile and test thoroughly
7. Once verified, remove old implementations and verification code

**Testing**:
- Build with `-DQORE_DEBUG_OPCODE_REGISTRY`
- Run full test suite (should be identical results)
- Verify no regressions in IR/JIT smoke tests

### Step 3b: Update maybeInsertNotNothingGuard()

In `lib/QoreIRLowering.cpp` (~line 3616), the function uses `opcodeCanReturnNothing()`. Once that's migrated, it automatically uses the registry.

**Verification**: No code changes needed, just verify behavior is identical.

---

## Phase 4: Add Enhanced Properties & Tooling (1-1.5 hours)

**Goal**: Add new properties and create analysis tools.
**Files**:
- `include/qore/intern/QoreOpcodeRegistry.h` (MODIFY)
- `lib/QoreOpcodeRegistry.cpp` (MODIFY)
- `tools/opcode_analyzer.py` (NEW - optional)

### Step 4a: Add New Fields to OpcodeInfo

Extend struct to include:
```cpp
struct OpcodeInfo {
    QoreIROpcode opcode;
    const char* name;
    bool can_return_nothing;
    bool never_returns_nothing;
    bool is_terminator;
    int expected_operands;

    // NEW FIELDS
    const char* description;              //! What the opcode does
    bool may_have_side_effects;           //! Can modify global state
    bool may_throw_exception;             //! Can raise exceptions
    const char* corresponding_ast_node;   //! AST node this comes from
};
```

### Step 4b: Populate New Fields

For each opcode, add descriptions and properties:
```cpp
{ QoreIROpcode::LoadLocal,
    "LoadLocal",
    true, false, false, 2,
    "Load value from local variable",
    false,                  // no side effects
    false,                  // doesn't throw
    "VarRefNode"           // AST source
},
```

### Step 4c: Add Analysis Tools

Create `tools/analyze_opcodes.py`:
```python
#!/usr/bin/env python3
"""Analyze opcode registry for coverage and consistency."""

import re
import sys

def parse_registry(header_file):
    """Extract opcode entries from registry."""
    with open(header_file) as f:
        content = f.read()

    # Parse entries...
    entries = []
    for match in re.finditer(r'\{\s*QoreIROpcode::(\w+).*?\}', content, re.DOTALL):
        entries.append(match.group(1))

    return entries

def check_coverage(registry):
    """Find missing or inconsistent entries."""
    # Check for gaps
    # Check for logical inconsistencies
    # Report summary
    pass

if __name__ == "__main__":
    registry = parse_registry("include/qore/intern/QoreOpcodeRegistry.h")
    check_coverage(registry)
```

Add to build system to run analysis during build.

---

## Implementation Workflow

### Week 1: Phase 1 & 2

**Day 1**: Create `QoreOpcodeRegistry.h` with empty entries
**Day 2**: Populate all 349 opcodes with conservative defaults
**Day 3**: Implement lookup functions, test compilation

### Week 2: Phase 3

**Day 1**: Replace `opcodeCanReturnNothing()` with verification
**Day 2**: Replace remaining property functions
**Day 3**: Run comprehensive testing, debug mismatches
**Day 4**: Remove old implementations, clean up

### Week 3: Phase 4 (Optional)

**Day 1**: Add new properties and descriptions
**Day 2**: Implement analysis tools
**Day 3**: Documentation and integration

---

## Testing Strategy

### Unit Tests
```cpp
// Add to examples/test/ir/ as new file

#include "qore/intern/QoreOpcodeRegistry.h"

void test_opcode_registry() {
    // Verify count
    assert(OPCODE_REGISTRY.size() == 349);

    // Verify no gaps
    for (int i = 0; i < 349; i++) {
        assert(OPCODE_REGISTRY[i].opcode == static_cast<QoreIROpcode>(i));
    }

    // Verify consistency
    for (const auto& info : OPCODE_REGISTRY) {
        assert(!(info.never_returns_nothing && info.can_return_nothing));
        assert(!info.name || strlen(info.name) > 0);
    }

    // Verify all functions work
    for (const auto& info : OPCODE_REGISTRY) {
        assert(getOpcodeCanReturnNothing(info.opcode) == info.can_return_nothing);
        assert(getOpcodeName(info.opcode) == info.name);
    }
}
```

### Regression Tests
- Run full test suite on each phase
- IR smoke tests must pass: 24 TieredSmoke, 2 IRExecMode, 153 JITSmoke
- No performance degradation

### Validation
- `validateOpcodeRegistry()` called at startup (with QORE_DEBUG_OPCODE_REGISTRY)
- Comparison of old vs new function results
- Static assertions prevent incomplete registry

---

## Rollback Plan

At any phase, can rollback by:
1. Delete `QoreOpcodeRegistry.h` and `QoreOpcodeRegistry.cpp`
2. Remove `#include "qore/intern/QoreOpcodeRegistry.h"`
3. Restore original property functions from git

Phase 1-2: Zero risk (additive only)
Phase 3: Very low risk (old functions still present during verification)
Phase 4: Zero risk (additive only)

---

## Benefits Summary

| Benefit | Impact |
|---------|--------|
| Single source of truth | Eliminates 6+ switch statements |
| Compile-time verification | Catches missing opcodes at build time |
| No silent bugs | Static assert + validation function |
| Easier to extend | Just add field to struct |
| Better debugging | All properties visible together |
| Enables tooling | Can analyze opcode coverage |
| Future-proof | Easy to add new properties without code changes |

---

## Expected Outcomes

- **Before**: Adding an opcode requires touching 6-8 files, easy to forget properties
- **After**: Adding an opcode requires:
  1. One enum entry in QoreIR.h
  2. One registry entry in QoreOpcodeRegistry.h
  3. Static assert catches any omission

- **Result**: Fewer bugs, faster development, better maintainability

---

## Questions for Review

1. Should we add a `corresponding_ast_node` field? (helps with tracing AST→IR lowering)
2. Should we include operand type information? (e.g., "operand 0 is the value to store")
3. Should we pre-populate all 349 entries, or do it piecemeal as we touch each opcode?
4. Should we create a code generator to auto-generate the registry from AST patterns?

---

## Appendix: Quick Reference for Conservative Defaults

When populating the registry, if you're unsure:

```
LOAD opcodes (LoadLocal, LoadClosure, LoadGlobal, LoadThreadLocal)
  → can_return_nothing: true
  → never_returns_nothing: false
  → is_terminator: false

STORE opcodes (StoreLocal, StoreClosure, StoreGlobal, StoreThreadLocal)
  → can_return_nothing: false
  → never_returns_nothing: true
  → is_terminator: false

CONST opcodes (ConstInt, ConstFloat, ConstString, etc.)
  → can_return_nothing: false (except ConstNothing=true)
  → never_returns_nothing: true
  → is_terminator: false

CALL opcodes (Call, CallMethod, CallStatic, etc.)
  → can_return_nothing: true
  → never_returns_nothing: false
  → is_terminator: false

CONTROL FLOW (Jump, CondJump, Return, Throw)
  → can_return_nothing: false
  → never_returns_nothing: false
  → is_terminator: true

HASH/LIST ACCESS (HashKeyAccess, ListIndexAccess, etc.)
  → can_return_nothing: true
  → never_returns_nothing: false
  → is_terminator: false

ARITHMETIC (AddInt, SubInt, MulInt, etc.)
  → can_return_nothing: false
  → never_returns_nothing: true
  → is_terminator: false

UNKNOWN/CUSTOM
  → can_return_nothing: false (conservative)
  → never_returns_nothing: false (conservative)
  → is_terminator: false (conservative)
```
