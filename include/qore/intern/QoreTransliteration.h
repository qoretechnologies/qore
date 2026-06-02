/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTransliteration.h

    regex-like transliteration class definition

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

#ifndef _QORE_QORETRANSLITERATION_H

#define _QORE_QORETRANSLITERATION_H

class QoreTransliteration : public QoreReferenceCounter {
private:
    const QoreProgramLocation* loc;
    QoreString source, target;
    bool sr = false,
        tr = false;

    DLLLOCAL void doRange(QoreString& str, char end);

public:
    DLLLOCAL QoreTransliteration(const QoreProgramLocation* loc);
    DLLLOCAL virtual ~QoreTransliteration();

    DLLLOCAL void finishSource();
    DLLLOCAL void finishTarget();
    DLLLOCAL QoreStringNode* exec(const QoreString *target, ExceptionSink* xsink) const;
    DLLLOCAL void concatSource(char c);
    DLLLOCAL void concatTarget(char c);
    DLLLOCAL void setTargetRange();
    DLLLOCAL void setSourceRange();

    //! Returns the source string (for AOT serialization)
    DLLLOCAL const QoreString& getSource() const { return source; }

    //! Returns the target string (for AOT serialization)
    DLLLOCAL const QoreString& getTarget() const { return target; }

    DLLLOCAL void ref() const {
        ROreference();
    }

    DLLLOCAL void deref() {
        if (ROdereference())
            delete this;
    }

    DLLLOCAL QoreTransliteration* refSelf() const {
        ref();
        return const_cast<QoreTransliteration*>(this);
    }
};

#endif // _QORE_QORETRANSLITERATION_H
