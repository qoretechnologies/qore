/*
    linenoise-module.cpp

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

#include "linenoise-module.h"

// Global variable definitions
QoreNamespace LinenoiseNS("Qore::Linenoise");

QoreRecursiveThreadLock lock;
thread_local int linenoise_lock_depth = 0;

//! the fork handler of the module lock (see QoreForkHandler.h)
/** The lock is held while a thread waits for terminal input and while user callbacks run, so fork() does not wait
    for it (QFR_MODULE_RESET): the child initializes it again and takes it again as often as the forking thread held
    it, since a callback can call fork()
*/
static QoreForkHandlerRegistration* linenoise_lock_fork_reg = nullptr;

static void linenoise_lock_fork_child(void*) {
    qore_fork_reinit_recursive_thread_lock(lock);
    for (int i = 0; i < linenoise_lock_depth; ++i) {
        lock.lock();
    }
}
ResolvedCallReferenceNode* callbackNode = nullptr;
ResolvedCallReferenceNode* hintsCallbackNode = nullptr;
ResolvedCallReferenceNode* syntaxCallbackNode = nullptr;

QoreObject* historyProviderObj = nullptr;
HistoryProviderPriv* historyProviderPriv = nullptr;

std::map<int, ResolvedCallReferenceNode*> keyBindingCallbacks;

std::map<int64_t, linenoiseState*> activeSessions;
int64_t nextSessionId = 1;

// Key code constants matching linenoise internal encoding
const int LN_META = 0x1c000000;
const int LN_CTRL = 0x0c000000;
const int LN_LEFT_ARROW_KEY = 0x10200000;
const int LN_RIGHT_ARROW_KEY = 0x10600000;
const int LN_UP_ARROW_KEY = 0x10800000;
const int LN_DOWN_ARROW_KEY = 0x10400000;
const int LN_HOME_KEY = 0x10a00000;
const int LN_END_KEY = 0x10c00000;
const int LN_DELETE_KEY = 0x10e00000;
const int LN_PAGE_UP_KEY = 0x11000000;
const int LN_PAGE_DOWN_KEY = 0x11200000;
const int LN_INSERT_KEY = 0x11400000;

// Parse a key specification string into an internal key code
// Formats: "ctrl-a", "meta-d", "ctrl-left", "meta-shift-d", "a", "space"
int parseKeySpec(const char* spec, ExceptionSink* xsink) {
    std::string s(spec);
    // convert to lowercase for comparison
    for (auto& ch : s) {
        ch = tolower(ch);
    }

    int modifiers = 0;

    // Parse modifiers
    while (true) {
        if (s.substr(0, 5) == "ctrl-" || s.substr(0, 5) == "ctrl+") {
            modifiers |= LN_CTRL;
            s = s.substr(5);
        } else if (s.substr(0, 5) == "meta-" || s.substr(0, 5) == "meta+" ||
                   s.substr(0, 4) == "alt-" || s.substr(0, 4) == "alt+") {
            modifiers |= LN_META;
            s = s.substr(s[3] == '-' || s[3] == '+' ? 4 : 5);
        } else {
            break;
        }
    }

    // Parse the key name
    int keyCode = 0;
    if (s == "left") {
        keyCode = LN_LEFT_ARROW_KEY;
    } else if (s == "right") {
        keyCode = LN_RIGHT_ARROW_KEY;
    } else if (s == "up") {
        keyCode = LN_UP_ARROW_KEY;
    } else if (s == "down") {
        keyCode = LN_DOWN_ARROW_KEY;
    } else if (s == "home") {
        keyCode = LN_HOME_KEY;
    } else if (s == "end") {
        keyCode = LN_END_KEY;
    } else if (s == "delete" || s == "del") {
        keyCode = LN_DELETE_KEY;
    } else if (s == "pageup" || s == "pgup") {
        keyCode = LN_PAGE_UP_KEY;
    } else if (s == "pagedown" || s == "pgdown") {
        keyCode = LN_PAGE_DOWN_KEY;
    } else if (s == "insert" || s == "ins") {
        keyCode = LN_INSERT_KEY;
    } else if (s == "space") {
        keyCode = ' ';
    } else if (s == "tab") {
        keyCode = 9;  // ctrl-I
    } else if (s == "return" || s == "enter") {
        keyCode = 13;  // ctrl-M
    } else if (s == "backspace") {
        keyCode = 8;  // ctrl-H
    } else if (s.length() == 1 && s[0] >= 'a' && s[0] <= 'z') {
        if (modifiers & LN_CTRL) {
            // ctrl-<char>: convert to control character (1-26)
            keyCode = s[0] - 'a' + 1;
            modifiers &= ~LN_CTRL;  // already encoded in keyCode
        } else {
            keyCode = s[0];
        }
    } else if (s.length() == 1) {
        keyCode = s[0];
    } else {
        if (xsink) {
            xsink->raiseException("LINENOISE-KEY-SPEC-ERROR",
                                  "invalid key specification: '%s'", spec);
        }
        return -1;
    }

    return keyCode | modifiers;
}

// C trampoline for user key bindings
static int qore_linenoise_user_key_cb(const char* line, int pos, char** new_line, int* new_pos, void* userData) {
    LinenoiseLockHelper al;

    ResolvedCallReferenceNode* cb = static_cast<ResolvedCallReferenceNode*>(userData);
    if (!cb) {
        return 0;
    }

    ExceptionSink xsink;

    // Create hash argument: {line: string, pos: int}
    ReferenceHolder<QoreHashNode> arg(new QoreHashNode(autoTypeInfo), &xsink);
    arg->setKeyValue("line", new QoreStringNode(line, QCS_UTF8), &xsink);
    arg->setKeyValue("pos", pos, &xsink);

    ValueHolder ret(&xsink);
    {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        args->push(arg.release(), nullptr);
        ret = cb->invoke(*args, &xsink);
    }

    if (xsink.isException()) {
        // Log exception info to stderr so it's not completely silent
        xsink.handleExceptions();
        return 0;
    }

    // If callback returns a hash, update the buffer via out params
    if (ret->getType() == NT_HASH) {
        QoreHashNode* h = ret->get<QoreHashNode>();
        QoreValue lineVal = h->getKeyValue("line");
        QoreValue posVal = h->getKeyValue("pos");
        if (lineVal.getType() == NT_STRING) {
            QoreStringValueHelper newLine(lineVal);
            *new_line = strdup(newLine->c_str());
        }
        if (posVal.getType() == NT_INT) {
            *new_pos = static_cast<int>(posVal.getAsBigInt());
        }
    }

    return 0;
}

static void qore_linenoise_completion_cb(const char *input, linenoiseCompletions *lc) {
    LinenoiseLockHelper al;

    if (!callbackNode) {
        return;
    }

    ExceptionSink xsink;

    ValueHolder ret(&xsink);
    {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        args->push(new QoreStringNode(input, QCS_UTF8), nullptr);
        ret = callbackNode->invoke(*args, &xsink);
    }

    // Check for exceptions from callback execution
    if (xsink.isException()) {
        // Log the exception but don't crash - completion just won't work
        xsink.clear();
        return;
    }

    if (ret->getType() != NT_LIST) {
        // Callback didn't return a list - silently ignore
        return;
    }

    QoreListNode* lst = ret->get<QoreListNode>();
    for (size_t i = 0; i < lst->size(); ++i) {
        QoreStringValueHelper str(lst->retrieveEntry(i));
        linenoiseAddCompletion(lc, str->c_str());
    }
}

