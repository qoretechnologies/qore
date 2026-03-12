# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyUpdateLeadInterestStatusDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating lead interest status
public class InstantlyUpdateLeadInterestStatusRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Lead ID",
                "short_desc": "Lead UUID",
                "desc": "The unique identifier (UUID) of the lead to update",
                "required": True,
            },
            "interest_status": {
                "type": StringType,
                "display_name": "Interest Status",
                "short_desc": "Lead interest status",
                "desc": "The interest status to set for the lead",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "interested", "display_name": "Interested"},
                    <AllowedValueInfo>{"value": "not_interested", "display_name": "Not Interested"},
                    <AllowedValueInfo>{"value": "neutral", "display_name": "Neutral"},
                    <AllowedValueInfo>{"value": "meeting_booked", "display_name": "Meeting Booked"},
                    <AllowedValueInfo>{"value": "meeting_completed", "display_name": "Meeting Completed"},
                    <AllowedValueInfo>{"value": "closed", "display_name": "Closed"},
                    <AllowedValueInfo>{"value": "out_of_office", "display_name": "Out of Office"},
                    <AllowedValueInfo>{"value": "wrong_person", "display_name": "Wrong Person"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Instantly update lead interest status action data provider
public class InstantlyUpdateLeadInterestStatusDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-lead-interest-status",
            "desc": "Update the interest status of a lead in Instantly",
            "type": "InstantlyUpdateLeadInterestStatusDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyUpdateLeadInterestStatusRequestDataType RequestType();

        #! Response type
        static InstantlyLeadDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest) : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "update-lead-interest-status";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update the interest status of a lead in Instantly";
    }

    #! Makes the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPatch(sprintf("/leads/%s/interest-status", req.id), {"interest_status": req.interest_status});
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
