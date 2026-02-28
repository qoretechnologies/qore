# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PdfCoSendEmailDataProvider.qc PDF.co send email action data provider

/*  PdfCoSendEmailDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the PdfCoDataProvider module
public namespace PdfCoDataProvider {

#! Request type for sending an email with file attachments via SMTP
public class PdfCoSendEmailRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "type": StringOrNothingType,
                "display_name": "Attachment URL",
                "short_desc": "URL to the file to attach to the email",
                "desc": "URL to the file to attach to the email",
            },
            "from": {
                "type": StringType,
                "display_name": "From",
                "short_desc": "Sender name and email address",
                "desc": "Sender name and email address (e.g., `John Doe <john@example.com>`)",
                "required": True,
                "preselected": True,
            },
            "to": {
                "type": StringType,
                "display_name": "To",
                "short_desc": "Recipient name and email address",
                "desc": "Recipient name and email address (e.g., `Jane Doe <jane@example.com>`)",
                "required": True,
                "preselected": True,
            },
            "subject": {
                "type": StringType,
                "display_name": "Subject",
                "short_desc": "Email subject line",
                "desc": "Email subject line",
                "required": True,
                "preselected": True,
            },
            "smtpserver": {
                "type": StringType,
                "display_name": "SMTP Server",
                "short_desc": "SMTP server address",
                "desc": "SMTP server address (e.g., `smtp.gmail.com`)",
                "required": True,
                "preselected": True,
            },
            "smtpport": {
                "type": SoftIntType,
                "display_name": "SMTP Port",
                "short_desc": "SMTP server port number",
                "desc": "SMTP server port number (e.g., `587` for TLS, `465` for SSL)",
                "required": True,
                "preselected": True,
                "example_value": 587,
            },
            "smtpusername": {
                "type": StringType,
                "display_name": "SMTP Username",
                "short_desc": "SMTP authentication username",
                "desc": "SMTP authentication username.\n\n"
                    "**Note:** This credential is sent to PDF.co servers for email delivery",
                "required": True,
                "sensitive": True,
            },
            "smtppassword": {
                "type": StringType,
                "display_name": "SMTP Password",
                "short_desc": "SMTP authentication password",
                "desc": "SMTP authentication password.\n\n"
                    "**Note:** This credential is sent to PDF.co servers for email delivery",
                "required": True,
                "sensitive": True,
            },
            "bodytext": {
                "type": StringOrNothingType,
                "display_name": "Body Text",
                "short_desc": "Plain text email body",
                "desc": "Plain text email body",
            },
            "bodyhtml": {
                "type": StringOrNothingType,
                "display_name": "Body HTML",
                "short_desc": "HTML-formatted email body",
                "desc": "HTML-formatted email body",
            },
            "async": {
                "type": SoftBoolOrNothingType,
                "display_name": "Async",
                "short_desc": "Send the email as a background job",
                "desc": "Set to `True` to send the email as a background job",
            },
            "profiles": {
                "type": StringOrNothingType,
                "display_name": "Profiles",
                "short_desc": "Advanced processing profiles",
                "desc": "Advanced processing profiles as a JSON string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! PDF.co send email action data provider
public class PdfCoSendEmailDataProvider inherits PdfCoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "send-email",
            "desc": "Sends an email with file attachments via SMTP",
            "type": "PdfCoSendEmailDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(PdfCoRestClient::PdfCoRestClient rest) : PdfCoDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "send-email";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Sends an email with file attachments via SMTP";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new PdfCoSendEmailRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new PdfCoSendEmailResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("email/send", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace PdfCoDataProvider
