/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreLibIntern.h

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

#ifndef _QORE_QORELIBINTERN_H

#define _QORE_QORELIBINTERN_H

#include "qore/intern/config.h"

#include <atomic>
#include <cstdarg>
#include <sys/types.h>

#include <openssl/crypto.h>

#if defined(OPENSSL_VERSION_MAJOR) && OPENSSL_VERSION_MAJOR >= 3
#define OPENSSL_3_PLUS
#endif

#ifdef HAVE_SYS_STATVFS_H
#define Q_HAVE_STATVFS
#include <sys/statvfs.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <cinttypes>
#endif

#ifndef HAVE_STRCASESTR
extern char* strcasestr(const char* s1, const char* s2);
#endif

// make sure that intmax support is available from mpfr
#define MPFR_USE_INTMAX_T 1

// for arbitrary-precision numeric support
#include <mpfr.h>

// printf format for size_t or size_t integers
#if TARGET_BITS == 64
#define QSD QLLD
#else
#define QSD "%d"
#endif

#include <set>
#include <list>
#include <map>
#include <vector>

static inline bool isoctaldigit(const char x) {
    return x >= '0' && x <= '7';
}

// here we define virtual types
#define NT_NONE             -1
#define NT_ALL              -2
#define NT_CODE             -3
#define NT_SOFTINT          -4
#define NT_SOFTFLOAT        -5
#define NT_SOFTNUMBER       -6
#define NT_SOFTBOOLEAN      -7
#define NT_SOFTSTRING       -8
#define NT_SOFTDATE         -9
#define NT_SOFTLIST         -10
#define NT_TIMEOUT          -11
#define NT_INTORFLOAT       -12
#define NT_INTFLOATORNUMBER -13
#define NT_FLOATORNUMBER    -14
#define NT_SOFTBINARY       -15
#define NT_HEXBINARY        -16
#define NT_BASE64BINARY     -17
#define NT_BASE64URLBINARY  -18

#define NT_SOMETHING    -101 // i.e. "not NOTHING"
#define NT_DATA         -102 // either QoreStringNode or BinaryNode

typedef std::set<QoreObject*> obj_set_t;

// issue #3818: Qore SSL app-specific data index
DLLLOCAL extern int qore_ssl_data_index;

//! for object import APIs to set the new public / private flag
enum q_setpub_t : unsigned char {
    CSP_UNCHANGED = 0,
    CSP_SETPRIV = 1,
    CSP_SETPUB = 2,
};

struct QoreParseContext {
    QoreProgram* pgm;
    LocalVar* oflag = nullptr;
    const QoreClass* class_ctx = nullptr;
    int pflag = 0;
    int lvids = 0;
    const QoreTypeInfo* typeInfo = nullptr;

    DLLLOCAL QoreParseContext(QoreProgram* pgm = getProgram()) : pgm(pgm) {
    }

    DLLLOCAL QoreParseContext(LocalVar* oflag, QoreProgram* pgm = getProgram());

    DLLLOCAL QoreParseContext(const QoreClass* class_ctx, QoreProgram* pgm = getProgram()) : pgm(pgm),
            class_ctx(class_ctx) {
    }

    DLLLOCAL int unsetFlags(int flags) {
        int rv = pflag;
        pflag &= ~flags;
        return rv;
    }

    DLLLOCAL int setFlags(int flags) {
        int rv = pflag;
        pflag |= flags;
        return rv;
    }
};

class QoreParseContextFlagHelper {
public:
    DLLLOCAL QoreParseContextFlagHelper(QoreParseContext& parse_context) : parse_context(parse_context),
            pflag(parse_context.pflag) {
    }

    DLLLOCAL ~QoreParseContextFlagHelper() {
        if (parse_context.pflag != pflag) {
            parse_context.pflag = pflag;
        }
    }

    DLLLOCAL void unsetFlags(int flags) {
        parse_context.pflag &= ~flags;
    }

    DLLLOCAL void setFlags(int flags) {
        parse_context.pflag |= flags;
    }

private:
    QoreParseContext& parse_context;
    int pflag;
};

class QoreParseContextLvarHelper {
public:
    DLLLOCAL QoreParseContextLvarHelper(QoreParseContext& parse_context, LVList*& lvars)
            : parse_context(parse_context), lvars(lvars), lvids(parse_context.lvids) {
        parse_context.lvids = 0;
    }

    DLLLOCAL ~QoreParseContextLvarHelper();

private:
    QoreParseContext& parse_context;
    LVList*& lvars;
    int lvids;
};

//! returns -1 = error, 0 = OK
DLLLOCAL int parse_init_value(QoreValue& val, QoreParseContext& parse_context);

// since Qore 0.9.5
DLLLOCAL QoreValue q_call_static_method_args(QoreProgram* pgm, const QoreStringNode* class_name,
        const QoreStringNode* method, const QoreListNode* args, ExceptionSink* xsink);

