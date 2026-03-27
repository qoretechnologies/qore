# -*- mode: qore; indent-tabs-mode: nil -*-

/*  PermutationImportance.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see PermutationImportance
public namespace QoreMLUtils {

#! Model-agnostic feature importance via column-shuffling
/** Measures how much a model's score degrades when each feature column is randomly
    shuffled. Features that cause the largest score drop when shuffled are the most
    important.

    This method works with **any** fitted model — it only requires a predictor closure
    and a scorer closure, making it fully model-agnostic.

    @par Example:
    @code{.py}
%requires QoreMLUtils
%requires ml

ML::LinearRegression lr({"fit_intercept": True});
lr.fitMatrix(X, y);

code<list<auto>(list<auto>)> predictor = list<auto> sub (list<auto> X_data) {
    return map $1.prediction, lr.predictMatrix(X_data);
};

PermutationImportance pi({"n_repeats": 10, "seed": 42});
hash<auto> result = pi.compute(predictor, X, y,
    \MLScoringUtils::r2Scorer(), ("feature_a", "feature_b", "feature_c"));

printf("Baseline: %f\n", result.baseline_score);
foreach string fname in (keys result.importances) {
    printf("%s: mean=%.4f std=%.4f\n", fname,
        result.importances{fname}.importance_mean,
        result.importances{fname}.importance_std);
}
    @endcode
*/
public class PermutationImportance {
    private {
        #! Number of times to shuffle each feature
        int n_repeats;

        #! Random seed for reproducibility
        int seed;
    }

    #! Creates a PermutationImportance instance
    /** @param options optional hash with:
        - \c n_repeats (int): number of times to shuffle each feature (default: `5`)
        - \c seed (int): random seed for reproducibility (default: `0` = random)
    */
    constructor(*hash<auto> options) {
        n_repeats = options.n_repeats ?? 5;
        seed = options.seed ?? 0;
    }

    #! Computes permutation importance for each feature
    /** @param predictor closure `(list X) -> list` that returns predictions for the given data
        @param X feature matrix (list of lists)
        @param y true target values
        @param scorer closure `(list y_true, list y_pred) -> float` where higher is better
        @param feature_names optional list of feature names; auto-generated as
          `"feature_0"`, `"feature_1"`, ... if not provided
        @return hash with:
        - \c baseline_score (float): model score on unshuffled data
        - \c importances (hash): maps feature names to hashes with
          \c importance_mean and \c importance_std (mean and std of score drops)

        @throw PERMUTATION-IMPORTANCE-ERROR if data is empty or dimensions mismatch
    */
    hash<auto> compute(code<list<auto> (list<auto>)> predictor, list<auto> X, list<auto> y,
            code<float (list<auto>, list<auto>)> scorer, *list<string> feature_names) {
        if (!X.size()) {
            throw "PERMUTATION-IMPORTANCE-ERROR", "empty feature matrix";
        }
        int n_samples = X.size();
        int n_features = X[0].size();

        if (!feature_names) {
            feature_names = map sprintf("feature_%d", $1), xrange(n_features);
        }
        if (feature_names.size() != n_features) {
            throw "PERMUTATION-IMPORTANCE-ERROR",
                sprintf("feature_names has %d entries but data has %d features",
                    feature_names.size(), n_features);
        }

        # Compute baseline score
        list<auto> y_pred_base = predictor(X);
        float baseline_score = scorer(y, y_pred_base);

        # Seed the RNG for reproducibility
        srand(seed > 0 ? seed : now_us().toInt());

        # For each feature, shuffle and measure score drop
        hash<auto> importances();

        for (int j = 0; j < n_features; ++j) {
            list<float> score_drops();

            for (int r = 0; r < n_repeats; ++r) {
                # Create shuffled copy of X with column j permuted
                list<auto> X_perm = shuffleColumn(X, j, n_samples);
                list<auto> y_pred_perm = predictor(X_perm);
                float perm_score = scorer(y, y_pred_perm);
                push score_drops, baseline_score - perm_score;
            }

            importances{feature_names[j]} = {
                "importance_mean": meanList(score_drops),
                "importance_std": stdList(score_drops),
            };
        }

        return {
            "baseline_score": baseline_score,
            "importances": importances,
        };
    }

    #! Shuffles column j of matrix X using Fisher-Yates
    private static list<auto> shuffleColumn(list<auto> X, int j, int n_samples) {
        # Deep copy the column values
        list<auto> col_values = map $1[j], X;

        # Fisher-Yates shuffle using Qore's built-in RNG
        for (int i = n_samples - 1; i > 0; --i) {
            int swap_idx = rand() % (i + 1);
            if (swap_idx != i) {
                auto tmp = col_values[i];
                col_values[i] = col_values[swap_idx];
                col_values[swap_idx] = tmp;
            }
        }

        # Build new matrix with shuffled column
        list<auto> X_perm();
        for (int i = 0; i < n_samples; ++i) {
            list<auto> row = X[i] + ();  # shallow copy of row
            row[j] = col_values[i];
            push X_perm, row;
        }
        return X_perm;
    }

    #! Computes mean of a list of floats
    private static float meanList(list<float> values) {
        float sum = 0.0;
        foreach float v in (values) {
            sum += v;
        }
        return sum / values.size();
    }

    #! Computes standard deviation of a list of floats
    private static float stdList(list<float> values) {
        float m = PermutationImportance::meanList(values);
        float sq_sum = 0.0;
        foreach float v in (values) {
            sq_sum += (v - m) * (v - m);
        }
        return sqrt(sq_sum / values.size());
    }
}

} # namespace QoreMLUtils
