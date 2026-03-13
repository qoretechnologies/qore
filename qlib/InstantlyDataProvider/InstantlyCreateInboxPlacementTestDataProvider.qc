# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyCreateInboxPlacementTestDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating an inbox placement test
public class InstantlyCreateInboxPlacementTestRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Test name",
                "desc": "The name of the inbox placement test to create",
                "required": True,
                "preselected": True,
            },
            "type": {
                "type": IntOrNothingType,
                "display_name": "Type",
                "short_desc": "Test type",
                "desc": "The test type: `1` = One-time, `2` = Automated",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "One-time"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Automated"},
                ),
            },
            "sending_method": {
                "type": IntOrNothingType,
                "display_name": "Sending Method",
                "short_desc": "Sending method",
                "desc": "The sending method: `1` = From Instantly, `2` = From Outside",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "From Instantly"},
                    <AllowedValueInfo>{"value": 2, "display_name": "From Outside"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating an inbox placement test
public class InstantlyCreateInboxPlacementTestDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-inbox-placement-test",
            "desc": "Create a new inbox placement test in Instantly",
            "type": "InstantlyCreateInboxPlacementTestDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyCreateInboxPlacementTestRequestDataType RequestType();

        #! Response type
        static InstantlyInboxPlacementTestDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create-inbox-placement-test";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new inbox placement test in Instantly";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("/inbox-placement-tests", req);
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
