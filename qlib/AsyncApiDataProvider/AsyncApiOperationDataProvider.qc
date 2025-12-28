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
public class AsyncApiOperationDataProvider inherits AsyncApiDataProviderBase {
    public {
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
            "supports_observable": op.action == "receive",
            "transaction_management": False,
            "children_can_support_apis": False,
            "children_can_support_records": False,
        };
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
    private *AbstractDataProviderType getMessageType() {
        # get message schemas from the operation (already resolved)
        if (!op.messages) {
            return;
        }

        # for now, handle the first message
        foreach AsyncApi::MessageObject msg in (op.messages) {
            if (msg.payload) {
                return getTypeFromSchema(msg.payload);
            }
        }
    }

    #! Returns a data provider type from a schema object
    private *AbstractDataProviderType getTypeFromSchema(AsyncApi::SchemaObject schemaObj) {
        string type = schemaObj.type ?? "object";
        switch (type) {
            case "object": {
                HashDataType rv(operation_id + ": message");
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
                    *AbstractDataProviderType element_type = getTypeFromSchema(schemaObj.items);
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
        *AbstractDataProviderType type = getTypeFromSchema(prop);
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
                    msg_type = getTypeFromSchema(msg.payload);
                }
                rv{msg_name} = <DataProviderMessageInfo>{
                    "desc": msg.summary ?? msg.desc ?? sprintf("Event for operation %y", operation_id),
                    "type": msg_type ?? AbstractDataProviderTypeMap."auto",
                };
            }
        }
        return rv;
    }

    #! Returns a hash of all supported outbound messages
    private hash<string, hash<DataProviderMessageInfo>> getMessageTypesImpl() {
        hash<string, hash<DataProviderMessageInfo>> rv = {};
        if (op.action == "send" && op.messages) {
            foreach AsyncApi::MessageObject msg in (op.messages) {
                string msg_name = msg.name ?? operation_id;
                *AbstractDataProviderType msg_type;
                if (msg.payload) {
                    msg_type = getTypeFromSchema(msg.payload);
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

        if (!transport) {
            throw "TRANSPORT-ERROR", "No transport configured; cannot send message";
        }

        # validate the message
        hash<auto> headers = {};
        schema.validateOutgoingMessage(channel_name, \req, \headers);

        # send via transport
        transport.send(channel_name, req, headers);
    }
}
}
