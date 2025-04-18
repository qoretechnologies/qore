/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreLib.h

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

#ifndef _QORE_QORELIB_H

#define _QORE_QORELIB_H

#include <qore/common.h>
#include <qore/QoreThreadLock.h>
#include <qore/qore_bitopts.h>
#include <qore/safe_dslist>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <strings.h>
#include <sys/types.h>
#include <vector>

//! signal vector
typedef std::vector<int> sig_vec_t;

/** @file QoreLib.h
    contains prototypes for various helper functions in the Qore library
 */

//! not defined because this version of Qore has moved all XML functionality to the "xml" module
#undef _QORE_HAS_QORE_XMLNODE_CLASS

//! not defined because this version of Qore has moved all XML functionality to the "xml" module
#undef _QORE_HAS_QORE_XMLREADER_CLASS

//! not defined because this version of Qore has moved all XML functionality to the "xml" module
#undef _QORE_HAS_QORE_XMLDOC_CLASS

//! defined because this version of Qore supports hard typing, overloading, default arguments, etc
#define _QORE_HAS_HARD_TYPING 1

//! defined because this version of Qore supports the execRaw() DBI function
#define _QORE_HAS_DBI_EXECRAW 1

//! defined because this version of Qore has time zone support
#define _QORE_HAS_TIME_ZONES 1

//! defined because this version of Qore has thread resource IDs
#define _QORE_HAS_THREAD_RESOURCE_IDS 1

//! defined because this version of Qore has the prepared statement API
#define _QORE_HAS_PREPARED_STATMENT_API 1

//! defined because this version of Qore has the Datasource::activeTransaction() function
#define _QORE_HAS_DATASOURCE_ACTIVETRANSACTION 1

//! defined because this version of Qore supports the DBI selectRow() function
#define _QORE_HAS_DBI_SELECT_ROW 1

//! defined because this version of Qore supports the number type (QoreNumberNode)
#define _QORE_HAS_NUMBER_TYPE 1

//! defined because this version of Qore has the q_path_is_readable() function
#define _QORE_HAS_PATH_IS_READABLE 1

//! defined because this version of Qore supports the DBI option APIs
#define _QORE_HAS_DBI_OPTIONS 1

//! defined because this version of Qore has the find_create_timezone() function
#define _QORE_HAS_FIND_CREATE_TIMEZONE 1

//! defined because this version of Qore has a QoreNumberNode constructor with a precision specifier
#define _QORE_HAS_NUMBER_CONS_WITH_PREC 1

//! defined because this version of Qore has the QoreFileOjectHelper class
#define _QORE_HAS_FILE_OBJECT_HELPER 1

//! defined because this version of Qore has the QoreQueueOjectHelper class
#define _QORE_HAS_QUEUE_OBJECT_HELPER 1

//! defined becaus this version of Qore has the QoreHttpClientObject class
#define _QORE_HAS_QOREHTTPCLIENTOBJECT 1

//! defined because this version of Qore supports the DBI describe API
#define _QORE_HAS_DBI_DESCRIBE 1

//! defined because this version of Qore supports the DBI event API
#define _QORE_HAS_DBI_EVENTS 1

//! defined because this version of Qore has a Queue class definition in public headers
#define _QORE_HAS_QUEUE_OBJECT 1

//! defined because this version of Qore has a public Socket performance API
#define _QORE_HAS_SOCKET_PERF_API 1

//! defined because this version of Qore has the QL_MIT license enum value
#define _QORE_HAS_QL_MIT 1

//! defined because this version of Qore has the DateTime::addSecondsTo() API
#define _QORE_HAS_DATETIME_ADD_SECONDS_TO 1

//! defined for q_enforce_thread_size_on_primary_thread()
#define _QORE_HAS_ENFORCE_THREAD_SIZE_ON_PRIMARY_THREAD 1

// qore code flags
#define QCF_NO_FLAGS                     0   //! no flag
#define QCF_NOOP                   (1 << 0)  //! this variant is a noop, meaning it returns a constant value with the given argument types
#define QCF_USES_EXTRA_ARGS        (1 << 1)  //! code accesses arguments beyond the declared parameter list
#define QCF_CONSTANT_INTERN        (1 << 2)  //! internal constant flag, use QCF_CONSTANT instead
#define QCF_DEPRECATED             (1 << 3)  //! function or method is deprecated and will be removed in a later release
#define QCF_RET_VALUE_ONLY         (1 << 4)  //! code only returns a value and has no other side effects
#define QCF_RUNTIME_NOOP           (1 << 5)  //! this variant is a noop like QCF_NOOP, but additionally is not available to programs executing with %require-types (PO_REQUIRE_TYPES)
#define QCF_ABSTRACT_OVERRIDE_ALL  (1 << 6)  //! this variant overrides all abstract base class variants in the same method

