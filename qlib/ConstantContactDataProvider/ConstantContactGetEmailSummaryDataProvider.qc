# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactGetEmailSummaryDataProvider.qc Constant Contact get email summary action data provider

/*  ConstantContactGetEmailSummaryDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConstantContactDataProvider module
public namespace ConstantContactDataProvider {

#! Request type for getting email campaign summaries
public class ConstantContactGetEmailSummaryRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "campaign_ids": {
                "type": StringOrNothingType,
                "display_name": "Campaign IDs",
                "short_desc": "Comma-separated campaign IDs",
                "desc": "A comma-separated list of email campaign IDs to get summaries for",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact get email summary action data provider
public class ConstantContactGetEmailSummaryDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-email-summary",
            "desc": "Get email campaign summary reports from Constant Contact",
            "type": "ConstantContactGetEmailSummaryDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Static request type
        static ConstantContactGetEmailSummaryRequestDataType RequestType();

        #! Static response type
        static ConstantContactEmailCampaignSummaryDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ConstantContactRestClient::ConstantContactRestClient rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "get-email-summary";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Get email campaign summary reports from Constant Contact";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConstantContactGetEmailSummaryRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ConstantContactEmailCampaignSummaryDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> params;
        if (req.campaign_ids) {
            params.campaign_ids = req.campaign_ids;
        }
        return doGet("reports/summary_reports/email_campaign_summaries", params);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider
