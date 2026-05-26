# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OnnxModelTools.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see QoreOnnxTools
public namespace QoreOnnxTools {

#! Structured ONNX model validation issue
public hashdecl OnnxValidationIssue {
    #! Issue severity: \c error, \c warning, or \c info
    string level;
    #! Stable issue code
    string code;
    #! Human-readable issue message
    string message;
    #! Related input/output/provider name, if applicable
    *string field;
    #! Suggested remediation for the issue, if applicable
    *string hint;
    #! Example command or code snippet showing a related action, if applicable
    *string example;
}

#! ONNX model validation result
public hashdecl OnnxValidationResult {
    #! Model path
    string path;
    #! True if no \c error level issues were found
    bool valid;
    #! Structured inspection metadata, if the model could be loaded
    *hash<auto> inspection;
    #! Effective provider report, if the model could be loaded
    *hash<auto> provider_report;
    #! Compact actionable diagnostics summary
    *hash<auto> diagnostics;
    #! Validation issues
    list<hash<OnnxValidationIssue>> issues;
}

#! ONNX benchmark options
public hashdecl OnnxBenchmarkOptions {
    #! Number of warmup runs before timing
    int warmup_runs = 1;
    #! Number of timed runs
    int runs = 10;
    #! Optional ONNX session configuration
    *hash<ML::OnnxSessionConfig> session_config;
    #! Optional input hash; when omitted, default single-row inputs are generated from model metadata
    *hash<auto> inputs;
    #! Return the final inference output in the result
    bool keep_output = False;
}

#! ONNX benchmark result
public hashdecl OnnxBenchmarkResult {
    #! Model path
    string path;
    #! Number of warmup runs performed
    int warmup_runs;
    #! Number of timed runs performed
    int runs;
    #! Total elapsed time across timed runs
    float total_ms;
    #! Average elapsed time per timed run
    float average_ms;
    #! Fastest timed run
    float min_ms;
    #! Slowest timed run
    float max_ms;
    #! Per-run latency values in milliseconds
    list<float> latencies_ms;
    #! Inference stats reported by ML::OnnxModel
    hash<auto> stats;
    #! Input hash used for the benchmark
    hash<auto> inputs;
    #! Final output, if \c keep_output is True
    *hash<auto> output;
}

#! Difference between source and candidate ONNX outputs
public hashdecl OnnxOutputDifference {
    #! Output path, for example \c Y[0][2]
    string path;
    #! Stable difference code
    string code;
    #! Human-readable difference message
    string message;
    #! Source value
    auto expected;
    #! Candidate value
    auto actual;
    #! Absolute error for numeric comparisons
    *float abs_error;
    #! Relative error for numeric comparisons
    *float rel_error;
}

#! Options for comparing two ONNX model artifacts
public hashdecl OnnxCompareOptions {
    #! Optional source model session configuration
    *hash<ML::OnnxSessionConfig> source_session_config;
    #! Optional candidate model session configuration
    *hash<ML::OnnxSessionConfig> candidate_session_config;
    #! Optional input hash; when omitted, default single-row inputs are generated from source metadata
    *hash<auto> inputs;
    #! Absolute tolerance for numeric output comparisons
    float abs_tolerance = 0.0001;
    #! Relative tolerance for numeric output comparisons
    float rel_tolerance = 0.0001;
    #! Also benchmark both models with the comparison input
    bool benchmark = False;
    #! Number of warmup runs when \c benchmark is True
    int warmup_runs = 1;
    #! Number of timed runs when \c benchmark is True
    int runs = 10;
    #! Include source and candidate output hashes in the result
    bool keep_outputs = False;
}

#! Result of comparing two ONNX model artifacts
public hashdecl OnnxCompareResult {
    #! Source model path
    string source_path;
    #! Candidate model path
    string candidate_path;
    #! True if all comparable outputs match within tolerance and no structural differences were found
    bool outputs_match;
    #! Number of scalar values compared
    int compared_values = 0;
    #! Maximum absolute error seen in numeric output comparisons
    float max_abs_error = 0.0;
    #! Maximum relative error seen in numeric output comparisons
    float max_rel_error = 0.0;
    #! Structured output differences
    list<hash<OnnxOutputDifference>> differences = ();
    #! Input hash used for the comparison
    hash<auto> inputs;
    #! Source output, when requested
    *hash<auto> source_output;
    #! Candidate output, when requested
    *hash<auto> candidate_output;
    #! Source benchmark, when requested
    *hash<OnnxBenchmarkResult> source_benchmark;
    #! Candidate benchmark, when requested
    *hash<OnnxBenchmarkResult> candidate_benchmark;
    #! Candidate/source average latency ratio, when benchmarking was requested
    *float latency_ratio;
    #! Source/candidate average latency speedup, when benchmarking was requested
    *float speedup;
    #! Candidate average latency minus source average latency in milliseconds
    *float latency_delta_ms;
}

#! ONNX optimization options
public hashdecl OnnxOptimizationOptions {
    #! Optional ONNX session configuration used while creating the optimized artifact
    *hash<ML::OnnxSessionConfig> session_config;
    #! Save optimized artifact in ORT format instead of optimized ONNX format
    bool ort_format = False;
    #! Allow overwriting an existing output file
    bool force = False;
    #! Validate the optimized artifact by loading it after writing
    bool validate = True;
}

#! ONNX optimization result
public hashdecl OnnxOptimizationResult {
    #! Source ONNX model path
    string input_path;
    #! Optimized model path
    string output_path;
    #! True if the optimized artifact was requested in ORT format
    bool ort_format;
    #! Raw optimization metadata returned by the ml module
    hash<auto> optimization;
    #! Validation result for the optimized artifact, if validation was requested
    *hash<OnnxValidationResult> validation;
}

/** Model inspection, validation, optimization, and benchmark helpers for ONNX
    deployment workflows.

    These helpers wrap the raw \c ML::OnnxModel APIs with deployment-oriented
    result records, default input generation, provider diagnostics, and CLI
    friendly output structures.
*/
public class OnnxModelTools {
    /** Inspects an ONNX or ORT artifact by loading it through \c ML::OnnxModel.

        @param path model artifact path
        @param session_config optional ONNX Runtime session configuration
        @return structured metadata including size, model info, inputs, outputs,
        provider diagnostics, and effective provider report
        @throw QORE-ONNX-TOOLS-ERROR if \a path is missing or not a regular file
        @throw ML-ONNX-ERROR if ONNX Runtime cannot load the model

        @par Example

        @code
hash<auto> info = QoreOnnxTools::OnnxModelTools::inspect(
    "/models/churn.onnx");
log(LL_INFO, "active provider: %s", info.provider_report.active_provider);
log(LL_INFO, "recommended actions: %y",
    info.diagnostics.recommended_actions);
        @endcode
    */
    static hash<auto> inspect(string path, *hash<ML::OnnxSessionConfig> session_config) {
        validateModelPath(path);
        hash<ML::OnnxSessionConfig> cfg = normalizeSessionConfig(session_config);
        ML::OnnxModel model(path, cfg);
        hash<ML::OnnxModelInfo> info = model.getModelInfo();
        hash<auto> rv = {
            "path": path,
            "size": hstat(path).size,
            "info": info,
            "inputs": model.getInputInfo(),
            "outputs": model.getOutputInfo(),
            "providers": model.getProviders(),
            "requested_providers": model.getRequestedProviders(),
            "provider_diagnostics": model.getProviderDiagnostics(),
            "provider_report": model.getEffectiveProviderReport(),
        };
        rv.diagnostics = OnnxDiagnostics::summarizeInspection(rv);
        return rv;
    }

    /** Validates model loadability, provider policy, and tensor metadata.

        @param path model artifact path
        @param session_config optional ONNX Runtime session configuration
        @return validation result with structured warnings/errors

        @par Example

        @code
hash<QoreOnnxTools::OnnxValidationResult> result =
    QoreOnnxTools::OnnxModelTools::validate("/models/churn.onnx");
if (!result.valid) {
    throw "DEPLOYMENT-ERROR", result.diagnostics.recommended_actions;
}
        @endcode
    */
    static hash<OnnxValidationResult> validate(string path,
            *hash<ML::OnnxSessionConfig> session_config) {
        list<hash<OnnxValidationIssue>> issues = ();
        hash<OnnxValidationResult> rv = <OnnxValidationResult>{
            "path": path,
            "valid": False,
            "issues": issues,
        };

        if (!path.val()) {
            addIssue(\issues, "error", "empty-path", "model path must not be empty",
                NOTHING, "Pass a path to an ONNX or ORT artifact.",
                "qore-onnx validate model.onnx");
            rv.issues = issues;
            rv.diagnostics = OnnxDiagnostics::summarizeValidation(rv);
            return rv;
        }
        if (!is_file(path)) {
            addIssue(\issues, "error", "not-found",
                sprintf("model path %y does not exist or is not a regular file", path),
                NOTHING, "Check the model path and package directory before deployment.",
                "qore-onnx inspect model.onnx");
            rv.issues = issues;
            rv.diagnostics = OnnxDiagnostics::summarizeValidation(rv);
            return rv;
        }

        try {
            hash<auto> inspection = inspect(path, session_config);
            rv.inspection = inspection;
            rv.provider_report = inspection.provider_report;
            validateTensorList(\issues, "input", inspection.inputs);
            validateTensorList(\issues, "output", inspection.outputs);
            validateProviders(\issues, inspection.provider_diagnostics);
        } catch (hash<ExceptionInfo> ex) {
            addIssue(\issues, "error", "load-failed",
                sprintf("%s: %s", ex.err, ex.desc), NOTHING,
                "Verify ONNX Runtime compatibility, external-data files, and provider libraries.",
                "qore-onnx inspect --session-config session.json " + path);
        }

        rv.issues = issues;
        rv.valid = !hasErrors(issues);
        rv.diagnostics = OnnxDiagnostics::summarizeValidation(rv);
        return rv;
    }

