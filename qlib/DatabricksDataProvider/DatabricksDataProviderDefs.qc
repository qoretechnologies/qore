# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider definitions

/*  DatabricksDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Application name constant
public const AppName = "Databricks";

#! Default poll interval in seconds for watch providers
public const DefaultPollInterval = 60;

#! Minimum poll interval in seconds
public const MinPollInterval = 30;

#! Maximum page size for API requests
public const MaxPageSize = 100;

#! Event name for watching new job runs
public const EVENT_WATCH_JOB_RUNS = "watch-job-runs";

#! Event name for watching cluster state changes
public const EVENT_WATCH_CLUSTER_STATE = "watch-cluster-state";

#! Event name for watching pipeline updates
public const EVENT_WATCH_PIPELINE_UPDATES = "watch-pipeline-updates";

#! The Databricks logo SVG
public const DatabricksLogo = File::readTextFile(get_script_dir() + "/databricks-logo.svg");

#! Event info for model prediction processor output
public hashdecl DatabricksModelPredictionEventInfo {
    string event_type = "databricks-model-predict";
    auto prediction;
    hash<auto> record_values;
    string endpoint_name;
}

#! Event info for batch model prediction processor output
public hashdecl DatabricksBatchModelPredictionEventInfo {
    string event_type = "databricks-batch-model-predict";
    list<auto> predictions;
    int batch_size;
    string endpoint_name;
}

#! Event info for chat completion processor output
public hashdecl DatabricksChatCompletionEventInfo {
    string event_type = "databricks-chat-completion";
    string completion;
    *hash<auto> usage;
    string endpoint_name;
    string prompt_value;
}

#! Event info for text embeddings processor output
public hashdecl DatabricksTextEmbeddingsEventInfo {
    string event_type = "databricks-text-embeddings";
    list<float> embedding;
    *hash<auto> usage;
    string endpoint_name;
    string text_value;
}

#! Event info for text completion processor output
public hashdecl DatabricksTextCompletionEventInfo {
    string event_type = "databricks-text-completion";
    string completion;
    *hash<auto> usage;
    string endpoint_name;
    string prompt_value;
}

#! Event info for cluster state change trigger
public hashdecl DatabricksClusterStateChangeEventInfo {
    string event_type = "databricks-cluster-state-change";
    string cluster_id;
    string cluster_name;
    string old_state;
    string new_state;
    *string state_message;
}

#! Event info for pipeline update trigger
public hashdecl DatabricksPipelineUpdateEventInfo {
    string event_type = "databricks-pipeline-update";
    string pipeline_id;
    string pipeline_name;
    string update_id;
    string update_state;
    *bool full_refresh;
}

} # namespace DatabricksDataProvider
