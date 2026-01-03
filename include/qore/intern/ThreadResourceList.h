/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ThreadResourceList.h

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

#ifndef _QORE_THREADRESOURCELIST_H

#define _QORE_THREADRESOURCELIST_H

#include <set>
#include <map>

typedef std::set<AbstractThreadResource*> trset_t;

struct ArgPgm {
    QoreValue arg{};
    QoreProgram* pgm;

    DLLLOCAL ArgPgm(const QoreValue arg, QoreProgram* pgm) : arg(arg.refSelf()), pgm(pgm) {
    }
};

typedef std::map<ResolvedCallReferenceNode*, ArgPgm> crmap_t;

class ThreadResourceList {
private:
    static Sequence seq;
    trset_t trset;
    crmap_t crmap;

public:
    ThreadResourceList* prev;

    DLLLOCAL ThreadResourceList(ThreadResourceList* p = nullptr) : prev(p) {
    }

    DLLLOCAL ~ThreadResourceList() {
        assert(trset.empty());
        assert(crmap.empty());
    }

    DLLLOCAL void set(AbstractThreadResource* atr);
    DLLLOCAL void set(const ResolvedCallReferenceNode* rcr, const QoreValue arg);

    DLLLOCAL bool check(AbstractThreadResource* atr) const;

    // returns 0 if removed, -1 if not found
    DLLLOCAL int remove(AbstractThreadResource* atr);
    // returns 0 if removed, -1 if not found
    DLLLOCAL int remove(const ResolvedCallReferenceNode* rcr, ExceptionSink* xsink);

    DLLLOCAL void purge(ExceptionSink* xsink);

    // purge thread resources tied to a particular Program (that's being destroyed)
    // returns: -1 if there are still thread resources left, 0 = all thread resources purged
    DLLLOCAL void purge(const QoreProgram* pgm, ExceptionSink* xsink);

    DLLLOCAL bool empty() const {
        return trset.empty();
    }
};

#endif