    /** Saves an optimized ONNX or ORT artifact and optionally validates it.

        @param input_path source ONNX model path
        @param output_path optimized output model path
        @param options optional optimization options
        @return optimization result with raw ml-module metadata and validation
        details when requested
        @throw QORE-ONNX-TOOLS-ERROR if paths/options are invalid
        @throw ML-ONNX-ERROR if ONNX Runtime cannot write the optimized artifact
    */
    static hash<OnnxOptimizationResult> optimize(string input_path, string output_path,
            *hash<OnnxOptimizationOptions> options) {
        hash<OnnxOptimizationOptions> opts = options ?? <OnnxOptimizationOptions>{};
        validateModelPath(input_path);
        if (!output_path.val()) {
            throw "QORE-ONNX-TOOLS-ERROR", "output_path must not be empty";
        }
        if (input_path == output_path) {
            throw "QORE-ONNX-TOOLS-ERROR",
                "input_path and output_path must be different files";
        }
        if (is_file(output_path) && !opts.force) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("output file %y already exists; pass force: True to overwrite it",
                    output_path);
        }

        hash<ML::OnnxSessionConfig> cfg = normalizeSessionConfig(opts.session_config);
        hash<auto> info = ML::onnx_optimize_model(input_path, output_path, cfg,
            opts.ort_format);
        hash<OnnxOptimizationResult> rv = <OnnxOptimizationResult>{
            "input_path": input_path,
            "output_path": output_path,
            "ort_format": opts.ort_format,
            "optimization": info,
        };
        if (opts.validate) {
            hash<ML::OnnxSessionConfig> validation_cfg = normalizeSessionConfig(
                opts.session_config);
            if (opts.ort_format) {
                validation_cfg.load_model_format = "ORT";
            }
            rv.validation = validate(output_path, validation_cfg);
        }
        return rv;
    }

    /** Benchmarks model inference with caller-supplied or generated inputs.

        @param path model artifact path
        @param options optional benchmark options
        @return latency summary, per-run latency samples, model inference stats,
        and the inputs used for the run
        @throw QORE-ONNX-TOOLS-ERROR if run counts or model input metadata are
        invalid
        @throw ML-ONNX-ERROR if ONNX Runtime cannot load or execute the model
    */
    static hash<OnnxBenchmarkResult> benchmark(string path,
            *hash<OnnxBenchmarkOptions> options) {
        validateModelPath(path);
        hash<OnnxBenchmarkOptions> opts = options ?? <OnnxBenchmarkOptions>{};
        if (opts.warmup_runs < 0) {
            throw "QORE-ONNX-TOOLS-ERROR", "warmup_runs must be >= 0";
        }
        if (opts.runs <= 0) {
            throw "QORE-ONNX-TOOLS-ERROR", "runs must be > 0";
        }

        ML::OnnxModel model(path, normalizeSessionConfig(opts.session_config));
        hash<auto> inputs = opts.inputs ?? buildDefaultInputs(model.getInputInfo());
        for (int i = 0; i < opts.warmup_runs; ++i) {
            model.run(inputs);
        }

        list<float> latencies = ();
        *hash<auto> output;
        float total_ms = 0.0;
        float min_ms = 0.0;
        float max_ms = 0.0;
        for (int i = 0; i < opts.runs; ++i) {
            date start = now_us();
            output = model.run(inputs);
            float elapsed_ms = (now_us() - start).durationMicroseconds() / 1000.0;
            push latencies, elapsed_ms;
            total_ms += elapsed_ms;
            if (!i || elapsed_ms < min_ms) {
                min_ms = elapsed_ms;
            }
            if (!i || elapsed_ms > max_ms) {
                max_ms = elapsed_ms;
            }
        }

        hash<OnnxBenchmarkResult> rv = <OnnxBenchmarkResult>{
            "path": path,
            "warmup_runs": opts.warmup_runs,
            "runs": opts.runs,
            "total_ms": total_ms,
            "average_ms": total_ms / opts.runs,
            "min_ms": min_ms,
            "max_ms": max_ms,
            "latencies_ms": latencies,
            "stats": model.getInferenceStats(),
            "inputs": inputs,
        };
        if (opts.keep_output) {
            rv.output = output;
        }
        return rv;
    }

