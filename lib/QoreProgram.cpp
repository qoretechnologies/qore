/*
    QoreProgram.cpp

    Program QoreObject Definition

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
#include <qore/Restrictions.h>
#include <qore/QoreCounter.h>
#include <qore/QoreSandboxManager.h>
#include <qore/ParseOptionMap.h>
#include <chrono>
#include "qore/intern/QoreThreadList.h"
#include "qore/intern/QoreSignal.h"
#include "qore/intern/LocalVar.h"
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_aot_deps.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreClassList.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreTypeInfo.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/CallReferenceCallNode.h"
#include "qore/intern/DoWhileStatement.h"
#include "qore/intern/ExpressionStatement.h"
#include "qore/intern/ForEachStatement.h"
#include "qore/intern/ForStatement.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/IfStatement.h"
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/QoreQuestionMarkOperatorNode.h"
#include "qore/intern/ReturnStatement.h"
#include "qore/intern/ScopedObjectCallNode.h"
#include "qore/intern/SwitchStatement.h"
#include "qore/intern/ThrowStatement.h"
#include "qore/intern/WhileStatement.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/QC_Breakpoint.h"
#include "qore/intern/ModuleInfo.h"
#include "qore/intern/QoreSerializable.h"
#include "qore/intern/QoreJIT.h"
#include "qore/intern/QoreForkHandlers.h"
#include "qore/intern/QoreProgramObjectLockOps.h"
#include "qore/intern/QoreSandboxProvenance.h"

#include <string>
#include <set>
#include <typeinfo>
#include <utility>
#include <vector>

extern bool threads_initialized;
#include <algorithm>
#include <functional>
#include <memory>
#include <condition_variable>
#include <new>
#include <map>

ParseOptionMaps pomaps;

// the insertions into the value sets of the parse options are not under way when fork() copies the process
// (issue #6177; see SyncParseOptions)
std::mutex qore_parse_options_values_mutex;
static QoreForkStdMutex parse_options_values_fork_handler(qore_parse_options_values_mutex, QFLR_LEAF,
    "parse_options_values");

// Optional callback invoked before program data is cleared (issue #4816)
// Can be used by modules to perform cleanup before namespace data is freed
// More than one module needs this notification, so it is a list; a single slot meant whichever
// module registered last silently disabled every other one.  Function-local statics avoid any
// dependency on static initialization order, since modules can register from their own init.
//
// The lock and the list are allocated once and deliberately never destroyed.  Programs are torn
// down from static destructors and atexit handlers while exit() runs, and objects with static
// storage duration are destroyed in reverse order of construction.  A module registers its
// callback from its module init function, which runs after the module's own static objects have
// already been registered for destruction, so a destructible container here would be destroyed
// first and then read by the very teardown it exists to serve: the read sees a freed buffer, the
// size still looks non-empty because the vector's members are untouched, and the call jumps to
// whatever now occupies that heap memory.  Immortal objects keep the list usable for the entire
// life of the process, which is what the previous single function-pointer slot got for free.
static QoreThreadLock& program_cleanup_lock() {
    static QoreThreadLock* l = new QoreThreadLock;
    return *l;
}

// the cleanup callback list can be used in a child process created with fork() (see QoreForkHandlers.h)
static QoreForkThreadLock program_cleanup_fork_handler(program_cleanup_lock(), QFLR_LEAF, "program_cleanup");

typedef std::vector<qore_program_cleanup_callback_t> program_cleanup_callback_list_t;

static program_cleanup_callback_list_t& program_cleanup_callbacks() {
    static program_cleanup_callback_list_t* l = new program_cleanup_callback_list_t;
    return *l;
}

void qore_register_program_cleanup_callback(qore_program_cleanup_callback_t callback) {
    assert(callback);
    AutoLocker al(program_cleanup_lock());
    program_cleanup_callback_list_t& l = program_cleanup_callbacks();
    // registering twice would both grow the list without bound and call the module back twice per
    // Program, so repeat registrations (ex: a module loaded again) are ignored
    if (std::find(l.begin(), l.end(), callback) == l.end()) {
        l.push_back(callback);
    }
}

void qore_deregister_program_cleanup_callback(qore_program_cleanup_callback_t callback) {
    assert(callback);
    AutoLocker al(program_cleanup_lock());
    program_cleanup_callback_list_t& l = program_cleanup_callbacks();
    program_cleanup_callback_list_t::iterator i = std::find(l.begin(), l.end(), callback);
    if (i != l.end()) {
        l.erase(i);
    }
}

void qore_call_program_cleanup_callback(QoreProgram* pgm) {
    assert(pgm);
    // teardown reaches this from more than one place; the callbacks must run once per Program
    if (!qore_program_private::get(*pgm)->takeCleanupCallbackFlag()) {
        return;
    }
    // copy the list and call outside the lock: a callback runs module code that must not be
    // serialized against registration, and must not be able to deadlock against it
    program_cleanup_callback_list_t l;
    {
        AutoLocker al(program_cleanup_lock());
        if (program_cleanup_callbacks().empty()) {
            return;
        }
        l = program_cleanup_callbacks();
    }
    for (auto& callback : l) {
        callback(pgm);
    }
}

// note the number and order of the warnings has to correspond to those in QoreProgram.h
static const char* qore_warnings_l[] = {
    "warning-mask-unchanged",
    "duplicate-local-vars",
    "unknown-warning",
    "undeclared-var",
    "duplicate-global-vars",
    "unreachable-code",
    "non-existent-method-call",
    "invalid-operation",
    "call-with-type-errors",
    "return-value-ignored",
    "deprecated",
    "excess-args",
    "duplicate-hash-key",
    "unreferenced-variable",
    "duplicate-block-vars",
    "module-only",
    "broken-logic-precedence",
    "invalid-catch",
    "ambiguous-call-resolution",
    "ambiguous-overload",
    "non-exhaustive-switch",
    "language-traps",
};
#define NUM_WARNINGS (sizeof(qore_warnings_l)/sizeof(const char* ))

void ParseOptionMaps::doMap(int64 code, const char* desc, const char* dom) {
    assert(pomap.find(code) == pomap.end());
    assert(pormap.find(desc) == pormap.end());
    pomap[code] = desc;
    pormap[desc] = code;

    if (dom) {
        assert(dommap.find(code) == dommap.end());
        assert(domrmap.find(desc) == domrmap.end());
        dommap[code] = dom;
        domrmap[dom] = code;
    }
}

namespace {
//! the parse options making up QORE_IR_REQUIRED_PARSE_OPTIONS, with the directives that set and clear them
struct IrRequiredParseOption {
    int64 code;
    const char* name;
    const char* directive;
};

constexpr IrRequiredParseOption ir_required_parse_options[] = {
    {PO_ALLOW_BARE_REFS, "PO_ALLOW_BARE_REFS", "%allow-bare-refs; cleared by %require-dollar and %old-style"},
    {PO_ASSUME_LOCAL, "PO_ASSUME_LOCAL", "%assume-local; cleared by %assume-global and %old-style"},
    {PO_REQUIRE_TYPES, "PO_REQUIRE_TYPES", "%require-types"},
    {PO_STRICT_ARGS, "PO_STRICT_ARGS", "%strict-args; cleared by %loose-args"},
    {PO_STRONG_ENCAPSULATION, "PO_STRONG_ENCAPSULATION", "%strong-encapsulation"},
};

constexpr int64 ir_required_parse_option_union() {
    int64 rv = 0;
    for (const IrRequiredParseOption& i : ir_required_parse_options) {
        rv |= i.code;
    }
    return rv;
}

// the description must name every required option
static_assert(ir_required_parse_option_union() == QORE_IR_REQUIRED_PARSE_OPTIONS,
    "ir_required_parse_options does not match QORE_IR_REQUIRED_PARSE_OPTIONS");
}

std::string qore_describe_missing_ir_parse_options(const QoreParseOptions& po) {
    std::string rv;
    for (const IrRequiredParseOption& i : ir_required_parse_options) {
        if ((po & QoreParseOptions(i.code)) == QoreParseOptions(i.code)) {
            continue;
        }
        if (!rv.empty()) {
            rv += ", ";
        }
        rv += i.name;
        rv += " (";
        rv += i.directive;
        rv += ")";
    }
    assert(rv.empty() == qore_parse_options_support_ir(po));
    return rv;
}

namespace {
//! a functional domain as named in a \c [dom=...] attribute list and a module's \c domains key
/** The names are those of the \c DOM_* constants and of the reflection API (getDomainStringList()); qore#5843
*/
struct SourceDomainName {
    const char* name;
    int64 dom;
    //! the parse option that restricts the domain, or that is required for an "allow" domain
    const char* po_name;
    //! true for domains that require a PO_ALLOW_* option instead of being restricted by a PO_NO_* option
    bool allow;
};

constexpr SourceDomainName source_domain_names[] = {
    {"DEFAULT", QDOM_DEFAULT, "PO_DEFAULT", false},
    {"PROCESS_CONTROL", QDOM_PROCESS, "PO_NO_PROCESS_CONTROL", false},
    {"NETWORK", QDOM_NETWORK, "PO_NO_NETWORK", false},
    {"EXTERNAL_PROCESS", QDOM_EXTERNAL_PROCESS, "PO_NO_EXTERNAL_PROCESS", false},
    {"FILESYSTEM", QDOM_FILESYSTEM, "PO_NO_FILESYSTEM", false},
    {"THREAD_CLASS", QDOM_THREAD_CLASS, "PO_NO_THREAD_CLASSES", false},
    {"THREAD_CONTROL", QDOM_THREAD_CONTROL, "PO_NO_THREAD_CONTROL", false},
    {"DATABASE", QDOM_DATABASE, "PO_NO_DATABASE", false},
    {"GUI", QDOM_GUI, "PO_NO_GUI", false},
    {"TERMINAL_IO", QDOM_TERMINAL_IO, "PO_NO_TERMINAL_IO", false},
    {"EXTERNAL_INFO", QDOM_EXTERNAL_INFO, "PO_NO_EXTERNAL_INFO", false},
    {"THREAD_INFO", QDOM_THREAD_INFO, "PO_NO_THREAD_INFO", false},
    {"LOCALE_CONTROL", QDOM_LOCALE_CONTROL, "PO_NO_LOCALE_CONTROL", false},
    {"MODULE", QDOM_MODULES, "PO_NO_MODULES", false},
    {"IN_MODULE", QDOM_IN_MODULE, "PO_IN_MODULE", false},
    {"EMBEDDED_LOGIC", QDOM_EMBEDDED_LOGIC, "PO_NO_EMBEDDED_LOGIC", false},
    {"REFLECTION", QDOM_REFLECTION, "PO_NO_REFLECTION", false},
    {"UNCONTROLLED_API", QDOM_UNCONTROLLED_API, "PO_NO_UNCONTROLLED_APIS", false},
    {"INJECTION", QDOM_INJECTION, "PO_ALLOW_INJECTION", true},
    {"DEBUGGER", QDOM_DEBUGGER, "PO_ALLOW_DEBUGGER", true},
};

//! other names accepted for a domain in Qore source: the names that qpp uses
constexpr SourceDomainName source_domain_aliases[] = {
    {"PROCESS", QDOM_PROCESS, "PO_NO_PROCESS_CONTROL", false},
    {"MODULES", QDOM_MODULES, "PO_NO_MODULES", false},
};

constexpr int64 source_domain_union() {
    int64 rv = 0;
    for (const SourceDomainName& i : source_domain_names) {
        rv |= i.dom;
    }
    return rv;
}

// every functional domain can be declared in Qore source
static_assert(source_domain_union() == QDOM_ALL, "source_domain_names does not match QDOM_ALL");
}

int64 qore_get_source_domain(const char* name) {
    assert(name);
    for (const SourceDomainName& i : source_domain_names) {
        if (!strcasecmp(i.name, name)) {
            return i.dom;
        }
    }
    for (const SourceDomainName& i : source_domain_aliases) {
        if (!strcasecmp(i.name, name)) {
            return i.dom;
        }
    }
    return -1;
}

std::string qore_get_source_domain_names(int64 dom) {
    std::string rv;
    for (const SourceDomainName& i : source_domain_names) {
        if (!i.dom || (dom & i.dom) != i.dom) {
            continue;
        }
        if (!rv.empty()) {
            rv += ',';
        }
        rv += i.name;
    }
    return rv;
}

std::string q_describe_functional_domains(int64 dom) {
    return qore_describe_source_domains(dom);
}

std::string qore_describe_source_domains(int64 dom) {
    std::string rv;
    for (const SourceDomainName& i : source_domain_names) {
        if (!i.dom || (dom & i.dom) != i.dom) {
            continue;
        }
        if (!rv.empty()) {
            rv += ", ";
        }
        rv += i.name;
        rv += i.allow ? " (requires " : " (restricted by ";
        rv += i.po_name;
        rv += ')';
    }
    return rv;
}

ParseOptionMaps::ParseOptionMaps() {
    doMap(PO_NO_GLOBAL_VARS, "PO_NO_GLOBAL_VARS");
    doMap(PO_NO_SUBROUTINE_DEFS, "PO_NO_SUBROUTINE_DEFS");
    doMap(PO_NO_THREAD_CONTROL, "PO_NO_THREAD_CONTROL", "THREAD_CONTROL");
    doMap(PO_NO_THREAD_CLASSES, "PO_NO_THREAD_CLASSES", "THREAD_CLASS");
    doMap(PO_NO_TOP_LEVEL_STATEMENTS, "PO_NO_TOP_LEVEL_STATEMENTS");
    doMap(PO_ALLOW_REPARSE, "PO_ALLOW_REPARSE");
    doMap(PO_NO_INHERIT_SYSTEM_CLASSES, "PO_NO_INHERIT_SYSTEM_CLASSES");
    doMap(PO_NO_INHERIT_USER_CLASSES, "PO_NO_INHERIT_USER_CLASSES");
    doMap(PO_NO_CHILD_PO_RESTRICTIONS, "PO_NO_CHILD_PO_RESTRICTIONS");
    doMap(PO_NO_EXTERNAL_PROCESS, "PO_NO_EXTERNAL_PROCESS", "EXTERNAL_PROCESS");
    doMap(PO_REQUIRE_OUR, "PO_REQUIRE_OUR");
    doMap(PO_NO_PROCESS_CONTROL, "PO_NO_PROCESS_CONTROL", "PROCESS_CONTROL");
    doMap(PO_NO_NETWORK, "PO_NO_NETWORK", "NETWORK");
    doMap(PO_NO_FILESYSTEM, "PO_NO_FILESYSTEM", "FILESYSTEM");
    doMap(PO_LOCK_WARNINGS, "PO_LOCK_WARNINGS");
    doMap(PO_NO_DATABASE, "PO_NO_DATABASE", "DATABASE");
    doMap(PO_NO_GUI, "PO_NO_GUI", "GUI");
    doMap(PO_NO_TERMINAL_IO, "PO_NO_TERMINAL_IO", "TERMINAL_IO");
    doMap(PO_REQUIRE_TYPES, "PO_REQUIRE_TYPES");
    doMap(PO_NO_EXTERNAL_INFO, "PO_NO_EXTERNAL_INFO", "EXTERNAL_INFO");
    doMap(PO_NO_THREAD_INFO, "PO_NO_THREAD_INFO", "THREAD_INFO");
    doMap(PO_NO_LOCALE_CONTROL, "PO_NO_LOCALE_CONTROL", "LOCALE_CONTROL");
    doMap(PO_REQUIRE_PROTOTYPES, "PO_REQUIRE_PROTOTYPES");
    doMap(PO_STRICT_ARGS, "PO_STRICT_ARGS");
    doMap(PO_ASSUME_LOCAL, "PO_ASSUME_LOCAL");
    doMap(PO_NO_MODULES, "PO_NO_MODULES", "MODULE");
    doMap(PO_NO_INHERIT_USER_FUNC_VARIANTS, "PO_NO_INHERIT_USER_FUNC_VARIANTS");
    doMap(PO_NO_INHERIT_SYSTEM_FUNC_VARIANTS, "PO_NO_INHERIT_SYSTEM_FUNC_VARIANTS");
    doMap(PO_NO_INHERIT_GLOBAL_VARS, "PO_NO_INHERIT_GLOBAL_VARS");
    doMap(PO_IN_MODULE, "PO_IN_MODULE");
    doMap(PO_NO_EMBEDDED_LOGIC, "PO_NO_EMBEDDED_LOGIC", "EMBEDDED_LOGIC");
    doMap(PO_STRICT_BOOLEAN_EVAL, "PO_STRICT_BOOLEAN_EVAL");
    doMap(PO_DEFAULT, "PO_DEFAULT");
    doMap(PO_ALLOW_BARE_REFS, "PO_ALLOW_BARE_REFS");
    doMap(PO_NO_THREADS, "PO_NO_THREADS");
    doMap(PO_NO_EXTERNAL_ACCESS, "PO_NO_EXTERNAL_ACCESS");
    doMap(PO_NO_IO, "PO_NO_IO");
    doMap(PO_LOCKDOWN, "PO_LOCKDOWN");
    doMap(PO_NEW_STYLE, "PO_NEW_STYLE");
    doMap(PO_ALLOW_INJECTION, "PO_ALLOW_INJECTION", "INJECTION");
    doMap(PO_NO_INHERIT_SYSTEM_CONSTANTS, "PO_NO_INHERIT_SYSTEM_CONSTANTS");
    doMap(PO_NO_INHERIT_USER_CONSTANTS, "PO_NO_INHERIT_USER_CONSTANTS");
    doMap(PO_BROKEN_LIST_PARSING, "PO_BROKEN_LIST_PARSING");
    doMap(PO_BROKEN_LOGIC_PRECEDENCE, "PO_BROKEN_LOGIC_PRECEDENCE");
    doMap(PO_BROKEN_LOOP_STATEMENT, "PO_BROKEN_LOOP_STATEMENT");
    doMap(PO_BROKEN_REFERENCES, "PO_BROKEN_REFERENCES");
    doMap(PO_NO_UNCONTROLLED_APIS, "PO_NO_UNCONTROLLED_APIS", "UNCONTROLLED_API");
    // 46
    doMap(PO_NO_DEBUGGING, "PO_NO_DEBUGGING");
    // 47
    doMap(PO_NO_INHERIT_USER_HASHDECLS, "PO_NO_INHERIT_USER_HASHDECLS");
    // 48
    doMap(PO_NO_INHERIT_SYSTEM_HASHDECLS, "PO_NO_INHERIT_SYSTEM_HASHDECLS");
    // 49
    doMap(PO_ALLOW_WEAK_REFERENCES, "PO_ALLOW_WEAK_REFERENCES");
    // 51
    doMap(PO_ALLOW_DEBUGGER, "PO_ALLOW_DEBUGGER", "DEBUGGER");
    // 52
    doMap(PO_ALLOW_STATEMENT_NO_EFFECT, "PO_ALLOW_STATEMENT_NO_EFFECT");
    // 53
    doMap(PO_NO_REFLECTION, "PO_NO_REFLECTION", "REFLECTION");
    // 54
    doMap(PO_NO_TRANSIENT, "PO_NO_TRANSIENT");
    // 55
    doMap(PO_BROKEN_SPRINTF, "PO_BROKEN_SPRINTF");
    // 56
    doMap(PO_BROKEN_CAST, "PO_BROKEN_CAST");
    // 57
    doMap(PO_ALLOW_RETURNS, "PO_ALLOW_RETURNS");
    // 58
    doMap(PO_BROKEN_NAMESPACE_RESOLUTION, "PO_BROKEN_NAMESPACE_RESOLUTION");
    // 59
    doMap(PO_BROKEN_RANGE, "PO_BROKEN_RANGE");
    // 60
    doMap(PO_NO_INHERIT_PROGRAM_DATA, "PO_NO_INHERIT_PROGRAM_DATA");
    // 61
    doMap(PO_BROKEN_VARARGS, "PO_BROKEN_ELLIPSES");
    // 62
    doMap(PO_BROKEN_LIST_RANGE, "PO_BROKEN_LIST_RANGE");
    // 63
    doMap(PO_NO_ENUM, "PO_NO_ENUM", "ENUM");
}

// program serialization magic
DLLLOCAL std::string ps_magic = "QOREPROG";

// program serialization constants
typedef unsigned char ps_code_t;
constexpr ps_code_t PSC_VERSION           = 1;
constexpr ps_code_t PSC_PARSE_WARN_OPTS   = 2;
constexpr ps_code_t PSC_PARSE_MODULES     = 3;
constexpr ps_code_t PSC_PGM_EXEC_STRINGS  = 4;
constexpr ps_code_t PSC_DEFINE_MAP        = 5;

// program serialization values
typedef unsigned char ps_char_t;
constexpr ps_char_t PSV_VERSION = 0;

QoreHashNode* ParseOptionMaps::getCodeToStringMap() const {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(stringTypeInfo), nullptr);

    QoreString key;
    for (auto& i : pomap) {
        key.clear();
        key.sprintf(QLLD, i.first);
        h->setKeyValue(key.c_str(), new QoreStringNode(i.second), nullptr);
    }

    return h.release();
}

QoreHashNode* ParseOptionMaps::getStringToCodeMap() const {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(bigIntTypeInfo), nullptr);
    qore_hash_private* ph = qore_hash_private::get(**h);

    for (auto& i : pormap) {
        ph->setKeyValueIntern(i.first, i.second);
    }

    return h.release();
}

QoreHashNode* ParseOptionMaps::getDomainToStringMap() const {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(stringTypeInfo), nullptr);

    QoreString key;
    for (auto& i : dommap) {
        key.clear();
        key.sprintf(QLLD, i.first);
        h->setKeyValue(key.c_str(), new QoreStringNode(i.second), nullptr);
    }

    return h.release();
}

QoreHashNode* ParseOptionMaps::getStringToDomainMap() const {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(bigIntTypeInfo), nullptr);
    qore_hash_private* ph = qore_hash_private::get(**h);

    for (auto& i : domrmap) {
        ph->setKeyValueIntern(i.first, i.second);
    }

    return h.release();
}

QoreListNode* parse_option_bitfield_to_string_list(int64 i, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(stringTypeInfo), xsink);

    for (int p = 0; p < 64; ++p) {
        int64 v = (i & (1ll << p));
        if (v) {
            ParseOptionMaps::pomap_t::const_iterator pi = pomaps.pomap.find(v);
            if (pi == pomaps.pomap.end()) {
                xsink->raiseException("PARSE-OPTION-ERROR", "bit position %d (value: " QLLD ") is set in the value "
                    "but is not a valid parse option bit", p, v);
                return nullptr;
            }

            rv->push(new QoreStringNode(pi->second), xsink);
        }
    }

    return rv.release();
}

QoreListNode* domain_bitfield_to_string_list(int64 i, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(stringTypeInfo), xsink);

    for (int p = 0; p < 64; ++p) {
        int64 v = (i & (1ll << p));
        if (v) {
            ParseOptionMaps::pomap_t::const_iterator pi = pomaps.dommap.find(v);
            if (pi == pomaps.dommap.end()) {
                xsink->raiseException("DOMAIN-ERROR", "bit position %d (value: " QLLD ") is set in the value but is "
                    "not a valid domain bit", p, v);
                return nullptr;
            }

            rv->push(new QoreStringNode(pi->second), xsink);
        }
    }

    return rv.release();
}

//public symbols
const char** qore_warnings = qore_warnings_l;
unsigned qore_num_warnings = NUM_WARNINGS;
qore_program_private::qore_program_set_t qore_program_private::qore_program_set;
qore_program_private::programid_to_program_map_t qore_program_private::programid_to_program_map;
QoreRWLock qore_program_private::lck_programMap;
qore_program_id_t qore_program_private::programIdCounter = 1;

namespace {
//! the lock of the graph of parse lock waits (qore#5693); taken after a Program's plock, never before one
QoreThreadLock parse_wait_lock;
//! the graph of parse lock waits: the Program whose parse lock each thread waits for, by TID; under parse_wait_lock
/** An entry is set and cleared by its own thread in qore_program_private::waitForParseLock(), which the Program's
    caller keeps alive while it runs
*/
qore_program_private* parse_waits[MAX_QORE_THREADS];
}

namespace {
//! The entry of a waiting thread in the graph of parse lock waits; removed at the latest when it goes out of scope
/** Added and removed with the Program's plock and parse_wait_lock held; the destructor runs with plock held (see
    qore_program_private::waitForParseLock()), so that no exception can leave the entry of a thread that no longer
    waits in the graph
*/
class ParseWaitEntry {
public:
    ParseWaitEntry(qore_program_private* pp, int tid) : pp(pp), tid(tid) {
    }

    ~ParseWaitEntry() {
        if (added) {
            AutoLocker al(parse_wait_lock);
            removeIntern();
        }
    }

    //! adds the entry; parse_wait_lock is held
    void addIntern() {
        assert(!added && !parse_waits[tid]);
        parse_waits[tid] = pp;
        ++pp->parse_lock_waiters;
        added = true;
    }

    //! removes the entry if it was added; parse_wait_lock is held
    void removeIntern() {
        if (added) {
            assert(parse_waits[tid] == pp);
            parse_waits[tid] = nullptr;
            assert(pp->parse_lock_waiters);
            --pp->parse_lock_waiters;
            added = false;
        }
    }

    bool isAdded() const {
        return added;
    }

private:
    qore_program_private* pp;
    int tid;
    bool added = false;
};
}

//! raises the exception for a parse lock wait that would close a cycle of waits
static void raise_parse_wait_cycle(ExceptionSink* xsink, int owner, int tid) {
    xsink->raiseException("PROGRAM-ERROR", "cannot wait for the parse lock of the Program, which is held by TID %d: "
        "that thread waits, directly or through other threads, for a parse lock that this thread (TID %d) holds, so "
        "the wait would never end", owner, tid);
}

int qore_program_private::waitForParseLock(int tid, ExceptionSink* xsink) {
    // a thread unknown to Qore cannot be registered, nor be part of a cycle that a registered thread sees
    ParseWaitEntry entry(this, tid);
    if (tid >= 0 && tid < MAX_QORE_THREADS) {
        bool refuse = false;
        {
            AutoLocker al(parse_wait_lock);
            // the cycle is complete only once its last wait is registered, so the thread that would close it sees it
            if (parseWaitClosesCycleIntern(tid)) {
                if (xsink) {
                    refuse = true;
                } else {
                    // a wait without an exception sink cannot be refused: its entry is added in this critical
                    // section, so the waits of the cycle that can be refused see the cycle when they check it before
                    // they block, and those that are blocked already are woken to check it (see below)
                    wakeParseWaitCycleIntern(tid);
                }
            }
            if (!refuse) {
                entry.addIntern();
            }
        }
        // the exception is raised without the lock of the graph: it reads the locations of the call stack, which
        // locks the Programs of the stack
        if (refuse) {
            raise_parse_wait_cycle(xsink, parse_tid, tid);
            return -1;
        }
    }

    //! what the check made before each block needs
    struct CycleCheck {
        qore_program_private* pp;
        int tid;
        ParseWaitEntry* entry;
        bool cycle;
    } check = {this, tid, &entry, false};
    // called with plock held after the wait is registered for a wakeup and before the thread blocks: a wait without
    // an exception sink that closes a cycle later either is seen here or wakes this one (see
    // QoreCondition::waitWithInterrupt()); a wait that sees the cycle leaves the graph in the same critical section,
    // so that the other waits of the cycle find it broken and only this one is refused
    auto before_block = [] (void* arg) -> int {
        CycleCheck* c = static_cast<CycleCheck*>(arg);
        AutoLocker al(parse_wait_lock);
        if (!c->pp->parseWaitClosesCycleIntern(c->tid)) {
            return 0;
        }
        c->entry->removeIntern();
        c->cycle = true;
        return 1;
    };

    int rc = 0;
    while (parse_tid != -1 && parse_tid != tid && !ptid) {
        ++thread_waiting;
        if (xsink) {
            // a cancellation point; see "Condition waits" in design/cooperative-cancellation.md
            if (qore_condition_ops(pcond).waitWithInterrupt(plock, xsink, "parse lock wait", entry.isAdded() ? +before_block : nullptr,
                    &check) == QORE_COND_RESULT_INTERRUPTED) {
                rc = -1;
            }
        } else {
            pcond.wait(plock);
        }
        --thread_waiting;
        if (check.cycle) {
            raise_parse_wait_cycle(xsink, parse_tid, tid);
            rc = -1;
        }
        if (rc) {
            break;
        }
    }
    return rc;
}

void qore_program_private::wakeParseWaitCycleIntern(int tid) const {
    // the chain is a cycle back to this thread (see parseWaitClosesCycleIntern()); each thread in it but this one
    // waits.  Waking never blocks (see QoreThreadList::wakeCondWaiter()); a wait that is not registered for a wakeup
    // (it has no exception sink, or waits while cancellation is deferred) is not woken
    int owner = parse_tid;
    for (unsigned i = 0; i < MAX_QORE_THREADS && owner != tid; ++i) {
        assert(owner >= 0 && owner < MAX_QORE_THREADS && parse_waits[owner]);
        thread_list.wakeCondWaiter(owner, true);
        owner = parse_waits[owner]->parse_tid;
    }
}

#ifdef DEBUG
bool qore_program_private::dbgParseWaitRegistered(int tid) {
    AutoLocker al(parse_wait_lock);
    return tid >= 0 && tid < MAX_QORE_THREADS && parse_waits[tid];
}
#endif

void qore_program_private::setParseTidWithWaitersIntern(int tid) {
    AutoLocker al(parse_wait_lock);
    parse_tid = tid;
}

bool qore_program_private::parseWaitClosesCycleIntern(int tid) const {
    // the owner of this Program's lock is stable under plock; that of each Program in the chain is stable under
    // parse_wait_lock, as a thread waits for it (see setParseTidIntern()).  The chain is followed at most once
    // through every thread: a cycle that does not include this thread (closed by waits without an exception sink,
    // which are not refused) ends it as well
    int owner = parse_tid;
    for (unsigned i = 0; i < MAX_QORE_THREADS; ++i) {
        if (owner == tid) {
            return true;
        }
        if (owner < 0 || owner >= MAX_QORE_THREADS) {
            return false;
        }
        const qore_program_private* next = parse_waits[owner];
        if (!next) {
            return false;
        }
        owner = next->parse_tid;
    }
    return false;
}

void qore_program_private::clearRemainingPrograms() {
    // the Programs are cleared one at a time, oldest first, and the registry is scanned again after each one: a
    // Program that an older one holds (a child Program in a global variable of its parent, for example) is cleared
    // when the older one releases it, as when its owner deletes the older one, so the objects of the child that the
    // parent holds are destroyed while the child still has its data; a Program that a destructor creates while the
    // Programs are cleared is cleared as well (its ID is higher than any cleared before it).  The IDs of the Programs
    // are never reused, so each one is cleared once
    ExceptionSink xsink;
    qore_program_id_t last_id = 0;
    while (true) {
        QoreProgram* p = nullptr;
        {
            QoreAutoRWReadLocker al(&lck_programMap);
            for (QoreProgram* i : qore_program_set) {
                // a Program without references is skipped: no reference can be taken to it; it has been cleared if
                // its last reference was released with an exception sink (see clear()), and it is freed by the code
                // that holds it otherwise; its references cannot drop to zero meanwhile, as that needs the write lock
                // (see derefUnregister())
                qore_program_id_t id = i->priv->programId;
                if (id > last_id && i->reference_count() > 0 && (!p || id < p->priv->programId)) {
                    p = i;
                }
            }
            if (!p) {
                break;
            }
            // a reference of its own while it is cleared: no reference of the owner is released
            p->ref();
        }
        last_id = p->priv->programId;
        printd(5, "qore_program_private::clearRemainingPrograms() clearing pgm: %p pgmid: " QLLD "\n", p,
            (int64)last_id);
        p->priv->waitForTerminationAndClear(&xsink);
        // the owner's references keep the Program; if the Program has lost them while it was cleared, it is released
        // here as by any other last reference
        p->deref(&xsink);
        // each Program's exceptions are reported with it
        xsink.handleExceptions();
    }
}

#ifdef DEBUG
namespace {
//! the destructions of Programs, for dbgWaitProgramDestroyed(); a leaf lock, taken with the registry lock held
struct QoreProgramDestroyedGate {
    std::mutex m;
    std::condition_variable cv;
    uint64_t gen = 0;
};

QoreProgramDestroyedGate& program_destroyed_gate() {
    // not destroyed: Programs are destroyed while static objects are destroyed
    static QoreProgramDestroyedGate* g = new QoreProgramDestroyedGate;
    return *g;
}
}

void qore_program_private::dbgProgramDestroyed() {
    QoreProgramDestroyedGate& g = program_destroyed_gate();
    std::lock_guard<std::mutex> lg(g.m);
    ++g.gen;
    g.cv.notify_all();
}

bool qore_program_private::dbgWaitProgramDestroyed(qore_program_id_t id, int64 timeout_ms) {
    QoreProgramDestroyedGate& g = program_destroyed_gate();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        uint64_t gen;
        {
            std::lock_guard<std::mutex> lg(g.m);
            gen = g.gen;
        }
        // the registry lock is not taken with the gate's lock held
        if (!dbgIsLiveProgram(id)) {
            return true;
        }
        std::unique_lock<std::mutex> l(g.m);
        if (!g.cv.wait_until(l, deadline, [&g, gen]() { return g.gen != gen; })) {
            return false;
        }
    }
}

bool qore_program_private::dbgIsLiveProgram(qore_program_id_t id) {
    QoreAutoRWReadLocker al(&lck_programMap);
    for (QoreProgram* p : qore_program_set) {
        if (p->priv->programId == id) {
            return true;
        }
    }
    return false;
}

size_t qore_program_private::dbgGetLiveProgramCount() {
    QoreAutoRWReadLocker al(&lck_programMap);
    return qore_program_set.size();
}
#endif

void qore_program_private::fixupAfterFork(int tid, ThreadProgramData* tpd) {
    plock.reinitAfterFork(&ma_recursive);
    qore_condition_ops(pcond).reinitAfterFork();
    // a thread of the parent that created or destroyed a variant of the Program's code can have held it; the list
    // that it protects is consistent, as it is changed under the stripe lock as well (see recordLock())
    new (&user_variant_lock) std::mutex();
    // a leaf lock held briefly by the changes and checks of the parse options; a thread of the parent can have held it
    new (&po_mutex) std::mutex();
    // the copies of the external data made by the parent's other threads do not go on in the child, and its waiters
    // do not exist there
    ext_copy_count = getOwnExtDataCopies();
    ext_copy_waiting = 0;
    qore_condition_ops(ext_copy_cond).reinitAfterFork();
    // the thread counts keep only the forking thread; the containers of the Program's threads are consistent here, as
    // fork() waited for every change of them in progress (see qore_program_private_base::thread_container_shards),
    // and they are changed without the stripe lock, as only this thread exists
    unsigned own = 0;
    ptid_map_t::iterator i = tidmap.find(tid);
    if (i != tidmap.end()) {
        own = i->second;
    }
    tidmap.clear();
    foreign_tids.clear();
    if (own) {
        tidmap[tid] = own;
        if (q_thread_is_foreign()) {
            foreign_tids.insert(tid);
        }
    }
    thread_count = own;
    thread_waiting = 0;
    // the holds of the parent's other threads, which do not exist in the child
    compile_holds = 0;
    // the background threads of the parent other than the forking thread do not exist in the child; a Program forked
    // while it was being constructed, after it was registered, has no hub yet
    if (thread_hub) {
        thread_hub->forkChild(tid, ++activity_gen, !own);
    }
    if (parse_tid != tid) {
        // a thread of the parent was parsing; the parse does not go on in the child, nor does its commit
        parse_count = 0;
        parse_tid = -1;
        commit_entry_restricted = false;
    }
    // the other threads of the parent do not exist in the child, whose TIDs can be given to other threads
    // (erased without allocating)
    for (std::set<int>::iterator i = parse_tids.begin(); i != parse_tids.end();) {
        if (*i == tid) {
            ++i;
        } else {
            i = parse_tids.erase(i);
        }
    }
    // the threads that waited for the parse lock do not exist in the child (see fixupAllAfterFork())
    parse_lock_waiters = 0;
    tlock.reinitAfterFork();
    qore_condition_ops(tcond).reinitAfterFork();
    twaiting = 0;
    if (tclear && tclear != tid) {
        tclear = 0;
    }
    // the Program data of the parent's other threads remains recorded in the Program, and the Program's teardown in
    // the child uses it; its lock can be held by its thread or by a thread that tore down another Program, and the
    // records that it protects are consistent, as they are changed under the stripe lock (see recordLock())
    for (auto& i : pgm_data_map) {
        if (i.first != tpd) {
            i.first->reinitLockAfterFork();
        }
    }
    tld_lock.reinitAfterFork();
    sm_lock.reinitAfterFork();
    qore_rwlock_ops(featureLock).reinitAfterFork();
    new (&runtime_metadata_lock) std::recursive_mutex();
    pwo.parse_options.reinitAfterFork();
    // a leaf lock held briefly while an expression is added, removed or looked up; the set is consistent, as it is
    // changed under the stripe lock as well (see recordLock())
    new (&exp_set_lock) std::mutex();
    ir_fallback_lock.reinitAfterFork();
    plugin_fallback_lock.reinitAfterFork();
    // the calls into the debug program in progress in the parent's other threads do not go on in the child
    qore_counter_ops(debug_program_counter).reinitAfterFork(
        static_cast<int>(QoreForkCountedHold::countOwn(&debug_program_counter)));
    qore_rwlock_ops(lck_breakpoint).reinitAfterFork();
    // the thread data of the parent's other threads remains registered; its lock can be held by its thread
    for (ThreadProgramData* i : tld_registry) {
        if (i != tpd) {
            i->reinitLockAfterFork();
        }
    }
}

namespace {
//! the debug programs, for qore_program_private::fixupAllAfterFork(); protected by the Program registry lock
std::set<qore_debug_program_private*>* qore_debug_program_set = nullptr;
}

void qore_program_private::registerDebugProgram(qore_debug_program_private* dp) {
    QoreAutoRWWriteLocker al(&lck_programMap);
    if (!qore_debug_program_set) {
        qore_debug_program_set = new std::set<qore_debug_program_private*>;
    }
    qore_debug_program_set->insert(dp);
}

void qore_program_private::unregisterDebugProgram(qore_debug_program_private* dp) {
    QoreAutoRWWriteLocker al(&lck_programMap);
    assert(qore_debug_program_set);
    qore_debug_program_set->erase(dp);
}

void qore_program_private::fixupAllAfterFork(int tid, ThreadProgramData* tpd) {
    // the graph of parse lock waits: only the forking thread exists in the child, and it was not waiting; a thread of
    // the parent could hold its lock
    parse_wait_lock.reinitAfterFork();
    std::fill(std::begin(parse_waits), std::end(parse_waits), nullptr);
    // the forking thread's own Program data can be locked by another thread that tore down a Program of the thread;
    // the forking thread itself never holds the lock across fork()
    if (tpd) {
        tpd->reinitLockAfterFork();
    }
    // all instances, also those whose last reference has been released and that are being destroyed
    for (QoreProgram* p : qore_program_set) {
        qore_program_private* pp = qore_program_private::get(*p);
        pp->fixupAfterFork(tid, tpd);
    }
    // every debug program, once, also one that is attached to no Program (issue #6404)
    if (qore_debug_program_set) {
        for (qore_debug_program_private* dp : *qore_debug_program_set) {
            dp->fixupAfterFork();
        }
    }
}

void qore_program_private::getUserFeaturesOfProgramsWithData(strset_t& features) {
    QoreAutoRWReadLocker al(&lck_programMap);
    for (QoreProgram* p : qore_program_set) {
        qore_program_private* pp = qore_program_private::get(*p);
        // only the teardown thread runs Qore code now, so ns_vars, which is set when the data is cleared, can be read
        // unlocked
        if (pp->ns_vars) {
            continue;
        }
        QoreSafeRWReadLocker rl(pp->featureLock);
        features.insert(pp->userFeatureList.begin(), pp->userFeatureList.end());
    }
}

namespace {
//! the fork handler of the Program registry: the Programs are made consistent in the child (see
//! qore_program_private::fixupAfterFork())
class QoreProgramRegistryForkHandler : public QoreForkHandler {
public:
    QoreProgramRegistryForkHandler() : QoreForkHandler(QFLR_LEAF, "program_registry") {
    }

    void prepare() noexcept override {
        qore_program_private::getProgramMapLock().wrlock();
    }

    void parent() noexcept override {
        qore_program_private::getProgramMapLock().unlock();
    }

    void child() noexcept override {
        ThreadProgramData* tpd = get_thread_program_data_if_any();
        qore_program_private::fixupAllAfterFork(qore_fork_get_tid(), tpd);
        qore_rwlock_ops(qore_program_private::getProgramMapLock()).reinitAfterFork();
    }
};

QoreProgramRegistryForkHandler program_registry_fork_handler;

//! the fork handler of the stripe locks of the Programs' thread containers, which are leaf locks never held together
//! (see qore_program_private_base::thread_container_shards and QoreForkHandlers.h)
/** Registered after the Program registry's handler, so that its child handler runs before the registry's, which
    makes the Programs consistent in the child (fixupAllAfterFork())
*/
class ProgramThreadContainersForkHandler : public QoreForkHandler {
public:
    static constexpr unsigned count = 1u << qore_program_private_base::ThreadContainerShardBits;

    ProgramThreadContainersForkHandler() : QoreForkHandler(QFLR_LEAF, "program_thread_containers") {
    }

    void prepare() noexcept override {
        if (QFLM_STRIPED == QFLM_ACQUIRE) {
            for (auto& shard : qore_program_private_base::thread_container_shards) {
                shard.lock.lock();
            }
        }
    }

    void parent() noexcept override {
        if (QFLM_STRIPED == QFLM_ACQUIRE) {
            for (unsigned i = count; i; --i) {
                qore_program_private_base::thread_container_shards[i - 1].lock.unlock();
            }
        }
    }

    void child() noexcept override {
        if (QFLM_STRIPED == QFLM_ACQUIRE) {
            // acquired by this thread in prepare(): plain mutexes, which this thread releases in the child
            for (unsigned i = count; i; --i) {
                qore_program_private_base::thread_container_shards[i - 1].lock.unlock();
            }
            return;
        }
        // not acquired before fork(): a lock can be held by a thread that does not exist in the child
        for (auto& shard : qore_program_private_base::thread_container_shards) {
            new (&shard.lock) std::mutex();
        }
    }
};

ProgramThreadContainersForkHandler program_thread_containers_fork_handler;
}

qore_program_private_base::ThreadContainerShard
    qore_program_private_base::thread_container_shards[1u << ThreadContainerShardBits];

void qore_set_program_parse_diagnostics_accumulated(QoreProgram& pgm) {
    qore_program_private::get(pgm)->setParseDiagnosticsAccumulated();
}

bool qore_get_exception_parse_diagnostic(const QoreException& e, QoreDiagnostic& diag) {
    if (!e.diagnostic_pgm_id || e.diagnostic_index < 0) {
        return false;
    }
    QoreProgram* pgm = qore_program_private::getReferencedProgram(e.diagnostic_pgm_id);
    if (!pgm) {
        return false;
    }
    bool rv = false;
    {
        qore_program_private* pp = qore_program_private::get(*pgm);
        if (pp->accumulate_diagnostics && e.diagnostic_gen == pp->diagnostics_gen
                && static_cast<size_t>(e.diagnostic_index) < pp->diagnostics.size()) {
            diag = pp->diagnostics[e.diagnostic_index];
            rv = true;
        }
    }
    ExceptionSink xsink;
    pgm->deref(&xsink);
    xsink.clear();
    return rv;
}

qore_program_private::qore_program_private(QoreProgram* n_pgm, const QoreParseOptions& n_parse_options,
        QoreProgram* p_pgm, const QoreParseOptions* p_parse_options, ExceptionSink* xsink)
        : qore_program_private_base(n_pgm, n_parse_options, p_pgm, p_parse_options) {
    // a Program that sandboxed code creates, or C++ code acting for it, is sandboxed (issue #5659); decided here, for
    // every Program wherever it is created, and before the Program is registered and can be found
    if (qore_caller_is_sandboxed()) {
        sandboxed.store(true, std::memory_order_release);
        // the Program keeps the sandbox of the code that creates it, which governs its code wherever it runs, also
        // when trusted code calls it without the Program object (see creator_sandbox_manager); before the Program is
        // registered, so that nothing has to be undone if the lookup fails (std::bad_alloc: no Program is created)
        QoreSandboxManager* all;
        QoreSandboxManager* policy;
        qore_caller_sandbox_refs(all, policy);
        ReferenceHolder<QoreSandboxManager> creator_sm(all, nullptr);
        ReferenceHolder<QoreSandboxManager> creator_policy(policy, nullptr);
        // code that the sandboxes of different tenants govern together belongs to no tenant, and a Program that it
        // created would keep their composite for good: the Program is not created (the caller deletes it; qore#7009);
        // a Program created without an exception sink (a module's) keeps the composite, which every isolation
        // decision treats as mixed
        if (xsink && creator_sm) {
            std::string domain;
            if (QoreSandboxComposition::foldIsolationDomain(*creator_sm, domain) < 0) {
                xsink->raiseException("SANDBOX-ISOLATION-ERROR", "code that the sandboxes of more than one isolation "
                    "domain govern together cannot create a Program");
            }
        }
        if (creator_sm || creator_policy) {
            try {
                addCreatorSandbox(*creator_sm, *creator_policy);
            } catch (...) {
                // a manager that was attached before the failure is released: the destructor does not run
                releaseSandboxManagers(nullptr);
                throw;
            }
        }
    }
    registerProgram();
    // the Program's ID is known now
    thread_hub = new ProgramThreadHub(programId);
    // every member of this object now exists, so the Program can serve as a runtime context; until
    // this point code reached from the base constructor can see the Program but must not switch
    // into it
    constructed = true;
    // the parent's external data is copied here rather than from setParent(), so that a module's
    // copy hook runs against a complete Program, as AbstractQoreProgramExternalData::copy()
    // documents; a failure is reported in xsink, and the caller deletes the Program (issue #6786)
    copyExternalData(xsink);
    // a Program that code that is not trusted creates has no host: none of its defines was set by one (its parse
    // option defines included); the library modules that it loads have their own platform defines
    if (qore_caller_is_untrusted()) {
        for (const auto& i : dmap) {
            source_defines.insert(i.first);
        }
    }
}

void qore_program_private::deleteFailedChild(QoreProgram* pgm, ExceptionSink* xsink) {
    assert(pgm->priv->extmap.empty());
    ExceptionSink del_xsink;
    pgm->waitForTerminationAndDeref(&del_xsink);
    xsink->assimilate(del_xsink);
}

void qore_program_private::markSandboxedParse() {
    // the flag is never cleared, so a Program that is sandboxed already needs no lookup; the sandbox of the code that
    // is parsed is attached when the code is committed (see getParseCreatorSandbox())
    if (!sandboxed.load(std::memory_order_acquire) && qore_caller_is_sandboxed()) {
        sandboxed.store(true, std::memory_order_release);
    }
}

#ifdef DEBUG
std::atomic<qore_program_private_base::dbg_teardown_hook_t> qore_program_private_base::dbg_teardown_hook{nullptr};
std::atomic<qore_program_private_base::dbg_thread_container_hook_t>
    qore_program_private_base::dbg_thread_container_hook{nullptr};
std::atomic<qore_program_private_base::dbg_record_hook_t> qore_program_private_base::dbg_record_hook{nullptr};
#endif

thread_local QoreForkCountedHold* QoreForkCountedHold::top = nullptr;

namespace {
class QoreProgramTeardownScope;

//! the innermost scope of the Programs whose data the calling thread deletes; see
//! qore_program_torn_down_by_current_thread()
/** The scopes are a stack linked through the scopes themselves (they live on the stack of the code that deletes the
    Program), and this is a plain pointer, not a container: a thread_local object with a destructor, such as a
    std::vector, is destroyed by the main thread when exit() starts, before the destructors of static objects run, and
    a static object that owns a Program (a module's, or QoreProgramHelper's) deletes it then, so it used the destroyed
    container (a heap use-after-free and "corrupted double-linked list" when a script ended normally with a Program
    held at static scope).  A pointer has no destructor and is valid for the life of the thread.
*/
thread_local const QoreProgramTeardownScope* qore_program_teardown_scope = nullptr;

//! records that the calling thread deletes a Program's data while it exists
/** Only the addresses are compared, so a Program deleted by another thread is never dereferenced
*/
class QoreProgramTeardownScope {
public:
    QoreProgramTeardownScope(const QoreProgram* pgm) : pgm(pgm), prev(qore_program_teardown_scope) {
        qore_program_teardown_scope = this;
        counted = qore_thread_program_teardown_enter();
    }

    ~QoreProgramTeardownScope() {
        if (counted) {
            qore_thread_program_teardown_leave();
        }
        assert(qore_program_teardown_scope == this);
        qore_program_teardown_scope = prev;
    }

    //! returns true if the calling thread is deleting the data of \a p in an enclosing scope
    static bool contains(const QoreProgram* p) {
        for (const QoreProgramTeardownScope* i = qore_program_teardown_scope; i; i = i->prev) {
            if (i->pgm == p) {
                return true;
            }
        }
        return false;
    }

    QoreProgramTeardownScope(const QoreProgramTeardownScope&) = delete;
    QoreProgramTeardownScope& operator=(const QoreProgramTeardownScope&) = delete;

private:
    //! the Program whose data is deleted
    const QoreProgram* pgm;
    //! the enclosing scope of the thread, or nullptr
    const QoreProgramTeardownScope* prev;
    //! the thread's data records the teardown (see qore_reference_program_needs_entry())
    bool counted;
};
}

bool qore_program_torn_down_by_current_thread(const QoreProgram* pgm) {
    return QoreProgramTeardownScope::contains(pgm);
}

QoreProgram* qore_program_private::getReferencedProgram(const QoreProgram* p, qore_program_id_t id) {
    QoreAutoRWReadLocker al(&lck_programMap);
    programid_to_program_map_t::const_iterator i = programid_to_program_map.find(id);
    if (i == programid_to_program_map.end() || i->second != p) {
        return nullptr;
    }
    // a registered Program has references (see derefUnregister())
    return i->second->optRef() ? i->second : nullptr;
}

int QoreDeclaringProgramHelper::raiseDeleted(ExceptionSink* xsink) {
    if (xsink) {
        xsink->raiseException("PROGRAM-ERROR", "the Program that declared the member initializer has already been "
            "deleted and therefore its code cannot be executed");
    }
    return -1;
}

int QoreDeclaringProgramHelper::enter(QoreProgram* p, qore_program_id_t id, std::atomic<bool>& module_lifetime,
        ExceptionSink* xsink) {
    assert(p);
    // the current Program, which the thread is in, and a Program that the thread deletes (which runs the code of its
    // declarations while it deletes their data, destructors for example) are allocated: their ID tells them from a
    // later Program at the same address
    int ctid;
    if (!qore_reference_program_needs_entry(p, ctid)) {
        return p->getProgramId() == id ? 0 : raiseDeleted(xsink);
    }
    // a module's Program stays allocated while any code can run (module lifetime), as for a call of a module's
    // function from another Program (see QoreSourceProgramHolder::setVariant())
    if (module_lifetime.load(std::memory_order_acquire) && qore_module_lifetime_active()) {
        return 0;
    }
    QoreProgram* rp = qore_program_private::getReferencedProgram(p, id);
    if (!rp) {
        return raiseDeleted(xsink);
    }
    if (qore_program_has_module_lifetime(rp)) {
        module_lifetime.store(true, std::memory_order_release);
    }
    return enterReferenced(rp, xsink);
}

int QoreDeclaringProgramHelper::enterReferenced(QoreProgram* p, ExceptionSink* xsink) {
    assert(p);
    if (pgm) {
        // the helper holds another Program already
        if (!next) {
            next.reset(new QoreDeclaringProgramHelper);
        }
        return next->enterReferenced(p, xsink);
    }
    int ctid = q_gettid();
    if (!qore_program_private::get(*p)->enterWithoutContext(ctid)) {
        // releasing the last reference deletes the Program, which needs a sink for its destructors
        ExceptionSink local_xsink;
        p->deref(xsink ? xsink : &local_xsink);
        return raiseDeleted(xsink);
    }
    pgm = p;
    tid = ctid;
    leave_xsink = xsink;
    return 0;
}

void QoreDeclaringProgramHelper::leave() {
    assert(pgm);
    QoreProgram* p = pgm;
    pgm = nullptr;
    // leaving the Program can release it, if it was deleted meanwhile, as can releasing the reference; the
    // destructors run then need a sink
    ExceptionSink local_xsink;
    ExceptionSink* xsink = leave_xsink ? leave_xsink : &local_xsink;
    qore_program_private::decThreadCount(*p, tid, xsink);
    p->deref(xsink);
}

bool qore_program_private::runWithRegisteredPrograms(
        const std::vector<std::pair<const QoreProgram*, qore_program_id_t>>& pgms, const std::function<bool()>& f) {
    QoreAutoRWReadLocker al(&lck_programMap);
    for (const auto& p : pgms) {
        programid_to_program_map_t::const_iterator i = programid_to_program_map.find(p.second);
        if (i == programid_to_program_map.end() || i->second != p.first) {
            return false;
        }
    }
    return f();
}

void qore_program_private::registerProgram() {
    QoreAutoRWWriteLocker al(&qore_program_private::lck_programMap);
    assert(qore_program_set.find(pgm) == qore_program_set.end());
    programId = programIdCounter++;
    qore_program_set.insert(pgm);
    // a new Program has its first reference
    programid_to_program_map.insert(programid_to_program_map_t::value_type(programId, pgm));
    printd(5, "qore_program_private::registerProgram() this: %p pgm: %p, pgmid: " QLLD "\n", this, pgm, (int64)programId);
}

qore_program_private::~qore_program_private() {
    // each user variant of the Program's code holds a dependency reference to it until it unregisters
    assert(!user_variants);
    printd(5, "qore_program_private::~qore_program_private() this: %p pgm: %p, pgmid: " QLLD "\n", this, pgm, (int64)programId);
    if (qore_debug_program_private* dp = dpgm.load(std::memory_order_relaxed)) {
        // if the object is being destroyed when thread is terminating via a deref then we send sync detach event to this thread
        // the thread can be stopped.
        ThreadLocalProgramData *tlpd = get_thread_local_program_data();
        if (tlpd) {
            tlpd->dbgDetach(nullptr);
        }
        dp->removeProgram(pgm);
    }
    // wait till all debug calls are finished, no new calls possible as dpgm->removeProgram() set dpmg to NULL
    qore_counter_ops(debug_program_counter).waitForZero();
    // a Program destroyed without being cleared still holds its sandbox managers
    releaseSandboxManagers(nullptr);
    deleteAllBreakpoints();
    // a hook that was not run (the Program was destroyed without being cleared) is never run
    qore_drop_exit_hooks(pgm);
    QoreAutoRWWriteLocker al(&qore_program_private::lck_programMap);
    qore_program_set_t::iterator i = qore_program_set.find(pgm);
    assert(i != qore_program_set.end());
    qore_program_set.erase(i);
#ifdef DEBUG
    dbgProgramDestroyed();
#endif
    // a Program left the registry when its last reference was released (see derefUnregister()), unless it is
    // destroyed with references, as a Program that was never handed out can be
    programid_to_program_map_t::iterator pi = programid_to_program_map.find(programId);
    if (pi != programid_to_program_map.end()) {
        assert(pi->second == pgm);
        programid_to_program_map.erase(pi);
    }
    printd(5, "qore_program_private::~qore_program_private() this: %p pgm: %p, pgmid: " QLLD " removed\n", this, pgm, (int64)programId);

    statementByFileIndex.clear();
    statementIds.clear();
    for (auto& i : aot_closure_debug_blocks) {
        delete i.second;
    }
    aot_closure_debug_blocks.clear();
    reverseStatementIds.clear();
    statementByLabelIndex.clear();

    // threads that have left the Program and have not published FINISHED yet keep the hub
    if (thread_hub) {
        thread_hub->deref();
        thread_hub = nullptr;
    }

    assert(!parseSink);
    assert(!warnSink);
    assert(!pendingParseSink);
    assert(pgm_data_map.empty());
    assert(tld_registry.empty());
    assert(tld_members.empty());
    assert(!tld_parent_root);
    assert(!exec_class_rv);
    assert(!exec_class_inst);
    assert(!dpgm.load(std::memory_order_relaxed));
}

static void set_diagnostic_span_hash(QoreHashNode& h, const QoreDiagnosticSpan& span) {
    auto ph = qore_hash_private::get(h);
    ph->setKeyValueIntern("file", span.file.empty() ? QoreValue() : QoreValue(new QoreStringNode(span.file)));
    ph->setKeyValueIntern("source", span.source.empty() ? QoreValue() : QoreValue(new QoreStringNode(span.source)));
    ph->setKeyValueIntern("startLine", (int64)span.start_line);
    ph->setKeyValueIntern("endLine", (int64)span.end_line);
    ph->setKeyValueIntern("startColumn", (int64)span.start_column);
    ph->setKeyValueIntern("endColumn", (int64)span.end_column);
}

static QoreListNode* make_diagnostic_related_location_list(
        const std::vector<QoreDiagnosticRelatedLocation>& related_locations) {
    ReferenceHolder<QoreListNode> list(
        new QoreListNode(hashdeclParseDiagnosticRelatedLocationInfo->getTypeInfo()), nullptr);
    for (const auto& related : related_locations) {
        ReferenceHolder<QoreHashNode> h(
            new QoreHashNode(hashdeclParseDiagnosticRelatedLocationInfo, nullptr), nullptr);
        set_diagnostic_span_hash(**h, related.span);
        auto ph = qore_hash_private::get(**h);
        ph->setKeyValueIntern("role", new QoreStringNode(related.role));
        ph->setKeyValueIntern("message", related.message.empty()
            ? QoreValue() : QoreValue(new QoreStringNode(related.message)));
        list->push(h.release(), nullptr);
    }
    return list.release();
}

static QoreListNode* make_diagnostic_fix_list(const std::vector<QoreDiagnosticFix>& fixes) {
    ReferenceHolder<QoreListNode> list(new QoreListNode(hashdeclParseDiagnosticFixInfo->getTypeInfo()), nullptr);
    for (const auto& fix : fixes) {
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclParseDiagnosticFixInfo, nullptr), nullptr);
        auto ph = qore_hash_private::get(**h);
        ph->setKeyValueIntern("title", new QoreStringNode(fix.title));
        ph->setKeyValueIntern("applicability", new QoreStringNode(fix.applicability));
        ReferenceHolder<QoreListNode> edits(
            new QoreListNode(hashdeclParseDiagnosticEditInfo->getTypeInfo()), nullptr);
        for (const auto& edit : fix.edits) {
            ReferenceHolder<QoreHashNode> eh(new QoreHashNode(hashdeclParseDiagnosticEditInfo, nullptr), nullptr);
            set_diagnostic_span_hash(**eh, edit.span);
            qore_hash_private::get(**eh)->setKeyValueIntern("replacement", new QoreStringNode(edit.replacement));
            edits->push(eh.release(), nullptr);
        }
        ph->setKeyValueIntern("edits", edits.release());
        list->push(h.release(), nullptr);
    }
    return list.release();
}

void qore_program_private::collectModuleLoadErrors(const QoreProgramLocation& loc, ExceptionSink& xsink) {
    if (!collect_diagnostics) {
        return;
    }
    for (QoreException* e = qore_es_private::get(xsink)->head; e; e = e->next) {
        QoreStringValueHelper err(e->err);
        QoreStringValueHelper desc(e->desc);
        diagnostics.push_back(QoreDiagnostic(true, err->c_str(), -1, loc, desc->c_str()));
        markDiagnostic(*e);
    }
}

void qore_program_private::addExceptionDiagnostics(ExceptionSink* xsink) {
    if (!collect_diagnostics || !xsink || !*xsink) {
        return;
    }
    for (QoreException* e = qore_es_private::get(*xsink)->head; e; e = e->next) {
        if (e->diagnostic_pgm_id == programId) {
            continue;
        }
        QoreStringValueHelper err(e->err);
        QoreStringValueHelper desc(e->desc);
        // an exception without a source location (raised by builtin code) has no file
        diagnostics.emplace_back(err->c_str(), e->isBuiltin() ? std::string() : e->file, e->source, *e,
            desc->c_str());
        markDiagnostic(*e);
    }
}

int64 qore_program_private::skipPendingStatements(QoreProgram& pgm, ExceptionSink* xsink) {
    if (lockParsing(pgm, xsink)) {
        return -1;
    }
    // the parse lock is released also if an exception leaves
    ON_BLOCK_EXIT([&pgm, xsink] () {
        qore_program_private::unlockParsing(pgm, xsink);
    });
    // without PO_ALLOW_REPARSE, a run executes all of the statements
    if (!(pgm.priv->pwo.parse_options & PO_ALLOW_REPARSE)) {
        return 0;
    }
    return pgm.priv->sb.skipUnexecuted();
}

void qore_add_parse_exception_diagnostics(QoreProgram& pgm, ExceptionSink& xsink) {
    qore_program_private::get(pgm)->addExceptionDiagnostics(&xsink);
}

QoreListNode* qore_program_private::getParseDiagnosticList() const {
    ReferenceHolder<QoreListNode> l(new QoreListNode(hashdeclParseDiagnosticInfo->getTypeInfo()), nullptr);
    for (auto& d : diagnostics) {
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclParseDiagnosticInfo, nullptr), nullptr);
        auto ph = qore_hash_private::get(**h);
        ph->setKeyValueIntern("severity", new QoreStringNode(d.error ? "error" : "warning"));
        ph->setKeyValueIntern("code", new QoreStringNode(d.code));
        ph->setKeyValueIntern("id", new QoreStringNode(d.metadata.id));
        ph->setKeyValueIntern("warningCode", d.warn_code >= 0 ? QoreValue((int64)d.warn_code) : QoreValue());
        ph->setKeyValueIntern("file", d.file.empty() ? QoreValue() : QoreValue(new QoreStringNode(d.file)));
        ph->setKeyValueIntern("source", d.source.empty() ? QoreValue() : QoreValue(new QoreStringNode(d.source)));
        ph->setKeyValueIntern("startLine", (int64)d.start_line);
        ph->setKeyValueIntern("endLine", (int64)d.end_line);
        ph->setKeyValueIntern("startColumn", (int64)d.start_column);
        ph->setKeyValueIntern("endColumn", (int64)d.end_column);
        ph->setKeyValueIntern("message", new QoreStringNode(d.message));
        ph->setKeyValueIntern("hint", d.metadata.hint.empty()
            ? QoreValue() : QoreValue(new QoreStringNode(d.metadata.hint)));
        ReferenceHolder<QoreListNode> suggestions(new QoreListNode(stringTypeInfo), nullptr);
        for (const auto& suggestion : d.metadata.suggestions) {
            suggestions->push(new QoreStringNode(suggestion), nullptr);
        }
        ph->setKeyValueIntern("suggestions", suggestions.release());
        ReferenceHolder<QoreHashNode> facts(new QoreHashNode(stringTypeInfo), nullptr);
        auto fph = qore_hash_private::get(**facts);
        for (const auto& fact : d.metadata.facts) {
            fph->setKeyValueIntern(fact.first.c_str(), new QoreStringNode(fact.second));
        }
        ph->setKeyValueIntern("facts", facts.release());
        ph->setKeyValueIntern("relatedLocations",
            make_diagnostic_related_location_list(d.metadata.related_locations));
        ph->setKeyValueIntern("fixes", make_diagnostic_fix_list(d.metadata.fixes));
        l->push(h.release(), nullptr);
    }
    return l.release();
}

// appends a JSON-escaped string (without surrounding quotes) to str
static void json_escape_append(std::string& str, const std::string& s) {
    for (char c : s) {
        switch (c) {
            case '"': str += "\\\""; break;
            case '\\': str += "\\\\"; break;
            case '\b': str += "\\b"; break;
            case '\f': str += "\\f"; break;
            case '\n': str += "\\n"; break;
            case '\r': str += "\\r"; break;
            case '\t': str += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    str += buf;
                } else {
                    str += c;
                }
        }
    }
}

static void json_string_append(std::string& str, const std::string& value) {
    str += '"';
    json_escape_append(str, value);
    str += '"';
}

static void json_nullable_string_append(std::string& str, const std::string& value) {
    if (value.empty()) {
        str += "null";
    } else {
        json_string_append(str, value);
    }
}

static void json_diagnostic_span_fields_append(std::string& str, const QoreDiagnosticSpan& span) {
    str += "\"file\":";
    json_nullable_string_append(str, span.file);
    str += ",\"source\":";
    json_nullable_string_append(str, span.source);
    str += ",\"startLine\":" + std::to_string(span.start_line);
    str += ",\"endLine\":" + std::to_string(span.end_line);
    str += ",\"startColumn\":" + std::to_string(span.start_column);
    str += ",\"endColumn\":" + std::to_string(span.end_column);
}

static void json_diagnostic_metadata_append(std::string& str, const QoreDiagnosticMetadata& metadata) {
    str += ",\"hint\":";
    json_nullable_string_append(str, metadata.hint);

    str += ",\"suggestions\":[";
    bool first = true;
    for (const auto& suggestion : metadata.suggestions) {
        if (!first) {
            str += ',';
        }
        first = false;
        json_string_append(str, suggestion);
    }
    str += ']';

    str += ",\"facts\":{";
    first = true;
    for (const auto& fact : metadata.facts) {
        if (!first) {
            str += ',';
        }
        first = false;
        json_string_append(str, fact.first);
        str += ':';
        json_string_append(str, fact.second);
    }
    str += '}';

    str += ",\"relatedLocations\":[";
    first = true;
    for (const auto& related : metadata.related_locations) {
        if (!first) {
            str += ',';
        }
        first = false;
        str += '{';
        json_diagnostic_span_fields_append(str, related.span);
        str += ",\"role\":";
        json_string_append(str, related.role);
        str += ",\"message\":";
        json_nullable_string_append(str, related.message);
        str += '}';
    }
    str += ']';

    str += ",\"fixes\":[";
    first = true;
    for (const auto& fix : metadata.fixes) {
        if (!first) {
            str += ',';
        }
        first = false;
        str += "{\"title\":";
        json_string_append(str, fix.title);
        str += ",\"applicability\":";
        json_string_append(str, fix.applicability);
        str += ",\"edits\":[";
        bool first_edit = true;
        for (const auto& edit : fix.edits) {
            if (!first_edit) {
                str += ',';
            }
            first_edit = false;
            str += '{';
            json_diagnostic_span_fields_append(str, edit.span);
            str += ",\"replacement\":";
            json_string_append(str, edit.replacement);
            str += '}';
        }
        str += "]}";
    }
    str += ']';
}

std::string qore_program_private::getParseDiagnosticsJSON() const {
    return qore_diagnostics_json(diagnostics);
}

std::string qore_diagnostics_json(const std::vector<QoreDiagnostic>& diags) {
    std::string j = "[";
    bool first = true;
    for (auto& d : diags) {
        if (!first) {
            j += ",";
        }
        first = false;
        j += "{\"severity\":\"";
        j += d.error ? "error" : "warning";
        j += "\",\"code\":\"";
        json_escape_append(j, d.code);
        j += "\",\"id\":\"";
        json_escape_append(j, d.metadata.id);
        j += "\",\"warningCode\":";
        j += (d.warn_code >= 0) ? std::to_string(d.warn_code) : std::string("null");
        j += ",\"file\":";
        if (d.file.empty()) {
            j += "null";
        } else {
            j += "\"";
            json_escape_append(j, d.file);
            j += "\"";
        }
        j += ",\"source\":";
        if (d.source.empty()) {
            j += "null";
        } else {
            j += "\"";
            json_escape_append(j, d.source);
            j += "\"";
        }
        j += ",\"startLine\":" + std::to_string(d.start_line);
        j += ",\"endLine\":" + std::to_string(d.end_line);
        j += ",\"startColumn\":" + std::to_string(d.start_column);
        j += ",\"endColumn\":" + std::to_string(d.end_column);
        j += ",\"message\":\"";
        json_escape_append(j, d.message);
        j += '"';
        json_diagnostic_metadata_append(j, d.metadata);
        j += '}';
    }
    j += "]";
    return j;
}

std::string qore_get_parse_diagnostics_json(QoreProgram& pgm) {
    return qore_program_private::get(pgm)->getParseDiagnosticsJSON();
}

// extracts 1-based line number `line` from source text into `out`; returns true if found
static bool extract_source_line(const char* src, int line, std::string& out) {
    if (!src || line < 1) {
        return false;
    }
    int cur = 1;
    const char* p = src;
    const char* line_start = src;
    while (cur < line && *p) {
        if (*p == '\n') {
            ++cur;
            line_start = p + 1;
        }
        ++p;
    }
    if (cur != line) {
        return false;
    }
    const char* line_end = line_start;
    while (*line_end && *line_end != '\n') {
        ++line_end;
    }
    out.assign(line_start, line_end - line_start);
    return true;
}

unsigned qore_render_parse_diagnostic_frames(QoreProgram& pgm, const char* source_text, FILE* os) {
    return qore_program_private::get(pgm)->renderParseDiagnosticFrames(source_text, os);
}

unsigned qore_program_private::renderParseDiagnosticFrames(const char* source_text, FILE* os) const {
    return qore_render_diagnostic_frames(diagnostics, [source_text] (const QoreDiagnostic&) { return source_text; },
        os);
}

unsigned qore_render_diagnostic_frames(const std::vector<QoreDiagnostic>& diags,
        const std::function<const char* (const QoreDiagnostic&)>& source_text, FILE* os) {
    unsigned errors = 0;
    for (auto& d : diags) {
        if (d.error) {
            ++errors;
        }
        const char* sev = d.error ? "error" : "warning";
        // header: file:line:col: severity[semantic-id]: message
        fprintf(os, "%s:%d", d.file.empty() ? "<input>" : d.file.c_str(), d.start_line);
        if (d.start_column >= 0) {
            fprintf(os, ":%d", d.start_column);
        }
        fprintf(os, ": %s[%s]: %s\n", sev, d.metadata.id.c_str(), d.message.c_str());

        // code frame: the source line plus a caret line under the offending span
        std::string line;
        if (extract_source_line(source_text(d), d.start_line, line)) {
            fprintf(os, "  %s\n", line.c_str());
            if (d.start_column >= 0) {
                std::string caret("  ");
                // pad up to the start column (columns are 0-based offsets within the line)
                for (int i = 0; i < d.start_column && i < (int)line.size(); ++i) {
                    caret += (line[i] == '\t') ? '\t' : ' ';
                }
                caret += '^';
                int span = (d.end_column > d.start_column) ? (d.end_column - d.start_column - 1) : 0;
                for (int i = 0; i < span; ++i) {
                    caret += '~';
                }
                fprintf(os, "%s\n", caret.c_str());
            }
        }
        if (!d.metadata.hint.empty()) {
            fprintf(os, "  help: %s\n", d.metadata.hint.c_str());
        }
    }
    return errors;
}

const QoreProgramLocation* qore_program_private_base::getLocationIntern(const QoreProgramLocation& loc) {
    // the unique locations are protected by the stripe lock alone: a lookup that adds a location (most of them, at
    // parse time) takes one lock, and fork() waits for the change (see recordLock()); nothing else is acquired here
    std::lock_guard<std::mutex> rl(threadContainerLock());
    loc_set_t::iterator i = loc_set.lower_bound(&loc);
    if (i == loc_set.end() || ltpgm()(&loc, *i)) {
        std::unique_ptr<QoreProgramLocation> lp(new QoreProgramLocation(loc));
        pgmloc.push_back(lp.get());
        // the vector owns the location now
        const QoreProgramLocation* rv = lp.release();
        loc_set.insert(i, rv);
        return rv;
    }
    return *i;
}

const QoreProgramLocation* qore_program_private_base::getLocation(int sline, int eline) {
    return getLocationIntern(QoreProgramLocation(sline, eline));
}

const char* qore_program_private_base::addRuntimeString(const char* str) {
    std::lock_guard<std::recursive_mutex> lock(runtime_metadata_lock);
    str_set_t::iterator i = rt_str_set.lower_bound(str);
    if (i == rt_str_set.end() || strcmp(*i, str)) {
        // fork() waits for the change (see recordLock()); the copy is freed if it cannot be stored
        std::unique_ptr<char, decltype(&free)> copy(strdup(str), &free);
        if (!copy) {
            throw std::bad_alloc();
        }
        std::lock_guard<std::mutex> rl(threadContainerLock());
        rt_str_vec.push_back(copy.get());
        copy.release();
        i = rt_str_set.insert(i, rt_str_vec.back());
    }
    return *i;
}

const QoreProgramLocation* qore_program_private_base::getRuntimeLocation(const QoreProgramLocation& loc, int sline,
        int eline) {
    QoreProgramLocation loc1(loc);
    loc1.start_line = sline;
    loc1.end_line = eline;

    // the strings that the location refers to are not rolled back either (each is interned under the runtime
    // metadata lock, as addString() interns them)
    if (loc1.getFile()) {
        loc1.setFile(addRuntimeString(loc1.getFile()));
    }
    if (loc1.getSource()) {
        loc1.setSource(addRuntimeString(loc1.getSource()));
    }

    // the runtime locations are protected by the stripe lock alone, as the locations of parsed code
    // (getLocationIntern()); fork() waits for the change (see recordLock())
    std::lock_guard<std::mutex> rl(threadContainerLock());
    loc_set_t::iterator i = rt_loc_set.find(&loc1);
    if (i != rt_loc_set.end()) {
        return *i;
    }
    std::unique_ptr<QoreProgramLocation> lp(new QoreProgramLocation(loc1));
    rt_pgmloc.push_back(lp.get());
    // owned by rt_pgmloc from here on
    QoreProgramLocation* rv = lp.release();
    rt_loc_set.insert(rv);
    return rv;
}

const QoreProgramLocation* qore_program_private_base::getRuntimeLocation(int sline, int eline) {
    return getRuntimeLocation(QoreProgramLocation(sline, eline), sline, eline);
}

const QoreProgramLocation* qore_program_private_base::getLocation(int sline, int eline, int scol, int ecol) {
    return getLocationIntern(QoreProgramLocation(sline, eline, scol, ecol));
}

const QoreProgramLocation* qore_program_private_base::getLocation(const QoreProgramLocation& loc, int sline, int eline) {
    QoreProgramLocation loc1(loc);
    loc1.start_line = sline;
    loc1.end_line = eline;
    return getLocationIntern(loc1);
}

const QoreProgramLocation* qore_program_private_base::getLocation(const QoreProgramLocation& loc, int sline,
        int eline, int scol, int ecol) {
    QoreProgramLocation loc1(loc);
    loc1.start_line = sline;
    loc1.end_line = eline;
    loc1.start_column = QoreProgramLineLocation::normalizeColumn(scol);
    loc1.end_column = QoreProgramLineLocation::normalizeColumn(ecol);
    return getLocationIntern(loc1);
}

int64 qore_program_private_base::setModuleOwnDomainsIntern(int64 n_dom, bool apply) {
    module_own_dom |= n_dom;
    if (!apply) {
        return QDOM_DEFAULT;
    }
    module_trusted_dom |= n_dom;
    // the Program's own options: the module header's restrictions are scoped options of the parsing thread, which
    // the module's code does not have (issue #6207)
    int64 removed = n_dom & pwo.parse_options.loadShared().getLo();
    if (!removed) {
        return QDOM_DEFAULT;
    }
    {
        // a change for the whole Program, made while the module header's options are in effect
        SharedParseOptionsWrite spw;
        pwo.parse_options &= ~QoreParseOptions(removed);
    }
    // the module's %ifdef directives see the options it has
    for (auto& i : pomaps.pomap) {
        if (i.first && (removed & i.first) == i.first) {
            dmap_t::iterator di = dmap.find(i.second);
            if (di != dmap.end()) {
                dmap.erase(di);
            }
        }
    }
    return removed;
}

void qore_program_private_base::setDefines() {
    for (auto& i : pomaps.pomap) {
        if ((pwo.parse_options & i.first) == i.first) {
            dmap[i.second] = true;
        }
    }
}

namespace {

// module API version as a formatted string for ${QORE_MODULE_VERSION}
inline std::string qore_module_api_version_str() {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d.%d", QORE_MODULE_API_MAJOR, QORE_MODULE_API_MINOR);
    return buf;
}

// Expand ${NAME} variable references in `in` into `out`.
// Returns true on success, false on error (with `err` populated with a human-readable reason).
// Supports \$ as a literal $ escape.  Recognises the predefined macros listed in the design doc;
// falls back to the process environment for anything else.  Unknown macros and unset env vars
// produce an error (no silent empty substitution — see design doc "Error cases").
bool expand_module_path_macros(const std::string& in, std::string& out, std::string& err,
        qore_program_private_base& pp) {
    out.clear();
    out.reserve(in.size());
    const char* const data = in.data();
    const size_t n = in.size();
    for (size_t i = 0; i < n; ) {
        char c = data[i];
        if (c == '\\' && i + 1 < n && data[i + 1] == '$') {
            out.push_back('$');
            i += 2;
            continue;
        }
        if (c == '$' && i + 1 < n && data[i + 1] == '{') {
            // find closing '}'
            size_t j = i + 2;
            while (j < n && data[j] != '}') {
                ++j;
            }
            if (j >= n) {
                err = "expected '}' to close variable reference";
                return false;
            }
            std::string name(data + i + 2, j - (i + 2));
            if (name.empty()) {
                err = "empty variable name in ${...}";
                return false;
            }
            // predefined macros
            const char* expansion = nullptr;
            std::string owned;
            if (name == "SCRIPT_DIR") {
                if (pp.script_dir.empty()) {
                    err = "${SCRIPT_DIR} is not available (parse input has no on-disk file)";
                    return false;
                }
                expansion = pp.script_dir.c_str();
            } else if (name == "SCRIPT_NAME") {
                if (pp.script_name.empty()) {
                    err = "${SCRIPT_NAME} is not available (parse input has no on-disk file)";
                    return false;
                }
                expansion = pp.script_name.c_str();
            } else if (name == "SCRIPT_PATH") {
                if (pp.script_path.empty()) {
                    err = "${SCRIPT_PATH} is not available (parse input has no on-disk file)";
                    return false;
                }
                expansion = pp.script_path.c_str();
            } else if (name == "PROGRAM_DIR") {
                // Per design, PROGRAM_DIR refers to the TOP-LEVEL program file's directory —
                // same as SCRIPT_DIR for the top-level program, but in a module (.qm/.qc) it
                // yields the parent program's script_dir.  We walk up via the thread-local
                // getProgram() chain isn't stable here, so approximate with script_dir —
                // this matches user intent in the Qorus case because the entrypoint directive
                // fires during the top-level parse.  (Future work: thread a parent pointer
                // through qore_program_private to distinguish the two.)
                if (pp.script_dir.empty()) {
                    err = "${PROGRAM_DIR} is not available (parse input has no on-disk file)";
                    return false;
                }
                expansion = pp.script_dir.c_str();
            } else if (name == "QORE_VERSION") {
                expansion = QORE_VERSION;
            } else if (name == "QORE_PREFIX") {
                expansion = QORE_INSTALL_PREFIX;
            } else if (name == "QORE_MODULE_VERSION") {
                owned = qore_module_api_version_str();
                expansion = owned.c_str();
            } else {
                QoreEnvironmentValue env_val_holder(name.c_str());
                if (!env_val_holder) {
                    err = "undefined variable '";
                    err += name;
                    err += "' in path argument";
                    return false;
                }
                // the value is copied: the holder's copy of it ends with this block, before it is appended
                owned = env_val_holder.str();
                expansion = owned.c_str();
            }
            out.append(expansion);
            i = j + 1;
            continue;
        }
        if (c == '$' && i + 1 < n && data[i + 1] == '$') {
            // literal $ via $$
            out.push_back('$');
            i += 2;
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return true;
}

} // anonymous namespace

int qore_program_private_base::applyModulePathDirective(bool prepend, std::string raw_path,
        const QoreProgramLocation& loc) {
    const char* directive_name = prepend ? "prepend-module-path" : "append-module-path";

    // Strip surrounding whitespace
    auto lstrip = [](std::string& s) {
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
            ++i;
        }
        if (i) {
            s.erase(0, i);
        }
    };
    auto rstrip = [](std::string& s) {
        while (!s.empty()) {
            char c = s.back();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                s.pop_back();
            } else {
                break;
            }
        }
    };
    lstrip(raw_path);
    rstrip(raw_path);

    // Strip surrounding double-quotes, if any — the new-style directive uses quoted paths
    if (raw_path.size() >= 2 && raw_path.front() == '"' && raw_path.back() == '"') {
        raw_path = raw_path.substr(1, raw_path.size() - 2);
    }

    if (raw_path.empty()) {
        parse_error(loc, "missing argument to %%%s", directive_name);
        return -1;
    }

    std::string expanded;
    std::string err;
    if (!expand_module_path_macros(raw_path, expanded, err, *this)) {
        parse_error(loc, "%%%s: %s", directive_name, err.c_str());
        return -1;
    }

    if (expanded.empty()) {
        parse_error(loc, "%%%s: empty path computed from variable expansion", directive_name);
        return -1;
    }

    auto& target = prepend ? prepended_module_paths : appended_module_paths;

    // silent dedup (design: "silent dedup at insert time")
    for (const std::string& existing : target) {
        if (existing == expanded) {
            return 0;
        }
    }

    if (prepend) {
        target.insert(target.begin(), std::move(expanded));
    } else {
        target.push_back(std::move(expanded));
    }
    return 0;
}

QoreHashNode* qore_program_private::runTimeGetAllDefines() {
    AutoLocker al(plock);
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    qore_hash_private* ph = qore_hash_private::get(*h);

    for (auto& i : dmap) {
        ph->setKeyValueIntern(i.first.c_str(), i.second.refSelf());
    }
    return h;
}

void qore_program_private::doThreadInit(ExceptionSink* xsink) {
    // create/destroy temporary ExceptionSink object if necessary
    std::unique_ptr<ExceptionSink> xs;
    if (!xsink) {
        xs.reset(new ExceptionSink);
        xsink = xs.get();
    }

    // if there is any thread-initialization code, execute it here
    ReferenceHolder<ResolvedCallReferenceNode> ti(xsink);
    QoreThreadRuntimeParseOptions ti_po;
    {
        AutoLocker al(tlock);
        ti = thr_init ? thr_init->refRefSelf() : 0;
        if (ti) {
            ti_po = thr_init_po;
        }
    }
    if (ti) {
        // the code runs with the options of the code that set it (qore#6208)
        QoreThreadRuntimeParseOptionsHelper po_helper(&ti_po);
        ValueHolder v(ti->invoke(0, xsink), xsink);
    }
}

// returns true if the code may need a scan: a value the Program reports to the garbage collector
static bool program_code_needs_scan(const ResolvedCallReferenceNode* code) {
    return code && needs_scan(code);
}

// returns true if the handlers hold a value that needs a scan
static bool program_handlers_need_scan(const QoreThreadEntryHandlers* h) {
    return h && (program_code_needs_scan(h->on_enter) || program_code_needs_scan(h->on_exit));
}

bool qore_program_private::scanValuesNeedScanIntern() const {
    return program_code_needs_scan(thr_init)
        || program_handlers_need_scan(thread_entry_handlers.load(std::memory_order_relaxed));
}

void qore_program_private::scanValuesChangedIntern(bool removed,
        std::initializer_list<const ResolvedCallReferenceNode*> stored) {
    assert(scan_holder);
    // a value given up loses the reference that the object's recursive set may count as internal; the mark is made
    // before the value can be released, which is done after tlock (see RObject::edgesRemoved())
    if (removed && scan_registered) {
        scan_holder->edgesRemoved();
    }
    // the object scans its private data only while the Program holds a value that needs a scan
    bool need = scanValuesNeedScanIntern();
    if (need != scan_registered) {
        scan_registered = need;
        if (need) {
            scan_holder->incScanPrivateData();
        } else {
            scan_holder->decScanPrivateData();
        }
    }
    // no scan follows the new edges: the recursive sets they change must not trust their counts until one has; the
    // values are marked under tlock, while the Program still references them (see qore_dgc_value_stored())
    for (const ResolvedCallReferenceNode* code : stored) {
        if (program_code_needs_scan(code)) {
            qore_dgc_value_stored(*scan_holder, QoreValue(const_cast<ResolvedCallReferenceNode*>(code)));
        }
    }
}

void qore_program_private::setScanHolder(QoreObject& obj) {
    qore_object_private* o = qore_object_private::get(obj);
    AutoLocker al(tlock);
    assert(!scan_holder);
    assert(!scan_registered);
    o->tRef();
    scan_holder = o;
    // code set before the object held the Program
    QoreThreadEntryHandlers* h = thread_entry_handlers.load(std::memory_order_relaxed);
    scanValuesChangedIntern(false, {thr_init, h ? h->on_enter : nullptr, h ? h->on_exit : nullptr});
}

void qore_program_private::clearScanHolder() {
    qore_object_private* o;
    {
        AutoLocker al(tlock);
        if (!scan_holder) {
            return;
        }
        o = scan_holder;
        if (scan_registered) {
            // the object no longer reports values that its recursive set may count as internal
            o->edgesRemoved();
            o->decScanPrivateData();
            scan_registered = false;
        }
        scan_holder = nullptr;
    }
    // the weak reference is released without the lock, as it can free the object's memory
    o->tDeref();
}

void qore_object_private_data_released(qore_classid_t key, AbstractPrivateData* pd) {
    // the object that held the Program no longer reports its code: the Program can be released or owned by others
    if (key == CID_PROGRAM) {
        qore_program_private::clearScanHolder(*static_cast<QoreProgram*>(pd));
    }
}

bool qore_program_private::scanMembers(RObject& obj, RSetHelper& rsh) {
    // the lock is only tried: a scan must not block (see design/dgc.md, "A container that a scan finds locked")
    if (tlock.trylock()) {
        rsh.containerContended(obj);
        return false;
    }
    AutoLocker al(tlock, true);

    // the values are reported only by the object that the Program counts as their holder, so that each is reported
    // once, and only while it counts them, so that every value it gives up marks the object
    if (!scan_registered || static_cast<RObject*>(scan_holder) != &obj) {
        return false;
    }

    if (thr_init && obj.scanCheck(rsh, thr_init)) {
        return true;
    }
    QoreThreadEntryHandlers* h = thread_entry_handlers.load(std::memory_order_relaxed);
    if (h) {
        if (h->on_enter && obj.scanCheck(rsh, h->on_enter)) {
            return true;
        }
        if (h->on_exit && obj.scanCheck(rsh, h->on_exit)) {
            return true;
        }
    }
    return false;
}

// returns true if there was already a thread init closure set, false if not
bool qore_program_private::setThreadInit(const ResolvedCallReferenceNode* n_thr_init, ExceptionSink* xsink) {
    // the old code is released after the lock, as releasing it can run destructors
    ReferenceHolder<ResolvedCallReferenceNode> old(xsink);
    // the options of the code that sets it, which it runs with, without the application of a call of module code,
    // as it runs for every thread that enters the Program (qore#6208); the old ones are released after the lock, as
    // their references to Programs can be the last ones
    QoreThreadRuntimeParseOptions po = n_thr_init ? QoreThreadRuntimeParseOptions::captureForThread()
        : QoreThreadRuntimeParseOptions();
    {
        AutoLocker al(tlock);
        old = thr_init;
        thr_init = n_thr_init ? n_thr_init->refRefSelf() : nullptr;
        std::swap(thr_init_po.po, po.po);
        std::swap(thr_init_po.override, po.override);
        // the init code runs for the code that set it, with its restrictions (qore#6844)
        std::swap(thr_init_po.caller_domain_restrictions, po.caller_domain_restrictions);
        if (scan_holder) {
            scanValuesChangedIntern(program_code_needs_scan(*old), {thr_init});
        }
    }
    return (bool)old;
}

bool qore_program_private::setThreadEntryHandler(const ResolvedCallReferenceNode* on_enter,
        const ResolvedCallReferenceNode* on_exit, ExceptionSink* xsink) {
    QoreThreadEntryHandlers* nh = (on_enter || on_exit) ? new QoreThreadEntryHandlers(on_enter, on_exit) : nullptr;
    QoreThreadEntryHandlers* old;
    {
        AutoLocker al(tlock);
        old = thread_entry_handlers.exchange(nh, std::memory_order_relaxed);
        if (scan_holder) {
            scanValuesChangedIntern(program_handlers_need_scan(old),
                {nh ? nh->on_enter : nullptr, nh ? nh->on_exit : nullptr});
        }
    }
    // threads that ran the old entry handler hold their own reference until they have run the paired exit handler;
    // releasing the handlers can run destructors, so it is done without the lock
    if (!old) {
        return false;
    }
    old->deref(xsink);
    return true;
}

void qore_program_private_base::newProgram() {
    base_object = true;
    po_locked = false;
    exec_class = false;

    // a Program created without a parent is the root of a thread-local data family; the hash of each thread is
    // created when the thread first saves data in the family (see qore_program_private::getThreadDataRef())
    tld_root.store(pgm->priv, std::memory_order_release);

    //printd(5, "qore_program_private_base::newProgram() this: %p\n", this);

    // copy global feature list to local list
    for (const auto& i : qoreFeatureList) {
        featureList.insert(i.c_str());
    }

    // the namespaces are set up in the new Program, which is only made current: no code of the Program runs here
    // (qore#6185)
    QoreProgramContextHelper pch(pgm);

    // setup namespaces
    RootNS = qore_root_ns_private::copy(*staticSystemNamespace, pwo.parse_options, pgm);

    QoreNS = RootNS->rootGetQoreNamespace();
    assert(QoreNS);

    // setup initial defines
    // add platform defines
    dmap["QoreVersionString"] = new QoreStringNode(qore_version_string);
    dmap["QoreVersionMajor"] = qore_version_major;
    dmap["QoreVersionMinor"] = qore_version_minor;
    dmap["QoreVersionSub"] = qore_version_sub;
    dmap["QoreVersionPatch"] = qore_version_patch;
    dmap["QoreVersionBuild"] = qore_build_number;
    dmap["QoreVersionBits"] = qore_target_bits;
    dmap["QorePlatformCPU"] = new QoreStringNode(TARGET_ARCH);
    dmap["QorePlatformOS"] = new QoreStringNode(TARGET_OS);

#ifdef _Q_WINDOWS
    dmap["Windows"] = true;
#else
    dmap["Unix"] = true;
#endif

    if (pwo.parse_options & PO_IN_MODULE) {
        dmap["QoreHasUserModuleLicense"] = true;
    }

    // the built-in defines that the identity of an AOT image fixes (the version and the platform): a conditional on
    // them takes the same branch where the code is compiled and where it is loaded (qore#6351); not the options
    // below or QoreDebug, which depend on the build configuration of the library (a qmod compiled by one build of the
    // library is loaded by another of the same sources) or on the process (HAVE_SIGNAL_HANDLING)
    for (auto& i : dmap) {
        builtin_defines.insert(i.first);
    }

    QoreNamespace* ns = QoreNS->findLocalNamespace("Option");
    assert(ns);
    ConstantListIterator cli(qore_ns_private::getConstantList(ns));
    while (cli.next()) {
        QoreValue v = cli.getEntry()->getParseDefineValue();
        // skip boolean options defined as False
        if (v.getType() == NT_BOOLEAN && !v.getAsBool()) {
            continue;
        }

        dmap[cli.getName()] = v.refSelf();
    }

#ifdef DEBUG
    // if Qore library debugging is enabled, then set an option
    dmap["QoreDebug"] = true;
#endif
}

void qore_program_private_base::setCreatorChain(const QoreProgram& creator) {
    assert(creator_ids.empty());
    // the creator chain: the creator first, then the Programs that created it
    creator_ids.reserve(creator.priv->creator_ids.size() + 1);
    creator_ids.push_back(creator.priv->getProgramId());
    creator_ids.insert(creator_ids.end(), creator.priv->creator_ids.begin(), creator.priv->creator_ids.end());
}

void qore_program_private_base::addCreatorChain(const QoreProgram& creator) {
    // the chain of the parent is set when the child is created
    assert(!creator_ids.empty());
    qore_program_id_t creator_id = creator.priv->getProgramId();
    if (!isCreatedBy(creator_id)) {
        creator_ids.push_back(creator_id);
    }
    for (qore_program_id_t id : creator.priv->creator_ids) {
        if (!isCreatedBy(id)) {
            creator_ids.push_back(id);
        }
    }
}

void qore_program_private_base::setParent(QoreProgram* p_pgm, const QoreParseOptions& n_parse_options,
        const QoreParseOptions* p_parse_options) {
    //printd(5, "qore_program_private_base::setParent() this: %p parent: %p (parent lvl: %p) this: %p (this pgm: %p) "
    //    "parent po: %lld new po: %lld parent no_child_po_restrictions: %d\n", this, p_pgm,
    //    p_pgm->priv->sb.getLVList(), this, pgm, p_pgm->priv->pwo.parse_options, n_parse_options,
    //    p_pgm->priv->pwo.parse_options & PO_NO_CHILD_PO_RESTRICTIONS);

    TZ.store(p_pgm->currentTZ(), std::memory_order_release);

    setCreatorChain(*p_pgm);

    // the parent's options are read once, so that the test and the steps below use the same value (issue #6078); the
    // creator's checks read them already, and the child inherits that value, not one that changed meanwhile (#6176)
    QoreParseOptions p_po = p_parse_options ? *p_parse_options : p_pgm->priv->pwo.parse_options.load();
    // PO_IN_MODULE marks the Program of a user module: a Program that the module's code creates is not module code,
    // so it does not inherit the mark, or the caller walk would look past it as module code (qore#7067)
    p_po &= ~QoreParseOptions(PO_IN_MODULE);
    // if children inherit restrictions, then set all child restrictions
    if (!(p_po & PO_NO_CHILD_PO_RESTRICTIONS)) {
        // lock child parse options
        po_locked = true;
        // turn on all restrictions in the child that are set in the parent, and make sure all options that give more
        // freedom and are off in the parent program are turned off in the child; the mask must be the full-width one:
        // the legacy PO_POSITIVE_OPTIONS cannot hold extended (bits 64+) positive options, which would then pass to
        // the child unchecked
        pwo.parse_options = (pwo.parse_options | p_po) & (p_po | ~QoreParseOptions::POSITIVE_OPTIONS);
        // the child keeps the restrictions of its parent and those it was created with
        locked_restrictions = pwo.parse_options & ~QoreParseOptions::POSITIVE_OPTIONS;
    } else {
        pwo.parse_options = n_parse_options;
        po_locked = !(n_parse_options & PO_NO_CHILD_PO_RESTRICTIONS);
        if (po_locked) {
            // a locked child keeps the restrictions that it was created with
            locked_restrictions = n_parse_options & ~QoreParseOptions::POSITIVE_OPTIONS;
        }
    }

    // share the thread-local data of the parent's family
    joinThreadDataFamily(p_pgm);

    {
        // keeps the parent from being cleared while its namespaces are copied; it takes no parse lock: the copy is
        // made under the parent's namespace read lock, and ProgramControl::createEvalProgram() holds the parent's
        // parse lock as well, so that no code is parsed in it meanwhile.  The parent is the current Program
        // (Program::constructor()), or a Program that the caller holds a thread count on (createEvalProgram()), so
        // its data are not cleared while they are copied, also if its deletion starts meanwhile, in which case
        // entering it fails and the exception is discarded
        ExceptionSink rah_xsink;
        ProgramRuntimeParseAccessHelper rah(&rah_xsink, p_pgm);
        rah_xsink.clear();
        // setup derived namespaces
        RootNS = qore_root_ns_private::copy(*p_pgm->priv->RootNS, n_parse_options, pgm);
    }
    QoreNS = RootNS->rootGetQoreNamespace();

    // An explicit parent execution mode is inherited for as long as the child's parse options support it, including
    // after they change (by the child's own %modern, for example); otherwise the child selects its automatic mode from
    // its own parse options (modern -> tiered, legacy -> AST); see syncExecModeWithParseOptions()
    applyParseOptionImplications();
    inheritExecMode(p_pgm->priv);

    // copy parent feature list; this Program is not visible to other threads yet, but the parent's list gains
    // entries while a module is loaded into the parent in another thread
    {
        QoreSafeRWReadLocker rl(p_pgm->priv->featureLock);
        for (auto& i : p_pgm->priv->featureList) {
            assert(featureList.find(i) == featureList.end());
            featureList.insert(i);
        }
    }

    // copy top-level local variables in case any are referenced in static methods in the parent program (static
    // methods are executed in the child's space)
    const LVList* lvl = p_pgm->priv->sb.getLVList();
    if (lvl)
        sb.assignLocalVars(lvl);

    // copy program defines to child program, with the record of those that the parent's host did not set
    for (auto& i : p_pgm->priv->dmap) {
        dmap[i.first] = i.second.refSelf();
    }
    source_defines = p_pgm->priv->source_defines;

    // inherit parent's %prepend-module-path / %append-module-path lists so subprograms
    // see the same module search surface their parent established (design:
    // design/qore-module-structure.md, "Module Search Path")
    prepended_module_paths = p_pgm->priv->prepended_module_paths;
    appended_module_paths = p_pgm->priv->appended_module_paths;

    // the external data of the parent is copied after this Program is fully constructed; see
    // copyExternalData(), called from qore_program_private's constructor, which reads the parent's external data
    // under the parent's lock
    ext_parent = !(n_parse_options & PO_NO_INHERIT_PROGRAM_DATA) ? p_pgm : nullptr;
}

// see InheritAllUserCodeHelper
thread_local bool q_inherit_all_user_code = false;

namespace {
//! a copy of the external data of a Program made by the current thread in copyExternalData()
/** The frames of a thread form a stack, as a copy() can create a Program itself, which copies the data of its own
    parent.  The child of a fork() counts the copies of its forking thread with them: the copies of the parent's other
    threads do not go on in the child (see qore_program_private::fixupAfterFork()).
*/
struct ExtDataCopyFrame {
    const qore_program_private_base* parent;
    ExtDataCopyFrame* prev;
};

thread_local ExtDataCopyFrame* ext_data_copy_frames = nullptr;
}

unsigned qore_program_private_base::getOwnExtDataCopies() const {
    unsigned rc = 0;
    for (const ExtDataCopyFrame* f = ext_data_copy_frames; f; f = f->prev) {
        if (f->parent == this) {
            ++rc;
        }
    }
    return rc;
}

int qore_program_private_base::copyExternalData(ExceptionSink* xsink) {
    if (!ext_parent) {
        return 0;
    }
    QoreProgram* p_pgm = ext_parent;
    ext_parent = nullptr;
    qore_program_private_base* parent = p_pgm->priv;

    // The entries are taken under the parent's lock, and copied without it: copy() runs module code, which takes
    // the module's own locks, while a thread of the module can hold one of them when it takes the Program lock (ex:
    // to parse or to look up its external data), which would deadlock with a copy made under the lock.  The entries
    // stay valid meanwhile: removeExternalData() and del() wait until no copy is in progress, and the parent is
    // referenced by the code creating the child Program.
    std::vector<extmap_t::value_type> parent_data;
    {
        AutoLocker al(parent->plock);
        if (parent->extmap.empty()) {
            return 0;
        }
        parent_data.assign(parent->extmap.begin(), parent->extmap.end());
        ++parent->ext_copy_count;
    }
    ON_BLOCK_EXIT([parent] () {
        AutoLocker al(parent->plock);
        assert(parent->ext_copy_count);
        if (!--parent->ext_copy_count && parent->ext_copy_waiting) {
            parent->ext_copy_cond.broadcast();
        }
    });
    // a copy() can create a Program itself, which copies the data of its own parent; the frame is removed before the
    // count is decremented
    ExtDataCopyFrame frame = {parent, ext_data_copy_frames};
    ext_data_copy_frames = &frame;
    ON_BLOCK_EXIT([&frame] () {
        ext_data_copy_frames = frame.prev;
    });

    // the errors of the module callbacks; the caller's sink may hold exceptions already (issue #6786)
    ExceptionSink copy_xsink;
    for (auto& i : parent_data) {
        // External data whose state belongs to the parent alone returns nullptr rather than a copy (the AOT
        // script data does: a child Program runs its own AOT load).  Storing that null would crash the
        // init() loop below, and again when the child's teardown calls doDeref() on it.
        AbstractQoreProgramExternalData* copy = qore_ext_data_copy(i.second, pgm, &copy_xsink);
        if (copy_xsink) {
            // a copy made with the exception was released already
            assert(!copy);
            break;
        }
        if (copy) {
            // the Program is registered already, so other threads can look up its external data
            AutoLocker al(plock);
            std::lock_guard<std::mutex> rl(threadContainerLock());
            extmap.insert(extmap_t::value_type(i.first, copy));
        }
    }
    if (!copy_xsink) {
        for (auto& i : extmap) {
            qore_ext_data_init(i.second, &copy_xsink);
            if (copy_xsink) {
                break;
            }
        }
        if (!copy_xsink) {
            return 0;
        }
    }

    // a copy or an init() failed: the child Program is not created (fail-closed: it must not run without the data of
    // a module, issue #6786); the copies already made are released, whether init() ran on them or not
    extmap_t copies;
    {
        AutoLocker al(plock);
        copies = extmap;
        // fork() waits for the change (see recordLock())
        std::lock_guard<std::mutex> rl(threadContainerLock());
        extmap.clear();
    }
    for (auto& i : copies) {
        qore_ext_data_deref(i.second, &copy_xsink);
    }
    if (xsink) {
        xsink->assimilate(copy_xsink);
    }
    // otherwise the destructor of copy_xsink reports the exceptions as unhandled
    return -1;
}

int qore_program_private::beginParse(ExceptionSink* xsink, ParseStartGuard& guard, bool nested) {
    {
        // PO_ALLOW_REPARSE is read and the mark set with the write lock of the options held, which the changes of the
        // options hold as well (see checkReparseOptionChange()); the write lock before plock
        std::lock_guard<std::recursive_mutex> wl(pwo.parse_options.writeLock());
        AutoLocker al(plock);
        if (!nested && checkParseIntern(xsink)) {
            return -1;
        }
        // set in the same critical section as the check that no thread runs the Program's code, so that no thread
        // enters it between them; a Program without PO_ALLOW_REPARSE makes no such check, and sets the flag when its
        // code is parsed, as before
        if ((pwo.parse_options & PO_ALLOW_REPARSE) && !parsing_in_progress) {
            parsing_in_progress = true;
            guard.began = true;
            // the strings and locations that exist now were not added by this parse: expressions created since the
            // last commit and code that ran meanwhile refer to them, so a rollback of the parse must not delete them
            // (issue #6354)
            std::lock_guard<std::recursive_mutex> lock(runtime_metadata_lock);
            // the locations are changed under the stripe lock alone (see getLocationIntern())
            std::lock_guard<std::mutex> rl(threadContainerLock());
            str_vec_hwm = str_vec.size();
            pgmloc_hwm = pgmloc.size();
        }
        // the threads that code of an earlier parse or of a debugger evaluation started are not this parse's
        // (issue #6243)
        clearParseTidsIntern();
    }
#ifdef DEBUG
    if (qore_dbg_parse_start_gate(pgm, xsink)) {
        return -1;
    }
#endif
    return 0;
}

void qore_program_private::internParseRollback(ExceptionSink* xsink) {
    // the code that sandboxed code parsed is gone, and so is the reason to attach its sandbox
    dropPendingCreatorSandbox();
    bool atomic_rollback = static_cast<bool>(pwo.parse_options & PO_ALLOW_REPARSE);
    // delete pending changes to namespaces: only what the failed parse added is removed, also without
    // PO_ALLOW_REPARSE, as the namespaces hold what the Program had before the parse, including the classes and
    // other symbols inherited from its parent when it was created, which a new parse must still find
    {
        // the declarations that the rollback removes name each other's types without references: they are destroyed
        // together, as a deleted Program's are, and the tombstones of those that derived types name are made while
        // all of them are alive (issue #6419)
        std::vector<QoreTypeOwner*> owners;
        qore_root_ns_private::get(*RootNS)->getRollbackTypeOwners(owners);
        QoreTypeOwnerBatch type_owner_batch(owners);
        qore_root_ns_private::get(*RootNS)->parseRollback(xsink, true);
    }

    // delete pending statements
    sb.parseRollback();
    native_parse_info = committed_native_parse_info;

    // free temporary data structures; the strings are protected by the runtime metadata lock, as code that is
    // deserialized lazily adds to them (issue #6354), and the structure of the strings and locations is changed under
    // the stripe lock, taken after it (see recordLock()): the entries are removed under it, and what they point to is
    // freed outside of it
    {
        std::lock_guard<std::recursive_mutex> lock(runtime_metadata_lock);
        // With PO_ALLOW_REPARSE, we do atomic rollback: only delete the items that the failed parse added (above the
        // high water marks, see beginParse()), preserving committed state for continued use. Without
        // PO_ALLOW_REPARSE, the program is considered unusable after rollback anyway, so we just clear the vectors
        // without freeing (the memory will be cleaned up when the Program is destroyed). This preserves backward
        // compatibility where error messages could still reference parse locations from failed parses.
        bool atomic = static_cast<bool>(pwo.parse_options & PO_ALLOW_REPARSE);
        std::vector<char*> strings;
        std::vector<QoreProgramLocation*> locations;
        {
            std::lock_guard<std::mutex> rl(threadContainerLock());
            str_set.clear();
            loc_set.clear();
            if (atomic) {
                // the pending strings and locations (those added after the high water marks) are taken out of the
                // Program: both are copied before either is removed, so that a failed allocation leaves the Program
                // owning all of them
                strings.insert(strings.end(), str_vec.begin() + str_vec_hwm, str_vec.end());
                locations.insert(locations.end(), pgmloc.begin() + pgmloc_hwm, pgmloc.end());
                str_vec.resize(str_vec_hwm);
                pgmloc.resize(pgmloc_hwm);
            }
        }
        for (char* str : strings) {
            free(str);
        }
        for (QoreProgramLocation* l : locations) {
            delete l;
        }
    }
    // Note: without PO_ALLOW_REPARSE, we don't clear str_vec/pgmloc - the memory will be freed
    // when the Program object is destroyed. This is intentional to avoid use-after-free when
    // error messages reference parse locations from the failed parse.

    // issue #2907 delete & clear statement index maps when doing a parse rollback
    for (auto& i : statementByFileIndex) {
        delete i.second;
    }
    statementByFileIndex.clear();

    for (auto& i : statementByLabelIndex) {
        delete i.second;
    }
    statementByLabelIndex.clear();

    // with PO_ALLOW_REPARSE, the Program is back in its committed state, so it can be used at runtime again; without
    // it, it cannot be parsed again
    if (atomic_rollback) {
        parsing_in_progress = false;
    }
    {
        AutoLocker al(plock);
        clearParseTidsIntern();
    }
}

//! the Programs that are left allocated at the end of the process; never destroyed, so they stay reachable
static std::set<QoreProgram*>* left_programs = nullptr;
static std::mutex* left_programs_lock = new std::mutex;
// a leaf lock: its holders only change or read the set
static QoreForkStdMutex left_programs_fork_handler(*left_programs_lock, QFLR_LEAF, "left_programs");

void qore_program_private::leaveAllocated() {
    std::lock_guard<std::mutex> al(*left_programs_lock);
    if (!left_programs) {
        left_programs = new std::set<QoreProgram*>;
    }
    // the reference is taken once per Program
    if (left_programs->insert(pgm).second) {
        depRef();
    }
}

bool qore_program_private::hasLeftPrograms() {
    std::lock_guard<std::mutex> al(*left_programs_lock);
    return left_programs && !left_programs->empty();
}

void qore_program_private::waitForTerminationAndClear(ExceptionSink* xsink) {
    // the code of the Program's classes that runs while it is deleted (its exit hooks, the cleanup callback and the
    // destructors run when its data is deleted) runs in it although its last reference may have been released (see
    // qore_class_private::getSourceProgramForCall()); the Program stays allocated until this function returns
    QoreProgramTeardownScope teardown_scope(pgm);

    // the exit hooks enter the Program: a parse state that keeps threads out of it is discarded first if no thread
    // runs in it, otherwise once its threads have ended, below (qore#6795)
    discardPendingParse(xsink, true);

    // the exit hooks that the Program registered can stop the threads waited for below
    qore_run_exit_hooks(pgm);

    // detach itself from debug
    if (qore_debug_program_private* dp = dpgm.load(std::memory_order_relaxed)) {
        // if the object is being destroyed when thread is terminating via a deref then we send sync detach event to
        // this thread the thread can be stopped.
        ThreadLocalProgramData *tlpd = get_thread_local_program_data();
        if (tlpd) {
            tlpd->dbgDetach(xsink);
        }
        dp->removeProgram(pgm);
    }

    // this is probably not necessary, as the next waiting for thread
    // termination will wait for the same threads as well
    debug_program_counter.waitForZero(xsink, 0);

    // we only clear the internal data structures once
    bool clr = false;
    bool need_cleanup = false;
    bool leave = false;
    {
        ReferenceHolder<QoreListNode> l(xsink);
        {
            AutoLocker al(plock);
            // wait for all threads to terminate
            waitForAllThreadsToTerminateIntern();
            // a foreign thread that runs in the Program at the end of the process is not waited for, and the data
            // that it uses is not destroyed (see leaveForeignThreads())
            leave = qore_shutdown.load(std::memory_order_acquire) && !ptid
                && (getOtherForeignThreadCountIntern() > 0 || leaveForeignThreadsInModuleCode());
            if (leave) {
                // the Program is kept allocated for the thread
                leaveAllocated();
            } else if (!ptid) {
                // no foreign thread can enter the Program from now on (see canEnterIntern()): the decision above is
                // final, and is made before any of the Program's data is destroyed
                foreign_teardown_tid = qore_shutdown.load(std::memory_order_acquire) ? q_gettid() : 0;
                if (!ns_const) {
                    l = new QoreListNode(autoTypeInfo);
                    qore_root_ns_private::clearConstants(*RootNS, **l);
                    ns_const = true;
                }
                need_cleanup = true;
            }
        }
        // the objects of the Program's own classes that its constants hold, if anything else holds them, are deleted
        // below, once only this thread can run code in the Program: the code can hold one of them past this point (a
        // constant's value folded into it), and the release of the code comes after the Program has been closed to
        // every thread (#6791)
        if (*l) {
            collectOwnedConstantObjects(**l);
        }
    }

    if (leave) {
        printd(5, "qore_program_private::waitForTerminationAndClear() leaving pgm: %p: a foreign thread runs in it\n",
            pgm);
        return;
    }

    // Call the cleanup callback OUTSIDE plock and BEFORE ptid is set.
    // The callback (e.g., async I/O cancelByProgram) cancels pending
    // operations and delivers results via call_dispatcher workers that
    // call evalMethod → incThreadCount.  incThreadCount takes plock, so
    // the callback must not hold plock.  ptid must not be set yet,
    // otherwise workers on other threads fail with PROGRAM-ERROR.
    if (need_cleanup) {
        qore_call_program_cleanup_callback(pgm);
#ifdef DEBUG
        if (dbg_teardown_hook_t hook = dbg_teardown_hook.load()) {
            hook(pgm, DbgTeardownPoint::AfterCleanupCallback);
        }
#endif
    }

    if (need_cleanup) {
        AutoLocker al(plock);
        // mark the program so that only code from this thread can run during data destruction
        ptid = q_gettid();
        // the fast paths that run the code of its variants without entering it take the call path from now on,
        // which refuses the calls of other threads and of other Programs (qore#5822)
        markUserVariantsDeleted();
#ifdef DEBUG
        if (dbg_teardown_hook_t hook = dbg_teardown_hook.load()) {
            hook(pgm, DbgTeardownPoint::BeforeFinalWait);
        }
#endif
        // threads could enter the Program while the cleanup callback ran (ptid was not set yet): wait for them to
        // leave it, as its data is destroyed below; no other thread can enter it now
        waitForAllThreadsToTerminateIntern();
        clr = true;
    }

    if (clr) {
        // only this thread can run code in the Program now, and its code is intact (#6791)
        deleteOwnedConstantObjects(xsink);
    }

    if (clr) {
        // the destructors run below enter the Program, which a parse state left until now would refuse (qore#6795)
        discardPendingParse(xsink, false);

        // purge thread resources before clearing pgm
        purge_pgm_thread_resources(pgm, xsink);

        //printd(5, "qore_program_private::waitForTerminationAndClear() this: %p pgm: %p clr: %d\n", this, pgm, clr);

#ifdef HAVE_SIGNAL_HANDLING
        // issue #5766: the signal handlers of this Program are removed before any of its data is destroyed: a handler
        // is code that runs in the Program, and its closure can hold the last reference to an object (a server that
        // the handler stops, for example), whose destructor runs user code; released after the data below, the
        // object was destroyed in a Program whose data had been deleted, and its destructor raised PROGRAM-ERROR.
        // The Program's threads have ended and no thread can enter it any more; a callback that is still finishing in
        // the signal thread (it has left the Program, but its handler is in progress) is waited for, and its handler
        // is handed over to this thread and released here, as the signal thread cannot enter the Program any more.
        // The code that runs below sets no handler (see QoreSignalManager::setHandler()), so none is left when the
        // Program is deleted.
        QSM.removeProgramHandlers({pgm}, xsink);
#endif

        // Discard the exec-class instance now that all threads have terminated.
        // This must happen before clearLocalVars()/clearNamespaceData() because the
        // instance's destructor may run user code that references globals or namespace data.
        exec_class_inst.discard(xsink);
        exec_class_inst = QoreValue();

        // the del closures of the local modules that a compiled script embeds run while the data of the Program
        // exists, as the del closure of a source module that a script loads does (qore#6015)
        runAOTScriptDelClosures();

        // release objects captured from languages without deterministic GC (jni, python) while the
        // program is still valid (data not yet cleared, ptid == tid), so their destructors run in a
        // valid program context instead of failing against a half-destroyed program
        clearSavedObjects(xsink);

        // break any cycle held up by an opaque reference ('@=') for the same reason and at the
        // same point: the collector cannot see those edges, so nothing else will ever release
        // them, and deleting the targets runs user destructors that need a program that is still
        // valid (data not yet cleared, ptid == tid).  This bounds the leak an opaque reference can
        // cause to the lifetime of the Program that created it.
        clearOpaqueTargets(xsink);

        // issue #3521: clear local variables first
        clearLocalVars(xsink);

        // delete all global variables, etc
        clearNamespaceData(xsink);

        // clear thread init code reference if any
        setThreadInit(nullptr, xsink);

        // clear the thread entry and exit handlers: they ran for the destructors run above, which are the last code
        // that runs in the Program on behalf of other threads' data
        setThreadEntryHandler(nullptr, nullptr, xsink);

        // no thread can start in the Program anymore: the lifecycle subscriptions are closed once the threads that
        // have left it have published their FINISHED events
        thread_hub->close(xsink);

        // free the thread-local data of every thread in this Program's family if it is a root; this is where the
        // data of the tearing-down thread used to be cleared, so that destructors run above can still use it
        freeFamilyThreadData(false, xsink);

        clearProgramThreadData(xsink);

        {
            AutoLocker al(plock);
            ptid = -1;
        }

        // now clear the original map
        {
            AutoLocker al(tlock);
            {
                std::lock_guard<std::mutex> cl(threadContainerLock());
                pgm_data_map.clear();
            }
            tclear = 0;

            if (twaiting)
                tcond.broadcast();
        }
        // merge pending parse exceptions into the passed exception sink, if any
        if (pendingParseSink) {
            xsink->assimilate(pendingParseSink);
            pendingParseSink = nullptr;
        }

        // clear any exec-class return value
        exec_class_rv.discard(xsink);
        exec_class_rv = QoreValue();

        // clear the sandbox managers if set
        {
            releaseSandboxManagers(xsink);
        }

        // delete code
        // method call can be repeated
        sb.del();
        //printd(5, "QoreProgram::~QoreProgram() this: %p deleting root ns %p\n", this, RootNS);

        // clear expressions; they are deleted with the lock released, as their deletion can run code
        q_exp_set_t exps;
        {
            std::lock_guard<std::mutex> el(exp_set_lock);
            std::lock_guard<std::mutex> rl(threadContainerLock());
            exps.swap(exp_set);
        }
        for (auto& i : exps) {
            delete i;
        }

        del(xsink);

        // release parse caches now that the program is cleared; their structure is changed under the stripe lock
        // (see recordLock())
        {
            std::lock_guard<std::mutex> rl(threadContainerLock());
            str_set.clear();
            loc_set.clear();
            str_vec.clear();
            str_vec.shrink_to_fit();
            pgmloc.clear();
            pgmloc.shrink_to_fit();
            statementIds.clear();
            statementIds.shrink_to_fit();
            reverseStatementIds.clear();

            // deque-backed arena: clearing destroys LocalVars in-place;
            // no manual delete loop needed.
            local_var_list.clear();
        }

        // Deferred AOT module initialization is scoped to this Program. Release
        // the per-Program markers together with the parse data so repeated
        // short-lived Program imports cannot retain module-name allocations.
        std::unordered_set<std::string>().swap(merged_aot_modules);
        std::unordered_set<std::string>().swap(initializing_aot_modules);
        std::unordered_set<std::string>().swap(initialized_aot_modules);

        // clear program location
        //update_runtime_location(&loc_builtin);
    }
}

namespace {
//! collects the objects of the Program's own classes in a value of a cleared constant, also in its lists and hashes
class QoreOwnedConstantObjectCollector {
public:
    DLLLOCAL QoreOwnedConstantObjectCollector(const QoreProgram* pgm, std::vector<QoreObject*>& objs)
            : pgm(pgm), objs(objs) {
    }

    DLLLOCAL void visit(QoreValue v) {
        switch (v.getType()) {
            case NT_OBJECT: {
                QoreObject* o = v.get<QoreObject>();
                // an object that only the constant holds is deleted when the list is released, also while the
                // Program can run code
                if (o->reference_count() > 1 && o->isValid()
                        && qore_class_private::get(*o->getClass())->ownedByProgram(pgm)
                        && seen.insert(o).second) {
                    o->ref();
                    objs.push_back(o);
                }
                break;
            }
            case NT_LIST: {
                const QoreListNode* l = v.get<const QoreListNode>();
                if (!seen.insert(l).second) {
                    break;
                }
                ConstListIterator i(l);
                while (i.next()) {
                    visit(i.getValue());
                }
                break;
            }
            case NT_HASH: {
                const QoreHashNode* h = v.get<const QoreHashNode>();
                if (!seen.insert(h).second) {
                    break;
                }
                ConstHashIterator i(h);
                while (i.next()) {
                    visit(i.get());
                }
                break;
            }
            default:
                break;
        }
    }

private:
    const QoreProgram* pgm;
    std::vector<QoreObject*>& objs;
    //! the containers and objects visited, which constant values can share
    std::unordered_set<const void*> seen;
};
}

void qore_program_private::collectOwnedConstantObjects(const QoreListNode& l) {
    QoreOwnedConstantObjectCollector c(pgm, owned_constant_objects);
    ConstListIterator i(&l);
    while (i.next()) {
        c.visit(i.getValue());
    }
}

void qore_program_private::deleteOwnedConstantObjects(ExceptionSink* xsink) {
    std::vector<QoreObject*> objs;
    objs.swap(owned_constant_objects);
    for (QoreObject* o : objs) {
        // another holder can have released it meanwhile, or deleted it
        if (o->isValid()) {
            o->doDelete(xsink);
        }
        o->deref(xsink);
    }
}

void qore_program_private::discardPendingParse(ExceptionSink* xsink, bool idle_only) {
    // the usual case: the Program is not in the parse state; nothing is left to discard once the Program has been
    // cleared (RootNS is deleted then)
    if (!parsing_in_progress || !RootNS) {
        return;
    }
    // the code that the rollback runs, such as the destructors of pending values, runs under the parse lock, as for
    // Program::parseRollback(); the lock is not waited for: a thread that holds it parses the Program
    ExceptionSink lock_xsink;
    ProgramRuntimeParseContextHelper pch(&lock_xsink, pgm, true);
    if (lock_xsink) {
        lock_xsink.clear();
        return;
    }
    {
        AutoLocker al(plock);
        // a parse count above one: the current thread held the parse lock already, and parses the Program
        if (!parsing_in_progress || parse_count > 1 || (idle_only && thread_count)) {
            return;
        }
    }
    internParseRollback(xsink);
    // without PO_ALLOW_REPARSE, the rollback leaves the state set (the Program cannot be parsed again); the code run
    // by the deletion of the Program can enter it now
    AutoLocker al(plock);
    parsing_in_progress = false;
}

// Helper function to eagerly compile all user-defined functions to IR/JIT when --exec-mode is specified.
// Native (LLVM) compilation is never done here; see the call site for why.
static void eagerlyCompileAllFunctions(qore_ns_private* ns, qore_exec_mode_t exec_mode) {
    // Walk functions in this namespace
    for (auto i = ns->func_list.begin(), e = ns->func_list.end(); i != e; ++i) {
        FunctionEntry* fe = i->second;
        QoreFunction* func = fe->getFunction();
        if (!func) {
            continue;
        }

        // Skip functions imported from modules: eager compilation must cover only
        // the program's OWN code.  Eager-compiling entire module trees (e.g. the
        // DataProvider/Util/ConnectionProvider stacks) at parse-commit is
        // impractically slow; module hot code is promoted to native at runtime
        // via the tiered execution-count threshold instead.
        if (func->getModuleName()) {
            continue;
        }

        // Iterate through all variants of the function
        QoreFunctionIterator vit(*func);
        while (vit.next()) {
            const AbstractQoreFunctionVariant* variant = vit.getVariant();
            UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
            if (!uvb || !uvb->hasBody()) {
                continue;
            }

            // Eagerly compile to IR/JIT as requested
            uvb->eagerlyCompileForExecMode(func->getName(), exec_mode);
        }
    }

    // Eagerly compile class methods for user classes defined in this parse context.
    // Module-imported classes (from_module non-empty) are skipped here. The HttpServerHttp3Stress
    // timeout problem was investigated extensively (9-second CPU vs 120-second wall-clock) and
    // determined to be caused by lock contention in the polling loop itself, not method compilation
    // overhead. Eager compilation of class methods improves performance for methods that reach
    // threshold-based warmup, but does not resolve architectural bottlenecks in polling/concurrency.
    // Builtin/system classes (sys == true) have no Qore bytecode to lower.
    ClassListIterator cli(ns->classList);
    while (cli.next()) {
        QoreClass* qc = cli.get();
        if (!qc) {
            continue;
        }
        qore_class_private* qcp = qore_class_private::get(*qc);
        if (qcp->sys || !qcp->from_module.empty()) {
            continue;
        }

        // Non-static methods
        for (auto& kv : qcp->hm) {
            QoreMethod* m = kv.second;
            qore_method_private* mp = qore_method_private::get(*m);
            MethodFunctionBase* mfb = mp->getFunction();
            QoreFunctionIterator vit(*mfb);
            while (vit.next()) {
                const AbstractQoreFunctionVariant* av = vit.getVariant();
                UserVariantBase* uvb =
                    const_cast<AbstractQoreFunctionVariant*>(av)->getUserVariantBase();
                if (!uvb || !uvb->hasBody()) {
                    continue;
                }
                uvb->eagerlyCompileForExecMode(m->getName(), exec_mode);
            }
        }

        // Static methods
        for (auto& kv : qcp->shm) {
            QoreMethod* m = kv.second;
            qore_method_private* mp = qore_method_private::get(*m);
            MethodFunctionBase* mfb = mp->getFunction();
            QoreFunctionIterator vit(*mfb);
            while (vit.next()) {
                const AbstractQoreFunctionVariant* av = vit.getVariant();
                UserVariantBase* uvb =
                    const_cast<AbstractQoreFunctionVariant*>(av)->getUserVariantBase();
                if (!uvb || !uvb->hasBody()) {
                    continue;
                }
                uvb->eagerlyCompileForExecMode(m->getName(), exec_mode);
            }
        }
    }

    // Recursively compile functions in child namespaces, skipping namespaces
    // imported from modules — their functions/methods belong to the module, not
    // the program's own code, and eager-compiling the whole module namespace tree
    // at parse-commit is impractically slow.  Module hot code promotes to native
    // at runtime via the tiered threshold.
    for (auto ni = ns->nsl.nsmap.begin(), ne = ns->nsl.nsmap.end(); ni != ne; ++ni) {
        QoreNamespace* child_ns = ni->second;
        if (child_ns) {
            qore_ns_private* child_priv = qore_ns_private::get(*child_ns);
            if (child_priv->imported) {
                continue;
            }
            eagerlyCompileAllFunctions(child_priv, exec_mode);
        }
    }
}

// called when the program's ref count = 0 (but the dc count may not go to 0 yet)
void qore_program_private::clear(ExceptionSink* xsink) {
    waitForTerminationAndClear(xsink);
    depDeref();
}

int qore_program_private::internParseCommit(bool standard_parse) {
    QORE_TRACE("qore_program_private::internParseCommit()");
    printd(5, "qore_program_private::internParseCommit() pgm: %p isEvent: %d standard_parse: %d parsing_done: %d\n", pgm, parseSink->isEvent(), standard_parse, parsing_done);

    // if a parse exception has occurred, then back out all new
    // changes to the QoreProgram atomically
    int rc;
    if (standard_parse) {
        // In REPL mode (PO_ALLOW_REPARSE), parsing_done may already be set
        assert(!parsing_done || (pwo.parse_options & PO_ALLOW_REPARSE));

        // if the first stage of parsing has already failed,
        // then don't go forward
        if (!parseSink->isEvent()) {
            // the top-level code and the namespaces are not in a class (classes set their own context): the Program can
            // be parsed while a method runs (ex: in Program::parse()), and the method's class is then the thread's class
            ClassOnlySubstitutionHelper class_helper(nullptr);
            // the functional operators decide which operands are statements of their own when all the code has been
            // initialized (qore#6288)
            QoreElementScopeParseHelper element_scope_helper;

            // the missing return checks of the code wait until all of the code is initialized, as they analyze the
            // code that it calls (issue #5940)
            QoreMissingReturnCheckHelper mrch(missing_return_checks);

            // initialize new statements second (for "our" and "my" declarations)
            // also initializes namespaces, constants, etc
            sb.parseInit();

            // do the initial parse phase
            qore_root_ns_private::get(*RootNS)->parseInit();

            if (!parseSink->isEvent()) {
                missing_return_checks.run(pgm);
            }

            printd(5, "QoreProgram::internParseCommit() this: %p RootNS: %p\n", pgm, RootNS);
        }

        if (parseSink->isEvent()) {
            internParseRollback(parseSink);
            requires_exception = false;
            rc = -1;
        } else if (commitPendingCreatorSandbox(parseSink)) {
            // the code that sandboxed code parsed is not committed without its sandbox
            internParseRollback(parseSink);
            requires_exception = false;
            rc = -1;
        } else { // otherwise commit them
            // the code that the commit runs can enter the Program, other threads cannot until the commit has finished
            // (issue #6243); both flags are changed in one critical section of plock, which incThreadCount() takes
            {
                AutoLocker al(plock);
                commit_entry_restricted = true;
                parsing_in_progress = false;
            }
            // lifts the restriction when the commit ends, also with an exception
            class CommitEntryRestrictionHelper {
            public:
                CommitEntryRestrictionHelper(qore_program_private& pp) : pp(pp) {
                }
                ~CommitEntryRestrictionHelper() {
                    AutoLocker al(pp.plock);
                    pp.commit_entry_restricted = false;
                    pp.clearParseTidsIntern();
                }
            private:
                qore_program_private& pp;
            } cerh(*this);
#ifdef DEBUG
            qore_dbg_parse_commit_gate(pgm, 0);
#endif
            parsing_done = true;

            // merge pending namespace additions and run namespace-level runtime init
            parse_commit_in_progress = true;
            try {
                qore_root_ns_private::parseCommit(*RootNS);
            } catch (...) {
                parse_commit_in_progress = false;
                throw;
            }
            parse_commit_in_progress = false;

            // commit pending statements
            sb.parseCommit(pgm);
            committed_native_parse_info = native_parse_info;

#ifdef DEBUG
            qore_dbg_parse_commit_gate(pgm, 1);
#endif
            initDeferredCode();
            assert(dset.empty());

            {
                // Runtime initialization can admit threads that materialize AOT metadata.
                std::lock_guard<std::recursive_mutex> lock(runtime_metadata_lock);
                std::lock_guard<std::mutex> rl(threadContainerLock());
                str_set.clear();
                loc_set.clear();

                // update high water marks for atomic rollback support
                str_vec_hwm = str_vec.size();
                pgmloc_hwm = pgmloc.size();
            }

            // After directives and extension-based defaults are known, choose
            // the execution mode: an inherited explicit mode if the final parse options support it (they may have
            // been restored without a sync, see restoreParseOptions()), otherwise the implicit mode; modern code
            // defaults to tiered, which also validates the %modern IR-lowerability guarantee below.
            syncExecModeWithParseOptions();

            // Eagerly compile all functions if --exec-mode=ir, --exec-mode=jit, or --exec-mode=tiered was specified.
            // Gate on PO_MODERN: `ensureIrExecMode` in parseCommit runs
            // right after this and will downgrade non-%modern programs to
            // QEM_AST.  Without the gate, every attemptIRLowering call
            // records a spurious `lowering: ...` fallback (e.g. `break`
            // under PO_BROKEN_LOOP_STATEMENT is legal in AST but fails
            // IR lowering) before the exec-mode is downgraded.
            if ((exec_mode == QEM_IR || exec_mode == QEM_JIT || exec_mode == QEM_TIERED)
                    && standard_parse
                    && qore_parse_options_support_ir(pwo.parse_options)) {
                // Eagerly lower the program's own functions to IR (cheap, no LLVM)
                // so they run as IR from the first call.  Native (LLVM) compilation
                // is NOT done here: it happens on demand at runtime when a function
                // is first called (--exec-mode=jit) or once it gets hot (tiered),
                // via the execution-count promotion path.  Compiling everything up
                // front floods the background compile queue and stalls program
                // teardown, so it is intentionally avoided (see eagerlyCompileForExecMode).
                qore_root_ns_private* root_ns_priv = qore_root_ns_private::get(*RootNS);
                eagerlyCompileAllFunctions(root_ns_priv, exec_mode);
            }

            rc = 0;
        }
    } else {
        parsing_in_progress = false;
        assert(!*parseSink);

        // free temporary data structures
        {
            std::lock_guard<std::recursive_mutex> lock(runtime_metadata_lock);
            std::lock_guard<std::mutex> rl(threadContainerLock());
            str_set.clear();
            loc_set.clear();
        }
        rc = 0;
    }

    return rc;
}

void qore_program_private::addStatement(AbstractStatement* s, const QoreNativeParseInfo& info) {
    if (expression_mode) {
        if (new_expression) {
            parse_error(*s->loc, "invalid expression; only a single expression can be parsed");
            delete s;
            return;
        }
        ReturnStatement* exp = dynamic_cast<ReturnStatement*>(s);
        if (!exp) {
            parse_error(*s->loc, "invalid expression; check expression syntax");
            delete s;
            return;
        }

        std::unique_ptr<StatementBlock> block(new StatementBlock(0, 0));
        block->addStatement(exp);
        block->native_parse_info = info;

        // initialize expression
        QoreParseContext parse_context;
        {
            QoreElementScopeParseHelper element_scope_helper;
            block->parseInit(parse_context);
        }
        // an expression is never committed: it runs the checks that a commit runs for the classes it instantiates,
        // so "new C()" of a class with abstract methods is refused as in any other code, and no check is left for
        // the Program's next commit
        qore_root_ns_private::get(*RootNS)->parseCheckDeferredAbstractNew();

        new_expression = block.get();
        {
            // other threads delete and run the expressions of the Program while this thread parses one (qore#6493)
            std::lock_guard<std::mutex> el(exp_set_lock);
            assert(exp_set.find(new_expression) == exp_set.end());
            // fork() waits for the change (see recordLock())
            std::lock_guard<std::mutex> rl(threadContainerLock());
            exp_set.insert(block.release());
        }
        return;
    }

    // see if top level statements are allowed
    if (pwo.parse_options & PO_NO_TOP_LEVEL_STATEMENTS && !s->isDeclaration()) {
        parse_error(*s->loc, "illegal top-level statement (conflicts with parse option NO_TOP_LEVEL_STATEMENTS)");
        delete s;
    } else {
        sb.addStatement(s);
        sb.native_parse_info.merge(info);
        recordNativeParseInfo(info);
    }
}

void qore_program_private::runtimeImportSystemClassesIntern(const qore_program_private& spgm, ExceptionSink* xsink) {
    assert(&spgm != pgm->priv);
    if (!(pwo.parse_options & PO_NO_INHERIT_SYSTEM_CLASSES)) {
        xsink->raiseException("IMPORT-SYSTEM-CLASSES-ERROR", "cannot import system classes in a Program container "
            "where system classes have already been imported");
        return;
    }
    pwo.parse_options &= ~PO_NO_INHERIT_SYSTEM_CLASSES;
    qore_root_ns_private::runtimeImportSystemClasses(*RootNS, *spgm.RootNS, xsink);
}

void qore_program_private::runtimeImportSystemHashDeclsIntern(const qore_program_private& spgm, ExceptionSink* xsink) {
    assert(&spgm != pgm->priv);
    if (!(pwo.parse_options & PO_NO_INHERIT_SYSTEM_HASHDECLS)) {
        xsink->raiseException("IMPORT-SYSTEM-CLASSES-ERROR", "cannot import system classes in a Program container "
            "where system classes have already been imported");
        return;
    }
    pwo.parse_options &= ~PO_NO_INHERIT_SYSTEM_HASHDECLS;
    qore_root_ns_private::runtimeImportSystemHashDecls(*RootNS, *spgm.RootNS, xsink);
}

void qore_program_private::runtimeImportSystemConstantsIntern(const qore_program_private& spgm, ExceptionSink* xsink) {
    assert(&spgm != pgm->priv);
    if (!(pwo.parse_options & PO_NO_INHERIT_SYSTEM_CONSTANTS)) {
        xsink->raiseException("IMPORT-SYSTEM-CONSTANTS-ERROR", "cannot import system constants in a Program "
            "container where system constants have already been imported");
        return;
    }
    pwo.parse_options &= ~PO_NO_INHERIT_SYSTEM_CONSTANTS;
    qore_root_ns_private::runtimeImportSystemConstants(*RootNS, *spgm.RootNS, xsink);
}

void qore_program_private::runtimeImportSystemFunctionsIntern(const qore_program_private& spgm, ExceptionSink* xsink) {
    assert(&spgm != pgm->priv);
    if (!(pwo.parse_options & PO_NO_INHERIT_SYSTEM_FUNC_VARIANTS)) {
        xsink->raiseException("IMPORT-SYSTEM-API-ERROR", "cannot import system functions in a Program container "
            "where system functions have already been imported");
        return;
    }
    if (po_locked) {
        xsink->raiseException("IMPORT-SYSTEM-API-ERROR", "parse options have been locked on this program object");
        return;
    }
    pwo.parse_options &= ~PO_NO_INHERIT_SYSTEM_FUNC_VARIANTS;
    qore_root_ns_private::runtimeImportSystemFunctions(*RootNS, *spgm.RootNS, xsink);
}

// Every runtime import of the system API below writes this Program's committed namespace and reads the calling
// Program's.  The target's parse lock excludes other writers to it; RuntimeNamespaceMergeLocker excludes runtime
// readers of the target - which could otherwise resolve a name against a half-copied namespace or a half-rebuilt
// index - and runtime writers of the source, such as a module being loaded into the calling Program by another
// thread, whose containers the copy iterates.
void qore_program_private::runtimeImportSystemClasses(ExceptionSink* xsink) {
    // must acquire current program before setting program context below
    const QoreProgram* spgm = getProgram();
    ProgramRuntimeParseContextHelper pch(xsink, pgm);
    if (*xsink) {
        return;
    }
    RuntimeNamespaceMergeLocker rnml(*RootNS, *spgm->priv->RootNS);

    runtimeImportSystemClassesIntern(*spgm->priv, xsink);
    // issue #3461: must rebuild all indexes here or symbols will appear missing
    qore_root_ns_private::get(*RootNS)->rebuildAllIndexes();
}

void qore_program_private::runtimeImportSystemHashDecls(ExceptionSink* xsink) {
    // must acquire current program before setting program context below
    const QoreProgram* spgm = getProgram();
    ProgramRuntimeParseContextHelper pch(xsink, pgm);
    if (*xsink) {
        return;
    }
    RuntimeNamespaceMergeLocker rnml(*RootNS, *spgm->priv->RootNS);

    runtimeImportSystemHashDeclsIntern(*spgm->priv, xsink);
    // Resolve cross-namespace parent hashdecl pointers after import
    qore_root_ns_private* rpriv = qore_root_ns_private::get(*RootNS);
    rpriv->resolveExternalParentHashDeclsRecursive(rpriv);
    // issue #3461: must rebuild all indexes here or symbols will appear missing
    rpriv->rebuildAllIndexes();
}

void qore_program_private::runtimeImportSystemConstants(ExceptionSink* xsink) {
    // must acquire current program before setting program context below
    const QoreProgram* spgm = getProgram();
    ProgramRuntimeParseContextHelper pch(xsink, pgm);
    if (*xsink) {
        return;
    }
    RuntimeNamespaceMergeLocker rnml(*RootNS, *spgm->priv->RootNS);

    runtimeImportSystemConstantsIntern(*spgm->priv, xsink);
    // issue #3461: must rebuild all indexes here or symbols will appear missing
    qore_root_ns_private::get(*RootNS)->rebuildAllIndexes();
}

void qore_program_private::runtimeImportSystemFunctions(ExceptionSink* xsink) {
    // must acquire current program before setting program context below
    const QoreProgram* spgm = getProgram();
    ProgramRuntimeParseContextHelper pch(xsink, pgm);
    if (*xsink) {
        return;
    }
    RuntimeNamespaceMergeLocker rnml(*RootNS, *spgm->priv->RootNS);

    runtimeImportSystemFunctionsIntern(*spgm->priv, xsink);
    // issue #3461: must rebuild all indexes here or symbols will appear missing
    qore_root_ns_private::get(*RootNS)->rebuildAllIndexes();
}

void qore_program_private::runtimeImportSystemApi(ExceptionSink* xsink) {
    // must acquire current program before setting program context below
    const QoreProgram* spgm = getProgram();
    ProgramRuntimeParseContextHelper pch(xsink, pgm);
    if (*xsink) {
        return;
    }
    RuntimeNamespaceMergeLocker rnml(*RootNS, *spgm->priv->RootNS);

    runtimeImportSystemClassesIntern(*spgm->priv, xsink);
    if (*xsink) {
        return;
    }
    runtimeImportSystemFunctionsIntern(*spgm->priv, xsink);
    if (*xsink) {
        return;
    }
    runtimeImportSystemConstantsIntern(*spgm->priv, xsink);
    if (*xsink) {
        return;
    }
    runtimeImportSystemHashDeclsIntern(*spgm->priv, xsink);
    if (*xsink) {
        return;
    }
    // merge builtin module / feature list; both lists are shared with other threads, so the source's is copied
    // under its own lock and then added under this Program's
    for (const auto& i : spgm->priv->copyFeatureList()) {
        addFeature(i.c_str());
    }
    // issue #3461: must rebuild all indexes here or symbols will appear missing
    qore_root_ns_private::get(*RootNS)->rebuildAllIndexes();
}

void qore_program_private::importClass(ExceptionSink* xsink, qore_program_private& from_pgm, const char* path,
        const char* new_name, bool inject, q_setpub_t set_pub, bool reexport) {
    if (&from_pgm == this) {
        xsink->raiseException("CLASS-IMPORT-ERROR", "cannot import class \"%s\" with the same source and target " \
            "Program objects", path);
        return;
    }

    if (inject && !(pwo.parse_options & PO_ALLOW_INJECTION)) {
        xsink->raiseException("CLASS-IMPORT-ERROR", "cannot import class \"%s\" with the injection flag set in a " \
            "Program object without PO_ALLOW_INJECTION set", path);
        return;
    }

    const qore_class_private* injectedClass = nullptr;
    const qore_ns_private* vns = nullptr;
    const QoreClass* c;
    {
        // acquire safe access to parse structures in the source program
        ProgramRuntimeParseAccessHelper rah(xsink, from_pgm.pgm);
        if (*xsink)
            return;
        c = qore_root_ns_private::runtimeFindClass(*from_pgm.RootNS, path, vns);

        // resolve the injected target class for the imported copy (set via makeImportClass)
        if (inject && c) {
            const qore_ns_private* tcns = nullptr;
            const QoreClass* oc = qore_root_ns_private::runtimeFindClass(*from_pgm.RootNS,
                new_name ? new_name : path, tcns);
            if (oc) {
                injectedClass = qore_class_private::get(*oc);
            }
            //printd(5, "qore_program_private::importClass() this: %p path: '%s' new_name: '%s' oc: %p\n", this, path,
            //  new_name ? new_name : "n/a", oc);
        }
    }

    if (!c) {
        xsink->raiseException("CLASS-IMPORT-ERROR", "can't find class \"%s\" in the source Program", path);
        return;
    }

    QoreProgram* src_pgm = c->getProgram();
    if (!src_pgm) {
        src_pgm = from_pgm.pgm;
    }

    // get exclusive access to program object for parsing
    ProgramRuntimeParseContextHelper pch(xsink, pgm);
    if (*xsink)
        return;
    // parse ownership excludes other writers; this excludes threads resolving names in this Program
    RuntimeNamespaceWriteLocker rnwl(*RootNS);

    // find/create target namespace based on source namespace
    QoreNamespace* tns;
    // When the imported symbol will be re-exported, mark each created namespace
    // along the path public — otherwise scanMergeCommittedNamespace skips
    // private namespaces during %requires merging, hiding all symbols inside.
    if (new_name && strstr(new_name, "::")) {
        NamedScope nscope(new_name);

        tns = qore_root_ns_private::runtimeFindCreateNamespacePath(*RootNS, nscope,
            qore_class_private::isPublic(*c) || reexport, !c->isSystem());
        qore_root_ns_private::runtimeImportClass(*RootNS, xsink, *tns, c, src_pgm, set_pub, nscope.getIdentifier(),
            inject, injectedClass, reexport);
    } else {
        tns = vns->root ? RootNS : qore_root_ns_private::runtimeFindCreateNamespacePath(*RootNS, *vns,
            !c->isSystem(), reexport);
        printd(5, "qore_program_private::importClass() this: %p pgm: %p path: '%s' tns: %p '%s' RootNS: %p '%s' " \
            "(fp: %p p: %p)\n", this, pgm, path, tns, tns->getName(), RootNS, RootNS->getName(), from_pgm.pgm,
            src_pgm);
        qore_root_ns_private::runtimeImportClass(*RootNS, xsink, *tns, c, src_pgm, set_pub, new_name, inject,
            injectedClass, reexport);
    }
}

void qore_program_private::importHashDecl(ExceptionSink* xsink, qore_program_private& from_pgm, const char* path,
        const char* new_name, q_setpub_t set_pub, bool reexport) {
    if (&from_pgm == this) {
        xsink->raiseException("HASHDECL-IMPORT-ERROR", "cannot import hashdecl \"%s\" with the same source and " \
            "target Program objects", path);
        return;
    }

    const qore_ns_private* vns = nullptr;
    const TypedHashDecl* hd;
    {
        // acquire safe access to parse structures in the source program
        ProgramRuntimeParseAccessHelper rah(xsink, from_pgm.pgm);
        if (*xsink) {
            return;
        }
        hd = qore_root_ns_private::runtimeFindHashDecl(*from_pgm.RootNS, path, vns);
    }

    if (!hd) {
        xsink->raiseException("HASHDECL-IMPORT-ERROR", "can't find hashdecl \"%s\" in source Program", path);
        return;
    }

    // get exclusive access to program object for parsing
    ProgramRuntimeParseContextHelper pch(xsink, pgm);
    if (*xsink)
        return;
    // parse ownership excludes other writers; this excludes threads resolving names in this Program
    RuntimeNamespaceWriteLocker rnwl(*RootNS);

    // find/create target namespace based on source namespace
    QoreNamespace* tns;
    // When the imported symbol will be re-exported, mark each created namespace
    // along the path public — otherwise scanMergeCommittedNamespace skips
    // private namespaces during %requires merging, hiding all symbols inside.
    if (new_name && strstr(new_name, "::")) {
        NamedScope nscope(new_name);

        tns = qore_root_ns_private::runtimeFindCreateNamespacePath(*RootNS, nscope,
            typed_hash_decl_private::get(*hd)->isPublic() || reexport, !hd->isSystem());
        qore_root_ns_private::runtimeImportHashDecl(*RootNS, xsink, *tns, hd, from_pgm.pgm, set_pub,
            nscope.getIdentifier(), reexport);
    } else {
        tns = vns->root ? RootNS : qore_root_ns_private::runtimeFindCreateNamespacePath(*RootNS, *vns,
            !hd->isSystem(), reexport);
        //printd(5, "qore_program_private::importHashDecl() this: %p path: %s nspath: %s tns: %p %s RootNS: %p %s\n",
        //  this, path, nspath.c_str(), tns, tns->getName(), RootNS, RootNS->getName());
        qore_root_ns_private::runtimeImportHashDecl(*RootNS, xsink, *tns, hd, from_pgm.pgm, set_pub, new_name,
            reexport);
    }

    // Resolve cross-namespace parent hashdecl pointer after import
    // The imported hashdecl may have a parent in a different namespace
    if (!*xsink) {
        qore_root_ns_private* rpriv = qore_root_ns_private::get(*RootNS);
        qore_ns_private::get(*tns)->hashDeclList.resolveExternalParentHashDecls(rpriv);
    }
}

void qore_program_private::inheritParseImports(QoreProgram& child, QoreProgram& parent, ExceptionSink* xsink) {
    RootQoreNamespace* parent_RootNS = parent.priv->RootNS;
    RootQoreNamespace* child_RootNS = child.priv->RootNS;
    qore_root_ns_private* parent_root = qore_root_ns_private::get(*parent_RootNS);
    // the parent's root indexes are iterated below while a module can be loaded into the parent in another thread
    RuntimeNamespaceMergeLocker rnml(*child_RootNS, *parent_RootNS);

    // Hashdecls: walk parent's root index, copy entries marked re-export into
    // child. Each entry was tagged via Program::importHashDecl(... reexport=True)
    // (or inherited from such) so the host explicitly opted to expose it.
    // System entries are skipped: the static system namespace already provides
    // them in the child.  Conflicts are swallowed silently — a duplicate name
    // in the child means the child's own %requires chain or static namespace
    // already has the symbol indexed, which is what we want.
    for (const auto& i : parent_root->thdmap) {
        const TypedHashDecl* hd = i.second.obj;
        if (!hd) {
            continue;
        }
        const typed_hash_decl_private* hdp = typed_hash_decl_private::get(*hd);
        if (!hdp->isReexport() || hdp->isSystem()) {
            continue;
        }
        QoreNamespace* tns = qore_root_ns_private::runtimeFindCreateNamespacePath(
            *child_RootNS, *i.second.ns, true);
        ExceptionSink xs;
        qore_root_ns_private::runtimeImportHashDecl(*child_RootNS, &xs, *tns, hd, &parent,
            CSP_UNCHANGED, nullptr, true);
        xs.clear();
    }

    // Classes: same shape as hashdecls.
    for (const auto& i : parent_root->clmap) {
        const QoreClass* c = i.second.obj;
        if (!c) {
            continue;
        }
        if (!qore_class_private::isReexport(*c) || c->isSystem()) {
            continue;
        }
        QoreNamespace* tns = qore_root_ns_private::runtimeFindCreateNamespacePath(
            *child_RootNS, *i.second.ns, true);
        ExceptionSink xs;
        qore_root_ns_private::runtimeImportClass(*child_RootNS, &xs, *tns, c, &parent,
            CSP_UNCHANGED, nullptr, false, nullptr, true);
        xs.clear();
    }

    (void)xsink;
}

namespace {
//! Appends the directories of \a source that \a target does not have to \a target
void qore_append_unique_module_paths(std::vector<std::string>& target, const std::vector<std::string>& source) {
    for (const std::string& path : source) {
        if (std::find(target.begin(), target.end(), path) == target.end()) {
            target.push_back(path);
        }
    }
}

//! An AOT module's own module path directories, added for a Program (see QoreModulePathScope)
struct QoreModulePathScopeEntry {
    const QoreProgram* pgm;
    const std::vector<std::string>* prepended;
    const std::vector<std::string>* appended;
};

//! The scopes of this thread, outermost first
thread_local std::vector<QoreModulePathScopeEntry> qore_module_path_scopes;
}

QoreModulePathScope::QoreModulePathScope(const QoreProgram* pgm, const std::vector<std::string>& prepended,
        const std::vector<std::string>& appended) {
    qore_module_path_scopes.push_back({pgm, &prepended, &appended});
}

QoreModulePathScope::~QoreModulePathScope() {
    assert(!qore_module_path_scopes.empty());
    qore_module_path_scopes.pop_back();
}

void qore_program_private::getEffectiveModulePathLists(const QoreProgram* pgm, std::vector<std::string>& prepended,
        std::vector<std::string>& appended) {
    prepended.clear();
    appended.clear();
    // a nested module's own directories come before those of the module it is loaded for, as a source module's
    // Program puts its own before those it inherits
    for (auto i = qore_module_path_scopes.rbegin(), e = qore_module_path_scopes.rend(); i != e; ++i) {
        if (i->pgm == pgm) {
            qore_append_unique_module_paths(prepended, *i->prepended);
        }
    }
    if (pgm) {
        qore_append_unique_module_paths(prepended, pgm->priv->prepended_module_paths);
        qore_append_unique_module_paths(appended, pgm->priv->appended_module_paths);
    }
    for (const QoreModulePathScopeEntry& entry : qore_module_path_scopes) {
        if (entry.pgm == pgm) {
            qore_append_unique_module_paths(appended, *entry.appended);
        }
    }
}

void qore_program_private::inheritModulePathLists(QoreProgram& child, QoreProgram& parent) {
    // the lists that resolve modules for the parent in this thread, so that a source module loaded as a dependency
    // of an AOT module finds its own dependencies with the AOT module's directories too
    std::vector<std::string> prepended, appended;
    getEffectiveModulePathLists(&parent, prepended, appended);
    qore_append_unique_module_paths(child.priv->prepended_module_paths, prepended);
    qore_append_unique_module_paths(child.priv->appended_module_paths, appended);
}

void qore_program_private::importFunction(ExceptionSink* xsink, QoreFunction* u, const qore_ns_private& oldns, const char* new_name, bool inject) {
    // get exclusive access to program object for parsing
    ProgramRuntimeParseContextHelper pch(xsink, pgm);
    if (*xsink)
        return;
    // parse ownership excludes other writers; this excludes threads resolving names in this Program
    RuntimeNamespaceWriteLocker rnwl(*RootNS);

    if (new_name && strstr(new_name, "::")) {
        NamedScope nscope(new_name);
        QoreNamespace* tns = qore_root_ns_private::runtimeFindCreateNamespacePath(*RootNS, nscope, u->hasPublic(), u->hasUser());
        qore_root_ns_private::runtimeImportFunction(*RootNS, xsink, *tns, u, nscope.getIdentifier());
        return;
    }

    // find/create target namespace based on source namespace
    QoreNamespace* tns = oldns.root ? RootNS : qore_root_ns_private::runtimeFindCreateNamespacePath(*RootNS, oldns, u->hasUser());
    //printd(5, "qore_program_private::importFunction() this: %p tns: %p '%s' oldns: '%s' RootNS: %p %s\n", this, tns, tns->getName(), oldns.name.c_str(), RootNS, RootNS->getName());
    assert(oldns.name == tns->getName());
    qore_root_ns_private::runtimeImportFunction(*RootNS, xsink, *tns, u, new_name, inject);
}

void qore_program_private::exportGlobalVariable(ExceptionSink* xsink, const char* vname, bool readonly,
            qore_program_private& tpgm, const char* import_as) {
    const char* target_name = import_as ? import_as : vname;
    if (&tpgm == this) {
        xsink->raiseException("PROGRAM-IMPORTGLOBALVARIABLE-EXCEPTION", "Cannot import global variable \"%s\" with "
            "the same source and target Program objects", target_name);
        return;
    }

    const qore_ns_private* vns = nullptr;
    Var* v;
    {
        ProgramRuntimeParseAccessHelper pah(xsink, pgm);
        if (*xsink) {
            return;
        }
        v = qore_root_ns_private::runtimeFindGlobalVar(*RootNS, vname, vns);
    }

    if (!v) {
        xsink->raiseException("PROGRAM-IMPORTGLOBALVARIABLE-EXCEPTION", "There is no global variable \"%s\"", vname);
        return;
    }

    // get exclusive access to program object for parsing
    ProgramRuntimeParseContextHelper pch(xsink, tpgm.pgm);
    if (*xsink) {
        return;
    }
    // parse ownership excludes other writers; this excludes threads resolving names in the target Program
    RuntimeNamespaceWriteLocker rnwl(*tpgm.RootNS);

    // find/create target namespace based on source namespace
    QoreString tmp;
    QoreNamespace* tns;
    if (import_as) {
        QoreString nspath(import_as);
        ssize_t p = nspath.rfind("::");
        if (p != -1) {
            tmp = nspath.c_str() + p;
            import_as = tmp.c_str();
            nspath.terminate(p);
            NamedScope ns(nspath.c_str());
            // import into the specified namespace
            tns = qore_root_ns_private::get(*tpgm.RootNS)->runtimeFindCreateNamespacePath(ns, false, !v->isBuiltin());
        } else {
            // import into the equivalent namespace as the source variable
            tns = vns->root ? tpgm.RootNS : qore_root_ns_private::runtimeFindCreateNamespacePath(*tpgm.RootNS,
                *vns, !v->isBuiltin());
        }
    } else {
        // import into the equivalent namespace as the source variable
        tns = vns->root ? tpgm.RootNS : qore_root_ns_private::runtimeFindCreateNamespacePath(*tpgm.RootNS,
            *vns, !v->isBuiltin());
    }
    //printd(5, "qore_program_private::exportGlobalVariable() this: %p vname: '%s' ro: %d vns: %p '%s::' RootNS: %p "
    //    "'%s::'\n", this, vname, readonly, vns, vns->name.c_str(), RootNS, RootNS->getName());
    qore_root_ns_private::get(*tpgm.RootNS)->runtimeImportGlobalVariable(xsink, *qore_ns_private::get(*tns), v,
        readonly, import_as);
}

qore_program_private_base::OpaqueTargetShard qore_program_private_base::opaque_shards[1u << OpaqueTargetShardBits];

namespace {
//! the fork handler of the opaque target registry's shard locks, which are leaf locks never held together (see
//! QoreForkHandlers.h)
class OpaqueTargetShardsForkHandler : public QoreForkHandler {
public:
    OpaqueTargetShardsForkHandler() : QoreForkHandler(QFLR_LEAF, "opaque_target_shards") {
    }

    void prepare() noexcept override {
        if (QFLM_STRIPED == QFLM_ACQUIRE) {
            for (auto& shard : qore_program_private_base::opaque_shards) {
                shard.lock.lock();
            }
        }
    }

    void parent() noexcept override {
        if (QFLM_STRIPED == QFLM_ACQUIRE) {
            constexpr unsigned count = 1u << qore_program_private_base::OpaqueTargetShardBits;
            for (unsigned i = count; i; --i) {
                qore_program_private_base::opaque_shards[i - 1].lock.unlock();
            }
        }
    }

