# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OnnxDiagnostics.qc Copyright 2026 Qore Technologies, s.r.o.

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

/** Shared ONNX diagnostics and UX metadata helpers.

    The returned hashes are intentionally extensible so CLI tools,
    QoreModelRegistry, and DataProviderML can expose the same actionable
    deployment hints without sharing a rigid result type.

    @since QoreOnnxTools 1.1
*/
public class OnnxDiagnostics {
    /** Builds a compact diagnostic summary from \c OnnxModelTools::inspect().

        @param inspection inspection hash returned by \c OnnxModelTools::inspect()

        @return normalized diagnostics with provider, tensor, action, and
            example-command metadata

        @par Example

        @code
hash<auto> info = QoreOnnxTools::OnnxModelTools::inspect("/models/fraud.onnx");
hash<auto> diagnostics = QoreOnnxTools::OnnxDiagnostics::summarizeInspection(info);
foreach string action in (diagnostics.recommended_actions) {
    log(LL_INFO, action);
}
        @endcode
    */
    static hash<auto> summarizeInspection(hash<auto> inspection) {
        # Populate the action/example lists before capturing them in the hash:
        # a list captured into a hash literal is a value-snapshot, so later
        # push/addUnique mutations against the local would not be visible
        # through the hash entry on certain code-gen paths (see the parallel
        # comment in summarizePackage() below).
        list<string> actions();
        list<string> examples();
        addTensorActions(\actions, "input", inspection.inputs);
        addTensorActions(\actions, "output", inspection.outputs);
        addProviderActions(\actions, inspection.provider_diagnostics);
        addModelExamples(\examples, inspection.path);

        hash<auto> rv = {
            "kind": "onnx-inspection",
            "path": inspection.path,
            "input_names": tensorNames(inspection.inputs),
            "output_names": tensorNames(inspection.outputs),
            "dynamic_inputs": dynamicTensorNames(inspection.inputs),
            "dynamic_outputs": dynamicTensorNames(inspection.outputs),
            "recommended_actions": actions,
            "examples": examples,
            "actionable": actions ? True : False,
        };
        addProviderSummary(\rv, inspection);
        return rv;
    }

    /** Builds a compact diagnostic summary from ONNX validation results.

        @param validation validation result hash

        @return normalized diagnostics with issue counts, recommended actions,
            and example commands

        @par Example

        @code
hash<QoreOnnxTools::OnnxValidationResult> validation =
    QoreOnnxTools::OnnxModelTools::validate("/models/fraud.onnx");
if (!validation.valid) {
    log(LL_ERROR, validation.diagnostics.recommended_actions);
}
        @endcode
    */
    static hash<auto> summarizeValidation(hash<auto> validation) {
        list<string> actions();
        list<string> examples();
        hash<auto> counts = {
            "error": 0,
            "warning": 0,
            "info": 0,
        };
        if (validation.issues) {
            foreach hash<auto> issue in (validation.issues) {
                if (counts.hasKey(issue.level)) {
                    ++counts{issue.level};
                }
                if (issue.hint.val()) {
                    addUnique(\actions, issue.hint);
                }
                if (issue.example.val()) {
                    addUnique(\examples, issue.example);
                }
            }
        }
        if (!examples && validation.path.val()) {
            addModelExamples(\examples, validation.path);
        }
        hash<auto> rv = {
            "kind": "onnx-validation",
            "path": validation.path,
            "valid": validation.valid ? True : False,
            "issue_counts": counts,
            "recommended_actions": actions,
            "examples": examples,
            "actionable": actions ? True : False,
        };
        if (validation.provider_report) {
            rv.provider = providerSummary(validation.provider_report);
        }
        return rv;
    }

