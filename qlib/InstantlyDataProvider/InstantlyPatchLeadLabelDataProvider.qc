# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyPatchLeadLabelDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for patching a lead label
public class InstantlyPatchLeadLabelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Label ID",
                "short_desc": "Label UUID",
                "desc": "The unique identifier (UUID) of the label to update",
                "required": True,
            },
            "label": {
                "type": StringOrNothingType,
                "display_name": "Label",
                "short_desc": "Label name",
                "desc": "The new name for the label",
            },
            "interest_status_label": {
                "type": StringOrNothingType,
                "display_name": "Interest Status Label",
                "short_desc": "Interest status for this label",
                "desc": "The interest status associated with this label (e.g., `positive`, "
                    "`negative`, `neutral`)",
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

#! Instantly patch lead label action data provider
public class InstantlyPatchLeadLabelDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "patch-lead-label",
            "desc": "Update a lead label by ID in Instantly",
            "type": "InstantlyPatchLeadLabelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyPatchLeadLabelRequestDataType RequestType();

        #! Response type
        static InstantlyLeadLabelDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest) : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "patch-lead-label";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update a lead label by ID in Instantly";
    }

    #! Makes the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPatch(sprintf("/lead-labels/%s", req.id), req - "id");
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
