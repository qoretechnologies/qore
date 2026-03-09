/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttpClientObject.h

    Qore Programming Language

    Copyright (C) 2006 - 2026 Qore Technologies, s.r.o.

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

#ifndef QORE_HTTP_CLIENT_OBJECT_H_
#define QORE_HTTP_CLIENT_OBJECT_H_

#include <qore/common.h>
#include <qore/QoreSocketObject.h>
#include <qore/OutputStream.h>

#define HTTPCLIENT_DEFAULT_PORT 80                 //!< the default port number to use
#define HTTPCLIENT_DEFAULT_HOST "localhost"        //!< the default host name to use

#define HTTPCLIENT_DEFAULT_TIMEOUT 300000          //!< the default connection and response packet timeout to use (300,000 ms = 5m)
#define HTTPCLIENT_DEFAULT_CONNECT_TIMEOUT 60000   //!< the default connection and response packet timeout to use (60,000 ms = 1m)

#define HTTPCLIENT_DEFAULT_MAX_REDIRECTS 5         //!< maximum number of HTTP redirects allowed

constexpr int64 UC_TARGET          = (1 << 0);
constexpr int64 UC_USERNAME        = (1 << 1);
constexpr int64 UC_PASSWORD        = (1 << 2);
constexpr int64 UC_MASK_PASSWORD   = (1 << 3);
constexpr int64 UC_PATH            = (1 << 4);

constexpr int64 URL_NO_AUTH        = UC_TARGET | UC_PATH;
constexpr int64 URL_NORMAL         = UC_TARGET | UC_USERNAME | UC_PASSWORD | UC_PATH;
constexpr int64 URL_MASK_PASSWORD  = URL_NORMAL | UC_MASK_PASSWORD;

class Queue;

//! HTTP/2 protocol mode options
/** @since %Qore 2.2
*/
enum Http2Mode {
    HTTP2_MODE_DISABLED = 0,  //!< HTTP/1.x only, never use HTTP/2
    HTTP2_MODE_AUTO = 1,      //!< Use HTTP/2 if available via ALPN, fallback to HTTP/1.1 (default)
    HTTP2_MODE_REQUIRED = 2,  //!< Require HTTP/2, fail if unavailable
    HTTP2_MODE_H2C_DIRECT = 3,    //!< h2c (HTTP/2 cleartext) using prior knowledge - starts HTTP/2 directly
    HTTP2_MODE_H2C_UPGRADE = 4    //!< h2c via HTTP/1.1 Upgrade header
};

//! HTTP/3 protocol mode options
/** @since %Qore 2.3
*/
enum Http3Mode {
    HTTP3_MODE_DISABLED = 0,  //!< Never use HTTP/3
    HTTP3_MODE_AUTO = 1,      //!< Use HTTP/3 if advertised via Alt-Svc (default)
    HTTP3_MODE_REQUIRED = 2   //!< Require HTTP/3, fail if unavailable
};

//! provides a way to communicate with HTTP servers using Qore data structures
/** thread-safe, uses QoreSocket for socket communication
 */
class QoreHttpClientObject : public QoreSocketObject {
    friend struct qore_httpclient_priv;
    friend class HttpClientConnectSendRecvPollOperation;

public:
    //! creates the QoreHttpClientObject object
    DLLEXPORT QoreHttpClientObject();

    //! destroys the object and frees all associated memory
    DLLEXPORT virtual ~QoreHttpClientObject();

    //! Starts a socket connect poll operation
    /** @since %Qore 1.12
    */
    DLLEXPORT QoreObject* startPollConnect(ExceptionSink* xsink, QoreObject* self);

    //! Starts a non-blocking, polling HTTP send/receive operation
    /** @since %QOre 1.12
    */
    DLLEXPORT QoreObject* startPollSendRecv(ExceptionSink* xsink, QoreObject* self, const QoreString* method,
            const QoreString* path, const AbstractQoreNode* data_save, const void* data, size_t size,
            const QoreHashNode* headers, const QoreEncoding* enc = nullptr);

