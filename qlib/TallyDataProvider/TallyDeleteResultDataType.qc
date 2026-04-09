# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyDeleteResultDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Delete result data type for submission deletions
public class TallyDeleteSubmissionResultDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "Deletion status",
                "desc": "The result status of the deletion operation",
                "type": AbstractDataProviderTypeMap."string",
            },
            "form_id": {
                "display_name": "Form ID",
                "short_desc": "The form ID",
                "desc": "The ID of the form containing the deleted submission",
                "type": AbstractDataProviderTypeMap."string",
            },
            "submission_id": {
                "display_name": "Submission ID",
                "short_desc": "The deleted submission ID",
                "desc": "The ID of the deleted submission",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const TallyDeleteSubmissionResultDataType = new TallyDeleteSubmissionResultDataType();

#! Delete result data type for webhook deletions
public class TallyDeleteWebhookResultDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "Deletion status",
                "desc": "The result status of the deletion operation",
                "type": AbstractDataProviderTypeMap."string",
            },
            "id": {
                "display_name": "Webhook ID",
                "short_desc": "The deleted webhook ID",
                "desc": "The ID of the deleted webhook",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const TallyDeleteWebhookResultDataType = new TallyDeleteWebhookResultDataType();

#! Delete result data type for form deletions
public class TallyDeleteFormResultDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "Deletion status",
                "desc": "The result status of the deletion operation",
                "type": AbstractDataProviderTypeMap."string",
            },
            "id": {
                "display_name": "Form ID",
                "short_desc": "The deleted form ID",
                "desc": "The ID of the deleted form",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const TallyDeleteFormResultDataType = new TallyDeleteFormResultDataType();

#! Update result data type for webhook updates (API returns 204)
public class TallyUpdateWebhookResultDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "Update status",
                "desc": "The result status of the update operation",
                "type": AbstractDataProviderTypeMap."string",
            },
            "id": {
                "display_name": "Webhook ID",
                "short_desc": "The updated webhook ID",
                "desc": "The ID of the updated webhook",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const TallyUpdateWebhookResultDataType = new TallyUpdateWebhookResultDataType();
}
