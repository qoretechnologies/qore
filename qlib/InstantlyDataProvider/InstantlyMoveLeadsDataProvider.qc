# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyMoveLeadsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Request type for moving leads
public class InstantlyMoveLeadsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "lead_ids": {
                "type": SoftAutoListType,
                "display_name": "Lead IDs",
                "short_desc": "List of lead UUIDs to move",
                "desc": "The list of lead UUIDs to move",
                "required": True,
            },
            "to_campaign_id": {
                "type": StringOrNothingType,
                "display_name": "To Campaign ID",
                "short_desc": "Target campaign UUID",
                "desc": "The UUID of the campaign to move leads to",
                "ref_data": "campaigns",
                "preselected": True,
            },
            "to_list_id": {
                "type": StringOrNothingType,
                "display_name": "To List ID",
                "short_desc": "Target lead list UUID",
                "desc": "The UUID of the lead list to move leads to",
                "ref_data": "lead_lists",
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Instantly move leads action data provider
public class InstantlyMoveLeadsDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "move-leads",
            "desc": "Move leads to a different campaign or list in Instantly",
            "type": "InstantlyMoveLeadsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyMoveLeadsRequestDataType RequestType();

        #! Response type
        static HashDataType ResponseType("MoveLeadsResponse");
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest) : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "move-leads";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Move leads to a different campaign or list in Instantly";
    }

    #! Makes the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("/leads/move", req);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
