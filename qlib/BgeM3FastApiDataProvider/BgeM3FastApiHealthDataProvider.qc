# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BgeM3FastApiDataProvider module definition

/** BgeM3FastApiHealthDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the BgeM3FastApiDataProvider module
public namespace BgeM3FastApiDataProvider {
#! Data type for the host memory pressure block returned by \c /health
public class BgeM3FastApiHealthMemoryDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "total_bytes": {
                "display_name": "Total Memory (bytes)",
                "desc": "Total host memory in bytes.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "available_bytes": {
                "display_name": "Available Memory (bytes)",
                "desc": "Currently available host memory in bytes.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "available_fraction": {
                "display_name": "Available Memory Fraction",
                "desc": "Currently available host memory as a fraction in `[0.0, 1.0]`.",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "pressure": {
                "display_name": "Pressure",
                "desc": "Memory pressure label used by upstream routers to drive backoff.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ok", "display_name": "OK"},
                    <AllowedValueInfo>{"value": "warn", "display_name": "Warn"},
                    <AllowedValueInfo>{"value": "critical", "display_name": "Critical"},
                ),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for BGE-M3 FastAPI \c /health responses
public class BgeM3FastApiHealthResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "status": {
                "display_name": "Status",
                "desc": "Liveness status; typically `\"ok\"`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "memory": {
                "display_name": "Memory Pressure",
                "desc": "Host memory pressure information used by upstream routers to "
                    "drive backoff escalation.",
                "type": new BgeM3FastApiHealthMemoryDataType(),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for the BGE-M3 FastAPI \c /health endpoint
/** No request input.  Returns liveness status and host memory pressure.
*/
public class BgeM3FastApiHealthDataProvider inherits BgeM3FastApiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "health",
            "type": "BgeM3FastApiHealthDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Response type
        static BgeM3FastApiHealthResponseDataType ResponseType =
            new BgeM3FastApiHealthResponseDataType();
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : BgeM3FastApiDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : BgeM3FastApiDataProviderCommon(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        # no request fields
        return;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "/health", NOTHING).body;
    }
}
}
