# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksSqlStatementDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Databricks SQL statement execution records
public class DatabricksSqlStatementDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "statement_id": {
                "type": StringType,
                "display_name": "Statement ID",
                "short_desc": "Unique identifier for the SQL statement",
                "desc": "The unique identifier of the SQL statement execution",
                "required": True,
            },
            "status": {
                "type": AutoHashOrNothingType,
                "display_name": "Status",
                "short_desc": "Execution status of the statement",
                "desc": "The execution status of the SQL statement, including the `state` field",
            },
            "manifest": {
                "type": AutoHashOrNothingType,
                "display_name": "Manifest",
                "short_desc": "Result set schema information",
                "desc": "The manifest describing the result set, including the `schema` definition",
            },
            "result": {
                "type": AutoHashOrNothingType,
                "display_name": "Result",
                "short_desc": "Query result data",
                "desc": "The result data from the SQL statement execution, including `data_array`",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
