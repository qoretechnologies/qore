/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ModuleManager.h

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

#ifndef _QORE_MODULEMANAGER_H

#define _QORE_MODULEMANAGER_H

#include <qore/QoreThreadLock.h>
#include <qore/QoreString.h>
#include <qore/QoreParseOptions.h>

#include <vector>

/** @file ModuleManager.h
    provides definitions required to load qore modules
 */

#define QORE_MODULE_API_MAJOR 2  //!< the major number of the Qore module API implemented
#define QORE_MODULE_API_MINOR 1  //!< the minor number of the Qore module API implemented

#define QORE_MODULE_COMPAT_API_MAJOR QORE_MODULE_API_MAJOR  //!< the major number of the earliest recommended Qore module API
#define QORE_MODULE_COMPAT_API_MINOR 0  //!< the minor number of the earliest recommended Qore module API

//! Revision of the module descriptor protocol of AOT Qore modules.
/** This is separate from the binary module API: ordinary C/C++ modules do not
    use it.  It versions how an AOT module describes itself to the loader, not
    the compatibility of its generated code: since revision 2 every AOT module
    also records the AOT runtime identity of the libqore that compiled it
    (QoreModuleInfo::aot_runtime_identity), and the loader refuses a module
    whose identity differs from its own.  The identity is derived automatically
    from the libqore sources that define the generated-code contract, so it
    changes by itself when that contract can change; this revision only needs
    to be incremented when the descriptor protocol itself changes.

    @since %Qore 3.0: revision 2 adds the AOT runtime identity
*/
#define QORE_AOT_MODULE_ABI_VERSION 2

//! element of qore_mod_api_list;
struct qore_mod_api_compat_s {
   unsigned char major;
   unsigned char minor;
};

//! list of module APIs this library supports
DLLEXPORT extern const qore_mod_api_compat_s* qore_mod_api_list;

//! number of elements in qore_mod_api_list;
DLLEXPORT extern const unsigned qore_mod_api_list_len;

class QoreNamespace;
class QoreStringNode;
class QoreListNode;
class ExceptionSink;
class QoreProgram;
struct QorePluginModuleHandle;

//! Context passed to the module init callback
/** @since %Qore 3.0 (module API 2.0)
*/
struct QoreModuleInitContext {
    //! path to the module itself
    std::string path;
    //! opaque plugin-registration handle; valid only during module init
    const QorePluginModuleHandle* plugin_module_handle = nullptr;
};

//! signature of the module constructor/initialization function
/** @since %Qore 3.0 (module API 2.0): changed from returning QoreStringNode* to void with ExceptionSink
*/
typedef void (*qore_module_init_t)(QoreModuleInitContext& ctx, ExceptionSink& xsink);

//! signature of the module namespace change/delta function
/** @since %Qore 3.0 (module API 2.0): added ExceptionSink parameter
*/
typedef void (*qore_module_ns_init_t)(QoreNamespace* root_ns, QoreNamespace* qore_ns, ExceptionSink& xsink);

//! signature of the module destructor function
typedef void (*qore_module_delete_t)();

//! signature of the module parse command function
typedef void (*qore_module_parse_cmd_t)(const QoreString& cmd, ExceptionSink* xsink);

enum mod_op_e { MOD_OP_NONE, MOD_OP_EQ, MOD_OP_GT,
		MOD_OP_GE, MOD_OP_LT, MOD_OP_LE };

typedef std::vector<std::string> strvec_t;

//! Qore module info
/** @since %Qore 0.9.5
    @since %Qore 3.0 (module API 2.0): removed init_info field; init callback signature changed
*/
struct QoreModuleInfo {
    QoreString name;
    QoreString version;
    QoreString desc;
    QoreString author;
    QoreString url;
    int api_major = -1;
    int api_minor = -1;
    //! module initialization function
    qore_module_init_t init = nullptr;
    //! namespace initialization function
    qore_module_ns_init_t ns_init = nullptr;
    //! module deletion/cleanup function
    qore_module_delete_t del = nullptr;
    //! parse command function (optional)
    qore_module_parse_cmd_t parse_cmd = nullptr;

