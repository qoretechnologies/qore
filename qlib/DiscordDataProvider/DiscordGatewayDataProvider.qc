# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DiscordGatewayDataProvider module definition

/** DiscordGatewayDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without wsriction, including without limitation
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

#! Contains all public definitions in the DiscordDataProvider module
public namespace DiscordDataProvider {
#! Discord MESSAGE_CREATE event constant
public const EVENT_MESSAGE_CREATE = "MESSAGE_CREATE";

#! The Discord gateway data provider class
public class DiscordGatewayDataProvider inherits DataProvider::AbstractDataProvider, DataProvider::DelayedObservable,
        DataProvider::Observer {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DiscordGatewayDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
            "supports_connections": True,
            "supports_auto_reconnect": True,
            "connection_event": EVENT_WS_CONNECTED,
            "disconnection_event": EVENT_WS_CLOSED,
        };

        #! Constructor options
        const ConstructorOptions = {
            "intents": <DataProviderOptionInfo>{
                "display_name": "Intents",
                "short_desc": "The intent bitfield for gateway events",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The intent bitfield for gateway events",
                "default_value": DI_GUILD_MESSAGES | DI_DIRECT_MESSAGES | DI_MESSAGE_CONTENT,
                "required": True,
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Token",
                "short_desc": "The bot token for the gateway",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The bot token for the gateway",
                "required": True,
            },
            "wsclient": <DataProviderOptionInfo>{
                "display_name": "WebSocket Client",
                "short_desc": "The DiscordWebSocketClient object",
                "type": AbstractDataProviderType::get(new Type("DiscordWebSocketClient")),
                "desc": "The DiscordWebSocketClient object",
            },
            "wsclient_options": <DataProviderOptionInfo>{
                "display_name": "WebSocket Client Options",
                "short_desc": "Options to the DiscordWebSocketClient constructor",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Options to the DiscordWebSocketClient constructor; only used if a DiscordWebSocketClient "
                    "object is created",
            },
        };
    }

    private {
        #! The connection object used to create the REST client
        *DiscordWebSocketClient::DiscordWebSocketConnection conn;

        #! The WebSocket client object
        *DiscordWebSocketClient::DiscordWebSocketClient ws;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        ws = getWebSocketConnection(copts, logger);
        ws.registerObserver(self);
    }

    #! Creates the object from a DiscordWebSocketClient
    constructor(DiscordWebSocketClient::DiscordWebSocketClient ws) {
        self.ws = ws;
    }

    #! Creates the object from a DiscordWebSocketConnection
    constructor(DiscordWebSocketClient::DiscordWebSocketConnection conn) {
        self.conn = conn;
        self.ws = conn.get();
    }

    #! Deletes the object
    destructor() {
        delete ws;
    }

    #! Disconnects from the gateway and stops all background threads
    disconnect() {
        ws.disconnect();
    }

    #! Reconnects with the gateway immediately
    bool reconnect() {
        return ws.reconnect();
    }

    #! This method is called when an event is raised
    /**
        @param event_id unique ID identifying an event type or class
        @param msg the data generated by the Observable event source
    */
    update(string event_id, hash<auto> msg) {
        debug("DiscordGatewayDataProvider::update() event_id: %y msg: %y", event_id, msg);
        if (event_id == EVENT_WS_DATA) {
            switch (msg."data".t) {
                case EVENT_MESSAGE_CREATE:
                    notifyObservers(EVENT_MESSAGE_CREATE, msg."data");
                    break;
            }
        }
    }

    #! Returns information about the connection
    hash<auto> getWebSocketInfo() {
        return ws ? ws.getInfo() : {};
    }

    #! Called when all observers have been added to the object
    /** This method is meant to trigger event generation
    */
    observersReady() {
        ws.observersReady();
    }

    #! Called to stop generating events
    /** Disconnects from the server
    */
    stopEvents() {
        ws.stopEvents();
    }

    #! Returns the data provider name
    string getName() {
        return "discordws";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Discord WebSocket API data provider for `%s`", ws.getSafeUrl());
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        ws.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Returns a DiscordWebSocketClient connection from the options
    static DiscordWebSocketClient::DiscordWebSocketClient getWebSocketConnection(*hash<auto> options,
            *Logger::LoggerInterface logger) {
        DiscordWebSocketClient ws;
        if (options.wsclient) {
            ws = options.wsclient;
        } else {
            if (!options.token) {
                throw "CONSTRUCTOR-ERROR", "no 'wsclient' or 'token' options passed; "
                    "cannot create WebSocket connection to Discord gateway";
            }

            hash<auto> opts;
            opts += options.wsclient_options + options{
                "token",
                "intents",
            } + {
                "logger": logger,
                "reconnect": True,
            };
            ws = new DiscordWebSocketClient(opts);
        }
        return ws;
    }

    #! Returns connection info
    /** @return the connection hash; see \c WebSocketClient::getConnectionInfo() for information about the \c info
        key

        @since DataProvider 3.0
    */
    private *hash<DataProviderConnectionInfo> getConnectionInfoImpl() {
        return <DataProviderConnectionInfo>{
            "connected": ws.isOpen(),
            "reconnecting": ws.isReconnecting(),
            "auto_reconnect": ws.getReconnect(),
            "url": ws.getUrl(),
            "info": ws.getConnectionInfo(),
        };
    }

    #! Returns a hash of all supported event types
    /** @return a hash of all supported event types

        @note only called if the provider supports the observer pattern / event API
    */
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_MESSAGE_CREATE: <DataProviderMessageInfo>{
                "desc": "A message has been posted to a Discord channel",
                "type": new DiscordMessageCreateEventDataType(),
            },
        };
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
