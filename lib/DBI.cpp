/*
    DBI.cpp

    Database Independent SQL Layer

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

#include "qore/intern/qore_dbi_private.h"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

typedef std::map<std::string, std::string> strstrmap_t;
strstrmap_t driver_mod_map = {
    {"jdbc", "jni"},
};

typedef safe_dslist<DBIDriver* > dbi_list_t;

// global qore library class for DBI driver management
DBIDriverList DBI;

struct dbi_cap_hash {
   int cap;
   const char* desc;
};

struct dbi_cap_hash dbi_cap_list[] =
{ { DBI_CAP_TIME_ZONE_SUPPORT,      "TimeZoneSupport" },
  { DBI_CAP_CHARSET_SUPPORT,        "CharsetSupport" },
  { DBI_CAP_TRANSACTION_MANAGEMENT, "TransactionManagement" },
  { DBI_CAP_STORED_PROCEDURES,      "StoredProcedures" },
  { DBI_CAP_LOB_SUPPORT,            "LargeObjectSupport" },
  { DBI_CAP_BIND_BY_VALUE,          "BindByValue" },
  { DBI_CAP_BIND_BY_PLACEHOLDER,    "BindByPlaceholder" },
  { DBI_CAP_HAS_EXECRAW,            "HasExecRaw" },
  { DBI_CAP_HAS_STATEMENT,          "HasStatementApi" },
  { DBI_CAP_HAS_SELECT_ROW,         "HasSelectRow" },
  { DBI_CAP_HAS_NUMBER_SUPPORT,     "HasNumberSupport" },
  { DBI_CAP_HAS_OPTION_SUPPORT,     "HasOptionSupport" },
  { DBI_CAP_SERVER_TIME_ZONE,       "ServerTimeZone" },
  { DBI_CAP_AUTORECONNECT,          "AutoReconnect" },
  { DBI_CAP_EVENTS,                 "Events" },
  { DBI_CAP_HAS_DESCRIBE,           "HasDescribe" },
  { DBI_CAP_HAS_ARRAY_BIND,         "HasArrayBind" },
  { DBI_CAP_HAS_RESULTSET_OUTPUT,   "HasResultsetOutput" },
};

#define NUM_DBI_CAPS (sizeof(dbi_cap_list) / sizeof(dbi_cap_hash))

struct qore_dbi_mlist_private {
    dbi_method_list_t l;
    dbi_opt_map_t omap;

    DLLLOCAL void registerOption(const char* name, const char* desc, const QoreTypeInfo* type = 0) {
        assert(omap.find(name) == omap.end());
        omap[name] = DbiOptInfo(desc, type);
    }

    DLLLOCAL static qore_dbi_mlist_private* get(const qore_dbi_method_list& ml) {
        return ml.priv;
    }
};

qore_dbi_method_list::qore_dbi_method_list() : priv(new qore_dbi_mlist_private) {
}

qore_dbi_method_list::~qore_dbi_method_list() {
    delete priv;
}

// covers open, commit, rollback, and begin transaction
void qore_dbi_method_list::add(int code, q_dbi_open_t method) {
    assert(code == QDBI_METHOD_OPEN
        || code == QDBI_METHOD_COMMIT
        || code == QDBI_METHOD_ROLLBACK
        || code == QDBI_METHOD_BEGIN_TRANSACTION
    );
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// for close
void qore_dbi_method_list::add(int code, q_dbi_close_t method) {
    assert(code == QDBI_METHOD_CLOSE);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers select, select_rows, and exec
void qore_dbi_method_list::add(int code, q_dbi_select_t method) {
    assert(code == QDBI_METHOD_SELECT || code == QDBI_METHOD_SELECT_ROWS || code == QDBI_METHOD_EXEC || code == QDBI_METHOD_DESCRIBE);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers select_row
void qore_dbi_method_list::add(int code, q_dbi_select_row_t method) {
    assert(code == QDBI_METHOD_SELECT_ROW);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers execRaw
void qore_dbi_method_list::add(int code, q_dbi_execraw_t method) {
    assert(code == QDBI_METHOD_EXECRAW);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers get_server_version
void qore_dbi_method_list::add(int code, q_dbi_get_server_version_t method) {
    assert(code == QDBI_METHOD_GET_SERVER_VERSION);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers get_client_version
void qore_dbi_method_list::add(int code, q_dbi_get_client_version_t method) {
    assert(code == QDBI_METHOD_GET_CLIENT_VERSION);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers get_driver_real_name
void qore_dbi_method_list::add(int code, q_dbi_get_driver_real_name_t method) {
    assert(code == QDBI_METHOD_GET_DRIVER_REAL_NAME);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers stmt prepare
void qore_dbi_method_list::add(int code, q_dbi_stmt_prepare_t method) {
    assert(code == QDBI_METHOD_STMT_PREPARE);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers stmt prepare_raw
void qore_dbi_method_list::add(int code, q_dbi_stmt_prepare_raw_t method) {
    assert(code == QDBI_METHOD_STMT_PREPARE_RAW);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers stmt bind
void qore_dbi_method_list::add(int code, q_dbi_stmt_bind_t method) {
    assert(code == QDBI_METHOD_STMT_BIND || code == QDBI_METHOD_STMT_BIND_PLACEHOLDERS
        || code == QDBI_METHOD_STMT_BIND_VALUES);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers stmt exec, close, define, and affectedRows
void qore_dbi_method_list::add(int code, q_dbi_stmt_exec_t method) {
    assert(code == QDBI_METHOD_STMT_EXEC || code == QDBI_METHOD_STMT_CLOSE || code == QDBI_METHOD_STMT_DEFINE
        || code == QDBI_METHOD_STMT_AFFECTED_ROWS || code == QDBI_METHOD_STMT_FREE
        || code == QDBI_METHOD_STMT_EXEC_DESCRIBE);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers stmt fetch_row, get_output and get_output_rows
void qore_dbi_method_list::add(int code, q_dbi_stmt_fetch_row_t method) {
    assert(code == QDBI_METHOD_STMT_FETCH_ROW || code == QDBI_METHOD_STMT_GET_OUTPUT_ROWS
        || code == QDBI_METHOD_STMT_GET_OUTPUT || code == QDBI_METHOD_STMT_DESCRIBE);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers stmt fetch_rows
void qore_dbi_method_list::add(int code, q_dbi_stmt_fetch_rows_t method) {
    assert(code == QDBI_METHOD_STMT_FETCH_ROWS);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers stmt fetch_columns
void qore_dbi_method_list::add(int code, q_dbi_stmt_fetch_columns_t method) {
    assert(code == QDBI_METHOD_STMT_FETCH_COLUMNS);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

// covers stmt next
void qore_dbi_method_list::add(int code, q_dbi_stmt_next_t method) {
    assert(code == QDBI_METHOD_STMT_NEXT);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

void qore_dbi_method_list::add(int code, q_dbi_option_set_t method) {
    assert(code == QDBI_METHOD_OPT_SET);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

void qore_dbi_method_list::add(int code, q_dbi_option_get_t method) {
    assert(code == QDBI_METHOD_OPT_GET);
    assert(priv->l.find(code) == priv->l.end());
    priv->l[code] = (void*)method;
}

void qore_dbi_method_list::registerOption(const char* name, const char* desc, const QoreTypeInfo* type) {
    priv->registerOption(name, desc, type);
}

DbiArgHelper::DbiArgHelper(const QoreListNode* ol, bool numeric, ExceptionSink* xs) : orig(ol), nl(0), xsink(xs) {
    if (numeric || !orig)
        return;

    // scan input list
    ConstListIterator li(orig);
    while (li.next()) {
        if (li.getValue().getType() == NT_NUMBER) {
            if (!nl) {
                nl = new QoreListNode(autoTypeInfo);
                for (unsigned i = 0; i < li.index(); ++i) {
                    nl->push(orig->getReferencedEntry(i), xs);
                }
            }
            nl->push(li.getValue().getAsFloat(), xs);
            continue;
        }
        if (nl)
            nl->push(li.getReferencedValue(), xs);
    }
}

OptInputHelper::OptInputHelper(ExceptionSink* xs, const qore_dbi_private& driver, const char* opt, bool set,
        const QoreValue v) : xsink(xs), val(v), tmp(false) {
    dbi_opt_map_t::const_iterator i = driver.omap.find(opt);
    if (i == driver.omap.end()) {
        xsink->raiseException("DBI-OPTION-ERROR", "driver '%s' does not support option '%s'", driver.name, opt);
        return;
    }
    if (!set) {
        return;
    }
    const QoreTypeInfo* ti = i->second.typeInfo;
    if (!QoreTypeInfo::mayRequireFilter(ti, v)) {
        return;
    }
    tmp = true;
    val.ref();
    QoreTypeInfo::acceptInputParam(ti, -1, "<dbi driver option>", val, xsink);
}

qore_dbi_private::qore_dbi_private(const char* nme, const qore_dbi_mlist_private& methods, int cps)
        : name(nme), caps(cps){
    // add methods to internal data structure
    for (dbi_method_list_t::const_iterator i = methods.l.begin(), e = methods.l.end(); i != e; ++i) {
        assert((*i).first > 0 && (*i).first <= QDBI_VALID_CODES);
        switch ((*i).first) {
            case QDBI_METHOD_OPEN:
                assert(!f.open);
                f.open = (q_dbi_open_t)(*i).second;
                break;
            case QDBI_METHOD_CLOSE:
                assert(!f.close);
                f.close = (q_dbi_close_t)(*i).second;
                break;
            case QDBI_METHOD_SELECT:
                assert(!f.select);
                f.select = (q_dbi_select_t)(*i).second;
                break;
            case QDBI_METHOD_SELECT_ROWS:
                assert(!f.selectRows);
                f.selectRows = (q_dbi_select_rows_t)(*i).second;
                break;
            case QDBI_METHOD_SELECT_ROW:
                assert(!f.selectRow);
                f.selectRow = (q_dbi_select_row_t)(*i).second;
                cps |= DBI_CAP_HAS_SELECT_ROW;
                break;
            case QDBI_METHOD_EXEC:
                assert(!f.execSQL);
                f.execSQL = (q_dbi_exec_t)(*i).second;
                break;
            case QDBI_METHOD_EXECRAW:
                assert(!f.execRawSQL);
                f.execRawSQL = (q_dbi_execraw_t)(*i).second;
                cps |= DBI_CAP_HAS_EXECRAW;
                break;
            case QDBI_METHOD_DESCRIBE:
                assert(!f.describe);
                f.describe = (q_dbi_describe_t)(*i).second;
                cps |= DBI_CAP_HAS_DESCRIBE;
                break;
            case QDBI_METHOD_COMMIT:
                assert(!f.commit);
                f.commit = (q_dbi_commit_t)(*i).second;
                break;
            case QDBI_METHOD_ROLLBACK:
                assert(!f.rollback);
                f.rollback = (q_dbi_rollback_t)(*i).second;
                break;
            case QDBI_METHOD_BEGIN_TRANSACTION:
                assert(!f.begin_transaction);
                f.begin_transaction = (q_dbi_begin_transaction_t)(*i).second;
                break;
            case QDBI_METHOD_GET_SERVER_VERSION:
                assert(!f.get_server_version);
                f.get_server_version = (q_dbi_get_server_version_t)(*i).second;
                break;
            case QDBI_METHOD_GET_CLIENT_VERSION:
                assert(!f.get_client_version);
                f.get_client_version = (q_dbi_get_client_version_t)(*i).second;
                break;
            case QDBI_METHOD_GET_DRIVER_REAL_NAME:
                assert(!f.get_driver_real_name);
                f.get_driver_real_name = (q_dbi_get_driver_real_name_t)(*i).second;
                break;

            case QDBI_METHOD_STMT_PREPARE:
                assert(!f.stmt.prepare);
                f.stmt.prepare = (q_dbi_stmt_prepare_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_PREPARE_RAW:
                assert(!f.stmt.prepare_raw);
                f.stmt.prepare_raw = (q_dbi_stmt_prepare_raw_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_BIND:
                assert(!f.stmt.bind);
                f.stmt.bind = (q_dbi_stmt_bind_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_BIND_PLACEHOLDERS:
                assert(!f.stmt.bind_placeholders);
                f.stmt.bind_placeholders = (q_dbi_stmt_bind_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_BIND_VALUES:
                assert(!f.stmt.bind_values);
                f.stmt.bind_values = (q_dbi_stmt_bind_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_EXEC_DESCRIBE:
                assert(!f.stmt.exec_describe);
                f.stmt.exec_describe = (q_dbi_stmt_exec_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_EXEC:
                assert(!f.stmt.exec);
                f.stmt.exec = (q_dbi_stmt_exec_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_FETCH_ROW:
                assert(!f.stmt.fetch_row);
                f.stmt.fetch_row = (q_dbi_stmt_fetch_row_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_FETCH_ROWS:
                assert(!f.stmt.fetch_rows);
                f.stmt.fetch_rows = (q_dbi_stmt_fetch_rows_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_FETCH_COLUMNS:
                assert(!f.stmt.fetch_columns);
                f.stmt.fetch_columns = (q_dbi_stmt_fetch_columns_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_DESCRIBE:
                assert(!f.stmt.describe);
                f.stmt.describe = (q_dbi_stmt_fetch_row_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_NEXT:
                assert(!f.stmt.next);
                f.stmt.next = (q_dbi_stmt_next_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_DEFINE:
                assert(!f.stmt.define);
                f.stmt.define = (q_dbi_stmt_define_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_CLOSE:
                assert(!f.stmt.close);
                f.stmt.close = (q_dbi_stmt_close_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_FREE:
                assert(!f.stmt.free);
                f.stmt.free = (q_dbi_stmt_close_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_AFFECTED_ROWS:
                assert(!f.stmt.affected_rows);
                f.stmt.affected_rows = (q_dbi_stmt_affected_rows_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_GET_OUTPUT:
                assert(!f.stmt.get_output);
                f.stmt.get_output = (q_dbi_stmt_get_output_t)(*i).second;
                break;
            case QDBI_METHOD_STMT_GET_OUTPUT_ROWS:
                assert(!f.stmt.get_output_rows);
                f.stmt.get_output_rows = (q_dbi_stmt_get_output_rows_t)(*i).second;
                break;

            case QDBI_METHOD_OPT_SET:
                assert(!f.opt.set);
                f.opt.set = (q_dbi_option_set_t)(*i).second;
                break;
            case QDBI_METHOD_OPT_GET:
                assert(!f.opt.get);
                f.opt.get = (q_dbi_option_get_t)(*i).second;
                break;

#ifdef DEBUG
            default:
                assert(false);
#endif
        }
    }
    // ensure minimum methods are defined
    assert(f.open);
    assert(f.close);
    assert(f.select);
    assert(f.selectRows);
    assert(f.execSQL);
    assert(f.commit);
    assert(f.rollback);

    // ensure either no or minimum stmt methods are defined
    assert(!f.stmt.prepare || (f.stmt.prepare_raw && f.stmt.bind && f.stmt.bind_values
            && f.stmt.exec && f.stmt.fetch_row && f.stmt.fetch_rows
            && f.stmt.fetch_columns && f.stmt.next && f.stmt.close
            && f.stmt.affected_rows && f.stmt.get_output
            && f.stmt.get_output_rows && f.stmt.define));

    // ensure either no or minimum opt methods are defined
    assert(!f.opt.set || (f.opt.set && f.opt.get));

    if (f.stmt.prepare) {
        caps |= DBI_CAP_HAS_STATEMENT;
#ifdef DEBUG
    } else {
        assert(!(caps & DBI_CAP_HAS_STATEMENT));
#endif
    }

    if (f.opt.set) {
        caps |= DBI_CAP_HAS_OPTION_SUPPORT;
    }

    // copy options
    omap = methods.omap;
}

QoreListNode* qore_dbi_private::getCapList() const {
    QoreListNode* l = new QoreListNode(stringTypeInfo);
    for (unsigned i = 0; i < NUM_DBI_CAPS; ++i) {
        if (caps & dbi_cap_list[i].cap) {
            l->push(new QoreStringNode(dbi_cap_list[i].desc), nullptr);
        }
    }
    return l;
}

DBIDriver::DBIDriver(qore_dbi_private* p) : priv(p) {
}

DBIDriver::~DBIDriver() {
    delete priv;
}

const char* DBIDriver::getName() const {
    return priv->name;
}

bool DBIDriver::hasStatementAPI() const {
    return priv->hasStatementAPI();
}

QoreHashNode* DBIDriver::getOptionHash() const {
    return priv->getOptionHash();
}

/* it's not necessary to lock this object because it will only be written to in one thread at a time
   (within the module lock)
   note that a safe_dslist is used because it can be safely read in multiple threads while
   being written to (in the lock).  The list should never be that long so the penalty for searching
   a linked list with strcmp() against using a hash with explicit locking around all searches
   should be acceptable...
*/
struct qore_dbi_dlist_private {
    dbi_list_t l;

