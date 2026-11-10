/*
    ModuleManager.cpp

    Qore module manager

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

#include "qore/Qore.h"
// inherit_module_sandbox_manager() derefs a QoreSandboxManager; QoreProgram.h only forward-declares
// it, so this translation unit needs the definition to compile outside a single-compilation-unit
// build
#include "qore/QoreSandboxManager.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreForkHandlers.h"
#include "qore/intern/ModuleInfo.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreException.h"
#include "qore/intern/QoreDir.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/QoreModuleSourceFingerprints.h"
#include "qore/intern/xxhash.h"
#include "qore/intern/QorePluginRegistry.h"
#include "qore/intern/QoreSignal.h"

// dlopen() flags
#define QORE_DLOPEN_FLAGS RTLD_LAZY|RTLD_GLOBAL
#define QORE_DLOPEN_NOW_FLAGS RTLD_NOW|RTLD_GLOBAL

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCode.h>
#include <Security/SecStaticCode.h>
#endif

#ifdef HAVE_GLOB_H
#include <glob.h>
#else
#include "qore/intern/qore_glob.h"
#endif

#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <functional>
#include <set>
#include <string>
#include <sys/stat.h>
#ifndef _Q_WINDOWS
#include <fcntl.h>
#include <unistd.h>
#endif
#include <sys/types.h>
#include <vector>
#ifndef _Q_WINDOWS
#include <dirent.h>
#endif

// newest first: a module file is looked up by its API suffix in this order
static const qore_mod_api_compat_s qore_mod_api_list_l[] = {
    {2, 1},
    {2, 0},
};
#define QORE_MOD_API_LEN (sizeof(qore_mod_api_list_l)/sizeof(struct qore_mod_api_compat_s))

// public symbols
const qore_mod_api_compat_s* qore_mod_api_list = qore_mod_api_list_l;
const unsigned qore_mod_api_list_len = QORE_MOD_API_LEN;

FILE* qore_open_regular_file(const char* path) {
#ifdef _Q_WINDOWS
    return fopen(path, "rb");
#else
    // a file that is not a regular file is not opened at all: opening a FIFO for reading blocks until a writer opens it
    struct stat sb;
    if (stat(path, &sb) || !S_ISREG(sb.st_mode)) {
        return nullptr;
    }
    // without blocking, in case the file was replaced by a FIFO since, and then only if it is still a regular file:
    // not necessarily the one stat() saw, as a build replaces a module or a depfile atomically by renaming a new file
    // over it, which must be read like the old one (refusing it would load an out-of-date module without a warning)
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return nullptr;
    }
    struct stat fsb;
    if (fstat(fd, &fsb) || !S_ISREG(fsb.st_mode)) {
        close(fd);
        return nullptr;
    }
    // reads of a regular file do not block anyway; the flag is cleared so that the stream behaves as fopen()'s
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        close(fd);
        return nullptr;
    }
    FILE* fp = fdopen(fd, "rb");
    if (!fp) {
        close(fd);
    }
    return fp;
#endif
}

const pthread_mutexattr_t* qore_module_manager_mutex_attr() {
#ifdef DEBUG
    // made once, before the module manager (a static object) is constructed
    static const pthread_mutexattr_t* attr = []() -> const pthread_mutexattr_t* {
        static pthread_mutexattr_t a;
        if (pthread_mutexattr_init(&a) || pthread_mutexattr_settype(&a, PTHREAD_MUTEX_ERRORCHECK)) {
            return nullptr;
        }
        return &a;
    }();
    return attr;
#else
    return nullptr;
#endif
}

QoreModuleManager QMM;

void QoreModuleManager::forkChild(int tid) {
    // the threads that waited for or loaded modules do not exist in the child
    module_load_waiting = 0;
    module_wait_for.clear();
    for (auto& i : module_load_map) {
        ModuleLoadEntry& e = i.second;
        e.waiters = 0;
        if (e.state == MLS_INITIALIZING && e.owner_tid != tid) {
            e.state = MLS_FAILED;
            // called by a fork handler, which cannot throw: without memory for the message, the load fails with
            // the message it has
            try {
                e.err = "LOAD-MODULE-ERROR";
                e.desc = "the module was being initialized by another thread when the process was forked; it "
                    "cannot be loaded in the child process";
            } catch (...) {
            }
        }
    }
    module_load_cond.reinitAfterFork();
    mutex.reinitAfterFork(qore_module_manager_mutex_attr());
}

namespace {
//! the fork handler of the module manager (see QoreForkHandlers.h)
class QoreModuleManagerForkHandler : public QoreForkHandler {
public:
    QoreModuleManagerForkHandler() : QoreForkHandler(QFLR_MODULE_MANAGER, "module_manager") {
    }

    void prepare() noexcept override {
    }

    void parent() noexcept override {
    }

    void child() noexcept override {
        QMM.forkChild(qore_fork_get_tid());
    }
};

QoreModuleManagerForkHandler qmm_fork_handler;
}
ModuleManager MM;

// thread-local counter tracking nested module loading depth
// when > 0, parseLoadModule should not try to acquire the mutex
static thread_local int module_load_depth = 0;

// parent modules queued for %try-child-module attachment when the outermost module load in this thread
// completes; see design/qore-module-structure.md "Child Modules"
static thread_local std::vector<std::string> child_attach_queue;

// modules whose children are currently being attached in this thread; prevents unbounded recursion when
// modules declare each other as children
static thread_local std::set<std::string> child_attach_set;

static bool show_errors = false;

// for detecting recursive user module dependencies
typedef std::set<const char*, ltstr> mod_set_t;
static mod_set_t modset;

QoreModuleDefContext::strset_t QoreModuleDefContext::vset;

static int module_load_check(const char* path) {
    mod_set_t::iterator i = modset.lower_bound(path);
    if (i != modset.end() && !strcmp(*i, path))
        return -1;
    modset.insert(i, path);
    return 0;
}

static void module_load_clear(const char* path) {
    mod_set_t::iterator i = modset.find(path);
    assert(i != modset.end());
    modset.erase(i);
}

static bool has_separated_module_main(const char* path, const char* feature) {
    QoreString modulePath(path);
    modulePath += QORE_DIR_SEP_STR;
    modulePath += feature;
    modulePath += ".qm";

    struct stat sb;
    return !stat(modulePath.c_str(), &sb) && S_ISREG(sb.st_mode);
}

static bool qore_binary_load_error_can_fallback_to_source(ExceptionSink& xsink) {
    if (!xsink.isException()) {
        return false;
    }

    QoreValue desc = xsink.getExceptionDesc();
    if (desc.getType() == NT_STRING) {
        QoreStringValueHelper str(desc);
        if (str->c_str() && strstr(str->c_str(), "source fallback is disabled")) {
            return false;
        }
    }

    return true;
}

//! A module load in progress in this thread that can receive the warnings of the module loader
/** There are two kinds of frames: a module load made by a \c %%requires or \c %%try-module directive, which has no
    warning sink of its own, so the parse action that has the directive reports its warnings (\a pgm is the Program
    being parsed), and a module load made at run time by an API (\a pgm is nullptr), which has the warning mask and
    the warning sink, if any, of its caller.  The frames of nested module loads are linked from the innermost one: a
    module loaded from its source parses its own \c %%requires directives while the load of the module is in progress.

    See qore_report_module_load_warning()
*/
struct QoreModuleLoadWarningFrame {
    //! the Program whose parse action has the directive, or nullptr for a module load made at run time
    QoreProgram* pgm;
    //! the location of the directive, if known
    const QoreProgramLocation* loc;
    //! the warning sink of the caller of a module load made at run time, if separate from its exception sink
    ExceptionSink* wsink;
    //! the warning mask of a module load made at run time
    int warning_mask;
    //! the frame of the module load in progress when this one started, if any
    QoreModuleLoadWarningFrame* parent;
};

//! The innermost module load in progress in this thread that can receive the warnings of the module loader, if any
static thread_local QoreModuleLoadWarningFrame* qore_module_load_warning_frame = nullptr;

//! Adds a frame for the warnings of the module loader for the duration of its scope
/** See qore_report_module_load_warning()
*/
class QoreModuleLoadWarningFrameHelper {
public:
    //! A module load made by a parse directive of \a pgm at the given location, if known
    DLLLOCAL QoreModuleLoadWarningFrameHelper(QoreProgram* pgm, const QoreProgramLocation* loc)
            : frame({pgm, loc, nullptr, 0, qore_module_load_warning_frame}) {
        assert(pgm);
        qore_module_load_warning_frame = &frame;
    }

    //! A module load made at run time with the given warning sink (nullptr if none) and warning mask
    DLLLOCAL QoreModuleLoadWarningFrameHelper(ExceptionSink* wsink, int warning_mask)
            : frame({nullptr, nullptr, wsink, warning_mask, qore_module_load_warning_frame}) {
        qore_module_load_warning_frame = &frame;
    }

    DLLLOCAL ~QoreModuleLoadWarningFrameHelper() {
        assert(qore_module_load_warning_frame == &frame);
        qore_module_load_warning_frame = frame.parent;
    }

    QoreModuleLoadWarningFrameHelper(const QoreModuleLoadWarningFrameHelper&) = delete;
    QoreModuleLoadWarningFrameHelper& operator=(const QoreModuleLoadWarningFrameHelper&) = delete;

private:
    QoreModuleLoadWarningFrame frame;
};

//! Reports a warning of the module loader about how a module was loaded
/** These are the loader's own warnings: an AOT module that was not loaded and whose source module was loaded instead
    (\c BINARY-MODULE-SOURCE-FALLBACK), an AOT module loaded without functionality (\c BINARY-MODULE-STALE), and a
    child module declared with \c %%try-child-module that failed to load or was not attached (\c MODULE-CHILD-FAILED,
    \c MODULE-CHILD-SKIPPED), as opposed to the parse warnings in the source of a module.  A warning does not fail the load of the module.

    A module load with a \a warning_mask of 0 issues no warning unless \a always is set.  The warning goes to the
    warning sink of the caller of the module load if the caller has one separate from its exception sink (the
    \c *Warn() variants of the module loading APIs) and \a warning_mask is not 0.  A module load without such a sink
    is part of an outer one (see QoreModuleLoadWarningFrame), which decides, from the innermost
    one outwards:
    - a module load made by a \c %%requires or \c %%try-module directive of a Program reports the warning as a
      warning of the parse action that has the directive, if that parse action has a warning sink separate from its
      exception sink and warnings enabled, so that Program::parse() and the other parse APIs return it like any other
      parse warning; if the parse action has warnings disabled (a warning mask of 0, the default of Program::parse()),
      the warning is not issued;
    - a module load made by a directive in the source of a user module is decided by the next frame, the load of the
      user module: the warning mask of the module's parse action can be the module's own (\c %%modern enables all
      warnings), and its parse warnings can be raised in its exception sink, where they are errors;
    - a module load made at run time by an API reports it in the warning sink of its caller if the caller has one and
      warnings enabled; with a warning mask of 0, the warning is not issued; otherwise no caller can receive it;
    - if no frame can receive the warning, it is written to stderr, unless warnings are disabled.

    With a warning mask of 0, no warnings are issued, as documented for the module loading and parse APIs; this
    includes \c BINARY-MODULE-SOURCE-FALLBACK, since the source module that is loaded instead is the module, and the
    program runs as it does without the AOT module.  A warning with \a always set is issued even then (on stderr if no
    frame can receive it), as the code run is not the code that the caller would expect: an AOT module that is out of
    date with its sources, or that lacks functionality.

    @param xsink the exception sink of the module load
    @param caller_wsink the warning sink of the caller of the module load if separate from \a xsink, otherwise nullptr
    @param warning_mask the warning mask of the module load
    @param always issue the warning even when warnings are disabled
    @param err the warning code
    @param name the name of the module
    @param desc the description of the warning; ownership is taken
*/
static void qore_report_module_load_warning(ExceptionSink& xsink, ExceptionSink* caller_wsink, int warning_mask,
        bool always, const char* err, const char* name, QoreStringNode* desc) {
    SimpleRefHolder<QoreStringNode> desc_holder(desc);
    assert(caller_wsink != &xsink);

    // a module load with warnings disabled issues none, whichever load it is part of (as a module load that checks
    // whether an optional module is available)
    if (!warning_mask && !always) {
        return;
    }

    ExceptionSink* target = nullptr;
    // the parse action that receives the warning, if any
    QoreModuleLoadWarningFrame* parse_frame = nullptr;
    bool issue = warning_mask || always;
    if (caller_wsink) {
        if (warning_mask) {
            target = caller_wsink;
        }
    } else {
        for (QoreModuleLoadWarningFrame* frame = qore_module_load_warning_frame; frame; frame = frame->parent) {
            if (frame->pgm) {
                // a module load made by a parse directive in the source of a user module: the load of the user
                // module decides, as the warning mask of the module's parse action can be the module's own (set by
                // %modern, for example), and its parse warnings can be raised in its exception sink, as errors
                if (frame->pgm->getParseOptions64() & PO_IN_MODULE) {
                    continue;
                }
                // a module load made by a parse directive of a Program
                if (!qore_program_private::getParseWarningMask(*frame->pgm)) {
                    issue = always;
                    break;
                }
                target = qore_program_private::getSeparateParseWarningSink(*frame->pgm);
                if (target) {
                    parse_frame = frame;
                } else {
                    // the parse action raises its warnings in its exception sink, where they are errors
                    issue = true;
                }
                break;
            }
            // a module load made at run time
            if (!frame->warning_mask) {
                issue = always;
            } else if (frame->wsink) {
                target = frame->wsink;
            } else {
                issue = true;
            }
            break;
        }
    }

    if (target) {
        // no allocation may happen between the release() calls; the argument evaluation order is unspecified
        SimpleRefHolder<QoreStringNode> arg(new QoreStringNode(name));
        if (parse_frame && parse_frame->loc) {
            // a warning of the parse action, at the location of the directive
            qore_program_private::raiseModuleLoadWarning(*parse_frame->pgm, *target, *parse_frame->loc, err,
                arg.release(), desc_holder.release());
        } else {
            target->raiseExceptionArg(err, arg.release(), desc_holder.release());
        }
        return;
    }
    if (issue) {
        printe("warning: %s\n", desc_holder->c_str());
    }
}

//! Reports that a binary module failed to load and that its source module was loaded instead
/** See qore_report_module_load_warning(); the warning is not reported with a warning mask of 0
*/
static void qore_warn_binary_module_source_fallback(ExceptionSink& xsink, ExceptionSink& wsink, int warning_mask,
        const char* name, const char* binary_path, const char* source_path, ExceptionSink& binary_xsink) {
    QoreStringValueHelper err(binary_xsink.getExceptionErr());
    QoreStringValueHelper desc(binary_xsink.getExceptionDesc());

    SimpleRefHolder<QoreStringNode> warn_desc(new QoreStringNodeMaker("binary module '%s' for feature '%s' failed "
        "to load; loading source module '%s' instead", binary_path, name, source_path));

    const char* err_str = err->c_str();
    const char* desc_str = desc->c_str();
    if (err_str && *err_str) {
        warn_desc->sprintf(": %s", err_str);
        if (desc_str && *desc_str) {
            warn_desc->sprintf(": %s", desc_str);
        }
    } else if (desc_str && *desc_str) {
        warn_desc->sprintf(": %s", desc_str);
    }

    qore_report_module_load_warning(xsink, &xsink == &wsink ? nullptr : &wsink, warning_mask, false,
        "BINARY-MODULE-SOURCE-FALLBACK", name, warn_desc.release());
}

//! Reports the failure of the source module loaded instead of a binary module that failed to load
/** The exception of the binary module, which tells why it was not loaded, follows the exception of the source module
    in \a xsink (as \c next in the exception hash), so that the caller sees both why the binary module was not used and
    why the source module failed.

    @param xsink the exception sink of the module load, with the exception of the source module, if any
    @param name the name of the module
    @param binary_path the path of the binary module
    @param source_path the path of the source module
    @param binary_xsink the exception of the binary module; it is moved to \a xsink
*/
static void qore_raise_binary_module_source_fallback_error(ExceptionSink& xsink, const char* name,
        const char* binary_path, const char* source_path, ExceptionSink& binary_xsink) {
    if (!xsink) {
        xsink.raiseException("LOAD-MODULE-ERROR", "cannot load user module '%s' from source '%s' after binary "
            "module '%s' failed", name, source_path, binary_path);
    }
    binary_xsink.appendLastDescription(" (the binary module was not loaded; source module '%s' was tried "
        "instead)", source_path);
    xsink.assimilate(binary_xsink);
}

#ifndef _Q_WINDOWS
//! Returns the modification time of a file in nanoseconds
static int64 qore_module_stat_mtime_ns(const struct stat& sb) {
    long nsec = 0;
#ifdef HAVE_STRUCT_STAT_ST_TIM
    nsec = sb.st_mtim.tv_nsec;
#elif defined(HAVE_STRUCT_STAT_ST_TIMESPEC)
    nsec = sb.st_mtimespec.tv_nsec;
#endif
    return static_cast<int64>(sb.st_mtime) * 1000000000LL + nsec;
}

//! Returns true if the file exists and was modified after \a mtime_ns
static bool qore_module_file_newer(const std::string& path, int64 mtime_ns) {
    struct stat sb;
    return !stat(path.c_str(), &sb) && S_ISREG(sb.st_mode) && qore_module_stat_mtime_ns(sb) > mtime_ns;
}

//! the content hashes of the sources of linked AOT modules computed by this process, by path, size and modification
//! time, so that a source that many modules depend on is hashed once
static std::mutex qore_aot_source_hash_mutex;
static std::map<std::tuple<std::string, uint64_t, uint64_t>, uint64_t> qore_aot_source_hashes;
static QoreForkStdMutex qore_aot_source_hash_fork_handler(qore_aot_source_hash_mutex, QFLR_LEAF,
    "aot_module_source_hashes");

//! Returns the content hash of a file whose size and modification time are given, from the cache or computed
static bool qore_module_source_hash(const std::string& path, const QoreAOTSourceStatFingerprint& st, uint64_t& hash,
        ExceptionSink& xsink) {
    std::tuple<std::string, uint64_t, uint64_t> key(path, st.size, st.mtime_ns);
    {
        std::lock_guard<std::mutex> lck(qore_aot_source_hash_mutex);
        auto i = qore_aot_source_hashes.find(key);
        if (i != qore_aot_source_hashes.end()) {
            hash = i->second;
            return true;
        }
    }
    // the file is hashed without the lock
    if (!qore_aot_file_content_hash(path.c_str(), hash, &xsink)) {
        return false;
    }
    // a file that changed while it was hashed is not cached
    QoreAOTSourceStatFingerprint after;
    if (getAOTSourceStatFingerprint(path.c_str(), after) && after.size == st.size && after.mtime_ns == st.mtime_ns) {
        std::lock_guard<std::mutex> lck(qore_aot_source_hash_mutex);
        qore_aot_source_hashes.emplace(std::move(key), hash);
    }
    return true;
}

//! Returns true if a source of a linked AOT module has changed since the module was compiled
/** The size and modification time that qcc recorded for the source decide first: if both are unchanged, the source
    is unchanged, and it is not read.  A source with another size has changed.  A source with the same size and
    another modification time, as after a checkout, an artifact copy or a package extraction, has changed only if its
    content hash differs from the recorded one.

    @param path the source
    @param fingerprint the size, modification time and content hash that qcc recorded for it
    @param qmod_mtime the modification time of the module, to compare a source that cannot be read with
    @param xsink for cancellation

    @return true if the source has changed
*/
static bool qore_module_source_changed(const std::string& path, const QoreAOTModuleSourceFingerprint& fingerprint,
        int64 qmod_mtime, ExceptionSink& xsink) {
    QoreAOTSourceStatFingerprint st;
    // a source that does not exist is not checked, as without a fingerprint
    if (!getAOTSourceStatFingerprint(path.c_str(), st)) {
        return false;
    }
    if (st.size == fingerprint.size && st.mtime_ns == fingerprint.mtime_ns) {
        return false;
    }
    if (st.size != fingerprint.size) {
        return true;
    }
    uint64_t hash;
    if (!qore_module_source_hash(path, st, hash, xsink)) {
        // a source that cannot be read is compared by its modification time, as without a fingerprint
        return !xsink && qore_module_file_newer(path, qmod_mtime);
    }
    return hash != fingerprint.hash;
}

//! Returns the target and the prerequisites of a Make-style dependency file written by qcc (\c --depfile)
/** The file has one rule, <tt>target: prerequisite...</tt>; a space or backslash in a path is escaped with a
    backslash, and a backslash before a line break continues the rule

    @param path the dependency file
    @param target set to the target of the rule, without its escapes
    @param deps set to the prerequisites of the rule, without their escapes

    @return false if the file cannot be read, or has no rule or a rule without a target
*/
static bool qore_read_module_depfile(const std::string& path, std::string& target, std::vector<std::string>& deps) {
    FILE* fp = qore_open_regular_file(path.c_str());
    if (!fp) {
        return false;
    }
    std::string content;
    char buf[4096];
    size_t len;
    // a dependency file is small; one that is larger than any qcc writes is not read
    static constexpr size_t max_size = 16 * 1024 * 1024;
    while ((len = fread(buf, 1, sizeof buf, fp)) > 0) {
        content.append(buf, len);
        if (content.size() > max_size) {
            fclose(fp);
            return false;
        }
    }
    bool error = ferror(fp);
    fclose(fp);
    if (error) {
        return false;
    }

    // the target ends at the first colon that is not escaped
    target.clear();
    size_t i = 0;
    for (; i < content.size(); ++i) {
        char c = content[i];
        if (c == '\\' && i + 1 < content.size()) {
            char n = content[i + 1];
            ++i;
            if (n == '\n' || n == '\r') {
                // a target is on one line
                return false;
            }
            target += n;
        } else if (c == ':') {
            break;
        } else if (c == '\n' || c == '\r') {
            // a line without a colon is no rule
            return false;
        } else if (c != ' ' && c != '\t') {
            target += c;
        } else if (!target.empty()) {
            // an unescaped blank ends the target; only blanks may follow it before the colon
            size_t j = i;
            while (j < content.size() && (content[j] == ' ' || content[j] == '\t')) {
                ++j;
            }
            if (j == content.size() || content[j] != ':') {
                return false;
            }
            i = j;
            break;
        }
    }
    if (i == content.size() || target.empty()) {
        return false;
    }
    std::string dep;
    for (++i; i < content.size(); ++i) {
        char c = content[i];
        if (c == '\\' && i + 1 < content.size()) {
            char n = content[i + 1];
            if (n == '\n' || n == '\r') {
                // a line continuation separates prerequisites
                c = ' ';
            } else {
                dep += n;
                ++i;
                continue;
            }
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!dep.empty()) {
                deps.push_back(dep);
                dep.clear();
            }
            continue;
        }
        dep += c;
    }
    if (!dep.empty()) {
        deps.push_back(dep);
    }
    return true;
}
#endif

bool qore_aot_file_content_hash(const char* path, uint64_t& hash, ExceptionSink* xsink) {
    std::unique_ptr<FILE, int (*)(FILE*)> fp(qore_open_regular_file(path), fclose);
    if (!fp) {
        return false;
    }
    XXH64_state_t state;
    XXH64_reset(&state, 0);
    std::vector<char> buf(65536);
    size_t len;
    size_t blocks = 0;
    while ((len = fread(buf.data(), 1, buf.size(), fp.get())) > 0) {
        XXH64_update(&state, buf.data(), len);
        if (xsink && !(++blocks % 100) && qore_check_cancel(xsink, "AOT module source hash")) {
            return false;
        }
    }
    if (ferror(fp.get())) {
        return false;
    }
    hash = XXH64_digest(&state);
    return true;
}

bool qore_aot_write_module_source_fingerprints(const std::string& depfile, const std::vector<std::string>& deps,
        std::string& error) {
    std::string content = QORE_AOT_MODULE_FINGERPRINT_HEADER "\n";
    ExceptionSink xsink;
    for (size_t i = 0; i < deps.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(&xsink, "AOT module source fingerprints")) {
            break;
        }
        const std::string& dep = deps[i];
        if (dep.empty() || dep.find_first_of("\r\n") != std::string::npos) {
            continue;
        }
        QoreAOTSourceStatFingerprint st;
        if (!getAOTSourceStatFingerprint(dep.c_str(), st)) {
            continue;
        }
        uint64_t hash;
        if (!qore_aot_file_content_hash(dep.c_str(), hash, &xsink)) {
            if (xsink) {
                break;
            }
            continue;
        }
        // a file that changed while it was hashed is not recorded
        QoreAOTSourceStatFingerprint after;
        if (!getAOTSourceStatFingerprint(dep.c_str(), after) || after.size != st.size
                || after.mtime_ns != st.mtime_ns) {
            continue;
        }
        char fields[64];
        snprintf(fields, sizeof fields, "%llu %llu %016llx ", static_cast<unsigned long long>(st.size),
            static_cast<unsigned long long>(st.mtime_ns), static_cast<unsigned long long>(hash));
        content += fields;
        content += dep;
        content += '\n';
    }
    if (xsink) {
        xsink.clear();
        error = "operation cancelled while writing the AOT module source fingerprints for '" + depfile + "'";
        return false;
    }

    std::string path = depfile + QORE_AOT_MODULE_FINGERPRINT_SUFFIX;
    std::string tmp = path + ".tmp." + std::to_string(static_cast<long long>(getpid()));
    {
        std::unique_ptr<FILE, int (*)(FILE*)> fp(fopen(tmp.c_str(), "wb"), fclose);
        if (!fp) {
            error = "cannot open '" + tmp + "' for writing: " + strerror(errno);
            return false;
        }
        if (fwrite(content.data(), 1, content.size(), fp.get()) != content.size()) {
            error = "cannot write '" + tmp + "': " + strerror(errno);
            fp.reset();
            unlink(tmp.c_str());
            return false;
        }
        if (fclose(fp.release())) {
            error = "cannot close '" + tmp + "': " + strerror(errno);
            unlink(tmp.c_str());
            return false;
        }
    }
    if (rename(tmp.c_str(), path.c_str())) {
        error = "cannot replace '" + path + "': " + strerror(errno);
        unlink(tmp.c_str());
        return false;
    }
    return true;
}

bool qore_aot_read_module_source_fingerprints(const std::string& path,
        std::map<std::string, QoreAOTModuleSourceFingerprint>& fingerprints) {
    std::unique_ptr<FILE, int (*)(FILE*)> fp(qore_open_regular_file(path.c_str()), fclose);
    if (!fp) {
        return false;
    }
    std::string content;
    char buf[4096];
    size_t len;
    // a fingerprint file is small; one that is larger than any qcc writes is not read
    static constexpr size_t max_size = 64 * 1024 * 1024;
    while ((len = fread(buf, 1, sizeof buf, fp.get())) > 0) {
        content.append(buf, len);
        if (content.size() > max_size) {
            return false;
        }
    }
    if (ferror(fp.get())) {
        return false;
    }
    fp.reset();

    size_t eol = content.find('\n');
    if (eol == std::string::npos || content.compare(0, eol, QORE_AOT_MODULE_FINGERPRINT_HEADER)) {
        return false;
    }
    std::map<std::string, QoreAOTModuleSourceFingerprint> rv;
    for (size_t start = eol + 1; start < content.size(); start = eol + 1) {
        eol = content.find('\n', start);
        if (eol == std::string::npos) {
            // a line without its line break is incomplete
            return false;
        }
        std::string line = content.substr(start, eol - start);
        const char* p = line.c_str();
        QoreAOTModuleSourceFingerprint f;
        char* end;
        errno = 0;
        f.size = strtoull(p, &end, 10);
        if (end == p || *end != ' ' || errno) {
            return false;
        }
        p = end + 1;
        f.mtime_ns = strtoull(p, &end, 10);
        if (end == p || *end != ' ' || errno) {
            return false;
        }
        p = end + 1;
        f.hash = strtoull(p, &end, 16);
        if (end - p != 16 || *end != ' ' || errno) {
            return false;
        }
        p = end + 1;
        if (!*p) {
            return false;
        }
        rv[p] = f;
    }
    fingerprints.swap(rv);
    return true;
}

#ifndef _Q_WINDOWS
//! Returns true if the target of a module dependency file is the given module
/** @param depfile the path of the dependency file
    @param depfile_target the target of its rule; a relative target is relative to the directory of \a depfile
    @param module the module, as a path whose symbolic links are resolved

    @return true if both resolve to the same file
*/
static bool qore_module_depfile_names_module(const std::string& depfile, const std::string& depfile_target,
        const std::string& module) {
    std::string path = depfile_target;
    if (path[0] != QORE_DIR_SEP) {
        size_t sep = depfile.rfind(QORE_DIR_SEP);
        path = (sep == std::string::npos ? std::string(".") : depfile.substr(0, sep)) + QORE_DIR_SEP_STR + path;
    }
    char* resolved = realpath(path.c_str(), nullptr);
    if (!resolved) {
        return false;
    }
    char* resolved_module = realpath(module.c_str(), nullptr);
    if (!resolved_module) {
        free(resolved);
        return false;
    }
    bool rv = !strcmp(resolved, resolved_module);
    free(resolved);
    free(resolved_module);
    return rv;
}
#endif

bool qore_aot_read_module_depfile(const std::string& qmod, std::vector<std::string>& deps) {
    deps.clear();
#ifdef _Q_WINDOWS
    return false;
#else
    char* resolved = realpath(qmod.c_str(), nullptr);
    if (!resolved) {
        return false;
    }
    std::string target_module = resolved;
    free(resolved);
    std::string depfile = target_module + ".d";
    std::string target;
    if (!qore_read_module_depfile(depfile, target, deps) || !qore_module_depfile_names_module(depfile, target,
            target_module)) {
        deps.clear();
        return false;
    }
    return true;
#endif
}

