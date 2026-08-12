/*
    ThreadResourceList.cpp

    POSIX thread library for Qore

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

#include <qore/Qore.h>
#include "qore/intern/ThreadResourceList.h"
#include <qore/AbstractThreadResource.h>

Sequence ThreadResourceList::seq;

void ThreadResourceList::set(AbstractThreadResource* atr) {
    //printd(5, "TRL::set() this: %p atr: %p\n", this, atr);
    // ignore object if already set
    if (trset.find(atr) != trset.end())
        return;

    atr->ref();
    trset.insert(atr);
}

void ThreadResourceList::set(const ResolvedCallReferenceNode* rcr, const QoreValue arg) {
    //printd(5, "TRL::set() this: %p rcr: %p\n", this, rcr);
    // ignore object if already set
    crmap_t::iterator i = crmap.lower_bound(const_cast<ResolvedCallReferenceNode*>(rcr));
    if (i != crmap.end() && i->first == rcr)
        return;

    QoreProgram* pgm = rcr->getProgram();
    if (!pgm)
        pgm = getProgram();

    crmap.insert(i, crmap_t::value_type(rcr->refRefSelf(), ArgPgm(arg, pgm)));
}

bool ThreadResourceList::check(AbstractThreadResource* atr) const {
    //printd(5, "TRL::check(atr: %p)\n", atr);
    return trset.find(atr) != trset.end();
}

void ThreadResourceList::purge(ExceptionSink* xsink) {
    purge(0, xsink);
    //printd(5, "TRL::purge() this: %p done\n", this);
}

void ThreadResourceList::purge(const QoreProgram* pgm, ExceptionSink* xsink) {
    for (trset_t::iterator i = trset.begin(), e = trset.end(); i != e;) {
        AbstractThreadResource* atr = *i;
        if (!pgm || ((*i)->getProgram() == pgm)) {
            trset_t::iterator j = i;
            ++i;

            //printd(5, "TRL::purge() this: %p cleaning up atr: %p\n", this, atr);
            // we have to remove the thread resource from the list before running cleanup in case of user thread resources
            // where the cleanup routine will cause the object to be deleted and therefore for remove_thread_resource() to
            // be called which can result in a crash
            trset.erase(j);

            atr->cleanup(xsink);
            // this reference can be the last one: a resource whose owning object was destroyed
            // while this thread still held it outlives that object, and then only the purge can
            // free it.  A resource that has to release something when it is freed - a datasource
            // has to close its connection - can only do that with an ExceptionSink to report
            // errors to, so the resource is dereferenced with the one this purge already has.  The
            // parameterless deref() has nowhere to report anything, which is why
            // ManagedDatasource::deref() asserts there that it is never the last reference: that
            // holds for remove() below, where the caller owns a reference, and not here.
            atr->deref(xsink);
        } else {
            ++i;
        }
    }

    for (crmap_t::iterator i = crmap.begin(), e = crmap.end(); i != e;) {
        ResolvedCallReferenceNode* rcr = i->first;
        //printd(5, "TRL::purge() this: %p cleaning up rcr: %p (pgm: %p) pgm: %p\n", this, rcr, i->second.pgm, pgm);
        if (!pgm || (i->second.pgm == pgm)) {
            QoreValue arg = i->second.arg;
            crmap_t::iterator j = i;
            ++i;

            //printd(5, "TRL::purge() this: %p cleaning up rcr: %p\n", this, rcr);
            // we have to remove the thread resource from the list before running cleanup
            crmap.erase(j);

            ReferenceHolder<QoreListNode> args(xsink);
            if (arg) {
                args = new QoreListNode(autoTypeInfo);
                args->push(arg, xsink);
            }

            // issue #3571: make sure that every time we run the callback, we run with a clean xsink
            ExceptionSink xsink2;
            rcr->execValue(*args, &xsink2).discard(&xsink2);
            rcr->deref(&xsink2);
            if (xsink2) {
                xsink->assimilate(xsink2);
            }
        } else {
            ++i;
        }
    }

    //printd(5, "TRL::purge() this: %p pgm: %p done\n", this, pgm);
}

int ThreadResourceList::remove(AbstractThreadResource* atr) {
    //printd(5, "TRL::remove() this: %p atr: %p\n", this, atr);

    trset_t::iterator i = trset.find(atr);
    if (i == trset.end())
        return -1;

    (*i)->deref();
    trset.erase(i);
    return 0;
}

int ThreadResourceList::remove(const ResolvedCallReferenceNode* rcr, ExceptionSink* xsink) {
    //printd(5, "TRL::remove() this: %p rcr: %p\n", this, rcr);

    crmap_t::iterator i = crmap.find(const_cast<ResolvedCallReferenceNode*>(rcr));
    if (i == crmap.end())
        return -1;

    i->first->deref(xsink);
    i->second.arg.discard(xsink);
    crmap.erase(i);
    return 0;
}
