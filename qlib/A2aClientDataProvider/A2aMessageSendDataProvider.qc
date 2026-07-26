# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore A2A message data providers

/*  Copyright 2026 Qore Technologies, s.r.o.

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
#! A2A messages container data provider
public class A2aMessagesDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "messages",
            "desc": "A2A messages data provider",
            "type": "A2aMessagesDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "messages";
    }

    private *list<string> getChildProviderNamesImpl() {
        return ("send",);
    }

    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "send":
                return new A2aMessageSendDataProvider(a2a);
        }
        return NOTHING;
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! A2A send message data provider
public class A2aMessageSendDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "send",
            "desc": "Sends a message to the A2A agent",
            "type": "A2aMessageSendDataProvider",
            "supports_request": True,
        };

    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "send";
    }

    *string getDesc() {
        return "Sends a message to the A2A agent and returns a task";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return A2aMessageSendRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return A2aTaskResultDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.role);
        @assert(req.parts);
        @debug("Sending message with role %y", req.role);

        hash<auto> msg = {
            "role": req.role,
            "parts": req.parts,
        };
        if (req.messageId) {
            msg.messageId = req.messageId;
        }
        if (req.metadata) {
            msg.metadata = req.metadata;
        }

        hash<auto> config;
        if (req.contextId) {
            config.contextId = req.contextId;
        }
        if (req.configuration) {
            config.configuration = req.configuration;
        }
        if (req.tenant) {
            config.tenant = req.tenant;
        }

        return a2a.sendMessage(msg, config);
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