// returns true if the node needs to be scanned for recursive references or not
DLLLOCAL bool needs_scan(const AbstractQoreNode* n);
DLLLOCAL bool needs_scan(const QoreValue& v);
// increments or decrements the object count depending on the sign of the argument (cannot be 0)
DLLLOCAL void inc_container_obj(const AbstractQoreNode* n, int dt);

DLLLOCAL AbstractQoreNode* missing_openssl_feature(const char* f, ExceptionSink* xsink);

struct ParseWarnOptions {
    int64 parse_options = 0;
    int warn_mask = 0;

    DLLLOCAL ParseWarnOptions() {
    }

    DLLLOCAL ParseWarnOptions(int64 n_parse_options, int n_warn_mask = 0)
            : parse_options(n_parse_options), warn_mask(n_warn_mask) {
    }

    DLLLOCAL void operator=(const ParseWarnOptions& pwo) {
        parse_options = pwo.parse_options;
        warn_mask = pwo.warn_mask;
    }

    DLLLOCAL bool operator==(const ParseWarnOptions& pwo) const {
        return parse_options == pwo.parse_options && warn_mask == pwo.warn_mask;
    }
};

struct QoreProgramLineLocation {
    int16_t start_line = -1,
        end_line = -1;

    // if sline is 0 and eline is > 0 then set sline to 1
    DLLLOCAL QoreProgramLineLocation(int sline, int eline) : start_line(sline ? sline : (eline ? 1 : 0)), end_line(eline) {
        assert(sline <= 0xffff);
        assert(eline <= 0xffff);
    }

    DLLLOCAL QoreProgramLineLocation() {
    }

    DLLLOCAL QoreProgramLineLocation(const QoreProgramLineLocation& old) = default;

    DLLLOCAL QoreProgramLineLocation(QoreProgramLineLocation&& old) = default;
};

struct QoreProgramLocation : public QoreProgramLineLocation {
public:
    // "blank" constructor
    DLLLOCAL QoreProgramLocation() {
    }

    DLLLOCAL explicit QoreProgramLocation(const char* f, int sline, int eline, const char* source, int offset,
        const char* lang = "Qore") :
        QoreProgramLineLocation(sline, eline), file(f), source(source), lang(lang), offset(offset) {
        assert(offset <= 0xffff);
    }

    DLLLOCAL explicit QoreProgramLocation(const char* f, int sline = 0, int eline = 0) :
        QoreProgramLineLocation(sline, eline), file(f) {
    }

    // sets file position info from thread-local parse information
    DLLLOCAL QoreProgramLocation(int sline, int eline);

    DLLLOCAL QoreProgramLocation(const QoreProgramLocation& old) = default;

    DLLLOCAL QoreProgramLocation(QoreProgramLocation&& old) = default;

    DLLLOCAL void clear() {
        start_line = end_line = -1;
        file = nullptr;
        source = nullptr;
        offset = 0;
    }

    DLLLOCAL void toString(QoreString& str) const;

    DLLLOCAL const char* getFile() const {
        return file;
    }

    DLLLOCAL const char* getFileValue() const {
        return file ? file : "";
    }

    DLLLOCAL const char* getSource() const {
        return source;
    }

    DLLLOCAL const char* getSourceValue() const {
        return source ? source : "";
    }

    DLLLOCAL const char* getLanguage() const {
        return lang;
    }

    DLLLOCAL const char* getLanguageValue() const {
        return lang ? lang : "";
    }

    DLLLOCAL void setFile(const char* f) {
        file = f;
    }

    DLLLOCAL void setSource(const char* s) {
        source = s;
    }

    DLLLOCAL void setLanguage(const char* l) {
        lang = l;
    }

    DLLLOCAL bool operator<(const QoreProgramLocation& loc) const {
        return start_line < loc.start_line
            || end_line < loc.end_line
            || file < loc.file
            || source < loc.source
            || offset < loc.offset
            || lang < loc.lang;
    }

    DLLLOCAL bool operator==(const QoreProgramLocation& loc) const {
        return start_line == loc.start_line
            && end_line == loc.end_line
            && file == loc.file
            && source == loc.source
            && offset == loc.offset
            && lang == loc.lang;
    }

    DLLLOCAL bool operator!=(const QoreProgramLocation& loc) const {
        return !(*this == loc);
    }

protected:
    const char* file = nullptr;
    const char* source = nullptr;
    const char* lang = "Qore";

public:
    int16_t offset = 0;
};

DLLLOCAL extern const QoreProgramLocation loc_builtin;

struct QoreCommandLineLocation : public QoreProgramLocation {
    DLLLOCAL QoreCommandLineLocation() : QoreProgramLocation("<command-line>", 1, 1) {
    }
};

// parse location for objects parsed on the command-line
DLLLOCAL extern QoreCommandLineLocation qoreCommandLineLocation;

