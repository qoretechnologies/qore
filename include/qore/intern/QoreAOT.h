/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOT.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QOREAOT_H
#define _QORE_QOREAOT_H

#include <cstdint>
#include <string>

class ExceptionSink;
class QoreProgram;

//! Descriptor for a pre-compiled AOT function
struct QoreAOTFunc {
    const char* name;                           //!< unique function identifier (from IR lowering)
    uint64_t (*fn_ptr)(ExceptionSink*);         //!< pre-compiled native code pointer
};

//! C ABI entry point called by AOT-compiled binaries from their generated main()
/** Initializes the Qore runtime, re-parses the embedded source to build the AST/type system,
    registers pre-compiled function pointers, and runs the program.
    Functions without pre-compiled pointers fall back to runtime JIT compilation.

    @param argc command-line argument count
    @param argv command-line argument vector
    @param source embedded Qore source text
    @param source_len length of source text in bytes
    @param label label for the source (e.g. script filename)
    @param parse_options parse options used during original compilation
    @param functions array of pre-compiled function descriptors
    @param num_functions number of entries in functions array
    @return exit code (0 = success)
*/
extern "C" int qore_aot_run(
    int argc, char** argv,
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const QoreAOTFunc* functions, int num_functions
);

//! AOT compiler class — compiles a parsed QoreProgram to a standalone executable
class QoreAOT {
public:
    //! Compile a parsed program to a standalone executable
    /** @param pgm parsed QoreProgram (must have been parsed successfully)
        @param source_text original source text to embed in the binary
        @param source_len length of source text
        @param label source label (filename)
        @param output_path path for the output executable
        @param parse_options parse options to embed
        @param error error message on failure
        @return true on success, false on failure
    */
    static bool compile(QoreProgram* pgm,
                       const char* source_text, int source_len,
                       const char* label,
                       const std::string& output_path,
                       int64_t parse_options,
                       std::string& error);
};

#endif
