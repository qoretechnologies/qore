# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OnnxPackageTools.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Options for deterministic ONNX package assembly
public hashdecl OnnxPackageOptions {
    #! Allow overwriting existing files in the output directory
    bool force = False;
    #! Include tokenizer/config companion files from the source directory
    bool include_companions = True;
    #! Include common ONNX external-data companion files
    bool include_external_data = True;
    #! Validate ONNX entry artifacts after packaging when ONNX Runtime is available
    bool validate = True;
    #! Write a JSON package manifest in the output directory
    bool write_manifest = True;
    #! Manifest filename written when \c write_manifest is True
    string manifest_name = "qore-onnx-package.json";
    #! Optional ONNX Runtime session configuration for inspection and validation
    *hash<ML::OnnxSessionConfig> session_config;
}

#! Metadata for a packaged artifact
public hashdecl OnnxPackageArtifactInfo {
    #! Package-local artifact name
    string name;
    #! Artifact role such as \c model, \c tokenizer, \c config, or \c weights
    string role;
    #! Artifact format such as \c onnx, \c json, or \c safetensors
    string format;
    #! Artifact size in bytes
    int size;
    #! SHA-256 checksum of the copied artifact
    string sha256;
    #! True if the artifact can be inspected without executing model code
    bool safe_to_parse = False;
    #! True if the artifact can be executed locally by ONNX Runtime
    bool executable = False;
    #! Original source path copied into the package
    string source_path;
    #! Optional extracted metadata
    *hash<auto> metadata;
}

#! Result of deterministic ONNX package assembly
public hashdecl OnnxPackageResult {
    #! Source file or directory packaged
    string source_path;
    #! Output package directory
    string output_dir;
    #! Source framework classification
    string framework;
    #! Runtime classification
    string runtime;
    #! Artifact safety classification
    string safety;
    #! Primary package artifact name
    string entry_artifact;
    #! True if the package is executable locally by ONNX Runtime
    bool executable;
    #! Sorted artifact metadata
    list<hash<OnnxPackageArtifactInfo>> artifacts;
    #! Optional manifest path written to disk
    *string manifest_path;
    #! Optional ONNX validation result for the packaged entry artifact
    *hash<OnnxValidationResult> validation;
}

/** Deterministic package assembly for ONNX deployment artifacts.

    The helper copies a raw ONNX file or a flat model package directory to a
    target directory, classifies companion artifacts, records SHA-256 checksums,
    and writes a manifest suitable for later registry ingestion. PyTorch pickle
    artifacts are copied only as unsafe source artifacts and are never marked as
    locally executable.

    @since QoreOnnxTools 1.0
*/
public class OnnxPackageTools {
    /** Creates a deterministic ONNX package directory.

        @param source_path source ONNX file or flat model package directory
        @param output_dir target directory
        @param options optional package assembly options

        @return package metadata and optional validation result

        @throw QORE-ONNX-TOOLS-ERROR if the source, target, or manifest options
        are invalid
    */
    static hash<OnnxPackageResult> createPackage(string source_path, string output_dir,
            *hash<OnnxPackageOptions> options) {
        hash<OnnxPackageOptions> opts = options ?? <OnnxPackageOptions>{};
        validateSource(source_path);
        validateOutputDir(output_dir);
        validatePackageArtifactName(opts.manifest_name);
        prepareOutputDir(output_dir, opts.force);

        list<string> files = discoverFiles(source_path, opts);
        if (!files) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("source %y does not contain supported ONNX package artifacts",
                    source_path);
        }

        list<hash<OnnxPackageArtifactInfo>> artifacts();
        foreach string path in (files) {
            string name = basename(path);
            validatePackageArtifactName(name);
            binary data = ReadOnlyFile::readBinaryFile(path);
            writeBinaryFile(output_dir + DirSep + name, data);
            push artifacts, inspectArtifact(path, name, data, opts.session_config);
        }
        artifacts = sortArtifacts(artifacts);

