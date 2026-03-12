# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyGetIPAnalyticsStatsByDateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for getting inbox placement analytics stats by date
public class InstantlyGetIPAnalyticsStatsByDateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "start_date": {
                "type": DateOrNothingType,
                "display_name": "Start Date",
                "short_desc": "Start date for the date range",
                "desc": "The start date for the analytics date range filter",
            },
            "end_date": {
                "type": DateOrNothingType,
                "display_name": "End Date",
                "short_desc": "End date for the date range",
                "desc": "The end date for the analytics date range filter",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for getting inbox placement analytics stats by date
public class InstantlyGetIPAnalyticsStatsByDateDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-ip-analytics-stats-by-date",
            "desc": "Get inbox placement analytics stats by date from Instantly",
            "type": "InstantlyGetIPAnalyticsStatsByDateDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyGetIPAnalyticsStatsByDateRequestDataType RequestType();

        #! Response type
        static HashDataType ResponseType("GetIPAnalyticsStatsByDateResponse");
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "get-ip-analytics-stats-by-date";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Get inbox placement analytics stats by date from Instantly";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> params;
        if (req.start_date) {
            params.start_date = string(req.start_date);
        }
        if (req.end_date) {
            params.end_date = string(req.end_date);
        }
        return doGet("/inbox-placement-analytics/stats-by-date", params);
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
