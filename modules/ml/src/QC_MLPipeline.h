/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_MLPipeline.h

    Qore ml module - MLPipeline class

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

#ifndef _QORE_MODULE_ML_QC_MLPIPELINE_H
#define _QORE_MODULE_ML_QC_MLPIPELINE_H

#include "ml_common.h"
#include "QoreMLModel.h"
#include "QC_AbstractMLModel.h"
#include "QC_StandardScaler.h"
#include "QC_MinMaxScaler.h"
#include "QC_Imputer.h"
#include "QC_LinearRegression.h"
#include "QC_KMeans.h"
#include "QC_PCA.h"
#include "QC_IsolationForest.h"
#include "QC_LOF.h"
#include "QC_GMM.h"
#include "QC_LogisticRegression.h"
#include "QC_KNN.h"

#include <mutex>
#include <string>
#include <variant>

DLLEXPORT extern qore_classid_t CID_MLPIPELINE;
DLLLOCAL extern QoreClass* QC_MLPIPELINE;

DLLLOCAL void preinitMLPipelineClass();
DLLLOCAL QoreClass* initMLPipelineClass(QoreNamespace& ns);

//! Pipeline step type
enum class PipelineStepType {
    STANDARD_SCALER,
    MINMAX_SCALER,
    IMPUTER,
    PCA,
    // Estimators (must be last step)
    LINEAR_REGRESSION,
    KMEANS,
    ISOLATION_FOREST,
    LOF,
    GMM,
    LOGISTIC_REGRESSION,
    KNN_CLASSIFICATION
};

//! A single pipeline step
struct PipelineStep {
    std::string name;
    PipelineStepType type;
    //! the step object; see QoreMLPipeline for which reference keeps it
    QoreObject* obj;
};

//! MLPipeline — chains preprocessors and an estimator into a single fit/predict unit
/** While the MLPipeline object exists, the references to the step objects are held by its \c steps internal member,
    where the deterministic garbage collector can follow them (see design/dgc.md, Pattern A), so that a cycle through a
    step that refers to its pipeline is collected; the steps vector only points to them.  Before the private data is
    attached to an object (when it is restored from serialized data) and after the object is destroyed (while a method
    still references the private data), the private data holds the references itself.
*/
class QoreMLPipeline : public QoreMLModel {
public:
    DLLLOCAL QoreMLPipeline() {}

    //! releases the references to the step objects if they are held here
    DLLLOCAL virtual void deref(ExceptionSink* xsink) override {
        if (ROdereference()) {
            if (owns_steps) {
                for (auto& step : steps) {
                    if (step.obj) {
                        step.obj->deref(xsink);
                        step.obj = nullptr;
                    }
                }
            }
            delete this;
        }
    }

    //! Attaches the private data to its MLPipeline object, whose steps internal member takes over the references
    /** @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int attach(QoreObject* self, ExceptionSink* xsink);

    //! Called when the MLPipeline object is destroyed; takes over the references to the step objects
    /** The steps internal member that references the step objects is released with the object, but the private data
        can be referenced longer by a method still running
    */
    DLLLOCAL void destructor(ExceptionSink* xsink);

    //! Add a step to the pipeline
    /** @param self the MLPipeline object, whose steps internal member references the step object, or nullptr if the
        private data is not yet attached to an object, in which case it references the step object itself

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int addStep(const std::string& name, PipelineStepType type, QoreObject* obj, QoreObject* self,
        ExceptionSink* xsink);

    //! Fit all steps: transformers are fit+transform sequentially, estimator is fit last
    DLLLOCAL void fitMatrix(const MatrixXd& X, const VectorXd* y, ExceptionSink* xsink);

    //! Transform through all transformer steps (no estimator)
    DLLLOCAL MatrixXd transformMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

    //! Predict: transform through all transformers, then call estimator.predict
    DLLLOCAL QoreHashNode* predict(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! PredictMatrix: transform + estimator.predictMatrix
    DLLLOCAL QoreListNode* predictMatrix(const MatrixXd& X, ExceptionSink* xsink) const;

    //! Whether the pipeline has been fitted
    DLLLOCAL bool isFitted() const override { return fitted; }

    //! Algorithm tag used by the serialization framework
    DLLLOCAL std::string getAlgorithmName() const override { return "MLPipeline"; }

    //! Get the number of steps
    DLLLOCAL int getNumSteps() const { return static_cast<int>(steps.size()); }

    //! Get step names
    DLLLOCAL QoreListNode* getStepNames(ExceptionSink* xsink) const;

    //! Serialize the pipeline (each step packed by algorithm tag + bytes)
    DLLLOCAL std::vector<uint8_t> serializeState() const override;

    //! Deserialize a pipeline blob; constructs child QoreObjects via getProgram()
    DLLLOCAL static QoreMLPipeline* deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink);

private:
    std::vector<PipelineStep> steps;
    bool fitted = false;
    //! true if the references to the step objects are held here instead of by the object's steps member
    bool owns_steps = true;
    mutable std::mutex mtx;

    //! Sets the steps internal member of the MLPipeline object to the step objects; must be called with the lock held
    DLLLOCAL int setStepsMember(QoreObject* self, ExceptionSink* xsink);

    //! Check if a step type is a transformer (not an estimator)
    DLLLOCAL static bool isTransformer(PipelineStepType type);

    //! Apply a single transformer step to data
    DLLLOCAL MatrixXd applyTransformer(const PipelineStep& step, const MatrixXd& data,
        ExceptionSink* xsink) const;

    //! Fit a single transformer step and return transformed data
    DLLLOCAL MatrixXd fitTransformer(const PipelineStep& step, const MatrixXd& data,
        ExceptionSink* xsink);
};

#endif // _QORE_MODULE_ML_QC_MLPIPELINE_H
