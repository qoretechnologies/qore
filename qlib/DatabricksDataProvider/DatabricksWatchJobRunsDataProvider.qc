# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksWatchJobRunsDataProvider class definition

/*  DatabricksWatchJobRunsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Watch for new Databricks job runs
/** Polls the Databricks Jobs API for new job runs and emits events for each new run detected.
    Deduplicates by run_id.
*/
public class DatabricksWatchJobRunsDataProvider inherits DatabricksWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-job-runs",
            "desc": "Watch for new Databricks job runs",
            "type": "DatabricksWatchJobRunsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions + {
                "start_time_ms": <DataProviderOptionInfo>{
                    "display_name": "Start Time (ms)",
                    "short_desc": "Only detect runs started after this time",
                    "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                    "desc": "Only detect runs started after this Unix timestamp in milliseconds; "
                        "defaults to current time at startup",
                },
            },
        };
    }

    private {
        #! Start time in milliseconds for filtering runs
        int start_time_ms;
    }

    #! Creates the object
    constructor(DatabricksRestClient::DatabricksRestClient rest, *hash<auto> options)
            : DatabricksWatchDataProviderBase(rest, options) {
        start_time_ms = options.start_time_ms ?? (now_us().getEpochSeconds() * 1000);
    }

    #! Returns the name of this data provider
    string getName() {
        return "watch-job-runs";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Polls for new Databricks job runs and emits events for each new run detected";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Polls for new job runs with pagination
    private pollOnce() {
        try {
            int new_start_time = start_time_ms;
            bool has_more = True;
            *string page_token;

            while (has_more) {
                if (stop) {
                    break;
                }

                hash<auto> params = {
                    "start_time_from": start_time_ms,
                    "limit": 25,
                };
                if (page_token) {
                    params.page_token = page_token;
                }
                hash<auto> response = doGet("api/2.1/jobs/runs/list", params);

                list<auto> runs = response.runs ?? ();
                foreach hash<auto> run in (runs) {
                    if (stop) {
                        break;
                    }

                    string run_id = string(run.run_id);
                    if (seen_ids{run_id}) {
                        continue;
                    }

                    seen_ids{run_id} = ++seen_seq;

                    # Track the latest start time for next poll
                    if (run.start_time > new_start_time) {
                        new_start_time = run.start_time;
                    }

                    notifyObservers(EVENT_WATCH_JOB_RUNS, run);
                }

                # Check for more pages
                has_more = response.has_more ?? False;
                page_token = response.next_page_token;
                if (!page_token) {
                    has_more = False;
                }
            }

            # Advance to the latest seen start time; seen_ids handles dedup for
            # runs that share the same millisecond timestamp
            if (new_start_time > start_time_ms) {
                start_time_ms = new_start_time;
            }
            pruneSeenIds();
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error polling job runs: %s: %s", ex.err, ex.desc);
        }
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_WATCH_JOB_RUNS;
    }

    #! Returns the event description
    private string getEventDesc() {
        return "A new Databricks job run has been started";
    }

    #! Returns fake example data
    private hash<auto> getFakeExampleData() {
        return {
            "run_id": 12345678,
            "job_id": 87654321,
            "run_name": "example-job-run",
            "state": {
                "life_cycle_state": "RUNNING",
                "state_message": "",
            },
            "start_time": now_us().getEpochSeconds() * 1000,
        };
    }
}

} # namespace DatabricksDataProvider