// the following functions are implemented in support.cpp
DLLLOCAL void parse_error(const QoreProgramLocation& loc, const char* fmt, ...);
DLLLOCAL void parseException(const QoreProgramLocation& loc, const char* err, const char* fmt, ...);
DLLLOCAL void parseException(const QoreProgramLocation& loc, const char* err, QoreStringNode* desc);

DLLLOCAL QoreString* findFileInPath(const char* file, const char* path);
DLLLOCAL QoreString* findFileInEnvPath(const char* file, const char* varname);
DLLLOCAL int qore_find_file_in_path(QoreString& str, const char* file, const char* path);

DLLLOCAL const QoreTypeInfo* getBuiltinUserTypeInfo(const char* str);
DLLLOCAL const QoreTypeInfo* getBuiltinUserOrNothingTypeInfo(const char* str);
//DLLLOCAL qore_type_t getBuiltinType(const char* str);
DLLLOCAL const char* getBuiltinTypeName(qore_type_t type);

// processes parameter information
DLLLOCAL void qore_process_params(unsigned num_params, type_vec_t& typeList, arg_vec_t& defaultArgList, va_list args);
DLLLOCAL void qore_process_params(unsigned num_params, type_vec_t& typeList, arg_vec_t& defaultArgList, name_vec_t& nameList, va_list args);

// call to get a node with reference count 1 (copy on write)
void ensure_unique(AbstractQoreNode** v, ExceptionSink* xsink);
void ensure_unique(QoreValue& v, ExceptionSink* xsink);

#ifndef HAVE_ATOLL
#ifdef HAVE_STRTOIMAX
static inline long long atoll(const char* str) {
   return strtoimax(str, 0, 10);
}
#else
static inline long long atoll(const char* str) {
   long long i;
   sscanf(str, "%lld", &i);
   return i;
}
#endif
#endif

#if !defined(HAVE_STRTOLL) && defined(HAVE_STRTOIMAX)
#include <cinttypes>
#define strtoll strtoimax
#endif

// use umem for memory allocation if available
#ifdef HAVE_UMEM_H
#include <umem.h>
#endif

#ifdef HAVE_OPENSSL_CONST
#define OPENSSL_CONST const
#else
#define OPENSSL_CONST
#endif

#ifdef HAVE_X509_GET0_SIGNATURE_CONST
#define OPENSSL_X509_GET0_SIGNATURE_CONST const
#else
#define OPENSSL_X509_GET0_SIGNATURE_CONST
#endif

typedef std::set<const AbstractQoreNode*> const_node_set_t;
typedef std::set<LocalVar*> lvar_set_t;

class LVarSet : public lvar_set_t {
protected:
   // true if at least one variable in the set could contain an object or a closure (also through a container)
   bool needs_scan;

public:
   // creates the object
   DLLLOCAL LVarSet() : needs_scan(false) {
   }

   // adds a local variable to the set
   DLLLOCAL void add(LocalVar* var);

   // returns true if at least one variable in the set could contain an object or a closure (also through a container)
   DLLLOCAL bool needsScan() const {
      return needs_scan;
   }
};

enum obe_type_e { OBE_Unconditional, OBE_Success, OBE_Error };

class StatementBlock;
typedef std::pair<enum obe_type_e, StatementBlock*> qore_conditional_block_exit_statement_t;

typedef std::list<qore_conditional_block_exit_statement_t> block_list_t;

#if defined(HAVE_CHECK_STACK_POS)
#define QORE_MANAGE_STACK
#endif

// Datasource Access Helper codes
#define DAH_NOCHANGE  0 // acquire lock temporarily
#define DAH_ACQUIRE   1 // acquire lock and hold
#define DAH_RELEASE   2 // release lock at end of action
#define DAH_NOCONN    3 // acquire lock temporarily and do not make a connection

#define DAH_TEXT(d) (d == DAH_RELEASE ? "RELEASE" : (d == DAH_ACQUIRE ? "ACQUIRE" : "NOCHANGE"))

DLLLOCAL int check_lvalue(QoreValue n, bool assign = true);
DLLLOCAL int check_lvalue(AbstractQoreNode* n, bool assign = true);
DLLLOCAL int check_lvalue_int(const QoreProgramLocation* loc, const QoreTypeInfo*& typeInfo, const char* name);
DLLLOCAL int check_lvalue_float(const QoreProgramLocation* loc, const QoreTypeInfo*& typeInfo, const char* name);
DLLLOCAL int check_lvalue_int_float_number(const QoreProgramLocation* loc, const QoreTypeInfo*& typeInfo, const char* name);
DLLLOCAL int check_lvalue_number(const QoreProgramLocation* loc, const QoreTypeInfo*& typeInfo, const char* name);

DLLLOCAL extern QoreClass* QC_PSEUDOVALUE;
DLLLOCAL extern QoreClass* QC_PSEUDONOTHING;

DLLLOCAL bool node_has_effect(const AbstractQoreNode* n);

DLLLOCAL QoreString* q_fix_decimal(QoreString* str, size_t offset);
DLLLOCAL QoreStringNode* q_fix_decimal(QoreStringNode* str, size_t offset);

