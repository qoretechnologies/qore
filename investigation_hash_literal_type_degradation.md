# Investigation: Hash Literal Type Degradation in Map Operators

**Created:** 2026-03-13
**Status:** OPEN
**Priority:** HIGH - Blocks Qorus DataProvider module loading

## Problem Statement

Hash literals used as values in map operators are losing their complex type information and degrading to generic `hash` instead of preserving `hash<string, object<T>>`.

### Example Error

```
PARSE-TYPE-ERROR: lvalue for assignment operator '=' expects
hash<string, object<DataProviderTypeEntry>> or no value (NOTHING),
but right-hand side is type 'hash'
```

### Affected Code Pattern

In `/home/david/src/Qorus/current/qlib/DataProvider/DataProviderTypeEntry.qc:485-486`:

```qore
type_children = map {$1.key: new DataProviderTypeEntry(path, $1.key, $1.value.getType())},
    fields.pairIterator();
```

The hash literal `{$1.key: new DataProviderTypeEntry(...)}` should return type `hash<string, object<DataProviderTypeEntry>>` but returns generic `hash`.

## Root Cause Analysis

### How Hash Map Operator Type Inference Works

1. **parseInitImpl** (QoreHashMapOperatorNode.cpp:79-110) parses three sub-expressions:
   - Iterator: `fields.pairIterator()` → type `pairiterator`
   - Key: `$1.key` → type `string` (auto from iterator)
   - Value: `new DataProviderTypeEntry(...)` → should be `object<DataProviderTypeEntry>`

2. **setReturnTypeInfo** (QoreHashMapOperatorNode.cpp:50-77) synthesizes return type:
   ```cpp
   if (QoreTypeInfo::hasType(expTypeInfo2)) {
       returnTypeInfo = qore_get_complex_hash_type(expTypeInfo2);
       // ...
   } else {
       returnTypeInfo = hashTypeInfo;  // ← DEFAULTS TO GENERIC HASH
   }
   ```

### The Issue

**`expTypeInfo2` is null or doesn't have type information**, so the function defaults to generic `hashTypeInfo` instead of synthesizing `hash<string, object<DataProviderTypeEntry>>`.

`expTypeInfo2` is supposed to be the type of the hash value expression `new DataProviderTypeEntry(...)`, which should be `object<DataProviderTypeEntry>`.

## Investigation Steps

### 1. Reproduce with Minimal Test Case

```qore
%modern

class TestClass {
    TestClass(int x) {}
}

# This should be hash<string, object<TestClass>> but is hash
hash<string, object<TestClass>> result =
    map {$1.key: new TestClass($1.value)},
    ({"key": "a", "value": 1}).pairIterator();
```

### 2. Type Inference Sequence to Trace

1. How is `new DataProviderTypeEntry(...)` typed?
   - Should be: `object<DataProviderTypeEntry>`
   - Check: QoreNewOperatorNode or similar

2. How is the hash literal `{...}` typed?
   - Should infer type from key (`string`) and value (`object<T>`)
   - Check: Hash literal node type inference

3. Why doesn't `expTypeInfo2` have the complex type?
   - Is it null? Missing? Generic?
   - Check: parseInitImpl value expression type assignment

### 3. Related Code Areas

- **QoreHashMapOperatorNode.cpp** - Hash map operator parsing and type synthesis
- **Hash literal type inference** - How hash literals get typed with complex types
- **Constructor call type inference** - How `new ClassName(...)` gets typed
- **Type folding** - How `matchCommonType()` or similar might degrade types

### 4. Historical Context

Recent commits show attempts to fix this:

```
ea96c50f2 - prevent optional modifiers from being applied to hash/list literal complexTypeInfo
77b0f96dd - preserve parse-time analysis tracking in map operator
12e4e3bff - resolve map operator parse-time type inference in nested helper scopes
d4e3dd130 - use base complex type in hash/list literal value folding (reverted)
615fe4cf8 - revert base complex type fix
96cd1c9a7 - revert reapply of above
```

These reversions suggest type folding and analysis helper interactions are complex.

## Suspect Areas

1. **Hash literal type construction**
   - Check how hash literals determine their complex type from key/value types
   - May need special handling for constructor calls as values

2. **Type folding in nested contexts**
   - Map operator creates implicit argument context (`$1`)
   - Type inference in this context may be affected
   - Previous analysis helper changes caused issues

3. **Optional type modifiers**
   - Commit ea96c50f2 suggests issues with optional modifiers
   - Check if optional types are bleeding into non-optional contexts

## Test Cases to Create

### Test 1: Hash Literal Type Preservation

```qore
# Test that hash literals preserve complex types
hash<string, object<MyClass>> h1 = {"a": new MyClass()};
assertTrue(type(h1) == "hash<string, object<MyClass>>");
```

### Test 2: Map Operator with Hash Literal

```qore
# Test that map returning hash literal preserves complex type
hash<string, object<MyClass>> h2 =
    map {$1.key: new MyClass()},
    ({"key": "a"}).pairIterator();
assertTrue(type(h2) == "hash<string, object<MyClass>>");
```

### Test 3: Nested Type Expressions

```qore
# Test complex nested types
hash<string, object<Container<MyClass>>> h3 =
    map {$1: new Container(new MyClass())},
    ("a", "b").iterator();
```

## Next Steps

1. ✅ Create minimal reproduction test case
2. [ ] Trace `new DataProviderTypeEntry(...)` type inference
3. [ ] Verify hash literal type construction
4. [ ] Check `expTypeInfo2` capture in parseInitImpl
5. [ ] Identify type folding/degradation point
6. [ ] Implement fix while preserving previous fixes
7. [ ] Verify fix doesn't break analysis tracking
8. [ ] Test against Qorus DataProvider module
9. [ ] Add regression tests

## Notes

- This is NOT related to iterator handling (fixed separately)
- This is NOT related to analysis helpers (reverted to simpler approach)
- This is a pure type inference/folding issue
- Requires careful changes to avoid breaking previous fixes
- May need to review how other operators handle complex type synthesis
