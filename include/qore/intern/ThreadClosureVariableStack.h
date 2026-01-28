/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ThreadClosureVariableStack.h

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

#ifndef _QORE_INTERN_THREADCLOSUREVARIABLESTACK_H
#define _QORE_INTERN_THREADCLOSUREVARIABLESTACK_H

#include <utility>
#include <vector>

//! Stack entry for closure variables, storing both the pointer and per-entry declaration order (issue #5168)
/** The declaration order is stored per-stack-entry rather than on the shared ClosureVarValue object
    because ClosureVarValue objects can be shared across multiple threads and pushed multiple times.
    Storing the order on the shared object would cause race conditions and incorrect destruction ordering.
*/
struct ClosureStackEntry {
    ClosureVarValue* cvv;
    uint64_t decl_order;

    DLLLOCAL ClosureStackEntry() : cvv(nullptr), decl_order(0) {
    }

    DLLLOCAL ClosureStackEntry(ClosureVarValue* c, uint64_t order) : cvv(c), decl_order(order) {
    }

    //! Returns true if this entry is a frame boundary marker
    DLLLOCAL bool isFrameBoundary() const {
        return cvv == nullptr;
    }
};

class ThreadClosureVariableStack : public ThreadLocalData<ClosureStackEntry> {
private:
    DLLLOCAL void instantiateIntern(ClosureVarValue* cvar, uint64_t order) {
        //printd(5, "ThreadClosureVariableStack::instantiateIntern(%p = '%s') this: %p pgm: %p\n", cvar ? cvar->id : "null", cvar ? cvar->id : "null", this, getProgram());

        if (curr->pos == QORE_THREAD_STACK_BLOCK) {
            if (curr->next) {
                curr = curr->next;
            } else {
                curr->next = new Block(curr);
                //printf("this: %p: add curr: %p, curr->next: %p\n", this, curr, curr->next);
                curr = curr->next;
            }
        }
        curr->var[curr->pos++] = ClosureStackEntry(cvar, order);
    }

public:
    // clears and marks all variables as finalized on the stack
    DLLLOCAL void finalize(SafeDerefHelper& sdh) {
        //printd(5, "ThreadClosureVariableStack::finalize() this: %p\n", this);
        ThreadClosureVariableStack::iterator i(curr);
        while (i.next()) {
            ClosureStackEntry& entry = i.get();
            if (entry.cvv) {
                //printd(5, "ThreadClosureVariableStack::finalize() this: %p %p %s\n", this, entry.cvv, entry.cvv->id);
                sdh.deref(entry.cvv->finalize());
            }
        }
    }

    //! Collects values with declaration order for sorted finalization (issue #5168)
    /** Uses the per-entry declaration order stored in ClosureStackEntry rather than the order
        stored on the shared ClosureVarValue object, ensuring thread-safe destruction ordering.
    */
    DLLLOCAL void collectForFinalize(std::vector<std::pair<uint64_t, QoreValue>>& ordered_values) {
        ThreadClosureVariableStack::iterator i(curr);
        while (i.next()) {
            ClosureStackEntry& entry = i.get();
            if (entry.cvv) {  // skip frame boundaries (nullptr entries)
                // Use per-entry order, not the shared object's order (thread-safety fix)
                uint64_t order = entry.decl_order;
                QoreValue val = entry.cvv->finalize();
                if (val) {
                    ordered_values.push_back(std::make_pair(order, val));
                }
            }
        }
    }

    // deletes everything on the stack
    DLLLOCAL void del(ExceptionSink* xsink) {
        while (curr->prev || curr->pos) {
            uninstantiate(xsink);
        }
    }

    DLLLOCAL ClosureVarValue* instantiate(const char* id, const QoreTypeInfo* typeInfo, QoreValue& nval,
            bool assign, uint64_t order) {
        ClosureVarValue* cvar = new ClosureVarValue(id, typeInfo, nval, assign);
        instantiateIntern(cvar, order);
        return cvar;
    }

    DLLLOCAL void instantiate(ClosureVarValue* cvar, uint64_t order) {
        instantiateIntern(cvar, order);
    }

    DLLLOCAL void uninstantiateIntern() {
#if 0
        if (!curr->pos) {
            printd(0, "ThreadClosureVariableStack::uninstantiate() this: %p pos: %d %p %s\n", this,
                curr->prev->pos - 1, curr->prev->var[curr->prev->pos - 1].cvv->id,
                curr->prev->var[curr->prev->pos - 1].cvv->id);
        } else {
            printd(0, "ThreadClosureVariableStack::uninstantiate() this: %p pos: %d %p %s\n", this,
                curr->pos - 1, curr->var[curr->pos - 1].cvv->id, curr->var[curr->pos - 1].cvv->id);
        }
#endif
        if (!curr->pos) {
            if (curr->next) {
                //printf("this %p: del curr: %p, curr->next: %p\n", this, curr, curr->next);
                delete curr->next;
                curr->next = nullptr;
            }
            curr = curr->prev;
            assert(curr);
        }
        --curr->pos;
    }

    DLLLOCAL void uninstantiate(ExceptionSink* xsink) {
        uninstantiateIntern();
        assert(curr->var[curr->pos].cvv);
        curr->var[curr->pos].cvv->deref(xsink);
    }

    DLLLOCAL ClosureVarValue* find(const char* id) {
        printd(5, "ThreadClosureVariableStack::find() this: %p id: %p\n", this, id);
        Block* w = curr;
        while (true) {
            int p = w->pos;
            while (p) {
                --p;
                ClosureVarValue* rv = w->var[p].cvv;
                //printd(5, "ThreadClosureVariableStack::find(%p '%s') this: %p checking %p '%s'\n", id, id, this, rv ? rv->id : nullptr, rv ? rv->id : "n/a");
                if (rv && rv->id == id) {
                    //printd(5, "ThreadClosureVariableStack::find(%p '%s') this: %p returning: %p\n", id, id, this, rv);
                    return rv;
                }
            }
            w = w->prev;
#ifdef DEBUG
            if (!w) {
                printd(0, "ThreadClosureVariableStack::find() this: %p no closure-bound local variable '%s' (%p) on stack (pgm: %p) p: %d curr->prev: %p\n", this, id, id, getProgram(), p, curr->prev);
                w = curr;
                while (w) {
                    p = w->pos;
                    while (p) {
                        --p;
                        ClosureVarValue* cvv = w->var[p].cvv;
                        printd(0, "var p: %d: %s (%p)\n", p, cvv ? cvv->id : "frame boundary", cvv ? cvv->id : nullptr);
                    }
                    w = w->prev;
                }
            }
#endif
            assert(w);
        }
        // to avoid a warning on most compilers - note that this generates a warning on aCC!
        return nullptr;
    }

    DLLLOCAL cvv_vec_t* getAll() const {
        cvv_vec_t* cv = 0;
        Block* w = curr;
        while (w) {
            int p = w->pos;
            while (p) {
                --p;
                ClosureVarValue* cvv = w->var[p].cvv;
                // skip frame boundaries
                if (!cvv) {
                    continue;
                }
                if (!cv) {
                    cv = new cvv_vec_t;
                }
                cv->push_back(cvv->refSelf());
            }
            w = w->prev;
        }
        //printd(5, "ThreadClosureVariableStack::getAll() this: %p cv: %p size: %d\n", this, cv, cv ? cv->size() : 0);
        return cv;
    }

    DLLLOCAL void pushFrameBoundary() {
        ++frame_count;
        //printd(5, "ThreadClosureVariableStack::pushFrameBoundary(): fc:%d\n", frame_count);
        instantiateIntern(nullptr, 0);
    }

    DLLLOCAL void popFrameBoundary() {
        assert(frame_count >= 0);
        --frame_count;
        //printd(5, "ThreadClosureVariableStack::popFrameBoundary(): fc:%d\n", frame_count);
        uninstantiateIntern();
        assert(!curr->var[curr->pos].cvv);
    }

    DLLLOCAL int getFrame(int frame, Block*& w, int& p);

    DLLLOCAL void getLocalVars(QoreHashNode& h, int frame, ExceptionSink* xsink);

    // returns 0 = OK, 1 = no such variable, -1 exception setting variable
    DLLLOCAL int setVarValue(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink);
};

#endif
