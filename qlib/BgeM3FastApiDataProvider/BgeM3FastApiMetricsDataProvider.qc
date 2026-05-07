# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BgeM3FastApiDataProvider module definition

/** BgeM3FastApiMetricsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Data type for BGE-M3 FastAPI \c /metrics responses
/** The endpoint emits Prometheus text-format metrics; the response body is
    surfaced as a single string under \c text.
*/
public class BgeM3FastApiMetricsResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "text": {
                "display_name": "Metrics",
                "desc": "Prometheus text-exposition-format metrics from the sidecar.  "
                    "Exposes host memory pressure as gauges.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for the BGE-M3 FastAPI \c /metrics endpoint
/** No request input.  Returns the Prometheus text-format metrics body in
    the \c text field of a wrapping hash.
*/
public class BgeM3FastApiMetricsDataProvider inherits BgeM3FastApiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "metrics",
            "type": "BgeM3FastApiMetricsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Response type
        static BgeM3FastApiMetricsResponseDataType ResponseType =
            new BgeM3FastApiMetricsResponseDataType();
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
    /** Wraps the raw Prometheus text body in a hash with a single \c text key
        so the typed response shape stays stable for downstream consumers.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doRestCommand("GET", "/metrics", NOTHING, {"Accept": "text/plain"});
        return {"text": resp.body};
    }
}
}