#ifdef _Q_WINDOWS
// simulated block size for statvfs() on Windows
#define Q_SVF_BSIZE 4096
#define Q_HAVE_STATVFS
struct statvfs {
    unsigned long   f_bsize;        /* File system block size */
    unsigned long   f_frsize;       /* Fundamental file system block size */
    unsigned int    f_blocks;       /* Blocks on FS in units of f_frsize */
    unsigned int    f_bfree;        /* Free blocks */
    unsigned int    f_bavail;       /* Blocks available to non-root */
    unsigned int    f_files;        /* Total inodes */
    unsigned int    f_ffree;        /* Free inodes */
    unsigned int    f_favail;       /* Free inodes for non-root */
    unsigned long   f_fsid;         /* Filesystem ID */
    unsigned long   f_flag;         /* Bit mask of values */
    unsigned long   f_namemax;      /* Max file name length */

    DLLLOCAL void set(int64 avail, int64 total, int64 free) {
        f_frsize = f_bsize = Q_SVF_BSIZE;
        f_blocks = total / Q_SVF_BSIZE;
        f_bfree = free / Q_SVF_BSIZE;
        f_bavail = avail / Q_SVF_BSIZE;
        // simulate inodes
        f_files = f_blocks / 8;
        f_ffree = f_bfree / 8;
        f_favail = f_bavail / 8;
        f_fsid = 0;
        f_flag = 0;
        f_namemax = 256;
    }
};
DLLLOCAL int statvfs(const char* path, struct statvfs* buf);
DLLLOCAL int q_fstatvfs(const char* filepath, struct statvfs* buf);
#endif

//! Helps dereference values outside of locks
class SafeDerefHelper {
public:
    DLLLOCAL SafeDerefHelper(ExceptionSink* xsink) : xsink(xsink) {
    }

    //! must be destroyed outside of any locks
    DLLLOCAL ~SafeDerefHelper() {
        if (value_list) {
            for (auto& i : *value_list) {
                i.discard(xsink);
            }
            delete value_list;
        }
    }

    //! dereferences the value immediately if it cannot throw an exception, or adds it to the list for dereferencing outside the lock
    DLLLOCAL void deref(QoreValue v) {
        if (v.derefCanThrowException()) {
            add(v);
        } else {
            v.discard(nullptr);
        }
    }

    //! adds a value for dereferencing on exit
    DLLLOCAL void add(QoreValue v) {
        if (!value_list) {
            value_list = new arg_vec_t;
        }
        value_list->push_back(v);
    }

protected:
    ExceptionSink* xsink;
    arg_vec_t* value_list = nullptr;
};

class QoreParseListNode;

class DeferredCodeObject {
public:
    DLLLOCAL virtual int parseInitDeferred() = 0;
};

#include "qore/intern/NamedScope.h"
#include "qore/intern/QoreTypeInfo.h"
#include "qore/intern/ParseNode.h"
#include "qore/intern/QoreThreadList.h"
#include "qore/intern/lvalue_ref.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/Function.h"
#include "qore/intern/CallReferenceCallNode.h"
#include "qore/intern/CallReferenceNode.h"
#include "qore/intern/BuiltinFunction.h"
#include "qore/intern/AbstractStatement.h"
#include "qore/intern/Variable.h"
#include "qore/intern/LocalVar.h"
#include "qore/intern/ScopedObjectCallNode.h"
#include "qore/intern/NewComplexTypeNode.h"
#include "qore/intern/ScopedRefNode.h"
#include "qore/intern/ClassRefNode.h"
#include "qore/intern/Context.h"
#include "qore/intern/BarewordNode.h"
#include "qore/intern/SelfVarrefNode.h"
#include "qore/intern/StaticClassVarRefNode.h"
#include "qore/intern/BackquoteNode.h"
#include "qore/intern/ContextrefNode.h"
#include "qore/intern/ContextRowNode.h"
#include "qore/intern/ComplexContextrefNode.h"
#include "qore/intern/FindNode.h"
#include "qore/intern/VRMutex.h"
#include "qore/intern/VLock.h"
#include "qore/intern/QoreException.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/QoreRegexSubst.h"
#include "qore/intern/QoreRegex.h"
#include "qore/intern/QoreTransliteration.h"
#include "qore/intern/ObjectMethodReferenceNode.h"
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/QoreClosureNode.h"
#include "qore/intern/QoreImplicitArgumentNode.h"
#include "qore/intern/QoreImplicitElementNode.h"
#include "qore/intern/QoreOperatorNode.h"
#include "qore/intern/QoreTimeZoneManager.h"
#include "qore/intern/ContextStatement.h"
#include "qore/intern/SwitchStatement.h"
#include "qore/intern/QorePseudoMethods.h"
#include "qore/intern/ParseReferenceNode.h"
#include "qore/intern/WeakReferenceNode.h"
#include "qore/intern/WeakHashReferenceNode.h"
#include "qore/intern/WeakListReferenceNode.h"
#include "qore/intern/QoreEllipsesNode.h"