//! Returns true if a source of an AOT module of a build tree has changed since the module was compiled
/** A build tree links the AOT modules it builds into the source \c qlib directory (\c QORE_AOT_LINK_SOURCE_MODULES),
    where the in-tree tests find them before the module sources, and the test runner puts the directory of the built
    modules (\c build/qlib-qmod) first in the module path; without this check, a module built before one of its
    sources was changed would run the old code without any sign of it.  A module that is a symbolic link is checked,
    and a module that is a regular file is checked if qcc wrote a dependency file next to it
    (<tt>\<module\>.d</tt>), as the build of a tree does.  An installed module has no dependency file (the install
    rules remove it, see QoreMacros.cmake), so it is never checked; nor is a module compiled with
    <tt>qcc --compile-module</tt> without <tt>--depfile</tt>.  A dependency file is used only if the target of its rule
    is the module checked (for a link, the module it resolves to): a dependency file copied next to another module,
    with a build tree copied to an installation, is not that module's and is ignored, as if there were none.

    The sources are the files listed in the dependency file that qcc wrote next to the module it links to, or next to
    the module itself (<tt>\<module\>.d</tt>), which include the sources of the modules it depends on.  A source has changed if its
    content differs from the content it had when the module was compiled, as recorded by qcc in the fingerprint file
    next to the dependency file (see QoreModuleSourceFingerprints.h): a source whose size and modification time are those
    recorded is not read, and one with only another modification time, as after a checkout or a copy of a build tree,
    is compared by its content hash.  A source that has no fingerprint, or a module without a fingerprint file, is
    compared by its modification time: a source that is newer than the module has changed.  For a linked module
    without a dependency file, the main source of the module and, for a module in a directory of its own, the module
    sources in that directory are compared by their modification time.

    The check is skipped if the Program cannot access the filesystem; a file that the filesystem policy of a sandbox
    does not allow to be read is not checked.

    @param qmod_path the path of the AOT module: found in the module path, or named by its path
    @param source_dir the directory where the source module was found, which the check of a linked module without a
    dependency file compares (\c \<source_dir\>/\<name\>.qm, or the module directory \c \<source_dir\>/\<name\>/);
    for a module without a source module to load instead, the directory of \a qmod_path, where such a check finds
    no source
    @param name the name of the module
    @param pgm the Program loading the module, if any
    @param stale_source set to a source that has changed since the module was compiled
    @param xsink for cancellation

    @return true if a source has changed since the module was compiled
*/
static bool qore_build_aot_module_is_stale(const char* qmod_path, const std::string& source_dir, const char* name,
        const QoreProgram* pgm, std::string& stale_source, ExceptionSink& xsink) {
#ifdef _Q_WINDOWS
    return false;
#else
    // a Program without filesystem access does not even look at the file
    if (pgm && (pgm->getParseOptions() & PO_NO_FILESYSTEM)) {
        return false;
    }
    struct stat lsb;
    if (lstat(qmod_path, &lsb)) {
        return false;
    }
    const bool linked = S_ISLNK(lsb.st_mode);
    if (!linked && !S_ISREG(lsb.st_mode)) {
        return false;
    }
    std::string target;
    if (linked) {
        char* resolved = realpath(qmod_path, nullptr);
        if (!resolved) {
            return false;
        }
        target = resolved;
        free(resolved);
    } else {
        target = qmod_path;
    }
    struct stat sb;
    if (stat(target.c_str(), &sb) || !S_ISREG(sb.st_mode)) {
        return false;
    }
    int64 qmod_mtime = qore_module_stat_mtime_ns(sb);

    // every file is checked against the filesystem policy of a sandbox; a file that cannot be read is not checked
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
    auto allowed = [&smh](const std::string& path) -> bool {
        if (!smh) {
            return true;
        }
        ExceptionSink sandbox_xsink;
        if (smh->checkFilesystemAccess(path.c_str(), QSEC_READ, &sandbox_xsink)) {
            return true;
        }
        sandbox_xsink.clear();
        return false;
    };

    std::string depfile = target + ".d";
    if (!allowed(depfile)) {
        return false;
    }

    // a module that is a regular file is checked only with its dependency file
    if (!linked) {
        struct stat dsb;
        if (stat(depfile.c_str(), &dsb) || !S_ISREG(dsb.st_mode)) {
            return false;
        }
    }

    // A dependency file describes the module that is its rule's target, and only that one: a copy next to another
    // module (a build tree's qlib-qmod copied to an installation with cp -a or rsync -a, which also keep the
    // modification times) names the module it was written for, whose sources have changed since the copy was
    // made, and must not make the copy look out of date.  So the dependency file is used only when its target is
    // the module checked here: the module itself, or for a link the module that it resolves to.  A relative target
    // is taken relative to the directory of the dependency file.  One that does not name it is not used, as if
    // there were none; the target of a file that cannot be read, or of one without a rule, is not compared.
    std::vector<std::string> deps;
    std::string depfile_target;
    bool use_depfile = qore_read_module_depfile(depfile, depfile_target, deps);
    if (use_depfile && !qore_module_depfile_names_module(depfile, depfile_target, target)) {
        printd(1, "qore_build_aot_module_is_stale() '%s': ignoring dependency file '%s', whose target '%s' is "
            "another module\n", qmod_path, depfile.c_str(), depfile_target.c_str());
        use_depfile = false;
        deps.clear();
    }
    if (use_depfile) {
        // the size, modification time and content hash of each source when the module was compiled, as qcc recorded
        // them next to the dependency file; without them, or for a source without one, a source that is newer than
        // the module has changed
        std::map<std::string, QoreAOTModuleSourceFingerprint> fingerprints;
        std::string fingerprint_file = depfile + QORE_AOT_MODULE_FINGERPRINT_SUFFIX;
        if (allowed(fingerprint_file)) {
            qore_aot_read_module_source_fingerprints(fingerprint_file, fingerprints);
        }
        for (size_t i = 0; i < deps.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(&xsink, "AOT module source check")) {
                return false;
            }
            if (!allowed(deps[i])) {
                continue;
            }
            auto fi = fingerprints.find(deps[i]);
            bool changed = fi == fingerprints.end()
                ? qore_module_file_newer(deps[i], qmod_mtime)
                : qore_module_source_changed(deps[i], fi->second, qmod_mtime, xsink);
            if (xsink) {
                return false;
            }
            if (changed) {
                stale_source = deps[i];
                return true;
            }
        }
        return false;
    }

    // a regular file whose dependency file cannot be read is not checked
    if (!linked) {
        return false;
    }

    // without a dependency file, the sources of the linked module itself are checked
    std::string main_source = source_dir + QORE_DIR_SEP_STR + name + ".qm";
    if (allowed(main_source) && qore_module_file_newer(main_source, qmod_mtime)) {
        stale_source = main_source;
        return true;
    }
    std::string module_dir = source_dir + QORE_DIR_SEP_STR + name;
    if (!allowed(module_dir)) {
        return false;
    }
    DIR* dir = opendir(module_dir.c_str());
    if (!dir) {
        return false;
    }
    bool stale = false;
    size_t count = 0;
    while (struct dirent* de = readdir(dir)) {
        if (++count % 100 == 0 && qore_check_cancel(&xsink, "AOT module source check")) {
            break;
        }
        size_t len = strlen(de->d_name);
        if (len < 4 || (strcmp(de->d_name + len - 3, ".qm") && strcmp(de->d_name + len - 3, ".qc"))) {
            continue;
        }
        std::string path = module_dir + QORE_DIR_SEP_STR + de->d_name;
        if (allowed(path) && qore_module_file_newer(path, qmod_mtime)) {
            stale_source = path;
            stale = true;
            break;
        }
    }
    closedir(dir);
    return stale;
#endif
}

#ifndef _Q_WINDOWS
//! Returns true if a module loaded from \a file is the module that an AOT module was compiled against
/** @param name the name of the module
    @param file the real path of the file the module was loaded from
    @param compiled_against the real paths of the files that the depfile of the AOT module lists
    @param allowed returns false for a file that the filesystem policy of a sandbox does not allow to be read
*/
static bool qore_aot_dependency_is_compiled_one(const std::string& name, const std::string& file, bool is_aot,
        const std::set<std::string>& compiled_against,
        const std::map<std::string, QoreAOTModuleSourceFingerprint>& fingerprints,
        const std::function<bool(const std::string&)>& allowed) {
    // a source module, a binary module, or an AOT module listed as itself (--depfile-module-deps=artifact)
    if (compiled_against.count(file)) {
        return true;
    }
    size_t file_sep = file.rfind(QORE_DIR_SEP);
    std::string file_base = file_sep == std::string::npos ? file : file.substr(file_sep + 1);
    // a copy of a file compiled against, with the same name, size and content (a link farm, an installed copy); this
    // is the only other file that counts for a native binary module too: AOT code depends on more of a native module
    // than the names of its symbols (constants folded into it, the variants and hashdecls it was compiled against,
    // purity flags), and the module API version in the name of its file is not the version of the module, so another
    // build of it is a different module
    {
        const std::string& base = file_base;
        struct stat fsb;
        bool have_hash = false;
        uint64_t hash = 0;
        if (!stat(file.c_str(), &fsb) && S_ISREG(fsb.st_mode)) {
            for (const auto& i : fingerprints) {
                size_t isep = i.first.rfind(QORE_DIR_SEP);
                if (i.first.compare(isep == std::string::npos ? 0 : isep + 1, std::string::npos, base)
                        || i.second.size != static_cast<uint64_t>(fsb.st_size)) {
                    continue;
                }
                if (!have_hash) {
                    if (!qore_aot_file_content_hash(file.c_str(), hash, nullptr)) {
                        break;
                    }
                    have_hash = true;
                }
                if (hash == i.second.hash) {
                    return true;
                }
            }
        }
    }
    struct stat sb;
    if (!stat(file.c_str(), &sb) && S_ISDIR(sb.st_mode)) {
        // a module directory: its main source
        return compiled_against.count(file + QORE_DIR_SEP_STR + name + ".qm") > 0;
    }
    // an AOT module of a build tree: its own sources must be those the module was compiled against
    if (file.size() <= 5 || file.compare(file.size() - 5, 5, ".qmod")) {
        return false;
    }
    if (!allowed(file + ".d")) {
        // not checked
        return true;
    }
    std::vector<std::string> deps;
    if (!qore_aot_read_module_depfile(file, deps)) {
        // an AOT module without its depfile (an installed module, or one compiled by hand without --depfile) is not
        // checked, as the loader does not check such a module's own sources either: what it was compiled from is not
        // known; a native binary module of another name or API version is a different module than the file listed
        return is_aot;
    }
    // the module's main source names its own sources: the file, or the directory of a module directory
    std::string main_name = name + ".qm";
    std::string root;
    bool dir = false;
    for (const std::string& dep : deps) {
        size_t sep = dep.rfind(QORE_DIR_SEP);
        if (sep == std::string::npos || dep.compare(sep + 1, std::string::npos, main_name)) {
            continue;
        }
        std::string parent = dep.substr(0, sep);
        size_t psep = parent.rfind(QORE_DIR_SEP);
        dir = psep != std::string::npos && !parent.compare(psep + 1, std::string::npos, name);
        root = dir ? parent : dep;
        break;
    }
    if (root.empty()) {
        return false;
    }
    for (const std::string& dep : deps) {
        bool own = dir ? !dep.compare(0, root.size() + 1, root + QORE_DIR_SEP_STR) : dep == root;
        if (own && !compiled_against.count(dep)) {
            return false;
        }
    }
    return true;
}
#endif

std::string QoreModuleManager::findModuleFileUnlocked(const char* name, QoreProgram* pgm) {
    // the search path of loadModuleIntern(): for the dependencies of an AOT module, which are checked in its
    // QoreModulePathScope, its own directories with those of the Program
    std::vector<const std::string*> search_paths;
    std::vector<std::string> prepended, appended;
    qore_program_private::getEffectiveModulePathLists(pgm, prepended, appended);
    for (const std::string& p : prepended) {
        search_paths.push_back(&p);
    }
    for (const std::string& p : moduleDirList) {
        search_paths.push_back(&p);
    }
    for (const std::string& p : appended) {
        search_paths.push_back(&p);
    }
    struct stat sb;
    auto exists = [&sb](const QoreString& p) -> bool {
        return !stat(p.c_str(), &sb) && S_ISREG(sb.st_mode);
    };
    // the order of loadModuleIntern(): in each directory, the binary forms (with each API version, then without),
    // then those in a directory of the module's name, then the source, then a module directory
    for (const std::string* dir : search_paths) {
        for (const char* sub : {"", name}) {
            for (unsigned ai = 0; ai <= qore_mod_api_list_len; ++ai) {
                QoreString str(*dir);
                if (*sub) {
                    str.sprintf(QORE_DIR_SEP_STR "%s", sub);
                }
                str.sprintf(QORE_DIR_SEP_STR "%s", name);
                if (ai < qore_mod_api_list_len) {
                    str.sprintf("-api-%d.%d.qmod", qore_mod_api_list[ai].major, qore_mod_api_list[ai].minor);
                } else {
                    str.concat(".qmod");
                }
                if (exists(str)) {
                    return str.c_str();
                }
            }
        }
        QoreString str(*dir);
        str.sprintf(QORE_DIR_SEP_STR "%s.qm", name);
        if (exists(str)) {
            return str.c_str();
        }
        str = *dir;
        str.sprintf(QORE_DIR_SEP_STR "%s", name);
        if (!stat(str.c_str(), &sb) && S_ISDIR(sb.st_mode) && has_separated_module_main(str.c_str(), name)) {
            return str.c_str();
        }
    }
    return std::string();
}

bool QoreModuleManager::checkAOTModuleDependencyFiles(ExceptionSink& xsink, const char* path, const char* feature,
        const QoreAOTModuleFileInfo& aot_info, QoreProgram* pgm, std::string& stale_desc) {
#ifndef _Q_WINDOWS
    if (pgm && (pgm->getParseOptions() & PO_NO_FILESYSTEM)) {
        return false;
    }
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
    std::function<bool(const std::string&)> allowed = [&smh](const std::string& file) -> bool {
        if (!smh) {
            return true;
        }
        ExceptionSink sandbox_xsink;
        if (smh->checkFilesystemAccess(file.c_str(), QSEC_READ, &sandbox_xsink)) {
            return true;
        }
        sandbox_xsink.clear();
        return false;
    };
    char* resolved = realpath(path, nullptr);
    if (!resolved) {
        return false;
    }
    std::string target = resolved;
    free(resolved);
    if (!allowed(target + ".d")) {
        return false;
    }
    // only a module of a build tree, with the depfile qcc wrote for it, is checked
    std::vector<std::string> deps;
    if (!qore_aot_read_module_depfile(target, deps)) {
        return false;
    }
    // qcc writes the real paths of the files (see write_depfile() in qcc-main.cpp), so they are compared as they are
    std::set<std::string> compiled_against(deps.begin(), deps.end());
    // the content of each file when the module was compiled, by which a copy of one is recognized
    std::map<std::string, QoreAOTModuleSourceFingerprint> fingerprints;
    if (allowed(target + ".d" QORE_AOT_MODULE_FINGERPRINT_SUFFIX)) {
        qore_aot_read_module_source_fingerprints(target + ".d" QORE_AOT_MODULE_FINGERPRINT_SUFFIX, fingerprints);
    }
    // every module the AOT module was compiled with: the dependencies of its descriptor, and the modules its metadata
    // names, which are those it requires directly or not and those they re-export (a re-exported module is not a
    // dependency of the descriptor, and a module further down the chain can be loaded from another file while its
    // dependent comes from its source); qcc names them, as the files of the depfile do not say which of them are
    // modules (an %include file can have the name of one)
    std::set<std::string> names(aot_info.dependencies.begin(), aot_info.dependencies.end());
    names.insert(aot_info.compiled_with.begin(), aot_info.compiled_with.end());
    names.erase(feature);
    names.erase("qore");
    size_t count = 0;
    for (const std::string& name : names) {
        if (++count % 10 == 0 && qore_check_cancel(&xsink, "AOT module dependency file check")) {
            return false;
        }
        // a module that is loaded is used as it is; one that is not is found as loadModuleIntern() would find it
        std::string file;
        bool is_aot = false;
        QoreAbstractModule* m = findModuleUnlocked(name.c_str());
        if (m) {
            const char* fn = m->getFileName();
            // a builtin module has no file
            if (!fn || !*fn || *fn == '<') {
                continue;
            }
            file = fn;
            is_aot = !m->isUser() && m->isBuiltin() && static_cast<QoreBuiltinModule*>(m)->isAot();
        } else {
            // as the AOT module's initialization will load it: with its own module path directives too, which the
            // caller's QoreModulePathScope adds
            file = findModuleFileUnlocked(name.c_str(), pgm);
            if (file.empty()) {
                continue;
            }
        }
        char* r = realpath(file.c_str(), nullptr);
        if (r) {
            file = r;
            free(r);
        }
        // a file the sandbox does not let the loader read is neither read nor checked
        if (!allowed(file)) {
            continue;
        }
        if (!m && file.size() > 5 && !file.compare(file.size() - 5, 5, ".qmod")) {
            QoreAOTModuleFileInfo info;
            std::string error;
            is_aot = qoreAOTReadModuleInfo(file, info, error) == QoreAOTModuleInfoStatus::FOUND;
        }
        if (!qore_aot_dependency_is_compiled_one(name, file, is_aot, compiled_against, fingerprints, allowed)) {
            stale_desc = std::string("it was compiled against other files of module '") + name + "' than '" + file
                + "', which is loaded for it now";
            return true;
        }
    }
#endif
    return false;
}

//! Raises the warning for an AOT module that is not loaded because one of its sources has changed
static void qore_warn_stale_aot_module(ExceptionSink& xsink, ExceptionSink& wsink, int warning_mask,
        const char* name, const char* qmod_path, const char* source_path, const char* stale_source) {
    // the warning is always reported, as the module that is run is not the one the build tree provides (see
    // qore_report_module_load_warning())
    qore_report_module_load_warning(xsink, &xsink == &wsink ? nullptr : &wsink, warning_mask, true,
        "BINARY-MODULE-SOURCE-FALLBACK", name, new QoreStringNodeMaker("AOT module '%s' for feature '%s' is out of "
        "date: its source '%s' has changed since it was compiled; loading source module '%s' instead; rebuild the "
        "AOT module", qmod_path, name, stale_source, source_path));
}

static bool qore_find_user_module_source(const std::string& dir, const char* name, QoreString& source_path,
        bool& separated) {
    struct stat sb;

    source_path.sprintf("%s" QORE_DIR_SEP_STR "%s.qm", dir.c_str(), name);
    if (!stat(source_path.c_str(), &sb) && S_ISREG(sb.st_mode)) {
        separated = false;
        return true;
    }

    source_path.clear();
    source_path.sprintf("%s" QORE_DIR_SEP_STR "%s", dir.c_str(), name);
    if (!stat(source_path.c_str(), &sb) && S_ISDIR(sb.st_mode) && has_separated_module_main(source_path.c_str(), name)) {
        separated = true;
        return true;
    }

    return false;
}

//! Sets dir to the directory of a separated module whose entry source is the explicit path given
/** An explicit path such as `<module-dir>/<feature>/<feature>.qm` names the entry source of a separated module, which
    is the directory with its sibling .qc and .ql files; loading only the entry file leaves the module incomplete.

    @return true if the path is the entry source of a separated module, in which case dir is set to its directory
*/
static bool qore_explicit_source_is_separated_main(const char* path, const char* feature, QoreString& dir) {
    dir.clear();
    const char* sep = q_find_last_path_sep(path);
    if (!sep) {
        return false;
    }
    dir = path;
    dir.terminate(sep - path);
    if (strcmp(q_basenameptr(dir.c_str()), feature) || !has_separated_module_main(dir.c_str(), feature)) {
        dir.clear();
        return false;
    }
    return true;
}

static bool qore_find_explicit_qmod_source(const char* path, const char* feature, QoreString& source_path,
        QoreString& separated_path) {
    separated_path.clear();
    size_t len = strlen(path);
    if (len <= 5 || strcasecmp(path + len - 5, ".qmod")) {
        return false;
    }

    source_path = path;
    source_path.terminate(len - 5);
    source_path.concat(".qm");

    struct stat sb;
    if (stat(source_path.c_str(), &sb) || !S_ISREG(sb.st_mode)) {
        return false;
    }

    // An explicit path such as `<module-dir>/<feature>/<feature>.qmod`
    // refers to a separated module. The source fallback must load the parent
    // directory so that all sibling .qc and .ql files are parsed as well.
    qore_explicit_source_is_separated_main(source_path.c_str(), feature, separated_path);
    return true;
}

ModuleReExportHelper::ModuleReExportHelper(QoreAbstractModule* mi, bool reexp) : m(set_reexport(mi, reexp, reexport)) {
    //printd(5, "ModuleReExportHelper::ModuleReExportHelper() %p '%s' (reexp: %d) to %p '%s' (reexp: %d)\n", mi, mi ? mi->getName() : "n/a", reexp, m, m ? m->getName() : "n/a", reexport);
    if (m && mi && reexp) {
        //printd(5, "ModuleReExportHelper::ModuleReExportHelper() adding '%s' to '%s'\n", mi->getName(), m->getName());
        m->addModuleReExport(mi->getName());
    }
}

ModuleReExportHelper::~ModuleReExportHelper() {
    set_reexport(m, reexport);
}

const char* qore_child_module_status_string(ChildModuleStatus status) {
    switch (status) {
        case CMS_PENDING: return "pending";
        case CMS_ATTACHED: return "attached";
        case CMS_ABSENT: return "absent";
        case CMS_SKIPPED: return "skipped";
        case CMS_FAILED: return "failed";
    }
    assert(false);
    return "unknown";
}

void qore_get_module_spec_name(const char* spec, QoreString& name) {
    name.clear();
    const char* p = strchrs(spec, "<>=");
    if (p) {
        name.concat(spec, p - spec);
    } else {
        name.concat(spec);
    }
    name.trim();
}

void QoreAbstractModule::setChildModules(const std::vector<std::string>& specs) {
    for (const std::string& spec : specs) {
        QoreString cname;
        qore_get_module_spec_name(spec.c_str(), cname);
        if (cname.empty()) {
            continue;
        }
        // ignore duplicate declarations; the parser rejects duplicates in a single module, but the same
        // module object can also receive declarations from a module description function
        bool dup = false;
        for (const ChildModuleInfo& c : children) {
            if (c.name == cname.c_str()) {
                dup = true;
                break;
            }
        }
        if (dup) {
            continue;
        }
        children.push_back(ChildModuleInfo(spec.c_str(), cname.c_str()));
    }
}

QoreHashNode* QoreAbstractModule::getHashIntern(bool with_filename) const {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);

    qore_hash_private* ph = qore_hash_private::get(*h);

    if (with_filename) {
        ph->setKeyValueIntern("filename", new QoreStringNode(filename));
    }
    ph->setKeyValueIntern("path", new QoreStringNode(path));
    ph->setKeyValueIntern("name", new QoreStringNode(name));
    ph->setKeyValueIntern("desc", new QoreStringNode(desc));
    ph->setKeyValueIntern("version", new QoreStringNode(*version_list));
    ph->setKeyValueIntern("author", new QoreStringNode(author));
    if (!url.empty())
        ph->setKeyValueIntern("url", new QoreStringNode(url));
    if (!license.empty())
        ph->setKeyValueIntern("license", new QoreStringNode(license));
    if (!rmod.empty()) {
        QoreListNode* l = new QoreListNode(stringTypeInfo);
        for (name_vec_t::const_iterator i = rmod.begin(), e = rmod.end(); i != e; ++i)
            l->push(new QoreStringNode(*i), nullptr);
        ph->setKeyValueIntern("reexported-modules", l);
    }
    if (!children.empty()) {
        // child modules declared with %try-child-module, keyed by feature name in declaration order
        QoreHashNode* ch = new QoreHashNode(autoTypeInfo);
        qore_hash_private* pch = qore_hash_private::get(*ch);
        for (const ChildModuleInfo& c : children) {
            QoreHashNode* ci = new QoreHashNode(autoTypeInfo);
            qore_hash_private* pci = qore_hash_private::get(*ci);
            pci->setKeyValueIntern("spec", new QoreStringNode(c.spec));
            pci->setKeyValueIntern("status", new QoreStringNode(qore_child_module_status_string(c.status)));
            if (!c.err.empty()) {
                pci->setKeyValueIntern("err", new QoreStringNode(c.err));
            }
            if (!c.desc.empty()) {
                pci->setKeyValueIntern("desc", new QoreStringNode(c.desc));
            }
            pch->setKeyValueIntern(c.name.c_str(), ci);
        }
        ph->setKeyValueIntern("child-modules", ch);
    }
    ph->setKeyValueIntern("injected", injected);
    ph->setKeyValueIntern("reinjected", reinjected);

    return h;
}

void QoreAbstractModule::reexport(ExceptionSink& xsink, QoreProgram* pgm) const {
    // import also any modules that should be reexported from the loaded module
    for (name_vec_t::const_iterator i = rmod.begin(), e = rmod.end(); i != e; ++i) {
        // Skip self-reexport: if the dep being reexported IS the module that owns
        // the target Program, loading it again would re-merge the module's own
        // user-public namespace into itself and raise "duplicate function" errors.
        //
        // This happens when a module M defines a runtime function that calls
        // `load_module(X)` (e.g. DataProvider::tryLoad → load_module("AmqpDataProvider"))
        // and X has `%requires(reexport) M`. getProgram() inside M's function
        // returns M.mod_pgm, so X's reexport tries to load M into M.mod_pgm.
        //
        // Supports both source user modules (QoreUserModule::getProgram()) and
        // AOT user modules (tracked in aot_module_map).
        QoreAbstractModule* dep_mi = QMM.findModuleUnlocked(i->c_str());
        if (dep_mi && dep_mi->isUser()
            && static_cast<QoreUserModule*>(dep_mi)->getProgram() == pgm) {
            continue;
        }
        if (qore_aot_get_module_pgm(i->c_str()) == pgm) {
            continue;
        }

        if (!qore_program_private::get(*pgm)->hasFeature(i->c_str())) {
            QMM.loadModuleForReexport(xsink, i->c_str(), pgm);
        }
    }
}

QoreModuleContext::QoreModuleContext(const char* n, qore_root_ns_private* n_rns, ExceptionSink& xs)
        : name(n), rns(n_rns), parent(get_module_context()), xsink(xs) {
}

void QoreModuleContext::error(const char* fmt, ...) {
    va_list args;
    QoreStringNode* err = new QoreStringNodeMaker("module '%s': ", name);

    while (true) {
        va_start(args, fmt);
        int rc = err->vsprintf(fmt, args);
        va_end(args);
        if (!rc)
            break;
    }

    xsink.raiseExceptionArg("MODULE-LOAD-ERROR", new QoreStringNode(name), err);
}

void QoreModuleContext::commit() {
    rns->commitModule(*this);

    mcfl.mcfl_t::clear();
    mcnl.mcnl_t::clear();
}

void qore_declare_child_module(const QoreProgramLocation* loc, const char* spec) {
    QoreModuleDefContext* qmd = get_module_def_context();
    if (!qmd) {
        // AOT-compiled modules re-parse their embedded source at runtime with the module context name set;
        // their child declarations arrive through the module description function instead, and the directive
        // is stripped from the embedded source, so ignore it here rather than failing the module load
        if (get_module_context_name()) {
            return;
        }
        parse_error(*loc, "the %%try-child-module parse directive can only be used in a user module");
        return;
    }
    qmd->addChild(loc, spec, get_module_context_name());
}

int QoreModuleDefContext::addChild(const QoreProgramLocation* loc, const char* spec, const char* mod_name) {
    QoreString cname;
    qore_get_module_spec_name(spec, cname);
    if (cname.empty()) {
        parse_error(*loc, "missing module name in the %%try-child-module declaration '%%try-child-module %s'",
            spec);
        return -1;
    }

    if (mod_name && !cname.compare(mod_name)) {
        parse_error(*loc, "module '%s' cannot declare itself as a child module with %%try-child-module",
            mod_name);
        return -1;
    }

    for (const std::string& i : child_vec) {
        QoreString iname;
        qore_get_module_spec_name(i.c_str(), iname);
        if (!iname.compare(cname.c_str())) {
            parse_error(*loc, "child module '%s' has already been declared with %%try-child-module in this "
                "module", cname.c_str());
            return -1;
        }
    }

    child_vec.push_back(spec);
    return 0;
}

int QoreModuleDefContext::set(const QoreProgramLocation* loc, const char* key, QoreValue val) {
    int err = 0;
    // special handling for "init" and "del"
    if (!strcmp(key, "init")) {
        if (init_c) {
            parse_error(*loc, "module key 'init' was given multiple times");
            err = -1;
        } else {
            // check type when code is committed
            init_c = val.refSelf();
            init_loc = loc;
        }
    } else if (!strcmp(key, "del")) {
        if (del_c) {
            parse_error(*loc, "module key 'del' was given multiple times");
            err = -1;
        } else {
            // check type when code is committed
            del_c = val.refSelf();
            del_loc = loc;
        }
    } else if (vset.find(key) == vset.end()) {
        parse_error(*loc, "module key '%s' is invalid", key);
        err = -1;
    } else if (vmap.find(key) != vmap.end()) {
        parse_error(*loc, "module key '%s' was given multiple times", key);
        err = -1;
    } else if (val.getType() != NT_STRING) {
        parse_error(*loc, "module key '%s' assigned type '%s' (expecting 'string')", key, val.getTypeName());
        err = -1;
    } else {
        QoreStringValueHelper str(val);
        vmap[key] = str->c_str();
    }

    return err;
}

// called only during parsing
int QoreModuleDefContext::parseInit() {
    int err = 0;
    if (init_c) {
        err = initClosure(init_loc, init_c, "init");
    }
    if (del_c) {
        if (initClosure(del_loc, del_c, "del") && !err) {
            err = -1;
        }
    }
    return err;
}

int QoreModuleDefContext::initClosure(const QoreProgramLocation* loc, QoreValue& c, const char* n) {
    // initialize closure
    QoreParseContext parse_context;
    // check for local variables at the top level - this can only happen if the expresion is not a closure
    int err = parse_init_value(c, parse_context);
    if (parse_context.lvids) {
        parseException(*loc, "ILLEGAL-LOCAL-VAR", "local variables may not be declared in module '%s' code", n);
        // discard variables immediately
        for (int i = 0; i < parse_context.lvids; ++i) {
            pop_local_var();
        }
        if (!err) {
            err = -1;
        }
    }

    qore_type_t t = c.getType();
    if (t != NT_CLOSURE && t != NT_FUNCREF) {
        parse_error(*loc, "the module '%s' key must be assigned to a closure or call reference (got type '%s')", n,
            c.getTypeName());
        if (!err) {
            err = -1;
        }
    }
    return err;
}

int QoreModuleDefContext::init(QoreProgram& pgm, ExceptionSink& xsink) {
    if (!init_c) {
        return 0;
    }

    {
        ProgramThreadCountContextHelper tch(&xsink, &pgm, true);
        if (xsink) {
            return -1;
        }
        // the loading Program does not run the init closure on its own behalf: it runs with the module's options,
        // not with the restrictions of the code that loads the module
        QoreModuleLifecycleBarrierHelper lifecycle_barrier;
        LoadTimeParseOptionsHelper lifecycle_po(pgm.getParseOptions());

        ValueHolder cn(init_c.eval(&xsink), &xsink);
        assert(!xsink);
        assert(cn->getType() == NT_RUNTIME_CLOSURE || cn->getType() == NT_FUNCREF);
        cn->get<ResolvedCallReferenceNode>()->invoke(0, &xsink).discard(&xsink);
    }

    return xsink ? -1 : 0;
}

AbstractQoreNode* QoreModuleDefContext::takeDel() {
    if (!del_c) {
        return nullptr;
    }

    AbstractQoreNode* rv = del_c.get<AbstractQoreNode>();
    del_c.clear();
    return rv;
}

QoreModuleContextHelper::QoreModuleContextHelper(const char* name, QoreProgram* pgm, ExceptionSink& xsink)
        : QoreModuleContext(name, qore_root_ns_private::get(*(pgm->getRootNS())), xsink) {
    set_module_context(this);
}

QoreModuleContextHelper::~QoreModuleContextHelper() {
    set_module_context(parent);
}

QoreUserModuleDefContextHelper::QoreUserModuleDefContextHelper(const char* name, const char* path, QoreProgram* p,
        ExceptionSink& xs)
        : mod_name(name), old_name(set_module_context_name(name)), old_path(set_module_context_path(path)),
        pgm(qore_program_private::get(*p)), po(0), xsink(xs), dup(false) {
    QMM.getUserModuleRegistration(name, saved_reg);
}

bool QoreModuleManager::isUserModuleContext(const char* name) {
    QoreAbstractModule* mi = findModuleUnlocked(name);
    if (mi) {
        return mi->isUser();
    }
    // a user module is registered only once its source has been parsed, and it loads the modules it requires while
    // it is parsed: it is then the module of the thread's module definition context, which is always a user module
    // definition context; the module context name set for the initialization of an AOT module does not change it
    QoreModuleDefContext* qmd = get_module_def_context();
    if (!qmd) {
        return false;
    }
    const char* def_name = static_cast<QoreUserModuleDefContextHelper*>(qmd)->getModuleName();
    return def_name && !strcmp(def_name, name);
}

void QoreModuleManager::trySetUserBinaryModuleDependency(const char* name) {
    const char* old_name = get_module_context_name();
    if (old_name && strcmp(old_name, name) && isUserModuleContext(old_name)) {
        binary_user_dep_map.addDep(name, old_name);
    }
}

void QoreModuleManager::trySetUserModuleDependency(const QoreAbstractModule* mi) {
    if (!mi->isUser()) {
        return;
    }

    const char* old_name = get_module_context_name();
    if (old_name && isUserModuleContext(old_name)) {
        setUserModuleDependency(mi->getName(), old_name);
    }
    trySetUserModule(mi->getName());
}

void QoreUserModuleDefContextHelper::setNameInit(const char* name) {
    assert(vmap.find("name") == vmap.end());
    vmap["name"] = name;

    assert(!po);

    po = pgm->pwo.parse_options;
    pgm->pwo.parse_options |= MOD_HEADER_PO;
}

void QoreUserModuleDefContextHelper::close() {
    pgm->pwo.parse_options = po;
}

void UniqueDirectoryList::addDirList(const char* str) {
    if (!str)
        return;

    // duplicate string for invasive searches
    QoreString plist(str);
    str = (char*)plist.c_str();

    const char* sep_chars;
#ifdef _Q_WINDOWS
    // suport both ";" and ":" for path separators on Windows
    sep_chars = ";:";
#else
    sep_chars = ":";
#endif

    // add each directory
    while (char* p = (char*)strchrs(str, sep_chars)) {
#ifdef _Q_WINDOWS
        // don't match ':' as the second character in a path as a path separator
        if (*p == ':' && isalpha(*str) && (p == (str + 1))) {
            p = (char*)strchrs(p + 1, sep_chars);
            if (!p) {
                break;
            }
        }
#endif
        // ignore empty entries
        if (p != str) {
            *p = '\0';
            // add string to list
            push_back(str);
        }
        str = p + 1;
    }

    // add last directory
    if (*str) {
        push_back(str);
    }
}

