#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
#
# Minimal test case for hash literal type degradation issue
# Run with: qore test_hash_literal_type_regression.q
#

%modern
%no-child-restrictions

class TestClass {
    TestClass() {}
}

sub test_simple_hash_literal {
    # Test 1: Simple hash literal - should work
    printf("Test 1: Simple hash literal with object values...\n");
    try {
        hash<string, object<TestClass>> h = {
            "key1": new TestClass(),
            "key2": new TestClass()
        };
        printf("  SUCCESS: type = %s\n", type(h));
    } catch (hash<ExceptionInfo> ex) {
        printf("  FAILED: %s\n", ex.desc);
    }
}

sub test_map_with_hash_literal {
    # Test 2: Map operator returning hash literal - THIS FAILS
    printf("\nTest 2: Map operator returning hash literal with objects...\n");
    try {
        hash<string, object<TestClass>> result =
            map {$1.key: new TestClass()},
            ({"key": "a"}).pairIterator();
        printf("  SUCCESS: type = %s\n", type(result));
    } catch (hash<ExceptionInfo> ex) {
        printf("  FAILED: %s\n", ex.desc);
    }
}

sub test_map_with_simple_values {
    # Test 3: Map operator with simple values - should work
    printf("\nTest 3: Map operator with simple string values...\n");
    try {
        hash<string, string> result =
            map {$1.key: $1.value},
            ({"key": "a", "value": "test"}).pairIterator();
        printf("  SUCCESS: type = %s\n", type(result));
    } catch (hash<ExceptionInfo> ex) {
        printf("  FAILED: %s\n", ex.desc);
    }
}

sub test_explicit_assignment {
    # Test 4: Explicit assignment of map result - shows the type issue
    printf("\nTest 4: Explicit assignment of map result to typed hash variable...\n");
    try {
        auto result = map {$1.key: new TestClass()},
            ({"key": "a"}).pairIterator();
        printf("  Map result type: %s\n", type(result));

        # This should fail if result is 'hash' instead of 'hash<string, object<TestClass>>'
        hash<string, object<TestClass>> typed = result;
        printf("  SUCCESS: Assignment to typed hash variable worked\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("  FAILED: %s\n", ex.desc);
    }
}

# Run all tests
test_simple_hash_literal();
test_map_with_hash_literal();
test_map_with_simple_values();
test_explicit_assignment();

printf("\nDone.\n");
