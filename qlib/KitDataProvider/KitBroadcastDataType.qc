# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KitDataProvider module definition

/*  KitBroadcastDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Kit broadcast record type
public class KitBroadcastDataType inherits HashDataType {
    public {
        #! Field definitions for Kit broadcasts
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "ID",
                "short_desc": "The broadcast ID",
                "desc": "The unique identifier of the broadcast",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "The broadcast subject",
                "desc": "The subject line of the broadcast email",
            },
            "content": {
                "type": StringOrNothingType,
                "display_name": "Content",
                "short_desc": "The broadcast content",
                "desc": "The HTML content of the broadcast email",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The broadcast description",
                "desc": "A description of the broadcast",
            },
            "public": {
                "type": SoftBoolOrNothingType,
                "display_name": "Public",
                "short_desc": "Whether the broadcast is public",
                "desc": "Indicates whether the broadcast is publicly accessible",
            },
            "published_at": {
                "type": DateOrNothingType,
                "display_name": "Published At",
                "short_desc": "When the broadcast was published",
                "desc": "The timestamp when the broadcast was published",
            },
            "send_at": {
                "type": DateOrNothingType,
                "display_name": "Send At",
                "short_desc": "When the broadcast is scheduled to send",
                "desc": "The timestamp when the broadcast is scheduled to be sent",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the broadcast was created",
                "desc": "The timestamp when the broadcast was created",
            },
            "email_template": {
                "type": AutoHashOrNothingType,
                "display_name": "Email Template",
                "short_desc": "The email template",
                "desc": "The email template object used for the broadcast",
            },
            "public_url": {
                "type": StringOrNothingType,
                "display_name": "Public URL",
                "short_desc": "The public URL",
                "desc": "The public URL of the broadcast",
            },
            "thumbnail_alt": {
                "type": StringOrNothingType,
                "display_name": "Thumbnail Alt",
                "short_desc": "The thumbnail alt text",
                "desc": "The alternative text for the broadcast thumbnail image",
            },
            "thumbnail_url": {
                "type": StringOrNothingType,
                "display_name": "Thumbnail URL",
                "short_desc": "The thumbnail URL",
                "desc": "The URL of the broadcast thumbnail image",
            },
            "preview_text": {
                "type": StringOrNothingType,
                "display_name": "Preview Text",
                "short_desc": "The preview text",
                "desc": "The preview text shown in email clients before the email is opened",
            },
            "email_address": {
                "type": StringOrNothingType,
                "display_name": "Email Address",
                "short_desc": "The sender email address",
                "desc": "The email address used as the sender for the broadcast",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KitDataProvider
