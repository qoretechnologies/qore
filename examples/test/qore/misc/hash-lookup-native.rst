Native hash lookup regression
=============================

Copyright 2026 Qore Technologies, s.r.o.

The native fixture checks both exception-aware QoreString lookup overloads
against UTF-8 keys using UTF-8, Latin-1, UTF-16LE and UTF-16BE input. It includes
empty and missing keys, values equal to NOTHING, UTF-16 byte-prefix collisions,
invalid encoding input, invalid hashdecl members and successful lookup after
errors. Both initial values of the existence output are tested.

Build and run from the source root, using the installed Qore prefix::

    cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build-debug --target qore qore-hash-lookup-test -j2
    LD_LIBRARY_PATH=build-debug build-debug/qore-hash-lookup-test
    LD_LIBRARY_PATH=build-debug valgrind --error-exitcode=99 --leak-check=full \
        --show-leak-kinds=all build-debug/qore-hash-lookup-test
    LD_LIBRARY_PATH=build-debug QORE_BINARY="$PWD/build-debug/qore" \
        build-debug/qore -b --enable-debug examples/test/qore/misc/hash-lookup-native.qtest

Use build/ with CMAKE_BUILD_TYPE=Release for optimized qualification. The native
fixture disables Qore's signal handling itself. Its optional encoded, conversion
and declaration arguments run the three groups separately. The conversion and
declaration groups reproduce unset error outputs before this fix; the encoded
group also guards the key-conversion behavior fixed in commit 9f94a962f. The fixture is built by default for source
tests and is not installed in the runtime package.
