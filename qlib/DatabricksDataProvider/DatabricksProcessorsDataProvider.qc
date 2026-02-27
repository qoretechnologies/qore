# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksProcessorsDataProvider class definition

/*  DatabricksProcessorsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Container data provider for Databricks ML pipeline processors
public class DatabricksProcessorsDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DatabricksProcessorsDataProvider",
            "supports_children": True,
            "children_can_support_pipeline_processors": True,
        };

        #! Child processor map
        const Children = {
            "model-predict": "DatabricksModelPredictionProcessor",
            "batch-model-predict": "DatabricksBatchModelPredictionProcessor",
            "chat-completion": "DatabricksChatCompletionProcessor",
            "text-embeddings": "DatabricksTextEmbeddingsProcessor",
            "text-completion": "DatabricksTextCompletionProcessor",
        };
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest)
            : DatabricksDataProviderBase(rest) {
    }

    #! Creates the object
    constructor() {
    }

    #! Returns the name of this data provider
    string getName() {
        return "processors";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Container for Databricks ML pipeline processors including model prediction, "
            "batch prediction, chat completion, text embeddings, and text completion";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys Children;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "model-predict":
                return new DatabricksModelPredictionProcessor();
            case "batch-model-predict":
                return new DatabricksBatchModelPredictionProcessor();
            case "chat-completion":
                return new DatabricksChatCompletionProcessor();
            case "text-embeddings":
                return new DatabricksTextEmbeddingsProcessor();
            case "text-completion":
                return new DatabricksTextCompletionProcessor();
        }
    }
}

} # namespace DatabricksDataProvider