static QoreStringNode* loadModuleError(const char* name, ExceptionSink& xsink) {
    QoreStringNode* rv = new QoreStringNodeMaker("failed to load module '%s':\n", name);
    qore_es_private::appendList(xsink, *rv);
    xsink.clear();
    return rv;
}

QoreProgram* QoreBuiltinModule::getModuleProgram() const {
    return qore_is_aot_user_module(name.c_str()) ? qore_aot_get_module_pgm(name.c_str()) : nullptr;
}

void QoreBuiltinModule::addToProgramImpl(QoreProgram* tpgm, ExceptionSink& xsink) const {
    QoreModuleContextHelper qmc(name.c_str(), tpgm, xsink);
    // issue #3592: must add feature first
    // Guard against double-registration: if module_ns_init already ran for this program
    // (e.g., loaded as a dependency of another module that shares this program), skip.
    if (qore_program_private::get(*tpgm)->hasFeature(name.c_str())) {
        qmc.commit();
        return;
    }
    // AOT user modules (source .qm compiled to .qmod binary form) export user-public
    // symbols that must be re-merged into each program's namespace tree via %requires.
    // Track them in userFeatureList so runtimeImportSystemApi() / setParent() do not
    // propagate them to child programs as though they were built-in features — which
    // would make child programs' %requires skip the per-program ns merge and leave
    // the module's user-public symbols missing from the child's namespace tree.
    const bool is_aot_user = qore_is_aot_user_module(name.c_str());
    if (is_aot_user) {
        qore_program_private::get(*tpgm)->addUserFeature(name.c_str());
    } else {
        tpgm->addFeature(name.c_str());
    }
    // a user module importing a binary module can hold objects of its classes, so it is deleted first
    QMM.trySetUserBinaryModuleDependency(name.c_str());

    // Make sure getProgram() returns this Program when module_ns_init() is called.
    // A call-only context is not enough when a child Program loads a module from
    // a thread already running in its parent Program; getProgram() prefers
    // current_pgm over call_program_context.
    QoreProgramContextHelper pcch(tpgm);

    RootQoreNamespace* rns = tpgm->getRootNS();
    QoreNamespace* qns = tpgm->getQoreNS();

    if (is_aot_user) {
        // an AOT module from the process-wide module path is library code: its initialization, which runs when it is
        // first applied to a Program, belongs to no sandbox (see QoreModuleInitSandboxBarrierHelper)
        QoreModuleInitSandboxBarrierHelper init_barrier(isTrusted());
        if (init_barrier.failed()) {
            xsink.outOfMemory();
        } else {
            // an AOT module merges its namespace under its own fence and must not hold the write lock
            // across the deferred initialization that follows the merge; see qore_aot_module_ns_init_impl()
            module_ns_init(rns, qns, xsink);
        }
    } else {
        // a binary module's namespace initialization writes directly into the target's committed
        // namespace (addSystemClass(), addConstant() and so on), which runtime lookups in the target
        // and threads importing from it read concurrently
        RuntimeNamespaceWriteLocker rnwl(*rns);
        module_ns_init(rns, qns, xsink);
    }

    if (xsink || qmc.hasError()) {
        // module_ns_init() may have registered classes, enums, constants into existing
        // namespaces via addSystemClass()/addEnum()/etc.  These direct modifications are
        // NOT tracked by QoreModuleContext and cannot be safely rolled back.
        // Commit partial changes to keep the namespace tree consistent, and keep the
        // feature flag to prevent a retry that would double-register the same items.
        {
            RuntimeNamespaceWriteLocker rnwl(*rns);
            qmc.commit();
        }
        qore_program_private::get(*tpgm)->commitFeature(name.c_str());
        return;
    }

    // commit all module changes
    {
        RuntimeNamespaceWriteLocker rnwl(*rns);
        qmc.commit();
    }

    // mark the feature as fully committed so the lock-free fast path in runTimeLoadModule() can see it
    qore_program_private::get(*tpgm)->commitFeature(name.c_str());
}

QoreHashNode* QoreBuiltinModule::getHash(bool with_filename) const {
    QoreHashNode* h = getHashIntern(with_filename);

    qore_hash_private* ph = qore_hash_private::get(*h);

    ph->setKeyValueIntern("user", false);
    ph->setKeyValueIntern("api_major", api_major);
    ph->setKeyValueIntern("api_minor", api_minor);

    if (info) {
        ph->setKeyValueIntern("info", info->hashRefSelf());
    }

    return h;
}

namespace {
//! What was decided when a module was loaded, until the module is registered
struct QoreModuleLoadFrame {
    //! the module's feature name
    const char* feature;
    //! true if the module is library code (see QoreModuleManager::isTrustedModulePath())
    bool trusted;
    //! true if code that is not trusted chose the module (see QoreModuleManager::checkShadowingLoad())
    bool tenant_chosen;
    //! the sandbox of that code, if any; a reference owned by the helper
    QoreSandboxManager* owner;
    //! the ID of the Program of that code if it has no sandbox
    qore_program_id_t owner_pgm_id;
    //! the frame of the enclosing load in this thread, if any
    QoreModuleLoadFrame* prev;
};

//! the innermost module load in progress in this thread
thread_local QoreModuleLoadFrame* module_load_frame = nullptr;

//! Records what was decided when a module is loaded, so that QoreModuleManager::addModule() records it on the module
class QoreModuleLoadFrameHelper {
public:
    //! takes over the reference of \a owner
    QoreModuleLoadFrameHelper(const char* feature, bool trusted, bool tenant_chosen = false,
            QoreSandboxManager* owner = nullptr, qore_program_id_t owner_pgm_id = QORE_NO_PROGRAM_ID)
            : frame{feature, trusted, tenant_chosen, owner, owner_pgm_id, module_load_frame} {
        module_load_frame = &frame;
    }

    ~QoreModuleLoadFrameHelper() {
        assert(module_load_frame == &frame);
        module_load_frame = frame.prev;
        if (frame.owner) {
            frame.owner->deref(nullptr);
        }
    }

private:
    QoreModuleLoadFrame frame;
};

//! returns true if the module that is being loaded on this thread is library code that no untrusted code chose
/** The commands that the source of such a module issues (its \c %module-cmd directives, and their replay when it is
    loaded compiled) are the library's own: the code that loads it does not issue them, and as module code is
    everywhere in %Qore they are judged by the sandbox, not by the parse options that the loader has.  A module that
    code which is not trusted chose by a path of its own is not library code (see isTrustedModulePath() and
    QoreAbstractModule::isTenantChosen()): its commands are judged by the restrictions of that code (issue #5699).
*/
bool qore_module_load_is_library() {
    const QoreModuleLoadFrame* f = module_load_frame;
    return f && f->trusted && !f->tenant_chosen;
}
}

void QoreModuleManager::addModule(QoreAbstractModule* m) {
    assert(map.find(m->getName()) == map.end());
    if (!m->load_recorded) {
        m->load_recorded = true;
        // the innermost load in this thread is the module's own, under its feature name; a private module was
        // renamed, and a module registered with no load in progress was not loaded from a file by this manager
        const QoreModuleLoadFrame* f = module_load_frame;
        if (f && (!strcmp(m->getName(), f->feature) || (m->getOrigName() && !strcmp(m->getOrigName(), f->feature)))) {
            m->trusted = f->trusted;
            if (f->tenant_chosen) {
                m->tenant_chosen = true;
                m->tenant_owner = f->owner;
                if (m->tenant_owner) {
                    m->tenant_owner->ref();
                }
                m->tenant_owner_pgm_id = f->owner_pgm_id;
            }
        } else {
            m->trusted = isTrustedModulePath(m->getFileName());
        }
    }
    map.insert(module_map_t::value_type(m->getName(), m));
    // the Program of a registered user module (source or AOT) is deleted only by cleanup: it gets module lifetime,
    // so calls into its code from other Programs need no reference to it (see qore_program_set_module_lifetime())
    if (QoreProgram* mpgm = m->getModuleProgram()) {
        qore_program_set_module_lifetime(*mpgm);
    }
    //printd(5, "QoreModuleManager::addModule() m: %p '%s'\n", m, m->getName());
}

QoreAbstractModule::~QoreAbstractModule() {
    //printd(5, "QoreAbstractModule::~QoreAbstractModule() this: %p name: %s\n", this, name.c_str());
    if (tenant_owner) {
        tenant_owner->deref(nullptr);
    }
    if (next) {
        assert(next->prev == this);
        next->prev = prev;
    }
    if (prev) {
        assert(prev->next == this);
        prev->next = next;
    }
}

void QoreUserModule::runDelCallback(ExceptionSink& xsink) {
    if (!del) {
        return;
    }
    // Claim del first to prevent double-execution if called from both Phase 0 and destructor
    AbstractQoreNode* d = del;
    del = nullptr;

    ProgramThreadCountContextHelper tch(&xsink, pgm, true);
    if (!xsink) {
        // the unloading Program does not run the del closure on its own behalf: it runs with the module's options
        QoreModuleLifecycleBarrierHelper lifecycle_barrier;
        LoadTimeParseOptionsHelper lifecycle_po(pgm->getParseOptions());
        ValueHolder cn(d->eval(&xsink), &xsink);
        assert(!xsink);
        if (!xsink) {
            assert(cn->getType() == NT_RUNTIME_CLOSURE || cn->getType() == NT_FUNCREF);
            cn->get<ResolvedCallReferenceNode>()->invoke(0, &xsink).discard(&xsink);
        }
    }
    d->deref(&xsink);
    xsink.handleExceptions();
}

QoreUserModule::~QoreUserModule() {
    //printd(5, "QoreUserModule::~QoreUserModule() this: %p name: %s\n", this, name.c_str());
    assert(pgm);
    ExceptionSink xsink;
    // del is normally already run by QoreModuleManager::delUser() Phase 0, but
    // guard here in case the module is destroyed outside the normal del path
    runDelCallback(xsink);
    init_c.discard(&xsink);
    init_c.clear();
    // issue #4816: Type objects in foreign modules can hold strong refs to
    // this program's data, so plain deref() can leave refcount > 0 at shutdown
    // — the destructor never runs, stranding the parse tree (namespaces,
    // classes, functions, constants, globals) until process exit.
    //
    // Run waitForTerminationAndClear() first. It takes plock, sets ptid,
    // runs the cleanup callback (breaks Type cycles via qore_release_type_refs),
    // then clears local vars and namespace data under the proper locking and
    // thread-termination discipline. This is the same path clear() uses when
    // refcount reaches zero — calling it explicitly forces that safe teardown
    // even when foreign strong refs are holding the program alive. The method
    // is already idempotent (guarded by `clr` / `ns_vars` / `ns_const` flags),
    // so the later clear() invoked by deref() → ROdereference() is a no-op.
    pgm->waitForTermination();
    qore_program_private::get(*pgm)->waitForTerminationAndClear(&xsink);
    pgm->deref(&xsink);
}

void QoreUserModule::addToProgramImpl(QoreProgram* tpgm, ExceptionSink& xsink) const {
    //printd(5, "QoreUserModule::addToProgramImpl() mod '%s': tpgm %p po: %p pgm dom: %p\n", name.c_str(), tpgm,
    //    tpgm->getParseOptions(), qore_program_private::getDomain(*pgm));
    // a module that another Program has loaded is not checked against the restrictions of the Program importing it:
    // it is a capability that the host gave the Program, and its code runs with the restrictions of its callers as
    // well as its own; only the first load is checked (see QoreModuleManager::loadSeparatedModule())
    // issue #3592: must add feature first
    qore_program_private::get(*tpgm)->addUserFeature(name.c_str());

    QoreModuleContextHelper qmc(name.c_str(), tpgm, xsink);
    ProgramThreadCountContextHelper ptcch(&xsink, tpgm, false);
    if (xsink) {
        // rollback all module changes
        qmc.rollback();
        qore_program_private::get(*tpgm)->removeUserFeature(name.c_str());
        return;
    }

    RootQoreNamespace* target_root_ns = tpgm->getRootNS();
    RootQoreNamespace* source_root_ns = pgm->getRootNS();
    {
        // exclude runtime readers of the target namespace and runtime writers of the source while
        // they are merged; parse ownership, held by the caller, excludes only other writers of the
        // target
        RuntimeNamespaceMergeLocker rnml(*target_root_ns, *source_root_ns);

        qore_root_ns_private::scanMergeCommittedNamespace(*target_root_ns, *source_root_ns, qmc);

        if (qmc.hasError()) {
            // rollback all module changes
            qmc.rollback();
            qore_program_private::get(*tpgm)->removeUserFeature(name.c_str());
            return;
        }

        // commit all module changes
        qore_root_ns_private::copyMergeCommittedNamespace(*target_root_ns, *source_root_ns);
    }

    // mark the feature as fully committed so the lock-free fast path in runTimeLoadModule() can see it
    qore_program_private::get(*tpgm)->commitFeature(name.c_str());

    QMM.trySetUserModuleDependency(this);
}

// the context of the module command that the handler running on this thread is handling (issue #5699)
static thread_local const QoreModuleCommandContext* qore_module_command_context = nullptr;

QoreModuleCommandRestrictions::QoreModuleCommandRestrictions() : sandboxed(false) {
}

QoreModuleCommandRestrictions::QoreModuleCommandRestrictions(const QoreParseOptions& n_po, bool n_sandboxed)
        : po(n_po & getRestrictingOptions()), sandboxed(n_sandboxed) {
}

QoreModuleCommandRestrictions::QoreModuleCommandRestrictions(const QoreModuleCommandRestrictions& old)
        : po(old.po), sandboxed(old.sandboxed) {
}

QoreModuleCommandRestrictions& QoreModuleCommandRestrictions::operator=(const QoreModuleCommandRestrictions& old) {
    po = old.po;
    sandboxed = old.sandboxed;
    return *this;
}

QoreModuleCommandRestrictions::~QoreModuleCommandRestrictions() {
}

QoreParseOptions QoreModuleCommandRestrictions::getParseOptions() const {
    return po;
}

bool QoreModuleCommandRestrictions::isSandboxed() const {
    return sandboxed;
}

bool QoreModuleCommandRestrictions::isRestricted() const {
    return sandboxed || !!po;
}

bool QoreModuleCommandRestrictions::hasAny(const QoreParseOptions& mask) const {
    return !!(po & mask);
}

int QoreModuleCommandRestrictions::check(const QoreParseOptions& mask, bool sandboxed_denied, const char* err,
        const char* what, ExceptionSink* xsink) const {
    if (sandboxed_denied && sandboxed) {
        xsink->raiseException(err, "cannot %s: the command was issued by sandboxed code", what);
        return -1;
    }
    QoreParseOptions forbidden = po & mask;
    if (!!forbidden) {
        xsink->raiseException(err, "cannot %s: the code that issued the command has parse options that forbid it "
            "(0x" QLLX ", 0x" QLLX ")", what, forbidden.getLo(), forbidden.getHi());
        return -1;
    }
    return 0;
}

QoreParseOptions QoreModuleCommandRestrictions::getRestrictingOptions() {
    // the options that restrict a functional domain: not the options that grant something, nor the in-module domain
    // that only marks the Programs of modules
    static const QoreParseOptions restricting = QoreParseOptions(QDOM_ALL) & ~QoreParseOptions::POSITIVE_OPTIONS
        & ~QoreParseOptions(QDOM_IN_MODULE);
    return restricting;
}

QoreModuleCommandContext::QoreModuleCommandContext(Origin n_origin, const QoreProgram* n_issuer,
        const QoreProgram* n_target, const QoreParseOptions& n_po, bool n_sandboxed)
        : QoreModuleCommandRestrictions(n_po, n_sandboxed), origin(n_origin), issuer(n_issuer), target(n_target) {
}

QoreModuleCommandContext::~QoreModuleCommandContext() {
}

const QoreModuleCommandContext* QoreModuleCommandContext::get() {
    return qore_module_command_context;
}

QoreModuleCommandContext::Origin QoreModuleCommandContext::getOrigin() const {
    return origin;
}

const QoreProgram* QoreModuleCommandContext::getIssuerProgram() const {
    return issuer;
}

const QoreProgram* QoreModuleCommandContext::getTargetProgram() const {
    return target;
}

QoreModuleCommandRestrictions QoreModuleCommandContext::copy() const {
    return QoreModuleCommandRestrictions(po, sandboxed);
}

//! makes a module command context current on the thread while a handler handles the command
class QoreModuleCommandScope {
public:
    DLLLOCAL QoreModuleCommandScope(const QoreModuleCommandContext& ctx) : previous(qore_module_command_context) {
        qore_module_command_context = &ctx;
    }

    DLLLOCAL ~QoreModuleCommandScope() {
        qore_module_command_context = previous;
    }

    QoreModuleCommandScope(const QoreModuleCommandScope&) = delete;
    QoreModuleCommandScope& operator=(const QoreModuleCommandScope&) = delete;

private:
    const QoreModuleCommandContext* previous;
};

//! returns the restrictions that a command issued by the code running on this thread has, for a module command
/** The restrictions are those of the issuing code with what it inherits from its callers: the effective run-time
    parse options of the current Program and whether a sandbox governs the thread.  They only grow within a command
    that a handler issues itself.  Called before the thread switches to the Program that the command applies to.

    @param declared_po the parse options that a compiled program is declared with, for a command that is replayed
    from it, or nullptr
*/
static void qore_get_command_restrictions(QoreParseOptions& po, bool& sandboxed, const QoreParseOptions* declared_po,
        bool replay) {
    sandboxed = qore_caller_is_sandboxed();
    if (replay && qore_module_load_is_library()) {
        // the commands of a library module that is loaded compiled are the library's own: the loader's parse options
        // are not theirs, only what was in force at the directive (see qore_module_load_is_library())
        po = declared_po ? *declared_po : QoreParseOptions();
    } else {
        // a command issued by the code of a library module is judged by the sandbox only, as library module code is
        // everywhere at run time: the parse options of its Program are only what it inherited from the code that
        // loaded it (a %no-filesystem loader), not restrictions of the library
        const QoreProgram* issuing = getProgram();
        po = QoreParseOptions();
        if (!issuing || !qore_program_private::get(*const_cast<QoreProgram*>(issuing))->library_module) {
            po = runtime_get_effective_program_parse_options();
        }
        if (declared_po) {
            po |= *declared_po;
        }
    }
    if (const QoreModuleCommandContext* outer = qore_module_command_context) {
        po |= outer->getParseOptions();
        sandboxed = sandboxed || outer->isSandboxed();
    }
}

QoreModuleCommandRestrictions QoreModuleCommandRestrictions::getCallerRestrictions() {
    QoreParseOptions po;
    bool sandboxed;
    qore_get_command_restrictions(po, sandboxed, nullptr, false);
    return QoreModuleCommandRestrictions(po, sandboxed);
}

void QoreBuiltinModule::issueModuleCmd(const QoreProgramLocation* loc, const QoreString& cmd, ExceptionSink* xsink) {
    if (!module_parse_cmd) {
        if (xsink) {
            xsink->raiseException(*loc, "PARSE-COMMAND-ERROR", QoreValue(), "module '%s' loaded from '%s' has not "
                "registered a parse command handler", name.c_str(), filename.c_str());
        }
        return;
    }

    // if parse exceptions have been disabled, then skip issuing the command
    if (!xsink) {
        return;
    }

    // a module that does not declare that its handler honors the restrictions of the code that issues a command is
    // not given a command from restricted code (issue #5699)
    const QoreModuleCommandContext* ctx = QoreModuleCommandContext::get();
    // every entry point sets the context; a command that reaches a handler without one is refused (fails closed)
    assert(ctx);
    if (!ctx) {
        xsink->raiseException(*loc, "RUNTIME-COMMAND-ERROR", QoreValue(), "module '%s' loaded from '%s': the module "
            "command has no context that says which code issued it", name.c_str(), filename.c_str());
        return;
    }
    if (!cmd_caller_aware && ctx->isRestricted()) {
        xsink->raiseException(*loc, ctx->getOrigin() == QoreModuleCommandContext::MC_PARSE
            ? "PARSE-COMMAND-ERROR" : "RUNTIME-COMMAND-ERROR", QoreValue(), "module '%s' loaded from '%s' has not "
            "declared that its module commands honor the restrictions of the code that issues them "
            "(QoreModuleInfo::cmd_caller_aware), and the code that issued this command is restricted", name.c_str(),
            filename.c_str());
        return;
    }

    try {
        module_parse_cmd(cmd, xsink);
    } catch (AbstractException& e) {
        e.convert(xsink);
    }
}

ModuleManager::ModuleManager() {
}

// to add a directory to the module directory search list; a directory added after init() is searched but does not
// hold library code; ignored for code that is not trusted
void ModuleManager::addModuleDir(const char* dir) {
    QMM.addModuleDir(dir);
}

// no longer supported; throws an exception
void ModuleManager::addAutoModuleDir(const char* dir) {
    printd(0, "ModuleManager::addAutoModuleDir() support for auto module directories was removed in 0.8.4\n");
    assert(false);
}

// to add a list of directories to the module directory search list; see addModuleDir()
void ModuleManager::addModuleDirList(const char* strlist) {
    QMM.addModuleDirList(strlist);
}

// no longer supported; throws an exception
void ModuleManager::addAutoModuleDirList(const char* strlist) {
    printd(0, "ModuleManager::addAutoModuleDir() support for auto module directories was removed in 0.8.4\n");
    assert(false);
}

void QoreModuleManager::init(bool se) {
    // setup possible user module keys
    QoreModuleDefContext::vset.insert("desc");
    QoreModuleDefContext::vset.insert("version");
    QoreModuleDefContext::vset.insert("author");
    QoreModuleDefContext::vset.insert("url");
    QoreModuleDefContext::vset.insert("license");

    show_errors = se;

    // setup module directory list from QORE_MODULE_DIR (if it hasn't already been manually set up)
    if (moduleDirList.empty()) {
        if (q_env_is_set("QORE_MODULE_DIR_ONLY")) {
            moduleDirList.addDirList(QoreEnvironmentValue("QORE_MODULE_DIR").c_str());
        } else {
            QoreModuleManager::addStandardModulePaths();
        }
    }

    setTrustedModuleDirs();
}

void QoreModuleManager::setTrustedModuleDirs() {
    assert(trusted_module_dirs.empty());
    for (const std::string& dir : moduleDirList) {
        TrustedModuleDir td;
        if (dir[0] == '/' && !strstr(dir.c_str(), "/../") && !strstr(dir.c_str(), "/./")) {
            size_t len = dir.size();
            while (len > 1 && dir[len - 1] == '/') {
                --len;
            }
            // a name that ends with a "." or ".." component is not used as configured
            if (!(len >= 2 && !dir.compare(len - 2, 2, "/."))
                && !(len >= 3 && !dir.compare(len - 3, 3, "/.."))) {
                td.named = dir.substr(0, len);
            }
        }
        // a relative directory is resolved now, against the current directory of the process when the library is
        // initialized
        std::unique_ptr<char, decltype(&free)> real(realpath(dir.c_str(), nullptr), &free);
        if (real) {
            td.real = real.get();
        }
        if (!td.named.empty() || !td.real.empty()) {
            trusted_module_dirs.push_back(std::move(td));
        }
    }
}

void QoreModuleManager::addModuleDir(const char* dir) {
    if (qore_caller_is_untrusted()) {
        printd(0, "QoreModuleManager::addModuleDir() '%s': ignored: the caller is sandboxed or restricted\n", dir);
        return;
    }
    AutoLocker al(mutex);
    moduleDirList.push_back(dir);
}

void QoreModuleManager::addModuleDirList(const char* strlist) {
    if (qore_caller_is_untrusted()) {
        printd(0, "QoreModuleManager::addModuleDirList() '%s': ignored: the caller is sandboxed or restricted\n",
            strlist);
        return;
    }
    AutoLocker al(mutex);
    moduleDirList.addDirList(strlist);
}

bool QoreModuleManager::mayChangeModulePath(const QoreProgram* pgm) {
    // the source of a library module, whoever loads it first: the directory it adds is searched but holds no library
    // code (see isTrustedModulePath())
    if (pgm && qore_program_private::get(*const_cast<QoreProgram*>(pgm))->library_module) {
        return true;
    }
    if (qore_caller_is_untrusted()) {
        return false;
    }
    if (!pgm) {
        return true;
    }
    static const QoreParseOptions restricting = QoreParseOptions(QDOM_ALL) & ~QoreParseOptions::POSITIVE_OPTIONS
        & ~QoreParseOptions(QDOM_IN_MODULE);
    if (!!(pgm->getParseOptions() & restricting)) {
        return false;
    }
    // also a Program that sandboxed code created, which has no manager of its own
    return !qore_program_is_sandboxed(pgm);
}

/* this internal helper function solves an issue with modules
   on MS Windows (or maybe macOS later).

   The problem with Windows is that we are distributing zip
   files which can be extracted anywhere. But these binaries
   had hardcoded paths for modules (/z/something/something or
   C:/msys64/home/pvanek/src/qore/RELEASE/share/qore-modules
   for example). These hardcoded paths are fine on Linux because
   the language should be distributed in a form of system packages
   (rpm, deb). But on Windows the user decides where to put
   (extract) the zip archive content. It can be anywhere.

   Of course there is QORE_MODULE_DIR env variable. But it
   requires logout/login on Windows to be in action. Also
   user has to set 4 paths there. And unfortunately 2 of
   them must contain qore version in the path. So it
   required to change env variable (and re-login) after any
   qore upgrade. It was annoying.

   So for now - if there is MODULES_RELATIVE_PATH defined
   from cmake (WIN builds only for now) the location of
   module(s) is constructed from the location of qore binary.
 */
#ifdef MODULES_RELATIVE_PATH
#ifdef _Q_WINDOWS
#include <windows.h>
#endif
#endif
std::string module_dir_prefix(const char * path) {
#ifdef MODULES_RELATIVE_PATH
#ifdef _Q_WINDOWS
    // get windows qore.exe binary
    // Full path may be longer than MAX_PATH
    // Let's expand it until it fits
    std::vector<char> buff;
    DWORD copied = 0;
    do {
        buff.resize(buff.size() + MAX_PATH);
        copied = GetModuleFileName(0, &buff.at(0), buff.size());
    } while (copied >= buff.size());

    buff.resize(copied);
    std::string prefix_path(buff.data(), buff.size());
    size_t found_pos = prefix_path.rfind('\\');

    return prefix_path.substr(0, found_pos).append("/../").append(path);
#else // _Q_WINDOWS
#warning MODULES_RELATIVE_PATH has been set but the operating system is not supported yet
#endif
#else // MODULES_RELATIVE_PATH
    return path;
#endif // MODULES_RELATIVE_PATH
}

void QoreModuleManager::addStandardModulePaths() {
   moduleDirList.addDirList(QoreEnvironmentValue("QORE_MODULE_DIR").c_str());

   // append version-specific binary/AOT module directory
   if (strcmp(MODULE_VER_DIR, USER_MODULE_VER_DIR))
      moduleDirList.push_back(module_dir_prefix(MODULE_VER_DIR));

   // append version-specific user module source directory
   moduleDirList.push_back(module_dir_prefix(USER_MODULE_VER_DIR));

   // append qore module directory
   if (strcmp(MODULE_DIR, USER_MODULE_DIR))
      moduleDirList.push_back(module_dir_prefix(MODULE_DIR));

   // append user-module source directory
   moduleDirList.push_back(module_dir_prefix(USER_MODULE_DIR));
}

void ModuleManager::addStandardModulePaths() {
   QMM.addStandardModulePaths();
}

int ModuleManager::runTimeLoadModule(const char* name, ExceptionSink* xsink) {
    assert(name);
    assert(xsink);
    return QMM.runTimeLoadModule(*xsink, *xsink, name, getProgram());
}

int ModuleManager::runTimeLoadModule(const char* name, QoreProgram* pgm, ExceptionSink* xsink) {
    assert(name);
    assert(xsink);
    return QMM.runTimeLoadModule(*xsink, *xsink, name, pgm);
}

int ModuleManager::runTimeLoadModule(ExceptionSink* xsink, const char* name, QoreProgram* pgm,
        qore_binary_module_desc_t mod_desc_func) {
    assert(name);
    assert(xsink);
    return QMM.runTimeLoadModule(*xsink, *xsink, name, pgm, nullptr, QMLO_NONE, QP_WARN_MODULES, false, mod_desc_func);
}

int ModuleManager::registerAOTStaticModule(ExceptionSink* xsink, QoreProgram* tpgm,
        qore_binary_module_desc_t desc_fn, const char* path) {
    assert(xsink);
    assert(tpgm);
    assert(desc_fn);
    return QMM.registerAOTStaticModuleIntern(*xsink, tpgm, desc_fn, path ? path : "<aot-static>");
}

static const char* get_feature_from_path(QoreString& tmp);

int QoreModuleManager::runTimeLoadModule(ExceptionSink& xsink, ExceptionSink& wsink, const char* name,
        QoreProgram* pgm, QoreProgram* mpgm, unsigned load_opt, int warning_mask, bool reexport,
        qore_binary_module_desc_t mod_desc_func) {
    // a Program container for the module is owned by this call; it is released when the module is not loaded into it
    ReferenceHolder<QoreProgram> pholder(mpgm, &xsink);

    // lock-free fast path: check committedFeatureList using only featureLock(read); this allows full
    // concurrency for the common case where the module is already loaded — no serialization on plock.
    // committedFeatureList is only populated AFTER namespace changes are committed in addToProgramImpl(),
    // so a hit here guarantees the module's types are fully available in this program.  Not for the Program of a
    // library module: whether a module that it has may be used is decided under the module manager mutex (see
    // mayUseTenantModule()), whoever calls
    const bool library_target = pgm && qore_program_private::get(*pgm)->library_module;
    if (pgm && !load_opt && !library_target) {
        QoreString tmp;
        const char* feat_name = name;
        if (strchrs(name, "./\\")) {
            tmp = name;
            feat_name = get_feature_from_path(tmp);
        }
        if (qore_program_private::get(*pgm)->hasCommittedFeature(feat_name)) {
            return 0;
        }
    }

    // AOT module application can execute initializers that resolve committed
    // methods in another Program and therefore take that Program's parse lock.
    // Serialize cold runtime module loading before taking the target Program's
    // parse lock so every participating thread observes the same lock order:
    // AOT initialization, then Program parsing.  The committed-feature fast
    // path above remains lock-free.
    QoreModuleLoadLockHelper aot_init_al;

    // the module is loaded for the caller, also when it is loaded into another Program (Program::loadModule()): the
    // switch to that Program below must not hide a sandboxed caller from the checks of the load and from the code
    // that the load runs (issue #5659)
    QoreCallerSandboxHelper caller_sandbox(pgm, &xsink);
    if (xsink) {
        return -1;
    }

    // slow path: grab the exclusive parse lock for actual module loading
    ProgramRuntimeParseContextHelper pah(&xsink, pgm);
    if (xsink) {
        return -1;
    }

    // double-check under parse lock using hasFeature() (which includes not-yet-committed features);
    // this catches the case where another thread is in the middle of addToProgramImpl() — the feature
    // is registered (for duplicate detection per issue #3592) but namespace changes aren't committed yet
    if (pgm && !load_opt && !library_target) {
        QoreString tmp;
        const char* feat_name = name;
        if (strchrs(name, "./\\")) {
            tmp = name;
            feat_name = get_feature_from_path(tmp);
        }
        if (qore_program_private::get(*pgm)->hasFeature(feat_name)) {
            return 0;
        }
    }

    // the warnings of the module loader for the modules loaded with this one are reported to the caller (see
    // qore_report_module_load_warning())
    QoreModuleLoadWarningFrameHelper mlwfh(&wsink == &xsink ? nullptr : &wsink, warning_mask);
    OptLocker ol(&mutex);
    loadModuleIntern(xsink, wsink, name, pgm, reexport, MOD_OP_NONE, 0, 0, pholder.release(), load_opt, warning_mask,
        mod_desc_func);
    return xsink ? -1 : 0;
}

int QoreModuleManager::loadProviderModule(ExceptionSink& xsink, const char* name, QoreProgram* path_pgm) {
    assert(name);
    QoreModuleLoadLockHelper aot_init_al;
    OptLocker ol(&mutex);
    loadModuleIntern(xsink, xsink, name, nullptr, false, MOD_OP_NONE, nullptr, nullptr, nullptr,
        QMLO_NONE, QP_WARN_MODULES, nullptr, path_pgm);
    return xsink ? -1 : 0;
}

void QoreModuleManager::loadModuleForReexport(ExceptionSink& xsink, const char* name, QoreProgram* pgm) {
    OptLocker ol(&mutex);
    loadModuleIntern(xsink, xsink, name, pgm);
}

