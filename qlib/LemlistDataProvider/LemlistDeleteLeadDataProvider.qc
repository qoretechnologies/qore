# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistDeleteLeadDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Request type for deleting a lead from a campaign
public class LemlistDeleteLeadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "campaignId": {
                "type": StringType,
                "display_name": "Campaign ID",
                "short_desc": "Campaign to remove the lead from",
                "desc": "The identifier of the campaign to remove the lead from",
                "required": True,
                "ref_data": "campaigns",
                "example_value": "cam_abc123",
            },
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "Lead email address to delete",
                "desc": "The email address of the lead to remove from the campaign",
                "required": True,
                "example_value": "john.smith@acme.com",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for deleting a lead
public class LemlistDeleteLeadResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "deleted": {
                "type": SoftBoolOrNothingType,
                "display_name": "Deleted",
                "short_desc": "Whether the lead was deleted",
                "desc": "`True` if the lead was successfully removed from the campaign",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for deleting a lead from a campaign
public class LemlistDeleteLeadDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-lead",
            "desc": "Remove a lead from a Lemlist campaign",
            "type": "LemlistDeleteLeadDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static LemlistDeleteLeadRequestDataType RequestType();

        #! Response type
        static LemlistDeleteLeadResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(LemlistRestClient::LemlistRestClient rest)
            : LemlistDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LemlistRestClient::LemlistRestConnection conn) : LemlistDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "delete-lead";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Remove a lead from a Lemlist campaign";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Preserve @ in email - Lemlist API doesn't decode %40 in path segments
        string encoded_email = replace(encode_url(req.email, True), "%40", "@");
        auto result = doDelete(sprintf("/campaigns/%s/leads/%s", encode_url(req.campaignId, True),
            encoded_email));
        if (!exists result || result.typeCode() == NT_NOTHING) {
            return {"deleted": True};
        }
        return result;
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

} # namespace LemlistDataProvider
