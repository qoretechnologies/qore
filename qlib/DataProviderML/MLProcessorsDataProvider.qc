# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MLProcessorsDataProvider class definition

/** MLProcessorsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Root data provider for ML pipeline processors
public class MLProcessorsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MLProcessorsDataProvider",
            "supports_children": True,
            "children_can_support_pipeline_processors": True,
        };

        #! Core child processor names (always available)
        const CoreChildren = {
            "isolation-forest": "QoreIsolationForestProcessor",
            "dbscan-clustering": "QoreDBSCANClusteringProcessor",
            "kmeans-clustering": "QoreKMeansProcessor",
            "pca": "QorePCAProcessor",
            "holt-winters": "QoreHoltWintersProcessor",
            "seasonal-decomposition": "QoreSeasonalDecompositionProcessor",
            "linear-regression": "QoreLinearRegressionProcessor",
            "lof": "QoreLOFProcessor",
            "gmm": "QoreGMMProcessor",
            "standard-scaler": "QoreStandardScalerProcessor",
            "min-max-scaler": "QoreMinMaxScalerProcessor",
            "imputer": "QoreImputerProcessor",
            "classification-metrics": "QoreClassificationMetricsProcessor",
            "regression-metrics": "QoreRegressionMetricsProcessor",
            "clustering-metrics": "QoreClusteringMetricsProcessor",
        };

        #! Full child processor names including optional ones
        hash<string, string> Children;
    }

    #! Creates the object
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", NOTHING, options);
        Children = CoreChildren;
        if (ML::ml_get_capabilities().has_onnx) {
            Children."onnx-model" = "QoreOnnxModelProcessor";
        }
    }

    #! Returns the name of this data provider
    string getName() {
        return "ml-processors";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Root data provider for ML-based data pipeline processors providing anomaly detection, "
            "clustering, forecasting, dimensionality reduction, and seasonal decomposition";
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
            case "isolation-forest":
                return new QoreIsolationForestProcessor();
            case "dbscan-clustering":
                return new QoreDBSCANClusteringProcessor();
            case "kmeans-clustering":
                return new QoreKMeansProcessor();
            case "pca":
                return new QorePCAProcessor();
            case "holt-winters":
                return new QoreHoltWintersProcessor();
            case "seasonal-decomposition":
                return new QoreSeasonalDecompositionProcessor();
            case "linear-regression":
                return new QoreLinearRegressionProcessor();
            case "lof":
                return new QoreLOFProcessor();
            case "gmm":
                return new QoreGMMProcessor();
            case "standard-scaler":
                return new QoreStandardScalerProcessor();
            case "min-max-scaler":
                return new QoreMinMaxScalerProcessor();
            case "imputer":
                return new QoreImputerProcessor();
            case "classification-metrics":
                return new QoreClassificationMetricsProcessor();
            case "regression-metrics":
                return new QoreRegressionMetricsProcessor();
            case "clustering-metrics":
                return new QoreClusteringMetricsProcessor();
            case "onnx-model":
                if (ML::ml_get_capabilities().has_onnx) {
                    return new QoreOnnxModelProcessor();
                }
                break;
        }
    }
}
}