    /** Compares two ONNX/ORT model artifacts on the same input.

        This helper is designed for deployment validation, including original
        vs optimized and original vs quantized artifact checks. It verifies
        output structure, compares numeric scalar values with absolute and
        relative tolerances, and can optionally benchmark both artifacts with
        the same input hash.

        @param source_path reference model path
        @param candidate_path candidate model path
        @param options comparison options

        @return structured output-difference and optional latency comparison

        @throw QORE-ONNX-TOOLS-ERROR if paths/options are invalid
        @throw ML-ONNX-ERROR if either model cannot load or execute
    */
    static hash<OnnxCompareResult> compare(string source_path, string candidate_path,
            *hash<OnnxCompareOptions> options) {
        validateModelPath(source_path);
        validateModelPath(candidate_path);
        hash<OnnxCompareOptions> opts = options ?? <OnnxCompareOptions>{};
        if (opts.abs_tolerance < 0.0) {
            throw "QORE-ONNX-TOOLS-ERROR", "abs_tolerance must be >= 0";
        }
        if (opts.rel_tolerance < 0.0) {
            throw "QORE-ONNX-TOOLS-ERROR", "rel_tolerance must be >= 0";
        }
        if (opts.warmup_runs < 0) {
            throw "QORE-ONNX-TOOLS-ERROR", "warmup_runs must be >= 0";
        }
        if (opts.runs <= 0) {
            throw "QORE-ONNX-TOOLS-ERROR", "runs must be > 0";
        }

        ML::OnnxModel source(source_path, normalizeSessionConfig(opts.source_session_config));
        ML::OnnxModel candidate(candidate_path, normalizeSessionConfig(
            opts.candidate_session_config));
        hash<auto> inputs = opts.inputs ?? buildDefaultInputs(source.getInputInfo());
        hash<auto> source_output = source.run(inputs);
        hash<auto> candidate_output = candidate.run(inputs);

        list<hash<OnnxOutputDifference>> differences = ();
        hash<auto> stats = {
            "compared_values": 0,
            "max_abs_error": 0.0,
            "max_rel_error": 0.0,
        };
        compareValue(\differences, \stats, source_output, candidate_output, "",
            opts.abs_tolerance, opts.rel_tolerance);

        hash<OnnxCompareResult> rv = <OnnxCompareResult>{
            "source_path": source_path,
            "candidate_path": candidate_path,
            "outputs_match": !differences,
            "compared_values": stats.compared_values,
            "max_abs_error": stats.max_abs_error,
            "max_rel_error": stats.max_rel_error,
            "differences": differences,
            "inputs": inputs,
        };
        if (opts.keep_outputs) {
            rv.source_output = source_output;
            rv.candidate_output = candidate_output;
        }

        if (opts.benchmark) {
            hash<OnnxBenchmarkOptions> source_bopts = <OnnxBenchmarkOptions>{
                "warmup_runs": opts.warmup_runs,
                "runs": opts.runs,
                "inputs": inputs,
            };
            if (opts.source_session_config) {
                source_bopts.session_config = opts.source_session_config;
            }
            hash<OnnxBenchmarkOptions> candidate_bopts = <OnnxBenchmarkOptions>{
                "warmup_runs": opts.warmup_runs,
                "runs": opts.runs,
                "inputs": inputs,
            };
            if (opts.candidate_session_config) {
                candidate_bopts.session_config = opts.candidate_session_config;
            }
            rv.source_benchmark = benchmark(source_path, source_bopts);
            rv.candidate_benchmark = benchmark(candidate_path, candidate_bopts);
            rv.latency_delta_ms = rv.candidate_benchmark.average_ms
                - rv.source_benchmark.average_ms;
            if (rv.source_benchmark.average_ms > 0.0) {
                rv.latency_ratio = rv.candidate_benchmark.average_ms
                    / rv.source_benchmark.average_ms;
            }
            if (rv.candidate_benchmark.average_ms > 0.0) {
                rv.speedup = rv.source_benchmark.average_ms
                    / rv.candidate_benchmark.average_ms;
            }
        }

        return rv;
    }

    #! Builds default single-row input values from ONNX input metadata
    static hash<auto> buildDefaultInputs(list<hash<ML::OnnxTensorInfo>> inputs) {
        if (!inputs) {
            throw "QORE-ONNX-TOOLS-ERROR",
                "cannot build default benchmark inputs because the model has no inputs";
        }
        hash<auto> rv = {};
        foreach hash<ML::OnnxTensorInfo> input in (inputs) {
            rv{input.name} = makeTensorValue(input.type, input.shape, 0);
        }
        return rv;
    }

    private static hash<ML::OnnxSessionConfig> normalizeSessionConfig(
            *hash<ML::OnnxSessionConfig> session_config) {
        return session_config ?? <ML::OnnxSessionConfig>{"providers": ()};
    }

    private static nothing validateModelPath(string path) {
        if (!path.val()) {
            throw "QORE-ONNX-TOOLS-ERROR", "model path must not be empty";
        }
        if (!is_file(path)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("model path %y does not exist or is not a regular file", path);
        }
    }

    private static auto makeTensorValue(string type, list<int> shape, int dim) {
        if (dim >= shape.size()) {
            return defaultScalar(type);
        }
        int n = shape[dim] <= 0 ? 1 : shape[dim];
        list<auto> rv = ();
        for (int i = 0; i < n; ++i) {
            push rv, makeTensorValue(type, shape, dim + 1);
        }
        return rv;
    }

