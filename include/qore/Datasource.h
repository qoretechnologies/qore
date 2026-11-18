/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Datasource.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    The Datasource class provides the low-level interface to Qore DBI drivers.

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

#ifndef _QORE_DATASOURCE_H

#define _QORE_DATASOURCE_H

#include <qore/QoreThreadLock.h>
#include <qore/QoreQueue.h>

#include <string>

class DBIDriver;
class QoreColumnarResult;
class DatasourceStatementHelper;
class ResolvedCallReferenceNode;
class SqlMutationContext;
class QoreModuleCommandRestrictions;
class QoreSandboxManager;

//! defined if Datasource::getOptionRestrictions() and Datasource::getOptionPolicyManagerRef() are available
/** A driver judges an option by the code that set it, not by the code that opens the connection (qore#5741)

    @since %Qore 3.0
*/
#define _QORE_HAS_DATASOURCE_OPTION_RESTRICTIONS 1

//! defined if Datasource::getConnectChoosers(), Datasource::addOptionChooser() and QoreDatasourceConnectChoosers are
//! available
/** A driver judges a connection by the code that chose it, not by the code that opens it (qore#6142)

    @since %Qore 3.0
*/
#define _QORE_HAS_DATASOURCE_CONNECT_CHOOSERS 1

//! The code that chose the connection of a datasource: its creators and the setters of the values that a connect uses
/** Filled by Datasource::getConnectChoosers() and Datasource::addOptionChooser().  Each entry is the record of a
    piece of code, made when the code created the datasource or set a value; each creation and each value set makes a
    record of its own, so code that created a datasource and set three of its values gives four entries.  A record is
    an entry once (the connections of a DatasourcePool, whose creator set all of their values with one record, have
    one entry), and the creators of a copy have no two records that always give the same decision (see
    Datasource::getConnectChoosers()).

    A driver that connects to a destination that sandboxed code must not choose judges the connection by these
    entries, not by the code that is running when it connects: a connection is restricted if any entry does not allow
    uncontrolled APIs (allowsUncontrolledApis()), and it is then checked against the policy managers of the entries
    that restrict it (getPolicyManagerRef()).

    An empty object is restricted: allowsUncontrolledApis() returns false for it.  A filled record always has an entry
    (the creator), and Datasource::getConnectChoosers() and Datasource::addOptionChooser() leave the object empty when
    they raise an exception; a driver that derives the decision from the entries itself (allowsUncontrolledApis(size_t)
    for each entry) must therefore treat size() == 0 as restricted, never as "no entry restricts".

    The object is created by the caller, is not copyable, holds its own references to the records, and may be read on
    any thread once it is filled.

    @since %Qore 3.0
*/
class QoreDatasourceConnectChoosers {
public:
    //! creates an empty object; allocates nothing and never throws
    DLLEXPORT QoreDatasourceConnectChoosers() noexcept;

    //! releases the references to the records
    DLLEXPORT ~QoreDatasourceConnectChoosers();

    //! returns the number of entries
    /** At least 1 after a successful Datasource::getConnectChoosers(): a datasource always has a creator; 0 before it,
        after clear(), or after an error
    */
    DLLEXPORT size_t size() const;

    //! returns true if every entry may use uncontrolled APIs; see allowsUncontrolledApis(size_t)
    /** @return false for an empty object: a missing record is never a permission

        @note Each call reads the policies again, and a host can change them at any time; a driver that also needs the
        result of each entry calls allowsUncontrolledApis(size_t) once for each entry and derives the aggregate from
        those results instead of calling this function as well
    */
    DLLEXPORT bool allowsUncontrolledApis() const;

    //! returns true if entry \a i may use uncontrolled APIs
    /** For an entry that a sandbox policy governed, the current \c getUncontrolledApiPolicy() of its policy manager
        (read now, so that a later change of the host's policy applies, in both directions); for an entry without a
        policy manager (no sandbox governed it, or it ran under a trusted barrier such as
        \c SandboxManager::callWithSystemPolicy()), qore_sandbox_allows_uncontrolled_apis() with the \c Policy purpose
        as it was when it chose, which is true for such an entry.

        @return the result; false if \a i is out of range (asserted in a debug build)
    */
    DLLEXPORT bool allowsUncontrolledApis(size_t i) const;

    //! sets \a r to the restrictions of entry \a i
    /** The restrictions are those that QoreModuleCommandRestrictions::getCallerRestrictions() gave the code when it
        chose.

        @return true if \a i is in range and \a r was set, false if not (asserted in a debug build)

        @note The restrictions are resolved for every purpose (QoreModuleCommandRestrictions::isSandboxed()), so they do
        not follow the barrier rule of a policy check: decide whether a connection is restricted with
        allowsUncontrolledApis(), not with them
    */
    DLLEXPORT bool getRestrictions(size_t i, QoreModuleCommandRestrictions& r) const;

    //! returns a new reference to the policy manager of entry \a i, or nullptr
    /** The manager that a policy check made by the code would have used when it chose
        (QoreSandboxManagerHelper(QoreSandboxManagerHelper::Policy)); nullptr if no sandbox policy governed it, or if
        \a i is out of range (asserted in a debug build).  The caller owns the reference.
    */
    DLLEXPORT QoreSandboxManager* getPolicyManagerRef(size_t i) const;

    //! removes every entry
    DLLEXPORT void clear();

