# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunDeleteMailingListMemberDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Request type for deleting a mailing list member
public class MailgunDeleteMailingListMemberRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "list_address": {
                "type": StringType,
                "display_name": "Mailing List",
                "short_desc": "The mailing list address",
                "desc": "The email address of the mailing list",
                "example_value": "developers@lists.example.com",
                "required": True,
            },
            "member_address": {
                "type": StringType,
                "display_name": "Member Email",
                "short_desc": "Email address to remove",
                "desc": "The email address of the member to remove",
                "example_value": "user@example.com",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for deleting a mailing list member
public class MailgunDeleteMailingListMemberResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "message": {
                "type": StringOrNothingType,
                "display_name": "Message",
                "short_desc": "Status message",
                "desc": "A status message from the API",
            },
            "member": {
                "type": AutoHashOrNothingType,
                "display_name": "Member",
                "short_desc": "Deleted member info",
                "desc": "Information about the deleted member",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for removing a member from a mailing list
public class MailgunDeleteMailingListMemberDataProvider inherits MailgunDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-mailing-list-member",
            "desc": "Remove a member from a Mailgun mailing list",
            "type": "MailgunDeleteMailingListMemberDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static MailgunDeleteMailingListMemberRequestDataType RequestType();

        #! Response type
        static MailgunDeleteMailingListMemberResponseDataType ResponseType();
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
        return "delete-mailing-list-member";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Remove a member from a Mailgun mailing list";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string list_address = req.list_address.value ?? req.list_address;
        if (!list_address.val()) {
            throw "MAILGUN-ERROR", "list_address is required";
        }

        string member_address = req.member_address;
        if (!member_address.val()) {
            throw "MAILGUN-ERROR", "member_address is required";
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: deleting member %s from list %s",
            member_address, list_address));

        string path = sprintf("/lists/%s/members/%s", list_address, member_address);
        hash<auto> response = doDelete(path);

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: response=%y", response));

        return response;
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
