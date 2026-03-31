# -*- mode: qore; indent-tabs-mode: nil -*-

/*  MLScoringUtils.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see MLScoringUtils
public namespace QoreMLUtils {

#! Default scoring functions for ML model evaluation
/** Provides standard scoring functions that can be used with GridSearch, RandomSearch,
    and PermutationImportance. Each scorer takes two lists — true values and predicted
    values — and returns a float score where higher is better.

    @par Example:
    @code{.py}
    # Use with GridSearch
    GridSearch gs({
        "model_factory": \my_factory(),
        "param_grid": {"alpha": (0.1, 1.0, 10.0)},
        "scorer": \MLScoringUtils::r2Scorer(),
    });
    @endcode
*/
public class MLScoringUtils {
    #! R-squared scorer for regression (higher is better; 1.0 = perfect)
    /** @param y_true list of actual target values
        @param y_pred list of predicted values
        @return R-squared coefficient of determination

        @par Example:
        @code{.py}
        float score = MLScoringUtils::r2Scorer((1.0, 2.0, 3.0), (1.1, 2.0, 2.9));
        @endcode
    */
    static float r2Scorer(list<auto> y_true, list<auto> y_pred) {
        return ML::ml_r2_score(y_true, y_pred);
    }

    #! Mean Squared Error scorer for regression (negated so higher is better)
    /** @param y_true list of actual target values
        @param y_pred list of predicted values
        @return negative MSE (higher is better; 0.0 = perfect)
    */
    static float negMseScorer(list<auto> y_true, list<auto> y_pred) {
        return -ML::ml_mse(y_true, y_pred);
    }

    #! Accuracy scorer for classification (higher is better; 1.0 = perfect)
    /** @param y_true list of actual class labels
        @param y_pred list of predicted class labels
        @return accuracy as fraction of correct predictions

        @par Example:
        @code{.py}
        float score = MLScoringUtils::accuracyScorer((0, 1, 1, 0), (0, 1, 0, 0));
        # returns 0.75
        @endcode
    */
    static float accuracyScorer(list<auto> y_true, list<auto> y_pred) {
        return ML::ml_accuracy(y_true, y_pred);
    }
}

} # namespace QoreMLUtils