    QoreDatasourceConnectChoosers(const QoreDatasourceConnectChoosers&) = delete;
    QoreDatasourceConnectChoosers& operator=(const QoreDatasourceConnectChoosers&) = delete;

private:
    //! the entries; nullptr until the object is first filled
    struct qore_ds_connect_choosers_private* priv;

    friend class Datasource;
    friend struct qore_ds_private;
};

//! the base class for accessing databases in Qore through a Qore DBI driver
/** This class is not thread-safe or even thread-aware.  Thread safety and thread
    resource management is implemented in higher-level classes such as ManagedDatasource
    and DatasourcePool (classes are currently internal)

    Two copies of connection values are kept in case the values are changed while a
    connection is in use.
    @see DBIDriver
*/
class Datasource {
    friend class QoreSQLStatement;
    friend struct qore_ds_private;

private:
    struct qore_ds_private *priv; // private implementation

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    Datasource& operator=(const Datasource&) = delete;

    //! this helper method shares code for exec() and execRaw() methods
    DLLLOCAL QoreValue exec_internal(bool doBind, const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);

protected:
    //! frees all connection values
    DLLEXPORT void freeConnectionValues();

    //! copies pending values to current values
    DLLEXPORT void setConnectionValues();

    //! sets the transaction status
    DLLEXPORT void setTransactionStatus(bool);

    //! copies all pending connection values to another Datasource
    DLLEXPORT void setPendingConnectionValues(const Datasource* other);

    //! calls the "begin_implicit_transaction" DBI method if it exists
    DLLEXPORT int beginImplicitTransaction(ExceptionSink* xsink);

public:
    //! creates the object; internal only
    /**
        @param driver the DBIDriver object to use for the connection
        @param dsh the interface of the real C++ parent object

        @since %Qore 0.8.13
    */
    DLLLOCAL Datasource(DBIDriver* driver, DatasourceStatementHelper* dsh);

    //! copy constructor; internal only
    /**
        @param old the old object to copy
        @param dsh the interface of the real C++ parent object

        @since %Qore 0.8.13
    */
    DLLLOCAL Datasource(const Datasource& old, DatasourceStatementHelper* dsh);

    //! creates the object and binds it to a particular DBIDriver
    /**
        @param driver the DBIDriver object to use for the connection

        @note the calling code is recorded as the creator of the datasource; see getConnectChoosers()

        @deprecated do not use
    */
    DLLEXPORT Datasource(DBIDriver* driver);

    //! copy constructor
    /*
        @note the creators of the copy are the calling code (the copier) and the creators of \a old, and the copy
        keeps the setters of the values that it copies; see getConnectChoosers()

        @deprecated do not use
    */
    DLLEXPORT Datasource(const Datasource& old);

    //! the Datasource is closed if it's still open and the object is destroyed
    /** an error closing the connection is reported as a DBI warning event; see close()
    */
    DLLEXPORT virtual ~Datasource();

    //! returns the autocommit status
    /**
        @return the autocommit status of the object
    */
    DLLEXPORT bool getAutoCommit() const;

    //! returns the username used for the last connection
    /**
        @return
    */
    DLLEXPORT const char* getUsername() const;

    //! returns the password used for the last connection
    /**
        @return the password used for the last connection (or 0 if none)
    */
    DLLEXPORT const char* getPassword() const;

    //! returns the database (or schema) name used for the last connection
    /**
        @return the database (or schema) name used for the last connection (or 0 if none)
    */
    DLLEXPORT const char* getDBName() const;

    //! returns the database-specific character encoding name used for the last connection
    /**
        @return the database-specific character encoding name used for the last connection
    */
    DLLEXPORT const char* getDBEncoding() const;

    //! returns the OS (or Qore) character encoding name used for the last connection
    /**
        @return the OS (or Qore) character encoding name used for the last connection
    */
    DLLEXPORT const char* getOSEncoding() const;

    //! returns the host name used for the last connection
    /**
        @return the host name used for the last connection (or 0 if none)
    */
    DLLEXPORT const char* getHostName() const;

    //! returns the port number used for the last connection
    /**
        @return the port number used for the last connection
    */
    DLLEXPORT int getPort() const;

    //! returns the username used for the last connection
    /**
        @return the username used for the last connection
    */
    DLLEXPORT const std::string& getUsernameStr() const;

    //! returns the password used for the last connection
    /**
        @return the password used for the last connection (or 0 if none)
    */
    DLLEXPORT const std::string& getPasswordStr() const;

    //! returns the database (or schema) name used for the last connection
    /**
        @return the database (or schema) name used for the last connection (or 0 if none)
    */
    DLLEXPORT const std::string& getDBNameStr() const;

    //! returns the database-specific character encoding name used for the last connection
    /**
        @return the database-specific character encoding name used for the last connection
    */
    DLLEXPORT const std::string& getDBEncodingStr() const;

    //! returns the host name used for the last connection
    /**
        @return the host name used for the last connection (or 0 if none)
    */
    DLLEXPORT const std::string& getHostNameStr() const;

    //! returns the private DBI-specific data structure for this object
    DLLEXPORT void* getPrivateData() const;

    //! returns the private DBI-specific data structure for this object
    template <typename T>
    DLLLOCAL T* getPrivateData() const {
        return reinterpret_cast<T*>(getPrivateData());
    }

