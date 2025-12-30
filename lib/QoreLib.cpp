/* -*- indent-tabs-mode: nil -*- */
/*
    QoreLib.cpp

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

#include "qore/Qore.h"
#include "qore/intern/git-revision.h"
#include "qore/intern/qore_number_private.h"
#include "qore/intern/QoreSignal.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/qore_qd_private.h"
#include "qore/intern/ql_crypto.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/StringReaderHelper.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/LocalVar.h"
#include "qore/intern/QoreTypeInfo.h"

#include <atomic>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstring>
#include <ctime>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef HAVE_PWD_H
#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#endif

FeatureList qoreFeatureList;

#define cpp_str(s) #s
#define cpp_xstr(s) cpp_str(s)

// global library variables
const char* qore_version_string      = VERSION;
int qore_version_major               = VERSION_MAJOR;
int qore_version_minor               = VERSION_MINOR;
int qore_version_sub                 = VERSION_SUB;
int qore_version_patch               = VERSION_PATCH;
int qore_target_bits                 = TARGET_BITS;

// obsolete: no more svn
int qore_build_number                = 0;

const char* qore_git_hash            = BUILD;
const char* qore_target_os           = TARGET_OS;
const char* qore_target_arch         = TARGET_ARCH;
const char* qore_module_dir          = MODULE_DIR;
const char* qore_user_module_dir     = USER_MODULE_DIR;
const char* qore_module_ver_dir      = MODULE_VER_DIR;
const char* qore_user_module_ver_dir = USER_MODULE_VER_DIR;
const char* qore_cplusplus_compiler  = QORE_LIB_CXX;
const char* qore_cflags              = QORE_LIB_CFLAGS;
const char* qore_ldflags             = QORE_LIB_LDFLAGS;
const char* qore_build_host          = QORE_BUILD_HOST;

int qore_min_mod_api_major = QORE_MODULE_COMPAT_API_MAJOR;
int qore_min_mod_api_minor = QORE_MODULE_COMPAT_API_MINOR;

DLLLOCAL QoreListNode* ARGV = nullptr;
DLLLOCAL QoreListNode* QORE_ARGV = nullptr;

// default location for Qore builtin C++ code
const QoreProgramLocation loc_builtin("<builtin>", -1, -1);

QoreString random_salt;

DLLLOCAL bool q_disable_gc = false;

// issue #3045: module options
DLLLOCAL QoreThreadLock mod_opt_lock;
typedef std::map<std::string, QoreValue> mod_opt_val_map_t;
typedef std::map<std::string, mod_opt_val_map_t> mod_opt_map_t;
DLLLOCAL mod_opt_map_t mod_opt_map;

#ifndef HAVE_LOCALTIME_R
DLLLOCAL QoreThreadLock lck_localtime;
#endif

#ifndef HAVE_GMTIME_R
DLLLOCAL QoreThreadLock lck_gmtime;
#endif

#if defined(HAVE_GETPWUID_R) || defined(HAVE_GETPWNAM_R)
static long pwsize = 0;
#else
// for the getpwuid() and getpwnam() functions
static QoreThreadLock lck_passwd;
#endif

#if defined(HAVE_GETGRGID_R) || defined(HAVE_GETGRNAM_R)
static long grsize = 0;
#else
// for the getgrgid() and getgrnam() functions
static QoreThreadLock lck_group;
#endif

// time zone information source
QoreTimeZoneManager QTZM;

// parse location for objects parsed on the command-line
QoreCommandLineLocation qoreCommandLineLocation;

// for base64 encoding
char table64[64] = {
   'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
   'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
   'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
   'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
   'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
   'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
   'w', 'x', 'y', 'z', '0', '1', '2', '3',
   '4', '5', '6', '7', '8', '9', '+', '/',
};

template<>
DLLLOCAL vector_set_t<const char*>::iterator vector_set_t<const char*>::find(const char* const& v) {
    return std::find_if(vector.begin(), vector.end(), string_compare(v));
}

template<>
DLLLOCAL vector_set_t<const char*>::const_iterator vector_set_t<const char*>::find(const char* const& v) const {
    return std::find_if(vector.begin(), vector.end(), string_compare(v));
}

// same as find, just for map compatibility
template<>
DLLLOCAL vector_set_t<const char*>::iterator vector_set_t<const char*>::lower_bound(const char* const& v) {
    return std::find_if(vector.begin(), vector.end(), string_compare(v));
}

// same as find, just for map compatibility
template<>
DLLLOCAL vector_set_t<const char*>::const_iterator vector_set_t<const char*>::lower_bound(const char* const& v) const {
    return std::find_if(vector.begin(), vector.end(), string_compare(v));
}

template<>
DLLLOCAL vector_set_t<char*>::iterator vector_set_t<char*>::find(char* const& v) {
    return std::find_if(vector.begin(), vector.end(), string_compare(v));
}

template<>
DLLLOCAL vector_set_t<char*>::const_iterator vector_set_t<char*>::find(char* const& v) const {
    return std::find_if(vector.begin(), vector.end(), string_compare(v));
}

// same as find, just for map compatibility
template<>
DLLLOCAL vector_set_t<char*>::iterator vector_set_t<char*>::lower_bound(char* const& v) {
    return std::find_if(vector.begin(), vector.end(), string_compare(v));
}

// same as find, just for map compatibility
template<>
DLLLOCAL vector_set_t<char*>::const_iterator vector_set_t<char*>::lower_bound(char* const& v) const {
    return std::find_if(vector.begin(), vector.end(), string_compare(v));
}

const qore_option_s qore_option_list_l[] = {
   { QORE_OPT_ATOMIC_OPERATIONS,
     "HAVE_ATOMIC_OPERATIONS",
     QO_OPTION,
     std::atomic_int{}.is_lock_free(),
   },
   { QORE_OPT_STACK_GUARD,
     "HAVE_STACK_GUARD",
     QO_OPTION,
#ifdef QORE_MANAGE_STACK
     true
#else
     false
#endif
   },
   { QORE_OPT_SIGNAL_HANDLING,
     "HAVE_SIGNAL_HANDLING",
     QO_OPTION,
#ifdef HAVE_SIGNAL_HANDLING
     true
#else
     false
#endif
   },
   { QORE_OPT_LIBRARY_DEBUGGING,
     "HAVE_LIBRARY_DEBUGGING",
     QO_OPTION,
#ifdef DEBUG
     true
#else
     false
#endif
   },
   { QORE_OPT_RUNTIME_STACK_TRACE,
     "HAVE_RUNTIME_THREAD_STACK_TRACE",
     QO_OPTION,
     true
   },
   { QORE_OPT_TERMIOS,
     "HAVE_TERMIOS",
     QO_OPTION,
#ifdef HAVE_TERMIOS_H
     true
#else
     false
#endif
   },
   { QORE_OPT_UNIX_USERMGT,
     "HAVE_UNIX_USERMGT",
     QO_OPTION,
#ifdef HAVE_GETUID
     true
#else
     false
#endif
   },
   { QORE_OPT_UNIX_FILEMGT,
     "HAVE_UNIX_FILEMGT",
     QO_OPTION,
#ifdef HAVE_CHOWN
     true
#else
     false
#endif
   },
   { QORE_OPT_FILE_LOCKING,
     "HAVE_FILE_LOCKING",
     QO_OPTION,
#ifdef HAVE_STRUCT_FLOCK
     true
#else
     false
#endif
   },
   { QORE_OPT_DETERMINISTIC_GC,
     "HAVE_DETEMINISTIC_GC",
     QO_OPTION,
     true
   },
   { QORE_OPT_SHA,
     "HAVE_SHA",
     QO_ALGORITHM,
#ifdef HAVE_OPENSSL_SHA
     true
#else
     false
#endif
   },
   { QORE_OPT_SHA224,
     "HAVE_SSH224",
     QO_ALGORITHM,
#if !defined(OPENSSL_NO_SHA256) && defined(HAVE_OPENSSL_SHA512)
     true
#else
     false
#endif
   },
   { QORE_OPT_SHA256,
     "HAVE_SSH256",
     QO_ALGORITHM,
#if !defined(OPENSSL_NO_SHA256) && defined(HAVE_OPENSSL_SHA512)
     true
#else
     false
#endif
   },
   { QORE_OPT_SHA384,
     "HAVE_SSH384",
     QO_ALGORITHM,
#if !defined(OPENSSL_NO_SHA512) && defined(HAVE_OPENSSL_SHA512)
     true
#else
     false
#endif
   },
   { QORE_OPT_SHA512,
     "HAVE_SSH512",
     QO_ALGORITHM,
#if !defined(OPENSSL_NO_SHA512) && defined(HAVE_OPENSSL_SHA512)
     true
#else
     false
#endif
   },
   { QORE_OPT_MDC2,
     "HAVE_MDC2",
     QO_ALGORITHM,
#ifdef OPENSSL_HAVE_MDC2
     true
#else
     false
#endif
   },
   { QORE_OPT_RC5,
     "HAVE_RC5",
     QO_ALGORITHM,
#ifndef OPENSSL_NO_RC5
     true
#else
     false
#endif
   },
   { QORE_OPT_MD2,
     "HAVE_MD2",
     QO_ALGORITHM,
#ifndef OPENSSL_NO_MD2
     true
#else
     false
#endif
   },
   { QORE_OPT_DSS,
    "HAVE_DSS",
    QO_ALGORITHM,
#ifndef HAVE_OPENSSL_INIT_CRYPTO
    true
#else
    false
#endif
  },
  { QORE_OPT_FUNC_ROUND,
     "HAVE_ROUND",
     QO_FUNCTION,
#ifdef HAVE_ROUND
     true
#else
     false
#endif
   },
   // HAVE_TIMEGM is always true now, we don't use the system function anymore anyway
   { QORE_OPT_FUNC_TIMEGM,
     "HAVE_TIMEGM",
     QO_FUNCTION,
     true
   },
   { QORE_OPT_FUNC_SETEUID,
     "HAVE_SETEUID",
     QO_FUNCTION,
#ifdef HAVE_SETEUID
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_SETEGID,
     "HAVE_SETEGID",
     QO_FUNCTION,
#ifdef HAVE_SETEGID
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_SYSTEM,
     "HAVE_SYSTEM",
     QO_FUNCTION,
#ifdef HAVE_SYSTEM
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_KILL,
     "HAVE_KILL",
     QO_FUNCTION,
#ifdef HAVE_KILL
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_FORK,
     "HAVE_FORK",
     QO_FUNCTION,
#ifdef HAVE_FORK
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_GETPPID,
     "HAVE_GETPPID",
     QO_FUNCTION,
#ifdef HAVE_GETPPID
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_STATVFS,
     "HAVE_STATVFS",
     QO_FUNCTION,
#ifdef Q_HAVE_STATVFS
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_SETSID,
     "HAVE_SETSID",
     QO_FUNCTION,
#ifdef HAVE_SETSID
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_IS_EXECUTABLE,
     "HAVE_IS_EXECUTABLE",
     QO_FUNCTION,
#ifdef HAVE_PWD_H
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_CLOSE_ALL_FD,
     "HAVE_CLOSE_ALL_FD",
     QO_FUNCTION,
#if defined(HAVE_UNISTD_H) && defined(_SC_OPEN_MAX)
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_GET_NETIF_LIST,
     "HAVE_GET_NETIF_LIST",
     QO_FUNCTION,
#ifdef HAVE_GETIFADDRS
     true
#else
     false
#endif
   },
   { QORE_OPT_FUNC_BROTLI,
     "HAVE_BROTLI",
     QO_FUNCTION,
#ifdef HAVE_BROTLI
     true
#else
     false
#endif
   },
};

const qore_option_s* qore_option_list = qore_option_list_l;

#define QORE_OPTION_LIST_SIZE (sizeof(qore_option_list_l) / sizeof(qore_option_s))

size_t qore_option_list_size = QORE_OPTION_LIST_SIZE;

int q_add_module_to_blacklist(const char* name, const char* msg) {
    return QMM.addModuleToBlacklist(name, msg);
}

bool q_get_option_value(const char* opt) {
    for (unsigned i = 0; i < QORE_OPTION_LIST_SIZE; ++i) {
        if (!strcasecmp(opt, qore_option_list_l[i].option))
            return qore_option_list_l[i].value;
    }
    return false;
}

bool q_get_option_constant_value(const char* opt) {
    for (unsigned i = 0; i < QORE_OPTION_LIST_SIZE; ++i) {
        if (!strcasecmp(opt, qore_option_list_l[i].constant))
            return qore_option_list_l[i].value;
    }
    return false;
}

static int get_number(char** param) {
    int num = 0;
    while (isdigit(**param)) {
        num = num*10 + (**param - '0');
        ++(*param);
    }
    //printd(5, "get_number(%x: %s) num: %d\n", *param, *param, num);
    return num;
}

// print options
#define P_JUSTIFY_LEFT      1
#define P_INCLUDE_PLUS      2
#define P_SPACE_FILL        4
#define P_ZERO_FILL         8

bool qore_has_debug() {
#ifdef DEBUG
   return true;
#else
   return false;
#endif
}

int parse_init_value(QoreValue& val, QoreParseContext& parse_context) {
    if (val.hasNode()) {
        AbstractQoreNode* n = val.getInternalNode();
        //printd(5, "parse_init_value() n: %p '%s'\n", n, get_type_name(n));
        return n->parseInit(val, parse_context);
    }

    parse_context.typeInfo = val.getFullTypeInfo();
    return 0;
}

QoreParseContext::QoreParseContext(LocalVar* oflag, QoreProgram* pgm) : pgm(pgm), oflag(oflag),
        class_ctx(oflag ? QoreTypeInfo::getUniqueReturnClass(oflag->getTypeInfo()) : nullptr) {
}

QoreAbstractIteratorBase::QoreAbstractIteratorBase() : tid(q_gettid()) {
}

QoreAbstractIteratorBase::~QoreAbstractIteratorBase() {
}

int QoreAbstractIteratorBase::check(ExceptionSink* xsink) const {
   if (tid != q_gettid()) {
      xsink->raiseException("ITERATOR-THREAD-ERROR", "this %s object was created in TID %d; it is an error to access it from any other thread (accessed from TID %d)", getName(), tid, q_gettid());
      return -1;
   }
   return 0;
}

QoreIteratorBase::QoreIteratorBase() {
}

QoreIteratorBase::~QoreIteratorBase() {
}

FeatureList::FeatureList() {
   // register default features
   push_back("sql");
   push_back("threads");
   push_back("DGC");
#ifdef DEBUG
   push_back("debug");
#endif
}

FeatureList::~FeatureList() {
}

// process a string argument from a *printf*-style string format specification
static void process_opt_string(QoreString& tbuf, QoreValue qv, int type, int opts, bool char_width, int width, ExceptionSink* xsink) {
    QoreStringValueHelper astr(qv);
    // use the character width and not the byte length for formatting fields if applicable
    int length = char_width ? astr->getCharWidth(xsink) : astr->size();
    //printd(5, "process_opt_string() astr: '%s' len: %d width: %d type: %d\n", astr->c_str(), length, width, type);
    if ((width != -1) && (length > width) && !type) {
        width = length;
    }
    if ((width != -1) && (length > width)) {
        if (char_width && type) {
            // count of positions
            int c = 0;
            UnicodeCharacterIterator i(**astr);
            while (i.next(xsink) && c < width) {
                int ucs = i.getValue();
                int w = qore_get_unicode_character_width(ucs);
                if ((c + w) > width) {
                    // pad with dots
                    while (c < width) {
                        tbuf.concat('.');
                        ++c;
                    }
                    break;
                }

                c += w;
                tbuf.concatUnicode(ucs, xsink);
                if (*xsink) {
                    break;
                }
            }
        } else {
            tbuf.concat(*astr, (size_t)width, xsink); // string encodings are converted here if necessary
        }
    } else {
        if ((width != -1) && (opts & P_JUSTIFY_LEFT)) {
            tbuf.concat(*astr, xsink);
            while (width > length) {
                tbuf.concat(' ');
                --width;
            }
        } else {
            while (width > length) {
                tbuf.concat(' ');
                --width;
            }
            tbuf.concat(*astr, xsink);
        }
    }
}

// if type = 0 then field widths are soft limits, otherwise they are hard
static int process_opt(QoreString *cstr, char* param, QoreValue qv, int type, bool& arg_used, ExceptionSink* xsink) {
    assert(arg_used == true);
    char* str = param;
    int opts = 0;
    int width = -1;
    int decimals = -1;
    char fmt[20], *f;
    QoreString tbuf(cstr->getEncoding());

    printd(5, "process_opt(): param: %s type: %d qv->getType(): %s refs: %d\n",
            param, type, qv.getTypeName(), qv.hasNode() ? qv.getInternalNode()->reference_count() : -1);
    qore_type_t t = qv.getType();
#ifdef DEBUG
    if (t == NT_STRING) {
        const QoreStringNode* nstr = qv.get<const QoreStringNode>();
        printd(5, "process_opt() %p (%d) \"%s\"\n", nstr->getBuffer(), nstr->strlen(), nstr->getBuffer());
    }
#endif

    // if it's just '%%' then output a single '%' and do not process arguments
    if (param[1] == '%') {
        cstr->concat('%');
        arg_used = false;
        return 1;
    }

    loop:
    switch (*(++param)) {
        case '-': opts |= P_JUSTIFY_LEFT; goto loop;
        case '+': opts |= P_INCLUDE_PLUS; goto loop;
        case ' ': opts |= P_SPACE_FILL; opts &= ~P_ZERO_FILL; goto loop;
        case '0': opts |= P_ZERO_FILL; opts &= ~P_SPACE_FILL; goto loop;
    }
    if (isdigit(*param)) {
        width = get_number(&param);
    }
    if ((*param) == '.') {
        param++;
        decimals = get_number(&param);
    }
    if (decimals < 0) {
        decimals = -1;
    }

    char p = *param;
    switch (*param) {
        case 's': {
            process_opt_string(tbuf, qv, type, opts, false, width, xsink);
            break;
        }
        case 'w': {
            process_opt_string(tbuf, qv, type, opts, true, width, xsink);
            break;
        }
        case 'p':
            p = 'x';
        case 'd':
        case 'o':
        case 'x':
        case 'X': {
            // recreate the sprintf format argument
            f = fmt;
            *(f++) = '%';
            if (opts & P_JUSTIFY_LEFT) {
                *(f++) = '-';
            }
            if (opts & P_INCLUDE_PLUS) {
                *(f++) = '+';
            }
            if (width != -1) {
                if (opts & P_SPACE_FILL) {
                    *(f++) = ' ';
                } else if (opts & P_ZERO_FILL) {
                    *(f++) = '0';
                }
                f += sprintf(f, "%d", width);
            }
#ifdef _Q_WINDOWS
            *(f++) = 'I';
            *(f++) = '6';
            *(f++) = '4';
#else
            *(f++) = 'l';
            *(f++) = 'l';
#endif
            *(f++) = p; // 'd', etc;
            *f = '\0';
            int64 val = qv.getAsBigInt();
            tbuf.sprintf(fmt, val);
            if (type && (width != -1)) {
                tbuf.terminate(width);
            }
            break;
        }
        case 'A':
        case 'a':
        case 'G':
        case 'g':
        case 'F':
        case 'f':
        case 'E':
        case 'e': {
            // recreate the sprintf format argument
            f = fmt;
            *(f++) = '%';
            if (opts & P_JUSTIFY_LEFT) {
                *(f++) = '-';
            }
            if (opts & P_INCLUDE_PLUS) {
                *(f++) = '+';
            }
            if (width != -1) {
                if (opts & P_SPACE_FILL) {
                    *(f++) = ' ';
                } else if (opts & P_ZERO_FILL) {
                    *(f++) = '0';
                }
                f += sprintf(f, "%d", width);
            }
            if (decimals != -1) {
                *(f++) = '.';
                f += sprintf(f, "%d", decimals);
            }
            if (t == NT_NUMBER) {
                *(f++) = QORE_MPFR_SPRINTF_ARG;
                *(f++) = *param; // a|A|e|E|f|F|g|G
                *f = '\0';
                qore_number_private::sprintf(*qv.get<const QoreNumberNode>(), tbuf, fmt);
            } else {
                *(f++) = *param; // a|A|e|E|f|F|g|G
                *f = '\0';
                double val = qv.getAsFloat();
                size_t offset = tbuf.size();
                tbuf.sprintf(fmt, val);
                // issue 1556: external modules that call setlocale() can change
                // the decimal point character used here from '.' to ','
                q_fix_decimal(&tbuf, offset);
                //printd(5, "fmt: '%s' val: %f tbuf: '%s'\n", fmt, val, tbuf.c_str());
            }
            if (type && (width != -1)) {
                tbuf.terminate(width);
            }
            break;
        }
        case 'n':
        case 'N': {
            QoreNodeAsStringHelper t(qv, *param == 'N'
                                    ? (width == -1 ? FMT_NORMAL : width)
                                    : FMT_NONE, xsink);
            tbuf.concat(*t, xsink);
            break;
        }
        case 'y': {
            QoreNodeAsStringHelper t(qv, FMT_YAML_SHORT, xsink);
            tbuf.concat(*t, xsink);
            break;
        }
        case 'Y': {
            QoreNodeAsStringHelper t(qv, FMT_YAML_LONG, xsink);
            tbuf.concat(*t, xsink);
            break;
        }
        default:
            // if the format argument is not understood, then make sure and just consume the '%' char
            tbuf.concat('%');
            param = str;
            arg_used = false;
    }

    cstr->concat(&tbuf, xsink);
    return (int)(param - str);
}

static QoreStringNode* qore_sprintf_intern(ExceptionSink* xsink, const QoreStringNode* fmt,
    const QoreListNode* arg_list, size_t arg_offset, int field, int last_arg = -1,
    bool ignore_broken_sprintf = false) {
    SimpleRefHolder<QoreStringNode> buf(new QoreStringNode(fmt->getEncoding()));

    bool broken_sprintf = ignore_broken_sprintf
        ? false
        : getProgram()->getParseOptions64() & PO_BROKEN_SPRINTF;

    const char* pstr = fmt->c_str();
    size_t l = fmt->strlen();
    size_t arg_size = last_arg == -1
        ? (arg_list ? arg_list->size() : 0)
        : QORE_MIN(static_cast<size_t>(last_arg), arg_list ? arg_list->size() : 0);

    //printd(5, "qore_sprintf_intern() bs: %d arg_offset: %zd arg_size: %zd last_arg: %d arg_list: %p (%zd) fmt: '%s'\n", broken_sprintf, arg_offset, arg_size, last_arg, arg_list, arg_list ? arg_list->size() : 0, fmt->c_str());
    for (size_t i = 0; i < l; ++i) {
        if (pstr[i] == '%' && (!broken_sprintf || arg_offset < arg_size)) {
            bool arg_used = true;
            bool use_arg;
            QoreValue param_value;
            if (arg_offset < arg_size) {
                param_value = get_param_value(arg_list, arg_offset++);
                use_arg = true;
            } else {
                use_arg = false;
            }
            i += process_opt(*buf, (char*)&pstr[i], param_value, field, arg_used, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (use_arg && !arg_used) {
                --arg_offset;
            }
        } else {
            buf->concat(pstr[i]);
            if (pstr[i] == '%' && pstr[i+1] == '%') {
                ++i;
            }
        }
    }

    return buf.release();
}

QoreStringNode* q_sprintf(const QoreListNode* params, int field, int offset, ExceptionSink* xsink) {
    assert(xsink);

    QoreValue pv = get_param_value(params, offset);
    if (pv.getType() != NT_STRING) {
        return new QoreStringNode;
    }

    return qore_sprintf_intern(xsink, pv.get<const QoreStringNode>(), params, offset + 1, field, -1, false);
}

QoreStringNode* q_vsprintf(const QoreListNode* params, int field, int offset, ExceptionSink* xsink) {
    assert(xsink);
    QoreValue pv = get_param_value(params, offset);
    if (pv.getType() != NT_STRING) {
        return new QoreStringNode;
    }

    const QoreStringNode* fmt = pv.get<QoreStringNode>();

    pv = get_param_value(params, offset + 1);
    int arg_offset;
    int last_arg;
    const QoreListNode* arg_list;
    // issue #3184: vsprintf(fmt, ()) resulted in non-broken behavior as a corner case
    bool ignore_broken_sprintf = false;
    if (pv.getType() == NT_LIST) {
        arg_list = pv.get<const QoreListNode>();
        arg_offset = 0;
        last_arg = -1;
        ignore_broken_sprintf = arg_list->empty();
    } else {
        // only process the single argument from the top-level arg list
        if (pv.isNothing()) {
            arg_list = nullptr;
            arg_offset = 0;
            last_arg = 0;
        } else {
            arg_list = params;
            arg_offset = offset + 1;
            last_arg = offset + 2;
        }
    }

    return qore_sprintf_intern(xsink, fmt, arg_list, arg_offset, field, last_arg, ignore_broken_sprintf);
}

static QoreValue do_method_intern(QoreObject* self, const QoreMethod* meth, ClassAccess access, const char* name, const qore_class_private* pcls, ExceptionSink* xsink) {
    if (!meth->isStatic() && !self) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "non-static method \"%s\" is not accessible without an object "
            "context", name);
        return QoreValue();
    }
    if (access > Public) {
        // ensure the method can be accessed
        if (self) {
            const qore_class_private* selfcls = qore_class_private::get(*self->getClass());
            if (selfcls != pcls) {
                if (access > Private || !selfcls->runtimeCheckPrivateClassAccess(pcls)) {
                    meth = nullptr;
                }
            }
        } else {
            meth = nullptr;
        }
        if (!meth) {
            xsink->raiseException("CALL-REFERENCE-ERROR", "%s method \"%s\" is not accessible in this context",
                privpub(access), name);
            return QoreValue();
        }
    }
    // see if non-static method is part of self's class hierarchy
    if (!meth->isStatic()) {
        bool priv;
        if (!self->getClass()->getClass(*meth->getClass(), priv)) {
            xsink->raiseException("CALL-REFERENCE-ERROR", "\"%s\" is not accessible from an object of " \
                "class '%s' as the classes are not in the same hierarchy", name, self->getClassName());
        }
    }
    if (self) {
        return QoreValue(new RunTimeResolvedMethodReferenceNode(&loc_builtin, self, meth, qore_class_private::get(*meth->getClass())));
    }
    return QoreValue(new LocalStaticMethodCallReferenceNode(&loc_builtin, meth));
}

QoreValue get_call_reference_intern(QoreObject* self, const QoreStringNode* identifier, ExceptionSink* xsink) {
    TempEncodingHelper name(identifier, QCS_DEFAULT, xsink);
    if (*xsink) {
        return QoreValue();
    }
    bool scoped_ref = strstr(name->c_str(), "::");
    const qore_class_private* class_ctx = runtime_get_class();
    if (scoped_ref) {
        NamedScope ns(name->c_str());
        const QoreClass* cls = qore_root_ns_private::get(*(getRootNS()))->runtimeFindScopedClassWithMethod(ns);
        if (cls) {
            const qore_class_private* pcls = qore_class_private::get(*cls);
            ClassAccess access;
            const QoreMethod* meth = pcls->runtimeFindCommittedMethodForEval(ns.getIdentifier(), access, class_ctx);
            if (!meth) {
                meth = pcls->runtimeFindCommittedStaticMethod(ns.getIdentifier(), access, class_ctx);
            }
            //printd(5, "get_call_reference_intern() '%s' -> cls: '%s' ctx: '%s' meth: %p\n", name->c_str(), cls->getName(), class_ctx ? class_ctx->name.c_str() : "n/a", meth);
            if (meth) {
                return do_method_intern(self, meth, access, name->c_str(), pcls, xsink);
            }
        }
    } else if (self) {
        const qore_class_private* pcls = qore_class_private::get(*self->getClass());
        ClassAccess access;
        const QoreMethod* meth = pcls->runtimeFindCommittedMethodForEval(name->c_str(), access, class_ctx);
        if (!meth) {
            meth = pcls->runtimeFindCommittedStaticMethod(name->c_str(), access, class_ctx);
        }
        if (meth) {
            return do_method_intern(self, meth, access, name->c_str(), pcls, xsink);
        }
    }

    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(*getRootNS(), name->c_str());
    if (fe) {
        return new FunctionCallReferenceNode(&loc_builtin, fe->getFunction(), ::getProgram());
    }

    xsink->raiseException("CALL-REFERENCE-ERROR", "cannot resolve \"%s\" to any callable reference", name->c_str());
    return QoreValue();
}

static void concatASCII(QoreString &str, unsigned char c) {
    str.sprintf("ascii %03d", c);
    if (c >= 32 && c < 127) {
        str.sprintf(" ('%c')", c);
    }
}

static char get_base64_value(ExceptionSink* xsink, const char* buf, size_t &offset, bool end_ok, bool url = false) {
    while (buf[offset] == '\n' || buf[offset] == '\r') {
        ++offset;
    }

    char c = buf[offset];

    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (url) {
        if (c == '-') {
            return 62;
        }
        if (c == '_') {
            return 63;
        }
    } else {
        if (c == '+') {
            return 62;
        }
        if (c == '/') {
            return 63;
        }
    }

    if (!c) {
        if (!end_ok) {
            xsink->raiseException("BASE64-PARSE-ERROR", "premature end of base64 string at string byte offset %lu",
                offset);
        }
    } else {
        QoreStringNode* desc = new QoreStringNode;
        concatASCII(*desc, c);
        if (url) {
            desc->concat(" is an invalid base64-url character according to RFC-4648");
        } else {
            desc->concat(" is an invalid base64 character");
        }
        xsink->raiseException("BASE64-PARSE-ERROR", desc);
    }
    return -1;
}

static BinaryNode* parse_base64_intern(ExceptionSink* xsink, const char* buf, int len, bool url) {
    if (!len) {
        return new BinaryNode;
    }

    char* binbuf = (char*)malloc(sizeof(char) * (len + 3));
    int blen = 0;

    size_t pos = 0;
    while (pos < (size_t)len) {
        // add first 6 bits
        char b = get_base64_value(xsink, buf, pos, true, url);
        if (xsink->isEvent()) {
            free(binbuf);
            return nullptr;
        }
        // if we've reached the end of the string here, then exit the loop
        if (!buf[pos])
            break;

        // get second 6 bits
        ++pos;
        char c = get_base64_value(xsink, buf, pos, false, url);
        if (xsink->isEvent()) {
            free(binbuf);
            return nullptr;
        }
        // do first byte (1st char=upper 6 bits, 2nd char=lower 2)
        binbuf[blen++] = (b << 2) | (c >> 4);

        // check special cases
        ++pos;
        if (buf[pos] == '=' || (url && !buf[pos])) {
            break;
        }

        // low 4 bits from 2nd char become high 4 bits of next value
        b = (c & 15) << 4;

        // get third 6 bits
        c = get_base64_value(xsink, buf, pos, false, url);
        if (xsink->isEvent()) {
            free(binbuf);
            return nullptr;
        }
        // do second byte (2nd char=upper 4 bits, 3rd char=lower 4 bits)
        binbuf[blen++] = b | (c >> 2);

        // check special cases
        ++pos;
        if (buf[pos] == '=' || (url && !buf[pos])) {
            break;
        }

        // low 2 bits from 3rd char become high 2 bits of next value
        b = (c & 3) << 6;

        // get fourth 6 bits
        c = get_base64_value(xsink, buf, pos, false, url);
        if (xsink->isEvent()) {
            free(binbuf);
            return nullptr;
        }

        binbuf[blen++] = b | c;
        ++pos;
    }
    return new BinaryNode(binbuf, blen);
}

// see: RFC-4648: https://datatracker.ietf.org/doc/html/rfc4648#page-7
BinaryNode* parseBase64Url(const char* buf, int len, ExceptionSink* xsink) {
    return parse_base64_intern(xsink, buf, len, true);
}

// see: RFC-1421: http://www.ietf.org/rfc/rfc1421.txt and RFC-2045: http://www.ietf.org/rfc/rfc2045.txt
BinaryNode* parseBase64(const char* buf, int len, ExceptionSink* xsink) {
    return parse_base64_intern(xsink, buf, len, false);
}

int get_nibble(char c, ExceptionSink* xsink) {
    if (isdigit(c)) {
        return c - 48;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 55;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 87;
    }

    xsink->raiseException("PARSE-HEX-ERROR", "invalid hex digit found '%c'", c);
    return -1;
}

BinaryNode* parseHex(const char* buf, int len, ExceptionSink* xsink) {
    if (!len) {
        return new BinaryNode();
    }

    if ((len / 2) * 2 != len) {
        xsink->raiseException("PARSE-HEX-ERROR", "cannot parse an odd number of hex digits (%d digit%s)", len, len == 1 ? "" : "s");
        return nullptr;
    }

    char* binbuf = (char* )malloc(sizeof(char) * (len / 2));
    int blen = 0;

    const char* end = buf + len;
    while (buf < end) {
        int b = get_nibble(*buf, xsink);
        if (b < 0) {
            free(binbuf);
            return nullptr;
        }
        buf++;
        int l = get_nibble(*buf, xsink);
        if (l < 0) {
            free(binbuf);
            return nullptr;
        }
        buf++;
        binbuf[blen++] = b << 4 | l;
    }
    return new BinaryNode(binbuf, blen);
}

static int parse_get_nibble(const QoreProgramLocation* loc, char c) {
    if (isdigit(c)) {
        return c - 48;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 55;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 87;
    }

    parseException(*loc, "PARSE-HEX-ERROR", "invalid hex digit found '%c'", c);
    return -1;
}

// for use while parsing - parses a null-terminated string and raises parse exceptions for errors
BinaryNode* parseHex(const QoreProgramLocation* loc, const char* buf, int len) {
   if (!buf || !(*buf))
      return new BinaryNode();

   char* binbuf = (char* )malloc(sizeof(char) * (len / 2));
   int blen = 0;

   const char* end = buf + len;
   while (buf < end) {
      int b = parse_get_nibble(loc, *buf);
      if (b < 0) {
         free(binbuf);
         return 0;
      }
      buf++;
#if 0
      // this can never happen; the parser guarantees an even number of digits
      if (!(*buf)) {
         free(binbuf);
         parseError("PARSE-HEX-ERROR", "cannot parse an odd number of hex digits (%d digit%s)", len, len == 1 ? "" : "s");
         return 0;
      }
#endif

      int l = parse_get_nibble(loc, *buf);
      if (l < 0) {
         free(binbuf);
         return 0;
      }
      buf++;
      binbuf[blen++] = b << 4 | l;
   }
   return new BinaryNode(binbuf, blen);
}

char* make_class_name(const char* str) {
   char* cn = q_basename(str);
   char* p = strrchr(cn, '.');
   if (p && p != cn)
      *p = '\0';
   p = cn;
   while (*p) {
      if (*p == '-')
         *p = '_';
      p++;
   }
   return cn;
}

void print_node(FILE* fp, const QoreValue qv) {
   printd(5, "print_node() node: %s\n", qv.getTypeName());
   QoreStringValueHelper str(qv);
   fputs(str->getBuffer(), fp);
}

void qore_setup_argv(int pos, int argc, char* argv[]) {
    ARGV = new QoreListNode(stringTypeInfo);
    QORE_ARGV = new QoreListNode(stringTypeInfo);
    int end = argc - pos;
    for (int i = 0; i < argc; i++) {
        if (i < end)
            ARGV->push(new QoreStringNode(argv[i + pos]), nullptr);
        QORE_ARGV->push(new QoreStringNode(argv[i]), nullptr);
    }
}

void init_lib_intern(char* env[]) {
   // set up environment hash
   int i = 0;
   ENV = new QoreHashNode(autoTypeInfo);
   while (env[i]) {
      char* p;

      if ((p = strchr(env[i], '='))) {
         char save = *p;
         *p = '\0';
         ENV->setKeyValue(env[i], new QoreStringNode(p + 1), 0);
         //printd(5, "creating $ENV{\"%s\"} = \"%s\"\n", env[i], p + 1);
         *p = save;
      }
      i++;
   }

   // initialize process-default local time zone
   QTZM.init();

   // other misc initialization
#if defined(HAVE_GETPWUID_R) || defined(HAVE_GETPWNAM_R)
   pwsize = sysconf(_SC_GETPW_R_SIZE_MAX);
   if (pwsize == -1)
      pwsize = 4096; // more than enough?
#endif
#if defined(HAVE_GETGRGID_R) || defined(HAVE_GETGRNAM_R)
   grsize = sysconf(_SC_GETGR_R_SIZE_MAX);
   if (grsize == -1)
      grsize = 4096; // more than enough?
#endif
}

void delete_global_variables() {
    QORE_TRACE("delete_global_variables()");
    if (QORE_ARGV)
        QORE_ARGV->deref(0);
    if (ARGV)
        ARGV->deref(0);
    if (ENV)
        ENV->deref(0);
}

struct tm *q_localtime(const time_t* clock, struct tm* tms) {
#ifdef HAVE_LOCALTIME_R
   localtime_r(clock, tms);
#else
   lck_localtime.lock();
   struct tm *t = localtime(clock);
   memcpy(tms, t, sizeof(struct tm));
   lck_localtime.unlock();
#endif
   return tms;
}

struct tm *q_gmtime(const time_t* clock, struct tm* tms) {
#ifdef HAVE_GMTIME_R
   gmtime_r(clock, tms);
#else
   lck_gmtime.lock();
   struct tm *t = gmtime(clock);
   memcpy(tms, t, sizeof(struct tm));
   lck_gmtime.unlock();
#endif
   return tms;
}

const char* q_find_first_path_sep(const char* path) {
#ifdef _Q_WINDOWS
   // on windows we have to find either '\\' (standard directory separator character) or '/' (UNIX-style, also accepted on Windows)
   const char* p = path;
   while (*p) {
      if (*p == '/' || *p == '\\')
         return p;
      ++p;
   }
   return 0;
#else
   return strchr(path, QORE_DIR_SEP);
#endif
}

const char* q_find_last_path_sep(const char* path) {
#ifdef _Q_WINDOWS
   // on windows we have to find either '\\' (standard directory separator character) or '/' (UNIX-style, also accepted on Windows)
   const char* p = path;
   const char* rv = 0;
   while (*p) {
      if (*p == '/' || *p == '\\')
         rv = p;
      ++p;
   }
   return rv;
#else
   return strrchr(path, QORE_DIR_SEP);
#endif
}

// thread-safe basename function (resulting pointer must be free()ed)
char* q_basename(const char* path) {
   const char* p = q_find_last_path_sep(path);
   if (!p)
      return strdup(path);
   return strdup(p + 1);
}

// returns a pointer within the same string
char* q_basenameptr(const char* path) {
   const char* p = q_find_last_path_sep(path);
   if (!p)
      return (char* )path;
   return (char*)p + 1;
}

// thread-safe dirname function (resulting pointer must be free()ed)
char* q_dirname(const char* path) {
   const char* p = q_find_last_path_sep(path);
   if (!p || p == path) {
      char* x = (char* )malloc(sizeof(char) * 2);
      x[0] = !p ? '.' : QORE_DIR_SEP;
      x[1] = '\0';
      return x;
   }
   char* x = (char* )malloc(sizeof(char) * (p - path + 1));
   strncpy(x, path, p - path);
   x[p - path] = '\0';
   return x;
}

void* q_realloc(void* ptr, size_t size) {
    void* p = realloc(ptr, size);
    if (!p) {
        free(ptr);
    }
    return p;
}

static inline void assign_hv(QoreHashNode* h, const char* key, char* val) {
   h->setKeyValue(key, new QoreStringNode(val), 0);
}

static inline void assign_hv(QoreHashNode* h, const char* key, int val) {
   h->setKeyValue(key, val, 0);
}

#ifdef HAVE_PWD_H
static QoreHashNode* pwd2hash(const struct passwd& pw) {
   QoreHashNode* h = new QoreHashNode(autoTypeInfo);
   // assign values
   assign_hv(h, "pw_name", pw.pw_name);
   assign_hv(h, "pw_passwd", pw.pw_passwd);
   assign_hv(h, "pw_gecos", pw.pw_gecos);
   assign_hv(h, "pw_dir", pw.pw_dir);
   assign_hv(h, "pw_shell", pw.pw_shell);
   assign_hv(h, "pw_uid", pw.pw_uid);
   assign_hv(h, "pw_gid", pw.pw_gid);
   return h;
}

QoreHashNode* q_getpwuid(uid_t uid) {
   struct passwd *pw;
#ifdef HAVE_GETPWUID_R
   struct passwd pw_rec;
   char* buf = (char* )malloc(pwsize * sizeof(char));
   ON_BLOCK_EXIT(free, buf);
   int rc = getpwuid_r(uid, &pw_rec, buf, pwsize, &pw);
   if (rc)
      errno = rc;
#else
   AutoLocker al(&lck_passwd);
   pw = getpwuid(uid);
#endif
   return !pw ? 0 : pwd2hash(*pw);
}

QoreHashNode* q_getpwnam(const char* name) {
   struct passwd *pw;
#ifdef HAVE_GETPWNAM_R
   struct passwd pw_rec;
   char* buf = (char* )malloc(pwsize * sizeof(char));
   ON_BLOCK_EXIT(free, buf);
   int rc = getpwnam_r(name, &pw_rec, buf, pwsize, &pw);
   if (rc)
      errno = rc;
#else
   AutoLocker al(&lck_passwd);
   pw = getpwnam(name);
#endif
   return !pw ? 0 : pwd2hash(*pw);
}

int q_uname2uid(const char* name, uid_t& uid) {
   struct passwd *pw;
#ifdef HAVE_GETPWNAM_R
   struct passwd pw_rec;
   char* buf = (char* )malloc(pwsize * sizeof(char));
   ON_BLOCK_EXIT(free, buf);
   int rc = getpwnam_r(name, &pw_rec, buf, pwsize, &pw);
   if (!rc)
      uid = pw_rec.pw_uid;
   return rc;
#else
   AutoLocker al(&lck_passwd);
   pw = getpwnam(name);
   if (!pw)
      return errno;
   uid = pw->pw_uid;
   return 0;
#endif
}

static QoreHashNode* gr2hash(struct group& gr) {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    // assign values
    assign_hv(h, "gr_name", gr.gr_name);
    assign_hv(h, "gr_passwd", gr.gr_passwd);
    assign_hv(h, "gr_gid", gr.gr_gid);

    QoreListNode* l = new QoreListNode(stringTypeInfo);
    for (char* *p = gr.gr_mem; *p; ++p)
        l->push(new QoreStringNode(*p), nullptr);

    h->setKeyValue("gr_mem", l, 0);
    return h;
}

QoreHashNode* q_getgrgid(gid_t gid) {
   struct group *gr;
#ifdef HAVE_GETGRGID_R
   struct group gr_rec;
   char* buf = (char* )malloc(grsize * sizeof(char));
   ON_BLOCK_EXIT(free, buf);
   int rc = getgrgid_r(gid, &gr_rec, buf, grsize, &gr);
   if (rc)
      errno = rc;
#else
   AutoLocker al(&lck_group);
   gr = getgrgid(gid);
#endif
   return !gr ? 0 : gr2hash(*gr);
}

QoreHashNode* q_getgrnam(const char* name) {
   struct group *gr;
#ifdef HAVE_GETGRNAM_R
   struct group gr_rec;
   char* buf = (char* )malloc(grsize * sizeof(char));
   ON_BLOCK_EXIT(free, buf);
   int rc = getgrnam_r(name, &gr_rec, buf, grsize, &gr);
   if (rc)
      errno = rc;
#else
   AutoLocker al(&lck_group);
   gr = getgrnam(name);
#endif
   return !gr ? 0 : gr2hash(*gr);
}

int q_gname2gid(const char* name, gid_t &gid) {
   struct group *gr;
#ifdef HAVE_GETGRNAM_R
   struct group gr_rec;
   char* buf = (char* )malloc(grsize * sizeof(char));
   ON_BLOCK_EXIT(free, buf);
   int rc = getgrnam_r(name, &gr_rec, buf, grsize, &gr);
   if (!rc)
      gid = gr_rec.gr_gid;
   return rc;
#else
   AutoLocker al(&lck_group);
   gr = getgrnam(name);
   if (!gr)
      return errno;
   gid = gr->gr_gid;
   return 0;
#endif
}
#endif // HAVE_PWD_H

qore_license_t qore_get_license() {
   return qore_license;
}

long long q_atoll(const char* str) {
   return atoll(str);
}

// returns seconds since epoch
int64 q_epoch() {
#ifdef HAVE_CLOCK_GETTIME
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        printd(0, "clock_gettime() failed: %s\n", strerror(errno));
        return 0;
    }
#else
    struct timeval ts;
    if (gettimeofday(&ts, 0)) {
        printd(0, "gettimeofday() failed: %s\n", strerror(errno));
        return 0;
    }
#endif
    return ts.tv_sec;
}

// returns seconds since epoch and gets microseconds
int64 q_epoch_us(int &us) {
#ifdef HAVE_CLOCK_GETTIME
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        printd(0, "clock_gettime() failed: %s\n", strerror(errno));
        us = 0;
        return 0;
    }
    us = ts.tv_nsec / 1000;
#else
    struct timeval ts;
    if (gettimeofday(&ts, 0)) {
        printd(0, "gettimeofday() failed: %s\n", strerror(errno));
        us = 0;
        return 0;
    }
    us = ts.tv_usec;
#endif
    return ts.tv_sec;
}

// returns seconds since epoch and gets nanoseconds
int64 q_epoch_ns(int &ns) {
#ifdef HAVE_CLOCK_GETTIME
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        printd(0, "clock_gettime() failed: %s\n", strerror(errno));
        ns = 0;
        return 0;
    }
    ns = ts.tv_nsec;
#else
    struct timeval ts;
    if (gettimeofday(&ts, 0)) {
        printd(0, "gettimeofday() failed: %s\n", strerror(errno));
        ns = 0;
        return 0;
    }
    ns = ts.tv_usec * 1000;
#endif
    return ts.tv_sec;
}

QoreParseListNode* make_args(const QoreProgramLocation* loc, QoreValue arg) {
    if (!arg) {
        return nullptr;
    }

    QoreParseListNode* l;
    if (arg.getType() == NT_PARSE_LIST) {
        l = arg.get<QoreParseListNode>();
        if (!l->isFinalized())
            return l;
    }

    l = new QoreParseListNode(loc);
    l->add(arg, loc);
    return l;
}

const char* check_hash_key(const QoreHashNode* h, const char* key, const char* err, ExceptionSink* xsink) {
   QoreValue p = h->getKeyValue(key);
   if (p.isNothing())
      return nullptr;

   if (p.getType() != NT_STRING) {
      xsink->raiseException(err, "'%s' key is not type 'string' but is type '%s'", key, p.getTypeName());
      return nullptr;
   }
   return p.get<const QoreStringNode>()->c_str();
}

void q_strerror(QoreString &str, int err) {
#ifdef HAVE_STRERROR_R

#ifndef STRERR_BUFSIZE
#define STRERR_BUFSIZE 256
#endif

   str.allocate(str.strlen() + STRERR_BUFSIZE);
   // ignore strerror() error message
#ifdef STRERROR_R_CHAR_P
   // we can't help but get this version because some of the Linux
   // header files define _GNU_SOURCE for us :-(
   str.concat(strerror_r(err, (char* )(str.getBuffer() + str.strlen()), STRERR_BUFSIZE));
#else
   // use portable XSI version of strerror_r()
   int rc = strerror_r(err, (char* )(str.getBuffer() + str.strlen()), STRERR_BUFSIZE);
   if (rc && rc != EINVAL && rc != ERANGE)
      str.sprintf("unable to retrieve error code %d: strerror() returned unexpected error code %d", err, rc);
   else
      str.terminate(str.strlen() + strlen(str.getBuffer() + str.strlen()));
#endif

#else
   // global static lock for strerror() access
   static QoreThreadLock strerror_m;

   AutoLocker al(strerror_m);
   str.concat(strerror(err));
#endif
}

QoreStringNode* q_strerror(int err) {
   QoreStringNode* rv = new QoreStringNode;
   q_strerror(*rv, err);
   return rv;
}

QoreStringNode* qore_reassign_signal(int sig, const char* name) {
#ifdef HAVE_SIGNAL_HANDLING
    return QSM.reassignSignal(sig, name, false);
#else
    return nullptr;
#endif
}

QoreStringNode* qore_reassign_signal(int sig, const char* name, bool reuse_sys) {
#ifdef HAVE_SIGNAL_HANDLING
    return QSM.reassignSignal(sig, name, reuse_sys);
#else
    return nullptr;
#endif
}

QoreStringNode* qore_reassign_signals(const sig_vec_t& sig_vec, const char* name) {
#ifdef HAVE_SIGNAL_HANDLING
    return QSM.reassignSignals(sig_vec, name, false);
#else
    return nullptr;
#endif
}

QoreStringNode* qore_reassign_signals(const sig_vec_t& sig_vec, const char* name, bool reuse_sys) {
#ifdef HAVE_SIGNAL_HANDLING
    return QSM.reassignSignals(sig_vec, name, reuse_sys);
#else
    return nullptr;
#endif
}

int qore_release_signal(int sig, const char* name) {
#ifdef HAVE_SIGNAL_HANDLING
    return QSM.releaseSignal(sig, name);
#else
    return 0;
#endif
}

int qore_release_signals(const sig_vec_t& sig_vec, const char* name) {
#ifdef HAVE_SIGNAL_HANDLING
    return QSM.releaseSignals(sig_vec, name);
#else
    return 0;
#endif
}

// returns 0 for OK, -1 for error
int check_lvalue(QoreValue n, bool assignment) {
    if (n.isNothing()) {
        return 0;
    }

    if (!n.hasNode()) {
        return -1;
    }

    return check_lvalue(n.getInternalNode(), assignment);
}

// returns 0 for OK, -1 for error
int check_lvalue(AbstractQoreNode* node, bool assignment) {
    qore_type_t ntype = node->getType();
    //printd(5, "type: %s\n", node->getTypeName());
    if (ntype == NT_VARREF) {
        if (assignment)
            reinterpret_cast<VarRefNode*>(node)->parseAssigned();
        return 0;
    }

    if (ntype == NT_SELF_VARREF)
        return 0;

    if (ntype == NT_CLASS_VARREF)
        return 0;

    if (ntype == NT_OPERATOR) {
        {
            QoreSquareBracketsOperatorNode* op = dynamic_cast<QoreSquareBracketsOperatorNode*>(node);
            if (op) {
                return check_lvalue(op->getLeft(), assignment);
            }
        }
        {
            QoreSquareBracketsRangeOperatorNode* op = dynamic_cast<QoreSquareBracketsRangeOperatorNode*>(node);
            if (op) {
                return check_lvalue(op->get(0), assignment);
            }
        }
        {
            QoreHashObjectDereferenceOperatorNode* op = dynamic_cast<QoreHashObjectDereferenceOperatorNode*>(node);
            if (op) {
                return check_lvalue(op->getLeft(), assignment);
            }
        }
        {
            QoreCastOperatorNode* op = dynamic_cast<QoreCastOperatorNode*>(node);
            if (op) {
                return check_lvalue(op->getExp(), assignment);
            }
        }
    }

    return -1;
}

static void stat_get_blocks(const struct stat& sbuf, int64& blksize, int64& blocks) {
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    blksize = sbuf.st_blksize;
#else
    blksize = 0;
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    blocks = sbuf.st_blocks;
#else
    blocks = 0;
#endif
}

QoreListNode* stat_to_list(const struct stat& sbuf) {
    QoreListNode* l = new QoreListNode(autoTypeInfo);

    // note that dev_t on Linux is an unsigned 64-bit integer, so we could lose precision here
    l->push(((int64)sbuf.st_dev), nullptr);
    l->push(sbuf.st_ino, nullptr);
    l->push(sbuf.st_mode, nullptr);
    l->push(sbuf.st_nlink, nullptr);
    l->push(sbuf.st_uid, nullptr);
    l->push(sbuf.st_gid, nullptr);
    // note that dev_t on Linux is an unsigned 64-bit integer, so we could lose precision here
    l->push((int64)sbuf.st_rdev, nullptr);
    l->push(sbuf.st_size, nullptr);

    l->push(DateTimeNode::makeAbsolute(currentTZ(), (int64)sbuf.st_atime), nullptr);
    l->push(DateTimeNode::makeAbsolute(currentTZ(), (int64)sbuf.st_mtime), nullptr);
    l->push(DateTimeNode::makeAbsolute(currentTZ(), (int64)sbuf.st_ctime), nullptr);

    int64 blksize, blocks;
    stat_get_blocks(sbuf, blksize, blocks);
    l->push(blksize, nullptr);
    l->push(blocks, nullptr);

    return l;
}

TypedHashDecl* qore_get_hashdecl(const char* name) {
    TypedHashDecl* rv = qore_ns_private::get(*staticSystemNamespace->rootGetQoreNamespace())->hashDeclList.find(name);
    assert(rv);
    return rv;
}

QoreHashNode* stat_to_hash(const struct stat& sbuf, const TypedHashDecl* hd) {
    QoreHashNode* h = new QoreHashNode(hd, nullptr);

    // note that dev_t on Linux is an unsigned 64-bit integer, so we could lose precision here
    qore_hash_private* ph = qore_hash_private::get(*h);
    ph->setKeyValueIntern("dev",     (int64)sbuf.st_dev);
    ph->setKeyValueIntern("inode",   (int64)sbuf.st_ino);
    ph->setKeyValueIntern("mode",    (int64)sbuf.st_mode);
    ph->setKeyValueIntern("nlink",   (int64)sbuf.st_nlink);
    ph->setKeyValueIntern("uid",     (int64)sbuf.st_uid);
    ph->setKeyValueIntern("gid",     (int64)sbuf.st_gid);
    // note that dev_t on Linux is an unsigned 64-bit integer, so we could lose precision here
    ph->setKeyValueIntern("rdev",    (int64)sbuf.st_rdev);
    ph->setKeyValueIntern("size",    (int64)sbuf.st_size);

    ph->setKeyValueIntern("atime",   DateTimeNode::makeAbsolute(currentTZ(), (int64)sbuf.st_atime));
    ph->setKeyValueIntern("mtime",   DateTimeNode::makeAbsolute(currentTZ(), (int64)sbuf.st_mtime));
    ph->setKeyValueIntern("ctime",   DateTimeNode::makeAbsolute(currentTZ(), (int64)sbuf.st_ctime));

    int64 blksize, blocks;
    stat_get_blocks(sbuf, blksize, blocks);
    ph->setKeyValueIntern("blksize", (int64)blksize);
    ph->setKeyValueIntern("blocks",  (int64)blocks);

    // process permissions
    QoreStringNode* perm = new QoreStringNode;
    const char* type = q_mode_to_perm(sbuf.st_mode, *perm);

    ph->setKeyValueIntern("type", new QoreStringNode(type));
    ph->setKeyValueIntern("perm", perm);

    return h;
}

#ifdef Q_HAVE_STATVFS
QoreHashNode* statvfs_to_hash(const struct statvfs& vfs) {
    QoreHashNode* h = new QoreHashNode(hashdeclFilesystemInfo, nullptr);
    qore_hash_private* ph = qore_hash_private::get(*h);

    ph->setKeyValueIntern("namemax", vfs.f_namemax);
    ph->setKeyValueIntern("fsid", vfs.f_fsid);
    ph->setKeyValueIntern("frsize", vfs.f_frsize);
    ph->setKeyValueIntern("bsize", vfs.f_bsize);
    ph->setKeyValueIntern("flag", vfs.f_flag);
    ph->setKeyValueIntern("blocks", vfs.f_blocks);
    ph->setKeyValueIntern("bfree", vfs.f_bfree);
    ph->setKeyValueIntern("bavail", vfs.f_bavail);
    ph->setKeyValueIntern("files", vfs.f_files);
    ph->setKeyValueIntern("ffree", vfs.f_ffree);
    ph->setKeyValueIntern("favail", vfs.f_favail);

    return h;
}
#endif

#if defined(DEBUG) && defined(HAVE_BACKTRACE)
#define _QORE_BT_SIZE 20
void qore_machine_backtrace() {
    void *array[_QORE_BT_SIZE];
    // get void*'s for all entries on the stack
    size_t size = backtrace(array, _QORE_BT_SIZE);

    // print out all the frames to stderr
    backtrace_symbols_fd(array, size, 2);
}
#else
void qore_machine_backtrace() {
}
#endif

#ifdef HAVE_NANOSLEEP
static int qore_nanosleep(int64 ns) {
    struct timespec ts;
    ts.tv_sec = ns / 1000000000ll;
    ts.tv_nsec = (ns - ts.tv_sec * 1000000000);
    int rc;
    while (true) {
        rc = nanosleep(&ts, 0);
        if (rc && errno == EINTR) {
            continue;
        }
        break;
    }
    return rc;
}
#endif

int qore_usleep(int64 usecs) {
#ifdef HAVE_NANOSLEEP
    return qore_nanosleep(usecs * 1000);
#else
#ifdef _Q_WINDOWS
    LARGE_INTEGER ft;
    ft.QuadPart = -(10 * usecs); // Convert to 100 nanosecond interval, negative value indicates relative time

    HANDLE timer = CreateWaitableTimer(0, TRUE, 0);
    SetWaitableTimer(timer, &ft, 0, 0, 0, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    return 0;
#else
    return ::usleep(usecs);
#endif
#endif
}

bool q_path_is_readable(const char* path) {
#if defined HAVE_PWD_H
    struct stat sbuf;
    int rc;

    if ((rc = stat(path, &sbuf)))
        return false;

    if (S_ISDIR(sbuf.st_mode)) { // If path is a directory.
        DIR* dp = opendir(path);
        if (dp != NULL) {
            closedir(dp);
            return true;
        }
        return false;
    }

    rc = open(path, O_RDONLY);
    if (rc != -1) {
        close(rc);
        return true;
    }
    return false;

#elif defined(HAVE_ACCESS) && defined(_Q_WINDOWS)
    // only use access(2) on Windows
    return !access(path, R_OK);
#else
    // check if it's a directory
    struct stat sbuf;
    int rc;
    if ((rc = stat(path, &sbuf)))
        return false;

    // just return true on windows if it's a directory
    if ((sbuf.st_mode & S_IFMT) == S_IFDIR)
        return true;

    // otherwise try to open file, if successful, then the file is readable
    rc = open(path, O_RDONLY);
    if (rc != -1) {
        close(rc);
        return true;
    }
    return false;
#endif
}

QoreProgramLocation::QoreProgramLocation(int sline, int eline) : QoreProgramLineLocation(sline, eline) {
    set_parse_file_info(*this);
}

void QoreProgramLocation::toString(QoreString& str) const {
    str.concat(file ? file : "<unknown>");
    if (start_line > 0) {
        str.sprintf(":%d", start_line);
        if (end_line > 0 && end_line != start_line)
            str.sprintf("-%d", end_line);
    }

    if (source)
        str.sprintf(" (source \"%s\":%d)", source, start_line + offset);
}

void LVarSet::add(LocalVar* var) {
    if (!needs_scan && var->needsScan())
        needs_scan = true;
    // insert var into the set
    insert(var);
}

bool q_parse_bool(QoreValue n) {
    return n.getType() == NT_STRING
        ? q_parse_bool(n.get<const QoreStringNode>()->c_str())
        : n.getAsBool();
}

bool q_parse_bool(const char* str) {
    if (!strcasecmp(str, "true") || !strcasecmp(str, "on") || !strcasecmp(str, "yes") || !strncasecmp(str, "enable", 6) || !strcasecmp(str, "y"))
        return true;

    return (bool)atoi(str);
}

int qore_get_library_init_options() {
    return qore_library_options;
}

int qore_set_library_cleanup_options(int options) {
    return (qore_library_options |= (options & QLO_CLEANUP_MASK));
}

bool qore_check_option(int opt) {
    return (qore_library_options & opt) == opt;
}

const char* q_mode_to_perm(mode_t mode, QoreString& perm) {
    const char* type;
    if (S_ISBLK(mode)) {
        type = "BLOCK-DEVICE";
        perm.concat('b');
    }
    else if (S_ISDIR(mode)) {
        type = "DIRECTORY";
        perm.concat('d');
    }
    else if (S_ISCHR(mode)) {
        type = "CHARACTER-DEVICE";
        perm.concat('c');
    }
    else if (S_ISFIFO(mode)) {
        type = "FIFO";
        perm.concat('p');
    }
#ifdef S_ISLNK
    else if (S_ISLNK(mode)) {
        type = "SYMBOLIC-LINK";
        perm.concat('l');
    }
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(mode)) {
        type = "SOCKET";
        perm.concat('s');
    }
#endif
    else if (S_ISREG(mode)) {
        type = "REGULAR";
        perm.concat('-');
    }
    else {
        type = "UNKNOWN";
        perm.concat('?');
    }

    // add user permission flags
    perm.concat(mode & S_IRUSR ? 'r' : '-');
    perm.concat(mode & S_IWUSR ? 'w' : '-');
#ifdef S_ISUID
    if (mode & S_ISUID)
        perm.concat(mode & S_IXUSR ? 's' : 'S');
    else
        perm.concat(mode & S_IXUSR ? 'x' : '-');
#else
    // Windows
    perm.concat('-');
#endif

    // add group permission flags
#ifdef S_IRGRP
    perm.concat(mode & S_IRGRP ? 'r' : '-');
    perm.concat(mode & S_IWGRP ? 'w' : '-');
#else
    // Windows
    perm.concat("--");
#endif
#ifdef S_ISGID
    if (mode & S_ISGID)
        perm.concat(mode & S_IXGRP ? 's' : 'S');
    else
        perm.concat(mode & S_IXGRP ? 'x' : '-');
#else
    // Windows
    perm.concat('-');
#endif

#ifdef S_IROTH
    // add other permission flags
    perm.concat(mode & S_IROTH ? 'r' : '-');
    perm.concat(mode & S_IWOTH ? 'w' : '-');
#ifdef S_ISVTX
    if (mode & S_ISVTX)
        perm.concat(mode & S_IXOTH ? 't' : 'T');
    else
#endif
        perm.concat(mode & S_IXOTH ? 'x' : '-');
#else
    // Windows
    perm.concat("---");
#endif

    return type;
}

int64 q_clock_getmillis() {
    int us;
    int64 seconds = q_epoch_us(us);
    return seconds * 1000 + us / 1000;
}

int64 q_clock_getmicros() {
    int us;
    int64 seconds = q_epoch_us(us);
    return seconds * 1000000ll + us;
}

int64 q_clock_getnanos() {
    int ns;
    int64 seconds = q_epoch_ns(ns);
    return seconds * 1000000000ll + ns;
}

#ifndef HAVE_STRCASESTR
#ifdef HAVE_STRNCASECMP
char* strcasestr(const char* s1, const char* s2) {
    if (!s2 || !s1) return 0;

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    if (len2 > len1)
        return 0;

    for (size_t i = 0, end = len1 - len2; i <= end; ++i) {
        if (!strncasecmp(s2, s1 + i, len2)) {
            return ((char*)s1 + i);
        }
    }

    return 0;
}
#else
#error no strcasestr implementation
#endif
#endif

bool q_absolute_path_unix(const char* path) {
    return path && path[0] == '/';
}

// returns true if the path is not relative; may not be strictly-speaking an absolute path
bool q_absolute_path_windows(const char* path) {
    if (!path)
        return false;
    // note that '\' and '/' are both accepted as path separator characters on Windows
    if (path[0] == '\\' || path[0] == '/')
        return true;
    if (isalpha(path[0]) && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
        return true;
    return false;
}

bool q_absolute_path(const char* path) {
#ifdef _Q_WINDOWS
    return q_absolute_path_windows(path);
#else
    return q_absolute_path_unix(path);
#endif
}

#ifdef _Q_WINDOWS
// this appends the root path
int q_get_root_path(QoreString& root, const char* cwd = 0) {
    QoreString Cwd;
    if (cwd) {
        Cwd.set(cwd);
        q_realpath(Cwd, Cwd);
    } else
        q_getcwd(Cwd);

    // return without making any changes in case of an error
    if (Cwd.empty())
        return -1;

    // see if we have a drive letter + ':'
    if (isalpha(Cwd[0])) {
        root.concat(Cwd[0]);
        root.concat(':');
        return 0;
    }

    // see if we have a UNC path, so we need to find the end of the \\server\share\ component
    if (Cwd[0] == '\\' && Cwd[1] == '\\') {
        char* c = strchr(Cwd.c_str() + 2, '\\');
        if (c) {
            c = strchr(c + 1, '\\');
            root.concat(Cwd.c_str(), c ? c - Cwd.c_str() : Cwd.size());
            return 0;
        }
    }
    return -1;
}
#endif

static void add_cwd(QoreString& path) {
    QoreString cwd_str;
    if (!q_getcwd(cwd_str))
        path.prepend(cwd_str.getBuffer(), cwd_str.size());
}

void q_normalize_path(QoreString& path, const char* cwd) {
    //printd(5, "q_normalize_path(path: '%s', cwd: '%s')\n", path.getBuffer(), cwd);
    if (!path.empty()) {
        if (!q_absolute_path(path.getBuffer())) {
            path.insertch(QORE_DIR_SEP, 0, 1);
            if (cwd) {
                QoreString Cwd(cwd);
                q_realpath(Cwd, Cwd);
                path.prepend(Cwd.getBuffer());
                if (path[0] == '.') {
                    path.insertch(QORE_DIR_SEP, 0, 1);
                    add_cwd(path);
                }
            }
            else
                add_cwd(path);
        }
#ifdef _Q_WINDOWS
        // prepend drive or UNC path root to path if the path begins with a single slash or backslash
        else if ((path[0] == '/' || path[0] == '\\')
                && (path[1] != '/' && path[1] != '\\')) {
            QoreString root;
            q_get_root_path(root, cwd);
            path.prepend(root.c_str());
            //printd(5, "normalized root: '%s' path: '%s'\n", root.c_str(), path.c_str());
        }
#endif
    }
    std::string str = path.getBuffer();
    str = qore_qd_private::normalizePath(str);
    //printd(5, "q_normalize_path() '%s' -> '%s' (cwd: '%s')\n", path.getBuffer(), str.c_str(), cwd);
    path = str;
}

int q_getcwd(QoreString& cwd) {
    int bs = 512;
    cwd.reserve(bs);

    while (true) {
        char* b = getcwd((char*)cwd.getBuffer(), bs);
        if (!b) {
            if (errno == ERANGE) {
                bs *= 2;
                cwd.reserve(bs);
                continue;
            }
            //printd(5, "q_getcwd() failed: errno: %d\n", errno);
            return -1;
        }
        break;
    }

    cwd.terminate(strlen(cwd.getBuffer()));
    //printd(5, "q_getcwd() succeeded: '%s'\n", cwd.getBuffer());
    return 0;
}

int q_get_mode(const QoreString& path) {
    struct stat sbuf;

    if (stat(path.getBuffer(), &sbuf))
        return 0;

    return sbuf.st_mode;
}

int q_realpath(const QoreString& path, QoreString& rv, ExceptionSink* xsink) {
#ifdef HAVE_REALPATH
#if defined(SOLARIS) || (defined(__NetBSD_Version__) && (__NetBSD_Version__ < 601000000))
    char buf[PATH_MAX];
    char* p = realpath(path.getBuffer(), buf);
#else
    char* p = realpath(path.getBuffer(), 0);
#endif
    if (!p) {
        if (xsink)
            xsink->raiseErrnoException("REALPATH-ERROR", errno, "error calling realpath()");
        return -1;
    }
#if defined(SOLARIS) || (defined(__NetBSD_Version__) && (__NetBSD_Version__ < 601000000))
    rv.set(buf);
#else
    rv.takeAndTerminate(p, strlen(p));
#endif
#else
#ifndef _Q_WINDOWS
#error must implement an alternative to realpath on UNIX systems
#endif
    if (&rv != &path)
        rv = path;
    q_normalize_path(rv);
    // verify that the path exists
    if (!q_get_mode(rv)) {
        if (xsink)
            xsink->raiseException("REALPATH-ERROR", "path '%s' does not exist", rv.getBuffer());
        return -1;
    }
    return 0;
#endif
    return 0;
}

void qore_disable_gc() {
    q_disable_gc = true;
}

bool qore_is_gc_enabled() {
    return !q_disable_gc;
}

int qore_set_library_options(int opts) {
    if (opts & QLO_MINIMUM_TLS_13) {
        opts &= ~QLO_DISABLE_TLS_13;
    }
    int val = opts & QLO_RUNTIME_OPTS;
    qore_library_options |= val;
    return val;
}

int qore_get_library_options() {
    return qore_library_options;
}

void qore_init_random_salt() {
   random_salt.sprintf(QLLD, q_clock_getmicros());
}

int qore_get_ptr_hash(QoreString& str, const void* ptr) {
   assert(!random_salt.empty());
   QoreString tmp(random_salt);
   tmp.sprintf("%p", ptr);
   DigestHelper dh(tmp);
   int rc = dh.doDigest(SHA1_ERR, EVP_sha1());
   if (!rc)
      dh.getString(str);
   else
      printd(0, "qore_get_ptr_hash() digest calculation failed for ptr %p\n", ptr);
   return rc;
}

void* q_memmem(const void* big, size_t big_len, const void* little, size_t little_len) {
    assert(big && little);
#ifdef HAVE_MEMMEM
    return memmem(big, big_len, little, little_len);
#else
    if (!big_len || little_len > big_len) {
        return nullptr;
    }
    if (!little_len) {
        return (void*)big;
    }
    const char* lt = (const char*)little;
    const char* bg = (const char*)big;
    //printd(5, "q_memmem() big: '%s' (%d) little: '%s' (%d)\n", big, (int)big_len, little, (int)little_len);
    const char* p = bg;
    const char* end = bg + big_len;
    while (p < end) {
        const char* f = (const char*)memchr(p, lt[0], end - p);
        //printd(5, "q_memmem() f: %p p: '%s' %p lt[0]: %c len: %d left: %d ll: %d\n", f, p, p, lt[0], (int)(end - p), (int)(big_len - (f - bg)), (int)little_len);
        // if there is no match or if there is not enough room to match the little string
        // then return with no match
        if (!f || ((big_len - (f - bg)) < little_len))
            return nullptr;
        p = f;
        bool found = true;
        for (size_t i = 1; i < little_len; ++i) {
            if (*(++f) != lt[i]) {
                found = false;
                break;
            }
        }
        if (found) {
            return (void*)p;
        }
        ++p;
    }
    return nullptr;
#endif
}

void* q_memrmem(const void* big, size_t big_len, const void* little, size_t little_len) {
    assert(big && little);
    if (!big_len || little_len > big_len) {
        return nullptr;
    }
    if (!little_len) {
        return (void*)big;
    }
    //printd(5, "q_memrmem() big: '%s' (%d) little: '%s' (%d)\n", big, (int)big_len, little, (int)little_len);
    const char* lt_end = (const char*)little + little_len - 1;
    const char* bg = (const char*)big;
    // first character to search in "big"
    const char* p = bg + big_len - 1;
    // last position in "big" where "little" can end
    const char* bg_begin = bg + little_len - 1;
    while (p >= bg_begin) {
        // compare from the end of the sequence
        if (*p == *lt_end) {
            if (little_len == 1) {
                return const_cast<void*>(reinterpret_cast<const void*>(p));
            }
            bool found = true;
            const char* p0_end = p - little_len + 1;
            const char* p0 = p - 1;
            const char* lt0 = lt_end - 1;
            while (true) {
                if (*p0 != *lt0) {
                    found = false;
                    break;
                }
                if (p0 == p0_end) {
                    break;
                }
                --p0;
                --lt0;
            }
            if (found) {
                return const_cast<void*>(reinterpret_cast<const void*>(p0));
            }
        }
        --p;
    }
    return nullptr;
}

double q_strtod(const char* str) {
    std::istringstream istr(str);
    istr.imbue(std::locale::classic());
    double rv;
    istr >> rv;
    return rv;
}

#ifdef _Q_WINDOWS
int statvfs(const char* path, struct statvfs* buf) {
    ULARGE_INTEGER avail;
    ULARGE_INTEGER total;
    ULARGE_INTEGER free;

    if (!GetDiskFreeSpaceEx(path, &avail, &total, &free)) {
        return -1;
    }

    buf->set(avail.QuadPart, total.QuadPart, free.QuadPart);
    return 0;
}

int q_fstatvfs(const char* filepath, struct statvfs* buf) {
    char* dir = q_dirname(filepath);
    if (!dir)
        return -1;
    ON_BLOCK_EXIT(free, dir);

    return statvfs(dir, buf);
}
#endif

// call to get a node with reference count 1 (copy on write)
void ensure_unique(AbstractQoreNode* *v, ExceptionSink* xsink) {
    assert(*v);
    if (!(*v)->is_unique()) {
        AbstractQoreNode* old = *v;
        (*v) = old->realCopy();
        old->deref(xsink);
        assert(!*xsink);
    }
}

void ensure_unique(QoreValue& v, ExceptionSink* xsink) {
    if (!v.hasNode()) {
        return;
    }
    AbstractQoreNode* n = v.getInternalNode();
    if (!n->is_unique()) {
        AbstractQoreNode* old = n;
        n = old->realCopy();
        old->deref(xsink);
        assert(!*xsink);
        v = n;
    }
}

// checks for illegal "self" assignments in an object context
int check_self_assignment(const QoreProgramLocation* loc, QoreValue n, LocalVar* selfid) {
    qore_type_t ntype = n.getType();

    // if it's a variable reference
    if (ntype == NT_VARREF) {
        VarRefNode* v = n.get<VarRefNode>();
        if (v->getType() == VT_LOCAL && v->ref.id == selfid) {
            parse_error(*loc, "illegal assignment to 'self' in an object context");
            return -1;
        }
    }
    return 0;
}

int check_lvalue_int(const QoreProgramLocation* loc, const QoreTypeInfo*& typeInfo, const char* name) {
    // make sure the lvalue can be assigned an integer value
    // raise a parse exception only if parse exceptions are not suppressed
    if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_INT)) {
        if (getProgram()->getParseExceptionSink()) {
            QoreStringNode* desc = new QoreStringNode("lvalue has type ");
            QoreTypeInfo::getThisType(typeInfo, *desc);
            desc->sprintf(", but the %s will assign it an integer value", name);
            qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
        }
        return -1;
    }
    return 0;
}

int check_lvalue_number(const QoreProgramLocation* loc, const QoreTypeInfo*& typeInfo, const char* name) {
    // make sure the lvalue can be assigned a floating-point value
    // raise a parse exception only if parse exceptions are not suppressed
    if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_NUMBER)) {
        if (getProgram()->getParseExceptionSink()) {
            QoreStringNode* desc = new QoreStringNode("lvalue has type ");
            QoreTypeInfo::getThisType(typeInfo, *desc);
            desc->sprintf(", but the %s will assign it a number value", name);
            qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
        }
        return -1;
    }
    return 0;
}

int check_lvalue_float(const QoreProgramLocation* loc, const QoreTypeInfo*& typeInfo, const char* name) {
    // make sure the lvalue can be assigned a floating-point value
    // raise a parse exception only if parse exceptions are not suppressed
    if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_FLOAT)) {
        if (getProgram()->getParseExceptionSink()) {
            QoreStringNode* desc = new QoreStringNode("lvalue has type ");
            QoreTypeInfo::getThisType(typeInfo, *desc);
            desc->sprintf(", but the %s will assign it a float value", name);
            qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
        }
        return -1;
    }
    return 0;
}

int check_lvalue_int_float_number(const QoreProgramLocation* loc, const QoreTypeInfo*& typeInfo, const char* name) {
    // make sure the lvalue can be assigned an integer value
    // raise a parse exception only if parse exceptions are not suppressed
    if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_INT)
            && !QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_FLOAT)
            && !QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_NUMBER)) {
        if (getProgram()->getParseExceptionSink()) {
            QoreStringNode* desc = new QoreStringNode("lvalue has type ");
            QoreTypeInfo::getThisType(typeInfo, *desc);
            desc->sprintf(", but the %s only works with integer, floating-point, or numeric lvalues", name);
            qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
        }
        return -1;
    }
    if (QoreTypeInfo::parseReturns(typeInfo, NT_INT)) {
        if (QoreTypeInfo::parseReturns(typeInfo, NT_FLOAT)) {
            if (QoreTypeInfo::parseReturns(typeInfo, NT_NUMBER))
                typeInfo = bigIntFloatOrNumberTypeInfo;
            else
                typeInfo = bigIntOrFloatTypeInfo;
        } else
            typeInfo = bigIntTypeInfo;
    } else {
        if (QoreTypeInfo::parseReturns(typeInfo, NT_FLOAT))
            if (QoreTypeInfo::parseReturns(typeInfo, NT_NUMBER))
                typeInfo = floatOrNumberTypeInfo;
            else
                typeInfo = floatTypeInfo;
        else
            typeInfo = numberTypeInfo;
    }

    return 0;
}

static void do_subst(QoreString& str, const char* i, const char* ep, int offset) {
    assert(i < ep);
    QoreString var(i + 1 + offset, ep - i - 1 - offset, str.getEncoding());
    QoreString val;
    SystemEnvironment::get(var.c_str(), val);

    //printd(5, "do_subst() '%s': '%s'\n", var.c_str(), val.c_str());

    str.replace(i - str.c_str(), ep - i + offset, val.c_str());
}

static int do_bracket_subst(QoreString& str, const char* i, char c) {
    const char* ep = strchr(i + 2, c);
    if (!ep)
        return -1;

    do_subst(str, i, ep, 1);
    return 0;
}

int q_env_subst(QoreString& str) {
    const char* i;
    while ((i = strchr(str.c_str(), '$'))) {
        const char* ep = i + 1;
        if (!*ep)
            return -1;
        if (*ep == '(') {
            if (do_bracket_subst(str, i, ')'))
                return -1;
            continue;
        }
        if (*ep == '{') {
            if (do_bracket_subst(str, i, '}'))
                return -1;
            continue;
        }
        while (*ep && (*ep == '_' || isalnum(*ep)))
            ++ep;
        do_subst(str, i, ep, 0);
    }

    return 0;
}

static void q_remove_bom_utf16_intern(QoreString* str, const QoreEncoding*& enc) {
    assert(str->getEncoding() == enc);
    if (str->size() > 1 && !enc->isAsciiCompat()) {
        if ((enc == QCS_UTF16 || enc == QCS_UTF16BE) && str->c_str()[0] == (char)0xfe && str->c_str()[1] == (char)0xff) {
            str->replace(0, 2, (const char*)nullptr);
            if (enc == QCS_UTF16) {
                str->setEncoding(QCS_UTF16BE);
                enc = QCS_UTF16BE;
            }
        }
        else if ((enc == QCS_UTF16 || enc == QCS_UTF16LE) && str->c_str()[1] == (char)0xfe && str->c_str()[0] == (char)0xff) {
            str->replace(0, 2, (const char*)nullptr);
            if (enc == QCS_UTF16) {
                str->setEncoding(QCS_UTF16LE);
                enc = QCS_UTF16LE;
            }
        }
    }
}

QoreString* q_remove_bom_utf16(QoreString* str, const QoreEncoding*& enc) {
    q_remove_bom_utf16_intern(str, enc);
    return str;
}

QoreStringNode* q_remove_bom_utf16(QoreStringNode* str, const QoreEncoding*& enc) {
    q_remove_bom_utf16_intern(str, enc);
    return str;
}

QoreStringNode* q_read_string_all(ExceptionSink* xsink, const QoreEncoding* enc, f_read_t my_read) {
    size_t size = 0;
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(enc));
    while (true) {
        // ensure there is space in the buffer
        str->reserve(size + DefaultStreamReaderHelperBufferSize);

        int64 rc = my_read((void*)(str->c_str() + size), DefaultStreamReaderHelperBufferSize, xsink);

        if (*xsink)
            return 0;
        if (!rc)
            break;

        size += rc;
    }
    if (!size)
        return 0;
    str->terminate(size);
    return str.release();
}

QoreStringNode* q_read_string(ExceptionSink* xsink, int64 size, const QoreEncoding* enc, f_read_t my_read) {
    if (!size)
        return nullptr;
    if (size < 0)
        return q_read_string_all(xsink, enc, my_read);

    // original number of characters requested
    size_t orig_size = size;

    // byte offset of the byte position directly after the last full character scanned
    size_t last_char = 0;

    // total number of characters read
    size_t char_len = 0;

    // minimum character width
    unsigned mw = enc->getMinCharWidth();
    // get minimum byte length
    size *= mw;

    bool check_bom = false;

    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(enc));
    while (char_len < orig_size) {
        // get the minimum number of bytes to read
        size_t bs = size - str->size();

        // ensure there is space in the buffer
        str->reserve(str->size() + bs);

        int rc = my_read((void*)(str->c_str() + str->size()), bs, xsink);
        if (*xsink)
            return 0;
        if (rc == 0)
            break;

        str->terminate(str->size() + rc);

        //printd(5, "srh bs: %d rc: %d str: '%s' (%d %s)\n", bs, rc, str->c_str(), str->size(), enc->getCode());

        // if we have a non-multi-byte character encoding, then we can use byte lengths
        if (!enc->isMultiByte()) {
            if ((size_t)size == str->size())
                break;
            continue;
        } else if (!check_bom && str->size() > 1) {
            check_bom = true;
            q_remove_bom_utf16(*str, enc);
        }

        // scan data read and find the last valid character position
        const char* e = str->c_str() + str->size();
        while (char_len < orig_size && last_char < str->size()) {
            const char* p = str->c_str() + last_char;
            int cc = enc->getCharLen(p, e - p);
            if (!cc) {
                xsink->raiseException("STREAM-ENCODING-ERROR", "invalid multi-byte character received in byte offset "
                    QSD " according to the input encoding: '%s'", last_char, enc->getCode());

                return 0;
            }

            //printd(5, "StreamReader::readString() orig: " QLLD " size: " QLLD " char_len: " QLLD " rc: %d "
            //    last_char: " QSD " c: %d (offset: " QLLD ") cc: %d '%s'\n", orig_size, size, char_len, rc,
            //    last_char, *p, p - str->c_str(), cc, enc->getCode());

            if (cc > 0) {
                // increment character count
                ++char_len;
                // increment byte position after last full character read
                last_char += cc;
                continue;
            }

            // otherwise we need to recalculate the total size to read and break
            cc = -cc;
            // how many bytes of this character do we have
            unsigned hb = (str->size() - last_char);
            assert((unsigned)cc > hb);
            // we will add one byte for the missing character below; here we add in any other bytes we might need
            if ((unsigned)cc > (hb + 1))
                size += (cc - hb - 1);

            break;
        }

        // now we add the minimum character byte length to the remaining size
        // for every character we have not yet read
        size = str->size() + ((orig_size - char_len) * mw);
    }

    return str->empty() ? 0 : str.release();
}

template <typename T>
T* q_fix_decimal_tmpl(T* str, size_t offset) {
    char* p = const_cast<char*>(strchr(str->c_str() + offset, ','));
    if (p)
        *p = '.';
    // concatentate ".0" to floating-point strings without a decimal point or an exponent
    if (!strchrs(str->c_str() + offset, ".e")) {
        str->concat(".0");
    }
    return str;
}

QoreString* q_fix_decimal(QoreString* str, size_t offset) {
    return q_fix_decimal_tmpl<QoreString>(str, offset);
}

QoreStringNode* q_fix_decimal(QoreStringNode* str, size_t offset) {
    return q_fix_decimal_tmpl<QoreStringNode>(str, offset);
}

bool q_libqore_shutdown() {
    return qore_shutdown.load(std::memory_order_relaxed);
}

bool q_libqore_initalized() {
    return qore_initialized.load(std::memory_order_relaxed);
}

bool q_libqore_exiting() {
    return qore_exiting.load(std::memory_order_relaxed);
}

QoreHashNode* q_get_thread_local_vars(int frame, ExceptionSink* xsink) {
    return thread_get_local_vars(frame, xsink);
}

int q_set_thread_var_value(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink) {
    int rc = thread_set_local_var_value(frame, name, val, xsink);

    if (rc == 1) {
        rc = thread_set_closure_var_value(frame, name, val, xsink);

        if (rc == 1) {
            xsink->raiseException("UNKNOWN-VARIABLE", "cannot find local variable '%s' in the current stack frame", name);
            rc = -1;
        }
    }

    return rc;
}

template<>
bool ThreadBlock<LocalVarValue>::frameBoundary(int p) {
    return var[p].frame_boundary;
}

template<>
bool ThreadBlock<ClosureVarValue*>::frameBoundary(int p) {
    return !(bool)var[p];
}

int q_get_data(const QoreValue& data, const char*& ptr, size_t& len) {
    switch (data.getType()) {
        case NT_STRING: {
            const QoreStringNode* str = data.get<const QoreStringNode>();
            ptr = str->getBuffer();
            len = str->size();
            return 0;
        }
        case NT_BINARY: {
            const BinaryNode* b = data.get<const BinaryNode>();
            ptr = (const char*)b->getPtr();
            len = b->size();
            return 0;
        }
    }
    return -1;
}

static const char* get_full_type_name(const AbstractQoreNode* n, bool with_namespaces, QoreString* scratch) {
    switch (get_node_type(n)) {
        case NT_HASH: {
            const qore_hash_private* h = qore_hash_private::get(*static_cast<const QoreHashNode*>(n));
            if (h->hashdecl) {
                return with_namespaces
                    ? QoreTypeInfo::getPath(h->hashdecl->getTypeInfo())
                    : QoreTypeInfo::getName(h->hashdecl->getTypeInfo());
            }
            if (h->complexTypeInfo) {
                return with_namespaces
                    ? QoreTypeInfo::getPath(h->complexTypeInfo)
                    : QoreTypeInfo::getName(h->complexTypeInfo);
            }
            break;
        }
        case NT_LIST: {
            const qore_list_private* l = qore_list_private::get(*static_cast<const QoreListNode*>(n));
            if (l->complexTypeInfo) {
                return with_namespaces
                    ? QoreTypeInfo::getPath(l->complexTypeInfo)
                    : QoreTypeInfo::getName(l->complexTypeInfo);
            }
            break;
        }
        case NT_OBJECT: {
            const char* clsname = with_namespaces
                ? QoreTypeInfo::getPath(static_cast<const QoreObject*>(n)->getClass()->getTypeInfo())
                : QoreTypeInfo::getName(static_cast<const QoreObject*>(n)->getClass()->getTypeInfo());
            if (!((QoreObject*)n)->isValid()) {
                if (scratch) {
                    scratch->sprintf("NOTHING (deleted object of class %s)", clsname);
                    return scratch->c_str();
                }
                return "NOTHING (deleted object)";
            }
            return clsname;
        }
        default:
            break;
    }
    return get_type_name(n);
}

const char* get_full_type_name(const AbstractQoreNode* n) {
    return get_full_type_name(n, false);
}

const char* get_full_type_name(const AbstractQoreNode* n, bool with_namespaces) {
    return get_full_type_name(n, with_namespaces, nullptr);
}

const char* get_full_type_name(const AbstractQoreNode* n, bool with_namespaces, QoreString& scratch) {
    return get_full_type_name(n, with_namespaces, &scratch);
}

void QorePossibleListNodeParseInitHelper::parseInit(QoreParseContext& parse_context) {
    //printd(5, "QoreListNodeParseInitHelper::parseInit() this: %p %d/%d (l: %p pl: %p)\n", this, l || pl ? pos : 0,
    //    (int)size(), l, pl);

    parse_context.typeInfo = nullptr;
    if (!l && !pl) {
        assert(finished);
        // FIXME: return list type info when list elements can be typed
        if (!pos) {
            if (singleTypeInfo) {
                parse_context.typeInfo = singleTypeInfo;
            }
        } else {
            // no argument available
            if (singleTypeInfo) {
                parse_context.typeInfo = nothingTypeInfo;
            }
        }
        return;
    }

    if (finished) {
        // no argument available
        parse_context.typeInfo = nothingTypeInfo;
    } else if (l) {
        assert(!pl);
        // return element type
        parse_context.typeInfo = qore_list_private::get(*l)->getEntryReference(pos).getFullTypeInfo();
    } else {
        assert(pl);
        assert(!l);
        // return element type
        parse_context.typeInfo = pl->get(pos).getFullTypeInfo();
    }
}

void qore_process_params(unsigned num_params, type_vec_t &typeList, arg_vec_t &defaultArgList, va_list args) {
    typeList.reserve(num_params);
    defaultArgList.reserve(num_params);
    for (unsigned i = 0; i < num_params; ++i) {
        typeList.push_back(va_arg(args, const QoreTypeInfo*));
        defaultArgList.push_back(va_arg(args, QoreSimpleValue));
        //printd(5, "qore_process_params() i: %d/%d typeInfo: %p (%s) defArg: %p\n", i, num_params, typeList[i], typeList[i]->getTypeName(), defaultArgList[i]);
    }
}

void qore_process_params(unsigned num_params, type_vec_t &typeList, arg_vec_t &defaultArgList, name_vec_t& nameList, va_list args) {
    typeList.reserve(num_params);
    defaultArgList.reserve(num_params);
    nameList.reserve(num_params);
    for (unsigned i = 0; i < num_params; ++i) {
        typeList.push_back(va_arg(args, const QoreTypeInfo *));
        defaultArgList.push_back(va_arg(args, QoreSimpleValue));
        nameList.push_back(va_arg(args, const char*));
        //printd(5, "qore_process_params() i: %d/%d typeInfo: %p (%s) defArg: %p\n", i, num_params, typeList[i], typeList[i]->getTypeName(), defaultArgList[i]);
    }
}

QoreHashNode* get_source_location(const QoreProgramLocation* loc) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclSourceLocationInfo, nullptr), nullptr);
    auto ph = qore_hash_private::get(**h);

    if (!loc) {
        loc = &loc_builtin;
        ph->setKeyValueIntern("builtin", true);
    }
    else {
        ph->setKeyValueIntern("builtin", false);
    }

    {
        const char* file = loc->getFile();
        if (file) {
            ph->setKeyValueIntern("file", new QoreStringNode(file));
        }
    }

    ph->setKeyValueIntern("line", loc->start_line);
    ph->setKeyValueIntern("endline", loc->end_line);

    {
        const char* source = loc->getSource();
        if (source) {
            ph->setKeyValueIntern("source", new QoreStringNode(source));
        }
    }

    ph->setKeyValueIntern("offset", loc->offset);

    return h.release();
}

const char* type_get_name(const QoreTypeInfo* t) {
    return QoreTypeInfo::getName(t);
}

void qore_delete_module_options() {
    if (mod_opt_map.empty()) {
        return;
    }
    ExceptionSink xsink;
    for (auto& i : mod_opt_map) {
        for (auto& vi : i.second) {
            vi.second.discard(&xsink);
        }
    }
}

void qore_set_module_option(std::string mod, std::string opt, QoreValue val) {
    AutoLocker al(mod_opt_lock);
    mod_opt_val_map_t::iterator vi;
    mod_opt_map_t::iterator i = mod_opt_map.lower_bound(mod);
    if (i == mod_opt_map.end() || i->first != mod) {
        i = mod_opt_map.insert(i, mod_opt_map_t::value_type(std::move(mod), mod_opt_val_map_t()));
        vi = i->second.end();
    } else {
        vi = i->second.lower_bound(opt);
    }

    if (vi == i->second.end() || vi->first != opt) {
        if (!val) {
            // do nothing; deleting a value that's not set
        } else {
            // set the new option value
            vi = i->second.insert(vi, mod_opt_val_map_t::value_type(std::move(opt), val));
        }
    } else {
        // first dereference the existing value
        ExceptionSink xsink;
        vi->second.discard(&xsink);
        if (!val) {
            // delete the option
            i->second.erase(vi);
        } else {
            // set the new value
            vi->second = val;
        }
    }
}

QoreValue qore_get_module_option(std::string mod, std::string opt) {
    AutoLocker al(mod_opt_lock);
    mod_opt_map_t::const_iterator i = mod_opt_map.find(mod);
    if (i == mod_opt_map.end()) {
        return QoreValue();
    }
    mod_opt_val_map_t::const_iterator vi = i->second.find(opt);
    return vi == i->second.end() ? QoreValue() : vi->second.refSelf();
}

const QoreTypeInfo* qore_get_type_from_string(const char* str, ExceptionSink& xsink) {
    ProgramRuntimeParseAccessHelper pah(&xsink, getProgram());
    if (xsink) {
        return nullptr;
    }
    const QoreTypeInfo* rv = qore_get_type_from_string_intern(str);
    if (!rv) {
        xsink.raiseException("UNKNOWN-TYPE", "cannot resolve '%s' to any known type", str);
    }
    return rv;
}

void qore_apply_rounding_heuristic(QoreString& str, int round_threshold_1, int round_threshold_2) {
    return qore_number_private::applyRoundingHeuristicToString(str, round_threshold_1, round_threshold_2);
}
