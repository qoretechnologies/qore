# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ModelArtifactInspector.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main QoreModelRegistry namespace
public namespace QoreModelRegistry {

#! Inspects model package artifacts without executing model code
/** The inspector classifies HuggingFace/PyTorch-style model packages for the
    registry. It parses JSON metadata and SafeTensors headers, but never loads
    or deserializes PyTorch pickle artifacts.

    @since QoreModelRegistry 2.2
*/
public class ModelArtifactInspector {
    #! Inspects a single artifact file
    /** @param path filesystem path to inspect
        @param artifact_name optional package-local artifact name

        @return artifact metadata

        @throw ML-REGISTRY-ERROR if the file cannot be inspected
    */
    static hash<ModelArtifactInfo> inspectFile(string path, *string artifact_name) {
        if (!is_file(path)) {
            throw "ML-REGISTRY-ERROR", sprintf("artifact file '%s' not found", path);
        }
        string name = artifact_name ?? basename(path);
        string format = inferArtifactFormat(name);
        string role = inferArtifactRole(name, format);
        *hash<StatInfo> st = hstat(path);

        hash<ModelArtifactInfo> info = <ModelArtifactInfo>{
            "name": name,
            "role": role,
            "format": format,
            "size": st ? st.size : 0,
            "safe_to_parse": isSafeToParse(format),
            "executable": format == "qml" || format == "onnx",
            "path_hint": name,
        };

        if (format == "json") {
            info.metadata = inspectJsonFile(path, name);
        } else if (format == "safetensors") {
            info.metadata = inspectSafeTensorsFile(path);
        }
        if (format == "onnx") {
            info.metadata = inspectOnnxFile(path);
        }
        return info;
    }

