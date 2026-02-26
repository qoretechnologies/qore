# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLDataProvider class definition

/*  BigMLProcessorsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigMLDataProvider module
public namespace BigMLDataProvider {
#! Root data provider for BigML pipeline processors and TypeScript actions
public class BigMLDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BigMLDataProvider",
            "supports_children": True,
            "children_can_support_pipeline_processors": True,
            "children_can_support_apis": True,
        };

        #! Child processor names
        const Children = {
            "predict": "BigMLPredictionProcessor",
            "anomaly-score": "BigMLAnomalyScoreProcessor",
            "centroid": "BigMLCentroidProcessor",
            "topic-distribution": "BigMLTopicDistributionProcessor",
            "train-and-predict": "BigMLTrainAndPredictProcessor",
            "batch-predict": "BigMLBatchPredictionProcessor",
            "batch-anomaly-score": "BigMLBatchAnomalyScoreProcessor",
            "batch-centroid": "BigMLBatchCentroidProcessor",
            "projection": "BigMLProjectionProcessor",
            "batch-projection": "BigMLBatchProjectionProcessor",
            "linear-regression": "BigMLLinearRegressionProcessor",
            "forecast": "BigMLForecastProcessor",
            "logistic-regression": "BigMLLogisticRegressionProcessor",
            "association-set": "BigMLAssociationSetProcessor",
        };
    }

    private {
        #! Dynamic child data providers registered by TypeScript actions
        static hash<string, code> dynamic_children;

        #! Connection for dynamic children
        *AbstractConnection conn;
    }

    #! Creates the object with a connection
    constructor(BigMLRestClient::BigMLRestConnection conn) {
        self.conn = conn;
    }

    #! Creates the object
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", NOTHING, options);
    }

    #! Returns the name of this data provider
    string getName() {
        return "bigml";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Root data provider for BigML machine learning pipeline processors providing "
            "predictions, anomaly detection, clustering, topic modeling, PCA projections, "
            "linear regression, logistic regression, time series forecasting, and association "
            "rule matching via the BigML API";
    }

    #! Registers a child data provider from TypeScript actions
    /** @param name the child's name
        @param generator code to generate the child; prototype: sub (string name, AbstractConnection conn) {}
    */
    static registerChild(string name, code generator) {
        dynamic_children{name} = generator;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        list<string> rv = keys Children;
        if (conn && dynamic_children) {
            rv += keys dynamic_children;
        }
        return rv;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "predict":
                return new BigMLPredictionProcessor();
            case "anomaly-score":
                return new BigMLAnomalyScoreProcessor();
            case "centroid":
                return new BigMLCentroidProcessor();
            case "topic-distribution":
                return new BigMLTopicDistributionProcessor();
            case "train-and-predict":
                return new BigMLTrainAndPredictProcessor();
            case "batch-predict":
                return new BigMLBatchPredictionProcessor();
            case "batch-anomaly-score":
                return new BigMLBatchAnomalyScoreProcessor();
            case "batch-centroid":
                return new BigMLBatchCentroidProcessor();
            case "projection":
                return new BigMLProjectionProcessor();
            case "batch-projection":
                return new BigMLBatchProjectionProcessor();
            case "linear-regression":
                return new BigMLLinearRegressionProcessor();
            case "forecast":
                return new BigMLForecastProcessor();
            case "logistic-regression":
                return new BigMLLogisticRegressionProcessor();
            case "association-set":
                return new BigMLAssociationSetProcessor();
        }
        # Check dynamic children (TypeScript actions)
        if (conn) {
            if (*code gen = dynamic_children{name}) {
                return gen(name, conn);
            }
        }
    }
}
}
