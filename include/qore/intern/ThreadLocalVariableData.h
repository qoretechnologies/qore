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
            if (curr->next) {
                // DEBUG: Track block transitions
                printd(0, "BLOCK_TRANSITION: curr=%p -> %p (reuse next)\n", (void*)curr, (void*)curr->next);
                curr = curr->next;
            } else {
                // DEBUG: Track new block allocation
                printd(0, "BLOCK_ALLOC_NEW: curr=%p allocating new block\n", (void*)curr);
                curr->next = new Block(curr);
                //printf("this: %p: add curr: %p, curr->next: %p\n", this, curr, curr->next);
                printd(0, "BLOCK_ALLOC_DONE: curr=%p -> %p (new)\n", (void*)curr->prev, (void*)curr->next);
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
                printd(0, "ThreadLocalVariableData::find() FAILED to find variable '%s' (%p) on stack (pgm: %p) "
                    "curr_block=%p curr_pos=%d frame_count=%d\n", id, id, getProgram(), (void*)curr, curr->pos, frame_count);
                printd(0, "  Variables on stack (from curr=%p pos=%d):\n", (void*)curr, curr->pos);
                while (p >= 0) {
                    printd(0, "    var p: %d: %s (%p) (frame_boundary: %d)\n", p, curr->var[p].id, curr->var[p].id,
                        curr->var[p].frame_boundary);
                    --p;
                }
                // Check previous blocks
                Block* check_prev = curr->prev;
                int block_num = 1;
                while (check_prev) {
                    printd(0, "  Previous block %d (%p): pos=%d (expected to be full with %d vars)\n",
                           block_num, (void*)check_prev, check_prev->pos, QORE_THREAD_STACK_BLOCK);
                    block_num++;
                    check_prev = check_prev->prev;
                }
            }
#endif
            assert(w);
        }
        // to avoid a warning on most compilers - note that this generates a warning on recent versions of aCC!
        return 0;
    }

    // returns nullptr if not found; avoids assertions for lookup checks
    DLLLOCAL LocalVarValue* findMaybe(const char* id) {
        Block* w = curr;
        while (w) {
            int p = w->pos;
            while (p) {
                --p;
                LocalVarValue* var = &w->var[p];
                if (var->id == id && !var->frame_boundary)
                    return var;
            }
            w = w->prev;
        }
        return nullptr;
    }

    DLLLOCAL void pushFrameBoundary() {
        ++frame_count;
        //printd(5, "ThreadLocalVariableData::pushFrameBoundary(): fc:%d\n", frame_count);
        LocalVarValue* v = instantiate();
        v->setFrameBoundary();
        v->frame_marker_id = frame_count;  // Store the frame count this marker belongs to
        // DEBUG: Track frame boundary pushes with block information
        printd(0, "PUSH_FB: fc=%d block=%p pos=%d (marker_at_pos=%d)\n",
               frame_count, (void*)curr, curr->pos - 1, curr->pos - 1);
    }

    DLLLOCAL void popFrameBoundary() {
        assert(frame_count >= 0);

        // First, try to uninstantiate the current position, which should be right after the frame boundary marker
        int expected_pos = curr->pos;  // Position BEFORE uninstantiateIntern
        uninstantiateIntern();
        int actual_pos = curr->pos;    // Position AFTER uninstantiateIntern

        // Check if the current position has the frame_boundary flag
        if (curr->var[curr->pos].frame_boundary) {
            // Normal case - frame boundary at expected position
            printd(0, "POP_FB: fc=%d block=%p pos=%d (match)\n", frame_count, (void*)curr, curr->pos);
            curr->var[curr->pos].frame_boundary = false;
            --frame_count;
            return;
        }

        // Mismatch case: marker is at a different position due to nested frames
        // Search backwards to find the frame boundary marker for this frame
        printd(0, "POP_FB_MISMATCH: fc=%d block=%p expected_pos=%d actual_pos=%d\n",
               frame_count, (void*)curr, expected_pos, actual_pos);

        // Search backwards from actual_pos to find the frame boundary marker for THIS frame
        // We match on both frame_boundary flag and frame_marker_id to ensure we clear the correct marker
        int checked_positions = 0;
        for (int back_offset = 1; back_offset <= (int)actual_pos + 1 + QORE_THREAD_STACK_BLOCK; ++back_offset) {
            Block* search_curr = curr;
            int search_pos = (int)actual_pos - back_offset;

            // Handle block transitions
            while (search_pos < 0 && search_curr->prev) {
                search_curr = search_curr->prev;
                search_pos += QORE_THREAD_STACK_BLOCK;
            }

            if (search_pos >= 0) {
                checked_positions++;
                // Check if this is the frame boundary marker for the current frame
                if (search_curr->var[search_pos].frame_boundary && search_curr->var[search_pos].frame_marker_id == frame_count) {
                    // Found the marker for THIS frame - clear it
                    search_curr->var[search_pos].frame_boundary = false;
                    printd(0, "POP_FB_FOUND_CLEARED: fc=%d marker_block=%p marker_pos=%d (checked %d positions)\n",
                           frame_count, (void*)search_curr, search_pos, checked_positions);
                    --frame_count;
                    return;
                }
            }
        }

        // Log detailed diagnostic when marker not found
        printd(0, "POP_FB_NOT_FOUND_DETAIL: fc=%d checked %d positions (search range: back_offset 1..%d)\n",
               frame_count, checked_positions, (int)actual_pos + 1 + QORE_THREAD_STACK_BLOCK);
        printd(0, "  current var at actual_pos=%d: fb=%d id=%s\n",
               actual_pos, curr->var[actual_pos].frame_boundary, curr->var[actual_pos].id ? curr->var[actual_pos].id : "(nil)");

        // Check if there are any frame boundaries at all in the search range
        int fb_count = 0;
        for (int i = 0; i <= actual_pos && i < QORE_THREAD_STACK_BLOCK; ++i) {
            if (curr->var[i].frame_boundary) fb_count++;
        }
        if (curr->prev) {
            for (int i = 0; i < QORE_THREAD_STACK_BLOCK; ++i) {
                if (curr->prev->var[i].frame_boundary) fb_count++;
            }
        }
        printd(0, "  found %d frame_boundary markers in range\n", fb_count);

        // Marker not found - this indicates a real bug
        // Decrement frame_count anyway to maintain count even if marker is missing
        printd(0, "WARNING: Frame boundary marker not found (fc=%d, block=%p pos=%d)\n",
               frame_count, (void*)curr, curr->pos);
        --frame_count;
    }

    DLLLOCAL int getFrame(int frame, Block*& w, int& p);

    DLLLOCAL void getLocalVars(QoreHashNode& h, int frame, ExceptionSink* xsink);

    // returns 0 = OK, 1 = no such variable, -1 exception setting variable
    DLLLOCAL int setVarValue(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink);
};

#endif
