# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksCreateSqlWarehouseDataProvider class definition

/*  DatabricksCreateSqlWarehouseDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request data type for the create-sql-warehouse action
public class DatabricksCreateSqlWarehouseRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Warehouse Name",
                "short_desc": "The name of the SQL warehouse",
                "desc": "The name of the SQL warehouse to create",
                "required": True,
            },
            "cluster_size": {
                "type": StringType,
                "display_name": "Cluster Size",
                "short_desc": "The size of the warehouse cluster",
                "desc": "The size of the clusters allocated to the warehouse",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "2X-Small", "desc": "Extra extra small cluster"},
                    <AllowedValueInfo>{"value": "X-Small", "desc": "Extra small cluster"},
                    <AllowedValueInfo>{"value": "Small", "desc": "Small cluster"},
                    <AllowedValueInfo>{"value": "Medium", "desc": "Medium cluster"},
                    <AllowedValueInfo>{"value": "Large", "desc": "Large cluster"},
                    <AllowedValueInfo>{"value": "X-Large", "desc": "Extra large cluster"},
                    <AllowedValueInfo>{"value": "2X-Large", "desc": "Extra extra large cluster"},
                    <AllowedValueInfo>{"value": "3X-Large", "desc": "Triple extra large cluster"},
                    <AllowedValueInfo>{"value": "4X-Large", "desc": "Quadruple extra large cluster"},
                ),
            },
            "auto_stop_mins": {
                "type": IntOrNothingType,
                "display_name": "Auto-Stop Minutes",
                "short_desc": "Idle time before auto-stop",
                "desc": "The number of minutes of inactivity after which the warehouse "
                    "will automatically stop",
            },
            "enable_photon": {
                "type": BoolOrNothingType,
                "display_name": "Enable Photon",
                "short_desc": "Enable Photon acceleration",
                "desc": "Whether to enable Photon acceleration for faster query execution",
            },
            "max_num_clusters": {
                "type": IntOrNothingType,
                "display_name": "Max Clusters",
                "short_desc": "Maximum number of clusters",
                "desc": "The maximum number of clusters that can be allocated to the warehouse "
                    "for scaling",
            },
            "warehouse_type": {
                "type": StringOrNothingType,
                "display_name": "Warehouse Type",
                "short_desc": "The type of SQL warehouse",
                "desc": "The type of SQL warehouse to create",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "CLASSIC", "desc": "Classic SQL warehouse"},
                    <AllowedValueInfo>{"value": "PRO", "desc": "Pro SQL warehouse with advanced features"},
                    <AllowedValueInfo>{"value": "TYPE_UNSPECIFIED", "desc": "Unspecified warehouse type"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the create-sql-warehouse action
public class DatabricksCreateSqlWarehouseResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Warehouse ID",
                "short_desc": "The ID of the created warehouse",
                "desc": "The unique identifier of the newly created SQL warehouse",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks create SQL warehouse action data provider
public class DatabricksCreateSqlWarehouseDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-sql-warehouse",
            "desc": "Create a new SQL warehouse in Databricks",
            "type": "DatabricksCreateSqlWarehouseDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksCreateSqlWarehouseRequestDataType();

        #! Response type
        const ResponseType = new DatabricksCreateSqlWarehouseResponseDataType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "create-sql-warehouse";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Create a new SQL warehouse in Databricks";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return the response including the `id` of the created warehouse
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/2.0/sql/warehouses", req);
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
