/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    MLPipeline.cpp

    Qore ml module - MLPipeline implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#include "QC_MLPipeline.h"
#include "ml_serialization.h"

#include <cassert>

namespace {

//! Algorithm tag corresponding to each PipelineStepType.
const char* algorithmForType(PipelineStepType t) {
    switch (t) {
        case PipelineStepType::STANDARD_SCALER:     return "StandardScaler";
        case PipelineStepType::MINMAX_SCALER:       return "MinMaxScaler";
        case PipelineStepType::IMPUTER:             return "Imputer";
        case PipelineStepType::PCA:                 return "PCA";
        case PipelineStepType::LINEAR_REGRESSION:   return "LinearRegression";
        case PipelineStepType::KMEANS:              return "KMeans";
        case PipelineStepType::ISOLATION_FOREST:    return "IsolationForest";
        case PipelineStepType::LOF:                 return "LOF";
        case PipelineStepType::GMM:                 return "GMM";
        case PipelineStepType::LOGISTIC_REGRESSION: return "LogisticRegression";
        case PipelineStepType::KNN_CLASSIFICATION:  return "KNN";
    }
    return "";
}

//! Maps the on-the-wire algorithm tag back to its PipelineStepType and the QoreClass +
//! classid needed to construct the wrapper QoreObject. Returns false if the algorithm
//! is not a valid pipeline step type.
bool resolvePipelineStep(const std::string& algorithm, PipelineStepType& type_out,
        QoreClass*& cls_out, qore_classid_t& cid_out) {
    struct Entry {
        const char* name;
        PipelineStepType type;
        QoreClass** cls;
        qore_classid_t* cid;
    };
    static const Entry table[] = {
        {"StandardScaler",     PipelineStepType::STANDARD_SCALER,     &QC_STANDARDSCALER,     &CID_STANDARDSCALER},
        {"MinMaxScaler",       PipelineStepType::MINMAX_SCALER,       &QC_MINMAXSCALER,       &CID_MINMAXSCALER},
        {"Imputer",            PipelineStepType::IMPUTER,             &QC_IMPUTER,            &CID_IMPUTER},
        {"PCA",                PipelineStepType::PCA,                 &QC_PCA,                &CID_PCA},
        {"LinearRegression",   PipelineStepType::LINEAR_REGRESSION,   &QC_LINEARREGRESSION,   &CID_LINEARREGRESSION},
        {"KMeans",             PipelineStepType::KMEANS,              &QC_KMEANS,             &CID_KMEANS},
        {"IsolationForest",    PipelineStepType::ISOLATION_FOREST,    &QC_ISOLATIONFOREST,    &CID_ISOLATIONFOREST},
        {"LOF",                PipelineStepType::LOF,                 &QC_LOF,                &CID_LOF},
        {"GMM",                PipelineStepType::GMM,                 &QC_GMM,                &CID_GMM},
        {"LogisticRegression", PipelineStepType::LOGISTIC_REGRESSION, &QC_LOGISTICREGRESSION, &CID_LOGISTICREGRESSION},
        {"KNN",                PipelineStepType::KNN_CLASSIFICATION,  &QC_KNN,                &CID_KNN},
    };
    for (const Entry& e : table) {
        if (algorithm == e.name) {
            type_out = e.type;
            cls_out = *e.cls;
            cid_out = *e.cid;
            return true;
        }
    }
    return false;
}

} // namespace

std::vector<uint8_t> QoreMLPipeline::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeBool(buf, fitted);
    MLSerialization::writeUint32(buf, static_cast<uint32_t>(steps.size()));
    ExceptionSink xs;
    for (const PipelineStep& step : steps) {
        MLSerialization::writeString(buf, step.name);
        const char* algo = algorithmForType(step.type);
        MLSerialization::writeString(buf, std::string(algo));

        // Reach the priv via the QoreMLModel base CID; vparent guarantees each step's
        // qclass derives from AbstractMLModel, so this is an internal invariant.
        QoreMLModel* m = static_cast<QoreMLModel*>(
            step.obj->getReferencedPrivateData(CID_ABSTRACTMLMODEL, &xs));
        assert(m && !xs);
        std::vector<uint8_t> state = m->serializeState();
        m->deref(&xs);

        MLSerialization::writeUint32(buf, static_cast<uint32_t>(state.size()));
        buf.insert(buf.end(), state.begin(), state.end());
    }
    return buf;
}