int QoreModuleManager::registerAOTStaticModuleIntern(ExceptionSink& xsink, QoreProgram* tpgm,
        qore_binary_module_desc_t desc_fn, const char* path, const QoreAOTModuleFileInfo* aot_info) {
    // Populate mod_info up front so the feature name is known before taking the
    // parse lock / module manager mutex.  The info hash is captured here and
    // re-assigned to mod_info.info before handing off to loadBinaryModuleFromDesc,
    // which owns it from there.
    QoreModuleInfo mod_info;
    desc_fn(mod_info);
    if (mod_info.name.empty()) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(path),
            "AOT static module at '%s': no feature name present in module descriptor", path);
        if (mod_info.info) {
            mod_info.info->deref(&xsink);
        }
        return -1;
    }
    // take ownership of the info hash across the fast-path / already-loaded checks;
    // loadBinaryModuleFromDesc will re-take it from mod_info.info when we get there
    ReferenceHolder<QoreHashNode> info_holder(mod_info.info, &xsink);
    mod_info.info = nullptr;

    const std::string feature_str(mod_info.name.c_str(), mod_info.name.size());
    const char* feature = feature_str.c_str();

    // lock-free fast path: module already committed to this program?
    if (qore_program_private::get(*tpgm)->hasCommittedFeature(feature)) {
        return 0;
    }

    // Match runTimeLoadModule()'s AOT-before-Program lock order.  This path is
    // AOT-specific and can otherwise participate in the same cross-Program
    // initializer deadlock.
    QoreModuleLoadLockHelper aot_init_al;
    // see runTimeLoadModule()
    QoreCallerSandboxHelper caller_sandbox(tpgm, &xsink);
    if (xsink) {
        return -1;
    }
    ProgramRuntimeParseContextHelper pah(&xsink, tpgm);
    if (xsink) {
        return -1;
    }

    // double-check under parse lock (matches runTimeLoadModule's race-window guard)
    if (qore_program_private::get(*tpgm)->hasFeature(feature)) {
        return 0;
    }

    OptLocker ol(&mutex);

    // tag any namespaces created by the module's init callback with the module name
    QoreModuleNameContextHelper mnch(feature);

    // see if the module is already registered with QMM; if so skip the init path
    // and just stitch it into the target program
    QoreAbstractModule* mi = findModuleUnlocked(feature);
    if (!mi) {
        // the metadata record linked with the module: as for a .qmod (see loadBinaryModuleFromPath()), the module
        // must be the one it describes, an optional module it was compiled without must not be available now, and its
        // dependencies are found with its own module path directives
        std::unique_ptr<QoreModulePathScope> path_scope;
        if (aot_info) {
            if (aot_info->module_name != feature) {
                xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(path), "the AOT module metadata "
                    "linked with module '%s' identifies feature '%s'", feature, aot_info->module_name.c_str());
                return -1;
            }
            // unlike for a .qmod (loadBinaryModuleFromPath() ends the scope before the module is mapped), the scope
            // lasts through loadBinaryModuleFromDesc(), which loads the dependencies and runs the module's
            // initialization: the initialization creates the module's own Program with its own directories first and
            // then the lists of tpgm, which include them now (inheritAOTModulePathLists() skips a directory it already
            // has), so its Program gets the same lists in the same order as without the scope; and a module that the
            // initialization loads uses that Program, not tpgm, so the scope does not apply to it
            if (!aot_info->prepended_module_paths.empty() || !aot_info->appended_module_paths.empty()) {
                path_scope = std::make_unique<QoreModulePathScope>(tpgm, aot_info->prepended_module_paths,
                    aot_info->appended_module_paths);
            }
            std::string available;
            if (!aot_info->unavailable_optional_modules.empty()
                    && findAvailableOptionalModule(aot_info->unavailable_optional_modules, tpgm, available)) {
                // a statically linked module has no source module to load instead: it is used, and the missing
                // functionality is always reported, as for a .qmod without its source
                qore_report_module_load_warning(xsink, nullptr, QP_WARN_MODULES, true, "BINARY-MODULE-STALE",
                    feature, new QoreStringNodeMaker("AOT module '%s' linked into this program was compiled when "
                    "optional module '%s' was not available, but it is available now; the module is used without the "
                    "functionality that requires the optional module; rebuild the program", feature,
                    available.c_str()));
                if (xsink) {
                    return -1;
                }
            }
        }
        // restore info hash for the descriptor handoff
        mod_info.info = info_holder.release();
        mi = loadBinaryModuleFromDesc(xsink, nullptr, mod_info, path, feature, false,
            nullptr, tpgm, QMLO_NONE);
        if (!mi || xsink) {
            return -1;
        }
    }

    // mirror qore_check_load_module_intern: attach child modules declared with %try-child-module, then
    // merge the module's namespace into tpgm
    if (queueChildModules(*mi, xsink, xsink, QP_WARN_MODULES)) {
        return -1;
    }

    {
        AutoUnlocker au(&mutex);
        mi->addToProgram(tpgm, xsink);
    }
    return xsink ? -1 : 0;
}

static const char* get_op_string(mod_op_e op) {
    if (op == MOD_OP_LT) return "<";
    if (op == MOD_OP_LE) return "<=";
    if (op == MOD_OP_EQ) return "=";
    if (op == MOD_OP_GE) return ">=";
    assert(op == MOD_OP_GT);
    return ">";
}

#define MVC_FAIL     0
#define MVC_OK       1
#define MVC_FINAL_OK 2
int check_component(mod_op_e op, int mod_ver, int req_ver, bool last) {
    // "promote" operator if not comparing last element
    if (!last) {
        if (op == MOD_OP_LT) op = MOD_OP_LE;
        else if (op == MOD_OP_GT) op = MOD_OP_GE;
    }
    //printd(5, "check_component(%d %s %d)\n", mod_ver, get_op_string(op), req_ver);
    if (op == MOD_OP_LT)
        return mod_ver < req_ver ? MVC_FINAL_OK : MVC_FAIL;
    if (op == MOD_OP_LE)
        return mod_ver < req_ver ? MVC_FINAL_OK : (mod_ver == req_ver ? MVC_OK : MVC_FAIL);
    if (op == MOD_OP_EQ)
        return mod_ver == req_ver ? MVC_OK : MVC_FAIL;
    if (op == MOD_OP_GE)
        return mod_ver > req_ver ? MVC_FINAL_OK : (mod_ver == req_ver ? MVC_OK : MVC_FAIL);
    assert(op == MOD_OP_GT);
    return mod_ver > req_ver ? MVC_FINAL_OK : MVC_FAIL;
}

// issue #2834: add context to exception description if possible
static void try_add_module_context(QoreStringNode* desc) {
    const char* mod = get_module_context_name();
    if (mod) {
        QoreStringMaker str("while loading module '%s': ", mod);
        desc->prepend(str.c_str(), str.size());
    }
}

static int check_qore_version(const char* name, mod_op_e op, version_list_t& version, ExceptionSink& xsink) {
    unsigned max = version.size() > 4 ? version.size() : 4;
    for (unsigned i = 0; i < max; ++i) {
        int mv = (!i ? QORE_VERSION_MAJOR :
                    (i == 1 ? QORE_VERSION_MINOR :
                    (i == 2 ? QORE_VERSION_SUB :
                    (i == 3 ? QORE_VERSION_PATCH : 0))));
        int rv = (i >= version.size() ? 0 : version[i]);
        int res = check_component(op, mv, rv, i == (max - 1));
        if (res == MVC_FAIL) {
            QoreStringNode* desc = new QoreStringNodeMaker("feature '%s' is built in, but the following version requirement is not satisfied: Qore library %s %s %s", name, QORE_VERSION, get_op_string(op), *version);
            try_add_module_context(desc);
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), desc);
            return -1;
        }
        if (res == MVC_FINAL_OK)
            break;
    }
    return 0;
}

static void check_module_version(QoreAbstractModule* mi, mod_op_e op, version_list_t& version, ExceptionSink& xsink) {
    unsigned max = version.size() > mi->version_list.size() ? version.size() : mi->version_list.size();
    //printd(5, "check_module_version(%s %s %s) max=%d vs=%d ms=%d\n", mi->getVersion(), get_op_string(op), version->getString(), max, version->size(), mi->version_list.size());
    for (unsigned i = 0; i < max; ++i) {
        int mv = (i >= mi->version_list.size() ? 0 : mi->version_list[i]);
        int rv = (i >= version.size() ? 0 : version[i]);
        int res = check_component(op, mv, rv, i == (max - 1));
        if (res == MVC_FAIL) {
            QoreStringNode* desc = new QoreStringNodeMaker("loaded module '%s' does not satisfy the following requirement: %s %s %s", mi->getName(), mi->getVersion(), get_op_string(op), *version);
            try_add_module_context(desc);
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(mi->getName()), desc);
            return;
        }
        if (res == MVC_FINAL_OK)
            break;
    }
}

static int qore_check_load_module_intern(QoreAbstractModule* mi, mod_op_e op, version_list_t* version,
        QoreProgram* pgm, ExceptionSink& xsink, ExceptionSink& wsink, int warning_mask,
        QoreThreadLock* unlock_lock = nullptr) {
    if (xsink) {
        return -1;
    }

    assert(mi);
    // check version if necessary
    if (version) {
        check_module_version(mi, op, *version, xsink);
        if (xsink) {
            return -1;
        }
    }

    // attach any child modules declared with %try-child-module before the module is applied to the
    // requesting Program, so that the extensions a caller can see are complete as soon as the module is
    // available to it
    if (QMM.queueChildModules(*mi, xsink, wsink, warning_mask)) {
        return -1;
    }

    if (pgm) {
        // Module namespace init can execute Qore code (especially for AOT user
        // modules represented as binary modules).  Keep the global module map
        // serialized for lookup/registration, but do not hold it while applying
        // the module to a Program; init code may legitimately inspect or load
        // modules and the mutex is intentionally non-recursive.
        AutoUnlocker au(unlock_lock);
        mi->addToProgram(pgm, xsink);
        if (xsink) {
            return -1;
        }
    }
    return 0;
}

void QoreModuleManager::getUniqueName(QoreString& nname, const char* name, const char* prefix) {
    int ver = 1;
    while (true) {
        nname.sprintf("!!%s-%s-%d", prefix, name, ver++);
        if (!findModuleUnlocked(nname.c_str()))
            break;
        nname.clear();
    }
}

void QoreModuleManager::reinjectModule(QoreAbstractModule* mi) {
    // handle reinjections here
    QoreString nname;
    getUniqueName(nname, mi->getName(), "orig-");
    module_map_t::iterator i = map.find(mi->getName());
    assert(i != map.end());
    map.erase(i);
    mi->rename(nname);
    addModule(mi);
}

static const char* get_feature_from_path(QoreString& tmp) {
    char* buf = q_basename(tmp.c_str());
    size_t len = strlen(buf);
    tmp.set(buf, len, len + 1, QCS_DEFAULT);
    qore_offset_t i = tmp.find('-');
    if (i < 0) {
        i = tmp.find('.');
    }
    if (i > 0) {
        tmp.terminate(i);
    }
    return tmp.c_str();
}

QoreAbstractModule* QoreModuleManager::loadModuleIntern(ExceptionSink& xsink, ExceptionSink& wsink, const char* name,
        QoreProgram* pgm, bool reexport, mod_op_e op, version_list_t* version, const char* src, QoreProgram* mpgm,
        unsigned load_opt, int warning_mask, qore_binary_module_desc_t mod_desc_func, QoreProgram* path_pgm,
        bool* not_found) {
    assert(!version || (version && op != MOD_OP_NONE));

    ReferenceHolder<QoreProgram> pholder(mpgm, &xsink);
    // a module Program that is not handed on is destroyed without the module manager mutex
    UnlockedModuleProgramRelease<ReferenceHolder<QoreProgram>> pholder_release(pholder);

    // temporary string buffer
    QoreString tmp_buf;
    const char* raw_path;
    bool is_bin;
    // see if "name" is a path
    if (strchrs(name, "./\\")) {
        tmp_buf = name;
        raw_path = name;

        if (tmp_buf.size() > 5 && !strcasecmp(".qmod", tmp_buf.c_str() + tmp_buf.size() - 5)) {
            is_bin = true;
        } else {
            is_bin = false;
        }
        name = get_feature_from_path(tmp_buf);
    } else {
        raw_path = nullptr;
        is_bin = false;
    }

    //printd(5, "QoreModuleManager::loadModuleIntern() name: '%s' path: '%s' reexport: %d pgm: %p\n", name,
    //    raw_path ? raw_path : "n/a", reexport, pgm);
    QoreThreadLock* unlock_lock = &mutex;

    // Program used for parse option and module search path inheritance; when there is no target Program
    // (child module loads made by attachChildModules()), the parent module's Program is used instead, so
    // that a child is resolved against the same module search path as its parent
    QoreProgram* ctx_pgm = pgm ? pgm : path_pgm;

    // check for special "qore" feature
    if (!raw_path && !strcmp(name, "qore")) {
        if (version) {
            check_qore_version(name, op, *version, xsink);
        }
        return nullptr;
    }

    if (raw_path && !src && pgm && (pgm->getParseOptions() & PO_NO_FILESYSTEM)) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "cannot load module '%s' from "
            "path '%s' because PO_NO_FILESYSTEM is set", name, raw_path);
        return nullptr;
    }

    // check for recursive / concurrent loads
    while (true) {
        module_load_map_t::iterator i = module_load_map.find(name);
        if (i == module_load_map.end()) {
            break;                              // NOT_LOADED: we become the single writer
        }
        ModuleLoadEntry& e = i->second;
        if (e.owner_tid == q_gettid()) {
            // same thread already loading this feature: in-progress recursion, tolerated as before
            return nullptr;
        }
        if (e.state == MLS_INITIALIZING) {
            // another thread is initializing this feature; detect a genuine cross-thread cycle
            // before parking, so a real circular dependency raises instead of deadlocking silently
            const int owner_tid = e.owner_tid;
            if (checkModuleLoadCycle(name, owner_tid, xsink)) {
                return nullptr;
            }
            ++e.waiters;
            ++module_load_waiting;
            int wait_rc = module_load_cond.waitWithInterrupt(mutex, &xsink);
            --module_load_waiting;
            // NOTE: the entry may have been erased while we were unlocked; re-find below.  Decrement
            // the waiter count on the (possibly still-present) entry and drop our wait-for edge.
            module_load_map_t::iterator wi = module_load_map.find(name);
            if (wi != module_load_map.end() && wi->second.owner_tid == owner_tid) {
                assert(wi->second.waiters);
                --wi->second.waiters;
            }
            clearModuleLoadWaitEdge(q_gettid());
            if (wait_rc == QORE_COND_RESULT_INTERRUPTED) {
                return nullptr;
            }
            continue;                           // re-check terminal state / new owner
        }
        if (e.state == MLS_FAILED) {
            // the owning thread's load failed; propagate rather than silently re-running a broken
            // binary init in every waiter (thundering herd)
            std::string err = e.err.empty() ? std::string("LOAD-MODULE-ERROR") : e.err;
            std::string desc = e.desc;
            if (e.waiters == 0) {
                module_load_map.erase(i);       // last observer GCs the terminal entry
            }
            xsink.raiseException(err.c_str(), "module '%s' failed to load in another thread%s%s",
                name, desc.empty() ? "" : ": ", desc.c_str());
            return nullptr;
        }
        // MLS_LOADED: the module is registered; GC the terminal entry if we are the last observer and
        // fall through to the module-map lookup below, which returns the loaded module
        if (e.waiters == 0) {
            module_load_map.erase(i);
        }
        break;
    }

    module_map_t::iterator mmi = map.find(name);
    assert(mmi == map.end() || !strcmp(mmi->second->getName(), name));

    QoreAbstractModule* mi = (mmi == map.end() ? nullptr : mmi->second);

    // a module that code that is not trusted loaded from outside the module path does not satisfy, and cannot be
    // reloaded by, other code, nor code with another sandbox: its file and its state are the choice of that code
    if (mi && mi->isTenantChosen() && !mayUseTenantModule(*mi, pgm)) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "cannot use module '%s' (%s): it was "
            "loaded from outside the module path by code that a sandbox governs or whose functional domains are "
            "restricted, and cannot be used by other code, nor by code with another sandbox", name,
            mi->getFileName());
        return nullptr;
    }

    // handle module reloads
    if (load_opt & QMLO_RELOAD) {
        assert(!version);
        assert(!src);
        // only loaded & injected modules can be reloaded
        if (!mi || !mi->isInjected()) {
            return xsink ? nullptr : mi;
        }

        // rename module and make private
        map.erase(mmi);

        QoreString orig_name(mi->getName());
        // rename to unique name
        QoreString nname;
        getUniqueName(nname, mi->getName(), "private");
        mi->rename(nname);
        mi->setOrigName(orig_name.c_str());
        mi->setPrivate();
        assert(mi->isUser());
        addModule(mi);

        QoreString modulePath;
        if (raw_path) {
            QoreProgram* p = pgm ? pgm : (load_opt & (QMLO_REINJECT | QMLO_PRIVATE) && mpgm ? mpgm : nullptr);
            if (!p) {
                p = getProgram();
            }

            if (p) {
                modulePath = raw_path;
                q_normalize_path(modulePath, p->parseGetScriptDir());
                raw_path = modulePath.c_str();
            }
        }

        QoreAbstractModule* nmi = loadUserModuleFromPath(xsink, wsink, raw_path ? raw_path : mi->getFileName(),
            mi->getOrigName(), pgm, reexport, pholder.release(), load_opt & QMLO_REINJECT ? mpgm : nullptr, load_opt,
            warning_mask);
        if (xsink) {
            mmi = map.find(mi->getName());
            assert(mmi != map.end());
            map.erase(mmi);
            mi->resetName();
            mi->setPrivate(false);
            addModule(mi);
            return nullptr;
        }

        assert(umset.find(mi->getName()) == umset.end());
        nmi->setLink(mi);
        trySetUserModuleDependency(mi);
        return mi;
    }

    // if the feature already exists in this program, then return
    if (pgm && qore_program_private::get(*pgm)->hasFeature(name)) {
        //printd(5, "QoreModuleManager::loadModuleIntern() '%s' pgm %p has feature\n", name, pgm);

        if (load_opt & QMLO_INJECT) {
            xsink.raiseException("LOAD-MODULE-ERROR", "cannot load module '%s' for injection because the module "
                "has already been loaded", name);
        }

        // check version if necessary
        if (version) {
            // if no module is found, then this is a builtin feature
            if (!mi) {
                check_qore_version(name, op, *version, xsink);
            } else {
                check_module_version(mi, op, *version, xsink);
            }
        }

        if (mi) {
            trySetUserModuleDependency(mi);
        }
        return xsink ? nullptr : mi;
    }

    // check if parse options allow loading any modules at all
    if (pgm && (pgm->getParseOptions() & PO_NO_MODULES)) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "cannot load modules ('%s' " \
            "requested) into the current Program object because PO_NO_MODULES is set", name);
        return nullptr;
    }

    // if the feature already exists, then load the namespace changes into this program and register the feature
    if (mi && !(load_opt & QMLO_REINJECT)) {
        // module init failed on a prior load — don't try to add it to programs
        if (mi->isInitFailed()) {
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name),
                "module '%s' cannot be loaded because its initialization failed on a prior attempt", name);
            return nullptr;
        }
        if (load_opt & QMLO_INJECT) {
            xsink.raiseException("LOAD-MODULE-ERROR", "cannot load module '%s' for injection because the " \
                "module has already been loaded; to reinject a module, call Program::loadApplyToUserModule() " \
                "with the reinject flag set to True", name);
            return nullptr;
        }

        //printd(5, "QoreModuleManager::loadModuleIntern() name: %s inject: %d, reinject: %d found: %p (%s, %s) "
        //    "injected: %d reinjected: %d\n", name, load_opt & QMLO_INJECT, load_opt & QMLO_REINJECT, mi,
        //    mi->getName(), mi->getFileName(), mi->isInjected(), mi->isReInjected());

        int rc = qore_check_load_module_intern(mi, op, version, pgm, xsink, wsink, warning_mask, unlock_lock);
        // make sure to add reexport info if the module should be reexported
        if (reexport && !xsink) {
            ModuleReExportHelper mrh(mi, true);
        }
        return rc ? nullptr : mi;
    }

    //printd(5, "QoreModuleManager::loadModuleIntern() this: %p name: %s not found\n", this, name);

    // see if we are loading a user module from an explicit source
    if (src) {
        QoreString modulePath;
        if (raw_path) {
            QoreProgram* p = pgm ? pgm : (load_opt & (QMLO_REINJECT | QMLO_PRIVATE) && mpgm ? mpgm : nullptr);
            if (!p) {
                p = getProgram();
            }

            if (p) {
                modulePath = raw_path;
                q_normalize_path(modulePath, p->parseGetScriptDir());
                raw_path = modulePath.c_str();
            }
        }

        mi = loadUserModuleFromSource(xsink, wsink, raw_path ? raw_path : name, name, pgm, src, reexport,
            pholder.release(), warning_mask);
        return qore_check_load_module_intern(mi, op, version, pgm, xsink, wsink, warning_mask, unlock_lock) ? nullptr : mi;
    }

    // see if this is actually a path
    if (raw_path) {
        QoreString modulePath;
        QoreProgram* p = ctx_pgm ? ctx_pgm : (load_opt & (QMLO_REINJECT | QMLO_PRIVATE) && mpgm ? mpgm : nullptr);
        if (!p) {
            p = getProgram();
        }

        if (p) {
            modulePath = raw_path;
            q_normalize_path(modulePath, p->parseGetScriptDir());
            raw_path = modulePath.c_str();
        }

        if (is_bin) {
            if (mpgm) {
                xsink.raiseException("LOAD-MODULE-ERROR", "cannot load a binary module with a Program container");
                return nullptr;
            }
            if (load_opt & QMLO_REINJECT) {
                xsink.raiseException("LOAD-MODULE-ERROR", "cannot reinject module '%s' because reinjection is not "
                    "currently supported for binary modules", name);
                return nullptr;
            }

            QoreString source_path;
            QoreString separated_path;
            bool source_available = qore_find_explicit_qmod_source(raw_path, name, source_path, separated_path);
            // an AOT module of a build tree named by its path is out of date as when it is found in the module path
            // (see load_source_for_stale_aot_module below): when one of its sources (an included file among them) has
            // changed, its source module is loaded instead, with the same warning
            if (source_available) {
                // the directory where the source module is: that of the module directory, or of the file
                std::string source_dir = separated_path.size() ? separated_path.c_str() : source_path.c_str();
                const char* dsep = q_find_last_path_sep(source_dir.c_str());
                source_dir = dsep ? source_dir.substr(0, dsep - source_dir.c_str()) : std::string(".");
                std::string stale_source;
                if (qore_build_aot_module_is_stale(raw_path, source_dir, name, p, stale_source, xsink)) {
                    const char* src = separated_path.size() ? separated_path.c_str() : source_path.c_str();
                    mi = separated_path.size()
                        ? loadSeparatedModule(xsink, wsink, src, name, pgm, reexport, pholder.release(),
                            load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask)
                        : loadUserModuleFromPath(xsink, wsink, src, name, pgm, reexport, pholder.release(),
                            load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask);
                    if (mi && !xsink) {
                        qore_warn_stale_aot_module(xsink, wsink, warning_mask, name, raw_path, source_path.c_str(),
                            stale_source.c_str());
                    } else {
                        ExceptionSink stale_xsink;
                        stale_xsink.raiseExceptionArg("AOT-MODULE-STALE", new QoreStringNode(raw_path), "AOT module "
                            "'%s' for feature '%s' is out of date: its source '%s' has changed since it was compiled; "
                            "rebuild the AOT module", raw_path, name, stale_source.c_str());
                        qore_raise_binary_module_source_fallback_error(xsink, name, raw_path, source_path.c_str(),
                            stale_xsink);
                    }
                    return qore_check_load_module_intern(mi, op, version, pgm, xsink, wsink, warning_mask,
                        unlock_lock) ? nullptr : mi;
                }
                if (xsink) {
                    return nullptr;
                }
            } else {
                // no source module next to it (a module of a build tree named by its path in the build tree): an
                // out-of-date module cannot be replaced by its source, and is not loaded, as an AOT module whose
                // dependency is another module than the one it was compiled against
                std::string module_dir = raw_path;
                const char* dsep = q_find_last_path_sep(module_dir.c_str());
                module_dir = dsep ? module_dir.substr(0, dsep - module_dir.c_str()) : std::string(".");
                std::string stale_source;
                if (qore_build_aot_module_is_stale(raw_path, module_dir, name, p, stale_source, xsink)) {
                    xsink.raiseExceptionArg("AOT-MODULE-STALE", new QoreStringNode(raw_path), "AOT module '%s' for "
                        "feature '%s' is out of date: its source '%s' has changed since it was compiled, and there "
                        "is no source module next to it to load instead; rebuild the AOT module", raw_path, name,
                        stale_source.c_str());
                    return nullptr;
                }
                if (xsink) {
                    return nullptr;
                }
            }
            ExceptionSink binary_xsink;
            mi = loadBinaryModuleFromPath(binary_xsink, raw_path, name, reexport, pholder.release(), p,
                load_opt, mod_desc_func, source_available,
                &wsink == &xsink ? nullptr : &wsink, warning_mask);
            if (binary_xsink) {
                if (qore_binary_load_error_can_fallback_to_source(binary_xsink) && source_available) {
                    if (separated_path.size()) {
                        mi = loadSeparatedModule(xsink, wsink, separated_path.c_str(), name, pgm, reexport, nullptr,
                            load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask);
                    } else {
                        mi = loadUserModuleFromPath(xsink, wsink, source_path.c_str(), name, pgm, reexport, nullptr,
                            load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask);
                    }
                    if (mi && !xsink) {
                        qore_warn_binary_module_source_fallback(xsink, wsink, warning_mask, name, raw_path,
                            source_path.c_str(), binary_xsink);
                        binary_xsink.clear();
                    } else {
                        qore_raise_binary_module_source_fallback_error(xsink, name, raw_path, source_path.c_str(),
                            binary_xsink);
                    }
                } else {
                    xsink.assimilate(binary_xsink);
                }
            }
        } else if (QoreDir::folder_exists(modulePath, xsink)) {
            if (!has_separated_module_main(raw_path, name)) {
                xsink.raiseException("LOAD-MODULE-ERROR", "cannot load separated user module '%s' from directory "
                    "'%s': missing main module source '%s.qm'", name, raw_path, name);
                return nullptr;
            }
            mi = loadSeparatedModule(xsink, wsink, raw_path, name, pgm, reexport, pholder.release(),
                load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask);
        } else {
            // the entry source of a separated module given by its path is loaded with the module's other sources,
            // as when the module is found by name or its directory is given
            QoreString separated_path;
            std::string entry = std::string(name) + ".qm";
            if (entry == q_basenameptr(raw_path)
                    && qore_explicit_source_is_separated_main(raw_path, name, separated_path)) {
                mi = loadSeparatedModule(xsink, wsink, separated_path.c_str(), name, pgm, reexport,
                    pholder.release(), load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask);
            } else {
                mi = loadUserModuleFromPath(xsink, wsink, raw_path, name, pgm, reexport, pholder.release(),
                    load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask);
            }
        }

        return qore_check_load_module_intern(mi, op, version, pgm, xsink, wsink, warning_mask, unlock_lock) ? nullptr : mi;
    }

    // otherwise, try to find module in the module path
    QoreString str;
    struct stat sb;

    // Build the effective search path: per-Program prepended paths FIRST (most-recently-added at
    // index 0 per design doc), then the process-global moduleDirList, then per-Program appended
    // paths LAST.  We collect raw pointers to avoid copying strings.  See
    // design/qore-module-structure.md "Module Search Path".
    // The Program's lists include those of the AOT modules whose dependencies this thread is resolving for it, as
    // the Program of each such module will have them (see QoreModulePathScope)
    std::vector<const std::string*> search_paths;
    std::vector<std::string> ctx_prepended, ctx_appended;
    qore_program_private::getEffectiveModulePathLists(ctx_pgm, ctx_prepended, ctx_appended);
    for (const std::string& p : ctx_prepended) {
        search_paths.push_back(&p);
    }
    for (const std::string& p : moduleDirList) {
        search_paths.push_back(&p);
    }
    for (const std::string& p : ctx_appended) {
        search_paths.push_back(&p);
    }

    // Installed AOT artifacts and sources can occupy different module directories
    // (for example /usr/lib/<triplet>/qore-modules and /usr/share/qore-modules).
    // Locate fallback source in the effective search order before loading a binary,
    // so optional-dependency checks also know whether source is available.
    auto find_user_module_source_dir = [&](std::string& source_dir) -> bool {
        size_t count = 0;
        for (const std::string* path : search_paths) {
            if (++count % 100 == 0 && qore_check_cancel(&xsink, "module source lookup")) {
                return false;
            }
            QoreString source_path;
            bool separated = false;
            if (qore_find_user_module_source(*path, name, source_path, separated)) {
                source_dir = *path;
                return true;
            }
        }
        return false;
    };

    auto load_user_module_source = [&](const std::string& dir, bool& found,
            QoreString* found_source_path = nullptr) -> QoreAbstractModule* {
        QoreString source_path;
        bool separated = false;
        if (!qore_find_user_module_source(dir, name, source_path, separated)) {
            found = false;
            return nullptr;
        }
        found = true;

        if (!q_absolute_path(source_path.c_str())) {
            q_normalize_path(source_path);
        }

        if (separated) {
            if (found_source_path) {
                *found_source_path = source_path;
                found_source_path->sprintf(QORE_DIR_SEP_STR "%s.qm", name);
            }
            return loadSeparatedModule(xsink, wsink, source_path.c_str(), name, pgm, reexport, pholder.release(),
                load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask);
        }
        if (found_source_path) {
            *found_source_path = source_path;
        }
        return loadUserModuleFromPath(xsink, wsink, source_path.c_str(), name, pgm, reexport, pholder.release(),
            load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask);
    };

    // an AOT module of a build tree (linked into a source tree, or a built module with its depfile) is not loaded if
    // one of its sources has changed: the source module is loaded instead, with a warning that the AOT module must be
    // rebuilt; source_dir is the directory of the module path where its source module was found (with no source
    // module found there, the module is refused: see refuse_stale_aot_module_without_source below)
    auto load_source_for_stale_aot_module = [&](const char* qmod_path, const std::string& source_dir,
            QoreAbstractModule*& rv) -> bool {
        std::string stale_source;
        if (!qore_build_aot_module_is_stale(qmod_path, source_dir, name, ctx_pgm, stale_source, xsink)) {
            return false;
        }
        bool source_found = false;
        QoreString source_path;
        rv = load_user_module_source(source_dir, source_found, &source_path);
        if (!source_found) {
            // nothing to load instead: the out-of-date module is not loaded, as when it is named by its path (see
            // loadModuleIntern() above)
            xsink.raiseExceptionArg("AOT-MODULE-STALE", new QoreStringNode(qmod_path), "AOT module '%s' for feature "
                "'%s' is out of date: its source '%s' has changed since it was compiled, and there is no source "
                "module to load instead; rebuild the AOT module", qmod_path, name, stale_source.c_str());
            rv = nullptr;
            return true;
        }
        if (rv && !xsink) {
            qore_warn_stale_aot_module(xsink, wsink, warning_mask, name, qmod_path, source_path.c_str(),
                stale_source.c_str());
        } else {
            // the source failed: why the AOT module was not loaded follows its error, as for an AOT module that
            // fails to load (see qore_raise_binary_module_source_fallback_error())
            ExceptionSink stale_xsink;
            stale_xsink.raiseExceptionArg("AOT-MODULE-STALE", new QoreStringNode(qmod_path), "AOT module '%s' for "
                "feature '%s' is out of date: its source '%s' has changed since it was compiled; rebuild the AOT "
                "module", qmod_path, name, stale_source.c_str());
            qore_raise_binary_module_source_fallback_error(xsink, name, qmod_path, source_path.c_str(),
                stale_xsink);
        }
        return true;
    };

    // an out-of-date AOT module of a build tree without a source module to load instead is not loaded, as when it is
    // named by its path (see above); it is checked with the directory of the AOT module as the source directory (see
    // qore_build_aot_module_is_stale()), which only a linked module without a dependency file uses
    auto refuse_stale_aot_module_without_source = [&](const char* qmod_path) -> bool {
        std::string module_dir = qmod_path;
        const char* dsep = q_find_last_path_sep(module_dir.c_str());
        module_dir = dsep ? module_dir.substr(0, dsep - module_dir.c_str()) : std::string(".");
        std::string stale_source;
        if (!qore_build_aot_module_is_stale(qmod_path, module_dir, name, ctx_pgm, stale_source, xsink)) {
            return static_cast<bool>(xsink);
        }
        xsink.raiseExceptionArg("AOT-MODULE-STALE", new QoreStringNode(qmod_path), "AOT module '%s' for feature "
            "'%s' is out of date: its source '%s' has changed since it was compiled, and there is no source module "
            "to load instead; rebuild the AOT module", qmod_path, name, stale_source.c_str());
        return true;
    };

    for (const std::string* path_ptr : search_paths) {
        const std::string& dir = *path_ptr;
        // Per module directory, the preference order is:
        //   1. `<name>-api-<x>.<y>.qmod` (binary, matching an active API version)
        //   2. `<name>.qmod`             (binary, no API suffix — AOT-compiled user
        //                                 modules land here)
        //   3. `<name>.qm`               (user module source)
        //   4. `<name>/`                 (split module folder, handled below)
        // Binary forms (2) MUST be preferred over (3): when an AOT-compiled module
        // is deployed alongside its source, the host expects the compiled artifact
        // to win.  Prior to this structure, the `.qm` check was nested inside the
        // API-version loop and intercepted the lookup before the bare `.qmod`
        // fallthrough at ai==qore_mod_api_list_len, causing AOT builds to be
        // silently ignored (Phase 1.5 investigation, 2026-04-18).
        for (unsigned ai = 0; ai <= qore_mod_api_list_len; ++ai) {
            // build path to binary module
            str.clear();
            str.sprintf("%s" QORE_DIR_SEP_STR "%s", dir.c_str(), name);

            // make new extension string
            if (ai < qore_mod_api_list_len) {
                str.sprintf("-api-%d.%d.qmod", qore_mod_api_list[ai].major, qore_mod_api_list[ai].minor);
            } else {
                str.concat(".qmod");
            }

            //printd(5, "ModuleManager::loadModule(%s) trying binary module: %s\n", name, str.c_str());
            if (!stat(str.c_str(), &sb)) {
                if (mpgm) {
                    // `mpgm` means the caller needs to inject the module INTO a
                    // specific Program container (loadApplyToUserModule and
                    // friends).  Binary / AOT-compiled modules have their
                    // namespace baked in and cannot be injected this way.  Fall
                    // through to the `.qm` source search below rather than
                    // erroring — if the source form exists alongside the `.qmod`
                    // (common for qlib modules installed with both forms), use
                    // it.  If neither `.qm` nor a split module folder exists,
                    // loadModule will surface a "feature not found" error later,
                    // which is more informative than "binary + Program" here.
                    break;
                }
                // only a binary module without an API suffix falls back to the source module
                std::string source_dir;
                bool source_available = ai == qore_mod_api_list_len && find_user_module_source_dir(source_dir);
                if (xsink) {
                    return nullptr;
                }
                // an AOT user module has no API suffix; a native binary module is not checked against sources
                if (!source_available && ai == qore_mod_api_list_len
                        && refuse_stale_aot_module_without_source(str.c_str())) {
                    return nullptr;
                }
                if (source_available && load_source_for_stale_aot_module(str.c_str(), source_dir, mi)) {
                    return qore_check_load_module_intern(mi, op, version, pgm, xsink, wsink, warning_mask,
                        unlock_lock) ? nullptr : mi;
                }
                if (xsink) {
                    return nullptr;
                }
                ExceptionSink binary_xsink;
                mi = loadBinaryModuleFromPath(binary_xsink, str.c_str(), name, reexport, pholder.release(),
                    ctx_pgm, load_opt, mod_desc_func, source_available,
                    &wsink == &xsink ? nullptr : &wsink, warning_mask);
                if (binary_xsink) {
                    if (ai == qore_mod_api_list_len && qore_binary_load_error_can_fallback_to_source(binary_xsink)) {
                        bool source_found = false;
                        QoreString source_path;
                        if (source_available) {
                            mi = load_user_module_source(source_dir, source_found, &source_path);
                        }
                        if (source_found) {
                            if (mi && !xsink) {
                                qore_warn_binary_module_source_fallback(xsink, wsink, warning_mask, name,
                                    str.c_str(), source_path.c_str(), binary_xsink);
                                binary_xsink.clear();
                            } else {
                                qore_raise_binary_module_source_fallback_error(xsink, name, str.c_str(),
                                    source_path.c_str(), binary_xsink);
                            }
                        } else {
                            xsink.assimilate(binary_xsink);
                        }
                    } else {
                        xsink.assimilate(binary_xsink);
                    }
                }
                return qore_check_load_module_intern(mi, op, version, pgm, xsink, wsink, warning_mask, unlock_lock) ? nullptr : mi;
            }
        }

        // No flat `.qmod` form — try the subdir `.qmod` form.  Split-dir
        // AOT-compiled modules land at `<dir>/<name>/<name>.qmod` so that
        // `get_script_dir()` during AOT init resolves to a directory
        // containing sibling resources (logo SVGs, asyncapi YAMLs, etc.).
        // This path wins over the `.qm` source inside the same folder —
        // binary forms MUST be preferred over source, matching the flat
        // `.qmod` > `.qm` rule above.
        for (unsigned ai = 0; ai <= qore_mod_api_list_len; ++ai) {
            str.clear();
            str.sprintf("%s" QORE_DIR_SEP_STR "%s" QORE_DIR_SEP_STR "%s",
                dir.c_str(), name, name);
            if (ai < qore_mod_api_list_len) {
                str.sprintf("-api-%d.%d.qmod", qore_mod_api_list[ai].major, qore_mod_api_list[ai].minor);
            } else {
                str.concat(".qmod");
            }
            if (!stat(str.c_str(), &sb)) {
                if (mpgm) {
                    // As above: `mpgm` means the caller wants to inject
                    // into a specific Program container, which binary
                    // modules can't satisfy.  Fall through to source
                    // search.
                    break;
                }
                // only a binary module without an API suffix falls back to the source module
                std::string source_dir;
                bool source_available = ai == qore_mod_api_list_len && find_user_module_source_dir(source_dir);
                if (xsink) {
                    return nullptr;
                }
                // an AOT user module has no API suffix; a native binary module is not checked against sources
                if (!source_available && ai == qore_mod_api_list_len
                        && refuse_stale_aot_module_without_source(str.c_str())) {
                    return nullptr;
                }
                if (source_available && load_source_for_stale_aot_module(str.c_str(), source_dir, mi)) {
                    return qore_check_load_module_intern(mi, op, version, pgm, xsink, wsink, warning_mask,
                        unlock_lock) ? nullptr : mi;
                }
                if (xsink) {
                    return nullptr;
                }
                ExceptionSink binary_xsink;
                mi = loadBinaryModuleFromPath(binary_xsink, str.c_str(), name, reexport, pholder.release(),
                    ctx_pgm, load_opt, mod_desc_func, source_available,
                    &wsink == &xsink ? nullptr : &wsink, warning_mask);
                if (binary_xsink) {
                    if (ai == qore_mod_api_list_len && qore_binary_load_error_can_fallback_to_source(binary_xsink)) {
                        bool source_found = false;
                        QoreString source_path;
                        if (source_available) {
                            mi = load_user_module_source(source_dir, source_found, &source_path);
                        }
                        if (source_found) {
                            if (mi && !xsink) {
                                qore_warn_binary_module_source_fallback(xsink, wsink, warning_mask, name,
                                    str.c_str(), source_path.c_str(), binary_xsink);
                                binary_xsink.clear();
                            } else {
                                qore_raise_binary_module_source_fallback_error(xsink, name, str.c_str(),
                                    source_path.c_str(), binary_xsink);
                            }
                        } else {
                            xsink.assimilate(binary_xsink);
                        }
                    } else {
                        xsink.assimilate(binary_xsink);
                    }
                }
                return qore_check_load_module_intern(mi, op, version, pgm, xsink, wsink, warning_mask, unlock_lock) ? nullptr : mi;
            }
        }

        // No `.qmod` form exists in this directory — try the `.qm` source.
        str.clear();
        str.sprintf("%s" QORE_DIR_SEP_STR "%s.qm", dir.c_str(), name);
        if (!stat(str.c_str(), &sb)) {
            // see if this is a relative path; if so normalize it; we cannot send a relative path to
            // loadUserModuleFromPath(), since it will try to normalize the path using the current program's
            // directory as the cwd
            if (!q_absolute_path(str.c_str())) {
                q_normalize_path(str);
            }
            mi = loadUserModuleFromPath(xsink, wsink, str.c_str(), name, pgm, reexport, pholder.release(),
                load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask);
            return qore_check_load_module_intern(mi, op, version, pgm, xsink, wsink, warning_mask, unlock_lock) ? nullptr : mi;
        }

        // check whether it is a module folder
        QoreString modulePath(dir);
        modulePath += QORE_DIR_SEP_STR;
        modulePath += name;

        if (QoreDir::folder_exists(modulePath, xsink)) {
            if (!has_separated_module_main(modulePath.c_str(), name)) {
                continue;
            }
            // Search paths are relative to the process cwd, just as for flat .qm files above.
            // Resolve them before the separated loader normalizes metadata against the importing
            // Program's directory; otherwise relative paths acquire that directory a second time.
            if (!q_absolute_path(modulePath.c_str())) {
                q_normalize_path(modulePath);
            }
            //printd(5, "ModuleManager::loadModule(%s) found separated module: %s\n", name, modulePath.c_str());
            mi = loadSeparatedModule(xsink, wsink, modulePath.c_str(), name, pgm, reexport, pholder.release(),
                load_opt & QMLO_REINJECT ? mpgm : path_pgm, load_opt, warning_mask);
            return qore_check_load_module_intern(mi, op, version, pgm, xsink, wsink, warning_mask, unlock_lock) ? nullptr : mi;
        }
    }

    // the module does not exist in the module path; child module attachment uses this to tell an absent
    // child module (not an error) from one that is present but cannot be loaded
    if (not_found) {
        *not_found = true;
    }

    QoreStringNode* desc = new QoreStringNodeMaker("feature '%s' is not builtin and no module with this name could "
        "be found in the module path: ", name);
    if (!ctx_prepended.empty()) {
        bool first = true;
        for (const std::string& p : ctx_prepended) {
            desc->sprintf("%s%s", first ? "" : ":", p.c_str());
            first = false;
        }
        desc->concat(':');
    }
    moduleDirList.appendPath(*desc);
    for (const std::string& p : ctx_appended) {
        desc->sprintf(":%s", p.c_str());
    }
    xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), desc);
    return nullptr;
}