DLLLOCAL extern std::atomic<bool> qore_initialized;
DLLLOCAL extern std::atomic<bool> qore_shutdown;
DLLLOCAL extern std::atomic<bool> qore_exiting;

DLLLOCAL extern int qore_library_options;

#ifndef HAVE_GETHOSTBYADDR_R
DLLLOCAL extern QoreThreadLock lck_gethostbyaddr;
#endif

DLLLOCAL extern qore_license_t qore_license;

#ifndef NET_BUFSIZE
#define NET_BUFSIZE      1024
#endif

#ifndef HOSTNAMEBUFSIZE
#define HOSTNAMEBUFSIZE 512
#endif

#ifndef HAVE_LOCALTIME_R
DLLLOCAL extern QoreThreadLock lck_localtime;
#endif

#ifndef HAVE_GMTIME_R
DLLLOCAL extern QoreThreadLock lck_gmtime;
#endif

DLLLOCAL extern char table64[64];

DLLLOCAL int get_nibble(char c, ExceptionSink* xsink);
DLLLOCAL BinaryNode* parseHex(const QoreProgramLocation* loc, const char* buf, int len);
DLLLOCAL void print_node(FILE* fp, const QoreValue qv);
DLLLOCAL void delete_global_variables();
DLLLOCAL void init_lib_intern(char* env[]);
DLLLOCAL QoreParseListNode* make_args(const QoreProgramLocation* loc, QoreValue arg);

DLLLOCAL QoreValue copy_value_and_resolve_lvar_refs(const QoreValue& n, ExceptionSink* xsink);

DLLLOCAL void init_qore_types();
DLLLOCAL void delete_qore_types();

DLLLOCAL QoreListNode* stat_to_list(const struct stat& sbuf);
DLLLOCAL QoreHashNode* stat_to_hash(const struct stat& sbuf, const TypedHashDecl* hd = hashdeclStatInfo);
DLLLOCAL QoreHashNode* statvfs_to_hash(const struct statvfs& statvfs);

// only called in stage 1 parsing: true means node requires run-time evaluation
//DLLLOCAL bool needsEval(AbstractQoreNode* n);

DLLLOCAL const char* check_hash_key(const QoreHashNode* h, const char* key, const char* err, ExceptionSink* xsink);

// class for master namespace of all builtin classes, constants, etc
class StaticSystemNamespace : public RootQoreNamespace {
public:
    DLLLOCAL StaticSystemNamespace();

    DLLLOCAL ~StaticSystemNamespace();
};

// master namespace of all builtin classes, constants, etc
DLLLOCAL extern StaticSystemNamespace* staticSystemNamespace;

class QoreParseListNodeParseInitHelper {
public:
    DLLLOCAL QoreParseListNodeParseInitHelper(QoreParseListNode* l) : l(l) {
    }

    DLLLOCAL QoreValue parseInit(QoreParseContext& parse_context) {
        //printd(5, "QoreListNodeParseInitHelper::parseInit() this=%p %d/%d (l=%p)\n", this, index(),
        //  getList()->size(), getList());

        parse_context.typeInfo = nullptr;
        QoreValue& n = l->getReference(pos);
        int pflag = parse_context.setFlags(PF_FOR_ASSIGNMENT);
        bool err = (bool)parse_init_value(n, parse_context);
        parse_context.pflag = pflag;
        if (err && !error) {
            error = true;
        }
        return n;
    }

    DLLLOCAL bool next() {
        ++pos;
        if (pos == (int)l->size()) {
            pos = -1;
            return false;
        }
        return true;
    }

    DLLLOCAL int index() {
        return pos;
    }

    DLLLOCAL bool hasError() const {
        return error;
    }

private:
    QoreParseListNode* l;
    int pos = -1;
    bool error = false;
};

class QorePossibleListNodeParseInitHelper {
public:
    DLLLOCAL QorePossibleListNodeParseInitHelper(QoreValue& n, QoreParseContext& parse_context) {
        // if the expression is not a list, then initialize it now
        // and save the return type
        parse_context.typeInfo = nullptr;
        error = parse_init_value(n, parse_context);
        //printd(5, "QorePossibleListNodeParseInitHelper::QorePossibleListNodeParseInitHelper() n: %s\n",
        //    n.getFullTypeName());
        if (n.getType() == NT_LIST) {
            l = n.get<QoreListNode>();
            finished = false;
        } else if (n.getType() == NT_PARSE_LIST) {
            pl = n.get<QoreParseListNode>();
            finished = false;
        } else {
            // set type info to 0 if the expression can return a list
            // FIXME: set list element type here when list elements can have types
            //printd(5, "singleTypeInfo: %s la: %d\n", n.getFullTypeName(),
            //    QoreTypeInfo::parseAccepts(listTypeInfo, singleTypeInfo));
            if (!QoreTypeInfo::parseAccepts(listTypeInfo, parse_context.typeInfo)) {
                singleTypeInfo = parse_context.typeInfo;
            }
        }
    }