    DLLLOCAL ~qore_dbi_dlist_private() {
        dbi_list_t::iterator i;
        while ((i = l.begin()) != l.end()) {
            DBIDriver* driv = *i;
            l.erase(i);
            delete driv;
        }
    }

    DLLLOCAL DBIDriver* find_intern(const char* name) const {
        for (dbi_list_t::const_iterator i = l.begin(); i != l.end(); ++i) {
            if (!strcmp(name, (*i)->getName())) {
                return *i;
            }
        }

        return nullptr;
    }

    DLLLOCAL QoreListNode* getDriverList() const {
        if (l.empty()) {
            return nullptr;
        }

        QoreListNode* lst = new QoreListNode(stringTypeInfo);

        for (dbi_list_t::const_iterator i = l.begin(); i != l.end(); ++i) {
            lst->push(new QoreStringNode((*i)->getName()), nullptr);
        }

        return lst;
    }
};

DBIDriverList::DBIDriverList() : priv(new qore_dbi_dlist_private) {
}

DBIDriverList::~DBIDriverList() {
    delete priv;
}

DBIDriver* DBIDriverList::find_intern(const char* name) const {
    return priv->find_intern(name);
}

DBIDriver* DBIDriverList::find(const char* name) const {
    // try to load the driver if it doesn't exist
    // ignore any exceptions
    ExceptionSink xs;
    DBIDriver* d = find(name, &xs);
    xs.clear();
    return d;
}

