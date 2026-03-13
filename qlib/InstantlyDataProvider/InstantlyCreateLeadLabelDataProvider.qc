# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyCreateLeadLabelDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a lead label
public class InstantlyCreateLeadLabelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "label": {
                "type": StringType,
                "display_name": "Label",
                "short_desc": "Label name",
                "desc": "The name of the label to create",
                "required": True,
            },
            "interest_status_label": {
                "type": StringType,
                "display_name": "Interest Status Label",
                "short_desc": "Interest status for this label",
                "desc": "The interest status associated with this label (e.g., `positive`, `negative`, `neutral`)",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "positive", "display_name": "Positive"},
                    <AllowedValueInfo>{"value": "negative", "display_name": "Negative"},
                    <AllowedValueInfo>{"value": "neutral", "display_name": "Neutral"},
                ),
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Label description",
                "desc": "The description for the label",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Instantly create lead label action data provider
public class InstantlyCreateLeadLabelDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-lead-label",
            "desc": "Create a new lead label in Instantly",
            "type": "InstantlyCreateLeadLabelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyCreateLeadLabelRequestDataType RequestType();

        #! Response type
        static InstantlyLeadLabelDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest) : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create-lead-label";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new lead label in Instantly";
    }

    #! Makes the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("/lead-labels", req);
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
