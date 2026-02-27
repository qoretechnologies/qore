# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksDbfsFileDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Databricks DBFS file records
public class DatabricksDbfsFileDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "path": {
                "type": StringType,
                "display_name": "Path",
                "short_desc": "DBFS file or directory path",
                "desc": "The absolute path of the file or directory in DBFS",
                "required": True,
            },
            "is_dir": {
                "type": BoolOrNothingType,
                "display_name": "Is Directory",
                "short_desc": "Whether the path is a directory",
                "desc": "Indicates whether the path refers to a directory (`True`) or a file "
                    "(`False`)",
            },
            "file_size": {
                "type": IntOrNothingType,
                "display_name": "File Size",
                "short_desc": "Size of the file in bytes",
                "desc": "The size of the file in bytes; `0` for directories",
            },
            "modification_time": {
                "type": IntOrNothingType,
                "display_name": "Modification Time",
                "short_desc": "Last modification timestamp",
                "desc": "The last modification time of the file, in epoch milliseconds",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
