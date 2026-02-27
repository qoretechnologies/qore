# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksExecuteSqlDataProvider class definition

/*  DatabricksExecuteSqlDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request data type for the execute-sql action
public class DatabricksExecuteSqlRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "warehouse_id": {
                "type": StringType,
                "display_name": "Warehouse ID",
                "short_desc": "The SQL warehouse to execute on",
                "desc": "The unique identifier of the SQL warehouse to execute the statement on",
                "required": True,
            },
            "statement": {
                "type": StringType,
                "display_name": "SQL Statement",
                "short_desc": "The SQL statement to execute",
                "desc": "The SQL statement string to execute",
                "required": True,
            },
            "catalog": {
                "type": StringOrNothingType,
                "display_name": "Catalog",
                "short_desc": "The catalog to use",
                "desc": "The name of the catalog to use for the SQL statement execution",
            },
            "schema": {
                "type": StringOrNothingType,
                "display_name": "Schema",
                "short_desc": "The schema to use",
                "desc": "The name of the schema to use for the SQL statement execution",
            },
            "wait_timeout": {
                "type": StringOrNothingType,
                "display_name": "Wait Timeout",
                "short_desc": "Timeout for synchronous execution",
                "desc": "The timeout for waiting for the statement to complete "
                    "(e.g., `10s`, `5m`); `0s` for asynchronous execution",
            },
            "on_wait_timeout": {
                "type": StringOrNothingType,
                "display_name": "On Wait Timeout",
                "short_desc": "Action when wait timeout expires",
                "desc": "The action to take when the wait timeout expires",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "CONTINUE", "desc": "Continue with asynchronous execution"},
                    <AllowedValueInfo>{"value": "CANCEL", "desc": "Cancel the statement execution"},
                ),
            },
            "disposition": {
                "type": StringOrNothingType,
                "display_name": "Disposition",
                "short_desc": "Result disposition mode",
                "desc": "The disposition for the result set: `INLINE` returns results directly, "
                    "`EXTERNAL_LINKS` provides external links to the data",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "INLINE", "desc": "Return results directly in the response"},
                    <AllowedValueInfo>{"value": "EXTERNAL_LINKS", "desc": "Provide external links to result data"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks execute SQL action data provider
public class DatabricksExecuteSqlDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "execute-sql",
            "desc": "Execute a SQL statement on a Databricks SQL warehouse",
            "type": "DatabricksExecuteSqlDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksExecuteSqlRequestDataType();

        #! Response type
        const ResponseType = new DatabricksSqlStatementDataType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "execute-sql";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Execute a SQL statement on a Databricks SQL warehouse";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return the SQL statement execution response with statement ID, status, manifest, and result
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/2.0/sql/statements", req);
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
