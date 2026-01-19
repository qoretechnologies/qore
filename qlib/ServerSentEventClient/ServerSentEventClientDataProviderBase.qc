# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventClientDataProviderBase.qc SSE connection client

/*  ServerSentEventClientDataProviderBase.qc Copyright 2025 Qore Technologies, s.r.o.

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
*/

#! the ServerSentEventClient namespace contains all the objects in the ServerSentEventClient module
public namespace ServerSentEventClient {
#! Event-based data provider for ServerSentEvent client-side events
/**
*/
public class ServerSentEventClientDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "supports_children": True,
            "supports_observable": True,
            "supports_connections": True,
            "supports_auto_reconnect": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "required_constructor_options": RequiredConstructorOptions,
            "desc": "Allows receiving ServerSentEvent messages from an HTTP server",
            "connection_event": EVENT_SSE_CONNECTED,
            "disconnection_event": EVENT_SSE_CLOSED,
        };

        #! Constructor options
        const ConstructorOptions = {
            "connect_timeout": <DataProviderOptionInfo>{
                "display_name": "Connect Timeout",
                "short_desc": "Connect timeout in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Connection timeout to use in milliseconds",
            },
            "data": <DataProviderOptionInfo>{
                "display_name": "Data Serialization",
                "short_desc": "Data serialization for string messages",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "data serialization to use:\n"
%ifndef NoJson
                    " - `json`: JSON serialization (default)\n"
%endif
                    " - `none`: no serialization; raw messages\n",
                "default_value":
%ifndef NoJson
                    "json"
%else
                    "none"
%endif
                    ,
                "allowed_values": (
%ifndef NoJson
                    <AllowedValueInfo>{
                        "value": "json",
                        "desc": "Use JSON serialization",
                    },
%endif
                    <AllowedValueInfo>{
                        "value": "none",
                        "desc": "Use no data serialization",
                    },
                ),
            },
            "headers": <DataProviderOptionInfo>{
                "display_name": "Request Headers",
                "short_desc": "HTTP headers to send in the request",
                "type": AbstractDataProviderTypeMap."hash",
                "desc": "HTTP headers to add in the initial connection request",
            },
            "http_version": <DataProviderOptionInfo>{
                "display_name": "HTTP Version",
                "short_desc": "HTTP version to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The HTTP version to use: `auto` (HTTP/2 if available, default), `1.0`, `1.1`, "
                    "or `2.0`. HTTP/2 requires TLS and is negotiated via ALPN",
                "default_value": "auto",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "auto",
                        "desc": "Use HTTP/2 if available via ALPN, fall back to HTTP/1.1 (default)",
                    },
                    <AllowedValueInfo>{
                        "value": "1.0",
                        "desc": "HTTP/1.0 only",
                    },
                    <AllowedValueInfo>{
                        "value": "1.1",
                        "desc": "HTTP/1.1 only",
                    },
                    <AllowedValueInfo>{
                        "value": "2.0",
                        "desc": "Require HTTP/2, fail if the server doesn't support it (TLS required)",
                    },
                ),
            },
            "last_event_id": <DataProviderOptionInfo>{
                "display_name": "Last Event ID",
                "short_desc": "Request the server restart sending events from this ID",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Request the server restart sending events from this ID. This will be send in the "
                    "`Last-Event-ID` header in the connection request to the SSE server",
            },
            "logger": <DataProviderOptionInfo>{
                "display_name": "Logger",
                "short_desc": "The logger to use",
                "type": AbstractDataProviderType::get(new Type("Logger::LoggerInterface")),
                "desc": "Any logger to use for information about data provider operations",
            },
            "max_redirects": <DataProviderOptionInfo>{
                "display_name": "Max HTTP Redirects",
                "short_desc": "Maximum HTTP redirects to support",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Maximum HTTP redirects to support",
            },
            "password": <DataProviderOptionInfo>{
                "display_name": "Password",
                "short_desc": "The password for HTTP basic authentication",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The password for HTTP basic authentication; only used if no username or password is set "
                    "in the URL and if the `username` option is also used",
                "sensitive": True,
            },
            "proxy": <DataProviderOptionInfo>{
                "display_name": "HTTP Proxy",
                "short_desc": "The HTTP proxy to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The proxy URL to use",
            },
            "reconnect": <DataProviderOptionInfo>{
                "display_name": "Reconnect?",
                "short_desc": "Automatically reconnect when the connection is closed or lost",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Automatically reconnect when the connection is closed or lost",
                "default_value": False,
            },
            "timeout": <DataProviderOptionInfo>{
                "display_name": "I/O Timeout",
                "short_desc": "Maximum time a single I/O operation can take in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Maximum time a single I/O operation can take in milliseconds",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Authorization Token",
                "short_desc": "Set an authorization token to be included in requests",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Any bearer token to use for the connection; will be passed as "
                    "`Authorization: Bearer ...`  (assuming that `token_type` is `Bearer`) in request headers; this "
                    "option cannot be used with username and password options or authentication information in the "
                    "URL; if this option is set then OAuth2 options are ignored for login",
                "sensitive": True,
            },
            "token_type": <DataProviderOptionInfo>{
                "display_name": "Authorization Token Type",
                "short_desc": "The type of authorization token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The type of `token` to use for the `Authorization` header; ignored if no `token` "
                    "option is set",
                "default_value": "Bearer",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to connect to",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A URL for a ServerSentEvent connection; either this or the `sseclient` option must be provided",
            },
            "username": <DataProviderOptionInfo>{
                "display_name": "Username",
                "short_desc": "Sets the username for HTTP basic authentication",
                "desc": "The username for HTTP basic authentication; only used if no username or password is set in "
                    "the URL and if the `password` option is also used",
                "type": AbstractDataProviderTypeMap."string",
            },
            "sseclient": <DataProviderOptionInfo>{
                "display_name": "ServerSentEvent Client",
                "short_desc": "The ServerSentEventClient object",
                "type": AbstractDataProviderType::get(new Type("ServerSentEventClient")),
                "desc": "A `ServerSentEventClient` for the connection; either this or the `url` option must "
                    "be provided; if this option is provided, then all other options are ignored",
            },
        };

        #! Required constructor option combinations
        const RequiredConstructorOptions = "url|sseclient";

        #! Registered API classes
        const ApiClasses = {
            "wait-for-message": Class::forName("ServerSentEventClientWaitForMessageDataProvider"),
        };
    }

    private {
        #! The SSE client itself
        ServerSentEventClient client;

        #! Mutex for wait for message child data provider handling
        Mutex m();

        #! Wait for message API data provider
        ServerSentEventClientWaitForMessageDataProvider child;

        #! Queued observer
        QueuedObserver queued_observer;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options, RequiredConstructorOptions);
        if (copts.sseclient) {
            client = copts.sseclient;
            if (copts.logger) {
                setLogger(copts.logger);
            }
        } else {
            client = new ServerSentEventClient(copts);
            if (copts.logger) {
                LoggerWrapper::setLogger(logger);
            }
        }
    }

    destructor() {
        delete client;
    }

    string getName() {
        return "sseclient";
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        client.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Regstiger a child data provider
    register(ServerSentEventClientWaitForMessageDataProvider child) {
        AutoLock al(m);
        if (self.child) {
            throw "SSE-ERROR", "A child data provider to wait for a message on this connection already exists; "
                "only one child data provider to wait on messages can exist at a time for a single connection";
        }
        self.child = child;
        queued_observer = new QueuedObserver();
        client.registerObserver(queued_observer);
    }

    #! Dergister a child data provider
    deregister() {
        AutoLock al(m);
        remove child;
        delete queued_observer;
    }

    #! Returs the child message queue
    Queue getQueue() {
        return queued_observer.getQueue();
    }

    #! The method registers the given observer
    registerObserver(object observer, ...) {
        client.registerObserverArgs(observer, argv);
    }

    #! The method deregisters the given observer
    /** @param observer the observer to be deregistered

        @return True if found otherwise False
    */
    bool deregisterObserver(object observer) {
        return client.deregisterObserver(observer);
    }

    #! Called when all observers have been added to the object
    /** This method is meant to trigger event generation
    */
    observersReady() {
        client.observersReady();
    }

    #! Returns True if the object is currently connected
    bool isConnected() {
        return client.isOpen();
    }

    #! Initiate a reconnect action on the client
    reconnect() {
        client.reconnect();
    }

    #! Returns connection info
    /** @return the connection hash; see \c ServerSentEventClient::getConnectionInfo() for information about the \c info
        key

        @since DataProvider 3.0
    */
    private *hash<DataProviderConnectionInfo> getConnectionInfoImpl() {
        return <DataProviderConnectionInfo>{
            "connected": client.isOpen(),
            "reconnecting": client.isReconnecting(),
            "auto_reconnect": client.getReconnect(),
            "url": client.getUrl(),
            "info": client.getConnectionInfo(),
        };
    }

    #! Returns a list of child data provider names
    /** @return a list of child data provider names
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ApiClasses;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @throw CHILD-PROVIDER-ERROR error acquiring child provider

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ApiClasses{name};
        if (cls) {
            return cls.newObject(self);
        }
    }

    #! Returns a hash of all supported event types
    /** @return a hash of all supported event types

        @note only called if the provider supports the observer pattern / event API
    */
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_SSE_CONNECTED: <DataProviderMessageInfo>{
                "desc": "The event signaling that the channel has been connected",
                "type": new HashDataType(),
            },
            EVENT_SSE_DATA: <DataProviderMessageInfo>{
                "desc": "A data event; data will be deserialized according to the serialization option",
                "type": ServerSentEventClientEventDataType,
            },
            EVENT_SSE_CLOSED: <DataProviderMessageInfo>{
                "desc": "The event signaling that the channel was closed",
                "type": new HashDataType(),
            },
        };
    }

    #! Returns options from a template
    static hash<auto> getOptionsFromTemplate(hash<auto> template_options, *hash<auto> options) {
        if (exists options && template_options.url) {
            # overwrite the URL with a sample value to ensure that the data provider can be created for configuration
            # information only
            template_options.url = "sse://localhost";
        }
        return template_options + options;
    }
}
}
