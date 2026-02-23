/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_QoreParseOptions.h

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

#ifndef _QORE_QC_PARSEOPTIONS_H
#define _QORE_QC_PARSEOPTIONS_H

#include <qore/Qore.h>
#include <qore/QoreParseOptions.h>
#include <qore/ParseOptionMap.h>

DLLEXPORT extern qore_classid_t CID_PARSEOPTIONS;
DLLLOCAL extern QoreClass* QC_PARSEOPTIONS;

DLLLOCAL QoreClass* initParseOptionsClass(QoreNamespace& ns);

class QoreParseOptionsData : public AbstractPrivateData {
public:
    DLLLOCAL QoreParseOptionsData() {}
    DLLLOCAL QoreParseOptionsData(const QoreParseOptions& po) : po(po) {}
    DLLLOCAL QoreParseOptionsData(const QoreParseOptionsData& other) : po(other.po) {}

    DLLLOCAL const QoreParseOptions& get() const { return po; }
    DLLLOCAL QoreParseOptions& getRef() { return po; }

    //! Resolve parse option from string name, raising an exception if not found
    DLLLOCAL static QoreParseOptions resolveOption(const QoreStringNode* name, ExceptionSink* xsink) {
        TempEncodingHelper str(name, QCS_DEFAULT, xsink);
        if (!str) {
            return QoreParseOptions(-1);
        }
        QoreParseOptions po = ParseOptionMap::find_code(str->c_str());
        if (po == QoreParseOptions(-1)) {
            xsink->raiseException("PARSE-OPTION-ERROR", "unknown parse option: '%s'", str->c_str());
        }
        return po;
    }

    //! Convert options to a list of string names
    DLLLOCAL QoreListNode* toStringList(ExceptionSink* xsink) const {
        ReferenceHolder<QoreListNode> rv(new QoreListNode(stringTypeInfo), xsink);

        // Iterate lo bits (0-63)
        for (int p = 0; p < 64; ++p) {
            int64 v = po.getLo() & (1LL << p);
            if (v) {
                const char* name = ParseOptionMap::find_name(QoreParseOptions(v));
                if (name) {
                    rv->push(new QoreStringNode(name), xsink);
                }
            }
        }

        // Iterate hi bits (64-127)
        for (int p = 0; p < 64; ++p) {
            int64 v = po.getHi() & (1LL << p);
            if (v) {
                const char* name = ParseOptionMap::find_name(QoreParseOptions(0, v));
                if (name) {
                    rv->push(new QoreStringNode(name), xsink);
                }
            }
        }

        return rv.release();
    }

private:
    QoreParseOptions po;
};

#endif
