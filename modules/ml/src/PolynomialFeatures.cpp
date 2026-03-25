/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PolynomialFeatures.cpp

    PolynomialFeatures implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_PolynomialFeatures.h"
#include "ml_serialization.h"

QorePolynomialFeatures::QorePolynomialFeatures(int degree, bool include_bias,
    bool interaction_only)
    : degree(degree), include_bias(include_bias), interaction_only(interaction_only) {
}

void QorePolynomialFeatures::generateTerms() {
    terms.clear();

    // Bias term (degree 0): constant 1
    if (include_bias) {
        terms.push_back(Term{{}});
    }

    // Degree 1: individual features
    for (int f = 0; f < n_features; ++f) {
        terms.push_back(Term{{f}});
    }

    // Degrees 2..degree: combinations with repetition (or without for interaction_only)
    for (int d = 2; d <= degree; ++d) {
        // Generate all sorted combinations of d features from [0, n_features)
        // For interaction_only: all features in the combination must be distinct
        // For full polynomial: features can repeat (e.g., x0*x0 = x0^2)
        std::vector<int> combo(d, 0);
        while (true) {
            // Check if this combination is valid
            bool valid = true;
            if (interaction_only) {
                // All indices must be strictly increasing (no repeated features)
                for (int i = 1; i < d; ++i) {
                    if (combo[i] <= combo[i - 1]) {
                        valid = false;
                        break;
                    }
                }
            }
            if (valid) {
                terms.push_back(Term{combo});
            }

            // Advance to next combination (non-decreasing sequence)
            int pos = d - 1;
            while (pos >= 0) {
                ++combo[pos];
                if (combo[pos] < n_features) {
                    // Fill subsequent positions
                    int start_val = interaction_only ? combo[pos] + 1 : combo[pos];
                    bool overflow = false;
                    for (int j = pos + 1; j < d; ++j) {
                        combo[j] = start_val;
                        if (interaction_only) {
                            ++start_val;
                        }
                        if (combo[j] >= n_features) {
                            overflow = true;
                            break;
                        }
                    }
                    if (!overflow) {
                        break;
                    }
                }
                --pos;
            }
            if (pos < 0) {
                break;
            }
        }
    }

    n_output_cols = (int)terms.size();
}

double QorePolynomialFeatures::evaluateTerm(const Eigen::VectorXd& row,
    const Term& term) const {
    double val = 1.0;
    for (int idx : term.indices) {
        val *= row(idx);
    }
    return val;
}

std::string QorePolynomialFeatures::termName(const Term& term) const {
    if (term.indices.empty()) {
        return "1";
    }
    if (term.indices.size() == 1) {
        int f = term.indices[0];
        return (f < (int)field_names.size()) ? field_names[f]
            : ("x" + std::to_string(f));
    }

    // Count feature occurrences for power notation
    std::string name;
    int i = 0;
    while (i < (int)term.indices.size()) {
        int f = term.indices[i];
        int count = 1;
        while (i + count < (int)term.indices.size() && term.indices[i + count] == f) {
            ++count;
        }
        if (!name.empty()) {
            name += " ";
        }
        std::string fname = (f < (int)field_names.size()) ? field_names[f]
            : ("x" + std::to_string(f));
        name += fname;
        if (count > 1) {
            name += "^" + std::to_string(count);
        }
        i += count;
    }
    return name;
}

void QorePolynomialFeatures::fit(const MatrixXd& X, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-POLYNOMIAL-FEATURES-ERROR",
            "cannot fit on empty data");
        return;
    }

    n_features = (int)X.cols();
    generateTerms();
    fitted = true;
}

MatrixXd QorePolynomialFeatures::transform(const MatrixXd& X,
    ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-POLYNOMIAL-FEATURES-ERROR",
            "transformer has not been fitted");
        return MatrixXd();
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-POLYNOMIAL-FEATURES-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return MatrixXd();
    }

    MatrixXd result(X.rows(), n_output_cols);
    for (int64_t i = 0; i < X.rows(); ++i) {
        Eigen::VectorXd row = X.row(i);
        for (int t = 0; t < n_output_cols; ++t) {
            result(i, t) = evaluateTerm(row, terms[t]);
        }
    }
    return result;
}

MatrixXd QorePolynomialFeatures::fitTransform(const MatrixXd& X,
    ExceptionSink* xsink) {
    fit(X, xsink);
    if (*xsink) {
        return MatrixXd();
    }
    return transform(X, xsink);
}

QoreListNode* QorePolynomialFeatures::getOutputFeatureNames(
    ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-POLYNOMIAL-FEATURES-ERROR",
            "transformer has not been fitted");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> names(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& term : terms) {
        names->push(new QoreStringNode(termName(term)), xsink);
    }
    return names.release();
}

int QorePolynomialFeatures::getNumOutputFeatures() const {
    return n_output_cols;
}

// --- Serialization ---

std::vector<uint8_t> QorePolynomialFeatures::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeInt32(buf, degree);
    MLSerialization::writeBool(buf, include_bias);
    MLSerialization::writeBool(buf, interaction_only);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, n_output_cols);
    MLSerialization::writeStringVector(buf, field_names);
    return buf;
}

QorePolynomialFeatures* QorePolynomialFeatures::deserializeState(
    const uint8_t* data, size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    int d = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool ib = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool io = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nf = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int noc = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    auto fnames = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    auto* pf = new QorePolynomialFeatures(d, ib, io);
    pf->n_features = nf;
    pf->field_names = std::move(fnames);
    pf->generateTerms();
    pf->fitted = true;
    // n_output_cols is set by generateTerms(), verify consistency
    if (pf->n_output_cols != noc) {
        delete pf;
        xsink->raiseException("ML-DESERIALIZE-ERROR",
            "PolynomialFeatures deserialization mismatch: expected %d output cols, got %d",
            noc, pf->n_output_cols);
        return nullptr;
    }
    return pf;
}
