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

%ifndef NoJson
        if (format == "json") {
            info.metadata = inspectJsonFile(path, name);
        } else if (format == "safetensors") {
            info.metadata = inspectSafeTensorsFile(path);
        }
%endif
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

        hash<ModelArtifactInfo> primary = selectPrimaryArtifact(artifacts);
        hash<TokenizerArtifactInfo> tokenizer = buildTokenizerInfo(artifacts);
        string framework = inferFramework(artifacts);
        string runtime = inferRuntime(primary);
        string artifact_kind = inferArtifactKind(primary, artifacts);
        string safety = inferPackageSafety(artifacts);
        *hash<auto> input_schema;
        *hash<auto> output_schema;
        if (primary.format == "onnx" && primary.metadata) {
            if (exists primary.metadata.inputs) {
                input_schema = {"tensors": primary.metadata.inputs};
            }
            if (exists primary.metadata.outputs) {
                output_schema = {"tensors": primary.metadata.outputs};
            }
        }

        return cast<hash<ModelVersionInfo>>({
            "version_id": options.version_id,
            "name": name,
            "algorithm": options.algorithm ?? inferAlgorithm(artifacts),
            "created": format_date("YYYY-MM-DDTHH:mm:ssZ", now()),
            "description": options.description,
            "metrics": options.metrics,
            "tags": options.tags ?? (),
            "file_size": primary.size,
            "format": primary.format,
            "training_data_hash": options.training_data_hash,
            "framework": options.framework ?? framework,
            "artifact_kind": artifact_kind,
            "runtime": runtime,
            "executable": runtime == "qore-ml" || runtime == "onnxruntime",
            "safety": safety,
            "artifacts": artifacts,
            "tokenizer": tokenizer,
            "execution": <ModelExecutionInfo>{
                "runtime": runtime,
                "local_executable": runtime == "qore-ml" || runtime == "onnxruntime",
                "entry_artifact": primary.name,
                "requires": runtime == "onnxruntime" ? ("ml",) : (),
                "input_schema": input_schema,
                "output_schema": output_schema,
            },
            "config": options.config,
        });
    }

    #! Returns True if a file name is part of a supported model package
    static bool isSupportedPackageFile(string name) {
        string lname = name.lwr();
        return lname == "config.json"
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

%ifndef NoJson
    private static hash<auto> inspectJsonFile(string path, string name) {
        hash<auto> json = parse_json(ReadOnlyFile::readTextFile(path));
        hash<auto> result;
        result.json_keys = sort(keys json);
        if (name == "config.json") {
            result.model_type = json.model_type;
            result.architectures = json.architectures;
            result.torch_dtype = json.torch_dtype;
            result.transformers_version = json.transformers_version;
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
%endif

    private static hash<ModelArtifactInfo> selectPrimaryArtifact(
            list<hash<ModelArtifactInfo>> artifacts) {
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

    private static hash<TokenizerArtifactInfo> buildTokenizerInfo(
            list<hash<ModelArtifactInfo>> artifacts) {
        hash<TokenizerArtifactInfo> info = <TokenizerArtifactInfo>{};
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
}

} # namespace QoreModelRegistry