    #! Inspects a model package directory
    /** @param path package directory
        @param name registry model name
        @param options optional metadata overrides

        @return version metadata suitable for registration

        @throw ML-REGISTRY-ERROR if the directory cannot be inspected
    */
    static hash<ModelVersionInfo> inspectDirectory(string path, string name,
            *hash<auto> options) {
        if (!is_dir(path)) {
            throw "ML-REGISTRY-ERROR", sprintf("model package directory '%s' not found", path);
        }

        Dir d();
        if (!d.chdir(path)) {
            throw "ML-REGISTRY-ERROR",
                sprintf("could not open model package directory '%s'", path);
        }

        list<hash<ModelArtifactInfo>> artifacts();
        foreach string file in (d.listFileNames()) {
            if (isSupportedPackageFile(file)) {
                hash<ModelArtifactInfo> info = inspectFile(path + DirSep + file, file);
                push artifacts, info;
            }
        }
        addCommonOnnxExternalDataArtifacts(path, \artifacts);

        if (!artifacts) {
            throw "ML-REGISTRY-ERROR",
                sprintf("model package directory '%s' does not contain a supported "
                    "model, tokenizer, or metadata artifact", path);
        }

        *hash<auto> package_manifest = findPackageManifest(artifacts);
        hash<ModelArtifactInfo> primary = selectPrimaryArtifact(artifacts, package_manifest);
        hash<TokenizerArtifactInfo> tokenizer = buildTokenizerInfo(artifacts,
            package_manifest);
        *string manifest_framework = package_manifest ? package_manifest.framework : NOTHING;
        *string manifest_runtime = package_manifest ? package_manifest.runtime : NOTHING;
        *string manifest_safety = package_manifest ? package_manifest.safety : NOTHING;
        *int manifest_opset = package_manifest ? package_manifest.opset : NOTHING;
        *string manifest_task = package_manifest ? package_manifest.task : NOTHING;
        *string manifest_producer = package_manifest ? package_manifest.producer : NOTHING;
        *string manifest_min_ort = package_manifest
            ? package_manifest.minimum_onnxruntime_version : NOTHING;
        string framework = options.framework ?? manifest_framework ?? inferFramework(artifacts);
        string runtime = manifest_runtime ?? inferRuntime(primary);
        string artifact_kind = inferArtifactKind(primary, artifacts);
        string safety = manifest_safety ?? inferPackageSafety(artifacts);
        *hash<auto> option_plan;
        if (exists options.execution_plan) {
            option_plan = cast<hash>(options.execution_plan);
        }
        *string plan_task = taskFromExecutionPlan(option_plan);
        if (plan_task.val() && options.task.val()) {
            string normalized_plan_task = normalizeExecutionTask(plan_task);
            string normalized_option_task = normalizeExecutionTask(options.task);
            if (normalized_plan_task != normalized_option_task) {
                throw "ML-REGISTRY-ERROR",
                    sprintf("conflicting ONNX package task metadata: option 'task' is %y "
                        "but execution_plan declares %y", normalized_option_task,
                        normalized_plan_task);
            }
        }
        string task = inferExecutionTask(plan_task ?? options.task ?? manifest_task,
            artifacts, runtime);
        *list<hash<auto>> inputs = primary.metadata && exists primary.metadata.inputs
            ? cast<list<hash<auto>>>(primary.metadata.inputs)
            : NOTHING;
        *list<hash<auto>> outputs = primary.metadata && exists primary.metadata.outputs
            ? cast<list<hash<auto>>>(primary.metadata.outputs)
            : NOTHING;
        *hash<auto> input_schema;
        *hash<auto> output_schema;
        if (inputs) {
            input_schema = {"tensors": inputs};
        }
        if (outputs) {
            output_schema = {"tensors": outputs};
        }
        *hash<auto> dynamic_axes = options.dynamic_axes
            ?? (package_manifest ? package_manifest.dynamic_axes : NOTHING)
            ?? buildDynamicAxes(inputs, outputs);
        *hash<auto> execution_config = options.execution_config
            ?? options.session_config
            ?? (runtime == "onnxruntime" ? options.config : NOTHING);
        # device-buffer binding policy: explicit option wins, else inherit any policy
        # already present in the session config or the package manifest
        *hash<auto> execution_device_binding = options.device_binding
            ?? (execution_config ? execution_config.device_binding : NOTHING)
            ?? (package_manifest ? package_manifest.device_binding : NOTHING);
        *hash<auto> source_preprocess;
        if (exists options.preprocess) {
            source_preprocess = cast<hash>(options.preprocess);
        } else if (option_plan && exists option_plan.preprocess) {
            source_preprocess = cast<hash>(option_plan.preprocess);
        } else if (package_manifest && package_manifest.preprocess) {
            source_preprocess = cast<hash>(package_manifest.preprocess);
        }
        *hash<auto> source_postprocess;
        if (exists options.postprocess) {
            source_postprocess = cast<hash>(options.postprocess);
        } else if (option_plan && exists option_plan.postprocess) {
            source_postprocess = cast<hash>(option_plan.postprocess);
        } else if (package_manifest && package_manifest.postprocess) {
            source_postprocess = cast<hash>(package_manifest.postprocess);
        }
        *hash<auto> preprocess = source_preprocess;
        *hash<auto> postprocess = source_postprocess;
        if (runtime == "onnxruntime") {
            hash<auto> processing_context = buildProcessingContext(artifacts, package_manifest);
            preprocess = QoreOnnxTools::OnnxProcessingSpecs::normalizePreprocess(
                source_preprocess, task, inputs, outputs, processing_context);
            postprocess = QoreOnnxTools::OnnxProcessingSpecs::normalizePostprocess(
                source_postprocess, task, inputs, outputs, processing_context);
        }
        hash<auto> execution_plan =
            buildExecutionPlan(runtime, task, primary, input_schema, output_schema, option_plan,
                preprocess, postprocess, dynamic_axes, tokenizer);
        *hash<auto> provider_info = buildProviderInfo(primary, package_manifest);
        *hash<auto> validation = options.validation
            ?? (package_manifest ? package_manifest.validation : NOTHING)
            ?? (primary.metadata ? primary.metadata.validation : NOTHING);
        *hash<auto> diagnostics;
        if (exists options.diagnostics) {
            diagnostics = cast<hash>(options.diagnostics);
        } else if (package_manifest && package_manifest.diagnostics) {
            diagnostics = cast<hash>(package_manifest.diagnostics);
        }
        *hash<auto> benchmarks;
        if (exists options.benchmarks) {
            benchmarks = cast<hash>(options.benchmarks);
        } else if (package_manifest && package_manifest.benchmarks) {
            benchmarks = cast<hash>(package_manifest.benchmarks);
        }
        *list<hash<auto>> golden_vectors;
        if (exists options.golden_vectors) {
            golden_vectors = cast<list<hash<auto>>>(options.golden_vectors);
        } else if (package_manifest && package_manifest.golden_vectors) {
            golden_vectors = cast<list<hash<auto>>>(package_manifest.golden_vectors);
        }
        *list<string> external_data = buildExternalDataList(artifacts, primary);
        if (!diagnostics && runtime == "onnxruntime") {
            diagnostics = QoreOnnxTools::OnnxDiagnostics::summarizePackage({
                "source_path": path,
                "output_dir": path,
                "runtime": runtime,
                "task": task,
                "entry_artifact": primary.name,
                "executable": True,
                "safety": safety,
                "inputs": inputs,
                "outputs": outputs,
                "preprocess": preprocess,
                "postprocess": postprocess,
                "providers": provider_info,
                "validation": validation,
                "benchmarks": benchmarks,
                "golden_vectors": golden_vectors,
            });
        }

        return cast<hash<ModelVersionInfo>>({
            "version_id": options.version_id,
            "name": name,
            "algorithm": options.algorithm ?? inferAlgorithm(artifacts),
            "created": format_date("YYYY-MM-DDTHH:mm:SSZ", now()),
            "description": options.description,
            "metrics": options.metrics,
            "tags": options.tags ?? (),
            "file_size": primary.size,
            "format": primary.format,
            "training_data_hash": options.training_data_hash,
            "framework": options.framework ?? framework,
            "artifact_kind": artifact_kind,
            "runtime": runtime,
            "task": task,
            "opset": options.opset ?? manifest_opset,
            "producer": options.producer
                ?? manifest_producer
                ?? (primary.metadata ? primary.metadata.producer_name : NOTHING),
            "minimum_onnxruntime_version": options.minimum_onnxruntime_version
                ?? manifest_min_ort,
            "inputs": inputs,
            "outputs": outputs,
            "dynamic_axes": dynamic_axes,
            "external_data": external_data,
            "preprocess": preprocess,
            "postprocess": postprocess,
            "providers": provider_info,
            "validation": validation,
            "diagnostics": diagnostics,
            "benchmarks": benchmarks,
            "golden_vectors": golden_vectors,
            "executable": runtime == "qore-ml" || runtime == "onnxruntime",
            "safety": safety,
            "artifacts": artifacts,
            "tokenizer": tokenizer,
            "execution": <ModelExecutionInfo>{
                "runtime": runtime,
                "task": task,
                "local_executable": runtime == "qore-ml" || runtime == "onnxruntime",
                "entry_artifact": primary.name,
                "plan": execution_plan,
                "preprocess": preprocess,
                "postprocess": postprocess,
                "requires": runtime == "onnxruntime" ? ("ml",) : (),
                "input_schema": input_schema,
                "output_schema": output_schema,
                "config": execution_config,
                "device_binding": execution_device_binding,
                "diagnostics": diagnostics,
            },
            "config": options.config,
        });
    }