QoreMLPipeline* QoreMLPipeline::deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    bool fitted_ = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) {
        return nullptr;
    }
    uint32_t nsteps = MLSerialization::readUint32(ptr, remaining, xsink);
    if (*xsink) {
        return nullptr;
    }

    std::unique_ptr<QoreMLPipeline> p(new QoreMLPipeline);
    QoreProgram* pgm = getProgram();
    for (uint32_t i = 0; i < nsteps; ++i) {
        std::string step_name = MLSerialization::readString(ptr, remaining, xsink);
        if (*xsink) {
            return nullptr;
        }
        std::string algorithm = MLSerialization::readString(ptr, remaining, xsink);
        if (*xsink) {
            return nullptr;
        }
        uint32_t state_len = MLSerialization::readUint32(ptr, remaining, xsink);
        if (*xsink) {
            return nullptr;
        }
        if (remaining < state_len) {
            xsink->raiseException("ML-DESERIALIZE-ERROR",
                "MLPipeline step '%s': declared state length %u exceeds %zu remaining bytes",
                step_name.c_str(), state_len, remaining);
            return nullptr;
        }

        PipelineStepType step_type;
        QoreClass* step_cls;
        qore_classid_t step_cid;
        if (!resolvePipelineStep(algorithm, step_type, step_cls, step_cid)) {
            xsink->raiseException("ML-DESERIALIZE-ERROR",
                "MLPipeline step '%s' has algorithm '%s' which is not a supported pipeline step type",
                step_name.c_str(), algorithm.c_str());
            return nullptr;
        }

        auto deserializer = MLSerialization::getDeserializer(algorithm);
        if (!deserializer) {
            xsink->raiseException("ML-DESERIALIZE-ERROR",
                "MLPipeline step '%s': no registered deserializer for algorithm '%s'",
                step_name.c_str(), algorithm.c_str());
            return nullptr;
        }
        AbstractPrivateData* priv = deserializer(ptr, state_len, xsink);
        if (*xsink || !priv) {
            return nullptr;
        }
        ptr += state_len;
        remaining -= state_len;

        QoreObject* step_obj = new QoreObject(step_cls, pgm);
        step_obj->setPrivate(step_cid, priv);

        // addStep refs the object; release our local one after.
        p->addStep(step_name, step_type, step_obj);
        step_obj->deref(xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    p->fitted = fitted_;
    return p.release();
}

void QoreMLPipeline::addStep(const std::string& name, PipelineStepType type, QoreObject* obj) {
    obj->ref();
    steps.push_back({name, type, obj});
}

bool QoreMLPipeline::isTransformer(PipelineStepType type) {
    switch (type) {
        case PipelineStepType::STANDARD_SCALER:
        case PipelineStepType::MINMAX_SCALER:
        case PipelineStepType::IMPUTER:
        case PipelineStepType::PCA:
            return true;
        default:
            return false;
    }
}

MatrixXd QoreMLPipeline::fitTransformer(const PipelineStep& step, const MatrixXd& data,
        ExceptionSink* xsink) {
    switch (step.type) {
        case PipelineStepType::STANDARD_SCALER: {
            QoreStandardScaler* s = static_cast<QoreStandardScaler*>(
                step.obj->getReferencedPrivateData(CID_STANDARDSCALER, xsink));
            if (*xsink) {
                return MatrixXd();
            }
            MatrixXd result = s->fitTransform(data, xsink);
            s->deref(xsink);
            return result;
        }
        case PipelineStepType::MINMAX_SCALER: {
            QoreMinMaxScaler* s = static_cast<QoreMinMaxScaler*>(
                step.obj->getReferencedPrivateData(CID_MINMAXSCALER, xsink));
            if (*xsink) {
                return MatrixXd();
            }
            MatrixXd result = s->fitTransform(data, xsink);
            s->deref(xsink);
            return result;
        }
        case PipelineStepType::IMPUTER: {
            QoreImputer* s = static_cast<QoreImputer*>(
                step.obj->getReferencedPrivateData(CID_IMPUTER, xsink));
            if (*xsink) {
                return MatrixXd();
            }
            MatrixXd result = s->fitTransform(data, xsink);
            s->deref(xsink);
            return result;
        }
        case PipelineStepType::PCA: {
            QorePCA* s = static_cast<QorePCA*>(
                step.obj->getReferencedPrivateData(CID_PCA, xsink));
            if (*xsink) {
                return MatrixXd();
            }
            QoreListNode* list_result = s->fitTransformInternal(data, xsink);
            s->deref(xsink);
            if (*xsink) {
                return MatrixXd();
            }
            // Convert back from list to matrix
            ReferenceHolder<QoreListNode> holder(list_result, xsink);
            if (!list_result || !list_result->size()) {
                return MatrixXd();
            }
            // Each element is a PCAResult hash — extract "components" field
            int n = static_cast<int>(list_result->size());
            // Get n_components from first result
            QoreHashNode* first = list_result->retrieveEntry(0).get<QoreHashNode>();
            QoreListNode* comps = first->getKeyValue("components").get<QoreListNode>();
            int nc = static_cast<int>(comps->size());

            MatrixXd result(n, nc);
            for (int i = 0; i < n; ++i) {
                QoreHashNode* h = list_result->retrieveEntry(i).get<QoreHashNode>();
                QoreListNode* c = h->getKeyValue("components").get<QoreListNode>();
                for (int j = 0; j < nc; ++j) {
                    result(i, j) = c->retrieveEntry(j).getAsFloat();
                }
            }
            return result;
        }
        default:
            xsink->raiseException("ML-PIPELINE-ERROR",
                "step '%s' is not a transformer", step.name.c_str());
            return MatrixXd();
    }
}

MatrixXd QoreMLPipeline::applyTransformer(const PipelineStep& step, const MatrixXd& data,
        ExceptionSink* xsink) const {
    switch (step.type) {
        case PipelineStepType::STANDARD_SCALER: {
            QoreStandardScaler* s = static_cast<QoreStandardScaler*>(
                step.obj->getReferencedPrivateData(CID_STANDARDSCALER, xsink));
            if (*xsink) {
                return MatrixXd();
            }
            MatrixXd result = s->transform(data, xsink);
            s->deref(xsink);
            return result;
        }
        case PipelineStepType::MINMAX_SCALER: {
            QoreMinMaxScaler* s = static_cast<QoreMinMaxScaler*>(
                step.obj->getReferencedPrivateData(CID_MINMAXSCALER, xsink));
            if (*xsink) {
                return MatrixXd();
            }
            MatrixXd result = s->transform(data, xsink);
            s->deref(xsink);
            return result;
        }
        case PipelineStepType::IMPUTER: {
            QoreImputer* s = static_cast<QoreImputer*>(
                step.obj->getReferencedPrivateData(CID_IMPUTER, xsink));
            if (*xsink) {
                return MatrixXd();
            }
            MatrixXd result = s->transform(data, xsink);
            s->deref(xsink);
            return result;
        }
        case PipelineStepType::PCA: {
            QorePCA* s = static_cast<QorePCA*>(
                step.obj->getReferencedPrivateData(CID_PCA, xsink));
            if (*xsink) {
                return MatrixXd();
            }
            QoreListNode* list_result = s->transformMatrix(data, xsink);
            s->deref(xsink);
            if (*xsink) {
                return MatrixXd();
            }
            ReferenceHolder<QoreListNode> holder(list_result, xsink);
            if (!list_result || !list_result->size()) {
                return MatrixXd();
            }
            int n = static_cast<int>(list_result->size());
            QoreHashNode* first = list_result->retrieveEntry(0).get<QoreHashNode>();
            QoreListNode* comps = first->getKeyValue("components").get<QoreListNode>();
            int nc = static_cast<int>(comps->size());

            MatrixXd result(n, nc);
            for (int i = 0; i < n; ++i) {
                QoreHashNode* h = list_result->retrieveEntry(i).get<QoreHashNode>();
                QoreListNode* c = h->getKeyValue("components").get<QoreListNode>();
                for (int j = 0; j < nc; ++j) {
                    result(i, j) = c->retrieveEntry(j).getAsFloat();
                }
            }
            return result;
        }
        default:
            xsink->raiseException("ML-PIPELINE-ERROR",
                "step '%s' is not a transformer", step.name.c_str());
            return MatrixXd();
    }
}

void QoreMLPipeline::fitMatrix(const MatrixXd& X, const VectorXd* y, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (steps.empty()) {
        xsink->raiseException("ML-PIPELINE-ERROR", "pipeline has no steps");
        return;
    }

    MatrixXd current = X;

    // Process all steps except the last
    for (size_t i = 0; i < steps.size() - 1; ++i) {
        if (!isTransformer(steps[i].type)) {
            xsink->raiseException("ML-PIPELINE-ERROR",
                "step '%s' at position %d is an estimator but is not the last step; "
                "only the last step may be an estimator",
                steps[i].name.c_str(), static_cast<int>(i));
            return;
        }
        current = fitTransformer(steps[i], current, xsink);
        if (*xsink) {
            return;
        }
    }

    // Last step: can be transformer or estimator
    const PipelineStep& last = steps.back();
    if (isTransformer(last.type)) {
        fitTransformer(last, current, xsink);
    } else {
        // Fit the estimator
        switch (last.type) {
            case PipelineStepType::LINEAR_REGRESSION: {
                if (!y) {
                    xsink->raiseException("ML-PIPELINE-ERROR",
                        "LinearRegression estimator requires target vector y; "
                        "use fitMatrix(X, y)");
                    return;
                }
                QoreLinearRegression* lr = static_cast<QoreLinearRegression*>(
                    last.obj->getReferencedPrivateData(CID_LINEARREGRESSION, xsink));
                if (*xsink) {
                    return;
                }
                lr->fit(current, *y, xsink);
                lr->deref(xsink);
                break;
            }
            case PipelineStepType::KMEANS: {
                QoreKMeans* km = static_cast<QoreKMeans*>(
                    last.obj->getReferencedPrivateData(CID_KMEANS, xsink));
                if (*xsink) {
                    return;
                }
                km->fit(current, xsink);
                km->deref(xsink);
                break;
            }
            case PipelineStepType::ISOLATION_FOREST: {
                QoreIsolationForest* ifo = static_cast<QoreIsolationForest*>(
                    last.obj->getReferencedPrivateData(CID_ISOLATIONFOREST, xsink));
                if (*xsink) {
                    return;
                }
                ifo->fit(current, xsink);
                ifo->deref(xsink);
                break;
            }
            case PipelineStepType::LOF: {
                QoreLOF* lof = static_cast<QoreLOF*>(
                    last.obj->getReferencedPrivateData(CID_LOF, xsink));
                if (*xsink) {
                    return;
                }
                lof->fit(current, xsink);
                lof->deref(xsink);
                break;
            }
            case PipelineStepType::GMM: {
                QoreGMM* gmm = static_cast<QoreGMM*>(
                    last.obj->getReferencedPrivateData(CID_GMM, xsink));
                if (*xsink) {
                    return;
                }
                gmm->fit(current, xsink);
                gmm->deref(xsink);
                break;
            }
            case PipelineStepType::LOGISTIC_REGRESSION: {
                if (!y) {
                    xsink->raiseException("ML-PIPELINE-ERROR",
                        "LogisticRegression estimator requires target vector y; "
                        "use fitMatrix(X, y)");
                    return;
                }
                QoreLogisticRegression* lr = static_cast<QoreLogisticRegression*>(
                    last.obj->getReferencedPrivateData(CID_LOGISTICREGRESSION, xsink));
                if (*xsink) {
                    return;
                }
                lr->fit(current, *y, xsink);
                lr->deref(xsink);
                break;
            }
            case PipelineStepType::KNN_CLASSIFICATION: {
                if (!y) {
                    xsink->raiseException("ML-PIPELINE-ERROR",
                        "KNN estimator requires target vector y; "
                        "use fitMatrix(X, y)");
                    return;
                }
                QoreKNN* knn = static_cast<QoreKNN*>(
                    last.obj->getReferencedPrivateData(CID_KNN, xsink));
                if (*xsink) {
                    return;
                }
                knn->fit(current, *y, xsink);
                knn->deref(xsink);
                break;
            }
            default:
                xsink->raiseException("ML-PIPELINE-ERROR",
                    "unknown estimator type for step '%s'", last.name.c_str());
                return;
        }
    }

    fitted = true;
}

MatrixXd QoreMLPipeline::transformMatrix(const MatrixXd& X, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-PIPELINE-ERROR",
            "pipeline has not been fitted: call fitMatrix() first");
        return MatrixXd();
    }

    MatrixXd current = X;

    // Apply all transformer steps (skip the estimator if last step is one)
    size_t n = steps.size();
    if (!isTransformer(steps.back().type)) {
        --n;  // skip last estimator
    }
    for (size_t i = 0; i < n; ++i) {
        current = applyTransformer(steps[i], current, xsink);
        if (*xsink) {
            return MatrixXd();
        }
    }

    return current;
}