    //! set options with a hash, returns -1 if an exception was thrown, 0 for OK
    /** options are:
        - protocols: a hash where each key is a protocol name and the value must be set to a integer giving a port
          number or a hash having the following keys:
          - port: giving the port number
          - ssl: giving a boolean true or false value
        - max_redirects: sets the max_redirects option
        - default_port: sets the default port number
        - proxy: sets the proxy URL
        - url: sets the default connection URL
        - default_path: sets the default path
        - timeout: sets the connection or response packet timeout value in milliseconds
        - http_version: either "1.0" or "1.1" to set the default HTTP version to use
        - connect_timeout: an integer giving the timeout value for new socket connections in milliseconds

        @note this function is unlocked and designed only to be called with the constructor

        @param opts the options to set for the object
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return -1 if an exception was thrown, 0 for OK
    */
    DLLEXPORT int setOptions(const QoreHashNode* opts, ExceptionSink* xsink);

    //! sets the default port
    /** useful for c++ derived classes
    */
    DLLEXPORT void setDefaultPort(int prt);

    //! sets the default path
    /** useful for c++ derived classes
    */
    DLLEXPORT void setDefaultPath(const char* pth);

    //! returns the default path or 0 if none is set
    /** @since Qore 0.8.8
    */
    DLLEXPORT const char* getDefaultPath() const;

    //! returns the current connection path or 0 if none is set
    /** @since Qore 0.8.8
    */
    DLLEXPORT const char* getConnectionPath() const;

    //! sets or clears the connection path
    /** @since Qore 0.9.3.2
    */
    DLLEXPORT void setConnectionPath(const char* path);

    //! adds a protocol
    /** useful for c++ derived classes
    */
    DLLEXPORT void addProtocol(const char* prot, int port, bool ssl = false);

    //! sets the connection and response packet timeout value in milliseconds
    DLLEXPORT void setTimeout(int to);

    //! returns the connection and response packet timeout value in milliseconds
    DLLEXPORT int getTimeout() const;

    //! sets the default encoding for the object
    DLLEXPORT void setEncoding(const QoreEncoding* qe);

    //! returns the default encoding for the object
    DLLEXPORT const QoreEncoding* getEncoding() const;

    //! sets the http version from a string
    /**
        @param version either "1.0" or "1.1"
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return -1 if an exception was thrown, 0 for OK
    */
    DLLEXPORT int setHTTPVersion(const char* version, ExceptionSink* xsink);

    //! returns the http version as a string (either "1.0" or "1.1")
    /**
        @return the http version as a string (either "1.0" or "1.1")
    */
    DLLEXPORT const char* getHTTPVersion() const;

    //! sets or clears HTTP 1.1 protocol compliance
    /**
        @param h11 if true sets HTTP 1.1 protocol compliance, if false set 1.0
    */
    DLLEXPORT void setHTTP11(bool h11);

    //! returns true if HTTP 1.1 protocol compliance has been set
    DLLEXPORT bool isHTTP11() const;

    //! Returns true if HTTP/2 support is enabled
    /** @return true if HTTP/2 support is enabled (will try to use h2 via ALPN)

        @since %Qore 2.2
    */
    DLLEXPORT bool isHttp2Enabled() const;

    //! Enables or disables HTTP/2 support
    /** When enabled, the client will offer h2 via ALPN during TLS negotiation.
        If the server supports HTTP/2, subsequent requests will use HTTP/2.

        @param enable true to enable HTTP/2, false to disable

        @note HTTP/2 requires TLS; enabling HTTP/2 on a non-TLS connection has no effect

        @since %Qore 2.2
    */
    DLLEXPORT void setHttp2Enabled(bool enable);

    //! Returns true if the connection is currently using HTTP/2
    /** @return true if HTTP/2 is active on the current connection

        @since %Qore 2.2
    */
    DLLEXPORT bool isHttp2Active() const;

    //! Returns the current HTTP/2 settings
    /** @return a hash with HTTP/2 settings, or nullptr if HTTP/2 is not active

        The returned hash may contain:
        - \c header_table_size: HPACK header table size
        - \c enable_push: whether server push is enabled
        - \c max_concurrent_streams: maximum concurrent streams
        - \c initial_window_size: initial flow control window size
        - \c max_frame_size: maximum frame size
        - \c max_header_list_size: maximum header list size

        @since %Qore 2.2
    */
    DLLEXPORT QoreHashNode* getHttp2Settings() const;

