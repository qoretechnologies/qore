/*
    QoreHttpClientObject.cpp

    Qore Programming Language

    Copyright (C) 2006 - 2024 Qore Technologies, s.r.o.

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

/*
    RFC 2616 HTTP 1.1
    RFC 2617 HTTP authentication
    RFC 3986 HTTP URI specification
*/

#include <qore/Qore.h>
#include <qore/QoreURL.h>
#include <qore/QoreHttpClientObject.h>
#include "qore/intern/ql_misc.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_Queue.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QoreHttpClientObjectIntern.h"
#include "qore/intern/QoreHashNodeIntern.h"

#include "qore/intern/qore_socket_private.h"

#include <cctype>
#include <map>
#include <set>
#include <string>

// issue #3564: set the I/O timeout for reading an incoming HTTP header after an aborted outbound chunked transfer
static const int ABORTED_TIMEOUT_MS = 5000;

method_map_t method_map;
strcase_set_t header_ignore;

constexpr int SPS_SENDING = 4;
constexpr int SPS_RECEIVING_HEADER = 5;
constexpr int SPS_RECEIVING_BODY = 6;
constexpr int SPS_CONNECTING_PROXY_SSL = 7;
constexpr int SPS_RECEIVED = 8;

// states: none [-> connecting [-> connecting-ssl]] -> sending -> receiving-header [-> receiving-body] [-> connecting-proxy-ssl] -> [received | connected]
/**
    state transitions:
    - none
      -> connecting
         -> connecting-ssl
            -> sending...
      -> sending
         -> receiving-header
            -> receiving->body
               -> received
            -> connecting...
            -> sending...
            -> connecting->proxy-ssl
               -> sending...
*/
class HttpClientConnectSendRecvPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL HttpClientConnectSendRecvPollOperation(ExceptionSink* xsink, QoreHttpClientObject* client);

    DLLLOCAL HttpClientConnectSendRecvPollOperation(ExceptionSink* xsink, QoreHttpClientObject* client, std::string method,
            std::string path, const void* data, size_t size, const QoreHashNode* headers,
            const QoreEncoding* enc = nullptr);

    DLLLOCAL virtual void deref(ExceptionSink* xsink);

    DLLLOCAL virtual bool goalReached() const {
        return state == SPS_RECEIVED || state == SPS_CONNECTED;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual QoreValue getOutput() const;

private:
    std::unique_ptr<AbstractPollState> poll_state;
    QoreHttpClientObject* client;
    std::string method, path;
    const void* data = nullptr;
    size_t size = 0;
    const QoreEncoding* enc = nullptr;
    con_info connection;

    ReferenceHolder<QoreHashNode> request_headers;
    ReferenceHolder<QoreHashNode> proxy_headers;
    ReferenceHolder<QoreHashNode> info;
    ReferenceHolder<QoreHashNode> response_headers;

    int state = SPS_NONE;

    bool set_non_block = false;
    bool keep_alive = true;
    bool host_override = false;
    bool use_proxy_connect = false;
    bool path_already_encoded = false;
    bool bodyp = false;
    bool close_connection = false;
    bool connect_only;
    const char* proxy_path = nullptr;

    unsigned redirect_count = 0;

    SimpleRefHolder<BinaryNode> send_data_holder;
    SimpleRefHolder<BinaryNode> recv_data_holder;
    int http_response_code = -1;

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (state) {
            case SPS_NONE:
                return "none";
            case SPS_CONNECTING:
                return "connecting";
            case SPS_CONNECTING_SSL:
                return "connecting-ssl";
            case SPS_CONNECTING_PROXY_SSL:
                return "connecting-proxy-ssl";
            case SPS_SENDING:
                return "sending";
            case SPS_RECEIVING_HEADER:
                return "receiving-header";
            case SPS_RECEIVING_BODY:
                return "receiving-body";
            case SPS_RECEIVED:
                return "received";
            case SPS_CONNECTED:
                return "connected";
            default:
                assert(false);
        }
        return "";
    }

    DLLLOCAL int checkContinuePoll(ExceptionSink* xsink, bool keep = false) {
        assert(poll_state.get());

        // see if we are able to continue
        int rc = poll_state->continuePoll(xsink);
        //printd(5, "HttpClientConnectSendRecvPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(),
        //    rc, (int)*xsink);
        if (*xsink) {
            assert(rc < 0);
            state = SPS_NONE;
            return -1;
        }
        if (!rc && !keep) {
            // release the AbstractPollState value
            poll_state.reset();
        }
        return rc;
    }

    DLLLOCAL int connectOrSend(ExceptionSink* xsink);
    DLLLOCAL int connectDone(ExceptionSink* xsink);
    DLLLOCAL int startSend(ExceptionSink* xsink);
    DLLLOCAL BinaryNode* getMessage(QoreString& hdr);
    DLLLOCAL int processResponse(ExceptionSink* xsink);
    DLLLOCAL int redirect(ExceptionSink* xsink);
    DLLLOCAL int processReceivedBody(ExceptionSink* xsink);
    DLLLOCAL int responseDone(ExceptionSink* xsink);
    DLLLOCAL void setFinal(int final_state);
};

static const QoreStringNode* get_string_header_node(ExceptionSink* xsink, QoreHashNode& h, const char* header,
        bool allow_multiple = false) {
    QoreValue n = h.getKeyValue(header);
    if (n.isNothing())
        return nullptr;

    qore_type_t t = n.getType();
    if (t == NT_STRING)
        return n.get<const QoreStringNode>();
    assert(t == NT_LIST);
    if (!allow_multiple) {
        xsink->raiseException("HTTP-HEADER-ERROR", "multiple \"%s\" headers received in HTTP message", header);
        return nullptr;
    }
    // convert list to a comma-separated string
    const QoreListNode* l = n.get<const QoreListNode>();
    // get first list entry
    n = l->retrieveEntry(0);
    assert(n.getType() == NT_STRING);
    QoreStringNode* rv = n.get<QoreStringNode>()->copy();
    for (size_t i = 1; i < l->size(); ++i) {
        n = l->retrieveEntry(i);
        assert(n.getType() == NT_STRING);
        rv->concat(',');
        rv->concat(n.get<const QoreStringNode>());
    }
    // dereference old list and save reference to return value in header hash
    h.setKeyValue(header, rv, xsink);
    return rv;
}

static const char* get_string_header(ExceptionSink* xsink, QoreHashNode& h, const char* header,
        bool allow_multiple = false) {
   const QoreStringNode* str = get_string_header_node(xsink, h, header, allow_multiple);
   return str && !str->empty() ? str->c_str() : nullptr;
}

void do_content_length_event(Queue* event_queue, qore_socket_private* priv, size_t len) {
    if (event_queue) {
        QoreHashNode* h = priv->getEvent(QORE_EVENT_HTTP_CONTENT_LENGTH, QORE_SOURCE_HTTPCLIENT);
        qore_hash_private* hh = qore_hash_private::get(*h);
        hh->setKeyValueIntern("len", len);
        event_queue->pushAndTakeRef(h);
    }
}

void do_redirect_event(Queue* event_queue, qore_socket_private* priv, const QoreStringNode* loc,
        const QoreStringNode* msg) {
    if (event_queue) {
        QoreHashNode* h = priv->getEvent(QORE_EVENT_HTTP_REDIRECT, QORE_SOURCE_HTTPCLIENT);
        qore_hash_private* hh = qore_hash_private::get(*h);
        hh->setKeyValueIntern("location", loc->refSelf());
        if (msg)
            hh->setKeyValueIntern("status_message", msg->refSelf());
        event_queue->pushAndTakeRef(h);
    }
}

void do_event(Queue* event_queue, qore_socket_private* priv, int event) {
    if (event_queue) {
        QoreHashNode* h = priv->getEvent(event, QORE_SOURCE_HTTPCLIENT);
        event_queue->pushAndTakeRef(h);
    }
}

struct qore_httpclient_priv {
    my_socket_priv* msock;

    prot_map_t prot_map;

    con_info connection, proxy_connection;

    // issue #3978: default output encoding
    const QoreEncoding* enc = nullptr;

    bool
        // are we using http 1.1 or 1.0?
        http11 = true,
        // when set, TCP_NODELAY is used on the socket
        nodelay = false,
        /** means that a CONNECT message has been processed and the connection is now made as if it were directly with
            the client
        */
        proxy_connected = false,
        // turns off implicit connections for the current connection only
        persistent = false,
        // HTTP response errors do not result in exceptions being thrown
        error_passthru = false,
        // redirect messages will not be processed but rather passed to the caller
        redirect_passthru = false,
        // known content encodings are not decoded when set
        encoding_passthru = false,
        // if URLs are pre-encoded
        pre_encoded_urls = false
        ;

    // persistent count
    unsigned persistent_count = 0;

    int default_port = HTTPCLIENT_DEFAULT_PORT,
        max_redirects = HTTPCLIENT_DEFAULT_MAX_REDIRECTS;

    std::string default_path;
    int timeout = HTTPCLIENT_DEFAULT_TIMEOUT;
    std::string socketpath;
    header_map_t default_headers;
    static header_map_t static_default_headers;
    int connect_timeout_ms = HTTPCLIENT_DEFAULT_CONNECT_TIMEOUT;

    method_map_t additional_methods_map;

    // characters that must be encoded when pre_encoded_urls is enabled
    static constexpr const char* must_encode_chars = "{}|\\^~[]`";

    // characters subject to percent encoding by Qore
    typedef std::map<char, const char*> pct_encoding_map_t;
    static pct_encoding_map_t pct_encoding_map;

    // any local map for this object for additional characters to encode
    typedef std::set<char> pct_encoding_set_t;
    pct_encoding_set_t local_pct_encoding_set;

    DLLLOCAL qore_httpclient_priv(my_socket_priv* ms) :
            msock(ms),
            connection(HTTPCLIENT_DEFAULT_PORT) {
        assert(ms);
        // setup protocol map
        prot_map["http"] = make_protocol(80, false);
        prot_map["https"] = make_protocol(443, true);

        // setup default headers
        default_headers = static_default_headers;
    }

    DLLLOCAL ~qore_httpclient_priv() {
    }

    QoreHashNode* getConfig(my_socket_priv& priv) const {
        qore_socket_private& sock = *qore_socket_private::get(*priv.socket);
        ReferenceHolder<QoreHashNode> rv(new QoreHashNode, nullptr);

        qore_hash_private* h = qore_hash_private::get(**rv);
        if (!additional_methods_map.empty()) {
            ReferenceHolder<QoreHashNode> amm(new QoreHashNode, nullptr);
            qore_hash_private* hh = qore_hash_private::get(**amm);
            for (auto& i : additional_methods_map) {
                hh->setKeyValueIntern(i.first.c_str(), i.second);
            }
            h->setKeyValueIntern("additional_methods", amm.release());
        }
        if (sock.assume_http_encoding != "ISO-8859-1") {
            h->setKeyValueIntern("assume_encoding", new QoreStringNode(sock.assume_http_encoding));
        }
        if (connect_timeout_ms != HTTPCLIENT_DEFAULT_CONNECT_TIMEOUT) {
            h->setKeyValueIntern("connect_timeout", connect_timeout_ms);
        }
        if (!default_path.empty()) {
            h->setKeyValueIntern("default_path", new QoreStringNode(default_path));
        }
        if (default_port != HTTPCLIENT_DEFAULT_PORT) {
            h->setKeyValueIntern("default_port", default_port);
        }
        if (!local_pct_encoding_set.empty()) {
            SimpleRefHolder<QoreStringNode> str(new QoreStringNode);
            for (auto& i : local_pct_encoding_set) {
                str->concat(i);
            }
            h->setKeyValueIntern("encode_chars", str.release());
        }
        if (enc) {
            h->setKeyValueIntern("encoding", new QoreStringNode(enc->getCode()));
        }
        if (encoding_passthru) {
            h->setKeyValueIntern("encoding_passthru", encoding_passthru);
        }
        if (error_passthru) {
            h->setKeyValueIntern("error_passthru", error_passthru);
        }
        if (!default_headers.empty()) {
            ReferenceHolder<QoreHashNode> amm(new QoreHashNode, nullptr);
            qore_hash_private* hh = qore_hash_private::get(**amm);
            for (auto& i : default_headers) {
                header_map_t::const_iterator hi = static_default_headers.find(i.first);
                if (hi != static_default_headers.end() && hi->second == i.second) {
                    continue;
                }
                hh->setKeyValueIntern(i.first.c_str(), new QoreStringNode(i.second));
            }
            if (!amm->empty()) {
                h->setKeyValueIntern("headers", amm.release());
            }
        }
        if (!http11) {
            h->setKeyValueIntern("http_version", new QoreStringNode("1.0"));
        }
        if (max_redirects != HTTPCLIENT_DEFAULT_MAX_REDIRECTS) {
            h->setKeyValueIntern("max_redirects", max_redirects);
        }
        if (!connection.password.empty()) {
            h->setKeyValueIntern("password", new QoreStringNode(connection.password));
        }
        if (pre_encoded_urls) {
            h->setKeyValueIntern("pre_encoded_urls", pre_encoded_urls);
        }
        if (!prot_map.empty()) {
            ReferenceHolder<QoreHashNode> amm(new QoreHashNode, nullptr);
            qore_hash_private* hh = qore_hash_private::get(**amm);
            for (auto& i : prot_map) {
                // skip standard protocols always present
                if (i.second == -443 || i.second == 80) {
                    continue;
                }
                if (i.second < 0) {
                    ReferenceHolder<QoreHashNode> v(new QoreHashNode, nullptr);
                    qore_hash_private* vh = qore_hash_private::get(**v);
                    vh->setKeyValueIntern("ssl", true);
                    vh->setKeyValueIntern("port", -i.second);
                    hh->setKeyValueIntern(i.first.c_str(), v.release());
                } else {
                    hh->setKeyValueIntern(i.first.c_str(), i.second);
                }
            }
            if (!amm->empty()) {
                h->setKeyValueIntern("protocols", amm.release());
            }
        }
        if (proxy_connection.has_url()) {
            h->setKeyValueIntern("proxy", proxy_connection.get_url());
        }
        if (redirect_passthru) {
            h->setKeyValueIntern("redirect_passthru", redirect_passthru);
        }
        if (priv.cert) {
            h->setKeyValueIntern("ssl_cert_data", priv.cert->getDER(nullptr));
        }
        if (priv.pk) {
            h->setKeyValueIntern("ssl_key_data", priv.pk->getDER(nullptr));
        }
        if (sock.ssl_verify_mode == SSL_VERIFY_PEER) {
            h->setKeyValueIntern("ssl_verify_cert", true);
        }
        if (timeout != HTTPCLIENT_DEFAULT_TIMEOUT) {
            h->setKeyValueIntern("timeout", timeout);
        }
        if (connection.has_url()) {
            h->setKeyValueIntern("url", connection.get_url(URL_NO_AUTH));
        }
        if (!connection.username.empty()) {
            h->setKeyValueIntern("username", new QoreStringNode(connection.username));
        }
        return rv.release();
    }

    DLLLOCAL void setSocketPathIntern(const con_info& con) {
        if (con.path.empty() || !con.host.empty()) {
            socketpath = con.host;
            if (!con.is_unix) {
                socketpath += ":";
                char buff[20];
                sprintf(buff, "%d", con.port);
                socketpath += buff;
            }
            return;
        }

        socketpath = con.path;
    }

    DLLLOCAL void setSocketPath(const con_info& connection) {
        setSocketPathIntern(proxy_connection.has_url() ? proxy_connection : connection);
        //printd(5, "setSocketPath() '%s'\n", socketpath.c_str());
    }

    DLLLOCAL void lock() { msock->m.lock(); }
    DLLLOCAL void unlock() { msock->m.unlock(); }

