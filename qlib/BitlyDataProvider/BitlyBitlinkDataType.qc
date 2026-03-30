# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyBitlinkDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly bitlink record type
public class BitlyBitlinkDataType inherits HashDataType {
    public {
        #! Field definitions for Bitly bitlinks
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Bitlink ID",
                "short_desc": "The bitlink identifier",
                "desc": "The unique identifier of the bitlink (e.g., `bit.ly/abc123`)",
            },
            "link": {
                "type": StringType,
                "display_name": "Short Link",
                "short_desc": "The shortened URL",
                "desc": "The shortened Bitly URL",
            },
            "long_url": {
                "type": StringType,
                "display_name": "Long URL",
                "short_desc": "The destination URL",
                "desc": "The original long URL that the bitlink redirects to",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The link title",
                "desc": "The title or description of the bitlink",
            },
            "archived": {
                "type": BoolType,
                "display_name": "Archived",
                "short_desc": "Whether the link is archived",
                "desc": "Whether the bitlink has been archived",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the bitlink was created",
                "desc": "The ISO 8601 timestamp when the bitlink was created",
            },
            "created_by": {
                "type": StringOrNothingType,
                "display_name": "Created By",
                "short_desc": "Who created the bitlink",
                "desc": "The identifier of the user who created the bitlink",
            },
            "client_id": {
                "type": StringOrNothingType,
                "display_name": "Client ID",
                "short_desc": "The OAuth client ID",
                "desc": "The OAuth client ID used to create the bitlink",
            },
            "tags": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Associated tags",
                "desc": "A list of tags associated with the bitlink",
            },
            "custom_bitlinks": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Custom Bitlinks",
                "short_desc": "Custom back-halves",
                "desc": "A list of custom back-half URLs associated with this bitlink",
            },
            "deeplinks": {
                "type": new ListDataType("deeplinks", new BitlyDeeplinkDataType(
                    AutoHashOrNothingType), True),
                "display_name": "Deeplinks",
                "short_desc": "Mobile deep links",
                "desc": "A list of mobile deep link configurations for iOS and Android",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace BitlyDataProvider
