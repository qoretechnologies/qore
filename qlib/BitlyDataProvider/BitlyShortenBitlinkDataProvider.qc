# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyShortenBitlinkDataProvider.qc Bitly shorten bitlink action data provider

/*  BitlyShortenBitlinkDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for shortening a URL
public class BitlyShortenBitlinkRequestDataType inherits HashDataType {
    public {
        #! Field definitions for shortening a URL
        const Fields = {
            "long_url": {
                "type": StringType,
                "display_name": "Long URL",
                "short_desc": "The URL to shorten",
                "desc": "The URL to be shortened",
                "required": True,
                "example_value": "https://example.com/my-long-page-url",
            },
            "domain": {
                "type": StringOrNothingType,
                "display_name": "Domain",
                "short_desc": "Custom short domain",
                "desc": "The short domain to use for the link. Defaults to `bit.ly` if not "
                    "specified. Can also use a branded short domain (BSD) configured on "
                    "the account",
                "example_value": "bit.ly",
                "preselected": True,
            },
            "group_guid": {
                "type": StringOrNothingType,
                "display_name": "Group GUID",
                "short_desc": "Group to create the link in",
                "desc": "The GUID of the group to create the bitlink in. "
                    "Uses the default group if not specified",
                "ref_data": "groups",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Bitly shorten bitlink action data provider
public class BitlyShortenBitlinkDataProvider inherits BitlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "shorten",
            "desc": "Shorten a URL to create a new bitlink",
            "type": "BitlyShortenBitlinkDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    string getName() {
        return "shorten";
    }

    *string getDesc() {
        return "Shorten a URL to create a new bitlink";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BitlyShortenBitlinkRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BitlyBitlinkDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("shorten", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
