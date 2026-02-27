# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksPipelineDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Databricks Delta Live Tables pipeline records
public class DatabricksPipelineDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "pipeline_id": {
                "type": StringType,
                "display_name": "Pipeline ID",
                "short_desc": "Unique identifier for the pipeline",
                "desc": "The unique identifier of the Delta Live Tables pipeline",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Pipeline name",
                "desc": "The human-readable name of the pipeline",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "Current state of the pipeline",
                "desc": "The current state of the pipeline (e.g., `IDLE`, `RUNNING`, `FAILED`)",
            },
            "creator_user_name": {
                "type": StringOrNothingType,
                "display_name": "Creator User Name",
                "short_desc": "User who created the pipeline",
                "desc": "The user name of the creator of the pipeline",
            },
            "latest_updates": {
                "type": AutoListOrNothingType,
                "display_name": "Latest Updates",
                "short_desc": "Recent pipeline update information",
                "desc": "A list of the most recent updates to the pipeline",
            },
            "cluster_id": {
                "type": StringOrNothingType,
                "display_name": "Cluster ID",
                "short_desc": "Associated cluster identifier",
                "desc": "The identifier of the cluster associated with the pipeline",
            },
            "catalog": {
                "type": StringOrNothingType,
                "display_name": "Catalog",
                "short_desc": "Unity Catalog name",
                "desc": "The name of the Unity Catalog associated with the pipeline",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
