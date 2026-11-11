/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreSignal.h

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

#ifdef HAVE_SIGNAL_HANDLING
#ifndef _QORE_QORESIGNAL_H

#define _QORE_QORESIGNAL_H

#include <qore/QoreThreadLock.h>
#include <qore/QoreCondition.h>
#include <qore/QoreCounter.h>

#include <csignal>
#include <map>
#include <string>

// maximum number of signals
#ifndef QORE_SIGNAL_MAX
#ifdef NSIG
#define QORE_SIGNAL_MAX (NSIG+1)
#elif defined _NSIG
#define QORE_SIGNAL_MAX _NSIG
#elif defined _NSIGS
#define QORE_SIGNAL_MAX _NSIGS
#elif defined __DARWIN_NSIG
#define QORE_SIGNAL_MAX (__DARWIN_NSIG+1)
#else
#error do not know maximum signal number on this platform
#endif
#endif

// the signal that wakes the signal handling thread (issue #5787)
/* On Linux a real-time signal: valgrind delivers a real-time signal only at system calls, while it treats SIGSYS as a
   synchronous fault signal, which it can deliver with its default action ("Bad system call") or deadlock on when
   another thread sends it.  SIGRTMIN is not a constant (the C library reserves the first real-time signals for
   itself), so the signal is determined once, at the first use, and the same signal is used from then on.  Other
   platforms (macOS has no real-time signals) keep SIGSYS.
*/
#if defined(__linux__) && defined(SIGRTMIN)
//! returns the signal that wakes the signal handling thread: SIGRTMIN + 1, determined once
DLLLOCAL int qore_get_status_signal();
#define QORE_STATUS_SIGNAL (qore_get_status_signal())
#define QORE_STATUS_SIGNAL_DESC "QORE (SIGRTMIN+1 for internal use)"
#else
#define QORE_STATUS_SIGNAL SIGSYS
#define QORE_STATUS_SIGNAL_DESC "QORE (SIGSYS for internal use)"
#endif

class CodePgm {
public:
   ResolvedCallReferenceNode* funcref;
   QoreProgram* pgm;

   DLLLOCAL void setProgram(QoreProgram* n_pgm) {
      assert(n_pgm);
      pgm = n_pgm;
      pgm->ref();
      //printd(5, "CodePgm::setProgram() pgm: %p %d -> %d\n", pgm, pgm->reference_count() - 1, pgm->reference_count());
   }

   DLLLOCAL CodePgm() : funcref(0), pgm(0) {
   }

   DLLLOCAL CodePgm(ResolvedCallReferenceNode* f, QoreProgram* p) : funcref(f), pgm(p) {
   }

   // must be called in the signal lock
   DLLLOCAL void set(const ResolvedCallReferenceNode* n_funcref, QoreProgram* n_pgm) {
      assert(!funcref);
      assert(!pgm);

      funcref = n_funcref->refRefSelf();
      setProgram(n_pgm);
   }

   // must be called in the signal lock
   DLLLOCAL CodePgm replace(const ResolvedCallReferenceNode* n_funcref, QoreProgram* n_pgm) {
      assert(funcref);
      assert(pgm);
      assert(n_funcref);
      CodePgm rv(funcref, pgm);

      funcref = n_funcref->refRefSelf();
      setProgram(n_pgm);
      return rv;
   }

   // must be called in the signal lock; returns a new reference to the callback for a caller that runs it outside
   // the lock, where the handler can be replaced or removed and the handler's own reference released
   DLLLOCAL ResolvedCallReferenceNode* refCallback() const {
      assert(funcref);
      return funcref->refRefSelf();
   }

   // must be called in the signal lock; returns a new reference to the Program, as refCallback()
   DLLLOCAL QoreProgram* refProgram() const {
      assert(pgm);
      pgm->ref();
      return pgm;
   }

   // must be called in the signal lock
   DLLLOCAL CodePgm take() {
      assert(funcref);
      CodePgm rv(funcref, pgm);
      funcref = 0;
      pgm = 0;
      return rv;
   }