QoreHashNode* QoreMLPipeline::predict(const RowVectorXd& point, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-PIPELINE-ERROR",
            "pipeline has not been fitted: call fitMatrix() first");
        return nullptr;
    }
    if (isTransformer(steps.back().type)) {
        xsink->raiseException("ML-PIPELINE-ERROR",
            "last step is a transformer, not an estimator; use transformMatrix() instead");
        return nullptr;
    }

    // Transform through all transformer steps
    MatrixXd current(1, point.size());
    current.row(0) = point;
    for (size_t i = 0; i < steps.size() - 1; ++i) {
        current = applyTransformer(steps[i], current, xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    RowVectorXd transformed = current.row(0);

    // Predict with the estimator
    const PipelineStep& last = steps.back();
    switch (last.type) {
        case PipelineStepType::LINEAR_REGRESSION: {
            QoreLinearRegression* lr = static_cast<QoreLinearRegression*>(
                last.obj->getReferencedPrivateData(CID_LINEARREGRESSION, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreHashNode* result = lr->predict(transformed, xsink);
            lr->deref(xsink);
            return result;
        }
        case PipelineStepType::KMEANS: {
            QoreKMeans* km = static_cast<QoreKMeans*>(
                last.obj->getReferencedPrivateData(CID_KMEANS, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreHashNode* result = km->predict(transformed, xsink);
            km->deref(xsink);
            return result;
        }
        case PipelineStepType::ISOLATION_FOREST: {
            QoreIsolationForest* ifo = static_cast<QoreIsolationForest*>(
                last.obj->getReferencedPrivateData(CID_ISOLATIONFOREST, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreHashNode* result = ifo->score(transformed, xsink);
            ifo->deref(xsink);
            return result;
        }
        case PipelineStepType::LOF: {
            QoreLOF* lof = static_cast<QoreLOF*>(
                last.obj->getReferencedPrivateData(CID_LOF, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreHashNode* result = lof->score(transformed, xsink);
            lof->deref(xsink);
            return result;
        }
        case PipelineStepType::GMM: {
            QoreGMM* gmm = static_cast<QoreGMM*>(
                last.obj->getReferencedPrivateData(CID_GMM, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreHashNode* result = gmm->predict(transformed, xsink);
            gmm->deref(xsink);
            return result;
        }
        case PipelineStepType::LOGISTIC_REGRESSION: {
            QoreLogisticRegression* lr = static_cast<QoreLogisticRegression*>(
                last.obj->getReferencedPrivateData(CID_LOGISTICREGRESSION, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreHashNode* result = lr->predict(transformed, xsink);
            lr->deref(xsink);
            return result;
        }
        case PipelineStepType::KNN_CLASSIFICATION: {
            QoreKNN* knn = static_cast<QoreKNN*>(
                last.obj->getReferencedPrivateData(CID_KNN, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreHashNode* result = (knn->getTask() == "regression")
                ? knn->predictRegression(transformed, xsink)
                : knn->predictClassification(transformed, xsink);
            knn->deref(xsink);
            return result;
        }
        default:
            xsink->raiseException("ML-PIPELINE-ERROR",
                "unknown estimator type for step '%s'", last.name.c_str());
            return nullptr;
    }
}

QoreListNode* QoreMLPipeline::predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-PIPELINE-ERROR",
            "pipeline has not been fitted: call fitMatrix() first");
        return nullptr;
    }
    if (isTransformer(steps.back().type)) {
        xsink->raiseException("ML-PIPELINE-ERROR",
            "last step is a transformer, not an estimator; use transformMatrix() instead");
        return nullptr;
    }

    // Transform through all transformer steps
    MatrixXd current = X;
    for (size_t i = 0; i < steps.size() - 1; ++i) {
        current = applyTransformer(steps[i], current, xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    // Predict with the estimator
    const PipelineStep& last = steps.back();
    switch (last.type) {
        case PipelineStepType::LINEAR_REGRESSION: {
            QoreLinearRegression* lr = static_cast<QoreLinearRegression*>(
                last.obj->getReferencedPrivateData(CID_LINEARREGRESSION, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreListNode* result = lr->predictMatrix(current, xsink);
            lr->deref(xsink);
            return result;
        }
        case PipelineStepType::KMEANS: {
            QoreKMeans* km = static_cast<QoreKMeans*>(
                last.obj->getReferencedPrivateData(CID_KMEANS, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreListNode* result = km->predictMatrix(current, xsink);
            km->deref(xsink);
            return result;
        }
        case PipelineStepType::ISOLATION_FOREST: {
            QoreIsolationForest* ifo = static_cast<QoreIsolationForest*>(
                last.obj->getReferencedPrivateData(CID_ISOLATIONFOREST, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreListNode* result = ifo->scoreMatrix(current, xsink);
            ifo->deref(xsink);
            return result;
        }
        case PipelineStepType::LOF: {
            QoreLOF* lof = static_cast<QoreLOF*>(
                last.obj->getReferencedPrivateData(CID_LOF, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreListNode* result = lof->scoreMatrix(current, xsink);
            lof->deref(xsink);
            return result;
        }
        case PipelineStepType::GMM: {
            QoreGMM* gmm = static_cast<QoreGMM*>(
                last.obj->getReferencedPrivateData(CID_GMM, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreListNode* result = gmm->predictMatrix(current, xsink);
            gmm->deref(xsink);
            return result;
        }
        case PipelineStepType::LOGISTIC_REGRESSION: {
            QoreLogisticRegression* lr = static_cast<QoreLogisticRegression*>(
                last.obj->getReferencedPrivateData(CID_LOGISTICREGRESSION, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreListNode* result = lr->predictMatrix(current, xsink);
            lr->deref(xsink);
            return result;
        }
        case PipelineStepType::KNN_CLASSIFICATION: {
            QoreKNN* knn = static_cast<QoreKNN*>(
                last.obj->getReferencedPrivateData(CID_KNN, xsink));
            if (*xsink) {
                return nullptr;
            }
            QoreListNode* result = (knn->getTask() == "regression")
                ? knn->predictRegressionMatrix(current, xsink)
                : knn->predictClassificationMatrix(current, xsink);
            knn->deref(xsink);
            return result;
        }
        default:
            xsink->raiseException("ML-PIPELINE-ERROR",
                "unknown estimator type for step '%s'", last.name.c_str());
            return nullptr;
    }
}

QoreListNode* QoreMLPipeline::getStepNames(ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& step : steps) {
        rv->push(new QoreStringNode(step.name), xsink);
    }
    return rv.release();
}