    void child() noexcept override {
        for (auto& shard : qore_program_private_base::opaque_shards) {
            shard.lock.reinitAfterFork();
        }
    }
};

OpaqueTargetShardsForkHandler opaque_target_shards_fork_handler;

#ifdef DEBUG
//! false once the registry's shards are about to be destroyed at process exit; constant-initialized
std::atomic<bool> opaque_registry_live{true};

//! clears opaque_registry_live: defined after opaque_shards, it is destroyed before them
struct OpaqueRegistryLiveSentinel {
    ~OpaqueRegistryLiveSentinel() {
        opaque_registry_live.store(false, std::memory_order_relaxed);
    }
};
OpaqueRegistryLiveSentinel opaque_registry_live_sentinel;
#endif
}

#ifdef DEBUG
std::atomic<size_t> qore_program_private_base::opaque_target_count{0};

bool qore_program_private_base::isOpaqueTarget(const AbstractQoreNode* n) {
    // a target is counted when it is inserted and uncounted when it is erased, under its shard's lock and with
    // sequentially consistent operations, so a node still registered when it is freed is counted then
    if (!opaque_target_count.load()) {
        return false;
    }
    OpaqueTargetShard& shard = getOpaqueTargetShard(n);
    AutoLocker al(shard.lock);
    return shard.targets.find(const_cast<AbstractQoreNode*>(n)) != shard.targets.end();
}

void qore_debug_check_freed_node_not_opaque_target(const AbstractQoreNode* n) {
    if (!opaque_registry_live.load(std::memory_order_relaxed)) {
        return;
    }
    if (qore_program_private_base::isOpaqueTarget(n)) {
        fprintf(stderr, "OPAQUE TARGET FREED WHILE REGISTERED: node %p (type %d): an opaque value's reference was "
            "released without its registration\n", n, n->getType());
        assert(false);
    }
}
#endif

QoreProgram* qore_program_private_base::getOpaqueTargetProgram(const AbstractQoreNode* n) {
    assert(n);
    OpaqueTargetShard& shard = getOpaqueTargetShard(n);
    AutoLocker al(shard.lock);
    auto i = shard.targets.find(const_cast<AbstractQoreNode*>(n));
    return i == shard.targets.end() ? nullptr : i->second.pgm;
}

void qore_program_private_base::setOpaqueTargetProgram(AbstractQoreNode* n, QoreProgram* pgm) {
    assert(n);
    assert(pgm);
    OpaqueTargetShard& shard = getOpaqueTargetShard(n);
    AutoLocker al(shard.lock);
    auto i = shard.targets.find(n);
    if (i == shard.targets.end()) {
        shard.targets.insert(opaque_target_map_t::value_type(n, OpaqueTargetInfo{pgm, 1}));
#ifdef DEBUG
        opaque_target_count.fetch_add(1);
#endif
        return;
    }
    assert(i->second.count == 1);
    i->second.pgm = pgm;
}

