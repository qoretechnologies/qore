# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksStartSqlWarehouseDataProvider class definition

/*  DatabricksStartSqlWarehouseDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request data type for the start-sql-warehouse action
public class DatabricksStartSqlWarehouseRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Warehouse ID",
                "short_desc": "The ID of the SQL warehouse to start",
                "desc": "The unique identifier of the SQL warehouse to start",
                "required": True,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks start SQL warehouse action data provider
public class DatabricksStartSqlWarehouseDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "start-sql-warehouse",
            "desc": "Start a stopped Databricks SQL warehouse",
            "type": "DatabricksStartSqlWarehouseDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksStartSqlWarehouseRequestDataType();

        #! Response type
        const ResponseType = AbstractDataProviderType::get(AutoHashType);
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "start-sql-warehouse";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Start a stopped Databricks SQL warehouse";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return an empty hash on success
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doPost(sprintf("api/2.0/sql/warehouses/%s/start", req.id));
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