    DLLLOCAL bool noArgument() const {
        return finished;
    }

    DLLLOCAL bool next() {
        if (finished) {
            return false;
        }

        if (++pos == sizeIntern()) {
            finished = true;
            return false;
        }
        return true;
    }

    DLLLOCAL size_t size() const {
        return finished ? 1 : sizeIntern();
    }

    DLLLOCAL void parseInit(QoreParseContext& parse_context);

    DLLLOCAL bool hasError() const {
        return error;
    }

private:
    QoreListNode* l = nullptr;
    QoreParseListNode* pl = nullptr;
    bool finished = true;
    size_t pos = -1;
    const QoreTypeInfo* singleTypeInfo = nullptr;
    bool error = false;

    DLLLOCAL size_t sizeIntern() const {
        return l ? l->size() : pl->size();
    }
};

DLLLOCAL void raise_nonexistent_method_call_warning(const QoreProgramLocation* loc, const QoreClass* qc,
        const char* method);

/*
class abstract_assignment_helper {
public:
   DLLLOCAL virtual AbstractQoreNode* swapImpl(AbstractQoreNode* v, ExceptionSink* xsink) = 0;
   DLLLOCAL virtual AbstractQoreNode* getValueImpl() const = 0;
};
*/

class qore_hash_private;
class qore_object_private;
class HashMember;

class hash_assignment_priv {
public:
    qore_hash_private& h;
    HashMember* om = nullptr;
    qore_object_private* o = nullptr;

    DLLLOCAL hash_assignment_priv(qore_hash_private& n_h, HashMember* n_om) : h(n_h), om(n_om) {
    }

    DLLLOCAL hash_assignment_priv(qore_hash_private& n_h, const char* key, bool must_already_exist = false,
            qore_object_private* obj = nullptr);

    DLLLOCAL hash_assignment_priv(QoreHashNode& n_h, const char* key, bool must_already_exist = false);

    DLLLOCAL hash_assignment_priv(QoreHashNode& n_h, const std::string &key, bool must_already_exist = false);

    DLLLOCAL hash_assignment_priv(ExceptionSink* xsink, QoreHashNode& n_h, const QoreString& key,
            bool must_already_exist = false);

    DLLLOCAL hash_assignment_priv(ExceptionSink* xsink, QoreHashNode& n_h, const QoreString* key,
            bool must_already_exist = false);

    DLLLOCAL void reassign(const char* key, bool must_already_exist = false);

    DLLLOCAL const char* getKey() const;

    DLLLOCAL QoreValue swapImpl(QoreValue v);

    DLLLOCAL QoreValue getImpl() const;

    DLLLOCAL QoreValue operator*() const {
        return getImpl();
    }

    DLLLOCAL bool exists() const {
        return (bool)om;
    }

    DLLLOCAL void assign(QoreValue v, ExceptionSink* xsink);

    DLLLOCAL void assign(QoreValue v, SafeDerefHelper& sdh, ExceptionSink* xsink);

    DLLLOCAL QoreValue swap(QoreValue v) {
        return swapImpl(v);
    }

    DLLLOCAL static hash_assignment_priv* get(HashAssignmentHelper& h) {
        return h.priv;
    }
};

DLLLOCAL void qore_machine_backtrace();

#ifndef QORE_THREAD_STACK_BLOCK
#define QORE_THREAD_STACK_BLOCK 64
#endif

template <typename T, int S1 = QORE_THREAD_STACK_BLOCK>
class ThreadBlock {
private:
    DLLLOCAL ThreadBlock(const ThreadBlock&);

public:
    T var[S1];
    int pos = 0;
    ThreadBlock<T, S1>* prev, * next = nullptr;

    DLLLOCAL ThreadBlock(ThreadBlock* prev = nullptr) : prev(prev) { }
    DLLLOCAL ~ThreadBlock() { }
    DLLLOCAL T& get(int p) {
        return var[p];
    }

    DLLLOCAL bool frameBoundary(int p);
};

template <typename T, int S1 = QORE_THREAD_STACK_BLOCK>
class ThreadLocalDataIterator {
    typedef ThreadLocalDataIterator<T, S1> self_t;

public:
    typedef ThreadBlock<T, S1> Block;

protected:
    Block* orig = nullptr, * curr = nullptr;
    int pos = 0;

public:
    DLLLOCAL ThreadLocalDataIterator(Block* n_orig)
        : orig(n_orig && n_orig->pos ? n_orig : nullptr) {
    }

    DLLLOCAL ThreadLocalDataIterator() {
    }

