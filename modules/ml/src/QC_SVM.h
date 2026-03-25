/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_SVM.h

    Support Vector Machine class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_SVM_H
#define _QORE_MODULE_ML_QC_SVM_H

#include "ml_common.h"

#include <mutex>

DLLEXPORT extern qore_classid_t CID_SVM;
DLLLOCAL extern QoreClass* QC_SVM;

DLLLOCAL void preinitSVMClass();
DLLLOCAL QoreClass* initSVMClass(QoreNamespace& ns);

//! Support Vector Machine for binary/multiclass classification
/** Implements SMO (Sequential Minimal Optimization) for training.
    Supports linear and RBF (Gaussian) kernels.
    Multiclass via one-vs-one decomposition.
*/
class QoreSVM : public AbstractPrivateData {
public:
    DLLLOCAL QoreSVM(double C, const std::string& kernel, double gamma,
        int degree, double tol, int max_iter, int64_t seed);

    DLLLOCAL void fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;
    DLLLOCAL QoreListNode* predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

    DLLLOCAL bool isFitted() const { return fitted; }
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) {
        field_names = names;
    }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const {
        return field_names;
    }
    DLLLOCAL void setTargetField(const std::string& name) { target_field = name; }
    DLLLOCAL const std::string& getTargetField() const { return target_field; }

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreSVM* deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink);

private:
    double C;              //!< regularization parameter
    std::string kernel;    //!< "linear" or "rbf"
    double gamma;          //!< RBF kernel parameter (0 = auto: 1/n_features)
    int degree;            //!< polynomial degree (unused for linear/rbf)
    double tol;            //!< convergence tolerance
    int max_iter;          //!< maximum SMO iterations
    std::mt19937 rng;

    //! Binary SVM model (one per class pair for multiclass)
    struct BinarySVM {
        VectorXd alpha;        //!< Lagrange multipliers
        double b = 0.0;        //!< bias
        MatrixXd support_X;    //!< support vectors
        VectorXd support_y;    //!< support vector labels
        VectorXd support_alpha; //!< alpha values for support vectors
        double class_pos;      //!< positive class label
        double class_neg;      //!< negative class label
    };

    std::vector<BinarySVM> models;  //!< one per class pair
    int n_features = 0;
    int n_classes = 0;
    std::vector<double> classes;
    bool fitted = false;
    std::vector<std::string> field_names;
    std::string target_field;
    mutable std::mutex mtx;

    //! Kernel function
    DLLLOCAL double kernelFunc(const RowVectorXd& a, const RowVectorXd& b) const;

    //! Train a binary SVM via simplified SMO
    DLLLOCAL BinarySVM trainBinary(const MatrixXd& X, const VectorXd& y,
        double pos_class, double neg_class, ExceptionSink* xsink);

    //! Predict with a binary model: returns decision value
    DLLLOCAL double predictBinary(const BinarySVM& model,
        const RowVectorXd& point) const;

    DLLLOCAL QoreHashNode* predictInternal(const RowVectorXd& point,
        ExceptionSink* xsink) const;
};

#endif
