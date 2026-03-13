# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyCreateAccountDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating an account
public class InstantlyCreateAccountRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "Email address for the account",
                "desc": "The email address to add as an account",
                "required": True,
                "preselected": True,
            },
            "provider_code": {
                "type": IntOrNothingType,
                "display_name": "Provider Code",
                "short_desc": "Email provider code",
                "desc": "The email provider code: `1` = Custom IMAP/SMTP, `2` = Google, `3` = Microsoft, "
                    "`4` = AWS, `8` = AirMail",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Custom IMAP/SMTP"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Google"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Microsoft"},
                    <AllowedValueInfo>{"value": 4, "display_name": "AWS"},
                    <AllowedValueInfo>{"value": 8, "display_name": "AirMail"},
                ),
            },
            "daily_limit": {
                "type": IntOrNothingType,
                "display_name": "Daily Limit",
                "short_desc": "Email sending limit per day",
                "desc": "The maximum number of emails that can be sent per day from this account",
            },
            "warmup_status": {
                "type": IntOrNothingType,
                "display_name": "Warmup Status",
                "short_desc": "Warmup status code",
                "desc": "The warmup status: `0` = Paused, `1` = Active",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "Paused"},
                    <AllowedValueInfo>{"value": 1, "display_name": "Active"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a new email account
public class InstantlyCreateAccountDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-account",
            "desc": "Create a new email account in Instantly",
            "type": "InstantlyCreateAccountDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyCreateAccountRequestDataType RequestType();

        #! Response type
        static InstantlyAccountDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create-account";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new email account in Instantly";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("/accounts", req);
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
