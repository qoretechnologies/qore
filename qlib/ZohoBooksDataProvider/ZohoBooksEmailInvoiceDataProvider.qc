# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksEmailInvoiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoBooksDataProvider module
public namespace ZohoBooksDataProvider {

#! Zoho Books email invoice data provider
public class ZohoBooksEmailInvoiceDataProvider inherits ZohoBooksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "email-invoice",
            "desc": "Sends an invoice by email to the customer",
            "type": "ZohoBooksEmailInvoiceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "invoice_id": <DataProviderOptionInfo>{
                "display_name": "Invoice ID",
                "short_desc": "The invoice to email",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the invoice to email",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("EmailInvoiceRequest", {
            "invoice_id": {
                "display_name": "Invoice ID",
                "type": SoftStringType,
                "desc": "The ID of the invoice to email",
            },
            "to_mail_ids": {
                "display_name": "To",
                "type": SoftListOrNothingType,
                "desc": "Email addresses to send the invoice to (comma-separated or list)",
            },
            "cc_mail_ids": {
                "display_name": "CC",
                "type": SoftListOrNothingType,
                "desc": "CC email addresses",
            },
            "subject": {
                "display_name": "Subject",
                "type": SoftStringOrNothingType,
                "desc": "Email subject line",
            },
            "body": {
                "display_name": "Body",
                "type": SoftStringOrNothingType,
                "desc": "Email body content",
            },
            "send_from_org_email_id": {
                "display_name": "Send From Org Email",
                "type": SoftBoolOrNothingType,
                "desc": "Whether to send from organization email",
            },
        });

        #! Response type
        const ResponseType = new HashDataType("EmailInvoiceResponse", {
            "code": {
                "display_name": "Code",
                "type": SoftIntType,
                "desc": "Response code (0 for success)",
            },
            "message": {
                "display_name": "Message",
                "type": SoftStringType,
                "desc": "Response message",
            },
        });
    }

    private {
        #! The invoice ID
        string invoice_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest, *hash<auto> options) : ZohoBooksDataProviderBase(rest) {
        if (options.invoice_id) {
            invoice_id = options.invoice_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "email-invoice";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Sends an invoice by email to the customer";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.invoice_id ?? request_options.invoice_id ?? invoice_id;
        if (!id) {
            throw "ZOHOBOOKS-ERROR", "invoice_id is required for email operation";
        }

        # Build email request body
        hash<auto> body;
        if (req.to_mail_ids) {
            body.to_mail_ids = req.to_mail_ids;
        }
        if (req.cc_mail_ids) {
            body.cc_mail_ids = req.cc_mail_ids;
        }
        if (req.subject) {
            body.subject = req.subject;
        }
        if (req.body) {
            body.body = req.body;
        }
        if (req.send_from_org_email_id.val()) {
            body.send_from_org_email_id = req.send_from_org_email_id;
        }

        return doPost(sprintf("/invoices/%s/email", id), body);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoBooksDataProvider
