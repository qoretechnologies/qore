/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreFtpClient.h

    thread-safe Qore QoreFtpClient object

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

#ifndef _QORE_QOREFTPCLIENT_H

#define _QORE_QOREFTPCLIENT_H

#include <qore/InputStream.h>
#include <qore/OutputStream.h>

#define DEFAULT_FTP_CONTROL_PORT  21
#define DEFAULT_FTP_DATA_PORT     20

#define DEFAULT_USERNAME "anonymous"
#define DEFAULT_PASSWORD "qore@nohost.com"

class FtpResp;
class Queue;

//! provides thread-safe access to FTP servers through Qore data structures
/**
   is "auto" mode, tries the following data modes in order:
   - EPSV mode (RFC 2428)
   - PASV mode (RFC 959)
   - then PORT

   references:
   - RFC 959: FTP
   - RFC 2428: EPSV mode only (no IPv6 support yet)
   - RFC 4217 (supercedes RFC 2228):
   - AUTH TLS: secure authentication
   - PBSZ 0 and PROT P: secure data connections

   @note RFC 1639: LPSV mode not implemented yet

   tested with:
   - tnftpd 20040810 (Darwin/OS X 10.3.8) EPSV, PASV, PORT
   - vsFTPd 2.0.1 (Fedora Core 3) EPSV
   - proFTPd 1.3.0 (Darwin/OS X 10.4.7) EPSV, PORT, AUTH TLS, PBSZ 0, PROT P
*/
class QoreFtpClient {
    friend class qore_ftp_private;
public:
    //! creates the object and sets connection parameters based on the url passed
    /** a Qore-language exception will be raised if the URL is invalid (protocol is not "ftp"
        or "ftps") or the hostname is missing.

        @param url the URL string to use to set connection parameters
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT QoreFtpClient(const QoreString *url, ExceptionSink* xsink);

    //! creates the object with no connection parameters
    DLLEXPORT QoreFtpClient();

    //! disconnects from the host if necessary and frees all memory associated with the object
    DLLEXPORT ~QoreFtpClient();

    //! connects to the remote host and logs in
    /** if there are any connection or authentication errors, Qore-language exceptions are raised
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)
    */
    DLLEXPORT int connect(ExceptionSink* xsink);

    //! disconnects from the remote host if connected
    /**
        @return 0 for OK, non-zero for error (currently always returns 0)
    */
    DLLEXPORT int disconnect();

    //! changes the working directory on the remote host
    /** if there are any errors (if no connection has been previously established, it's an error),
        Qore-language exceptions are raised.

        @param dir the directory to change to
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)
    */
    DLLEXPORT int cwd(const char* dir, ExceptionSink* xsink);

    //! returns the working directory on the remote host (caller owns the reference count returned)
    /** the connection must be already established before this function is called or an error will be raised.

        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a string giving the working directory on the remote host (caller owns the reference count returned), 0 if an error occured
    */
    DLLEXPORT QoreStringNode* pwd(ExceptionSink* xsink);

    //! sends a file from the local filesystem to the remote server
    /** the connection must be already established before this function is called or an error will be raised.

        @param localpath the local path of the file to send
        @param remotename the name of the file on the remote server
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)
    */
    DLLEXPORT int put(const char* localpath, const char* remotename, ExceptionSink* xsink);

    //! sends the content of an InputStream to the remote server
    /** the connection must be already established before this function is called or an error will be raised.

        @param is the input stream
        @param remotename the name of the file on the remote server
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)
    */
    DLLEXPORT int put(InputStream* is, const char* remotename, ExceptionSink* xsink);

    //! gets a file from the remote server and saves it on the local filesystem
    /** the connection must be already established before this function is called or an error will be raised.

        @param remotepath the path of the file on the remote server
        @param localname the local name of the file
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)

        @see QoreFtpClient::getAsString()
        @see QoreFtpCleint::getAsBinary()
    */
    DLLEXPORT int get(const char* remotepath, const char* localname, ExceptionSink* xsink);

    //! gets a file from the remote server and writes it to an OutputStream
    /** the connection must be already established before this function is called or an error will be raised.

        @param remotepath the path of the file on the remote server
        @param os the output stream
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)

        @see QoreFtpClient::getAsString()
        @see QoreFtpCleint::getAsBinary()
    */
    DLLEXPORT int get(const char* remotepath, OutputStream* os, ExceptionSink* xsink);

