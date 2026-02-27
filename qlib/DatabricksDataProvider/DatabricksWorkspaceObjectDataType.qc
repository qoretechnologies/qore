# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksWorkspaceObjectDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Databricks workspace object records
public class DatabricksWorkspaceObjectDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "object_id": {
                "type": IntOrNothingType,
                "display_name": "Object ID",
                "short_desc": "Unique identifier for the workspace object",
                "desc": "The unique identifier of the workspace object",
            },
            "object_type": {
                "type": StringOrNothingType,
                "display_name": "Object Type",
                "short_desc": "Type of the workspace object",
                "desc": "The type of the workspace object",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "NOTEBOOK",
                        "display_name": "Notebook",
                        "desc": "A Databricks notebook",
                    },
                    <AllowedValueInfo>{
                        "value": "DIRECTORY",
                        "display_name": "Directory",
                        "desc": "A workspace directory",
                    },
                    <AllowedValueInfo>{
                        "value": "LIBRARY",
                        "display_name": "Library",
                        "desc": "A library object",
                    },
                    <AllowedValueInfo>{
                        "value": "FILE",
                        "display_name": "File",
                        "desc": "A workspace file",
                    },
                    <AllowedValueInfo>{
                        "value": "REPO",
                        "display_name": "Repo",
                        "desc": "A Git repository",
                    },
                    <AllowedValueInfo>{
                        "value": "DASHBOARD",
                        "display_name": "Dashboard",
                        "desc": "A Databricks dashboard",
                    },
                ),
            },
            "path": {
                "type": StringType,
                "display_name": "Path",
                "short_desc": "Workspace path of the object",
                "desc": "The absolute path of the object in the workspace",
                "required": True,
            },
            "language": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "Programming language of the notebook",
                "desc": "The programming language of the notebook (e.g., `PYTHON`, `SQL`, "
                    "`SCALA`, `R`)",
            },
            "modified_at": {
                "type": IntOrNothingType,
                "display_name": "Modified At",
                "short_desc": "Last modification timestamp",
                "desc": "The last modification time of the object, in epoch milliseconds",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