    //! module license type
    qore_license_t license = QL_MIT;
    //! module license string
    QoreString license_str;

    //! list of binary modules that this binary module depends on
    strvec_t dependencies;

    //! list of optional child modules declared by this module with the %try-child-module parse directive
    /** Child modules are loaded after this module has been completely loaded, initialized, and published; a
        child module that is not installed is ignored, while a child module that is installed but cannot be
        loaded raises an exception.  Unlike \a dependencies, child modules are not required by this module and
        are not loaded before its initialization function runs.

        @since %Qore 3.0
    */
    strvec_t child_modules;

    //! extra information to appear in the module info hash
    QoreHashNode* info = nullptr;

    //! true if this is an AOT-compiled Qore module loaded as a binary module
    bool is_aot = false;

    //! native generated-code ABI revision for an AOT-compiled Qore module
    unsigned aot_abi_version = 0;

    //! the AOT runtime identity of the libqore that compiled an AOT-compiled Qore module
    /** Empty for ordinary binary modules and for AOT modules compiled before the identity was recorded.

        @since %Qore 3.0
    */
    std::string aot_runtime_identity;

    //! set to true if the module's parse command handler honors the restrictions of the code that issues a command
    /** The handler of a module that sets this checks QoreModuleCommandContext::get() for every command that executes
        code, changes a classpath or search path, touches the filesystem or the network, or changes state shared by
        all Programs, and refuses it when the issuing code is not allowed to do that.

        A module that does not set it (any module built with module API 2.0) is not given a command issued by
        restricted code: the command is refused with \c PARSE-COMMAND-ERROR for a \c %module-cmd directive, and with
        \c RUNTIME-COMMAND-ERROR for a command issued at run time or replayed from a compiled program.  Code that is
        not restricted can issue commands to every module.

        @see QoreModuleCommandContext

        @since %Qore 3.0 (module API 2.1)
    */
    bool cmd_caller_aware = false;
};

//! Module description function
typedef void (*qore_binary_module_desc_t)(QoreModuleInfo& mod_info);

class QoreProgram;
class ExceptionSink;

//! The restrictions of the code that issued a module command
/** A module command (a \c %module-cmd directive, Program::issueModuleCmd(), QoreProgram::issueModuleCmd(),
    ModuleManager::issueRuntimeCmd(), or a command recorded in a compiled program and replayed when it is loaded) is
    handled by the parse command function of a binary module (QoreModuleInfo::parse_cmd), which runs in the Program
    that the command applies to: getProgram() there is the target of the command, and its parse options say nothing
    about the code that issued it.  A handler decides what a command may do from an object of this class, which Qore
    fills in from the issuing code before it switches to the target Program (see QoreModuleCommandContext).

    The restrictions are those of the code that issued the command, with the restrictions that it inherits from its
    callers: the effective run-time parse options (including a \c %push-parse-options block), whether a sandbox governs
    it, and, for a command replayed from a compiled program, also the parse options that program is declared with.
    Code of a library module (a module from the module path, not one that the code loading it chose by a path of its
    own) has no parse options: only the sandbox that governs it restricts it, as module code is everywhere in %Qore.  They only grow within a command: a command that a handler
    issues itself has at least the restrictions of the command it is handling.

    An object of this class is a plain value: a handler that hands the restrictions to another thread passes a copy
    (QoreModuleCommandContext::copy()), since the context that QoreModuleCommandContext::get() returns is valid only
    while the handler runs.

    @see QoreModuleCommandContext
    @see QoreModuleInfo::cmd_caller_aware

    @since %Qore 3.0 (module API 2.1)
*/
class QoreModuleCommandRestrictions {
public:
    //! creates unrestricted restrictions
    DLLEXPORT QoreModuleCommandRestrictions();

    //! creates restrictions from parse options and the sandbox state of the issuing code
    /** @param po the parse options of the issuing code; only the options that restrict a functional domain are kept
        (see getRestrictingOptions())
        @param sandboxed true if a sandbox governs the issuing code
    */
    DLLEXPORT QoreModuleCommandRestrictions(const QoreParseOptions& po, bool sandboxed);

    DLLEXPORT QoreModuleCommandRestrictions(const QoreModuleCommandRestrictions& old);
    DLLEXPORT QoreModuleCommandRestrictions& operator=(const QoreModuleCommandRestrictions& old);
    DLLEXPORT virtual ~QoreModuleCommandRestrictions();

    //! returns the parse options of the issuing code that restrict a functional domain
    DLLEXPORT QoreParseOptions getParseOptions() const;

    //! returns true if a sandbox governs the issuing code, or the issuing code runs in a sandboxed Program
    DLLEXPORT bool isSandboxed() const;

    //! returns true if the issuing code is sandboxed or has a restricting parse option
    DLLEXPORT bool isRestricted() const;

    //! returns true if the issuing code has any of the options in \a mask
    DLLEXPORT bool hasAny(const QoreParseOptions& mask) const;

    //! checks that the issuing code may do what a command does, and raises an exception if it may not
    /** Raises the exception in \a xsink, so it must be called on a thread that Qore knows (the thread of the
        handler, or one started with the Qore thread API); another thread asks with hasAny() and isSandboxed() on a
        copy of the restrictions and reports the refusal itself.

        @param mask the restricting parse options (such as \c PO_NO_UNCONTROLLED_APIS) that forbid the action
        @param sandboxed_denied true if the action is also forbidden for sandboxed code
        @param err the exception code to raise (a module's own, such as \c "PYTHON-PARSE-COMMAND-ERROR")
        @param what the action, for the exception description (such as \c "exec")
        @param xsink the exception sink

        @return 0 if the action is allowed, -1 if an exception was raised
    */
    DLLEXPORT int check(const QoreParseOptions& mask, bool sandboxed_denied, const char* err, const char* what,
        ExceptionSink* xsink) const;

    //! returns the parse options that restrict a functional domain
    /** all domain restrictions except the options that grant something and the domain that marks the Program of a
        module
    */
    DLLEXPORT static QoreParseOptions getRestrictingOptions();

protected:
    //! the restricting parse options of the issuing code
    QoreParseOptions po;
    //! true if a sandbox governs the issuing code
    bool sandboxed;
};

//! The context of the module command that the parse command function of a binary module is handling
/** Valid only while the handler runs, on the thread that runs it; see QoreModuleCommandRestrictions.

    @code{.cpp}
static void my_module_parse_cmd(const QoreString& cmd, ExceptionSink* xsink) {
    const QoreModuleCommandContext* ctx = QoreModuleCommandContext::get();
    assert(ctx);
    if (cmd == "exec") {
        // a command that runs code: not for sandboxed code, nor for code that forbids uncontrolled APIs
        if (ctx->check(PO_NO_UNCONTROLLED_APIS | PO_NO_EMBEDDED_LOGIC, true, "MY-COMMAND-ERROR", "exec", xsink)) {
            return;
        }
        ...
    }
}
    @endcode

    @since %Qore 3.0 (module API 2.1)
*/
class QoreModuleCommandContext : public QoreModuleCommandRestrictions {
public:
    //! how the command reached the handler
    enum Origin {
        MC_PARSE = 0,    //!< a \c %module-cmd directive
        MC_RUNTIME = 1,  //!< Program::issueModuleCmd(), QoreProgram::issueModuleCmd() or ModuleManager::issueRuntimeCmd()
        MC_REPLAY = 2,   //!< a command recorded in a compiled program, replayed when the program is loaded
    };

    //! returns the context of the command that the calling thread's handler is handling, or nullptr
    DLLEXPORT static const QoreModuleCommandContext* get();

    //! returns how the command reached the handler
    DLLEXPORT Origin getOrigin() const;

    //! returns the Program that the issuing code runs in; the Program of a command may be its own issuer
    /** For identity only: to decide what the command may do, use the restrictions
    */
    DLLEXPORT const QoreProgram* getIssuerProgram() const;

    //! returns the Program that the command applies to; the same as getProgram() in the handler
    DLLEXPORT const QoreProgram* getTargetProgram() const;

    //! returns a copy of the restrictions that can be kept or given to another thread
    DLLEXPORT QoreModuleCommandRestrictions copy() const;

    DLLEXPORT virtual ~QoreModuleCommandContext();

    //! not for module use: Qore creates the context
    DLLLOCAL QoreModuleCommandContext(Origin origin, const QoreProgram* issuer, const QoreProgram* target,
        const QoreParseOptions& po, bool sandboxed);

    QoreModuleCommandContext(const QoreModuleCommandContext&) = delete;
    QoreModuleCommandContext& operator=(const QoreModuleCommandContext&) = delete;

private:
    Origin origin;
    const QoreProgram* issuer;
    const QoreProgram* target;
};

//! manages the loading of Qore modules from feature or path names.  Also manages adding module changes into QoreProgram objects.
/** in the case that a QoreProgram object is created before a module is loaded externally
    (either through another QoreProgram object or through a direct call to the appropriate
    ModuleManager function), if the QoreProgram object then requests the feature, the
    ModuleManager will load in all namespace (class, constant, etc) changes into the
    QoreProgram object.
    All members and methods are static; there will always only be one of these...
*/
class ModuleManager {
public:
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;

    //! to add a single directory to the QORE_MODULE_DIR list
    /** A directory added before the library initialization function qore_init() holds library code; one added
        afterwards is searched for modules but does not.  The call is ignored for code that a sandbox governs or whose
        parse options restrict a functional domain.

        @param dir the directory path to add to the list
    */
    DLLEXPORT static void addModuleDir(const char* dir);

    //! no longer supported - removed for security reasons
    /** this function will abort() in debug builds, does nothing in production builds
        @since qore 0.8.4 support for auto module directories was removed
    */
    DLLEXPORT static void addAutoModuleDir(const char* dir);

    //! to add a list of directories separated by ':' characters to the QORE_MODULE_DIR list
    /** see addModuleDir()

        @param strlist a list of directories separated by ':' characters to add to the QORE_MODULE_DIR list
    */
    DLLEXPORT static void addModuleDirList(const char* strlist);

    //! no longer supported - removed for security reasons
    /** this function will abort() in debug builds, does nothing in production builds
        @since qore 0.8.4 support for auto module directories was removed
    */
    DLLEXPORT static void addAutoModuleDirList(const char* strlist);

    //! retuns a list of module information hashes, caller owns the list reference returned
    DLLEXPORT static QoreListNode* getModuleList();

    //! retuns a hash of module information hashes, caller owns the list reference returned
    DLLEXPORT static QoreHashNode* getModuleHash();

    //! loads the named module at run time, returns -1 if an exception was raised, 0 for OK
    /** If the feature is already loaded, then the function returns immediately without raising an error.
        The feature's namespace changes are added to the QoreProgram object if the feature is loaded.

        @param name can be either a feature name or the full path to the module file
        @param xsink if any errors are encountered loading the module, then a Qore-language "LOAD-MODULE-ERROR" exception is raised here

        @return -1 if an exception was raised, 0 for OK
    */
    DLLEXPORT static int runTimeLoadModule(const char* name, ExceptionSink* xsink);

    //! loads the named module at run time, returns -1 if an exception was raised, 0 for OK
    /** If the feature is already loaded, then the function returns immediately without raising an error.
        The feature's namespace changes are added to the QoreProgram object if the feature is loaded.

        @param name can be either a feature name or the full path to the module file
        @param pgm the QoreProgram object in which to include all module additions (namespaces, classes, constants, etc) immediately
        @param xsink if any errors are encountered loading the module, then a Qore-language "LOAD-MODULE-ERROR" exception is raised here

        @return -1 if an exception was raised, 0 for OK
    */
    DLLEXPORT static int runTimeLoadModule(const char* name, QoreProgram* pgm, ExceptionSink* xsink);

    //! Loads the module at runtime, returns -1 if an exception was raised, 0 for OK
    /** If the feature is already loaded, then the function loads the namespace additions into the target program, if
        any.

        @param xsink if any errors are encountered loading the module, then a Qore-language "LOAD-MODULE-ERROR"
        exception is raised here
        @param name can be either a feature name or the full path to the module file
        @param pgm the QoreProgram object in which to include all module additions (namespaces, classes, constants,
        etc) immediately
        @param mod_desc_func the module description function; this is for cases when an existing binary code registers
        a %Qore module at runtime; this argument is only used if the module to be loaded is a binary module that has
        not yet been loaded, otherwise it is ignored

        @since %Qore 0.9.5
    */
    DLLEXPORT static int runTimeLoadModule(ExceptionSink* xsink, const char* name, QoreProgram* pgm = nullptr,
        qore_binary_module_desc_t mod_desc_func = nullptr);

    //! Register a statically-linked AOT-compiled module into a target program without dlopen
    /** Intended for AOT <tt>.qo</tt> object files that have been linked directly into the host
        image. The caller provides a pointer to the module's descriptor function (emitted
        by qcc as <tt>&lt;name&gt;_qore_module_desc</tt>); the descriptor is invoked, validated and
        routed through the standard module registration path, skipping the filesystem
        search and dlopen that the normal loader uses.
        @param xsink if any errors occur, a Qore-language "LOAD-MODULE-ERROR" exception is raised here
        @param tpgm the target QoreProgram to register the module into; must be non-null
        @param desc_fn the module descriptor function pointer
        @param path a label used for diagnostics; defaults to "<aot-static>"
        @return 0 on success, -1 if an exception was raised
        @since %Qore 3.0
    */
    DLLEXPORT static int registerAOTStaticModule(ExceptionSink* xsink, QoreProgram* tpgm,
        qore_binary_module_desc_t desc_fn, const char* path = "<aot-static>");

    //! loads the named module at parse time (or before run time, even if parsing is not active), returns a non-0 QoreStringNode pointer if an error occured, caller owns the QoreStringNode pointer's reference count returned if non-0
    /** if the feature is already loaded, then the function returns immediately without raising an error
        The feature's namespace changes are added to the QoreProgram object if the feature is loaded and the pgm argument is non-zero.
        @param name can be either a feature name or the full path to the module file
        @param pgm the QoreProgram object in which to include all module additions (namespaces, classes, constants, etc) immediately
    */
    DLLEXPORT static QoreStringNode* parseLoadModule(const char* name, QoreProgram* pgm = nullptr);

    //! registers the given user module from the module source given as an argument
    DLLEXPORT void registerUserModuleFromSource(const char* name, const char* src, QoreProgram* pgm, ExceptionSink* xsink);

    //! adds the standard module directories to the module path (only necessary if the module paths are set up manually, otherwise these paths are added automatically when qore_init() is called)
    DLLEXPORT static void addStandardModulePaths();

    //! Issues a module command for the given module
    /** If the module was not already loaded in the given QoreProgram, it will be loaded for this call

        @param mname the module to issue the command in
        @param pgm the QoreProgram for the module and command
        @param cmd the command to issue
        @param xsink if any errors are encountered, a Qore-language exception is raised here

        @return -1 if a Qore-language exception was raised, 0 for OK

        @since %Qore 2.0
    */
    DLLEXPORT int issueRuntimeCmd(const char* mname, QoreProgram* pgm, const QoreString& cmd, ExceptionSink* xsink);