// composite flags
#define QCF_CONSTANT (QCF_CONSTANT_INTERN | QCF_RET_VALUE_ONLY) //! code is safe to use in a constant expression (i.e. has no side effects, does not change internal state, cannot throw an exception under any circumstances, just returns a calculation based on its arguments)

class BinaryNode;
class QoreStringNode;
class ExceptionSink;

//! function to try and make a class name out of a file path, returns a new string that must be free()ed
DLLEXPORT char* make_class_name(const char* fn);

//! a string formatting function that works with Qore data structures
DLLEXPORT QoreStringNode* q_sprintf(const QoreListNode* params, int field, int offset, ExceptionSink* xsink);

//! a string formatting function that works with Qore data structures
DLLEXPORT QoreStringNode* q_vsprintf(const QoreListNode* params, int field, int offset, ExceptionSink* xsink);

//! thread-safe version of "localtime()"
DLLEXPORT struct tm* q_localtime(const time_t* clock, struct tm* tms);

//! thread-safe version of "gmtime()"
DLLEXPORT struct tm* q_gmtime(const time_t* clock, struct tm* tms);

//! returns the seconds from the epoch
DLLEXPORT int64 q_epoch();

//! returns the seconds and microseconds from the epoch
DLLEXPORT int64 q_epoch_us(int &us);

//! returns the seconds and nanoseconds from the epoch
DLLEXPORT int64 q_epoch_ns(int &us);

//! thread-safe basename function (resulting pointer must be free()ed)
DLLEXPORT char* q_basename(const char* path);

//! returns a pointer within the same string
DLLEXPORT char* q_basenameptr(const char* path);

//! thread-safe dirname function (resulting pointer must be free()ed)
DLLEXPORT char* q_dirname(const char* path);

//! frees memory if there is an allocation error
DLLEXPORT void* q_realloc(void* ptr, size_t size);

#ifndef _Q_WINDOWS
//! thread-safe version of getpwuid(): returns a Qore hash of the passwd information from the uid if possible, otherwise 0
DLLEXPORT QoreHashNode* q_getpwuid(uid_t uid);

//! thread-safe version of getpwnam(): returns a Qore hash of the passwd information from the username if possible, otherwise 0
DLLEXPORT QoreHashNode* q_getpwnam(const char* name);

//! thread-safe version of getgrgid(): returns a Qore hash of the group information from the gid if possible, otherwise 0
DLLEXPORT QoreHashNode* q_getgrgid(uid_t uid);

//! thread-safe version of getgrnam(): returns a Qore hash of the group information from the group name if possible, otherwise 0
DLLEXPORT QoreHashNode* q_getgrnam(const char* name);

//! thread-safe way to lookup a uid from a username
/**
   @param name the username to look up
   @param uid the uid returned
   @return 0 for no error, non-zero is an error code like errno
 */
int q_uname2uid(const char* name, uid_t &uid);

//! thread-safe way to lookup a gid from a group name
/**
   @param name the group to look up
   @param gid the gid returned
   @return 0 for no error, non-zero is an error code like errno
 */
int q_gname2gid(const char* name, gid_t &gid);
#endif // ! windows

//! sets up the Qore ARGV and QORE_ARGV values
DLLEXPORT void qore_setup_argv(int pos, int argc, char* argv[]);

//! returns the license type that the library has been initialized under
DLLEXPORT qore_license_t qore_get_license();

//! instead of calling "exit()", call qore_exit_process() to exit without risking a crash if other threads are running
DLLEXPORT void qore_exit_process(int rc);

//! STL-like list containing all presently-loaded Qore features
/** this list must be thread-safe for reading, writing under a lock
 */
class FeatureList : public safe_dslist<std::string> {
private:
   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL FeatureList(const FeatureList&);

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL FeatureList& operator=(const FeatureList&);

public:
   //! initialized by the library, constructor not exported
   DLLLOCAL FeatureList();