    //! Sets HTTP/2 settings to be used for new connections
    /** @param settings a hash with HTTP/2 settings to apply
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @since %Qore 2.2
    */
    DLLEXPORT void setHttp2Settings(const QoreHashNode* settings, ExceptionSink* xsink);

    //! Sets HTTP/2 stream priority
    /** @param stream_id the stream ID to set priority for
        @param weight priority weight (1-256, default 16)
        @param dependency stream ID this stream depends on (0 for root)
        @param exclusive if true, becomes exclusive dependency
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @since %Qore 2.2
    */
    DLLEXPORT void setHttp2StreamPriority(int32_t stream_id, int32_t weight, int32_t dependency,
        bool exclusive, ExceptionSink* xsink);

    //! Returns the negotiated HTTP protocol version string
    /** @return "HTTP/2" if HTTP/2 is active, otherwise "HTTP/1.1" or "HTTP/1.0"

        @since %Qore 2.2
    */
    DLLEXPORT QoreStringNode* getHttpVersion() const;

    //! Sets the HTTP/2 protocol mode
    /** @param mode the HTTP/2 mode: HTTP2_MODE_DISABLED, HTTP2_MODE_AUTO, or HTTP2_MODE_REQUIRED
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @note The default mode is HTTP2_MODE_AUTO, which uses HTTP/2 if available via ALPN

        @since %Qore 2.2
    */
    DLLEXPORT void setHttp2Mode(int mode, ExceptionSink* xsink);

    //! Returns the current HTTP/2 protocol mode
    /** @return the current HTTP/2 mode: HTTP2_MODE_DISABLED, HTTP2_MODE_AUTO, or HTTP2_MODE_REQUIRED

        @since %Qore 2.2
    */
    DLLEXPORT int getHttp2Mode() const;

    //! Sends an HTTP/2 extended CONNECT request (RFC 8441)
    /** @param path the request path
        @param headers request headers
        @param protocol the :protocol pseudo-header value (e.g., "websocket")
        @param info optional reference to a hash to receive connection info
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return response hash with headers and stream_id, or nullptr on error

        @note This method is used for WebSocket over HTTP/2 (RFC 8441)

        @since %Qore 2.2
    */
    DLLEXPORT QoreHashNode* sendHttp2Connect(const char* path, const QoreHashNode* headers,
        const char* protocol, QoreHashNode* info, ExceptionSink* xsink);

    //! Sends an HTTP/3 extended CONNECT request (RFC 9220) and waits for the response
    /** @param path the request path
        @param headers request headers
        @param protocol the :protocol pseudo-header value (e.g., "websocket")
        @param info optional reference to a hash to receive connection info
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return response hash with headers and stream_id, or nullptr on error

        @note This method is used for WebSocket over HTTP/3 (RFC 9220)

        @since %Qore 2.2
    */
    DLLEXPORT QoreHashNode* sendHttp3Connect(const char* path, const QoreHashNode* headers,
        const char* protocol, QoreHashNode* info, ExceptionSink* xsink);

    //! Sends data on an HTTP/2 stream
    /** @param stream_id the stream ID to send data on
        @param data the data to send
        @param end_stream if true, signals end of stream (no more data will be sent)
        @param timeout_ms timeout in milliseconds
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return 0 on success, -1 on error

        @note This is used for bidirectional streaming protocols like WebSocket over HTTP/2

        @since %Qore 2.2
    */
    DLLEXPORT int sendHttp2StreamData(int32_t stream_id, const BinaryNode* data,
        bool end_stream, int timeout_ms, ExceptionSink* xsink);

    //! Reads data from an HTTP/2 stream
    /** @param stream_id the stream ID to read data from
        @param timeout_ms timeout in milliseconds
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return binary data read, or nullptr if no data available or error

        @note This is used for bidirectional streaming protocols like WebSocket over HTTP/2

        @since %Qore 2.2
    */
    DLLEXPORT BinaryNode* readHttp2StreamData(int32_t stream_id, int timeout_ms, ExceptionSink* xsink);

    //! Returns the HTTP/2 stream ID for the current request
    /** @return the stream ID, or 0 if not in HTTP/2 mode

        @since %Qore 2.2
    */
    DLLEXPORT int32_t getHttp2StreamId() const;

    //! Returns true if the HTTP/2 stream has buffered data available
    /** @param stream_id the stream ID to check

        @return true if the stream has buffered data, false otherwise

        @since %Qore 2.3
    */
    DLLEXPORT bool hasHttp2StreamData(int32_t stream_id) const;

    //! Checks if HTTP/2 stream data is available, polling the socket if needed
    /** @param stream_id the stream ID to check
        @param timeout_ms timeout in milliseconds for polling
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return true if data is available, false otherwise

        @note This method first checks for buffered data, then polls the socket for new
        HTTP/2 frames if no data is buffered. This is the correct way to check for
        data availability when using HTTP/2 streams.

        @since %Qore 2.3
    */
    DLLEXPORT bool isHttp2DataAvailable(int32_t stream_id, int timeout_ms, ExceptionSink* xsink);

    //! Check if an HTTP/2 stream is fully closed (both sides sent END_STREAM)
    /** @param stream_id the stream to check

        @return true if the stream is closed or does not exist

        @since %Qore 2.3
    */
    DLLEXPORT bool isHttp2StreamClosed(int32_t stream_id) const;

    //! Check if an HTTP/3 stream is complete (END_STREAM / FIN received from server)
    /** @param stream_id the stream to check

        @return true if the stream is complete or does not exist

        @since %Qore 2.3
    */
    DLLEXPORT bool isHttp3StreamClosed(int64_t stream_id) const;

    //! Sends data on an HTTP/3 extended CONNECT stream (bidirectional tunnel)
    /** @param stream_id the stream ID (from sendHttp3Connect())
        @param data the data to send (or nullptr for end-of-stream only)
        @param end_stream if true, signals end of stream (no more data will be sent)
        @param timeout_ms timeout in milliseconds
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return 0 on success, -1 on error

        @note This is used for bidirectional streaming protocols like WebSocket over HTTP/3

        @since %Qore 2.2
    */
    DLLEXPORT int sendHttp3StreamData(int64_t stream_id, const BinaryNode* data,
        bool end_stream, int timeout_ms, ExceptionSink* xsink);

    //! Reads data from an HTTP/3 extended CONNECT stream (bidirectional tunnel)
    /** @param stream_id the stream ID (from sendHttp3Connect())
        @param timeout_ms timeout in milliseconds for polling
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return binary data read, or nullptr if no data available or error

        @note This is used for bidirectional streaming protocols like WebSocket over HTTP/3

        @since %Qore 2.2
    */
    DLLEXPORT BinaryNode* readHttp3StreamData(int64_t stream_id, int timeout_ms, ExceptionSink* xsink);

    //! Sets the HTTP/3 protocol mode
    /** @param mode the HTTP/3 mode: HTTP3_MODE_DISABLED, HTTP3_MODE_AUTO, or HTTP3_MODE_REQUIRED
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @note The default mode is HTTP3_MODE_AUTO, which uses HTTP/3 if advertised via Alt-Svc

        @since %Qore 2.3
    */
    DLLEXPORT void setHttp3Mode(int mode, ExceptionSink* xsink);

    //! Returns the current HTTP/3 protocol mode
    /** @return the current HTTP/3 mode: HTTP3_MODE_DISABLED, HTTP3_MODE_AUTO, or HTTP3_MODE_REQUIRED

        @since %Qore 2.3
    */
    DLLEXPORT int getHttp3Mode() const;

    //! Returns @ref True if the connection is currently using HTTP/3 (QUIC)
    /** @return @ref True if HTTP/3 is active on the current connection

        @since %Qore 2.3
    */
    DLLEXPORT bool isHttp3Active() const;

    //! Returns the pollable file descriptor for this connection
    /** When HTTP/3 is active, returns the QUIC UDP socket fd instead of the
        (closed) TCP socket fd.

        @return the file descriptor to use for polling, or -1 if not open

        @since %Qore 2.3
    */
    DLLEXPORT int getPollableDescriptor() const override;

    //! sets the connection URL
    /** @param url the URL to use for connection parameters
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return -1 if an exception was thrown, 0 for OK
    */
    DLLEXPORT int setURL(const char* url, ExceptionSink* xsink);