bool QoreModuleManager::isTrustedModulePath(const char* path, std::string* load_path) const {
    if (!path || !*path) {
        return false;
    }
    auto within = [] (const char* name, size_t nlen, const std::string& dir) -> bool {
        return !dir.empty() && nlen > dir.size() + 1 && !strncmp(name, dir.c_str(), dir.size())
            && name[dir.size()] == '/';
    };
    // the file as named, under a directory as configured: a module directory can hold links to the files of a
    // build; a name that is relative or has a "." or ".." component is not used
    size_t plen = strlen(path);
    if (path[0] == '/' && !strstr(path, "/../") && !strstr(path, "/./")) {
        for (const TrustedModuleDir& dir : trusted_module_dirs) {
            if (within(path, plen, dir.named)) {
                if (load_path) {
                    *load_path = path;
                }
                return true;
            }
        }
    }
    // the file it is, under a directory's canonical path
    std::unique_ptr<char, decltype(&free)> real(realpath(path, nullptr), &free);
    if (!real) {
        return false;
    }
    size_t rlen = strlen(real.get());
    for (const TrustedModuleDir& dir : trusted_module_dirs) {
        if (within(real.get(), rlen, dir.real) || within(real.get(), rlen, dir.named)) {
            if (load_path) {
                *load_path = real.get();
            }
            return true;
        }
    }
    return false;
}

//! Returns the file or directory in a module directory that a module with the name given is loaded from, or ""
/** The forms and their order are those of the module search in QoreModuleManager::loadModuleIntern(): a binary
    module for each active API version and an AOT module, flat (\c dir/name...) and then split
    (\c dir/name/name...), the source module \c dir/name.qm, and a separated module directory \c dir/name with
    \c name.qm in it.  The name is used as is: no pattern matching takes place.
*/
static std::string qore_find_module_in_dir(const std::string& dir, const char* name) {
    struct stat sb;
    for (bool split : {false, true}) {
        QoreString base(dir.c_str());
        base.sprintf(QORE_DIR_SEP_STR "%s", name);
        if (split) {
            base.sprintf(QORE_DIR_SEP_STR "%s", name);
        }
        for (unsigned ai = 0; ai <= qore_mod_api_list_len; ++ai) {
            QoreString path(base);
            if (ai < qore_mod_api_list_len) {
                path.sprintf("-api-%d.%d.qmod", qore_mod_api_list[ai].major, qore_mod_api_list[ai].minor);
            } else {
                path.concat(".qmod");
            }
            if (!stat(path.c_str(), &sb)) {
                return path.c_str();
            }
        }
    }
    QoreString path(dir.c_str());
    path.sprintf(QORE_DIR_SEP_STR "%s.qm", name);
    if (!stat(path.c_str(), &sb)) {
        return path.c_str();
    }
    path = dir.c_str();
    path.sprintf(QORE_DIR_SEP_STR "%s", name);
    if (!stat(path.c_str(), &sb) && S_ISDIR(sb.st_mode) && has_separated_module_main(path.c_str(), name)) {
        return path.c_str();
    }
    return std::string();
}

std::string QoreModuleManager::findTrustedModule(const char* name) const {
    // a feature name has no directory separator; one that has one names no module in a module directory
    if (!name || !*name || strchr(name, '/') || strchr(name, '\\')) {
        return std::string();
    }
    for (const TrustedModuleDir& td : trusted_module_dirs) {
        for (const std::string* dir : {&td.named, &td.real}) {
            if (dir->empty() || (dir == &td.real && td.real == td.named)) {
                continue;
            }
            std::string found = qore_find_module_in_dir(*dir, name);
            if (!found.empty()) {
                return found;
            }
        }
    }
    return std::string();
}

int QoreModuleManager::checkShadowingLoad(ExceptionSink& xsink, const char* path, const char* feature,
        bool trusted, bool& tenant_chosen, QoreSandboxManager*& owner, qore_program_id_t& owner_pgm_id,
        qore_shadow_load_e kind) const {
    tenant_chosen = false;
    owner = nullptr;
    owner_pgm_id = QORE_NO_PROGRAM_ID;
    if (trusted || !qore_caller_is_untrusted()) {
        return 0;
    }
    // a private module is registered under a name of its own, which no other code requests
    std::string library = kind == QSL_PRIVATE_CONTAINER ? std::string() : findTrustedModule(feature);
    if (!library.empty() && kind == QSL_CONTAINER) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(feature), "cannot load module '%s' into a "
            "Program container: code that a sandbox governs or whose functional domains are restricted cannot load "
            "a module into a Program container of its own under the name of module '%s' in the module path, which it "
            "would replace for every later user; load it into a private container instead", feature,
            library.c_str());
        return -1;
    }
    if (!library.empty()) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(feature), "cannot load module '%s' from "
            "path '%s': code that a sandbox governs or whose functional domains are restricted cannot load a module "
            "outside the module path under the name of module '%s' in the module path, which it would replace for "
            "every later user", feature, path, library.c_str());
        return -1;
    }
    tenant_chosen = true;
    owner = qore_caller_sandbox_ref();
    // code with no sandbox is told apart by its Program
    if (!owner) {
        owner_pgm_id = qore_caller_owner_program_id();
    }
    return 0;
}

bool QoreModuleManager::mayUseTenantModule(const QoreAbstractModule& mi, const QoreProgram* pgm) {
    assert(mi.isTenantChosen());
    // library code does not bind to a module that a tenant chose, whoever loaded it, when it is initialized or later
    // (a module loaded into a library module's Program at run time is used by every caller)
    if (!qore_caller_is_untrusted() || qore_module_init_library()
        || (pgm && qore_program_private::get(*const_cast<QoreProgram*>(pgm))->library_module)) {
        return false;
    }
    ReferenceHolder<QoreSandboxManager> sm(qore_caller_sandbox_ref(), nullptr);
    if (sm.operator->() != mi.tenantOwner()) {
        return false;
    }
    return sm || qore_caller_owner_program_id() == mi.tenantOwnerProgramId();
}

//! Gives a source module's Program the sandbox manager of the Program loading it, unless the module is library code
/** A module from a trusted module directory (see QoreModuleManager::isTrustedModulePath()) is initialized with no
    sandbox provenance, and every later call into it is governed by its caller; any other module, which a caller chose
    by path or through its own module paths, is governed by the sandbox of the Program that loads it first
*/
static void inherit_module_sandbox_manager(QoreProgram& module_pgm, QoreProgram* parent_pgm, bool trusted) {
    if (trusted || !parent_pgm || &module_pgm == parent_pgm) {
        return;
    }
    QoreSandboxManager* sm = qore_program_private::get(*parent_pgm)->getSandboxManagerRef();
    // loaded by library code for code that is not trusted: governed by the sandbox of that code
    if (!sm) {
        sm = qore_module_init_lift_manager_ref();
    }
    // a module that sandboxed code chose, loaded into a Program that has no manager (Program::loadModule() on a
    // Program that it was handed, or trusted code that it called), has the sandbox of that code, as when it loads the
    // module into its own Program; the tenant chose the path (see checkShadowingLoad()) (issue #5659)
    const bool sandboxed_caller = qore_caller_is_sandboxed();
    if (!sm && sandboxed_caller) {
        sm = qore_caller_sandbox_ref();
    }
    module_pgm.setSandboxManager(sm);
    // a module that a sandboxed Program without a manager of its own (one that sandboxed code created), or a
    // sandboxed caller, chose is sandboxed as well; its code is governed by the sandbox of its callers
    if (sandboxed_caller || qore_program_is_sandboxed(parent_pgm)) {
        qore_program_mark_sandboxed(module_pgm);
    }
    if (sm) {
        sm->deref(nullptr);
    }
}

//! Returns the parse options of the Program loading a user module that the module inherits
/** A module is parsed with the restrictions and permissions of the Program loading it, but not with the options that
    only select language behavior (QoreParseOptions::FREE_OPTIONS), which would change what the module's own code
    means, nor with the ones the caller handles itself (\a exclude).  The full-width mask is required: the complement
    of a legacy integer mask has an empty high word and would drop every extended option.
*/
static QoreParseOptions get_inherited_module_parse_options(const QoreParseOptions& parent_po,
        const QoreParseOptions& exclude) {
    return parent_po & ~(QoreParseOptions::FREE_OPTIONS | exclude);
}

QoreParseOptions qore_get_inherited_module_parse_options(const QoreParseOptions& parent_po,
        const QoreParseOptions& exclude) {
    return get_inherited_module_parse_options(parent_po, exclude);
}

//! Returns the parse options a user module loaded by a Program inherits from it, with a parse option mask applied
/** PO_ENABLE_DEBUG is inherited only from a Program that may control the process; the mask, which holds only
    restrictions (QoreModuleParseOptionMask::maskable()), strips restrictions from the inherited options and adds
    restrictions to them, also when there is no loading Program.

    @param p the Program loading the module, if any
    @param exclude the options the module does not inherit
    @param mask the module parse option mask
*/
static QoreParseOptions get_masked_module_parse_options(QoreProgram* p, const QoreParseOptions& exclude,
        const QoreModuleParseOptionMask& mask) {
    QoreParseOptions inherited;
    if (p) {
        QoreParseOptions parent_po = p->getParseOptions();
        // PO_ENABLE_DEBUG is not propagated automatically: only from a parent that may control the process
        inherited = get_inherited_module_parse_options(parent_po, exclude | PO_ENABLE_DEBUG);
        if ((parent_po & PO_ENABLE_DEBUG) && !(parent_po & PO_NO_PROCESS_CONTROL)) {
            inherited |= PO_ENABLE_DEBUG;
        }
    }
    return mask.apply(inherited);
}

QoreParseOptions qore_get_masked_module_parse_options(QoreProgram* p, const QoreParseOptions& exclude,
        const QoreModuleParseOptionMask& mask) {
    return get_masked_module_parse_options(p, exclude, mask);
}

QoreAbstractModule* QoreModuleManager::loadSeparatedModule(ExceptionSink& xsink, ExceptionSink& wsink,
        const char* path, const char* feature, QoreProgram* pgm, bool reexport, QoreProgram* mpgm,
        QoreProgram* path_pgm, unsigned load_opt, int warning_mask) {
    ReferenceHolder<QoreProgram> pholder(mpgm, &xsink);
    // a module Program that is not handed on is destroyed without the module manager mutex
    UnlockedModuleProgramRelease<ReferenceHolder<QoreProgram>> pholder_release(pholder);
    assert(feature);
    printd(5, "QoreModuleManager::loadSeparatedModule() path: %s, feature: %s, pgm: %p, reexport: %d, mpgm: %p, "
        "path_pgm: %p, load_opt: %d warning_mask: %d\n", path, feature, pgm, reexport, mpgm, path_pgm,
        load_opt, warning_mask);
    if (module_load_check(feature)) {
        xsink.raiseException("LOAD-MODULE-ERROR", "cannot load user module '%s'; recursive module dependency "
            "detected", feature);
        return nullptr;
    }
    ON_BLOCK_EXIT(module_load_clear, feature);

    // a library module that code that is not trusted loads into a Program container of its own is parsed and
    // initialized with the container's code and parse defines: it is not library code
    const bool untrusted_container = mpgm && qore_caller_is_untrusted();
    // a module from the process-wide module path is library code: it is initialized with no sandbox provenance
    std::string trusted_path;
    const bool trusted = !untrusted_container && isTrustedModulePath(path, &trusted_path);
    if (trusted) {
        // the file that was found to be library code is the file that is loaded
        path = trusted_path.c_str();
    }
    // code that is not trusted cannot replace a library module with a module from elsewhere, nor with one in a
    // container of its own; a private module has a name of its own
    bool tenant_chosen;
    QoreSandboxManager* owner;
    qore_program_id_t owner_pgm_id;
    if (checkShadowingLoad(xsink, path, feature, trusted, tenant_chosen, owner, owner_pgm_id,
            untrusted_container ? ((load_opt & QMLO_PRIVATE) ? QSL_PRIVATE_CONTAINER : QSL_CONTAINER) : QSL_LOAD)) {
        return nullptr;
    }
    // recorded on the module when it is registered
    QoreModuleLoadFrameHelper load_frame(feature, trusted, tenant_chosen, owner, owner_pgm_id);
    // library code loaded by code that is not trusted does not take what that code chose (see below); checked before
    // the barrier is set
    const bool untrusted_loader = trusted && qore_caller_is_untrusted();
    QoreModuleInitSandboxBarrierHelper init_barrier(trusted);
    if (init_barrier.failed()) {
        xsink.outOfMemory();
        return nullptr;
    }

    QoreParseCountContextHelper pcch;

    // Find the best program to inherit options from
    QoreProgram* p = pgm ? pgm : path_pgm;
    if (!p) {
        p = mpgm;
        if (!p) {
            p = getProgram();
        }
    }

    // parse options for the module: those inherited from the current program, if any, with the module parse option
    // mask applied, disabling style and types options already set with USER_MOD_PO
    const QoreModuleParseOptionMask load_mask = getLoadParseOptionMask();
    QoreParseOptions parseOptions = USER_MOD_PO
        | get_masked_module_parse_options(p, PO_REQUIRE_TYPES | PO_NO_GLOBAL_VARS, load_mask);

    QoreString modulePath(path);
    modulePath += QORE_DIR_SEP_STR;
    modulePath += feature;
    modulePath += ".qm";

    const char* td = p ? p->parseGetScriptDir() : nullptr;

    if (mpgm) {
        qore_program_private::forceReplaceParseOptions(*mpgm, parseOptions);
    } else {
        pholder = mpgm = new QoreProgram(parseOptions);
        mpgm->setScriptPath(modulePath.c_str());
        // Inherit user-set parse defines from the parent so module-source
        // %ifdef/%ifndef directives evaluate against the caller's view (see
        // qore_program_private::inheritParseDefines for rationale).  Also
        // inherit reexport-marked symbol imports and the parent's
        // %prepend-module-path / %append-module-path lists (see
        // qore_program_private::inheritModulePathLists), so the module's own
        // nested %requires(reexport) dependencies resolve against the same
        // module search surface the importer established.
        if (trusted) {
            qore_program_private::get(*mpgm)->library_module = true;
        }
        if (p) {
            // library code loaded by code that is not trusted takes only the defines that that code's host set, and
            // does not resolve its own dependencies, optional dependencies and child modules through that code's
            // module paths; a library module's own Program has only library code's own
            const bool from_untrusted = untrusted_loader && !qore_program_private::get(*p)->library_module;
            qore_program_private::inheritParseDefines(*mpgm, *p, from_untrusted);
            // only trusted code can mark imports for re-export (see Program::importClass())
            qore_program_private::inheritParseImports(*mpgm, *p, &xsink);
            if (!from_untrusted) {
                qore_program_private::inheritModulePathLists(*mpgm, *p);
            }
        }
    }
    // A source module has its own Program, so inherit the caller's sandbox manager explicitly.  This makes module
    // constant initialization subject to the caller's sandbox even though it does not execute through a normal
    // caller Program frame.  A narrowly-scoped exception in QoreSandboxManager permits read-only access to the
    // module's own resources while its module context is active.
    inherit_module_sandbox_manager(*mpgm, p, trusted);
    // inherit execution mode from parent program
    if (p) {
        // an explicitly requested mode applies to the module's Program as to a child Program (issue #5687)
        qore_program_private::get(*mpgm)->inheritExecMode(qore_program_private::get(*p));
    }
    // issue #3592: must add feature first
    if (qore_program_private::get(*mpgm)->addUserFeature(feature)) {
        xsink.raiseException("LOAD-MODULE-ERROR", "cannot load user module '%s'; feature '%s' is already loaded in "
            "this Program container", path, feature);
        return nullptr;
    }

    std::unique_ptr<QoreUserModule> userModule(new QoreUserModule(pholder.release(), td, modulePath.c_str(), feature,
        load_opt, warning_mask, path));
    // a module that fails to load is destroyed without the module manager mutex
    UnlockedModuleProgramRelease<std::unique_ptr<QoreUserModule>> userModule_release(userModule);
    userModule->addParseOptionMask(load_mask);

    ModuleReExportHelper reExportHelper(userModule.get(), reexport);
    QoreUserModuleDefContextHelper qmd(feature, path,  mpgm, xsink);

    std::string moduleCode = QoreDir::get_file_content(modulePath.c_str());

    // the module's sources are parsed from memory, so apply the default a file-based module gets from parseFile()
    qore_program_private::get(*userModule->getProgram())->applyModernSourceDefault(modulePath.c_str());

    {
        ModuleLoadMapHelper mlmh(feature, xsink);

        // issue #3212: warning sink
        userModule->getProgram()->parsePending(moduleCode.c_str(), path, &xsink, &xsink, warning_mask);
        if (xsink) {
            xsink.appendLastDescription(" (while loading user module \"%s\" from path \"%s\")", feature, path);
            return nullptr;
        }

        QoreDir moduleDir(&xsink, QCS_DEFAULT, path);
        ReferenceHolder<QoreListNode> fileList(moduleDir.listModuleSources(&xsink), &xsink);
        if (xsink) {
            xsink.appendLastDescription(" (while loading user module \"%s\" from path \"%s\")", feature, path);
            return nullptr;
        }
        int file_warning_mask = qore_program_private::getModuleFileWarningMask(userModule->getProgram());
        for (size_t i = 0; i < fileList->size(); ++i) {
            if (!(i % 10) && qore_check_cancel(&xsink, "module source parsing")) {
                return nullptr;
            }
            QoreString filePath(path);
            filePath += QORE_DIR_SEP_STR;
            QoreStringValueHelper file(fileList->retrieveEntry(i));
            filePath += file->c_str();

            std::string fileCode = QoreDir::get_file_content(filePath);
            userModule->getProgram()->parsePending(fileCode.c_str(), filePath.c_str(), &xsink, &xsink,
                file_warning_mask);
            if (xsink) {
                xsink.appendLastDescription(" (while loading user module \"%s\" from path \"%s\")", feature, path);
                return nullptr;
            }
        }
        userModule->getProgram()->parseCommit(&xsink);
    }
    if (xsink) {
        xsink.appendLastDescription(" (while loading user module \"%s\" from path \"%s\")", feature, path);

        return nullptr;
    }

    return setupUserModule(xsink, userModule, qmd, load_opt, warning_mask);
}

void ModuleManager::registerUserModuleFromSource(const char* name, const char* src, QoreProgram* pgm, ExceptionSink* xsink) {
    QMM.registerUserModuleFromSource(name, src, pgm, *xsink);
}

QoreStringNode* ModuleManager::parseLoadModule(const char* name, QoreProgram* pgm) {
    ExceptionSink xsink;

    if (!QMM.parseLoadModule(xsink, xsink, name, pgm)) {
        assert(!xsink);
        return nullptr;
    }

    return loadModuleError(name, xsink);
}

//! Parses a module specification of the form "<feature>[<op> <version>]"
/** @param spec the specification as given in the source
    @param name the feature name is returned here
    @param op the version operator is returned here, or MOD_OP_NONE if no version constraint was given
    @param version the version is returned here; only valid if \a op is not MOD_OP_NONE
    @param xsink exception sink

    @return 0 for OK, -1 if an exception was raised
*/
static int qore_parse_module_spec(const char* spec, QoreString& name, mod_op_e& op, version_list_t& version,
        ExceptionSink& xsink) {
    op = MOD_OP_NONE;

    char* p = strchrs(spec, "<>=");
    if (!p) {
        name = spec;
        name.trim();
        return 0;
    }

    name.set(spec, p - spec);
    name.trim();

    QoreString ops;
    do {
        if (!qore_isblank(*p)) {
            ops.concat(*p);
        }
        ++p;
    } while (*p == '<' || *p == '>' || *p == '=' || qore_isblank(*p));

    // get version operator
    if (!ops.compare("<")) {
        op = MOD_OP_LT;
    } else if (!ops.compare("<=")) {
        op = MOD_OP_LE;
    } else if (!ops.compare("=") || !ops.compare("==")) {
        op = MOD_OP_EQ;
    } else if (!ops.compare(">=")) {
        op = MOD_OP_GE;
    } else if (!ops.compare(">")) {
        op = MOD_OP_GT;
    } else {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(spec), "module '%s': cannot parse "
            "module operator '%s'; expecting one of: '<', '<=', '=', '>=', or '>'", spec, ops.c_str());
        return -1;
    }

    char ec = version.set(p);
    if (ec) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(spec), "module '%s': only numeric "
            "digits and '.' characters are allowed in module/feature version specifications, got '%c'", spec, ec);
        return -1;
    }

    if (!version.size()) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(spec), "module '%s': empty version "
            "specification given in feature/module request", spec);
        return -1;
    }

    return 0;
}

bool QoreModuleManager::findAvailableOptionalModule(const std::vector<std::string>& specs, QoreProgram* path_pgm,
        std::string& available) {
    for (size_t i = 0; i < specs.size(); ++i) {
        if (i && !(i % 10) && qore_check_cancel(nullptr, "AOT optional module check")) {
            return false;
        }
        ExceptionSink tmp;
        QoreString name;
        mod_op_e op;
        version_list_t version;
        if (qore_parse_module_spec(specs[i].c_str(), name, op, version, tmp)) {
            tmp.clear();
            continue;
        }
        loadModuleIntern(tmp, tmp, name.c_str(), nullptr, false, op, op == MOD_OP_NONE ? nullptr : &version,
            nullptr, nullptr, QMLO_NONE, 0, nullptr, path_pgm);
        if (!tmp) {
            available = specs[i];
            return true;
        }
        tmp.clear();
    }
    return false;
}