    //! returns the private DBI-specific data structure for this object
    template <typename T>
    DLLLOCAL T& getPrivateDataRef() const {
        return *getPrivateData<T>();
    }

    //! sets the private DBI-specific data structure for this object
    /** this should only be called once in the actual DBI driver code
        @param data the data for the DBI driver that holds the driver-specific state of the connection
    */
    DLLEXPORT void setPrivateData(void* data);

    //! sets the database-specific character encoding name used for the current connection
    /** this function should only be called by the DBI driver when a connection is established
        @param name the database-specific character encoding name used for the current connection
    */
    DLLEXPORT void setDBEncoding(const char* name);

    //! returns the QoreEncoding pointer used for this connection
    DLLEXPORT const QoreEncoding* getQoreEncoding() const;

    //! sets the QoreEncoding used for this connection
    /** this function should only be called by the DBI driver when a connection is established
        @param enc the QoreEncoding used for the current connection
    */
    DLLEXPORT void setQoreEncoding(const QoreEncoding* enc);

    //! sets the name for the QoreEncoding used for this connection
    /** this function should only be called by the DBI driver when a connection is established
        @param name the name for the QoreEncoding used for the current connection
    */
    DLLEXPORT void setQoreEncoding(const char* name);

    //! sets the username to be used for the next connection
    /**
        @param u the username to be used for the next connection

        @note the calling code is recorded as the setter of the value; see getConnectChoosers()
    */
    DLLEXPORT void setPendingUsername(const char* u);

    //! sets the password to be used for the next connection
    /**
        @param p the password to be used for the next connection

        @note the calling code is recorded as the setter of the value; see getConnectChoosers()
    */
    DLLEXPORT void setPendingPassword(const char* p);

    //! sets the database (or schema) name to be used for the next connection
    /**
        @param d the database (or schema) name to be used for the next connection

        @note the calling code is recorded as the setter of the value; see getConnectChoosers()
    */
    DLLEXPORT void setPendingDBName(const char* d);

    //! sets the database-specific name of the character-encoding  to be used for the next connection
    /**
        @param c the database-specific name of the character-encoding to be used for the next connection

        @note the encoding does not choose the destination or the credentials of the connection, so no setter is
        recorded for it (see getConnectChoosers())
    */
    DLLEXPORT void setPendingDBEncoding(const char* c);

    //! sets the hostname to be used for the next connection
    /**
        @param h the hostname to be used for the next connection

        @note the calling code is recorded as the setter of the value; see getConnectChoosers()
    */
    DLLEXPORT void setPendingHostName(const char* h);

    //! sets the port number to be used for the next connection
    /**
        @param port the port number to be used for the next connection

        @note the calling code is recorded as the setter of the value; see getConnectChoosers()
    */
    DLLEXPORT void setPendingPort(int port);

    DLLEXPORT void setAutoCommit(bool ac);

    //! opens a connection to the database
    /** calls the DBI driver's "open" method
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT int open(ExceptionSink* xsink);

    //! executes SQL throught the "select" function of the DBI driver and returns the result, makes an implicit connection if necessary
    /** this function is not "const" to allow for implicit connections (and reconnections)
        @param query_str the query to execute
        @param args query arguments for %s, %n, %d placeholders
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue select(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);

    //! executes SQL through the typed select function and returns a typed result, makes an implicit connection if necessary
    /** this function is not "const" to allow for implicit connections (and reconnections)
        @param query_str the query to execute
        @param args query arguments for %s, %n, %d placeholders
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @since %Qore 3.0
    */
    DLLEXPORT QoreValue selectTyped(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);

    //! executes SQL through the columnar select function and returns a columnar result
    /** this function is not "const" to allow for implicit connections (and reconnections)
        @param query_str the query to execute
        @param args query arguments for placeholders or DBI formatting codes
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return a columnar result; caller owns the reference

        @since %Qore 3.0
    */
    DLLEXPORT QoreColumnarResult* selectColumnar(const QoreString* query_str, const QoreListNode* args,
        ExceptionSink* xsink);

    //! executes SQL throught the "selectRows" function of the DBI driver and returns the result, makes an implicit connection if necessary
    /** this function is not "const" to allow for implicit connections (and reconnections)
        @param query_str the query to execute
        @param args query arguments for %s, %n, %d placeholders
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue selectRows(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);

    //! executes SQL through the typed selectRows function and returns a typed result, makes an implicit connection if necessary
    /** this function is not "const" to allow for implicit connections (and reconnections)
        @param query_str the query to execute
        @param args query arguments for %s, %n, %d placeholders
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @since %Qore 3.0
    */
    DLLEXPORT QoreValue selectRowsTyped(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);

    //! executes SQL throught the "selectRow" function of the DBI driver and returns the result, makes an implicit connection if necessary
    /** This function is not "const" to allow for implicit connections (and reconnections).
        An exception will be thrown by the DBI driver if the query returns more than 1 row of data.
        @param query_str the query to execute
        @param args query arguments for %s, %n, %d placeholders
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return the row data returned or 0
    */
    DLLEXPORT QoreHashNode* selectRow(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);

