# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelCreateOpportunityDataProvider.qc GoHighLevel create opportunity action data provider

/*  GoHighLevelCreateOpportunityDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! Request data type for the create opportunity action
public class GoHighLevelCreateOpportunityRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "pipelineId": {
                "type": StringType,
                "display_name": "Pipeline ID",
                "short_desc": "The pipeline ID for the opportunity",
                "desc": "The pipeline ID for the opportunity",
                "required": True,
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The opportunity name",
                "desc": "The opportunity name",
                "required": True,
            },
            "status": {
                "type": StringType,
                "display_name": "Status",
                "short_desc": "The opportunity status",
                "desc": "Opportunity status: `open`, `won`, `lost`, or `abandoned`",
                "required": True,
            },
            "contactId": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID for the opportunity",
                "desc": "The contact ID for the opportunity",
                "required": True,
            },
            "pipelineStageId": {
                "type": StringOrNothingType,
                "display_name": "Pipeline Stage ID",
                "short_desc": "The pipeline stage ID",
                "desc": "The pipeline stage ID",
            },
            "monetaryValue": {
                "type": SoftFloatOrNothingType,
                "display_name": "Monetary Value",
                "short_desc": "The monetary value",
                "desc": "The monetary value",
            },
            "assignedTo": {
                "type": StringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "User ID to assign to",
                "desc": "User ID to assign to",
            },
            "customFields": {
                "type": new SoftListDataType(new GoHighLevelCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Custom field values; each entry has an `id` "
                    "(custom field ID) and `field_value`",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! GoHighLevel create opportunity action data provider
public class GoHighLevelCreateOpportunityDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new opportunity in GoHighLevel",
            "type": "GoHighLevelCreateOpportunityDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new opportunity in GoHighLevel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GoHighLevelCreateOpportunityRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new GoHighLevelOpportunityDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("opportunities", req).opportunity;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
