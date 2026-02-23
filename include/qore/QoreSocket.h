/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSocket.h

    IPv4, IPv6, unix socket class with SSL support

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    will unlink (delete) UNIX domain socket files when closed

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

#ifndef _QORE_QORESOCKET_H

#define _QORE_QORESOCKET_H

#include <qore/Qore.h>
#include <qore/InputStream.h>
#include <qore/OutputStream.h>
#include <qore/AbstractPollState.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#define QSE_MISC_ERR      0 //!< error in errno
#define QSE_RECV_ERR     -1 //!< error in recv()
#define QSE_NOT_OPEN     -2 //!< socket is not open
#define QSE_TIMEOUT      -3 //!< timeout occured
#define QSE_SSL_ERR      -4 //!< SSL error occured
#define QSE_IN_OP        -5 //!< in another operation (socket call made in socket callback)
#define QSE_IN_OP_THREAD -6 //!< in another operation (socket call made in another thread while a callback operation is in progress)

// forward references
class Queue;
class QoreSSLCertificate;
class QoreSSLPrivateKey;

//! a helper class for getting socket origination information
/** objects of this class are used in some QoreSocket functions
    @see QoreSocket::accept()
    @see QoreSocket::acceptSSL()
    @see QoreSocket::acceptAndReplace()

    @deprecated use QoreSocket::getPeerInfo() and QoreSocket::getSocketInfo() to get more detailed information; this data structure will not be enhanced and will disappear in the next major version of the Qore library
 */
class SocketSource {
    friend struct qore_socket_private;

private:
    struct qore_socketsource_private *priv; // private implementation

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL SocketSource(const SocketSource&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL SocketSource& operator=(const SocketSource&);

public:
    //! creates an empty object
    DLLEXPORT SocketSource();

    //! destroys the object and frees all memory
    DLLEXPORT ~SocketSource();

    //! returns the host address string field and leaves the object's host address field empty; the caller owns the QoreStringNode reference count returned
    /** @return the host address string; the caller owns the QoreStringNode reference count returned
    */
    DLLEXPORT QoreStringNode* takeAddress();

    //! returns the hostname string field and leaves the object's hostname field empty; the caller owns the QoreStringNode reference count returned
    /** @return the hostname string; the caller owns the QoreStringNode reference count returned
    */
    DLLEXPORT QoreStringNode* takeHostName();

    //! returns the host address string as a C string
    /** @return the host address string as a C string
    */
    DLLEXPORT const char* getAddress() const;

    //! returns the hostname string as a C string
    /** @return the hostname string as a C string
    */
    DLLEXPORT const char* getHostName() const;

    DLLLOCAL void setAll(QoreObject* o, ExceptionSink* xsink);
};

//! provides access to sockets using Qore data structures
/** QoreSocket objects also have a QoreEncoding associated with them used for sending
    and receiving string data.  String data received from the socket will be tagged with
    the appropriate encoding; string data send through the socket will be implicitly
    converted to the socket's encoding if necessary.
    This class does no implement any thread locking; thread locking must be performed at
    a higher level (for example, as with QoreHTTPClient and QoreFtpClient).

    @note currently only supports TCP sockets
    @see QoreEncoding
*/
class QoreSocket {
    friend struct qore_socket_private;
    friend struct qore_httpclient_priv;
    friend class QoreSocketObject;
    friend class QoreFtpClient;
    friend class HttpClientRecvHeaderPollState;
    friend class HttpClientRecvChunkedPollState;
    friend class HttpClientRecvUntilClosePollState;
    friend class HttpClientConnectSendRecvPollOperation;
    friend class SocketAcceptPollOperation;
    friend class SocketReadHttpHeaderPollOperation;
    friend class SocketSendAndReadHeaderPollOperation;
    friend class my_socket_priv;
    friend class SocketHttp2ServerPollOperation;
    friend class SocketHttp2SendResponsePollOperation;
    friend class SocketHttp2SendStreamingResponsePollOperation;
    friend class SocketHttp2FlushPollOperation;
    friend class SocketHttp2ClientMultiplexPollOperation;

public:
    //! creates an empty, unconnected socket
    DLLEXPORT QoreSocket();

    //! disconnects if necessary, frees all data, and destroys the socket
    DLLEXPORT ~QoreSocket();

    //! Starts a non-blocking connection to a socket and returns a poll state object
    /** If "name" has a ':' in it; it's assumed to be a hostname:port specification and QoreSocket::startConnectINET() is called.
        Otherwise "name" is assumed to be a file name for a UNIX domain socket and QoreSocket::startConnectUNIX() is called.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param name the name of the socket (either hostname:port or file name)

        @return a socket poll state object or nullptr in case of an exception or an immediate connection

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startConnect(ExceptionSink* xsink, const char* name);

    //! Starts a non-blocking upgrade to an SSL connection on a connected client connection
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param cert the client certificate to use
        @param pkey the private key to use

        @return a socket poll state object or nullptr in case of an exception or an immediate connection

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startSslConnect(ExceptionSink* xsink, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

    //! Starts a non-blocking send operation on a connected socket
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param data the data to send, must stay valid for the lifetime of the AbstractPollState object returned
        @param size the size of the data to send

        @return a socket poll state object or nullptr in case of an exception or an immediate send

        @since %Qore 1.12
     */
    DLLEXPORT AbstractPollState* startSend(ExceptionSink* xsink, const char* data, size_t size);

    //! Starts a non-blocking accept operation to a socket and returns a poll state object
    /** The socket must be already bound and listening

        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a socket poll state object or nullptr in case of an exception or an immediate accept

        @since %Qore 2.0
    */
    DLLEXPORT AbstractPollState* startAccept(ExceptionSink* xsink);

    //! Starts a non-blocking upgrade to an SSL connection on a connected server connection
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param cert the server certificate to use
        @param pkey the private key to use

        @return a socket poll state object or nullptr in case of an exception or an immediate connection

        @since %Qore 2.0
    */
    DLLEXPORT AbstractPollState* startSslAccept(ExceptionSink* xsink, QoreSSLCertificate* cert = nullptr,
        QoreSSLPrivateKey* pkey = nullptr);

    //! Starts a non-blocking receive operation on a connected socket
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param size the size of the data to read, must be > 0

        @return a socket poll state object or nullptr in case of an exception or an immediate receive

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startRecv(ExceptionSink* xsink, size_t size);

    //! Starts a non-blocking receive operation on a connected socket
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param pattern the bytes to expect that indicate the end of the data, must stay valid for the lifetime of the
        AbstractPollState object returned
        @param size the size of pattern in bytes

        @return a socket poll state object or nullptr in case of an exception or an immediate receive

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startRecvUntilBytes(ExceptionSink* xsink, const char* pattern, size_t size);

    //! Starts a non-blocking receive packet operation on a connected socket
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a socket poll state object or nullptr in case of an exception or an immediate receive

        @since %Qore 2.0
    */
    DLLEXPORT AbstractPollState* startRecvPacket(ExceptionSink* xsink);

#if 0
    //! Starts a non-blocking accept operation on the socket
    /**
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startAccept(ExceptionSink* xsink);

    //! Starts a non-blocking upgrade to an SSL connection on a connected server connection
    /**
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startSslAccept(ExceptionSink* xsink, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);
#endif

    //! connects to a socket and returns a status code, Qore-language exceptions are raised in the case of any errors
    /** If "name" has a ':' in it; it's assumed to be a hostname:port specification and QoreSocket::connectINET() is called.
        Otherwise "name" is assumed to be a file name for a UNIX domain socket and QoreSocket::connectUNIX() is called.
        @param name the name of the socket (either hostname:port or file name)
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here
        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised
        @see QoreSocket::connectINET()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::connectUNIXSSL()
    */
    DLLEXPORT int connect(const char* name, ExceptionSink* xsink = nullptr);

    //! connects to a socket and returns a status code, Qore-language exceptions are raised in the case of any errors
    /** If "name" has a ':' in it; it's assumed to be a hostname:port specification and QoreSocket::connectINET() is called.
        Otherwise "name" is assumed to be a file name for a UNIX domain socket and QoreSocket::connectUNIX() is called.
        @param name the name of the socket (either hostname:port or file name)
        @param timeout_ms the timeout period in milliseconds
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here
        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised
        @see QoreSocket::connectINET()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::connectUNIXSSL()
    */
    DLLEXPORT int connect(const char* name, int timeout_ms, ExceptionSink* xsink = nullptr);

    //! connects to an INET socket by hostname and port number and returns a status code, Qore-language exceptions are raised in the case of any errors
    /** @param host the name or IP address of the host
        @param prt the port number of the remote socket
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here
        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised
        @see QoreSocket::connect()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::connectUNIXSSL()
    */
    DLLEXPORT int connectINET(const char* host, int prt, ExceptionSink* xsink = nullptr);

    //! connects to an INET or INET6 socket by hostname and port number or service name and returns a status code, Qore-language exceptions are raised in the case of any errors
    /** @param name the name or address of the host
        @param service the port number or service name of the remote socket
        @param family should be either AF_INET (for ipv4) or AF_INET6 (for ipv6)
        @param sock_type the type of socket, normally SOCK_STREAM for tcp sockets or SOCK_DGRAM for udp sockets
        @param protocol the protocol number, use 0 for the default
        @param timeout_ms the timeout period in milliseconds
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here
        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised
        @see QoreSocket::connect()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::connectUNIXSSL()
    */
    DLLEXPORT int connectINET2(const char* name, const char* service, int family = Q_AF_UNSPEC,
            int sock_type = Q_SOCK_STREAM, int protocol = 0, int timeout_ms = -1, ExceptionSink* xsink = nullptr);