    //! executes SQL throught the "exec" function of the DBI driver and returns the result, makes an implicit connection if necessary
    /** The "in_transaction" flag will be set to true if this method executes without
        throwing an exception and the object was not already in a transaction.
        this function is not "const" to allow for implicit connections (and reconnections)
        @param query_str the query to execute
        @param args query arguments for %s, %n, %d placeholders
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue exec(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);

    //! executes SQL throught the "execRaw" function of the DBI driver and returns the result, makes an implicit connection if necessary
    /** The "in_transaction" flag will be set to true if this method executes without
        throwing an exception and the object was not already in a transaction.
        this function is not "const" to allow for implicit connections (and reconnections)

        @param query_str the query to execute
        @param args this argument is ignored
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @deprecated this function will be removed in the next major version of the %Qore library due to the inclusion of the extraneous and ignored \a args parameter
    */
    DLLEXPORT QoreValue execRaw(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);

    //! executes SQL throught the "execRaw" function of the DBI driver and returns the result, makes an implicit connection if necessary
    /** The "in_transaction" flag will be set to true if this method executes without
        throwing an exception and the object was not already in a transaction.
        this function is not "const" to allow for implicit connections (and reconnections)

        @param query_str the query to execute
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue execRaw(const QoreString* query_str, ExceptionSink* xsink);

    //! Starts a driver-native bulk-load operation.
    /** @param table SQL-rendered target table name
        @param columns SQL-rendered target columns in row-hash order
        @param options optional driver-specific options
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return 0 if started, 1 if dynamically unavailable, -1 on error
        @throw DBI-BULK-LOAD-ERROR invalid arguments, unavailable capability, active protocol, or driver error

        A successful call must be followed by bulkLoadEnd(), including when bulkLoadRows() fails.

        @since %Qore 3.0
    */
    DLLEXPORT int bulkLoadBegin(const QoreString* table, const QoreListNode* columns,
        const QoreHashNode* options, ExceptionSink* xsink);

    //! Sends one hash-of-columns block to an active driver-native bulk-load operation.
    /** @param rows the row block
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return 0 for OK, -1 on error
        @throw DBI-BULK-LOAD-ERROR invalid arguments, inactive protocol, or driver error

        @since %Qore 3.0
    */
    DLLEXPORT int bulkLoadRows(const QoreHashNode* rows, ExceptionSink* xsink);

    //! Ends an active driver-native bulk-load operation.
    /** @param success true to finish the operation, false to abort it
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return 0 for OK, -1 on error
        @throw DBI-BULK-LOAD-ERROR inactive protocol or driver error

        @since %Qore 3.0
    */
    DLLEXPORT int bulkLoadEnd(bool success, ExceptionSink* xsink);

    //! executes SQL that returns a result set and then returns a hash description of the result set
    /**
        @param query_str the query to execute
        @param args this argument is ignored
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @since %Qore 0.8.9
    */
    DLLEXPORT QoreHashNode* describe(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);

    //! commits the current transaction to the database
    /** Calls the DBI driver's "commit" method.
        this function is not "const" to allow for implicit connections (and reconnections)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT int commit(ExceptionSink* xsink);

    //! rolls back the current transaction to the database
    /** Calls the DBI driver's "rollback" method.
        this function is not "const" to allow for implicit connections (and reconnections)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT int rollback(ExceptionSink* xsink);

    //! closes the connection and the statements prepared on it
    /** An error closing the connection or one of its statements cannot be raised here: it is reported as a DBI
        warning event (@ref QDBI_EVENT_WARNING) on the datasource's event queue, and discarded if no event queue is
        set; use close(ExceptionSink*) to receive it.

        @return 0 if the connection was closed, -1 if it was not open
    */
    DLLEXPORT int close();

    //! closes the connection and the statements prepared on it, raising any error closing them
    /** The connection is closed after the call in every case; an error closing it can only be reported by DBI
        drivers that register a q_dbi_close_sink_t method.

        @param xsink an error closing the connection or one of its statements is added here

        @return 0 if the connection was closed without an error or was not open, -1 if an exception was raised

        @since %Qore 3.0
    */
    DLLEXPORT int close(ExceptionSink* xsink);

    //! closes and opens the connection
    /** the connection is not opened again if an error closing it was raised; see close(ExceptionSink*)
    */
    DLLEXPORT void reset(ExceptionSink* xsink);

    //! returns a QoreListNode object of all capability strings of the current driver, the caller owns the reference count in the object returned
    /**
        @return list of capability strings of the driver
    */
    DLLEXPORT QoreListNode* getCapabilityList() const;

    //! returns the capability mask of the current driver
    /**
        @return the capability mask of the current driver
    */
    DLLEXPORT int getCapabilities() const;

    //! returns the pending username for the next connection
    /** caller owns the pointer's reference count returned
    */
    DLLEXPORT QoreStringNode* getPendingUsername() const;

    //! returns the pending password for the next connection
    /** caller owns the pointer's reference count returned
    */
    DLLEXPORT QoreStringNode* getPendingPassword() const;

    //! returns the pending database (or schema) name for the next connection
    /** caller owns the pointer's reference count returned
    */
    DLLEXPORT QoreStringNode* getPendingDBName() const;

    //! returns the pending database-specific character encoding name for the next connection
    /** caller owns the pointer's reference count returned
    */
    DLLEXPORT QoreStringNode* getPendingDBEncoding() const;

    //! returns the pending host name for the next connection
    /** caller owns the pointer's reference count returned
        @return the pending host name for the next connection
    */
    DLLEXPORT QoreStringNode* getPendingHostName() const;