void qore_program_private_base::clearOpaqueTargets(ExceptionSink* xsink) {
    // take this Program's entries out of the registry under the lock, then do all the work outside
    // it: deleting an object runs its destructor, which is user code
    std::vector<AbstractQoreNode*> roots;
    for (OpaqueTargetShard& shard : opaque_shards) {
        // one shard lock at a time: each is a leaf lock
        AutoLocker al(shard.lock);
        for (auto i = shard.targets.begin(), e = shard.targets.end(); i != e;) {
            if (i->second.pgm == pgm) {
                roots.push_back(i->first);
                i = shard.targets.erase(i);
#ifdef DEBUG
                opaque_target_count.fetch_sub(1);
#endif
            } else {
                ++i;
            }
        }
    }
    if (roots.empty()) {
        return;
    }

    // Collect every object owned by this Program that is reachable from a registered target through
    // containers.  Deleting such an object releases its members, which breaks the cycle running
    // through the opaque edge.  Only this Program's objects are collected, and containers are never
    // modified: a container reached here may be shared with the Program that created it, and
    // touching that Program's data would corrupt it.
    //
    // The walk is iterative rather than recursive: container nesting is unbounded and this runs
    // during teardown, where a stack overflow could not be recovered from.  There is deliberately
    // no qore_check_cancel() here either — teardown must run to completion, or the cycles it exists
    // to break would be leaked instead.
    //
    // The registry holds no reference of its own; a registered target is alive here because the
    // opaque values referring to it are still in place.
    SafeDerefHelper sdh(xsink);
    std::set<AbstractQoreNode*> seen;
    std::vector<AbstractQoreNode*> pending(roots.begin(), roots.end());
    std::vector<QoreObject*> targets;
    while (!pending.empty()) {
        AbstractQoreNode* n = pending.back();
        pending.pop_back();
        if (!n || !seen.insert(n).second) {
            continue;
        }
        switch (n->getType()) {
            case NT_OBJECT: {
                QoreObject* o = static_cast<QoreObject*>(n);
                if (o->getProgram() == pgm && o->isValid()) {
                    // keep the object alive until it has been deleted below; sdh releases the
                    // reference even if anything here throws
                    o->ref();
                    sdh.deref(QoreValue(o));
                    targets.push_back(o);
                }
                // members are not walked: deleting the object releases them
                break;
            }
            case NT_HASH: {
                ConstHashIterator hi(static_cast<const QoreHashNode*>(n));
                while (hi.next()) {
                    QoreValue v = hi.get();
                    if (v.hasNode()) {
                        pending.push_back(v.getInternalNode());
                    }
                }
                break;
            }
            case NT_LIST: {
                ConstListIterator li(static_cast<const QoreListNode*>(n));
                while (li.next()) {
                    QoreValue v = li.getValue();
                    if (v.hasNode()) {
                        pending.push_back(v.getInternalNode());
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    for (auto* o : targets) {
        o->doDelete(xsink);
    }
}

void qore_program_private::del(ExceptionSink* xsink) {
    printd(5, "qore_program_private::del() pgm: %p (base_object: %d)\n", pgm, base_object);

    // Idempotent guard: del() is called only from waitForTerminationAndClear(), which can run twice: once from
    // ModuleManager at shutdown to break cross-program Type strong refs (issue #4816), and again when the reference
    // count reaches 0.  RootNS is set to nullptr at the end of del() so this flag detects the second call and
    // short-circuits.
    if (!RootNS) {
        return;
    }
    // waitForTerminationAndClear() waited for the batch JIT compiles that read the Program's code, after it had set
    // ptid, which refuses new ones (qore#5874)
    assert(!compile_holds);

    // dereference all external data, once no child Program copies it
    {
        AutoLocker al(plock);
        waitForExternalDataCopiesIntern();
    }
    // each data is released, also when the release of another one raises an exception (issue #6786); a module's
    // callback is always given a sink
    if (!extmap.empty()) {
        ExceptionSink local_xsink;
        ExceptionSink* deref_xsink = xsink ? xsink : &local_xsink;
        for (auto& i : extmap) {
            qore_ext_data_deref(i.second, deref_xsink);
        }
        // fork() waits for the change (see recordLock())
        std::lock_guard<std::mutex> rl(threadContainerLock());
        extmap.clear();
    }

    // the Program object released the Program after it had stopped reporting its code (clearScanHolder()), unless the
    // data of the Program is deleted while the object still holds it (a Program cleared at the end of the process by
    // qore_cleanup(), see clearRemainingPrograms(), while its owner holds the object): the object reports only the
    // thread initialization code and the thread entry handlers, which are released below, so it stops reporting first;
    // its later release of the Program finds nothing to clear
    clearScanHolder();
    assert(!scan_holder);
    ResolvedCallReferenceNode* old_thr_init;
    // the options of the code that set it, released with it after the lock (qore#6208)
    QoreThreadRuntimeParseOptions old_thr_init_po;
    {
        // the object that survives can still set or get the code (Program::setThreadInit())
        AutoLocker al(tlock);
        old_thr_init = thr_init;
        thr_init = nullptr;
        std::swap(old_thr_init_po.po, thr_init_po.po);
        std::swap(old_thr_init_po.override, thr_init_po.override);
        std::swap(old_thr_init_po.caller_domain_restrictions, thr_init_po.caller_domain_restrictions);
    }
    if (old_thr_init) {
        old_thr_init->deref(xsink);
    }

    // a Program that never ran waitForTerminationAndClear() can still have handlers
    setThreadEntryHandler(nullptr, nullptr, xsink);

    // and lifecycle subscriptions
    thread_hub->close(xsink);

    base_object = false;

    // delete defines
    for (auto& i : dmap) {
        i.second.discard(xsink);
    }
    dmap.clear();

    assert(RootNS);
    // have to delete global variables first because of destructors.
    clearNamespaceData(xsink);

    // clear constants if not already cleared
    if (!ns_const) {
        ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), xsink);
        qore_root_ns_private::clearConstants(*RootNS, **l);
        ns_const = true;
        // see waitForTerminationAndClear() (#6791)
        collectOwnedConstantObjects(**l);
        deleteOwnedConstantObjects(xsink);
        //printd(5, "qore_program_private::del() this: %p cleared constants\n", this);
    }

    // drain background JIT compilations that reference THIS program before
    // deleting namespace data: the bg thread holds raw pointers to QoreIRFunction
    // objects (and the program-owned LocalVars/AST they reference) owned by this
    // program.  Only this program's compiles need draining — other programs'
    // queued/in-progress compiles are left running, so teardown is not serialized
    // behind the entire global compile queue.
    QoreJIT::instance().waitForBgCompileQueue(pgm);

    {
        // the declarations of the namespace that survive it (referenced by values or by other Programs) keep the
        // declarations whose types they name, and the others are destroyed together (issue #5790)
        QoreTypeOwnerBatch type_owner_batch(*qore_root_ns_private::get(*RootNS));

        // delete the namespace and all data
        qore_root_ns_private::get(*RootNS)->deleteData(xsink);

        // no more code of this Program runs: free the thread-local data of the family for the last time if this
        // Program is its root, including the data that the destructors run above have saved; the members of the
        // family that are still alive become roots of families of their own
        freeFamilyThreadData(true, xsink);

        delete RootNS;
        RootNS = nullptr;
    }

    // delete all root code
    // method call can be repeated
    sb.del();
    // and the code compiled from it, which refers to the closures that it creates (qore#5822)
    sb.delCompiledCode();

    // clear expressions; they are deleted with the lock released, as their deletion can run code (qore#6493)
    {
        q_exp_set_t exps;
        {
            std::lock_guard<std::mutex> el(exp_set_lock);
            std::lock_guard<std::mutex> rl(threadContainerLock());
            exps.swap(exp_set);
        }
        for (auto& i : exps) {
            delete i;
        }
    }

    // no code of this Program runs any more and its background compiles are drained: the native code that only it
    // owns is removed from the JIT (qore#5772)
    QoreJIT::instance().releaseCodeOwner(pgm);

    //printd(5, "QoreProgram::~QoreProgram() this: %p deleting root ns %p\n", this, RootNS);

    for (name_section_sline_statement_map_t::iterator it = statementByFileIndex.begin(); it != statementByFileIndex.end(); it++) {
        it->second->sectionMap.clear();
        it->second->statementMap.clear();
        delete it->second;
    }
    statementByFileIndex.clear();
    for (name_section_sline_statement_map_t::iterator it = statementByLabelIndex.begin(); it != statementByLabelIndex.end(); it++) {
        it->second->sectionMap.clear();
        it->second->statementMap.clear();
        delete it->second;
    }
    statementByLabelIndex.clear();

    // release the dependency reference on the root of the family last, as it can delete the root
    leaveThreadDataFamily();
}

template <bool Native>
const QoreClass* qore_program_private::runtimeFindClassImpl(const char* class_name, ExceptionSink* xsink) const {
    // acquire safe access to parse structures in the source program
    ProgramRuntimeParseAccessHelper rah(xsink, pgm);
    if (*xsink)
        return nullptr;

    // include the ns arg to ensure that namespace-justified classes are resolved properly
    const qore_ns_private* ns;
    if constexpr (Native) {
        return qore_root_ns_private::runtimeFindClass(*RootNS, class_name, ns);
    } else {
        return qore_root_ns_private::runtimeFindClass(*RootNS, class_name, ns, LexicalSemanticPolicy{});
    }
}

const QoreClass* qore_program_private::runtimeFindClass(const char* class_name, ExceptionSink* xsink) const {
    return runtimeFindClassImpl<true>(class_name, xsink);
}

const QoreClass* qore_program_private::findClassCanonical(const char* path, ExceptionSink* xsink) const {
    return runtimeFindClassImpl<false>(path, xsink);
}

int qore_program_private::setGlobalVarValue(const char* name, QoreValue val, ExceptionSink* xsink) {
   ValueHolder holder(val, xsink);

   const qore_ns_private* vns = 0;
   Var* v;
   {
      ProgramRuntimeParseAccessHelper pah(xsink, pgm);
      if (*xsink)
         return -1;
      v = qore_root_ns_private::runtimeFindGlobalVar(*RootNS, name, vns);
   }

   if (!v) {
      xsink->raiseException("UNKNOWN-VARIABLE", "there is no global variable \"%s\"", name);
      return -1;
   }

   LValueHelper lvh(xsink);
   if (v->getLValue(lvh, false))
      return -1;

   return lvh.assign(holder.release(), "<API assignment>");
}

QoreHashNode* qore_program_private::getTopLevelLocalVars(ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    ThreadLocalProgramData* tlpd = nullptr;
    {
        AutoLocker al(tlock);
        pgm_data_map_t::iterator i = pgm_data_map.find(get_thread_program_data());
        if (i != pgm_data_map.end()) {
            tlpd = i->second;
        }
    }
    const LVList* lvl = sb.getLVList();
    if (!tlpd || !lvl) {
        return rv.release();
    }
    // only the variables that are instantiated in the thread have values; the inherited ones are not variables of the
    // code of this Program
    unsigned n = std::min(static_cast<unsigned>(lvl->size()), tlpd->inst_count);
    for (unsigned i = sb.getInheritedLocalVarCount(); i < n; ++i) {
        const LocalVar* lv = lvl->lv[i];
        ReferenceHolder<QoreHashNode> v(new QoreHashNode(autoTypeInfo), xsink);
        // a variable that a closure uses is found by its name, as the code finds it
        if (lv->closureUse()) {
            ClosureVarValue* cvv = tlpd->cvstack.try_find(lv->getName());
            if (!cvv) {
                continue;
            }
            v->setKeyValue("type", new QoreStringNode("closure"), xsink);
            v->setKeyValue("value", cvv->eval(xsink), xsink);
        } else {
            LocalVarValue* lvv = tlpd->lvstack.findMaybe(lv);
            if (!lvv) {
                continue;
            }
            v->setKeyValue("type", new QoreStringNode("local"), xsink);
            v->setKeyValue("value", lvv->eval(xsink), xsink);
        }
        if (*xsink) {
            return nullptr;
        }
        qore_set_local_var_debug_type(**v, lv->getTypeInfo(), xsink);
        rv->setKeyValue(lv->getName(), v.release(), xsink);
    }
    return rv.release();
}

LocalVar* qore_program_private::createLocalVar(const char* name, const QoreTypeInfo* typeInfo) {
    std::lock_guard<std::recursive_mutex> lock(runtime_metadata_lock);
    // emplace into the deque-backed arena; returns a stable pointer
    // that stays valid for the Program's lifetime.  Avoids the per-
    // LocalVar `new`+list-node allocation of the prior safe_dslist
    // implementation — hot path in AOT deserialization.
    // fork() waits for the change (see recordLock())
    std::lock_guard<std::mutex> rl(threadContainerLock());
    return createLocalVarLocked(name, typeInfo);
}

void qore_program_private::addStatementToIndexIntern(name_section_sline_statement_map_t* statementIndex, const char* key, AbstractStatement *statement, int offs, const char* section, int sectionOffs) {
    // index is being built when parsing
    if ((!statement && !section) || !key)
        return;
    section_sline_statement_map_t *sssm;
    name_section_sline_statement_map_t::iterator it = statementIndex->find(key);
    if (it == statementIndex->end()) {
        printd(5, "qore_program_private::addStatementToIndexIntern(%p,'%s',%d,'%s',%d) key not found, this: %p\n", statementIndex, key, offs, section ? section : (const char*)"(null)",sectionOffs, this);
        std::unique_ptr<section_sline_statement_map_t> new_sssm(new section_sline_statement_map_t());
        // the working directory is the one that a relative file name was given in
        QoreString abs_path(key);
        q_normalize_path(abs_path);
        new_sssm->abs_path = abs_path.c_str();
        statementIndex->insert(name_section_sline_statement_map_t::value_type(key, new_sssm.get()));
        sssm = new_sssm.release();
    } else {
        sssm = it->second;
        if (statement && statement->loc) {
            int line = statement->loc->start_line + offs;
            sline_statement_map_t::iterator li = sssm->statementMap.find(line);
            if (li != sssm->statementMap.end() && li->first == line) {
                // The index stores raw statement pointers owned elsewhere; do not
                // dereference existing entries here. First statement for a line wins.
                printd(5, "qore_program_private::addStatementToIndexIntern(%p,'%s',%d) skipping line (%d-%d), this: %p\n", statementIndex, key, offs, statement->loc->start_line, statement->loc->end_line, this);
                return;
            }
        }
    }
    if (statement && statement->loc) {
        printd(5, "qore_program_private::addStatementToIndexIntern(%p,'%s',%d) insert line %d (%d-%d), this: %p\n", statementIndex, key, offs, statement->loc->start_line+offs, statement->loc->start_line, statement->loc->end_line, this);
        sssm->statementMap.insert(std::pair<int, AbstractStatement*>(statement->loc->start_line+offs, statement));
    }
    if (section) {
        if (sssm->sectionMap.find(section) == sssm->sectionMap.end()) {
            printd(5, "qore_program_private::addStatementToIndexIntern(%p,'%s','%s',%d) insert section, this: %p\n", statementIndex, key, section, sectionOffs, this);
            sssm->sectionMap.insert(std::pair<const char*, int>(section, sectionOffs));
        }
    }
}

AbstractStatement* qore_program_private::findStatementInIndexIntern(
        const name_section_sline_statement_map_t& statementIndex, const char* key, int line) const {
    if (!key) {
        return nullptr;
    }
    name_section_sline_statement_map_t::const_iterator it = statementIndex.find(key);
    if (it == statementIndex.end() || !it->second) {
        return nullptr;
    }
    sline_statement_map_t::const_iterator li = it->second->statementMap.find(line);
    return li == it->second->statementMap.end() ? nullptr : li->second;
}

AbstractStatement* qore_program_private::findIndexedStatementForLocation(const char* file, const char* source,
        int start_line, int offset) const {
    if (start_line <= 0) {
        return nullptr;
    }
    if (source && *source) {
        return findStatementInIndexIntern(statementByFileIndex, source, start_line + offset);
    }
    return findStatementInIndexIntern(statementByFileIndex, file, start_line + offset);
}

StatementBlock* qore_program_private::getAOTClosureDebugBlock(const QoreProgramLocation* loc) {
    AutoLocker al(&plock);
    if (!loc) {
        // the block is kept for the life of the Program: its location is never rolled back with a failed parse
        // (issue #6354)
        loc = getRuntimeLocation(0, 0);
    }
    aot_closure_debug_block_map_t::iterator i = aot_closure_debug_blocks.find(loc);
    if (i != aot_closure_debug_blocks.end()) {
        return i->second;
    }
    std::unique_ptr<StatementBlock> b(new StatementBlock(this, loc));
    {
        // fork() waits for the change (see recordLock())
        std::lock_guard<std::mutex> rl(threadContainerLock());
        i = aot_closure_debug_blocks.insert(aot_closure_debug_block_map_t::value_type(loc, b.get())).first;
    }
    StatementBlock* rv = b.release();
    registerStatement(pgm, rv, false);
    return rv;
}

void qore_program_private::registerStatement(QoreProgram *pgm, AbstractStatement *statement, bool addToIndex) {
    if (pgm && statement) {
        // plock must already be held
        ReverseStatementIdMap_t::iterator i = pgm->priv->reverseStatementIds.find(statement);
        if (i == pgm->priv->reverseStatementIds.end()) {
            // statements are registered at run time as well (getAOTClosureDebugBlock()): fork() waits for the change
            // (see recordLock())
            std::lock_guard<std::mutex> rl(pgm->priv->threadContainerLock());
            // a new block can start at a line of the cache of blocks
            pgm->priv->blockIndexCache.clear();
            pgm->priv->statementIds.push_back(statement);
            pgm->priv->reverseStatementIds.insert(std::pair<AbstractStatement*, unsigned long>(statement, pgm->priv->statementIds.size()));
        }
        if (addToIndex && statement->loc) {
            if (statement->loc->getSource()) {
                printd(5, "qore_program_private::registerStatement(file+source), this: %p, statement: %p\n", pgm->priv, statement);
                pgm->priv->addStatementToIndexIntern(&pgm->priv->statementByFileIndex, statement->loc->getSource(), statement, statement->loc->offset, statement->loc->getFile(), statement->loc->offset);
                pgm->priv->addStatementToIndexIntern(&pgm->priv->statementByLabelIndex, statement->loc->getFile(), statement, 0, statement->loc->getSource(), statement->loc->offset);
            } else {
                printd(5, "qore_program_private::registerStatement(file), this: %p, statement: %p\n", pgm->priv, statement);
                pgm->priv->addStatementToIndexIntern(&pgm->priv->statementByFileIndex, statement->loc->getFile(), statement, statement->loc->offset/*is zero*/, nullptr, -1);
            }
        }
    }
}

QoreHashNode* qore_program_private::getSourceIndicesIntern(name_section_sline_statement_map_t* statementIndex,
        ExceptionSink* xsink) const {
    const QoreTypeInfo* subType = qore_get_complex_hash_type(bigIntTypeInfo);

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(subType), xsink);
    for (auto& it : *statementIndex) {
        QoreHashNode* h2 = new QoreHashNode(bigIntTypeInfo);
        qore_hash_private* ph2 = qore_hash_private::get(*h2);
        for (auto& it2 : it.second->sectionMap) {
            ph2->setKeyValueIntern(it2.first, it2.second);
        }
        rv->setKeyValue(it.first, h2, xsink);
        // no possibility for an exception
        assert(!*xsink);
    }
    return rv.release();
}

void qore_program_private::onAttach(DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
    AutoQoreCounterDec ad(&debug_program_counter, false);
    qore_debug_program_private* p = getDebugProgram(ad);
    printd(5, "qore_program_private::onAttach() this: %p, dp: %p\n", this, p);
    if (p) {
        p->onAttach(pgm, rs, rts, xsink);
    } else {
        rs = DBG_RS_DETACH;
    }
}
void qore_program_private::onDetach(DebugRunStateEnum &rs, const AbstractStatement *&rts, ExceptionSink* xsink) {
    AutoQoreCounterDec ad(&debug_program_counter, false);
    qore_debug_program_private* p = getDebugProgram(ad);
    printd(5, "qore_program_private::onDetach() this: %p, dp: %p\n", this, p);
    if (p) {
        p->onDetach(pgm, rs, rts, xsink);
    } else {
        rs = DBG_RS_DETACH;
    }
}

void qore_program_private::onStep(const StatementBlock *blockStatement, const AbstractStatement *statement, unsigned bkptId, int &flow, DebugRunStateEnum &rs, const AbstractStatement *&rts, ExceptionSink* xsink) {
    AutoQoreCounterDec ad(&debug_program_counter, false);
    qore_debug_program_private* p = getDebugProgram(ad);
    if (p) {
        p->onStep(pgm, blockStatement, statement, bkptId, flow, rs, rts, xsink);
    } else {
        rs = DBG_RS_DETACH;
    }
}

void qore_program_private::onFunctionEnter(const StatementBlock *statement, DebugRunStateEnum &rs, const AbstractStatement *&rts, ExceptionSink* xsink) {
    AutoQoreCounterDec ad(&debug_program_counter, false);
    qore_debug_program_private* p = getDebugProgram(ad);
    if (p) {
        p->onFunctionEnter(pgm, statement, rs, rts, xsink);
    } else {
        rs = DBG_RS_DETACH;
    }
}

void qore_program_private::onFunctionExit(const StatementBlock *statement, QoreValue& returnValue, DebugRunStateEnum &rs, const AbstractStatement *&rts, ExceptionSink* xsink) {
    AutoQoreCounterDec ad(&debug_program_counter, false);
    qore_debug_program_private* p = getDebugProgram(ad);
    if (p) {
        p->onFunctionExit(pgm, statement, returnValue, rs, rts, xsink);
    } else {
        rs = DBG_RS_DETACH;
    }
}

void qore_program_private::onException(const AbstractStatement *statement, DebugRunStateEnum &rs, const AbstractStatement *&rts, ExceptionSink* xsink) {
    AutoQoreCounterDec ad(&debug_program_counter, false);
    qore_debug_program_private* p = getDebugProgram(ad);
    if (p) {
        p->onException(pgm, statement, rs, rts, xsink);
    } else {
        rs = DBG_RS_DETACH;
    }
}

void qore_program_private::onExit(const StatementBlock *statement, QoreValue& returnValue, DebugRunStateEnum &rs, const AbstractStatement *&rts, ExceptionSink* xsink) {
    AutoQoreCounterDec ad(&debug_program_counter, false);
    qore_debug_program_private* p = getDebugProgram(ad);
    if (p) {
        p->onExit(pgm, statement, returnValue, rs, rts, xsink);
    } else {
        rs = DBG_RS_DETACH;
    }
}

const AbstractQoreFunctionVariant* qore_program_private::runtimeFindCall(const char* name, const QoreListNode* params, ExceptionSink* xsink) {
    // acquire safe access to parse structures in the source program
    ProgramRuntimeParseAccessHelper rah(xsink, pgm);

    return qore_root_ns_private::get(*RootNS)->runtimeFindCall(name, params, xsink);
}

QoreListNode* qore_program_private::runtimeFindCallVariants(const char* name, ExceptionSink* xsink) {
    // acquire safe access to parse structures in the source program
    ProgramRuntimeParseAccessHelper rah(xsink, pgm);

    return qore_root_ns_private::get(*RootNS)->runtimeFindCallVariants(name, xsink);
}

int qore_program_private::initDeferredCode() {
    int err = 0;
    while (!dset.empty()) {
        dset_t dset0 = std::move(dset);
        dset.clear();
        for (auto& i : dset0) {
            if (i->parseInitDeferred() && !err) {
                err = -1;
            }
        }
    }
    return err;
}

/*
    When a debug handler is invoked then current stack frame/node is not visible in get_thread_call_stack()
    as a statement is injected (not real function call). The statment information are passed as handler parameters
    so the higher level library can assemble backtrace.
    To do it correctly v C++ code requires manipulating call stack (CallStackHelper+CodeContextHelperBase) to provide
    fake entry and also ProgramThreadCountContextHelper to provide new frame corresponding to new stack entry for
    get_local_vars(frameId) function.
    These manipulations are very tricky so let's leave stack unchanged and assemble stack in higher-level
    server side debug library.
*/
void ThreadLocalProgramData::dbgAttach(ExceptionSink* xsink) {
    // a thread that the debugger does not want is not offered to it again (#3293)
    if (runState == DBG_RS_DETACH && !dbg_dont_attach.load(std::memory_order_acquire)) {
        DebugRunStateEnum rs = runState;
        const AbstractStatement* rts = runToStatement;
        functionCallLevel = 0;
        printd(5, "ThreadLocalProgramData::dbgAttach this: %p, rs: %d, rts: %p, tid: %d\n", this, runState, runToStatement, q_gettid());
        runState = DBG_RS_STOPPED;
        runToStatement = nullptr;
        // the frames running now make their exit events when they return (issue #6602)
        dbgRecordAttachFrames();
        getProgram()->priv->onAttach(rs, rts, xsink);
        setRunState(rs, rts);
        printd(5, "ThreadLocalProgramData::dbgAttach this: %p, rs: %d, rts: %p, xsink:%d\n", this, runState, runToStatement, xsink && xsink->isEvent());
    }
}

void ThreadLocalProgramData::dbgRecordAttachFrames() {
    dbgDropAttachFrames(0);
    QoreProgram* pgm = getProgram();
    // a frame runs in the Program that was current when the frame above it was pushed, and the top frame in the
    // thread's current Program
    QoreProgram* frame_pgm = ::getProgram();
    for (const QoreStackLocation* loc = get_runtime_stack_location(); loc; loc = loc->getNext()) {
        if (frame_pgm == pgm && loc->getCallType() == CT_USER) {
            // a location of native code is tagged, so that a frame pushed later at its address is not taken for it
            if (const StatementBlock* block = qore_rt_native_frame_block(loc, true)) {
                dbg_attach_frames.push_back({loc, block, true});
            } else if (dynamic_cast<const CodeEvaluationHelper*>(loc)) {
                // the call of a function, which runs as native code, in the IR interpreter or in the AST
                // interpreter; the latter two make their own exit events
                dbg_attach_frames.push_back({loc, nullptr, false});
            }
        }
        frame_pgm = loc->getProgram();
    }
    if (!dbg_attach_frames.empty()) {
        std::reverse(dbg_attach_frames.begin(), dbg_attach_frames.end());
        // the thread's count of the records of all Programs, which the native returns of any Program keep requested
        dbg_frame_count = qore_jit_debug_frame_count();
        dbg_frame_count->fetch_add(static_cast<int>(dbg_attach_frames.size()), std::memory_order_relaxed);
        qore_jit_request_debug_frame_return();
    }
}

void ThreadLocalProgramData::dbgDropAttachFrames(size_t keep) {
    if (dbg_attach_frames.size() > keep) {
        assert(dbg_frame_count);
        dbg_frame_count->fetch_sub(static_cast<int>(dbg_attach_frames.size() - keep), std::memory_order_relaxed);
        dbg_attach_frames.resize(keep);
    }
}

bool ThreadLocalProgramData::dbgIsAttachFrame(const DbgAttachFrame& frame, const QoreStackLocation* loc) {
    if (frame.loc != loc) {
        return false;
    }
    // a frame pushed later at the same address is not the recorded one: a location of native code is not tagged,
    // and the call of a function made since the debugger attached does not run as native code
    return frame.native_loc ? qore_rt_native_frame_tagged(loc)
        : dynamic_cast<const CodeEvaluationHelper*>(loc) != nullptr;
}

size_t ThreadLocalProgramData::dbgFlushReturnedFrames(ExceptionSink* xsink) {
    // the innermost recorded frame still on the call stack; the recorded frames inside it have returned
    size_t live = 0;
    for (const QoreStackLocation* loc = get_runtime_stack_location(); loc && !live; loc = loc->getNext()) {
        for (size_t i = dbg_attach_frames.size(); i; --i) {
            if (dbgIsAttachFrame(dbg_attach_frames[i - 1], loc)) {
                live = i;
                break;
            }
        }
    }
    // a frame that left the call stack without its exit event ended with an exception: it makes its exit event with
    // no value now, innermost first, as the AST and IR interpreters make it when the exception leaves the frame; a
    // call whose block is not known (a function called through the call path that ran as native code) makes none
    while (dbg_attach_frames.size() > live) {
        const StatementBlock* block = dbg_attach_frames.back().block;
        dbgDropAttachFrames(dbg_attach_frames.size() - 1);
        if (block) {
            QoreValue rv;
            dbgFunctionExitIntern(block, rv, xsink);
        }
    }
    return live;
}

void ThreadLocalProgramData::dbgNativeFrameReturn(const QoreStackLocation* top, const StatementBlock* block,
        const QoreValue& val, ExceptionSink* xsink) {
    if (dbg_attach_frames.empty()) {
        return;
    }
    // a debugger that detached, or is detaching, gets no more events: the records are dropped, so that the native
    // returns of the thread stop checking for them
    if (runState == DBG_RS_DETACH || attachFlag.load(std::memory_order_acquire) < 0) {
        dbgDropAttachFrames(0);
        return;
    }
    size_t live = dbgFlushReturnedFrames(xsink);
    if (live && dbgIsAttachFrame(dbg_attach_frames.back(), top)) {
        if (!block) {
            block = dbg_attach_frames.back().block;
        }
        dbgDropAttachFrames(live - 1);
        if (block) {
            // the debugger gets a copy: the frame's result cannot be changed
            QoreValue rv = val.refSelf();
            dbgFunctionExitIntern(block, rv, xsink);
            rv.discard(xsink);
        }
    }
}

void ThreadLocalProgramData::dbgDetach(ExceptionSink* xsink) {
    if (runState != DBG_RS_STOPPED && runState != DBG_RS_DETACH) {
        dbgDropAttachFrames(0);
        printd(5, "ThreadLocalProgramData::dbgDetach this: %p, rs: %d, rts: %p, tid: %d\n", this, runState, runToStatement, q_gettid());
        DebugRunStateEnum rs = DBG_RS_DETACH;
        const AbstractStatement* rts = runToStatement;
        runState = DBG_RS_STOPPED;
        runToStatement = nullptr;
        getProgram()->priv->onDetach(rs, rts, xsink);
        setRunState(rs, rts);
        printd(5, "ThreadLocalProgramData::dbgDetach this: %p, rs: %d, rts: %p, xsink:%d\n", this, runState, runToStatement, xsink && xsink->isEvent());
    }
}

void ThreadLocalProgramData::dbgClearTermination(ExceptionSink& xsink) {
    if (!xsink.isException()) {
        return;
    }
    QoreProgram* pgm = getProgram();
    if (!pgm || !pgm->priv->debugger_terminating) {
        return;
    }
    // the error code can be an inline short string, which has no node (issue #7038)
    if (QoreStringDataHelper(xsink.getExceptionErr()) == "THREAD-CANCELLED") {
        printd(5, "ThreadLocalProgramData::dbgClearTermination() this: %p, tid: %d\n", this, q_gettid());
        xsink.clear();
    }
}

int ThreadLocalProgramData::dbgStep(const StatementBlock* blockStatement, const AbstractStatement* statement, ExceptionSink* xsink) {
    checkAttach(xsink);
    checkBreakFlag();
    if (runState == DBG_RS_STOPPED || runState == DBG_RS_DETACH) {
        return 0;
    }
    // the exit events of the frames that were running when the debugger attached and ended with an exception since
    // (#6602)
    dbgCheckAttachFrames(xsink);
    if (*xsink) {
        return 0;
    }
    int rc = 0;
    unsigned bkptId = 0;
    bool cond = runState == DBG_RS_STEP || (runState == DBG_RS_STEP_OVER && functionCallLevel == 0);
    if (!cond) {
        const AbstractStatement* st = statement ? statement : blockStatement;
        cond = isRunToStatement(st);
        if (!cond && st->getBreakpointFlag()) {   // fast breakpoint check
            printd(5, "ThreadLocalProgramData::dbgStep() this: %p, rs: %d, tid: %d, breakpoint phase-1\n", this, runState, q_gettid());
            bkptId = getProgram()->priv->onCheckBreakpoint(st, xsink);  // more precise check requiring lock
            cond = bkptId > 0;
        }
    }
    if (cond) {
        printd(5, "ThreadLocalProgramData::dbgStep() this: %p, bkptId: %d, rs: %d, rts: %p, tid: %d\n", this, bkptId, runState, runToStatement, q_gettid());
        functionCallLevel = 0;
        DebugRunStateEnum rs = runState;
        const AbstractStatement* rts = runToStatement;
        runState = DBG_RS_STOPPED;
        runToStatement = nullptr;
        getProgram()->priv->onStep(blockStatement, statement, bkptId, rc, rs, rts, xsink);
        setRunState(rs, rts);
        printd(5, "ThreadLocalProgramData::dbgStep() this: %p, rs: %d, rts: %p, rc: %d, xsink:%d\n", this, runState, runToStatement, rc, xsink && xsink->isEvent());
    }
    return rc;
}

int ThreadLocalProgramData::dbgSyntheticBlockStep(const StatementBlock* blockStatement, ExceptionSink* xsink) {
    checkAttach(xsink);
    checkBreakFlag();
    if (runState == DBG_RS_STOPPED || runState == DBG_RS_DETACH) {
        return 0;
    }
    // the exit events of the frames that were running when the debugger attached and ended with an exception since
    // (#6602)
    dbgCheckAttachFrames(xsink);
    if (*xsink) {
        return 0;
    }

    int rc = 0;
    if (runState == DBG_RS_STEP || (runState == DBG_RS_STEP_OVER && functionCallLevel == 0)) {
        printd(5, "ThreadLocalProgramData::dbgSyntheticBlockStep() this: %p, rs: %d, tid: %d\n",
            this, runState, q_gettid());
        functionCallLevel = 0;
        DebugRunStateEnum rs = runState;
        const AbstractStatement* rts = runToStatement;
        runState = DBG_RS_STOPPED;
        runToStatement = nullptr;
        getProgram()->priv->onStep(blockStatement, nullptr, 0, rc, rs, rts, xsink);
        setRunState(rs, rts);
        printd(5, "ThreadLocalProgramData::dbgSyntheticBlockStep() this: %p, rs: %d, rts: %p, rc: %d, xsink:%d\n",
            this, runState, runToStatement, rc, xsink && xsink->isEvent());
    }
    return rc;
}

void ThreadLocalProgramData::dbgFunctionEnter(const StatementBlock* statement, ExceptionSink* xsink) {
    checkAttach(xsink);
    checkBreakFlag();
    if (runState != DBG_RS_STOPPED && runState != DBG_RS_DETACH) {
        // the exit events of the frames that were running when the debugger attached and ended with an exception
        // since (#6602)
        dbgCheckAttachFrames(xsink);
        if (*xsink) {
            return;
        }
    }
    if (runState == DBG_RS_STOPPED || runState == DBG_RS_DETACH) {
        // no code
    } else if (runState == DBG_RS_STEP || isRunToStatement(statement)) {
        printd(5, "ThreadLocalProgramData::dbgFunctionEnter() this: %p, rs: %d, rts: %p, tid: %d\n", this, runState, runToStatement, q_gettid());
        functionCallLevel = 0;
        DebugRunStateEnum rs = runState;
        const AbstractStatement* rts = runToStatement;
        runState = DBG_RS_STOPPED;
        runToStatement = nullptr;
        getProgram()->priv->onFunctionEnter(statement, rs, rts, xsink);
        setRunState(rs, rts);
        printd(5, "ThreadLocalProgramData::dbgFunctionEnter() this: %p, rs: %d, rts: %p, xsink: %d\n", this, runState, runToStatement, xsink && xsink->isEvent());
    } else if (runState == DBG_RS_STEP_OVER && functionCallLevel == 0) {
        functionCallLevel = 1;
        printd(5, "ThreadLocalProgramData::dbgFunctionEnter(), stepping over, this: %p, rs: %d, tid: %d\n", this, runState, q_gettid());
    } else if (functionCallLevel > 0) {
        functionCallLevel++;
    }
}

void ThreadLocalProgramData::dbgFunctionExit(const StatementBlock* statement, QoreValue& returnValue, ExceptionSink* xsink) {
    if (!dbg_attach_frames.empty() && runState != DBG_RS_STOPPED && runState != DBG_RS_DETACH) {
        // the exit events of the frames that were running when the debugger attached and ended with an exception
        // since, which are inside this one; a frame of the AST or IR interpreter that was running then makes its own
        // exit event now, also with an exception pending: its record is removed (#6602)
        size_t live = dbgFlushReturnedFrames(xsink);
        if (live && dbgIsAttachFrame(dbg_attach_frames.back(), get_runtime_stack_location())) {
            dbgDropAttachFrames(live - 1);
        }
    }
    dbgFunctionExitIntern(statement, returnValue, xsink);
}

void ThreadLocalProgramData::dbgFunctionExitIntern(const StatementBlock* statement, QoreValue& returnValue,
        ExceptionSink* xsink) {
    if (runState == DBG_RS_STOPPED || runState == DBG_RS_DETACH) {
        // no code
    } else if ((runState == DBG_RS_UNTIL_RETURN && functionCallLevel == 1) || runState == DBG_RS_STEP || (runState == DBG_RS_STEP_OVER && functionCallLevel == 0) || isRunToStatement(statement)) {
        printd(5, "ThreadLocalProgramData::dbgFunctionExit() this: %p, rs: %d, rts: %p, tid: %d\n", this, runState, runToStatement, q_gettid());
        DebugRunStateEnum rs = runState;
        const AbstractStatement* rts = runToStatement;
        functionCallLevel = 0;
        runState = DBG_RS_STOPPED;
        runToStatement = nullptr;
        getProgram()->priv->onFunctionExit(statement, returnValue, rs, rts, xsink);
        setRunState(rs, rts);
        printd(5, "ThreadLocalProgramData::dbgFunctionExit() this: %p, rs: %d, rts: %p, xsink: %d\n", this, runState, runToStatement, xsink && xsink->isEvent());
    } else if (functionCallLevel > 0) {
        functionCallLevel--;
        printd(5, "ThreadLocalProgramData::dbgFunctionExit() exit, this: %p, rs: %d, level: %d, tid: %d\n", this, runState, functionCallLevel, q_gettid());
    } else {
        checkAttach(xsink);
        checkBreakFlag();
    }
}

void ThreadLocalProgramData::dbgException(const AbstractStatement* statement, ExceptionSink* xsink) {
    // A sink can request thread exit without holding an exception. Only real exceptions have an
    // exception object for the debugger callback; non-exception unwinds preserve the current run state.
    if (!xsink || !xsink->isException()) {
        return;
    }
    if (runState != DBG_RS_STOPPED && runState != DBG_RS_DETACH) {
        printd(5, "ThreadLocalProgramData::dbgException() this: %p, rs: %d, rts: %p, tid: %d\n", this, runState, runToStatement, q_gettid());
        checkAttach(xsink);
        // the exit events of the frames that were running when the debugger attached and ended with an exception
        // before this one was raised (#6602)
        if (!dbg_attach_frames.empty()) {
            ExceptionSink frame_xsink;
            dbgFlushReturnedFrames(&frame_xsink);
            xsink->assimilate(frame_xsink);
        }
        functionCallLevel = 0;
        DebugRunStateEnum rs = runState;
        const AbstractStatement* rts = runToStatement;
        runState = DBG_RS_STOPPED;
        runToStatement = nullptr;
        getProgram()->priv->onException(statement, rs, rts, xsink);
        setRunState(rs, rts);
        printd(5, "ThreadLocalProgramData::dbgException() this: %p, rs: %d, rts: %p, xsink: %d\n", this, runState, runToStatement, xsink && xsink->isEvent());
    }
}

void qore_program_private::dbgRunExit(QoreValue& rv, ExceptionSink* xsink) {
    // called in the Program's context: the thread-local data is this Program's, if a debugger is attached
    ThreadLocalProgramData* tlpd = get_thread_local_program_data();
    if (tlpd) {
        tlpd->dbgExit(&sb, rv, xsink);
    }
}

void ThreadLocalProgramData::dbgExit(const StatementBlock* statement, QoreValue& returnValue, ExceptionSink* xsink) {
    if (runState != DBG_RS_STOPPED && runState != DBG_RS_DETACH) {
        // the exit events of the frames that were running when the debugger attached and ended with an exception
        // since; the others make no exit events after the exit (#6602)
        dbgCheckAttachFrames(xsink);
        dbgDropAttachFrames(0);
        printd(5, "ThreadLocalProgramData::dbgExit() this: %p, rs: %d, rts: %p, tid: %d\n", this, runState, runToStatement, q_gettid());
        DebugRunStateEnum rs = runState;
        const AbstractStatement* rts = runToStatement;
        functionCallLevel = 0;
        runState = DBG_RS_STOPPED;
        runToStatement = nullptr;
        getProgram()->priv->onExit(statement, returnValue, rs, rts, xsink);
        setRunState(rs, rts);
        printd(5, "ThreadLocalProgramData::dbgExit() this: %p, rs: %d, rts: %p, xsink: %d\n", this, runState, runToStatement, xsink && xsink->isEvent());
    }
}

QoreProgram::~QoreProgram() {
    printd(5, "QoreProgram::~QoreProgram() this: %p, pgmid: " QLLD "\n", this, (int64)priv->getProgramId());
    delete priv;
}

// setup independent program object
QoreProgram::QoreProgram() : priv(new qore_program_private(this, PO_DEFAULT)) {
    printd(5, "QoreProgram::QoreProgram() this: %p, pgmid: " QLLD "\n", this, (int64)priv->getProgramId());
}

// setup independent program object
QoreProgram::QoreProgram(const QoreParseOptions& po) : priv(new qore_program_private(this, po)) {
}

QoreProgram::QoreProgram(int64 po) : QoreProgram(QoreParseOptions(po)) {
}

QoreProgram::QoreProgram(QoreProgram* pgm, const QoreParseOptions& po, bool ec, const char* ecn,
        const QoreParseOptions* parent_po, ExceptionSink* xsink)
        : priv(new qore_program_private(this, po, pgm, parent_po, xsink)) {
    printd(QPP_DBG_LVL, "QoreProgram::QoreProgram(), this: %p, pgm: %p, priv: %p, pgmid: " QLLD "\n", this, pgm, priv,
        (int64)priv->getProgramId());
    priv->exec_class = ec;
    if (ecn) {
        priv->exec_class_name = ecn;
    }
}

QoreThreadLock* qore_program_ops::getParseLockImpl(QoreProgram* b) {
    return &b->priv->plock;
}

void QoreProgram::deref(ExceptionSink* xsink) {
    printd(QPP_DBG_LVL, "QoreProgram::deref() this: %p priv: %p %d->%d\n", this, priv, reference_count(), reference_count() - 1);
    if (priv->derefUnregister()) {
        // breakpoints refer to Programs with references only
        priv->deleteAllBreakpoints();
        priv->clear(xsink);
    }
}

void QoreProgram::deref() {
    printd(QPP_DBG_LVL, "QoreProgram::deref() this: %p priv: %p %d->%d\n", this, priv, reference_count(),
        reference_count() - 1);
    if (priv->derefUnregister()) {
        // the Program's own dependency reference is released, as clear() does, so that the Program is destroyed once
        // nothing refers to it (the user variants of its code, for example; qore#5822); its code is not deleted here,
        // so a Program that was never cleared stays allocated with its code (its owner is expected to clear it first
        // with waitForTerminationAndDeref())
        priv->depDeref();
    }
}

bool qore_program_private::derefUnregister() {
    // a reference that is not the last one is released without the registry lock
    int r = pgm->references.load(std::memory_order_relaxed);
    while (r > 1) {
        if (pgm->references.compare_exchange_weak(r, r - 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
            return false;
        }
    }
    // the last reference: the count reaches zero with the registry lock held, where lookups take their references,
    // so the Program leaves the registry before any lookup can find it without references; another reference may
    // have been taken since the count was read, in which case this one is not the last one
    QoreAutoRWWriteLocker al(&lck_programMap);
    if (!pgm->ROdereference()) {
        return false;
    }
    programid_to_program_map_t::iterator i = programid_to_program_map.find(programId);
    assert(i != programid_to_program_map.end() && i->second == pgm);
    programid_to_program_map.erase(i);
    // the method lookups cached for code of this Program are no longer known to be stable (the classes that it is the
    // source Program of raise an error for a call once it is torn down; QoreCallTargetCheckCache, issue #5824)
    qore_bump_method_dispatch_epoch();
    return true;
}

ExceptionSink* qore_program_ops::getParseExceptionSinkImpl(QoreProgram* b) {
    if (b->priv->requires_exception)
        return nullptr;

    return b->priv->parseSink;
}

int QoreProgram::setWarningMask(int wm) {
    // the check and the change are atomic with a change of PO_LOCK_WARNINGS (issue #6078)
    std::lock_guard<std::recursive_mutex> lg(priv->pwo.parse_options.writeLock());
    if (!(priv->pwo.parse_options & PO_LOCK_WARNINGS)) {
        priv->pwo.setWarnMask(wm);
        return 0;
    }
    return -1;
}

// returns 0 for success, -1 for error
int QoreProgram::enableWarning(int code) {
    // the check and the change are atomic with a change of PO_LOCK_WARNINGS (issue #6078)
    std::lock_guard<std::recursive_mutex> lg(priv->pwo.parse_options.writeLock());
    if (!(priv->pwo.parse_options & PO_LOCK_WARNINGS)) {
        priv->pwo.orWarnMask(code);
        return 0;
    }
    return -1;
}

// returns 0 for success, -1 for error
int QoreProgram::disableWarning(int code) {
    // the check and the change are atomic with a change of PO_LOCK_WARNINGS (issue #6078)
    std::lock_guard<std::recursive_mutex> lg(priv->pwo.parse_options.writeLock());
    if (!(priv->pwo.parse_options & PO_LOCK_WARNINGS)) {
        priv->pwo.andWarnMask(~code);
        return 0;
    }
    return -1;
}

RootQoreNamespace* QoreProgram::getRootNS() const {
    return priv->RootNS;
}

QoreParseOptions QoreProgram::getParseOptions() const {
    return priv->pwo.parse_options;
}

int64 QoreProgram::getParseOptions64() const {
    return getParseOptions().getLo();
}

QoreListNode* QoreProgram::getUserFunctionList() {
    ExceptionSink xsink;
    ProgramRuntimeParseAccessHelper pah(&xsink, this);
    if (xsink) {
        xsink.clear();
        return nullptr;
    }
    return qore_ns_private::getUserFunctionList(*priv->RootNS);
}

void QoreProgram::waitForTermination() {
    priv->waitForAllThreadsToTerminate();
}

void qore_program_private::decThreadCount(QoreProgram& pgm, int tid, ExceptionSink* xsink) {
    bool clear = false;
    unsigned derefs = pgm.priv->decThreadCount(tid, clear);
    if (derefs) {
        releaseDeferredDerefs(pgm, derefs, clear, xsink);
    }
}

void qore_program_private::unlockParsing(QoreProgram& pgm, ExceptionSink* xsink) {
    bool clear = false;
    unsigned derefs = pgm.priv->unlockParsing(clear);
    if (derefs) {
        releaseDeferredDerefs(pgm, derefs, clear, xsink);
    }
}

void qore_program_private::releaseDeferredDerefs(QoreProgram& pgm, unsigned derefs, bool clear,
        ExceptionSink* xsink) {
    assert(derefs);
    // the thread deleted the Program while running its code or holding its parse lock, and has left the code and
    // released the lock now: clear and dereference it as the deletion would have (see
    // QoreProgram::waitForTerminationAndDeref())
    ExceptionSink local_xsink;
    if (clear) {
        pgm.priv->waitForTerminationAndClear(&local_xsink);
    }
    while (derefs--) {
        pgm.deref(&local_xsink);
    }
    if (local_xsink) {
        if (xsink) {
            xsink->assimilate(local_xsink);
        } else {
            local_xsink.handleExceptions();
        }
    }
}

void QoreProgram::waitForTerminationAndDeref(ExceptionSink* xsink) {
    // a thread that runs the Program's code cannot wait for itself to leave it: the Program is kept until it has
    // left it, as it is kept while another thread runs its code (issue #5538)
    if (priv->deferDerefIfRunning()) {
        return;
    }
    if (xsink) {
        priv->waitForTerminationAndClear(xsink);
        deref(xsink);
        return;
    }

    ExceptionSink local_xsink;
    priv->waitForTerminationAndClear(&local_xsink);
    deref(&local_xsink);
    local_xsink.clear();
}

void QoreProgram::lockOptions() {
    // atomic with the lock check of a change of the options (issue #6078); the write lock before po_mutex (see
    // qore_program_private_base::po_mutex); the restrictions that it has now are kept
    std::lock_guard<std::recursive_mutex> lg(priv->pwo.parse_options.writeLock());
    std::lock_guard<std::mutex> po_guard(priv->po_mutex);
    QORE_DBG_RECORD_CHANGE(QPRS_PARSE_OPTION_LOCK, priv);
    // the restrictions in effect for the caller: in a file with %push-parse-options, the file's own ones
    QoreParseOptions current = priv->pwo.parse_options.load();
    priv->locked_restrictions |= current & ~QoreParseOptions::POSITIVE_OPTIONS;
    // the Program's own compatibility options are locked as well: %modern does not reset them (issue #6382); those of
    // a file with %push-parse-options end with it
    priv->locked_compat |= priv->pwo.parse_options.loadShared()
        & qore_program_private_base::modernIncompatibleOptions();
    priv->po_locked = true;
    // the restrictions that a file with %push-parse-options locks are kept after it: they are the Program's own from
    // now on (issue #6207)
    const ParseOptionsOverlayFrame* frame = q_parse_overlay_find_delta(&priv->pwo.parse_options);
    if (frame && frame->kind == ParseOptionsOverlayFrame::PUSHED_FILE) {
        priv->keepLockedRestrictionsIntern();
    }
}

QoreParseOptions qore_program_private_base::getCreatorRestrictions(const QoreParseOptions& creator_po) {
    // the options of style and backward compatibility, %modern's included, are the created Program's own; the
    // options that decide what user code a child inherits from its parent are not restrictions of a Program that is
    // not a child (nor is the inheritance of the module data of a parent), and the system options are never
    // inherited this way; the options that keep system code from a Program are taken, as an independent Program
    // copies its namespace from the system namespace, not from its creator's
    static const QoreParseOptions not_inherited = QoreParseOptions::POSITIVE_OPTIONS | lockedMayDisable()
        | modernImpliedOptions() | modernIncompatibleOptions()
        | QoreParseOptions(PO_USER_INHERITANCE_OPTIONS | PO_NO_INHERIT_PROGRAM_DATA | PO_SYSTEM_OPS);
    return creator_po & ~not_inherited;
}

QoreParseOptions qore_program_private_base::restrictForCreator(const QoreProgram* creator,
        const QoreParseOptions& po, QoreParseOptions& restrictions, bool& restricted) {
    restricted = false;
    restrictions = QoreParseOptions();
    if (!creator) {
        return po;
    }
    QoreParseOptions creator_po;
    bool creator_locked;
    {
        // the options of the creator may be changed in another thread
        std::lock_guard<std::mutex> po_guard(creator->priv->po_mutex);
        creator_po = creator->priv->pwo.parse_options;
        creator_locked = creator->priv->po_locked;
    }
    QoreParseOptions creator_restrictions = getCreatorRestrictions(creator_po);
    // sandboxed code cannot create a Program that is free of its restrictions
    const bool sandboxed = qore_caller_is_sandboxed();
    // code without restrictions that is not locked has none that the Program could escape; such code creates an
    // independent Program with the options it gives, as before (QoreRepl, for example, creates one with
    // PO_ALLOW_REPARSE); a locked Program has no positive options that it cannot have, which the new Program does not
    // get either
    if (!sandboxed && ((creator_po & PO_NO_CHILD_PO_RESTRICTIONS) || (!creator_restrictions && !creator_locked))) {
        return po;
    }
    restricted = true;
    restrictions = creator_restrictions;
    // the restrictions of the creator are added, and the options that give more freedom that it does not have are
    // removed, as setParent() does for a child; the children of the Program of sandboxed code inherit them;
    // PO_ALLOW_REPARSE is kept, as it only allows code to be parsed in the new Program more than once (QoreRepl
    // creates its Program with it)
    QoreParseOptions allowed = creator_po | ~QoreParseOptions::POSITIVE_OPTIONS | QoreParseOptions(PO_ALLOW_REPARSE);
    if (sandboxed) {
        allowed &= ~QoreParseOptions(PO_NO_CHILD_PO_RESTRICTIONS);
    }
    return (po | restrictions) & allowed;
}

void qore_program_private_base::lockCreatorRestrictions(QoreProgram& pgm, const QoreParseOptions& restrictions) {
    std::lock_guard<std::mutex> po_guard(pgm.priv->po_mutex);
    // the options were given by restrictForCreator(), so the Program has the restrictions already
    assert((pgm.priv->pwo.parse_options & restrictions) == restrictions);
    pgm.priv->locked_restrictions |= restrictions;
    pgm.priv->po_locked = true;
}

// setExecClass() NOTE: string passed here will copied
void QoreProgram::setExecClass(const char* ecn) {
    priv->exec_class = true;
    if (ecn)
        priv->exec_class_name = ecn;
    else
        priv->exec_class_name.clear();
}

QoreNamespace* QoreProgram::getQoreNS() const {
    return priv->QoreNS;
}

void QoreProgram::depRef() {
    priv->depRef();
}

void QoreProgram::depDeref() {
    priv->depDeref();
}

bool QoreProgram::checkWarning(int code) const {
    return priv->warnSink && (code & priv->pwo.getWarnMask());
}

int QoreProgram::getWarningMask() const {
    return priv->warnSink ? priv->pwo.getWarnMask() : 0;
}

bool QoreProgram::existsFunction(const char* name) {
    ExceptionSink xsink;
    ProgramRuntimeParseAccessHelper pah(&xsink, this);
    if (xsink) {
        xsink.clear();
        return false;
    }
    return qore_root_ns_private::runtimeExistsFunction(*priv->RootNS, name) ? true : false;
}

void QoreProgram::parseSetParseOptions(const QoreParseOptions& po) {
    priv->parseSetParseOptions(&loc_builtin, po);
}

void QoreProgram::parseDisableParseOptions(const QoreParseOptions& po) {
    priv->parseDisableParseOptions(&loc_builtin, po);
}

namespace {
//! the parse directives of the options that %modern implies or does not allow (issue #5682)
struct QoreModernOptionName {
    const QoreParseOptions* option;
    int64 legacy_option;
    const char* directive;
};
}

void qore_program_private_base::describeParseOptions(const QoreParseOptions& po, QoreString& str) {
    static const QoreModernOptionName names[] = {
        {nullptr, PO_BROKEN_NARROWED_TYPES, "%broken-narrowed-types"},
        {nullptr, PO_STRICT_BOOLEAN_EVAL, "%strict-bool-eval"},
        {nullptr, PO_BROKEN_LIST_PARSING, "%broken-list-parsing"},
        {nullptr, PO_BROKEN_LOGIC_PRECEDENCE, "%broken-logic-precedence"},
        {nullptr, PO_BROKEN_INT_ASSIGNMENTS, "%broken-int-assignments"},
        {nullptr, PO_BROKEN_OPERATORS, "%broken-operators"},
        {nullptr, PO_BROKEN_LOOP_STATEMENT, "%broken-loop-statement"},
        {nullptr, PO_BROKEN_REFERENCES, "%broken-references"},
        {nullptr, PO_BROKEN_SPRINTF, "%broken-sprintf"},
        {nullptr, PO_BROKEN_CAST, "%broken-cast"},
        {nullptr, PO_BROKEN_NAMESPACE_RESOLUTION, "%broken-namespace-resolution"},
        {nullptr, PO_BROKEN_RANGE, "%broken-range"},
        {nullptr, PO_BROKEN_VARARGS, "%broken-varargs"},
        {nullptr, PO_BROKEN_LIST_RANGE, "%broken-list-range"},
        {&QoreParseOptions::BROKEN_SOFT_TYPES, 0, "%broken-soft-types"},
        {&QoreParseOptions::BROKEN_AUTO_CAST, 0, "%broken-auto-cast"},
        {&QoreParseOptions::NO_STRING_INDEX_CHAR, 0, "%no-string-index-char"},
        {&QoreParseOptions::NO_SUMMARIZE, 0, "%no-summarize"},
        {&QoreParseOptions::STREAMING_ANY, 0, "%streaming-any"},
        {&QoreParseOptions::NEGATIVE_OFFSETS, 0, "%negative-offsets"},
    };
    bool first = true;
    for (const auto& n : names) {
        QoreParseOptions o = n.option ? *n.option : QoreParseOptions(n.legacy_option);
        if (!(po & o)) {
            continue;
        }
        if (!first) {
            str.concat(", ");
        }
        first = false;
        str.concat(n.directive);
    }
}

//! The reason that ends the descriptions of the conflicts between %modern and the options it does not allow
static constexpr const char* qore_modern_conflict_reason = ": the option changes the meaning of code, and %modern "
    "guarantees a consistent set of parse options";

void qore_program_private_base::describeModernSetConflict(const QoreParseOptions& current, const QoreParseOptions& po,
        const QoreParseOptions& conflict, const char* verb, QoreString& desc) {
    if (isModern(current)) {
        // an option that %modern does not allow is set while %modern is in effect
        describeParseOptions(conflict & po, desc);
        desc.sprintf(" cannot be %s while %%modern is in effect", verb);
    } else {
        // the options set put %modern in effect: given as such, or completed by the options set
        desc.concat((po & PO_MODERN) == PO_MODERN ? "%modern" : "%modern (which the options set complete)");
        QoreParseOptions with = conflict & po;
        QoreParseOptions in_effect = conflict & ~po;
        if (with) {
            desc.concat(" and ");
            describeParseOptions(with, desc);
            desc.sprintf(" cannot be %s together", verb);
            if (in_effect) {
                desc.concat(", nor while ");
                describeParseOptions(in_effect, desc);
                desc.concat(" is in effect");
            }
        } else {
            desc.sprintf(" cannot be %s while ", verb);
            describeParseOptions(in_effect, desc);
            desc.concat(" is in effect");
        }
    }
    desc.concat(qore_modern_conflict_reason);
}

void qore_program_private_base::describeModernSourceDefaultConflict(const char* filename,
        const QoreParseOptions& conflict, QoreString& desc) {
    desc.sprintf("'%s' is parsed in %%modern mode by its extension, which cannot be used while ", filename);
    describeParseOptions(conflict, desc);
    desc.concat(" is in effect");
    desc.concat(qore_modern_conflict_reason);
    desc.concat("; parse the code as a .q file, or without the option");
}

int qore_program_private_base::checkModernConflict(const QoreParseOptions& current, const QoreParseOptions& po, bool set,
        const char* what, ExceptionSink* xsink) {
    QoreParseOptions conflict = getModernConflict(current, po, set);
    if (!conflict) {
        return 0;
    }
    QoreStringNode* desc = new QoreStringNodeMaker("%s: ", what);
    if (set) {
        describeModernSetConflict(current, po, conflict, "set", *desc);
    } else {
        describeParseOptions(conflict, *desc);
        desc->concat(" cannot be disabled while %modern is in effect, which implies the option and guarantees a "
            "consistent set of parse options");
    }
    xsink->raiseException("OPTION-ERROR", desc);
    return -1;
}

int qore_program_private_base::parseCheckModernConflict(const QoreProgramLocation* loc, const QoreParseOptions& po,
        bool set, const QoreParseOptions& reset) {
    QoreParseOptions current = pwo.parse_options.load() & ~reset;
    QoreParseOptions conflict = getModernConflict(current, po, set);
    if (!conflict) {
        return 0;
    }
    QoreStringNode* desc = new QoreStringNode;
    if (set) {
        describeModernSetConflict(current, po, conflict, "used", *desc);
        // %modern is complete only with the restrictions that a module parse option mask added to the Program of the
        // module: the error names them, as the user did not write %modern (issue #5682)
        QoreParseOptions all = current | po;
        if (module_mask_added && isModern(all) && !isModern(all & ~module_mask_added)) {
            desc->concat("; %modern is complete only with ");
            describeParseOptions(module_mask_added, *desc);
            desc->concat(", which the module parse option mask adds (see set_module_parse_option_mask() and "
                "load_module_masked())");
            desc->concat("; remove the option, or load the module with a mask that does not add them");
        } else {
            desc->concat(isModern(current)
                ? "; remove the directive, or parse the code without %modern (as a .q file, or after %old-style)"
                : "; remove the option, or parse the code without %modern");
        }
    } else {
        // %no-negative-offsets is the only directive that disables an option that %modern implies
        desc->concat("%no-negative-offsets cannot be used while %modern is in effect, which implies ");
        describeParseOptions(conflict, *desc);
        desc->concat(" and guarantees a consistent set of parse options");
        // %modern is complete only with the restrictions that a module parse option mask added (see above)
        if (module_mask_added && isModern(current) && !isModern(current & ~module_mask_added)) {
            desc->concat("; %modern is complete only with ");
            describeParseOptions(module_mask_added, *desc);
            desc->concat(", which the module parse option mask adds (see set_module_parse_option_mask() and "
                "load_module_masked()); remove the directive, or load the module with a mask that does not add them");
        } else {
            desc->concat("; remove the directive, or parse the code without %modern (as a .q file, or after "
                "%old-style)");
        }
    }
    qore_program_private::makeParseException(pgm, *loc, "PARSE-OPTION-ERROR", desc);
    return -1;
}

int qore_program_private::checkModernSourceDefault(const QoreParseOptions& current, const char* filename,
        ExceptionSink* xsink) {
    QoreParseOptions conflict = current & modernIncompatibleOptions();
    if (!conflict) {
        return 0;
    }
    QoreStringNode* desc = new QoreStringNode;
    describeModernSourceDefaultConflict(filename, conflict, *desc);
    xsink->raiseException("PARSE-OPTION-ERROR", desc);
    return -1;
}

QoreParseOptions qore_aot_merge_parse_options(const QoreParseOptions& final, const QoreParseOptions& base) {
    return final | (base & ~qore_program_private_base::modernIncompatibleOptions());
}

bool qore_check_modern_parse_options(const QoreParseOptions& po, std::string& error) {
    if (!qore_program_private_base::isModern(po)) {
        return true;
    }
    QoreParseOptions conflict = po & qore_program_private_base::modernIncompatibleOptions();
    if (!conflict) {
        return true;
    }
    QoreString desc("PARSE-OPTION-ERROR: ");
    // the options are given together: nothing is in effect before them
    qore_program_private_base::describeModernSetConflict(QoreParseOptions(), po, conflict, "set", desc);
    error = desc.c_str();
    return false;
}

QoreParseOptions ParseOptionsOverlayFrame::view(const QoreParseOptions& shared) const {
    if (kind == STATEMENT) {
        return po;
    }
    QoreParseOptions rv = (shared & ~remove) | add;
    // a change for the whole Program made while the frame is in effect can conflict with the frame's own options
    // (%modern with a compatibility option that %modern does not allow)
    if (qore_program_private_base::isModern(rv)) {
        QoreParseOptions incompatible = rv & qore_program_private_base::modernIncompatibleOptions();
        if (incompatible) {
            if (add & incompatible) {
                // the frame's code is legacy code with a compatibility option of its own, and the Program got
                // %modern after the frame began: the frame's code keeps every option of %modern and every option
                // that %modern implies as it had them, the Program's options when the frame began with the frame's
                // own changes
                static const QoreParseOptions modern_related = QoreParseOptions(PO_MODERN)
                    | qore_program_private_base::modernImpliedOptions() | QoreParseOptions::STRICT_PARSE_ERRORS;
                rv = (rv & ~modern_related) | (((base & ~remove) | add) & modern_related);
                // the frame's code was not %modern with its compatibility option (refused when it was set)
                assert(!qore_program_private_base::isModern(rv));
                if (qore_program_private_base::isModern(rv)) {
                    rv &= ~(QoreParseOptions(PO_MODERN) & ~add);
                }
            } else {
                // the frame's code set %modern itself, and the Program got a compatibility option that it does not
                // allow after the frame began: the frame's %modern wins for its code
                rv &= ~incompatible;
            }
        }
    }
    return qore_program_private_base::withImplications(rv);
}

QoreParseOptions SyncParseOptions::loadView() const {
    QoreParseOptions shared = loadShared();
    const ParseOptionsOverlayFrame* frame = q_parse_overlay_find(this);
    return frame ? frame->view(shared) : shared;
}

ParseWarnOptions ProgramParseWarnOptions::getView() const {
    QoreParseOptions shared = parse_options.loadShared();
    const ParseOptionsOverlayFrame* frame = q_parse_overlay_find(&parse_options);
    if (!frame) {
        return ParseWarnOptions(shared, warn_mask.load(std::memory_order_relaxed));
    }
    // a statement has a warning mask of its own, the other frames have the Program's own
    if (frame->kind == ParseOptionsOverlayFrame::STATEMENT) {
        return ParseWarnOptions(frame->po, frame->warn_mask);
    }
    return ParseWarnOptions(frame->view(shared), warn_mask.load(std::memory_order_relaxed));
}

QoreParseOptions qore_program_private_base::readParseOptions(const QoreProgram& p) {
    return p.priv->pwo.parse_options;
}

int qore_program_private_base::checkNewProgramModernOptions(const QoreProgram* parent,
        const QoreParseOptions& parent_po, const QoreParseOptions& po, ExceptionSink* xsink, const char* what) {
    // a child Program has its parent's options too, unless the parent allows other options for its children; the
    // caller read the parent's options once and creates the child with the same value (issues #6078, #6176)
    QoreParseOptions inherited;
    if (parent && !(parent_po & PO_NO_CHILD_PO_RESTRICTIONS)) {
        inherited = parent_po;
    }
    if (!isModern(inherited | po)) {
        return 0;
    }
    if (isModern(po) || !isModern(inherited)) {
        return checkModernConflict(inherited, po, true, what, xsink);
    }
    // %modern comes from the parent
    QoreStringMaker label("%s (the new Program has %%modern from its parent, which can allow other options for its "
        "child Programs with %%no-child-restrictions, or create an independent Program)", what);
    return checkModernConflict(inherited, po, true, label.c_str(), xsink);
}

int qore_program_private::raiseLockedOptions(const QoreParseOptions& requested, ExceptionSink* xsink) {
    QoreStringNode* desc = new QoreStringNode("parse options have been locked on this program object; requested:");
    for (unsigned i = 0; i < 64; ++i) {
        int64 bit = static_cast<int64>(uint64_t(1) << i);
        if (requested.getLo() & bit) {
            auto it = pomaps.pomap.find(bit);
            const char* name = it == pomaps.pomap.end() ? ParseOptionMap::find_name(QoreParseOptions(bit))
                : it->second;
            if (name) {
                desc->sprintf(" %s", name);
            } else {
                desc->sprintf(" bit %u", i);
            }
        }
        if (requested.getHi() & bit) {
            const char* name = ParseOptionMap::find_name(QoreParseOptions(0, bit));
            if (name) {
                desc->sprintf(" %s", name);
            } else {
                desc->sprintf(" bit %u", i + 64);
            }
        }
    }
    xsink->raiseException("OPTIONS-LOCKED", desc);
    return -1;
}

void QoreProgram::setParseOptions(const QoreParseOptions& po, ExceptionSink* xsink) {
    priv->setParseOptions(po, xsink);
}

void QoreProgram::disableParseOptions(const QoreParseOptions& po, ExceptionSink* xsink) {
    priv->disableParseOptions(po, xsink);
}

void QoreProgram::replaceParseOptions(const QoreParseOptions& po, ExceptionSink* xsink) {
    priv->replaceParseOptions(po, xsink);
}

QoreHashNode* QoreProgram::getThreadData() {
    // the thread's map holds the only reference, so the caller can change the hash
    QoreHashNode* h = priv->getThreadDataRef(true, false);
    assert(h);
    return h;
}

void qore_program_private_base::joinThreadDataFamily(QoreProgram* p_pgm) {
    qore_program_private* root = p_pgm->priv->tld_root.load(std::memory_order_acquire);
    assert(root);
    // the reference keeps the root valid while tld_root can point to it, including after the root has made this
    // Program the root of a family of its own
    root->depRef();
    tld_parent_root = root;
    bool member;
    {
        AutoLocker al(root->tld_lock);
        member = !root->tld_final;
        if (member) {
            std::lock_guard<std::mutex> cl(root->threadContainerLock());
            root->tld_members.insert(pgm->priv);
        }
    }
    tld_root.store(member ? root : pgm->priv, std::memory_order_release);
}

void qore_program_private_base::leaveThreadDataFamily() {
    qore_program_private* root = tld_parent_root;
    if (!root) {
        return;
    }
    tld_parent_root = nullptr;
    {
        AutoLocker al(root->tld_lock);
        std::lock_guard<std::mutex> cl(root->threadContainerLock());
        root->tld_members.erase(pgm->priv);
    }
    root->depDeref();
}

QoreHashNode* qore_program_private::getThreadDataRef(bool create, bool ref) {
    ThreadProgramData* tpd = get_thread_program_data();
    assert(tpd);
    qore_program_private* root = tld_root.load(std::memory_order_acquire);
    while (true) {
        bool final = false;
        QoreHashNode* h = tpd->getThreadDataRef(root, create, final, ref);
        if (h || !final) {
            return h;
        }
        // the family's thread-local data has been freed for the last time; if this Program has become the root of a
        // family of its own meanwhile, its data is used
        qore_program_private* new_root = tld_root.load(std::memory_order_acquire);
        if (new_root != root) {
            root = new_root;
            continue;
        }
        // no code of the family runs after its root's teardown has freed the family's thread-local data for the last
        // time; release builds give the thread a hash that no family shares, freed when the thread ends
        assert(false);
        return tpd->getOrphanThreadDataRef(ref);
    }
}

void qore_program_private::clearThreadData(ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(getThreadDataRef(false), xsink);
    printd(5, "qore_program_private::clearThreadData() this: %p h: %p (size: %d)\n", this, *h, (int)(h ? h->size() : 0));
    if (h) {
        // no lock is held here: clearing the hash can run destructors
        qore_hash_private::get(**h)->clear(xsink, false);
    }
}

void qore_program_private::freeFamilyThreadData(bool final, ExceptionSink* xsink) {
    while (true) {
        std::vector<ThreadProgramData*> threads;
        {
            AutoLocker al(tld_lock);
            if (tld_registry.empty()) {
                if (final && !tld_final) {
                    tld_final = true;
                    // the members still alive become roots of families of their own; each keeps its dependency
                    // reference on this Program, so a thread that has just read its old root can still use it
                    for (qore_program_private* member : tld_members) {
                        member->tld_root.store(member, std::memory_order_release);
                    }
                    std::lock_guard<std::mutex> cl(threadContainerLock());
                    tld_members.clear();
                }
                break;
            }
            threads.reserve(tld_registry.size());
            for (ThreadProgramData* tpd : tld_registry) {
                // the reference keeps the thread's data valid until it has been processed here, even if the thread
                // ends meanwhile
                tpd->ref();
                threads.push_back(tpd);
            }
            std::lock_guard<std::mutex> cl(threadContainerLock());
            tld_registry.clear();
        }
        for (ThreadProgramData* tpd : threads) {
            QoreHashNode* h = tpd->detachThreadData(this);
            if (h) {
                // no lock is held here: dereferencing the hash can run destructors, which can save thread-local data
                // in the family again; the loop frees that too
                h->deref(xsink);
            }
            tpd->deref();
        }
    }
}

QoreValue QoreProgram::run(ExceptionSink* xsink) {
    printd(5, "QoreProgram::run() exec_class_name='%s' empty=%d\n",
        priv->exec_class_name.c_str(), (int)priv->exec_class_name.empty());
    if (!priv->exec_class_name.empty()) {
        runClass(priv->exec_class_name.c_str(), xsink);
        QoreValue rv = priv->exec_class_rv;
        priv->exec_class_rv = QoreValue();
        return rv;
    }
    return runTopLevel(xsink);
}

void QoreProgram::parse(FILE* fp, const char* name, ExceptionSink* xsink, ExceptionSink* wS, int wm) {
    priv->parse(fp, name, xsink, wS, wm);
}

void QoreProgram::parse(const QoreString* str, const QoreString* lstr, ExceptionSink* xsink, ExceptionSink* wS,
        int wm) {
    if (!str || str->empty())
        return;

    priv->parse(str, lstr, xsink, wS, wm);
}

void QoreProgram::parse(const QoreString* str, const QoreString* lstr, ExceptionSink* xsink, ExceptionSink* wS,
        int wm, const QoreString* source, int offset) {
    if (!str || str->empty())
        return;

    priv->parse(str, lstr, xsink, wS, wm, source, offset);
}

void QoreProgram::parse(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS, int wm) {
    if (!code || !code[0])
        return;

    priv->parse(code, label, xsink, wS, wm);
}

void QoreProgram::parse(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS, int wm,
        const char* source, int offset) {
    if (!code || !code[0])
        return;

    priv->parse(code, label, xsink, wS, wm, source, offset);
}

void QoreProgram::parseFile(const char* filename, ExceptionSink* xsink, ExceptionSink* wS, int wm,
        bool only_first_except) {
    // set with the parse lock held (see qore_program_private::parseFile())
    priv->parseFile(filename, xsink, wS, wm, only_first_except);
}

void QoreProgram::parsePending(const QoreString* str, const QoreString* lstr, ExceptionSink* xsink, ExceptionSink* wS,
        int wm) {
    if (!str || str->empty())
        return;

    priv->parsePending(str, lstr, xsink, wS, wm);
}

void QoreProgram::parsePending(const QoreString* str, const QoreString* lstr, ExceptionSink* xsink, ExceptionSink* wS,
        int wm, const QoreString* source, int offset) {
    if (!str || str->empty())
        return;

    priv->parsePending(str, lstr, xsink, wS, wm, source, offset);
}

void QoreProgram::parsePending(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS, int wm) {
    if (!code || !code[0])
        return;

    priv->parsePending(code, label, xsink, wS, wm);
}

void QoreProgram::parsePending(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS, int wm,
        const char* source, int offset) {
    if (!code || !code[0])
        return;

    priv->parsePending(code, label, xsink, wS, wm, source, offset);
}

QoreValue QoreProgram::runTopLevel(ExceptionSink* xsink) {
    ProgramThreadCountContextHelper tch(xsink, this, true);
    if (*xsink)
        return QoreValue();
    // the frame of the top-level code is a frame of this context (see getContextFrame())
    tch.setTopLevel();

    // the top-level code runs in this Program with no object or class context: the code that runs it, such as the
    // builtin Program::run() method, has the Program object and class and the calling Program as its context, which
    // the closures created and the frames of the calls made in the top-level code would otherwise get
    ProgramCallContextHelper pcch(this);
    ObjectSubstitutionHelper osh(nullptr, nullptr);

    // Save and set runtime_po for this program's top-level code.
    // Without this, runtime_po retains the outer program's value,
    // causing runtime_check_parse_option() to check the wrong program's
    // options (e.g., PO_STRICT_ARGS from an outer %modern program leaking
    // into a sub-program without it).
    // RAII guard ensures restore even if an unexpected C++ exception propagates.
    // As with QoreProgramLocationHelper, the stack is checked when the location is set, with the caller's exception
    // sink, and not when it is restored
    class RuntimePoGuard {
    public:
        //! sets the parse options; raises STACK-LIMIT-EXCEEDED in xsink if the stack limit has been reached
        RuntimePoGuard(ExceptionSink* xsink, const QoreParseOptions& new_po) {
            swap_runtime_statement_location(xsink, nullptr, nullptr, new_po, saved_stmt, saved_loc, saved_po);
        }
        ~RuntimePoGuard() {
            update_runtime_statement_location(saved_stmt, saved_loc, saved_po);
        }
    private:
        const AbstractStatement* saved_stmt = nullptr;
        const QoreProgramLocation* saved_loc = nullptr;
        QoreParseOptions saved_po;
    };
    RuntimePoGuard po_guard(xsink, priv->pwo.parse_options);
    if (*xsink) {
        return QoreValue();
    }
    ValueHolder rv(priv->sb.exec(xsink), xsink);
    // the end of the top-level code of a Program that a debugger ends is a cancellation point: a thread that the
    // debugger ended while it raised another exception, which the code caught, has a cancellation request, which it
    // raises here if the code ended with no statement or cancellation point after the catch (qore#5987)
    if (!*xsink && priv->debugger_terminating.load(std::memory_order_relaxed)) {
        qore_check_cancel(xsink, "end of the program's top-level code");
    }
    // the debugger's exit event of the top-level code gets the value that Program::run() returns, which it can
    // replace (#3401); after an exception, rv releases the value, and the event gets none
    QoreValue result = *xsink ? QoreValue() : rv.release();
    priv->dbgRunExit(result, xsink);
    if (*xsink) {
        // nothing is returned with an exception, also if the debugger set a value
        result.discard(xsink);
        return QoreValue();
    }
    return result;
}

template <bool LexicalInvocation>
QORE_INTERN_ALWAYS_INLINE QoreValue qore_program_private::callFunctionImpl(const char* name,
        const QoreListNode* args, ExceptionSink* xsink, LexicalSemanticPolicy policy, bool given_args) {
    SimpleRefHolder<FunctionCallNode> fc;

    printd(5, "QoreProgram::callFunction() creating function call to %s()\n", name);

    const FunctionEntry* fe;

    // need to grab parse lock for safe access to the user function map and imported function map
    {
        ProgramRuntimeParseAccessHelper pah(xsink, pgm);
        if (*xsink)
            return QoreValue();
        fe = qore_root_ns_private::runtimeFindFunctionEntry(*RootNS, name, policy);
    }

    if (!fe) {
        xsink->raiseException("NO-FUNCTION", "function '%s' does not exist", name);
        return QoreValue();
    }

    // Evaluate the call through the standard FunctionCallNode path with this (the target) Program
    // bound as the call's Program: this is what correctly resolves the cross-Program context and parse
    // options for every kind of target (user function defined in the target, Program-internal builtin
    // like load_module() operating on the target, and privileged builtins like set_save_object_callback()
    // invoked by a trusted caller on a sandboxed target).  Hand-rolling the parse-option handling here
    // gets one of those cases wrong; the shared path handles all of them.
    if constexpr (LexicalInvocation) {
        fc = qore_make_function_call<FunctionCallNode>(policy, get_runtime_location(), fe,
            const_cast<QoreListNode*>(args), pgm);
        ParseNode::bindSemanticPolicy(*fc, policy);
    } else {
        fc = new FunctionCallNode(get_runtime_location(), fe, const_cast<QoreListNode*>(args), pgm);
    }
    // a list that the caller gives is a temporary list of evaluated values that the call takes over (qore#6052)
    if (given_args && args) {
        fc->setTmpArgs();
    }
    QoreValue rv = !*xsink ? fc->eval(xsink) : QoreValue();

    // let caller delete function arguments if necessary
    fc->takeArgs();

    return rv;
}

QoreValue QoreProgram::callFunction(const char* name, const QoreListNode* args, ExceptionSink* xsink) {
    return priv->callFunctionImpl<false>(name, args, xsink, {});
}

QoreValue qore_program_private::callFunction(const char* name, const QoreListNode* args, ExceptionSink* xsink,
        LexicalSemanticPolicy policy) {
    return callFunctionImpl<true>(name, args, xsink, policy);
}

QoreValue qore_program_private::callFunctionGivenArgs(const char* name, QoreListNode* args, ExceptionSink* xsink) {
    return callFunctionImpl<false>(name, args, xsink, {}, true);
}

void QoreProgram::parseCommit(ExceptionSink* xsink, ExceptionSink* wS, int wm) {
    // before the commit switches to the Program: a sandboxed caller stays the caller of the initializers that the
    // commit runs (see QoreCallerSandboxHelper)
    QoreCallerSandboxHelper caller_sandbox(this, xsink);
    if (*xsink) {
        return;
    }
    ProgramRuntimeParseCommitContextHelper pch(xsink, this);
    if (*xsink)
        return;
    priv->parseCommit(xsink, wS, wm);
}

int QoreProgram::parseRollback(ExceptionSink* xsink) {
    return priv->parseRollback(xsink);
}

void QoreProgram::setExecMode(qore_exec_mode_t mode, bool user_requested) {
    priv->exec_mode = mode;
    priv->user_requested_exec_mode.store(user_requested, std::memory_order_release);
    priv->inherited_user_requested_exec_mode.store(false, std::memory_order_release);
}

qore_exec_mode_t QoreProgram::getExecMode() const {
    return priv->exec_mode;
}

int QoreProgram::setJitOptimizationLevel(int level) {
    if (level < -1 || level > 3) {
        return -1;
    }
    priv->jit_opt_level.store(level, std::memory_order_release);
    return 0;
}

int QoreProgram::getJitOptimizationLevel() const {
    const int level = priv->jit_opt_level.load(std::memory_order_acquire);
    return level >= 0 ? level : QoreJIT::getJITOptLevel();
}

void QoreProgram::setIRDump(bool dump) {
    priv->ir_dump = dump;
}

bool QoreProgram::getIRDump() const {
    return priv->ir_dump;
}

void QoreProgram::setIRFallbackWarn(bool warn) {
    priv->ir_fallback_warn = warn;
}

bool QoreProgram::getIRFallbackWarn() const {
    return priv->ir_fallback_warn;
}

void QoreProgram::setIRFallbackReport(bool report) {
    priv->ir_fallback_report = report;
}

bool QoreProgram::getIRFallbackReport() const {
    return priv->ir_fallback_report;
}

void QoreProgram::recordIRFallback(const char* reason) const {
    priv->recordIRFallback(reason);
}

void QoreProgram::printIRFallbackReport() const {
    priv->printIRFallbackReport();
}

void QoreProgram::runClass(const char* classname, ExceptionSink* xsink) {
    printd(5, "QoreProgram::runClass('%s') entered\n", classname);
    // find class
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(*priv->RootNS, classname);
    if (!qc) {
        xsink->raiseException("CLASS-NOT-FOUND", "cannot find any class '%s' in any namespace", classname);
        return;
    }

    if (qore_class_private::runtimeCheckInstantiateClass(*qc, xsink)) {
        return;
    }

    ProgramThreadCountContextHelper tch(xsink, this, true);
    if (!*xsink) {
        // Save the exec-class instance in the program private data instead of discarding it
        // immediately.  The constructor may spawn background threads that reference members of
        // the exec-class object (e.g. an HttpServer member).  If we discard the instance here,
        // those members are destroyed while the background threads are still running, leading to
        // deadlocks or use-after-free.  The instance is discarded in
        // waitForTerminationAndClear() after all threads have terminated.
        priv->exec_class_inst.discard(xsink);
        priv->exec_class_inst = qc->execConstructor((QoreListNode*)0, xsink);
        // the debugger's exit event of the exec class gets the value that Program::run() returns (the return value
        // that the class set), which it can replace (#3401)
        priv->dbgRunExit(priv->exec_class_rv, xsink);
    }
}

void QoreProgram::parseFileAndRunClass(const char* filename, const char* classname) {
    ExceptionSink xsink;

    parseFile(filename, &xsink);

    if (!xsink.isEvent())
        runClass(classname, &xsink);
}

void QoreProgram::parseAndRunClass(FILE* fp, const char* name, const char* classname) {
    ExceptionSink xsink;

    parse(fp, name, &xsink);

    if (!xsink.isEvent())
        runClass(classname, &xsink);
}

void QoreProgram::parseAndRunClass(const char* str, const char* name, const char* classname) {
    ExceptionSink xsink;

    parse(str, name, &xsink);

    if (!xsink.isEvent())
        runClass(classname, &xsink);
}

void QoreProgram::parseFileAndRun(const char* filename) {
    ExceptionSink xsink;

    parseFile(filename, &xsink);

    if (!xsink.isEvent()) {
        // get class name
        if (priv->exec_class) {
            if (!priv->exec_class_name.empty())
                runClass(priv->exec_class_name.c_str(), &xsink);
            else {
                char* c, *bn = q_basenameptr(filename);
                if (!(c = strrchr(bn, '.')))
                    runClass(filename, &xsink);
                else {
                    QoreString qcn; // for possible class name
                    qcn.concat(bn, c - bn);
                    runClass(qcn.getBuffer(), &xsink);
                }
            }
        } else
            run(&xsink);
    }
}

void QoreProgram::parseAndRun(FILE* fp, const char* name) {
    ExceptionSink xsink;

    if (priv->exec_class && priv->exec_class_name.empty())
        xsink.raiseException("EXEC-CLASS-ERROR", "class name required if executing from stdin");
    else {
        parse(fp, name, &xsink);

        if (!xsink.isEvent())
            run(&xsink);
    }
}

void QoreProgram::parseAndRun(const char* str, const char* name) {
    ExceptionSink xsink;

    if (priv->exec_class && priv->exec_class_name.empty())
        xsink.raiseException("EXEC-CLASS-ERROR", "class name required if executing from a direct string");
    else {
        parse(str, name, &xsink);

        if (!xsink.isEvent())
            run(&xsink);
    }
}

bool QoreProgram::checkFeature(const char* f) const {
    qore_program_private::strset_t::const_iterator i = priv->featureList.find(f);
    return (i != priv->featureList.end());
}

void QoreProgram::addFeature(const char* f) {
    priv->addFeature(f);
}

QoreListNode* QoreProgram::getFeatureList() const {
    return priv->getFeatureList();
}

QoreListNode* qore_program_ops::getVarListImpl(QoreProgram* b) {
    return b->priv->getVarList();
}

const char* qore_program_ops::parseGetScriptDirImpl(QoreProgram* b) {
    return b->priv->parseGetScriptDir();
}

QoreStringNode* QoreProgram::getScriptDir() const {
    return priv->getScriptDir();
}

QoreStringNode* QoreProgram::getScriptPath() const {
    return priv->getScriptPath();
}

QoreStringNode* QoreProgram::getScriptName() const {
    return priv->getScriptName();
}

void QoreProgram::setScriptPath(const char* path) {
    priv->setScriptPathExtern(path);
}

const LVList* qore_program_ops::getTopLevelLVListImpl(QoreProgram* b) {
    return b->priv->sb.getLVList();
}

QoreValue QoreProgram::getGlobalVariableVal(const char* var, bool& found) const {
    const qore_ns_private* vns = nullptr;
    Var* v = qore_root_ns_private::runtimeFindGlobalVar(*(priv->RootNS), var, vns);
    if (!v) {
        found = false;
        return QoreValue();
    }
    found = true;
    return v->eval();
}

QoreValue QoreProgram::getGlobalVariableValue(const char* var, bool& found) const {
    return getGlobalVariableVal(var, found);
}

// only called when parsing, therefore in the parse thread lock
void qore_program_ops::parseSetIncludePathImpl(QoreProgram* b, const char* path) {
    b->priv->include_path = path;
}

// only called when parsing, therefore in the parse thread lock
const char* qore_program_ops::parseGetIncludePathImpl(QoreProgram* b) {
    return b->priv->include_path.empty() ? nullptr : b->priv->include_path.c_str();
}

const AbstractQoreZoneInfo* QoreProgram::currentTZ() const {
    return priv->currentTZ();
}

void QoreProgram::setTZ(const AbstractQoreZoneInfo* n_TZ) {
    priv->setTZ(n_TZ);
}

bool qore_program_ops::parseExceptionRaisedImpl(QoreProgram* b) {
    return b->priv->parseExceptionRaised();
}

void QoreProgram::parseSetTimeZone(const char* zone) {
    return priv->parseSetTimeZone(zone);
}

QoreValue qore_parse_get_define_value(const QoreProgramLocation* loc, const char* str, QoreString& arg, bool& ok) {
    ok = true;
    char c = arg[0];
    // see if a string is being defined
    if (c == '"' || c == '\'') {
        // make sure the string is terminated in the same way
        char e = arg[arg.strlen() - 1];
        if (c != e || arg.strlen() == 1) {
            parse_error(*loc, "'%s' is defined with an unterminated string; %%define directives must be made on a single line", str);
            ok = false;
            return QoreValue();
        }

        // string is OK, remove quotes
        arg.trim_single_trailing(c);
        arg.trim_single_leading(c);
        size_t len = arg.strlen();
        const QoreEncoding* encoding = arg.getEncoding();
        return new QoreStringNode(arg.giveBuffer(), len, len + 1, encoding);
    }

    const char* p = arg.getBuffer();
    // check for 'true' and 'false'
    if (!strcasecmp(p, "true"))
        return true;
    if (!strcasecmp(p, "false"))
        return false;

    // an empty value is no value, as with %define without one (issue #6564)
    if (!*p) {
        return QoreValue();
    }

    // a number: an optional sign, decimal digits and an optional fraction; anything else, as text after the number
    // (1-2), is an error (issue #6564)
    const char* q = p;
    if (*q == '-' || *q == '+') {
        ++q;
    }
    bool digits = false;
    while (isdigit(static_cast<unsigned char>(*q))) {
        ++q;
        digits = true;
    }
    bool flt = false;
    if (*q == '.') {
        flt = true;
        ++q;
        while (isdigit(static_cast<unsigned char>(*q))) {
            ++q;
            digits = true;
        }
    }
    if (*q || !digits) {
        for (const char* a = p; *a; ++a) {
            if (isalpha(static_cast<unsigned char>(*a))) {
                parse_error(*loc, "'%s' has unquoted alphabetic characters in the value; use quotes (\" or ') to "
                    "define strings", str);
                ok = false;
                return QoreValue();
            }
        }
        parse_error(*loc, "'%s' is defined with an invalid number: '%s'", str, arg.getBuffer());
        ok = false;
        return QoreValue();
    }

    // a number is decimal, also with a leading zero (issue #6530); an integer stays an integer: the two results are
    // not given by one conditional expression, whose type would be double
    if (flt) {
        double d = q_strtod(p);
        if (std::isinf(d)) {
            parse_error(*loc, "'%s' is defined with a number that cannot be represented: '%s'", str, arg.getBuffer());
            ok = false;
            return QoreValue();
        }
        return d;
    }
    errno = 0;
    int64 i = strtoll(p, 0, 10);
    if (errno == ERANGE) {
        parse_error(*loc, "'%s' is defined with a number that cannot be represented: '%s'; integers must be in the "
            "range " QLLD " (MININT) to " QLLD " (MAXINT)", str, arg.getBuffer(), -LLONG_MAX, LLONG_MAX);
        ok = false;
        return QoreValue();
    }
    return i;
}

void QoreProgram::parseDefine(const char* str, QoreValue val) {
   priv->parseDefine(&qoreCommandLineLocation, str, val);
}

void QoreProgram::parseDefine(const char* str, const char* val) {
   priv->parseDefine(&qoreCommandLineLocation, str, new QoreStringNode(val));
}

void QoreProgram::parseCmdLineDefines(const std::map<std::string, std::string> defmap, ExceptionSink& xs, ExceptionSink& ws, int wm) {
   parseCmdLineDefines(xs, ws, wm, defmap);
}

void QoreProgram::parseCmdLineDefines(ExceptionSink& xs, ExceptionSink& ws, int wm, const std::map<std::string, std::string>& defmap) {
    // before the parse switches to the Program, as for any parse (see qore_program_private::parse())
    QoreCallerSandboxHelper caller_sandbox(this, &xs);
    if (xs) {
        return;
    }
    priv->markSandboxedParse();
    ProgramRuntimeParseCommitContextHelper pch(&xs, this);

    if (xs) {
        return;
    }

    if (priv->checkParse(&xs)) {
        return;
    }

    priv->startParsing(&xs, &ws, wm);

    for (std::map<std::string, std::string>::const_iterator it = defmap.begin(); it != defmap.end(); ++it) {
        const char *str = it->first.c_str();
        const char *val = it->second.c_str();
        QoreString arg(val);
        arg.trim();

        bool ok;
        QoreValue v = qore_parse_get_define_value(&qoreCommandLineLocation, str, arg, ok);
        if (!ok)
            break;
        priv->parseDefine(&qoreCommandLineLocation, str, v);
    }

    priv->parseSink = nullptr;
    priv->warnSink = nullptr;
}

QoreProgram* QoreProgram::programRefSelf() const {
   const_cast<QoreProgram*>(this)->ref();
   return const_cast<QoreProgram*>(this);
}

void QoreProgram::setExternalData(const char* owner, AbstractQoreProgramExternalData* pud) {
   priv->setExternalData(owner, pud);
}

AbstractQoreProgramExternalData* QoreProgram::getExternalData(const char* owner) const {
   return priv->getExternalData(owner);
}

AbstractQoreProgramExternalData* QoreProgram::removeExternalData(const char* owner) {
    // the removal waits until the copies of the data are made, so a copy() must not remove the data it copies
    assert(!ext_data_copy_frames || ext_data_copy_frames->parent != priv);
    return priv->removeExternalData(owner);
}

void QoreProgram::saveObject(QoreObject* obj) {
    priv->saveObject(obj);
}

QoreHashNode* QoreProgram::getGlobalVars() const {
   return priv->getGlobalVars();
}

int QoreProgram::setGlobalVarValue(const char* name, QoreValue val, ExceptionSink* xsink) {
   return priv->setGlobalVarValue(name, val, xsink);
}

QoreListNode* QoreProgram::getThreadList() const {
   ReferenceHolder<QoreListNode> rv(new QoreListNode(bigIntTypeInfo), nullptr);
   priv->getThreadList(**rv);
   return rv.release();
}

unsigned QoreProgram::getThreadCount() const {
   return priv->getThreadCount();
}

bool QoreProgram::waitForThreads(ExceptionSink* xsink, int64 timeout_ms, int tid) const {
    if (tid < 0) {
        xsink->raiseException("PROGRAM-THREAD-ERROR", "thread ID must be nonnegative");
        return false;
    }
    AutoLocker al(priv->plock);
    if ((!tid && (priv->tidmap.find(q_gettid()) != priv->tidmap.end() || priv->parse_tid == q_gettid()))
            || tid == q_gettid()) {
        xsink->raiseException("PROGRAM-THREAD-ERROR", "cannot wait for the calling thread's Program activity");
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    int64 max_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::time_point::max() - now).count();
    auto deadline = now + std::chrono::milliseconds(timeout_ms > 0 ? std::min(timeout_ms, max_ms) : 0);
    while (tid ? priv->tidmap.find(tid) != priv->tidmap.end() : (priv->thread_count || priv->parse_count)) {
        int64 remaining = -1;
        if (timeout_ms >= 0) {
            remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) {
                return false;
            }
        }
        ++priv->thread_waiting;
        int rc = remaining < 0 ? priv->pcond.waitWithInterrupt(&priv->plock, xsink)
            : priv->pcond.waitWithInterrupt(&priv->plock, remaining, xsink);
        --priv->thread_waiting;
        if (rc == QORE_COND_RESULT_INTERRUPTED) {
            return false;
        }
    }
    return true;
}

bool QoreProgram::cancelThread(int tid, const char* reason, ExceptionSink* xsink) const {
    if (tid <= 0 || tid == q_gettid()) {
        xsink->raiseException("PROGRAM-THREAD-ERROR", "cannot cancel invalid thread ID %d or the calling thread", tid);
        return false;
    }
    {
        AutoLocker al(priv->plock);
        if (priv->tidmap.find(tid) == priv->tidmap.end()) {
            return false;
        }
    }
    // Do not hold the Program lock while acquiring the thread registry lock. If the TID was
    // recycled, the cancellation checkpoint rejects the request outside this Program's scope.
    return !thread_list.cancelThread(tid, reason, getProgramId());
}

unsigned QoreProgram::cancelThreads(ExceptionSink* xsink, const char* reason) const {
    std::vector<int> tids;
    {
        AutoLocker al(priv->plock);
        if (priv->tidmap.find(q_gettid()) != priv->tidmap.end()) {
            xsink->raiseException("PROGRAM-THREAD-ERROR", "cannot cancel all threads from inside the Program");
            return 0;
        }
        unsigned checked = 0;
        for (auto& i : priv->tidmap) {
            if (!(checked++ % 100) && qore_check_cancel(xsink, "Program::cancelThreads()")) {
                return 0;
            }
            tids.push_back(i.first);
        }
        priv->cancel_new_threads = !!priv->thread_count;
    }
    unsigned count = 0;
    unsigned checked = 0;
    for (int tid : tids) {
        if (!(checked++ % 100) && qore_check_cancel(xsink, "Program::cancelThreads()")) {
            return count;
        }
        if (!thread_list.cancelThread(tid, reason, getProgramId())) {
            ++count;
        }
    }
    return count;
}

AbstractQoreProgramExternalData::~AbstractQoreProgramExternalData() {
}

void AbstractQoreProgramExternalData::init() {
}

AbstractQoreProgramExternalDataEx::~AbstractQoreProgramExternalDataEx() {
}

void AbstractQoreProgramExternalDataEx::init(ExceptionSink* xsink) {
}

AbstractQoreProgramExternalData* AbstractQoreProgramExternalDataEx::copy(QoreProgram* pgm) const {
    ExceptionSink xsink;
    AbstractQoreProgramExternalData* rv = qore_ext_data_copy(this, pgm, &xsink);
    // the destructor of the sink reports the exception as unhandled
    return rv;
}

void AbstractQoreProgramExternalDataEx::init() {
    ExceptionSink xsink;
    init(&xsink);
}

void AbstractQoreProgramExternalDataEx::doDeref() {
    ExceptionSink xsink;
    doDeref(&xsink);
}

namespace {
// converts a C++ exception that a legacy callback of AbstractQoreProgramExternalData let escape (issue #6786)
void ext_data_convert_std_exception(const char* func, const std::exception& e, ExceptionSink* xsink) {
    xsink->raiseException("PROGRAM-EXTERNAL-DATA-ERROR", "AbstractQoreProgramExternalData::%s() threw a C++ "
        "exception: %s", func, e.what());
}
}

AbstractQoreProgramExternalData* qore_ext_data_copy(const AbstractQoreProgramExternalData* data, QoreProgram* pgm,
        ExceptionSink* xsink) {
    assert(!*xsink);
    AbstractQoreProgramExternalData* rv;
    if (const AbstractQoreProgramExternalDataEx* ex = dynamic_cast<const AbstractQoreProgramExternalDataEx*>(data)) {
        rv = ex->copy(pgm, xsink);
    } else {
        try {
            rv = data->copy(pgm);
        } catch (AbstractException& e) {
            e.convert(xsink);
            return nullptr;
        } catch (std::exception& e) {
            ext_data_convert_std_exception("copy", e, xsink);
            return nullptr;
        }
    }
    // a copy returned together with an exception is released at once: the copy failed
    if (rv && *xsink) {
        qore_ext_data_deref(rv, xsink);
        return nullptr;
    }
    return rv;
}

void qore_ext_data_init(AbstractQoreProgramExternalData* data, ExceptionSink* xsink) {
    if (AbstractQoreProgramExternalDataEx* ex = dynamic_cast<AbstractQoreProgramExternalDataEx*>(data)) {
        ex->init(xsink);
        return;
    }
    try {
        data->init();
    } catch (AbstractException& e) {
        e.convert(xsink);
    } catch (std::exception& e) {
        ext_data_convert_std_exception("init", e, xsink);
    }
}

void qore_ext_data_deref(AbstractQoreProgramExternalData* data, ExceptionSink* xsink) {
    if (AbstractQoreProgramExternalDataEx* ex = dynamic_cast<AbstractQoreProgramExternalDataEx*>(data)) {
        ex->doDeref(xsink);
        return;
    }
    try {
        data->doDeref();
    } catch (AbstractException& e) {
        e.convert(xsink);
    } catch (std::exception& e) {
        ext_data_convert_std_exception("doDeref", e, xsink);
    }
}

int get_warning_code(const char* str) {
   for (unsigned i = 0; i < NUM_WARNINGS; i++)
      if (!strcasecmp(str, qore_warnings[i]))
         return 1 << i;
   return 0;
}

QoreDebugProgram::QoreDebugProgram(): priv(new qore_debug_program_private(this)) {};

QoreDebugProgram::~QoreDebugProgram() {
   printd(5, "QoreDebugProgram::~QoreDebugProgram() this: %p\n", this);
   delete priv;
}

void QoreDebugProgram::refForCallback() {
   ref();
}

void QoreDebugProgram::derefForCallback(ExceptionSink* xsink) {
   deref(xsink);
}

void QoreDebugProgram::onAttach(QoreProgram *pgm, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
    printd(5, "QoreDebugProgram::onAttach() this: %p\n", this);
    rs = DBG_RS_RUN;
}

void QoreDebugProgram::onDetach(QoreProgram *pgm, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
    printd(5, "QoreDebugProgram::onDetach() this: %p\n", this);
    rs = DBG_RS_DETACH;
}

void QoreDebugProgram::onStep(QoreProgram *pgm, const StatementBlock *blockStatement, const AbstractStatement *statement, unsigned bkptId, int &flow, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
}

void QoreDebugProgram::onFunctionEnter(QoreProgram *pgm, const StatementBlock *statement, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
}

void QoreDebugProgram::onFunctionExit(QoreProgram *pgm, const StatementBlock *statement, QoreValue& returnValue, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
}

void QoreDebugProgram::onException(QoreProgram *pgm, const AbstractStatement *statement, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
}

void QoreDebugProgram::onExit(QoreProgram *pgm, const StatementBlock *statement, QoreValue& returnValue, DebugRunStateEnum &rs, const AbstractStatement* &rts, ExceptionSink* xsink) {
}

void QoreDebugProgram::addProgram(QoreProgram *pgm, ExceptionSink* xsink) {
   printd(5, "QoreDebugProgram::addProgram(), this: %p, pgm: %p\n", this, pgm);
   priv->addProgram(pgm, xsink);
}

void QoreDebugProgram::removeProgram(QoreProgram *pgm) {
   printd(5, "QoreDebugProgram::removeProgram(), this: %p, pgm: %p\n", this, pgm);
   priv->removeProgram(pgm);
}

QoreListNode* QoreDebugProgram::getAllProgramObjects() {
   return priv->getAllProgramObjects();
}

bool QoreDebugProgram::attachThread(QoreProgram* pgm, int tid) const {
   return priv->attachThread(pgm, tid);
}

int QoreDebugProgram::breakProgramThread(QoreProgram *pgm, int tid) const {
   printd(5, "QoreDebugProgram::breakProgramThread(), this: %p, pgm: %p, tid: %d\n", this, pgm, tid);
   return priv->breakProgramThread(pgm, tid);
}

int QoreDebugProgram::setProgramTerminating(QoreProgram* pgm) const {
   printd(5, "QoreDebugProgram::setProgramTerminating(), this: %p, pgm: %p\n", this, pgm);
   return priv->setProgramTerminating(pgm);
}

int QoreDebugProgram::breakProgram(QoreProgram *pgm) const {
   printd(5, "QoreDebugProgram::breakProgram(), this: %p, pgm: %p\n", this, pgm);
   return priv->breakProgram(pgm);
}

void QoreDebugProgram::waitForTerminationAndClear(ExceptionSink* xsink) {
   printd(5, "QoreDebugProgram::waitForTerminationAndClear(), this: %p\n", this);
   priv->waitForTerminationAndClear(xsink);
}

int QoreDebugProgram::getInterruptedCount() {
   return priv->getInterruptedCount();
}

void QoreProgram::assignBreakpoint(QoreBreakpoint *bkpt, ExceptionSink *xsink) {
   priv->assignBreakpoint(bkpt, xsink);
}

void QoreProgram::deleteAllBreakpoints() {
   priv->deleteAllBreakpoints();
}

void QoreProgram::getBreakpoints(QoreBreakpointList_t &bkptList) {
   priv->getBreakpoints(bkptList);
}

void QoreProgram::getStatementBreakpoints(const AbstractStatement* statement, QoreBreakpointList_t &bkptList) {
   priv->getStatementBreakpoints(statement, bkptList);
}

QoreListNode* qore_program_private::findStatementIds(const char* name, int line, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(bigIntTypeInfo), xsink);
    const AbstractStatement* st = getStatementFromIndex(name, line);
    if (!st) {
        return rv.release();
    }
    auto same_str = [](const char* a, const char* b) -> bool {
        return a == b || (a && b && !strcmp(a, b));
    };
    // the start column and the ID of each statement found
    std::vector<std::pair<int, int64>> found;
    {
        const QoreProgramLocation* sloc = st->loc;
        AutoLocker al(&plock);
        for (size_t i = 0; i < statementIds.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "statement search")) {
                return nullptr;
            }
            const AbstractStatement* s = statementIds[i];
            if (!s || !s->loc) {
                continue;
            }
            const QoreProgramLocation* loc = s->loc;
            // another statement of the line is one with a column, as a block or a statement of AOT code has none
            if (s != st && (loc->start_column < 0 || loc->start_line != sloc->start_line
                    || loc->offset != sloc->offset || !same_str(loc->getFile(), sloc->getFile())
                    || !same_str(loc->getSource(), sloc->getSource()))) {
                continue;
            }
            found.emplace_back(loc->start_column, static_cast<int64>(i + 1));
        }
    }
    std::stable_sort(found.begin(), found.end(),
        [](const std::pair<int, int64>& a, const std::pair<int, int64>& b) { return a.first < b.first; });
    for (const auto& i : found) {
        rv->push(i.second, xsink);
        assert(!*xsink);
    }
    return rv.release();
}