    DLLLOCAL QoreObject* startPollSendRecv(ExceptionSink* xsink, QoreObject* self, QoreHttpClientObject* client,
            const QoreString* method, const QoreString* path, const AbstractQoreNode* data_save, const void* data,
            size_t size, const QoreHashNode* headers, const QoreEncoding* enc = nullptr) {
        client->ref();
        ReferenceHolder<HttpClientConnectSendRecvPollOperation> poller(
            new HttpClientConnectSendRecvPollOperation(xsink, client,
                method->c_str(), path ? path->c_str() : "", data, size, headers, enc
            ), xsink);
        if (*xsink) {
            return nullptr;
        }
        SocketPollOperationBase* p = *poller;
        ReferenceHolder<QoreObject> rv(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
        if (!*xsink) {
            p->setSelf(*rv);
            rv->setValue("sock", self->objectRefSelf(), xsink);
            rv->setValue("goal", new QoreStringNode("received"), xsink);
            if (data_save && size) {
                assert(data);
                rv->setValue("data_save", data_save->refSelf(), xsink);
            } else {
                assert(!data || !size);
            }
        }
        return rv.release();
    }

    DLLLOCAL QoreObject* startPollConnect(ExceptionSink* xsink, QoreObject* self, QoreHttpClientObject* client) {
        //printd(5, "qore_http_client_priv::startPoll() self: %p client: %p msock: %p (== %p)\n", self, client,
        //    msock, client->priv);

        client->ref();
        ReferenceHolder<SocketPollOperationBase> poller(new HttpClientConnectSendRecvPollOperation(xsink, client),
            xsink);
        if (*xsink) {
            return nullptr;
        }
        SocketPollOperationBase* p = *poller;
        ReferenceHolder<QoreObject> rv(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
        if (!*xsink) {
            p->setSelf(*rv);
            rv->setValue("sock", self->objectRefSelf(), xsink);
            rv->setValue("goal", new QoreStringNode("connect"), xsink);
        }
        return rv.release();
    }

    // returns -1 if an exception was thrown, 0 for OK
    DLLLOCAL int connect_unlocked(ExceptionSink* xsink, const con_info& connection) {
        assert(!msock->socket->isOpen());
        bool connect_ssl = proxy_connection.has_url()
            ? proxy_connection.ssl
            : connection.ssl;

        int rc;
        if (connect_ssl) {
            rc = msock->socket->connectSSL(xsink, socketpath.c_str(), connect_timeout_ms,
                msock->cert ? msock->cert->getData() : nullptr,
                msock->pk ? msock->pk->getData() : nullptr,
                msock->cert ? msock->cert->getVerifyCACertificate() : nullptr);
        } else {
            rc = msock->socket->connect(socketpath.c_str(), connect_timeout_ms, xsink);
        }

        if (!rc) {
            setNoDelay();
        }
        return rc;
    }

    DLLLOCAL void setNoDelay() {
        if (nodelay) {
            if (msock->socket->setNoDelay(1)) {
                nodelay = false;
            }
        }
    }

    DLLLOCAL void disconnect_unlocked() {
        if (msock->socket->isOpen()) {
            msock->socket->close();
            proxy_connected = false;
            persistent = false;
            persistent_count = 0;
        }
    }

    DLLLOCAL int setNoDelay(bool nd) {
        AutoLocker al(msock->m);

        if (!msock->socket->isOpen()) {
            nodelay = true;
            return 0;
        }

        if (nodelay) {
            return 0;
        }

        if (msock->socket->setNoDelay(1)) {
            return -1;
        }

        nodelay = true;
        return 0;
    }

    DLLLOCAL bool getNoDelay() const {
        return nodelay;
    }

    DLLLOCAL void setPersistent(ExceptionSink* xsink) {
        AutoLocker al(msock->m);

        if (!msock->socket->isOpen()) {
            xsink->raiseException("PERSISTENCE-ERROR", "HTTPClient::setPersistent() can only be called once an "
                "initial connection has been established; currently there is no connection to the server");
            return;
        }

        if (!persistent) {
            persistent = true;
        }
        ++persistent_count;
    }

    DLLLOCAL void clearPersistent() {
        AutoLocker al(msock->m);

        if (!persistent_count) {
            return;
        }
        if (!--persistent_count) {
            assert(persistent);
            persistent = false;
        }
    }

    // FIXME: redirects reset the connection URL for all further connections - they need to reset it only for the next
    // connection
    // issue #3474: process redirect messages correctly
    DLLLOCAL int redirectUrlUnlocked(const char* str, con_info& connection, ExceptionSink* xsink) {
        QoreURL url(str);
        if (!url.isValid()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "redirect location '%s' cannot be parsed", str);
            return -1;
        }

        // check if the location is only a path, in which case we need to keep the rest of the connection info the same
        if (!url.getPort() && !url.getHost() && url.getPath()) {
            connection.path = url.getPath()->c_str();
            return 0;
        }

        bool port_set = false;
        if (connection.set_url(url, port_set, xsink)) {
            return -1;
        }

        const QoreString* tmp = url.getProtocol();
        if (tmp) {
            prot_map_t::const_iterator i = prot_map.find(tmp->c_str());
            if (i == prot_map.end()) {
                xsink->raiseException("HTTP-CLIENT-UNKNOWN-PROTOCOL", "protocol '%s' in redirect message is unknown",
                    tmp->c_str());
                return -1;
            }

            // set port only if it wasn't overridden in the URL
            if (!port_set && !connection.is_unix) {
                connection.port = get_port(i->second);
            }

            // set SSL setting from protocol default
            connection.ssl = get_ssl(i->second);
        } else {
            connection.ssl = false;
            if (!port_set && !connection.is_unix) {
                connection.port = default_port;
            }
        }

        if (!proxy_connection.has_url()) {
            setSocketPath(connection);
        }

        return 0;
    }

    DLLLOCAL int setUrlUnlocked(const char* str, ExceptionSink* xsink) {
        QoreURL url(str);

        if (!url.isValid()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "URL '%s' cannot be parsed", str);
            return -1;
        }

        bool port_set = false;
        if (connection.set_url(url, port_set, xsink)) {
            return -1;
        }

        const QoreString* tmp = url.getProtocol();
        if (tmp) {
            prot_map_t::const_iterator i = prot_map.find(tmp->c_str());
            if (i == prot_map.end()) {
                xsink->raiseException("HTTP-CLIENT-UNKNOWN-PROTOCOL", "protocol '%s' is not supported",
                    tmp->c_str());
                return -1;
            }

            // set port only if it wasn't overridden in the URL
            if (!port_set && !connection.is_unix) {
                connection.port = get_port(i->second);
            }

            // set SSL setting from protocol default
            connection.ssl = get_ssl(i->second);
        } else {
            connection.ssl = false;
            if (!port_set && !connection.is_unix) {
                connection.port = default_port;
            }
        }

        if (!proxy_connection.has_url()) {
            setSocketPath(connection);
        }

        return 0;
    }

    DLLLOCAL int setProxyUrlUnlocked(const char* pstr, ExceptionSink* xsink) {
        QoreURL url(pstr);

        if (!url.isValid()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "proxy URL '%s' cannot be parsed", pstr);
            return -1;
        }

        bool port_set = false;
        if (proxy_connection.set_url(url, port_set, xsink))
            return -1;

        const QoreString *tmp = url.getProtocol();
        if (tmp) {
            if (strcasecmp(tmp->c_str(), "http") && strcasecmp(tmp->c_str(), "https")) {
                xsink->raiseException("HTTP-CLIENT-PROXY-PROTOCOL-ERROR", "protocol '%s' is not supported for "
                    "proxies, only 'http' and 'https'", tmp->c_str());
                return -1;
            }

            prot_map_t::const_iterator i = prot_map.find(tmp->c_str());
            assert(i != prot_map.end());

            // set port only if it wasn't overridden in the URL
            if (!port_set && !proxy_connection.is_unix)
                proxy_connection.port = get_port(i->second);

            // set SSL setting from protocol default
            proxy_connection.ssl = get_ssl(i->second);
        } else {
            proxy_connection.ssl = false;
            if (!port_set)
                proxy_connection.port = default_port;
        }

