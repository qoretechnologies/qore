# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyBackgroundJobDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Instantly background job data type
public class InstantlyBackgroundJobDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Job ID",
                "short_desc": "Job UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the background job",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Job status",
                "type": StringOrNothingType,
                "desc": "The current status of the background job",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Job type",
                "type": StringOrNothingType,
                "desc": "The type of the background job",
            },
            "result": {
                "display_name": "Result",
                "short_desc": "Job result",
                "type": AutoHashOrNothingType,
                "desc": "The result data of the background job",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "type": DateOrNothingType,
                "desc": "The date and time the job was created",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
