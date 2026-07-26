# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore A2aEventDataProvider class definition

/*  A2aEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the A2aClientDataProvider module
public namespace A2aClientDataProvider {
#! A2A event IDs
public const EVENT_A2A_TASK_STATUS_UPDATE = "a2a-task-status-update";
public const EVENT_A2A_TASK_ARTIFACT_UPDATE = "a2a-task-artifact-update";
public const EVENT_A2A_TASK_MESSAGE_UPDATE = "a2a-task-message-update";
public const EVENT_A2A_CONNECTED = "a2a-connected";
public const EVENT_A2A_DISCONNECTED = "a2a-disconnected";

#! A2A event data type
public class A2aEventDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "event": {
                "type": StringType,
                "display_name": "Event",
                "short_desc": "A2A event type",
                "desc": "The A2A event identifier",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Task Status Update",
                        "short_desc": "Task status changed",
                        "desc": "A task's status has been updated",
                        "value": EVENT_A2A_TASK_STATUS_UPDATE,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Task Artifact Update",
                        "short_desc": "New or modified artifact",
                        "desc": "A task artifact has been added or modified",
                        "value": EVENT_A2A_TASK_ARTIFACT_UPDATE,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Task Message Update",
                        "short_desc": "New message",
                        "desc": "A new message has been added to a task",
                        "value": EVENT_A2A_TASK_MESSAGE_UPDATE,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Connected",
                        "short_desc": "Connected to agent",
                        "desc": "The connection to the A2A agent has been established",
                        "value": EVENT_A2A_CONNECTED,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Disconnected",
                        "short_desc": "Disconnected from agent",
                        "desc": "The connection to the A2A agent has been closed",
                        "value": EVENT_A2A_DISCONNECTED,
                    },
                ),
                "example_value": EVENT_A2A_TASK_STATUS_UPDATE,
            },
            "task_id": {
                "type": StringOrNothingType,
                "display_name": "Task ID",
                "short_desc": "Task identifier",
                "desc": "The task ID associated with this event",
            },
            "data": {
                "type": AutoType,
                "display_name": "Data",
                "short_desc": "Event data",
                "desc": "Event payload data",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const A2aEventDataType = new A2aEventDataType();

#! A2A event data provider
public class A2aEventDataProvider inherits A2aClientDataProviderBase, DataProvider::DelayedObservable {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "A2aEventDataProvider",
            "supports_observable": True,
            "supports_connections": True,
            "connection_event": EVENT_A2A_CONNECTED,
            "disconnection_event": EVENT_A2A_DISCONNECTED,
        };
    }

    private {
        bool started;
        bool ready;
        bool callbacks_set;
        Mutex m();
    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    constructor(*hash<auto> options) : A2aClientDataProviderBase(options) {
    }

    destructor() {
        stopEvents();
    }

    string getName() {
        return "events";
    }

    *string getDesc() {
        return sprintf("A2A event provider for %s", a2a.getUrl());
    }

    observersReady() {
        ready = True;
        start();
    }

    stopEvents() {
        stop();
    }

    #! Emits a task status update event
    /** Can be called externally to inject events from SSE streams or push notifications.
    */
    emitTaskStatusUpdate(string task_id, hash<auto> status_data) {
        notifyObservers(EVENT_A2A_TASK_STATUS_UPDATE, {
            "event": EVENT_A2A_TASK_STATUS_UPDATE,
            "task_id": task_id,
            "data": status_data,
        });
    }

    #! Emits a task artifact update event
    emitTaskArtifactUpdate(string task_id, hash<auto> artifact_data) {
        notifyObservers(EVENT_A2A_TASK_ARTIFACT_UPDATE, {
            "event": EVENT_A2A_TASK_ARTIFACT_UPDATE,
            "task_id": task_id,
            "data": artifact_data,
        });
    }

    #! Emits a task message update event
    emitTaskMessageUpdate(string task_id, hash<auto> message_data) {
        notifyObservers(EVENT_A2A_TASK_MESSAGE_UPDATE, {
            "event": EVENT_A2A_TASK_MESSAGE_UPDATE,
            "task_id": task_id,
            "data": message_data,
        });
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_A2A_CONNECTED: <DataProviderMessageInfo>{
                "desc": "Connection established to the A2A agent",
                "type": A2aEventDataType,
            },
            EVENT_A2A_DISCONNECTED: <DataProviderMessageInfo>{
                "desc": "Connection closed to the A2A agent",
                "type": A2aEventDataType,
            },
            EVENT_A2A_TASK_STATUS_UPDATE: <DataProviderMessageInfo>{
                "desc": "A task's status has been updated",
                "type": A2aEventDataType,
            },
            EVENT_A2A_TASK_ARTIFACT_UPDATE: <DataProviderMessageInfo>{
                "desc": "A task artifact has been added or modified",
                "type": A2aEventDataType,
            },
            EVENT_A2A_TASK_MESSAGE_UPDATE: <DataProviderMessageInfo>{
                "desc": "A new message has been added to a task",
                "type": A2aEventDataType,
            },
        };
    }

    private auto getExampleEventDataImpl(string event_id) {
        switch (event_id) {
            case EVENT_A2A_TASK_STATUS_UPDATE:
                return {
                    "event": EVENT_A2A_TASK_STATUS_UPDATE,
                    "task_id": "task-123",
                    "data": {"state": "completed", "timestamp": "2026-01-01T00:00:00Z"},
                };
            case EVENT_A2A_TASK_ARTIFACT_UPDATE:
                return {
                    "event": EVENT_A2A_TASK_ARTIFACT_UPDATE,
                    "task_id": "task-123",
                    "data": {"artifactId": "art-1", "name": "result.json"},
                };
            default:
                return {
                    "event": event_id,
                };
        }
    }

    #! Starts the event provider and registers A2aClient callbacks
    private start() {
        m.lock();
        on_exit m.unlock();

        if (started) {
            return;
        }
        started = True;
        registerCallbacks();

        notifyObservers(EVENT_A2A_CONNECTED, {
            "event": EVENT_A2A_CONNECTED,
        });
    }

    #! Stops the event provider and unregisters callbacks
    private stop() {
        m.lock();
        on_exit m.unlock();

        if (!started) {
            return;
        }
        started = False;
        unregisterCallbacks();

        notifyObservers(EVENT_A2A_DISCONNECTED, {
            "event": EVENT_A2A_DISCONNECTED,
        });
    }

    #! Registers A2aClient callbacks for task events
    private registerCallbacks() {
        if (callbacks_set) {
            return;
        }
        callbacks_set = True;
        a2a.setCallback("onTaskStatusUpdate", sub (string task_id, hash<auto> data) {
            emitTaskStatusUpdate(task_id, data);
        });
        a2a.setCallback("onTaskArtifactUpdate", sub (string task_id, hash<auto> data) {
            emitTaskArtifactUpdate(task_id, data);
        });
        a2a.setCallback("onTaskMessageUpdate", sub (string task_id, hash<auto> data) {
            emitTaskMessageUpdate(task_id, data);
        });
    }

    #! Unregisters A2aClient callbacks
    private unregisterCallbacks() {
        if (!callbacks_set) {
            return;
        }
        callbacks_set = False;
        a2a.removeCallback("onTaskStatusUpdate");
        a2a.removeCallback("onTaskArtifactUpdate");
        a2a.removeCallback("onTaskMessageUpdate");
    }
}
}
