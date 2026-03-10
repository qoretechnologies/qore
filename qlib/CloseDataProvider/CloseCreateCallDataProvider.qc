# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseCreateCallDataProvider.qc Close create call action data provider

/*  CloseCreateCallDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Request type for creating a call
public class CloseCreateCallRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a call
        const Fields = {
            "lead_id": {
                "type": StringType,
                "display_name": "Lead ID",
                "short_desc": "The lead ID",
                "desc": "The unique identifier of the lead to associate with this call",
                "required": True,
            },
            "direction": {
                "type": StringOrNothingType,
                "display_name": "Direction",
                "short_desc": "Call direction",
                "desc": "Call direction (`inbound` or `outbound`)",
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Inbound",
                        "short_desc": "Incoming call",
                        "desc": "Call received from an external party",
                        "value": "inbound",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Outbound",
                        "short_desc": "Outgoing call",
                        "desc": "Call made to an external party",
                        "value": "outbound",
                    },
                ),
            },
            "duration": {
                "type": IntOrNothingType,
                "display_name": "Duration",
                "short_desc": "Call duration in seconds",
                "desc": "Call duration in seconds",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The phone number",
                "desc": "The phone number associated with this call",
            },
            "disposition": {
                "type": StringOrNothingType,
                "display_name": "Disposition",
                "short_desc": "Call disposition",
                "desc": "The disposition or outcome of the call",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "Call note",
                "desc": "A text note to associate with this call",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Call status",
                "desc": "The status of the call",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact",
                "desc": "The unique identifier of the contact associated with this call",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The user who made the call",
                "desc": "The unique identifier of the user who made or received this call",
            },
            "note_html": {
                "type": StringOrNothingType,
                "display_name": "Note HTML",
                "short_desc": "HTML note for the call",
                "desc": "An HTML-formatted note to associate with this call",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Close create call action data provider
public class CloseCreateCallDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new call activity in Close",
            "type": "CloseCreateCallDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new call activity in Close";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new CloseCreateCallRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new CloseCallDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("activity/call", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