   //! destructor not exported
   DLLLOCAL ~FeatureList();
};

//! list of qore features
DLLEXPORT extern FeatureList qoreFeatureList;

//! find one of any characters in a string
static inline char* strchrs(const char* str, const char* chars) {
   while (*str) {
      if (strchr(chars, *str))
	 return (char* )str;
      str++;
   }
   return 0;
}

//! find a character in a string up to len
static inline char* strnchr(const char* str, int len, char c) {
   int i = 0;
   while (i++ != len) {
      if (*str == c)
	 return (char* )str;
      ++str;
   }
   return 0;
}

//! convert a string to lower-case in place
static inline void strtolower(char* str) {
   while (*(str)) {
      (*str) = tolower(*str);
      str++;
   }
}

//! convert a string to upper-case in place
static inline char* strtoupper(char* str) {
   char* p = str;
   while (*(p)) {
      *p = toupper(*p);
      p++;
   }
   return str;
}

//! for getting an integer number of seconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int getSecZeroInt(QoreValue a);

//! for getting an integer number of seconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int64 getSecZeroBigInt(QoreValue a);

//! for getting an integer number of seconds, with -1 as the default, from either a relative time value or an integer value
DLLEXPORT int getSecMinusOneInt(QoreValue a);

//! for getting an integer number of seconds, with -1 as the default, from either a relative time value or an integer value
DLLEXPORT int64 getSecMinusOneBigInt(QoreValue a);

//! for getting an integer number of milliseconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int getMsZeroInt(QoreValue a);

//! for getting an integer number of milliseconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int64 getMsZeroBigInt(QoreValue a);

//! for getting an integer number of milliseconds, with -1 as the default, from either a relative time value or an integer value
DLLEXPORT int getMsMinusOneInt(QoreValue a);

//! for getting an integer number of milliseconds, with -1 as the default, from either a relative time value or an integer value
DLLEXPORT int64 getMsMinusOneBigInt(QoreValue a);

//! for getting an integer number of microseconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int getMicroSecZeroInt(QoreValue a);

//! for getting an integer number of microseconds, with 0 as the default, from either a relative time value or an integer value
DLLEXPORT int64 getMicroSecZeroInt64(QoreValue a);

//! to check if an AbstractQoreNode object is NOTHING
static inline bool is_nothing(const AbstractQoreNode* n) {
    if (!n || n->getType() == NT_NOTHING)
        return true;

    return false;
}

//! to deref an AbstractQoreNode (when the pointer may be 0)
static inline void discard(AbstractQoreNode* n, ExceptionSink* xsink) {
    if (n)
        n->deref(xsink);
}

static inline const char* get_type_name(const AbstractQoreNode* n) {
    return n ? n->getTypeName() : "nothing";
}

//! returns a string type description of the full type of the value contained (ex: \c "nothing" for a null AbstractQoreNode pointer)
/** differs from the return value of get_type_name() for complex types (ex: \c "hash<string, int>")

    @param n the value to process

    @since %Qore 0.8.13
*/
DLLEXPORT const char* get_full_type_name(const AbstractQoreNode* n);

//! returns a string type description of the full type of the value contained (ex: \c "nothing" for a null AbstractQoreNode pointer)
/** differs from the return value of get_type_name() for complex types (ex: \c "hash<string, int>")

    @param n the value to process

    @param with_namespaces if true then class and hashdecl names are given with full namespace paths

    @since %Qore 1.0
*/
DLLEXPORT const char* get_full_type_name(const AbstractQoreNode* n, bool with_namespaces);

//! returns a string type description of the full type of the value contained (ex: \c "nothing" for a null AbstractQoreNode pointer)
/** will return NOTHING (delete object of class ...) for invalid objects

    @param n the value to process
    @param with_namespaces if true then class and hashdecl names are given with full namespace paths
    @param scratch scratch space for creating formatting string return values

    @since %Qore 1.18
*/
DLLEXPORT const char* get_full_type_name(const AbstractQoreNode* n, bool with_namespaces, QoreString& scratch);

static inline qore_type_t get_node_type(const AbstractQoreNode* n) {
    return n ? n->getType() : NT_NOTHING;
}

typedef QoreStringNode* (*qore_uncompress_to_string_t)(const BinaryNode* b, const QoreEncoding* enc,
    ExceptionSink* xsink);
typedef BinaryNode* (*qore_uncompress_to_binary_t)(const BinaryNode* b, ExceptionSink* xsink);

//! compresses data with the DEFLATE algorithm
DLLEXPORT BinaryNode* qore_deflate(void* ptr, unsigned long len, int level, ExceptionSink* xsink);
//! decompresses data compressed with the DEFLATE algorithm to a string
DLLEXPORT QoreStringNode* qore_inflate_to_string(const BinaryNode* b, const QoreEncoding* enc, ExceptionSink* xsink);
//! decompresses data compressed with the DEFLATE algorithm to a binary
DLLEXPORT BinaryNode* qore_inflate_to_binary(const BinaryNode* b, ExceptionSink* xsink);
//! gzips data
DLLEXPORT BinaryNode* qore_gzip(void* ptr, unsigned long len, int level, ExceptionSink* xsink);
//! gunzips compressed data to a string
DLLEXPORT QoreStringNode* qore_gunzip_to_string(const BinaryNode* bin, const QoreEncoding* enc, ExceptionSink* xsink);
//! gunzips compressed data to a binary
DLLEXPORT BinaryNode* qore_gunzip_to_binary(const BinaryNode* bin, ExceptionSink* xsink);
//! compresses data with bzip2
DLLEXPORT BinaryNode* qore_bzip2(void* ptr, unsigned long len, int level, ExceptionSink* xsink);
//! decompresses bzip2 data to a string
DLLEXPORT QoreStringNode* qore_bunzip2_to_string(const BinaryNode* bin, const QoreEncoding* enc,
    ExceptionSink* xsink);
//! decompresses bzip2 data to a binary
DLLEXPORT BinaryNode* qore_bunzip2_to_binary(const BinaryNode* bin, ExceptionSink* xsink);

//! parses a string of base64-encoded data and returns a BinaryNode
DLLEXPORT BinaryNode* parseBase64(const char* buf, int len, ExceptionSink* xsink);

//! parses a string of base64-url-encoded data and returns a BinaryNode
DLLEXPORT BinaryNode* parseBase64Url(const char* buf, int len, ExceptionSink* xsink);

//! parses a string of hex characters and returns a BinaryNode
DLLEXPORT BinaryNode* parseHex(const char* buf, int len, ExceptionSink* xsink);

class AbstractQoreZoneInfo;

//! returns a time zone for the given time zone UTC offset
DLLEXPORT const AbstractQoreZoneInfo* findCreateOffsetZone(int seconds_east);

//! returns a time zone for the given region name or UTC offset given as a string ("+01:00")
/** @param name the name of the region to find or a UTC offset given as a string ("+01:00")
    @param xsink if the given region is not found or valid or any error occur finding or loading the given region, exception info is stored here and the function returns 0

    @return the time zone region found or 0 if the timezone is UTC (also in case of an exception 0 is returned - check xsink after calling)
 */
DLLEXPORT const AbstractQoreZoneInfo* find_create_timezone(const char* name, ExceptionSink* xsink);

//! returns the UTC offset and local time zone name for the given time given as seconds from the epoch (1970-01-01Z)
DLLEXPORT int tz_get_utc_offset(const AbstractQoreZoneInfo* tz, int64 epoch_offset, bool &is_dst, const char* &zone_name);
//! returns true if the zone has daylight savings time ever
DLLEXPORT bool tz_has_dst(const AbstractQoreZoneInfo* tz);
//! returns the reion name for the given time zone
DLLEXPORT const char* tz_get_region_name(const AbstractQoreZoneInfo* tz);