    //! Returns the owning QoreProgram for the named user module, or nullptr.
    /** User (.qm) modules are parsed and run inside a dedicated QoreProgram; that
        Program is the canonical home for the module's classes / constants / functions
        and is the same instance for every consumer Program that loads the module.
        Binary modules don't have an owning Program and return nullptr.

        @param name the module name (e.g. "QUnit", "SqlUtil")
        @return the module's owning QoreProgram, or nullptr if the module is not
                loaded, is not a user module, or cannot be resolved

        @since %Qore 3.0
    */
    DLLEXPORT static QoreProgram* findUserModuleProgram(const char* name);

    //! Sets the parse option mask applied to every user module loaded from now on
    /** A user module (source or AOT) inherits the restrictions of the Program that loads it.  The mask strips the
        restrictions in \a strip from what the module inherits and adds the restrictions in \a add.  For example, stripping
        @ref PO_NO_THREAD_CONTROL and @ref PO_NO_THREAD_CLASSES lets code that may not use threads load trusted
        modules that do.  Call it before loading the modules it should apply to; it replaces the previous mask.  Use
        addModuleParseOptionMask() for modules that are loaded already.

        @param strip the restrictions that modules do not inherit
        @param add the restrictions added to the parse options of modules
        @param xsink raises \c MODULE-PARSE-OPTION-MASK-ERROR if a mask contains an option that is not a
        restriction (an explicitly-allowed, language, style or module option), or an option in both masks

        @return 0 for OK, -1 if an exception was raised

        @note this C++ API is not restricted; the %Qore API (set_module_parse_option_mask()) requires a Program that
        may control the process and has no sandbox manager

        @since %Qore 3.0
    */
    DLLEXPORT static int setModuleParseOptionMask(const QoreParseOptions& strip, const QoreParseOptions& add,
            ExceptionSink* xsink);

    //! Returns every restriction that a module parse option mask may strip or add
    /** Explicitly-allowed (positive), language, style and module options cannot be masked.  A host that grants its
        trusted modules all functionality loads them with runTimeLoadModuleMasked() and this mask as \a strip

        @since %Qore 3.0
    */
    DLLEXPORT static QoreParseOptions getMaskableParseOptions();

    //! Returns the parse option mask applied to every user module loaded from now on
    /** @param strip set to the restrictions that modules do not inherit
        @param add set to the restrictions added to the parse options of modules

        @since %Qore 3.0
    */
    DLLEXPORT static void getModuleParseOptionMask(QoreParseOptions& strip, QoreParseOptions& add);

    //! Loads a module with a parse option mask, in addition to the mask that applies to every module
    /** The mask applies to the module and to the modules it loads while it is loaded; a module that is loaded
        already gets the mask with addModuleParseOptionMask()

        @param xsink for exceptions loading the module and for \c MODULE-PARSE-OPTION-MASK-ERROR (see
        setModuleParseOptionMask())
        @param name the name or path of the module
        @param pgm the Program to load the module into, or nullptr for the current Program
        @param strip the restrictions that the module does not inherit
        @param add the restrictions added to the module's parse options

        @return 0 for OK, -1 if an exception was raised

        @since %Qore 3.0
    */
    DLLEXPORT static int runTimeLoadModuleMasked(ExceptionSink* xsink, const char* name, QoreProgram* pgm,
            const QoreParseOptions& strip, const QoreParseOptions& add);

    //! Adds a parse option mask to a loaded module
    /** The restrictions in \a strip are removed from the parse options of the module's Program, with which its code
        runs, and those in \a add are added to them.  Restrictions stripped earlier are not restored.  A binary module
        has no Program; its mask is only recorded.

        @param name the name of the loaded module
        @param strip the restrictions to strip
        @param add the restrictions to add
        @param xsink raises \c MODULE-PARSE-OPTION-MASK-ERROR for an invalid mask (see setModuleParseOptionMask())
        or a module that is not loaded

        @return 0 for OK, -1 if an exception was raised

        @since %Qore 3.0
    */
    DLLEXPORT static int addModuleParseOptionMask(const char* name, const QoreParseOptions& strip,
            const QoreParseOptions& add, ExceptionSink* xsink);