    #! Returns True if a file name is part of a supported model package
    static bool isSupportedPackageFile(string name) {
        string lname = name.lwr();
        return lname == "config.json"
            || lname == "qore-onnx-package.json"
            || lname == "tokenizer.json"
            || lname == "tokenizer_config.json"
            || lname == "generation_config.json"
            || lname == "special_tokens_map.json"
            || lname =~ /[.]onnx$/
            || lname =~ /[.]onnx[.]data$/
            || lname =~ /[.]data$/
            || lname =~ /[.]qml$/
            || lname =~ /[.]safetensors$/
            || lname =~ /[.](pt|pth|bin)$/
            || lname =~ /[.]pt2$/;
    }

    private static string basename(string path) {
        list<string> parts = path.split(DirSep);
        if (parts.size()) {
            return parts.last();
        }
        return path;
    }

    private static string inferArtifactFormat(string name) {
        string lname = name.lwr();
        if (lname =~ /[.]qml$/) {
            return "qml";
        }
        if (lname =~ /[.]onnx$/) {
            return "onnx";
        }
        if (lname =~ /[.]onnx[.]data$/ || lname =~ /[.]data$/) {
            return "onnx-external-data";
        }
        if (lname =~ /[.]json$/) {
            return "json";
        }
        if (lname =~ /[.]safetensors$/) {
            return "safetensors";
        }
        if (lname =~ /[.](pt|pth|bin)$/) {
            return "pytorch-pickle";
        }
        if (lname =~ /[.]pt2$/) {
            return "torch-export";
        }
        return "binary";
    }