//! option: atomic operations
#define QORE_OPT_ATOMIC_OPERATIONS       "atomic operations"
//! option: stack guard
#define QORE_OPT_STACK_GUARD             "stack guard"
//! option: signal handling
#define QORE_OPT_SIGNAL_HANDLING         "signal handling"
//! option: runtime stack tracing
#define QORE_OPT_RUNTIME_STACK_TRACE     "runtime stack tracing"
//! option: library debugging
#define QORE_OPT_LIBRARY_DEBUGGING       "library debugging"
//! option: ssh0 algorithm supported (depends on openssl used to compile qore)
#define QORE_OPT_SHA                     "openssl sha"
//! option: ssh224 algorithm supported (depends on openssl used to compile qore)
#define QORE_OPT_SHA224                  "openssl sha224"
//! option: ssh256 algorithm supported (depends on openssl used to compile qore)
#define QORE_OPT_SHA256                  "openssl sha256"
//! option: ssh384 algorithm supported (depends on openssl used to compile qore)
#define QORE_OPT_SHA384                  "openssl sha384"
//! option: ssh512 algorithm supported (depends on openssl used to compile qore)
#define QORE_OPT_SHA512                  "openssl sha512"
//! option: mdc2 algorithm supported (depends on openssl used to compile qore)
#define QORE_OPT_MDC2                    "openssl mdc2"
//! option: rc5 algorithm supported (depends on openssl used to compile qore)
#define QORE_OPT_RC5                     "openssl rc5"
//! option: md2 algorithm supported (depends on openssl used to compile qore)
#define QORE_OPT_MD2                     "openssl md2"
//! option: dss & dss1 algorithms supported (depends on openssl used to compile qore)
#define QORE_OPT_DSS                     "openssl dss"
//! option: TermIOS class available
#define QORE_OPT_TERMIOS                 "termios"
//! option: file locking
#define QORE_OPT_FILE_LOCKING            "file locking"
//! option: unix user/group management functions available
#define QORE_OPT_UNIX_USERMGT            "unix user management"
//! option: unix file management functions available
#define QORE_OPT_UNIX_FILEMGT            "unix file management"
//! options: deterministic garbage collection
#define QORE_OPT_DETERMINISTIC_GC        "deterministic GC"
//! option: round() function available
#define QORE_OPT_FUNC_ROUND              "round()"
//! option: timegm() function available
#define QORE_OPT_FUNC_TIMEGM             "timegm()"
//! option: seteuid() function available
#define QORE_OPT_FUNC_SETEUID            "seteuid()"
//! option: setegid() function available
#define QORE_OPT_FUNC_SETEGID            "setegid()"
//! option: system() function available
#define QORE_OPT_FUNC_SYSTEM             "system()"
//! option: kill() function available
#define QORE_OPT_FUNC_KILL               "kill()"
//! option: fork() function available
#define QORE_OPT_FUNC_FORK               "fork()"
//! option: getppid() function available
#define QORE_OPT_FUNC_GETPPID            "getppid()"
//! option: statvfs() function available
#define QORE_OPT_FUNC_STATVFS            "statvfs()"
//! option: setsid() function available
#define QORE_OPT_FUNC_SETSID             "setsid()"
//! option: is_executable() function available
#define QORE_OPT_FUNC_IS_EXECUTABLE      "is_executable()"
//! option: close_all_fd() function available
#define QORE_OPT_FUNC_CLOSE_ALL_FD       "close_all_fd()"
//! option: get_netif_list() function available
#define QORE_OPT_FUNC_GET_NETIF_LIST     "get_netif_list()"

//! option type feature
#define QO_OPTION     0
#define QO_ALGORITHM  1
#define QO_FUNCTION   2

//! definition of the elements in the qore_option_list
struct qore_option_s {
    const char* option;   //!< name of the option
    const char* constant; //!< name of the constant for this option
    int type;             //!< the type of the option
    bool value;           //!< the value of the option
};

//! returns the error string as a QoreStringNode
DLLEXPORT QoreStringNode* q_strerror(int errnum);
//! concatenates the error string to the given string
DLLEXPORT void q_strerror(QoreString &str, int errnum);

//! list of qore options
DLLEXPORT extern const qore_option_s* qore_option_list;
//! number of elements in the option list
DLLEXPORT extern size_t qore_option_list_size;

//! allows a module to take over ownership of a signal
/** @param sig signal number
    @param name module name taking ownership of managing the signal

    @return nullptr for OK, non-zero for failed (error message); in case a QoreStringNode pointer is returned, the
    caller must dereference it

    @note this is equivalent to qore_reassign_signal(sig, name, false)

    @see
    - @ref qore_reassign_signals() to allocate multiple signals to a module atomically
    - @ref qore_release_signal() to release a signal allocation manually
    - @ref qore_release_signals() to release multiple signal allocations manually
 */
DLLEXPORT QoreStringNode* qore_reassign_signal(int sig, const char* name);