    //! returns the connection parameters as a URL according to the parameters passed
    /** @param code URL codes as a bitfield

        @return the connection parameters as a URL according to the parameters passed, caller owns the reference count
        returned

        @since %Qore 2.0
    */
    DLLEXPORT QoreStringNode* getUrl(int64 code = URL_NORMAL);

    //! sets the username and password for the connection
    /** @param user the username to set
        @param pass the password to set
        @note setURL() will overwrite any settings set here
    */
    DLLEXPORT void setUserPassword(const char* user, const char* pass);

    //! clears the username and password for the connection
    DLLEXPORT void clearUserPassword();

    //! sets the proxy URL
    /** @param proxy the URL to use for connection to the proxy
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return -1 if an exception was thrown, 0 for OK
    */
    DLLEXPORT int setProxyURL(const char* proxy, ExceptionSink* xsink);

    //! returns the proxy connection parameters as a URL (or 0 if there is none)
    /**
        @return the proxy connection parameters as a URL, caller owns the reference count returned
    */
    DLLEXPORT QoreStringNode* getProxyURL();

    //! returns the proxy connection parameters as a URL without any password (or 0 if there is none)
    /**
        @return the proxy connection parameters as a URL without any password, caller owns the reference count returned

        @since %!ore 1.6.0
    */
    DLLEXPORT QoreStringNode* getSafeProxyURL();

    //! clears the proxy URL
    DLLEXPORT void clearProxyURL();

    //! sets the username and password for the proxy connection
    /** @param user the username to set
        @param pass the password to set
        @note these settings will only take effect if a proxy URL is set, so it only makes sense to call this function
        after setProxyURL(); also setProxyURL() will overwrite any settings here.
    */
    DLLEXPORT void setProxyUserPassword(const char* user, const char* pass);

    //! clears the username and password for the proxy connection
    DLLEXPORT void clearProxyUserPassword();

    //! Returns the username for the connection, if any
    /** @return the username for the connection, if any

        @since %Qore 1.18.2
    */
    DLLEXPORT QoreStringNode* getUsername() const;

    //! Returns the password for the connection, if any
    /** @return the password for the connection, if any

        @since %Qore 1.18.2
    */
    DLLEXPORT QoreStringNode* getPassword() const;

    //! Returns the username for the proxy connection, if any
    /** @return the username for the proxy connection, if any

        @since %Qore 1.18.2
    */
    DLLEXPORT QoreStringNode* getProxyUsername() const;

    //! Returns the password for the proxy connection, if any
    /** @return the password for the proxy connection, if any

        @since %Qore 1.18.2
    */
    DLLEXPORT QoreStringNode* getProxyPassword() const;

    //! sets the SSL flag for use in the next connection
    DLLEXPORT void setSecure(bool is_secure);

    //! returns the SSL connection parameter flag
    DLLEXPORT bool isSecure() const;

    //! sets the SSL flag for use in the next connection to the proxy
    DLLEXPORT void setProxySecure(bool is_secure);

    //! returns the SSL proxy connection parameter flag
    DLLEXPORT bool isProxySecure() const;

    //! sets the max_redirects option
    DLLEXPORT void setMaxRedirects(int max);

    //! returns the value of the max_redirects option
    DLLEXPORT int getMaxRedirects() const;

    //! opens a connection and returns a code giving the result
    /** @return -1 if an exception was thrown, 0 for OK
    */
    DLLEXPORT int connect(ExceptionSink* xsink);

    //! disconnects from the remote server
    DLLEXPORT void disconnect();

    //! sends a message to the remote server and returns the entire response as a hash
    /** possible errors: method not recognized, redirection errors, socket communication errors, timeout errors

        @param meth the HTTP method name to send
        @param path the path string to send in the header
        @param headers a hash of headers to add to the message
        @param data optional data to send (may be 0)
        @param size the byte length of the data to send (if this is 0 then no data is sent)
        @param getbody if true then a body will be read even if there is no "Content-Length:" header
        @param info if not 0 then additional information about the HTTP communication will be added to the hash
        (key-value pairs), keys "headers", and optionally "redirect-#", "redirect-message-#" (where # is substituted
        with the redirect sequence number), and "chunked" (boolean, present only if the response was chunked)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the entire response as a hash, caller owns the QoreHashNode reference returned (0 if there was an error)
    */
    DLLEXPORT QoreHashNode* send(const char* meth, const char* path, const QoreHashNode* headers, const void* data,
            unsigned size, bool getbody, QoreHashNode* info, ExceptionSink* xsink);