static char* qore_linenoise_hints_cb(const char* input, int* color, int* bold) {
    LinenoiseLockHelper al;

    if (!hintsCallbackNode) {
        return NULL;
    }

    ExceptionSink xsink;

    ValueHolder ret(&xsink);
    {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        args->push(new QoreStringNode(input, QCS_UTF8), nullptr);
        ret = hintsCallbackNode->invoke(*args, &xsink);
    }

    if (xsink.isException()) {
        xsink.clear();
        return NULL;
    }

    if (ret->getType() == NT_STRING) {
        // Return plain string hint with default color (dark grey)
        *color = 90;  // bright black (dark grey)
        *bold = 0;
        QoreStringValueHelper str(*ret);
        return strdup(str->c_str());
    }

    if (ret->getType() == NT_HASH) {
        // Hash with {text, color, bold} keys
        QoreHashNode* h = ret->get<QoreHashNode>();
        QoreValue text = h->getKeyValue("text");
        if (text.getType() != NT_STRING) {
            return NULL;
        }
        QoreValue c = h->getKeyValue("color");
        *color = c.getType() == NT_INT ? (int)c.getAsBigInt() : 90;
        QoreValue b = h->getKeyValue("bold");
        *bold = b.getAsBool() ? 1 : 0;
        QoreStringValueHelper str(text);
        return strdup(str->c_str());
    }

    return NULL;
}

static void qore_linenoise_free_hints_cb(void* hint) {
    free(hint);
}

static char* qore_linenoise_syntax_cb(const char* buf) {
    LinenoiseLockHelper al;

    if (!syntaxCallbackNode) {
        return NULL;
    }

    ExceptionSink xsink;

    ValueHolder ret(&xsink);
    {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        args->push(new QoreStringNode(buf, QCS_UTF8), nullptr);
        ret = syntaxCallbackNode->invoke(*args, &xsink);
    }

    if (xsink.isException()) {
        xsink.clear();
        return NULL;
    }

    if (ret->getType() == NT_STRING) {
        QoreStringValueHelper str(*ret);
        return strdup(str->c_str());
    }

    return NULL;
}

// History provider C trampolines
char* qore_history_provider_prev_cb(const char* prefix, void* userData) {
    LinenoiseLockHelper al;
    if (!historyProviderPriv) {
        return NULL;
    }
    ExceptionSink xsink;
    char* rv = historyProviderPriv->callPrev(prefix, &xsink);
    if (xsink.isException()) {
        xsink.handleExceptions();
        return NULL;
    }
    return rv;
}

char* qore_history_provider_next_cb(const char* prefix, void* userData) {
    LinenoiseLockHelper al;
    if (!historyProviderPriv) {
        return NULL;
    }
    ExceptionSink xsink;
    char* rv = historyProviderPriv->callNext(prefix, &xsink);
    if (xsink.isException()) {
        xsink.handleExceptions();
        return NULL;
    }
    return rv;
}

void qore_history_provider_reset_cb(void* userData) {
    LinenoiseLockHelper al;
    if (!historyProviderPriv) {
        return;
    }
    ExceptionSink xsink;
    historyProviderPriv->callReset(&xsink);
    if (xsink.isException()) {
        xsink.handleExceptions();
    }
}

char* qore_history_provider_search_cb(const char* pattern, int direction, void* userData) {
    LinenoiseLockHelper al;
    if (!historyProviderPriv) {
        return NULL;
    }
    ExceptionSink xsink;
    char* rv = historyProviderPriv->callSearch(pattern, direction, &xsink);
    if (xsink.isException()) {
        xsink.handleExceptions();
        return NULL;
    }
    return rv;
}

// Module lifecycle functions
// takes the callback if it belongs to the Program
static void linenoise_take_program_call(ResolvedCallReferenceNode*& call, const QoreProgram* pgm,
        std::vector<ResolvedCallReferenceNode*>& calls) {
    if (call && call->getProgram() == pgm) {
        calls.push_back(call);
        call = nullptr;
    }
}

