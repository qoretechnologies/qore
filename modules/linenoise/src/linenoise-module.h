/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    linenoise-module.h

    Qore Programming Language

    Copyright 2012 - 2026 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef LINENOISE_MODULE_H
#define LINENOISE_MODULE_H

#include <qore/Qore.h>

#include <map>
#include <string>

extern "C" {
#include "linenoise.h"
}

#include "QC_AbstractHistoryProvider.h"

// Module namespace
extern QoreNamespace LinenoiseNS;

// Thread lock for module state
extern QoreRecursiveThreadLock lock;

// Callback references
extern ResolvedCallReferenceNode* callbackNode;
extern ResolvedCallReferenceNode* hintsCallbackNode;
extern ResolvedCallReferenceNode* syntaxCallbackNode;

// History provider state
extern QoreObject* historyProviderObj;
extern HistoryProviderPriv* historyProviderPriv;

// User key binding callback storage
extern std::map<int, ResolvedCallReferenceNode*> keyBindingCallbacks;

// Async editing session management
extern std::map<int64_t, linenoiseState*> activeSessions;
extern int64_t nextSessionId;

// Key code constants matching linenoise internal encoding
extern const int LN_META;
extern const int LN_CTRL;
extern const int LN_LEFT_ARROW_KEY;
extern const int LN_RIGHT_ARROW_KEY;
extern const int LN_UP_ARROW_KEY;
extern const int LN_DOWN_ARROW_KEY;
extern const int LN_HOME_KEY;
extern const int LN_END_KEY;
extern const int LN_DELETE_KEY;
extern const int LN_PAGE_UP_KEY;
extern const int LN_PAGE_DOWN_KEY;
extern const int LN_INSERT_KEY;

// Parse a key specification string into an internal key code
DLLLOCAL int parseKeySpec(const char* spec, ExceptionSink* xsink);

// History provider C trampolines (used by both linenoise-module.cpp and ql_linenoise.qpp)
DLLLOCAL char* qore_history_provider_prev_cb(const char* prefix, void* userData);
DLLLOCAL char* qore_history_provider_next_cb(const char* prefix, void* userData);
DLLLOCAL void qore_history_provider_reset_cb(void* userData);
DLLLOCAL char* qore_history_provider_search_cb(const char* pattern, int direction, void* userData);

// Initialize linenoise Qore function bindings in the given namespace
DLLLOCAL void init_linenoise_functions(QoreNamespace& ns);

// Initialize the AbstractHistoryProvider class
DLLLOCAL QoreClass* initAbstractHistoryProviderClass(QoreNamespace& ns);

#endif // LINENOISE_MODULE_H