//! allows a module to take over ownership of a signal
/** @param sig signal number
    @param name module name taking ownership of managing the signal
    @param reuse_sys do not fail if signals are allocated to the system already; in this case a failuse will only
    happen when %Qore code has a signal handler assigned

    @return nullptr for OK, non-zero for failed (error message); in case a QoreStringNode pointer is returned, the
    caller must dereference it

    @since %Qore 1.19

    @see
    - @ref qore_reassign_signals() to allocate multiple signals to a module atomically
    - @ref qore_release_signal() to release a signal allocation manually
    - @ref qore_release_signals() to release multiple signal allocations manually
 */
DLLEXPORT QoreStringNode* qore_reassign_signal(int sig, const char* name, bool reuse_sys);

//! allows a module to take over ownership of multiple signals atomically
/** @param sig_vec a vector of signal numbers to allocate to the module
    @param name module name taking ownership of managing the signal

    @return nullptr for OK (all signals were allocated to the module), non-zero for failed (error message; no signals
    were allocated); in case a QoreStringNode pointer is returned, the caller must dereference it

    @note
    - if any signal cannot be allocated to the module, no signals are allocated to the module; the call either
      succeeds for all or fails for all; no partial failures are possible; use this variant when allocating multiple
      signals to a module
    - this is equivalent to qore_reassign_signals(sig_vec, name, false)

    @see
    - @ref qore_reassign_signal() to allocate a single signal to a module
    - @ref qore_release_signal() to release a signal allocation manually
    - @ref qore_release_signals() to release multiple signal allocations manually

    @since %Qore 0.8.13.1
*/
DLLEXPORT QoreStringNode* qore_reassign_signals(const sig_vec_t& sig_vec, const char* name);

//! allows a module to take over ownership of multiple signals atomically
/** @param sig_vec a vector of signal numbers to allocate to the module
    @param name module name taking ownership of managing the signal
    @param reuse_sys do not fail if signals are allocated to the system already; in this case a failuse will only
    happen when %Qore code has a signal handler assigned

    @return nullptr for OK (all signals were allocated to the module), non-zero for failed (error message; no signals
    were allocated); in case a QoreStringNode pointer is returned, the caller must dereference it

    @note if any signal cannot be allocated to the module, no signals are allocated to the module; the call either
    succeeds for all or fails for all; no partial failures are possible; use this variant when allocating multiple
    signals to a module

    @see
    - @ref qore_reassign_signal() to allocate a single signal to a module
    - @ref qore_release_signal() to release a signal allocation manually
    - @ref qore_release_signals() to release multiple signal allocations manually

    @since %Qore 1.19
*/
DLLEXPORT QoreStringNode* qore_reassign_signals(const sig_vec_t& sig_vec, const char* name, bool reuse_sys);

//! releases the signal allocated to the given module
/** @param sig the signal number allocated to the module
    @param name module name owning the signal

    @return 0 = signal allocation released, -1 = no changes were made (module does not manage signal)

    @see
    - @ref qore_reassign_signal() to allocate a single signal to a module
    - @ref qore_reassign_signals() to allocate multiple signals to a module atomically
    - @ref qore_release_signals() to release multiple signal allocations manually

    @since %Qore 0.8.13.1
*/
DLLEXPORT int qore_release_signal(int sig, const char* name);

//! releases multiple signals allocated to the given module atomically
/** @param sig_vec a vector of signal numbers to allocate to the module
    @param name module name owning the signals

    @return 0 = signal allocations released, -1 = no changes were made (module does not manage at least one signal)

    @since %Qore 0.8.13.1
*/
DLLEXPORT int qore_release_signals(const sig_vec_t& sig_vec, const char* name);

//! macro to return the maximum of 2 numbers
#define QORE_MAX(a, b) ((a) > (b) ? (a) : (b))

//! macro to return the minimum of 2 numbers
#define QORE_MIN(a, b) ((a) < (b) ? (a) : (b))

//! macro for no argument
#define QORE_PARAM_NO_ARG QoreSimpleValue().assign(nullptr)

// define QORE_PATH_MAX
#ifndef QORE_PATH_MAX
#ifdef _XOPEN_PATH_MAX
#define QORE_PATH_MAX _XOPEN_PATH_MAX
#else
#define QORE_PATH_MAX 1024
#endif
#endif

//! to set the time zone from the command line
/** @note this function can only be called when a program exists
 */
DLLEXPORT void parse_set_time_zone(const char* zone);

//! use this function instead of usleep(), as usleep() is not signal-safe on some platforms (ex: Solaris 8, 9)
DLLEXPORT int qore_usleep(int64 usecs);

//! this function will cause garbage collection to be disabled
DLLEXPORT void qore_disable_gc();

//! returns true if garbage collection is enabled, false if not
DLLEXPORT bool qore_is_gc_enabled();

//! platform-independent API that tells if the given path is readable by the current user
DLLEXPORT bool q_path_is_readable(const char* path);

//! tries to parse a boolean value - standard conversion or uses q_parse_bool(const char*) if it's a string
DLLEXPORT bool q_parse_bool(QoreValue n);

//! parses a string and returns a boolean (ie case-insensitive "on","true","enable*","yes" are True, the rest is interpreted as a number where 0=false, everything else=true)
DLLEXPORT bool q_parse_bool(const char* str);

//! returns the boolean value of qore library the given option name; false if the option is unknown
DLLEXPORT bool q_get_option_value(const char* opt);

//! returns the boolean value of qore library the given name of the constant for the option; false if the option constant name is unknown
DLLEXPORT bool q_get_option_constant_value(const char* opt);

//! concatenates UNIX-style permissions to perm and from mode and returns a string giving the file type
DLLEXPORT const char* q_mode_to_perm(mode_t mode, QoreString& perm);

//! returns the current working directory in the given string; -1 is returned if an error occurred, 0 = OK
int q_getcwd(QoreString& cwd);

//! returns true if the given string is an absolute path on UNIX systems
DLLEXPORT bool q_absolute_path_unix(const char* path);

//! returns true if the given string is an absolute path on Windows systems
DLLEXPORT bool q_absolute_path_windows(const char* path);

//! returns true if the given string is an absolute path on the current platform
DLLEXPORT bool q_absolute_path(const char* path);

//! normalizes the given path for the current platform in place (makes absolute, removes "." and "..")
DLLEXPORT void q_normalize_path(QoreString& path, const char* cwd = 0);

//! normalizes the given path and resolves any symlinks
DLLEXPORT int q_realpath(const QoreString& path, QoreString& rv, ExceptionSink* xsink = 0);

//! finds a memory sequence in a larger memory sequence
DLLEXPORT void* q_memmem(const void* big, size_t big_len, const void* little, size_t little_len);

//! finds a memory sequence in a larger memory sequence searching from the end of the sequence
/** @note returns <tt>void*</tt> for compatibility with memmem() and q_memmem() signatures

    @since Qore 0.9.1
*/
DLLEXPORT void* q_memrmem(const void* big, size_t big_len, const void* little, size_t little_len);

//! performs environment variable substitution on the string argument
/** return 0 for OK, -1 if an error occurred (mismatched parens, etc)

    @since %Qore 0.8.13
 */
DLLEXPORT int q_env_subst(QoreString& str);

//! converts a string to a double in a locale-independent way
/** @since %Qore 0.8.13
 */
DLLEXPORT double q_strtod(const char* str);

//! returns true if the Qore library has been initialized
/** @since %Qore 0.9.5
 */
DLLEXPORT bool q_libqore_initalized();

//! returns true if the Qore library has been shut down
/** @since %Qore 0.8.13
 */
DLLEXPORT bool q_libqore_shutdown();

//! returns true if the Qore library is exiting without a shutdown
/** @since %Qore 0.9.5
 */
DLLEXPORT bool q_libqore_exiting();

//! retrieves a hash of all thread local variables and their values for the given stack frame in the current thread's QoreProgram object
/** @param frame the stack frame starting from 0 (the current frame)
    @param xsink for Qore-language exceptions

    @return a hash of local variables and their values; if the frame does not exist, an empty hash is returned; if a Qore-language exception is thrown, then nullptr is returned

    @note the current thread must be a valid Qore thread with a current QoreProgram context or the results are undefined (i.e. expect a crash)

    @since %Qore 0.8.13
*/
DLLEXPORT QoreHashNode* q_get_thread_local_vars(int frame, ExceptionSink* xsink);

//! sets the value of the given thread-local variable (which may be a closure-bound variable as well) in the current stack frame for the current thread's QoreProgram object
/** @param frame the stack frame where 0 is the current (highest) stack frame
    @param name the name of the variable
    @param val the value to assign; the value will be referenced for the assignment if one is made
    @param xsink for Qore-language exceptions

    @return 0 = OK or -1 = a Qore-language exception occurred making the assignment (ex: incompatible types) or 1 = variable not found or inaccessible stack frame (no exception thrown)

    @note pure local variables (i.e. not closure bound and not subject to the reference operator) are not stored with type information at runtime; type information is only enforced at parse / compile time, therefore it's possible to set local variables with invalid values that contradict their declarations with this function

    @since %Qore 0.8.13
 */
