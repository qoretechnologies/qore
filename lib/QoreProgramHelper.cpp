/*
    QoreProgramHelper.cpp

    QoreProgramHelper QoreObject Definition

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_aot_deps.h"

//! Creates a Program with the given options, or raises OPTION-ERROR if they have %modern and a compatibility option
//! that it does not allow (issue #5682)
/** The caller checks the exception before it parses or runs any code in the Program; the Program created then has
    none of the conflicting options
*/
static QoreProgram* qore_new_checked_program(const QoreParseOptions& parse_options, ExceptionSink& xsink) {
    std::string error;
    if (!qore_check_modern_parse_options(parse_options, error)) {
        static const char prefix[] = "PARSE-OPTION-ERROR: ";
        const char* desc = error.c_str();
        if (!error.compare(0, sizeof(prefix) - 1, prefix)) {
            desc += sizeof(prefix) - 1;
        }
        xsink.raiseException("OPTION-ERROR", "QoreProgramHelper: %s", desc);
        return new QoreProgram(parse_options & ~qore_program_private_base::modernIncompatibleOptions());
    }
    return new QoreProgram(parse_options);
}

//! creates the QoreProgram object: DEPRECATED: use QoreProgramHelper(int64, ExceptionSink&) instead
QoreProgramHelper::QoreProgramHelper(ExceptionSink& xs) : pgm(new QoreProgram), xsink(xs) {
}

//! creates the QoreProgram object and sets the parse options
QoreProgramHelper::QoreProgramHelper(const QoreParseOptions& parse_options, ExceptionSink& xs)
        : pgm(qore_new_checked_program(parse_options, xs)), xsink(xs) {
}

//! waits until all background threads in the Qore library have terminated and until the QoreProgram object is done executing and then dereferences the object
/** QoreProgram objects are deleted when there reference count reaches 0.
 */
QoreProgramHelper::~QoreProgramHelper() {
    // the exit hooks can stop threads that would otherwise run forever
    qore_run_exit_hooks(nullptr);

    // waits for all background threads to execute
    thread_counter.waitForZero(&xsink);

    // issue #4228: ensure we are running in a qore thread
    QoreForeignThreadHelper qfth;

    // waits for the current Program to terminate
    pgm->waitForTerminationAndDeref(&xsink);
}

//! returns the QoreProgram object being managed
QoreProgram* QoreProgramHelper::operator->() {
    return pgm;
}

//! returns the QoreProgram object being managed
QoreProgram* QoreProgramHelper::operator*() {
    return pgm;
}
