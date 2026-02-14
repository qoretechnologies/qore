/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DBI.h

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

#ifndef _QORE_DBI_H
#define _QORE_DBI_H

#include <string>

//! @file DBI.h describes Qore's DBI interface for writing database drivers

// DBI Driver capabilities
#define DBI_CAP_NONE                     0         //!< no capabilities
#define DBI_CAP_TIME_ZONE_SUPPORT        (1 << 0)  //!< supports time zones in date/time values
#define DBI_CAP_CHARSET_SUPPORT          (1 << 1)  //!< support tagging/converting character encoding in strings
#define DBI_CAP_TRANSACTION_MANAGEMENT   (1 << 2)  //!< supports transaction management (commit/rollback)
#define DBI_CAP_STORED_PROCEDURES        (1 << 3)  //!< supports calling stored procedures/functions
#define DBI_CAP_LOB_SUPPORT              (1 << 4)  //!< supports large objects in binding and retrieving
#define DBI_CAP_BIND_BY_VALUE            (1 << 5)  //!< supports bind by value
#define DBI_CAP_BIND_BY_PLACEHOLDER      (1 << 6)  //!< supports or requires placeholder buffer specifications for output variables
#define DBI_CAP_HAS_EXECRAW              (1 << 7)  //!< provides the Datasource::execRaw() method (set automatically by the Qore library)
#define DBI_CAP_HAS_STATEMENT            (1 << 8)  //!< supports the SQLStatement class (set automatically by the Qore library)
#define DBI_CAP_HAS_SELECT_ROW           (1 << 9)  //!< provides a native selectRow() method (set automatically by the Qore library)
#define DBI_CAP_HAS_NUMBER_SUPPORT       (1 << 10) //!< supports arbitrary-precision numeric support for binding and retrieving values; if this is not true then any QoreNumberNode bind arguments will be converted to floats before binding
#define DBI_CAP_HAS_OPTION_SUPPORT       (1 << 11) //!< supports the new driver option API (set automatically by the Qore library)
#define DBI_CAP_SERVER_TIME_ZONE         (1 << 12) //!< supports automatically converting date/time values to the server's presumed time zone (can be set with options) and tagging date/time values with the same; this is independent from the client's current time zone setting
#define DBI_CAP_AUTORECONNECT            (1 << 13) //!< supports automatically/transparently reconnecting to the server if the connection is lost while not in a transaction
#define DBI_CAP_EVENTS                   (1 << 14) //!< supports DBI events
#define DBI_CAP_HAS_DESCRIBE             (1 << 15) //!< supports the describe API
#define DBI_CAP_HAS_ARRAY_BIND           (1 << 16) //!< supports binding arrays by value for bulk DML operations
#define DBI_CAP_HAS_RESULTSET_OUTPUT     (1 << 17) //!< supports the "resultset" placeholder buffer specification
#define DBI_CAP_OPTION_PASSTHRU          (1 << 18) //!< supports all options; all options are passed through to the driver

#define BN_PLACEHOLDER  0
#define BN_VALUE        1

#define DBI_DEFAULT_STR_LEN 512

// DBI method codes
#define QDBI_METHOD_OPEN                      1
#define QDBI_METHOD_CLOSE                     2
#define QDBI_METHOD_SELECT                    3
#define QDBI_METHOD_SELECT_ROWS               4
#define QDBI_METHOD_EXEC                      5
#define QDBI_METHOD_COMMIT                    6
#define QDBI_METHOD_ROLLBACK                  7
#define QDBI_METHOD_BEGIN_TRANSACTION         8
#define QDBI_METHOD_GET_SERVER_VERSION        9
#define QDBI_METHOD_GET_CLIENT_VERSION       10
#define QDBI_METHOD_EXECRAW                  11
#define QDBI_METHOD_STMT_PREPARE             12
#define QDBI_METHOD_STMT_PREPARE_RAW         13
#define QDBI_METHOD_STMT_BIND                14
#define QDBI_METHOD_STMT_BIND_PLACEHOLDERS   15
#define QDBI_METHOD_STMT_BIND_VALUES         16
#define QDBI_METHOD_STMT_EXEC                17
#define QDBI_METHOD_STMT_FETCH_ROW           18
#define QDBI_METHOD_STMT_FETCH_ROWS          19
#define QDBI_METHOD_STMT_FETCH_COLUMNS       20
#define QDBI_METHOD_STMT_NEXT                21
#define QDBI_METHOD_STMT_CLOSE               22
#define QDBI_METHOD_STMT_AFFECTED_ROWS       23
#define QDBI_METHOD_STMT_GET_OUTPUT          24
#define QDBI_METHOD_STMT_GET_OUTPUT_ROWS     25
#define QDBI_METHOD_STMT_DEFINE              26
#define QDBI_METHOD_SELECT_ROW               27
#define QDBI_METHOD_OPT_SET                  28
#define QDBI_METHOD_OPT_GET                  29
#define QDBI_METHOD_STMT_DESCRIBE            30
#define QDBI_METHOD_DESCRIBE                 31
#define QDBI_METHOD_STMT_FREE                32
#define QDBI_METHOD_STMT_EXEC_DESCRIBE       33
#define QDBI_METHOD_GET_DRIVER_REAL_NAME     34

