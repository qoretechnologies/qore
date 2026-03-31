/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Qore.h

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

#ifndef _QORE_QORE_H

#define _QORE_QORE_H

/** @file Qore.h
    the main header file for the Qore library.  All code using any part of the Qore library's functionality should include this file
*/

// include configuration first if compiling the library
#ifdef _QORE_LIB_INTERN
#include "qore/intern/config.h"
#endif

#include <qore/common.h>
#include <qore/AbstractException.h>
#include <qore/QoreStandardException.h>
#include <qore/QoreXSinkException.h>
#include <qore/QoreCounter.h>

//! global background thread counter (for threads started explicitly by Qore)
DLLEXPORT extern QoreCounter thread_counter;

//! global counter for threads with external lifecycle management (e.g. ThreadPool)
/** These threads are not counted in thread_counter and don't block QoreProgramHelper
    shutdown. They are waited on after program data destruction.
    @since %Qore 2.3
*/
DLLEXPORT extern QoreCounter tp_thread_counter;

#include <qore/QoreEncoding.h>
#include <qore/ReferenceHolder.h>
#include <qore/RuntimeConfig.h>
#include <qore/QoreValue.h>
#include <qore/AbstractQoreNode.h>
#include <qore/QoreListNode.h>
#include <qore/TypedHashDecl.h>
#include <qore/QoreEnumDecl.h>
#include <qore/QoreHashNode.h>
#include <qore/AbstractPrivateData.h>
#include <qore/QoreIteratorBase.h>
#include <qore/QoreObject.h>
#include <qore/QoreProgram.h>
#include <qore/QoreDebugProgram.h>
#include <qore/ModuleManager.h>
#include <qore/QoreLib.h>
#include <qore/QoreStringNode.h>
#include <qore/DateTimeNode.h>
#include <qore/QoreNumberNode.h>
#include <qore/QoreNothingNode.h>
#include <qore/QoreNullNode.h>
#include <qore/QoreNet.h>
#include <qore/QoreURL.h>
#include <qore/QoreFile.h>
#include <qore/QoreQueueHelper.h>
#include <qore/QoreRWLock.h>
#include <qore/QoreNamespace.h>
#include <qore/ExceptionSink.h>
#include <qore/BinaryNode.h>
#include <qore/QoreString.h>
#include <qore/DateTime.h>
#include <qore/QoreType.h>
#include <qore/qore_thread.h>
#include <qore/QoreThreadLock.h>
#include <qore/QoreThreadLocalStorage.h>
#include <qore/QoreCondition.h>
#include <qore/QoreFuture.h>
#include <qore/QoreQueue.h>
#include <qore/DBI.h>
#include <qore/Datasource.h>
#include <qore/SQLStatement.h>
#include <qore/QoreClass.h>
#include <qore/QoreReflection.h>
#include <qore/ScopeGuard.h>
#include <qore/SystemEnvironment.h>
#include <qore/AutoVLock.h>
#include <qore/CallReferenceNode.h>
#include <qore/ReferenceNode.h>
#include <qore/params.h>
#include <qore/QoreTypeSafeReferenceHelper.h>
#include <qore/QoreEvents.h>
#include <qore/UnicodeCharacterIterator.h>
#include <qore/QoreRegexInterface.h>
#include <qore/Sequence.h>
#include <qore/qore-version.h>

//! the complete version string of the qore library
DLLEXPORT extern const char* qore_version_string;

//! the major version number of the qore library
DLLEXPORT extern int qore_version_major;

//! the minor version number of the qore library
DLLEXPORT extern int qore_version_minor;

//! the version number below the minor version number of the qore library
DLLEXPORT extern int qore_version_sub;

//! the version number below the sub version number of the qore library
DLLEXPORT extern int qore_version_patch;

//! the build number of the qore library
/** @deprecated always assigned to 0 - no longer relevant with git
 */
DLLEXPORT extern int qore_build_number;

//! the build target machine word size in bits (32 or 64 normally)
DLLEXPORT extern int qore_target_bits;

//! the git hash of the sources
DLLEXPORT extern const char* qore_git_hash;

//! the build target Operating System name
DLLEXPORT extern const char* qore_target_os;

//! the build target machine architecture name
DLLEXPORT extern const char* qore_target_arch;

//! the qore module directory
DLLEXPORT extern const char* qore_module_dir;

//! the qore user module directory
DLLEXPORT extern const char* qore_user_module_dir;

//! the qore version-specific module directory
DLLEXPORT extern const char* qore_module_ver_dir;

//! the qore version-specific user module directory
DLLEXPORT extern const char* qore_user_module_ver_dir;

//! the c++ compiler used to build qore
DLLEXPORT extern const char* qore_cplusplus_compiler;

//! the compiler flags used to build qore
DLLEXPORT extern const char* qore_cflags;

//! the linker flags used to link qore
DLLEXPORT extern const char* qore_ldflags;