    private static string inferArtifactRole(string name, string format) {
        string lname = name.lwr();
        if (lname == "tokenizer.json") {
            return "tokenizer";
        }
        if (lname == "tokenizer_config.json") {
            return "tokenizer_config";
        }
        if (lname == "generation_config.json") {
            return "generation_config";
        }
        if (lname == "special_tokens_map.json") {
            return "special_tokens";
        }
        if (lname == "config.json") {
            return "config";
        }
        if (lname == "qore-onnx-package.json") {
            return "manifest";
        }
        if (format == "safetensors" || format == "pytorch-pickle") {
            return "weights";
        }
        if (format == "onnx-external-data") {
            return "external_data";
        }
        return "model";
    }

    private static bool isSafeToParse(string format) {
        return format == "qml" || format == "onnx" || format == "json"
            || format == "safetensors" || format == "torch-export";
    }

    private static hash<auto> inspectOnnxFile(string path) {
        hash<auto> result = {
            "format": "onnx",
            "runtime_available": ML::ml_get_capabilities().has_onnx,
            "metadata_read": False,
        };

        list<string> external_data_files = findCommonOnnxExternalDataFiles(path);
        if (external_data_files) {
            result.has_external_data = True;
            result.external_data_files = external_data_files;
        } else {
            result.has_external_data = False;
        }

        if (!result.runtime_available) {
            result.note = "ONNX Runtime is not available; tensor metadata was not inspected";
            return result;
        }

        try {
            hash<ML::OnnxSessionConfig> config = <ML::OnnxSessionConfig>{
                "providers": (),
            };
            ML::OnnxModel model(path, config);
            hash<ML::OnnxModelInfo> info = model.getModelInfo();
            result.metadata_read = True;
            result.producer_name = info.producer_name;
            result.description = info.description;
            result.version = info.version;
            result.inputs = info.inputs;
            result.outputs = info.outputs;
            result.active_provider = info.active_provider;
            result.available_providers = info.available_providers;
            result.requested_providers = info.requested_providers;
            result.provider_diagnostics = info.provider_diagnostics;
            result.cpu_fallback_used = info.cpu_fallback_used;
            result.provider_report = model.getEffectiveProviderReport();
            result.validation = QoreOnnxTools::OnnxModelTools::validate(path, config);
        } catch (hash<ExceptionInfo> ex) {
            result.error = sprintf("%s: %s", ex.err, ex.desc);
        }
        return result;
    }

    private static list<string> findCommonOnnxExternalDataFiles(string onnx_path) {
        string model_name = basename(onnx_path);
        string stem_data_name = model_name;
        stem_data_name =~ s/[.]onnx$/.data/;
        list<string> candidates = (
            model_name + ".data",
            stem_data_name,
        );

        list<string> parts = onnx_path.split(DirSep);
        pop parts;
        string dir = parts ? join(DirSep, parts) : ".";

        list<string> rv();
        foreach string c in (candidates) {
            if (c.val() && is_file(dir + DirSep + c) && !rv.contains(c)) {
                push rv, c;
            }
        }
        return rv;
    }

    private static nothing addCommonOnnxExternalDataArtifacts(string package_path,
            reference<list<hash<ModelArtifactInfo>>> artifacts) {
        list<string> known = map $1.name, artifacts;
        foreach hash<ModelArtifactInfo> artifact in (artifacts) {
            if (artifact.format != "onnx" || !artifact.metadata
                    || !artifact.metadata.external_data_files) {
                continue;
            }
            foreach string external_name in (artifact.metadata.external_data_files) {
                if (known.contains(external_name)) {
                    continue;
                }
                string external_path = package_path + DirSep + external_name;
                if (!is_file(external_path)) {
                    continue;
                }
                hash<ModelArtifactInfo> external = inspectFile(external_path, external_name);
                push artifacts, external;
                push known, external_name;
            }
        }
    }