namespace {
//! Collects the calls of the expressions of a statement that a debugger can step into, in the order in which they are
//! evaluated
class StepInTargetCollector {
public:
    StepInTargetCollector(qore_program_private& pgm, ExceptionSink* xsink) : pgm(pgm), xsink(xsink),
            rv(new QoreListNode(hashdeclStepInTargetInfo->getTypeInfo()), xsink) {
    }

    //! Adds the calls of a statement; its blocks are not searched
    void addStatement(const AbstractStatement* st) {
        if (auto* s = dynamic_cast<const ExpressionStatement*>(st)) {
            add(s->getExpression());
        } else if (auto* s = dynamic_cast<const ReturnStatement*>(st)) {
            add(s->getExpression());
        } else if (auto* s = dynamic_cast<const IfStatement*>(st)) {
            add(s->getCond());
        } else if (auto* s = dynamic_cast<const WhileStatement*>(st)) {
            // also a do-while statement
            add(s->getCond());
        } else if (auto* s = dynamic_cast<const ForStatement*>(st)) {
            add(s->getAssignment());
            add(s->getCond());
            add(s->getIterator());
        } else if (auto* s = dynamic_cast<const ForEachStatement*>(st)) {
            add(s->getList());
        } else if (auto* s = dynamic_cast<const SwitchStatement*>(st)) {
            add(s->getSwitchExp());
        } else if (auto* s = dynamic_cast<const ThrowStatement*>(st)) {
            add(s->getArgs());
        }
    }

    QoreListNode* release() {
        return *xsink ? nullptr : rv.release();
    }

private:
    //! the maximum depth of the expressions that are searched; deeper parts of an expression are not
    static constexpr int MaxDepth = 256;

    qore_program_private& pgm;
    ExceptionSink* xsink;
    ReferenceHolder<QoreListNode> rv;
    int depth = 0;

    void addArgs(const QoreParseListNode* parse_args, const QoreListNode* args) {
        if (parse_args) {
            for (size_t i = 0; i < parse_args->size(); ++i) {
                add(parse_args->get(i));
            }
        }
        if (args) {
            ConstListIterator li(args);
            while (li.next()) {
                add(li.getValue());
            }
        }
    }

    //! Adds a call
    /** @param variant the variant of the called code, if known at parse time; builtin code and the code of other
        Programs are not stepped into, so their calls are not added
        @param loc the location of the call
        @param cls the class of a method, if not given by its variant
        @param name the name of the called function or method, nullptr for the call of a call reference
        @param label the label of a call of a call reference
    */
    void addCall(const AbstractQoreFunctionVariant* variant, const QoreProgramLocation* loc,
            const QoreClass* cls, const char* name, const char* label = nullptr) {
        unsigned long sid = 0;
        if (variant) {
            const UserVariantBase* uvb = variant->getUserVariantBase();
            const StatementBlock* entry = uvb ? uvb->getEntryStatementBlock() : nullptr;
            if (!entry) {
                return;
            }
            // the called code of another Program has no statement ID here
            sid = pgm.getStatementId(entry);
            if (!sid) {
                return;
            }
            // the class of a method, also of a constructor
            if (!cls) {
                if (auto* mvb = dynamic_cast<const MethodVariantBase*>(variant)) {
                    cls = mvb->getClass();
                }
            }
        }
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclStepInTargetInfo, xsink), xsink);
        qore_hash_private* ph = qore_hash_private::get(**h);
        QoreStringNode* str = new QoreStringNode;
        if (name) {
            if (cls) {
                str->sprintf("%s::", cls->getName());
            }
            str->concat(name);
        } else {
            str->concat(label ? label : "<call>");
        }
        ph->setKeyValueIntern("name", str);
        ph->setKeyValueIntern("function", new QoreStringNode(name ? name : ""));
        ph->setKeyValueIntern("statement_id", static_cast<int64>(sid));
        if (loc) {
            ph->setKeyValueIntern("start_line", loc->start_line);
            ph->setKeyValueIntern("end_line", loc->end_line);
            ph->setKeyValueIntern("start_column", loc->start_column);
            ph->setKeyValueIntern("end_column", loc->end_column);
        }
        rv->push(h.release(), xsink);
    }

    void add(const QoreValue& val) {
        const AbstractQoreNode* node = val.getInternalNode();
        if (!node || val.isValue() || depth >= MaxDepth || *xsink) {
            return;
        }
        ++depth;
        addNode(node);
        --depth;
    }

    void addNode(const AbstractQoreNode* node) {
        // a closure is called later, if at all
        if (dynamic_cast<const QoreClosureParseNode*>(node)) {
            return;
        }
        // obj.method(args): the object is evaluated first, then the arguments
        if (auto* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
            add(dot->getExpression());
            const MethodCallNode* call = dot->getMethodCall();
            if (call) {
                addArgs(call->getParseArgs(), call->getArgs());
                addCall(call->getVariant(), dot->loc, call->getClass(), call->getName());
            }
            return;
        }
        if (auto* call = dynamic_cast<const ScopedObjectCallNode*>(node)) {
            addArgs(call->getParseArgs(), call->getArgs());
            addCall(call->getVariant(), call->loc, nullptr, "constructor");
            return;
        }
        // a declaration that creates an object: "C c(args)"
        if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
            if (vrn->isObjectConstruct()) {
                addArgs(vrn->getParseArgs(), vrn->getArgs());
                addCall(vrn->getVariant(), vrn->loc, QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo()),
                    "constructor");
            }
            return;
        }
        if (auto* call = dynamic_cast<const AbstractMethodCallNode*>(node)) {
            addArgs(call->getParseArgs(), call->getArgs());
            addCall(call->getVariant(), call->loc, call->getClass(), call->getName());
            return;
        }
        if (auto* call = dynamic_cast<const AbstractFunctionCallNode*>(node)) {
            addArgs(call->getParseArgs(), call->getArgs());
            // a function whose variant is chosen at runtime is stepped into only if it has user code
            if (!call->getVariant()) {
                auto* fc = qore_get_function_call_node(call);
                const QoreFunction* f = fc ? fc->getFunction() : nullptr;
                if (f && !f->hasUser()) {
                    return;
                }
            }
            addCall(call->getVariant(), call->loc, nullptr, call->getName());
            return;
        }
        // the called code is only known at runtime
        if (auto* call = dynamic_cast<const CallReferenceCallNode*>(node)) {
            add(call->getExp());
            addArgs(call->getParseArgs(), call->getArgs());
            const AbstractQoreNode* exp = call->getExp().getInternalNode();
            auto* var = dynamic_cast<const VarRefNode*>(exp);
            addCall(nullptr, call->loc, nullptr, nullptr, var ? var->getName() : nullptr);
            return;
        }
        if (auto* op = dynamic_cast<const QoreQuestionMarkOperatorNode*>(node)) {
            for (unsigned i = 0; i < 3; ++i) {
                add(op->get(i));
            }
            return;
        }
        // also the [] and the {} operators
        if (auto* op = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
            add(op->getLeft());
            add(op->getRight());
            return;
        }
        if (auto* op = dynamic_cast<const QoreBinaryOperatorNode<LValueOperatorNode>*>(node)) {
            add(op->getLeft());
            add(op->getRight());
            return;
        }
        if (auto* op = dynamic_cast<const QoreSingleExpressionOperatorNode<>*>(node)) {
            add(op->getExp());
            return;
        }
        if (auto* op = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(node)) {
            add(op->getExp());
            return;
        }
        if (auto* l = dynamic_cast<const QoreParseListNode*>(node)) {
            for (const QoreValue& v : l->getValues()) {
                add(v);
            }
            return;
        }
        if (auto* l = dynamic_cast<const QoreListNode*>(node)) {
            ConstListIterator li(l);
            while (li.next()) {
                add(li.getValue());
            }
            return;
        }
        if (auto* h = dynamic_cast<const QoreParseHashNode*>(node)) {
            const auto& keys = h->getKeys();
            const auto& values = h->getValues();
            for (size_t i = 0; i < keys.size(); ++i) {
                add(keys[i]);
                if (i < values.size()) {
                    add(values[i]);
                }
            }
            return;
        }
        // other expressions make no calls that are known at parse time, or their calls are not searched
    }
};
}

const StatementBlock* qore_program_private::getBlockFromIndex(const char* file, int line, int end_line) {
    if (!file) {
        return nullptr;
    }
    AutoLocker al(&plock);
    std::tuple<std::string, int, int> key(file, line, end_line);
    auto ci = blockIndexCache.find(key);
    if (ci != blockIndexCache.end()) {
        return ci->second;
    }
    const StatementBlock* rv = nullptr;
    int rv_lines = -1;
    for (const AbstractStatement* s : statementIds) {
        const StatementBlock* b = dynamic_cast<const StatementBlock*>(s);
        if (!b || !b->loc || b->loc->start_line + b->loc->offset != line || !b->loc->getFile()
            || strcmp(b->loc->getFile(), file)) {
            continue;
        }
        // the block with the given lines
        if (end_line >= 0 && b->loc->end_line + b->loc->offset == end_line) {
            rv = b;
            break;
        }
        // otherwise the innermost block, which has the fewest lines, as for the code of an AOT program, whose
        // statement index has one entry for each line that a statement starts at, as a block, so a block that starts
        // at the line of its first statement is the entry of that statement
        int lines = b->loc->end_line - b->loc->start_line;
        if (rv_lines < 0 || lines < rv_lines) {
            rv = b;
            rv_lines = lines;
        }
    }
    {
        // fork() waits for the change (see recordLock())
        std::lock_guard<std::mutex> rl(threadContainerLock());
        blockIndexCache[key] = rv;
    }
    return rv;
}