    //! returns the pending port number for the next connection
    /**
        @return the pending port number used for the next connection
    */
    DLLEXPORT int getPendingPort() const;

    /** sets the "in_transaction" flag to true if autocommit is not set
        throws an exception if autocommit is true
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return -1 for error, 0 for OK
    */
    DLLEXPORT int beginTransaction(ExceptionSink* xsink);

    //! returns the transaction status flag
    /**
        @return the transaction status flag
    */
    DLLEXPORT bool isInTransaction() const;

    //! returns true if a transaction is in progress and DB commands have been issued since the transaction was started
    /** note that this function will return false if the transaction was started with beginTransaction() and no DB
        commands have been issued since then
        @return true if a transaction is in progress and DB commands have been issued since the transaction was started
    */
    DLLEXPORT bool activeTransaction() const;

    //! returns true if the connection is currently open
    /**
        @return the connection status (true if open)
    */
    DLLEXPORT bool isOpen() const;

    //! returns a copy of this object with the same DBIDriver and pending connection values
    /**
        return a copy of this object
    */
    DLLEXPORT Datasource* copy() const;

    //! returns the name of the current %Qore DBI driver
    /** @see getDriverRealName()
    */
    DLLEXPORT const char* getDriverName() const;

    //! executes the "get_server_version" function of the driver, if any, and returns the result
    /** makes an implicit connection if necessary

        the caller owns the pointer's reference count returned (if the QoreValue returned holds a pointer)
        this function is not "const" to allow for implicit connections (and reconnections)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue getServerVersion(ExceptionSink* xsink);

    //! executes the "get_client_version" function of the driver, if any, and returns the result
    /** the caller owns the pointer's reference count returned (if the QoreValue returned holds a pointer)

        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreValue getClientVersion(ExceptionSink* xsink) const;

    //! Executes the "get_driver_name" function of the driver, if any, and returns the result
    /** If the driver does not support this function, then the name of the %Qore driver is returned directly.

        If the driver supports this function, a connection is made implicitly before executing the call.

        This function is designed to return the real DB driver name for %Qore DBI drivers that wrap other drivers,
        such as ODBC and JDBC, for example.

        The caller owns the pointer's reference count returned

        @param xsink if an error occurs, the Qore-language exception information will be added here

        @see getDriverName()

        @since %Qore 1.14
    */
    DLLEXPORT QoreStringNode* getDriverRealName(ExceptionSink* xsink);

    //! returns the DBIDriver pointer used for this object
    /**
        @return the DBIDriver pointer used for this object
    */
    DLLEXPORT const DBIDriver* getDriver() const;

    //! should be called by the DBIDriver if the connection to the server is lost
    /** The DBIDriver should raise its own exception when this call is made, as making this call will
        suppress further Qore exceptions from being raised in the Datasource destructor (at least for
        derived classes)

        @note This call results in all open statements being closed and then the Datasource
        itself is closed, resulting in the deletion of the driver-specific Datasource local
        data; if the driver needs to free memory before the Datasource is closed, then the
        driver should call connectionLost() instead, free its memory, and then call
        the variant of function taking an ExceptionSink pointer argument.

        @deprecated use connectionAborted(ExceptionSink*) instead
    */
    DLLEXPORT void connectionAborted();

    //! should be called by the DBI driver if the connection to the server has been lost
    /** The DBIDriver should raise its own exception when this call is made, as making this call will
        suppress further Qore exceptions from being raised in the Datasource destructor (at least for
        derived classes)

        This function should be called after a connectionLost() call if the connection is
        not able to be reopened.

        @note This call results in all open statements being closed and then the Datasource
        itself is closed, resulting in the deletion of the driver-specific Datasource local
        data; if the driver needs to free memory before the Datasource is closed, then the
        driver should call connectionLost() instead, free its memory, and then call
        this function.

        @since %Qore 0.8.12.10
    */
    DLLEXPORT void connectionAborted(ExceptionSink* xsink);

    //! should be called be the DBI driver to signify that the connection to the server has been lost
    /** This call does not result in the Datasource being closed, but rather ensures that
        all open statements are closed while the driver-specific Datasource local data remains
        in place.

        This flag does not result in the connection aborted flag being set.

        @since %Qore 0.8.12.10
    */
    DLLEXPORT void connectionLost(ExceptionSink* xsink);

    //! should be called be the DBI driver to signify that the connection to the server has been recovered
    /** This call does not result in the Datasource being closed, but rather ensures that
        all open statements are closed and also that driver-specific Datasource local data
        is deleted.

        This function should be called after a connectionLost() call if the connection is
        then reopened.

        @since %Qore 0.8.12.10
    */
    DLLEXPORT void connectionRecovered(ExceptionSink* xsink);

    //! returns the connection aborted status
    /** @return the connection aborted status
        */
    DLLEXPORT bool wasConnectionAborted() const;

    //! called from subclasses when releasing the transaction lock
    /** Calls the DBI driver's "commit" method if autocommit is enabled and the current connection was not lost and the driver requires a commit
        this function is not "const" to allow for implicit connections (and reconnections)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT int autoCommit(ExceptionSink* xsink);

    //! returns the valid options for this driver with descriptions and current values for the current datasource
    /** @return a hash where the keys are valid option names, and the values are hashes with the following keys:
        - \c "desc": a string description of the option
        - \c "type": a string giving the data type restriction for the option
        - \c "value": the current value of the option

        The caller owns the reference count for the hash returned
    */
    DLLEXPORT QoreHashNode* getOptionHash() const;

