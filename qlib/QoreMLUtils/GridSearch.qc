# -*- mode: qore; indent-tabs-mode: nil -*-

/*  GridSearch.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see GridSearch
public namespace QoreMLUtils {

#! Exhaustive hyperparameter search with k-fold cross-validation
/** Evaluates every combination of parameters in a grid, scoring each with
    cross-validation, and returns the best-performing parameter set.

    @par Example:
    @code{.py}
%requires QoreMLUtils
%requires ml

# Define a model factory that creates and fits a Ridge model
code model_factory = sub (hash<auto> params, list<auto> X_train, list<auto> y_train) {
    ML::Ridge model(params);
    model.fitMatrix(X_train, y_train);
    return model;
};

# Define a prediction extractor
code predictor = sub (object model, list<auto> X_test) {
    return map $1.prediction, model.predictMatrix(X_test);
};

GridSearch gs({
    "model_factory": model_factory,
    "predictor": predictor,
    "param_grid": {"alpha": (0.01, 0.1, 1.0, 10.0), "normalize": (True, False)},
    "scorer": \MLScoringUtils::r2Scorer(),
    "n_folds": 5,
    "seed": 42,
});

hash<auto> result = gs.fitMatrix(X, y);
printf("Best params: %y\n", result.best_params);
printf("Best score: %f\n", result.best_score);
    @endcode
*/
public class GridSearch {
    private {
        #! Closure that creates and fits a model: (hash params, list X_train, list y_train) -> object
        code<object (hash<auto>, list<auto>, list<auto>)> model_factory;

        #! Closure that extracts predictions: (object model, list X_test) -> list<auto>
        code<list<auto> (object, list<auto>)> predictor;

        #! Hash mapping parameter names to lists of values to try
        hash<auto> param_grid;

        #! Closure that scores predictions: (list y_true, list y_pred) -> float
        code<float (list<auto>, list<auto>)> scorer;

        #! Number of cross-validation folds
        int n_folds;

        #! Whether to shuffle data before splitting
        bool shuffle;

        #! Random seed for reproducibility
        int seed;
    }

    #! Creates a GridSearch instance
    /** @param options a hash with the following keys:
        - \c model_factory (code): closure `(hash params, list X_train, list y_train) -> model` that
          creates and fits a model with the given parameters on the training data
        - \c predictor (code): closure `(object model, list X_test) -> list` that returns predictions
        - \c param_grid (hash): maps parameter names to lists of values, e.g.
          `{"alpha": (0.01, 0.1, 1.0), "normalize": (True, False)}`
        - \c scorer (code): closure `(list y_true, list y_pred) -> float` where higher is better
        - \c n_folds (int): number of cross-validation folds (default: `5`)
        - \c shuffle (bool): whether to shuffle data before splitting (default: `True`)
        - \c seed (int): random seed for reproducibility (default: `0` = random)

        @throw GRID-SEARCH-ERROR if required options are missing or param_grid is empty
    */
    constructor(hash<auto> options) {
        if (!exists options.model_factory) {
            throw "GRID-SEARCH-ERROR", "missing required option 'model_factory'";
        }
        if (!exists options.predictor) {
            throw "GRID-SEARCH-ERROR", "missing required option 'predictor'";
        }
        if (!exists options.param_grid || !options.param_grid.size()) {
            throw "GRID-SEARCH-ERROR", "missing or empty 'param_grid'";
        }
        if (!exists options.scorer) {
            throw "GRID-SEARCH-ERROR", "missing required option 'scorer'";
        }
        model_factory = options.model_factory;
        predictor = options.predictor;
        param_grid = options.param_grid;
        scorer = options.scorer;
        n_folds = options.n_folds ?? 5;
        shuffle = options.shuffle ?? True;
        seed = options.seed ?? 0;
    }

    #! Runs grid search with matrix-format data
    /** @param X feature matrix (list of lists)
        @param y target vector (list of values)
        @return hash with:
        - \c best_params (hash): the parameter combination with the highest mean score
        - \c best_score (float): the mean cross-validated score of the best parameters
        - \c cv_results (list): list of hashes with \c params, \c mean_score, \c std_score,
          \c fold_scores for every parameter combination tested

        @throw GRID-SEARCH-ERROR if data is empty or too small for the number of folds
    */
    hash<auto> fitMatrix(list<auto> X, list<auto> y) {
        if (!X.size() || !y.size()) {
            throw "GRID-SEARCH-ERROR", "empty training data";
        }
        if (X.size() != y.size()) {
            throw "GRID-SEARCH-ERROR",
                sprintf("X has %d rows but y has %d elements", X.size(), y.size());
        }

        # Generate all parameter combinations
        list<hash<auto>> combinations = cartesianProduct(param_grid);
        if (!combinations.size()) {
            throw "GRID-SEARCH-ERROR", "param_grid produced no combinations";
        }

        # Create cross-validation splits
        ML::CrossValidator cv({"n_folds": n_folds, "shuffle": shuffle, "seed": seed});
        list<hash<auto>> folds = cv.split(X.size());

        # Evaluate each combination
        list<hash<auto>> cv_results();
        *hash<auto> best_params;
        float best_score;

        foreach hash<auto> params in (combinations) {
            list<float> fold_scores();

            foreach hash<auto> fold in (folds) {
                # Split data
                list<auto> X_train = map X[$1], fold.train_indices;
                list<auto> y_train = map y[$1], fold.train_indices;
                list<auto> X_test = map X[$1], fold.test_indices;
                list<auto> y_test = map y[$1], fold.test_indices;

                # Train and predict
                object model = model_factory(params, X_train, y_train);
                list<auto> y_pred = predictor(model, X_test);

                # Score
                float score = scorer(y_test, y_pred);
                push fold_scores, score;
            }

            float mean_score = meanList(fold_scores);
            float std_score = stdList(fold_scores);

            push cv_results, {
                "params": params,
                "mean_score": mean_score,
                "std_score": std_score,
                "fold_scores": fold_scores,
            };

            if (!exists best_score || mean_score > best_score) {
                best_score = mean_score;
                best_params = params;
            }
        }

        return {
            "best_params": best_params,
            "best_score": best_score,
            "cv_results": cv_results,
        };
    }

    #! Generates the Cartesian product of parameter lists
    private static list<hash<auto>> cartesianProduct(hash<auto> grid) {
        list<string> param_names = grid.keys();
        if (!param_names.size()) {
            return ();
        }

        list<hash<auto>> result = (new hash<auto>(),);

        foreach string pname in (param_names) {
            list<auto> pvalues = grid{pname};
            list<hash<auto>> new_result();
            foreach hash<auto> combo in (result) {
                foreach auto val in (pvalues) {
                    hash<auto> new_combo = combo + {pname: val};
                    push new_result, new_combo;
                }
            }
            result = new_result;
        }

        return result;
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
        float m = GridSearch::meanList(values);
        float sq_sum = 0.0;
        foreach float v in (values) {
            sq_sum += (v - m) * (v - m);
        }
        return sqrt(sq_sum / values.size());
    }
}

} # namespace QoreMLUtils