    DLLLOCAL bool next() {
        if (!orig) {
            return false;
        }

        do {
            if (!curr) {
                curr = orig;
                pos = orig->pos - 1;
            } else {
                --pos;
                if (pos < 0) {
                    if (!curr->prev) {
                        curr = 0;
                        pos = 0;
                        return false;
                    }
                    curr = curr->prev;
                    pos = curr->pos - 1;
                }
            }
            //printd(5, "ThreadLocalDataIterator::next() this: %p curr: %p pos: %d b: %d\n", this, curr, pos, curr->frameBoundary(pos));
        } while (curr->frameBoundary(pos));

        return true;
    }

    DLLLOCAL T& get() {
        assert(curr);
        return curr->get(pos);
    }
};

template <typename T, int S1 = QORE_THREAD_STACK_BLOCK>
class ThreadLocalData {
public:
    typedef ThreadBlock<T, S1> Block;
    typedef ThreadLocalDataIterator<T, S1> iterator;

    Block* curr;

    DLLLOCAL ThreadLocalData() {
        curr = new Block;
        //printf("this=%p: first curr=%p\n", this, curr);
    }

    DLLLOCAL ~ThreadLocalData() {
#ifdef DEBUG
        //if (curr->pos)
            //printf("~ThreadLocalData::~~ThreadLocalData() this=%p: del curr=%p pos=%d next=%p prev=%p\n", this, curr, curr->pos, curr->next, curr->prev);
#endif
        assert(!curr->prev);
        assert(!curr->pos);
        if (curr->next)
            delete curr->next;
        delete curr;
    }
#ifdef DEBUG
    DLLLOCAL bool empty() const {
        return (!curr->pos && !curr->prev);
    }
#endif

    DLLLOCAL int getFrameCount() const {
        return frame_count;
    }

protected:
    int frame_count = -1;

private:
    DLLLOCAL ThreadLocalData(const ThreadLocalData&);
};

// maps from Q_AF_* to standard system AF_ constants
DLLLOCAL int q_get_af(int type);
// maps from AF_* to Q_AF_ constants
DLLLOCAL int q_get_raf(int type);
// maps from Q_SOCK_ to standard system SOCK_ constants
DLLLOCAL int q_get_sock_type(int t);

class OptHashRefHelper {
    const ReferenceNode* ref;
    ExceptionSink* xsink;
    ReferenceHolder<QoreHashNode> info;

public:
    DLLLOCAL OptHashRefHelper(QoreListNode* args, unsigned i, ExceptionSink* n_xsink) : ref(test_reference_param(args, i)), xsink(n_xsink), info(ref ? new QoreHashNode(autoTypeInfo) : 0, xsink) {
    }
    DLLLOCAL OptHashRefHelper(const ReferenceNode* n_ref, ExceptionSink* n_xsink) : ref(n_ref), xsink(n_xsink), info(ref ? new QoreHashNode(autoTypeInfo) : 0, xsink) {
    }
    DLLLOCAL ~OptHashRefHelper() {
        if (!ref)
            return;

        ExceptionSink xs;
        QoreTypeSafeReferenceHelper rh(ref, &xs);
        if (xs)
            xsink->assimilate(xs);
        if (!rh)
            return;

        rh.assign(info.release());
        if (xs)
            xsink->assimilate(xs);
    }
    DLLLOCAL QoreHashNode* operator*() {
        return *info;
    }
};

// pushes a marker on the local variable parse stack so that searches can skip to global thread-local variables when the search hits the marker
class VariableBlockHelper {
public:
    DLLLOCAL VariableBlockHelper();
    DLLLOCAL ~VariableBlockHelper();
};

class ParseOptionMaps {
protected:
    DLLLOCAL void doMap(int64 code, const char* desc, const char* dom = nullptr);

public:
    typedef std::map<int64, const char*> pomap_t;
    typedef std::map<const char*, int64, ltstr> pormap_t;

    pomap_t pomap, dommap;
    pormap_t pormap, domrmap;

    DLLLOCAL ParseOptionMaps();

    DLLLOCAL QoreHashNode* getCodeToStringMap() const;
    DLLLOCAL QoreHashNode* getStringToCodeMap() const;

    DLLLOCAL QoreHashNode* getDomainToStringMap() const;
    DLLLOCAL QoreHashNode* getStringToDomainMap() const;
};

DLLLOCAL extern ParseOptionMaps pomaps;

DLLLOCAL extern QoreString YamlNullString;

DLLLOCAL extern bool q_disable_gc;

DLLLOCAL QoreValue qore_parse_get_define_value(const QoreProgramLocation* loc, const char* str, QoreString& arg,
    bool& ok);

#ifndef HAVE_INET_NTOP
DLLLOCAL const char* inet_ntop(int af, const void* src, char* dst, size_t size);
#endif
#ifndef HAVE_INET_PTON
DLLLOCAL int inet_pton(int af, const char* src, void* dst);
#endif

DLLLOCAL AbstractQoreNode* missing_function_error(const char* func, ExceptionSink* xsink);
DLLLOCAL AbstractQoreNode* missing_function_error(const char* func, const char* opt, ExceptionSink* xsink);
DLLLOCAL AbstractQoreNode* missing_method_error(const char* meth, const char* opt, ExceptionSink* xsink);

