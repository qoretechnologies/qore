# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyBitlinkClicksDataProvider.qc Bitly bitlink clicks action data provider

/*  BitlyBitlinkClicksDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for getting bitlink click summary
public class BitlyBitlinkClicksRequestDataType inherits HashDataType {
    public {
        #! Field definitions for click summary request
        const Fields = {
            "bitlink": {
                "type": StringType,
                "display_name": "Bitlink",
                "short_desc": "The bitlink to get clicks for",
                "desc": "The bitlink ID to retrieve click summary for (e.g., `bit.ly/abc123`)",
                "required": True,
                "ref_data": "bitlinks",
            },
            "unit": {
                "type": StringOrNothingType,
                "display_name": "Time Unit",
                "short_desc": "The time unit for aggregation",
                "desc": "The time unit to use for aggregating click data",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "minute", "display_name": "Minute"},
                    <AllowedValueInfo>{"value": "hour", "display_name": "Hour"},
                    <AllowedValueInfo>{"value": "day", "display_name": "Day (default)"},
                    <AllowedValueInfo>{"value": "week", "display_name": "Week"},
                    <AllowedValueInfo>{"value": "month", "display_name": "Month"},
                ),
                "preselected": True,
            },
            "units": {
                "type": SoftIntOrNothingType,
                "display_name": "Number of Units",
                "short_desc": "How many time units to query",
                "desc": "The number of time units to query. Use `-1` for all available data",
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Bitly bitlink clicks action data provider
public class BitlyBitlinkClicksDataProvider inherits BitlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "clicks",
            "desc": "Get click summary for a bitlink",
            "type": "BitlyBitlinkClicksDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    string getName() {
        return "clicks";
    }

    *string getDesc() {
        return "Get click summary for a bitlink";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BitlyBitlinkClicksRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BitlyBitlinkClicksDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string bitlink = remove req.bitlink;
        hash<auto> params;
        if (exists req.unit) {
            params.unit = req.unit;
        }
        if (exists req.units) {
            params.units = req.units;
        }
        return doGet("bitlinks/" + encodeBitlink(bitlink) + "/clicks/summary", params);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
