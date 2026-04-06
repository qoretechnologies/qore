/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    MpscQueue.h

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

#ifndef _QORE_MPSCQUEUE_H
#define _QORE_MPSCQUEUE_H

#include <atomic>
#include <utility>
#include <vector>

//! Lock-free Multiple Producer, Single Consumer queue
/** Uses the Vyukov MPSC algorithm:
    - push() is wait-free for producers (single CAS on tail pointer)
    - pop()/drain() is for the single consumer only (no CAS needed)
    - Heap-allocated nodes; trivial cost compared to the futex being eliminated

    @note The consumer (pop/drain) must be called from exactly ONE thread.
    Multiple threads may call push() concurrently.

    @since %Qore 2.3
*/
template <typename T>
class MpscQueue {
public:
    MpscQueue() {
        // Sentinel node — always present; head points here initially
        Node* sentinel = new Node();
        head = sentinel;
        tail.store(sentinel, std::memory_order_relaxed);
    }

    ~MpscQueue() {
        // Drain remaining nodes
        Node* n = head;
        while (n) {
            Node* next = n->next.load(std::memory_order_relaxed);
            delete n;
            n = next;
        }
    }

    // Non-copyable, non-movable
    MpscQueue(const MpscQueue&) = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;

    //! Push an item by copy (wait-free, safe from any thread)
    void push(const T& item) {
        Node* node = new Node(T(item));
        Node* prev = tail.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    //! Push an item by move (wait-free, safe from any thread)
    void push(T&& item) {
        Node* node = new Node(std::move(item));
        // Atomically swap tail to point to new node; prev_tail->next will link to it
        Node* prev = tail.exchange(node, std::memory_order_acq_rel);
        // Link previous tail to new node — this makes the node visible to the consumer
        prev->next.store(node, std::memory_order_release);
    }

    //! Pop a single item (single consumer only)
    /** @return true if an item was popped, false if queue was empty
    */
    bool pop(T& out) {
        Node* sentinel = head;
        Node* next = sentinel->next.load(std::memory_order_acquire);
        if (!next) {
            return false;
        }
        // Move data out of the next node, then advance head past sentinel
        out = std::move(next->data);
        head = next;
        delete sentinel;
        return true;
    }

    //! Drain all available items into a vector (single consumer only)
    /** More efficient than repeated pop() calls — avoids per-node
        sentinel management overhead.
    */
    void drain(std::vector<T>& out) {
        Node* sentinel = head;
        Node* node = sentinel->next.load(std::memory_order_acquire);
        if (!node) {
            return;
        }
        // Walk the chain, collecting items
        // The last node in the chain becomes the new sentinel
        Node* last = sentinel;
        while (node) {
            out.push_back(std::move(node->data));
            last = node;
            node = node->next.load(std::memory_order_acquire);
        }
        // Free all consumed nodes except the last (which becomes the new sentinel)
        head = last;
        Node* n = sentinel;
        while (n != last) {
            Node* next = n->next.load(std::memory_order_relaxed);
            delete n;
            n = next;
        }
    }

    //! Check if queue appears empty (single consumer only, approximate)
    bool empty() const {
        Node* sentinel = head;
        return sentinel->next.load(std::memory_order_acquire) == nullptr;
    }

private:
    struct Node {
        std::atomic<Node*> next{nullptr};
        T data{};

        Node() = default;
        explicit Node(T&& d) : data(std::move(d)) {}
    };

    Node* head;                     //!< Consumer reads from here (single-threaded)
    std::atomic<Node*> tail;        //!< Producers CAS here (multi-threaded)
};

#endif // _QORE_MPSCQUEUE_H