// the callbacks, the key bindings and the history provider of a Program end with it, while it can still run their
// destructors; kept after the Program is destroyed, they could not be called or destroyed
static void linenoise_program_cleanup(QoreProgram* pgm) {
    std::vector<ResolvedCallReferenceNode*> calls;
    QoreObject* provider_obj = nullptr;
    HistoryProviderPriv* provider_priv = nullptr;
    {
        LinenoiseLockHelper al;
        for (std::map<int, ResolvedCallReferenceNode*>::iterator i = keyBindingCallbacks.begin();
                i != keyBindingCallbacks.end();) {
            if (i->second && i->second->getProgram() == pgm) {
                linenoiseUnbindKey(i->first);
                calls.push_back(i->second);
                i = keyBindingCallbacks.erase(i);
            } else {
                ++i;
            }
        }
        linenoise_take_program_call(callbackNode, pgm, calls);
        linenoise_take_program_call(hintsCallbackNode, pgm, calls);
        linenoise_take_program_call(syntaxCallbackNode, pgm, calls);
        if (historyProviderObj && historyProviderObj->getProgram() == pgm) {
            linenoiseSetHistoryProvider(NULL, NULL, NULL, NULL, NULL);
            provider_obj = historyProviderObj;
            provider_priv = historyProviderPriv;
            historyProviderObj = nullptr;
            historyProviderPriv = nullptr;
        }
    }
    if (calls.empty() && !provider_obj) {
        return;
    }
    // released outside the lock, as a destructor runs Qore code
    ExceptionSink xsink;
    for (ResolvedCallReferenceNode* call : calls) {
        call->deref(&xsink);
    }
    if (provider_priv) {
        provider_priv->deref(&xsink);
    }
    if (provider_obj) {
        provider_obj->deref(&xsink);
    }
}

static void linenoise_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    assert(!linenoise_lock_fork_reg);
    linenoise_lock_fork_reg = qore_register_fork_handler(QFR_MODULE_RESET, "linenoise", nullptr, nullptr,
        linenoise_lock_fork_child, nullptr);
    if (!linenoise_lock_fork_reg) {
        xsink.outOfMemory();
        return;
    }

    LinenoiseNS.addSystemClass(initAbstractHistoryProviderClass(LinenoiseNS));
    init_linenoise_functions(LinenoiseNS);

    linenoiseSetCompletionCallback(qore_linenoise_completion_cb);
    linenoiseSetHintsCallback(qore_linenoise_hints_cb);
    linenoiseSetFreeHintsCallback(qore_linenoise_free_hints_cb);
    linenoiseSetSyntaxCallback(qore_linenoise_syntax_cb);
    linenoiseSetUserKeyCallback(qore_linenoise_user_key_cb);

    qore_register_program_cleanup_callback(linenoise_program_cleanup);
}

static void linenoise_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(LinenoiseNS.copy());
}

static void linenoise_module_delete() {
    qore_unregister_fork_handler(linenoise_lock_fork_reg);
    linenoise_lock_fork_reg = nullptr;
    qore_deregister_program_cleanup_callback(linenoise_program_cleanup);
    ExceptionSink xsink;
    if (callbackNode) {
        callbackNode->deref(&xsink);
    }
    if (hintsCallbackNode) {
        hintsCallbackNode->deref(&xsink);
    }
    if (syntaxCallbackNode) {
        syntaxCallbackNode->deref(&xsink);
    }
    if (historyProviderPriv) {
        historyProviderPriv->deref(&xsink);
        historyProviderPriv = nullptr;
    }
    if (historyProviderObj) {
        historyProviderObj->deref(&xsink);
        historyProviderObj = nullptr;
    }
    for (auto& kv : keyBindingCallbacks) {
        if (kv.second) {
            kv.second->deref(&xsink);
        }
    }
    keyBindingCallbacks.clear();
    for (auto& kv : activeSessions) {
        if (kv.second) {
            linenoiseEditStop(kv.second);
        }
    }
    activeSessions.clear();
    linenoiseHistoryFree();
}

extern "C" DLLEXPORT void linenoise_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "linenoise";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "Linenoise module";
    mod_info.author = "Petr Vanek";
    mod_info.url = "http://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = linenoise_module_init;
    mod_info.ns_init = linenoise_module_ns_init;
    mod_info.del = linenoise_module_delete;
    mod_info.license = QL_LGPL;
    mod_info.license_str = "LGPL";
}