    //! connects to an INET socket by hostname and port number and returns a status code, Qore-language exceptions are raised in the case of any errors
    /** @param host the name or IP address of the host
        @param prt the port number of the remote socket
        @param timeout_ms the timeout period in milliseconds
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here
        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised
        @see QoreSocket::connect()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::connectUNIXSSL()
    */
    DLLEXPORT int connectINET(const char* host, int prt, int timeout_ms, ExceptionSink* xsink = nullptr);

    //! connects to a UNIX domain socket and returns a status code, Qore-language exceptions are raised in the case of any errors
    /** @param p the file name of the UNIX domain socket
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here
        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised
        @see QoreSocket::connect()
        @see QoreSocket::connectINET()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::connectUNIXSSL()
    */
    DLLEXPORT int connectUNIX(const char* p, ExceptionSink* xsink = nullptr);

    //! connects to a UNIX domain socket and returns a status code, Qore-language exceptions are raised in the case of any errors
    /** @param p the file name of the UNIX domain socket
        @param socktype the type of socket (SOCK_STREAM = tcp socket)
        @param protocol the protocol for the socket (use 0 for default)
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here
        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised
        @see QoreSocket::connect()
        @see QoreSocket::connectINET()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::connectUNIXSSL()
    */
    DLLEXPORT int connectUNIX(const char* p, int socktype, int protocol = 0, ExceptionSink* xsink = nullptr);

