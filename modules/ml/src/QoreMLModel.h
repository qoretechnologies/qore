/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreMLModel.h

    Qore ml module - abstract base for serializable ML models

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

#ifndef _QORE_MODULE_ML_QOREMLMODEL_H
#define _QORE_MODULE_ML_QOREMLMODEL_H

#include <qore/Qore.h>

#include <string>
#include <vector>
#include <cstdint>

//! Abstract C++ base for all serializable ML model private classes.
/** Provides the four virtual methods that the generic ml_serialize / ml_deserialize /
    ml_save_model / ml_load_model functions dispatch on.

    Concrete model classes (QoreKMeans, QoreLinearRegression, ...) inherit from this in
    place of AbstractPrivateData and override the abstract methods.

    getFieldNames() is non-pure so models without a hash-record API (e.g. QoreHoltWinters)
    do not need to override it - the default returns an empty vector.
*/
class QoreMLModel : public AbstractPrivateData {
public:
    //! Returns true if the model has been fitted and is ready for prediction/transformation
    virtual bool isFitted() const = 0;

    //! Returns the algorithm name used as the serialization tag (e.g. "KMeans", "Ridge")
    virtual std::string getAlgorithmName() const = 0;

    //! Serializes the fitted model state to a binary blob
    virtual std::vector<uint8_t> serializeState() const = 0;

    //! Returns the field names used as columns for hash-record input; default empty
    virtual std::vector<std::string> getFieldNames() const { return {}; }
};

#endif // _QORE_MODULE_ML_QOREMLMODEL_H