#define QDBI_VALID_CODES 34

/* DBI EVENT Types
   all DBI events must have the following keys:
   - user: db username (if available)
   - db: db name (if available)
   - eventtype: integer event code
*/
// warning events have the following additional keys: warning, desc, [info]
#define QDBI_EVENT_WARNING      1

class Datasource;
class ExceptionSink;
class QoreString;
class QoreListNode;
class QoreHashNode;
class QoreNamespace;
class SQLStatement;

// DBI method signatures - note that only get_client_version uses a "const Datasource"
// the others do not so that automatic reconnects can be supported (which will normally
// require writing to the Datasource)

//! signature for the DBI "open" method - must be defined in each DBI driver
/** @param ds the Datasource for the connection
    @param xsink if any errors occur, error information should be added to this object
    @return 0 for OK, non-zero for error
*/
typedef int (*q_dbi_open_t)(Datasource* ds, ExceptionSink* xsink);

//! signature for the DBI "close" method - must be defined in each DBI driver
/** this function cannot throw an exception and currently any return error code is ignored
    @param ds the Datasource for the connection to close
    @return 0 for OK, non-zero for error
*/
typedef int (*q_dbi_close_t)(Datasource* ds);

//! signature for the DBI "select" method - must be defined in each DBI driver
/**
    @param ds the Datasource for the connection
    @param str the SQL string to execute, may not be in the encoding of the Datasource
    @param args arguments for placeholders or DBI formatting codes in the SQL string
    @param xsink if any errors occur, error information should be added to this object
    @return the data returned by executing the SQL or 0
*/
typedef QoreValue (*q_dbi_select_t)(Datasource* ds, const QoreString* str, const QoreListNode* args, ExceptionSink* xsink);

//! signature for the DBI "selectRows" method - must be defined in each DBI driver
/**
    @param ds the Datasource for the connection
    @param str the SQL string to execute, may not be in the encoding of the Datasource
    @param args arguments for placeholders or DBI formatting codes in the SQL string
    @param xsink if any errors occur, error information should be added to this object
    @return the data returned by executing the SQL or 0
*/
typedef QoreValue (*q_dbi_select_rows_t)(Datasource* ds, const QoreString* str, const QoreListNode* args, ExceptionSink* xsink);

//! signature for the DBI "selectRow" method - must be defined in each DBI driver
/** if the SQL causes more than 1 row to be returned, then the driver must raise an exception
    @param ds the Datasource for the connection
    @param str the SQL string to execute, may not be in the encoding of the Datasource; must cause at most one row to be returned
    @param args arguments for placeholders or DBI formatting codes in the SQL string
    @param xsink if any errors occur, error information should be added to this object
    @return the row data returned by executing the SQL or 0
    @since qore 0.8.2
*/
typedef QoreHashNode* (*q_dbi_select_row_t)(Datasource* ds, const QoreString* str, const QoreListNode* args, ExceptionSink* xsink);

//! signature for the DBI "execSQL" method - must be defined in each DBI driver
/**
    @param ds the Datasource for the connection
    @param str the SQL string to execute, may not be in the encoding of the Datasource
    @param args arguments for placeholders or DBI formatting codes in the SQL string
    @param xsink if any errors occur, error information should be added to this object
    @return the data returned by executing the SQL or 0
*/
typedef QoreValue (*q_dbi_exec_t)(Datasource* ds, const QoreString* str, const QoreListNode* args, ExceptionSink* xsink);

//! signature for the DBI "execRawSQL" method - must be defined in each DBI driver
/**
   @param ds the Datasource for the connection
   @param str the SQL string to execute, may not be in the encoding of the Datasource
   @param xsink if any errors occur, error information should be added to this object
   @return the data returned by executing the SQL or 0
*/
typedef QoreValue (*q_dbi_execraw_t)(Datasource* ds, const QoreString* str, ExceptionSink* xsink);

//! signature for the DBI "commit" method - must be defined in each DBI driver
/**
    @param ds the Datasource for the connection
    @param xsink if any errors occur, error information should be added to this object
    @return 0 for OK, non-zero for error
*/
typedef int (*q_dbi_commit_t)(Datasource* ds, ExceptionSink* xsink);

//! signature for the DBI "rollback" method - must be defined in each DBI driver
/**
    @param ds the Datasource for the connection
    @param xsink if any errors occur, error information should be added to this object
    @return 0 for OK, non-zero for error
*/
typedef int (*q_dbi_rollback_t)(Datasource* ds, ExceptionSink* xsink);

//! signature for the DBI "begin_transaction" method, should only be defined for drivers needing this to explicitly start a transaction
/**
    @param ds the Datasource for the connection
    @param xsink if any errors occur, error information should be added to this object
    @return 0 for OK, non-zero for error
*/
typedef int (*q_dbi_begin_transaction_t)(Datasource* ds, ExceptionSink* xsink);

//! signature for the "get_server_version" method
/**
    @param ds the Datasource for the connection
    @param xsink if any errors occur, error information should be added to this object
    @return a value describing the server's version
*/
typedef QoreValue (*q_dbi_get_server_version_t)(Datasource* ds, ExceptionSink* xsink);

//! signature for the "get_client_version" method
/**
    @param ds the Datasource for the connection
    @param xsink if any errors occur, error information should be added to this object
    @return a value describing the client's version
*/
typedef QoreValue (*q_dbi_get_client_version_t)(const Datasource* ds, ExceptionSink* xsink);

//! signature for the "get_driver_real_name" method
/**
    @param ds the Datasource for the connection
    @param xsink if any errors occur, error information should be added to this object
    @return a value describing the database name
*/
typedef QoreStringNode* (*q_dbi_get_driver_real_name_t)(Datasource* ds, ExceptionSink* xsink);

//! prepare statement and process placeholder specifications and bind parameters
/** @returns -1 = an exception occurred, 0 = OK
 */
typedef int (*q_dbi_stmt_prepare_t)(SQLStatement* stmt, const QoreString& str, const QoreListNode* args, ExceptionSink* xsink);

//! prepare statement with no bind parsing
/** @returns -1 = an exception occurred, 0 = OK
 */
typedef int (*q_dbi_stmt_prepare_raw_t)(SQLStatement* stmt, const QoreString& str, ExceptionSink* xsink);

//! bind input values and optionally describe output parameters
/** @returns -1 = an exception occurred, 0 = OK
 */
typedef int (*q_dbi_stmt_bind_t)(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink);

//! execute statement
/** @returns -1 = an exception occurred, 0 = OK
 */
typedef int (*q_dbi_stmt_exec_t)(SQLStatement* stmt, ExceptionSink* xsink);

//! get number of affected rows
/** @returns number of rows affected by last query
 */
typedef int (*q_dbi_stmt_affected_rows_t)(SQLStatement* stmt, ExceptionSink* xsink);

//! get output values, any row sets are returned as a hash of lists
/** @returns a hash of output values, any row sets are returned as a hash of lists
 */
typedef QoreHashNode* (*q_dbi_stmt_get_output_t)(SQLStatement* stmt, ExceptionSink* xsink);

//! get output values, any row sets are returned as a list of hashes
/** @returns a hash of output values, any row sets are returned as a list of hashes
 */
typedef QoreHashNode* (*q_dbi_stmt_get_output_rows_t)(SQLStatement* stmt, ExceptionSink* xsink);