    private static auto defaultScalar(string type) {
        switch (type) {
            case "string":
                return "";
            case "bool":
                return False;
            case "float":
            case "double":
            case "float16":
            case "bfloat16":
                return 0.0;
            default:
                return 0;
        }
    }

    private static nothing compareValue(reference<list<hash<OnnxOutputDifference>>> differences,
            reference<hash<auto>> stats, auto expected, auto actual, string path,
            float abs_tolerance, float rel_tolerance) {
        int expected_type = expected.typeCode();
        int actual_type = actual.typeCode();
        if (isNumericType(expected_type) && isNumericType(actual_type)) {
            compareNumeric(\differences, \stats, expected, actual, path,
                abs_tolerance, rel_tolerance);
            return;
        }
        if (expected_type == NT_LIST || actual_type == NT_LIST) {
            if (expected_type != NT_LIST || actual_type != NT_LIST) {
                addDifference(\differences, path, "type-mismatch",
                    sprintf("expected type %d but candidate has type %d", expected_type,
                        actual_type), expected, actual);
                return;
            }
            if (expected.size() != actual.size()) {
                addDifference(\differences, path, "list-size-mismatch",
                    sprintf("expected list size %d but candidate has size %d",
                        expected.size(), actual.size()), expected, actual);
            }
            int n = expected.size() < actual.size() ? expected.size() : actual.size();
            for (int i = 0; i < n; ++i) {
                compareValue(\differences, \stats, expected[i], actual[i],
                    path + sprintf("[%d]", i), abs_tolerance, rel_tolerance);
            }
            return;
        }
        if (expected_type == NT_HASH || actual_type == NT_HASH) {
            if (expected_type != NT_HASH || actual_type != NT_HASH) {
                addDifference(\differences, path, "type-mismatch",
                    sprintf("expected type %d but candidate has type %d", expected_type,
                        actual_type), expected, actual);
                return;
            }
            foreach string key in (keys expected) {
                string child_path = path.val() ? path + "." + key : key;
                if (!actual.hasKey(key)) {
                    addDifference(\differences, child_path, "missing-key",
                        sprintf("candidate output is missing key %y", key),
                        expected{key}, NOTHING);
                    continue;
                }
                compareValue(\differences, \stats, expected{key}, actual{key},
                    child_path, abs_tolerance, rel_tolerance);
            }
            foreach string key in (keys actual) {
                if (!expected.hasKey(key)) {
                    string child_path = path.val() ? path + "." + key : key;
                    addDifference(\differences, child_path, "unexpected-key",
                        sprintf("candidate output has unexpected key %y", key),
                        NOTHING, actual{key});
                }
            }
            return;
        }
        ++stats.compared_values;
        if (expected != actual) {
            addDifference(\differences, path, "value-mismatch",
                "non-numeric output values are not equal", expected, actual);
        }
    }

    private static nothing compareNumeric(reference<list<hash<OnnxOutputDifference>>> differences,
            reference<hash<auto>> stats, auto expected, auto actual, string path,
            float abs_tolerance, float rel_tolerance) {
        float e = expected.toFloat();
        float a = actual.toFloat();
        float abs_error = abs(e - a);
        float denom = abs(e) > 1.0 ? abs(e) : 1.0;
        float rel_error = abs_error / denom;
        ++stats.compared_values;
        if (abs_error > stats.max_abs_error) {
            stats.max_abs_error = abs_error;
        }
        if (rel_error > stats.max_rel_error) {
            stats.max_rel_error = rel_error;
        }
        if (abs_error > abs_tolerance && rel_error > rel_tolerance) {
            hash<OnnxOutputDifference> diff = <OnnxOutputDifference>{
                "path": path,
                "code": "numeric-mismatch",
                "message": sprintf("numeric values differ by abs=%f rel=%f",
                    abs_error, rel_error),
                "expected": expected,
                "actual": actual,
                "abs_error": abs_error,
                "rel_error": rel_error,
            };
            push differences, diff;
        }
    }

    private static bool isNumericType(int type_code) {
        return type_code == NT_INT || type_code == NT_FLOAT || type_code == NT_NUMBER;
    }

    private static nothing addDifference(reference<list<hash<OnnxOutputDifference>>> differences,
            string path, string code, string message, auto expected, auto actual) {
        push differences, <OnnxOutputDifference>{
            "path": path,
            "code": code,
            "message": message,
            "expected": expected,
            "actual": actual,
        };
    }

    private static nothing validateTensorList(reference<list<hash<OnnxValidationIssue>>> issues,
            string kind, list<hash<ML::OnnxTensorInfo>> tensors) {
        if (!tensors) {
            addIssue(\issues, "error", kind + "-missing",
                sprintf("model has no %s tensors", kind), NOTHING,
                sprintf("Re-export the model with named %s tensors.", kind));
            return;
        }
        foreach hash<ML::OnnxTensorInfo> tensor in (tensors) {
            if (!tensor.name.val()) {
                addIssue(\issues, "warning", kind + "-unnamed",
                    sprintf("model has an unnamed %s tensor", kind), NOTHING,
                    sprintf("Name all %s tensors during model export.", kind));
            }
            if (!tensor.type.val()) {
                addIssue(\issues, "warning", kind + "-unknown-type",
                    sprintf("%s tensor %y has no element type", kind, tensor.name),
                    tensor.name,
                    "Provide explicit tensor element types during export.");
            }
            if (!tensor.shape) {
                addIssue(\issues, "info", kind + "-scalar-or-unknown-shape",
                    sprintf("%s tensor %y is scalar or has unknown shape", kind,
                        tensor.name), tensor.name,
                    "Provide representative benchmark inputs when tensor shapes are dynamic or unknown.");
            }
        }
    }

    private static nothing validateProviders(reference<list<hash<OnnxValidationIssue>>> issues,
            list<hash<ML::OnnxProviderDiagnostic>> diagnostics) {
        foreach hash<ML::OnnxProviderDiagnostic> diag in (diagnostics) {
            if (diag.required && !diag.appended) {
                addIssue(\issues, "error", "required-provider-unavailable",
                    sprintf("required provider %s was not configured", diag.name),
                    diag.name,
                    sprintf("Install or configure ONNX Runtime provider %s, or remove it from required_providers.", diag.name));
            } else if (diag.requested && !diag.appended) {
                addIssue(\issues, "warning", "requested-provider-unavailable",
                    sprintf("requested provider %s was not configured", diag.name),
                    diag.name,
                    sprintf("Install provider libraries for %s or use an explicit CPU provider configuration.", diag.name));
            }
            if (diag.cpu_fallback) {
                addIssue(\issues, "warning", "cpu-fallback",
                    sprintf("provider %s caused CPU fallback", diag.name), diag.name,
                    "Verify device libraries and provider options, or force CPU explicitly for reproducible deployment.");
            }
            if (diag.error.val()) {
                addIssue(\issues, diag.required ? "error" : "warning", "provider-error",
                    diag.error, diag.name,
                    sprintf("Review ONNX Runtime provider %s configuration and library paths.", diag.name));
            }
        }
    }

    private static nothing addIssue(reference<list<hash<OnnxValidationIssue>>> issues,
            string level, string code, string message, *string field = NOTHING,
            *string hint = NOTHING, *string example = NOTHING) {
        hash<OnnxValidationIssue> issue = <OnnxValidationIssue>{
            "level": level,
            "code": code,
            "message": message,
        };
        if (field.val()) {
            issue.field = field;
        }
        if (hint.val()) {
            issue.hint = hint;
        }
        if (example.val()) {
            issue.example = example;
        }
        push issues, issue;
    }

    private static bool hasErrors(list<hash<OnnxValidationIssue>> issues) {
        foreach hash<OnnxValidationIssue> issue in (issues) {
            if (issue.level == "error") {
                return True;
            }
        }
        return False;
    }
}

} # namespace QoreOnnxTools