        setSocketPath(connection);
        return 0;
    }

    DLLLOCAL void setUserPassword(const char* user, const char* pass) {
        assert(user && pass);
        AutoLocker al(msock->m);

        connection.setUserPassword(user, pass);
    }

    DLLLOCAL void clearUserPassword() {
        AutoLocker al(msock->m);
        connection.clearUserPassword();
    }

    DLLLOCAL void setProxyUserPassword(const char* user, const char* pass) {
        assert(user && pass);
        AutoLocker al(msock->m);

        proxy_connection.setUserPassword(user, pass);
    }

    DLLLOCAL void clearProxyUserPassword() {
        AutoLocker al(msock->m);
        proxy_connection.clearUserPassword();
    }

    DLLLOCAL bool setErrorPassthru(bool set) {
        AutoLocker al(msock->m);
        bool rv = error_passthru;
        error_passthru = set;
        return rv;
    }

    DLLLOCAL bool getErrorPassthru() const {
        AutoLocker al(msock->m);
        return error_passthru;
    }

    DLLLOCAL bool setRedirectPassthru(bool set) {
        AutoLocker al(msock->m);
        bool rv = redirect_passthru;
        redirect_passthru = set;
        return rv;
    }

    DLLLOCAL bool getRedirectPassthru() const {
        AutoLocker al(msock->m);
        return redirect_passthru;
    }

    DLLLOCAL bool setEncodingPassthru(bool set) {
        AutoLocker al(msock->m);
        bool rv = encoding_passthru;
        encoding_passthru = set;
        return rv;
    }

    DLLLOCAL bool getEncodingPassthru() const {
        AutoLocker al(msock->m);
        return encoding_passthru;
    }

    DLLLOCAL void setEncoding(const QoreEncoding* qe) {
        SafeLocker sl(msock->m);
        msock->socket->setEncoding(qe);
        enc = qe;
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        SafeLocker sl(msock->m);
        if (enc) {
            return enc;
        }
        return msock->socket->getEncoding();
    }

    DLLLOCAL void addHttpMethod(const char* method, bool enable) {
        additional_methods_map.insert(method_map_t::value_type(method, enable));
    }

    // issue #2340: duplicate headers are overwritten; duplicate headers are checked with a case-insensitive search
    // the last header that matches is used for sending
    DLLLOCAL static QoreStringNode* getHeaderString(strcase_set_t& hdrs, QoreHashNode& nh, const char* key, ExceptionSink* xsink) {
        SimpleRefHolder<QoreStringNode> str(new QoreStringNode);
        strcase_set_t::iterator i = hdrs.find(key);
        if (i == hdrs.end()) {
            hdrs.insert(i, key);
        } else {
            //printd(5, "qore_httpclient_priv::getHeaderString() taking '%s' -> setting '%s'\n", (*i).c_str(), key);
            // remove the key
            QoreValue t = nh.takeKeyValue((*i).c_str());
            assert(!t.isNothing());
            assert(t.getType() == NT_STRING);
            t.discard(xsink);
            assert(!*xsink);
            // replace key in set with new case if difference
            if (*i != key) {
                hdrs.erase(i);
                hdrs.insert(key);
            }
        }
        nh.setKeyValue(key, *str, xsink);
        assert(!*xsink);

        return str.release();
    }

    DLLLOCAL static void addAppendHeader(strcase_set_t& hdrs, QoreHashNode& nh, const char* key, const QoreValue v,
            ExceptionSink* xsink) {
        if (v.getType() == NT_LIST) {
            QoreStringNode* str = getHeaderString(hdrs, nh, key, xsink);
            ConstListIterator li(v.get<const QoreListNode>());
            while (li.next()) {
                QoreStringNodeValueHelper vh(li.getValue());
                if (!vh->empty()) {
                    if (!str->empty())
                        str->concat(',');
                    str->concat(*vh);
                }
            }
            return;
        }

        QoreStringNodeValueHelper vh(v);
        if (!vh->empty()) {
            QoreStringNode* str = getHeaderString(hdrs, nh, key, xsink);
            if (!str->empty()) {
                str->concat(',');
            }
            str->concat(*vh, xsink);
            //printd(5, "qore_httpclient_priv::addAppendHeader() %s: %s\n", key, str->c_str());
        }
    }

    DLLLOCAL QoreStringNode* getHostHeaderValue() {
        AutoLocker al(msock->m);
        return getHostHeaderValueUnlocked(connection);
    }

    // always generate a Host header pointing to the host hosting the resource, not the proxy
    // (RFC 2616 is not totally clear on this, but other clients do it this way)
    DLLLOCAL QoreStringNode* getHostHeaderValueUnlocked(const con_info& connection) {
        //printd(5, "getHostHeaderValueUnlocked() connection %s:%d\n", connection.host.c_str(), connection.port);

        // RFC 7230 section 5.5: "if the connection's incoming TCP port number
        //   differs from the default port for the effective request URI's
        //   scheme, then a colon (":") and the incoming port number (in
        //   decimal form) are appended to the authority component"
        // https://tools.ietf.org/html/rfc7230#section-5.5
        // therefore, we don't include the port number if it's the default port for the protocol
        if ((!connection.ssl && connection.port == 80) || (connection.ssl && connection.port == 443)) {
            return new QoreStringNode(connection.host.c_str());
        }

        QoreStringNodeHolder str(new QoreStringNode);
        // issue #3474: Host: headers with UNIX domain sockets must be URL encoded
        if (connection.is_unix) {
            str->concat(connection.unix_urlencoded_path);
        } else {
            str->concat(connection.host);
            str->sprintf(":%d", connection.port);
        }
        return str.release();
    }

    DLLLOCAL QoreHashNode* sendMessageAndGetResponse(con_info& connection, const char* mname,
            const char* meth, const char* mpath,
            const QoreHashNode& nh, const QoreStringNode* body, const void* data, unsigned size,
            const ResolvedCallReferenceNode* send_callback, InputStream* is, size_t max_chunk_size,
            const ResolvedCallReferenceNode* trailer_callback, QoreHashNode* info, bool with_connect, int timeout_ms,
            int& code, bool& aborted, bool path_already_encoded, ExceptionSink* xsink);

    // called locked
    DLLLOCAL const char* getMsgPath(ExceptionSink* xsink, const con_info& connection, const char* mpath,
            QoreString& pstr, bool already_encoded = false) {
        pstr.clear();

        // use default path if no path is set
        if (!mpath || !mpath[0]) {
            mpath = connection.path.empty()
                ? (default_path.empty() ? "/" : (const char*)default_path.c_str())
                : (const char*)connection.path.c_str();
        }

        if (proxy_connection.has_url() && !proxy_connected) {
            // create URL string for path for proxy
            pstr.concat("http");
            if (connection.ssl) {
                pstr.concat('s');
            }
            pstr.concat("://");
            pstr.concat(connection.host.c_str());
            if (connection.port && connection.port != 80) {
                pstr.sprintf(":%d", connection.port);
            }
            if (mpath[0] != '/') {
                pstr.concat('/');
            }
        }

        if (already_encoded) {
            pstr.concat(mpath);
        } else if (pre_encoded_urls) {
            const char* p = strchrs(mpath, must_encode_chars);
            if (p) {
                xsink->raiseException("URL-ENCODING-ERROR", "URI path '%s' contains at least one unencoded character "
                    "('%c'), when the 'pre_encoded_urls' option is set, URLs must be already encoded with percent "
                    "encoding", mpath, *p);
                return nullptr;
            }
            pstr.concat(mpath);
        } else {
            // concat mpath to pstr, performing minimal URL encoding until '?'
            const char* p = mpath;
            while (*p) {
                pct_encoding_map_t::const_iterator i = pct_encoding_map.find(*p);
                if (i == pct_encoding_map.end()) {
                    pct_encoding_set_t::iterator j = local_pct_encoding_set.find(*p);
                    if (j == local_pct_encoding_set.end()) {
                        pstr.concat(*p);
                    } else {
                        QoreStringMaker tmp("%%%X", *p);
                        pstr.concat(tmp.c_str());
                    }
                } else {
                    pstr.concat(i->second);
                }
                ++p;
            }
        }

        //printd(5, "qore_httpclient_priv::getMsgPath() cpath: '%s' dp: '%s' mpath: '%s' pstr: '%s'\n",
        //    !connection.path.empty() ? connection.path.c_str() : "",
        //    !default_path.empty() ? default_path.c_str() : "",
        //    mpath, pstr.c_str());
        return (const char*)pstr.c_str();
    }

    DLLLOCAL const char* checkMethod(ExceptionSink* xsink, const char* meth, bool& bodyp) const {
        method_map.find(meth);

        // check if method is valid
        method_map_t::const_iterator i = method_map.find(meth);
        if (i == method_map.end()) {
            i = additional_methods_map.find(meth);
            if (i == additional_methods_map.end()) {
                xsink->raiseException("HTTP-CLIENT-METHOD-ERROR", "HTTP method (%s) not recognized.", meth);
                return nullptr;
            }
        }

        // make sure the capitalized version is used
        bodyp = i->second;
        return i->first.c_str();
    }

    DLLLOCAL QoreHashNode* getRequestHeaders(ExceptionSink* xsink, const QoreHashNode* headers,
            const QoreEncoding* string_body_enc, bool msg_body, bool send_chunked, bool& keep_alive,
            bool& host_override) {
        ReferenceHolder<QoreHashNode> nh(new QoreHashNode(autoTypeInfo), xsink);
        bool transfer_encoding = false;
        // issue #1824: find content-type header, if any
        const char* ct = nullptr;
        if (headers) {
            // issue #2340 track headers in a case-insensitive way
            strcase_set_t hdrs;
            ConstHashIterator hi(headers);
            while (hi.next()) {
                // if one of the mandatory headers is found, then ignore it
                strcase_set_t::iterator si = header_ignore.find(hi.getKey());
                if (si != header_ignore.end()) {
                    continue;
                }

                // otherwise set the value in the hash
                const QoreValue n = hi.get();
                if (!n.isNothing()) {
                    if (!strcasecmp(hi.getKey(), "transfer-encoding")) {
                        transfer_encoding = true;
                    } else if (!strcasecmp(hi.getKey(), "host")) {
                        host_override = true;
                    }

                    addAppendHeader(hdrs, **nh, hi.getKey(), n, xsink);

                    if (!strcasecmp(hi.getKey(), "connection")
                        || (proxy_connection.has_url() && !strcasecmp(hi.getKey(), "proxy-connection"))) {
                        const char* conn = get_string_header(xsink, **nh, hi.getKey(), true);
                        if (*xsink) {
                            disconnect_unlocked();
                            return 0;
                        }
                        if (conn && !strcasecmp(conn, "close")) {
                            keep_alive = false;
                        }
                    } else if (!strcasecmp(hi.getKey(), "content-type")) {
                        const char* ct_value = get_string_header(xsink, **nh, hi.getKey(), true);
                        if (*xsink) {
                            disconnect_unlocked();
                            return nullptr;
                        }
                        if (ct_value && !strstr(ct_value, "charset=") && !strstr(ct_value, "boundary=")) {
                            ct = hi.getKey();
                        }
                    }
                }
            }
        }

        // add default headers if they weren't overridden
        for (auto& hdri : default_headers) {
            // look in original headers to see if the key was already given
            if (headers) {
                bool skip = false;
                ConstHashIterator hi(headers);
                while (hi.next()) {
                    if (!strcasecmp(hi.getKey(), hdri.first.c_str())) {
                        skip = true;
                        break;
                    }
                }
                if (skip) {
                    continue;
                }
            }
            const char* hdr_value = hdri.second.c_str();
            // if there is no message body then do not send the "content-type" header
            if (!strcmp(hdri.first.c_str(), "Content-Type")) {
                if (!msg_body) {
                    continue;
                }
                if (!strstr(hdr_value, "charset=") && !strstr(hdr_value, "boundary=")) {
                    ct = hdri.first.c_str();
                }
            }
            nh->setKeyValue(hdri.first.c_str(), new QoreStringNode(hdr_value), xsink);
        }

        // issue #1824: add ";charset=xxx" to Content-Type header if sending non-ISO-8891-1 text
        if (string_body_enc && ct) {
            if (!enc) {
                enc = msock->socket->getEncoding();
            }
            // any string will be converted to the socket's encoding before sending, so we have to compare the socket's
            // encoding and not the string's
            if (enc != QCS_ISO_8859_1) {
                QoreStringNode* v = nh->getKeyValue(ct).get<QoreStringNode>();
                assert(v->is_unique());
                QoreString code(string_body_enc->getCode());
                code.tolwr();
                v->sprintf(";charset=%s", code.c_str());
            }
        }

        // set Transfer-Encoding: chunked if used with a send callback
        if (send_chunked && !transfer_encoding) {
            nh->setKeyValue("Transfer-Encoding", new QoreStringNode("chunked"), xsink);
        }

        if (!connection.username.empty()) {
            // check for "Authorization" header
            bool auth_found = false;
            if (headers) {
                ConstHashIterator hi(headers);
                while (hi.next()) {
                    if (!strcasecmp(hi.getKey(), "Authorization")) {
                        auth_found = true;
                        break;
                    }
                }
            }

            if (!auth_found) {
                QoreString tmp;
                tmp.sprintf("%s:%s", connection.username.c_str(), connection.password.c_str());
                QoreStringNode* auth_str = new QoreStringNode("Basic ");
                auth_str->concatBase64(&tmp);
                nh->setKeyValue("Authorization", auth_str, xsink);
            }
        }

        return nh.release();
    }

    DLLLOCAL QoreHashNode* setProxyHeaders(ExceptionSink* xsink, const con_info& connection, QoreHashNode* headers,
            bool& use_proxy_connect, const char*& proxy_path) const {
        ReferenceHolder<QoreHashNode> proxy_headers(xsink);
        // use CONNECT if we need to make an HTTPS connection from the proxy
        if (!proxy_connection.ssl && connection.ssl) {
            use_proxy_connect = true;
            SimpleRefHolder<QoreStringNode> hostport(new QoreStringNode(connection.host));
            // RFC 7231 section 4.3.6 (https://tools.ietf.org/html/rfc7231#section-4.3.6) states
            // that the hostname and port number should be included when establishing an HTTP tunnel
            // with the CONNECT method
            if (connection.port) {
                hostport->sprintf(":%d", connection.port);
            }
            proxy_path = hostport->c_str();
            proxy_headers = new QoreHashNode(autoTypeInfo);
            proxy_headers->setKeyValue("Host", hostport.release(), xsink);

            addProxyAuthorization(headers, **proxy_headers, xsink);
        } else {
            addProxyAuthorization(headers, *headers, xsink);
        }
        return proxy_headers.release();
    }

    //! process content type header
    DLLLOCAL int processContentType(ExceptionSink* xsink, QoreHashNode& ans) {
        const QoreStringNode* v = get_string_header_node(xsink, ans, "content-type");
        if (*xsink) {
            return -1;
        }

        if (!v) {
            return 0;
        }

        // see if there is a character set specification in the content-type header
        // save original content-type header before processing
        ans.setKeyValue("_qore_orig_content_type", v->refSelf(), xsink);

        const char* str = v->c_str();
        const char* p = strstr(str, "charset=");
        if (p && (p == str || *(p - 1) == ';' || *(p - 1) == ' ')) {
            // move p to start of encoding
            const char* c = p + 8;
            char quote = '\0';
            if (*c == '\'' || *c == '"') {
                quote = *c;
                ++c;
            }
            QoreString enc;
            while (*c && *c != ';' && *c != ' ' && *c != quote) {
                enc.concat(*(c++));
            }

            if (quote && *c == quote) {
                ++c;
            }

            printd(5, "qore_httpclient_priv::processContentType() setting encoding to '%s' from content-type "
                "header: '%s' (cs: %p c: %p %d)\n", enc.c_str(), str, p + 8, c);

            // set new encoding
            msock->socket->setEncoding(QEM.findCreate(&enc));
            // strip from content-type
            QoreStringNode* nc = new QoreStringNode;
            // skip any spaces before the charset=
            while (p != str && (*(p - 1) == ' ' || *(p - 1) == ';')) {
                p--;
            }
            if (p != str) {
                nc->concat(str, p - str);
            }
            if (*c) {
                nc->concat(c);
            }
            ans.setKeyValue("content-type", nc, xsink);
            str = nc->c_str();
        }
        // split into a list if ";" characters are present
        p = strchr(str, ';');
        if (p) {
            bool multipart = false;
            QoreListNode* l = new QoreListNode(stringTypeInfo);
            do {
                // skip whitespace
                while (*str == ' ') str++;
                if (str != p) {
                    int len = p - str;
                    check_headers(str, len, multipart, ans, msock->socket->getEncoding(), xsink);
                    l->push(new QoreStringNode(str, len, msock->socket->getEncoding()), xsink);
                }
                str = p + 1;
            } while ((p = strchr(str, ';')));
            // skip whitespace
            while (*str == ' ') ++str;
            // add last field
            if (*str) {
                check_headers(str, strlen(str), multipart, ans, msock->socket->getEncoding(), xsink);
                l->push(new QoreStringNode(str, msock->socket->getEncoding()), xsink);
            }
            ans.setKeyValue("content-type", l, xsink);
        }
        return 0;
    }

    DLLLOCAL QoreHashNode* send_internal(ExceptionSink* xsink, const char* mname, const char* meth, const char* mpath,
        const QoreHashNode* headers, const QoreStringNode* body, const void* data, unsigned size,
        const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback = nullptr, QoreObject* obj = nullptr,
        OutputStream* os = nullptr, InputStream* is = nullptr, size_t max_chunk_size = 0,
        const ResolvedCallReferenceNode* trailer_callback = nullptr);

    DLLLOCAL void addProxyAuthorization(const QoreHashNode* headers, QoreHashNode& h, ExceptionSink* xsink) const {
        if (proxy_connection.username.empty())
            return;

        QoreValue pauth;
        // check for "Proxy-Authorization" header
        if (headers) {
            ConstHashIterator hi(headers);
            while (hi.next()) {
                if (!strcasecmp(hi.getKey(), "Proxy-Authorization")) {
                    pauth = hi.getReferenced();
                    h.setKeyValue("Proxy-Authorization", pauth, xsink);
                    assert(!*xsink);
                    break;
                }
            }
        }

        if (!pauth) {
            QoreString tmp;
            tmp.sprintf("%s:%s", proxy_connection.username.c_str(), proxy_connection.password.c_str());
            QoreStringNode* auth_str = new QoreStringNode("Basic ");
            auth_str->concatBase64(&tmp);
            h.setKeyValue("Proxy-Authorization", auth_str, xsink);
            assert(!*xsink);
        }
    }

    int getDiscardMessageBody(ExceptionSink* xsink, QoreHashNode& ans, int timeout_ms) {
        const char* te = get_string_header(xsink, ans, "transfer-encoding");
        if (*xsink) {
            return -1;
        }

        // get response body, if any
        const char* cl = get_string_header(xsink, ans, "content-length");
        if (*xsink) {
            return -1;
        }

        int len = cl ? atoi(cl) : 0;
        Queue* event_queue = msock->socket->getQueue();
        // do not try to get a body in any case if Content-Length: 0 is sent
        if (cl && event_queue) {
            do_content_length_event(event_queue, msock->socket->priv, len);
        }

        if (te && !strcasecmp(te, "chunked")) { // check for chunked response body
            do_event(event_queue, msock->socket->priv, QORE_EVENT_HTTP_CHUNKED_START);
            ReferenceHolder<QoreHashNode> nah(msock->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink,
                "HTTPClient", QORE_SOURCE_HTTPCLIENT), xsink);
            do_event(event_queue, msock->socket->priv, QORE_EVENT_HTTP_CHUNKED_END);
            if (*xsink) {
                return -1;
            }
            return 0;
        }

        if (len) {
            qore_offset_t rc;
            QoreStringNodeHolder bstr(msock->socket->priv->recv(xsink, len, timeout_ms, rc,
                    QORE_SOURCE_HTTPCLIENT));
            if (*xsink) {
                return -1;
            }
        }

        return 0;
    }

    // Set additional characters to encode in the URI path
    DLLLOCAL void setEncodeChar(const char c) {
        pct_encoding_map_t::iterator i = pct_encoding_map.find(c);
        // ignore if already in the global map
        if (i == pct_encoding_map.end()) {
            pct_encoding_set_t::iterator j = local_pct_encoding_set.find(c);
            // ignore if already in the local set
            if (j == local_pct_encoding_set.end()) {
                local_pct_encoding_set.insert(j, c);
            }
        }
    }

    DLLLOCAL static qore_httpclient_priv* get(QoreHttpClientObject& client) {
        return client.http_priv;
    }

    DLLLOCAL static const qore_httpclient_priv* get(const QoreHttpClientObject& client) {
        return client.http_priv;
    }
};

// setup default headers
header_map_t qore_httpclient_priv::static_default_headers = {
    {"Accept", "text/html"},
    {"Content-Type", "text/html"},
    {"Connection", "Keep-Alive"},
    {"User-Agent", "Qore-HTTP-Client/" PACKAGE_VERSION},
    {"Accept-Encoding", "deflate,gzip,bzip2"},
};

// RFC-1738: encode space, <, >, ", #, %, {, }, |, \, ^, ~, [, ], `
qore_httpclient_priv::pct_encoding_map_t qore_httpclient_priv::pct_encoding_map = {
    {' ', "%20"},
    {'<', "%3C"},
    {'>', "%3E"},
    {'"', "%22"},
    {'#', "%23"},
    {'%', "%25"},
    {'{', "%7B"},
    {'}', "%7D"},
    {'|', "%7C"},
    {'\\', "%5C"},
    {'^', "%5E"},
    {'~', "%7E"},
    {'[', "%5B"},
    {']', "%5D"},
    {'`', "%60"},
};

constexpr int HS_NONE = 0;
constexpr int HS_R = 1;
constexpr int HS_RN = 2;
constexpr int HS_RNR = 3;

class HttpClientRecvHeaderPollState : public AbstractPollState {
public:
    DLLLOCAL HttpClientRecvHeaderPollState(ExceptionSink* xsink, qore_httpclient_priv* http,
            bool exit_early = false) : http(http), hdr(
                new QoreStringNode(http->msock->socket->priv->enc ? http->msock->socket->priv->enc : http->enc)
            ), exit_early(exit_early) {
        assert(http->msock->m.trylock());
    }

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) {
#ifdef DEBUG
        qore_socket_private* spriv = http->msock->socket->priv;
        assert(http->msock->m.trylock());
        assert(spriv->isOpen());
#endif

        return readHeaderIntern(xsink);
    }

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        return hdr.release();
    }

protected:
    qore_httpclient_priv* http;
    QoreStringNodeHolder hdr;
    int state = HS_NONE;
    bool exit_early;

    // returns -1 = err, 0 = data available in buffer, 1 = wait read, 2 = wait write
    DLLLOCAL int doRecv(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;
        OptionalNonBlockingHelper nbh(*spriv, true, xsink);
        if (*xsink) {
            return -1;
        }

        while (true) {
            ssize_t rc;
            if (spriv->ssl) {
                size_t real_io = 0;
                rc = spriv->ssl->doNonBlockingIo(xsink, "read", spriv->rbuf, DEFAULT_SOCKET_BUFSIZE, SslAction::READ,
                    real_io);
                if (*xsink) {
                    return -1;
                }
                if (!rc) {
                    assert(!spriv->bufoffset);
                    spriv->buflen = real_io;
                }
                assert(!rc || rc == 1 || rc == 2 || rc == 3 || rc == -1);
                return rc;
            } else {
                rc = ::recv(spriv->sock, spriv->rbuf, DEFAULT_SOCKET_BUFSIZE, 0);
                if (!rc) {
                    return 0;
                }
                if (rc > 0) {
                    assert(!spriv->bufoffset);
                    spriv->buflen = rc;
                    break;
                }
                sock_get_error();
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN
#ifdef EWOULDBLOCK
                    || errno == EWOULDBLOCK
#endif
                ) {
                    return SOCK_POLLIN;
                }
                xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
                return -1;
            }
        }
        return 0;
    }

    DLLLOCAL int readHeaderIntern(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;
        while (true) {
            char c;
            if (spriv->readByteFromBuffer(c)) {
                int rc = doRecv(xsink);
                if (!rc) {
                    if (!spriv->buflen) {
                        xsink->raiseException("SOCKET-HTTP-ERROR", "remote end closed connection while reading "
                            "header (read %zu byte%s; '%s')", hdr->size(), hdr->size() == 1 ? "" : "s", hdr->c_str());
                        return -1;
                    }
                    continue;
                }
                if (*xsink) {
                    return -1;
                }
                return rc;
            }

            // check if we can progress to the next state
            if (c == '\n') {
                if (state == HS_R) {
                    if (exit_early && hdr->empty()) {
                        return 0;
                    }
                    state = HS_RN;
                    continue;
                }
                if (state == HS_RNR) {
                    break;
                }
            } else if (c == '\r') {
                if (state == HS_NONE) {
                    state = HS_R;
                    continue;
                }
                if (state == HS_RN) {
                    state = HS_RNR;
                    continue;
                }
            }

            if (state != HS_NONE) {
                switch (state) {
                    case HS_R: hdr->concat("\r"); break;
                    case HS_RN: hdr->concat("\r\n"); break;
                    case HS_RNR: hdr->concat("\r\n\r"); break;
                }
                state = HS_NONE;
            }
            hdr->concat(c);
            if (hdr->size() >= QORE_MAX_HEADER_SIZE) {
                xsink->raiseException("SOCKET-HTTP-ERROR", "header size cannot exceed %zu bytes", hdr->size());
                return -1;
            }
        }
        hdr->concat('\n');

        //printd(5, "HttpClientRecvHeaderPollState::readHeaderIntern() read hdr: '%s'\n", hdr->c_str());

        return 0;
    }
};

constexpr int PSC_READING_SIZE = 0;
constexpr int PSC_READING_CHUNK = 1;
constexpr int PSC_READING_CHUNK_EOR = 2;
constexpr int PSC_READING_TRAILERS = 3;

class HttpClientRecvChunkedPollState : public HttpClientRecvHeaderPollState {
public:
    DLLLOCAL HttpClientRecvChunkedPollState(ExceptionSink* xsink, qore_httpclient_priv* http)
            : HttpClientRecvHeaderPollState(xsink, http, true), chunked_body(new BinaryNode) {
    }

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) {
#ifdef DEBUG
        qore_socket_private* spriv = http->msock->socket->priv;
        assert(http->msock->m.trylock());
        assert(spriv->isOpen());
#endif

        int rc;
        while (true) {
            //printd(5, "HttpClientRecvChunkedPollState::continuePoll() pstate: %d\n", pstate);
            if (pstate == PSC_READING_SIZE) {
                rc = readSizeIntern(xsink);
                //printd(5, "rc: %d *xsink: %d chunk size: %ld\n", rc, (bool)*xsink, chunk_size);
            } else if (pstate == PSC_READING_CHUNK) {
                rc = readChunkIntern(xsink);
            } else if (pstate == PSC_READING_CHUNK_EOR) {
                rc = readChunkEorIntern(xsink);
            } else if (pstate == PSC_READING_TRAILERS) {
                hdr->clear();
                rc = readHeaderIntern(xsink);
                //printd(5, "rc: %d (*xsink: %d) trailer: '%s' last chunk size: %ld\n", rc, (bool)*xsink,
                //    hdr->c_str(), chunk_size);
                if (!rc) {
                    if (chunk_size || !hdr->empty()) {
                        pstate = PSC_READING_SIZE;
                        hdr->clear();
                    } else {
                        break;
                    }
                }
            } else {
                assert(false);
            }
            if (rc) {
                break;
            }
        }

        return rc;
    }

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        return chunked_body.release();
    }

private:
    int pstate = PSC_READING_SIZE;
    SimpleRefHolder<BinaryNode> chunked_body;
    size_t chunk_size = 0;
    size_t chunk_received = 0;
    // read \r while reading size
    bool got_r = false;

    DLLLOCAL int readSizeIntern(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;

        while (true) {
            char c;
            if (spriv->readByteFromBuffer(c)) {
                int rc = doRecv(xsink);
                if (!rc) {
                    if (!spriv->buflen) {
                        xsink->raiseException("SOCKET-HTTP-ERROR", "remote end closed connection while reading "
                            "chunk");
                        return -1;
                    }
                    continue;
                }
                if (*xsink) {
                    //printd(5, "HttpClientRecvChunkedPollState::readSizeIntern() doRecv() return -1\n");
                    return -1;
                }
                return rc;
            }

            if (c == '\r') {
                if (got_r) {
                    xsink->raiseException("READ-HTTP-CHUNK-ERROR", "unexpected \\r character found in chunked input "
                        "while reading chunk size");
                    return -1;
                }
                got_r = true;
                continue;
            }
            if (c == '\n') {
                if (got_r) {
                    if (!hdr) {
                        xsink->raiseException("READ-HTTP-CHUNK-ERROR", "chunk has no size");
                        return -1;
                    }

                    //printd(5, "HttpClientRecvChunkedPollState::readSizeIntern() hdr: '%s'\n", hdr->c_str());

                    // terminate string at ';' char if present
                    ssize_t i = hdr->find(';');
                    if (i >= 0) {
                        hdr->terminate(i);
                    }
                    chunk_size = strtoll(hdr->c_str(), nullptr, 16);
                    spriv->do_chunked_read(QORE_EVENT_HTTP_CHUNK_SIZE, chunk_size, hdr->size(),
                        QORE_SOURCE_HTTPCLIENT);
                    hdr->clear();
                    if (!chunk_size) {
                        pstate = PSC_READING_TRAILERS;
                    } else if (chunk_size < 0) {
                        xsink->raiseException("READ-HTTP-CHUNK-ERROR", "negative value given for chunk size (%ld)",
                            chunk_size);
                        return -1;
                    } else {
                        chunk_received = 0;
                        pstate = PSC_READING_CHUNK;
                    }
                    break;
                }
            }
            if (got_r) {
                xsink->raiseException("READ-HTTP-CHUNK-ERROR", "unexpected character (ASCII %d) found in chunked "
                    "input after \\r character while reading chunk size", (int)c);
                return -1;
            }
            hdr->concat(c);
        }
        return 0;
    }

    DLLLOCAL int readChunkIntern(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;

        int rc;
        while (true) {
            size_t chunk_needed = chunk_size - chunk_received;

            // first take any data in the socket buffer
            if (spriv->buflen) {
                if (spriv->buflen <= chunk_needed) {
                    chunked_body->append(spriv->rbuf + spriv->bufoffset, spriv->buflen);
                    chunk_received += spriv->buflen;
                    chunk_needed -= spriv->buflen;
                    spriv->buflen = 0;
                    spriv->bufoffset = 0;
                } else {
                    chunked_body->append(spriv->rbuf + spriv->bufoffset, chunk_needed);
                    chunk_received += chunk_needed;
                    spriv->buflen -= chunk_needed;
                    spriv->bufoffset += chunk_needed;
                }

                if (chunk_received == chunk_size) {
                    pstate = PSC_READING_CHUNK_EOR;
                    got_r = false;
                    //printd(5, "HttpClientRecvChunkedPollState::readChunkIntern() chunk: %ld received: %ld "
                    //    "needed: %ld total read: %ld\n", chunk_size, chunk_received, chunk_needed,
                    //    chunked_body->size());
                    return 0;
                }
            }

            rc = doRecv(xsink);
            if (!rc) {
                if (!spriv->buflen) {
                    xsink->raiseException("SOCKET-HTTP-ERROR", "remote end closed connection while reading "
                        "chunk data");
                    return -1;
                }
                continue;
            }
            if (*xsink) {
                return -1;
            }
            break;
        }

        return rc;
    }

    DLLLOCAL int readChunkEorIntern(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;

        while (true) {
            char c;
            if (spriv->readByteFromBuffer(c)) {
                int rc = doRecv(xsink);
                if (!rc) {
                    if (!spriv->buflen) {
                        xsink->raiseException("SOCKET-HTTP-ERROR", "remote end closed connection while reading "
                            "chunk data trailing bytes");
                        return -1;
                    }
                    continue;
                }
                if (*xsink) {
                    //printd(5, "HttpClientRecvChunkedPollState::readSizeIntern() doRecv() return -1\n");
                    return -1;
                }
                return rc;
            }

            if (c == '\r' && !got_r) {
                got_r = true;
                continue;
            } else if (c == '\n' && got_r) {
                got_r = false;
                pstate = PSC_READING_SIZE;
                break;
            }

            xsink->raiseException("READ-HTTP-CHUNK-ERROR", "unexpected character with ASCII %d found in chunked "
                "input while end of chunk data", (int)c);
            return -1;
        }
        return 0;
    }
};

class HttpClientRecvUntilClosePollState : public AbstractPollState {
public:
    DLLLOCAL HttpClientRecvUntilClosePollState(ExceptionSink* xsink, qore_httpclient_priv* http) : http(http),
            body(new BinaryNode) {
        assert(http->msock->m.trylock());
    }

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;
        assert(http->msock->m.trylock());

        assert(spriv->isOpen());

        OptionalNonBlockingHelper nbh(*spriv, true, xsink);
        if (*xsink) {
            return -1;
        }

        // first take any data in the socket buffer
        if (spriv->buflen) {
            body->append(spriv->rbuf + spriv->bufoffset, spriv->buflen);
            spriv->buflen = 0;
            spriv->bufoffset = 0;
        }
        // socket buffer must be empty
        assert(!spriv->buflen);
        assert(!spriv->bufoffset);

        while (true) {
            ssize_t rc;
            if (spriv->ssl) {
                size_t real_io = 0;
                rc = spriv->ssl->doNonBlockingIo(xsink, "read", spriv->rbuf, DEFAULT_SOCKET_BUFSIZE, SslAction::READ,
                    real_io);
                if (*xsink) {
                    return -1;
                }
                if (!rc) {
                    if (!real_io) {
                        break;
                    }
                    body->append(spriv->rbuf, real_io);
                    continue;
                }
                assert(!rc || rc == 1 || rc == 2 || rc == 3 || rc == -1);
                return rc;
            } else {
                rc = ::recv(spriv->sock, spriv->rbuf, DEFAULT_SOCKET_BUFSIZE, 0);
                assert(rc);
                if (rc > 0) {
                    body->append(spriv->rbuf, rc);
                    continue;
                }
                if (!rc) {
                    break;
                }
                sock_get_error();
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN
#ifdef EWOULDBLOCK
                    || errno == EWOULDBLOCK
#endif
                ) {
                    return SOCK_POLLIN;
                }
                xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
                return -1;
            }
        }
        return 0;
    }

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        return body.release();
    }

protected:
    qore_httpclient_priv* http;
    SimpleRefHolder<BinaryNode> body;
};

// states: none [-> connecting [-> connecting-ssl]] -> sending -> receiving-header [-> receiving-body] [-> connecting-proxy-ssl] -> [received | connected]
/**
    state transitions:
    - none
      -> connecting
         -> connecting-ssl
            -> sending...
      -> sending
         -> receiving-header
            -> receiving->body
               -> received
            -> connecting...
            -> sending...
            -> connecting->proxy-ssl
               -> sending...
*/
HttpClientConnectSendRecvPollOperation::HttpClientConnectSendRecvPollOperation(ExceptionSink* xsink,
        QoreHttpClientObject* client) : client(client), request_headers(nullptr), proxy_headers(nullptr),
        info(new QoreHashNode(autoTypeInfo), nullptr), response_headers(nullptr) {
    connect_only = true;

    SafeLocker sl(client->priv->m);

    if (client->priv->setNonBlock(xsink)) {
        assert(*xsink);
        return;
    }
    set_non_block = true;

    connection = client->http_priv->connection;

    if (client->http_priv->msock->socket->isOpen()) {
        client->http_priv->disconnect_unlocked();
    }

    if (client->http_priv->proxy_connection.has_url()) {
        proxy_headers = client->http_priv->setProxyHeaders(xsink, connection, *request_headers, use_proxy_connect,
            proxy_path);
        if (proxy_headers) {
            assert(use_proxy_connect);
            assert(proxy_path);
        }
    }

    // NOTE: this will always connect, as we closed the socket above
    connectOrSend(xsink);
}

HttpClientConnectSendRecvPollOperation::HttpClientConnectSendRecvPollOperation(ExceptionSink* xsink, QoreHttpClientObject* client,
        std::string method, std::string path, const void* data, size_t size, const QoreHashNode* headers,
        const QoreEncoding* enc) : client(client), method(method), path(path), data(size && data ? data : nullptr),
        size(size), request_headers(nullptr), proxy_headers(nullptr), info(new QoreHashNode(autoTypeInfo), nullptr),
        response_headers(nullptr) {
    connect_only = false;
    {
        const char* m = client->http_priv->checkMethod(xsink, method.c_str(), bodyp);
        if (*xsink) {
            return;
        }
        method = m;
    }

    SafeLocker sl(client->priv->m);

    if (client->priv->setNonBlock(xsink)) {
        assert(*xsink);
        return;
    }
    set_non_block = true;

    connection = client->http_priv->connection;

    request_headers = client->http_priv->getRequestHeaders(xsink, headers, enc, data && size, false, keep_alive,
        host_override);
    if (*xsink) {
        return;
    }

    if (!client->http_priv->proxy_connected && client->http_priv->proxy_connection.has_url()) {
        proxy_headers = client->http_priv->setProxyHeaders(xsink, connection, *request_headers, use_proxy_connect,
            proxy_path);
        if (proxy_headers) {
            assert(use_proxy_connect);
            assert(proxy_path);
        }
    }
    printd(5, "HttpClientConnectSendRecvPollOperation::HttpClientConnectSendRecvPollOperation() this: %p "
        "proxy connected: %d proxy: %d use_proxy_connect: %d proxy_path: %s\n", this, client->http_priv->proxy_connected,
        client->http_priv->proxy_connection.has_url(), use_proxy_connect, proxy_path ? proxy_path : "n/a");

    connectOrSend(xsink);
}

void HttpClientConnectSendRecvPollOperation::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        if (set_non_block) {
            client->clearNonBlock();
        }
        if (info) {
            info.release()->deref(xsink);
        }
        client->deref(xsink);
        delete this;
    }
}

QoreValue HttpClientConnectSendRecvPollOperation::getOutput() const {
    ReferenceHolder<QoreHashNode> rv(nullptr);
    if (http_response_code != -1) {
        rv = new QoreHashNode(autoTypeInfo);
        rv->setKeyValue("code", http_response_code, nullptr);
    }
    if (!info->empty()) {
        if (!rv) {
            rv = new QoreHashNode(autoTypeInfo);
        }
        rv->setKeyValue("info", info->hashRefSelf(), nullptr);
    }
    if (request_headers) {
        if (!rv) {
            rv = new QoreHashNode(autoTypeInfo);
        }
        rv->setKeyValue("request-headers", request_headers->refSelf(), nullptr);
    }
    if (recv_data_holder) {
        if (!rv) {
            rv = new QoreHashNode(autoTypeInfo);
        }
        rv->setKeyValue("response-body", recv_data_holder->refSelf(), nullptr);
    }
    return rv.release();
}

// states: none [-> connecting [-> connecting-ssl]] -> sending -> receiving-header [-> receiving-body] [-> connecting-proxy-ssl] -> [received | connected]
/**
    state transitions:
    - none
      -> connecting
         -> connecting-ssl
            -> sending...
      -> sending
         -> receiving-header
            -> receiving->body
               -> received
            -> connecting...
            -> sending...
            -> connecting->proxy-ssl
               -> sending...
*/
QoreHashNode* HttpClientConnectSendRecvPollOperation::continuePoll(ExceptionSink* xsink) {
    QoreHashNode* rv = nullptr;

    SafeLocker sl(client->priv->m);

    if (state == SPS_NONE || state == SPS_RECEIVED) {
        // throw an exception and exit if the object is no longer valid
        if (client->priv->checkValid(xsink)) {
            return nullptr;
        }
    } else {
        // throw an exception and exit if the object is no longer open or valid
        if (client->priv->checkOpen(xsink)) {
            return nullptr;
        }
    }

    while (true) {
        //printd(5, "HttpClientConnectSendRecvPollOperation::continuePoll() this: %p %s xsink: %d poll_state: %d "
        //    "connection: %s:%d\n", this, getStateImpl(), (bool)*xsink, (bool)poll_state, connection.host.c_str(),
        //    connection.port);

        if (state == SPS_CONNECTING) {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(!poll_state);

            if (connectDone(xsink)) {
                break;
            }
            continue;
        } else if (state == SPS_CONNECTING_SSL) {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(!poll_state);

            if (startSend(xsink)) {
                break;
            }
            continue;
       } else if (state == SPS_CONNECTING_PROXY_SSL) {
            assert(!client->http_priv->proxy_connected);
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(!poll_state);
            client->http_priv->proxy_connected = true;
            // remove "Proxy-Authorization" header
            if (request_headers) {
                if (!request_headers->is_unique()) {
                    request_headers = request_headers->copy();
                }
                request_headers->removeKey("Proxy-Authorization", xsink);
                assert(!*xsink);
            }
            assert(!use_proxy_connect);
            assert(!proxy_path);
            assert(client->http_priv->proxy_connected);
            send_data_holder = nullptr;

            if (startSend(xsink)) {
                break;
            }

            continue;
        } else if (state == SPS_SENDING) {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(!poll_state);
            poll_state.reset(new HttpClientRecvHeaderPollState(xsink, client->http_priv));
            if (*xsink) {
                break;
            }
            state = SPS_RECEIVING_HEADER;
            continue;
        } else if (state == SPS_RECEIVING_HEADER) {
            // do not delete poll state when complete
            int rc = checkContinuePoll(xsink, true);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(poll_state);

            if (processResponse(xsink)) {
                break;
            }

            continue;
        } else if (state == SPS_RECEIVING_BODY) {
            // do not delete poll state when complete
            int rc = checkContinuePoll(xsink, true);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(poll_state);

            if (processReceivedBody(xsink)) {
                break;
            }

            continue;
        } else if (state == SPS_RECEIVED || state == SPS_CONNECTED) {
            if (close_connection) {
                client->http_priv->disconnect_unlocked();
            }
            break;
        } else {
            assert(false);
        }
        break;
    }

    if (*xsink) {
        assert(!rv);
        if (poll_state) {
            poll_state.reset();
        }
        client->priv->clearNonBlock();
        set_non_block = false;
        state = SPS_NONE;
    }

    return rv;
}

int HttpClientConnectSendRecvPollOperation::connectOrSend(ExceptionSink* xsink) {
    //printd(5, "HttpClientConnectSendRecvPollOperation::connectOrSend() this: %p connection: %s:%d\n", this,
    //    connection.host.c_str(), connection.port);

    assert(client->http_priv->msock->m.trylock());

    if (!client->http_priv->msock->socket->isOpen()) {
        poll_state.reset(client->http_priv->msock->socket->startConnect(xsink,
            client->http_priv->socketpath.c_str()));
        if (*xsink) {
            return -1;
        }
        if (poll_state) {
            state = SPS_CONNECTING;
            return 0;
        }
        if (connectDone(xsink)) {
            return -1;
        }
        return 0;
    }

    if (startSend(xsink)) {
        return -1;
    }
    return 0;
}

int HttpClientConnectSendRecvPollOperation::processResponse(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());
    assert(poll_state);

    // get response string
    QoreStringNodeHolder rhdr(poll_state->takeOutput().get<QoreStringNode>());
    poll_state.reset();
    qore_socket_private* spriv = client->http_priv->msock->socket->priv;
    response_headers = spriv->processHttpHeaderString(xsink, rhdr, *info, QORE_SOURCE_HTTPCLIENT);
    if (*xsink) {
        return -1;
    }

    //printd(5, "HttpClientConnectSendRecvPollOperation::processResponse() this: %p got: '%s'\n", this,
    //    rhdr->c_str());

    // check HTTP status code
    QoreValue n = response_headers->getKeyValue("status_code");
    if (n.isNothing()) {
        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "no HTTP status code received in response");
        return -1;
    }

    http_response_code = (int)n.getAsBigInt();
    // continue processing if "100 Continue" response received (ignore this response)
    if (http_response_code == 100) {
        recv_data_holder = nullptr;
        return startSend(xsink);
    }

    info->setKeyValue("response-headers", response_headers->refSelf(), xsink);
    assert(!*xsink);

    if (!response_headers->is_unique()) {
        response_headers = response_headers->copy();
    }

    // process content-type
    if (client->http_priv->processContentType(xsink, **response_headers)) {
        return -1;
    }

    // do not read any message body for messages that cannot have one
    // rfc 2616 4.4 p1 (http://tools.ietf.org/html/rfc2616#section-4.4)
    /*
        1.Any response message which "MUST NOT" include a message-body (such
        as the 1xx, 204, and 304 responses and any response to a HEAD
        request) is always terminated by the first empty line after the
        header fields, regardless of the entity-header fields present in
        the message.
    */

    // do not process message body if no message body can be sent
    if (!bodyp || (http_response_code >= 100 && http_response_code < 200) || (http_response_code == 204)
        || (http_response_code == 304)) {
        return responseDone(xsink);
    }

    // process message body

    // get Connection header
    const char* conn = get_string_header(xsink, **response_headers, "connection", true);
    if (*xsink) {
        return -1;
    }
    close_connection = conn && !strcasecmp(conn, "close");

    // get Transfer-Encoding header
    bool chunked = false;
    const char* transfer_encoding = get_string_header(xsink, **response_headers, "transfer-encoding");
    if (*xsink) {
        return -1;
    }
    if (transfer_encoding) {
        if (!strcasecmp(transfer_encoding, "chunked")) {
            chunked = true;
        } else {
            xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "cannot handle Transfer-Encoding: %s; expecting "
                "'chunked'", transfer_encoding);
            return -1;
        }
    }

    // get response body, if any
    const char* content_length = get_string_header(xsink, **response_headers, "content-length");
    if (*xsink) {
        return -1;
    }
    ssize_t len = content_length ? strtoll(content_length, nullptr, 10) : 0;

    // issue #3691: read the body until the socket is closed if we have Connection: close
    if (!chunked && close_connection && !content_length) {
        poll_state.reset(new HttpClientRecvUntilClosePollState(xsink, client->http_priv));
        state = SPS_RECEIVING_BODY;
        //printd(5, "HttpClientConnectSendRecvPollOperation::processResponse() created "
        //    "HttpClientRecvUntilClosePollState\n", len);
        return 0;
    }

    if (content_length) {
        Queue* event_queue = client->priv->socket->getQueue();
        if (event_queue) {
            do_content_length_event(event_queue, spriv, len);
        }
    }

    if (chunked) {
        poll_state.reset(new HttpClientRecvChunkedPollState(xsink, client->http_priv));
        state = SPS_RECEIVING_BODY;
        //printd(5, "HttpClientConnectSendRecvPollOperation::processResponse() created "
        //    "HttpClientRecvChunkedPollState (Transfer-Encoding: %s Content-Length: %ld)\n", transfer_encoding, len);
        return 0;
    }

    if (len > 0) {
        poll_state.reset(new SocketRecvPollState(xsink, spriv, len));
        state = SPS_RECEIVING_BODY;
        //printd(5, "HttpClientConnectSendRecvPollOperation::processResponse() created SocketRecvPollState "
        //    "(Content-Length: %ld)\n", len);
        return 0;
    }

    //printd(5, "HttpClientConnectSendRecvPollOperation::processResponse() this: %p calling responseDone()\n", this);
    return responseDone(xsink);
}

int HttpClientConnectSendRecvPollOperation::processReceivedBody(ExceptionSink* xsink) {
    recv_data_holder = poll_state->takeOutput().get<BinaryNode>();
    poll_state.reset();

    const char* content_encoding = get_string_header(xsink, **response_headers, "content-encoding");
    if (*xsink) {
        return -1;
    }

    if (content_encoding) {
        // check for misuse of this header by including a character encoding value
        if (!strncasecmp(content_encoding, "iso", 3) || !strncasecmp(content_encoding, "utf-", 4)) {
            client->http_priv->msock->socket->setEncoding(QEM.findCreate(content_encoding));
        } else {
            qore_uncompress_to_binary_t dec = nullptr;
            // only decode message bodies automatically if there is no receive callback
            if (!strcasecmp(content_encoding, "deflate") || !strcasecmp(content_encoding, "x-deflate")) {
                dec = qore_inflate_to_binary;
            } else if (!strcasecmp(content_encoding, "gzip") || !strcasecmp(content_encoding, "x-gzip")) {
                dec = qore_gunzip_to_binary;
            } else if (!strcasecmp(content_encoding, "bzip2") || !strcasecmp(content_encoding, "x-bzip2")) {
                dec = qore_bunzip2_to_binary;
            } else {
                xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "don't know how to handle content-encoding "
                    "'%s'", content_encoding);
                return -1;
            }

            int64 body_size = recv_data_holder->size();
            recv_data_holder = dec(*recv_data_holder, xsink);
            if (*xsink) {
                xsink->appendLastDescription(": while decompressing '%s' Content-Encoding with size " QLLD,
                    content_encoding, body_size);
                return -1;
            }
        }
    }

    //printd(5, "HttpClientConnectSendRecvPollOperation::processReceivedBody() this: %p received body: %ld "
    //    "(Content-Encoding: '%s')\n", this, recv_data_holder ? recv_data_holder->size() : 0l,
    //    content_encoding ? content_encoding : "n/a");

    // warning, the following line will output raw binary data
    //printd(5, "HttpClientConnectSendRecvPollOperation::processReceivedBody() this: %p body received: '%s'\n", this,
    //    recv_data_holder ? recv_data_holder->getPtr() : "n/a");

    return responseDone(xsink);
}

int HttpClientConnectSendRecvPollOperation::responseDone(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());

    // issue #3116: pass a 304 Not Modified message back to the caller without processing
    if (!client->http_priv->redirect_passthru && http_response_code >= 300 && http_response_code < 400
        && http_response_code != 304) {
        //printd(5, "HttpClientConnectSendRecvPollOperation::responseDone() code: %d redirecting\n",
        //    http_response_code);
        return redirect(xsink);
    }

    if (use_proxy_connect) {
        //printd(5, "HttpClientConnectSendRecvPollOperation::responseDone() code: %d connecting SSL (proxy connect: %d "
        //    "proxy path: %s)\n", http_response_code, use_proxy_connect, proxy_path);

        use_proxy_connect = false;
        proxy_path = nullptr;

        // set client target for SNI
        client->http_priv->msock->socket->priv->client_target = client->http_priv->connection.host;

        poll_state.reset(client->http_priv->msock->socket->startSslConnect(xsink,
            client->http_priv->msock->cert ? client->http_priv->msock->cert->getData() : nullptr,
            client->http_priv->msock->pk ? client->http_priv->msock->pk->getData() : nullptr,
            client->http_priv->msock->cert ? client->http_priv->msock->cert->getVerifyCACertificate() : nullptr));
        if (*xsink) {
            return -1;
        }

        state = SPS_CONNECTING_PROXY_SSL;
        return 0;
    }

    //printd(5, "HttpClientConnectSendRecvPollOperation::responseDone() code: %d done -> received\n",
    //    http_response_code);
    setFinal(SPS_RECEIVED);
    return 0;
}

int HttpClientConnectSendRecvPollOperation::redirect(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());

    // if we need to redirect to a new host, then disconnect the current socket
    if (!client->http_priv->proxy_connection.has_url()) {
        client->http_priv->disconnect_unlocked();
    } else if (use_proxy_connect) {
        use_proxy_connect = false;
        proxy_path = nullptr;
        client->http_priv->disconnect_unlocked();
    }
    // purge outbound message buffer
    send_data_holder = nullptr;

    host_override = false;
    const QoreStringNode* mess = response_headers->getKeyValue("status_message").get<QoreStringNode>();
    const QoreStringNode* loc = get_string_header_node(xsink, **response_headers, "location");
    if (*xsink) {
        return -1;
    }
    const char* location = loc && !loc->empty() ? loc->c_str() : 0;
    if (!location) {
        const char* msg = mess && !mess->empty() ? mess->c_str() : "<no message>";
        xsink->raiseException("HTTP-CLIENT-REDIRECT-ERROR", "no redirect location given for status code %d: "
            "message: '%s'", http_response_code, msg);
        return -1;
    }

    qore_socket_private* spriv = client->http_priv->msock->socket->priv;
    Queue* event_queue = client->priv->socket->getQueue();
    if (event_queue) {
        do_redirect_event(event_queue, spriv, loc, mess);
    }

    if (++redirect_count > (unsigned)client->http_priv->max_redirects) {
        const char* msg = mess && !mess->empty() ? mess->c_str() : "<no message>";
        xsink->raiseException("HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED", "maximum redirections (%d) exceeded; "
            "redirect code %d to '%s' ignored (message: '%s')", client->http_priv->max_redirects, http_response_code,
            location, msg);
        return -1;
    }

    if (client->http_priv->redirectUrlUnlocked(location, connection, xsink)) {
        assert(*xsink);
        const char* msg = mess && !mess->empty() ? mess->c_str() : "<no message>";
        xsink->appendLastDescription(": while setting URL for redirect location '%s' (code %d: message: '%s')",
            location, http_response_code, msg);
        return -1;
    }

    if (!path_already_encoded) {
        path_already_encoded = true;
    }

    // set redirect info in info hash
    QoreString tmp;
    tmp.sprintf("redirect-%d", redirect_count);
    info->setKeyValue(tmp.c_str(), loc->refSelf(), xsink);
    assert(!*xsink);
    tmp.clear();
    tmp.sprintf("redirect-message-%d", redirect_count);
    info->setKeyValue(tmp.c_str(), mess ? mess->refSelf() : 0, xsink);
    assert(!*xsink);

    //printd(5, "HttpClientConnectSendRecvPollOperation::redirect() this: %p redirecting to: '%s'\n", this, location);

    return connectOrSend(xsink);
}

int HttpClientConnectSendRecvPollOperation::connectDone(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());

    if (client->http_priv->proxy_connection.has_url()
        ? client->http_priv->proxy_connection.ssl
        : client->http_priv->connection.ssl) {
        poll_state.reset(client->http_priv->msock->socket->startSslConnect(xsink,
            client->http_priv->msock->cert ? client->http_priv->msock->cert->getData() : nullptr,
            client->http_priv->msock->pk ? client->http_priv->msock->pk->getData() : nullptr));
        if (*xsink) {
            return -1;
        }
        state = SPS_CONNECTING_SSL;
        return 0;
    }
    if (startSend(xsink)) {
        return -1;
    }
    return 0;
}

int HttpClientConnectSendRecvPollOperation::startSend(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());

    qore_socket_private* spriv = client->http_priv->msock->socket->priv;

    assert(!poll_state);
    if (!send_data_holder) {
        if (use_proxy_connect) {
            QoreString hdr(spriv->enc);
            spriv->getSendHttpMessageHeaders(hdr, *info, "CONNECT",
                proxy_path, client->http_priv->http11 ? "1.1" : "1.0", *proxy_headers, 0, QORE_SOURCE_HTTPCLIENT);
            send_data_holder = getMessage(hdr);
        } else if (connect_only) {
            setFinal(SPS_CONNECTED);
            return 0;
        } else {
            // set host field automatically if not overridden
            if (!host_override) {
                request_headers->setKeyValue("Host", client->http_priv->getHostHeaderValueUnlocked(connection),
                    xsink);
                assert(!*xsink);
                //printd(5, "HttpClientConnectSendRecvPollOperation::startSend() this: %p set host: %s\n", this,
                //    request_headers->getKeyValue("Host").get<QoreStringNode>()->c_str());
            }

            QoreString pathstr(spriv->enc);
            client->http_priv->getMsgPath(xsink, connection, path.c_str(), pathstr, path_already_encoded);
            if (*xsink) {
                return -1;
            }
            //printd(5, "HttpClientConnectSendRecvPollOperation::startSend() this: %p %s %s\n", this, method.c_str(),
            //    pathstr.c_str());
            QoreString hdr(spriv->enc);
            spriv->getSendHttpMessageHeaders(hdr, *info, method.c_str(),
                pathstr.c_str(), client->http_priv->http11 ? "1.1" : "1.0", *request_headers, size,
                QORE_SOURCE_HTTPCLIENT);
            send_data_holder = getMessage(hdr);
            if (data) {
                assert(size);
                send_data_holder->append(data, size);
            }
        }
    }

    assert(send_data_holder);
    poll_state.reset(new SocketSendPollState(xsink, spriv, (const char*)send_data_holder->getPtr(),
        send_data_holder->size()));
    if (*xsink) {
        return -1;
    }

    //printd(5, "HttpClientConnectSendRecvPollOperation::startSend() this: %p sending %ld bytes\n", this,
    //    send_data_holder->size());

    // NOTE: the following line will print binary data
    //printd(5, "HttpClientConnectSendRecvPollOperation::startSend() this: %p sending: '%s'\n", this,
    //    send_data_holder->getPtr());

    state = SPS_SENDING;
    assert(poll_state);
    return 0;
}

BinaryNode* HttpClientConnectSendRecvPollOperation::getMessage(QoreString& hdr) {
    size_t strsize = hdr.size();
    return new BinaryNode(hdr.giveBuffer(), strsize);
}

void HttpClientConnectSendRecvPollOperation::setFinal(int final_state) {
    assert(client->http_priv->msock->m.trylock());

    state = final_state;
    client->priv->clearNonBlock();
    set_non_block = false;
}

// static initialization
void QoreHttpClientObject::static_init() {
    // setup static members of QoreHttpClientObject class
    method_map.insert(method_map_t::value_type("OPTIONS", true));
    // FIXME: GET should not take a message body; this should be false
    // but it cannot be changed or it would break backwards compatibility
    // appropriate notes have been added to the API docs
    method_map.insert(method_map_t::value_type("GET", true));
    method_map.insert(method_map_t::value_type("HEAD", false));
    method_map.insert(method_map_t::value_type("POST", true));
    method_map.insert(method_map_t::value_type("PUT", true));
    method_map.insert(method_map_t::value_type("DELETE", true));
    method_map.insert(method_map_t::value_type("TRACE", true));
    method_map.insert(method_map_t::value_type("CONNECT", true));
    // PATCH: https://tools.ietf.org/html/rfc5789
    method_map.insert(method_map_t::value_type("PATCH", true));

    header_ignore.insert("Content-Length");
}

QoreHttpClientObject::QoreHttpClientObject() : http_priv(new qore_httpclient_priv(priv)) {
    http_priv->setSocketPath(http_priv->connection);
}

QoreHttpClientObject::~QoreHttpClientObject() {
    delete http_priv;
}

void QoreHttpClientObject::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        cleanup(xsink);
        delete this;
    }
}

QoreObject* QoreHttpClientObject::startPollConnect(ExceptionSink* xsink, QoreObject* self) {
    return http_priv->startPollConnect(xsink, self, this);
}

QoreObject* QoreHttpClientObject::startPollSendRecv(ExceptionSink* xsink, QoreObject* self, const QoreString* method,
            const QoreString* path, const AbstractQoreNode* data_save, const void* data, size_t size,
            const QoreHashNode* headers, const QoreEncoding* enc) {
    return http_priv->startPollSendRecv(xsink, self, this, method, path, data_save, data, size, headers, enc);
}

void QoreHttpClientObject::setDefaultPort(int def_port) {
    http_priv->default_port = def_port;
}

const char* QoreHttpClientObject::getDefaultPath() const {
    return http_priv->default_path.empty() ? 0 : http_priv->default_path.c_str();
}

const char* QoreHttpClientObject::getConnectionPath() const {
    SafeLocker sl(priv->m);
    return http_priv->connection.path.empty() ? getDefaultPath() : http_priv->connection.path.c_str();
}

void QoreHttpClientObject::setConnectionPath(const char* path) {
    SafeLocker sl(priv->m);
    if (path && path[0]) {
        http_priv->connection.path = path;
    } else {
        http_priv->connection.path.clear();
    }
}

void QoreHttpClientObject::setDefaultPath(const char* def_path) {
    // issue #2610: assigning a std::string a nullptr causes a crash
    http_priv->default_path = def_path ? def_path : "";
}

void QoreHttpClientObject::setTimeout(int to) {
    http_priv->timeout = to;
}

int QoreHttpClientObject::getTimeout() const {
    return http_priv->timeout;
}

void QoreHttpClientObject::setEncoding(const QoreEncoding* qe) {
    http_priv->setEncoding(qe);
}

const QoreEncoding* QoreHttpClientObject::getEncoding() const {
    return http_priv->getEncoding();
}

int QoreHttpClientObject::setOptions(const QoreHashNode* opts, ExceptionSink* xsink) {
    // process new protocols
    QoreValue n = opts->getKeyValue("protocols");

    if (n.getType() == NT_HASH) {
        const QoreHashNode* h = n.get<const QoreHashNode>();
        ConstHashIterator hi(h);
        while (hi.next()) {
            const QoreValue v = hi.get();
            qore_type_t vtype = v.getType();
            if (vtype != NT_HASH && vtype != NT_INT) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "value of protocol hash key '%s' is not a hash or "
                    "an int", hi.getKey());
                return -1;
            }
            bool need_ssl = false;
            int need_port;
            if (vtype == NT_INT) {
                need_port = (int)v.getAsBigInt();
            } else {
                const QoreHashNode* vh = v.get<const QoreHashNode>();
                need_port = (int)vh->getKeyValue("port").getAsBigInt();
                if (!need_port) {
                    xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "'port' key in protocol hash key '%s' is "
                        "missing or zero", hi.getKey());
                    return -1;
                }
                need_ssl = vh->getKeyValue("ssl").getAsBool();
            }
            http_priv->prot_map[hi.getKey()] = make_protocol(need_port, need_ssl);
        }
    }

    n = opts->getKeyValue("max_redirects");
    if (!n.isNothing())
        http_priv->max_redirects = (int)n.getAsBigInt();

    n = opts->getKeyValue("default_port");
    if (!n.isNothing()) {
        http_priv->default_port = (int)n.getAsBigInt();
    } else {
        http_priv->default_port = HTTPCLIENT_DEFAULT_PORT;
    }

    // check if proxy is true
    n = opts->getKeyValue("proxy");
    if (n.getType() == NT_STRING && http_priv->setProxyUrlUnlocked((n.get<const QoreStringNode>())->c_str(), xsink)) {
        return -1;
    }

    // parse url option if present
    n = opts->getKeyValue("url");
    if (n.getType() == NT_STRING && http_priv->setUrlUnlocked((n.get<const QoreStringNode>())->c_str(), xsink)) {
        return -1;
    }

    // set username and password, if applicable
    if (http_priv->connection.username.empty() && http_priv->connection.password.empty()) {
        n = opts->getKeyValue("username");
        if (n.getType() == NT_STRING) {
            const QoreValue p = opts->getKeyValue("password");
            if (p.getType() == NT_STRING) {
                http_priv->setUserPassword(n.get<const QoreStringNode>()->c_str(),
                    p.get<const QoreStringNode>()->c_str());
            }
        }
    }

    n = opts->getKeyValue("default_path");
    if (n.getType() == NT_STRING) {
        http_priv->default_path = (n.get<const QoreStringNode>())->c_str();
    }

    // set default timeout if given in option hash - accept relative date/time values as well as integers
    n = opts->getKeyValue("timeout");
    if (!n.isNothing()) {
        http_priv->timeout = get_ms_zero(n);
    }

    n = opts->getKeyValue("http_version");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string version ('1.0', '1.1') as value for "
                "the \"http_version\" key in the options hash");
            return -1;
        }
        if (setHTTPVersion((n.get<const QoreStringNode>())->c_str(), xsink))
            return -1;
    }

    n = opts->getKeyValue("headers");
    if (!n.isNothing()) {
        if (n.getType() != NT_HASH) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting hash of headers as value for the "
                "\"headers\" key in the options hash");
            return -1;
        }
        addDefaultHeaders(n.get<const QoreHashNode>());
    }

    n = opts->getKeyValue("event_queue");
    if (n.getType() == NT_OBJECT) {
        const QoreObject* o = n.get<const QoreObject>();
        Queue* q = static_cast<Queue*>(o->getReferencedPrivateData(CID_QUEUE, xsink));
        if (*xsink)
            return -1;

        if (q) { // pass reference from QoreObject::getReferencedPrivateData() to function
            priv->socket->setEventQueue(xsink, q, QoreValue(), false);
        }
    }

    n = opts->getKeyValue("connect_timeout");
    if (!n.isNothing()) {
        http_priv->connect_timeout_ms = (int)get_ms_zero(n);
    }

    if (http_priv->connection.path.empty() && !http_priv->default_path.empty()) {
        http_priv->connection.path = http_priv->default_path;
    }

    // additional HTTP methods for customized extensions like WebDAV
    n = opts->getKeyValue("additional_methods");
    if (!n.isNothing()) {
        if (n.getType() != NT_HASH) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "Option \"additional_methods\" requires a hash as a "
                "value; got: %s", n.getTypeName());
            return -1;
        }
        ConstHashIterator hi(n.get<const QoreHashNode>());
        while (hi.next()) {
            http_priv->addHttpMethod(hi.getKey(), hi.get().getAsBool());
        }
    }

    // issue #3693: assume HTTP encoding
    n = opts->getKeyValue("assume_encoding");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string as value for the \"assume_encoding\" "
                "key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        const QoreStringNode* val = n.get<const QoreStringNode>();
        qore_socket_private::get(*priv->socket)->setAssumedEncoding(!val->empty() ? val->c_str() : nullptr);
    }

    n = opts->getKeyValue("ssl_cert_data");
    if (n) {
        SimpleRefHolder<QoreSSLCertificate> cert;
        if (n.getType() == NT_BINARY) {
            cert = new QoreSSLCertificate(n.get<const BinaryNode>(), xsink);
            if (*xsink) {
                return -1;
            }
        } else if (n.getType() == NT_STRING) {
            cert = new QoreSSLCertificate(n.get<const QoreStringNode>(), xsink);
            if (*xsink) {
                return -1;
            }
        } else {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting \"string\" or \"binary\" value assigned to "
                "\"ssl_cert_data\" HTTPClient option; got type \"%s\" instead", n.getTypeName());
            return -1;
        }

        assert(!priv->cert);
        priv->cert = cert.release();
    } else {
        n = opts->getKeyValue("ssl_cert_path");
        if (!n.isNothing()) {
            if (n.getType() != NT_STRING) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string filename as value for the "
                    "\"ssl_cert_path\" key in the options hash; got type \"%s\" instead", n.getTypeName());
                return -1;
            }
            const QoreStringNode* path = n.get<const QoreStringNode>();
            if (runtime_check_parse_option(PO_NO_FILESYSTEM)) {
                xsink->raiseException("ILLEGAL-FILESYSTEM-ACCESS", "cannot use the \"ssl_cert_path\" option = \"%s\" "
                    "when sandboxing restriction PO_NO_FILESYSTEM is set", path->c_str());
                return -1;
            }

            // read in certificate file and set the certificate
            QoreFile f;
            if (f.open2(xsink, path->c_str())) {
                return -1;
            }

            QoreString pem;
            if (f.read(pem, -1, xsink)) {
                return -1;
            }

            SimpleRefHolder<QoreSSLCertificate> cert(new QoreSSLCertificate(&pem, xsink));
            if (*xsink) {
                return -1;
            }

            assert(!priv->cert);
            priv->cert = cert.release();
        }
    }

    const char* key_password = nullptr;
    n = opts->getKeyValue("ssl_key_password");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string value for the \"ssl_key_password\" "
                "key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        key_password = n.get<const QoreStringNode>()->c_str();
    }

    n = opts->getKeyValue("ssl_key_data");
    if (n) {
        SimpleRefHolder<QoreSSLPrivateKey> pk;
        if (n.getType() == NT_BINARY) {
            // no private key possible with keys in DER format
            pk = new QoreSSLPrivateKey(n.get<const BinaryNode>(), xsink);
            if (*xsink) {
                return -1;
            }
        } else if (n.getType() == NT_STRING) {
            pk = new QoreSSLPrivateKey(n.get<const QoreStringNode>(), key_password, xsink);
            if (*xsink) {
                return -1;
            }
        } else {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting \"string\" or \"binary\" value assigned to "
                "\"ssl_key_data\" HTTPClient option; got type \"%s\" instead", n.getTypeName());
            return -1;
        }

        assert(!priv->pk);
        priv->pk = pk.release();
    } else {
        n = opts->getKeyValue("ssl_key_path");
        if (!n.isNothing()) {
            if (n.getType() != NT_STRING) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string filename as value for the "
                    "\"ssl_key_path\" key in the options hash; got type \"%s\" instead", n.getTypeName());
                return -1;
            }
            const QoreStringNode* path = n.get<const QoreStringNode>();
            if (runtime_check_parse_option(PO_NO_FILESYSTEM)) {
                xsink->raiseException("ILLEGAL-FILESYSTEM-ACCESS", "cannot use the \"ssl_key_path\" option = \"%s\" "
                    "when sandboxing restriction PO_NO_FILESYSTEM is set", path->c_str());
                return -1;
            }

            // read in private key file and set the private key
            QoreFile f;
            if (f.open2(xsink, path->c_str())) {
                return -1;
            }

            QoreString pem;
            if (f.read(pem, -1, xsink)) {
                return -1;
            }

            SimpleRefHolder<QoreSSLPrivateKey> pk(new QoreSSLPrivateKey(&pem, key_password, xsink));
            if (*xsink) {
                return -1;
            }

            assert(!priv->pk);
            priv->pk = pk.release();
        }
    }

    n = opts->getKeyValue("ssl_verify_cert");
    if (n.getAsBool()) {
        priv->socket->setSslVerifyMode(SSL_VERIFY_PEER);
    }

    n = opts->getKeyValue("error_passthru");
    if (n.getAsBool()) {
        http_priv->error_passthru = true;
    }

    n = opts->getKeyValue("redirect_passthru");
    if (n.getAsBool()) {
        http_priv->redirect_passthru = true;
    }

    n = opts->getKeyValue("encoding_passthru");
    if (n.getAsBool()) {
        http_priv->encoding_passthru = true;
    }

    n = opts->getKeyValue("pre_encoded_urls");
    if (n.getAsBool()) {
        http_priv->pre_encoded_urls = true;
    }

    // issue #4773: allow the set of automatically-percent-encoded characters to be expanded
    n = opts->getKeyValue("encode_chars");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting a string as the value for the "
                "\"encode_chars\" key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        const QoreStringNode* chars = n.get<const QoreStringNode>();
        for (size_t i = 0, e = chars->size(); i < e; ++i) {
            http_priv->setEncodeChar((*chars)[i]);
        }
    }

    // issue #3978: allow the output encoding to be set as an option
    n = opts->getKeyValue("encoding");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting a string encoding as the value for the "
                "\"encoding\" key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        const QoreStringNode* enc_str = n.get<const QoreStringNode>();
        const QoreEncoding* enc = QEM.findCreate(enc_str);
        priv->socket->setEncoding(enc);
        http_priv->enc = enc;
    }

    return 0;
}

void QoreHttpClientObject::setConnectTimeout(int ms) {
    SafeLocker sl(priv->m);
    http_priv->connect_timeout_ms = ms < 0 ? -1 : ms;
}

int QoreHttpClientObject::getConnectTimeout() const {
    return http_priv->connect_timeout_ms;
}

int QoreHttpClientObject::setURL(const char* str, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    // disconnect immediately if not using a proxy
    if (!http_priv->proxy_connection.has_url())
        http_priv->disconnect_unlocked();
    return http_priv->setUrlUnlocked(str, xsink);
}

QoreStringNode* QoreHttpClientObject::getURL() {
    SafeLocker sl(priv->m);

    if (!http_priv->connection.has_url())
        return nullptr;

    return http_priv->connection.get_url();
}

QoreStringNode* QoreHttpClientObject::getSafeURL() {
    SafeLocker sl(priv->m);

    if (!http_priv->connection.has_url())
        return nullptr;

    return http_priv->connection.get_url(URL_MASK_PASSWORD);
}

int QoreHttpClientObject::setHTTPVersion(const char* version, ExceptionSink* xsink) {
    int rc = 0;
    SafeLocker sl(priv->m);
    if (!strcmp(version, "1.0")) {
        http_priv->http11 = false;
    } else if (!strcmp(version, "1.1")) {
        http_priv->http11 = true;
    } else {
        xsink->raiseException("HTTP-VERSION-ERROR", "only '1.0' and '1.1' are valid (value passed: '%s')", version);
        rc = -1;
    }
    return rc;
}

const char* QoreHttpClientObject::getHTTPVersion() const {
    return http_priv->http11 ? "1.1" : "1.0";
}

void QoreHttpClientObject::setHTTP11(bool val) {
    http_priv->http11 = val;
}

bool QoreHttpClientObject::isHTTP11() const {
    return http_priv->http11;
}

int QoreHttpClientObject::setProxyURL(const char* proxy, ExceptionSink* xsink)  {
    SafeLocker sl(priv->m);

    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    http_priv->disconnect_unlocked();
    if (!proxy || !proxy[0]) {
        http_priv->proxy_connection.clear();
        return 0;
    }
    return http_priv->setProxyUrlUnlocked(proxy, xsink);
}

QoreStringNode* QoreHttpClientObject::getProxyURL()  {
    SafeLocker sl(priv->m);

    if (!http_priv->proxy_connection.has_url())
        return nullptr;

    return http_priv->proxy_connection.get_url();
}

QoreStringNode* QoreHttpClientObject::getSafeProxyURL()  {
    SafeLocker sl(priv->m);

    if (!http_priv->proxy_connection.has_url()) {
        return nullptr;
    }

    return http_priv->proxy_connection.get_url(URL_MASK_PASSWORD);
}

void QoreHttpClientObject::clearProxyURL() {
    SafeLocker sl(priv->m);
    http_priv->proxy_connection.clear();
    http_priv->setSocketPath(http_priv->connection);
}

QoreStringNode* QoreHttpClientObject::getUsername() const {
    SafeLocker sl(priv->m);
    if (!http_priv->connection.username.empty()) {
        return new QoreStringNode(http_priv->connection.username);
    }
    return nullptr;
}

QoreStringNode* QoreHttpClientObject::getPassword() const {
    SafeLocker sl(priv->m);
    if (!http_priv->connection.password.empty()) {
        return new QoreStringNode(http_priv->connection.password);
    }
    return nullptr;
}

QoreStringNode* QoreHttpClientObject::getProxyUsername() const {
    SafeLocker sl(priv->m);
    if (!http_priv->proxy_connection.username.empty()) {
        return new QoreStringNode(http_priv->proxy_connection.username);
    }
    return nullptr;
}

QoreStringNode* QoreHttpClientObject::getProxyPassword() const {
    SafeLocker sl(priv->m);
    if (!http_priv->proxy_connection.password.empty()) {
        return new QoreStringNode(http_priv->proxy_connection.password);
    }
    return nullptr;
}

void QoreHttpClientObject::setSecure(bool is_secure) {
    lock();
    http_priv->connection.ssl = is_secure;
    unlock();
}

bool QoreHttpClientObject::isSecure() const {
    return http_priv->connection.ssl;
}

