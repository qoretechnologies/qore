# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventConnection.qc SSE connection client

/*  ServerSentEventConnection.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! class for SSE connections
/** Returns an object of class @ref ServerSentEventClient for receiving sse events from a sse server; supports
    all @ref ConnectionProvider::HttpConnection "HttpConnection" options.

    Also supports the following runtime options in getImpl():
    - \c "logger": a \c Logger::LoggerInterface "LoggerInterface" object for internal logging
    - \c "start_thread": a closure to start a new thread (signature:
      @verbatim int sub start_thread(code callable) {}@endverbatim)
    - \c "yield": a closure to yield current thread execution
*/
public class ServerSentEventConnection inherits ConnectionProvider::AbstractConnectionWithInfo {
    public {
        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "ServerSentEvent Connection",
            "short_desc": "A connection to a ServerSentEvent server service",
            "desc": "A connection to a ServerSentEvent server service",
            "cls": Class::forName("ServerSentEventConnection"),
            "schemes": {
                "sses": True,
                "sse": True,
            },
            "options": HttpConnection::ConnectionScheme.options + {
                "data": <ConnectionOptionInfo>{
                    "display_name": "Data Serialization",
                    "short_desc": "Sets the data serialization option",
                    "type": "string",
                    "desc": "Sets the data serialization option",
                    "allowed_values": (
%ifndef NoJson
                        <AllowedValueInfo>{
                            "value": "json",
                            "desc": "use JSON serialization",
                        },
%endif
                        <AllowedValueInfo>{
                            "value": "none",
                            "desc": "no serialization is used",
                        },
                    ),
                    "default_value": "none",
                },
                "last_event_id": <ConnectionOptionInfo>{
                    "display_name": "Last Event ID",
                    "short_desc": "Request the server restart sending events from this ID",
                    "type": "softstring",
                    "desc": "Request the server restart sending events from this ID. This will be send in the "
                        "`Last-Event-ID` header in the connection request to the SSE server",
                },
                "reconnect": <ConnectionOptionInfo>{
                    "display_name": "Reconnect?",
                    "short_desc": "Try to reconnect when the connection is closed or lost",
                    "type": "bool",
                    "desc": "Try to reconnect when the connection is closed or lost",
                    "default_value": False,
                },
                "token": <ConnectionOptionInfo>{
                    "display_name": "Authorization Token",
                    "short_desc": "Set an authorization token",
                    "type": "string",
                    "desc": "Any token to use for the initial HTTP request to authenticate with the server",
                    "sensitive": True,
                },
                "token_type": <ConnectionOptionInfo>{
                    "display_name": "Authorization Token Type",
                    "short_desc": "The type of authorization token",
                    "type": "string",
                    "desc": "The type of `token` to use for the `Authorization` header; ignored if no `token` "
                        "option is set",
                    "default_value": "Bearer",
                },
                "version": <ConnectionOptionInfo>{
                    "display_name": "ServerSentEvent Version",
                    "short_desc": "Sets the ServerSentEvent protocol version",
                    "type": "int",
                    "desc": "The ServerSentEvent protocol version to use in the client",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": 8,
                            "desc": "Version 8",
                        },
                        <AllowedValueInfo>{
                            "value": 13,
                            "desc": "Version 13",
                        },
                    ),
                },
            },
        };
    }

    #! creates the ServerSentEventConnection connection object
    /** @param name the name of the connection
        @param description connection description
        @param url connection URL (potentially with username and password info for HTTP basic authentication)
        @param attributes various attributes. See below
        @param options connection options

        See \c AbstractConnection::constructor() for \c attributes and \c options reference.

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option
    */
    constructor(string name, string description, string url, hash<auto> attributes = {}, hash<auto> options = {})
            : AbstractConnectionWithInfo(name, description, url, attributes, options) {
    }

    #! creates the ServerSentEventConnection connection object
    /** @param config with the following keys:
        - name (required string): the connection name
        - display_name (optional string): the display name
        - short_desc (optional string): a short description in plain text
        - desc (optional string): a long description with markdown formatting
        - url (required string): the connection URL
        - opts (optional hash): connection options
        - logger (optional LoggerInterface object): logger for the connection
        @param attr optional connection attributes
        - monitor (optional bool): should the connection be monitored? Default: True
        - enabled (optional bool): is the connection enabled? Default: True
        - locked (optional bool): is the connection locked? Default: False
        - debug_data (optional bool): debug data? Default: False
        - tags (optional hash): tags for the connection; no default value

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option or attribute
    */
    constructor(hash<auto> config, *hash<auto> attr) : AbstractConnectionWithInfo(config, attr) {
    }

    #! returns \c "sse"
    string getType() {
        return "sse";
    }

    #! returns runtime options
    /** @return a hash with the following keys reflecting support for the corresponding runtime options in getImpl():
        - \c "logger": a \c Logger::LoggerInterface "LoggerInterface" object for internal logging
        - \c "start_thread": a closure to start a new thread (signature:
          @verbatim int sub start_thread(code callable) {}@endverbatim)
        - \c "yield": a closure to yield current thread execution
    */
    *hash<auto> getRuntimeOptions() {
        return {
            "logger": True,
            "start_thread": True,
            "yield": True,
        };
    }

    private hash<PingInfo> pingIntern(hash<PingInfo> rv) {
        date start = now_us();
        object o = get();
        on_exit o.disconnect();
        return rv += {
            "ok": True,
            "time": now_us() - start,
            "info": "OK",
        };
    }

    #! returns a @ref ServerSentEventClient object
    /** @param connect if @ref True "True", then @ref ServerSentEventClient::connect() is called
        @param rtopts supports the following runtime options in getImpl():
        - \c "logger": a \c Logger::LoggerInterface "LoggerInterface" object for internal logging
        - \c "reconnect": try to reconnect when the connection is closed or lost (default @ref False)
        - \c "start_thread": a closure to start a new thread (signature:
          @verbatim int sub start_thread(code callable) {}@endverbatim)
        - \c "yield": a closure to yield current thread execution

        @return a @ref ServerSentEventClient object
    */
    private ServerSentEventClient getImpl(bool connect = True, *hash<auto> rtopts) {
        # to avoid type issues with rtopts
        if (rtopts) {
            rtopts = {} + rtopts;
        }
        hash<auto> opts = {"url": url} + rtopts{"logger", "reconnect", "yield", "start_thread"} + self.opts;
        if (!opts.logger && logger) {
            opts.logger = logger;
        }

        ServerSentEventClient sse(opts);
        if (connect) {
            sse.connect();
        }
        return sse;
    }

    #! returns an ServerSentEventClientDataProvider object for this connection
    /** @param constructor_options any additional constructor options for the data provider

        @return an ServerSentEventClientDataProvider object for this connection

        @see hasDataProvider()
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        return new ServerSentEventClientDataProvider({
            "url": url,
        } + opts{"token", "token_type", "username", "password"});
    }

    #! returns @ref True, as the connection returns a data provider with the \c getDataProvider() method
    /** @return @ref True, as the connection returns a data provider with the \c getDataProvider() method

        @see \c getDataProvider()
    */
    bool hasDataProvider() {
        return True;
    }

    #! Sets child data provider capabilities
    private setChildCapabilities() {
        children_can_support_observers = True;
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}
}
