# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelSubmitTimesheetDataProvider.qc Deel submit timesheet action data provider

/*  DeelSubmitTimesheetDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for submitting a timesheet
public class DeelSubmitTimesheetRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "contract_id": {
                "type": StringType,
                "display_name": "Contract ID",
                "ref_data": "contracts",
                "short_desc": "The associated contract ID",
                "desc": "The unique identifier of the contract this timesheet belongs to",
                "required": True,
            },
            "date_submitted": {
                "type": StringType,
                "display_name": "Date Submitted",
                "short_desc": "The timesheet date",
                "desc": "The date for this timesheet entry (YYYY-MM-DD format)",
                "required": True,
                "example_value": "2026-02-24",
            },
            "quantity": {
                "type": NumberType,
                "display_name": "Hours (Quantity)",
                "short_desc": "The number of hours worked",
                "desc": "The number of hours worked for this timesheet entry",
                "required": True,
                "example_value": 8.0,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Work description",
                "desc": "A description of the work performed during this timesheet period",
                "example_value": "Developed new API endpoints",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel submit timesheet action data provider
public class DeelSubmitTimesheetDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "submit",
            "desc": "Submit a timesheet entry in Deel",
            "type": "DeelSubmitTimesheetDataProvider",
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
        return "submit";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Submit a timesheet entry in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelSubmitTimesheetRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelTimesheetDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("timesheets", {"data": req}).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
