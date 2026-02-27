# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksWatchClusterStateDataProvider class definition

/*  DatabricksWatchClusterStateDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Watch for Databricks cluster state changes
/** Polls the Databricks Clusters API and emits events when a cluster changes state
    (e.g., `PENDING` -> `RUNNING`, `RUNNING` -> `TERMINATED`).
*/
public class DatabricksWatchClusterStateDataProvider inherits DatabricksWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-cluster-state",
            "desc": "Watch for Databricks cluster state changes",
            "type": "DatabricksWatchClusterStateDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    private {
        #! Last known state per cluster_id
        hash<string, string> cluster_states;
    }

    #! Creates the object
    constructor(DatabricksRestClient::DatabricksRestClient rest, *hash<auto> options)
            : DatabricksWatchDataProviderBase(rest, options) {
    }

    #! Returns the name of this data provider
    string getName() {
        return "watch-cluster-state";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Polls Databricks clusters and emits events when a cluster changes state "
            "(e.g., `PENDING` to `RUNNING`, `RUNNING` to `TERMINATED`)";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Polls for cluster state changes
    private pollOnce() {
        try {
            hash<auto> response = doGet("api/2.0/clusters/list");
            list<auto> clusters = response.clusters ?? ();

            # Track which cluster IDs are still active
            hash<string, bool> current_ids;

            foreach hash<auto> cluster in (clusters) {
                if (stop) {
                    break;
                }

                string cluster_id = cluster.cluster_id;
                string new_state = cluster.state ?? "UNKNOWN";
                current_ids{cluster_id} = True;
                *string old_state = cluster_states{cluster_id};

                if (!exists old_state) {
                    # First time seeing this cluster — record state but don't emit
                    cluster_states{cluster_id} = new_state;
                    continue;
                }

                if (old_state != new_state) {
                    cluster_states{cluster_id} = new_state;

                    notifyObservers(EVENT_WATCH_CLUSTER_STATE,
                        <DatabricksClusterStateChangeEventInfo>{
                            "event_type": "databricks-cluster-state-change",
                            "cluster_id": cluster_id,
                            "cluster_name": cluster.cluster_name ?? "",
                            "old_state": old_state,
                            "new_state": new_state,
                            "state_message": cluster.state_message,
                        },
                    );
                }
            }

            # Prune entries for clusters no longer returned by the API
            foreach string cid in (keys cluster_states) {
                if (!current_ids{cid}) {
                    remove cluster_states{cid};
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error polling cluster state: %s: %s", ex.err, ex.desc);
        }
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_WATCH_CLUSTER_STATE;
    }

    #! Returns the event description
    private string getEventDesc() {
        return "A Databricks cluster has changed state";
    }

    #! Returns fake example data
    private hash<auto> getFakeExampleData() {
        return {
            "event_type": "databricks-cluster-state-change",
            "cluster_id": "0123-456789-abcdefgh",
            "cluster_name": "my-cluster",
            "old_state": "PENDING",
            "new_state": "RUNNING",
        };
    }
}

} # namespace DatabricksDataProvider