void QoreHttpClientObject::setProxySecure(bool is_secure) {
    lock();
    http_priv->proxy_connection.ssl = is_secure;
    unlock();
}

bool QoreHttpClientObject::isProxySecure() const {
    return http_priv->proxy_connection.ssl;
}

int QoreHttpClientObject::connect(ExceptionSink* xsink) {
    SafeLocker sl(priv->m);

    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    http_priv->disconnect_unlocked();
    return http_priv->connect_unlocked(xsink, http_priv->connection);
}

void QoreHttpClientObject::disconnect() {
    SafeLocker sl(priv->m);
    http_priv->disconnect_unlocked();
}

QoreHashNode* qore_httpclient_priv::sendMessageAndGetResponse(con_info& connection, const char* mname,
        const char* meth, const char* mpath,
        const QoreHashNode& nh, const QoreStringNode* body, const void* data, unsigned size,
        const ResolvedCallReferenceNode* send_callback, InputStream* is, size_t max_chunk_size,
        const ResolvedCallReferenceNode* trailer_callback, QoreHashNode* info, bool with_connect, int timeout_ms,
        int& code, bool& aborted, bool path_already_encoded, ExceptionSink* xsink) {
    // issue #3978: make sure and reset output encoding if any is set
    if (enc) {
        msock->socket->setEncoding(enc);
    }

    QoreString pathstr(msock->socket->getEncoding());
    const char* msgpath = with_connect ? mpath : getMsgPath(xsink, connection, mpath, pathstr, path_already_encoded);
    if (*xsink) {
        return nullptr;
    }

    if (!msock->socket->isOpen()) {
        if (persistent) {
            xsink->raiseException("PERSISTENCE-ERROR", "the current connection has been temporarily marked as " \
                "persistent, but has been disconnected");
            return nullptr;
        }

        if (connect_unlocked(xsink, connection)) {
            // if we have an info hash then write the request-uri key for reporting/logging purposes
            if (info)
                info->setKeyValue("request-uri", new QoreStringNodeMaker("%s %s HTTP/%s", meth,
                    msgpath && msgpath[0] ? msgpath : "/", http11 ? "1.1" : "1.0"), 0);
            return nullptr;
        }
    }

    // send the message
    int rc = msock->socket->priv->sendHttpMessage(xsink, info, "HTTPClient", mname, meth, msgpath,
        http11 ? "1.1" : "1.0", &nh, body, data, size, send_callback, is, max_chunk_size, trailer_callback,
        QORE_SOURCE_HTTPCLIENT, timeout_ms, &msock->m, &aborted);

    //printd(5, "qore_httpclient_priv::sendMessageAndGetResponse() '%s' path: '%s' data: %p size: %d send_callback: %p is: %p aborted: %d rc: %d\n", meth, msgpath, data, (int)size, send_callback, is, aborted, rc);

    // do not exit immediately if the transfer was aborted with a streaming send unless the socket was already closed
    if (rc && (!send_callback || !aborted || !msock->socket->isOpen())) {
        assert(*xsink);
        if (rc == QSE_NOT_OPEN) {
            disconnect_unlocked();
        }
        return nullptr;
    }

    // issue #3564 in case an outbound chunked transfer was aborted and we have incoming data,
    // change the timeout to 5 seconds to avoid stalling I/O in case of a slow or incomplete response,
    // because anyway the connection must be aborted; we just try to quickly read any possible incoming
    // HTTP response for error reporting
    if (aborted && (timeout < 0 || timeout > ABORTED_TIMEOUT_MS)) {
        timeout = ABORTED_TIMEOUT_MS;
        //printd(5, "qore_httpclient_priv::sendMessageAndGetResponse() aborted: %d timeout: %d open: %d\n", aborted,
        //    timeout, msock->socket->isOpen());
    }

    // if the transfer was aborted with a streaming send, but the socket is still open, then try to read a response
    ReferenceHolder<QoreHashNode> response_hash(xsink);
    while (true) {
        qore_offset_t rc;
        response_hash = msock->socket->priv->readHTTPHeader(xsink, info, timeout, rc, QORE_SOURCE_HTTPCLIENT,
            "response-headers-raw");
        if (!(*response_hash)) {
            disconnect_unlocked();
            assert(*xsink);
            return nullptr;
        }

        // check HTTP status code
        QoreValue v = response_hash->getKeyValue("status_code");
        if (v.isNothing()) {
            xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "no HTTP status code received in response");
            return nullptr;
        }

        code = (int)v.getAsBigInt();
        // continue processing if "100 Continue" response received (ignore this response)
        if (code == 100) {
            continue;
        }

        break;
    }

    // only clear exceptions if a streaming (ie chunked) send was aborted and we really got a response from the remote
    if (*xsink) {
        assert(aborted);
        xsink->clear();
    }

    return response_hash.release();
}

void check_headers(const char* str, int len, bool &multipart, QoreHashNode& ans, const QoreEncoding *enc,
        ExceptionSink* xsink) {
    // see if the string starts with "multipart/"
    if (!multipart) {
        if (len > 10 && !strncasecmp(str, "multipart/", 10)) {
            ans.setKeyValue("_qore_multipart", new QoreStringNode(str + 10, len - 10, enc), xsink);
            multipart = true;
        }
    } else {
        if (len > 9 && !strncasecmp(str, "boundary=", 9))
            ans.setKeyValue("_qore_multipart_boundary", new QoreStringNode(str + 9, len - 9, enc), xsink);
        else if (len > 6 && !strncasecmp(str, "start=", 6))
            ans.setKeyValue("_qore_multipart_start", new QoreStringNode(str + 6, len - 6, enc), xsink);
    }
}

QoreHashNode* qore_httpclient_priv::send_internal(ExceptionSink* xsink, const char* mname, const char* meth,
        const char* mpath, const QoreHashNode* headers, const QoreStringNode* msg_body, const void* data,
        unsigned size, const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info,
        int timeout_ms, const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream* os,
        InputStream* is, size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback) {
    assert(!(data && send_callback));
    assert(!(data && is));
    assert(!(is && send_callback));
    assert(!info || info->is_unique());

    // issue #4841: do not override the connection path when sending
    con_info this_connection = connection;

    bool bodyp = false;
    meth = checkMethod(xsink, meth, bodyp);
    if (*xsink) {
        return nullptr;
    }

    // use the default timeout value if a zero value is given in the call
    if (!timeout_ms)
        timeout_ms = timeout;

    SafeLocker sl(msock->m);

    if (msock->checkNonBlock(xsink)) {
        return nullptr;
    }

    Queue* event_queue = msock->socket->getQueue();

    bool keep_alive = true;
    bool host_override = false;
    ReferenceHolder<QoreHashNode> nh(getRequestHeaders(xsink, headers,
        msg_body ? msg_body->getEncoding() : nullptr, (data || is || send_callback), (send_callback || is),
        keep_alive, host_override), xsink);

    // save original HTTP method in case we have to issue a CONNECT request to a proxy for an HTTPS connection
    const char* meth_orig = meth;

    bool use_proxy_connect = false;
    const char* proxy_path = nullptr;
    ReferenceHolder<QoreHashNode> proxy_headers(xsink);
    if (!proxy_connected && proxy_connection.has_url()) {
        proxy_headers = setProxyHeaders(xsink, this_connection, *nh, use_proxy_connect, proxy_path);
        if (*xsink) {
            return nullptr;
        }
        if (proxy_headers) {
            meth = "CONNECT";
            assert(use_proxy_connect);
            assert(proxy_path);
        } else {
            assert(!use_proxy_connect);
            assert(!proxy_path);
        }
    }

    int code;
    ReferenceHolder<QoreHashNode> ans(xsink);
    int redirect_count = 0;
    const char* location = nullptr;

    // flag for aborted chunked sends
    bool send_aborted = false;

    ReferenceHolder<QoreHashNode> callback_info(xsink);
    if (recv_callback && !info) {
        callback_info = new QoreHashNode(autoTypeInfo);
        info = *callback_info;
    }

    bool path_already_encoded = false;
    // only reconnect if we already have an open connection
    unsigned retries = msock->socket->isOpen() ? 0 : 1;
    while (true) {
        // set host field automatically if not overridden
        if (!host_override) {
            nh->setKeyValue("Host", getHostHeaderValueUnlocked(this_connection), xsink);
        }

        if (info) {
            info->setKeyValue("headers", nh->copy(), xsink);
            if (*xsink) {
                return nullptr;
            }
        }

        //printd(5, "qore_httpclient_priv::send_internal() meth: %s proxy_path: %s mpath: %s upc: %d\n", meth,
        //    proxy_path ? proxy_path : "n/a", mpath, use_proxy_connect);
        // send HTTP message and get response header
        if (use_proxy_connect) {
            ans = sendMessageAndGetResponse(this_connection, mname, meth, proxy_path,
                *(*proxy_headers), nullptr, nullptr, 0, nullptr, nullptr, 0, nullptr, info, true, timeout_ms, code,
                send_aborted, false, xsink);
        } else {
            ans = sendMessageAndGetResponse(this_connection, mname, meth, mpath, *(*nh),
                msg_body, data, size, send_callback, is, max_chunk_size, trailer_callback, info, false, timeout_ms,
                code, send_aborted, path_already_encoded, xsink);
        }

        if (!ans) {
            assert(*xsink);
            // issue# 4879: reconnect immediately and try again if the socket was closed the first time
            if (!msock->socket->isOpen() && !retries) {
                ++retries;
                xsink->clear();
                continue;
            }
            return nullptr;
        }

        if (info) {
            info->setKeyValue("response-headers", ans->refSelf(), xsink);
            if (*xsink) {
                return nullptr;
            }
        }

        if (!ans->is_unique()) {
            ans = ans->copy();
        }

        // issue #3116: pass a 304 Not Modified message back to the caller without processing
        if (!redirect_passthru && code >= 300 && code < 400 && code != 304) {
            // only disconnect if we have no proxy or we need to use proxy_connect
            if (!proxy_connection.has_url()) {
                //printd(5, "qore_httpclient_priv::send_internal() disconnecting; no proxy\n");
                disconnect_unlocked();
            } else if (proxy_connected) {
                proxy_connected = false;
            }

            host_override = false;
            const QoreStringNode* mess = ans->getKeyValue("status_message").get<QoreStringNode>();

            const QoreStringNode* loc = get_string_header_node(xsink, **ans, "location");
            if (*xsink) {
                return nullptr;
            }
            const char* location = loc && !loc->empty() ? loc->c_str() : 0;
            if (!location) {
                sl.unlock();
                const char* msg = mess ? mess->c_str() : "<no message>";
                xsink->raiseException("HTTP-CLIENT-REDIRECT-ERROR", "no redirect location given for status code %d: "
                    "message: '%s'", code, msg);
                return nullptr;
            }

            if (event_queue) {
                do_redirect_event(event_queue, msock->socket->priv, loc, mess);
            }

            // issue #4601: get and ignore any message body
            if (msock->socket->priv->isOpen() && strcmp(mname, "HEAD") && code != 204 && code != 304
                && getDiscardMessageBody(xsink, **ans, timeout_ms)) {
                assert(*xsink);
                disconnect_unlocked();
                return nullptr;
            }

            if (++redirect_count > max_redirects) {
                break;
            }

            if (redirectUrlUnlocked(location, this_connection, xsink)) {
                sl.unlock();
                const char* msg = mess ? mess->c_str() : "<no message>";
                xsink->appendLastDescription(": while setting URL for redirect location '%s' (code %d: "
                    "message: '%s')", location, code, msg);
                return nullptr;
            }
            if (!path_already_encoded) {
                path_already_encoded = true;
            }

            if (proxy_connection.has_url()) {
                proxy_headers = setProxyHeaders(xsink, this_connection, *nh, use_proxy_connect, proxy_path);
                if (*xsink) {
                    return nullptr;
                }
                if (proxy_headers) {
                    meth = "CONNECT";
                    assert(use_proxy_connect);
                    assert(proxy_path);
                } else {
                    assert(!use_proxy_connect);
                    assert(!proxy_path);
                }
            }

            // set redirect info in info hash if present
            if (info) {
                QoreString tmp;
                tmp.sprintf("redirect-%d", redirect_count);
                info->setKeyValue(tmp.c_str(), loc->refSelf(), xsink);
                if (*xsink)
                    return nullptr;

                tmp.clear();
                tmp.sprintf("redirect-message-%d", redirect_count);
                info->setKeyValue(tmp.c_str(), mess ? mess->refSelf() : 0, xsink);
            }

            // FIXME: reset send callback and send_aborted here

            // set mpath to NULL so that the new path will be taken
            mpath = nullptr;
            continue;
        } else if (use_proxy_connect) {
            meth = meth_orig;
            use_proxy_connect = false;
            proxy_path = nullptr;

            // set client target for SNI
            msock->socket->priv->client_target = this_connection.host;

            if (msock->socket->upgradeClientToSSL(xsink, msock->cert ? msock->cert->getData() : nullptr,
                msock->pk ? msock->pk->getData() : nullptr,
                msock->cert ? msock->cert->getVerifyCACertificate() : nullptr)) {
                disconnect_unlocked();
                return nullptr;
            }
            proxy_connected = true;

            // remove "Proxy-Authorization" header
            nh->removeKey("Proxy-Authorization", xsink);
            if (*xsink) {
                return nullptr;
            }

            // try again as if we are talking directly to the client
            continue;
        }

        break;
    }

    if (!redirect_passthru && code >= 300 && code < 400 && code != 304) {
        sl.unlock();
        const char* mess = get_string_header(xsink, **ans, "status_message");
        if (!mess) {
            mess = "<no message>";
        }
        if (!location) {
            location = "<no location>";
        }
        xsink->raiseException("HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED", "maximum redirections (%d) exceeded; "
            "redirect code %d to '%s' ignored (message: '%s')", max_redirects, code, location, mess);
        return nullptr;
    }

    // process content-type
    if (processContentType(xsink, **ans)) {
        disconnect_unlocked();
        return nullptr;
    }

    // send headers to recv_callback
    // cannot reference info here; must copy it
    if (recv_callback
        && msock->socket->priv->runHeaderCallback(xsink, "HTTPClient", mname, *recv_callback, &msock->m, *ans,
            info ? info->copy() : nullptr, send_aborted, obj)) {
        return nullptr;
    }

    QoreValue body;
    const char* content_encoding = nullptr;

    // do not read any message body for messages that cannot have one
    // rfc 2616 4.4 p1 (http://tools.ietf.org/html/rfc2616#section-4.4)
    /*
        1.Any response message which "MUST NOT" include a message-body (such
        as the 1xx, 204, and 304 responses and any response to a HEAD
        request) is always terminated by the first empty line after the
        header fields, regardless of the entity-header fields present in
        the message.
    */
    //printd(5, "qore_httpclient_priv::send_internal() this: %p bodyp: %d code: %d\n", this, bodyp, code);

    qore_uncompress_to_string_t dec = 0;

    const char* conn = get_string_header(xsink, **ans, "connection", true);
    if (*xsink) {
        disconnect_unlocked();
        return nullptr;
    }

    // code >= 300 && < 400 is already handled above
    if (bodyp && (code < 100 || code >= 200) && code != 204) {
        // see if we should do a binary or string read
        content_encoding = get_string_header(xsink, **ans, "content-encoding");
        if (*xsink) {
            disconnect_unlocked();
            return nullptr;
        }

        if (content_encoding && !os) {
            // check for misuse of this header by including a character encoding value
            if (!strncasecmp(content_encoding, "iso", 3) || !strncasecmp(content_encoding, "utf-", 4)) {
                msock->socket->setEncoding(QEM.findCreate(content_encoding));
                content_encoding = nullptr;
            } else if (!recv_callback) {
                // only decode message bodies automatically if there is no receive callback
                if (!strcasecmp(content_encoding, "deflate") || !strcasecmp(content_encoding, "x-deflate"))
                    dec = qore_inflate_to_string;
                else if (!strcasecmp(content_encoding, "gzip") || !strcasecmp(content_encoding, "x-gzip"))
                    dec = qore_gunzip_to_string;
                else if (!strcasecmp(content_encoding, "bzip2") || !strcasecmp(content_encoding, "x-bzip2"))
                    dec = qore_bunzip2_to_string;
                // issue #2953 ignore unknown content encodings or a crash will result
                else {
                    content_encoding = nullptr;
                }
            }
        }

        const char* te = get_string_header(xsink, **ans, "transfer-encoding");
        if (*xsink) {
            disconnect_unlocked();
            return nullptr;
        }

        // get response body, if any
        const char* cl = get_string_header(xsink, **ans, "content-length");
        if (*xsink) {
            disconnect_unlocked();
            return nullptr;
        }
        int len = cl ? atoi(cl) : 0;
        // do not try to get a body in any case if Content-Length: 0 is sent
        if (cl) {
            if (!len) {
                getbody = false;
            }
        } else {
            // issue #3691: ready the body if we have Connection: close and a content-type and a potential response
            if (!te && conn && !strcasecmp("close", conn) && strcmp(mname, "HEAD") && code != 204 && code != 304) {
                len = -1;
            }
        }

        if (cl && event_queue)
            do_content_length_event(event_queue, msock->socket->priv, len);

        if (te && !strcasecmp(te, "chunked")) { // check for chunked response body
            do_event(event_queue, msock->socket->priv, QORE_EVENT_HTTP_CHUNKED_START);
            ReferenceHolder<QoreHashNode> nah(xsink);
            if (os) {
                msock->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink, "HTTPClient",
                    QORE_SOURCE_HTTPCLIENT, recv_callback, &msock->m, obj, os);
            } else if (recv_callback) {
                if (content_encoding) {
                    msock->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink, "HTTPClient",
                        QORE_SOURCE_HTTPCLIENT, recv_callback, &msock->m, obj);
                } else {
                    msock->socket->priv->readHttpChunkedBody(timeout_ms, xsink, "HTTPClient", QORE_SOURCE_HTTPCLIENT,
                        recv_callback, &msock->m, obj);
                }
            } else {
                if (content_encoding) {
                    nah = msock->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink, "HTTPClient",
                        QORE_SOURCE_HTTPCLIENT);
                } else {
                    nah = msock->socket->priv->readHttpChunkedBody(timeout_ms, xsink, "HTTPClient",
                        QORE_SOURCE_HTTPCLIENT);
                }
            }
            do_event(event_queue, msock->socket->priv, QORE_EVENT_HTTP_CHUNKED_END);

            if (!nah && !recv_callback) {
                if (!msock->socket->isOpen()) {
                    disconnect_unlocked();
                }
                return nullptr;
            }

            if (info) {
                info->setKeyValue("chunked", true, xsink);
            }

            if (*xsink) {
                return nullptr;
            }

            if (!recv_callback && !os) {
                // merge all keys except the "body" key into ans
                ConstHashIterator hi(*nah);
                while (hi.next()) {
                if (!strcmp(hi.getKey(), "body")) {
                    assert(!body);
                    body = hi.getReferenced();
                    continue;
                }
                ans->setKeyValue(hi.getKey(), hi.getReferenced(), xsink);
                if (*xsink)
                    return nullptr;
                }
            }
        } else if (getbody || len) {
            if (os) {
                msock->socket->priv->recvToOutputStream(os, len, timeout_ms, xsink, &msock->m,
                    QORE_SOURCE_HTTPCLIENT);
            } else if (content_encoding) {
                qore_offset_t rc;
                SimpleRefHolder<BinaryNode> bobj(msock->socket->priv->recvBinary(xsink, len, timeout_ms, rc,
                    QORE_SOURCE_HTTPCLIENT));
                if (!(*xsink) && bobj) {
                    body = bobj.release();
                }
            } else {
                qore_offset_t rc;
                QoreStringNodeHolder bstr(msock->socket->priv->recv(xsink, len, timeout_ms, rc,
                    QORE_SOURCE_HTTPCLIENT));
                if (!(*xsink) && bstr) {
                    body = bstr.release();
                }
            }

            if (*xsink && !msock->socket->isOpen()) {
                disconnect_unlocked();
            }
            //printf("body: %p\n", body);
        }
    }

    if (*xsink) {
        disconnect_unlocked();
        return nullptr;
    }

    // check for connection: close header
    if (!keep_alive) {
        disconnect_unlocked();
    } else {
        if (conn && !strcasecmp(conn, "close")) {
            disconnect_unlocked();
        }
    }

    sl.unlock();

    // for content-encoding processing we can run unlocked

    // add body to result hash and process content encoding if necessary
    if (body) {
        if (content_encoding && !encoding_passthru) {
            if (!dec) {
                if (!recv_callback) {
                    xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "don't know how to handle content-encoding "
                        "'%s'", content_encoding);
                    ans = nullptr;
                }
            } else {
                BinaryNode* bobj = body.get<BinaryNode>();
                QoreStringNode* str = dec(bobj, msock->socket->getEncoding(), xsink);
                bobj->deref();
                body = str;
            }
        }

        if (body) {
            // send data to recv_callback (already unlocked)
            if (recv_callback) {
                ValueHolder bh(body, xsink);
                // call body callbback and then header callback with no argument
                if (msock->socket->priv->runDataCallback(xsink, "HTTPClient", mname, *recv_callback, nullptr,
                        body.getInternalNode(), false)
                    || msock->socket->priv->runHeaderCallback(xsink, "HTTPClient", mname, *recv_callback, nullptr,
                        nullptr, nullptr, send_aborted, obj))
                    return nullptr;
            } else {
                ans->setKeyValue("body", body, xsink);
            }
        }
    }

    // do not throw an exception if a receive callback is used
    if (!error_passthru && !recv_callback && !*xsink && (code < 100 || code >= 300)) {
        const char* mess = get_string_header(xsink, **ans, "status_message");
        if (!mess) {
            mess = "<no message>";
        }
        assert(!*xsink);

        xsink->raiseExceptionArg("HTTP-CLIENT-RECEIVE-ERROR", ans.release(), "HTTP status code %d received: message: "
            "%s", code, mess);
        return nullptr;
    }

    return *xsink || recv_callback || os ? nullptr : ans.release();
}

