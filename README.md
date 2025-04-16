# Qore

![Qore logo](https://github.com/qorelanguage/qore/raw/develop/doxygen/qore-logo-55x200.png)

## Files
This document contains some general information on the Qore language.  Please
refer to the following files/locations for specific information:

### ABOUT:
  General description of the Qore programming language.

### BUILDING:
  Information on how to build and install qore from sources - some quick
  information below.

### README-LICENSE:
  Read about Qore's open-source software licenses.

### README-GIT:
  Learn how to build qore from git sources.

### README-MODULES:
  Information about qore modules (delivered separately from the qore library).

### RELEASE-NOTES:
  Release notes, known issues, etc - however see the following URL for full
  and up-to-date release notes online:
  	  https://docs.qore.org/current/lang/html/release_notes.html

### docs/lang/html:
  Qore reference manual (built during the build process if you have doxygen).

### docs/library/html/index.html:
  API documentation for qore's public API (built during the build process if
  you have doxygen).

### examples/:
  Many example Qore scripts/programs.

### examples/test/:
  Qore test scripts. Use `run_tests.sh` script to run all the tests.


## Quick Build Info

 * only UNIX-like platforms are currently supported (although Windows binaries
   can be built with mxe, and theoretically qore could be built directly on a
   Windows host)
 * requires POSIX threading support
 * requires at least flex 2.5.31 (older distributions have flex 2.5.4, flex
   2.5.37 recommended) in order to compile the multithreaded parser; you can
   get this version at:
      http://sourceforge.net/projects/flex
 * requires pcre2 headers and libraries for perl5-compatible regex support;
   POSIX regex support is no longer used
 * requires openssl headers and libraries
 * requires zlib & bzlib headers and libraries
 * requires mpfr (and gmp) for the arbitrary-precision numeric support
 * optionally support for XML, Oracle, MySQL, PostgreSQL, Sybase, MS SQL
   Server, SSH2 and more can be built (see README-MODULES for details)
 * XML support has been removed as of Qore 0.8.1+; use the "xml" module
   instead


## History

Qore was originally designed to facilitate embedding integration logic in a
workflow/technical order management system (the system is called Qorus
Integration Engine).

The initial requirements for the language were: clean threading model, SMP
scalability, efficient resource sharing, safe embedding of logic in autonomous
objects with restricted capabilities, good networking and lightweight (ex
xml-rpc, json-rpc) web-service and other common protocol support, system
stability and memory cleanliness.

However, while qore was originally designed as an embedded application
scripting library (and still excels at this task), it has evolved to be a
fully-functional standalone language as well.