// checks for illegal "self" assignments in an object context
DLLLOCAL int check_self_assignment(const QoreProgramLocation* loc, QoreValue n, LocalVar* selfid);

DLLLOCAL void ignore_return_value(QoreSimpleValue& n);

DLLLOCAL void qore_string_init();

DLLLOCAL QoreListNode* split_regex_intern(QoreRegex& regex, const char* str, size_t sl, const QoreEncoding* enc,
    bool with_separator = false);
DLLLOCAL QoreListNode* split_intern(const char* pattern, size_t pl, const char* str, size_t sl,
    const QoreEncoding* enc, bool with_separator = false);
DLLLOCAL QoreStringNode* join_intern(const QoreStringNode* p0, const QoreListNode* l, int offset,
    ExceptionSink* xsink);
DLLLOCAL QoreListNode* split_with_quote(ExceptionSink* xsink, const char* sep, size_t seplen, const QoreString* str,
    const char* quote, size_t quotelen, bool trim_unquoted, AbstractIteratorHelper* h = nullptr,
    const QoreString* eol = nullptr);
DLLLOCAL QoreListNode* split_with_quote(ExceptionSink* xsink, const QoreString* sep, const QoreString* str,
    const QoreString* quote, bool trim_unquoted, AbstractIteratorHelper* h = nullptr,
    const QoreString* eol = nullptr);
DLLLOCAL bool inlist_intern(const QoreValue arg, const QoreListNode* l, ExceptionSink* xsink);
DLLLOCAL QoreStringNode* format_float_intern(const QoreString& fmt, double num, ExceptionSink* xsink);
DLLLOCAL QoreStringNode* format_float_intern(int prec, const QoreString& dsep, const QoreString& tsep, double num,
    ExceptionSink* xsink);
DLLLOCAL DateTimeNode* make_date_with_mask(const AbstractQoreZoneInfo* tz, const QoreString& dtstr,
    const QoreString& mask, ExceptionSink* xsink);
DLLLOCAL QoreHashNode* date_info(const DateTime& d);
DLLLOCAL void init_charmaps();
DLLLOCAL int do_unaccent(QoreString& str, const QoreString& src, ExceptionSink* xsink);
DLLLOCAL int do_unaccent(QoreString& str, const QoreString& src, ExceptionSink* xsink);
DLLLOCAL int do_tolower(QoreString& str, const QoreString& src, ExceptionSink* xsink);
DLLLOCAL int do_toupper(QoreString& str, const QoreString& src, ExceptionSink* xsink);

DLLLOCAL int64 q_clock_getmillis();
DLLLOCAL int64 q_clock_getmicros();
DLLLOCAL int64 q_clock_getnanos();

DLLLOCAL void qore_init_random_salt();
DLLLOCAL int qore_get_ptr_hash(QoreString& str, const void* ptr);

// find the position of the first path separator in the string, or 0
DLLLOCAL const char* q_find_first_path_sep(const char* path);
// find the position of the last path separator in the string, or 0
DLLLOCAL const char* q_find_last_path_sep(const char* path);

// reutrns the given file's mode or 0 if the stat() call fails
DLLLOCAL int q_get_mode(const QoreString& path);
//! returns the byte length of the next UTF-8 character or 0 for an encoding error or a negative number if the string is too short to represent the character
DLLLOCAL qore_offset_t q_UTF8_get_char_len(const char* p, size_t valid_len);

//! returns the byte length of the next UTF-16 (big-endian encoded) character or 0 for an encoding error or a negative number if the string is too short to represent the character
DLLLOCAL qore_offset_t q_UTF16BE_get_char_len(const char* p, size_t valid_len);
DLLLOCAL qore_offset_t q_UTF16LE_get_char_len(const char* p, size_t len);

DLLLOCAL int64 get_ms_zero(const QoreValue& v);

DLLLOCAL AbstractQoreNode* copy_strip_complex_types(const AbstractQoreNode* n);
DLLLOCAL QoreValue copy_strip_complex_types(const QoreValue& n);

// for IPv4/v6 only
DLLLOCAL void* qore_get_in_addr(struct sockaddr *sa);
// for IPv4/v6 only
DLLLOCAL size_t qore_get_in_len(struct sockaddr *sa);

DLLLOCAL QoreHashNode* get_source_location(const QoreProgramLocation* loc);

DLLLOCAL void qore_delete_module_options();

DLLLOCAL const QoreTypeInfo* qore_get_type_from_string_intern(const char* str);

DLLLOCAL QoreValue get_call_reference_intern(QoreObject* self, const QoreStringNode* identifier, ExceptionSink* xsink);

// initialize thread-local storage
DLLLOCAL void qore_thread_local_storage_init();

// destroy thread-local storage
DLLLOCAL void qore_thread_local_storage_destroy();

#endif