void QoreModuleManager::getModuleCandidateFiles(const char* name, QoreProgram* pgm,
        std::vector<std::string>& files) {
    std::vector<std::string> dirs;
    {
        AutoLocker al(mutex);
        std::vector<std::string> prepended, appended;
        qore_program_private::getEffectiveModulePathLists(pgm, prepended, appended);
        dirs.insert(dirs.end(), prepended.begin(), prepended.end());
        for (const std::string& dir : moduleDirList) {
            dirs.push_back(dir);
        }
        dirs.insert(dirs.end(), appended.begin(), appended.end());
    }

    auto add = [&files](const QoreString& path) {
        struct stat sb;
        if (!stat(path.c_str(), &sb) && S_ISREG(sb.st_mode)) {
            char* resolved = realpath(path.c_str(), nullptr);
            files.emplace_back(resolved ? resolved : path.c_str());
            free(resolved);
        }
    };
    for (size_t i = 0; i < dirs.size(); ++i) {
        if (i && !(i % 10) && qore_check_cancel(nullptr, "module candidate file search")) {
            return;
        }
        // the flat and split module forms
        for (const char* sub : {"", name}) {
            QoreString base(dirs[i]);
            if (*sub) {
                base.sprintf(QORE_DIR_SEP_STR "%s", sub);
            }
            for (unsigned ai = 0; ai < qore_mod_api_list_len; ++ai) {
                QoreString path(base);
                path.sprintf(QORE_DIR_SEP_STR "%s-api-%d.%d.qmod", name, qore_mod_api_list[ai].major,
                    qore_mod_api_list[ai].minor);
                add(path);
            }
            QoreString path(base);
            path.sprintf(QORE_DIR_SEP_STR "%s.qmod", name);
            add(path);
            path = base;
            path.sprintf(QORE_DIR_SEP_STR "%s.qm", name);
            add(path);
        }
    }
}

int QoreModuleManager::parseLoadModule(ExceptionSink& xsink, ExceptionSink& wsink, const char* name,
        QoreProgram* pgm, bool reexport, const QoreProgramLocation* loc) {
    //printd(5, "ModuleManager::parseLoadModule(name: %s, pgm: %p, reexport: %d)\n", name, pgm, reexport);

    assert(!xsink);

    QoreString str;
    mod_op_e mo;
    version_list_t iv;
    if (qore_parse_module_spec(name, str, mo, iv, xsink)) {
        return -1;
    }

    QoreAbstractModule* mod = nullptr;
    {
        // the warnings of the module loader for this load are reported by the parse action of pgm (see
        // qore_report_module_load_warning())
        QoreModuleLoadWarningFrameHelper mlwfh(pgm, loc);
        OptLocker ol(&mutex);
        // Load module; handle reexport flag separately to ensure correct module context
        mod = loadModuleIntern(xsink, wsink, str.c_str(), pgm, false, mo, mo == MOD_OP_NONE ? nullptr : &iv);
        // If reexport directive is present, register module for reexport (module context is correct here)
        if (reexport && mod && !xsink) {
            ModuleReExportHelper mrh(mod, true);
        }
    }

    if (mod) {
        assert(!xsink);
        qore_program_private::get(*pgm)->addParseModule(mod->getName());
    }

    return xsink ? -1 : 0;
}

ChildAttachHelper::ChildAttachHelper(QoreAbstractModule& mi, const std::string& name) : mi(mi), name(name) {
    assert(!mi.children_attaching);
    mi.children_attaching = true;
    child_attach_set.insert(name);
}

ChildAttachHelper::~ChildAttachHelper() {
    mi.children_attaching = false;
    child_attach_set.erase(name);
    // wake any thread waiting for this module's children to be attached
    if (QMM.module_load_waiting) {
        QMM.module_load_cond.broadcast();
    }
}

//! Reports a child module declaration that could not be honored
/** A child declared with %try-child-module is optional, so neither a skipped nor a failed child is an
    error for the parent; the diagnostic is reported as a warning instead.

    A broken child is reported unconditionally, as \c MODULE-CHILD-FAILED: if no caller can receive the
    warning (see qore_report_module_load_warning()), as when warnings are disabled, the message is written to
    \c stderr.  A broken extension must never be silently indistinguishable from an absent one.  For a module
    loaded by \c %%requires, which has no warning sink separate from its exception sink (raising the warning
    there would make it a parse error), the warning is a parse warning of the code with the directive.

    A skipped child (\c MODULE-CHILD-SKIPPED) reflects a deliberate restriction in the container Program and is
    only reported when warnings are enabled.
*/
static void qore_report_child_module_not_attached(ExceptionSink& xsink, ExceptionSink& wsink,
        int warning_mask, const char* mod_name, const ChildModuleInfo& c) {
    const bool failed = c.status == CMS_FAILED;
    if (!warning_mask && !failed) {
        return;
    }

    SimpleRefHolder<QoreStringNode> warn_desc(failed
        ? new QoreStringNodeMaker("module '%s': child module '%s' declared with %%try-child-module is "
            "present but failed to load and was skipped; the module was loaded without it: %s: %s",
            mod_name, c.name.c_str(), c.err.c_str(), c.desc.c_str())
        : new QoreStringNodeMaker("module '%s': child module '%s' declared with %%try-child-module was "
            "not attached: %s", mod_name, c.name.c_str(), c.desc.c_str()));

    // reported where the other warnings of the module loader go (see qore_report_module_load_warning()); the
    // warning's argument is the name of the child module
    qore_report_module_load_warning(xsink, &xsink == &wsink ? nullptr : &wsink, warning_mask, failed,
        failed ? "MODULE-CHILD-FAILED" : "MODULE-CHILD-SKIPPED", c.name.c_str(), warn_desc.release());
}

bool QoreModuleManager::hasUserModuleDependencyPath(const std::string& from, const std::string& to) {
    strset_t seen;
    std::vector<std::string> stack;
    stack.push_back(from);
    while (!stack.empty()) {
        const std::string cur = stack.back();
        stack.pop_back();
        if (!seen.insert(cur).second) {
            continue;
        }
        const strset_t* deps = md_map.getDeps(cur);
        if (!deps) {
            continue;
        }
        if (deps->find(to) != deps->end()) {
            return true;
        }
        for (const std::string& d : *deps) {
            stack.push_back(d);
        }
    }
    return false;
}

int QoreModuleManager::attachChildModules(QoreAbstractModule& mi, ExceptionSink& xsink, ExceptionSink& wsink,
        int warning_mask) {
    if (!mi.hasChildModules()) {
        return 0;
    }

    const std::string mname = mi.getName();

    while (true) {
        if (mi.children_done) {
            return 0;
        }

        // ignore re-entrant attaches in this thread; this happens when two modules declare each other as
        // children, and when a child module requires its own parent
        if (child_attach_set.find(mname) != child_attach_set.end()) {
            return 0;
        }

        if (!mi.children_attaching) {
            break;
        }

        // another thread is attaching this module's children; wait for it to finish so that this load does
        // not return before the children have been registered.  No module-load reservation is held here (the
        // attach runs after the parent's load has completed), so this wait cannot form a load cycle
        ++module_load_waiting;
        int wait_rc = module_load_cond.waitWithInterrupt(mutex, &xsink);
        --module_load_waiting;
        if (wait_rc == QORE_COND_RESULT_INTERRUPTED) {
            return -1;
        }
    }

    ChildAttachHelper cah(mi, mname);

    // the children of a library module are attached as library code: a module that a tenant chose is not one of
    // them, and a child that is not library code is governed by the sandbox of the code that loaded the parent
    QoreModuleInitSandboxBarrierHelper library_attach(mi.isTrusted());

    // parse options and module search paths for child loads come from the parent module's Program, so that
    // child resolution does not depend on which Program triggered the parent's load; AOT-compiled user
    // modules are loaded as binary modules but have a module Program registered with the AOT runtime
    QoreProgram* ppgm = mi.isUser()
        ? static_cast<QoreUserModule&>(mi).getProgram()
        : qore_aot_get_module_pgm(mi.getName());
    const bool no_modules = ppgm && (ppgm->getParseOptions() & PO_NO_MODULES);

    bool all_done = true;
    for (ChildModuleInfo& c : mi.children) {
        // CMS_FAILED is final for the life of the process: a module load failure is deterministic, so
        // retrying the child's failing parse on every load of the parent would only repeat the cost and the
        // diagnostic.  The recorded failure stays visible in the module hash
        if (c.status == CMS_ATTACHED || c.status == CMS_ABSENT || c.status == CMS_FAILED) {
            continue;
        }

        if (no_modules) {
            // module loading is not allowed in the parent module's Program; degrade predictably and retry
            // the attach if the module is loaded again in a context where modules are allowed
            c.status = CMS_SKIPPED;
            c.desc = "module loading is not allowed in the parent module's Program object (PO_NO_MODULES)";
            all_done = false;
            qore_report_child_module_not_attached(xsink, wsink, warning_mask, mi.getName(), c);
            continue;
        }

        ExceptionSink cx;
        QoreString cname;
        mod_op_e op = MOD_OP_NONE;
        version_list_t version;
        bool not_found = false;
        QoreAbstractModule* cmi = nullptr;
        if (!qore_parse_module_spec(c.spec.c_str(), cname, op, version, cx)) {
            printd(5, "QoreModuleManager::attachChildModules() '%s': attaching child '%s'\n", mi.getName(),
                cname.c_str());
            // the child's warnings are its own: with no warning sink of the caller's separate from its exception
            // sink (the %requires path), they are reported as for a module loaded with %requires (see
            // qore_report_module_load_warning()), and not raised in the caller's sink, where they would fail the
            // import of the module that declares the child
            cmi = loadModuleIntern(cx, &xsink == &wsink ? cx : wsink, cname.c_str(), nullptr, false, op,
                op == MOD_OP_NONE ? nullptr : &version, nullptr, nullptr, QMLO_NONE, warning_mask, nullptr,
                ppgm, &not_found);
        }

        if (!cx && cmi && qore_is_aot_user_module(cmi->getName())) {
            // Source child modules execute their init closure during global registration.  AOT module
            // initialization is deferred until the module is applied to a Program, but child attachment
            // intentionally loads with pgm=nullptr so the optional child cannot change the parent's public
            // namespace.  Run the AOT child initializer against its private module Program to preserve both
            // contracts: the extension side effects are active, and no child symbols are reexported.
            AutoUnlocker au(&mutex);
            qore_aot_initialize_module(cmi->getName(), cx);
        }

        if (!cx) {
            // a null module with no exception means that the child's load is already in progress in this
            // thread (i.e. the child was loaded explicitly and pulled its parent in); it will complete
            // normally, so it is attached in either case
            c.status = CMS_ATTACHED;
            // ensure that the parent is only deleted after the child, which holds references to the parent's
            // classes; binary modules are not deleted by delUser() and must not be tracked here
            if (cmi && cmi->isUser() && mi.isUser()) {
                // do not record the edge if the parent must already be deleted before the child: that
                // happens when modules declare each other as children, and a cycle in the dependency map
                // would make module teardown impossible.  The existing edge, which comes from a %requires,
                // reflects a real symbol dependency and is the one to keep
                if (hasUserModuleDependencyPath(cmi->getName(), mi.getName())) {
                    printd(5, "QoreModuleManager::attachChildModules() '%s': not adding a dependency on child "
                        "'%s'; the child already depends on this module\n", mi.getName(), cmi->getName());
                } else {
                    setUserModuleDependency(mi.getName(), cmi->getName());
                }
            }
            continue;
        }

        if (not_found) {
            // the child is not installed: this is not an error
            c.status = CMS_ABSENT;
            cx.clear();
            continue;
        }

        // The child is present but could not be loaded.  The declaration is optional, so this does not fail
        // the parent: an extension that cannot be used must never make the module that declares it
        // unusable.  Record the child's own error for introspection, report it, and carry on with the next
        // declaration
        c.status = CMS_FAILED;
        QoreStringValueHelper err(cx.getExceptionErr());
        QoreStringValueHelper desc(cx.getExceptionDesc());
        c.err = err->empty() ? "LOAD-MODULE-ERROR" : err->c_str();
        c.desc = desc->c_str();
        cx.clear();

        qore_report_child_module_not_attached(xsink, wsink, warning_mask, mi.getName(), c);
    }

    if (all_done) {
        mi.children_done = true;
    }

    return 0;
}

int QoreModuleManager::queueChildModules(QoreAbstractModule& mi, ExceptionSink& xsink, ExceptionSink& wsink,
        int warning_mask) {
    if (mi.hasChildModules() && !mi.children_done) {
        bool found = false;
        for (const std::string& i : child_attach_queue) {
            if (i == mi.getName()) {
                found = true;
                break;
            }
        }
        if (!found) {
            child_attach_queue.push_back(mi.getName());
        }
    }

    // Child modules are attached at the outermost module load boundary in this thread: a child parsed while
    // an enclosing module load is still in progress cannot resolve that module's symbols, since the loader
    // returns silently for a feature already reserved by this thread.  See
    // design/qore-module-structure.md "Child Modules"
    if (module_load_depth > 0 || child_attach_queue.empty()) {
        return 0;
    }

    // if the enclosing load already failed, record child statuses without adding to the caller's sink; a
    // broken child never fails the parent in any case, so only an interrupted attach can be lost here, and
    // that is reported by the enclosing failure
    const bool use_scratch = (bool)xsink;
    ExceptionSink scratch;
    // ensure that the scratch sink is empty on every exit path; a discarded sink would otherwise report the
    // exception on the console when it is destroyed
    ON_BLOCK_EXIT_OBJ(scratch, &ExceptionSink::clear);
    ExceptionSink& sink = use_scratch ? scratch : xsink;

    int rc = 0;
    while (!child_attach_queue.empty()) {
        const std::string name = child_attach_queue.front();
        child_attach_queue.erase(child_attach_queue.begin());

        QoreAbstractModule* pmi = findModuleUnlocked(name.c_str());
        if (!pmi) {
            continue;
        }

        if (attachChildModules(*pmi, sink, wsink, warning_mask)) {
            // the attach was interrupted; leave the rest of the queue in place, it is drained when the next
            // module load completes in this thread
            rc = use_scratch ? 0 : -1;
            break;
        }
    }

    return rc;
}

int QoreModuleManager::importModuleNSUnlocked(const char* name, QoreProgram* pgm, ExceptionSink& xsink) {
    printd(5, "QoreModuleManager::importModuleNSUnlocked() name='%s' pgm=%p\n", name, pgm);

    // Find the module (assumes lock is already held)
    QoreAbstractModule* mi = findModuleUnlocked(name);
    if (!mi) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name),
            "cannot find module '%s' for namespace import", name);
        return -1;
    }

    // Import the module's namespace into the program
    // We do a direct namespace merge WITHOUT calling addToProgramImpl to avoid:
    // 1. Module dependency tracking (causes cleanup assertion failures)
    // 2. Reexport handling (can cause deadlock)
    // This is specifically for AOT module initialization where dependencies
    // are already handled by the module manager via qore_module_dependencies.

    // Add the feature to the program
    qore_program_private::get(*pgm)->addUserFeature(name);

    // Get namespace pointers
    RootQoreNamespace* target_root_ns = pgm->getRootNS();
    RootQoreNamespace* source_root_ns = mi->isUser()
        ? static_cast<QoreUserModule*>(mi)->getProgram()->getRootNS()
        : nullptr;

    if (source_root_ns) {
        // For user modules, merge the namespace from the module's program
        QoreModuleContext qmc(name, qore_root_ns_private::get(*target_root_ns), xsink);
        // exclude runtime readers of the target namespace and runtime writers of the source while
        // they are merged; parse ownership, held by the caller, excludes only other writers of the
        // target
        RuntimeNamespaceMergeLocker rnml(*target_root_ns, *source_root_ns);

        qore_root_ns_private::scanMergeCommittedNamespace(*target_root_ns, *source_root_ns, qmc);

        if (qmc.hasError()) {
            qmc.rollback();
            qore_program_private::get(*pgm)->removeUserFeature(name);
            return -1;
        }

        qore_root_ns_private::copyMergeCommittedNamespace(*target_root_ns, *source_root_ns);
    } else {
        // For builtin modules, use the ns_init function to import
        // This should not happen in typical AOT scenarios since qore_module_dependencies
        // only lists user modules, but handle it for completeness
        mi->addToProgramImpl(pgm, xsink);
        if (xsink) {
            return -1;
        }
    }

    printd(2, "QoreModuleManager::importModuleNSUnlocked() imported '%s' into pgm %p\n", name, pgm);
    return 0;
}

void QoreModuleManager::registerUserModuleFromSource(const char* name, const char* src, QoreProgram* pgm,
        ExceptionSink& xsink) {
    OptLocker ol(&mutex);
    loadModuleIntern(xsink, xsink, name, pgm, false, MOD_OP_NONE, 0, src);
}

// const char* path, const char* feature, ReferenceHolder<QoreProgram>& pgm

// the mask of the masked module load in progress in this thread, if any; see QoreModuleManager::runTimeLoadModuleMasked()
static thread_local const QoreModuleParseOptionMask* thread_load_po_mask = nullptr;

namespace {
//! Sets the parse option mask of the masked module load in progress in this thread; nested loads add to it
class QoreLoadParseOptionMaskHelper {
public:
    QoreLoadParseOptionMaskHelper(const QoreModuleParseOptionMask& mask) : saved(thread_load_po_mask) {
        if (saved) {
            combined = *saved;
        }
        combined |= mask;
        thread_load_po_mask = &combined;
    }

    ~QoreLoadParseOptionMaskHelper() {
        thread_load_po_mask = saved;
    }

    QoreLoadParseOptionMaskHelper(const QoreLoadParseOptionMaskHelper&) = delete;
    QoreLoadParseOptionMaskHelper& operator=(const QoreLoadParseOptionMaskHelper&) = delete;

private:
    const QoreModuleParseOptionMask* saved;
    QoreModuleParseOptionMask combined;
};
}

QoreParseOptions QoreModuleParseOptionMask::maskable() {
    // every user module has USER_MOD_PO; the style options are set by the module's own directives
    return ~(QoreParseOptions::POSITIVE_OPTIONS | QoreParseOptions::FREE_OPTIONS
        | QoreParseOptions(PO_STYLE_OPTIONS | USER_MOD_PO));
}

int QoreModuleParseOptionMask::check(const QoreParseOptions& strip, const QoreParseOptions& add,
        ExceptionSink* xsink) {
    const QoreParseOptions bad = (strip | add) & ~maskable();
    if (bad) {
        xsink->raiseException("MODULE-PARSE-OPTION-MASK-ERROR", "a module parse option mask may only strip or add "
            "restrictions; options " QLLX ":" QLLX " are explicitly-allowed, language, style or module options",
            bad.getHi(), bad.getLo());
        return -1;
    }
    const QoreParseOptions both = strip & add;
    if (both) {
        xsink->raiseException("MODULE-PARSE-OPTION-MASK-ERROR", "a module parse option mask cannot both strip and "
            "add options " QLLX ":" QLLX, both.getHi(), both.getLo());
        return -1;
    }
    return 0;
}

int QoreModuleManager::setParseOptionMask(const QoreModuleParseOptionMask& mask, ExceptionSink* xsink) {
    if (QoreModuleParseOptionMask::check(mask.strip, mask.add, xsink)) {
        return -1;
    }
    std::lock_guard<std::mutex> lck(po_mask_lock);
    po_mask = mask;
    return 0;
}

QoreModuleParseOptionMask QoreModuleManager::getParseOptionMask() const {
    std::lock_guard<std::mutex> lck(po_mask_lock);
    return po_mask;
}

QoreModuleParseOptionMask QoreModuleManager::getLoadParseOptionMask() const {
    QoreModuleParseOptionMask rv = getParseOptionMask();
    if (thread_load_po_mask) {
        rv |= *thread_load_po_mask;
    }
    return rv;
}

//! returns the feature name of a module given by name or by path
static std::string get_module_feature_name(const char* name) {
    std::string feature = q_basenameptr(name);
    for (const char* ext : {".qmod", ".qm"}) {
        size_t len = strlen(ext);
        if (feature.size() > len && !feature.compare(feature.size() - len, len, ext)) {
            feature.erase(feature.size() - len);
            break;
        }
    }
    return feature;
}

int QoreModuleManager::runTimeLoadModuleMasked(ExceptionSink& xsink, ExceptionSink& wsink, const char* name,
        QoreProgram* pgm, const QoreModuleParseOptionMask& mask, int warning_mask) {
    if (QoreModuleParseOptionMask::check(mask.strip, mask.add, &xsink)) {
        return -1;
    }
    {
        QoreLoadParseOptionMaskHelper lpomh(mask);
        if (runTimeLoadModule(xsink, wsink, name, pgm, nullptr, QMLO_NONE, warning_mask)) {
            return -1;
        }
    }
    // a module that was loaded before gets the mask as well
    return addModuleParseOptionMask(get_module_feature_name(name).c_str(), mask, &xsink);
}

int QoreModuleManager::addModuleParseOptionMask(const char* name, const QoreModuleParseOptionMask& mask,
        ExceptionSink* xsink) {
    if (QoreModuleParseOptionMask::check(mask.strip, mask.add, xsink)) {
        return -1;
    }
    QoreAbstractModule* mi = findModule(name);
    if (!mi) {
        xsink->raiseException("MODULE-PARSE-OPTION-MASK-ERROR", "module '%s' is not loaded", name);
        return -1;
    }
    mi->addParseOptionMask(mask);
    // the module's code runs with the parse options of its Program
    QoreProgram* mpgm = mi->getModuleProgram();
    if (mpgm && !mask.empty()) {
        qore_program_private::forceReplaceParseOptions(*mpgm, mask.apply(mpgm->getParseOptions()));
        qore_program_private::setAOTInheritedParseOptions(*mpgm,
            mask.apply(qore_program_private::getAOTInheritedParseOptions(*mpgm)));
    }
    return 0;
}

QoreAbstractModule* QoreModuleManager::setupUserModule(ExceptionSink& xsink, std::unique_ptr<QoreUserModule>& mi,
        QoreUserModuleDefContextHelper& qmd, unsigned load_opt, int warning_mask) {
    // see if a module with this name is already registered
    QoreAbstractModule* omi = findModuleUnlocked(mi->getName());
    if (omi) {
        qmd.setDuplicate();
    }

    printd(5, "QoreModuleManager::setupUserModule() '%s' omi: %p\n", mi->getName(), omi);

    if (xsink) {
        return nullptr;
    }

    const char* name = qmd.get("name");

    if (!name) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': no feature name "
            "present in module", mi->getFileName());
        return nullptr;
    }

    //printd(5, "QoreModuleManager::setupUserModule() path: %s name: %s feature: %s\n", mi->getFileName(), name,
    //  mi->getName());

    if (strcmp(mi->getName(), name)) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': provides feature "
            "'%s', expecting feature '%s', skipping, rename module to %s.qm to load", mi->getFileName(), name,
            mi->getName(), name);
        return nullptr;
    }

    // see if a module with this name is already registered
    if (omi) {
        if (!(load_opt & QMLO_REINJECT)) {
            // if the module is the same, then do not return an error unless trying to inject
            if (mi->equalTo(omi)) {
                if (load_opt & QMLO_INJECT) {
                    xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature "
                        "'%s' already loaded therefore cannot be used for injection unless the reinject flag is set",
                        mi->getFileName(), name);
                    return nullptr;
                }
                return omi;
            }
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s' "
                "already registered by '%s'", mi->getFileName(), name, omi->getFileName());
            return nullptr;
        }
    }

    // get qore module description
    const char* desc = qmd.get("desc");
    if (!desc) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': missing "
            "description", mi->getFileName(), name);
        return nullptr;
    }

    // get qore module version
    const char* version = qmd.get("version");
    if (!version) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': missing "
            "version", mi->getFileName(), name);
        return nullptr;
    }

    // get qore module author
    const char* author = qmd.get("author");
    if (!author) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': missing "
            "author", mi->getFileName(), name);
        return nullptr;
    }

    const char* url = qmd.get("url");

    const char* license = qmd.get("license");
    QoreString license_str(license ? license : "unknown");

    // record any child modules declared with %try-child-module; they are attached after the module has been
    // registered and published (see QoreModuleManager::attachChildModules())
    if (!qmd.child_vec.empty()) {
        mi->setChildModules(qmd.child_vec);
    }

    // issue #4254 do not run any initialization code while holding the global module lock
    if (qmd.hasInit()) {
        ModuleLoadMapHelper mlmh(name, xsink);

        // init & run module initialization code if any
        if (qmd.init(*mi->getProgram(), xsink)) {
            // The init closure may have registered data (factories, types) in foreign
            // modules (e.g., DataProvider) containing TAG_ENUM values that reference enum
            // declarations in this module's QoreProgram.  If we destroy the module now,
            // those enum declarations are freed and the TAG_ENUM values become dangling
            // pointers.  Keep the module alive (in the global list) so its QoreProgram
            // and enum declarations survive.  ImplicitModuleTransaction will clean up the
            // foreign registrations; the module's memory stays valid until process exit.
            mi->set(desc, version, author, url, license_str, qmd.takeDel());
            mi->setInitFailed();
            qore_program_private::get(*mi->getProgram())->addUserFeature(mi->getName());
            omi = mi.release();
            addModule(omi);
            trySetUserModule(name);
            return nullptr;
        }
        assert(!xsink);
        mi->setInitClosure(qmd.init_c.refSelf());
    }

    mi->set(desc, version, author, url, license_str, qmd.takeDel());

    // handle reinjections here
    if (omi) {
        assert(load_opt & QMLO_REINJECT);
        reinjectModule(omi);
        name = mi->getName();
        assert(umset.find(omi->getName()) == umset.end());
        mi->setLink(omi);
    } else if (mi->isPrivate()) {
        QoreString orig_name(mi->getName());
        // rename to unique name
        QoreString nname;
        getUniqueName(nname, mi->getName(), "private");
        mi->rename(nname);
        mi->setOrigName(orig_name.c_str());
        name = mi->getName();
    }

    //printd(5, "QoreModuleManager::setupUserModule() path: %s name: %s feature: %s injected: %d reinjected: %d "
    //  "orig: %s\n", mi->getFileName(), name, mi->getName(), mi->isInjected(), mi->isReInjected(),
    //  mi->getOrigName() ? mi->getOrigName() : "n/a");

    qore_program_private::get(*mi->getProgram())->addUserFeature(mi->getName());

    omi = mi.release();
    addModule(omi);
    trySetUserModule(name);

    return omi;
}

int QoreModuleManager::addModuleToBlacklist(const char* name, const char* msg) {
    AutoLocker al(mutex); // make sure checking and loading are atomic
    // see if a module with this name is already registered
    QoreAbstractModule* mi = findModuleUnlocked(name);
    if (mi) {
        return -1;
    }

    // check if it's been blacklisted
    auto i = mod_blacklist.lower_bound(name);
    if ((i != mod_blacklist.end()) && i->first == name) {
        return -2;
    }

    // store std::string copies — callers pass TempEncodingHelper::c_str() buffers
    // that are freed once the helper destructs
    mod_blacklist.emplace_hint(i, std::string(name), std::string(msg));
    return 0;
}

int QoreModuleManager::checkBlacklist(ExceptionSink& xsink, const char* name) {
    // check if it's been blacklisted
    auto i = mod_blacklist.find(name);
    if (i != mod_blacklist.end()) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s' was blacklisted %s",
            name, i->second.c_str());
        return -1;
    }
    return 0;
}

QoreAbstractModule* QoreModuleManager::loadUserModuleFromPath(ExceptionSink& xsink, ExceptionSink& wsink,
        const char* path, const char* feature, QoreProgram* tpgm, bool reexport, QoreProgram* mpgm,
        QoreProgram* path_pgm, unsigned load_opt, int warning_mask) {
    ReferenceHolder<QoreProgram> pholder(mpgm, &xsink);
    // a module Program that is not handed on is destroyed without the module manager mutex
    UnlockedModuleProgramRelease<ReferenceHolder<QoreProgram>> pholder_release(pholder);
    assert(feature);
    printd(5, "QoreModuleManager::loadUserModuleFromPath() path: '%s' feature: '%s' tpgm: %p ('%s') path_pgm: %p "
        "('%s')\n", path, feature, tpgm, tpgm && tpgm->parseGetScriptDir() ? tpgm->parseGetScriptDir() : "n/a",
        path_pgm, path_pgm && path_pgm->parseGetScriptDir() ? path_pgm->parseGetScriptDir() : "n/a");

    if (checkBlacklist(xsink, feature)) {
        return nullptr;
    }

    // a library module that code that is not trusted loads into a Program container of its own is parsed and
    // initialized with the container's code and parse defines: it is not library code
    const bool untrusted_container = mpgm && qore_caller_is_untrusted();
    // a module from the process-wide module path is library code: it is initialized with no sandbox provenance
    std::string trusted_path;
    const bool trusted = !untrusted_container && isTrustedModulePath(path, &trusted_path);
    if (trusted) {
        // the file that was found to be library code is the file that is loaded
        path = trusted_path.c_str();
    }
    // code that is not trusted cannot replace a library module with a module from elsewhere, nor with one in a
    // container of its own; a private module has a name of its own
    bool tenant_chosen;
    QoreSandboxManager* owner;
    qore_program_id_t owner_pgm_id;
    if (checkShadowingLoad(xsink, path, feature, trusted, tenant_chosen, owner, owner_pgm_id,
            untrusted_container ? ((load_opt & QMLO_PRIVATE) ? QSL_PRIVATE_CONTAINER : QSL_CONTAINER) : QSL_LOAD)) {
        return nullptr;
    }
    // recorded on the module when it is registered
    QoreModuleLoadFrameHelper load_frame(feature, trusted, tenant_chosen, owner, owner_pgm_id);
    // library code loaded by code that is not trusted does not take what that code chose (see below); checked before
    // the barrier is set
    const bool untrusted_loader = trusted && qore_caller_is_untrusted();
    QoreModuleInitSandboxBarrierHelper init_barrier(trusted);
    if (init_barrier.failed()) {
        xsink.outOfMemory();
        return nullptr;
    }

    QoreParseCountContextHelper pcch;

    // Find the best program to inherit options from
    QoreProgram* p = tpgm ? tpgm : path_pgm;
    if (!p) {
        p = mpgm;
        if (!p) {
            p = getProgram();
        }
    }

    // parse options for the module: those inherited from the current program, if any, with the module parse option
    // mask applied, disabling style and types options already set with USER_MOD_PO
    const QoreModuleParseOptionMask load_mask = getLoadParseOptionMask();
    QoreParseOptions po = USER_MOD_PO
        | get_masked_module_parse_options(p, PO_REQUIRE_TYPES | PO_NO_GLOBAL_VARS, load_mask);

    const char* td = p ? p->parseGetScriptDir() : nullptr;

    if (mpgm) {
        qore_program_private::forceReplaceParseOptions(*mpgm, po);
    } else {
        pholder = mpgm = new QoreProgram(po);
        mpgm->setScriptPath(path);
        // Inherit user-set parse defines from the parent so module-source
        // %ifdef/%ifndef directives evaluate against the caller's view (see
        // qore_program_private::inheritParseDefines for rationale).  Also
        // inherit reexport-marked symbol imports and the parent's
        // %prepend-module-path / %append-module-path lists (see
        // qore_program_private::inheritModulePathLists), so the module's own
        // nested %requires(reexport) dependencies resolve against the same
        // module search surface the importer established.
        if (trusted) {
            qore_program_private::get(*mpgm)->library_module = true;
        }
        if (p) {
            // library code loaded by code that is not trusted takes only the defines that that code's host set, and
            // does not resolve its own dependencies, optional dependencies and child modules through that code's
            // module paths; a library module's own Program has only library code's own
            const bool from_untrusted = untrusted_loader && !qore_program_private::get(*p)->library_module;
            qore_program_private::inheritParseDefines(*mpgm, *p, from_untrusted);
            // only trusted code can mark imports for re-export (see Program::importClass())
            qore_program_private::inheritParseImports(*mpgm, *p, &xsink);
            if (!from_untrusted) {
                qore_program_private::inheritModulePathLists(*mpgm, *p);
            }
        }
    }
    // See loadSeparatedModule(): source-module constant initialization must inherit the caller's sandbox.
    inherit_module_sandbox_manager(*mpgm, p, trusted);
    // inherit execution mode from parent program
    if (p) {
        // an explicitly requested mode applies to the module's Program as to a child Program (issue #5687)
        qore_program_private::get(*mpgm)->inheritExecMode(qore_program_private::get(*p));
    }
    // issue #3592: add feature to module container program immediately
    if (qore_program_private::get(*mpgm)->addUserFeature(feature)) {
        xsink.raiseException("LOAD-MODULE-ERROR", "cannot load user module '%s'; feature '%s' is already loaded in "
            "this Program container", path, feature);
        return nullptr;
    }

    //printd(5, "QoreModuleManager::loadUserModuleFromPath(path: '%s') cwd: '%s' tpgm: %p po: " QLLD
    //  " allow-injection: %s tpgm allow-injection: %s pgm allow-injection: %s\n", path, td ? td : "n/a", tpgm, po,
    //  po & PO_ALLOW_INJECTION ? "true" : "false",
    //  (tpgm ? tpgm->getParseOptions() & PO_ALLOW_INJECTION : 0) ? "true" : "false",
    //  mpgm->getParseOptions() & PO_ALLOW_INJECTION ? "true" : "false");

    // note: the module will contain a normalized path which will be used for parsing
    std::unique_ptr<QoreUserModule> mi(new QoreUserModule(pholder.release(), td, path, feature, load_opt,
        warning_mask));
    // a module that fails to load is destroyed without the module manager mutex
    UnlockedModuleProgramRelease<std::unique_ptr<QoreUserModule>> mi_release(mi);
    mi->addParseOptionMask(load_mask);

    td = mi->getFileName();
    //printd(5, "QoreModuleManager::loadUserModuleFromPath() normalized path: '%s'\n", td);

    if (!QoreDir::file_exists(td)) {
        xsink.raiseException("LOAD-MODULE-ERROR", "cannot load user module '%s'; the module doesn't exist", td);
        return nullptr;
    }

    if (module_load_check(td)) {
        xsink.raiseException("LOAD-MODULE-ERROR", "cannot load user module '%s'; recursive module dependency "
            "detected", td);
        return nullptr;
    }
    ON_BLOCK_EXIT(module_load_clear, td);

    ModuleReExportHelper mrh(mi.get(), reexport);
    QoreUserModuleDefContextHelper qmd(feature, path, mpgm, xsink);

    {
        ModuleLoadMapHelper mlmh(feature, xsink);

        // issue #3212: warning mask
        mi->getProgram()->parseFile(td, &xsink, &wsink, warning_mask);
    }

    if (xsink) {
        xsink.appendLastDescription(" (while loading user module \"%s\" from path \"%s\")", feature, path);
    }

    return setupUserModule(xsink, mi, qmd, load_opt, warning_mask);
}

