# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDeleteDbfsDataProvider class definition

/*  DatabricksDeleteDbfsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DatabricksDataProvider module
public namespace DatabricksDataProvider {

#! Request data type for the delete-dbfs action
public class DatabricksDeleteDbfsRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "path": {
                "type": StringType,
                "display_name": "Path",
                "short_desc": "The DBFS path to delete",
                "desc": "The absolute path of the file or directory to delete in DBFS",
                "required": True,
            },
            "recursive": {
                "type": BoolOrNothingType,
                "display_name": "Recursive",
                "short_desc": "Delete recursively",
                "desc": "Whether to recursively delete the directory and all of its contents; "
                    "required for non-empty directories",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks delete DBFS action data provider
public class DatabricksDeleteDbfsDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-dbfs",
            "desc": "Delete a file or directory from the Databricks File System (DBFS)",
            "type": "DatabricksDeleteDbfsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksDeleteDbfsRequestDataType();

        #! Response type
        const ResponseType = AbstractDataProviderType::get(AutoHashType);
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "delete-dbfs";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Delete a file or directory from the Databricks File System (DBFS)";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return an empty hash on success
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doPost("api/2.0/dbfs/delete", req);
        return {};
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
