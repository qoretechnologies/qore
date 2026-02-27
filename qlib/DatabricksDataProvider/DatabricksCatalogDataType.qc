# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksCatalogDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Databricks Unity Catalog records
public class DatabricksCatalogDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Catalog name",
                "desc": "The name of the Unity Catalog",
                "required": True,
            },
            "comment": {
                "type": StringOrNothingType,
                "display_name": "Comment",
                "short_desc": "Catalog description",
                "desc": "A user-provided description or comment for the catalog",
            },
            "owner": {
                "type": StringOrNothingType,
                "display_name": "Owner",
                "short_desc": "Catalog owner",
                "desc": "The owner of the catalog",
            },
            "created_at": {
                "type": IntOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The time the catalog was created, in epoch milliseconds",
            },
            "created_by": {
                "type": StringOrNothingType,
                "display_name": "Created By",
                "short_desc": "User who created the catalog",
                "desc": "The user name of the creator of the catalog",
            },
            "metastore_id": {
                "type": StringOrNothingType,
                "display_name": "Metastore ID",
                "short_desc": "Associated metastore identifier",
                "desc": "The unique identifier of the metastore associated with the catalog",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
