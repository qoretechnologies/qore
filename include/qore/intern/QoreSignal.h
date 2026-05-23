/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreSignal.h

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

// use SIGSYS for the status signal
#define QORE_STATUS_SIGNAL SIGSYS

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

   DLLLOCAL void init();
   DLLLOCAL void runHandler(int sig, ExceptionSink* xsink);
   DLLLOCAL bool isSet() const {
      return (bool)funcref;
   }
   DLLLOCAL QoreProgram* getProgram() const {
      return pgm;
   }
};

// map of signals to module names
typedef std::map<int, std::string> sig_map_t;

class QoreSignalManager {
   friend class QoreSignalManagerBusyHelper;

private:
   bool is_enabled;        // signal handling enabled?
   pthread_t ptid;         // handler thread
   int tid;                // handler thread TID
   QoreCounter tcount;     // thread counter, for synchronization only
   QoreCondition cond;     // to ensure atomicity of set and remove calls
   bool block;
   int waiting;

   DLLLOCAL void reload();
   DLLLOCAL void stop_signal_thread_unlocked();
   DLLLOCAL int start_signal_thread(ExceptionSink* xsink);
   DLLLOCAL void stop_signal_thread();
   DLLLOCAL void wait_signal_thread_stopped();
   DLLLOCAL void setMask(sigset_t& mask);

public:
   enum sig_cmd_e { C_None = 0, C_Reload = 1, C_Exit = 2 };

   // set of signals we are managing
   sigset_t mask;

   // set of signals we do not manage (empty at start)
   sig_map_t fmap;

   int num_handlers;
   bool thread_running;
   QoreSignalHandler handlers[QORE_SIGNAL_MAX];
   QoreThreadLock mutex;
   sig_cmd_e cmd;
   bool thread_joinable;

   DLLLOCAL QoreSignalManager();
   DLLLOCAL void init(bool disable_signal_mask = false);
   DLLLOCAL void del();
   DLLLOCAL int setHandler(int sig, const ResolvedCallReferenceNode* fr, ExceptionSink* xsink);
   DLLLOCAL int removeHandler(int sig, ExceptionSink* xsink);
   DLLLOCAL const char* getSignalName(int sig);
   DLLLOCAL void signal_handler_thread();
   DLLLOCAL void lock_idle();
   DLLLOCAL void release_idle();
   DLLLOCAL void start_handler();
   DLLLOCAL void end_handler();
   DLLLOCAL void preFork();
   DLLLOCAL void postFork(bool new_process, ExceptionSink* xsink);
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
