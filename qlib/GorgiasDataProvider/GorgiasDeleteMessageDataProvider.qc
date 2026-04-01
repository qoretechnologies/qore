# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasDeleteMessageDataProvider.qc Gorgias delete message action data provider

/*  GorgiasDeleteMessageDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Request type for deleting a message
public class GorgiasDeleteMessageRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a message
        const Fields = {
            "ticket_id": {
                "type": IntType,
                "display_name": "Ticket ID",
                "short_desc": "The ticket containing the message",
                "desc": "The ID of the ticket containing the message to delete",
                "required": True,
                "ref_data": "tickets",
            },
            "message_id": {
                "type": IntType,
                "display_name": "Message ID",
                "short_desc": "The message to delete",
                "desc": "The ID of the message to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for delete message operation
public class GorgiasDeleteMessageResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "deleted": {
                "type": BoolType,
                "display_name": "Deleted",
                "short_desc": "Whether the deletion was successful",
                "desc": "Returns `True` if the message was successfully deleted",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Gorgias delete message action data provider
public class GorgiasDeleteMessageDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a message from a Gorgias ticket",
            "type": "GorgiasDeleteMessageDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static GorgiasDeleteMessageResponseDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    string getName() {
        return "delete";
    }

    *string getDesc() {
        return "Delete a message from a Gorgias ticket";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GorgiasDeleteMessageRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("tickets/" + req.ticket_id + "/messages/" + req.message_id);
        return {"deleted": True};
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