QoreListNode* qore_program_private::getStepInTargets(int64 statement_id, ExceptionSink* xsink) {
    const AbstractStatement* st = resolveStatementId(statement_id);
    if (!st) {
        xsink->raiseException("PROGRAM-STATEMENT-ERROR", "cannot resolve statement id from (" QLLD ")",
            statement_id);
        return nullptr;
    }
    StepInTargetCollector collector(*this, xsink);
    collector.addStatement(st);
    return collector.release();
}

AbstractStatement* QoreProgram::findStatement(const char* fileName, int line) const {
   return priv->getStatementFromIndex(fileName, line);
}

AbstractStatement* QoreProgram::findFunctionStatement(const char* functionName, const QoreListNode* params, ExceptionSink* xsink) const {
    const AbstractQoreFunctionVariant* uv = runtimeFindCall(functionName, params, xsink);
    if (!uv)
        return nullptr;
    const UserVariantBase* uvb = uv->getUserVariantBase();
    //printd(5, "QoreProgram::findFunctionStatement() '%s' -> %p\n", functionName, uvb);
    if (!uvb)
        return nullptr;
    return uvb->getEntryStatementBlock();
}

unsigned long QoreProgram::getStatementId(const AbstractStatement* statement) const {
   return priv->getStatementId(statement);
}

AbstractStatement* QoreProgram::resolveStatementId(unsigned long statementId) const {
   return priv->resolveStatementId(statementId);
}

QoreHashNode* QoreProgram::getSourceFileNames(ExceptionSink* xsink) const {
   return priv->getSourceFileNames(xsink);
}

QoreHashNode* QoreProgram::getSourceLabels(ExceptionSink* xsink) const {
   return priv->getSourceLabels(xsink);
}

qore_program_id_t QoreProgram::getProgramId() const {
   return priv->getProgramId();
}

QoreProgram* QoreProgram::resolveProgramId(qore_program_id_t programId) {
   return qore_program_private::resolveProgramId(programId);
}

void QoreProgram::registerQoreObject(QoreObject *o, ExceptionSink* xsink) const {
}

void QoreProgram::unregisterQoreObject(QoreObject *o, ExceptionSink* xsink) const {
}

QoreObject* QoreProgram::findQoreObject() const {
   return nullptr;
}

QoreObject* QoreProgram::getQoreObject(QoreProgram* pgm) {
    return qore_program_private::getProgramControl(pgm);
}

void qore_program_private::cleanupAllPrograms(ExceptionSink* xsink) {
    if (!qore_shutdown.load(std::memory_order_relaxed)) {
        return;
    }

    std::vector<QoreProgram*> programs;
    {
        QoreAutoRWReadLocker al(&lck_programMap);
        programs.reserve(programid_to_program_map.size());
        for (auto& i : programid_to_program_map) {
            // a registered Program has references; a reference is taken with the registry lock held, so that the
            // Program cannot be destroyed during the iteration
            i.second->ref();
            programs.push_back(i.second);
        }
    }

    for (QoreProgram* p : programs) {
        // waitForTerminationAndDeref does cleanup and one deref
        p->waitForTerminationAndDeref(xsink);
        // Deref for the extra reference we added
        p->deref(xsink);
    }
}

QoreListNode* QoreProgram::getAllQoreObjects(ExceptionSink* xsink) {
    return qore_program_private::getAllQoreObjects(xsink);
}

bool QoreProgram::checkAllowDebugging(ExceptionSink* xsink) {
   return priv->checkAllowDebugging(xsink);
}

void QoreProgram::setSandboxManager(QoreSandboxManager* sm) {
    // the C++ API sets a manager that governs the Program's code alone (see qore_program_private::own_manager_additive)
    priv->setOwnSandboxManager(sm, false, false);
}

const AbstractQoreFunctionVariant* QoreProgram::runtimeFindCall(const char* name, const QoreListNode* params, ExceptionSink* xsink) const {
   return priv->runtimeFindCall(name, params, xsink);
}

QoreListNode* QoreProgram::runtimeFindCallVariants(const char* name, ExceptionSink* xsink) const {
   return priv->runtimeFindCallVariants(name, xsink);
}

const QoreClass* QoreProgram::findClass(const char* cls_path, ExceptionSink* xsink) const {
    return priv->runtimeFindClass(cls_path, xsink);
}

class_vec_t QoreProgram::findAllClassesRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    return qore_root_ns_private::get(*priv->RootNS)->runtimeFindAllClassesRegex(pattern, re_opts, xsink);
}

hashdecl_vec_t QoreProgram::findAllHashDeclsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    return qore_root_ns_private::get(*priv->RootNS)->runtimeFindAllHashDeclsRegex(pattern, re_opts, xsink);
}

func_vec_t QoreProgram::findAllFunctionsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    return qore_root_ns_private::get(*priv->RootNS)->runtimeFindAllFunctionsRegex(pattern, re_opts, xsink);
}

ns_vec_t QoreProgram::findAllNamespacesRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    return qore_root_ns_private::get(*priv->RootNS)->runtimeFindAllNamespacesRegex(pattern, re_opts, xsink);
}

gvar_vec_t QoreProgram::findAllGlobalVarsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    return qore_root_ns_private::get(*priv->RootNS)->runtimeFindAllGlobalVarsRegex(pattern, re_opts, xsink);
}

const_vec_t QoreProgram::findAllNamespaceConstantsRegex(const QoreString& pattern, int re_opts, ExceptionSink* xsink) const {
    return qore_root_ns_private::get(*priv->RootNS)->runtimeFindAllNamespaceConstantsRegex(pattern, re_opts, xsink);
}

const QoreExternalFunction* QoreProgram::findFunction(const char* path) const {
    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(*priv->RootNS, path);
    return reinterpret_cast<const QoreExternalFunction*>(fe ? fe->getFunction() : nullptr);
}

const QoreExternalFunction* QoreProgram::findFunction(const char* path, const QoreNamespace*& pns) const {
    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(*priv->RootNS, path);
    if (!fe) {
        pns = nullptr;
        return nullptr;
    }
    pns = fe->getNamespace()->ns;
    return reinterpret_cast<const QoreExternalFunction*>(fe->getFunction());
}

func_ns_vec_t QoreProgram::findAllFunctionsRegexNs(const QoreString& pattern, int re_opts,
        ExceptionSink* xsink) const {
    return qore_root_ns_private::get(*priv->RootNS)->runtimeFindAllFunctionsRegexNs(pattern, re_opts, xsink);
}

template <bool Native>
const TypedHashDecl* qore_program_private::findHashDeclImpl(const char* path, const QoreNamespace*& pns) const {
    pns = nullptr;

    if (parsing_in_progress || parse_commit_in_progress) {
        TypedHashDecl* hd = qore_root_ns_private::parseTryFindHashDecl(*RootNS, path);
        if (hd) {
            pns = hd->getNamespace();
            return hd;
        }
    }

    const qore_ns_private* pns_priv;
    const TypedHashDecl* th;
    if constexpr (Native) {
        th = qore_root_ns_private::runtimeFindHashDecl(*RootNS, path, pns_priv);
    } else {
        th = qore_root_ns_private::runtimeFindHashDecl(*RootNS, path, pns_priv, LexicalSemanticPolicy{});
    }
    if (th) {
        pns = pns_priv->ns;
    }
    return th;
}

const TypedHashDecl* QoreProgram::findHashDecl(const char* path, const QoreNamespace*& pns) const {
    return priv->findHashDeclImpl<true>(path, pns);
}

const TypedHashDecl* qore_program_private::findHashDeclCanonical(const char* path, const QoreNamespace*& pns) const {
    return findHashDeclImpl<false>(path, pns);
}

template <bool Native>
const QoreEnumDecl* qore_program_private::findEnumImpl(const char* path, const QoreNamespace*& pns) const {
    pns = nullptr;

    if (parsing_in_progress || parse_commit_in_progress) {
        const QoreEnumDecl* e = qore_root_ns_private::parseTryFindEnum(*RootNS, path);
        if (e) {
            pns = e->getNamespace();
            return e;
        }
    }

    const qore_ns_private* pns_priv;
    const QoreEnumDecl* e;
    if constexpr (Native) {
        e = qore_root_ns_private::runtimeFindEnum(*RootNS, path, pns_priv);
    } else {
        e = qore_root_ns_private::runtimeFindEnum(*RootNS, path, pns_priv, LexicalSemanticPolicy{});
    }
    if (e) {
        pns = pns_priv->ns;
    }
    return e;
}

const QoreEnumDecl* QoreProgram::findEnum(const char* path, const QoreNamespace*& pns) const {
    return priv->findEnumImpl<true>(path, pns);
}

const QoreEnumDecl* qore_program_private::findEnumCanonical(const char* path, const QoreNamespace*& pns) const {
    return findEnumImpl<false>(path, pns);
}

// issue #1796: include a non-const variant for binary modules
QoreNamespace* QoreProgram::findNamespace(const QoreString& path) {
    if (path == "::") {
        return priv->RootNS;
    }
    return qore_root_ns_private::get(*priv->RootNS)->runtimeFindNamespace(path);
}

const QoreNamespace* QoreProgram::findNamespace(const QoreString& path) const {
    if (path == "::") {
        return priv->RootNS;
    }
    return qore_root_ns_private::get(*priv->RootNS)->runtimeFindNamespace(path);
}

const QoreExternalGlobalVar* QoreProgram::findGlobalVar(const char* path, const QoreNamespace*& pns) const {
    const qore_ns_private* pns_priv;
    Var* var = qore_root_ns_private::runtimeFindGlobalVar(*priv->RootNS, path, pns_priv);
    if (var) {
        pns = pns_priv->ns;
    }
    return reinterpret_cast<const QoreExternalGlobalVar*>(var);
}

const QoreExternalConstant* QoreProgram::findNamespaceConstant(const char* path, const QoreNamespace*& pns) const {
    const qore_ns_private* pns_priv;
    const ConstantEntry* con = qore_root_ns_private::runtimeFindNamespaceConstant(*priv->RootNS, path, pns_priv);
    if (con) {
        pns = pns_priv->ns;
    }
    return reinterpret_cast<const QoreExternalConstant*>(con);
}

int QoreProgram::issueModuleCmd(const char* module, const char* cmd, ExceptionSink* xsink) {
    QMM.issueRuntimeCmd(module, this, cmd, xsink);
    return *xsink ? -1 : 0;
}

QoreRWLock QoreBreakpoint::lck_breakpoint;
// the breakpoint list can be used in a child process created with fork() (see QoreForkHandlers.h)
QoreRWLock& qore_program_private::getBreakpointLockForFork() {
    return QoreBreakpoint::lck_breakpoint;
}

static QoreForkRWLock breakpoint_fork_handler(qore_program_private::getBreakpointLockForFork(), QFLR_LEAF,
    "breakpoints");
bkp_list_t QoreBreakpoint::breakpointList;
unsigned QoreBreakpoint::breakpointIdCounter = 1;

void qore_breakpoint_ops::unassignAllStatementsImpl(QoreBreakpoint* b) {
    for (std::list<AbstractStatement*>::iterator it = b->statementList.begin(); it != b->statementList.end(); ++it) {
        (*it)->unassignBreakpoint(b);
    }
    b->statementList.clear();
}

QoreBreakpoint& QoreBreakpoint::operator=(const QoreBreakpoint& other) {
    qore_program_private* other_pgm = other.getPgm();
    QoreAutoRWReadLocker al(other_pgm ? &other_pgm->lck_breakpoint : nullptr);
    enabled = other.enabled;
    policy = other.policy;
    pgm = other_pgm;
    statementList = other.statementList;
    tidMap = other.tidMap;

    return *this;
}

QoreBreakpoint::QoreBreakpoint(): pgm(nullptr), enabled(false), policy(BKP_PO_NONE) {
    QoreAutoRWWriteLocker al(&QoreBreakpoint::lck_breakpoint);
    QoreBreakpoint::breakpointList.push_back(this);
    breakpointId = breakpointIdCounter++;
}

QoreBreakpoint::QoreBreakpoint(const QoreBreakpoint& other) : QoreBreakpoint() {
    // a new breakpoint with its own ID, which is not assigned to a Program or to statements
    qore_program_private* other_pgm = other.getPgm();
    QoreAutoRWReadLocker al(other_pgm ? &other_pgm->lck_breakpoint : nullptr);
    enabled = other.enabled;
    policy = other.policy;
    tidMap = other.tidMap;
}

void QoreBreakpoint::deref(ExceptionSink* xsink) {
    deref();
}

void QoreBreakpoint::deref() {
    // a reference that is not the last one is released without the lock of the list of all breakpoints
    int r = references.load(std::memory_order_relaxed);
    while (r > 1) {
        if (references.compare_exchange_weak(r, r - 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
            return;
        }
    }
    // the last reference: the breakpoint leaves the list as its count reaches zero, with the lock held, where
    // getReferencedBreakpoint() takes its references, so the list never holds a breakpoint without references
    {
        QoreAutoRWWriteLocker al(&lck_breakpoint);
        if (!ROdereference()) {
            return;
        }
        breakpointList.remove(this);
    }
    delete this;
}

QoreBreakpoint::~QoreBreakpoint() {
    // the breakpoint left the list of all breakpoints with its last reference (see deref())
    if (qore_program_private* p = getPgm()) {
        QoreAutoRWWriteLocker al(&p->lck_breakpoint);
        // fork() waits for the change (see qore_program_private_base::recordLock())
        std::lock_guard<std::mutex> rl(p->threadContainerLock());
        p->breakpointList.remove(this);
        qore_breakpoint_ops(this).unassignAllStatements();
    } else {
        qore_breakpoint_ops(this).unassignAllStatements();
    }
}

// lck_breakpoint lock should be acquired
bool qore_breakpoint_ops::isStatementAssignedImpl(QoreBreakpoint* b, const AbstractStatement *statement) {
    return std::find(b->statementList.begin(), b->statementList.end(), statement) != b->statementList.end();
}

bool QoreBreakpoint::checkBreak() const {
    if (!enabled)
        return false;
    switch (policy) {
    case BKP_PO_NONE:
        return true;
    case BKP_PO_ACCEPT:
        return tidMap.find(q_gettid()) != tidMap.end();
    case BKP_PO_REJECT:
        return tidMap.find(q_gettid()) == tidMap.end();
    default:
        return false;
    }
}

bool qore_breakpoint_ops::checkPgmImpl(QoreBreakpoint* b, ExceptionSink* xsink) {
    if (b->getPgm()) {
        return true;
    } else {
        if (xsink) {
            xsink->raiseException("BREAKPOINT-ERROR", "QoreProgram is not assigned");
        }
        return false;
    }
}

void QoreBreakpoint::assignProgram(QoreProgram *new_pgm, ExceptionSink* xsink) {
    if (new_pgm) {
        new_pgm->assignBreakpoint(this, xsink);
    } else {
        if (qore_program_private* p = getPgm()) {
            QoreAutoRWWriteLocker al(&p->lck_breakpoint);
            {
                // fork() waits for the change (see qore_program_private_base::recordLock()); not held while the
                // breakpoint is released, which takes the lock of all breakpoints
                std::lock_guard<std::mutex> rl(p->threadContainerLock());
                p->breakpointList.remove(this);
                qore_breakpoint_ops(this).unassignAllStatements();
            }
            pgm = nullptr;
            deref();
        }
    }
}

QoreProgram* QoreBreakpoint::getProgram() const {
    qore_program_private* p = getPgm();
    return p ? p->pgm : nullptr;
}

QoreProgram* qore_breakpoint_ops::getReferencedProgramImpl(QoreBreakpoint* b) {
    // the Program of the breakpoint is read with the registry lock held: a Program unassigns its breakpoints when its
    // last reference is released and when it is destroyed, before it takes the registry lock to leave the set of
    // instances, so a Program read here exists until the lock is released, and it has references if it is still
    // registered
    QoreAutoRWReadLocker al(&qore_program_private::lck_programMap);
    qore_program_private* p = b->getPgm();
    if (!p || !qore_program_private::isRegisteredIntern(p->pgm)) {
        return nullptr;
    }
    p->pgm->ref();
    return p->pgm;
}

void QoreBreakpoint::assignStatement(AbstractStatement* statement, ExceptionSink* xsink) {
    if (qore_breakpoint_ops(this).checkPgm(xsink)) {
        QoreAutoRWWriteLocker al(&getPgm()->lck_breakpoint);
        if (getPgm()->isBreakpointRegistered(this)) {
            if (statement && !qore_breakpoint_ops(this).isStatementAssigned(statement)) {
                // fork() waits for the change (see qore_program_private_base::recordLock())
                std::lock_guard<std::mutex> rl(getPgm()->threadContainerLock());
                statementList.push_back(statement);
                statement->assignBreakpoint(this);
            }
        }
    }
}

void QoreBreakpoint::unassignStatement(AbstractStatement* statement, ExceptionSink* xsink) {
    if (qore_breakpoint_ops(this).checkPgm(xsink)) {
        QoreAutoRWWriteLocker al(&getPgm()->lck_breakpoint);
        if (getPgm()->isBreakpointRegistered(this)) {
            if (statement && qore_breakpoint_ops(this).isStatementAssigned(statement)) {
                std::lock_guard<std::mutex> rl(getPgm()->threadContainerLock());
                statementList.remove(statement);
                statement->unassignBreakpoint(this);
            }
        }
    }
}

void QoreBreakpoint::getStatements(AbstractStatementList_t &statList, ExceptionSink* xsink) {
    statList.clear();
    QoreAutoRWReadLocker al(getPgm() ? &getPgm()->lck_breakpoint : nullptr);
    for (AbstractStatementList_t::iterator it = statementList.begin(); it != statementList.end(); ++it) {
        statList.push_back(*it);
    }
}

QoreListNode* QoreBreakpoint::getStatementIds(ExceptionSink* xsink) {
    QoreAutoRWReadLocker al(getPgm() ? &getPgm()->lck_breakpoint : nullptr);
    ReferenceHolder<QoreListNode> l(new QoreListNode(bigIntTypeInfo), xsink);
    for (AbstractStatementList_t::iterator it = statementList.begin(); it != statementList.end(); ++it) {
        (*l)->push(getPgm()->getStatementId(*it), nullptr);
    }
    return l.release();
}

AbstractStatement* QoreBreakpoint::resolveStatementId(unsigned long statementId, ExceptionSink* xsink) const {
    AbstractStatement *s = nullptr;
    if (qore_breakpoint_ops(this).checkPgm(xsink)) {
        s = getPgm()->resolveStatementId(statementId);
        if (!s) {
            if (xsink) {
                xsink->raiseException("BREAKPOINT-ERROR", "Cannot resolve statement (%lu)", statementId);
            }
        }
    }
    return s;
}

void QoreBreakpoint::getThreadIds(TidList_t &tidList, ExceptionSink* xsink) {
    tidList.clear();
    QoreAutoRWReadLocker al(getPgm() ? &getPgm()->lck_breakpoint : nullptr);
    for (TidMap_t::iterator it = tidMap.begin(); it != tidMap.end(); ++it) {
        for (int j = 0; j < it->second; j++) {
            tidList.push_back(it->first+j);
        }
    }
}

void QoreBreakpoint::setThreadIds(TidList_t tidList, ExceptionSink* xsink) {
    QoreAutoRWWriteLocker al(getPgm() ? &getPgm()->lck_breakpoint : nullptr);
    tidList.sort();  // or create copy not to sort argument list ?
    // the thread IDs of a breakpoint are changed under the stripe lock of the breakpoint (see
    // qore_program_private_base::recordLock())
    std::lock_guard<std::mutex> rl(qore_program_private_base::recordLock(this));
    tidMap.clear();
    TidList_t::iterator it = tidList.begin();
    while (it != tidList.end()) {
        int tid = *it;
        int cnt = 1;
        ++it;
        while (it != tidList.end()) {
            if (tid + cnt != *it) break;
            ++it;
            cnt++;
        }
        //printd(5, "QoreBreakpoint::setThreadIds(), tid: %d, cnt: %d\n", tid, cnt);
        tidMap.insert(TidMap_t::value_type(tid, cnt));
    }
}

void QoreBreakpoint::addThreadId(int tid, ExceptionSink* xsink) {
    QoreAutoRWWriteLocker al(getPgm() ? &getPgm()->lck_breakpoint : nullptr);
    // see setThreadIds()
    std::lock_guard<std::mutex> rl(qore_program_private_base::recordLock(this));
    if (!tidMap.empty()) {
        TidMap_t::iterator it2 = tidMap.upper_bound(tid);
        if (it2 != tidMap.begin()) {
            TidMap_t::iterator it = it2;
            --it;
            if ((tid >= it->first && tid < it->first + it->second) ) {
                // already exists
                return;
            }
            if (tid == it->first + it->second) {
                // extend existing range
                it->second++;
                if (it2 != tidMap.end() && tid == it2->first - 1) {
                    // join
                    it->second += it2->second;
                    tidMap.erase(it2);
                }
                return;
            }
        }
        if (it2 != tidMap.end() && tid == it2->first - 1) {
            // extend lower bound
            tidMap.insert(TidMap_t::value_type(it2->first - 1, it2->second + 1));
            tidMap.erase(it2);
            return;
        }
    }
    tidMap.insert(TidMap_t::value_type(tid, 1));
}

void QoreBreakpoint::removeThreadId(int tid, ExceptionSink* xsink) {
    QoreAutoRWWriteLocker al(getPgm() ? &getPgm()->lck_breakpoint : nullptr);
    // see setThreadIds()
    std::lock_guard<std::mutex> rl(qore_program_private_base::recordLock(this));
    if (!tidMap.empty()) {
        TidMap_t::iterator it = tidMap.upper_bound(tid);
        if (it != tidMap.begin()) {
            --it;  // we can iterate from end() as well
            if (tid >= it->first && tid < it->first + it->second) {
                if (tid == it->first) {
                    tidMap.insert(TidMap_t::value_type(it->first + 1, it->second - 1));
                    tidMap.erase(it);
                } else if (tid == it->first + it->second - 1) {
                    it->second--;
                } else {
                    tidMap.insert(TidMap_t::value_type(tid + 1, it->first + it->second - tid - 1));
                    it->second = tid - it->first;
                }
            }
        }
    }
}

bool QoreBreakpoint::isThreadId(int tid, ExceptionSink* xsink) {
    QoreAutoRWReadLocker al(getPgm() ? &getPgm()->lck_breakpoint : nullptr);
    //printd(5, "QoreBreakpoint::isThreadId(%d), tidMap.empty()==%d\n", tid, tidMap.empty());
    if (!tidMap.empty()) {
        TidMap_t::iterator it = tidMap.upper_bound(tid);
        if (it != tidMap.begin()) {
            --it;  // we can iterate from end() as well
            //printd(5, "QoreBreakpoint::isThreadId(%d), it.tid: %d, it.cnt: %d\n", tid, it->first, it->second);
            return (tid >= it->first && tid < it->first + it->second);
        }
        //printd(5, "QoreBreakpoint::isThreadId(%d), it@begin()\n", tid);
    }
    return false;
}

void QoreBreakpoint::clearThreadIds(ExceptionSink* xsink) {
    QoreAutoRWWriteLocker al(getPgm() ? &getPgm()->lck_breakpoint : nullptr);
    // see setThreadIds()
    std::lock_guard<std::mutex> rl(qore_program_private_base::recordLock(this));
    tidMap.clear();
}

unsigned QoreBreakpoint::getBreakpointId() const {
    return breakpointId;
}

QoreBreakpoint* qore_breakpoint_ops::getReferencedBreakpoint(unsigned breakpointId) {
    QoreAutoRWReadLocker al(&QoreBreakpoint::lck_breakpoint);
    for (QoreBreakpoint* b : QoreBreakpoint::breakpointList) {
        if (b->breakpointId == breakpointId) {
            b->ref();
            return b;
        }
    }
    return nullptr;
}

QoreBreakpoint* QoreBreakpoint::resolveBreakpointId(unsigned breakpointId) {
    QoreAutoRWReadLocker al(&QoreBreakpoint::lck_breakpoint);
    for (bkp_list_t::const_iterator i = QoreBreakpoint::breakpointList.begin();
            i != QoreBreakpoint::breakpointList.end(); i++) {
        if ((*i)->breakpointId == breakpointId)
            return *i;
    }
    return nullptr;
}

QoreObject* QoreBreakpoint::getQoreObject() {
    // the object holds its own reference; objects are not cached, as a cached object could be found after its last
    // reference had been released and while it was being destroyed
    ref();
    return new QoreObject(QC_BREAKPOINT, ::getProgram(), this);
}

void QoreBreakpoint::setQoreObject(QoreObject *n_qo) {
}

QoreExternalProgramContextHelper::QoreExternalProgramContextHelper(ExceptionSink* xsink, QoreProgram* pgm) {
    // a C++ exception of the entry propagates to the C++ code that enters the Program, once the thread is as it was
    // (issue #6198); the interpreter's entries raise it as a Qore exception instead (issue #6255)
    std::unique_ptr<ProgramThreadCountContextHelper> p(new ProgramThreadCountContextHelper);
    p->setRethrow(xsink, pgm, true);
    priv = p.release();
}

QoreExternalProgramContextHelper::~QoreExternalProgramContextHelper() {
    delete priv;
}

bool qore_program_thread_entry_pending(const QoreProgram* pgm) {
    return pgm && thread_entry_pending(pgm);
}

class QoreExternalProgramCallContextHelperPriv : ProgramCallContextHelper {
public:
    DLLLOCAL QoreExternalProgramCallContextHelperPriv(QoreProgram* pgm) : ProgramCallContextHelper(pgm) {
    }
};

QoreExternalProgramCallContextHelper::QoreExternalProgramCallContextHelper(QoreProgram* pgm) : priv(new QoreExternalProgramCallContextHelperPriv(pgm)) {
}

QoreExternalProgramCallContextHelper::~QoreExternalProgramCallContextHelper() {
    delete priv;
}

// exit hooks: code that the library calls before it waits for the threads of the process, or of a Program being
// deleted, to end; see qore_register_exit_hook()
namespace {
struct QoreExitHook {
    //! the hook's ID, which also orders the hooks: they run in the reverse order of their registration
    int64 id;
    //! the Program whose code registered the hook, with a dependency reference (QoreProgram::depRef())
    /** The reference keeps the Program's memory while the hook is taken out of the registry to run; it does not keep
        the Program from being deleted, which runs the Program's hooks that are still registered
    */
    QoreProgram* pgm;
    //! the hook; referenced
    ResolvedCallReferenceNode* code;
    //! the run-time parse options of the code that registered the hook, which the hook runs with (qore#6208)
    /** A hook that is a call reference to builtin code sets no options of its own, and the thread that runs the hooks
        (the one that deletes the Program, or the main thread at the end of the process) has its own
    */
    QoreThreadRuntimeParseOptions po;

    //! Releases the hook's references; called without the registry lock
    void release(ExceptionSink* xsink) {
        // the options' references to Programs are released here, not when the object is overwritten or destroyed
        // with the registry lock held (qore#6208)
        po = QoreThreadRuntimeParseOptions();
        code->deref(xsink);
        pgm->depDeref();
    }
};

//! guards the hooks; never held while a hook runs or a reference is released
std::mutex exit_hook_lock;
//! the hooks registered, in registration order
std::vector<QoreExitHook> exit_hooks;
//! the last hook ID assigned
int64 exit_hook_seq = 0;
//! set when the hooks of the process have started to run: no hook can be registered afterwards
bool exit_hooks_closed = false;
//! the system threads (qore_start_system_thread()) being started outside the lock
/** The hooks do not start to run while one is being started, so that the hook that stops it runs after it exists
*/
int system_threads_starting = 0;
//! signalled when the last system thread being started has been started
std::condition_variable exit_hook_cond;

//! The fork handler of the exit hook registry, a leaf lock: its holders take no other registered lock
/** In the child, the threads that were starting system threads are gone with their reservations
*/
class QoreExitHookForkHandler : public QoreForkHandler {
public:
    QoreExitHookForkHandler() : QoreForkHandler(QFLR_LEAF, "exit_hooks") {
    }

    void prepare() noexcept override {
        exit_hook_lock.lock();
    }

    void parent() noexcept override {
        exit_hook_lock.unlock();
    }

    void child() noexcept override {
        // the condition is created again in place: the waiters are gone with their threads; the mutex is held by this
        // thread since prepare(): released, so that ThreadSanitizer sees the release (issue #6817)
        new (&exit_hook_cond) std::condition_variable();
        exit_hook_lock.unlock();
        system_threads_starting = 0;
    }
};

QoreExitHookForkHandler exit_hook_fork_handler;

//! Closes the registry for the process, or for a Program, and waits for the system threads being started
void qore_close_exit_hooks(QoreProgram* pgm) {
    std::unique_lock<std::mutex> l(exit_hook_lock);
    if (!pgm) {
        exit_hooks_closed = true;
    } else {
        // the Program's hooks start: its code can register no more
        qore_program_private::get(*pgm)->exit_hooks_closed = true;
    }
    exit_hook_cond.wait(l, [] () { return !system_threads_starting; });
}

//! Takes the last hook registered of a Program, or of any Program, out of the registry
/** @return true if a hook was taken
*/
bool qore_take_last_exit_hook(QoreProgram* pgm, QoreExitHook& hook) {
    std::lock_guard<std::mutex> l(exit_hook_lock);
    for (auto i = exit_hooks.rbegin(), e = exit_hooks.rend(); i != e; ++i) {
        if (!pgm || i->pgm == pgm) {
            hook = *i;
            exit_hooks.erase(std::next(i).base());
            return true;
        }
    }
    return false;
}

//! Runs a hook taken from the registry and releases it
void qore_run_exit_hook(QoreExitHook& hook) {
    ExceptionSink xsink;
    {
        // the hook runs in its Program, without the policy of a sandboxed caller that deletes a Program; a Program
        // that is being deleted meanwhile refuses it with an exception
        // the hook runs with the options of the code that registered it (qore#6208), installed before the Program
        // is entered, whose thread entry handler and initialization run then
        QoreThreadRuntimeParseOptionsHelper po_helper(&hook.po);
        ProgramThreadCountContextHelper tch(&xsink, hook.pgm, true);
        if (!xsink) {
            QoreSandboxPolicyBarrierHelper barrier;
            hook.code->invoke(nullptr, &xsink).discard(&xsink);
        }
    }
    hook.release(&xsink);
    // an exception is reported, and the next hook runs
    xsink.handleExceptions();
}
}

int64 qore_register_exit_hook(const ResolvedCallReferenceNode* code, ExceptionSink* xsink) {
    // a hook runs, at the end of the process, with its Program's access and without a sandbox: the exit hooks are
    // process-wide, so code that is sandboxed or acts for sandboxed code cannot register one, also through a policy
    // barrier (as qore_check_process_wide_registration() decides, with the exception documented for this API)
    if (qore_caller_is_sandboxed()) {
        xsink->raiseException("EXIT-HOOK-ERROR", "an exit hook cannot be registered by code that is sandboxed or acts "
            "for sandboxed code: the exit hooks are process-wide");
        return -1;
    }
    QoreProgram* pgm = getProgram();
    assert(pgm);
    std::lock_guard<std::mutex> l(exit_hook_lock);
    if (exit_hooks_closed) {
        xsink->raiseException("EXIT-HOOK-ERROR", "an exit hook cannot be registered after the exit hooks have run");
        return -1;
    }
    if (qore_program_private::get(*pgm)->exit_hooks_closed) {
        // it would never run: the Program's hooks have run, or are running, because it is being deleted
        xsink->raiseException("EXIT-HOOK-ERROR", "an exit hook cannot be registered by the code of a Program whose "
            "exit hooks have run");
        return -1;
    }
    int64 id = ++exit_hook_seq;
    // taking the references cannot fail, and releasing them takes no lock
    pgm->depRef();
    exit_hooks.push_back({id, pgm, code->refRefSelf(), QoreThreadRuntimeParseOptions::capture()});
    return id;
}

bool qore_deregister_exit_hook(int64 id, ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();
    QoreExitHook hook;
    bool found = false;
    {
        std::lock_guard<std::mutex> l(exit_hook_lock);
        for (auto i = exit_hooks.begin(), e = exit_hooks.end(); i != e; ++i) {
            // a Program removes only its own hooks
            if (i->id == id && i->pgm == pgm) {
                hook = *i;
                exit_hooks.erase(i);
                found = true;
                break;
            }
        }
    }
    if (!found) {
        return false;
    }
    hook.release(xsink);
    return true;
}

void qore_run_exit_hooks(QoreProgram* pgm) {
    // the hooks are taken one at a time, so that a hook that deletes a Program, or a Program deleted meanwhile by
    // another thread, runs that Program's hooks still registered when it is deleted, and none is run after it
    qore_close_exit_hooks(pgm);
    QoreExitHook hook;
    if (!qore_take_last_exit_hook(pgm, hook)) {
        return;
    }
    // a hook runs in a Qore thread
    QoreForeignThreadHelper qfth;
    do {
        qore_run_exit_hook(hook);
    } while (qore_take_last_exit_hook(pgm, hook));
}

bool qore_exit_hooks_started() {
    std::lock_guard<std::mutex> l(exit_hook_lock);
    return exit_hooks_closed;
}

namespace {
//! The argument of a thread started by qore_start_system_thread()
struct QoreSystemThreadArg {
    //! the callback; referenced
    ResolvedCallReferenceNode* code;
    //! the Program the callback runs in; with a dependency reference
    QoreProgram* pgm;

    //! takes the references; cannot throw
    QoreSystemThreadArg(const ResolvedCallReferenceNode* code, QoreProgram* pgm) noexcept
            : code(code->refRefSelf()), pgm(pgm) {
        pgm->depRef();
    }

    void release(ExceptionSink* xsink) {
        code->deref(xsink);
        pgm->depDeref();
    }
};

//! Releases the argument of a system thread that did not start, also when the start throws
class QoreSystemThreadArgHolder {
public:
    QoreSystemThreadArgHolder(QoreSystemThreadArg* arg, ExceptionSink* xsink) : arg(arg), xsink(xsink) {
    }

    ~QoreSystemThreadArgHolder() {
        if (arg) {
            arg->release(xsink);
            delete arg;
        }
    }

    //! the thread owns the argument
    void disown() {
        arg = nullptr;
    }

    QoreSystemThreadArg* get() const {
        return arg;
    }

private:
    QoreSystemThreadArg* arg;
    ExceptionSink* xsink;
};

//! Releases a system thread start reserved in qore_start_system_thread() however the start ends
class QoreSystemThreadStartReservation {
public:
    ~QoreSystemThreadStartReservation() {
        std::lock_guard<std::mutex> l(exit_hook_lock);
        if (!--system_threads_starting) {
            exit_hook_cond.notify_all();
        }
    }
};

void qore_system_thread_main(ExceptionSink* xsink, void* arg) {
    std::unique_ptr<QoreSystemThreadArg> sta(static_cast<QoreSystemThreadArg*>(arg));
    {
        ProgramThreadCountContextHelper tch(xsink, sta->pgm, true);
        if (!*xsink) {
            // counted in the Program now: a cancellation of the Program's threads in progress includes this thread,
            // as it includes the threads that the Program starts with background (see QoreProgram::cancelThreads())
            if (qore_program_private::get(*sta->pgm)->cancellingNewThreads()) {
                thread_list.cancelThread(q_gettid(), "Program thread cancellation requested",
                    sta->pgm->getProgramId());
            }
            sta->code->invoke(nullptr, xsink).discard(xsink);
        }
    }
    sta->release(xsink);
}

#ifdef DEBUG
//! the number of system thread starts that fail after they were reserved; see qore_dbg_fail_next_system_thread_starts()
std::atomic<int> system_thread_dbg_fail_starts{0};

//! Returns true if a system thread start must fail (debug builds, for tests)
bool qore_dbg_fail_system_thread_start() {
    int count = system_thread_dbg_fail_starts.load();
    while (count > 0) {
        if (system_thread_dbg_fail_starts.compare_exchange_weak(count, count - 1)) {
            return true;
        }
    }
    return false;
}
#endif
}

int qore_start_system_thread(const ResolvedCallReferenceNode* code, QoreProgram* pgm, ExceptionSink* xsink) {
    // allocated before the start is reserved: whatever throws after the reservation releases it, and the argument
    QoreSystemThreadArgHolder arg(new QoreSystemThreadArg(code, pgm), xsink);
    // the start is reserved under the lock; the hooks do not start to run until it has been made (see
    // qore_close_exit_hooks()), so that the hook that stops the thread runs after it exists
    {
        std::lock_guard<std::mutex> l(exit_hook_lock);
        if (exit_hooks_closed || qore_program_private::get(*pgm)->exit_hooks_closed) {
            xsink->raiseException("SANDBOX-CAPABILITY-ERROR", "a system thread cannot be started after the exit hooks "
                "of the process or of its Program have started");
            return -1;
        }
        ++system_threads_starting;
    }
    // released after the start, also if the start throws
    QoreSystemThreadStartReservation reservation;
    // started without the lock, which creating a thread must not hold (it takes other locks, and fork() handlers)
#ifdef DEBUG
    if (qore_dbg_fail_system_thread_start()) {
        xsink->raiseException("THREAD-CREATION-FAILURE", "the system thread start failed (debug fault injection)");
        return -1;
    }
#endif
    int tid = q_start_thread(xsink, qore_system_thread_main, arg.get(), QTF_NO_SANDBOX_INHERIT);
    if (tid >= 0) {
        // the thread owns the argument
        arg.disown();
    }
    return tid;
}

#ifdef DEBUG
void qore_dbg_fail_next_system_thread_starts(int count) {
    system_thread_dbg_fail_starts.store(count > 0 ? count : 0);
}

int qore_dbg_system_threads_starting() {
    std::lock_guard<std::mutex> l(exit_hook_lock);
    return system_threads_starting;
}
#endif

void qore_drop_exit_hooks(QoreProgram* pgm) {
    // the Program is destroyed: its hooks are released without their dependency references, which belong to the
    // destruction already
    std::vector<QoreExitHook> drop;
    {
        std::lock_guard<std::mutex> l(exit_hook_lock);
        for (auto i = exit_hooks.begin(); i != exit_hooks.end();) {
            if (i->pgm == pgm) {
                drop.push_back(*i);
                i = exit_hooks.erase(i);
            } else {
                ++i;
            }
        }
    }
    if (drop.empty()) {
        return;
    }
    ExceptionSink xsink;
    for (auto& i : drop) {
        i.code->deref(&xsink);
    }
    xsink.handleExceptions();
}
