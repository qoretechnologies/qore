/* -*- mode: c++; indent-tabs-mode: nil -*- */

#ifndef _QORE_INTERN_POLLSTATECLOSEHOOK_H
#define _QORE_INTERN_POLLSTATECLOSEHOOK_H

#ifdef DEBUG
#include <cstdint>

//! Debug-only: a one-shot test hook that closes a poll operation right before its I/O thread sets an open state
/** The poll operations close from other threads too (e.g. abort() runs on any thread), while their I/O thread
    checks their state and then sets the next one; a close between the check and the store must not be undone (the
    closed state is final).  A test arms the hook for an operation and a store site; when the I/O thread reaches that
    site, the operation closes itself the way another thread does at that moment (see each operation's
    setStateUnlessClosed() or setOpenState()), then sets its state, and the test is signaled with whether the
    operation was still closed right after the store (later cycles of the I/O thread can close it again for other
    reasons, e.g. an aborted inner operation, so only this tells whether the store undid the close).

    The operation is identified by the address of its private data, which is compared only, never dereferenced by the
    hook: the test keeps the operation alive until the hook fired, or disarms it.
*/
class PollStateCloseHook {
public:
    //! Arms the hook for the operation @a op at @a site; @a op nullptr disarms it
    DLLLOCAL static void arm(const void* op, const char* site);

    //! Returns true and disarms the hook if it is armed for @a op at @a site; the caller then closes the operation
    //! and calls fired()
    DLLLOCAL static bool take(const void* op, const char* site);

    //! Signals the test that the hook fired; @a closed tells whether the operation was closed right after the store
    DLLLOCAL static void fired(bool closed);

    //! Returns whether the operation was closed right after the store of the hook that fired last
    DLLLOCAL static bool closedAfterStore();

    //! Waits until the hook fired; returns false on timeout
    DLLLOCAL static bool wait(int64_t timeout_ms);
};
#endif

#endif
