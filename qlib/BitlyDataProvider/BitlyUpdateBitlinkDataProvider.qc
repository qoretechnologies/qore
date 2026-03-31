# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyUpdateBitlinkDataProvider.qc Bitly update bitlink action data provider

/*  BitlyUpdateBitlinkDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a bitlink
public class BitlyUpdateBitlinkRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a bitlink
        const Fields = {
            "bitlink": {
                "type": StringType,
                "display_name": "Bitlink",
                "short_desc": "The bitlink to update",
                "desc": "The bitlink ID to update (e.g., `bit.ly/abc123`)",
                "required": True,
                "ref_data": "bitlinks",
            },
            "long_url": {
                "type": StringOrNothingType,
                "display_name": "Long URL",
                "short_desc": "New destination URL",
                "desc": "The new destination URL for the bitlink",
                "preselected": True,
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "New title",
                "desc": "The new title for the bitlink",
                "preselected": True,
            },
            "archived": {
                "type": SoftBoolOrNothingType,
                "display_name": "Archived",
                "short_desc": "Archive status",
                "desc": "Set to `True` to archive the bitlink or `False` to unarchive it",
            },
            "tags": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "New tags",
                "desc": "A new list of tags for the bitlink (replaces existing tags)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Bitly update bitlink action data provider
public class BitlyUpdateBitlinkDataProvider inherits BitlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing bitlink",
            "type": "BitlyUpdateBitlinkDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing bitlink";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BitlyUpdateBitlinkRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BitlyBitlinkDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string bitlink = remove req.bitlink;
        return doPatch("bitlinks/" + encodeBitlink(bitlink), req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