   DLLLOCAL void del(ExceptionSink* xsink) {
      if (funcref) {
         funcref->deref(xsink);
         assert(pgm);
         //printd(5, "CodePgm::del() pgm: %p %d -> %d\n", pgm, pgm->reference_count() + 1, pgm->reference_count());
         pgm->deref(xsink);
      }
   }
};

class QoreSignalHandler : public CodePgm {
public:
   enum sh_status_e { SH_OK = 0, SH_InProgress = 1, SH_Delete = 2 };
   sh_status_e status;
   //! the TID of a thread that waits for the callback of the removed handler to return and releases the handler, or
   //! -1 (see QoreSignalManager::removeProgramHandlers())
   int release_tid = -1;

   DLLLOCAL void init();
   //! runs the callback with the signal number; the caller holds a reference to the callback
   DLLLOCAL static void runHandler(const ResolvedCallReferenceNode& callback, int sig, ExceptionSink* xsink);
   DLLLOCAL bool isSet() const {
      return (bool)funcref;
   }
};

// map of signals to module names
typedef std::map<int, std::string> sig_map_t;

#ifdef DEBUG
//! a test hook called by a thread that starts waiting for a fork() block to be lifted, with the mutex held
DLLLOCAL extern std::atomic<void (*)()> qore_qsm_block_wait_hook;
#endif

class QoreSignalManager {
   friend class QoreSignalManagerBusyHelper;

private:
   bool is_enabled;        // signal handling enabled?
   pthread_t ptid;         // handler thread
   int tid;                // handler thread TID
   QoreCounter tcount;     // thread counter, for synchronization only
   QoreCondition cond;     // to ensure atomicity of set and remove calls
   bool block;
   //! the TID of the thread that set block in preFork(), or -1
   int block_tid = -1;
   int waiting;
   // Includes callback and thread-resource cleanup; protected by mutex.
   bool handler_running = false;
   //! the number of threads waiting for handler_running to be reset; see removeProgramHandlers()
   int idle_waiting = 0;
   //! handlers removed while their callbacks ran, which the signal thread hands over to the threads whose TIDs are
   //! given, which wait in removeProgramHandlers() and release them
   std::vector<std::pair<int, CodePgm>> released_handlers;
#ifdef DEBUG
   //! debug builds: the signal thread is held after the next callback has left its Program (see
   //! dbgHoldAfterCallback())
   bool dbg_hold_armed = false;
   //! debug builds: the signal thread is held
   bool dbg_held = false;
#endif

   //! resets handler_running and wakes the threads waiting for it; the mutex must be held
   DLLLOCAL void setHandlerIdleUnlocked() {
      handler_running = false;
      if (idle_waiting) {
         cond.broadcast();
      }
   }

   //! removes the handler of the signal; the mutex must be held
   /** @param sig the signal
       @param old receives the handler removed, which the caller releases outside the lock

       @return true if the handler's callback is running: it is then removed by the signal thread once it returns
   */
   DLLLOCAL bool removeHandlerUnlocked(int sig, CodePgm& old);

   DLLLOCAL void reload();
   //! waits while another thread's fork() blocks changes; the mutex must be held
   DLLLOCAL void waitForBlockUnlocked();
   DLLLOCAL void wake_signal_thread_unlocked();
   DLLLOCAL void stop_signal_thread_unlocked();
   DLLLOCAL int start_signal_thread(ExceptionSink* xsink);
   DLLLOCAL void stop_signal_thread();
   DLLLOCAL void wait_signal_thread_stopped();
   DLLLOCAL void setMask(sigset_t& mask);
   //! Returns the signals that the signal thread waits for; the signal manager's mutex must be held
   DLLLOCAL void getWaitMask(sigset_t& wait_mask);
   //! releases a signal taken over by a module; the signal manager's mutex must be held
   /** @param i the entry of the signal in fmap
   */
   DLLLOCAL void releaseSignalUnlocked(sig_map_t::iterator i);

public:
   enum sig_cmd_e { C_None = 0, C_Reload = 1, C_Exit = 2 };

   // set of signals we are managing
   sigset_t mask;

   // set of signals we do not manage (empty at start)
   sig_map_t fmap;