QoreHashNode* QoreHttpClientObject::send(const char* meth, const char* new_path, const QoreHashNode* headers,
        const void* data, unsigned size, bool getbody, QoreHashNode* info, ExceptionSink* xsink) {
    return http_priv->send_internal(xsink, "send", meth, new_path, headers, nullptr, data, size, nullptr, getbody,
        info, http_priv->timeout, nullptr);
}

QoreHashNode* QoreHttpClientObject::send(const char* meth, const char* new_path, const QoreHashNode* headers,
    const QoreStringNode& body, bool getbody, QoreHashNode* info, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return nullptr;
    }
    return http_priv->send_internal(xsink, "send", meth, new_path, headers, *tstr, tstr->c_str(), tstr->size(),
        nullptr, getbody, info, http_priv->timeout, nullptr);
}

QoreHashNode* QoreHttpClientObject::sendWithSendCallback(const char* meth, const char* mpath,
        const QoreHashNode* headers, const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info,
        int timeout_ms, ExceptionSink* xsink) {
    return http_priv->send_internal(xsink, "sendWithSendCallback", meth, mpath, headers, nullptr, nullptr, 0,
        send_callback, getbody, info, timeout_ms, nullptr);
}

void QoreHttpClientObject::sendWithRecvCallback(const char* meth, const char* mpath, const QoreHashNode* headers,
        const void* data, unsigned size, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    http_priv->send_internal(xsink, "sendWithRecvCallback", meth, mpath, headers, nullptr, data, size, nullptr,
        getbody, info, timeout_ms, recv_callback, obj);
}

void QoreHttpClientObject::sendWithRecvCallback(const char* meth, const char* mpath, const QoreHashNode* headers,
        const QoreStringNode& body, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return;
    }
    http_priv->send_internal(xsink, "sendWithRecvCallback", meth, mpath, headers, *tstr, tstr->c_str(), tstr->size(),
        nullptr, getbody, info, timeout_ms, recv_callback, obj);
}

void QoreHttpClientObject::sendWithOutputStream(const char* meth, const char* mpath, const QoreHashNode* headers,
        const void* data, unsigned size, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream *os, ExceptionSink* xsink) {
    http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers, nullptr, data, size, nullptr,
        getbody, info, timeout_ms, recv_callback, obj, os);
}

void QoreHttpClientObject::sendWithOutputStream(const char* meth, const char* mpath, const QoreHashNode* headers,
        const QoreStringNode& body, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream *os, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return;
    }
    http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers, *tstr, tstr->c_str(), tstr->size(),
        nullptr, getbody, info, timeout_ms, recv_callback, obj, os);
}

void QoreHttpClientObject::sendChunked(const char* meth, const char* mpath, const QoreHashNode* headers, bool getbody,
        QoreHashNode* info, int timeout_ms, const ResolvedCallReferenceNode* recv_callback, QoreObject* obj,
        OutputStream *os, InputStream* is, size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback, ExceptionSink* xsink) {
    assert(max_chunk_size);
    http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers, nullptr, nullptr, 0, nullptr,
        getbody, info, timeout_ms, recv_callback, obj, os, is, max_chunk_size, trailer_callback);
}

void QoreHttpClientObject::sendWithCallbacks(const char* meth, const char* mpath, const QoreHashNode* headers,
        const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    http_priv->send_internal(xsink, "sendWithCallbacks", meth, mpath, headers, nullptr, nullptr, 0, send_callback,
        getbody, info, timeout_ms, recv_callback, obj);
}

// returns *string
// @since Qore 0.8.12: do not send getbody = true which only works with completely broken HTTP servers and small messages and causes deadlocks on correct HTTP servers
AbstractQoreNode* QoreHttpClientObject::get(const char* new_path, const QoreHashNode* headers, QoreHashNode* info,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> ans(http_priv->send_internal(xsink, "get", "GET", new_path, headers, nullptr,
        nullptr, 0, nullptr, false, info, http_priv->timeout), xsink);
    if (!ans) {
        return nullptr;
    }
    return ans->takeKeyValue("body").getInternalNode();
}

QoreHashNode* QoreHttpClientObject::head(const char* new_path, const QoreHashNode* headers, QoreHashNode* info,
        ExceptionSink* xsink) {
   return http_priv->send_internal(xsink, "head", "HEAD", new_path, headers, nullptr, nullptr, 0, nullptr, false,
    info, http_priv->timeout);
}

// returns *string
// @since Qore 0.8.12: do not send getbody = true which only works with completely broken HTTP servers and small messages and causes deadlocks on correct HTTP servers
AbstractQoreNode* QoreHttpClientObject::post(const char* new_path, const QoreHashNode* headers, const void* data,
        unsigned size, QoreHashNode* info, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> ans(http_priv->send_internal(xsink, "post", "POST", new_path, headers, nullptr,
        data, size, nullptr, false, info, http_priv->timeout), xsink);
    if (!ans) {
        return nullptr;
    }
    return ans->takeKeyValue("body").getInternalNode();
}

// returns *string
// @since Qore 0.9.4: do not send getbody = true which only works with completely broken HTTP servers and small messages and causes deadlocks on correct HTTP servers
AbstractQoreNode* QoreHttpClientObject::post(const char* new_path, const QoreHashNode* headers,
        const QoreStringNode& body, QoreHashNode* info, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return nullptr;
    }
    ReferenceHolder<QoreHashNode> ans(http_priv->send_internal(xsink, "post", "POST", new_path, headers, *tstr,
        tstr->c_str(), tstr->size(), nullptr, false, info, http_priv->timeout), xsink);
    if (!ans) {
        return nullptr;
    }
    return ans->takeKeyValue("body").getInternalNode();
}

void QoreHttpClientObject::addProtocol(const char* prot, int new_port, bool new_ssl) {
    AutoLocker al(priv->m);
    http_priv->prot_map[prot] = make_protocol(new_port, new_ssl);
}

void QoreHttpClientObject::setMaxRedirects(int max) {
    AutoLocker al(priv->m);
    http_priv->max_redirects = max;
}

int QoreHttpClientObject::getMaxRedirects() const {
    return http_priv->max_redirects;
}

void QoreHttpClientObject::setDefaultHeaderValue(const char* header, const char* val) {
    AutoLocker al(priv->m);
    http_priv->default_headers[header] = val;
}

void QoreHttpClientObject::addDefaultHeaders(const QoreHashNode* hdr) {
    AutoLocker al(priv->m);
    ConstHashIterator i(hdr);
    while (i.next()) {
        QoreStringValueHelper str(i.get());
        http_priv->default_headers[i.getKey()] = str->c_str();
    }
}

QoreHashNode* QoreHttpClientObject::getDefaultHeaders() const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(stringTypeInfo), nullptr);
    qore_hash_private* h = qore_hash_private::get(**rv);

    AutoLocker al(priv->m);
    for (auto i : http_priv->default_headers) {
        h->setKeyValueIntern(i.first.c_str(), new QoreStringNode(i.second));
    }

    return rv.release();
}

void QoreHttpClientObject::setEventQueue(Queue *cbq, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->setEventQueue(xsink, cbq, QoreValue(), false);
}

void QoreHttpClientObject::setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    AutoLocker al(priv->m);
    priv->socket->setEventQueue(xsink, q, arg, with_data);
}

void QoreHttpClientObject::cleanup(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->close();
    priv->invalidate();
    priv->socket->cleanup(xsink);
}

void QoreHttpClientObject::lock() {
    priv->m.lock();
}

void QoreHttpClientObject::unlock() {
    priv->m.unlock();
}

int QoreHttpClientObject::setNoDelay(bool nd) {
    return http_priv->setNoDelay(nd);
}

bool QoreHttpClientObject::getNoDelay() const {
    return http_priv->getNoDelay();
}

bool QoreHttpClientObject::isConnected() const {
    return http_priv->msock->socket->isOpen();
}

void QoreHttpClientObject::setUserPassword(const char* user, const char* pass) {
    http_priv->setUserPassword(user, pass);
}

void QoreHttpClientObject::clearUserPassword() {
    http_priv->clearUserPassword();
}

void QoreHttpClientObject::setProxyUserPassword(const char* user, const char* pass) {
    http_priv->setProxyUserPassword(user, pass);
}

void QoreHttpClientObject::clearProxyUserPassword() {
    http_priv->clearProxyUserPassword();
}

void QoreHttpClientObject::setPersistent(ExceptionSink* xsink) {
    return http_priv->setPersistent(xsink);
}

void QoreHttpClientObject::clearPersistent() {
    return http_priv->clearPersistent();
}

bool QoreHttpClientObject::isPersistent() const {
    return http_priv->persistent_count ? true : false;
}

unsigned QoreHttpClientObject::getPersistentCount() const {
    return http_priv->persistent_count;
}

void QoreHttpClientObject::clearWarningQueue(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->clearWarningQueue(xsink);
}

void QoreHttpClientObject::setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq,
        QoreValue arg, int64 min_ms) {
    AutoLocker al(priv->m);
    priv->socket->setWarningQueue(xsink, warning_ms, warning_bs, wq, arg, min_ms);
}

QoreHashNode* QoreHttpClientObject::getUsageInfo() const {
    AutoLocker al(priv->m);
    return priv->socket->getUsageInfo();
}

void QoreHttpClientObject::clearStats() {
    AutoLocker al(priv->m);
    priv->socket->clearStats();
}

bool QoreHttpClientObject::setEncodingPassthru(bool set) {
    return http_priv->setEncodingPassthru(set);
}

bool QoreHttpClientObject::getEncodingPassthru() const {
    return http_priv->getEncodingPassthru();
}

bool QoreHttpClientObject::setErrorPassthru(bool set) {
    return http_priv->setErrorPassthru(set);
}

bool QoreHttpClientObject::getErrorPassthru() const {
    return http_priv->getErrorPassthru();
}

bool QoreHttpClientObject::setRedirectPassthru(bool set) {
    return http_priv->setRedirectPassthru(set);
}

bool QoreHttpClientObject::getRedirectPassthru() const {
    return http_priv->getRedirectPassthru();
}

QoreStringNode* QoreHttpClientObject::getHostHeaderValue() const {
    return http_priv->getHostHeaderValue();
}

void QoreHttpClientObject::setAssumedEncoding(const char* enc) {
    AutoLocker al(priv->m);
    qore_socket_private::get(*priv->socket)->setAssumedEncoding(enc);
}

QoreStringNode* QoreHttpClientObject::getAssumedEncoding() const {
    AutoLocker al(priv->m);
    return new QoreStringNode(qore_socket_private::get(*priv->socket)->getAssumedEncoding());
}

bool QoreHttpClientObject::setPreEncodedUrls(bool set) {
    AutoLocker al(priv->m);
    bool rv = http_priv->pre_encoded_urls;
    if (rv != set) {
        http_priv->pre_encoded_urls = set;
    }
    return rv;
}

bool QoreHttpClientObject::getPreEncodedUrls() const {
    AutoLocker al(priv->m);
    return http_priv->pre_encoded_urls;
}

QoreHashNode* QoreHttpClientObject::getConfig() const {
    AutoLocker al(priv->m);
    return http_priv->getConfig(*priv);
}