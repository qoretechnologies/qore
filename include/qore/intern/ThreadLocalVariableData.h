/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ThreadLocalVariableData.h

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

#ifndef _QORE_INTERN_THREADLOCALVARIABLEDATA_H
#define _QORE_INTERN_THREADLOCALVARIABLEDATA_H

#include <utility>
#include <vector>

class ThreadLocalVariableData : public ThreadLocalData<LocalVarValue> {
public:
    // clears and marks all variables as finalized on the stack
    DLLLOCAL void finalize(SafeDerefHelper& sdh) {
        ThreadLocalVariableData::iterator i(curr);
        while (i.next()) {
            sdh.deref(i.get().finalize());
        }
    }

    //! Collects values with declaration order for sorted finalization (issue #5168)
    DLLLOCAL void collectForFinalize(std::vector<std::pair<uint64_t, QoreValue>>& ordered_values) {
        ThreadLocalVariableData::iterator i(curr);
        while (i.next()) {
            LocalVarValue& var = i.get();
            uint64_t order = var.getDeclOrder();
            QoreValue val = var.finalize();
            if (val) {
                ordered_values.push_back(std::make_pair(order, val));
            }
        }
    }

    // deletes everything on the stack
    DLLLOCAL void del(ExceptionSink* xsink) {
        //printd(5, "ThreadLocalVariableData::del() this: %p empty: %d prev: %p pos: %d\n", this, empty(), curr->prev, curr->pos);

        // then we uninstantiate
        while (curr->prev || curr->pos)
            uninstantiate(xsink);
    }

    DLLLOCAL LocalVarValue* instantiate() {
        if (curr->pos == QORE_THREAD_STACK_BLOCK) {
            if (curr->next)
                curr = curr->next;
            else {
                curr->next = new Block(curr);
                //printf("this: %p: add curr: %p, curr->next: %p\n", this, curr, curr->next);
                curr = curr->next;
            }
        }
        return &curr->var[curr->pos++];
    }

    DLLLOCAL void uninstantiate(ExceptionSink* xsink) {
        uninstantiateIntern();
        //printd(5, "ThreadLocalVariableData::uninstantiate() this: %p '%s' pos: %d\n", this, curr->var[curr->pos].id, curr->pos);
        curr->var[curr->pos].uninstantiate(xsink);
    }

    DLLLOCAL void uninstantiateSelf() {
        uninstantiateIntern();
        curr->var[curr->pos].uninstantiateSelf();
    }

    DLLLOCAL void uninstantiateIntern() {
        if (!curr->pos) {
            if (curr->next) {
                //printf("this %p: del curr: %p, curr->next: %p\n", this, curr, curr->next);
                delete curr->next;
                curr->next = 0;
            }
            curr = curr->prev;
            assert(curr);
        }
        --curr->pos;
    }

    DLLLOCAL LocalVarValue* find(const char* id) {
        Block* w = curr;
        while (true) {
            int p = w->pos;
            while (p) {
                --p;
                LocalVarValue* var = &w->var[p];
                if (var->id == id && !var->frame_boundary)
                    return var;
            }
            w = w->prev;
#ifdef DEBUG
            if (!w) {
                p = curr->pos - 1;
                printd(0, "ThreadLocalVariableData::find() this: %p no local variable '%s' (%p) on stack (pgm: %p) "
                    "p: %d\n", this, id, id, getProgram(), p);
                while (p >= 0) {
                    printd(0, "var p: %d: %s (%p) (frame_boundary: %d)\n", p, curr->var[p].id, curr->var[p].id,
                        curr->var[p].frame_boundary);
                    --p;
                }
            }
#endif
            assert(w);
        }
        // to avoid a warning on most compilers - note that this generates a warning on recent versions of aCC!
        return 0;
    }

    DLLLOCAL void pushFrameBoundary() {
        ++frame_count;
        //printd(5, "ThreadLocalVariableData::pushFrameBoundary(): fc:%d\n", frame_count);
        LocalVarValue* v = instantiate();
        v->setFrameBoundary();
    }

    DLLLOCAL void popFrameBoundary() {
        assert(frame_count >= 0);
        --frame_count;
        //printd(5, "ThreadLocalVariableData::popFrameBoundary(): fc:%d\n", frame_count);
        uninstantiateIntern();
        assert(curr->var[curr->pos].frame_boundary);
        curr->var[curr->pos].frame_boundary = false;
    }

    DLLLOCAL int getFrame(int frame, Block*& w, int& p);

    DLLLOCAL void getLocalVars(QoreHashNode& h, int frame, ExceptionSink* xsink);

    // returns 0 = OK, 1 = no such variable, -1 exception setting variable
    DLLLOCAL int setVarValue(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink);
};

#endif
