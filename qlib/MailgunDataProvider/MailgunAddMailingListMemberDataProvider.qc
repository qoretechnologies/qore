# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunAddMailingListMemberDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Request type for adding a mailing list member
public class MailgunAddMailingListMemberRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "list_address": {
                "type": StringType,
                "display_name": "Mailing List",
                "short_desc": "The mailing list address",
                "desc": "The email address of the mailing list to add the member to",
                "example_value": "developers@lists.example.com",
                "required": True,
            },
            "address": {
                "type": StringType,
                "display_name": "Member Email",
                "short_desc": "Email address to add",
                "desc": "The email address of the member to add",
                "example_value": "user@example.com",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Member's name",
                "desc": "The name of the member",
                "example_value": "John Doe",
            },
            "vars": {
                "type": AutoHashOrNothingType,
                "display_name": "Variables",
                "short_desc": "Custom variables",
                "desc": "Custom JSON variables to associate with the member",
            },
            "subscribed": {
                "type": BoolOrNothingType,
                "display_name": "Subscribed",
                "short_desc": "Subscription status",
                "desc": "Whether the member is subscribed (default: `True`)",
                "default_value": True,
            },
            "upsert": {
                "type": BoolOrNothingType,
                "display_name": "Upsert",
                "short_desc": "Update if exists",
                "desc": "If `True`, update the member if they already exist (default: `False`)",
                "default_value": False,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for adding a member to a mailing list
public class MailgunAddMailingListMemberDataProvider inherits MailgunDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-mailing-list-member",
            "desc": "Add a member to a Mailgun mailing list",
            "type": "MailgunAddMailingListMemberDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static MailgunAddMailingListMemberRequestDataType RequestType();

        #! Response type
        static MailgunMailingListMemberDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(MailgunRestClient::MailgunRestClient rest, *string api_domain)
            : MailgunDataProviderBase(rest, api_domain) {
    }

    #! Creates the object with a connection
    constructor(MailgunRestClient::MailgunRestConnection conn) : MailgunDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "add-mailing-list-member";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Add a member to a Mailgun mailing list";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string list_address = req.list_address.value ?? req.list_address;
        if (!list_address.val()) {
            throw "MAILGUN-ERROR", "list_address is required";
        }

        string address = req.address;
        if (!address.val()) {
            throw "MAILGUN-ERROR", "address is required";
        }

        hash<auto> body = {
            "address": address,
        };

        if (req.name) {
            body.name = req.name;
        }
        if (req.vars) {
%ifdef NoJson
            throw "JSON-ERROR", "the json module is required for this operation";
%else
            body.vars = make_json(req.vars);
%endif
        }
        if (exists req.subscribed) {
            body.subscribed = req.subscribed ? "yes" : "no";
        }
        if (exists req.upsert) {
            body.upsert = req.upsert ? "yes" : "no";
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: adding member to list %s with body: %y",
            list_address, body));

        string path = sprintf("%s/lists/%s/members", ApiV3, list_address);
        hash<auto> response = doPostFormEncoded(path, body);

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: response=%y", response));

        # Return the member data from response
        return response.member ?? response;
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

} # namespace MailgunDataProvider