typedef int (*q_dbi_stmt_define_t)(SQLStatement* stmt, ExceptionSink* xsink);
typedef QoreHashNode* (*q_dbi_stmt_fetch_row_t)(SQLStatement* stmt, ExceptionSink* xsink);
typedef QoreHashNode* (*q_dbi_stmt_fetch_columns_t)(SQLStatement* stmt, int rows, ExceptionSink* xsink);
typedef QoreListNode* (*q_dbi_stmt_fetch_rows_t)(SQLStatement* stmt, int rows, ExceptionSink* xsink);
typedef bool (*q_dbi_stmt_next_t)(SQLStatement* stmt, ExceptionSink* xsink);
typedef int (*q_dbi_stmt_close_t)(SQLStatement* stmt, ExceptionSink* xsink);

typedef int (*q_dbi_option_set_t)(Datasource* ds, const char* opt, const QoreValue val, ExceptionSink* xsink);
typedef QoreValue (*q_dbi_option_get_t)(const Datasource* ds, const char* opt);

//! signature for the DBI "describe" method
/**
    @param ds the Datasource for the connection
    @param str the SQL string to execute, may not be in the encoding of the Datasource, must return a result set to be described
    @param args arguments for placeholders or DBI formatting codes in the SQL string
    @param xsink if any errors occur, error information should be added to this object
    @return the data returned by executing the SQL or 0
*/
typedef QoreHashNode* (*q_dbi_describe_t)(Datasource* ds, const QoreString* str, const QoreListNode* args, ExceptionSink* xsink);

//! numeric/decimal/number values converted to optimal Qore type (either int or number)
#define DBI_OPT_NUMBER_OPT "optimal-numbers"
//! numeric/decimal/number values converted to Qore strings (original solution)
#define DBI_OPT_NUMBER_STRING "string-numbers"
//! numeric/decimal/number values converted to arbitrary-precision number values
#define DBI_OPT_NUMBER_NUMERIC "numeric-numbers"
//! set server=side timezone rules for automatic conversions/date-time value tagging
#define DBI_OPT_TIMEZONE "timezone"

//! this is the data structure Qore DBI drivers will use to pass the supported DBI methods
/** the minimum methods that must be supported are: open, close, select, selectRows, execSQL, execRawSQL, commit, and rollback
 */
class qore_dbi_method_list {
    friend struct qore_dbi_mlist_private;

private:
    struct qore_dbi_mlist_private* priv; // private implementation

    // not implemented
    DLLLOCAL qore_dbi_method_list(const qore_dbi_method_list&);
    DLLLOCAL qore_dbi_method_list& operator=(const qore_dbi_method_list&);

public:
    DLLEXPORT qore_dbi_method_list();
    DLLEXPORT ~qore_dbi_method_list();

    // covers open, commit, rollback, and begin transaction
    DLLEXPORT void add(int code, q_dbi_open_t method);
    // for close
    DLLEXPORT void add(int code, q_dbi_close_t method);
    // covers select, select_rows, select, and exec
    DLLEXPORT void add(int code, q_dbi_select_t method);
    // covers select_row
    DLLEXPORT void add(int code, q_dbi_select_row_t method);
    // covers execRaw
    DLLEXPORT void add(int code, q_dbi_execraw_t method);
    // covers get_server_version
    DLLEXPORT void add(int code, q_dbi_get_server_version_t method);
    // covers get_client_version
    DLLEXPORT void add(int code, q_dbi_get_client_version_t method);
    // covers get_driver_real_name
    DLLEXPORT void add(int code, q_dbi_get_driver_real_name_t method);

    // covers prepare
    DLLEXPORT void add(int code, q_dbi_stmt_prepare_t method);
    // covers prepare_raw
    DLLEXPORT void add(int code, q_dbi_stmt_prepare_raw_t method);
    // covers bind, bind_placeholders, bind_values
    DLLEXPORT void add(int code, q_dbi_stmt_bind_t method);
    // covers exec, close, affected_rows, define, and exec_describe
    DLLEXPORT void add(int code, q_dbi_stmt_exec_t method);
    // covers fetch_row, get_output, and get_output_rows
    DLLEXPORT void add(int code, q_dbi_stmt_fetch_row_t method);
    // covers fetch_columns
    DLLEXPORT void add(int code, q_dbi_stmt_fetch_columns_t method);
    // covers fetch_rows
    DLLEXPORT void add(int code, q_dbi_stmt_fetch_rows_t method);
    // covers next
    DLLEXPORT void add(int code, q_dbi_stmt_next_t method);