    private static hash<auto> inspectJsonFile(string path, string name) {
        hash<auto> json = parse_json(ReadOnlyFile::readTextFile(path));
        hash<auto> result;
        result.json_keys = sort(keys json);
        if (name == "config.json") {
            result.model_type = json.model_type;
            result.architectures = json.architectures;
            result.torch_dtype = json.torch_dtype;
            result.transformers_version = json.transformers_version;
        } else if (name == "qore-onnx-package.json") {
            result.manifest_version = json.manifest_version;
            result.framework = json.framework;
            result.runtime = json.runtime;
            result.safety = json.safety;
            result.entry_artifact = json.entry_artifact;
            result.executable = json.executable;
            result.task = json.task;
            result.opset = json.opset;
            result.producer = json.producer;
            result.minimum_onnxruntime_version = json.minimum_onnxruntime_version;
            result.dynamic_axes = json.dynamic_axes;
            result.preprocess = json.preprocess;
            result.postprocess = json.postprocess;
            result.execution = json.execution;
            result.providers = json.providers;
            result.validation = json.validation;
            result.diagnostics = json.diagnostics;
            result.benchmarks = json.benchmarks;
            result.golden_vectors = json.golden_vectors;
        } else if (name == "tokenizer_config.json") {
            result.model_max_length = json.model_max_length;
            result.has_chat_template = exists json.chat_template && json.chat_template.val();
            result.eos_token = json.eos_token;
            result.bos_token = json.bos_token;
            result.pad_token = json.pad_token;
            result.unk_token = json.unk_token;
        }
        return result;
    }

    private static hash<auto> inspectSafeTensorsFile(string path) {
        binary data = ReadOnlyFile::readBinaryFile(path);
        hash<auto> result = {
            "format": "safetensors",
            "header_parsed": False,
        };
        if (data.size() < 8) {
            result.error = "file is too small to contain a SafeTensors header";
            return result;
        }

        int header_len = readUInt64Le(data);
        if (header_len <= 0 || header_len > 10485760 || data.size() < 8 + header_len) {
            result.error = "invalid or unsupported SafeTensors header length";
            return result;
        }

        string header = data[8..(8 + header_len - 1)].toString();
        hash<auto> parsed = parse_json(header);
        result.header_parsed = True;
        result.tensor_count = 0;
        result.metadata = parsed.__metadata__;
        list<hash<auto>> tensors();
        foreach string key in (sort(keys parsed)) {
            if (key == "__metadata__") {
                continue;
            }
            hash<auto> t = parsed{key};
            push tensors, {
                "name": key,
                "dtype": t.dtype,
                "shape": t.shape,
            };
            ++result.tensor_count;
        }
        result.tensors = tensors;
        return result;
    }

    private static int readUInt64Le(binary data) {
        int rv = 0;
        for (int i = 0; i < 8; ++i) {
            rv += int(data[i]) << (i * 8);
        }
        return rv;
    }

    private static hash<ModelArtifactInfo> selectPrimaryArtifact(
            list<hash<ModelArtifactInfo>> artifacts, *hash<auto> package_manifest) {
        if (package_manifest && package_manifest.entry_artifact.val()) {
            foreach hash<ModelArtifactInfo> a in (artifacts) {
                if (a.name == package_manifest.entry_artifact) {
                    return a;
                }
            }
        }
        foreach string format in ("qml", "onnx", "safetensors", "torch-export", "pytorch-pickle") {
            foreach hash<ModelArtifactInfo> a in (artifacts) {
                if (a.role == "model" && a.format == format) {
                    return a;
                }
            }
            foreach hash<ModelArtifactInfo> a in (artifacts) {
                if (a.format == format) {
                    return a;
                }
            }
        }
        return artifacts[0];
    }

    private static *hash<auto> findPackageManifest(list<hash<ModelArtifactInfo>> artifacts) {
        foreach hash<ModelArtifactInfo> a in (artifacts) {
            if (a.role == "manifest" && a.metadata) {
                return a.metadata;
            }
        }
        return NOTHING;
    }