    DLLEXPORT QoreHashNode* send(const char* meth, const char* path, const QoreHashNode* headers,
            const QoreStringNode& body, bool getbody, QoreHashNode* info, ExceptionSink* xsink);

    //! Send HTTP request and return headers only, leaving body on socket for streaming
    /** @param meth the HTTP method (e.g. "GET", "POST")
        @param path the path for the request
        @param headers any headers to add to the request
        @param data the message body to send (can be nullptr)
        @param size the size of the message body
        @param info a hash reference to receive request/response info
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return response headers without body; caller owns the QoreHashNode reference returned (nullptr on error)
        @note The response body is NOT read; use readHTTPChunk() or readServerSentEvent() to read it
        @since %Qore 2.3
    */
    DLLEXPORT QoreHashNode* sendAndStream(const char* meth, const char* path, const QoreHashNode* headers,
            const void* data, unsigned size, QoreHashNode* info, ExceptionSink* xsink);

    //! Send HTTP request and return headers only, leaving body on socket for streaming
    /** @param meth the HTTP method (e.g. "GET", "POST")
        @param path the path for the request
        @param headers any headers to add to the request
        @param body the message body to send
        @param info a hash reference to receive request/response info
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return response headers without body; caller owns the QoreHashNode reference returned (nullptr on error)
        @note The response body is NOT read; use readHTTPChunk() or readServerSentEvent() to read it
        @since %Qore 2.3
    */
    DLLEXPORT QoreHashNode* sendAndStream(const char* meth, const char* path, const QoreHashNode* headers,
            const QoreStringNode& body, QoreHashNode* info, ExceptionSink* xsink);

    DLLEXPORT QoreHashNode* sendWithSendCallback(const char* meth, const char* mpath, const QoreHashNode* headers,
            const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info, int timeout_ms,
            ExceptionSink* xsink);

    DLLEXPORT void sendWithRecvCallback(const char* meth, const char* mpath, const QoreHashNode* headers,
            const void* data, unsigned size, bool getbody, QoreHashNode* info, int timeout_ms,
            const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink);

    DLLEXPORT void sendWithRecvCallback(const char* meth, const char* mpath, const QoreHashNode* headers,
            const QoreStringNode& body, bool getbody, QoreHashNode* info, int timeout_ms,
            const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink);

    DLLEXPORT void sendWithCallbacks(const char* meth, const char* mpath, const QoreHashNode* headers,
            const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info, int timeout_ms,
            const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink);

    //! make an HTTP request and receive the response to an OutputStream
    /** @since %Qore 0.8.13
    */
    DLLEXPORT void sendWithOutputStream(const char* meth, const char* mpath, const QoreHashNode* headers,
            const void* data, unsigned size, bool getbody, QoreHashNode* info, int timeout_ms,
            const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream *os, ExceptionSink* xsink);

    //! make an HTTP request and receive the response to an OutputStream
    /** @since %Qore 0.9.4
    */
    DLLEXPORT void sendWithOutputStream(const char* meth, const char* mpath, const QoreHashNode* headers,
            const QoreStringNode& body, bool getbody, QoreHashNode* info, int timeout_ms,
            const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream *os, ExceptionSink* xsink);

    //! send a chunked HTTP message through an InputStream and receive the response to an OutputStream
    /** @since %Qore 0.8.13
    */
    DLLEXPORT void sendChunked(const char* meth, const char* mpath, const QoreHashNode* headers, bool getbody,
            QoreHashNode* info, int timeout_ms, const ResolvedCallReferenceNode* recv_callback, QoreObject* obj,
            OutputStream *os, InputStream* is, size_t max_chunk_size,
            const ResolvedCallReferenceNode* trailer_callback, ExceptionSink* xsink);

