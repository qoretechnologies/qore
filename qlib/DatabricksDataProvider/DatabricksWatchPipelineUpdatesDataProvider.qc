# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksWatchPipelineUpdatesDataProvider class definition

/*  DatabricksWatchPipelineUpdatesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Watch for Databricks Delta Live Tables pipeline updates
/** Polls the Databricks Pipelines API and emits events when new pipeline updates
    are detected. Deduplicates by update_id.
*/
public class DatabricksWatchPipelineUpdatesDataProvider inherits DatabricksWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-pipeline-updates",
            "desc": "Watch for Databricks pipeline update events",
            "type": "DatabricksWatchPipelineUpdatesDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    private {
        #! Flag to seed seen_ids on first poll without emitting events
        bool first_poll = True;
    }

    #! Creates the object
    constructor(DatabricksRestClient::DatabricksRestClient rest, *hash<auto> options)
            : DatabricksWatchDataProviderBase(rest, options) {
    }

    #! Returns the name of this data provider
    string getName() {
        return "watch-pipeline-updates";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Polls Databricks pipelines and emits events when new pipeline updates are "
            "detected (e.g., a pipeline run starts or completes)";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Polls for pipeline updates
    private pollOnce() {
        try {
            # Paginate through all pipelines
            list<auto> pipelines = paginateGet("api/2.0/pipelines", "statuses", {
                "max_results": 50,
            });

            foreach hash<auto> pipeline in (pipelines) {
                if (stop) {
                    break;
                }

                # Check latest_updates for new update IDs
                list<auto> updates = pipeline.latest_updates ?? ();
                if (!updates) {
                    continue;
                }

                # Check all updates, not just the latest one
                foreach hash<auto> update in (updates) {
                    string update_id = update.update_id ?? "";
                    if (!update_id.val()) {
                        continue;
                    }

                    if (seen_ids{update_id}) {
                        continue;
                    }

                    seen_ids{update_id} = ++seen_seq;

                    # On first poll, seed seen_ids without emitting events
                    if (first_poll) {
                        continue;
                    }

                    notifyObservers(EVENT_WATCH_PIPELINE_UPDATES,
                        <DatabricksPipelineUpdateEventInfo>{
                            "event_type": "databricks-pipeline-update",
                            "pipeline_id": pipeline.pipeline_id ?? "",
                            "pipeline_name": pipeline.name ?? "",
                            "update_id": update_id,
                            "update_state": update.state ?? "",
                            "full_refresh": update.full_refresh,
                        },
                    );
                }
            }

            first_poll = False;
            pruneSeenIds();
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error polling pipeline updates: %s: %s", ex.err, ex.desc);
        }
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_WATCH_PIPELINE_UPDATES;
    }

    #! Returns the event description
    private string getEventDesc() {
        return "A Databricks pipeline update has been detected";
    }

    #! Returns fake example data
    private hash<auto> getFakeExampleData() {
        return {
            "event_type": "databricks-pipeline-update",
            "pipeline_id": "abcd1234-5678-90ab-cdef-1234567890ab",
            "pipeline_name": "my-dlt-pipeline",
            "update_id": "ef012345-6789-abcd-0123-456789abcdef",
            "update_state": "COMPLETED",
        };
    }
}

} # namespace DatabricksDataProvider