    //! gets a file from the remote server starting at the given offset and writes it to the output stream
    /** the connection must be already established before this function is called or an error will be raised; the
        transfer is restarted at the offset with the \c REST command, which the server must support

        @param remotepath the path of the file on the remote server
        @param os the output stream
        @param offset the offset in bytes in the remote file at which the transfer starts; 0 transfers the whole file
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)

        @since %Qore 3.0
    */
    DLLEXPORT int get(const char* remotepath, OutputStream* os, int64 offset, ExceptionSink* xsink);

    //! returns the size in bytes of a file on the remote server
    /** the connection must be already established before this function is called or an error will be raised; the
        size is determined with the \c SIZE command in binary mode, which the server must support

        @param remotepath the path of the file on the remote server
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the size of the file in bytes, or -1 if an exception has been raised

        @since %Qore 3.0
    */
    DLLEXPORT int64 size(const char* remotepath, ExceptionSink* xsink);

    //! sends a file data io the remote server
    /** the connection must be already established before this function is called or an error will be raised.

        @param data the data to send
        @param len the length of the data to send (if 0, a Qore-language exception will be raised)
        @param remotename the name of the file on the remote server
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)
    */
    DLLEXPORT int putData(const void* data, size_t len, const char* remotename, ExceptionSink* xsink);

    //! gets a file from the remote server and returns it as a string
    /** the connection must be already established before this function is called or an error will be raised.
        @param remotepath the path of the file on the remote server
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return file data received as a string, otherwise 0 = error (meaning that an exception has been raised)
    */
    DLLEXPORT QoreStringNode* getAsString(const char* remotepath, ExceptionSink* xsink);

    //! gets a file from the remote server and returns it as a string
    /** the connection must be already established before this function is called or an error will be raised.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param remotepath the path of the file on the remote server
        @param encoding the encoding to use for the output string

        @return file data received as a string, otherwise 0 = error (meaning that an exception has been raised)

        @since %Qore 1.3
    */
    DLLEXPORT QoreStringNode* getAsString(ExceptionSink* xsink, const char* remotepath,
            const QoreEncoding* encoding = QCS_DEFAULT);

    //! gets a file from the remote server and returns it as a binary node
    /** the connection must be already established before this function is called or an error will be raised.

        @param remotepath the path of the file on the remote server
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return file data received as a binary node, otherwise 0 = error (meaning that an exception has been raised)
    */
    DLLEXPORT BinaryNode* getAsBinary(const char* remotepath, ExceptionSink* xsink);

    //! renames/moves a file on the remote server
    /** the connection must be already established before this function is called or an error will be raised.

        @param from the original file path on the remote server
        @param to the new file path on the remote server
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)
    */
    DLLEXPORT int rename(const char* from, const char* to, ExceptionSink* xsink);

    //! returns a string listing the directory contents on the remote host (caller owns the reference count returned)
    /** the connection must be already established before this function is called or an error will be raised.

        @param path the path to list
        @param long_list if true then a "long list" is made
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a string giving the directory listing on the remote host (caller owns the reference count returned), 0 if an error occured
    */
    DLLEXPORT QoreStringNode* list(const char* path, bool long_list, ExceptionSink* xsink);

    //! deletes the given file on the remote server
    /** the connection must be already established before this function is called or an error will be raised.

        @param file the filename to delete
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)
    */
    DLLEXPORT int del(const char* file, ExceptionSink* xsink);

    //! creates a directory on the remote server
    /** the connection must be already established before this function is called or an error will be raised.

        @param remotepath the path of the directory on the remote server
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)
    */
    DLLEXPORT int mkdir(const char* remotepath, ExceptionSink* xsink);

    //! removes a directory on the remote server
    /** the connection must be already established before this function is called or an error will be raised.

        @param remotepath the path of the directory on the remote server
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, non-zero for error (meaning that an exception has been raised)
    */
    DLLEXPORT int rmdir(const char* remotepath, ExceptionSink* xsink);

    //! returns the port number connection parameter
    DLLEXPORT int getPort() const;

    //! returns the user name connection parameter
    DLLEXPORT const char* getUserName() const;

    //! returns the password connection parameter
    DLLEXPORT const char* getPassword() const;

    //! returns the hostname connection parameter
    DLLEXPORT const char* getHostName() const;

    //! sets the connection parameters from a URL
    /** a Qore-language exception will be raised if the URL is invalid (protocol is not "ftp"
        or "ftps") or the hostname is missing.

        @param url the URL string to use to set connection parameters
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT void setURL(const QoreString *url, ExceptionSink* xsink);

    //! returns a URL string representing the current connection parameters, caller owns the reference count returned
    /** this function always returns a value
         @return a URL string representing the current connection parameters, caller owns the reference count returned
    */
    DLLEXPORT QoreStringNode* getURL() const;