    //! returns the options currently set for this object
    /** @return a hash where the keys and values are option names and option values

        The caller owns the reference count for the hash returned; returns 0 if no options are set on the current object or if the driver does not support options

        @since 0.8.9
    */
    DLLEXPORT QoreHashNode* getCurrentOptionHash() const;

    //! sets an option for the datasource
    /** @param opt the option to set
        @param val the value to set
        @param xsink if any errors are raised (invalid option, etc), the exception info is raised here

        @return -1 for error (exception raised), 0 for OK

        @note the calling code is recorded as the code that set the option; see getOptionRestrictions(), and
        addOptionChooser() for an option that chooses the destination of the connection

        @deprecated use setOption(const char*, const QoreValue, ExceptionSink*);
    */
    DLLEXPORT int setOption(const char* opt, const QoreValue val, ExceptionSink* xsink);

    //! Returns the current value for the given option
    /** @param opt the option to get
        @param xsink if any errors are raised (invalid option, etc), the exception info is raised here

        The caller owns the reference count for the value returned

        @note this function is only safe to call when a connection is established
    */
    DLLEXPORT QoreValue getOption(const char* opt, ExceptionSink* xsink);

    //! returns the valid options for this driver with descriptions and current values for the current datasource
    /** @return a hash where the keys are valid option names, and the values are hashes with the following keys:
        - \c "desc": a string description of the option
        - \c "type": a string giving the data type restriction for the option
        - \c "value": the current value of the option

        This function returns the same value as getOptionHash() but the caller should not modify the value returned, also
        this function is meant to be used during the open() call to read any options that may be relevant for opening a new connection

        @note
        - while the driver's option-setting function runs for setOption() on an open connection, an option that had no
          value before appears in the hash as a placeholder entry with no value (\c NOTHING): its value is stored only
          once the driver accepts it.  The Qore library does not give a placeholder to the driver's option-setting
          function when it opens a connection meanwhile (a driver that reconnects from its option-setting function),
          but a driver's open function that reads this hash directly sees it.  A placeholder has no setter during an
          open: getOptionRestrictions() returns false for it, and the driver treats it as not set.  An option whose
          stored value is \c NOTHING (set to no value with setOption()) has a setter, and is a value like any other
        - while a connection is opened, getOptionRestrictions() and getOptionPolicyManagerRef() report the setters of
          the stored values that the connection is given, also while another value of an option is being set
        - a driver must not call setOption() on the datasource from its own open or option-setting functions
        - a driver that judges the destination of a connection by the code that chose it reads
          getConnectChoosers() and adds the options that choose the destination with addOptionChooser()
    */
    DLLEXPORT const QoreHashNode* getConnectOptions() const;

    //! returns the restrictions of the code that set an option
    /** A driver applies the options of a datasource when a connection is opened (getConnectOptions()), which can
        happen long after they were set, on another thread and for other code: a DatasourcePool opens a connection on
        the thread that first needs it, and trusted code can open a datasource that sandboxed code configured.  An
        option that does something that restricted or sandboxed code must not do (such as adding to a classpath) is
        therefore judged by the code that set it: whoever configures a datasource decides what its options may do.

        The code that set an option is recorded when it is set: by setOption() (also on an open connection, where
        the new setter is pending while the driver's option-setting function runs: it is visible to the driver but
        not yet stored, and copies of the datasource do not see it; it is stored with the value once the driver
        accepts it, and a value that is refused keeps the previous setter), by the
        options of a Datasource constructor or of a connection string, and, for every connection of a
        DatasourcePool, the code that created the pool.  A copy of a datasource (also for the connections of a pool
        that is copied) keeps the setters of the options that it copies.

        The restrictions are those that QoreModuleCommandRestrictions::getCallerRestrictions() gave the code that set
        the option.

        Call it as getConnectOptions() is called: from the driver's open and option-setting functions, or while the
        calling thread holds the connection.

        @param opt the name of the option, as a key of getConnectOptions() or as passed to the driver's option-setting
        function
        @param restrictions receives the restrictions of the code that set the option if they are known

        @return true if the code that set the option is known and \a restrictions were set, false if not (the option
        is not set, or it was set in a way that recorded no setter): then the driver judges the code that is running

        @note For an option that is judged like a module command (an option that does something itself, such as adding
        to a classpath), a driver checks these restrictions first, and an option that they refuse (the setter was
        sandboxed or restricted) is refused whatever getOptionPolicyManagerRef() returns: that manager is nullptr
        whenever no sandbox policy governed the setter, also for sandboxed code that ran inside a trusted
        \c SandboxManager::callWithSystemPolicy() call (a barrier), so for such an option a missing policy is not a
        permission.  Only an option that these restrictions allow is then checked against the policy, if there is one.
        An option that chooses the destination of a connection (such as a JDBC URL), like the connection values and
        the database name, is judged by the policy rule instead (addOptionChooser() and getConnectChoosers()): a
        setter without a policy manager (code that no sandbox governed, or that ran under a trusted barrier or a
        capability) counts as trusted, as QoreDatasourceConnectChoosers::allowsUncontrolledApis(size_t) reports

        @see getOptionPolicyManagerRef()
        @see getConnectChoosers() and addOptionChooser() for the code that chose the destination of a connection
        @see the feature macro \c _QORE_HAS_DATASOURCE_OPTION_RESTRICTIONS

        @since %Qore 3.0
    */
    DLLEXPORT bool getOptionRestrictions(const char* opt, QoreModuleCommandRestrictions& restrictions) const;

