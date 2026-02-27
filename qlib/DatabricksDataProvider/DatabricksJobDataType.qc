# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksJobDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Databricks job records
public class DatabricksJobDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "job_id": {
                "type": IntType,
                "display_name": "Job ID",
                "short_desc": "Unique identifier for the job",
                "desc": "The unique identifier of the Databricks job",
                "required": True,
            },
            "settings": {
                "type": AutoHashOrNothingType,
                "display_name": "Settings",
                "short_desc": "Job configuration settings",
                "desc": "The settings for the job, including `name`, `tasks`, `schedule`, and other "
                    "configuration options",
            },
            "created_time": {
                "type": IntOrNothingType,
                "display_name": "Created Time",
                "short_desc": "Job creation timestamp",
                "desc": "The time the job was created, in epoch milliseconds",
            },
            "creator_user_name": {
                "type": StringOrNothingType,
                "display_name": "Creator User Name",
                "short_desc": "User who created the job",
                "desc": "The user name of the creator of the job",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