    //! sets the port connection parameter
    DLLEXPORT void setPort(int p);

    //! sets the user name connection parameter
    DLLEXPORT void setUserName(const char* u);

    //! sets the password connection parameter
    DLLEXPORT void setPassword(const char* p);

    //! sets the host name connection parameter
    DLLEXPORT void setHostName(const char* h);

    //! sets the secure connection parameter flag (to use the FTPS protocol)
    /** @return 0 for OK, -1 for error, not set (if a connection is currently established, then this flag cannot be changed)
    */
    DLLEXPORT int setSecure();

    //! unsets the secure connection parameter flag (to use the FTP protocol)
    /** @return 0 for OK, -1 for error, not set (if a connection is currently established, then this flag cannot be changed)
    */
    DLLEXPORT int setInsecure();

    //! sets the secure data connection parameter flag
    /** after calling QoreFtpClient::setSecure(), this function can be set to indicate that
         data connection should not be encrypted (while logins will be encrypted)
        @return 0 for OK, -1 for error, not set (if a connection is currently established, then this flag cannot be changed)
    */
    DLLEXPORT int setInsecureData();

    //! returns the secure connection parameter flag
    /** true indicates that current control connection (if any) is encrypted, or that the next control connection can only be established with a secure connection
         @return the secure connection parameter flag
    */
    DLLEXPORT bool isSecure() const;

    //! returns the secure data connection parameter flag
    /** true indicates that the current data connection (if any) is encrypted, or that the next data connection can only be established with a secure connection
         @return the secure dataconnection parameter flag
    */
    DLLEXPORT bool isDataSecure() const;

    //! Returns the socket connection status
    /** @return the socket connection status

        @since %Qore 2.0
    */
    DLLEXPORT bool isConnected() const;

    //! returns the name of the SSL Cipher for the currently-connected control connection, or 0 if there is none
    /**
         @return the name of the SSL Cipher for the currently-connected control connection, or 0 if there is none
    */
    DLLEXPORT const char* getSSLCipherName() const;

    //! returns the version string of the SSL Cipher for the currently-connected control connection, or 0 if there is none
    /**
         @return the version string of the SSL Cipher for the currently-connected control connection, or 0 if there is none
    */
    DLLEXPORT const char* getSSLCipherVersion() const;

    //! returns the peer certificate verification code
    DLLEXPORT long verifyPeerCertificate() const;

    //! sets the connection mode for the next connection to "auto"
    DLLEXPORT void setModeAuto();

    //! sets the connection mode for the next connection to "EPSV" (extended passive mode)
    DLLEXPORT void setModeEPSV();

    //! sets the connection mode for the next connection to "PASV" (passive mode)
    DLLEXPORT void setModePASV();

    //! sets the connection mode for the next connection to "PORT"
    DLLEXPORT void setModePORT();

    //! returns a string for the connection mode: "port", "pasv", "epsv", or "auto" if not connected and auto mode is set
    DLLEXPORT const char* getMode() const;