    private static *hash<auto> buildProviderInfo(hash<ModelArtifactInfo> primary,
            *hash<auto> package_manifest) {
        if (package_manifest && package_manifest.providers) {
            return cast<hash>(package_manifest.providers);
        }
        if (!primary.metadata) {
            return NOTHING;
        }
        hash<auto> provider_info;
        if (primary.metadata.provider_report) {
            provider_info.provider_report = primary.metadata.provider_report;
            if (primary.metadata.provider_report.active_provider.val()) {
                provider_info.active_provider = primary.metadata.provider_report.active_provider;
            }
            if (exists primary.metadata.provider_report.available_providers) {
                provider_info.available_providers = primary.metadata.provider_report.available_providers;
            }
            if (exists primary.metadata.provider_report.requested_providers) {
                provider_info.requested_providers = primary.metadata.provider_report.requested_providers;
            }
        } else if (primary.metadata.active_provider.val()) {
            provider_info.active_provider = primary.metadata.active_provider;
        }
        if (exists primary.metadata.provider_diagnostics) {
            provider_info.diagnostics = primary.metadata.provider_diagnostics;
        }
        if (exists primary.metadata.cpu_fallback_used) {
            provider_info.cpu_fallback_used = primary.metadata.cpu_fallback_used;
        }
        return provider_info ? provider_info : NOTHING;
    }

    private static *hash<auto> buildDynamicAxes(*list<hash<auto>> inputs,
            *list<hash<auto>> outputs) {
        hash<auto> rv;
        addDynamicAxes(\rv, inputs);
        addDynamicAxes(\rv, outputs);
        return rv ? rv : NOTHING;
    }

    private static nothing addDynamicAxes(reference<hash<auto>> rv,
            *list<hash<auto>> tensors) {
        if (!tensors) {
            return;
        }
        foreach hash<auto> tensor in (tensors) {
            if (!tensor.name.val() || !tensor.shape) {
                continue;
            }
            list<int> axes();
            for (int i = 0; i < tensor.shape.size(); ++i) {
                if (tensor.shape[i] <= 0) {
                    push axes, i;
                }
            }
            if (axes) {
                rv{tensor.name} = axes;
            }
        }
    }

    private static *list<string> buildExternalDataList(list<hash<ModelArtifactInfo>> artifacts,
            hash<ModelArtifactInfo> primary) {
        list<string> rv();
        foreach hash<ModelArtifactInfo> artifact in (artifacts) {
            if (artifact.role == "external_data" && !rv.contains(artifact.name)) {
                push rv, artifact.name;
            }
        }
        if (primary.metadata && primary.metadata.external_data_files) {
            foreach string name in (primary.metadata.external_data_files) {
                if (!rv.contains(name)) {
                    push rv, name;
                }
            }
        }
        return rv ? rv : NOTHING;
    }

    private static hash<auto> buildProcessingContext(
            list<hash<ModelArtifactInfo>> artifacts, *hash<auto> package_manifest) {
        hash<auto> rv = {};
        if (package_manifest) {
            if (package_manifest.tokenizer) {
                rv.tokenizer = package_manifest.tokenizer;
            }
            if (package_manifest.config_artifact_name.val()) {
                rv.config_artifact_name = package_manifest.config_artifact_name;
            }
        }
        foreach hash<ModelArtifactInfo> artifact in (artifacts) {
            switch (artifact.role) {
                case "tokenizer":
                    ensureTokenizerContext(\rv);
                    rv.tokenizer.artifact_name = artifact.name;
                    break;
                case "tokenizer_config":
                    ensureTokenizerContext(\rv);
                    rv.tokenizer.config_artifact_name = artifact.name;
                    if (artifact.metadata && artifact.metadata.model_max_length) {
                        rv.tokenizer.model_max_length = artifact.metadata.model_max_length;
                    }
                    break;
                case "config":
                    rv.config_artifact_name = artifact.name;
                    if (artifact.metadata) {
                        if (artifact.metadata.model_type.val()) {
                            rv.model_type = artifact.metadata.model_type;
                        }
                        if (artifact.metadata.architectures) {
                            rv.architectures = artifact.metadata.architectures;
                        }
                    }
                    break;
            }
        }
        return rv;
    }

    private static nothing ensureTokenizerContext(reference<hash<auto>> ctx) {
        if (!exists ctx.tokenizer || ctx.tokenizer.typeCode() != NT_HASH) {
            ctx.tokenizer = {};
        }
    }

    private static hash<TokenizerArtifactInfo> buildTokenizerInfo(
            list<hash<ModelArtifactInfo>> artifacts, *hash<auto> package_manifest) {
        hash<TokenizerArtifactInfo> info = <TokenizerArtifactInfo>{};
        if (package_manifest && package_manifest.tokenizer) {
            hash<auto> manifest_tokenizer = cast<hash>(package_manifest.tokenizer);
            if (manifest_tokenizer.tokenizer_json.val()) {
                info.tokenizer_json = manifest_tokenizer.tokenizer_json;
            } else if (manifest_tokenizer.artifact_name.val()) {
                info.tokenizer_json = manifest_tokenizer.artifact_name;
            }
            if (manifest_tokenizer.tokenizer_config_json.val()) {
                info.tokenizer_config_json = manifest_tokenizer.tokenizer_config_json;
            } else if (manifest_tokenizer.config_artifact_name.val()) {
                info.tokenizer_config_json = manifest_tokenizer.config_artifact_name;
            }
            if (manifest_tokenizer.special_tokens_map_json.val()) {
                info.special_tokens_map_json = manifest_tokenizer.special_tokens_map_json;
            }
            if (manifest_tokenizer.generation_config_json.val()) {
                info.generation_config_json = manifest_tokenizer.generation_config_json;
            }
            if (manifest_tokenizer.model_max_length) {
                info.model_max_length = manifest_tokenizer.model_max_length;
            }
            if (exists manifest_tokenizer.has_chat_template) {
                info.has_chat_template = manifest_tokenizer.has_chat_template ? True : False;
            }
            if (manifest_tokenizer.special_tokens) {
                info.special_tokens = manifest_tokenizer.special_tokens;
            }
        }
        foreach hash<ModelArtifactInfo> a in (artifacts) {
            switch (a.role) {
                case "tokenizer":
                    info.tokenizer_json = a.name;
                    break;
                case "tokenizer_config":
                    info.tokenizer_config_json = a.name;
                    if (a.metadata) {
                        info.model_max_length = a.metadata.model_max_length;
                        info.has_chat_template = a.metadata.has_chat_template ?? False;
                    }
                    break;
                case "generation_config":
                    info.generation_config_json = a.name;
                    break;
                case "special_tokens":
                    info.special_tokens_map_json = a.name;
                    if (a.metadata) {
                        info.special_tokens = a.metadata;
                    }
                    break;
            }
        }
        return info;
    }

    private static string inferFramework(list<hash<ModelArtifactInfo>> artifacts) {
        foreach hash<ModelArtifactInfo> a in (artifacts) {
            if (a.format == "qml") {
                return "qore";
            }
            if (a.format == "onnx") {
                return "onnx";
            }
        }
        foreach hash<ModelArtifactInfo> a in (artifacts) {
            if (a.format == "safetensors" || a.format == "pytorch-pickle"
                    || a.format == "torch-export") {
                return "huggingface";
            }
        }
        return "unknown";
    }

    private static string inferRuntime(hash<ModelArtifactInfo> primary) {
        if (primary.format == "qml") {
            return "qore-ml";
        }
        if (primary.format == "onnx") {
            return "onnxruntime";
        }
        return "metadata-only";
    }

    private static string inferArtifactKind(hash<ModelArtifactInfo> primary,
            list<hash<ModelArtifactInfo>> artifacts) {
        if (primary.format == "qml" || primary.format == "onnx") {
            return "executable";
        }
        if (primary.format == "safetensors") {
            return "weights-only";
        }
        if (primary.format == "pytorch-pickle" || primary.format == "torch-export") {
            return "source";
        }
        foreach hash<ModelArtifactInfo> a in (artifacts) {
            if (a.role == "tokenizer") {
                return "tokenizer-only";
            }
        }
        return "metadata-only";
    }

    private static string inferPackageSafety(list<hash<ModelArtifactInfo>> artifacts) {
        foreach hash<ModelArtifactInfo> a in (artifacts) {
            if (a.format == "pytorch-pickle") {
                return "unsafe-pickle";
            }
        }
        foreach hash<ModelArtifactInfo> a in (artifacts) {
            if (a.format == "safetensors") {
                return "safe-metadata";
            }
        }
        return "safe";
    }

    private static string inferAlgorithm(list<hash<ModelArtifactInfo>> artifacts) {
        foreach hash<ModelArtifactInfo> a in (artifacts) {
            if (a.role == "config" && a.metadata.model_type) {
                return string(a.metadata.model_type);
            }
        }
        return "ModelPackage";
    }