    //! returns the filesystem policy of the code that set an option
    /** The manager that a policy check made by the code that set the option would have used
        (QoreSandboxManagerHelper(QoreSandboxManagerHelper::Policy)), for an option whose value names files that the
        driver reads: the driver checks them against this policy instead of against the policy of the code that opens
        the connection, so that the code that configured the datasource can read only what its own policy allows.

        @param opt the name of the option

        @return a new reference to the policy manager (the caller owns it), or nullptr if no sandbox policy governed
        the code that set the option, or if the setter is not known (see getOptionRestrictions())

        @note For an option that is judged like a module command, call it after getOptionRestrictions() has allowed
        the option, never instead of it: nullptr does not mean that the setter was trusted (under a trusted barrier,
        \c SandboxManager::callWithSystemPolicy(), the code that set the option has no policy manager but can still be
        sandboxed), so a driver that refuses the option for a restricted setter refuses it before it looks at the
        policy.  An option that chooses the destination of a connection is judged by the policy rule of
        addOptionChooser() instead, where a setter without a policy manager counts as trusted

        @since %Qore 3.0
    */
    DLLEXPORT QoreSandboxManager* getOptionPolicyManagerRef(const char* opt) const;

    //! returns the destinations of the connection as they were checked against the sandbox, or nullptr
    /** For a connection whose connection values were chosen by code that a sandbox restricting network or
        filesystem access governed (see getConnectChoosers()), the Qore library asks the driver for the destinations of
        the connection (@ref QDBI_METHOD_GET_CONNECT_TARGETS) before it opens it, checks each of them against the
        policy of every such sandbox, and keeps the result here while the connection is open.  The driver must then
        connect (and reconnect) to these destinations only, so that a host name that resolves to another address
        after the check cannot redirect the connection.

        @return nullptr if the connection was not checked (no such sandbox chose it), otherwise a list with a hash
        for each destination that the driver reported, in the same order: \c "socket" (string), the path of a UNIX
        socket, or \c "host" (string) and \c "port" (int) as the driver reported them, and \c "addresses" (a list of
        strings, never empty), the numeric addresses of the host that were checked, in the order of their resolution;
        the list is owned by the datasource and is valid until the connection is closed

        @since %Qore 3.0
    */
    DLLEXPORT const QoreListNode* getCheckedConnectTargets() const;

    //! replaces the content of \a c with the code that chose the connection: its creators and the setters of the
    //! connection values in use
    /** A driver connects when a datasource is opened, which can happen on another thread and for other code than the
        code that configured it: a DatasourcePool opens a connection on the thread that first needs one, and trusted
        code can open a datasource that sandboxed code configured (and the reverse).  A driver that must not connect
        to a destination that sandboxed code chose therefore judges the connection by this record, not by the code
        that is running (qore#6142).  The entries are:
        - the creators of the datasource: the code that created it (also by deserialization), and, for a copy, the
          code that copied it and the creators of the original; for a connection of a DatasourcePool, the creators of
          the pool, whatever thread grows it
        - the setters of the connection values in use: the user name, password, database name, host name and port
          that the connection is opened with, each recorded when it was set and copied to the values in use when the
          connection is opened (so a value that was set since then judges the next open, not a reconnect with the
          values in use); the last setter of a value replaces the previous one.  The encoding is not one of them

        Add the options that choose the destination with addOptionChooser().

        Call it as getConnectOptions() is called: from the driver's open function, or while the calling thread holds
        the connection (also for a driver's own reconnect).

        @param c receives the entries; its previous entries are removed
        @param xsink for an out-of-memory error

        @return 0 for OK, -1 if an exception was raised (\a c is empty then)

        @note must not be called from the driver's statement close or free functions (see the DBI driver rules)

        @see the feature macro \c _QORE_HAS_DATASOURCE_CONNECT_CHOOSERS

        @since %Qore 3.0
    */
    DLLEXPORT int getConnectChoosers(QoreDatasourceConnectChoosers& c, ExceptionSink* xsink) const;

    //! adds the setter of option \a opt, an option that the driver uses to choose the destination, to \a c
    /** The setter is the one that getOptionRestrictions() reports for the option (the pending setter during the
        driver's option-setting function, the stored one during an open; a placeholder entry has none).  A setter that
        is already an entry of \a c is not added again.

        @param opt the name of the option, as a key of getConnectOptions()
        @param c the entries to add to, normally filled by getConnectChoosers() first
        @param xsink for an out-of-memory error

        @return 1 if the setter of the option is an entry of \a c now, 0 if the option has no recorded setter, -1 if
        an exception was raised (\a c is empty then, also of the entries that it had: an empty record is never a
        permission)

        @note must not be called from the driver's statement close or free functions (see the DBI driver rules)

        @since %Qore 3.0
    */
    DLLEXPORT int addOptionChooser(const char* opt, QoreDatasourceConnectChoosers& c, ExceptionSink* xsink) const;

    //! returns a hash representing the configuration of the current object
    /**
        @since %Qore 0.8.8
    */
    DLLEXPORT QoreHashNode* getConfigHash() const;