DLLEXPORT int q_set_thread_var_value(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink);

//! returns the pointer and size for string or binary data (return 0); no change for other data (return -1)
DLLEXPORT int q_get_data(const QoreValue& data, const char*& ptr, size_t& len);

//! returns a list<string> of parse option strings for the given bitfield; a Qore-language exception is raised for invalid values
/** @since %Qore 0.9
*/
DLLEXPORT QoreListNode* parse_option_bitfield_to_string_list(int64 i, ExceptionSink* xsink);

//! returns a list<string> of domain strings for the given bitfield; a Qore-language exception is raised for invalid values
/** @since %Qore 0.9
*/
DLLEXPORT QoreListNode* domain_bitfield_to_string_list(int64 i, ExceptionSink* xsink);

//! returns the "or nothing" type for the given type
/** @since %Qore 0.9
*/
DLLEXPORT const QoreTypeInfo* get_or_nothing_type_check(const QoreTypeInfo* typeInfo);

//! returns the pseudo-class for the given type
/** @since %Qore 0.9
*/
DLLEXPORT const QoreClass* qore_pseudo_get_class(qore_type_t t);

//! returns the pseudo-class for the given type
/** @since %Qore 0.9
*/
DLLEXPORT const QoreClass* qore_pseudo_get_class(const QoreTypeInfo* t);

//! returns the caller's Program context, if any
/** @since %Qore 0.9
*/
DLLEXPORT QoreProgram* qore_get_call_program_context();

//! sets a module option for the given module
/** @param mod the module name
    @param opt the option name
    @param val the option value; must be already referenced for the assignment

    @since %Qore 0.9
*/
DLLEXPORT void qore_set_module_option(std::string mod, std::string opt, QoreValue val);

//! get module option for the given module
/** @param mod the module name
    @param opt the option name

    @return the referenced option value; if a value is returned here, it must be dereferenced

    @since %Qore 0.9
*/
DLLEXPORT QoreValue qore_get_module_option(std::string mod, std::string opt);

// try to remove noise in insignificant digits from a number string
/** finds the decimal point and attempts to remove noise and round the number if found

    @param str the number string to round
    @param round_threshold_1 the number of consecutive trailing 0 or 9 digits that will be rounded in string output
    @param round_threshold_2 the number of consecutive trailing 0 or 9 digits that will be rounded in string output if there are trailing non-0/9 digits

    @since %Qore 0.9.4
*/
DLLEXPORT void qore_apply_rounding_heuristic(QoreString& str, int round_threshold_1, int round_threshold_2);

//! returns the default thread stack size
/** @since %Qore 0.9.5 a public API
*/
DLLEXPORT size_t q_thread_get_stack_size();

//! returns the thread stack size for the current thread
/** @since %Qore 1.0.8
*/
DLLEXPORT size_t q_thread_get_this_stack_size();

//! sets the default thread stack size or throws an exception on error
/** @since %Qore 0.9.5 a public API
*/
DLLEXPORT size_t q_thread_set_stack_size(size_t size, ExceptionSink* xsink);

//! Returns the number of bytes left in the current thread stack
/** @since %Qore 0.9.5
*/
DLLEXPORT size_t q_thread_stack_remaining();

//! Returns the number of bytes used in the current thread stack
/** @since %Qore 0.9.5
*/
DLLEXPORT size_t q_thread_stack_used();

//! Sets the thread stack limit on the primary thread
/** @since %Qore 1.19
*/
DLLEXPORT void q_enforce_thread_size_on_primary_thread();

//! Returns true if there is an active %Qore exception in the current thread
/** @since %Qore 2.0
*/
DLLEXPORT bool q_active_exception();

//! Add the given module to the module blacklist
/** @param name the name of the module
    @param msg the message that will be included in any exception when the module is attempted to be loaded

    @ereturn 0 = module added, -1 = module already loaded, cannot be blacklisted, -2 = module already in blacklist

    @since %Qore 2.0.1
*/
DLLEXPORT int q_add_module_to_blacklist(const char* name, const char* msg);

#endif // _QORE_QORELIB_H
