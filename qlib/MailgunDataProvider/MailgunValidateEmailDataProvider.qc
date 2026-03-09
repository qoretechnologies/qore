# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunValidateEmailDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Data provider for validating email addresses via Mailgun
public class MailgunValidateEmailDataProvider inherits MailgunDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "validate-email",
            "desc": "Validate an email address using Mailgun's validation API",
            "type": "MailgunValidateEmailDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static MailgunValidateEmailRequestDataType RequestType();

        #! Response type
        static MailgunValidateEmailResponseDataType ResponseType();
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
        return "validate-email";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Validate an email address using Mailgun's validation API";
    }

    #! Makes a request and returns the response
    /** @param req the request data containing the email address
        @param request_options the request options

        @return the validation result

        Note: Mailgun email validation uses the /v4/address/validate endpoint which
        requires a separate API base URL. This is handled by constructing the full path.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string address = req.address;
        if (!address.val()) {
            throw "MAILGUN-ERROR", "address is required for email validation";
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: validating email address: %s", address));

        # Email validation API uses /v4 endpoint (most other APIs use /v3)
        hash<auto> response = doGet("/v4/address/validate", {"address": address});

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: validation response=%y", response));

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