//! information about the build host
DLLEXPORT extern const char* qore_build_host;

//! information about the MPFR library used by the qore library
/** this pointer is null until after qore_init() has been called
 */
DLLEXPORT extern const char* qore_mpfr_info;

//! if the qore library includes debugging or not
DLLEXPORT bool qore_has_debug();

//! the recommended minimum module api major number to use
DLLEXPORT extern int qore_min_mod_api_major;

//! the recommended minimum module api minor number to use
DLLEXPORT extern int qore_min_mod_api_minor;

//! a string giving information about the MPFR library used by the qore library
DLLEXPORT extern const QoreStringMaker mpfrInfo;

#define QLO_NONE                             0   //!< no options (default)
#define QLO_DISABLE_SIGNAL_HANDLING    (1 << 0)  //!< disable qore signal handling entirely
#define QLO_DISABLE_OPENSSL_INIT       (1 << 1)  //!< do not initialize the openssl library (= is initialized before the qore library is initialized)
#define QLO_DISABLE_OPENSSL_CLEANUP    (1 << 2)  //!< do not perform cleanup on the openssl library (= is cleaned up manually)
#define QLO_DISABLE_GARBAGE_COLLECTION (1 << 3)  //!< disable garbage collection / recursive object reference detection
#define QLO_DO_NOT_SEED_RNG            (1 << 4)  //!< disable seeding the random number generator when the Qore library is initialized
#define QLO_DISABLE_TLS_13             (1 << 5)  //!< disable TLS v1.3 (may be set at runtime with qore_set_library_options())
#define QLO_MINIMUM_TLS_13             (1 << 6)  //!< minimum TLS v1.3; overrides QLO_DISABLE_TLS_13 (may be set at runtime with qore_set_library_options())
#define QLO_DISABLE_HTTP2              (1 << 7)  //!< disable HTTP/2 support globally (may be set at runtime with qore_set_library_options())

#define QLO_RUNTIME_OPTS (QLO_DISABLE_TLS_13|QLO_DISABLE_HTTP2)

//! do not perform any initialization or cleanup of the openssl library (= is performed outside of the qore library)
#define QLO_DISABLE_OPENSSL_INIT_CLEANUP (QLO_DISABLE_OPENSSL_INIT|QLO_DISABLE_OPENSSL_CLEANUP)

//! mask of qore library init options that affect qore library cleanup (ie settable with qore_set_library_cleanup_options())
#define QLO_CLEANUP_MASK (QLO_DISABLE_OPENSSL_CLEANUP)

//! initializes the Qore library
/** @param license the license that the library will be used under; note that if the license type is QL_LGPL or
    QL_MIT, then modules tagged with QL_GPL cannot be loaded
    @param default_encoding the default character encoding for the library, if 0 then the environment variables
    QORE_CHARSET and LANG will be processed, in that order, to determine the default character encoding.  If no
    character encoding can be determined from either of these environment variables, UTF-8 will be used as the default
    @param show_module_errors if true then any errors loading qore modules will be output to stdout
    @param init_options a binary "or" sum of the qore library options

    @note The openssl library is also initialized in this function
    @note This function can only be called once and must be called before any other qore facilities are used
    @note The license value must be QL_LGPL or QL_MIT unless the program using Qore is a GPL program, in which case
    QL_GPL may be used (the default)

    @see qore_cleanup()
*/
DLLEXPORT void qore_init(qore_license_t license = QL_GPL, const char* default_encoding = 0,
        bool show_module_errors = false, int init_options = QLO_NONE);

//! frees all memory allocated by the library
/** @note The openssl library is cleaned up as well
    @note This function can only be called once and should be called when a program using the Qore library terminates

    @see qore_init()
*/
DLLEXPORT void qore_cleanup();

//! returns the current library options
/** this function could be checked, for example, if performing external openssl cleanup, if a module has set
    QLO_DISABLE_OPENSSL_CLEANUP, for example, indicating that the openssl library has already been cleaned up, meaning
    that the cleanup should also not be performed externally.
*/
DLLEXPORT int qore_get_library_init_options();

//! the given options will be combined with binary or to the library init options
/** only options that affect library cleanup are settable; returns the new library init option mask
*/
DLLEXPORT int qore_set_library_cleanup_options(int options);

//! returns true if all the bits set in the argument are also set in the qore library init option variable
DLLEXPORT bool qore_check_option(int opt);

//! set library options at runtime
/** @param opts options to set must be in QLO_RUNTIME_OPTS

    @return the options set (opts & QLO_RUNTIME_OPTS)
*/
DLLEXPORT int qore_set_library_options(int opts);

//! returns library options
/** @return library options
*/
DLLEXPORT int qore_get_library_options();

#include <qore/support.h>

// include private definitions if compiling the library
#ifdef _QORE_LIB_INTERN
#include "qore/intern/QoreLibIntern.h"
#endif

#endif  // _QORE_QORE_H
