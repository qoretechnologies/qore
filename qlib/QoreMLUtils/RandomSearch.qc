# -*- mode: qore; indent-tabs-mode: nil -*-

/*  RandomSearch.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see RandomSearch
public namespace QoreMLUtils {

#! Random hyperparameter search with k-fold cross-validation
/** Randomly samples parameter combinations from a distribution and evaluates
    them with cross-validation. More efficient than GridSearch when the parameter
    space is large, as it tries a fixed number of random combinations.

    @par Example:
    @code{.py}
%requires QoreMLUtils
%requires ml

code<object(hash<auto>, list<auto>, list<auto>)> model_factory =
    object sub (hash<auto> params, list<auto> X_train, list<auto> y_train) {
        ML::DecisionTree model(params);
        model.fitMatrix(X_train, y_train);
        return model;
    };

code<list<auto>(object, list<auto>)> predictor =
    list<auto> sub (object model, list<auto> X_test) {
        return map $1.predicted_class,
            cast<ML::DecisionTree>(model).predictClassificationMatrix(X_test);
    };

RandomSearch rs({
    "model_factory": model_factory,
    "predictor": predictor,
    "param_distributions": {
        "max_depth": (3, 5, 7, 10, 15),
        "min_samples_split": (2, 5, 10, 20),
        "criterion": ("gini", "entropy"),
        "task": ("classification",),
    },
    "scorer": \MLScoringUtils::accuracyScorer(),
    "n_iter": 20,
    "seed": 42,
});

hash<auto> result = rs.fitMatrix(X, y);
printf("Best params: %y\n", result.best_params);
    @endcode
*/
public class RandomSearch {
    private {
        #! Closure that creates and fits a model
        code<object (hash<auto>, list<auto>, list<auto>)> model_factory;

        #! Closure that extracts predictions
        code<list<auto> (object, list<auto>)> predictor;

        #! Hash mapping parameter names to lists of candidate values
        hash<auto> param_distributions;

        #! Closure that scores predictions
        code<float (list<auto>, list<auto>)> scorer;

        #! Number of random combinations to evaluate
        int n_iter;

        #! Number of cross-validation folds
        int n_folds;

        #! Whether to shuffle data before splitting
        bool shuffle;

        #! Random seed
        int seed;
    }

    #! Creates a RandomSearch instance
    /** @param options a hash with the following keys:
        - \c model_factory (code): closure `(hash params, list X_train, list y_train) -> model`
        - \c predictor (code): closure `(object model, list X_test) -> list`
        - \c param_distributions (hash): maps parameter names to lists of candidate values to
          sample from, e.g. `{"max_depth": (3, 5, 7, 10), "criterion": ("gini", "entropy")}`
        - \c scorer (code): closure `(list y_true, list y_pred) -> float` where higher is better
        - \c n_iter (int): number of random parameter combinations to try (default: `10`)
        - \c n_folds (int): number of cross-validation folds (default: `5`)
        - \c shuffle (bool): whether to shuffle data before splitting (default: `True`)
        - \c seed (int): random seed for reproducibility (default: `0` = random)

        @throw RANDOM-SEARCH-ERROR if required options are missing
    */
    constructor(hash<auto> options) {
        if (!exists options.model_factory) {
            throw "RANDOM-SEARCH-ERROR", "missing required option 'model_factory'";
        }
        if (!exists options.predictor) {
            throw "RANDOM-SEARCH-ERROR", "missing required option 'predictor'";
        }
        if (!exists options.param_distributions || !options.param_distributions.size()) {
            throw "RANDOM-SEARCH-ERROR", "missing or empty 'param_distributions'";
        }
        if (!exists options.scorer) {
            throw "RANDOM-SEARCH-ERROR", "missing required option 'scorer'";
        }
        model_factory = options.model_factory;
        predictor = options.predictor;
        param_distributions = options.param_distributions;
        scorer = options.scorer;
        n_iter = options.n_iter ?? 10;
        n_folds = options.n_folds ?? 5;
        shuffle = options.shuffle ?? True;
        seed = options.seed ?? 0;
    }

    #! Runs random search with matrix-format data
    /** @param X feature matrix (list of lists)
        @param y target vector (list of values)
        @return hash with:
        - \c best_params (hash): the parameter combination with the highest mean score
        - \c best_score (float): the mean cross-validated score of the best parameters
        - \c cv_results (list): list of hashes with \c params, \c mean_score, \c std_score,
          \c fold_scores for every parameter combination tested

        @throw RANDOM-SEARCH-ERROR if data is empty
    */
    hash<auto> fitMatrix(list<auto> X, list<auto> y) {
        if (!X.size() || !y.size()) {
            throw "RANDOM-SEARCH-ERROR", "empty training data";
        }
        if (X.size() != y.size()) {
            throw "RANDOM-SEARCH-ERROR",
                sprintf("X has %d rows but y has %d elements", X.size(), y.size());
        }

        # Generate random parameter combinations
        list<hash<auto>> combinations = sampleCombinations(param_distributions, n_iter, seed);

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
                list<auto> X_train = map X[$1], fold.train_indices;
                list<auto> y_train = map y[$1], fold.train_indices;
                list<auto> X_test = map X[$1], fold.test_indices;
                list<auto> y_test = map y[$1], fold.test_indices;

                object model = model_factory(params, X_train, y_train);
                list<auto> y_pred = predictor(model, X_test);
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

    #! Samples n_iter random parameter combinations
    private static list<hash<auto>> sampleCombinations(hash<auto> distributions,
            int n_iter, int seed) {
        list<string> param_keys = distributions.keys();

        # Use a deterministic sequence if seed > 0
        int rng_state = seed > 0 ? seed : now_us().toInt();

        list<hash<auto>> result();
        for (int i = 0; i < n_iter; ++i) {
            hash<auto> combo();
            foreach string key in (param_keys) {
                list<auto> candidates = distributions{key};
                # Simple LCG-based index selection for determinism
                rng_state = (rng_state * 1103515245 + 12345) & 0x7FFFFFFF;
                int idx = rng_state % candidates.size();
                combo{key} = candidates[idx];
            }
            push result, combo;
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
        float m = RandomSearch::meanList(values);
        float sq_sum = 0.0;
        foreach float v in (values) {
            sq_sum += (v - m) * (v - m);
        }
        return sqrt(sq_sum / values.size());
    }
}

} # namespace QoreMLUtils
