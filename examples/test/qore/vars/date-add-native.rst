DateTime C++ addition regression
===============================

Copyright 2026 Qore Technologies, s.r.o.

The native test calls both exported ``DateTime::add()`` overloads. Integer
microsecond expectations cover all four absolute/relative combinations,
negative and zero epochs, positive and negative durations, microsecond carry,
calendar years/months/days, leap days, fixed timezone offsets, both daylight-saving
transitions and self-aliasing.
Each case checks result ownership and that neither source changed. The test
also runs through ``date-add-native.qtest`` in normal build-tree test runs.
The Qore regression checks equivalent mixed-sign durations, including opposite
signs separated by zero minutes or seconds, and subtraction back to zero.

With Qore installed under ``/usr``, build and run without installing::

    cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build-debug --target qore qore-date-add-test -j4
    LD_LIBRARY_PATH=build-debug build-debug/qore-date-add-test
    LD_LIBRARY_PATH=build-debug valgrind --error-exitcode=99 --leak-check=full \
        --show-leak-kinds=all --errors-for-leak-kinds=definite,indirect,possible \
        build-debug/qore-date-add-test

The native executable disables Qore signal handling itself. The Qore wrapper
should be run with ``-b --enable-debug`` and the usual local module paths.
Use ``QORE_DATE_ADD_TEST`` to select a native executable outside the Qore
binary's directory. Match the installation prefix if Qore is installed elsewhere.
Memory checks require zero lost allocations and invalid accesses. Still-reachable
library caches remain visible in the report, as in the existing core qualification.
