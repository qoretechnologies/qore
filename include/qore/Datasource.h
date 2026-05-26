/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Datasource.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

        @deprecated do not use
    */
    DLLEXPORT Datasource(DBIDriver* driver);

    //! copy constructor
    /*
        @deprecated do not use
    */
    DLLEXPORT Datasource(const Datasource& old);

    //! the Datasource is closed if it's still open and the object is destroyed
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
    */
    DLLEXPORT void setPendingUsername(const char* u);

    //! sets the password to be used for the next connection
    /**
        @param p the password to be used for the next connection
    */
    DLLEXPORT void setPendingPassword(const char* p);

    //! sets the database (or schema) name to be used for the next connection
    /**
        @param d the database (or schema) name to be used for the next connection
    */
    DLLEXPORT void setPendingDBName(const char* d);

    //! sets the database-specific name of the character-encoding  to be used for the next connection
    /**
        @param c the database-specific name of the character-encoding to be used for the next connection
    */
    DLLEXPORT void setPendingDBEncoding(const char* c);

    //! sets the hostname to be used for the next connection
    /**
        @param h the hostname to be used for the next connection
    */
    DLLEXPORT void setPendingHostName(const char* h);

    //! sets the port number to be used for the next connection
    /**
        @param port the port number to be used for the next connection
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

        @since %Qore 2.3
    */
    DLLEXPORT QoreValue selectTyped(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);

    //! executes SQL through the columnar select function and returns a columnar result
    /** this function is not "const" to allow for implicit connections (and reconnections)
        @param query_str the query to execute
        @param args query arguments for placeholders or DBI formatting codes
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return a columnar result; caller owns the reference

        @since %Qore 2.3
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

        @since %Qore 2.3
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

    //! closes the connection
    DLLEXPORT int close();

    //! closes and opens the connection
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
    */
    DLLEXPORT const QoreHashNode* getConnectOptions() const;

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
};

#endif // _QORE_DATASOURCE_H