   //! the signals the signal thread waits for by default (without a Qore-language handler)
   sigset_t default_wait_mask;

   //! the signals that were ignored when their handler was set; they are ignored again when it is removed
   sigset_t ignored_with_handler;

   //! the signals reserved by the library itself at initialization (see setMask())
   sig_map_t sys_fmap;

   int num_handlers;
   bool thread_running;
   QoreSignalHandler handlers[QORE_SIGNAL_MAX];
   QoreThreadLock mutex;
   sig_cmd_e cmd;
   bool thread_joinable;

   DLLLOCAL QoreSignalManager();
   DLLLOCAL void init(bool disable_signal_mask = false);
   DLLLOCAL void del();
   // Join the idle signal thread before C exit(); an active callback requires _Exit().
   DLLLOCAL bool stopForExit();
   DLLLOCAL int setHandler(int sig, const ResolvedCallReferenceNode* fr, ExceptionSink* xsink);
   DLLLOCAL int removeHandler(int sig, ExceptionSink* xsink);
   //! returns true if a handler is set for the signal (for tests)
   DLLLOCAL bool hasHandler(int sig);
   //! removes the handlers of the given Programs; waits for a callback of one of them that runs in the signal thread
   /** Called by the module cleanup for the Programs of the modules before it clears their data
       (QoreModuleManager::delUser()): the signal thread, which can run until the end of the cleanup, then runs none
       of their code while it does
   */
   DLLLOCAL void removeProgramHandlers(const std::vector<QoreProgram*>& pgms, ExceptionSink* xsink);
#ifdef DEBUG
   //! debug builds: holds the signal thread after the next callback has left its Program, until a thread waits for
   //! it in removeProgramHandlers() (the deletion of the callback's Program, for example), at most 30 seconds
   /** For the tests of a handler whose callback finishes in the signal thread while its Program is deleted (issue
       #5766)
   */
   DLLLOCAL void dbgHoldAfterCallback();
   //! debug builds: waits until the signal thread is held by dbgHoldAfterCallback(); returns false on timeout
   DLLLOCAL bool dbgWaitHeldAfterCallback(int timeout_ms);
#endif
   DLLLOCAL const char* getSignalName(int sig);
   DLLLOCAL void signal_handler_thread();
   DLLLOCAL void lock_idle();
   DLLLOCAL void release_idle();
   DLLLOCAL void start_handler();
   DLLLOCAL void end_handler();
   DLLLOCAL void preFork();
   DLLLOCAL void postFork(bool new_process, ExceptionSink* xsink);
   //! acquires the locks before fork(), whoever calls it (see QoreForkHandlers.h)
   DLLLOCAL void forkPrepare();
   //! releases the locks in the parent after fork()
   DLLLOCAL void forkParent();
   //! makes the signal manager consistent in the child after fork(), where only the forking thread exists
   /** @param fork_tid the TID of the thread that called fork(), or -1 for a thread unknown to Qore
   */
   DLLLOCAL void forkChild(int fork_tid);
   DLLLOCAL int q_gettid() {
      return tid;
   }
   DLLLOCAL void reset_default_signal_mask();
   DLLLOCAL bool running() { return tid != -1; }
   DLLLOCAL bool enabled() { return is_enabled; }

   // try to allow the signal to be managed externally (by a module)
   // sig = signal number, name = name of module to manage signal
   // returns 0 for OK, or an error string on error
   DLLLOCAL QoreStringNode* reassignSignal(int sig, const char* name, bool reuse_sys = false);

   // 0 = OK, -1 = no signals allocated (all or nothing)
   DLLLOCAL QoreStringNode* reassignSignals(const sig_vec_t& sig_vec, const char* name, bool reuse_sys = false);

   DLLLOCAL int releaseSignal(int sig, const char* name);

   // 0 = OK, -1 = no signals release (all or nothing)
   DLLLOCAL int releaseSignals(const sig_vec_t& sig_vec, const char* name);
};

DLLLOCAL extern QoreSignalManager QSM;

#endif // _QORE_QORESIGNAL_H
#endif // HAVE_SIGNAL_HANDLING
