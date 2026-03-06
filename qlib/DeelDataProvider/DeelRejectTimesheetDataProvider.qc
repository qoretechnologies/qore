# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelRejectTimesheetDataProvider.qc Deel reject timesheet action data provider

/*  DeelRejectTimesheetDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for rejecting a timesheet
public class DeelRejectTimesheetRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "timesheet_id": {
                "type": StringType,
                "display_name": "Timesheet ID",
                "ref_data": "timesheets",
                "short_desc": "The timesheet ID to reject",
                "desc": "The unique identifier of the timesheet entry to reject",
                "required": True,
            },
            "rejection_reason": {
                "type": StringOrNothingType,
                "display_name": "Rejection Reason",
                "short_desc": "The reason for rejection",
                "desc": "An explanation for why the timesheet entry was rejected",
                "example_value": "Incorrect hours reported",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel reject timesheet action data provider
public class DeelRejectTimesheetDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "reject",
            "desc": "Reject a timesheet entry in Deel",
            "type": "DeelRejectTimesheetDataProvider",
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
        return "reject";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Reject a timesheet entry in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelRejectTimesheetRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelTimesheetDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string timesheet_id = remove req.timesheet_id;
        return doPost("timesheets/" + encode_url(timesheet_id, True) + "/reject", req).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
