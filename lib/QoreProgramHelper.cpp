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

//! creates the QoreProgram object: DEPRECATED: use QoreProgramHelper(int64, ExceptionSink&) instead
QoreProgramHelper::QoreProgramHelper(ExceptionSink& xs) : pgm(new QoreProgram), xsink(xs) {
}

//! creates the QoreProgram object and sets the parse options
QoreProgramHelper::QoreProgramHelper(const QoreParseOptions& parse_options, ExceptionSink& xs) : pgm(new QoreProgram(parse_options)), xsink(xs) {
}

//! waits until all background threads in the Qore library have terminated and until the QoreProgram object is done executing and then dereferences the object
/** QoreProgram objects are deleted when there reference count reaches 0.
 */
QoreProgramHelper::~QoreProgramHelper() {
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
