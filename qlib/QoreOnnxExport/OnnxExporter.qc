# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OnnxExporter.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see OnnxExporter
public namespace QoreOnnxExport {

#! Exports trained Qore ML models to ONNX format
/** Constructs ONNX computation graphs from fitted model parameters and
    serializes them via protobuf. The exported \c .onnx files can be loaded
    by any ONNX-compatible runtime (ONNX Runtime, TensorFlow, PyTorch, etc.).

    @par Supported Algorithms
    - @ref ML::StandardScaler "StandardScaler"
    - @ref ML::MinMaxScaler "MinMaxScaler"
    - @ref ML::Imputer "Imputer"
    - @ref ML::LinearRegression "LinearRegression"
    - @ref ML::LogisticRegression "LogisticRegression"
    - @ref ML::PCA "PCA"
    - @ref ML::KMeans "KMeans"
    - @ref ML::DecisionTree "DecisionTree"
    - @ref ML::RandomForest "RandomForest"
    - @ref ML::GradientBoostedTrees "GradientBoostedTrees"
    - @ref ML::IsolationForest "IsolationForest"

    @par Example:
    @code{.py}
%requires QoreOnnxExport
%requires ml

ML::LinearRegression lr({"fit_intercept": True});
lr.fitMatrix(training_X, training_y);

# Export to ONNX binary
binary onnx_data = OnnxExporter::export(lr, "my_regression");

# Or export directly to a file
OnnxExporter::exportToFile(lr, "model.onnx");
    @endcode
*/
public class OnnxExporter {
    #! Exports a fitted ML model to ONNX binary format
    /** @param model the fitted ML model object
        @param model_name optional model name for the ONNX graph (default: algorithm name)
        @return the ONNX-encoded binary data

        @throw ONNX-EXPORT-ERROR if the model is not fitted or not supported
    */
    static binary export(object model, *string model_name) {
        if (model instanceof ML::StandardScaler) {
            return exportStandardScaler(cast<ML::StandardScaler>(model), model_name);
        }
        if (model instanceof ML::Imputer) {
            return exportImputer(cast<ML::Imputer>(model), model_name);
        }
        if (model instanceof ML::LinearRegression) {
            return exportLinearRegression(cast<ML::LinearRegression>(model), model_name);
        }
        if (model instanceof ML::MinMaxScaler) {
            return exportMinMaxScaler(cast<ML::MinMaxScaler>(model), model_name);
        }
        if (model instanceof ML::PCA) {
            return exportPCA(cast<ML::PCA>(model), model_name);
        }
        if (model instanceof ML::LogisticRegression) {
            return exportLogisticRegression(cast<ML::LogisticRegression>(model), model_name);
        }
        if (model instanceof ML::KMeans) {
            return exportKMeans(cast<ML::KMeans>(model), model_name);
        }
        if (model instanceof ML::DecisionTree) {
            return exportDecisionTree(cast<ML::DecisionTree>(model), model_name);
        }
        if (model instanceof ML::RandomForest) {
            return exportRandomForest(cast<ML::RandomForest>(model), model_name);
        }
        if (model instanceof ML::GradientBoostedTrees) {
            return exportGBT(cast<ML::GradientBoostedTrees>(model), model_name);
        }
        if (model instanceof ML::IsolationForest) {
            return exportIsolationForest(cast<ML::IsolationForest>(model), model_name);
        }

        string algo = model.className();
        throw "ONNX-EXPORT-ERROR",
            sprintf("algorithm '%s' cannot be exported to ONNX", algo);
    }

