# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedVerbosityDataProvider class definition

/** MemcachedVerbosityDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

%require-types
%enable-all-warnings
%new-style
%strict-args

public namespace MemcachedDataProvider {
#! Data provider for memcached verbosity command
public class MemcachedVerbosityDataProvider inherits MemcachedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "verbosity",
            "desc": "Memcached verbosity command - sets the server logging verbosity level",
            "type": "MemcachedVerbosityDataProvider",
            "constructor_options": MemcachedDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new MemcachedVerbosityRequestDataType();

        #! Response type
        const ResponseType = new MemcachedVerbosityResponseDataType();
    }

    #! Creates the object from a MemcachedClient
    constructor(MemcachedClient memcached) : MemcachedDataProviderBase(memcached) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : MemcachedDataProviderBase(options) {
    }

    #! Returns the name of the data provider
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns a description of the data provider
    *string getDesc() {
        return sprintf("Memcached verbosity provider for `%s`", memcached.getSafeURL());
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return memcached.verbosity(req.level);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns provider info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request type for verbosity command
public class MemcachedVerbosityRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "level": {
                "type": IntType,
                "display_name": "Level",
                "short_desc": "Verbosity level",
                "desc": "The verbosity level (0 = minimal, 1 = some, 2 = lots)",
                "example_value": 1,
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "desc": "Minimal logging"},
                    <AllowedValueInfo>{"value": 1, "desc": "Some logging"},
                    <AllowedValueInfo>{"value": 2, "desc": "Verbose logging"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for verbosity command
public class MemcachedVerbosityResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "success": {
                "type": BoolType,
                "display_name": "Success",
                "short_desc": "Command succeeded",
                "desc": "True if the verbosity level was set successfully",
                "example_value": True,
            },
            "status": {
                "type": StringType,
                "display_name": "Status",
                "short_desc": "Response status",
                "desc": "Status code from the server",
                "example_value": "OK",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
