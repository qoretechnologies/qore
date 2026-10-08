Ignored QPP parameter regression
================================

Copyright 2026 Qore Technologies, s.r.o.

QPP's ``[doc]`` parameter annotation retains an argument in its public
signature, metadata, named-argument registration and documentation, while
omitting the unused C++ binding.  The eight parameters covered here are
intentionally unused: three abstract bulk-load methods always reject calls,
the AsyncIoController constructor always rejects direct construction, and
the string/binary Base64-URL methods accept an ignored line-length argument
for compatibility.  Their signatures and behavior are unchanged.

Run the generator and native checks against a configured Qore build::

    QORE_TEST_VALGRIND=1 python3 -B -W error \
      examples/test/cmake/test_ignored_qpp_parameters.py -v

The default is ``build/``.  For Debug qualification, set
``QORE_TEST_BUILD_DIR`` to the absolute ``build-debug/`` path and
``QORE_TEST_CXXFLAGS='-Og -g -DDEBUG'``.  ``QORE_QPP_EXECUTABLE`` selects qpp;
``QORE_TEST_INCLUDE_DIR`` selects the matching Qore public headers.
``QORE_TEST_OUTPUT_DIR`` retains generated files, command receipts and logs.
The default native flags are ``-O3 -DNDEBUG -g``; distribution qualification
passes the actual package flags through ``QORE_TEST_CXXFLAGS``.  Both modes
add ``-Wall -Werror`` for the affected wrappers.  The RPM test section runs
this regression with the distribution compiler flags.

The regression processes the four actual QPP source files, and also restores
only the six original declarations to form a negative compiler control.
It compares complete metadata and stub outputs, runtime method registration,
and documentation.  The original wrappers must report exactly eight unused
variable errors; the corrected wrappers must compile without diagnostics.

Native checks compile the unchanged generated bodies and the original string
argument helper.  They exercise abstract bulk-load rejection, both boolean
values for bulk-load completion and direct controller construction, and
string/binary Base64-URL results across six ignored line lengths.  Data cover
empty input, encoding boundaries, embedded NUL, non-ASCII bytes and UTF-8.
Every returned string and exception is released.  Qore signal handling is
disabled for Valgrind.

The datasource private pointer is forward-declared because these rejecting
bodies do not dereference it.  No replacement method implementation or fake
runtime is used.  This focused test does not instantiate a database driver
or qualify the full async controller.  Public runtime argument validation is
preserved by the byte-identical registration and metadata; final complete
RPM and installed runtime qualification remain separate gates.