        hash<OnnxPackageArtifactInfo> primary = selectPrimaryArtifact(artifacts);
        hash<OnnxPackageResult> result = <OnnxPackageResult>{
            "source_path": source_path,
            "output_dir": output_dir,
            "framework": inferFramework(artifacts),
            "runtime": inferRuntime(primary),
            "safety": inferSafety(artifacts),
            "entry_artifact": primary.name,
            "executable": primary.format == "onnx",
            "artifacts": artifacts,
        };

        if (opts.validate && primary.format == "onnx"
                && ML::ml_get_capabilities().has_onnx) {
            result.validation = OnnxModelTools::validate(output_dir + DirSep + primary.name,
                opts.session_config);
        }

        if (opts.write_manifest) {
            string manifest_path = output_dir + DirSep + opts.manifest_name;
            writeManifest(manifest_path, result);
            result.manifest_path = manifest_path;
        }

        return result;
    }

    private static nothing validateSource(string source_path) {
        if (!source_path.val()) {
            throw "QORE-ONNX-TOOLS-ERROR", "source_path must not be empty";
        }
        if (!is_file(source_path) && !is_dir(source_path)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("source path %y does not exist or is not a file or directory",
                    source_path);
        }
    }

    private static nothing validateOutputDir(string output_dir) {
        if (!output_dir.val()) {
            throw "QORE-ONNX-TOOLS-ERROR", "output_dir must not be empty";
        }
        validatePackageArtifactName(basename(output_dir));
    }

    private static nothing prepareOutputDir(string output_dir, bool force) {
        if (!is_dir(output_dir)) {
            mkdir_ex(output_dir, 0755, True);
            return;
        }
        Dir d();
        if (!d.chdir(output_dir)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("could not open output directory %y", output_dir);
        }
        if (d.listFileNames() && !force) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("output directory %y is not empty; pass force: True to overwrite "
                    "package files", output_dir);
        }
    }

    private static list<string> discoverFiles(string source_path,
            hash<OnnxPackageOptions> opts) {
        list<string> files();
        if (is_file(source_path)) {
            if (!isSupportedPackageFile(basename(source_path))) {
                return files;
            }
            appendFile(\files, source_path);
            string dir = dirname(source_path);
            if (opts.include_external_data && inferArtifactFormat(basename(source_path)) == "onnx") {
                foreach string external_name in (commonOnnxExternalDataNames(basename(source_path))) {
                    appendFile(\files, dir + DirSep + external_name);
                }
            }
            if (opts.include_companions) {
                foreach string companion in (companionFileNames()) {
                    appendFile(\files, dir + DirSep + companion);
                }
            }
            return sort(files);
        }

        Dir d();
        if (!d.chdir(source_path)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("could not open source directory %y", source_path);
        }
        foreach string file in (sort(d.listFileNames())) {
            if (!isSupportedPackageFile(file)) {
                continue;
            }
            string format = inferArtifactFormat(file);
            string role = inferArtifactRole(file, format);
            if (!opts.include_external_data && role == "external_data") {
                continue;
            }
            if (!opts.include_companions && isCompanionRole(role)) {
                continue;
            }
            appendFile(\files, source_path + DirSep + file);
        }
        return files;
    }

    private static nothing appendFile(reference<list<string>> files, string path) {
        if (is_file(path) && !files.contains(path)) {
            push files, path;
        }
    }

    private static hash<OnnxPackageArtifactInfo> inspectArtifact(string path, string name,
            binary data, *hash<ML::OnnxSessionConfig> session_config) {
        string format = inferArtifactFormat(name);
        string role = inferArtifactRole(name, format);
        hash<OnnxPackageArtifactInfo> info = <OnnxPackageArtifactInfo>{
            "name": name,
            "role": role,
            "format": format,
            "size": data.size(),
            "sha256": data.toSHA256(),
            "safe_to_parse": isSafeToParse(format),
            "executable": format == "onnx",
            "source_path": path,
        };

        if (format == "onnx") {
            info.metadata = inspectOnnxArtifact(path, session_config);
        } else if (format == "json") {
            info.metadata = inspectJsonArtifact(path);
        }
        return info;
    }

    private static hash<auto> inspectOnnxArtifact(string path,
            *hash<ML::OnnxSessionConfig> session_config) {
        hash<auto> metadata;
        metadata.runtime_available = ML::ml_get_capabilities().has_onnx;
        if (!metadata.runtime_available) {
            return metadata;
        }
        try {
            hash<auto> inspection = OnnxModelTools::inspect(path, session_config);
            metadata.info = inspection.info;
            metadata.inputs = inspection.inputs;
            metadata.outputs = inspection.outputs;
            metadata.provider_report = inspection.provider_report;
        } catch (hash<ExceptionInfo> ex) {
            metadata.error = sprintf("%s: %s", ex.err, ex.desc);
        }
        return metadata;
    }

    private static hash<auto> inspectJsonArtifact(string path) {
%ifdef NoJson
        return {"json_available": False};
%else
        hash<auto> parsed = parse_json(ReadOnlyFile::readTextFile(path));
        hash<auto> metadata;
        metadata.json_available = True;
        metadata.json_keys = sort(keys parsed);
        if (parsed.model_type.val()) {
            metadata.model_type = parsed.model_type;
        }
        if (parsed.architectures) {
            metadata.architectures = parsed.architectures;
        }
        if (parsed.transformers_version.val()) {
            metadata.transformers_version = parsed.transformers_version;
        }
        if (parsed.model_max_length) {
            metadata.model_max_length = parsed.model_max_length;
        }
        return metadata;
%endif
    }

    private static nothing writeManifest(string manifest_path,
            hash<OnnxPackageResult> result) {
%ifdef NoJson
        throw "QORE-ONNX-TOOLS-ERROR",
            "the json module is required to write an ONNX package manifest; "
            "install module-json or pass write_manifest: False";
%else
        writeTextFile(manifest_path, make_json(makePortableManifest(result)) + "\n");
%endif
    }

    private static hash<auto!> makePortableManifest(hash<OnnxPackageResult> result) {
        hash<auto!> manifest;
        manifest.manifest_version = 1;
        manifest.framework = result.framework;
        manifest.runtime = result.runtime;
        manifest.safety = result.safety;
        manifest.entry_artifact = result.entry_artifact;
        manifest.executable = result.executable;
        list<hash<auto!>> artifacts();
        foreach hash<OnnxPackageArtifactInfo> artifact in (result.artifacts) {
            hash<auto!> portable;
            portable.name = artifact.name;
            portable.role = artifact.role;
            portable.format = artifact.format;
            portable.size = artifact.size;
            portable.sha256 = artifact.sha256;
            portable.safe_to_parse = artifact.safe_to_parse;
            portable.executable = artifact.executable;
            if (artifact.metadata) {
                portable.metadata = artifact.metadata;
            }
            push artifacts, portable;
        }
        manifest.artifacts = artifacts;
        if (result.validation) {
            manifest.validation = {
                "valid": result.validation.valid,
                "issues": result.validation.issues,
            };
        }
        return manifest;
    }

    private static list<hash<OnnxPackageArtifactInfo>> sortArtifacts(
            list<hash<OnnxPackageArtifactInfo>> artifacts) {
        list<string> names = sort(map $1.name, artifacts);
        list<hash<OnnxPackageArtifactInfo>> rv();
        foreach string name in (names) {
            foreach hash<OnnxPackageArtifactInfo> artifact in (artifacts) {
                if (artifact.name == name) {
                    push rv, artifact;
                    break;
                }
            }
        }
        return rv;
    }

    private static hash<OnnxPackageArtifactInfo> selectPrimaryArtifact(
            list<hash<OnnxPackageArtifactInfo>> artifacts) {
        foreach string format in ("onnx", "safetensors", "torch-export", "pytorch-pickle") {
            foreach hash<OnnxPackageArtifactInfo> artifact in (artifacts) {
                if (artifact.role == "model" && artifact.format == format) {
                    return artifact;
                }
            }
            foreach hash<OnnxPackageArtifactInfo> artifact in (artifacts) {
                if (artifact.format == format) {
                    return artifact;
                }
            }
        }
        return artifacts[0];
    }

    private static string inferFramework(list<hash<OnnxPackageArtifactInfo>> artifacts) {
        foreach hash<OnnxPackageArtifactInfo> artifact in (artifacts) {
            if (artifact.format == "onnx") {
                string producer = artifact.metadata && artifact.metadata.info
                    ? string(artifact.metadata.info.producer_name)
                    : "";
                if (producer =~ /skl|sklearn|scikit/i) {
                    return "scikit-learn";
                }
                if (producer =~ /torch|pytorch/i) {
                    return "pytorch";
                }
                return "onnx";
            }
        }
        foreach hash<OnnxPackageArtifactInfo> artifact in (artifacts) {
            if (artifact.format == "safetensors" || artifact.format == "pytorch-pickle"
                    || artifact.format == "torch-export") {
                return "huggingface";
            }
        }
        return "unknown";
    }

    private static string inferRuntime(hash<OnnxPackageArtifactInfo> primary) {
        if (primary.format == "onnx") {
            return "onnxruntime";
        }
        return "metadata-only";
    }

    private static string inferSafety(list<hash<OnnxPackageArtifactInfo>> artifacts) {
        foreach hash<OnnxPackageArtifactInfo> artifact in (artifacts) {
            if (artifact.format == "pytorch-pickle") {
                return "unsafe-pickle";
            }
        }
        foreach hash<OnnxPackageArtifactInfo> artifact in (artifacts) {
            if (artifact.format == "safetensors") {
                return "safe-metadata";
            }
        }
        return "safe";
    }

    private static bool isSupportedPackageFile(string name) {
        string lname = name.lwr();
        return companionFileNames().contains(lname)
            || lname =~ /[.]onnx$/
            || lname =~ /[.]onnx[.]data$/
            || lname =~ /[.]data$/
            || lname =~ /[.]safetensors$/
            || lname =~ /[.](pt|pth|bin)$/
            || lname =~ /[.]pt2$/;
    }

    private static list<string> companionFileNames() {
        return (
            "config.json",
            "tokenizer.json",
            "tokenizer_config.json",
            "generation_config.json",
            "special_tokens_map.json",
        );
    }

    private static list<string> commonOnnxExternalDataNames(string model_name) {
        string stem_data_name = model_name;
        stem_data_name =~ s/[.]onnx$/.data/;
        list<string> rv = (model_name + ".data",);
        if (stem_data_name != model_name + ".data") {
            push rv, stem_data_name;
        }
        return rv;
    }

    private static string inferArtifactFormat(string name) {
        string lname = name.lwr();
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
        if (format == "safetensors" || format == "pytorch-pickle"
                || format == "torch-export") {
            return "weights";
        }
        if (format == "onnx-external-data") {
            return "external_data";
        }
        return "model";
    }

    private static bool isCompanionRole(string role) {
        return role == "tokenizer" || role == "tokenizer_config"
            || role == "generation_config" || role == "special_tokens"
            || role == "config";
    }

    private static bool isSafeToParse(string format) {
        return format == "onnx" || format == "json" || format == "safetensors"
            || format == "torch-export" || format == "onnx-external-data";
    }

    private static string basename(string path) {
        list<string> parts = path.split(DirSep);
        while (parts.size() > 1 && parts.last() == "") {
            pop parts;
        }
        return parts ? parts.last() : path;
    }

    private static string dirname(string path) {
        list<string> parts = path.split(DirSep);
        if (parts.size() <= 1) {
            return ".";
        }
        pop parts;
        string rv = join(DirSep, parts);
        return rv.val() ? rv : DirSep;
    }

    private static nothing validatePackageArtifactName(string artifact_name) {
        if (!artifact_name.val() || artifact_name == "." || artifact_name == ".."
                || artifact_name =~ /[\\\/]/) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("invalid package artifact name '%s'", artifact_name);
        }
    }

    private static nothing writeBinaryFile(string path, binary data) {
        File f();
        f.open(path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
        f.write(data);
    }

    private static nothing writeTextFile(string path, string data) {
        File f();
        f.open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        f.write(data);
    }
}

} # namespace QoreOnnxTools
