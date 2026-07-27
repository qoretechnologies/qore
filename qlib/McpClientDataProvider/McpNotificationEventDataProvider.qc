# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpNotificationEventDataProvider class definition

/*  McpNotificationEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the McpClientDataProvider module
public namespace McpClientDataProvider {
#! MCP notification event IDs
public const EVENT_MCP_CONNECTED = "mcp-connected";
public const EVENT_MCP_DISCONNECTED = "mcp-disconnected";
public const EVENT_MCP_TOOLS_CHANGED = "mcp-tools-changed";
public const EVENT_MCP_RESOURCES_CHANGED = "mcp-resources-changed";
public const EVENT_MCP_PROMPTS_CHANGED = "mcp-prompts-changed";
public const EVENT_MCP_LOG_MESSAGE = "mcp-log-message";

#! MCP notification event data type
public class McpNotificationEventDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "event": {
                "type": StringType,
                "display_name": "Event",
                "short_desc": "MCP notification event",
                "desc": "The MCP notification event identifier",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Connected",
                        "short_desc": "SSE connected",
                        "desc": "The SSE connection to the MCP server has been established",
                        "value": EVENT_MCP_CONNECTED,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Disconnected",
                        "short_desc": "SSE disconnected",
                        "desc": "The SSE connection to the MCP server has been closed",
                        "value": EVENT_MCP_DISCONNECTED,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Tools Changed",
                        "short_desc": "Tool list changed",
                        "desc": "The MCP server tool list has changed",
                        "value": EVENT_MCP_TOOLS_CHANGED,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Resources Changed",
                        "short_desc": "Resource list changed",
                        "desc": "The MCP server resource list has changed",
                        "value": EVENT_MCP_RESOURCES_CHANGED,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Prompts Changed",
                        "short_desc": "Prompt list changed",
                        "desc": "The MCP server prompt list has changed",
                        "value": EVENT_MCP_PROMPTS_CHANGED,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Log Message",
                        "short_desc": "Server log message",
                        "desc": "A log message was received from the MCP server",
                        "value": EVENT_MCP_LOG_MESSAGE,
                    },
                ),
                "example_value": EVENT_MCP_LOG_MESSAGE,
            },
            "level": {
                "type": StringOrNothingType,
                "display_name": "Log Level",
                "short_desc": "Log message level",
                "desc": "The log level for log message notifications",
                "example_value": "info",
            },
            "logger": {
                "type": StringOrNothingType,
                "display_name": "Logger",
                "short_desc": "Logger name",
                "desc": "Optional logger name for log message notifications",
            },
            "data": {
                "type": AutoType,
                "display_name": "Data",
                "short_desc": "Notification data",
                "desc": "Notification payload data",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! MCP notification event data type constant
public const McpNotificationEventDataType = new McpNotificationEventDataType();

#! MCP notification event data provider
public class McpNotificationEventDataProvider inherits McpClientDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "McpNotificationEventDataProvider",
            "supports_observable": True,
            "supports_connections": True,
            "connection_event": EVENT_MCP_CONNECTED,
            "disconnection_event": EVENT_MCP_DISCONNECTED,
        };
    }

    private {
        bool started;
        bool ready;
        bool callbacks_set;
        Mutex m();
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : McpClientDataProviderBase(options) {
    }

    #! Deletes the object
    destructor() {
        stopEvents();
    }

    #! Returns the data provider name
    string getName() {
        return "events";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("MCP notification event provider for %s", mcp.getUrl());
    }

    #! Called when all observers have been added to the object
    observersReady() {
        ready = True;
        start();
    }

    #! Called to stop generating events
    stopEvents() {
        stop();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_MCP_CONNECTED: <DataProviderMessageInfo>{
                "desc": "SSE connection established to the MCP server",
                "type": McpNotificationEventDataType,
            },
            EVENT_MCP_DISCONNECTED: <DataProviderMessageInfo>{
                "desc": "SSE connection closed to the MCP server",
                "type": McpNotificationEventDataType,
            },
            EVENT_MCP_TOOLS_CHANGED: <DataProviderMessageInfo>{
                "desc": "The MCP server tool list has changed",
                "type": McpNotificationEventDataType,
            },
            EVENT_MCP_RESOURCES_CHANGED: <DataProviderMessageInfo>{
                "desc": "The MCP server resource list has changed",
                "type": McpNotificationEventDataType,
            },
            EVENT_MCP_PROMPTS_CHANGED: <DataProviderMessageInfo>{
                "desc": "The MCP server prompt list has changed",
                "type": McpNotificationEventDataType,
            },
            EVENT_MCP_LOG_MESSAGE: <DataProviderMessageInfo>{
                "desc": "A log message was received from the MCP server",
                "type": McpNotificationEventDataType,
            },
        };
    }

    #! Returns example data for the given observable event
    private auto getExampleEventDataImpl(string event_id) {
        switch (event_id) {
            case EVENT_MCP_LOG_MESSAGE:
                return {
                    "event": EVENT_MCP_LOG_MESSAGE,
                    "level": "info",
                    "logger": "mcp",
                    "data": "Server log message",
                };
            default:
                return {
                    "event": event_id,
                };
        }
    }

    #! Starts the SSE listener and registers callbacks
    private start() {
        m.lock();
        on_exit m.unlock();

        if (started) {
            return;
        }
        started = True;
        registerCallbacks();
        try {
            mcp.startSseListener();
        } catch (hash<ExceptionInfo> ex) {
            started = False;
            unregisterCallbacks();
            throw "MCP-NOTIFICATION-ERROR", sprintf("Failed to start MCP notification listener: %s: %s",
                ex.err, ex.desc);
        }
    }

    #! Stops the SSE listener and clears callbacks
    private stop() {
        m.lock();
        on_exit m.unlock();

        if (!started) {
            @debug("McpNotificationEventDataProvider::stop(): not started");
            return;
        }
        @debug("McpNotificationEventDataProvider::stop(): stopping notification listener");
        started = False;
        unregisterCallbacks();
        mcp.stopSseListener();
        @debug("McpNotificationEventDataProvider::stop(): notification listener stopped");
    }

    #! Registers MCP callbacks for notifications
    private registerCallbacks() {
        if (callbacks_set) {
            return;
        }
        @debug("McpNotificationEventDataProvider: registering callbacks");
        callbacks_set = True;
        mcp.setCallback("onConnected", \onConnected());
        mcp.setCallback("onDisconnected", \onDisconnected());
        mcp.setCallback("onToolsChanged", \onToolsChanged());
        mcp.setCallback("onResourcesChanged", \onResourcesChanged());
        mcp.setCallback("onPromptsChanged", \onPromptsChanged());
        mcp.setCallback("onLogMessage", \onLogMessage());
    }

    #! Unregisters MCP callbacks
    private unregisterCallbacks() {
        if (!callbacks_set) {
            return;
        }
        @debug("McpNotificationEventDataProvider: unregistering callbacks");
        callbacks_set = False;
        mcp.removeCallback("onConnected");
        mcp.removeCallback("onDisconnected");
        mcp.removeCallback("onToolsChanged");
        mcp.removeCallback("onResourcesChanged");
        mcp.removeCallback("onPromptsChanged");
        mcp.removeCallback("onLogMessage");
    }

    #! Handles SSE connected event
    private onConnected() {
        notifyObservers(EVENT_MCP_CONNECTED, {
            "event": EVENT_MCP_CONNECTED,
        });
    }

    #! Handles SSE disconnected event
    private onDisconnected() {
        notifyObservers(EVENT_MCP_DISCONNECTED, {
            "event": EVENT_MCP_DISCONNECTED,
        });
    }

    #! Handles tools changed notifications
    private onToolsChanged() {
        notifyObservers(EVENT_MCP_TOOLS_CHANGED, {
            "event": EVENT_MCP_TOOLS_CHANGED,
        });
    }

    #! Handles resources changed notifications
    private onResourcesChanged() {
        notifyObservers(EVENT_MCP_RESOURCES_CHANGED, {
            "event": EVENT_MCP_RESOURCES_CHANGED,
        });
    }

    #! Handles prompts changed notifications
    private onPromptsChanged() {
        notifyObservers(EVENT_MCP_PROMPTS_CHANGED, {
            "event": EVENT_MCP_PROMPTS_CHANGED,
        });
    }

    #! Handles log message notifications
    private onLogMessage(string level, *auto data, *string logger) {
        notifyObservers(EVENT_MCP_LOG_MESSAGE, {
            "event": EVENT_MCP_LOG_MESSAGE,
            "level": level,
            "logger": logger,
            "data": data,
        });
    }
}
}
