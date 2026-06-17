# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AsyncApiOperationDataProvider module definition

/** AsyncApiOperationDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AsyncApiDataProvider module
public namespace AsyncApiDataProvider {
#! The AsyncAPI operation data provider class
public class AsyncApiOperationDataProvider inherits AsyncApiDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Common transport metadata accepted when sending AsyncAPI messages
        const SendMessageOptions = {
            "headers": <DataProviderOptionInfo>{
                "display_name": "Headers",
                "short_desc": "Transport headers to pass with the message",
                "desc": "Optional transport headers or metadata passed to the AsyncAPI transport.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "correlation_id": <DataProviderOptionInfo>{
                "display_name": "Correlation ID",
                "short_desc": "Request/reply correlation identifier",
                "desc": "Optional request/reply correlation identifier for transports that support correlated "
                    "responses.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "reply_topic": <DataProviderOptionInfo>{
                "display_name": "Reply Topic",
                "short_desc": "Reply topic for broker request/reply flows",
                "desc": "Optional broker reply topic used by MQTT or Kafka-style request/reply flows.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "response_topic": <DataProviderOptionInfo>{
                "display_name": "Response Topic",
                "short_desc": "MQTT response topic",
                "desc": "Optional MQTT response topic.  This mirrors the AsyncAPI MQTT message binding name.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "key": <DataProviderOptionInfo>{
                "display_name": "Message Key",
                "short_desc": "Broker message key",
                "desc": "Optional broker message key, for example the Kafka record key.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "partition": <DataProviderOptionInfo>{
                "display_name": "Partition",
                "short_desc": "Kafka target partition",
                "desc": "Optional Kafka target partition.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "qos": <DataProviderOptionInfo>{
                "display_name": "QoS",
                "short_desc": "MQTT quality-of-service level",
                "desc": "Optional MQTT quality-of-service level.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "retain": <DataProviderOptionInfo>{
                "display_name": "Retain",
                "short_desc": "MQTT retain flag",
                "desc": "Optional MQTT retain flag.",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };

        #! The channel name
        string channel_name;

        #! The channel object
        AsyncApi::ChannelObject channel;

        #! The operation ID
        string operation_id;

        #! The operation object
        AsyncApi::OperationObject op;

        #! The message type
        *AbstractDataProviderType message_type;

        #! transport subscription handle for receive operations
        auto subscription;
    }

    #! Creates the object from the arguments
    constructor(AsyncApi::AsyncApi3Schema schema, *AbstractAsyncApiTransport transport, string channel_name,
            AsyncApi::ChannelObject channel, string operation_id, AsyncApi::OperationObject op)
            : AsyncApiDataProviderBase(transport) {
        self.schema = schema;
        self.channel_name = channel_name;
        self.channel = channel;
        self.operation_id = operation_id;
        self.op = op;
    }

    #! Returns the data provider name
    string getName() {
        return operation_id;
    }

    #! Returns the AsyncAPI channel name for this operation
    string getChannelName() {
        return channel_name;
    }

    #! Returns the resolved channel address/topic for this operation
    string getChannelAddress(*hash<auto> params) {
        return channel.getAddress(params);
    }

    #! Returns the AsyncAPI operation action
    string getOperationAction() {
        return op.action;
    }

    #! Returns the data provider description
    *string getDesc() {
        string desc = sprintf("AsyncAPI operation %y (%s) on channel %y", operation_id, op.action, channel_name);
        if (op.summary) {
            desc += sprintf("\n\n## Summary\n\n%s", op.summary);
        }
        if (op.desc) {
            desc += sprintf("\n\n## Description\n\n%s", op.desc);
        }
        return desc;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "type": "AsyncApiOperationDataProvider",
            "supports_read": False,
            "supports_create": False,
            "supports_update": False,
            "supports_upsert": False,
            "supports_delete": False,
            "supports_native_search": False,
            "supports_bulk_create": False,
            "supports_bulk_upsert": False,
            "supports_children": False,
            "supports_request": op.action == "send",
            "supports_messages": op.action == "send" ? DataProvider::MSG_Async : DataProvider::MSG_None,
            "send_message_options": op.action == "send" ? SendMessageOptions : NOTHING,
            "supports_observable": op.action == "receive",
            "transaction_management": False,
            "children_can_support_apis": False,
            "children_can_support_records": False,
        };
    }

    #! Starts observing inbound messages for receive operations
    observersReady() {
        if (op.action != "receive" || subscription) {
            return;
        }
        if (!transport) {
            throw "TRANSPORT-ERROR", sprintf("No transport configured; cannot observe receive operation %y",
                operation_id);
        }
        subscription = transport.subscribe(channel_name, \handleIncomingMessage(), {
            "operation": operation_id,
            "channel_address": channel.getAddress(),
        });
    }

    #! Stops observing inbound messages
    stopEvents() {
        if (subscription && transport) {
            transport.unsubscribe(subscription);
        }
        subscription = NOTHING;
    }

    #! Returns the description of a request message, if any
    private *AbstractDataProviderType getRequestTypeImpl() {
        if (op.action != "send") {
            return;
        }
        if (message_type) {
            return message_type;
        }
        return message_type = getMessageType();
    }

    #! Returns the description of a response message, if any
    private *AbstractDataProviderType getResponseTypeImpl() {
        # for send operations, there is no response in the traditional sense
        # AsyncAPI is event-driven, not request-response
        return;
    }

    #! Returns the message type for this operation
    /** @return the message type for the operation, or NOTHING if no messages define payloads

        Operations with multiple payload schemas are exposed as a data-provider union type so type metadata,
        action forms, and schema-driven callers do not silently lose alternatives.
    */
    private *AbstractDataProviderType getMessageType() {
        # get message schemas from the operation (already resolved)
        if (!op.messages) {
            return;
        }

        list<AbstractDataProviderType> types;
        foreach AsyncApi::MessageObject msg in (op.messages) {
            if (msg.payload) {
                if (*AbstractDataProviderType type = getTypeFromSchema(msg.payload, msg.name ?? operation_id)) {
                    types += type;
                }
            }
        }
        if (!types) {
            return;
        }
        return types.size() == 1 ? types[0] : new QoreUnionDataType(types);
    }

    #! Returns a data provider type from a schema object
    private *AbstractDataProviderType getTypeFromSchema(AsyncApi::SchemaObject schemaObj, *string type_name) {
        string type = schemaObj.type ?? "object";
        switch (type) {
            case "object": {
                HashDataType rv(sprintf("%s: %s", operation_id, type_name ?? "message"));
                if (schemaObj.properties) {
                    foreach hash<auto> i in (schemaObj.properties.pairIterator()) {
                        bool is_required = schemaObj.required{i.key} ?? False;
                        AbstractDataProviderType prop_type = getPropertyType(i.key, i.value, is_required);
                        rv.addField(new QoreDataField(i.key, i.value.desc ?? "", prop_type));
                    }
                }
                return rv;
            }
            case "array":
                if (schemaObj.items) {
                    *AbstractDataProviderType element_type = getTypeFromSchema(schemaObj.items,
                        type_name ? type_name + " item" : "item");
                    if (element_type) {
                        return new ListDataType(element_type, True);
                    }
                }
                return AbstractDataProviderTypeMap."list";
            case "string":
                return AbstractDataProviderTypeMap."string";
            case "integer":
                return AbstractDataProviderTypeMap."int";
            case "number":
                return AbstractDataProviderTypeMap."float";
            case "boolean":
                return AbstractDataProviderTypeMap."bool";
            case "any":
            default:
                return AbstractDataProviderTypeMap."auto";
        }
    }

    #! Returns the type for a property
    private AbstractDataProviderType getPropertyType(string name, AsyncApi::SchemaObject prop, bool required) {
        *AbstractDataProviderType type = getTypeFromSchema(prop, name);
        if (!type) {
            type = AbstractDataProviderTypeMap."auto";
        }
        if (!required) {
            type = type.getOrNothingType();
        }
        return type;
    }

    #! Returns a hash of all supported observable events
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        hash<string, hash<DataProviderMessageInfo>> rv = {};
        if (op.action == "receive" && op.messages) {
            foreach AsyncApi::MessageObject msg in (op.messages) {
                string msg_name = msg.name ?? operation_id;
                *AbstractDataProviderType msg_type;
                if (msg.payload) {
                    msg_type = getTypeFromSchema(msg.payload, msg_name);
                }
                rv{msg_name} = <DataProviderMessageInfo>{
                    "desc": msg.summary ?? msg.desc ?? sprintf("Event for operation %y", operation_id),
                    "type": msg_type ?? AbstractDataProviderTypeMap."auto",
                };
            }
        }
        return rv;
    }

    #! Handles a transport-delivered inbound message
    private handleIncomingMessage(auto msg, *hash<auto> headers) {
        hash<auto> hdrs = headers ?? {};
        string message_name = validateIncomingOperationMessage(\msg, \hdrs);
        notifyObservers(message_name, makeEventData(msg, hdrs, message_name));
    }

    #! Validates an inbound operation message and returns the matched AsyncAPI message name
    private string validateIncomingOperationMessage(reference<auto> msg, reference<hash<auto>> headers) {
        if (!op.messages) {
            return operation_id;
        }
        *hash<ExceptionInfo> last_ex;
        foreach AsyncApi::MessageObject msg_schema in (op.messages) {
            try {
                msg_schema.validatePayload(channel_name, operation_id, \msg);
                msg_schema.validateHeaders(channel_name, operation_id, \headers);
                return msg_schema.name ?? operation_id;
            } catch (hash<ExceptionInfo> ex) {
                last_ex = ex;
            }
        }
        if (last_ex) {
            throw last_ex.err, last_ex.desc, last_ex.arg;
        }
        return operation_id;
    }

    #! Builds observable event data while preserving scalar/list payloads
    private static hash<auto> makeEventData(auto msg, hash<auto> headers, string message_name) {
        if (msg.typeCode() == NT_HASH) {
            hash<auto> rv = msg;
            if (headers) {
                rv.headers = headers;
            }
            rv.message_id = message_name;
            return rv;
        }
        return {
            "payload": msg,
            "headers": headers,
            "message_id": message_name,
        };
    }

    #! Returns a hash of all supported outbound messages
    private hash<string, hash<DataProviderMessageInfo>> getMessageTypesImpl() {
        hash<string, hash<DataProviderMessageInfo>> rv = {};
        if (op.action == "send" && op.messages) {
            foreach AsyncApi::MessageObject msg in (op.messages) {
                string msg_name = msg.name ?? operation_id;
                *AbstractDataProviderType msg_type;
                if (msg.payload) {
                    msg_type = getTypeFromSchema(msg.payload, msg_name);
                }
                rv{msg_name} = <DataProviderMessageInfo>{
                    "desc": msg.summary ?? msg.desc ?? sprintf("Message for operation %y", operation_id),
                    "type": msg_type ?? AbstractDataProviderTypeMap."auto",
                };
            }
        }
        return rv;
    }

    #! Makes a request and returns the response
    /** @param req the message to send
        @param request_options the request options

        @return nothing for async operations

        @throw INVALID-OPERATION if the operation does not support sending
        @throw TRANSPORT-ERROR if no transport is configured
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (op.action != "send") {
            throw "INVALID-OPERATION", sprintf("Operation %y is a 'receive' operation and does not support "
                "sending messages", operation_id);
        }

        sendOperationMessage(req, request_options);
    }

    #! Sends a typed AsyncAPI message by message id
    /** @param message_id the message id selected by the data-provider message API
        @param msg the already type-validated message payload
        @param send_message_options optional transport headers/options

        @throw TRANSPORT-ERROR if no transport is configured or the transport send fails
    */
    private sendMessageImpl(string message_id, auto msg, *hash<auto> send_message_options) {
        hash<auto> headers = send_message_options && send_message_options.headers
            ? send_message_options.headers : {};
        if (send_message_options) {
            headers += send_message_options - "headers";
        }
        headers.message_id = message_id;
        sendOperationMessage(msg, headers);
    }

    #! Validates and sends an outbound operation message through the configured transport
    private sendOperationMessage(auto msg, *hash<auto> options) {
        if (!transport) {
            throw "TRANSPORT-ERROR", "No transport configured; cannot send message";
        }

        # validate the message
        hash<auto> headers = options ? options : {};
        schema.validateOutgoingMessage(channel_name, \msg, \headers);

        # send via transport
        transport.send(channel_name, msg, headers);
    }
}
}
