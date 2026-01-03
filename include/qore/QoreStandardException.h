/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreStandardException.h

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

#ifndef _QORE_QORESTANDARDEXCEPTION_H

#define _QORE_QORESTANDARDEXCEPTION_H

/** @file QoreStandardException.h
    Defines the base class for concrete c++ exceptions in the %Qore library
*/

#include "QoreValue.h"

#include <cstdarg>
#include <string>

// forward references
class QoreStringNode;

//! abstract base class for c++ Exceptions in the %Qore library
class QoreStandardException : public AbstractException {
public:
    //! creates the exception object with error and description strings
    DLLEXPORT QoreStandardException(const char* err, const char* desc_fmt, ...);

    //! creates the exception object with error and description strings
    /** this function takes ownership of the string reference for desc

        @since %Qore 0.9.5
    */
    DLLEXPORT QoreStandardException(const char* err, QoreStringNode* desc, QoreValue arg = QoreValue());

    //! creates the exception object with error and description strings
    /** this function takes ownership of the string references
    */
    DLLEXPORT QoreStandardException(QoreStringNode* err, QoreStringNode* desc);

    //! creates the exception object with error and description strings
    /** this function takes ownership of the string references

        @since %Qore 0.9.5
    */
    DLLEXPORT QoreStandardException(QoreStringNode* err, QoreStringNode* desc, QoreValue arg);

    //! Default move constructor
    DLLLOCAL QoreStandardException(QoreStandardException&&) = default;

    //! Destroys the object
    DLLEXPORT virtual ~QoreStandardException();

    //! Default assignment operator
    DLLLOCAL QoreStandardException& operator=(QoreStandardException&&) = default;

    //! Raises the corresponding Qore exception in the ExceptionSink.
    /** @param xsink the exception sink
    */
    virtual void convert(ExceptionSink* xsink);

private:
    //! qore exception error code
    QoreStringNode* err;
    //! qore exception error description
    QoreStringNode* desc;
    //! qore exeption arg
    QoreValue arg{};
};

#endif