    DLLEXPORT void clearWarningQueue(ExceptionSink* xsink);
    DLLEXPORT void setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg, int64 min_ms = 1000);
    DLLEXPORT QoreHashNode* getUsageInfo() const;
    DLLEXPORT void clearStats();

    //! sets the network family to use with new connections
    /** @param family the network family to use with new connections

        @note must be a valid value (AF_INET, AF_INET6, AF_UNSPEC); values are not validated by this API

        @since Qore 0.9.0
    */
    DLLEXPORT void setNetworkFamily(int family);

    //! returns the network family to use with new connections
    /** @return the network family to use with new connections (default: AF_UNSPEC)

        @since Qore 0.9.0
    */
    DLLEXPORT int getNetworkFamily() const;

    //! returns peer information for a connected control socket
    /** if the socket is not connected, a Qore-language exception is thrown

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param host_lookup do a host lookup (if this is false the \c "hostname" and \c "hostname_desc" are not present in the response hash)

        @return a hash with the following keys:
        - \c hostname: the hostname of the remote end (if known or appropriate for the socket type, only performed if \a host_lookup is true)
        - \c hostname_desc: a descriptive string for the remote hostname (including the socket type - ie "ipv6[host]", only performed if \a host_lookup is true)
        - \c address: the address of the remote end - for UNIX sockets this is the file path
        - \c address_desc: a descriptive string for the remote address
        - \c port: the port number if known
        - \c family: the address family (ie AF_INET, AF_INET6, AF_UNIX, ...)
        - \c familystr: a string description of the address family ("ipv4", "ipv6", etc)

        @since Qore 0.9.0
    */
    DLLEXPORT QoreHashNode* getControlPeerInfo(ExceptionSink* xsink, bool host_lookup) const;

    //! returns peer information for a connected data socket
    /** if the socket is not connected, a Qore-language exception is thrown

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param host_lookup do a host lookup (if this is false the \c "hostname" and \c "hostname_desc" are not present in the response hash)

        @return a hash with the following keys:
        - \c hostname: the hostname of the remote end (if known or appropriate for the socket type, only performed if \a host_lookup is true)
        - \c hostname_desc: a descriptive string for the remote hostname (including the socket type - ie "ipv6[host]", only performed if \a host_lookup is true)
        - \c address: the address of the remote end - for UNIX sockets this is the file path
        - \c address_desc: a descriptive string for the remote address
        - \c port: the port number if known
        - \c family: the address family (ie AF_INET, AF_INET6, AF_UNIX, ...)
        - \c familystr: a string description of the address family ("ipv4", "ipv6", etc)

        @since Qore 0.9.0
    */
    DLLEXPORT QoreHashNode* getDataPeerInfo(ExceptionSink* xsink, bool host_lookup) const;

    //! returns information for the current control socket; the socket must be open
    /** if the socket is not open, a Qore-language exception is thrown

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param host_lookup do a host lookup (if this is false the \c "hostname" and \c "hostname_desc" are not present in the response hash)

        @return a hash with the following keys:
        - \c hostname: the hostname for the local interface (if known or appropriate for the socket type)
        - \c hostname_desc: a descriptive string for the local hostname (including the socket type - ie "ipv6[host]")
        - \c address: the address of the local interface - for UNIX sockets this is the file path
        - \c address_desc: a descriptive string for the local interface
        - \c port: the port number if known
        - \c family: the address family (ie AF_INET, AF_INET6, AF_UNIX, ...)
        - \c familystr: a string description of the address family ("ipv4", "ipv6", etc)

        @since Qore 0.9.0
    */
    DLLEXPORT QoreHashNode* getControlSocketInfo(ExceptionSink* xsink, bool host_lookup) const;

    //! returns information for the current control socket; the socket must be open
    /** if the socket is not open, a Qore-language exception is thrown

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param host_lookup do a host lookup (if this is false the \c "hostname" and \c "hostname_desc" are not present in the response hash)

        @return a hash with the following keys:
        - \c hostname: the hostname for the local interface (if known or appropriate for the socket type)
        - \c hostname_desc: a descriptive string for the local hostname (including the socket type - ie "ipv6[host]")
        - \c address: the address of the local interface - for UNIX sockets this is the file path
        - \c address_desc: a descriptive string for the local interface
        - \c port: the port number if known
        - \c family: the address family (ie AF_INET, AF_INET6, AF_UNIX, ...)
        - \c familystr: a string description of the address family ("ipv4", "ipv6", etc)

        @since Qore 0.9.0
    */
    DLLEXPORT QoreHashNode* getDataSocketInfo(ExceptionSink* xsink, bool host_lookup) const;

    //! Sends a message on the control port and returns a FtpResponseInfo hash or nullptr (if an exception is thrown)
    /** @param cmd the command to send
        @param arg the argument for the command
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @since %Qore 0.9.4
    */
    DLLEXPORT QoreHashNode* sendControlMessage(const char* cmd, const char* arg, ExceptionSink* xsink);

    //! sets the socket I/O timeout value in milliseconds
    /** @since Qore 0.8.12.3
    */
    DLLLOCAL void setTimeout(int timeout_ms);

    //! returns the socket I/O timeout value as an integer in milliseconds
    /** @since Qore 0.8.12.3
    */
    DLLLOCAL int getTimeout() const;

    //! sets the same event queue for data and control sockets
    DLLLOCAL void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);

    //! sets the event queue for the data socket
    DLLLOCAL void setDataEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);

    //! sets the event queue for the control socket
    DLLLOCAL void setControlEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);

    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    //! private implementation of the object
    struct qore_ftp_private* priv;

    DLLLOCAL QoreFtpClient(const QoreFtpClient&) = delete;
    DLLLOCAL QoreFtpClient& operator=(const QoreFtpClient&) = delete;
};

#endif // _QORE_QOREFTPCLIENT_H
