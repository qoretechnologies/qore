# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksSqlWarehouseDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Databricks SQL warehouse records
public class DatabricksSqlWarehouseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Warehouse ID",
                "short_desc": "Unique identifier for the SQL warehouse",
                "desc": "The unique identifier of the Databricks SQL warehouse",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "SQL warehouse name",
                "desc": "The name of the SQL warehouse",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "Current state of the warehouse",
                "desc": "The current state of the SQL warehouse",
            },
            "size": {
                "type": StringOrNothingType,
                "display_name": "Size",
                "short_desc": "T-shirt size of the warehouse",
                "desc": "The size of the SQL warehouse (e.g., `2X-Small`, `X-Small`, `Small`, "
                    "`Medium`, `Large`)",
            },
            "cluster_size": {
                "type": StringOrNothingType,
                "display_name": "Cluster Size",
                "short_desc": "Cluster size of the warehouse",
                "desc": "The cluster size of the SQL warehouse",
            },
            "auto_stop_mins": {
                "type": IntOrNothingType,
                "display_name": "Auto Stop Minutes",
                "short_desc": "Idle time before auto-stop",
                "desc": "The number of minutes of idle time after which the warehouse will be "
                    "automatically stopped",
            },
            "creator_name": {
                "type": StringOrNothingType,
                "display_name": "Creator Name",
                "short_desc": "User who created the warehouse",
                "desc": "The user name of the creator of the SQL warehouse",
            },
            "num_clusters": {
                "type": IntOrNothingType,
                "display_name": "Number of Clusters",
                "short_desc": "Number of clusters in the warehouse",
                "desc": "The number of clusters allocated to the SQL warehouse",
            },
            "enable_photon": {
                "type": BoolOrNothingType,
                "display_name": "Enable Photon",
                "short_desc": "Whether Photon acceleration is enabled",
                "desc": "Indicates whether Photon query acceleration is enabled for the warehouse",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
