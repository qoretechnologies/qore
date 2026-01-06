/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Context.h

    Qore Programming Language

    Copyright (C) 2004 - 2024 Qore Technologies, s.r.o.

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

#ifndef QORE_CONTEXT_H

#define QORE_CONTEXT_H

#ifdef _QORE_LIB_INTERN

#include <qore/common.h>

#define CM_WHERE_NODE           1
#define CM_SORT_ASCENDING       2
#define CM_SORT_DESCENDING      3
#define CM_SUMMARIZE_BY         4

class Context {
public:
    char* name;
    QoreHashNode* value = nullptr;
    // master row list needed for summary contexts
    int master_max_pos = 0;
    int* master_row_list = nullptr;
    int max_pos = 0;
    int pos = 0;
    int* row_list = nullptr;
    int group_pos = 0;
    int max_group_pos = 0;
    struct node_row_list_s* group_values = nullptr;
    Context* next;
    int sub;

    DLLLOCAL Context(char *nme, ExceptionSink *xsinkx, QoreValue exp,
        QoreValue cond = QoreValue(),
        int sort_type = -1, QoreValue sort = QoreValue(),
        QoreValue summary = QoreValue(), int ignore_key = 0);
    DLLLOCAL QoreValue eval(const char *field, ExceptionSink *xsink);

    DLLLOCAL QoreHashNode* getRow(ExceptionSink *xsink);
    DLLLOCAL int next_summary();
    DLLLOCAL int check_condition(QoreValue cond, ExceptionSink *xsinkx);
    DLLLOCAL void deref(ExceptionSink *xsink);

    DLLLOCAL bool isFirst() const {
        return !pos;
    }

    DLLLOCAL bool isLast() const {
        return pos == (max_pos - 1);
    }

    DLLLOCAL size_t getPos() const {
        return pos;
    }

    DLLLOCAL size_t getTotal() const {
        return max_pos;
    }

private:
    DLLLOCAL void Sort(QoreValue sort, int sort_type = CM_SORT_ASCENDING);

    ExceptionSink* sort_xsink;

protected:
    DLLLOCAL ~Context();
};

DLLLOCAL QoreValue eval_context_ref(const char* key, ExceptionSink* xsink);
DLLLOCAL QoreHashNode* eval_context_row(ExceptionSink* xsink);

#endif

#endif