DBIDriver* DBIDriverList::find(const char* name, ExceptionSink* xsink) const {
    DBIDriver* d = priv->find_intern(name);
    if (d) {
        return d;
    }

    const char* dbi_name = name;
    // try to load the driver if it doesn't exist
    // see if the module name differs from the DBI driver name
    strstrmap_t::iterator i = driver_mod_map.find(name);
    if (i != driver_mod_map.end()) {
        name = i->second.c_str();
    }
    if (MM.runTimeLoadModule(name, xsink)) {
        return nullptr;
    }

    return priv->find_intern(dbi_name);
}

DBIDriver* DBIDriverList::registerDriver(const char* name, const qore_dbi_method_list& methods, int caps) {
    assert(!priv->find_intern(name));

    DBIDriver* dd = new DBIDriver(new qore_dbi_private(name, *qore_dbi_mlist_private::get(methods), caps));
    priv->l.push_back(dd);
    return dd;
}

QoreListNode* DBIDriverList::getDriverList() const {
    return priv->getDriverList();
}

void DBI_concat_numeric(QoreString* str, QoreValue v) {
    if (v.isNullOrNothing()) {
        str->concat("null");
        return;
    }

    qore_type_t t = v.getType();
    if (t == NT_FLOAT || (t == NT_STRING && strchr((v.get<const QoreStringNode>())->c_str(), '.'))) {
        size_t offset = str->size();
        str->sprintf("%g", v.getAsFloat());
        // issue 1556: external modules that call setlocale() can change
        // the decimal point character used here from '.' to ','
        // only search the double added, QoreString::sprintf() concatenates
        q_fix_decimal(str, offset);
        return;
    } else if (t == NT_NUMBER) {
        v.get<const QoreNumberNode>()->getStringRepresentation(*str);
        return;
    }
    str->sprintf("%lld", v.getAsBigInt());
}

