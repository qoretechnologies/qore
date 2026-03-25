/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DecisionTree.cpp

    DecisionTree implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_DecisionTree.h"
#include "ml_serialization.h"

extern const TypedHashDecl* hashdeclDecisionTreeClassificationResult;
extern const TypedHashDecl* hashdeclDecisionTreeRegressionResult;
extern const TypedHashDecl* hashdeclFeatureImportanceInfo;

QoreDecisionTree::QoreDecisionTree(int max_depth, int min_samples_split,
        int min_samples_leaf, const std::string& criterion,
        const std::string& task, int64_t seed)
    : max_depth(max_depth), min_samples_split(min_samples_split),
      min_samples_leaf(min_samples_leaf), criterion(criterion),
      task(task), rng(seed == 0 ? std::random_device{}() : (unsigned)seed) {
}

void QoreDecisionTree::fit(const MatrixXd& X, const VectorXd& y,
        ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "cannot fit on empty data");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "X has " QLLD " rows but y has " QLLD " elements",
            (int64_t)X.rows(), (int64_t)y.size());
        return;
    }

    n_features = (int)X.cols();
    tree.fit(X, y, max_depth, min_samples_split, min_samples_leaf,
        criterion, task, rng);
    fitted = true;
}

QoreHashNode* QoreDecisionTree::predictInternal(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    if (task == "classification") {
        auto probs = tree.predictProba(point);
        double predicted_class = tree.predict(point);
        double max_prob = 0;
        for (double p : probs) {
            if (p > max_prob) {
                max_prob = p;
            }
        }

        ReferenceHolder<QoreHashNode> rv(
            new QoreHashNode(hashdeclDecisionTreeClassificationResult, xsink), xsink);
        rv->setKeyValue("predicted_class", (int64_t)predicted_class, xsink);
        rv->setKeyValue("max_probability", max_prob, xsink);

        ReferenceHolder<QoreListNode> prob_list(new QoreListNode(autoTypeInfo), xsink);
        for (double p : probs) {
            prob_list->push(p, xsink);
        }
        rv->setKeyValue("probabilities", prob_list.release(), xsink);

        ReferenceHolder<QoreListNode> cls_list(new QoreListNode(autoTypeInfo), xsink);
        for (double c : tree.classes) {
            cls_list->push(c, xsink);
        }
        rv->setKeyValue("classes", cls_list.release(), xsink);

        return rv.release();
    }

    // Regression
    double prediction = tree.predict(point);
    ReferenceHolder<QoreHashNode> rv(
        new QoreHashNode(hashdeclDecisionTreeRegressionResult, xsink), xsink);
    rv->setKeyValue("prediction", prediction, xsink);
    return rv.release();
}

QoreHashNode* QoreDecisionTree::predictClassification(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-DECISION-TREE-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (task != "classification") {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "model was configured for '%s', not classification", task.c_str());
        return nullptr;
    }
    if (point.size() != n_features) {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "input has %d features, expected %d", (int)point.size(), n_features);
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreHashNode* QoreDecisionTree::predictRegression(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-DECISION-TREE-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (task != "regression") {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "model was configured for '%s', not regression", task.c_str());
        return nullptr;
    }
    if (point.size() != n_features) {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "input has %d features, expected %d", (int)point.size(), n_features);
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreDecisionTree::predictClassificationMatrix(const MatrixXd& X,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-DECISION-TREE-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (task != "classification") {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "model was configured for '%s', not classification", task.c_str());
        return nullptr;
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> results(
        new QoreListNode(hashdeclDecisionTreeClassificationResult->getTypeInfo()), xsink);
    for (int64_t i = 0; i < X.rows(); ++i) {
        if ((i % 100) == 0 && i > 0) {
            if (qore_check_cancel(xsink, "predicting with decision tree")) {
                return nullptr;
            }
        }
        QoreHashNode* result = predictInternal(X.row(i), xsink);
        if (*xsink) { return nullptr; }
        results->push(result, xsink);
    }
    return results.release();
}

QoreListNode* QoreDecisionTree::predictRegressionMatrix(const MatrixXd& X,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-DECISION-TREE-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (task != "regression") {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "model was configured for '%s', not regression", task.c_str());
        return nullptr;
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> results(
        new QoreListNode(hashdeclDecisionTreeRegressionResult->getTypeInfo()), xsink);
    for (int64_t i = 0; i < X.rows(); ++i) {
        if ((i % 100) == 0 && i > 0) {
            if (qore_check_cancel(xsink, "predicting with decision tree")) {
                return nullptr;
            }
        }
        QoreHashNode* result = predictInternal(X.row(i), xsink);
        if (*xsink) { return nullptr; }
        results->push(result, xsink);
    }
    return results.release();
}

QoreHashNode* QoreDecisionTree::getFeatureImportances(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-DECISION-TREE-ERROR",
            "model has not been fitted");
        return nullptr;
    }

    VectorXd imp = tree.featureImportances();

    ReferenceHolder<QoreHashNode> rv(
        new QoreHashNode(hashdeclFeatureImportanceInfo, xsink), xsink);

    ReferenceHolder<QoreListNode> names(new QoreListNode(stringTypeInfo), xsink);
    ReferenceHolder<QoreListNode> values(new QoreListNode(autoTypeInfo), xsink);
    for (int i = 0; i < n_features; ++i) {
        if (i < (int)field_names.size()) {
            names->push(new QoreStringNode(field_names[i]), xsink);
        } else {
            names->push(new QoreStringNode("feature_" + std::to_string(i)), xsink);
        }
        values->push(imp(i), xsink);
    }
    rv->setKeyValue("feature_names", names.release(), xsink);
    rv->setKeyValue("importances", values.release(), xsink);

    return rv.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreDecisionTree::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeInt32(buf, max_depth);
    MLSerialization::writeInt32(buf, min_samples_split);
    MLSerialization::writeInt32(buf, min_samples_leaf);
    MLSerialization::writeString(buf, criterion);
    MLSerialization::writeString(buf, task);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeStringVector(buf, field_names);
    MLSerialization::writeString(buf, target_field);
    tree.serialize(buf);
    return buf;
}

QoreDecisionTree* QoreDecisionTree::deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    int max_depth = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int min_samples_split = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int min_samples_leaf = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string criterion = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string task = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<std::string> field_names = MLSerialization::readStringVector(
        ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string target_field = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    FlatTree tree = FlatTree::deserialize(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    auto* dt = new QoreDecisionTree(max_depth, min_samples_split, min_samples_leaf,
        criterion, task, 42);
    dt->n_features = n_features;
    dt->field_names = std::move(field_names);
    dt->target_field = std::move(target_field);
    dt->tree = std::move(tree);
    dt->fitted = true;
    return dt;
}