    #! Exports a fitted ML model to an ONNX file
    /** @param model the fitted ML model object
        @param path the output file path (should end with .onnx)
        @param model_name optional model name for the ONNX graph

        @throw ONNX-EXPORT-ERROR if the model is not fitted or not supported
    */
    static nothing exportToFile(object model, string path, *string model_name) {
        binary data = OnnxExporter::export(model, model_name);
        File f();
        f.open(path, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(data);
    }

    # --- Per-algorithm exporters ---

    #! Exports a StandardScaler to ONNX
    /** Graph: Sub(X, mean) -> Div(std)
    */
    private static binary exportStandardScaler(ML::StandardScaler scaler, *string model_name) {
        if (!scaler.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "StandardScaler has not been fitted";
        }

        hash<auto> info = scaler.getInfo();
        list<auto> mean_data = info.mean;
        list<auto> std_data = info.std;
        int n_features = info.n_features;
        string name = model_name ?? "standard_scaler";

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        # Sub(X, mean)
        initializers += OnnxGraphBuilder::makeTensorFloat("mean", (n_features,), mean_data);
        nodes += OnnxGraphBuilder::makeNode("Sub", ("X", "mean"), ("sub_out",));

        # Div(sub_out, std)
        initializers += OnnxGraphBuilder::makeTensorFloat("std", (n_features,), std_data);
        nodes += OnnxGraphBuilder::makeNode("Div", ("sub_out", "std"), ("Y",));

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeOutput("Y", n_features),),
            initializers,
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Exports an Imputer to ONNX
    /** Graph: IsNaN(X) -> Where(mask, fill_values, X)
    */
    private static binary exportImputer(ML::Imputer imputer, *string model_name) {
        if (!imputer.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "Imputer has not been fitted";
        }

        hash<auto> info = imputer.getInfo();
        list<auto> fill_data = info.fill_values;
        int n_features = info.n_features;
        string name = model_name ?? "imputer";

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        # IsNaN(X) -> mask
        nodes += OnnxGraphBuilder::makeNode("IsNaN", ("X",), ("nan_mask",));

        # fill_values initializer (broadcast shape)
        initializers += OnnxGraphBuilder::makeTensorFloat("fill_values",
            (1, n_features), fill_data);

        # Where(mask, fill_values, X) -> Y
        nodes += OnnxGraphBuilder::makeNode("Where", ("nan_mask", "fill_values", "X"), ("Y",));

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeOutput("Y", n_features),),
            initializers,
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Exports a LinearRegression to ONNX
    /** Graph: MatMul(X, coefficients) + Add(bias)
        If normalize was used: Sub(X, mean) -> Div(std) -> MatMul -> Add
    */
    private static binary exportLinearRegression(ML::LinearRegression lr, *string model_name) {
        if (!lr.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "LinearRegression has not been fitted";
        }

        hash<auto> info = lr.getModelInfo();
        list<auto> coeff = info.coefficients;
        float intercept = info.intercept;
        int n_features = info.n_features;
        string name = model_name ?? "linear_regression";

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();
        string current_input = "X";

        # If normalized, prepend Sub(X, mean) -> Div(std)
        if (lr.isNormalized()) {
            list<auto> means = lr.getFeatureMeans();
            list<auto> stds = lr.getFeatureStds();
            initializers += OnnxGraphBuilder::makeTensorFloat("norm_mean",
                (n_features,), means);
            initializers += OnnxGraphBuilder::makeTensorFloat("norm_std",
                (n_features,), stds);
            nodes += OnnxGraphBuilder::makeNode("Sub", (current_input, "norm_mean"),
                ("norm_sub_out",));
            nodes += OnnxGraphBuilder::makeNode("Div", ("norm_sub_out", "norm_std"),
                ("norm_out",));
            current_input = "norm_out";
        }

        # MatMul(X, coefficients) — coefficients as column vector [n_features, 1]
        initializers += OnnxGraphBuilder::makeTensorFloat("coefficients",
            (n_features, 1), coeff);
        nodes += OnnxGraphBuilder::makeNode("MatMul", (current_input, "coefficients"),
            ("matmul_out",));

        # Add(matmul_out, bias)
        initializers += OnnxGraphBuilder::makeTensorFloat("bias", (1,), (intercept,));
        nodes += OnnxGraphBuilder::makeNode("Add", ("matmul_out", "bias"), ("Y",));

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeOutput("Y", 1),),
            initializers,
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Exports a MinMaxScaler to ONNX
    /** Graph: Sub(X, data_min) -> Div(data_range) -> Mul(feature_width) -> Add(feature_min)
    */
    private static binary exportMinMaxScaler(ML::MinMaxScaler scaler, *string model_name) {
        if (!scaler.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "MinMaxScaler has not been fitted";
        }

        hash<auto> info = scaler.getInfo();
        list<auto> data_min = info.data_min;
        list<auto> data_max = info.data_max;
        float feature_min = info.feature_min;
        float feature_max = info.feature_max;
        int n_features = info.n_features;
        string name = model_name ?? "min_max_scaler";

        # Compute data_range = data_max - data_min (avoid division by zero)
        list<auto> data_range();
        for (int i = 0; i < n_features; ++i) {
            float range = float(data_max[i]) - float(data_min[i]);
            if (range == 0.0) {
                range = 1.0;
            }
            push data_range, range;
        }

        float feature_width = feature_max - feature_min;

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        # Sub(X, data_min)
        initializers += OnnxGraphBuilder::makeTensorFloat("data_min",
            (n_features,), data_min);
        nodes += OnnxGraphBuilder::makeNode("Sub", ("X", "data_min"), ("sub_out",));

        # Div(sub_out, data_range)
        initializers += OnnxGraphBuilder::makeTensorFloat("data_range",
            (n_features,), data_range);
        nodes += OnnxGraphBuilder::makeNode("Div", ("sub_out", "data_range"), ("div_out",));

        # Mul(div_out, feature_width)
        initializers += OnnxGraphBuilder::makeTensorFloat("feature_width",
            (1,), (feature_width,));
        nodes += OnnxGraphBuilder::makeNode("Mul", ("div_out", "feature_width"), ("mul_out",));

        # Add(mul_out, feature_min)
        initializers += OnnxGraphBuilder::makeTensorFloat("feature_min_val",
            (1,), (feature_min,));
        nodes += OnnxGraphBuilder::makeNode("Add", ("mul_out", "feature_min_val"), ("Y",));

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeOutput("Y", n_features),),
            initializers,
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Exports a PCA to ONNX
    /** Graph: Sub(X, mean) [-> Div(std) if scaled] -> MatMul(components^T)
    */
    private static binary exportPCA(ML::PCA pca, *string model_name) {
        if (!pca.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "PCA has not been fitted";
        }

        list<auto> mean = pca.getMean();
        list<auto> components = pca.getComponents();
        int n_components = pca.getNumComponents();
        int n_features = mean.size();
        string name = model_name ?? "pca";

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        # Sub(X, mean)
        initializers += OnnxGraphBuilder::makeTensorFloat("mean", (n_features,), mean);
        nodes += OnnxGraphBuilder::makeNode("Sub", ("X", "mean"), ("centered",));
        string current = "centered";

        # If PCA used scaling, divide by stddev
        *list<auto> stddev = pca.getStddev();
        if (stddev) {
            initializers += OnnxGraphBuilder::makeTensorFloat("stddev",
                (n_features,), stddev);
            nodes += OnnxGraphBuilder::makeNode("Div", (current, "stddev"), ("scaled",));
            current = "scaled";
        }

        # MatMul(centered, components^T)
        # components is [n_components x n_features], we need [n_features x n_components]
        # Flatten and transpose
        list<auto> comp_t_data();
        for (int j = 0; j < n_features; ++j) {
            for (int i = 0; i < n_components; ++i) {
                push comp_t_data, float(components[i][j]);
            }
        }
        initializers += OnnxGraphBuilder::makeTensorFloat("components_t",
            (n_features, n_components), comp_t_data);
        nodes += OnnxGraphBuilder::makeNode("MatMul", (current, "components_t"), ("Y",));

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeOutput("Y", n_components),),
            initializers,
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Exports a LogisticRegression to ONNX
    /** Binary: MatMul(X, W^T) + Add(b) -> Sigmoid
        Multiclass: MatMul(X, W^T) + Add(b) -> Softmax
    */
    private static binary exportLogisticRegression(ML::LogisticRegression lr,
            *string model_name) {
        if (!lr.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "LogisticRegression has not been fitted";
        }

        list<auto> weights = lr.getWeights();
        list<auto> intercepts = lr.getIntercepts();
        bool is_binary = lr.isBinary();
        int n_classes = weights.size();
        int n_features = weights[0].size();
        string name = model_name ?? "logistic_regression";

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        # Transpose weights: [n_classes x n_features] -> [n_features x n_classes]
        list<auto> w_t_data();
        for (int j = 0; j < n_features; ++j) {
            for (int i = 0; i < n_classes; ++i) {
                push w_t_data, float(weights[i][j]);
            }
        }
        initializers += OnnxGraphBuilder::makeTensorFloat("weights_t",
            (n_features, n_classes), w_t_data);

        # MatMul(X, W^T)
        nodes += OnnxGraphBuilder::makeNode("MatMul", ("X", "weights_t"), ("matmul_out",));

        # Add bias
        list<auto> bias_data();
        foreach auto b in (intercepts) {
            push bias_data, float(b);
        }
        initializers += OnnxGraphBuilder::makeTensorFloat("bias",
            (n_classes,), bias_data);
        nodes += OnnxGraphBuilder::makeNode("Add", ("matmul_out", "bias"), ("logits",));

        if (is_binary) {
            # Sigmoid for binary
            nodes += OnnxGraphBuilder::makeNode("Sigmoid", ("logits",), ("Y",));
        } else {
            # Softmax for multiclass (axis=1)
            nodes += OnnxGraphBuilder::makeNode("Softmax", ("logits",), ("Y",),
                (OnnxGraphBuilder::makeAttrInt("axis", 1),));
        }

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeOutput("Y", n_classes),),
            initializers,
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Exports a KMeans to ONNX
    /** Graph: compute squared distances to each centroid, then ArgMin
    */
    private static binary exportKMeans(ML::KMeans km, *string model_name) {
        if (!km.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "KMeans has not been fitted";
        }

        list<auto> centroids = km.getCentroids();
        int k = km.getK();
        int n_features = centroids[0].size();
        string name = model_name ?? "kmeans";

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        # For each centroid, compute squared distance:
        # diff_i = Sub(X, centroid_i) -> Mul(diff_i, diff_i) -> ReduceSum(axis=1)
        # Then Concat all distances and ArgMin

        list<string> dist_names();
        for (int i = 0; i < k; ++i) {
            string c_name = sprintf("centroid_%d", i);
            string diff_name = sprintf("diff_%d", i);
            string sq_name = sprintf("sq_%d", i);
            string dist_name = sprintf("dist_%d", i);

            # Centroid as [1, n_features] for broadcast
            list<auto> c_data();
            foreach auto v in (centroids[i]) {
                push c_data, float(v);
            }
            initializers += OnnxGraphBuilder::makeTensorFloat(c_name,
                (1, n_features), c_data);

            # Sub(X, centroid)
            nodes += OnnxGraphBuilder::makeNode("Sub", ("X", c_name), (diff_name,));

            # Mul(diff, diff) = element-wise square
            nodes += OnnxGraphBuilder::makeNode("Mul", (diff_name, diff_name), (sq_name,));

            # ReduceSum(axis=1, keepdims=1)
            # For opset 18, axes is an input not attribute
            string axes_name = sprintf("axes_%d", i);
            initializers += OnnxGraphBuilder::makeTensorInt64(axes_name, (1,), (1,));
            nodes += OnnxGraphBuilder::makeNode("ReduceSum", (sq_name, axes_name),
                (dist_name,),
                (OnnxGraphBuilder::makeAttrInt("keepdims", 1),));

            push dist_names, dist_name;
        }

        # Concat all distances along axis=1
        nodes += OnnxGraphBuilder::makeNode("Concat", dist_names, ("all_dists",),
            (OnnxGraphBuilder::makeAttrInt("axis", 1),));

        # ArgMin along axis=1
        nodes += OnnxGraphBuilder::makeNode("ArgMin", ("all_dists",), ("Y",),
            (OnnxGraphBuilder::makeAttrInt("axis", 1),
             OnnxGraphBuilder::makeAttrInt("keepdims", 0),));

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeScalarOutput("Y", ONNX_INT64),),
            initializers,
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Exports an IsolationForest to ONNX
    /** Uses ai.onnx.ml.TreeEnsembleRegressor for tree traversal,
        then post-processing to compute anomaly score: 2^(-path_length / c(n))
    */
    private static binary exportIsolationForest(ML::IsolationForest iforest,
            *string model_name) {
        if (!iforest.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "IsolationForest has not been fitted";
        }

        list<auto> trees = iforest.getTrees();
        int n_trees = iforest.getNumTrees();
        int actual_sample_size = iforest.getActualSampleSize();
        string name = model_name ?? "isolation_forest";

        # Flatten all trees into attribute arrays for TreeEnsembleRegressor
        list<int> nodes_treeids();
        list<int> nodes_nodeids();
        list<int> nodes_featureids();
        list<float> nodes_values();
        list<string> nodes_modes();
        list<int> nodes_truenodeids();
        list<int> nodes_falsenodeids();
        list<int> target_ids();
        list<int> target_nodeids();
        list<int> target_treeids();
        list<float> target_weights();

        for (int t = 0; t < n_trees; ++t) {
            list<auto> tree_nodes = trees[t];
            for (int n = 0; n < tree_nodes.size(); ++n) {
                hash<auto> node = tree_nodes[n];
                push nodes_treeids, t;
                push nodes_nodeids, n;

                bool is_leaf = (node.left == -1 && node.right == -1);
                if (is_leaf) {
                    push nodes_featureids, 0;
                    push nodes_values, 0.0;
                    push nodes_modes, "LEAF";
                    push nodes_truenodeids, 0;
                    push nodes_falsenodeids, 0;

                    # Leaf weight = path length adjustment c(size)
                    float leaf_val = averagePathLength(node.size);
                    push target_ids, 0;
                    push target_nodeids, n;
                    push target_treeids, t;
                    push target_weights, leaf_val;
                } else {
                    push nodes_featureids, node.feature;
                    push nodes_values, float(node.split_value);
                    push nodes_modes, "BRANCH_LEQ";
                    push nodes_truenodeids, node.left;
                    push nodes_falsenodeids, node.right;
                }
            }
        }

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        # Determine n_features from first tree's first non-leaf node
        int n_features = 1;
        for (int t = 0; t < n_trees && n_features == 1; ++t) {
            list<auto> tree_nodes = trees[t];
            for (int n = 0; n < tree_nodes.size(); ++n) {
                if (tree_nodes[n].feature >= n_features) {
                    n_features = tree_nodes[n].feature + 1;
                }
            }
        }

        # TreeEnsembleRegressor node (ai.onnx.ml domain)
        nodes += OnnxGraphBuilder::makeNode("TreeEnsembleRegressor", ("X",),
            ("raw_score",),
            (
                OnnxGraphBuilder::makeAttrInts("nodes_treeids", nodes_treeids),
                OnnxGraphBuilder::makeAttrInts("nodes_nodeids", nodes_nodeids),
                OnnxGraphBuilder::makeAttrInts("nodes_featureids", nodes_featureids),
                OnnxGraphBuilder::makeAttrFloats("nodes_values", nodes_values),
                OnnxGraphBuilder::makeAttrStrings("nodes_modes", nodes_modes),
                OnnxGraphBuilder::makeAttrInts("nodes_truenodeids", nodes_truenodeids),
                OnnxGraphBuilder::makeAttrInts("nodes_falsenodeids", nodes_falsenodeids),
                OnnxGraphBuilder::makeAttrInts("target_ids", target_ids),
                OnnxGraphBuilder::makeAttrInts("target_nodeids", target_nodeids),
                OnnxGraphBuilder::makeAttrInts("target_treeids", target_treeids),
                OnnxGraphBuilder::makeAttrFloats("target_weights", target_weights),
                OnnxGraphBuilder::makeAttrInt("n_targets", 1),
                OnnxGraphBuilder::makeAttrString("aggregate_function", "AVERAGE"),
                OnnxGraphBuilder::makeAttrString("post_transform", "NONE"),
            ),
            NOTHING,
            "ai.onnx.ml",
        );

        # Post-processing: score = 2^(-raw_score / c(n))
        float c_n = averagePathLength(actual_sample_size);
        initializers += OnnxGraphBuilder::makeTensorFloat("c_n", (1,), (c_n,));
        initializers += OnnxGraphBuilder::makeTensorFloat("neg_one", (1,), (-1.0,));
        initializers += OnnxGraphBuilder::makeTensorFloat("two", (1,), (2.0,));

        # Negate: -raw_score
        nodes += OnnxGraphBuilder::makeNode("Mul", ("raw_score", "neg_one"), ("neg_score",));

        # Divide by c(n)
        nodes += OnnxGraphBuilder::makeNode("Div", ("neg_score", "c_n"), ("normalized",));

        # Pow(2, normalized) = 2^(normalized)
        nodes += OnnxGraphBuilder::makeNode("Pow", ("two", "normalized"), ("Y",));

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeOutput("Y", 1),),
            initializers,
            # Both default opset and ai.onnx.ml opset
            (
                {"domain": "", "version": OnnxGraphBuilder::DefaultOpsetVersion},
                {"domain": "ai.onnx.ml", "version": 1},
            ),
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Exports a DecisionTree to ONNX
    /** Uses TreeEnsembleClassifier for classification, TreeEnsembleRegressor for regression.
    */
    private static binary exportDecisionTree(ML::DecisionTree dt, *string model_name) {
        if (!dt.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "DecisionTree has not been fitted";
        }

        list<hash<auto>> tree_nodes = dt.getTreeData();
        string task = dt.getTask();
        int n_features = dt.getNumFeatures();
        string name = model_name ?? "decision_tree";

        if (task == "regression") {
            return exportTreeEnsembleRegressor((tree_nodes,), n_features, name,
                "SUM", NOTHING, NOTHING);
        }
        # Classification
        int n_classes = dt.getNumClasses();
        list<auto> class_labels = dt.getClasses();
        return exportTreeEnsembleClassifier((tree_nodes,), n_features, n_classes,
            class_labels, name, "NONE");
    }

    #! Exports a RandomForest to ONNX
    /** Uses TreeEnsembleClassifier for classification, TreeEnsembleRegressor for regression.
    */
    private static binary exportRandomForest(ML::RandomForest rf, *string model_name) {
        if (!rf.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "RandomForest has not been fitted";
        }

        list<list<hash<auto>>> all_trees = rf.getTreesData();
        string task = rf.getTask();
        int n_features = rf.getNumFeatures();
        string name = model_name ?? "random_forest";

        if (task == "regression") {
            return exportTreeEnsembleRegressor(all_trees, n_features, name,
                "AVERAGE", NOTHING, NOTHING);
        }
        int n_classes = rf.getNumClasses();
        list<auto> class_labels = rf.getClasses();
        return exportTreeEnsembleClassifier(all_trees, n_features, n_classes,
            class_labels, name, "NONE");
    }

    #! Exports a GradientBoostedTrees to ONNX
    /** Uses TreeEnsembleRegressor with post-processing.
        Regression: SUM with base_values and learning rate baked into leaves.
        Binary classification: TreeEnsembleRegressor -> Sigmoid.
        Multiclass: TreeEnsembleRegressor -> Softmax.
    */
    private static binary exportGBT(ML::GradientBoostedTrees gbt, *string model_name) {
        if (!gbt.isFitted()) {
            throw "ONNX-EXPORT-ERROR", "GradientBoostedTrees has not been fitted";
        }

        list<list<hash<auto>>> all_trees = gbt.getTreesData();
        string task = gbt.getTask();
        int n_features = gbt.getNumFeatures();
        float lr = gbt.getLearningRate();
        string name = model_name ?? "gradient_boosted_trees";

        if (task == "regression") {
            float init_pred = gbt.getInitialPrediction();
            # Bake learning rate into leaf values by scaling them
            return exportTreeEnsembleRegressor(all_trees, n_features, name,
                "SUM", (init_pred,), lr);
        }

        int n_classes = gbt.getNumClasses();
        list<auto> class_labels = gbt.getClasses();

        if (n_classes == 2) {
            # Binary classification: trees predict log-odds residuals
            # F(x) = init + lr * sum(tree_i(x)), then sigmoid
            float init_pred = gbt.getInitialPrediction();
            return exportGBTBinaryClassification(all_trees, n_features, n_classes,
                class_labels, name, init_pred, lr);
        }

        # Multiclass: K trees per round, one per class
        list<float> init_preds = gbt.getInitialPredictions();
        return exportGBTMulticlassClassification(all_trees, n_features, n_classes,
            class_labels, name, init_preds, lr);
    }

    #! Shared helper: exports tree ensemble as ONNX TreeEnsembleRegressor
    /** Used by DecisionTree (regression), RandomForest (regression), GBT (regression),
        and IsolationForest (via separate method).

        @param all_trees list of trees, each tree is a list of node hashes
        @param n_features number of input features
        @param name model name for ONNX graph
        @param aggregate aggregation function ("SUM", "AVERAGE")
        @param base_values optional base values (e.g., initial predictions for GBT)
        @param learning_rate optional learning rate to bake into leaf values
    */
    private static binary exportTreeEnsembleRegressor(list<list<hash<auto>>> all_trees,
            int n_features, string name, string aggregate, *list<float> base_values,
            *float learning_rate) {
        # Flatten all trees into ONNX attribute arrays
        list<int> nodes_treeids();
        list<int> nodes_nodeids();
        list<int> nodes_featureids();
        list<float> nodes_values();
        list<string> nodes_modes();
        list<int> nodes_truenodeids();
        list<int> nodes_falsenodeids();
        list<int> target_ids();
        list<int> target_nodeids();
        list<int> target_treeids();
        list<float> target_weights();

        for (int t = 0; t < all_trees.size(); ++t) {
            list<hash<auto>> tree_nodes = all_trees[t];
            for (int n = 0; n < tree_nodes.size(); ++n) {
                hash<auto> node = tree_nodes[n];
                push nodes_treeids, t;
                push nodes_nodeids, n;

                bool is_leaf = (node.feature_index == -1);
                if (is_leaf) {
                    push nodes_featureids, 0;
                    push nodes_values, 0.0;
                    push nodes_modes, "LEAF";
                    push nodes_truenodeids, 0;
                    push nodes_falsenodeids, 0;

                    float leaf_val = float(node.value);
                    if (exists learning_rate) {
                        leaf_val *= learning_rate;
                    }
                    push target_ids, 0;
                    push target_nodeids, n;
                    push target_treeids, t;
                    push target_weights, leaf_val;
                } else {
                    push nodes_featureids, int(node.feature_index);
                    push nodes_values, float(node.threshold);
                    push nodes_modes, "BRANCH_LEQ";
                    push nodes_truenodeids, int(node.left);
                    push nodes_falsenodeids, int(node.right);
                }
            }
        }

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        list<hash<auto>> attrs = (
            OnnxGraphBuilder::makeAttrInts("nodes_treeids", nodes_treeids),
            OnnxGraphBuilder::makeAttrInts("nodes_nodeids", nodes_nodeids),
            OnnxGraphBuilder::makeAttrInts("nodes_featureids", nodes_featureids),
            OnnxGraphBuilder::makeAttrFloats("nodes_values", nodes_values),
            OnnxGraphBuilder::makeAttrStrings("nodes_modes", nodes_modes),
            OnnxGraphBuilder::makeAttrInts("nodes_truenodeids", nodes_truenodeids),
            OnnxGraphBuilder::makeAttrInts("nodes_falsenodeids", nodes_falsenodeids),
            OnnxGraphBuilder::makeAttrInts("target_ids", target_ids),
            OnnxGraphBuilder::makeAttrInts("target_nodeids", target_nodeids),
            OnnxGraphBuilder::makeAttrInts("target_treeids", target_treeids),
            OnnxGraphBuilder::makeAttrFloats("target_weights", target_weights),
            OnnxGraphBuilder::makeAttrInt("n_targets", 1),
            OnnxGraphBuilder::makeAttrString("aggregate_function", aggregate),
            OnnxGraphBuilder::makeAttrString("post_transform", "NONE"),
        );
        if (exists base_values) {
            attrs += OnnxGraphBuilder::makeAttrFloats("base_values", base_values);
        }

        nodes += OnnxGraphBuilder::makeNode("TreeEnsembleRegressor", ("X",),
            ("Y",), attrs, NOTHING, "ai.onnx.ml");

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeOutput("Y", 1),),
            initializers,
            (
                {"domain": "", "version": OnnxGraphBuilder::DefaultOpsetVersion},
                {"domain": "ai.onnx.ml", "version": 1},
            ),
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Shared helper: exports tree ensemble as ONNX TreeEnsembleClassifier
    /** Used by DecisionTree (classification) and RandomForest (classification).

        @param all_trees list of trees, each tree is a list of node hashes
        @param n_features number of input features
        @param n_classes number of target classes
        @param class_labels list of class label values
        @param name model name for ONNX graph
        @param post_transform post-transform function (e.g., "NONE")
    */
    private static binary exportTreeEnsembleClassifier(list<list<hash<auto>>> all_trees,
            int n_features, int n_classes, list<auto> class_labels,
            string name, string post_transform) {
        list<int> nodes_treeids();
        list<int> nodes_nodeids();
        list<int> nodes_featureids();
        list<float> nodes_values();
        list<string> nodes_modes();
        list<int> nodes_truenodeids();
        list<int> nodes_falsenodeids();
        list<int> class_treeids();
        list<int> class_nodeids();
        list<int> class_ids();
        list<float> class_weights();

        for (int t = 0; t < all_trees.size(); ++t) {
            list<hash<auto>> tree_nodes = all_trees[t];
            for (int n = 0; n < tree_nodes.size(); ++n) {
                hash<auto> node = tree_nodes[n];
                push nodes_treeids, t;
                push nodes_nodeids, n;

                bool is_leaf = (node.feature_index == -1);
                if (is_leaf) {
                    push nodes_featureids, 0;
                    push nodes_values, 0.0;
                    push nodes_modes, "LEAF";
                    push nodes_truenodeids, 0;
                    push nodes_falsenodeids, 0;

                    # Add class probabilities for this leaf
                    list<auto> probs = node.class_probs ?? ();
                    for (int c = 0; c < n_classes; ++c) {
                        push class_treeids, t;
                        push class_nodeids, n;
                        push class_ids, c;
                        push class_weights, (c < probs.size()) ? float(probs[c]) : 0.0;
                    }
                } else {
                    push nodes_featureids, int(node.feature_index);
                    push nodes_values, float(node.threshold);
                    push nodes_modes, "BRANCH_LEQ";
                    push nodes_truenodeids, int(node.left);
                    push nodes_falsenodeids, int(node.right);
                }
            }
        }

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        # Build class labels as int64
        list<int> class_labels_int = map int($1), class_labels;

        nodes += OnnxGraphBuilder::makeNode("TreeEnsembleClassifier", ("X",),
            ("Y", "Y_probs"),
            (
                OnnxGraphBuilder::makeAttrInts("nodes_treeids", nodes_treeids),
                OnnxGraphBuilder::makeAttrInts("nodes_nodeids", nodes_nodeids),
                OnnxGraphBuilder::makeAttrInts("nodes_featureids", nodes_featureids),
                OnnxGraphBuilder::makeAttrFloats("nodes_values", nodes_values),
                OnnxGraphBuilder::makeAttrStrings("nodes_modes", nodes_modes),
                OnnxGraphBuilder::makeAttrInts("nodes_truenodeids", nodes_truenodeids),
                OnnxGraphBuilder::makeAttrInts("nodes_falsenodeids", nodes_falsenodeids),
                OnnxGraphBuilder::makeAttrInts("class_treeids", class_treeids),
                OnnxGraphBuilder::makeAttrInts("class_nodeids", class_nodeids),
                OnnxGraphBuilder::makeAttrInts("class_ids", class_ids),
                OnnxGraphBuilder::makeAttrFloats("class_weights", class_weights),
                OnnxGraphBuilder::makeAttrInts("classlabels_int64s", class_labels_int),
                OnnxGraphBuilder::makeAttrString("post_transform", post_transform),
            ),
            NOTHING,
            "ai.onnx.ml",
        );

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (
                OnnxGraphBuilder::makeScalarOutput("Y", ONNX_INT64),
                OnnxGraphBuilder::makeOutput("Y_probs", n_classes),
            ),
            initializers,
            (
                {"domain": "", "version": OnnxGraphBuilder::DefaultOpsetVersion},
                {"domain": "ai.onnx.ml", "version": 1},
            ),
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Exports GBT binary classification to ONNX
    /** Uses TreeEnsembleRegressor for raw logit, then Sigmoid for probability.
        Learning rate is baked into leaf values; initial prediction is in base_values.
    */
    private static binary exportGBTBinaryClassification(list<list<hash<auto>>> all_trees,
            int n_features, int n_classes, list<auto> class_labels,
            string name, float init_pred, float lr) {
        # Use TreeEnsembleRegressor to get raw logit, then sigmoid
        # Bake learning_rate into leaf values and init_pred into base_values
        list<int> nodes_treeids();
        list<int> nodes_nodeids();
        list<int> nodes_featureids();
        list<float> nodes_values();
        list<string> nodes_modes();
        list<int> nodes_truenodeids();
        list<int> nodes_falsenodeids();
        list<int> target_ids();
        list<int> target_nodeids();
        list<int> target_treeids();
        list<float> target_weights();

        for (int t = 0; t < all_trees.size(); ++t) {
            list<hash<auto>> tree_nodes = all_trees[t];
            for (int n = 0; n < tree_nodes.size(); ++n) {
                hash<auto> node = tree_nodes[n];
                push nodes_treeids, t;
                push nodes_nodeids, n;

                if (node.feature_index == -1) {
                    push nodes_featureids, 0;
                    push nodes_values, 0.0;
                    push nodes_modes, "LEAF";
                    push nodes_truenodeids, 0;
                    push nodes_falsenodeids, 0;

                    push target_ids, 0;
                    push target_nodeids, n;
                    push target_treeids, t;
                    push target_weights, float(node.value) * lr;
                } else {
                    push nodes_featureids, int(node.feature_index);
                    push nodes_values, float(node.threshold);
                    push nodes_modes, "BRANCH_LEQ";
                    push nodes_truenodeids, int(node.left);
                    push nodes_falsenodeids, int(node.right);
                }
            }
        }

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        # TreeEnsembleRegressor for raw logit score
        nodes += OnnxGraphBuilder::makeNode("TreeEnsembleRegressor", ("X",),
            ("raw_logit",),
            (
                OnnxGraphBuilder::makeAttrInts("nodes_treeids", nodes_treeids),
                OnnxGraphBuilder::makeAttrInts("nodes_nodeids", nodes_nodeids),
                OnnxGraphBuilder::makeAttrInts("nodes_featureids", nodes_featureids),
                OnnxGraphBuilder::makeAttrFloats("nodes_values", nodes_values),
                OnnxGraphBuilder::makeAttrStrings("nodes_modes", nodes_modes),
                OnnxGraphBuilder::makeAttrInts("nodes_truenodeids", nodes_truenodeids),
                OnnxGraphBuilder::makeAttrInts("nodes_falsenodeids", nodes_falsenodeids),
                OnnxGraphBuilder::makeAttrInts("target_ids", target_ids),
                OnnxGraphBuilder::makeAttrInts("target_nodeids", target_nodeids),
                OnnxGraphBuilder::makeAttrInts("target_treeids", target_treeids),
                OnnxGraphBuilder::makeAttrFloats("target_weights", target_weights),
                OnnxGraphBuilder::makeAttrInt("n_targets", 1),
                OnnxGraphBuilder::makeAttrString("aggregate_function", "SUM"),
                OnnxGraphBuilder::makeAttrString("post_transform", "NONE"),
                OnnxGraphBuilder::makeAttrFloats("base_values", (init_pred,)),
            ),
            NOTHING,
            "ai.onnx.ml",
        );

        # Sigmoid to convert logit to probability
        nodes += OnnxGraphBuilder::makeNode("Sigmoid", ("raw_logit",), ("Y_prob_1",));

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeOutput("Y_prob_1", 1),),
            initializers,
            (
                {"domain": "", "version": OnnxGraphBuilder::DefaultOpsetVersion},
                {"domain": "ai.onnx.ml", "version": 1},
            ),
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Exports GBT multiclass classification to ONNX
    /** Uses TreeEnsembleRegressor with n_targets = n_classes, then Softmax.
        Trees are interleaved: K trees per round, one per class.
    */
    private static binary exportGBTMulticlassClassification(list<list<hash<auto>>> all_trees,
            int n_features, int n_classes, list<auto> class_labels,
            string name, list<float> init_preds, float lr) {
        # GBT multiclass: trees are interleaved, K trees per round
        # Use TreeEnsembleRegressor with n_targets = n_classes
        list<int> nodes_treeids();
        list<int> nodes_nodeids();
        list<int> nodes_featureids();
        list<float> nodes_values();
        list<string> nodes_modes();
        list<int> nodes_truenodeids();
        list<int> nodes_falsenodeids();
        list<int> target_ids();
        list<int> target_nodeids();
        list<int> target_treeids();
        list<float> target_weights();

        for (int t = 0; t < all_trees.size(); ++t) {
            # Tree t corresponds to class (t % n_classes)
            int class_idx = t % n_classes;
            list<hash<auto>> tree_nodes = all_trees[t];
            for (int n = 0; n < tree_nodes.size(); ++n) {
                hash<auto> node = tree_nodes[n];
                push nodes_treeids, t;
                push nodes_nodeids, n;

                if (node.feature_index == -1) {
                    push nodes_featureids, 0;
                    push nodes_values, 0.0;
                    push nodes_modes, "LEAF";
                    push nodes_truenodeids, 0;
                    push nodes_falsenodeids, 0;

                    push target_ids, class_idx;
                    push target_nodeids, n;
                    push target_treeids, t;
                    push target_weights, float(node.value) * lr;
                } else {
                    push nodes_featureids, int(node.feature_index);
                    push nodes_values, float(node.threshold);
                    push nodes_modes, "BRANCH_LEQ";
                    push nodes_truenodeids, int(node.left);
                    push nodes_falsenodeids, int(node.right);
                }
            }
        }

        list<hash<auto>> nodes();
        list<hash<auto>> initializers();

        # TreeEnsembleRegressor with n_targets = n_classes
        nodes += OnnxGraphBuilder::makeNode("TreeEnsembleRegressor", ("X",),
            ("raw_logits",),
            (
                OnnxGraphBuilder::makeAttrInts("nodes_treeids", nodes_treeids),
                OnnxGraphBuilder::makeAttrInts("nodes_nodeids", nodes_nodeids),
                OnnxGraphBuilder::makeAttrInts("nodes_featureids", nodes_featureids),
                OnnxGraphBuilder::makeAttrFloats("nodes_values", nodes_values),
                OnnxGraphBuilder::makeAttrStrings("nodes_modes", nodes_modes),
                OnnxGraphBuilder::makeAttrInts("nodes_truenodeids", nodes_truenodeids),
                OnnxGraphBuilder::makeAttrInts("nodes_falsenodeids", nodes_falsenodeids),
                OnnxGraphBuilder::makeAttrInts("target_ids", target_ids),
                OnnxGraphBuilder::makeAttrInts("target_nodeids", target_nodeids),
                OnnxGraphBuilder::makeAttrInts("target_treeids", target_treeids),
                OnnxGraphBuilder::makeAttrFloats("target_weights", target_weights),
                OnnxGraphBuilder::makeAttrInt("n_targets", n_classes),
                OnnxGraphBuilder::makeAttrString("aggregate_function", "SUM"),
                OnnxGraphBuilder::makeAttrString("post_transform", "NONE"),
                OnnxGraphBuilder::makeAttrFloats("base_values", init_preds),
            ),
            NOTHING,
            "ai.onnx.ml",
        );

        # Softmax to convert raw logits to probabilities
        nodes += OnnxGraphBuilder::makeNode("Softmax", ("raw_logits",), ("Y",),
            (OnnxGraphBuilder::makeAttrInt("axis", 1),));

        hash<auto> model_proto = OnnxGraphBuilder::buildModel(name, nodes,
            (OnnxGraphBuilder::makeInput("X", n_features),),
            (OnnxGraphBuilder::makeOutput("Y", n_classes),),
            initializers,
            (
                {"domain": "", "version": OnnxGraphBuilder::DefaultOpsetVersion},
                {"domain": "ai.onnx.ml", "version": 1},
            ),
        );
        return OnnxGraphBuilder::serialize(model_proto);
    }

    #! Average path length of unsuccessful search in BST with n elements: c(n)
    /** Used for IsolationForest anomaly score normalization.
        c(n) = 2*H(n-1) - 2*(n-1)/n where H(i) is the harmonic number.
    */
    private static float averagePathLength(int n) {
        if (n <= 1) {
            return 0.0;
        }
        if (n == 2) {
            return 1.0;
        }
        # H(n-1) approximated by ln(n-1) + Euler-Mascheroni constant
        float h = nlog(float(n - 1)) + 0.5772156649;
        return 2.0 * h - 2.0 * float(n - 1) / float(n);
    }
}

} # namespace QoreOnnxExport
