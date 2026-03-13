# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyDeleteLeadLabelDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a lead label
public class InstantlyDeleteLeadLabelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Label ID",
                "short_desc": "Label UUID",
                "desc": "The unique identifier (UUID) of the label to delete",
                "required": True,
            },
            "reassigned_status": {
                "type": IntOrNothingType,
                "display_name": "Reassigned Status",
                "short_desc": "Interest status to reassign leads to",
                "desc": "The interest status to reassign leads and emails to when the label "
                    "is deleted",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Instantly delete lead label action data provider
public class InstantlyDeleteLeadLabelDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-lead-label",
            "desc": "Delete a lead label by ID from Instantly",
            "type": "InstantlyDeleteLeadLabelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyDeleteLeadLabelRequestDataType RequestType();

        #! Response type
        static HashDataType ResponseType("DeleteLeadLabelResponse");
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest) : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "delete-lead-label";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Delete a lead label by ID from Instantly";
    }

    #! Makes the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.id;
        return doDelete(sprintf("/lead-labels/%s", id), req);
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
