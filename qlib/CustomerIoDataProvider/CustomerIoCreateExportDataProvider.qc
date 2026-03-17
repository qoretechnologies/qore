# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoCreateExportDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Request type for creating an export
public class CustomerIoCreateExportRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "export_type": {
                "type": StringType,
                "display_name": "Export Type",
                "short_desc": "Export type",
                "desc": "The type of export to create (customers or deliveries)",
                "required": True,
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "customers", "desc": "Export customers"},
                    <AllowedValueInfo>{"value": "deliveries", "desc": "Export deliveries"},
                ),
            },
            "filters": {
                "type": AutoHashOrNothingType,
                "display_name": "Filters",
                "short_desc": "Export filters",
                "desc": "Filters to apply to the export",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for creating an export
public class CustomerIoCreateExportResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "Export ID",
                "short_desc": "The export job ID",
                "desc": "The unique identifier for the export job",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Export job status",
                "desc": "The status of the export job (e.g., `pending`, `running`, `completed`)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating an export
public class CustomerIoCreateExportDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-export",
            "desc": "Create a Customer.io data export",
            "type": "CustomerIoCreateExportDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoCreateExportRequestDataType RequestType();

        #! Response type
        static CustomerIoCreateExportResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest)
            : CustomerIoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CustomerIoRestClient::CustomerIoRestConnection conn)
            : CustomerIoDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "create-export";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a Customer.io data export";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string export_type = req.export_type;
        *hash<auto> body;
        if (exists req.filters) {
            body.filters = req.filters;
        }
        return doPost(sprintf("/exports/%s", export_type), body);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider
