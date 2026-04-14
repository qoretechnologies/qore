# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbouncePageDataType.qc Unbounce page data type

/*  UnbouncePageDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! Unbounce page data type
public class UnbouncePageDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Page unique identifier",
                "desc": "The unique identifier of the page",
                "type": SoftStringType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Page name",
                "desc": "The name of the page",
                "type": SoftStringType,
            },
            "url": {
                "display_name": "URL",
                "short_desc": "Page URL",
                "desc": "The URL of the page",
                "type": SoftStringOrNothingType,
            },
            "domain": {
                "display_name": "Domain",
                "short_desc": "Page domain",
                "desc": "The domain the page is published on",
                "type": SoftStringOrNothingType,
            },
            "state": {
                "display_name": "State",
                "short_desc": "Page state",
                "desc": "The current publication state of the page",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "published", "desc": "Page is published and live"},
                    <AllowedValueInfo>{"value": "unpublished", "desc": "Page is unpublished"},
                ),
            },
            "sub_account_id": {
                "display_name": "Sub Account ID",
                "short_desc": "Sub-account ID",
                "desc": "The ID of the sub-account this page belongs to",
                "type": SoftStringOrNothingType,
            },
            "variants_count": {
                "display_name": "Variants Count",
                "short_desc": "Number of variants",
                "desc": "The number of A/B test variants for this page",
                "type": SoftIntOrNothingType,
            },
            "integrations_count": {
                "display_name": "Integrations Count",
                "short_desc": "Number of integrations",
                "desc": "The number of integrations configured for this page",
                "type": SoftIntOrNothingType,
            },
            "integrations_errors_count": {
                "display_name": "Integrations Errors Count",
                "short_desc": "Number of integration errors",
                "desc": "The number of integration errors for this page",
                "type": SoftIntOrNothingType,
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "When the page was created",
                "type": SoftDateOrNothingType,
            },
            "last_published_at": {
                "display_name": "Last Published At",
                "short_desc": "Last publish timestamp",
                "desc": "When the page was last published",
                "type": SoftDateOrNothingType,
            },
            "integrations": {
                "display_name": "Integrations",
                "short_desc": "Page integrations",
                "desc": "List of integrations configured for this page",
                "type": new ListDataType(new UnbouncePageIntegrationDataType(), True),
            },
        };
    }

    constructor() : HashDataType("UnbouncePageDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnbounceDataProvider
