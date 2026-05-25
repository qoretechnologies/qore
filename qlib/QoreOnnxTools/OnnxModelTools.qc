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
    */
    static hash<auto> inspect(string path, *hash<ML::OnnxSessionConfig> session_config) {
        validateModelPath(path);
        hash<ML::OnnxSessionConfig> cfg = normalizeSessionConfig(session_config);
        ML::OnnxModel model(path, cfg);
        hash<ML::OnnxModelInfo> info = model.getModelInfo();
        return {
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
    }

    /** Validates model loadability, provider policy, and tensor metadata.

        @param path model artifact path
        @param session_config optional ONNX Runtime session configuration
        @return validation result with structured warnings/errors
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
            addIssue(\issues, "error", "empty-path", "model path must not be empty");
            rv.issues = issues;
            return rv;
        }
        if (!is_file(path)) {
            addIssue(\issues, "error", "not-found",
                sprintf("model path %y does not exist or is not a regular file", path));
            rv.issues = issues;
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
                sprintf("%s: %s", ex.err, ex.desc));
        }

        rv.issues = issues;
        rv.valid = !hasErrors(issues);
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

    private static nothing validateTensorList(reference<list<hash<OnnxValidationIssue>>> issues,
            string kind, list<hash<ML::OnnxTensorInfo>> tensors) {
        if (!tensors) {
            addIssue(\issues, "error", kind + "-missing",
                sprintf("model has no %s tensors", kind));
            return;
        }
        foreach hash<ML::OnnxTensorInfo> tensor in (tensors) {
            if (!tensor.name.val()) {
                addIssue(\issues, "warning", kind + "-unnamed",
                    sprintf("model has an unnamed %s tensor", kind));
            }
            if (!tensor.type.val()) {
                addIssue(\issues, "warning", kind + "-unknown-type",
                    sprintf("%s tensor %y has no element type", kind, tensor.name),
                    tensor.name);
            }
            if (!tensor.shape) {
                addIssue(\issues, "info", kind + "-scalar-or-unknown-shape",
                    sprintf("%s tensor %y is scalar or has unknown shape", kind,
                        tensor.name), tensor.name);
            }
        }
    }

    private static nothing validateProviders(reference<list<hash<OnnxValidationIssue>>> issues,
            list<hash<ML::OnnxProviderDiagnostic>> diagnostics) {
        foreach hash<ML::OnnxProviderDiagnostic> diag in (diagnostics) {
            if (diag.required && !diag.appended) {
                addIssue(\issues, "error", "required-provider-unavailable",
                    sprintf("required provider %s was not configured", diag.name),
                    diag.name);
            } else if (diag.requested && !diag.appended) {
                addIssue(\issues, "warning", "requested-provider-unavailable",
                    sprintf("requested provider %s was not configured", diag.name),
                    diag.name);
            }
            if (diag.cpu_fallback) {
                addIssue(\issues, "warning", "cpu-fallback",
                    sprintf("provider %s caused CPU fallback", diag.name), diag.name);
            }
            if (diag.error.val()) {
                addIssue(\issues, diag.required ? "error" : "warning", "provider-error",
                    diag.error, diag.name);
            }
        }
    }

    private static nothing addIssue(reference<list<hash<OnnxValidationIssue>>> issues,
            string level, string code, string message, *string field = NOTHING) {
        hash<OnnxValidationIssue> issue = <OnnxValidationIssue>{
            "level": level,
            "code": code,
            "message": message,
        };
        if (field.val()) {
            issue.field = field;
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
