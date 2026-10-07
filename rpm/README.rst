RPM packaging
=============

MongoDB builds accept either the 1.x or 2.x system C driver, matching CMake's
existing API detection. This also covers EPEL's transition to 2.x during EL10.

Copyright 2026 Qore Technologies, s.r.o.

``qore.spec-multi`` is the canonical portable spec. ``qore.spec-fedora`` and
``qore.spec-opensuse`` are aliases. Source preparation and repository
qualification are coordinated by ``qoretechnologies/qore-packaging``.

The package split separates the interpreter, SONAME library, architecture-
dependent standard library, SDK, RPM build helpers, command-line tools,
debugger tools, and documentation. ONNX support is mandatory for packaged ML.
The SDK requires the LLVM major used to build its public plugin headers.
On openSUSE, the build and SDK depend on the versioned ``llvm19-devel`` package
rather than the unversioned default selector. This keeps SDK headers matched
when a repository also supplies a newer compiler for another dependency.
The runtime also declares minimum HTTP/2, QUIC, HTTP/3 and tree-sitter package
versions: their SONAME dependencies alone cannot enforce API additions or the
qualified patch level within an ABI series.
The runtime requires glibc's extra character conversion modules, which iconv
loads dynamically. This keeps legacy encodings available on minimal systems;
the installed tests verify a non-ASCII ISO-8859-2 round trip.

``qore-rpm-macros`` supplies the module build environment and automatic RPM
requirements. Native Qore modules deliberately leave libqore symbols unresolved;
the generator therefore reads the installed SDK's exact runtime dependency
and emits the corresponding minimum epoch/version/release and module ABI.
It runs during binary packaging, never while preparing an SRPM.
The helpers require Python 3.11 or later; their RPM dependency enforces this
before module builds begin.

``%qore_enable_aot_post`` preserves AOT EOF metadata around the distribution's
complete post-install pipeline, including find-debuginfo and stripping.
Executable permissions on ELF qmods let find-debuginfo produce normal separate
debug files. Unsupported dwz processing is disabled for AOT packages, and
LLVM's existing ``.debug_names`` index is retained without asking GDB to
generate a second index.

Run helper checks with Python, GCC, binutils, cpio and rpm-build installed::

    python3 -B -W error -m unittest discover -s rpm/tests -v

Call ``qore_set_source_prefix_maps "%{qore_debug_source_dir}"`` after sourcing
the build environment in ``%build``. It passes a quoted file-prefix map through
the native compiler, qpp and qcc. The destination matches RPM's debug-source
directory, so paths are independent of the build root and debugedit still
collects the corresponding sources. The RPM integration test builds in two
different roots and verifies identical installed ELF files, retained AOT
trailers, separate debug information and packaged source files.

LLVM emits synthetic ``<aot>`` compilation units for Qore modules, so RPM's
``debugedit`` does not enumerate every Qore input file. Call
``%qore_install_aot_sources qlib`` in ``%install`` to include those inputs in
the same debug-source directory. The helper preserves their paths and
timestamps, excludes generated binaries, and rejects paths outside the source
tree or build root.
The core package supplements its debug-source tree after RPM's automatic
extraction. This preserves inputs absent from DWARF without asking cpio to
copy files over an already populated tree. A real RPM regression checks both
source categories and rejects copy diagnostics.

The integration test creates and extracts real runtime and debuginfo RPMs and
checks both the retained metadata and separated ELF debug sections. Run it in
each supported distro; parsing one distro's macros on a different host is not
equivalent.

With the SDK under qualification on the library and module search paths, run
the real qcc integration check explicitly::

    QORE_TEST_QCC=$PWD/build/qcc QORE_TEST_QORE=$PWD/build/qore \
        python3 -B -W error rpm/tests/qcc-rpm-integration.py -v

This builds two RPMs in different directories, compares their installed AOT
modules byte for byte, loads them, and verifies their metadata, separate debug
information and original Qore source files. The core RPM runs it in ``%check``.

``tests-installed/`` must run in clean containers with installed RPMs, from a
temporary directory outside the source checkout. Set ``QORE_RPM_TEST_TMP`` to
a fresh directory for each script. These tests cover module loading, ONNX
inference, CMake/pkg-config SDK use, qcc, metadata extraction and tool startup.
Install ``findutils`` in the test container for the runtime script's AOT file
inventory. This is a test dependency; the runtime RPM does not require it.

Set ``QORE_RPM_VERIFY_INSTALLED_DEPS=1`` when running the helper tests in a
target build dependency image. This additionally resolves the emitted parser
runtime requirement against the real RPM database and compares its version
with RPM semantics. Fedora uses ``libtree-sitter``; Enterprise Linux uses the
``tree-sitter`` backport, and openSUSE uses ``libtree-sitter0_26``.

Documentation builds declare the distribution package providing ``hardlink``
(``util-linux-core`` on Fedora/EL, ``util-linux`` on openSUSE), so OBS can resolve
it without file-provider metadata. The dependency check verifies this against
the target RPM database. OCR also declares libcurl and libarchive SDKs because
some Tesseract pkg-config files expose these libraries directly.
HTML documentation is built with ``QORE_GENERATE_JAVA_BINDINGS=OFF`` because
these RPMs do not distribute generated Java bindings or require JNI.
The portable ML build requires ONNX Runtime CPU inference and disables CUDA
toolkit discovery. An undeclared workstation CUDA SDK must not change RPM
dependencies; accelerator builds require separate dependencies and qualification.

Fedora file post-processing
---------------------------

Fedora builds require the qore-packaging backport capabilities for add-determinism
and linkdupes. The fixes exclude live temporary output from parallel directory
walks and bound descriptor use while comparing large documentation trees. The
standard RPM normalization, SELinux checks, stripping and debug packages remain
enabled. These tools are build dependencies only; end-user installations do not
need them. Their offline source pins, regressions and backport rationale are in
qore-packaging/dependencies/add-determinism.rst.

Documentation package contents
------------------------------

The documentation RPM includes user examples with the packaged Qore interpreter
in their shebangs. Development regression tests and Git metadata remain in the
source distribution. License data contains the complete leading notices from
linenoise and wcwidth, without implementation source. Build environment helpers
are sourced shell fragments, so they have neither execute permission nor a
shebang. Snapshot compatibility capabilities obsolete only older versions.

The SDK provides ``qore-devel(module-doc-peers) = 1`` when the installed CMake
helpers support final documentation passes for both native and peer user
modules. For example, AMQP's AmqpUtil and AmqpDataProvider pages can link to
each other without imposing circular compile dependencies. SDK assets are
resolved beside the installed template, and imported indexes resolve explicit
links without adding unrelated pages to a module's navigation.

This RPM backport retains the existing external-module installation manifest;
it does not enable the separate upstream change that automatically installs
native module tag files. Modules may continue to request tag installation
explicitly with ``qore_install_module_doxygen_tag()``.

Scanner debug-source regression
------------------------------

The Flex input uses an absolute source path so its generated line directives
remain correct when the C++ compiler runs in the build directory. With CMake,
Flex, a C++ compiler, debugedit and Valgrind installed, run::

    python3 -B -W error rpm/tests/test_flex_source_paths.py -v

The test builds in a path containing spaces, verifies all extracted DWARF
source paths exist, checks empty, numeric, word and UTF-8 input, and exercises
scanner teardown under Valgrind. It requires no Qore installation.