    //! Returns the parse option mask of a loaded module
    /** @param name the name of the loaded module
        @param strip set to the restrictions the module does not inherit
        @param add set to the restrictions added to the module's parse options
        @param xsink raises \c MODULE-PARSE-OPTION-MASK-ERROR if the module is not loaded

        @return 0 for OK, -1 if an exception was raised

        @since %Qore 3.0
    */
    DLLEXPORT static int getModuleParseOptionMask(const char* name, QoreParseOptions& strip, QoreParseOptions& add,
            ExceptionSink* xsink);

    // not exported in the public API
    DLLLOCAL ModuleManager();
};

//! the global ModuleManager object
DLLEXPORT extern ModuleManager MM;

//! RAII helper marking that the current thread is inside a module load.
/** Module loading does not serialize on a process-global lock: each module is initialized by one
    thread while other threads that need it wait for that thread, and a wait that would close a
    cycle between threads raises \c CIRCULAR-MODULE-DEPENDENCY instead of deadlocking.  This
    helper acquires no lock.  It records, per thread, how deeply the thread is nested in module
    loads; libqore's own module-loading paths construct it, and a binary module may construct it
    around code that loads modules.

    Its purpose is the debug-build check paired with QoreModuleInnerLockHelper: a module-private
    lock must never be held across the <i>start</i> of a module load, because the thread that
    loads the module can need that same lock (for example to import Java classes through the
    module) while the holder waits for the load.  A thread that holds a lock marked with
    QoreModuleInnerLockHelper when it enters its outermost module load trips the assertion in
    this helper's constructor.  Taking a marked lock during a module load, inside this helper's
    scope, is legal.

    In non-debug builds this has no effect beyond the depth counter.

    @since %Qore 3.0
*/
class QoreModuleLoadLockHelper {
public:
    //! Marks the current thread as inside a module load; in debug builds, asserts that no module-inner lock is held when the outermost load starts
    DLLEXPORT QoreModuleLoadLockHelper();

    //! Marks the end of the module load started by the constructor
    DLLEXPORT ~QoreModuleLoadLockHelper();

    QoreModuleLoadLockHelper(const QoreModuleLoadLockHelper&) = delete;
    QoreModuleLoadLockHelper& operator=(const QoreModuleLoadLockHelper&) = delete;
};

//! RAII helper marking that the current thread holds a module-private lock that must not be held across a module load.
/** Modules use this to mark a private lock that code running inside a module load can take.
    Starting a module load while such a lock is held - whether directly or by triggering a module
    load anywhere in libqore - can deadlock against the thread loading the module, and in debug
    builds QoreModuleLoadLockHelper asserts that this does not happen.

    A marked lock taken while the thread is already inside a module load (within a
    QoreModuleLoadLockHelper scope) is allowed.

    In non-debug builds this is inert.

    @since %Qore 3.0
*/
class QoreModuleInnerLockHelper {
public:
    //! Marks a module-inner lock as held by the current thread
    DLLEXPORT QoreModuleInnerLockHelper();

    //! Marks a module-inner lock as released by the current thread
    DLLEXPORT ~QoreModuleInnerLockHelper();

    QoreModuleInnerLockHelper(const QoreModuleInnerLockHelper&) = delete;
    QoreModuleInnerLockHelper& operator=(const QoreModuleInnerLockHelper&) = delete;
};

static inline bool is_module_api_supported(int major, int minor) {
    for (unsigned i = 0; i < qore_mod_api_list_len; ++i)
        if (qore_mod_api_list[i].major == major && qore_mod_api_list[i].minor == minor)
        return true;
    return false;
}

#endif // _QORE_MODULEMANAGER_H
