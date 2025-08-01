/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ModuleManager.h

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#include <vector>

/** @file ModuleManager.h
    provides definitions required to load qore modules
 */

#define QORE_MODULE_API_MAJOR 1  //!< the major number of the Qore module API implemented
#define QORE_MODULE_API_MINOR 4  //!< the minor number of the Qore module API implemented

#define QORE_MODULE_COMPAT_API_MAJOR QORE_MODULE_API_MAJOR  //!< the major number of the earliest recommended Qore module API
#define QORE_MODULE_COMPAT_API_MINOR QORE_MODULE_API_MINOR  //!< the minor number of the earliest recommended Qore module API

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

//! Module info for the init_info method
struct qore_module_init_info {
    //! path to the module itself
    std::string path;
};

//! signature of the module constructor/initialization function
typedef QoreStringNode* (*qore_module_init_t)();

//! signature of the module constructor/initialization function with info
typedef QoreStringNode* (*qore_module_init_info_t)(qore_module_init_info& info);

//! signature of the module namespace change/delta function
typedef void (*qore_module_ns_init_t)(QoreNamespace* root_ns, QoreNamespace* qore_ns);

//! signature of the module destructor function
typedef void (*qore_module_delete_t)();

//! signature of the module parse command function
typedef void (*qore_module_parse_cmd_t)(const QoreString& cmd, ExceptionSink* xsink);

enum mod_op_e { MOD_OP_NONE, MOD_OP_EQ, MOD_OP_GT,
		MOD_OP_GE, MOD_OP_LT, MOD_OP_LE };

typedef std::vector<std::string> strvec_t;

//! Qore module info
/** @since %Qore 0.9.5
*/
struct QoreModuleInfo {
    QoreString name;
    QoreString version;
    QoreString desc;
    QoreString author;
    QoreString url;
    int api_major = -1;
    int api_minor = -1;
    //! either init or init_info can be set
    qore_module_init_t init = nullptr;
    qore_module_ns_init_t ns_init = nullptr;
    qore_module_delete_t del = nullptr;
    qore_module_parse_cmd_t parse_cmd = nullptr;

    qore_license_t license;
    QoreString license_str;

    //! list of binary modules that this binary module depends on
    strvec_t dependencies;

    //! extra information to appear in the module info hash
    QoreHashNode* info = nullptr;

    //! either init or init_info can be set
    qore_module_init_info_t init_info = nullptr;
};

//! Module description function
typedef void (*qore_binary_module_desc_t)(QoreModuleInfo& mod_info);

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

    //! to add a single directory to the QORE_MODULE_DIR list, can only be called before the library initialization function qore_init()
    /**
        @param dir the directory path to add to the list
    */
    DLLEXPORT static void addModuleDir(const char* dir);

    //! no longer supported - removed for security reasons
    /** this function will abort() in debug builds, does nothing in production builds
        @since qore 0.8.4 support for auto module directories was removed
    */
    DLLEXPORT static void addAutoModuleDir(const char* dir);

    //! to add a list of directories separated by ':' characters to the QORE_MODULE_DIR list, can only be called before the library initialization function qore_init()
    /**
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

    // not exported in the public API
    DLLLOCAL ModuleManager();
};

//! the global ModuleManager object
DLLEXPORT extern ModuleManager MM;

static inline bool is_module_api_supported(int major, int minor) {
    for (unsigned i = 0; i < qore_mod_api_list_len; ++i)
        if (qore_mod_api_list[i].major == major && qore_mod_api_list[i].minor == minor)
        return true;
    return false;
}

#endif // _QORE_MODULEMANAGER_H
