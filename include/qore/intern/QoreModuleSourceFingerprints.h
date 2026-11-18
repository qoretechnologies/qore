/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreModuleSourceFingerprints.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREMODULESOURCEFINGERPRINTS_H
#define _QORE_INTERN_QOREMODULESOURCEFINGERPRINTS_H

/** @file QoreModuleSourceFingerprints.h
    The source fingerprints of an AOT module

    qcc writes the dependency file of a module it compiles (<tt>\<module\>.d</tt>, with \c --depfile) for make, and
    next to it the fingerprint of each file that the dependency file lists, in
    <tt>\<module\>.d</tt>@ref QORE_AOT_MODULE_FINGERPRINT_SUFFIX: its size, modification time and content hash at the
    time of the compile.  The module loader uses them to decide whether an AOT module that a build tree links into a
    source tree is older than its sources by their content, as a checkout, an artifact copy or a package extraction
    gives files new modification times without changing them (see design/aot-linked-module-sources.md).

    The file is text: a header line, then one line per source,
    <tt>\<size\> \<mtime_ns\> \<xxh64\> \<path\></tt>, with the size and the modification time in nanoseconds in
    decimal, the XXH64 hash of the content in 16 hexadecimal digits, and the path last, as it can contain spaces; a
    path with a line break is not recorded.
*/

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class ExceptionSink;

//! the suffix of the source fingerprint file, after the name of the dependency file of the module
#define QORE_AOT_MODULE_FINGERPRINT_SUFFIX ".fingerprints"

//! the first line of a source fingerprint file
#define QORE_AOT_MODULE_FINGERPRINT_HEADER "# qore AOT module source fingerprints 1"

//! the recorded fingerprint of a source of an AOT module
struct QoreAOTModuleSourceFingerprint {
    //! the size of the file in bytes
    uint64_t size = 0;
    //! the modification time of the file in nanoseconds since the Unix epoch
    uint64_t mtime_ns = 0;
    //! the XXH64 hash of the content of the file (seed 0)
    uint64_t hash = 0;
};

//! returns the XXH64 hash (seed 0) of the content of a file, read in blocks
/** @param path the file
    @param hash receives the hash
    @param xsink for cancellation (checked every 100 blocks); may be nullptr

    @return false if the file cannot be read or the thread was cancelled
*/
DLLEXPORT bool qore_aot_file_content_hash(const char* path, uint64_t& hash, ExceptionSink* xsink);

//! writes the source fingerprint file of an AOT module next to its dependency file
/** The fingerprint file is replaced atomically; a source that cannot be read, or whose path has a line break, is
    not recorded, so the loader compares its modification time with that of the module as without a fingerprint file.

    @param depfile the path of the dependency file; the fingerprint file is this followed by
    @ref QORE_AOT_MODULE_FINGERPRINT_SUFFIX
    @param deps the files that the dependency file lists
    @param error receives the reason on failure

    @return true if the file was written
*/
DLLEXPORT bool qore_aot_write_module_source_fingerprints(const std::string& depfile,
        const std::vector<std::string>& deps, std::string& error);

//! reads a source fingerprint file
/** @param path the fingerprint file
    @param fingerprints receives the fingerprints by path

    @return false if the file cannot be read, does not start with @ref QORE_AOT_MODULE_FINGERPRINT_HEADER or has a
    malformed line
*/
DLLEXPORT bool qore_aot_read_module_source_fingerprints(const std::string& path,
        std::map<std::string, QoreAOTModuleSourceFingerprint>& fingerprints);

//! reads the dependency file that qcc wrote next to an AOT module (<tt>\<module\>.d</tt>)
/** The dependency file is the one next to the file that \a qmod resolves to, and it is used only if the target of its
    rule is that module (see design/aot-linked-module-sources.md)

    @param qmod the module
    @param deps receives the prerequisites of the rule

    @return false if the module has no such dependency file (always on Windows)
*/
DLLEXPORT bool qore_aot_read_module_depfile(const std::string& qmod, std::vector<std::string>& deps);

//! the result of qore_aot_check_module_sources()
enum class QoreAOTModuleCheck {
    //! the module is current: the loader would not find it out of date
    CURRENT,
    //! the module is out of date
    STALE,
    //! the module cannot be checked
    ERROR,
};

//! checks an AOT module of a build tree as the module loader checks it, for the build (qcc --check-module-sources,
//! qore#6598)
/** The build compares the modification times of the dependencies of an AOT module with its own, and the loader
    their contents (qore_build_aot_module_is_stale()): a dependency replaced by a file with another content and an
    older modification time (an install that keeps the time of the build: cp -p, cmake --install) makes the loader
    find the module out of date, and the build does not rebuild it.  This check makes the loader's comparison for the
    build:
    - each file of the module's dependency file (<tt>\<module\>.d</tt>) is compared with its fingerprint (<tt>.d
      .fingerprints</tt>): another size, or the same size and another content hash when its modification time
      differs, makes the module out of date; a file without a fingerprint makes it out of date if it is newer than
      the module, and a file that no longer exists makes it out of date;
    - for a module with failed %try-module directives (a non-empty <tt>\<module\>.d.optional</tt>), the checks of
      the loader for the optional modules, with the module path of the process (that of the compile): a candidate
      file of one of them that the dependency file does not list, and a module that can be loaded now
      (QoreModuleManager::findAvailableOptionalModule()).

    Without a dependency file, which Ninja removes once it has read it into its log, the files of the fingerprint
    file are compared.

    Unlike the loader, the check fails closed: a module whose dependency file or fingerprint file cannot be read or
    does not describe it, or a dependency that cannot be read, cannot be checked.

    Internal: for qcc only.

    @param qmod the AOT module, which exists
    @param reason set to the reason when the module is out of date
    @param dependency set to the dependency concerned, if any, when the module is out of date or cannot be checked
    @param error set to the error when the module cannot be checked
*/
DLLEXPORT QoreAOTModuleCheck qore_aot_check_module_sources(const char* qmod, std::string& reason,
        std::string& dependency, std::string& error);

#endif
