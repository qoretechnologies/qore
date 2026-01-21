# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunSendEmailDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Data provider for sending emails via Mailgun
public class MailgunSendEmailDataProvider inherits MailgunDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "send-email",
            "desc": "Send an email via Mailgun",
            "type": "MailgunSendEmailDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static MailgunSendEmailRequestDataType RequestType();

        #! Response type
        static MailgunSendEmailResponseDataType ResponseType();
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
        return "send-email";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Send an email via Mailgun";
    }

    #! Makes a request and returns the response
    /** @param req the request data (email fields)
        @param request_options the request options

        @return the response with message ID

        Mailgun expects multipart/form-data for sending emails with potential attachments.
        For simple emails without attachments, we use form-encoded POST.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Get the domain to use
        string domain = api_domain;
        if (!domain) {
            throw "MAILGUN-ERROR", "api_domain is required to send emails";
        }

        # Build the request body
        hash<auto> body = {};

        # Required fields
        body."from" = req."from";
        body."subject" = req.subject;

        # Handle 'to' - can be string or list
        if (req."to".typeCode() == NT_LIST) {
            body."to" = req."to".join(",");
        } else {
            body."to" = req."to";
        }

        # Optional cc/bcc
        if (req.cc) {
            if (req.cc.typeCode() == NT_LIST) {
                body.cc = req.cc.join(",");
            } else {
                body.cc = req.cc;
            }
        }
        if (req.bcc) {
            if (req.bcc.typeCode() == NT_LIST) {
                body.bcc = req.bcc.join(",");
            } else {
                body.bcc = req.bcc;
            }
        }

        # Body - at least one of text or html
        if (req.text) {
            body.text = req.text;
        }
        if (req.html) {
            body.html = req.html;
        }

        # Tracking options
        if (exists req."o:tracking") {
            body."o:tracking" = req."o:tracking" ? "yes" : "no";
        }
        if (exists req."o:tracking-clicks") {
            body."o:tracking-clicks" = req."o:tracking-clicks" ? "yes" : "no";
        }
        if (exists req."o:tracking-opens") {
            body."o:tracking-opens" = req."o:tracking-opens" ? "yes" : "no";
        }

        # Tags
        if (req."o:tag") {
            body."o:tag" = req."o:tag";
        }

        # Reply-To header
        if (req."h:Reply-To") {
            body."h:Reply-To" = req."h:Reply-To";
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: sending email via domain %s with body: %y", domain, body));

        # Send using form-encoded POST (Mailgun's preferred format)
        string path = sprintf("%s/%s/messages", ApiV3, domain);
        hash<auto> response = doPostFormEncoded(path, body);

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