    //! connects to a socket, negotiates an SSL connection, and returns a status code, Qore-language exceptions are raised in the case of any errors
    /** If "name" has a ':' in it; it's assumed to be a hostname:port specification and QoreSocket::connectINETSSL() is called.
        Otherwise "name" is assumed to be a file name for a UNIX domain socket and QoreSocket::connectUNIXSSL() is called.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param name the name of the socket (either hostname:port or file name)
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised

        @note the same as calling QoreSocket::connect() and then QoreSocket::upgradeClientToSSL()

        @see QoreSocket::connect()
        @see QoreSocket::connectINET()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::connectUNIXSSL()
        @see QoreSocket::upgradeClientToSSL()
    */
    DLLEXPORT int connectSSL(ExceptionSink* xsink, const char* name, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

    //! connects to a socket, negotiates an SSL connection, and returns a status code, Qore-language exceptions are raised in the case of any errors
    /** If "name" has a ':' in it; it's assumed to be a hostname:port specification and QoreSocket::connectINETSSL() is called.
        Otherwise "name" is assumed to be a file name for a UNIX domain socket and QoreSocket::connectUNIXSSL() is called.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param name the name of the socket (either hostname:port or file name)
        @param timeout_ms the timeout period in milliseconds
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised

        @note the same as calling QoreSocket::connect() and then QoreSocket::upgradeClientToSSL()

        @see QoreSocket::connect()
        @see QoreSocket::connectINET()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::connectUNIXSSL()
        @see QoreSocket::upgradeClientToSSL()
    */
    DLLEXPORT int connectSSL(ExceptionSink* xsink, const char* name, int timeout_ms, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

    //! connects to an INET socket by hostname and port number, negotiates an SSL connection, and returns a status code, Qore-language exceptions are raised in the case of any errors
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param host the name or IP address of the host
        @param prt the port number of the remote socket
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised

        @note the same as calling QoreSocket::connectINET() and then QoreSocket::upgradeClientToSSL()

        @see QoreSocket::connect()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectINET()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectUNIXSSL()
        @see QoreSocket::upgradeClientToSSL()
    */
    DLLEXPORT int connectINETSSL(ExceptionSink* xsink, const char* host, int prt, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

    //! connects to an INET socket by hostname and port number, negotiates an SSL connection, and returns a status code, Qore-language exceptions are raised in the case of any errors
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param host the name or IP address of the host
        @param prt the port number of the remote socket
        @param timeout_ms the timeout period in milliseconds
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised

        @note the same as calling QoreSocket::connectINET() and then QoreSocket::upgradeClientToSSL()

        @see QoreSocket::connect()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectINET()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectUNIXSSL()
        @see QoreSocket::upgradeClientToSSL()
    */
    DLLEXPORT int connectINETSSL(ExceptionSink* xsink, const char* host, int prt, int timeout_ms,
            QoreSSLCertificate* cert = nullptr, QoreSSLPrivateKey* pkey = nullptr);

    //! connects to an INET or INET6 socket by hostname and port number or service name and returns a status code, Qore-language exceptions are raised in the case of any errors
    /** @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here
        @param name the name or address of the host
        @param service the port number or service name of the remote socket
        @param family should be either AF_INET (for ipv4) or AF_INET6 (for ipv6)
        @param sock_type the type of socket, normally SOCK_STREAM for tcp sockets or SOCK_DGRAM for udp sockets
        @param protocol the protocol number, use 0 for the default
        @param timeout_ms the timeout period in milliseconds
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised

        @see QoreSocket::connect()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::connectUNIXSSL()
    */
    DLLEXPORT int connectINET2SSL(ExceptionSink* xsink, const char* name, const char* service, int family,
            int sock_type, int protocol, int timeout_ms, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

    //! connects to a UNIX domain socket, negotiates an SSL connection, and returns a status code, Qore-language exceptions are raised in the case of any errors
    /**
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here
        @param p the file name of the UNIX domain socket
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised

        @note the same as calling QoreSocket::connectUNIX() and then QoreSocket::upgradeClientToSSL()

        @see QoreSocket::connect()
        @see QoreSocket::connectINET()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::upgradeClientToSSL()
    */
    DLLEXPORT int connectUNIXSSL(ExceptionSink* xsink, const char* p, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

    //! connects to a UNIX domain socket, negotiates an SSL connection, and returns a status code, Qore-language exceptions are raised in the case of any errors
    /**
        @param xsink if not 0, if an error occurs, the Qore-language exception information will be added here
        @param p the file name of the UNIX domain socket
        @param socktype the type of socket (SOCK_STREAM = tcp socket)
        @param protocol the protocol for the socket (use 0 for default)
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return 0 for OK, -1 means that an error occured and a Qore-language exception was raised

        @note the same as calling QoreSocket::connectUNIX() and then QoreSocket::upgradeClientToSSL()

        @see QoreSocket::connect()
        @see QoreSocket::connectINET()
        @see QoreSocket::connectUNIX()
        @see QoreSocket::connectSSL()
        @see QoreSocket::connectINETSSL()
        @see QoreSocket::upgradeClientToSSL()
    */
    DLLEXPORT int connectUNIXSSL(ExceptionSink* xsink, const char* p, int socktype, int protocol,
            QoreSSLCertificate* cert = nullptr, QoreSSLPrivateKey* pkey = nullptr);

    //! binds to a UNIX domain socket or INET interface:port using TCP and returns a status code
    /** @note a socket file will be created on the filesystem if a UNIX domain socket is opened.
        @note the socket will be closed and reopened if necessary
        @param name address:port or filename to bind to; if the name has a ':' in it; it's assumed to be an address:port specification for binding to an INET socket, otherwise it is assumed to be a file name for a UNIX domain socket.  If there is more than 1 ':' in the name, then the address is assumed to be an ipv6 interface address, otherwise it is assumed to be an ipv4 address.
        @param reuseaddr if true then setsockopt() will be called with SO_REUSEADDR, allowing the bind to succeed even if the port is still in a TIME_WAIT state, for example
        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int bind(const char* name, bool reuseaddr = false);

    //! binds to a TCP INET (ipv4 only) port on all interfaces and returns a status code
    /** @note the socket will be closed and reopened if necessary
        @param prt the port to bind to
        @param reuseaddr if true then setsockopt() will be called with SO_REUSEADDR, allowing the bind to succeed even if the port is still in a TIME_WAIT state, for example
        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int bind(int prt, bool reuseaddr);

    //! binds to a TCP INET (ipv4 only) port on the given interface and returns a status code
    /** @note the socket will be closed and reopened if necessary
         @param iface the interface to bind to (hostname or IP address)
        @param prt the port to bind to
        @param reuseaddr if true then setsockopt() will be called with SO_REUSEADDR, allowing the bind to succeed even if the port is still in a TIME_WAIT state, for example
        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int bind(const char* iface, int prt, bool reuseaddr = false);

    //! binds an INET TCP socket to a specific socket address - do not use, deprecated, use bind(int family, const struct sockaddr *addr, int addr_size, int socktype, int protocol) instead
    /** this function will not set the port number; use bind(int family, const struct sockaddr *addr, int addr_size) instead
        @note the socket will be closed and reopened if necessary
        @param addr the socket address to bind to
        @param addr_size the size of the addr argument
        @return 0 for OK, not 0 for error
        @deprecated use bind(int family, const struct sockaddr *addr, int addr_size, int socktype, int protocol) instead
    */
    DLLEXPORT int bind(const struct sockaddr *addr, int addr_size);

    //! binds an INET or INET6 TCP socket to a specific socket address
    /** @note the socket will be closed and reopened if necessary
        @param family the address family (AF_INET or AF_INET6)
        @param addr the socket address to bind to
        @param addr_size the size of the addr argument
        @param socktype the type of socket (SOCK_STREAM = tcp socket)
        @param protocol the protocol for the socket
        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int bind(int family, const struct sockaddr *addr, int addr_size, int socktype = Q_SOCK_STREAM,
            int protocol = 0);

    //! binds to a UNIX domain socket and returns a status code
    /** @note a socket file will be created on the filesystem if a UNIX domain socket is opened.
        @note the socket will be closed and reopened if necessary

        @param name UNIX filename to bind to
        @param xsink if not 0 and an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int bindUNIX(const char* name, ExceptionSink* xsink = nullptr);

    //! binds to a UNIX domain socket and returns a status code
    /** @note a socket file will be created on the filesystem if a UNIX domain socket is opened.
        @note the socket will be closed and reopened if necessary

        @param name UNIX filename to bind to
        @param socktype the type of socket (SOCK_STREAM = tcp socket)
        @param protocol the protocol for the socket (use 0 for default)
        @param xsink if not 0 and an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int bindUNIX(const char* name, int socktype, int protocol = 0, ExceptionSink* xsink = nullptr);

    //! binds an INET or INET6 TCP socket to a specific socket address
    /** @note the socket will be closed and reopened if necessary

        @param name the name or address of the interface, can be 0 meaning all interfaces
        @param service the service name or port number
        @param reuseaddr if true then setsockopt() will be called with SO_REUSEADDR, allowing the bind to succeed even
        if the port is still in a TIME_WAIT state, for example
        @param family the address family (AF_INET, AF_INET6, or AF_UNSPEC)
        @param socktype the type of socket (SOCK_STREAM = tcp socket)
        @param protocol the protocol for the socket
        @param xsink if not 0 and an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int bindINET(const char* name, const char* service, bool reuseaddr = true, int family = Q_AF_UNSPEC,
            int socktype = Q_SOCK_STREAM, int protocol = 0, ExceptionSink* xsink = nullptr);

    //! returns the TCP port number, also assigns the interal port number if it must be discovered
    DLLEXPORT int getPort();

    //! accepts a new connection on a listening socket and returns a new QoreSocket object for the new connection
    /** the socket must be opened and in a listening state before making this call.

        @param source source connection information will be written to this object if not 0
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a new QoreSocket object for the new connection (or 0 if an error occured)

        @see
        - QoreSocket::listen()
        - QoreSocket::acceptSSL()
        - SocketSource
    */
    DLLEXPORT QoreSocket* accept(SocketSource* source, ExceptionSink* xsink);

    //! accepts a new connection on a listening socket, negotiates an SSL connection, and returns a new QoreSocket object for the new connection
    /** the socket must be opened and in a listening state before making this call.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param source source connection information will be written to this object if not 0
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return a new QoreSocket object for the new connection (or 0 if an error occured)

        @note the same as calling QoreSocket::accept() and then QoreSocket::upgradeServerToSSL() on the new socket

        @see
        - QoreSocket::listen()
        - SocketSource
    */
    DLLEXPORT QoreSocket* acceptSSL(ExceptionSink* xsink, SocketSource* source, QoreSSLCertificate* cert,
            QoreSSLPrivateKey* pkey);

    //! accepts a new connection on a listening socket and replaces the current socket with the new connection
    /** the socket must be opened and in a listening state before making this call.

        @param source source connection information will be written to this object if not 0

        @return 0 for OK, not 0 if an error occured

        @see
        - QoreSocket::listen()
        - QoreSocket::accept()
        - QoreSocket::acceptSSL()
        - SocketSource
    */
    DLLEXPORT int acceptAndReplace(SocketSource* source);

    //! accepts a new connection on a listening socket and returns a new QoreSocket object for the new connection with a timeout; if no connection is accepted within the timeout period 0 is returned
    /** the socket must be opened and in a listening state before making this call.

        @param timeout_ms the timeout in milliseconds; if a timeout occurs then 0 is returned (no Qore-language excepton is raised)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a new QoreSocket object for the new connection (or 0 if an error or timeout occured)

        @see
        - QoreSocket::listen()
        - QoreSocket::acceptSSL(ExceptionSink* xsink, int timeout_ms, )QoreSSLCertificate* cert,
          QoreSSLPrivateKey* pkey
    */
    DLLEXPORT QoreSocket* accept(int timeout_ms, ExceptionSink* xsink);

    //! accepts a new connection on a listening socket, negotiates an SSL connection, and returns a new QoreSocket object for the new connection with a timeout; if no connection is accepted within the timeout period 0 is returned
    /** the socket must be opened and in a listening state before making this call.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param timeout_ms the timeout in milliseconds; if a timeout occurs then 0 is returned (no Qore-language
        excepton is raised)
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return a new QoreSocket object for the new connection (or 0 if an error or timeout occured)

        @note the same as calling QoreSocket::accept() and then QoreSocket::upgradeServerToSSL() on the new socket

        @see
        - QoreSocket::accept(int timeout_ms, ExceptionSink* xsink)
        - QoreSocket::listen()
    */
    DLLEXPORT QoreSocket* acceptSSL(ExceptionSink* xsink, int timeout_ms, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

    //! accepts a new connection on a listening socket and replaces the current socket with the new connection with a timeout; if no connection is accepted within the timeout period, -3 (QSE_TIMEOUT) is returned
    /** the socket must be opened and in a listening state before making this call.
        @param timeout_ms the timeout in milliseconds; if a timeout occurs then 0 is returned (no Qore-language excepton is raised)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, -1 if an error occured, -3 (QSE_TIMEOUT) if a timeout occured

        @see
        - QoreSocket::listen()
        - QoreSocket::accept(int timeout_ms, ExceptionSink* xsink)
        - QoreSocket::acceptSSL(ExceptionSink* xsink, int timeout_ms, QoreSSLCertificate* cert,
          QoreSSLPrivateKey* pkey)
    */
    DLLEXPORT int acceptAndReplace(int timeout_ms, ExceptionSink* xsink);

    //! sets an open socket to the listening state
    /**
        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int listen();

    //! sets an open socket to the listening state
    /** @param backlog the size of the queue for pending connections

        @return 0 for OK, not 0 if an error occured

        @since Qore 0.8.8
    */
    DLLEXPORT int listen(int backlog);

    //! sends binary data on a connected socket
    /**
        @param buf the data to send
        @param size the size of the data to send

        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int send(const char* buf, size_t size);

    //! sends binary data on a connected socket
    /**
        @param buf the data to send
        @param size the size of the data to send
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int send(const char* buf, size_t size, ExceptionSink* xsink);

    //! sends binary data on a connected socket
    /**
        @param buf the data to send
        @param size the size of the data to send
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int send(const char* buf, size_t size, int timeout_ms, ExceptionSink* xsink);

    //! sends string data on a connected socket, converts the string encoding to the socket's encoding if necessary
    /**
        @param msg the string to send (must not be 0)
        @param xsink if an error occurs in converting the string's character encoding or in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int send(const QoreString *msg, ExceptionSink* xsink);

    //! sends string data on a connected socket, converts the string encoding to the socket's encoding if necessary
    /**
        @param msg the string to send (must not be 0)
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in converting the string's character encoding or in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int send(const QoreString *msg, int timeout_ms, ExceptionSink* xsink);

    //! sends string data on a connected socket, converts the string encoding to the socket's encoding if necessary
    /**
        @param msg the string to send (must not be 0)
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in converting the string's character encoding or in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured

        @since %Qore 0.9.4
    */
    DLLEXPORT int send(const QoreStringNode& msg, int timeout_ms, ExceptionSink* xsink);

    //! sends binary data on a connected socket
    /**
        @param msg the data to send

        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int send(const BinaryNode* msg);

    //! sends binary data on a connected socket
    /**
        @param msg the data to send
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int send(const BinaryNode* msg, ExceptionSink* xsink);

    //! sends binary data on a connected socket
    /**
        @param msg the data to send
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int send(const BinaryNode* msg, int timeout_ms, ExceptionSink* xsink);

    //! sends untranslated data from an open file descriptor
    /**
        @param fd a file descriptor, open for reading
        @param size the number of bytes to send (-1 = send all until EOF)

        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int send(int fd, qore_offset_t size = -1);

    //! sends untranslated data from an open file descriptor
    /**
        @param fd a file descriptor, open for reading
        @param size the number of bytes to send (-1 = send all until EOF)
        @param timeout_ms a timeout in milliseconds for socket I/O operations
        @param xsink if an I/O error occurs, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured

        @since Qore 0.8.12.3
    */
    DLLEXPORT int send(int fd, qore_offset_t size, int timeout_ms, ExceptionSink* xsink);

    //! sends a 1-byte binary integer data to a connected socket
    /** The socket must be connected before this call is made.
        @param i the 1-byte integer to send through the socket
        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int sendi1(char i);

    //! sends a 2-byte (16bit) binary integer in MSB (Most Significant Byte first, big endian, network) format through a connected socket
    /** The socket must be connected before this call is made.
        @param i the integer to write to the file
        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi2LSB()
    */
    DLLEXPORT int sendi2(short i);

    //! sends a 4-byte (32bit) binary integer in MSB (Most Significant Byte first, big endian, network) format through a connected socket
    /** The socket must be connected before this call is made.
        @param i the integer to write to the file
        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi4LSB()
    */
    DLLEXPORT int sendi4(int i);

    //! sends an 8-byte (64bit) binary integer in MSB (Most Significant Byte first, big endian, network) format through a connected socket
    /** The socket must be connected before this call is made.
        @param i the integer to write to the file
        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi8LSB()
    */
    DLLEXPORT int sendi8(int64 i);

    //! sends a 2-byte (16bit) binary integer in LSB (Least Significant Byte first, little endian) format through a connected socket
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::sendi2() should be used
        @param i the integer to write to the file
        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi2()
    */
    DLLEXPORT int sendi2LSB(short i);

    //! sends a 4-byte (32bit) binary integer in LSB (Least Significant Byte first, little endian) format through a connected socket
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::sendi4() should be used
        @param i the integer to write to the file
        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi4()
    */
    DLLEXPORT int sendi4LSB(int i);

    //! sends an 8-byte (64bit) binary integer in LSB (Least Significant Byte first, little endian) format through a connected socket
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::sendi8() should be used
        @param i the integer to write to the file
        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi8()
    */
    DLLEXPORT int sendi8LSB(int64 i);

    //! sends a 1-byte binary integer data to a connected socket
    /** The socket must be connected before this call is made.

        @param i the 1-byte integer to send through the socket
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
    */
    DLLEXPORT int sendi1(char i, int timeout_ms, ExceptionSink* xsink);

    //! sends a 2-byte (16bit) binary integer in MSB (Most Significant Byte first, big endian, network) format through a connected socket
    /** The socket must be connected before this call is made.

        @param i the integer to write to the file
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi2LSB()
    */
    DLLEXPORT int sendi2(short i, int timeout_ms, ExceptionSink* xsink);

    //! sends a 4-byte (32bit) binary integer in MSB (Most Significant Byte first, big endian, network) format through a connected socket
    /** The socket must be connected before this call is made.

        @param i the integer to write to the file
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi4LSB()
    */
    DLLEXPORT int sendi4(int i, int timeout_ms, ExceptionSink* xsink);

    //! sends an 8-byte (64bit) binary integer in MSB (Most Significant Byte first, big endian, network) format through a connected socket
    /** The socket must be connected before this call is made.

        @param i the integer to write to the file
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi8LSB()
    */
    DLLEXPORT int sendi8(int64 i, int timeout_ms, ExceptionSink* xsink);

    //! sends a 2-byte (16bit) binary integer in LSB (Least Significant Byte first, little endian) format through a connected socket
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::sendi2() should be used

        @param i the integer to write to the file
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi2()
    */
    DLLEXPORT int sendi2LSB(short i, int timeout_ms, ExceptionSink* xsink);

    //! sends a 4-byte (32bit) binary integer in LSB (Least Significant Byte first, little endian) format through a connected socket
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::sendi4() should be used

        @param i the integer to write to the file
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi4()
    */
    DLLEXPORT int sendi4LSB(int i, int timeout_ms, ExceptionSink* xsink);

    //! sends an 8-byte (64bit) binary integer in LSB (Least Significant Byte first, little endian) format through a connected socket
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::sendi8() should be used

        @param i the integer to write to the file
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return 0 for OK, not 0 if an error occured
        @see QoreSocket::sendi8()
    */
    DLLEXPORT int sendi8LSB(int64 i, int timeout_ms, ExceptionSink* xsink);

    //! reads a 1-byte signed integer from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvu1()
    **/
    DLLEXPORT int recvi1(int timeout_ms, char* val);

    //! reads a 2-byte signed integer in MSB (Most Significant Byte first, big endian, network) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvi2LSB()
        @see QoreSocket::recvu2()
        @see QoreSocket::recvu2LSB()
    **/
    DLLEXPORT int recvi2(int timeout_ms, short *val);

    //! reads a 4-byte signed integer in MSB (Most Significant Byte first, big endian, network) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvi4LSB()
        @see QoreSocket::recvu4()
        @see QoreSocket::recvu4LSB()
    **/
    DLLEXPORT int recvi4(int timeout_ms, int* val);

    //! reads an 8-byte signed integer in MSB (Most Significant Byte first, big endian, network) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvi8LSB()
        @see QoreSocket::recvu8()
        @see QoreSocket::recvu8LSB()
    **/
    DLLEXPORT int recvi8(int timeout_ms, int64 *val);

    //! reads a 2-byte signed integer in LSB (Most Significant Byte first, little endian) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::recvi2() should be used instead
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvi2()
        @see QoreSocket::recvu2()
        @see QoreSocket::recvu2LSB()
    **/
    DLLEXPORT int recvi2LSB(int timeout_ms, short *val);

    //! reads a 4-byte signed integer in LSB (Most Significant Byte first, little endian) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::recvi4() should be used instead
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvi4()
        @see QoreSocket::recvu4()
        @see QoreSocket::recvu4LSB()
    **/
    DLLEXPORT int recvi4LSB(int timeout_ms, int* val);

    //! reads an 8-byte signed integer in LSB (Most Significant Byte first, little endian) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::recvi8() should be used instead
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvi8()
    **/
    DLLEXPORT int recvi8LSB(int timeout_ms, int64 *val);

    //! reads a 1-byte unsigned integer from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvi1()
    **/
    DLLEXPORT int recvu1(int timeout_ms, unsigned char* val);

    //! reads a 2-byte unsigned integer in MSB (Most Significant Byte first, big endian, network) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvi2()
        @see QoreSocket::recvu2LSB()
        @see QoreSocket::recvi2LSB()
    **/
    DLLEXPORT int recvu2(int timeout_ms, unsigned short *val);

    //! reads a 4-byte unsigned integer in MSB (Most Significant Byte first, big endian, network) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvi4()
        @see QoreSocket::recvu4LSB()
        @see QoreSocket::recvi4LSB()
    **/
    DLLEXPORT int recvu4(int timeout_ms, unsigned int* val);

    //! reads a 2-byte unsigned integer in LSB (Most Significant Byte first, little endian) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::recvu2() should be used instead

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvu2()
        @see QoreSocket::recvi2()
        @see QoreSocket::recvi2LSB()
    **/
    DLLEXPORT int recvu2LSB(int timeout_ms, unsigned short *val);

    //! reads a 4-byte unsigned integer in LSB (Most Significant Byte first, little endian) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::recvu4() should be used instead

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file

        @return 0 for OK, not 0 for error

        @see QoreSocket::recvu4()
        @see QoreSocket::recvi4()
        @see QoreSocket::recvi4LSB()
    **/
    DLLEXPORT int recvu4LSB(int timeout_ms, unsigned int* val);

    //! reads a 1-byte signed integer from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvu1()
    **/
    DLLEXPORT int64 recvi1(int timeout_ms, char* val, ExceptionSink* xsink);

    //! reads a 2-byte signed integer in MSB (Most Significant Byte first, big endian, network) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvi2LSB()
        @see QoreSocket::recvu2()
        @see QoreSocket::recvu2LSB()
    **/
    DLLEXPORT int64 recvi2(int timeout_ms, short *val, ExceptionSink* xsink);

    //! reads a 4-byte signed integer in MSB (Most Significant Byte first, big endian, network) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvi4LSB()
        @see QoreSocket::recvu4()
        @see QoreSocket::recvu4LSB()
    **/
    DLLEXPORT int64 recvi4(int timeout_ms, int* val, ExceptionSink* xsink);

    //! reads an 8-byte signed integer in MSB (Most Significant Byte first, big endian, network) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvi8LSB()
        @see QoreSocket::recvu8()
        @see QoreSocket::recvu8LSB()
    **/
    DLLEXPORT int64 recvi8(int timeout_ms, int64 *val, ExceptionSink* xsink);

    //! reads a 2-byte signed integer in LSB (Most Significant Byte first, little endian) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::recvi2() should be used instead

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvi2()
        @see QoreSocket::recvu2()
        @see QoreSocket::recvu2LSB()
    **/
    DLLEXPORT int64 recvi2LSB(int timeout_ms, short *val, ExceptionSink* xsink);

    //! reads a 4-byte signed integer in LSB (Most Significant Byte first, little endian) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::recvi4() should be used instead

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvi4()
        @see QoreSocket::recvu4()
        @see QoreSocket::recvu4LSB()
    **/
    DLLEXPORT int64 recvi4LSB(int timeout_ms, int* val, ExceptionSink* xsink);

    //! reads an 8-byte signed integer in LSB (Most Significant Byte first, little endian) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.
        @note that this is not network byte order, normally QoreSocket::recvi8() should be used instead

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvi8()
    **/
    DLLEXPORT int64 recvi8LSB(int timeout_ms, int64 *val, ExceptionSink* xsink);

    //! reads a 1-byte unsigned integer from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvi1()
    **/
    DLLEXPORT int64 recvu1(int timeout_ms, unsigned char* val, ExceptionSink* xsink);

    //! reads a 2-byte unsigned integer in MSB (Most Significant Byte first, big endian, network) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvi2()
        @see QoreSocket::recvu2LSB()
        @see QoreSocket::recvi2LSB()
    **/
    DLLEXPORT int64 recvu2(int timeout_ms, unsigned short *val, ExceptionSink* xsink);

    //! reads a 4-byte unsigned integer in MSB (Most Significant Byte first, big endian, network) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvi4()
        @see QoreSocket::recvu4LSB()
        @see QoreSocket::recvi4LSB()
    **/
    DLLEXPORT int64 recvu4(int timeout_ms, unsigned int* val, ExceptionSink* xsink);

    //! reads a 2-byte unsigned integer in LSB (Most Significant Byte first, little endian) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.

        @note that this is not network byte order, normally QoreSocket::recvu2() should be used instead

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvu2()
        @see QoreSocket::recvi2()
        @see QoreSocket::recvi2LSB()
    **/
    DLLEXPORT int64 recvu2LSB(int timeout_ms, unsigned short *val, ExceptionSink* xsink);

    //! reads a 4-byte unsigned integer in LSB (Most Significant Byte first, little endian) format from the socket with a timeout value and returns the value read as an output parameter
    /** The socket must be connected before this call is made.

        @note that this is not network byte order, normally QoreSocket::recvu4() should be used instead

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param val output parameter: the integer value read from the file
        @param xsink if an error occurs in socket communication, the Qore-language exception information will be added here

        @return <= 0 = error, otherwise the number of bytes received

        @see QoreSocket::recvu4()
        @see QoreSocket::recvi4()
        @see QoreSocket::recvi4LSB()
    **/
    DLLEXPORT int64 recvu4LSB(int timeout_ms, unsigned int* val, ExceptionSink* xsink);

    //! receive a certain number of bytes with a timeout value and return a QoreStringNode, caller owns the reference count returned
    /** The socket must be connected before this call is made.
        @param bufsize number of bytes to read from the socket; if <= 0, read all data available from the socket until the socket is closed from the other side
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param prc output parameter: 0 for OK, not 0 for error

        @return the data read as a QoreStringNode tagged with the socket's QoreEncoding, caller owns the reference count returned (0 if an error occurs)

        @see QoreEncoding
    */
    DLLEXPORT QoreStringNode* recv(qore_offset_t bufsize, int timeout_ms, int* prc);

    //! receive a certain number of bytes with a timeout value and return a QoreStringNode, caller owns the reference count returned
    /** The socket must be connected before this call is made.
        @param bufsize number of bytes to read from the socket; if <= 0, read all data available from the socket until the socket is closed from the other side
        @param timeout_ms in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the data read as a QoreStringNode tagged with the socket's QoreEncoding, caller owns the reference count returned (0 if an error occurs)

        @see QoreEncoding
    */
    DLLEXPORT QoreStringNode* recv(qore_offset_t bufsize, int timeout_ms, ExceptionSink* xsink);

    //! receive a certain number of bytes with a timeout value and return a BinaryNode, caller owns the reference count returned
    /** The socket must be connected before this call is made.
        @param bufsize number of bytes to read from the socket; if <= 0, read all data available from the socket until the socket is closed from the other side
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param prc output parameter: 0 for OK, not 0 for error

        @return the data read as a BinaryNode, caller owns the reference count returned (0 if an error occurs)
    */
    DLLEXPORT BinaryNode* recvBinary(qore_offset_t bufsize, int timeout_ms, int* prc);

    //! receive a certain number of bytes with a timeout value and return a BinaryNode, caller owns the reference count returned
    /** The socket must be connected before this call is made.
        @param bufsize number of bytes to read from the socket; if <= 0, read all data available from the socket until the socket is closed from the other side
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the data read as a BinaryNode, caller owns the reference count returned (0 if an error occurs)
    */
    DLLEXPORT BinaryNode* recvBinary(qore_offset_t bufsize, int timeout_ms, ExceptionSink* xsink);

    //! receive with a timeout value and return a QoreStringNode, caller owns the reference count returned
    /** The socket must be connected before this call is made.
        This call will read data, blocking according to the timeout value.  Then all data
        available on the socket will be read and returned as a QoreStringNode.  As soon as the
        first timeout occurs, the data will be returned immediately without blocking.
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param prc output parameter: 0 for OK, not 0 for error

        @return the data read as a QoreStringNode tagged with the socket's QoreEncoding, caller owns the reference count returned (0 if an error occurs)

        @see QoreEncoding
    */
    DLLEXPORT QoreStringNode* recv(int timeout_ms, int* prc);

    //! receive with a timeout value and return a QoreStringNode, caller owns the reference count returned
    /** The socket must be connected before this call is made.
        This call will read data, blocking according to the timeout value.  Then all data
        available on the socket will be read and returned as a QoreStringNode.  As soon as the
        first timeout occurs, the data will be returned immediately without blocking.
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the data read as a QoreStringNode tagged with the socket's QoreEncoding, caller owns the reference count returned (0 if an error occurs)

        @see QoreEncoding
    */
    DLLEXPORT QoreStringNode* recv(int timeout_ms, ExceptionSink* xsink);

    //! receive with a timeout value and return a BinaryNode, caller owns the reference count returned
    /** The socket must be connected before this call is made.
        This call will read data, blocking according to the timeout value.  Then all data
        available on the socket will be read and returned as a BinaryNode.  As soon as the
        first timeout occurs, the data will be returned immediately without blocking.

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param prc output parameter: 0 for OK, not 0 for error

        @return the data read as a BinaryNode, caller owns the reference count returned (0 if an error occurs)
    */
    DLLEXPORT BinaryNode* recvBinary(int timeout_ms, int* prc);

    //! receive with a timeout value and return a BinaryNode, caller owns the reference count returned
    /** The socket must be connected before this call is made.
        This call will read data, blocking according to the timeout value.  Then all data
        available on the socket will be read and returned as a BinaryNode.  As soon as the
        first timeout occurs, the data will be returned immediately without blocking.

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the data read as a BinaryNode, caller owns the reference count returned (0 if an error occurs)
    */
    DLLEXPORT BinaryNode* recvBinary(int timeout_ms, ExceptionSink* xsink);

    //! receive data on the socket and write it to a file descriptor
    /** The socket must be connected before this call is made.

        @param fd the file descriptor to write to, must be already opened for writing
        @param size the number of bytes to read from the socket, -1 to read until the socket is closed
        @param timeout_ms timeout for socket I/O in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting

        @return 0 for OK, not 0 for error

        @note the timeout value applies to each read from the socket
    */
    DLLEXPORT int recv(int fd, qore_offset_t size, int timeout_ms);

    //! receive data on the socket and write it to a file descriptor
    /** The socket must be connected before this call is made.

        @param fd the file descriptor to write to, must be already opened for writing
        @param size the number of bytes to read from the socket, -1 to read until the socket is closed
        @param timeout_ms timeout for socket I/O in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return 0 for OK, not 0 for error

        @note the timeout value applies to each read from the socket

        @since Qore 0.8.12.3
    */
    DLLEXPORT int recv(int fd, qore_offset_t size, int timeout_ms, ExceptionSink* xsink);

    //! send an HTTP request message on the socket
    /** The socket must be connected before this call is made.
        @param method the method string to use in the header - no validity checking is made on this string
        @param path the path string to use in the header, if the path is empty then '/' is sent
        @param http_version should be either "1.0" or "1.1"
        @param headers a hash of headers to send (key: value)
        @param data optional message body to send (may be 0)
        @param size the length of the message body (may be 0)
        @param source the event source code for socket events

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int sendHTTPMessage(const char* method, const char* path, const char* http_version, const QoreHashNode* headers, const void* data, size_t size, int source = QORE_SOURCE_SOCKET);

    //! send an HTTP request message on the socket
    /** The socket must be connected before this call is made.
        @param info if not null, the request-uri and response-uri will be written to this hash, neither of these keys should be set before this call
        @param method the method string to use in the header - no validity checking is made on this string
        @param path the path string to use in the header, if the path is empty then '/' is sent
        @param http_version should be either "1.0" or "1.1"
        @param headers a hash of headers to send (key: value)
        @param data optional message body to send (may be 0)
        @param size the length of the message body (may be 0)
        @param source the event source code for socket events

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int sendHTTPMessage(QoreHashNode* info, const char* method, const char* path, const char* http_version, const QoreHashNode* headers, const void* data, size_t size, int source = QORE_SOURCE_SOCKET);

    //! send an HTTP request message on the socket
    /** The socket must be connected before this call is made.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param info if not null, the request-uri and response-uri will be written to this hash, neither of these keys should be set before this call
        @param method the method string to use in the header - no validity checking is made on this string
        @param path the path string to use in the header, if the path is empty then '/' is sent
        @param http_version should be either "1.0" or "1.1"
        @param headers a hash of headers to send (key: value)
        @param data optional message body to send (may be 0)
        @param size the length of the message body (may be 0)
        @param source the event source code for socket events

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path, const char* http_version, const QoreHashNode* headers, const void* data, size_t size, int source = QORE_SOURCE_SOCKET);

    //! send an HTTP request message on the socket with a timeout value
    /** The socket must be connected before this call is made.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param info if not null, the request-uri and response-uri will be written to this hash, neither of these keys should be set before this call
        @param method the method string to use in the header - no validity checking is made on this string
        @param path the path string to use in the header, if the path is empty then '/' is sent
        @param http_version should be either "1.0" or "1.1"
        @param headers a hash of headers to send (key: value)
        @param data optional message body to send (may be 0)
        @param size the length of the message body (may be 0)
        @param source the event source code for socket events
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path, const char* http_version, const QoreHashNode* headers, const void* data, size_t size, int source, int timeout_ms);

    //! send an HTTP request message on the socket with a timeout value with a chunked message body using a calback
    /** The socket must be connected before this call is made.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param info if not null, the request-uri and response-uri will be written to this hash, neither of these keys should be set before this call
        @param method the method string to use in the header - no validity checking is made on this string
        @param path the path string to use in the header, if the path is empty then '/' is sent
        @param http_version should be either "1.0" or "1.1"
        @param headers a hash of headers to send (key: value)
        @param send_callback the callback for the chunked message body
        @param source the event source code for socket events
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int sendHTTPMessageWithCallback(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path, const char* http_version, const QoreHashNode* headers, const ResolvedCallReferenceNode& send_callback, int source, int timeout_ms);

    //! send an HTTP response message on the socket
    /** The socket must be connected before this call is made.

        @param code the HTTP response code
        @param desc the text description for the response code
        @param http_version should be either "1.0" or "1.1"
        @param headers a hash of headers to send (key: value)
        @param data optional message body to send (may be 0)
        @param size the length of the message body (may be 0)
        @param source the event source code for socket events

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int sendHTTPResponse(int code, const char* desc, const char* http_version, const QoreHashNode* headers, const void* data, size_t size, int source = QORE_SOURCE_SOCKET);

    //! send an HTTP response message on the socket
    /** The socket must be connected before this call is made.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param code the HTTP response code
        @param desc the text description for the response code
        @param http_version should be either "1.0" or "1.1"
        @param headers a hash of headers to send (key: value)
        @param data optional message body to send (may be 0)
        @param size the length of the message body (may be 0)
        @param source the event source code for socket events

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int sendHTTPResponse(ExceptionSink* xsink, int code, const char* desc, const char* http_version, const QoreHashNode* headers, const void* data, size_t size, int source = QORE_SOURCE_SOCKET);

    //! send an HTTP response message on the socket
    /** The socket must be connected before this call is made.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param code the HTTP response code
        @param desc the text description for the response code
        @param http_version should be either "1.0" or "1.1"
        @param headers a hash of headers to send (key: value)
        @param data optional message body to send (may be 0)
        @param size the length of the message body (may be 0)
        @param source the event source code for socket events
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds

        @return 0 for OK, not 0 for error
    */
    DLLEXPORT int sendHTTPResponse(ExceptionSink* xsink, int code, const char* desc, const char* http_version, const QoreHashNode* headers, const void* data, size_t size, int source, int timeout_ms);

    //! send an HTTP response message on the socket
    /** The socket must be connected before this call is made.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param info if not null, the response-uri will be written to this hash, neither of these keys should be set before this call
        @param code the HTTP response code
        @param desc the text description for the response code
        @param http_version should be either "1.0" or "1.1"
        @param headers a hash of headers to send (key: value)
        @param data optional message body to send (may be 0)
        @param size the length of the message body (may be 0)
        @param source the event source code for socket events
        @param timeout_ms the maximum amount of time the socket can block on a single send as an integer in milliseconds

        @return 0 for OK, not 0 for error

        @since %Qore 0.9.4
    */
    DLLEXPORT int sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, const void* data, size_t size, int source,
        int timeout_ms);

    //! read and parse HTTP header, caller owns AbstractQoreNode reference count returned
    /** The socket must be connected before this call is made.
        @note does not read the message body; message body must be read manually

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param prc output parameter: 0 or -2: remote end closed the connection, -1: receive error, -3: timeout
        @param source the event source code for socket events

        @return if 0 (and prc == 0), the socket was closed on the remote end without a response, if the type is NT_STRING, the response could not be parsed, if not 0, caller owns the reference count returned
    */
    DLLEXPORT AbstractQoreNode* readHTTPHeader(int timeout_ms, int* prc, int source = QORE_SOURCE_SOCKET);

    //! read and parse HTTP header, caller owns AbstractQoreNode reference count returned
    /** The socket must be connected before this call is made.
        @note does not read the message body; message body must be read manually

        @param info the request-uri or response-uri is written to this hash if non-null; neither of these keys should be present in the hash before the call
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param prc output parameter: 0 or -2: remote end closed the connection, -1: receive error, -3: timeout
        @param source the event source code for socket events

        @return if 0 (and prc == 0), the socket was closed on the remote end without a response, if the type is NT_STRING, the response could not be parsed, if not 0, caller owns the reference count returned
    */
    DLLEXPORT AbstractQoreNode* readHTTPHeader(QoreHashNode* info, int timeout_ms, int* prc, int source = QORE_SOURCE_SOCKET);

    //! read and parse HTTP header, caller owns QoreHashNode reference count returned
    /** The socket must be connected before this call is made.

        @note does not read the message body; message body must be read manually

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param info the request-uri or response-uri is written to this hash if non-null; neither of these keys should be present in the hash before the call
        @param timeout_ms in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param source the event source code for socket events

        @return if 0, an exception was raised, if not 0, caller owns the reference count returned
    */
    DLLEXPORT QoreHashNode* readHTTPHeader(ExceptionSink* xsink, QoreHashNode* info, int timeout_ms, int source = QORE_SOURCE_SOCKET);

    //! read a HTTP header, caller owns QoreStringNode reference count returned
    /** The socket must be connected before this call is made.

        @note does not read the message body; message body must be read manually

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param timeout_ms in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param source the event source code for socket events

        @return if 0, an exception was raised, if not 0, caller owns the reference count returned
    */
    DLLEXPORT QoreStringNode* readHTTPHeaderString(ExceptionSink* xsink, int timeout_ms, int source = QORE_SOURCE_SOCKET);

    //! receive a binary message in HTTP chunked transfer encoding, caller owns QoreHashNode reference count returned
    /** The socket must be connected before this call is made.
        The message body is returned as a BinaryNode in the "body" key, any footers read after the body
        are returned as the other hash keys in the hash.

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param source the event source code for socket events

        @return the message body as the value of the "body" key and any footers read after the body as other keys (0 if an error occurs)

        @see BinaryNode
    */
    DLLEXPORT QoreHashNode* readHTTPChunkedBodyBinary(int timeout_ms, ExceptionSink* xsink, int source = QORE_SOURCE_SOCKET);

    //! receive a string message in HTTP chunked transfer encoding, caller owns QoreHashNode reference count returned
    /** The socket must be connected before this call is made.
        The message body is returned as a QoreStringNode in the "body" key, any footers read after the body
        are returned as the other hash keys in the hash.

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param source the event source code for socket events

        @return the message body as the value of the "body" key and any footers read after the body as other keys (0 if an error occurs)

        @see QoreStringNode
    */
    DLLEXPORT QoreHashNode* readHTTPChunkedBody(int timeout_ms, ExceptionSink* xsink, int source = QORE_SOURCE_SOCKET);

    //! receive a single HTTP chunk
    /** The socket must be connected before this call is made.
        The message body is returned as a QoreStringNode in the "body" key, any footers read after the body
        are returned as the other hash keys in the hash.

        @param timeout timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no
        data waiting
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the message body as the value of the "body" key and any footers read after the body as other keys
        (nullptr if an error occurs)

        @since %Qore 2.0
    */
   DLLEXPORT QoreHashNode* readHttpChunk(int timeout, ExceptionSink* xsink);

    //! Read and return a Server Sent Event message
    /** The socket must be connected before this call is made.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param content_encoding any content encoding to use when decoding the stream; may be nullptr
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no
        data waiting

        @return a server sent event hash (caller owns the reference) or nullptr (error occurred)

        @since %Qore 2.0
    */
    DLLEXPORT QoreHashNode* readServerSentEvent(ExceptionSink* xsink, const QoreStringNode* content_encoding,
            int timeout_ms);

    //! Parse a string as a Server Sent Event (SSE) string
    /** @param xsink if an error occurs, the Qore-language exception information will be added here
        @param buf the string to parse as a Server Sent Event (SSE) string

        @since %Qore 2.0
    */
    DLLEXPORT static QoreHashNode* parseServerSentEvent(ExceptionSink* xsink, const QoreString& buf);

    //! set send timeout in milliseconds
    DLLEXPORT int setSendTimeout(int ms);

    //! set recv timeout in milliseconds
    DLLEXPORT int setRecvTimeout(int ms);

    //! get send timeout in milliseconds
    DLLEXPORT int getSendTimeout() const;

    //! get recv timeout in milliseconds
    DLLEXPORT int getRecvTimeout() const;

    //! returns true if data is available on the socket in the timeout period in milliseconds
    /** The socket must be connected before this call is made.
         use a timeout of 0 to see if there is any data available on the socket

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting

        @return true if data is available within the timeout period

        @see asyncIoWait()

        @note if data is available in the socket buffer, this call returns immediately
    */
    DLLEXPORT bool isDataAvailable(int timeout_ms = 0) const;

    //! returns true if data is available on the socket in the timeout period in milliseconds
    /** The socket must be connected before this call is made.
         use a timeout of 0 to see if there is any data available on the socket

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting

        @return true if data is available within the timeout period

        @see asyncIoWait()

        @note if data is available in the socket buffer, this call returns immediately

        @since Qore 0.8.8
    */
    DLLEXPORT bool isDataAvailable(ExceptionSink* xsink, int timeout_ms = 0) const;

    //! returns 1 if the event was satisfied in the timeout period, 0 if not (= timeout), or -1 in case of an error (see errno in this case)
    /** @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting
        @param read wait for data to be available for reading from the socket
        @param write wait for data to be written on the socket

        @see
        - isDataAvailable()
        - isWriteFinished()

        @note This is a low-level socket function, if an SSL connection is in progress, then this function could return 1 for reading due to SSL protocol renegotiation for example.  The socket buffer is ignored for this call (unlike isDataAvaialble())

        @since Qore 0.8.12
    */
    DLLEXPORT int asyncIoWait(int timeout_ms, bool read, bool write) const;

    //! closes the socket
    /** Deletes the socket file if it was a UNIX domain socket and was created with the QoreSocket::bind() call.
         Also implicitly calls QoreSocket::shutdownSSL() if an SSL connection is active.
        @return 0 if OK, not 0 on error
    */
    DLLEXPORT int close();

    //! calls shutdown on the socket
    /** shuts down the socket for reading and writing, after this call further sends and receives are
        disallowed until the socket is reopened.

        @note QoreSocket::shutdown() should normally be called before calling this function.

        @return 0 if OK, not 0 on error

        @see QoreSocket::shutdown()
    */
    DLLEXPORT int shutdown();

    //! shuts down an active SSL connection
    /** called implicitly by QoreSocket::close()

        @return 0 if OK, not 0 on error

        @see QoreSocket::close()
    */
    DLLEXPORT int shutdownSSL(ExceptionSink* xsink);

    //! returns the file descriptor associated with this socket
    /** @return the file descriptor associated with this socket
    */
    DLLEXPORT int getSocket() const;

    //! returns the character encoding associated with this socket
    /** @return the character encoding associated with this socket
    */
    DLLEXPORT const QoreEncoding *getEncoding() const;

    //! sets the character encoding for strings sent and received with this socket
    /** @param id the character encoding for strings sent and received with this socket
    */
    DLLEXPORT void setEncoding(const QoreEncoding *id);

    //! returns true if the socket is open
    /** @return true if the socket is open
    */
    DLLEXPORT bool isOpen() const;

    //! returns the name of the SSL Cipher for the currently-connected control connection, or 0 if there is none
    /** @return the name of the SSL Cipher for the currently-connected control connection, or 0 if there is none
    */
    DLLEXPORT const char* getSSLCipherName() const;

    //! returns the version string of the SSL Cipher for the currently-connected control connection, or 0 if there is none
    /** @return the version string of the SSL Cipher for the currently-connected control connection, or 0 if there is none
    */
    DLLEXPORT const char* getSSLCipherVersion() const;

    //! returns true if an SSL connection is active
    /** @return true if an SSL connection is active
    */
    DLLEXPORT bool isSecure() const;

    //! Sets ALPN protocols for TLS negotiation
    /** Must be called before SSL upgrade methods (upgradeClientToSSL, upgradeServerToSSL).
        ALPN (Application-Layer Protocol Negotiation) is used to negotiate HTTP/2 over TLS.

        @param protocols List of protocol names in preference order (e.g., "h2", "http/1.1")
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return 0 on success, -1 on error

        @since Qore 2.1
    */
    DLLEXPORT int setAlpnProtocols(const QoreListNode* protocols, ExceptionSink* xsink);

    //! Clears ALPN protocol settings
    /** This should be called when HTTP/2 mode is disabled to ensure proper protocol negotiation

        @since Qore 2.1
    */
    DLLEXPORT void clearAlpnProtocols();

    //! Returns the negotiated ALPN protocol after TLS handshake
    /** @return the selected protocol string, or nullptr if ALPN was not negotiated

        @since Qore 2.1
    */
    DLLEXPORT QoreStringNode* getAlpnProtocol() const;

    //! Returns true if HTTP/2 ("h2") was negotiated via ALPN
    /** @return true if the connection uses HTTP/2

        @since Qore 2.1
    */
    DLLEXPORT bool isHttp2() const;

    //! Submits an HTTP/2 PUSH_PROMISE frame for server push
    /** @param stream_id the HTTP/2 stream ID of the associated request
        @param path the path of the resource being pushed
        @param headers hash of headers for the pushed resource
        @param xsink exception sink for error reporting

        @return the promised stream ID on success (positive integer), or -1 on error

        @since Qore 2.2
    */
    DLLEXPORT int32_t submitHttp2PushPromise(int32_t stream_id, const char* path,
            const QoreHashNode* headers, ExceptionSink* xsink);

    //! Submits an HTTP/2 response to the nghttp2 session without creating a poll operation
    /** The response headers and body are queued in the nghttp2 session.  The actual
        socket write is handled by the active read poll operation's sendPendingData() calls.
        This is thread-safe and can be called from any thread.

        @param stream_id the HTTP/2 stream ID from the request
        @param status_code the HTTP status code
        @param headers response headers
        @param body response body data (optional)
        @param body_len response body length
        @param xsink exception sink for Qore-language exceptions
        @return 0 on success, -1 on error (exception raised)

        @since %Qore 2.3
    */
    DLLEXPORT int submitHttp2Response(int32_t stream_id, int status_code,
            const QoreHashNode* headers, const void* body, size_t body_len,
            ExceptionSink* xsink);

    //! Submits an HTTP/2 CONNECT response without creating a poll operation (RFC 8441)
    /** Queues the CONNECT response in the nghttp2 session without END_STREAM, keeping
        the stream open for bidirectional data transfer. The actual socket write is handled
        by the active read poll operation's sendPendingData() calls.

        This is designed for HTTP/2 multiplexing where the read operation stays on the I/O
        thread and CONNECT responses are submitted from handler threads.

        @param stream_id the HTTP/2 stream ID from the CONNECT request
        @param status_code the HTTP status code to send (200 to accept, 4xx to reject)
        @param headers response headers
        @param xsink exception sink for error reporting
        @return 0 on success, -1 on error

        @since %Qore 2.3
    */
    DLLEXPORT int submitHttp2ConnectResponse(int32_t stream_id, int status_code,
            const QoreHashNode* headers, ExceptionSink* xsink);

    //! Submits an HTTP/2 client request on an active multiplexed connection
    /** @param headers request headers (must include :method, :path, :scheme, :authority)
        @param body request body (optional)
        @param body_len body length
        @param xsink exception sink for error reporting

        @return the stream ID assigned to this request, or -1 on error

        @since %Qore 2.3
    */
    DLLEXPORT int32_t submitHttp2Request(const QoreHashNode* headers, const void* body,
            size_t body_len, ExceptionSink* xsink, bool streaming = false);

    //! Cancels a pending HTTP/2 stream by sending RST_STREAM
    /** @param stream_id the stream ID to cancel
        @param xsink exception sink for error reporting

        @since %Qore 2.3
    */
    DLLEXPORT void cancelHttp2Stream(int32_t stream_id, ExceptionSink* xsink);

    //! Sets whether to advertise ENABLE_CONNECT_PROTOCOL in HTTP/2 server SETTINGS
    /** Must be called before the HTTP/2 session is created (i.e., before the connection
        preface is exchanged). When \c enable is \c false, the server will not advertise
        RFC 8441 extended CONNECT protocol support, so clients will not attempt WebSocket
        over HTTP/2 CONNECT.

        @param enable \c true to advertise ENABLE_CONNECT_PROTOCOL (default), \c false to disable

        @since %Qore 2.3
    */
    DLLEXPORT void setHttp2ConnectProtocolEnabled(bool enable);

    DLLEXPORT int sendHttp2StreamData(int32_t stream_id, const BinaryNode* data,
            bool end_stream, ExceptionSink* xsink);
    DLLEXPORT BinaryNode* readHttp2StreamData(int32_t stream_id, size_t max_bytes, ExceptionSink* xsink);

    //! Sends HTTP/2 trailer headers on a stream
    /** Submits a trailing HEADERS frame with END_STREAM on the specified stream.
        The stream's data provider is configured to use NGHTTP2_DATA_FLAG_NO_END_STREAM
        on the last DATA frame so trailers carry the END_STREAM flag.

        @param stream_id the HTTP/2 stream ID
        @param trailers trailer header name/value pairs
        @param xsink exception sink for error reporting
        @return 0 on success, -1 on error

        @since %Qore 2.3
    */
    DLLEXPORT int sendHttp2Trailers(int32_t stream_id, const QoreHashNode* trailers,
            ExceptionSink* xsink);

    //! Submits HTTP/2 streaming response headers without body or END_STREAM
    /** Calls Http2Session::submitResponseStreaming() to send response HEADERS
        without END_STREAM. A deferred data provider is created so that body data
        can be sent later via sendHttp2StreamData() and trailers via sendHttp2Trailers().

        @param stream_id the HTTP/2 stream ID
        @param status_code HTTP status code (e.g., 200)
        @param headers response headers
        @param xsink exception sink for error reporting
        @return 0 on success, -1 on error

        @since %Qore 2.3
    */
    DLLEXPORT int submitHttp2StreamingResponseHeaders(int32_t stream_id, int status_code,
            const QoreHashNode* headers, ExceptionSink* xsink);

    //! Sets the active HTTP/2 stream ID for transparent send/recv operations
    /** @param stream_id the stream ID to use for subsequent send/recv operations
        @param xsink exception sink for error reporting

        @since Qore 2.3
    */
    DLLEXPORT void setHttp2ActiveStream(int32_t stream_id, ExceptionSink* xsink);

    //! Gets the active HTTP/2 stream ID
    /** @return the active stream ID, or -1 if no stream is active

        @since Qore 2.3
    */
    DLLEXPORT int32_t getHttp2ActiveStream() const;

    //! Blocking read of HTTP/2 stream data for incremental server-side streaming
    /** Blocks until data is available on the specified stream, the stream completes
        (END_STREAM), or the timeout expires.

        @param stream_id the HTTP/2 stream ID
        @param timeout_ms read timeout in milliseconds
        @param xsink exception sink for error reporting

        @return binary data from the stream, or nullptr on timeout or stream end.
        Use isHttp2StreamComplete() to distinguish timeout from end-of-stream.

        @since %Qore 2.3
    */
    DLLEXPORT BinaryNode* readHttp2StreamDataBlock(int32_t stream_id, int timeout_ms,
            ExceptionSink* xsink);

    //! Check if an HTTP/2 stream has received END_STREAM
    /** @param stream_id the HTTP/2 stream ID
        @return true if END_STREAM has been received (body_complete) or stream not found

        @since %Qore 2.3
    */
    DLLEXPORT bool isHttp2StreamComplete(int32_t stream_id) const;

    //! Flush all pending HTTP/2 outgoing data (blocking)
    /** Calls sendPendingDataBlocking() to send all queued frames to the socket.

        @param timeout_ms send timeout in milliseconds; -1 for infinite
        @param xsink exception sink for error reporting
        @return 0 on success, -1 on error

        @since %Qore 2.3
    */
    DLLEXPORT int flushHttp2(int timeout_ms, ExceptionSink* xsink);

    //! Remove an HTTP/2 stream from the session (cleanup after handler finishes)
    /** @param stream_id the HTTP/2 stream ID to remove

        @since %Qore 2.3
    */
    DLLEXPORT void cleanupHttp2Stream(int32_t stream_id);

    //! returns the peer certificate verification code if an SSL connection is in progress
    DLLEXPORT long verifyPeerCertificate() const;

    //! negotiates an SSL connection from the client side
    /** The socket must be connected before this call is made.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param timeout_ms timeout in milliseconds, -1 = never timeout
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return 0 if OK, not 0 on error
    */
    DLLEXPORT int upgradeServerToSSL(ExceptionSink* xsink, int timeout_ms = -1, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

    //! negotiates an SSL connection from the client side
    /** The socket must be connected before this call is made.

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param timeout_ms timeout in milliseconds, -1 = never timeout
        @param cert the X509 certificate to use for the connection, may be 0 if no certificate should be used
        @param pkey the private key to use for the connection, may be 0 if no private key should be used

        @return 0 if OK, not 0 on error
    */
    DLLEXPORT int upgradeClientToSSL(ExceptionSink* xsink, int timeout_ms = -1, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

    //! returns true if all write data has been written within the timeout period in milliseconds
    /** The socket must be connected before this call is made.
        use a timeout of 0 to receive an answer immediately

        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting

        @return true if data is available within the timeout period

        @see asyncIoWait()
    */
    DLLEXPORT bool isWriteFinished(int timeout_ms = 0) const;

    //! returns true if all write data has been written within the timeout period in milliseconds
    /** The socket must be connected before this call is made.
        use a timeout of 0 to receive an answer immediately

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param timeout_ms timeout in milliseconds, -1=never timeout, 0=do not block, return immediately if there is no data waiting

        @return true if data is available within the timeout period

        @since Qore 0.8.8
    */
    DLLEXPORT bool isWriteFinished(ExceptionSink* xsink, int timeout_ms = 0) const;

    //! returns peer information for a connected socket
    /** if the socket is not connected, a Qore-language exception is thrown

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return a hash with the following keys:
        - \c hostname: the hostname of the remote end (if known or appropriate for the socket type)
        - \c hostname_desc: a descriptive string for the remote hostname (including the socket type - ie "ipv6[host]")
        - \c address: the address of the remote end - for UNIX sockets this is the file path
        - \c address_desc: a descriptive string for the remote address
        - \c port: the port number if known
        - \c family: the address family (ie AF_INET, AF_INET6, AF_UNIX, ...)
        - \c familystr: a string description of the address family ("ipv4", "ipv6", etc)
    */
    DLLEXPORT QoreHashNode* getPeerInfo(ExceptionSink* xsink) const;

    //! returns peer information for a connected socket
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
    */
    DLLEXPORT QoreHashNode* getPeerInfo(ExceptionSink* xsink, bool host_lookup) const;

    //! returns information for the current socket; the socket must be open
    /** if the socket is not open, a Qore-language exception is thrown

        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a hash with the following keys:
        - \c hostname: the hostname of the remote end (if known or appropriate for the socket type, only performed if \a host_lookup is true)
        - \c hostname_desc: a descriptive string for the remote hostname (including the socket type - ie "ipv6[host]", only performed if \a host_lookup is true)
        - \c address: the address of the local interface - for UNIX sockets this is the file path
        - \c address_desc: a descriptive string for the local interface
        - \c port: the port number if known
        - \c family: the address family (ie AF_INET, AF_INET6, AF_UNIX, ...)
        - \c familystr: a string description of the address family ("ipv4", "ipv6", etc)
    */
    DLLEXPORT QoreHashNode* getSocketInfo(ExceptionSink* xsink) const;

    //! returns information for the current socket; the socket must be open
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
    */
    DLLEXPORT QoreHashNode* getSocketInfo(ExceptionSink* xsink, bool host_lookup) const;

    DLLEXPORT void clearWarningQueue(ExceptionSink* xsink);
    DLLEXPORT void setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg, int64 min_ms = 1000);
    DLLEXPORT QoreHashNode* getUsageInfo() const;
    DLLEXPORT void clearStats();

    //! posts deleted message and removes any event queue
    DLLEXPORT void cleanup(ExceptionSink* xsink);

    //! returns true if a HTTP header was read indicating chunked transfer encoding, but no chunked body has been read
    DLLEXPORT bool pendingHttpChunkedBody() const;

    //! sets the SSL verification mode
    /** If no SSL connection is in place, then the mode is saved for use with future SSL connections.

        @param mode see the \c mode parameter of SSL_set_verify() for details

        @since %Qore 0.8.13
    */
    DLLEXPORT void setSslVerifyMode(int mode);

    //! returns the SSL verification mode
    /** @return see the \c mode parameter of SSL_set_verify() for details

        @since %Qore 0.8.13
    */
    DLLEXPORT int getSslVerifyMode() const;

    //! with peer verification enabled, all certificates are accepted regardless of the validity of the Certificate Authority
    /** @param accept_all if true, accepts self-signed certs, if false, then not

        @since %Qore 0.8.13
    */
    DLLEXPORT void acceptAllCertificates(bool accept_all = true);

    //! returns the current value of the "accept all certificates" flag
    /** @return the current value of the "accept all certificates" flag; if true, accepts self-signed certs, if false, then not

        @since %Qore 0.8.13
    */
    DLLEXPORT bool getAcceptAllCertificates() const;

    //! sets the capture SSL certificate flag
    /** @param set the new value of the flag

        @return the old value of the flag

        @see getRemoteCertificate()

        @since %Qore 0.9.3
    */
    DLLEXPORT bool captureRemoteCertificates(bool set);

    //! returns a remote certificate if available
    /** @return a remote certificate if available or nullptr if not

        @see captureRemoteCertificate()

        @since %Qore 0.9.3
    */
    DLLEXPORT QoreObject* getRemoteCertificate() const;

    //! returns a connection ID to help identifying when new connections are made
    /** @return a connection ID to help identifying when new connections are made

        @since %Qore 0.9.3.1
    */
    DLLEXPORT int64 getConnectionId() const;

    //! Sets the maximum body size for chunked HTTP reads (0 = unlimited)
    /** When set, readHTTPChunkedBodyBinary() and readHTTPChunkedBody() will
        raise an HTTP-BODY-TOO-LARGE exception if the accumulated body exceeds this limit.

        @param size maximum body size in bytes; 0 means unlimited

        @since %Qore 2.1
    */
    DLLEXPORT void setMaxChunkedBodySize(int64 size);

    //! Returns the maximum body size for chunked HTTP reads
    /** @return the maximum body size in bytes; 0 means unlimited

        @since %Qore 2.1
    */
    DLLEXPORT int64 getMaxChunkedBodySize() const;

    //! Sets the maximum request body size for HTTP/2 streams (0 = unlimited)
    /** When set, HTTP/2 DATA frame accumulation exceeding this limit will cause
        the stream to be reset with REFUSED_STREAM.

        @param size maximum body size in bytes; 0 means unlimited

        @since %Qore 2.1
    */
    DLLEXPORT void setHttp2MaxRequestBodySize(int64 size);

    //! Returns the maximum request body size for HTTP/2 streams
    /** @return the maximum body size in bytes; 0 means unlimited

        @since %Qore 2.1
    */
    DLLEXPORT int64 getHttp2MaxRequestBodySize() const;

    DLLLOCAL static void doException(int rc, const char* meth, int timeout_ms, ExceptionSink* xsink);

    //! sets the event queue (not part of the library's pubilc API), must be already referenced before call
    DLLLOCAL void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);

    //! returns the event queue (not part of the library's public API)
    DLLLOCAL Queue* getQueue();

    //! returns a unique ID for the socket to be used in event messages
    DLLLOCAL int64 getObjectIDForEvents() const;

    DLLLOCAL int setNoDelay(int nodelay);
    DLLLOCAL int getNoDelay() const;

    //! sets backwards-compatible members on accept in a new object - will be removed in a future version of qore
    DLLLOCAL void setAccept(QoreObject* o);

private:
    //! private implementation of the class
    struct qore_socket_private *priv;

    //! private constructor, not exported in the library's public interface
    DLLLOCAL QoreSocket(int n_sock, int n_sfamily, int n_stype, int s_prot, const QoreEncoding* csid);

    DLLLOCAL static void convertHeaderToHash(QoreHashNode* h, char* p);

    DLLLOCAL QoreSocket(const QoreSocket&) = delete;
    DLLLOCAL QoreSocket& operator=(const QoreSocket&) = delete;
};

class QoreSocketTimeoutHelper {
protected:
    class PrivateQoreSocketTimeoutHelper* priv;
public:
    DLLEXPORT QoreSocketTimeoutHelper(QoreSocket& s, const char* op);
    DLLEXPORT ~QoreSocketTimeoutHelper();
};

class QoreSocketThroughputHelper {
protected:
    class PrivateQoreSocketThroughputHelper* priv;
public:
    DLLEXPORT QoreSocketThroughputHelper(QoreSocket& s, bool snd);
    DLLEXPORT ~QoreSocketThroughputHelper();
    DLLEXPORT void finalize(int64 bytes);
};

#endif // _QORE_QORESOCKET_H