int DBI_concat_string(QoreString* str, QoreValue v, ExceptionSink* xsink) {
    assert(xsink);
    if (v.isNullOrNothing()) {
        return 0;
    }

    QoreStringValueHelper tstr(v, str->getEncoding(), xsink);
    if (*xsink) {
        return -1;
    }

    str->concat(*tstr, xsink);
    return *xsink;
}

static char* check_datasource_chars(char* str, const char* chars) {
    char* p;
    char* str0 = str;
    while (true) {
        // get dbname
        p = strchrs(str0, chars);
        if (!p) {
            break;
        }
        // rule out invalid matches with a ':' with non-digits following
        if ((*p == ':') && !isdigit(*(p + 1))) {
            str0 = p + 1;
            continue;
        }
        break;
    }
    return p;
}

/*
  parseDatasource()
  parses strings of the form: driver:user/pass@db(encoding)%host:post{option1,option2=value}
  everything except "@db" is optional
*/
QoreHashNode* parseDatasource(const char* ds, ExceptionSink* xsink) {
    static const char* DATASOURCE_PARSE_ERROR = "DATASOURCE-PARSE-ERROR";

    if (!ds || !ds[0]) {
        xsink->raiseException(DATASOURCE_PARSE_ERROR, "empty text passed to parseDatasource()");
        return nullptr;
    }

    QoreStringNode* driver = nullptr;
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);

    // use a QoreString to create a temporary buffer
    QoreString tmp(ds);
    tmp.trim();
    char* str = const_cast<char*>(tmp.c_str());

    // first remove the option substring from the string, if any
    if (tmp.size() > 2 && tmp[tmp.size() - 1] == '}') {
        char* p = strrchr(str, '{');
        if (p) {
            // remove } char from final string
            tmp.terminate(tmp.size() - 1);
            // terminate the string before the option string
            tmp.terminate(p - str);
            ++p;
            // parse option hash
            ReferenceHolder<QoreHashNode> opt(new QoreHashNode(autoTypeInfo), xsink);

            while (true) {
                if (!*p) {
                    break;
                }
                char* eq = strchr(p, '=');
                char* oend = strchr(p, ',');
                size_t len = 0;
                // if there is only an option left with no more options and no value
                if (!eq && !oend) {
                    opt->setKeyValue(p, true, xsink);
                    p += strlen(p);
                } else {
                    // if there is more than one option and the next option to be parsed has no value
                    if (oend && (!eq || oend < eq)) {
                        len = oend - p;
                        QoreString tmp(p, len);
                        opt->setKeyValue(tmp.c_str(), true, nullptr);
                        p += len;
                    } else {
                        // here we must have an equals sign
                        assert(eq);
                        if (eq == p) {
                            xsink->raiseException(DATASOURCE_PARSE_ERROR, "missing value after '=' in option "
                                "specification in '%s'", ds);
                            return nullptr;
                        }
                        *eq = '\0';
                        ++eq;
                        len = oend ? oend - eq : strlen(eq);
                        if (opt->existsKey(p)) {
                            xsink->raiseException(DATASOURCE_PARSE_ERROR, "option '%s' repeated in '%s'", p, ds);
                            return nullptr;
                        }

                        QoreString key(p);
                        key.trim();

                        QoreString value(eq, len);
                        value.trim();

                        opt->setKeyValue(key.c_str(), new QoreStringNode(value), nullptr);

                        p = eq + len;
                    }
                }
                if (oend) {
                    ++p;
                }
            }

            h->setKeyValue("options", opt.release(), nullptr);
        }
    }

    char* p = strchr(str, ':');
    // make sure this is the driver name and not the port at the end
    if (p) {
        *p = '\0';
        if (strchrs(str, "@/%")) {
            // this is the port at the end, so skip it
            *p = ':';
        } else {
            driver = new QoreStringNode(str);
            h->setKeyValue("type", driver, nullptr);
            str = p + 1;
        }
    }

    bool has_pass = false;
    p = strchr(str, '/');
    // make sure this is not the start of a path for an in-memory DB with no username and password
    if (p) {
        if ((*str == '@') && ((p == (str + 1)) || (*(str + 1) && (p == (str + 2)) && (*(p - 1) == '.')))) {
            p = nullptr;
        // make sure this is not a path in an option
        } else if (strchr(p + 1, '}')) {
            *p = '\0';
            const char* p0 = strchr(str, '{');
            *p = '/';
            if (p0) {
                p = nullptr;
            }
        }
    }
    if (p) {
        *p = '\0';
        if (*str) {
            h->setKeyValue("user", new QoreStringNode(str), nullptr);
        }
        str = p + 1;
        has_pass = true;
    }

    // take last '@' sign in string in case there's one in the password
    p = strrchr(str, '@');
    if (!p) {
        xsink->raiseException(DATASOURCE_PARSE_ERROR, "missing database name delimited by '@' in '%s'", ds);
        return nullptr;
    }

    *p = '\0';
    if (p != str) {
        if (has_pass) {
            h->setKeyValue("pass", new QoreStringNode(str), nullptr);
        } else {
            h->setKeyValue("user", new QoreStringNode(str), nullptr);
        }
    }
    str = p + 1;

    char* db = str;
    p = strchr(str, '(');
    if (p) {
        char* end = strchr(p, ')');
        if (!end) {
            xsink->raiseException(DATASOURCE_PARSE_ERROR, "missing closing parenthesis in charset specification in "
                "'%s'", ds);
            return nullptr;
        }
        *p = '\0';  // terminate for db
        *end = '\0';  // terminate charset
        p++;
        h->setKeyValue("charset", new QoreStringNode(p), nullptr);
        str = end + 1;
    }

    // get db name
    p = check_datasource_chars(str, "%:");
    if (!p) {
        h->setKeyValue("db", new QoreStringNode(db), nullptr);
    } else {
        char tok = *p;
        *p = '\0';

        h->setKeyValue("db", new QoreStringNode(db), nullptr);
        str = p + 1;

        if (tok == '%') {
            p = check_datasource_chars(str, ":");
            if ((p == str) || !*str) {
                xsink->raiseException(DATASOURCE_PARSE_ERROR, "missing hostname string after '%%' delimeter in '%s'",
                    ds);
                return nullptr;
            }

            const char* hstr = str;

            if (p) {
                tok = *p;
                *p = '\0';
                str = p + 1;
            } else {
                str += strlen(str);
            }

            h->setKeyValue("host", new QoreStringNode(hstr), nullptr);
        }

        if (tok == ':') {
            int port = atoi(str);
            assert(port);
            h->setKeyValue("port", port, nullptr);

            char* pstr = str;
            while (isdigit(*pstr)) {
                ++pstr;
            }

            if (*pstr) {
                xsink->raiseException(DATASOURCE_PARSE_ERROR, "invalid characters present in port number in '%s'",
                    ds);
                return nullptr;
            }
            str = pstr;
        }

        if (*str) {
            xsink->raiseException(DATASOURCE_PARSE_ERROR, "unrecognized characters at end of datasource definition "
                "in '%s'", ds);
            return nullptr;
        }
    }

    return h.release();
}

QoreStringNode* makeConfigString(const QoreHashNode* h, ExceptionSink* xsink) {
    // "type" key is required
    QoreValue type_val = h->getKeyValue("type");
    if (type_val.isNullOrNothing() || (type_val.getType() == NT_STRING
        && !type_val.get<const QoreStringNode>()->size())) {
        xsink->raiseException("MAKE-DATASOURCE-STRING-ERROR",
            "missing or empty 'type' key in datasource configuration hash");
        return nullptr;
    }
    if (type_val.getType() != NT_STRING) {
        xsink->raiseException("MAKE-DATASOURCE-STRING-ERROR",
            "'type' key must be a string; got type '%s'", type_val.getTypeName());
        return nullptr;
    }

    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(type_val.get<const QoreStringNode>()->c_str()));
    str->concat(':');

    // user
    QoreValue user_val = h->getKeyValue("user");
    if (!user_val.isNullOrNothing() && user_val.getType() == NT_STRING
        && user_val.get<const QoreStringNode>()->size()) {
        str->concat(user_val.get<const QoreStringNode>()->c_str());
    }

    // password
    QoreValue pass_val = h->getKeyValue("pass");
    if (!pass_val.isNullOrNothing() && pass_val.getType() == NT_STRING
        && pass_val.get<const QoreStringNode>()->size()) {
        str->concat('/');
        str->concat(pass_val.get<const QoreStringNode>()->c_str());
    }

    // '@' is always present
    str->concat('@');

    // db
    QoreValue db_val = h->getKeyValue("db");
    if (!db_val.isNullOrNothing() && db_val.getType() == NT_STRING
        && db_val.get<const QoreStringNode>()->size()) {
        str->concat(db_val.get<const QoreStringNode>()->c_str());
    }

    // charset
    QoreValue charset_val = h->getKeyValue("charset");
    if (!charset_val.isNullOrNothing() && charset_val.getType() == NT_STRING
        && charset_val.get<const QoreStringNode>()->size()) {
        str->sprintf("(%s)", charset_val.get<const QoreStringNode>()->c_str());
    }

    // host
    bool has_host = false;
    QoreValue host_val = h->getKeyValue("host");
    if (!host_val.isNullOrNothing() && host_val.getType() == NT_STRING
        && host_val.get<const QoreStringNode>()->size()) {
        str->sprintf("%%%s", host_val.get<const QoreStringNode>()->c_str());
        has_host = true;
    }

    // port (only emitted if host is also present, since the parser expects %host:port)
    if (has_host) {
        QoreValue port_val = h->getKeyValue("port");
        if (!port_val.isNullOrNothing()) {
            int64 port = port_val.getAsBigInt();
            if (port) {
                str->sprintf(":%lld", port);
            }
        }
    }

    // options
    QoreValue opts_val = h->getKeyValue("options");
    if (!opts_val.isNullOrNothing() && opts_val.getType() == NT_HASH) {
        const QoreHashNode* opts = opts_val.get<const QoreHashNode>();
        bool has_options = false;
        ConstHashIterator hi(*opts);
        while (hi.next()) {
            QoreValue v = hi.get();
            if (v.isNullOrNothing() || (v.getType() == NT_BOOLEAN && !v.getAsBool())) {
                continue;
            }

            if (has_options) {
                str->concat(',');
            } else {
                str->concat('{');
                has_options = true;
            }
            str->concat(hi.getKey());
            if (v.getType() == NT_BOOLEAN && v.getAsBool()) {
                continue;
            }

            QoreStringValueHelper sv(v);
            str->sprintf("=%s", sv->c_str());
        }
        if (has_options) {
            str->concat('}');
        }
    }

    return str.release();
}
