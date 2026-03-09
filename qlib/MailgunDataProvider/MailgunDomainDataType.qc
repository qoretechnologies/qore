# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunDomainDataType.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Mailgun domain record type
public class MailgunDomainDataType inherits HashDataType {
    public {
        #! Field definitions for Mailgun domains
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Domain Name",
                "short_desc": "The domain name",
                "desc": "The domain name for sending emails",
                "example_value": "mg.example.com",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "Domain verification state",
                "desc": "The verification state of the domain (`active`, `unverified`, `disabled`)",
                "example_value": "active",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Domain type",
                "desc": "The type of domain (`custom` or `sandbox`)",
                "example_value": "custom",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the domain was created",
                "desc": "The date and time when the domain was added",
                "example_value": "2025-01-01T09:00:00.000Z",
            },
            "smtp_login": {
                "type": StringOrNothingType,
                "display_name": "SMTP Login",
                "short_desc": "SMTP username",
                "desc": "The SMTP username for this domain",
                "example_value": "postmaster@mg.example.com",
            },
            "smtp_password": {
                "type": StringOrNothingType,
                "display_name": "SMTP Password",
                "short_desc": "SMTP password",
                "desc": "The SMTP password for this domain",
                "sensitive": True,
            },
            "web_prefix": {
                "type": StringOrNothingType,
                "display_name": "Web Prefix",
                "short_desc": "Web prefix for tracking",
                "desc": "The web prefix used for tracking links",
                "example_value": "email",
            },
            "web_scheme": {
                "type": StringOrNothingType,
                "display_name": "Web Scheme",
                "short_desc": "URL scheme for tracking",
                "desc": "The URL scheme used for tracking links (`http` or `https`)",
                "example_value": "https",
            },
            "spam_action": {
                "type": StringOrNothingType,
                "display_name": "Spam Action",
                "short_desc": "Action for spam",
                "desc": "The action taken when spam is detected",
                "example_value": "disabled",
            },
            "wildcard": {
                "type": BoolOrNothingType,
                "display_name": "Wildcard",
                "short_desc": "Wildcard domain",
                "desc": "Whether this is a wildcard domain",
                "example_value": False,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace MailgunDataProvider
