BSON array conversion regression
================================

Copyright 2026 Qore Technologies, s.r.o.

The native regression compiles the actual converter and generated ObjectId
translation unit.  It links the local Qore runtime and the selected libbson;
it does not replace the converter or require a running MongoDB server.

Build and run it with the configured local runtime::

    cmake --build build --target qore-mongodb-bson-array-test -j2
    LD_LIBRARY_PATH=build build/modules/mongodb/qore-mongodb-bson-array-test
    LD_LIBRARY_PATH=build valgrind --error-exitcode=99 --leak-check=full \
      --show-leak-kinds=definite,indirect,possible \
      --errors-for-leak-kinds=definite,indirect,possible \
      build/modules/mongodb/qore-mongodb-bson-array-test

Use ``build-debug`` in place of ``build`` for Debug qualification.  The test
initializes Qore with signal handling disabled.  The RPM specification builds
and runs this target in its mandatory test section.

Coverage includes empty and single-element arrays, numeric-key boundaries
through 1,005 elements, BSON type validation and round trips, nulls at both
ends, UTF-8 strings, nested arrays/documents, and invalid UTF-16 after both
small and buffer-growing partial arrays.  Cancellation checks cover zero,
one, 99, 100, 101 and 1,005 elements, cleanup deferral, preserved pending
requests and recovery.  A sandbox control exercises program interruption.

``bson_append_array_unsafe_begin`` is the supported name of the old array
begin function starting with libbson 2.3.  Qore supplies every contiguous key
it requires, including keys for null values.  Older drivers retain the original
API.  Entry cancellation returns before altering the parent BSON document;
periodic checks limit uninterrupted array conversion to 100 elements.  As with
other conversion failures, callers discard partially written documents.

The test target compiles against the public Qore API, as the mongodb module
does.  It links the runtime file with an explicit build dependency, without
inheriting libqore's internal-only compilation definitions.