    //! sends an HTTP "GET" method and returns the value of the message body returned
    /** if you need to get all the headers received, then use QoreHttpClientObject::send() instead
        @param path the path string to send in the header
        @param headers a hash of headers to add to the message
        @param info if not 0 then additional information about the HTTP communication will be added to the hash
        (key-value pairs), keys "headers", and optionally "redirect-#", "redirect-message-#" (where # is substituted
        with the redirect sequence number), and "chunked" (boolean, present only if the response was chunked)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the body of the response message, caller owns the QoreHashNode reference returned (0 if there was an
        error or no body returned)
    */
    DLLEXPORT AbstractQoreNode* get(const char* path, const QoreHashNode* headers, QoreHashNode* info,
            ExceptionSink* xsink);

    //! sends an HTTP "HEAD" method and returns the headers returned, the caller owns the QoreHashNode reference returned
    /** @param path the path string to send in the header
        @param headers a hash of headers to add to the message
        @param info if not 0 then additional information about the HTTP communication will be added to the hash
        (key-value pairs), keys "headers", and optionally "redirect-#", "redirect-message-#" (where # is substituted
        with the redirect sequence number), and "chunked" (boolean, present only if the response was chunked)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the entire response as a hash, caller owns the QoreHashNode reference returned (0 if there was an error)
    */
    DLLEXPORT QoreHashNode* head(const char* path, const QoreHashNode* headers, QoreHashNode* info,
            ExceptionSink* xsink);

    //! sends an HTTP "POST" message to the remote server and returns the message body of the response
    /** possible errors: method not recognized, redirection errors, socket communication errors, timeout errors
        @param path the path string to send in the header
        @param headers a hash of headers to add to the message
        @param data optional data to send (should not be 0 for a POST)
        @param size the byte length of the data to send (if this is 0 then no data is sent)
        @param info if not 0 then additional information about the HTTP communication will be added to the hash
        (key-value pairs), keys "headers", and optionally "redirect-#", "redirect-message-#" (where # is substituted
        with the redirect sequence number), and "chunked" (boolean, present only if the response was chunked)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the body of the response message, caller owns the QoreHashNode reference returned (0 if there was an
        error or no body returned)
    */
    DLLEXPORT AbstractQoreNode* post(const char* path, const QoreHashNode* headers, const void* data, unsigned size,
            QoreHashNode* info, ExceptionSink* xsink);

    //! sends an HTTP "POST" message to the remote server and returns the message body of the response
    /** possible errors: method not recognized, redirection errors, socket communication errors, timeout errors
        @param path the path string to send in the header
        @param headers a hash of headers to add to the message
        @param body optional message body to send (should not be 0 for a POST)
        @param info if not 0 then additional information about the HTTP communication will be added to the hash
        (key-value pairs), keys "headers", and optionally "redirect-#", "redirect-message-#" (where # is substituted
        with the redirect sequence number), and "chunked" (boolean, present only if the response was chunked)
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return the body of the response message, caller owns the QoreHashNode reference returned (0 if there was an
        error or no body returned)
    */
    DLLEXPORT AbstractQoreNode* post(const char* path, const QoreHashNode* headers, const QoreStringNode& body,
            QoreHashNode* info, ExceptionSink* xsink);

    //! sets the value of a default header to send with every outgoing message
    /**
        @param header the name of the header to send
        @param val the string value to use in the HTTP header
    */
    DLLEXPORT void setDefaultHeaderValue(const char* header, const char* val);

    //! Sets the value of multiple headers to send with every outgoing message
    /** @param hdr the hash of headers to set

        @since %Qore 0.9.5
    */
    DLLEXPORT void addDefaultHeaders(const QoreHashNode* hdr);

    //! Returns a hash of default headers to be sent with every outgoing request
    /** @return a hash of default headers to be sent with every outgoing request

        @since %Qore 0.9.5
    */
    DLLEXPORT QoreHashNode* getDefaultHeaders() const;

    using AbstractPrivateData::deref;
    //! decrements the reference count and deletes the object when it reaches 0
    /**
         @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT virtual void deref(ExceptionSink* xsink);

    //! sets the connect timeout in ms
    /**
         @param ms connect timeout in ms
    **/
    DLLEXPORT void setConnectTimeout(int ms);

