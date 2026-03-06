# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelCreateTimeOffRequestDataProvider.qc Deel create time-off request action data provider

/*  DeelCreateTimeOffRequestDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Response type for a time-off request
public class DeelTimeOffRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftStringType,
                "display_name": "ID",
                "short_desc": "The time-off request ID",
                "desc": "The unique identifier of the time-off request",
            },
            "contract_id": {
                "type": StringOrNothingType,
                "display_name": "Contract ID",
                "short_desc": "The associated contract ID",
                "desc": "The ID of the contract this time-off request belongs to",
            },
            "type_id": {
                "type": StringOrNothingType,
                "display_name": "Type ID",
                "short_desc": "The time-off type ID",
                "desc": "The ID of the time-off type (entitlement) for this request",
            },
            "start_date": {
                "type": StringOrNothingType,
                "display_name": "Start Date",
                "short_desc": "The time-off start date",
                "desc": "The first day of the time-off period (YYYY-MM-DD format)",
            },
            "end_date": {
                "type": StringOrNothingType,
                "display_name": "End Date",
                "short_desc": "The time-off end date",
                "desc": "The last day of the time-off period (YYYY-MM-DD format)",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The request status",
                "desc": "The current status of the time-off request",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Notes about the request",
                "desc": "Additional notes or reason for the time-off request",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the request was created",
                "desc": "The timestamp when the time-off request was created (ISO 8601 format)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type for creating a time-off request
public class DeelCreateTimeOffRequestRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "contract_id": {
                "type": StringType,
                "display_name": "Contract ID",
                "ref_data": "contracts",
                "short_desc": "The associated contract ID",
                "desc": "The unique identifier of the contract for this time-off request",
                "required": True,
            },
            "type_id": {
                "type": StringType,
                "display_name": "Time-Off Type ID",
                "ref_data": "time_off_types",
                "short_desc": "The time-off type ID",
                "desc": "The ID of the time-off type (entitlement) to use for this request",
                "required": True,
            },
            "start_date": {
                "type": StringType,
                "display_name": "Start Date",
                "short_desc": "The time-off start date",
                "desc": "The first day of the requested time-off period (YYYY-MM-DD format)",
                "required": True,
                "example_value": "2026-04-01",
            },
            "end_date": {
                "type": StringType,
                "display_name": "End Date",
                "short_desc": "The time-off end date",
                "desc": "The last day of the requested time-off period (YYYY-MM-DD format)",
                "required": True,
                "example_value": "2026-04-05",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Notes about the request",
                "desc": "Optional notes or reason for the time-off request",
                "example_value": "Family vacation",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel create time-off request action data provider
public class DeelCreateTimeOffRequestDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-request",
            "desc": "Create a new time-off request in Deel",
            "type": "DeelCreateTimeOffRequestDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest) : DeelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create-request";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new time-off request in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelCreateTimeOffRequestRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelTimeOffRequestDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # time_offs POST requires body wrapped in {"data": {...}}
        return doPost("time_offs", {"data": req}).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