QoreAbstractModule* QoreModuleManager::loadUserModuleFromSource(ExceptionSink& xsink, ExceptionSink& wsink,
        const char* path, const char* feature, QoreProgram* tpgm, const char* src, bool reexport, QoreProgram* mpgm,
        int warning_mask) {
    ReferenceHolder<QoreProgram> pholder(mpgm, &xsink);
    // a module Program that is not handed on is destroyed without the module manager mutex
    UnlockedModuleProgramRelease<ReferenceHolder<QoreProgram>> pholder_release(pholder);
    assert(feature);
    //printd(5, "QoreModuleManager::loadUserModuleFromSource() path: %s feature: %s tpgm: %p\n", path, feature, tpgm);

    if (checkBlacklist(xsink, feature)) {
        return nullptr;
    }

    // source text is never library code; loaded while library code is initialized, it is governed by the sandbox of
    // the code that loaded that, if it is not trusted
    QoreModuleInitSandboxBarrierHelper init_barrier(false);
    if (init_barrier.failed()) {
        xsink.outOfMemory();
        return nullptr;
    }
    // code that is not trusted cannot use source text to replace a library module
    bool tenant_chosen;
    QoreSandboxManager* owner;
    qore_program_id_t owner_pgm_id;
    if (checkShadowingLoad(xsink, path, feature, false, tenant_chosen, owner, owner_pgm_id)) {
        return nullptr;
    }
    // recorded on the module when it is registered
    QoreModuleLoadFrameHelper load_frame(feature, false, tenant_chosen, owner, owner_pgm_id);

    QoreParseCountContextHelper pcch;

    // Find the best program to inherit options from
    QoreProgram* p = tpgm;
    if (!p) {
        p = mpgm;
        if (!p) {
            p = getProgram();
        }
    }

    // parse options for the module: those inherited from the current program, if any, with the module parse option
    // mask applied, disabling style and types options already set with USER_MOD_PO
    const QoreModuleParseOptionMask load_mask = getLoadParseOptionMask();
    QoreParseOptions po = USER_MOD_PO | get_masked_module_parse_options(p, PO_REQUIRE_TYPES, load_mask);

    if (mpgm) {
        qore_program_private::forceReplaceParseOptions(*mpgm, po);
    } else {
        pholder = mpgm = new QoreProgram(po);
        mpgm->setScriptPath(path);
        // Inherit user-set parse defines from the parent so module-source
        // %ifdef/%ifndef directives evaluate against the caller's view (see
        // qore_program_private::inheritParseDefines for rationale).  Also
        // inherit reexport-marked symbol imports and the parent's
        // %prepend-module-path / %append-module-path lists (see
        // qore_program_private::inheritModulePathLists), so the module's own
        // nested %requires(reexport) dependencies resolve against the same
        // module search surface the importer established.
        if (p) {
            qore_program_private::inheritParseDefines(*mpgm, *p);
            qore_program_private::inheritParseImports(*mpgm, *p, &xsink);
            qore_program_private::inheritModulePathLists(*mpgm, *p);
        }
    }
    // inherit execution mode from parent program
    if (p) {
        // an explicitly requested mode applies to the module's Program as to a child Program (issue #5687)
        qore_program_private::get(*mpgm)->inheritExecMode(qore_program_private::get(*p));
    }
    // issue #3592: add feature to module container program immediately
    if (qore_program_private::get(*mpgm)->addUserFeature(feature)) {
        xsink.raiseException("LOAD-MODULE-ERROR", "cannot load user module '%s'; feature '%s' is already loaded in " \
            "this Program container", path, feature);
        return nullptr;
    }

    std::unique_ptr<QoreUserModule> mi(new QoreUserModule(pholder.release(), nullptr, path, feature, QMLO_NONE));
    // a module that fails to load is destroyed without the module manager mutex
    UnlockedModuleProgramRelease<std::unique_ptr<QoreUserModule>> mi_release(mi);
    mi->addParseOptionMask(load_mask);

    ModuleReExportHelper mrh(mi.get(), reexport);

    QoreUserModuleDefContextHelper qmd(feature, path, mpgm, xsink);

    {
        // run initialization unlocked
        ModuleLoadMapHelper mlmh(feature, xsink);

        mi->getProgram()->parse(src, path, &xsink, &wsink, warning_mask);
    }

    if (xsink) {
        xsink.appendLastDescription(" (while loading user module \"%s\" from source with given path \"%s\")", feature,
            path);
    }

    return setupUserModule(xsink, mi, qmd);
}

static qore_binary_module_desc_t get_binary_module_desc(void* ptr, const char* feature) {
    QoreStringMaker sym("%s_qore_module_desc", feature);
    sym.replaceAll("-", "_");
    return (qore_binary_module_desc_t)dlsym(ptr, sym.c_str());
}

int QoreModuleManager::loadAOTBinaryModuleDependencies(ExceptionSink& xsink,
        const std::vector<std::string>& dependencies, QoreProgram* path_pgm) {
    for (size_t i = 0; i < dependencies.size(); ++i) {
        if (i && !(i % 10) && qore_check_cancel(&xsink, "AOT binary module dependency loading")) {
            return -1;
        }
        // Resolve native symbols before dlopen without importing private
        // dependencies into the host. A source module imports these into its
        // own Program, not the caller; generated AOT init does the same.
        // Importing here can collide with unrelated host declarations even
        // when the dependency is absent from the module's public surface.
        // Preserve the caller's module search path and sandbox context.
        loadModuleIntern(xsink, xsink, dependencies[i].c_str(), nullptr, false,
            MOD_OP_NONE, nullptr, nullptr, nullptr, QMLO_NONE, QP_WARN_MODULES,
            nullptr, path_pgm);
        if (xsink) {
            return -1;
        }
    }
    return 0;
}

//! True when the caller asked for module files to be checked before they are mapped.
/** Off by default, and read once: the check costs a whole-file hash (measured at ~5 ms/MB
    on macOS), which a process loading a hundred modules would pay on every start.
*/
static bool qore_verify_module_signatures() {
    static constinit QoreLazyValue<bool> rv_setting([]() -> bool {
        QoreEnvironmentValue v_holder("QORE_VERIFY_MODULE_SIGNATURES");
        const char* v = v_holder.c_str();
        return v && *v && strcmp(v, "0");
    });
    const bool rv = rv_setting.get();
    return rv;
}

//! Checks that a binary module file is intact before dlopen() maps it.
/** macOS signs every dylib -- the linker attaches an ad-hoc signature -- and on Apple
    Silicon the kernel SIGKILLs a process that maps a page whose signature no longer
    validates.  A `.qmod` damaged on disk (a partial write, a bad copy, a build interrupted
    mid-write) therefore kills the loading process outright, with no diagnostic: the signal
    arrives inside dlopen(), before anything here can report which file was at fault, and
    SIGKILL cannot be caught.  Verifying first turns that into an ordinary
    LOAD-MODULE-ERROR naming the file.

    It is opt-in because the check hashes the whole file, so making it unconditional would
    add hundreds of milliseconds to the start of any process that loads many modules.  The
    cost is only worth paying when something is already wrong -- which is exactly when a
    user, faced with an unexplained "Killed: 9", turns it on.

    Nothing here is macOS-specific in principle; it is implemented only there because that
    is the platform whose kernel kills the process rather than letting dlopen() fail.

    @param path the module file about to be opened
    @param err receives a description when the file cannot be verified

    @return 0 when the file is intact or cannot be checked on this platform, -1 with @a err
    set when it is not
*/
static int qore_check_module_file_integrity(const char* path, QoreString& err) {
    if (!qore_verify_module_signatures()) {
        return 0;
    }
#ifdef __APPLE__
    CFStringRef cf_path = CFStringCreateWithCString(nullptr, path, kCFStringEncodingUTF8);
    if (!cf_path) {
        return 0;
    }
    ON_BLOCK_EXIT(CFRelease, cf_path);
    CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, cf_path, kCFURLPOSIXPathStyle, false);
    if (!url) {
        return 0;
    }
    ON_BLOCK_EXIT(CFRelease, url);

    SecStaticCodeRef code = nullptr;
    OSStatus rc = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &code);
    if (rc != noErr) {
        // an unsigned file is not evidence of damage: report only what we can stand behind
        return 0;
    }
    ON_BLOCK_EXIT(CFRelease, code);

    rc = SecStaticCodeCheckValidity(code, kSecCSDefaultFlags, nullptr);
    if (rc == noErr) {
        return 0;
    }
    err.sprintf("the file's code signature does not match its contents (OSStatus %d); the "
        "file is damaged -- reinstall the module.  Loading it would be fatal: macOS kills "
        "a process that maps a page failing signature validation, without a diagnostic",
        (int)rc);
    return -1;
#else
    (void)path;
    (void)err;
    return 0;
#endif
}

QoreAbstractModule* QoreModuleManager::loadBinaryModuleFromPath(ExceptionSink& xsink, const char* path,
        const char* feature, bool reexport, QoreProgram* mpgm, QoreProgram* path_pgm,
        unsigned load_opt, qore_binary_module_desc_t mod_desc, bool source_available, ExceptionSink* wsink,
        int warning_mask) {
    ReferenceHolder<QoreProgram> pholder(mpgm, &xsink);
    QoreModuleInfo mod_info;

    // set the module's name in thread-local data so that any namespaces created when the module is loaded can be
    // appropriately tagged
    QoreString feature_str;
    if (!feature) {
        feature_str = path;
        feature = get_feature_from_path(feature_str);
    }

    if (checkBlacklist(xsink, feature)) {
        return nullptr;
    }

    // a module from the process-wide module path is library code: it is initialized with no sandbox provenance
    std::string trusted_path;
    const bool trusted = isTrustedModulePath(path, &trusted_path);
    if (trusted) {
        // the file that was found to be library code is the file that is loaded
        path = trusted_path.c_str();
    }
    // code that is not trusted cannot replace a library module with a module from elsewhere
    bool tenant_chosen;
    QoreSandboxManager* owner;
    qore_program_id_t owner_pgm_id;
    if (checkShadowingLoad(xsink, path, feature, trusted, tenant_chosen, owner, owner_pgm_id)) {
        return nullptr;
    }
    // recorded on the module when it is registered
    QoreModuleLoadFrameHelper load_frame(feature, trusted, tenant_chosen, owner, owner_pgm_id);
    // library code loaded by code that is not trusted does not resolve its dependencies through the module paths of
    // that code; checked before the barrier is set
    if (trusted && path_pgm && !qore_program_private::get(*path_pgm)->library_module && qore_caller_is_untrusted()) {
        path_pgm = nullptr;
    }
    QoreModuleInitSandboxBarrierHelper init_barrier(trusted);
    if (init_barrier.failed()) {
        xsink.outOfMemory();
        return nullptr;
    }

    QoreModuleNameContextHelper mnch(feature);

#ifndef _Q_WINDOWS
    // a binary module is a regular file: reading or mapping a FIFO or a device with the name of one would block the
    // loader with the module manager's mutex held (a file that does not exist is left to dlopen() to report).  The
    // check is by path, as are the reads that follow (the metadata reader, which checks again through
    // qore_open_regular_file(), the integrity check and dlopen()): replacing the file between them takes write access
    // to the module's directory, which can plant code in the module path anyway, so it is no boundary to defend here
    {
        struct stat sb;
        if (!stat(path, &sb) && !S_ISREG(sb.st_mode)) {
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(path),
                "cannot load qore module '%s': it is not a regular file", path);
            return nullptr;
        }
    }
#endif

    // must happen before dlopen(): on macOS an invalid signature is fatal inside dlopen()
    {
        QoreString integrity_err;
        if (qore_check_module_file_integrity(path, integrity_err)) {
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(path),
                "cannot load qore module '%s': %s", path, integrity_err.c_str());
            return nullptr;
        }
    }

    // qcc stores the metadata of an AOT module -- its name, its dependencies, and the optional modules that were
    // unavailable when it was compiled -- in a section of its image, which is read before the module is mapped
    QoreAOTModuleFileInfo aot_info;
    std::string aot_info_error;
    QoreAOTModuleInfoStatus aot_info_status = qoreAOTReadModuleInfo(path, aot_info, aot_info_error);
    if (aot_info_status == QoreAOTModuleInfoStatus::INVALID) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(path),
            "cannot load binary module '%s': %s", path, aot_info_error.c_str());
        return nullptr;
    }
    bool have_aot_info = aot_info_status == QoreAOTModuleInfoStatus::FOUND;

    // until the module is mapped, its dependencies are found as its initialization finds them, and as a source
    // module finds them: with its own %prepend-module-path and %append-module-path directories (written by qcc in
    // its metadata) around the lists of the Program loading it; a dependency found without them is another module
    // than the one it was compiled against, which the check below reports as out of date, and the source module
    // is loaded instead
    std::unique_ptr<QoreModulePathScope> path_scope;
    if (have_aot_info && (!aot_info.prepended_module_paths.empty() || !aot_info.appended_module_paths.empty())) {
        path_scope = std::make_unique<QoreModulePathScope>(path_pgm, aot_info.prepended_module_paths,
            aot_info.appended_module_paths);
    }

    // an AOT module compiled when a module requested with %try-module was unavailable takes the branch for the
    // module being unavailable, so it is stale once the module can be loaded
    if (have_aot_info && !aot_info.unavailable_optional_modules.empty()) {
        std::string available;
        if (findAvailableOptionalModule(aot_info.unavailable_optional_modules, path_pgm, available)) {
            if (source_available) {
                xsink.raiseExceptionArg("AOT-MODULE-STALE", new QoreStringNode(path), "AOT module '%s' was "
                    "compiled when optional module '%s' was not available, but it is available now; rebuild the "
                    "binary module", path, available.c_str());
                return nullptr;
            }
            // without the source module, the binary module is loaded, but the missing functionality is always
            // reported, as for a broken %try-child-module
            SimpleRefHolder<QoreStringNode> desc(new QoreStringNodeMaker("AOT module '%s' was compiled when "
                "optional module '%s' was not available, but it is available now, and no source module was "
                "found; the binary module is loaded without the functionality that requires the optional module; "
                "rebuild the binary module", path, available.c_str()));
            qore_report_module_load_warning(xsink, wsink != &xsink ? wsink : nullptr, warning_mask, true,
                "BINARY-MODULE-STALE", feature, desc.release());
        }
    }

    std::unique_ptr<ModuleLoadMapHelper> preload_guard;
    if (have_aot_info) {
        if (aot_info.module_name != feature) {
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(path),
                "the AOT module metadata of module '%s' identifies feature '%s', expecting '%s'",
                path, aot_info.module_name.c_str(), feature);
            return nullptr;
        }
        // Reserve the parent before recursively loading its dependency graph,
        // matching loadBinaryModuleFromDesc()'s cycle and waiter semantics.
        preload_guard = std::make_unique<ModuleLoadMapHelper>(feature, xsink, false);
        if (loadAOTBinaryModuleDependencies(xsink, aot_info.dependencies, path_pgm)) {
            return nullptr;
        }
        // a module of a build tree is out of date when a module it was compiled with is loaded from another module
        // than the one it was compiled against (one that appeared in a directory searched earlier, for example): the
        // caller then loads its source module, as for any AOT module that cannot be loaded; without a source module
        // the AOT module is loaded, and the warning tells that it must be rebuilt, as for an optional module that is
        // available now
        std::string stale_desc;
        bool stale = checkAOTModuleDependencyFiles(xsink, path, feature, aot_info, path_pgm,
            stale_desc);
        if (xsink) {
            return nullptr;
        }
        if (stale) {
            // an error also when no source module can be loaded instead: AOT code compiled against another module
            // than the one loaded for it can misbehave (see design/aot-linked-module-sources.md); an identical copy of
            // the module compiled against is not another module, so it does not get here
            xsink.raiseExceptionArg("AOT-MODULE-STALE", new QoreStringNode(path), "AOT module '%s' for feature "
                "'%s' is out of date: %s; rebuild the AOT module", path, feature, stale_desc.c_str());
            return nullptr;
        }
    }
    // the module's initialization runs in a Program of its own, which has its directories
    path_scope.reset();

    // with its dependencies loaded, an AOT module is mapped once, resolving all of its symbols; a module without the
    // metadata is mapped lazily so that its descriptor can be read, and an AOT module without it is refused in
    // loadBinaryModuleFromDesc()
    int dlopen_flags = have_aot_info ? QORE_DLOPEN_NOW_FLAGS : QORE_DLOPEN_FLAGS;
    void* ptr = dlopen(path, dlopen_flags);
    if (!ptr) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(path),
            have_aot_info ? "error resolving symbols in qore AOT module '%s': %s"
                : "error loading qore module '%s': %s",
            path, dlerror());
        return nullptr;
    }

    DLHelper dlh(ptr);

    if (!mod_desc) {
        // check for new-style module declaration; convert hyphens to underscores for valid C identifier
        mod_desc = get_binary_module_desc(ptr, feature);
    }

    if (mod_desc) {
        mod_desc(mod_info);
        if (have_aot_info && (!mod_info.is_aot
                || mod_info.name != aot_info.module_name
                || mod_info.dependencies != aot_info.dependencies)) {
            ReferenceHolder<QoreHashNode> info(mod_info.info, &xsink);
            mod_info.info = nullptr;
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(path),
                "the AOT module metadata of module '%s' does not match its module descriptor", path);
            return nullptr;
        }
        return loadBinaryModuleFromDesc(xsink, &dlh, mod_info, path, feature, reexport, pholder.release(),
            path_pgm, load_opt, preload_guard.get());
    }

    // construct a valid C identifier for the error message suggestion
    QoreStringMaker suggested_sym("%s_qore_module_desc", feature);
    suggested_sym.replaceAll("-", "_");
    xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(path), "module '%s': modules must implement "
        "the API 2.0 module description function '%s'", path, suggested_sym.c_str());
    return nullptr;
}

//! The section of a module image that holds its AOT module metadata on this platform, for diagnostics
#ifdef __APPLE__
#define QORE_AOT_MODULE_INFO_CONTAINER "the Mach-O section " QORE_AOT_MODULE_INFO_MACHO_SEGMENT "," \
    QORE_AOT_MODULE_INFO_MACHO_SECTION
#else
#define QORE_AOT_MODULE_INFO_CONTAINER "the ELF note section " QORE_AOT_MODULE_INFO_ELF_SECTION
#endif

QoreAbstractModule* QoreModuleManager::loadBinaryModuleFromDesc(ExceptionSink& xsink, DLHelper* dlh,
        QoreModuleInfo& mod_info, const char* path, const char* feature, bool reexport,
        QoreProgram* mpgm, QoreProgram* path_pgm, unsigned load_opt, ModuleLoadMapHelper* load_guard) {
    ReferenceHolder<QoreProgram> pholder(mpgm, &xsink);
    // take info hash immediately, if any
    ReferenceHolder<QoreHashNode> info(mod_info.info, &xsink);
    mod_info.info = nullptr;

    // Save and restore the try-module count so that any parsing triggered by the
    // binary module's init function (e.g., AOT modules re-parsing embedded source)
    // does not corrupt the caller's scanner state.  This matches the pattern used
    // by loadUserModuleFromPath() and loadSeparatedModule().
    QoreParseCountContextHelper pcch;

    // get module name
    if (mod_info.name.empty()) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(path), "module '%s': no feature name "
            "present in module", path);
        return nullptr;
    }

    const char* name = mod_info.name.c_str();

    if (mod_info.is_aot) {
        if (mod_info.aot_abi_version != QORE_AOT_MODULE_ABI_VERSION) {
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name),
                "AOT module '%s': feature '%s': native ABI version %u is incompatible with runtime version %u; "
                "rebuild the module with the current qcc",
                path, name, mod_info.aot_abi_version,
                static_cast<unsigned>(QORE_AOT_MODULE_ABI_VERSION));
            return nullptr;
        }
        // the generated code of an AOT module can only run against a libqore with the generated-code contract it was
        // compiled for; see design/aot-object-files-and-module-artifacts.md "AOT Module Runtime Identity"
        if (mod_info.aot_runtime_identity != qore_aot_runtime_identity) {
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name),
                "AOT module '%s': feature '%s': the module was compiled for AOT runtime identity '%s', but this Qore "
                "runtime (libqore %s) has AOT runtime identity '%s'; the module's native code was generated for a "
                "different Qore library and cannot be run with this one; rebuild the module with the qcc of this "
                "Qore installation",
                path, name,
                mod_info.aot_runtime_identity.empty() ? "<none>" : mod_info.aot_runtime_identity.c_str(),
                qore_version_string, qore_aot_runtime_identity);
            return nullptr;
        }
        // qcc stores the metadata of every AOT module it links in a section of the image, and the loader read it
        // before mapping the module (load_guard is set then); an AOT module loaded from a file without it was changed
        // after it was built, and loading it would skip the check for optional modules that have become available
        if (dlh && !load_guard) {
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name),
                "AOT module '%s': feature '%s': the module has no AOT module metadata (" QORE_AOT_MODULE_INFO_CONTAINER
                "); the file was changed after it was built, for example by a tool that removed the section: "
                "reinstall or rebuild the module", path, name);
            return nullptr;
        }
    }

    // ensure provided feature matches with expected feature
    if (feature && mod_info.name != feature) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': provides feature "
            "'%s', expecting feature '%s', skipping, rename module to %s.qmod to load", path, name,
            feature, name);
        return nullptr;
    }

    // get qore module API major number
    if (mod_info.api_major < 0) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': no qore "
            "module API major number", path, name);
        return nullptr;
    }

    // get qore module API minor number
    if (mod_info.api_minor < 0) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': no qore "
            "module API minor number", path, name);
        return nullptr;
    }

    // get initialization function
    if (!mod_info.init) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': missing "
            "module init method", path, name);
        return nullptr;
    }

    // get namespace initialization function
    if (!mod_info.ns_init) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': missing "
            "namespace init method", path, name);
        return nullptr;
    }

    // get deletion function
    if (!mod_info.del) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': missing "
            "delete method", path, name);
        return nullptr;
    }

    // get qore module description
    if (mod_info.desc.empty()) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': missing "
            "description", path, name);
        return nullptr;
    }

    // get qore module version
    if (mod_info.version.empty()) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': missing "
            "version", path, name);
        return nullptr;
    }

    // get qore module author
    if (mod_info.author.empty()) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': missing "
            "author", path, name);
        return nullptr;
    }

    // NOTE: no module program is possible to support injections in builtin modules for the moment, since we use the
    // module's "module_ns_init()" function to apply the module's namespace changes to each requesting QoreProgram
    // container. To support injections in builtin modules, we would need to embed a QoreProgram container in the
    // QoreBuiltinModule object, and import the module's changes into that program while supporting injections like
    // we do for user modules
    assert(!mpgm);

    // Reserve this module in the load map before loading dependencies.  A
    // dependency's own ModuleLoadMapHelper unlocks the module-manager mutex
    // while its init code runs; without this reservation, another thread can
    // start loading the same parent module before this thread reaches the
    // parent's own initialization.
    bool dependencies_preloaded = load_guard;
    std::unique_ptr<ModuleLoadMapHelper> owned_load_guard;
    if (!load_guard) {
        owned_load_guard = std::make_unique<ModuleLoadMapHelper>(name, xsink, false);
        load_guard = owned_load_guard.get();
    }

    // Use path_pgm for search-path layering: AOT qmods expose source %requires
    // as binary-module dependencies, and those must honor the importing
    // Program's %prepend-module-path / %append-module-path lists.
    if (!mod_info.dependencies.empty() && !dependencies_preloaded) {
        if (mod_info.is_aot) {
            if (loadAOTBinaryModuleDependencies(xsink, mod_info.dependencies, path_pgm)) {
                return nullptr;
            }
        } else {
            for (size_t i = 0; i < mod_info.dependencies.size(); ++i) {
                if (i && !(i % 10) && qore_check_cancel(&xsink, "binary module dependency loading")) {
                    return nullptr;
                }
                loadModuleIntern(xsink, xsink, mod_info.dependencies[i].c_str(), path_pgm);
                if (xsink) {
                    return nullptr;
                }
            }
        }
    }

    // see if a module with this name is already registered
    QoreAbstractModule* mi = findModuleUnlocked(name);
    if (mi) {
        // if the module is the same, then do not return an error
        if (mi->isPath(path)) {
            return mi;
        }
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s' already "
            "registered by '%s'", path, name, mi->getFileName());
        return nullptr;
    }

    if (checkBlacklist(xsink, name)) {
        return nullptr;
    }

    if (!is_module_api_supported(mod_info.api_major, mod_info.api_minor)) {
        QoreStringNode* str = new QoreStringNodeMaker("module '%s': feature '%s': API mismatch, module supports "
            "API %d.%d, however only version", path, name, mod_info.api_major, mod_info.api_minor);

        if (qore_mod_api_list_len > 1)
            str->concat('s');
        // add all supported api pairs to the string
        for (unsigned j = 0; j < qore_mod_api_list_len; ++j) {
            str->sprintf(" %d.%d", qore_mod_api_list[j].major, qore_mod_api_list[j].minor);
            if (j != qore_mod_api_list_len - 1) {
                if (qore_mod_api_list_len > 2) {
                    if (j != qore_mod_api_list_len - 2) {
                        str->concat(",");
                    } else {
                        str->concat(", and");
                    }
                } else {
                    str->concat(" and");
                }
            }
            if (j == qore_mod_api_list_len - 1) {
                str->concat(' ');
                if (qore_mod_api_list_len > 1) {
                    str->concat("are");
                } else {
                    str->concat("is");
                }
                str->concat(" supported");
            }
        }

        printd(5, "QoreModuleManager::loadBinaryModuleFromDesc() error: %s\n", str->c_str());
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), str);
        return nullptr;
    }

    //printd(5, "module_license_str: '%s' license_str: '%s'\n", module_license_str ? module_license_str : "n/a",
    //    license_str.c_str());

    switch (mod_info.license) {
        case QL_GPL: if (mod_info.license_str.empty()) mod_info.license_str = "GPL"; break;
        case QL_LGPL: if (mod_info.license_str.empty()) mod_info.license_str = "LGPL"; break;
        case QL_MIT: if (mod_info.license_str.empty()) mod_info.license_str = "MIT"; break;
        default:
            xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': "
                "invalid qore_module_license symbol (%d)", path, name, mod_info.license);
            return nullptr;
    }

    if (qore_license != QL_GPL && mod_info.license == QL_GPL) {
        xsink.raiseExceptionArg("LOAD-MODULE-ERROR", new QoreStringNode(name), "module '%s': feature '%s': qore "
            "library initialized with non-GPL license, but module requires GPL", path, name);
        return nullptr;
    }

    try {
        assert(q_gettid());
        // Run only module init code without the module-manager mutex; the in-progress
        // reservation remains active so concurrent loads of this module still wait.
        load_guard->unlock();
        assert(mod_info.init);
        printd(5, "QoreModuleManager::loadBinaryModuleFromDesc(%s) %s: calling module_init@%p\n", path,
            name, mod_info.init);
        QoreModuleInitContext ctx;
        ctx.path = path;
        QorePluginModuleHandle plugin_handle(name, path, dlh ? dlh->ptr : nullptr);
        QorePluginModuleInitScope plugin_scope(plugin_handle);
        ctx.plugin_module_handle = plugin_scope.getHandle();
        // an AOT module inherits the restrictions of the Program loading it, as a source module does (#5556)
        QoreAOTModuleInitLoaderHelper aot_loader(path_pgm);
        mod_info.init(ctx, xsink);
        if (xsink) {
            return nullptr;
        }
        plugin_scope.commit(&xsink);
        if (xsink) {
            return nullptr;
        }
    } catch (AbstractException& e) {
        e.convert(&xsink);
        return nullptr;
    }
    load_guard->lock();

    std::unique_ptr<QoreBuiltinModule> bmi(new QoreBuiltinModule(nullptr, path, mod_info,
        dlh ? dlh->release() : nullptr, info, load_opt));
    // an AOT user module has the mask its Program was created with (see qore_aot_module_init_impl())
    if (qore_is_aot_user_module(name)) {
        bmi->addParseOptionMask(getLoadParseOptionMask());
    }
    // record any child modules declared with %try-child-module; for AOT-compiled modules these are
    // delivered by the module description function, since the directive cannot be processed again when the
    // embedded source is parsed (see design/qore-module-structure.md "Child Modules")
    if (!mod_info.child_modules.empty()) {
        bmi->setChildModules(mod_info.child_modules);
    }
    mi = bmi.get();
    QMM.addModule(bmi.release());

    ModuleReExportHelper mrh(mi, reexport);

    printd(5, "QoreModuleManager::loadBinaryModuleFromPath(%s) registered '%s'\n", path, name);
    return mi;
}

void QoreModuleManager::delOrig(QoreAbstractModule* mi) {
    while (mi) {
        const char *n = mi->getName();
        //printd(5, "QoreModuleManager::delOrig() mi: %p '%s'\n", mi, mi->getName());

        module_map_t::iterator i = map.find(n);
        assert(i != map.end());
        assert(i->second == mi);

        QoreAbstractModule* next = mi->getNext();
        map.erase(i);
        delete mi;
        mi = next;
    }
}

