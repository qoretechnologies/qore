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

#endif