    /** Builds a package-level deployment diagnostic summary.

        @param package_info package result or manifest hash

        @return normalized diagnostics with execution, provider, package, and
            registry example metadata

        @par Example

        @code
hash<QoreOnnxTools::OnnxPackageResult> package =
    QoreOnnxTools::OnnxPackageTools::createPackage(
        "/models/fraud.onnx", "/srv/models/fraud");
hash<auto> diagnostics =
    QoreOnnxTools::OnnxDiagnostics::summarizePackage(package);
        @endcode
    */
    static hash<auto> summarizePackage(hash<auto> package_info) {
        # Populate `actions` and `examples` BEFORE assembling the result hash.
        # Capturing a list into a hash literal takes a value-snapshot of the
        # list at hash-construction time; subsequent push/addUnique mutations
        # against the local variable update the local, not the hash entry.
        # Empirically the AOT-compiled qmod exposes this on Linux CI (the
        # Linux x86_64 + arm64 runs in pipeline 50611 reported
        # `result.diagnostics.examples.size() > 0` as False, while macOS
        # passed) — populate-then-capture is the deterministic fix.
        list<string> actions();
        list<string> examples();

        hash<auto> validation;
        if (package_info.validation) {
            validation = summarizeValidation(cast<hash<auto>>(package_info.validation));
            foreach string action in (validation.recommended_actions) {
                addUnique(\actions, action);
            }
        }
        if (!package_info.executable) {
            addUnique(\actions, "Convert the package to ONNX before in-process execution; non-ONNX source artifacts are recorded only as metadata.");
        }
        if (package_info.safety == "unsafe-pickle") {
            addUnique(\actions, "Avoid executing PyTorch pickle artifacts directly; export a safe ONNX artifact or SafeTensors metadata package.");
        }
        if (package_info.runtime == "onnxruntime" && !package_info.preprocess) {
            addUnique(\actions, "Add a preprocessing contract or task name so callers know how to build model inputs.");
        }
        if (package_info.runtime == "onnxruntime" && !package_info.postprocess) {
            addUnique(\actions, "Add a postprocessing contract or task name so callers know which model outputs carry predictions.");
        }
        if (!package_info.benchmarks) {
            addUnique(\actions, "Record benchmark metadata after packaging so deployment latency can be compared across artifacts.");
        }
        if (!package_info.golden_vectors) {
            addUnique(\actions, "Add golden vectors when promoting the package so deployment changes can be validated deterministically.");
        }

        addPackageExamples(\examples, package_info);

        hash<auto> rv = {
            "kind": "onnx-package",
            "source_path": package_info.source_path,
            "output_dir": package_info.output_dir,
            "runtime": package_info.runtime,
            "task": package_info.task,
            "entry_artifact": package_info.entry_artifact,
            "executable": package_info.executable ? True : False,
            "safety": package_info.safety,
            "input_names": tensorNames(package_info.inputs),
            "output_names": tensorNames(package_info.outputs),
            "dynamic_inputs": dynamicTensorNames(package_info.inputs),
            "dynamic_outputs": dynamicTensorNames(package_info.outputs),
            "preprocess_kind": package_info.preprocess ? package_info.preprocess.kind : NOTHING,
            "postprocess_kind": package_info.postprocess ? package_info.postprocess.kind : NOTHING,
            "recommended_actions": actions,
            "examples": examples,
            "actionable": actions ? True : False,
        };

        if (package_info.providers) {
            rv.provider = providerSummary(package_info.providers);
        }
        if (validation) {
            rv.validation = validation;
        }

        return rv;
    }

    private static nothing addProviderSummary(reference<hash<auto>> rv,
            hash<auto> inspection) {
        if (inspection.provider_report) {
            rv.provider = providerSummary(inspection.provider_report);
        } else {
            rv.provider = {
                "active_provider": inspection.info.active_provider,
                "available_providers": inspection.providers,
                "requested_providers": inspection.requested_providers,
            };
        }
    }

    private static hash<auto> providerSummary(hash<auto> provider_info) {
        hash<auto> rv = {};
        if (provider_info.active_provider.val()) {
            rv.active_provider = provider_info.active_provider;
        }
        if (provider_info.available_providers) {
            rv.available_providers = provider_info.available_providers;
        }
        if (provider_info.requested_providers) {
            rv.requested_providers = provider_info.requested_providers;
        }
        if (exists provider_info.cpu_fallback_used) {
            rv.cpu_fallback_used = provider_info.cpu_fallback_used ? True : False;
        }
        if (exists provider_info.auto_provider_selected) {
            rv.auto_provider_selected = provider_info.auto_provider_selected ? True : False;
        }
        if (provider_info.provider_report) {
            return providerSummary(cast<hash<auto>>(provider_info.provider_report));
        }
        return rv;
    }

    private static nothing addProviderActions(reference<list<string>> actions,
            *list<hash<auto>> diagnostics) {
        if (!diagnostics) {
            return;
        }
        foreach hash<auto> diag in (diagnostics) {
            if (diag.required && !diag.appended) {
                addUnique(\actions, sprintf("Install or configure required ONNX Runtime provider %s before deployment.", diag.name));
            } else if (diag.requested && !diag.appended) {
                addUnique(\actions, sprintf("Requested ONNX Runtime provider %s was not used; check provider libraries and options.", diag.name));
            }
            if (diag.cpu_fallback) {
                addUnique(\actions, sprintf("Provider %s fell back to CPU; verify device libraries or force CPU explicitly.", diag.name));
            }
            if (diag.error.val()) {
                addUnique(\actions, sprintf("Review provider %s error: %s", diag.name, diag.error));
            }
        }
    }

    private static nothing addTensorActions(reference<list<string>> actions,
            string kind, *list<hash<auto>> tensors) {
        if (!tensors) {
            addUnique(\actions, sprintf("Inspect the model with ONNX Runtime to populate %s tensor metadata.", kind));
            return;
        }
        foreach hash<auto> tensor in (tensors) {
            if (!tensor.name.val()) {
                addUnique(\actions, sprintf("Name all %s tensors during export to make deployment mappings stable.", kind));
            }
            if (hasDynamicAxis(tensor)) {
                addUnique(\actions, sprintf("Provide representative inputs for dynamic %s tensor %s when benchmarking or validating.", kind, tensor.name));
            }
        }
    }

    private static list<string> tensorNames(*list<hash<auto>> tensors) {
        list<string> rv();
        if (!tensors) {
            return rv;
        }
        foreach hash<auto> tensor in (tensors) {
            if (tensor.name.val()) {
                push rv, string(tensor.name);
            }
        }
        return rv;
    }

    private static list<string> dynamicTensorNames(*list<hash<auto>> tensors) {
        list<string> rv();
        if (!tensors) {
            return rv;
        }
        foreach hash<auto> tensor in (tensors) {
            if (tensor.name.val() && hasDynamicAxis(tensor)) {
                push rv, string(tensor.name);
            }
        }
        return rv;
    }

    private static bool hasDynamicAxis(hash<auto> tensor) {
        if (!tensor.shape || tensor.shape.typeCode() != NT_LIST) {
            return False;
        }
        foreach auto dim in (tensor.shape) {
            if (dim.typeCode() == NT_STRING || int(dim) <= 0) {
                return True;
            }
        }
        return False;
    }

    private static nothing addModelExamples(reference<list<string>> examples, *string path) {
        string p = quoteExamplePath(path, "model.onnx");
        addUnique(\examples, "qore-onnx validate --json " + p);
        addUnique(\examples, "qore-onnx benchmark --runs 50 --benchmark-inputs inputs.json " + p);
        addUnique(\examples, "qore-onnx package --task regression " + p + " ./model-package");
    }

    private static nothing addPackageExamples(reference<list<string>> examples,
            hash<auto> package_info) {
        string output_dir = quoteExamplePath(package_info.output_dir, "./model-package");
        string entry = package_info.entry_artifact.val()
            ? output_dir + "/" + package_info.entry_artifact
            : output_dir + "/model.onnx";
        addUnique(\examples, "qore-onnx validate --json " + entry);
        addUnique(\examples, "qore-onnx benchmark --runs 50 --benchmark-inputs inputs.json " + entry);
        addUnique(\examples, "QoreModelRegistry::ModelRegistry({\"path\": \"/var/models\"}).registerPackage(" + output_dir + ", {\"name\": \"model-name\"})");
    }

    private static string quoteExamplePath(*string path, string fallback) {
        string p = path.val() ? path : fallback;
        if (p =~ /^[A-Za-z0-9_.,:\/%+=@-]+$/) {
            return p;
        }
        return "\"" + replace(p, "\"", "\\\"") + "\"";
    }

    private static nothing addUnique(reference<list<string>> values, string value) {
        if (value.val() && !values.contains(value)) {
            push values, value;
        }
    }
}

} # namespace QoreOnnxTools