// deletes only user modules
void QoreModuleManager::delUser() {
    //md_map.show("md_map");
    //rmd_map.show("rmd_map");

    // every user module, including the originals of reinjected modules; umset holds only the modules that no other
    // user module depends on, which are the first to be deleted in Phase 2
    std::vector<QoreUserModule*> user_modules;
    for (auto& i : map) {
        if (i.second->isUser()) {
            user_modules.push_back(static_cast<QoreUserModule*>(i.second));
        }
    }

    // issue #5164: Phase 0 - run all del callbacks before clearing any module data
    // This ensures del callbacks execute while all module programs (and their TypeInfos) are alive,
    // preventing use-after-free when a del callback accesses a static var whose TypeInfo belongs to
    // a class from a module that would otherwise have been deleted in Phase 2 already
    {
        ExceptionSink xsink;
        for (QoreUserModule* um : user_modules) {
            um->runDelCallback(xsink);
        }
    }

    // calls into module code take the thread count of the module's Program again from now on, and the threads in
    // module code entered without it leave it first, so that the teardown below waits for every thread in it (see
    // qore_stop_module_uncounted_entries())
    qore_stop_module_uncounted_entries();
    // and no other thread can enter module code from now on; those that are in it are waited for below
    qore_close_module_programs();

#ifdef HAVE_SIGNAL_HANDLING
    // the signal handlers that module code set belong to the module's Program; the signal thread runs until the end of
    // the cleanup, so they are removed now: a handler entering a module's Program between the wait for its threads
    // and the clearing of its data below would run module code on data being destroyed.  A handler that is running
    // ends first, with the release of the signal thread's local data after it
    {
        std::vector<QoreProgram*> mpgms;
        qore_get_module_lifetime_programs(mpgms);
        ExceptionSink xsink;
        QSM.removeProgramHandlers(mpgms, &xsink);
    }
#endif

    // issue #5164: Phase 1 - clear namespace data on all user module programs before destroying any modules
    // This ensures static variable destructors run while all module programs are still alive, preventing
    // use-after-free when a shared class's static vars are destroyed by the last module to clear them
    // (which may not be the module that owns the class program)
    {
        ExceptionSink xsink;
        for (QoreUserModule* um : user_modules) {
            QoreProgram* pgm = um->getProgram();
            pgm->waitForTermination();
            qore_program_private::get(*pgm)->clearNamespaceData(&xsink);
        }
        qore_aot_clear_all_module_namespace_data(xsink);

        // break the cycles held up by opaque references ('@=') that user module programs own while all module
        // programs are still alive: such a cycle survives the namespace data cleared above, and an object that a
        // module owns can hold objects of the classes of the modules that depend on it, which are deleted before
        // it in Phase 2 (a type that DataProvider creates and a RestSchemaValidator schema object caches through an
        // opaque reference refers to schema objects, for example); breaking the cycle only when the owning module
        // is deleted would run the destructors of those objects after the programs of their classes are gone
        for (QoreUserModule* um : user_modules) {
            qore_program_private::get(*um->getProgram())->clearOpaqueTargets(&xsink);
        }
        // AOT user modules are binary modules, deleted after all user modules in cleanup()
        qore_aot_clear_all_module_opaque_targets(xsink);
    }

    // Phase 2 - delete the user modules and the native binary modules in dependency order
    // - a native binary module (a binary module that is not an AOT module) is deleted first, as soon as no module left
    //   requires it: it can own Programs that it deletes only when it is deleted (the jni module's global context
    //   Program), whose data is not cleared above and can hold objects of the classes of any module imported into
    //   them; deleting it here releases them while the Programs of their classes exist, and while the services that
    //   destructors use (the asynchronous I/O controller and thread pools, stopped after delUser()) still run
    // - a user module is deleted after the user modules requiring it, and not while it is imported into a Program that
    //   still holds data (one that a binary module owns), which can hold objects of its classes
    // - a user module that an AOT user module requires, directly or through other user modules, is left to cleanup(),
    //   as AOT user modules are binary modules deleted there, which can hold objects of its classes; so are the user
    //   modules imported into Programs that still hold data once no other module can be deleted here, and the native
    //   binary modules that AOT modules require
    strset_t left;
    while (true) {
        if (deleteUnrequiredNativeModule()) {
            continue;
        }
        // the user modules imported into Programs that still hold data
        strset_t held;
        qore_program_private::getUserFeaturesOfProgramsWithData(held);
        left = getUserModulesLeftForCleanup(held);
        // a user module that no other user module requires
        strset_t::iterator ui = umset.begin();
        while (ui != umset.end() && left.find(*ui) != left.end()) {
            ++ui;
        }
        if (ui != umset.end()) {
            module_map_t::iterator i = map.find((*ui).c_str());
            assert(i != map.end());
            assert(i->second->isUser());
            deleteUserModule(i);
            continue;
        }
        // the user modules left are left to cleanup(), or they are in a dependency cycle or required by one; a cycle
        // is broken by deleting one of its modules
        module_map_t::iterator i = map.end();
        for (module_map_t::iterator j = map.begin(), e = map.end(); j != e; ++j) {
            if (!j->second->isUser() || left.find(j->first) != left.end()) {
                continue;
            }
            if (i == map.end()) {
                i = j;
            }
            if (isInUserModuleDependencyCycle(j->first)) {
                i = j;
                break;
            }
        }
        if (i == map.end()) {
            break;
        }
        printd(5, "QoreModuleManager::delUser() breaking a dependency cycle at '%s'\n", i->first);
        deleteUserModule(i);
    }

#ifdef DEBUG
    for (module_map_t::iterator i = map.begin(), e = map.end(); i != e; ++i) {
        if (i->second->isUser() && left.find(i->first) == left.end()) {
            printd(0, "QoreModuleManager::delUser() '%s' %p not yet removed\n", i->second->getName(), i->second);

            for (md_map_t::iterator i = md_map.begin(), e = md_map.end(); i != e; ++i) {
                QoreString str("[");
                for (strset_t::iterator si = i->second.begin(), se = i->second.end(); si != se; ++si) {
                    str.sprintf("'%s',", (*si).c_str());
                }
                str.concat("]");

                printd(0, " + md_map '%s' -> %s\n", i->first.c_str(), str.c_str());
            }

            rmd_map.show("rmd_map");

            assert(false);
        }
    }
#endif

    // the dependencies are all released unless user modules are left for cleanup()
    assert(!left.empty() || (md_map.empty() && rmd_map.empty()));
}

strset_t QoreModuleManager::getUserModulesLeftForCleanup(const strset_t& held) {
    strset_t rv;
    std::vector<std::string> pending;
    // the user modules that AOT user modules require
    for (module_map_t::iterator i = map.begin(), e = map.end(); i != e; ++i) {
        if (i->second->isUser() && qore_aot_module_has_dependent(i->first)) {
            pending.push_back(i->first);
        }
    }
    // the user modules imported into Programs that still hold data
    for (const std::string& name : held) {
        module_map_t::iterator i = map.find(name.c_str());
        if (i != map.end() && i->second->isUser()) {
            pending.push_back(name);
        }
    }
    // add the user modules that they require
    while (!pending.empty()) {
        std::string name = std::move(pending.back());
        pending.pop_back();
        if (!rv.insert(name).second) {
            continue;
        }
        const strset_t* required = rmd_map.getDeps(name);
        if (required) {
            for (const std::string& dep : *required) {
                pending.push_back(dep);
            }
        }
    }
    return rv;
}

bool QoreModuleManager::getBinaryModuleDependencies(module_map_t::const_iterator i, std::vector<std::string>& deps) {
    assert(!i->second->isUser());
    assert(i->second->isBuiltin());
    if (qore_aot_get_module_dependencies(i->first, deps)) {
        return false;
    }
    const QoreBuiltinModule* bm = static_cast<const QoreBuiltinModule*>(i->second);
    if (bm->isAot()) {
        return false;
    }
    deps = bm->getDependencies();
    return true;
}

void QoreModuleManager::getModuleDependencies(const char* name, strset_t& deps) {
    AutoLocker al(mutex);
    module_map_t::const_iterator i = map.find(name);
    if (i == map.end()) {
        return;
    }
    if (i->second->isUser()) {
        // the user modules it requires
        if (const strset_t* user_deps = rmd_map.getDeps(name)) {
            deps.insert(user_deps->begin(), user_deps->end());
        }
        // the binary modules it requires
        for (md_map_t::const_iterator bi = binary_user_dep_map.begin(), e = binary_user_dep_map.end(); bi != e; ++bi) {
            if (bi->second.find(name) != bi->second.end()) {
                deps.insert(bi->first);
            }
        }
        return;
    }
    std::vector<std::string> binary_deps;
    getBinaryModuleDependencies(i, binary_deps);
    deps.insert(binary_deps.begin(), binary_deps.end());
}

bool QoreModuleManager::deleteUnrequiredNativeModule() {
    // the modules that the binary modules left and the user modules left require
    strset_t required;
    // the native binary modules left
    std::vector<module_map_t::iterator> native_mods;
    for (module_map_t::iterator i = map.begin(), e = map.end(); i != e; ++i) {
        if (i->second->isUser()) {
            continue;
        }
        std::vector<std::string> deps;
        if (getBinaryModuleDependencies(i, deps)) {
            native_mods.push_back(i);
        }
        required.insert(deps.begin(), deps.end());
    }
    for (md_map_t::iterator i = binary_user_dep_map.begin(), e = binary_user_dep_map.end(); i != e; ++i) {
        for (const std::string& user : i->second) {
            if (map.find(user.c_str()) != map.end()) {
                required.insert(i->first);
                break;
            }
        }
    }
    for (module_map_t::iterator i : native_mods) {
        if (required.find(i->first) == required.end()) {
            printd(5, "QoreModuleManager::deleteUnrequiredNativeModule() deleting '%s'\n", i->first);
            QoreAbstractModule* m = i->second;
            map.erase(i);
            delete m;
            return true;
        }
    }
    return false;
}

bool QoreModuleManager::isInUserModuleDependencyCycle(const std::string& name) {
    // follows the modules requiring the module until it is found again
    strset_t seen;
    std::vector<std::string> pending;
    const strset_t* dependents = md_map.getDeps(name);
    if (dependents) {
        pending.assign(dependents->begin(), dependents->end());
    }
    while (!pending.empty()) {
        std::string dep = std::move(pending.back());
        pending.pop_back();
        if (dep == name) {
            return true;
        }
        if (!seen.insert(dep).second) {
            continue;
        }
        dependents = md_map.getDeps(dep);
        if (dependents) {
            pending.insert(pending.end(), dependents->begin(), dependents->end());
        }
    }
    return false;
}

void QoreModuleManager::deleteUserModule(module_map_t::iterator i) {
    QoreAbstractModule* m = i->second;
    assert(m->isUser());
    delOrig(m->getNext());
    //printd(5, "QoreModuleManager::deleteUserModule() deleting '%s' %p\n", i->first, m);
    umset.erase(m->getName());
    // a module that a reload replaced is registered under a private name, and the dependencies it shares with the
    // module that replaced it are registered under its original name, which that module now owns: they are removed
    // when that module is deleted
    const char* orig_name = m->getOrigName();
    if (orig_name && map.find(orig_name) != map.end()) {
        orig_name = nullptr;
    }
    removeUserModuleDependency(m->getName(), orig_name);
    map.erase(i);
    delete m;
}

//! The deletion rank of a module that cleanup() can delete: the module with the lowest rank is deleted first
enum class CleanupRank {
    //! a native binary module: it can own Programs holding objects of the classes of any module loaded
    NATIVE = 0,
    //! a user module that a native binary module left needs deleted first: it requires one, maybe indirectly
    NATIVE_DEPENDENT_USER = 1,
    //! an AOT module that a native binary module left needs deleted first
    NATIVE_DEPENDENT_AOT = 2,
    //! another user module
    USER = 3,
    //! another AOT module
    AOT = 4,
};

void QoreModuleManager::cleanup() {
    QORE_TRACE("ModuleManager::cleanup()");

    {
        ExceptionSink xsink;
        qore_aot_clear_all_module_namespace_data(xsink);
        qore_aot_clear_all_module_opaque_targets(xsink);
    }

    // The modules left are deleted so that the classes of a module outlive every object of them:
    // - a module is deleted only after the modules that require it, which can hold objects of its classes (in the
    //   constants of their Programs, which are released only when the module is deleted): AOT user modules, native
    //   binary modules, and the user modules left by delUser() (see getUserModulesLeftForCleanup())
    // - of the modules that can be deleted, a native binary module (a binary module that is not an AOT module) is
    //   deleted first: it can own Programs that it deletes only when it is deleted (the jni module's global context
    //   Program, for example), which can hold objects of the classes of any module imported into them, user and AOT
    //   modules that no recorded dependency links to the native module; then the modules that a native binary module
    //   left requires (directly or indirectly), as they must be deleted before it; then the other modules
    // - user modules are deleted before AOT user modules of the same rank, and modules of the same rank in the order
    //   of their names
    // The data of the user and AOT module Programs has been cleared in delUser() and above, while all module Programs
    // existed, and the native binary modules that no module required have been deleted in delUser(); a native binary
    // module left here is required by an AOT module or by a user module left.  A dependency cycle is broken at the
    // module of the lowest rank.
    // module -> the modules left that it requires
    std::map<std::string, std::vector<std::string>> required_mods;
    // the native binary modules left
    strset_t native_mods;
    for (module_map_t::iterator i = map.begin(), e = map.end(); i != e; ++i) {
        std::vector<std::string>& req = required_mods[i->first];
        if (i->second->isUser()) {
            const strset_t* deps = rmd_map.getDeps(i->first);
            if (deps) {
                req.assign(deps->begin(), deps->end());
            }
            continue;
        }
        if (getBinaryModuleDependencies(i, req)) {
            native_mods.insert(i->first);
        }
    }
    // a user module requiring a binary module
    for (md_map_t::iterator i = binary_user_dep_map.begin(), e = binary_user_dep_map.end(); i != e; ++i) {
        for (const std::string& user : i->second) {
            auto ri = required_mods.find(user);
            if (ri != required_mods.end()) {
                ri->second.push_back(i->first);
            }
        }
    }
    // module -> the number of modules left that require it
    std::map<std::string, int> dependents;
    for (auto& i : required_mods) {
        dependents.emplace(i.first, 0);
    }
    for (auto& i : required_mods) {
        // a module listed twice is counted once
        strset_t seen;
        for (const std::string& dep : i.second) {
            auto di = dependents.find(dep);
            if (di != dependents.end() && dep != i.first && seen.insert(dep).second) {
                ++di->second;
            }
        }
        i.second.assign(seen.begin(), seen.end());
    }
    while (!map.empty()) {
        // the native binary modules left and the modules that require one of them, directly or indirectly
        strset_t native_reach = native_mods;
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& i : required_mods) {
                if (native_reach.find(i.first) != native_reach.end()) {
                    continue;
                }
                for (const std::string& dep : i.second) {
                    if (native_reach.find(dep) != native_reach.end()) {
                        native_reach.insert(i.first);
                        changed = true;
                        break;
                    }
                }
            }
        }
        auto get_rank = [&](module_map_t::iterator j) -> CleanupRank {
            if (native_mods.find(j->first) != native_mods.end()) {
                return CleanupRank::NATIVE;
            }
            bool user = j->second->isUser();
            if (native_reach.find(j->first) != native_reach.end()) {
                return user ? CleanupRank::NATIVE_DEPENDENT_USER : CleanupRank::NATIVE_DEPENDENT_AOT;
            }
            return user ? CleanupRank::USER : CleanupRank::AOT;
        };
        // the module of the lowest rank that no module left requires; if every module left is required by another
        // one, the modules left are in a cycle, which is broken at the module of the lowest rank
        module_map_t::iterator i = map.end();
        CleanupRank rank = CleanupRank::AOT;
        for (int pass = 0; pass < 2 && i == map.end(); ++pass) {
            for (module_map_t::iterator j = map.begin(), e = map.end(); j != e; ++j) {
                if (!pass) {
                    auto di = dependents.find(j->first);
                    if (di != dependents.end() && di->second) {
                        continue;
                    }
                }
                CleanupRank r = get_rank(j);
                if (i == map.end() || r < rank) {
                    i = j;
                    rank = r;
                }
            }
        }
        assert(i != map.end());
        printd(5, "QoreModuleManager::cleanup() deleting '%s' (rank %d)\n", i->first, static_cast<int>(rank));
        native_mods.erase(i->first);
        if (i->second->isUser()) {
            deleteUserModule(i);
        } else {
            QoreAbstractModule* m = i->second;
            map.erase(i);
            delete m;
        }
        // the modules that the modules deleted (with the originals of a reinjected user module) required have one
        // dependent less
        for (auto ri = required_mods.begin(); ri != required_mods.end();) {
            if (map.find(ri->first.c_str()) != map.end()) {
                ++ri;
                continue;
            }
            for (const std::string& dep : ri->second) {
                auto di = dependents.find(dep);
                if (di != dependents.end()) {
                    --di->second;
                }
            }
            ri = required_mods.erase(ri);
        }
    }

    assert(modset.empty());
}

void QoreModuleManager::issueParseCmd(const QoreProgramLocation* loc, const char* mname, const QoreString& cmd) {
    ExceptionSink xsink;

    QoreProgram* pgm = getProgram();

    // issue #4254: must run module commands unlocked
    QoreAbstractModule* mi;
    {
        OptLocker ol(&mutex);
        mi = loadModuleIntern(xsink, xsink, mname, pgm);

        if (xsink) {
            parseException(*loc, "PARSE-COMMAND-ERROR", loadModuleError(mname, xsink));
            return;
        }
        if (!mi) {
            parseException(*loc, "PARSE-COMMAND-ERROR", new QoreStringNodeMaker("cannot load builtin feature '%s' as "
                "a module", mname));
            return;
        }
    }

    // the code that issues the command is the Program being parsed: it has the parse options in force at the
    // directive and, as a parse can be run by restricted code, the restrictions of the code running the parse.  The
    // source of a library module that is being loaded is judged as module code is everywhere in Qore: by the sandbox
    // that governs it, not by parse options, which here are only what the module's Program inherited from the code
    // that loads it; that code does not issue the commands that the module's source issues.  A module that code
    // which is not trusted chose by a path of its own is not library code: that code issues them (issue #5699)
    QoreParseOptions po;
    if (!get_module_def_context() || !qore_module_load_is_library()) {
        po = pgm->getParseOptions() | runtime_get_parse_options();
    }
    bool sandboxed = qore_caller_is_sandboxed();
    if (const QoreModuleCommandContext* outer = QoreModuleCommandContext::get()) {
        po |= outer->getParseOptions();
        sandboxed = sandboxed || outer->isSandboxed();
    }
    QoreModuleCommandContext ctx(QoreModuleCommandContext::MC_PARSE, pgm, pgm, po, sandboxed);
    QoreModuleCommandScope scope(ctx);
    mi->issueModuleCmd(loc, cmd, pgm->getParseExceptionSink());
    // a compiled program replays the command with the restrictions that were in force at the directive
    qore_program_private::get(*pgm)->addModuleParseCommand(mname, cmd, ctx.getParseOptions());
}

#define QORE_MAX_MODULE_ERROR_DESC 200
int QoreModuleManager::issueRuntimeCmd(const char* mname, QoreProgram* pgm, const QoreString& cmd,
        ExceptionSink* xsink, QoreModuleCommandContext::Origin origin, const QoreParseOptions* declared_po) {
    // the restrictions of the code that issues the command, taken before the thread switches to the Program that
    // the command applies to: in the handler, getProgram() is that Program (issue #5699)
    const QoreProgram* issuer = getProgram();
    QoreParseOptions cmd_po;
    bool cmd_sandboxed;
    qore_get_command_restrictions(cmd_po, cmd_sandboxed, declared_po,
        origin == QoreModuleCommandContext::MC_REPLAY);

    // a sandboxed caller stays the caller of the load and of the command, which runs in the target Program below; a
    // module that it chose by path is checked and governed as one that it loads itself (issue #5659)
    QoreCallerSandboxHelper caller_sandbox(pgm, xsink);
    if (*xsink) {
        return -1;
    }

    // a command issued to another Program runs there, and the module decides what it allows by that Program's
    // parse options and sandbox: code that is not trusted would get what its own Program does not allow, such as a
    // Python interpreter with native file access (issue #5659).  It may issue a command to another Program only if its
    // own Program is not sandboxed and the target has every restriction that its code has
    if (pgm && pgm != getProgram()) {
        static const QoreParseOptions restricting = QoreParseOptions(QDOM_ALL) & ~QoreParseOptions::POSITIVE_OPTIONS
            & ~QoreParseOptions(QDOM_IN_MODULE);
        if (qore_program_is_sandboxed(getProgram())) {
            xsink->raiseException("RUNTIME-COMMAND-ERROR", new QoreStringNodeMaker("cannot issue a command for "
                "module '%s' to another Program: sandboxed code can only issue module commands to its own Program",
                mname));
            return -1;
        }
        QoreParseOptions missing = runtime_get_effective_program_parse_options() & restricting
            & ~pgm->getParseOptions();
        if (!!missing) {
            xsink->raiseException("RUNTIME-COMMAND-ERROR", new QoreStringNodeMaker("cannot issue a command for "
                "module '%s' to another Program: the target Program lacks restrictions that the calling code has "
                "(parse options 0x" QLLX ")", mname, missing.getLo()));
            return -1;
        }
    }

    QoreAbstractModule* mi = nullptr;
    // a module that the Program has already loaded is not loaded again: as in runTimeLoadModule(), its parse lock is
    // not needed then, so the command does not wait for a parse of the Program in another thread
    const std::string feature = get_module_feature_name(mname);
    if (pgm && qore_program_private::get(*pgm)->hasCommittedFeature(feature.c_str())) {
        OptLocker ol(&mutex);
        mi = findModuleUnlocked(feature.c_str());
        // a module that a tenant chose is used only as for any lookup of it (see mayUseTenantModule())
        if (mi && mi->isTenantChosen() && !mayUseTenantModule(*mi, pgm)) {
            mi = nullptr;
        }
    }
    if (!mi) {
        // the module is loaded as with Program::loadModule() (see runTimeLoadModule()), with the target Program's
        // parse lock held; the lock is not waited for: a parse of the Program in another thread could be waiting for
        // this thread (two commits that issue module commands to each other's Program, or a parse that waits for a
        // lock that this thread's caller holds, such as the Python GIL), so the command is refused then
        QoreModuleLoadLockHelper aot_init_al;
        ProgramRuntimeParseContextHelper pah(xsink, pgm, true);
        if (*xsink) {
            return -1;
        }
        OptLocker ol(&mutex);
        mi = loadModuleIntern(*xsink, *xsink, mname, pgm);
        if (!mi && !*xsink) {
            xsink->raiseException("RUNTIME-COMMAND-ERROR", new QoreStringNodeMaker("cannot load builtin feature '%s' "
                "as a module", mname));
        }
        if (*xsink) {
            return -1;
        }
        assert(mi);
    }

    // ensure the program is in context
    QoreProgramContextHelper pch(pgm);

    // the handler gets the restrictions of the issuing code, not those of the target Program it runs in (issue #5699)
    QoreModuleCommandContext cmd_ctx(origin, issuer, pgm, cmd_po, cmd_sandboxed);
    QoreModuleCommandScope cmd_scope(cmd_ctx);

    // issue #4254: module commands run unlocked
    mi->issueModuleCmd(&loc_builtin, cmd, xsink);
    // enrich exception description if present
    if (*xsink) {
        // truncate command at first eol or at max 200 chars
        qore_offset_t i = cmd.find('\n');
        if (i == -1) {
            i = cmd.find('\r');
        }
        if (((i == -1) && (cmd.size() > QORE_MAX_MODULE_ERROR_DESC)) || (i > QORE_MAX_MODULE_ERROR_DESC)) {
            i = QORE_MAX_MODULE_ERROR_DESC;
        }
        if (i > 0) {
            QoreString cmd_copy(&cmd, i);
            cmd_copy.trim();
            xsink->appendLastDescription(": module command error from command '%s...'", cmd_copy.c_str());
        } else {
            xsink->appendLastDescription(": module command error from command '%s'", cmd.c_str());
        }
    }
    return *xsink ? -1 : 0;
}

int ModuleManager::issueRuntimeCmd(const char* mname, QoreProgram* pgm, const QoreString& cmd,
        ExceptionSink* xsink) {
    return QMM.issueRuntimeCmd(mname, pgm, cmd, xsink);
}

QoreProgram* ModuleManager::findUserModuleProgram(const char* name) {
    QoreAbstractModule* m = QMM.findModule(name);
    if (!m || !m->isUser()) {
        return nullptr;
    }
    return static_cast<QoreUserModule*>(m)->getProgram();
}

int ModuleManager::setModuleParseOptionMask(const QoreParseOptions& strip, const QoreParseOptions& add,
        ExceptionSink* xsink) {
    return QMM.setParseOptionMask(QoreModuleParseOptionMask{strip, add}, xsink);
}

QoreParseOptions ModuleManager::getMaskableParseOptions() {
    return QoreModuleParseOptionMask::maskable();
}

void ModuleManager::getModuleParseOptionMask(QoreParseOptions& strip, QoreParseOptions& add) {
    QoreModuleParseOptionMask mask = QMM.getParseOptionMask();
    strip = mask.strip;
    add = mask.add;
}

int ModuleManager::runTimeLoadModuleMasked(ExceptionSink* xsink, const char* name, QoreProgram* pgm,
        const QoreParseOptions& strip, const QoreParseOptions& add) {
    return QMM.runTimeLoadModuleMasked(*xsink, *xsink, name, pgm ? pgm : getProgram(),
        QoreModuleParseOptionMask{strip, add});
}

int ModuleManager::addModuleParseOptionMask(const char* name, const QoreParseOptions& strip,
        const QoreParseOptions& add, ExceptionSink* xsink) {
    return QMM.addModuleParseOptionMask(name, QoreModuleParseOptionMask{strip, add}, xsink);
}

int ModuleManager::getModuleParseOptionMask(const char* name, QoreParseOptions& strip, QoreParseOptions& add,
        ExceptionSink* xsink) {
    QoreAbstractModule* mi = QMM.findModule(name);
    if (!mi) {
        xsink->raiseException("MODULE-PARSE-OPTION-MASK-ERROR", "module '%s' is not loaded", name);
        return -1;
    }
    QoreModuleParseOptionMask mask = mi->getParseOptionMask();
    strip = mask.strip;
    add = mask.add;
    return 0;
}

QoreHashNode* ModuleManager::getModuleHash() {
   return QMM.getModuleHash();
}

QoreHashNode* QoreModuleManager::getModuleHash() {
    bool with_filename = !(runtime_get_parse_options() & PO_NO_EXTERNAL_INFO);
    QoreHashNode* h = new QoreHashNode(hashTypeInfo);
    qore_hash_private* ph = qore_hash_private::get(*h);
    AutoLocker al(mutex);
    for (module_map_t::const_iterator i = map.begin(); i != map.end(); ++i) {
        if (!i->second->isPrivate())
            ph->setKeyValueIntern(i->second->getName(), i->second->getHash(with_filename));
    }
    return h;
}

QoreListNode* ModuleManager::getModuleList() {
   return QMM.getModuleList();
}

QoreListNode* QoreModuleManager::getModuleList() {
    bool with_filename = !(runtime_get_parse_options() & PO_NO_EXTERNAL_INFO);
    QoreListNode* l = new QoreListNode(hashTypeInfo);
    AutoLocker al(mutex);
    for (module_map_t::const_iterator i = map.begin(); i != map.end(); ++i) {
        if (!i->second->isPrivate())
            l->push(i->second->getHash(with_filename), nullptr);
    }
    return l;
}

char version_list_t::set(const char* v) {
    ver = v;

    // set version list
    ver.trim();

    char* a;
    char* p = a = (char*)ver.c_str();
    while (*p) {
        if (*p == '.') {
            char save = *p;
            *p = '\0';
            push_back(atoi(a));
            //printd(5, "this=%p a=%s\n", this, a);
            *p = save;
            a = p + 1;
        } else if (!isdigit(*p))
            return *p;
        ++p;
    }
    //printd(5, "this=%p a=%s FINAL\n", this, a);
    push_back(atoi(a));
    return '\0';
}

bool QoreModuleManager::checkModuleLoadCycle(const char* feature, int owner_tid, ExceptionSink& xsink) {
    // walk the wait-for chain starting from the owner we are about to block on; if it leads back to
    // us, parking would deadlock -> a genuine circular module dependency across threads.  Called with
    // mutex held; module_wait_for is guarded by the same mutex as module_load_map.
    int me = q_gettid();
    std::vector<int> chain;
    chain.reserve(module_wait_for.size() + 2);
    chain.push_back(me);
    int cur = owner_tid;
    size_t cancel_check = 0;
    while (true) {
        if (++cancel_check % 100 == 0
                && qore_check_cancel(&xsink, "module dependency cycle check")) {
            return true;
        }
        chain.push_back(cur);
        if (cur == me) {
            // format a readable chain, annotating each thread with the feature it is initializing
            std::map<int, const std::string*> features_by_owner;
            cancel_check = 0;
            for (const auto& entry : module_load_map) {
                if (++cancel_check % 100 == 0
                        && qore_check_cancel(&xsink, "module dependency cycle report")) {
                    return true;
                }
                features_by_owner.emplace(entry.second.owner_tid, &entry.first);
            }
            SimpleRefHolder<QoreStringNode> desc(new QoreStringNodeMaker(
                "circular module dependency detected while "
                "loading feature '%s': ", feature));
            cancel_check = 0;
            for (size_t j = 0; j < chain.size(); ++j) {
                if (++cancel_check % 100 == 0
                        && qore_check_cancel(&xsink, "module dependency cycle report")) {
                    return true;
                }
                if (j) {
                    desc->concat(" -> ");
                }
                auto fi = features_by_owner.find(chain[j]);
                if (fi != features_by_owner.end()) {
                    desc->sprintf("'%s' (tid %d)", fi->second->c_str(), chain[j]);
                } else {
                    desc->sprintf("tid %d", chain[j]);
                }
            }
            xsink.raiseException("CIRCULAR-MODULE-DEPENDENCY", desc.release());
            return true;
        }
        std::map<int, int>::iterator it = module_wait_for.find(cur);
        if (it == module_wait_for.end()) {
            break;                      // owner is runnable (not itself parked) -> no cycle
        }
        cur = it->second;
    }
    // no cycle: commit our edge before waiting so a counterpart can observe it
    module_wait_for[me] = owner_tid;
    return false;
}

void QoreModuleManager::clearModuleLoadWaitEdge(int tid) {
    module_wait_for.erase(tid);
}

ModuleLoadMapHelper::ModuleLoadMapHelper(const char* feature, ExceptionSink& xsink, bool unlock_now)
        : xsink(xsink), unlocked(false) {
    // Only a lingering terminal (LOADED/FAILED) entry, kept alive for waiters to observe, may still be
    // present for this feature; an in-progress reservation on another thread must never reach here
    // (the waiter loop in loadModuleIntern guarantees the feature is absent before we get here).  Such
    // an entry is always this thread's own leftover from an earlier phase of the same load (e.g. the
    // parse-phase reservation handing off to this init-phase reservation for the same feature).
    //
    // Re-open that same entry in place rather than erasing and re-inserting it.  A cross-thread waiter
    // may be parked on it with its wait recorded in the entry's waiters count; erasing would silently
    // discard that count, and a fresh entry that (as here) reuses this thread's TID as owner_tid would
    // then fail the waiter's assert(waiters) on wake-up — it re-finds an owner-matching entry whose
    // waiters count has been reset to 0.  Preserving waiters keeps the parse->init handoff a single
    // continuous single-writer reservation as far as waiter accounting is concerned.
    QoreModuleManager::module_load_map_t::iterator ex = QMM.module_load_map.find(feature);
    if (ex != QMM.module_load_map.end()) {
        assert(ex->second.state != QoreModuleManager::MLS_INITIALIZING);
        // reset the entry to a fresh reservation owned by this thread, preserving its waiters count
        ex->second.owner_tid = q_gettid();
        ex->second.state = QoreModuleManager::MLS_INITIALIZING;
        ex->second.err.clear();
        ex->second.desc.clear();
        i = ex;
    } else {
        i = QMM.module_load_map.insert(QoreModuleManager::module_load_map_t::value_type(
            feature, QoreModuleManager::ModuleLoadEntry(q_gettid()))).first;
    }

    // increment nested load depth counter
    ++module_load_depth;

    if (unlock_now) {
        unlock();
    }
}

void ModuleLoadMapHelper::unlock() {
    if (!unlocked) {
        // Run initialization unlocked.
        QMM.mutex.unlock();
        unlocked = true;
    }
}

void ModuleLoadMapHelper::lock() {
    if (unlocked) {
        QMM.mutex.lock();
        unlocked = false;
    }
}

ModuleLoadMapHelper::~ModuleLoadMapHelper() {
    // decrement nested load depth counter
    --module_load_depth;

    if (unlocked) {
        QMM.mutex.lock();
    }

    // record the terminal state so waiters observe success/failure instead of racing the module map.
    // Every failure path sets xsink before the reservation is released, so a pending exception means
    // this load failed; a clean xsink means it succeeded.
    if (xsink) {
        i->second.state = QoreModuleManager::MLS_FAILED;
        // note: these values can be held in inline short string storage, which has no
        // QoreStringNode, so the data helper must be used to read the bytes
        QoreStringDataHelper err(xsink.getExceptionErr());
        if (err) {
            i->second.err = err.c_str();
        }
        QoreStringDataHelper edesc(xsink.getExceptionDesc());
        if (edesc) {
            i->second.desc = edesc.c_str();
        }
    } else {
        i->second.state = QoreModuleManager::MLS_LOADED;
    }

    // wake any waiters so they can observe the terminal state
    if (QMM.module_load_waiting) {
        QMM.module_load_cond.broadcast();
    }

    // if no cross-thread waiter is parked on this entry, GC it now (fast path, as before); otherwise
    // leave it for the last waiter to erase after it has read the terminal state
    if (i->second.waiters == 0) {
        QMM.module_load_map.erase(i);
    }
}