    // covers set option
    DLLEXPORT void add(int code, q_dbi_option_set_t method);
    // covers get option
    DLLEXPORT void add(int code, q_dbi_option_get_t method);

    // for registering valid options
    DLLEXPORT void registerOption(const char* name, const char* desc, const QoreTypeInfo* type = 0);
};

//! this class provides the internal link to the database driver for Qore's DBI layer
/**
     most of these functions are not exported; the Datasource class should be used
    instead of using the DBIDriver class directly
    @see Datasource
*/
class DBIDriver {
    friend struct qore_dbi_private;

private:
    //! private implementation
    struct qore_dbi_private* priv;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL DBIDriver(const DBIDriver&);
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL DBIDriver& operator=(const DBIDriver&);

public:
    //! this is the only public exported function available in this class
    /**
         @return the name of the driver (ex: "oracle")
    */
    DLLEXPORT const char* getName() const;

    //! returns true if the driver supports the statement API
    DLLEXPORT bool hasStatementAPI() const;

    //! returns the valid options for this driver with descriptions
    /** @return a hash where the keys are valid option names, and the values are hashes with the following keys:
        - \c "desc": a string description of the option
        - \c "type": a string giving the data type restriction for the option

        The caller owns the reference count for the hash returned
    */
    DLLEXPORT QoreHashNode* getOptionHash() const;

    DLLLOCAL DBIDriver(struct qore_dbi_private* p);
    DLLLOCAL ~DBIDriver();
};

struct qore_dbi_dlist_private;

//! this class is used to register and find DBI drivers loaded in qore
/**
    this class will all use the ModuleManager to try and load a driver if it is not already loaded when find() is called
    @see ModuleManager
*/
class DBIDriverList {
private:
    //! private implementation
    struct qore_dbi_dlist_private *priv;

    DLLLOCAL DBIDriver* find_intern(const char* name) const;

public:
    //! registers a new DBI driver
    /**
        @param name the name of the driver (ex: "oracle")
        @param methods the list of methods the driver supports
        @param caps the capabilities the driver supports

        @return the DBIDriver object created
    */
    DLLEXPORT DBIDriver* registerDriver(const char* name, const qore_dbi_method_list &methods, int caps);

    //! finds a driver, will try to load the driver using the ModuleManager if no such driver is already present
    /**
        @param name the name of the driver to find (or load)

        @return the DBIDriver found or 0 if not found and was not loaded

        @see ModuleManager
    */
    DLLEXPORT DBIDriver* find(const char* name) const;

    //! finds a driver, will try to load the driver using the ModuleManager if no such driver is already present
    /**
        @param name the name of the driver to find (or load)
        @param xsink Qore-language exceptions saved here if any occur

        @return the DBIDriver found or 0 if not found and was not loaded

        @see ModuleManager
    */
    DLLEXPORT DBIDriver* find(const char* name, ExceptionSink* xsink) const;

    DLLLOCAL DBIDriverList();
    DLLLOCAL ~DBIDriverList();
    DLLLOCAL QoreListNode* getDriverList() const;
};

//! list of DBI drivers currently reigsted by the Qore library
DLLEXPORT extern DBIDriverList DBI;

//! parses a datasource string and returns a hash of the component parts
DLLEXPORT QoreHashNode* parseDatasource(const char* ds, ExceptionSink* xsink);

//! builds a datasource connection string from a hash of component parts
/** @param h the hash with datasource configuration keys: type (required), user, pass, db, charset, host, port, options
    @param xsink Qore-language exceptions are stored here

    @return the datasource connection string

    @since Qore 2.3
*/
DLLEXPORT QoreStringNode* makeConfigString(const QoreHashNode* h, ExceptionSink* xsink);

//! concatenates a numeric value to the QoreString from the value
DLLEXPORT void DBI_concat_numeric(QoreString* str, QoreValue v);

//! concatenates a string value to the QoreString from the value
/** NOTE: no escaping is done here
    this function is most useful for table prefixes, etc in queries
*/
DLLEXPORT int DBI_concat_string(QoreString* str, QoreValue v, ExceptionSink* xsink);

#endif  // _QORE_DBI_H