    //! returns the connect timeout in ms, negative numbers mean no timeout
    /**
         @return the connect timeout in ms, negative numbers mean no timeout
    **/
    DLLEXPORT int getConnectTimeout() const;

    //! sets the TCP_NODELAY flag on the object
    /**
         This function will try to set the TCP_NODELAY flag immediately if the
        socket is connected, otherwise will it set a flag and the TCP_NODELAY
        option will be set on the next connection.  If an error occurs
        setting TCP_NODELAY on a connected socket, then this function will
        return a non-zero value, and errno will be set
        @param nodelay 0=turn off TCP_NODELAY, non-zero=turn on TCP_NODELAY
        @return 0=OK, non-zero means an error occured, errno is set
    */
    DLLEXPORT int setNoDelay(bool nodelay);

    //! sets the event queue, must be already referenced before call
    DLLEXPORT void setEventQueue(Queue* cbq, ExceptionSink* xsink);

    //! sets the event queue, must be already referenced before call
    DLLEXPORT void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);

    //! returns the value of the TCP_NODELAY flag on the object
    DLLEXPORT bool getNoDelay() const;

    //! returns the connection status of the object
    DLLEXPORT bool isConnected() const;

    //! temporarily disables implicit reconnections; must be called when the server is already connected
    DLLEXPORT void setPersistent(ExceptionSink* xsink);

    //! Clears the persistent connection flag and reenables implicit reconnections
    /** @since %Qore 2.0
    */
    DLLEXPORT void clearPersistent();

    //! Returns the value of the persistent connection flag
    /** @since %Qore 2.0
    */
    DLLEXPORT bool isPersistent() const;

    //! Returns the persistent connection count
    /** @since %Qore 2.0
    */
    DLLEXPORT unsigned getPersistentCount() const;

    DLLEXPORT void clearWarningQueue(ExceptionSink* xsink);
    DLLEXPORT void setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg,
            int64 min_ms = 1000);
    DLLEXPORT QoreHashNode* getUsageInfo() const;
    DLLEXPORT void clearStats();

    //! sets the new and returns the old error_passthru status
    /** @since %Qore 0.9.3
     */
    DLLEXPORT bool setErrorPassthru(bool set);

    //! returns the current error_passthru status
    /** @since %Qore 0.9.3
     */
    DLLEXPORT bool getErrorPassthru() const;

    //! sets the new and returns the old redirect_passthru status
    /** @since %Qore 0.9.3
     */
    DLLEXPORT bool setRedirectPassthru(bool set);

    //! returns the current redirect_passthru status
    /** @since %Qore 0.9.3
     */
    DLLEXPORT bool getRedirectPassthru() const;

    //! sets the new and returns the old encoding_passthru status
    /** @since %Qore 0.9.3
     */
    DLLEXPORT bool setEncodingPassthru(bool set);

    //! returns the current encoding_passthru status
    /** @since %Qore 0.9.3
     */
    DLLEXPORT bool getEncodingPassthru() const;

    //! returns the \c Host header value
    /** @since %Qore 0.9.3
     */
    DLLEXPORT QoreStringNode* getHostHeaderValue() const;

    //! sets the assumed encoding
    /** @since %Qore 0.9.4.2
    */
    DLLEXPORT void setAssumedEncoding(const char* enc);

    //! gets the assumed encoding
    /** @since %Qore 0.9.4.2
    */
    DLLEXPORT QoreStringNode* getAssumedEncoding() const;

    //! sets the new and returns the old pre_encoded_urls flag
    /** @since %Qore 1.13.0
    */
    DLLEXPORT bool setPreEncodedUrls(bool set);

    //! returns the current pre_encoded_urls flag
    /** @since %Qore 1.13.0
    */
    DLLEXPORT bool getPreEncodedUrls() const;

    //! Returns a configuration hash for the object
    /** @since %Qore 2.0
    */
    DLLEXPORT QoreHashNode* getConfig() const;

    DLLLOCAL static void static_init();

    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLEXPORT void lock();
    DLLEXPORT void unlock();

private:
    //! private implementation of the class
    struct qore_httpclient_priv* http_priv;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreHttpClientObject(const QoreHttpClientObject&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreHttpClientObject& operator=(const QoreHttpClientObject&);
};

#endif
