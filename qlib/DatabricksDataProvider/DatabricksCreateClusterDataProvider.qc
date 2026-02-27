# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksCreateClusterDataProvider class definition

/*  DatabricksCreateClusterDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request data type for the create-cluster action
public class DatabricksCreateClusterRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "cluster_name": {
                "type": StringType,
                "display_name": "Cluster Name",
                "short_desc": "The name of the cluster",
                "desc": "The name of the cluster to create",
                "required": True,
            },
            "spark_version": {
                "type": StringType,
                "display_name": "Spark Version",
                "short_desc": "The Spark runtime version",
                "desc": "The Databricks runtime version for the cluster (e.g., `13.3.x-scala2.12`)",
                "required": True,
            },
            "node_type_id": {
                "type": StringType,
                "display_name": "Node Type ID",
                "short_desc": "The node type for worker nodes",
                "desc": "The node type identifier for worker and driver nodes "
                    "(e.g., `Standard_DS3_v2` or `i3.xlarge`)",
                "required": True,
            },
            "num_workers": {
                "type": IntOrNothingType,
                "display_name": "Number of Workers",
                "short_desc": "Fixed number of worker nodes",
                "desc": "The fixed number of worker nodes for the cluster; mutually exclusive "
                    "with `autoscale`",
            },
            "autoscale": {
                "type": AutoHashOrNothingType,
                "display_name": "Autoscale",
                "short_desc": "Autoscaling configuration",
                "desc": "Autoscaling configuration with `min_workers` and `max_workers` fields; "
                    "mutually exclusive with `num_workers`",
            },
            "autotermination_minutes": {
                "type": IntOrNothingType,
                "display_name": "Auto-Termination Minutes",
                "short_desc": "Idle time before auto-termination",
                "desc": "The number of minutes of inactivity after which the cluster will "
                    "automatically terminate",
            },
            "driver_node_type_id": {
                "type": StringOrNothingType,
                "display_name": "Driver Node Type ID",
                "short_desc": "The node type for the driver node",
                "desc": "The node type identifier for the driver node; defaults to the same "
                    "value as `node_type_id` if not specified",
            },
            "spark_conf": {
                "type": AutoHashOrNothingType,
                "display_name": "Spark Config",
                "short_desc": "Custom Spark configuration key-value pairs",
                "desc": "A map of key-value pairs for custom Spark configuration properties",
            },
            "aws_attributes": {
                "type": AutoHashOrNothingType,
                "display_name": "AWS Attributes",
                "short_desc": "AWS-specific cluster attributes",
                "desc": "AWS-specific attributes for the cluster such as availability zone, "
                    "instance profile ARN, and EBS volume configuration",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the create-cluster action
public class DatabricksCreateClusterResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "cluster_id": {
                "type": StringType,
                "display_name": "Cluster ID",
                "short_desc": "The ID of the created cluster",
                "desc": "The unique identifier of the newly created cluster",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks create cluster action data provider
public class DatabricksCreateClusterDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-cluster",
            "desc": "Create a new cluster in the Databricks workspace",
            "type": "DatabricksCreateClusterDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksCreateClusterRequestDataType();

        #! Response type
        const ResponseType = new DatabricksCreateClusterResponseDataType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "create-cluster";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Create a new cluster in the Databricks workspace";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return the response including the `cluster_id` of the created cluster
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/2.0/clusters/create", req);
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