    private static *string taskFromExecutionPlan(*hash<auto> plan) {
        if (!plan) {
            return NOTHING;
        }
        if (plan.task.val()) {
            return string(plan.task);
        }
        if (plan.type.val()) {
            string type = string(plan.type).lwr();
            type =~ s/^onnx[\/:]//;
            return type;
        }
        return NOTHING;
    }

    private static string inferExecutionTask(*auto requested_task,
            list<hash<ModelArtifactInfo>> artifacts, string runtime) {
        if (requested_task.val()) {
            return normalizeExecutionTask(requested_task);
        }
        if (runtime != "onnxruntime") {
            return "raw";
        }

        string hints = "";
        foreach hash<ModelArtifactInfo> a in (artifacts) {
            hints += " " + a.name.lwr() + " " + a.role.lwr();
            if (!a.metadata) {
                continue;
            }
            if (a.metadata.model_type.val()) {
                hints += " " + string(a.metadata.model_type).lwr();
            }
            if (a.metadata.architectures) {
                foreach auto arch in (a.metadata.architectures) {
                    hints += " " + string(arch).lwr();
                }
            }
        }

        if (hints =~ /(rerank|ranker|cross[-_ ]?encoder)/) {
            return "reranker";
        }
        if (hints =~ /(embedding|embedder|sentence[-_ ]?transformer|bge|e5|minilm|mpnet)/) {
            return "embedding";
        }
        if (hints =~ /(tok(en)?[-_ ]?classification|for[-_ ]?token[-_ ]?classification)/) {
            return "token-classification";
        }
        if (hints =~ /(sequence[-_ ]?classification|for[-_ ]?sequence[-_ ]?classification|classifier)/) {
            return "sequence-classification";
        }
        if (hints =~ /(regression|regressor)/) {
            return "regression";
        }
        if (hints =~ /(encoder[-_ ]?decoder|seq2seq|bart|t5)/) {
            return "encoder-decoder";
        }
        if (hints =~ /(causal[-_ ]?lm|for[-_ ]?causal[-_ ]?lm|text[-_ ]?generation|gpt|llama|mistral|qwen)/) {
            return "causal-lm";
        }
        return "raw";
    }

    private static string normalizeExecutionTask(auto task) {
        string t = string(task).lwr();
        t =~ s/^onnx[\/:]//;
        t =~ s/_/-/g;
        switch (t) {
            case "raw":
            case "classification":
            case "regression":
            case "embedding":
            case "reranker":
            case "token-classification":
            case "sequence-classification":
            case "encoder-decoder":
            case "causal-lm":
                return t;
            case "text-classification":
                return "sequence-classification";
            case "feature-extraction":
            case "sentence-embedding":
            case "sentence-transformer":
                return "embedding";
            case "ranking":
            case "cross-encoder":
                return "reranker";
            case "text-generation":
            case "causal-language-modeling":
                return "causal-lm";
            case "seq2seq":
                return "encoder-decoder";
        }
        throw "ML-REGISTRY-ERROR",
            sprintf("unsupported ONNX execution task %y; valid tasks are: "
                "raw, classification, regression, embedding, reranker, "
                "token-classification, sequence-classification, encoder-decoder, "
                "and causal-lm", task);
    }

    private static hash<auto> buildExecutionPlan(string runtime, string task,
            hash<ModelArtifactInfo> primary, *hash<auto> input_schema,
            *hash<auto> output_schema, *hash<auto> option_plan,
            *hash<auto> preprocess, *hash<auto> postprocess,
            *hash<auto> dynamic_axes, *hash<TokenizerArtifactInfo> tokenizer) {
        return QoreOnnxTools::OnnxProcessingSpecs::buildExecutionPlan({
            "plan": option_plan,
            "runtime": runtime,
            "task": task,
            "artifact_name": primary.name,
            "local_executable": runtime == "onnxruntime",
            "requires": runtime == "onnxruntime" ? ("ml",) : (),
            "input_schema": input_schema,
            "output_schema": output_schema,
            "dynamic_axes": dynamic_axes,
            "tokenizer": tokenizer,
            "preprocess": preprocess,
            "postprocess": postprocess,
        });
    }
}

} # namespace QoreModelRegistry
