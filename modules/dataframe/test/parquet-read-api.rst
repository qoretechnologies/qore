Parquet SDK compatibility regression
====================================

Copyright 2026 Qore Technologies, s.r.o.

``parquet-read-api.qtest`` exercises all four Parquet reads: the entire table,
projected columns, selected row groups, and projected row groups.  The test
checks column and row-group ordering, null values, empty results after pruning,
damaged page headers, and successful reads after a failure.  The footer is
preserved in damaged files so the reader opens before decoding fails.

Run with the local development module and runtime::

    LD_LIBRARY_PATH=build \
      QORE_MODULE_DIR=build/modules/dataframe:build/modules/reflection:qlib \
      build/qore -b --enable-debug modules/dataframe/test/parquet-read-api.qtest

Replace ``build`` with ``build-debug`` to test a Debug build.  Run the same
command under Valgrind after C++ changes.  The RPM specification runs this
regression in its mandatory test section.

Arrow 24 and later provide Result-returning read overloads.  Qore checks the
status before moving the table from the Result; errors retain the
``DATAFRAME-IO-ERROR`` exception and the Arrow detail.  Builds using older
Arrow versions retain the supported output-parameter overloads.  Qualification
covers Arrow 17, 23 and 25, with the original source also checked against Arrow
25 using ``-Werror=deprecated-declarations`` to demonstrate the four obsolete
calls this change removes.
