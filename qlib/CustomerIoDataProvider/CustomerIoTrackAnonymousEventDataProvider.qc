# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoTrackAnonymousEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Request type for tracking an anonymous event
public class CustomerIoTrackAnonymousEventRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Event Name",
                "short_desc": "Event name",
                "desc": "Event name",
                "required": True,
                "preselected": True,
            },
            "data": {
                "type": AutoHashOrNothingType,
                "display_name": "Event Data",
                "short_desc": "Event data/properties",
                "desc": "Event data/properties",
            },
            "anonymous_id": {
                "type": StringOrNothingType,
                "display_name": "Anonymous ID",
                "short_desc": "Anonymous identifier",
                "desc": "Anonymous identifier",
            },
            "timestamp": {
                "type": IntOrNothingType,
                "display_name": "Timestamp",
                "short_desc": "Unix timestamp",
                "desc": "Unix timestamp for the event",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for tracking an anonymous event via the Track API
public class CustomerIoTrackAnonymousEventDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "track-anonymous-event",
            "desc": "Track an anonymous event in Customer.io (not tied to a customer)",
            "type": "CustomerIoTrackAnonymousEventDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoTrackAnonymousEventRequestDataType RequestType();

        #! Response type
        #! Response type: auto hash (Track API returns empty body on success)
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest)
            : CustomerIoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CustomerIoRestClient::CustomerIoRestConnection conn)
            : CustomerIoDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "track-anonymous-event";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Track an anonymous event in Customer.io (not tied to a customer)";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash body = {
            "name": req.name,
        };
        if (exists req.data) {
            body.data = req.data;
        }
        if (exists req.anonymous_id) {
            body.anonymous_id = req.anonymous_id;
        }
        if (exists req.timestamp) {
            body.timestamp = req.timestamp;
        }
        auto response = doTrackPost("/events", body);
        return response ?? {"success": True};
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashType);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider
