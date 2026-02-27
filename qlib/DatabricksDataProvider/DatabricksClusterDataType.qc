# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksClusterDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Databricks cluster records
public class DatabricksClusterDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "cluster_id": {
                "type": StringType,
                "display_name": "Cluster ID",
                "short_desc": "Unique identifier for the cluster",
                "desc": "The unique identifier of the Databricks cluster",
                "required": True,
            },
            "cluster_name": {
                "type": StringOrNothingType,
                "display_name": "Cluster Name",
                "short_desc": "Human-readable cluster name",
                "desc": "The human-readable name of the cluster",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "Current state of the cluster",
                "desc": "The current state of the cluster",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "PENDING",
                        "display_name": "Pending",
                        "desc": "Cluster is being prepared",
                    },
                    <AllowedValueInfo>{
                        "value": "RUNNING",
                        "display_name": "Running",
                        "desc": "Cluster is running and ready for use",
                    },
                    <AllowedValueInfo>{
                        "value": "RESTARTING",
                        "display_name": "Restarting",
                        "desc": "Cluster is being restarted",
                    },
                    <AllowedValueInfo>{
                        "value": "RESIZING",
                        "display_name": "Resizing",
                        "desc": "Cluster is being resized",
                    },
                    <AllowedValueInfo>{
                        "value": "TERMINATING",
                        "display_name": "Terminating",
                        "desc": "Cluster is being terminated",
                    },
                    <AllowedValueInfo>{
                        "value": "TERMINATED",
                        "display_name": "Terminated",
                        "desc": "Cluster has been terminated",
                    },
                    <AllowedValueInfo>{
                        "value": "ERROR",
                        "display_name": "Error",
                        "desc": "Cluster is in an error state",
                    },
                    <AllowedValueInfo>{
                        "value": "UNKNOWN",
                        "display_name": "Unknown",
                        "desc": "Cluster state is unknown",
                    },
                ),
            },
            "spark_version": {
                "type": StringOrNothingType,
                "display_name": "Spark Version",
                "short_desc": "Spark runtime version",
                "desc": "The Spark runtime version of the cluster",
            },
            "node_type_id": {
                "type": StringOrNothingType,
                "display_name": "Node Type ID",
                "short_desc": "Instance type for worker nodes",
                "desc": "The node type identifier for the worker nodes in the cluster",
            },
            "num_workers": {
                "type": IntOrNothingType,
                "display_name": "Number of Workers",
                "short_desc": "Number of worker nodes",
                "desc": "The number of worker nodes in the cluster",
            },
            "autoscale": {
                "type": AutoHashOrNothingType,
                "display_name": "Autoscale",
                "short_desc": "Autoscaling configuration",
                "desc": "The autoscaling configuration for the cluster, including `min_workers` and "
                    "`max_workers`",
            },
            "driver": {
                "type": AutoHashOrNothingType,
                "display_name": "Driver",
                "short_desc": "Driver node information",
                "desc": "Information about the driver node, including `host_private_ip` and "
                    "`node_id`",
            },
            "start_time": {
                "type": IntOrNothingType,
                "display_name": "Start Time",
                "short_desc": "Cluster start timestamp",
                "desc": "The time the cluster was started, in epoch milliseconds",
            },
            "state_message": {
                "type": StringOrNothingType,
                "display_name": "State Message",
                "short_desc": "Message about the current state",
                "desc": "A message associated with the current state of the cluster",
            },
            "autotermination_minutes": {
                "type": IntOrNothingType,
                "display_name": "Auto-Termination Minutes",
                "short_desc": "Idle time before auto-termination",
                "desc": "The number of minutes of idle time after which the cluster will be "
                    "automatically terminated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