    //! returns a string representing the configuration of the current object
    /** @note must be connected before calling or a crash could result

        @since %Qore 0.8.8
    */
    DLLEXPORT QoreStringNode* getConfigString() const;

    //! sets an event queue for datasource events
    /**
        @since %Qore 0.8.9
    */
    DLLEXPORT void setEventQueue(Queue* q, QoreValue arg, ExceptionSink* xsink);

    //! returns an event hash with only default information in it or 0 if no event queue is set
    /** meant to be called from drivers while a transaction or action lock is held
    */
    DLLEXPORT QoreHashNode* getEventQueueHash(Queue*& q, int event_code) const;

    //! returns an SQLStatement object representing a result set
    /** @param stmt_private_data private data for the QoreSQLStatement object

        @return an SQLStatement object representing a result set

        @since %Qore 0.8.13
    */
    DLLEXPORT QoreObject* getSQLStatementObjectForResultSet(void* stmt_private_data);

    //! registers an opt-in structured mutation observer on this datasource
    /** Takes ownership of the \a observer reference and of \a arg.  Any previously-registered
        observer is replaced.

        The observer receives deterministic pre-execution and outcome boundaries for autocommit and
        explicit transactions; see design/datasource-mutation-observer.md for the full contract.

        @param observer the callback to invoke; must not be nullptr
        @param event_mask the bitmask of event classes to deliver; see \c SQL_MUTATION_MASK_*
        @param arg an argument reported unchanged in every event
        @param xsink receives exceptions raised while invoking or releasing the observer

        @since %Qore 3.0
    */
    DLLEXPORT void setMutationObserver(ResolvedCallReferenceNode* observer, int64 event_mask, QoreValue arg,
            ExceptionSink* xsink);

    //! removes any registered mutation observer
    /** @since %Qore 3.0
    */
    DLLEXPORT void clearMutationObserver(ExceptionSink* xsink);

    //! returns true if a mutation observer is registered on this datasource
    /** @since %Qore 3.0
    */
    DLLEXPORT bool hasMutationObserver() const;

    //! pushes producer-declared mutation metadata for the current thread
    /** Declarations nest; the innermost declaration in the calling thread is the one reported to
        the observer.  Every push must be matched by a call to popMutationDeclaration().

        @return 0 for OK, -1 if the declaration is invalid, in which case a
        \c SQL-MUTATION-DECLARATION-ERROR exception has been raised and nothing was pushed

        @since %Qore 3.0
    */
    DLLEXPORT int pushMutationDeclaration(const QoreHashNode* info, ExceptionSink* xsink);

    //! pops the innermost mutation declaration for the current thread
    /** @return 0 for OK, -1 if there was no declaration to pop

        @since %Qore 3.0
    */
    DLLEXPORT int popMutationDeclaration(ExceptionSink* xsink);

    //! returns the innermost mutation declaration for the current thread or nullptr
    /** the caller owns any reference returned

        @since %Qore 3.0
    */
    DLLEXPORT QoreHashNode* getMutationDeclaration() const;

    //! returns true if a mutation observer wants bounded stream events
    /** DBI drivers should call this before doing any bookkeeping for stream reporting, so that
        unmonitored datasources do no extra work at all

        @since %Qore 3.0
    */
    DLLEXPORT bool sqlMutationObserverActive() const;

    //! reports the start of a bounded stream to the mutation observer
    /** The core never sees or buffers the stream payload; only byte counts are propagated.

        @param declared_bytes the declared size of the stream in bytes; if <= 0 then the
        \c "max_growth_bytes" value of the current declaration is used, if any
        @param xsink receives exceptions raised while invoking or releasing the observer

        @return 0 to continue; -1 if the observer rejected the stream or raised an exception, in
        which case an exception has been raised in \a xsink and the caller must abort the stream

        @since %Qore 3.0
    */
    DLLEXPORT int reportMutationStreamBegin(int64 declared_bytes, ExceptionSink* xsink);

    //! reports bounded stream progress to the mutation observer
    /** @param consumed_bytes the total number of bytes consumed by the stream so far
        @param xsink receives exceptions raised while invoking or releasing the observer

        @return 0 to continue; -1 if the observer rejected further streaming or raised an exception,
        in which case an exception has been raised in \a xsink and the caller must abort the stream

        @since %Qore 3.0
    */
    DLLEXPORT int reportMutationStreamProgress(int64 consumed_bytes, ExceptionSink* xsink);

    //! reports the end of a bounded stream to the mutation observer
    /** @param consumed_bytes the total number of bytes consumed by the stream
        @param ok false if the stream failed
        @param xsink receives exceptions raised while invoking or releasing the observer

        @return 0; stream end is a notification and cannot be rejected

        @since %Qore 3.0
    */
    DLLEXPORT int reportMutationStreamEnd(int64 consumed_bytes, bool ok, ExceptionSink* xsink);

    //! returns the mutation observer context for this datasource or nullptr; internal only
    DLLLOCAL SqlMutationContext* getMutationContext() const;

    //! replaces the mutation observer context; takes ownership of the reference; internal only
    DLLLOCAL void setMutationContext(SqlMutationContext* ctx, ExceptionSink* xsink);

    //! returns the mutation observer context, creating it if necessary; internal only
    DLLLOCAL SqlMutationContext* getOrCreateMutationContext();
};

#endif // _QORE_DATASOURCE_H
