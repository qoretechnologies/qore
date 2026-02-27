# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksJobRunDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Databricks job run records
public class DatabricksJobRunDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "run_id": {
                "type": IntType,
                "display_name": "Run ID",
                "short_desc": "Unique identifier for the run",
                "desc": "The unique identifier of the job run",
                "required": True,
            },
            "job_id": {
                "type": IntType,
                "display_name": "Job ID",
                "short_desc": "Identifier of the parent job",
                "desc": "The unique identifier of the job that this run belongs to",
            },
            "state": {
                "type": AutoHashOrNothingType,
                "display_name": "State",
                "short_desc": "Current state of the run",
                "desc": "The state of the run, including `life_cycle_state`, `result_state`, and "
                    "`state_message`",
            },
            "start_time": {
                "type": IntOrNothingType,
                "display_name": "Start Time",
                "short_desc": "Run start timestamp",
                "desc": "The time the run started, in epoch milliseconds",
            },
            "end_time": {
                "type": IntOrNothingType,
                "display_name": "End Time",
                "short_desc": "Run end timestamp",
                "desc": "The time the run ended, in epoch milliseconds",
            },
            "run_name": {
                "type": StringOrNothingType,
                "display_name": "Run Name",
                "short_desc": "Human-readable name for the run",
                "desc": "An optional human-readable name for the run",
            },
            "run_type": {
                "type": StringOrNothingType,
                "display_name": "Run Type",
                "short_desc": "Type of the run",
                "desc": "The type of the run (e.g., `JOB_RUN`, `WORKFLOW_RUN`, `SUBMIT_RUN`)",
            },
            "setup_duration": {
                "type": IntOrNothingType,
                "display_name": "Setup Duration",
                "short_desc": "Time spent setting up the run",
                "desc": "The time spent setting up the cluster for the run, in milliseconds",
            },
            "execution_duration": {
                "type": IntOrNothingType,
                "display_name": "Execution Duration",
                "short_desc": "Time spent executing the run",
                "desc": "The time spent executing the run commands, in milliseconds",
            },
            "cleanup_duration": {
                "type": IntOrNothingType,
                "display_name": "Cleanup Duration",
                "short_desc": "Time spent cleaning up after the run",
                "desc": "The time spent cleaning up the cluster after the run, in milliseconds",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
