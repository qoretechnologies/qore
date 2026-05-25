# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OnnxQuantizer.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ONNX Runtime Python dependency status
public hashdecl OnnxPythonDependencyInfo {
    #! Python executable used for dependency checks and tooling
    string python;
    #! True if \c onnxruntime and \c onnxruntime.quantization can be imported
    bool available;
    #! ONNX Runtime Python package version, if available
    *string onnxruntime_version;
    #! Raw diagnostic output from the dependency check
    *string output;
    #! Process return code from the dependency check
    int rc = 0;
    #! Human-readable installation hint when dependencies are missing
    *string install_hint;
}

#! Options for ONNX quantization
public hashdecl OnnxQuantizationOptions {
    #! Python executable; defaults to \c QORE_ONNX_PYTHON or \c python3
    *string python;
    #! Quantization method: \c dynamic or \c static
    string method = "dynamic";
    #! Quantized weight type: \c QInt8 or \c QUInt8
    string weight_type = "QInt8";
    #! Static quantization activation type: \c QInt8 or \c QUInt8
    string activation_type = "QUInt8";
    #! Static calibration method: \c MinMax, \c Entropy, or \c Percentile
    string calibration_method = "MinMax";
    #! Optional comma-separated ONNX op types to quantize
    *string op_types;
    #! Optional comma-separated node names to quantize
    *string nodes_to_quantize;
    #! Optional comma-separated node names to exclude
    *string nodes_to_exclude;
    #! Enable per-channel quantization when supported
    bool per_channel = False;
    #! Enable ONNX Runtime reduce_range quantization option
    bool reduce_range = False;
    #! For static quantization: JSON file containing calibration input records
    *string calibration_data_json_path;
    #! Optional JSON file containing ONNX Runtime extra_options
    *string extra_options_json_path;
    #! Return the execution plan without invoking Python
    bool dry_run = False;
    #! Allow overwriting the output file
    bool force = False;
    #! Load the produced model with ML::OnnxModel after quantization
    bool validate = True;
}

#! Result of ONNX quantization
public hashdecl OnnxQuantizationResult {
    #! Source ONNX model path
    string input_path;
    #! Quantized ONNX model path
    string output_path;
    #! Quantization method
    string method;
    #! True if Python quantization was executed
    bool executed;
    #! Command vector before shell quoting
    list<string> command;
    #! Shell command used for execution, if executed
    *string shell_command;
    #! Process return code, if executed
    *int rc;
    #! Captured process output
    *string output;
    #! Dependency status for the selected Python executable
    hash<OnnxPythonDependencyInfo> dependency;
    #! Source model metadata when available
    *hash<auto> source_model;
    #! Quantized model metadata when validation is enabled and succeeds
    *hash<auto> quantized_model;
}

/** ONNX Runtime Python quantization orchestrator.

    This class provides Qore-owned validation, dependency diagnostics, dry-run
    planning, command construction, and result metadata for ONNX Runtime's
    Python quantization helpers.  The implementation intentionally delegates
    the quantization algorithm to ONNX Runtime so that Qore deployment code can
    use the same quantized artifacts accepted by other ONNX tooling.
*/
public class OnnxQuantizer {
    private {
        const DependencyCheckCode =
            "import onnxruntime, onnxruntime.quantization\n"
            "print(onnxruntime.__version__)\n";

        const InstallHint =
            "Install the Python ONNX Runtime package with quantization support, for example: "
            "python3 -m pip install onnxruntime onnx. If you need GPU tooling, install the "
            "ONNX Runtime package matching the target provider, then pass python: \"/path/to/python\".";
    }

    /** Checks whether the selected Python executable has ONNX Runtime
        quantization support.

        @param options optional quantization options; \c python selects the
        executable to probe, otherwise \c QORE_ONNX_PYTHON or \c python3 is
        used
        @return dependency status including the selected executable, ONNX
        Runtime version when available, captured diagnostics, and an
        installation hint when dependencies are missing
    */
    static hash<OnnxPythonDependencyInfo> checkPythonDependencies(
            *hash<OnnxQuantizationOptions> options) {
        hash<OnnxQuantizationOptions> opts = normalizeOptions(options);
        string python = resolvePython(opts);
        int rc;
        string out = backquote(sprintf("%s -c %s 2>&1",
            shellQuote(python), shellQuote(DependencyCheckCode)), \rc);
        out = trim(out);

        hash<OnnxPythonDependencyInfo> rv = <OnnxPythonDependencyInfo>{
            "python": python,
            "available": rc == 0,
            "rc": rc,
            "output": out,
        };
        if (rc == 0) {
            rv.onnxruntime_version = out.split("\n")[0];
        } else {
            rv.install_hint = InstallHint;
        }
        return rv;
    }

    /** Builds a quantization execution plan without invoking Python tooling.

        @param input_path source ONNX model path
        @param output_path planned quantized ONNX model path
        @param options optional quantization options
        @return dry-run quantization result with command arguments and source
        model metadata
        @throw QORE-ONNX-TOOLS-ERROR if input paths or quantization options are
        invalid
    */
    static hash<OnnxQuantizationResult> plan(string input_path, string output_path,
            *hash<OnnxQuantizationOptions> options) {
        return quantize(input_path, output_path, withDryRun(options));
    }

    /** Quantizes an ONNX model through ONNX Runtime's Python tooling.

        @param input_path source ONNX model path
        @param output_path quantized ONNX model path to create
        @param options optional quantization options controlling method,
        quantized value types, calibration input, op/node filters, extra ONNX
        Runtime options, overwrite policy, and output validation
        @return quantization result with command arguments, dependency
        diagnostics, source metadata, process output, and output model metadata
        when validation succeeds
        @throw QORE-ONNX-TOOLS-ERROR if paths/options are invalid, required
        Python dependencies are missing, ONNX Runtime quantization fails, or the
        expected output model is not produced
    */
    static hash<OnnxQuantizationResult> quantize(string input_path, string output_path,
            *hash<OnnxQuantizationOptions> options) {
        hash<OnnxQuantizationOptions> opts = normalizeOptions(options);
        validatePaths(input_path, output_path, opts);
        validateOptions(opts);

        hash<OnnxPythonDependencyInfo> dep = opts.dry_run
            ? <OnnxPythonDependencyInfo>{
                "python": resolvePython(opts),
                "available": False,
                "install_hint": InstallHint,
            }
            : checkPythonDependencies(opts);

        list<string> command = buildCommand("<generated-python-script>", input_path,
            output_path, opts);
        hash<OnnxQuantizationResult> result = <OnnxQuantizationResult>{
            "input_path": input_path,
            "output_path": output_path,
            "method": opts.method,
            "executed": False,
            "command": command,
            "dependency": dep,
            "source_model": inspectModel(input_path),
        };

        if (opts.dry_run) {
            return result;
        }

        if (!dep.available) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("ONNX quantization requires Python package 'onnxruntime' "
                    "with 'onnxruntime.quantization' import support for Python executable %y. "
                    "Dependency check failed with rc=%d and output: %s. %s",
                    dep.python, dep.rc, dep.output ?? "", dep.install_hint ?? InstallHint);
        }

        string script_path = writeTempScript();
        on_exit { safeUnlink(script_path); }
        command = buildCommand(script_path, input_path, output_path, opts);
        result.command = command;
        string shell_command = commandToShell(command) + " 2>&1";
        result.shell_command = shell_command;

        int rc;
        string out = backquote(shell_command, \rc);
        result.executed = True;
        result.rc = rc;
        result.output = out;
        if (rc != 0) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("ONNX quantization failed with rc=%d: %s", rc, trim(out));
        }
        if (!is_file(output_path)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("ONNX quantization completed but output file %y was not created",
                    output_path);
        }

        if (opts.validate) {
            result.quantized_model = inspectModel(output_path);
        }
        return result;
    }

    private static hash<OnnxQuantizationOptions> withDryRun(
            *hash<OnnxQuantizationOptions> options) {
        hash<OnnxQuantizationOptions> opts = normalizeOptions(options);
        opts.dry_run = True;
        return opts;
    }

    private static hash<OnnxQuantizationOptions> normalizeOptions(
            *hash<OnnxQuantizationOptions> options) {
        return options ?? <OnnxQuantizationOptions>{};
    }

    private static string resolvePython(hash<OnnxQuantizationOptions> opts) {
        if (opts.python.val()) {
            return opts.python;
        }
        if (ENV.QORE_ONNX_PYTHON.val()) {
            return ENV.QORE_ONNX_PYTHON;
        }
        return "python3";
    }

    private static nothing validatePaths(string input_path, string output_path,
            hash<OnnxQuantizationOptions> opts) {
        if (!input_path.val()) {
            throw "QORE-ONNX-TOOLS-ERROR", "input_path must not be empty";
        }
        if (!output_path.val()) {
            throw "QORE-ONNX-TOOLS-ERROR", "output_path must not be empty";
        }
        if (input_path == output_path) {
            throw "QORE-ONNX-TOOLS-ERROR",
                "input_path and output_path must be different files";
        }
        if (!is_file(input_path)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("input ONNX model %y does not exist or is not a regular file",
                    input_path);
        }
        if (is_file(output_path) && !opts.force) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("output file %y already exists; pass force: True to overwrite it",
                    output_path);
        }
        if (opts.calibration_data_json_path.val()
                && !is_file(opts.calibration_data_json_path)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("calibration_data_json_path %y does not exist or is not a regular file",
                    opts.calibration_data_json_path);
        }
        if (opts.extra_options_json_path.val()
                && !is_file(opts.extra_options_json_path)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("extra_options_json_path %y does not exist or is not a regular file",
                    opts.extra_options_json_path);
        }
    }

    private static nothing validateOptions(hash<OnnxQuantizationOptions> opts) {
        if (!("dynamic", "static").contains(opts.method)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("unsupported quantization method %y; valid values are 'dynamic' and 'static'",
                    opts.method);
        }
        validateQuantType("weight_type", opts.weight_type);
        validateQuantType("activation_type", opts.activation_type);
        if (!("MinMax", "Entropy", "Percentile").contains(opts.calibration_method)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("unsupported calibration_method %y; valid values are 'MinMax', "
                    "'Entropy', and 'Percentile'", opts.calibration_method);
        }
        if (opts.method == "static" && !opts.calibration_data_json_path.val()) {
            throw "QORE-ONNX-TOOLS-ERROR",
                "static ONNX quantization requires calibration_data_json_path with "
                "a JSON list of input-name to tensor-array records";
        }
    }

    private static nothing validateQuantType(string opt_name, string value) {
        if (!("QInt8", "QUInt8").contains(value)) {
            throw "QORE-ONNX-TOOLS-ERROR",
                sprintf("unsupported %s %y; valid values are 'QInt8' and 'QUInt8'",
                    opt_name, value);
        }
    }

    private static list<string> buildCommand(string script_path, string input_path,
            string output_path, hash<OnnxQuantizationOptions> opts) {
        list<string> cmd = (resolvePython(opts), script_path,
            "--input", input_path,
            "--output", output_path,
            "--method", opts.method,
            "--weight-type", opts.weight_type,
            "--activation-type", opts.activation_type,
            "--calibration-method", opts.calibration_method);
        if (opts.per_channel) {
            push cmd, "--per-channel";
        }
        if (opts.reduce_range) {
            push cmd, "--reduce-range";
        }
        appendCsvOption(\cmd, "--op-types", opts.op_types);
        appendCsvOption(\cmd, "--nodes-to-quantize", opts.nodes_to_quantize);
        appendCsvOption(\cmd, "--nodes-to-exclude", opts.nodes_to_exclude);
        if (opts.calibration_data_json_path.val()) {
            push cmd, "--calibration-data";
            push cmd, opts.calibration_data_json_path;
        }
        if (opts.extra_options_json_path.val()) {
            push cmd, "--extra-options";
            push cmd, opts.extra_options_json_path;
        }
        return cmd;
    }

    private static nothing appendCsvOption(reference<list<string>> cmd,
            string option_name, *string value) {
        if (!value.val()) {
            return;
        }
        push cmd, option_name;
        push cmd, value;
    }

    private static string writeTempScript() {
        hash<FsUtil::TmpFileHash> tmp = FsUtil::make_tmp_file("qore-onnx-quantize-", ".py");
        tmp.file.write(makePythonScript());
        tmp.file.close();
        return tmp.path;
    }

    private static string makePythonScript() {
        list<string> lines = (
            "import argparse",
            "import json",
            "import numpy as np",
            "from onnxruntime.quantization import (",
            "    CalibrationDataReader, CalibrationMethod, QuantFormat, QuantType,",
            "    quantize_dynamic, quantize_static,",
            ")",
            "",
            "def split_csv(value):",
            "    if not value:",
            "        return None",
            "    return [part.strip() for part in value.split(',') if part.strip()]",
            "",
            "def quant_type(name):",
            "    try:",
            "        return getattr(QuantType, name)",
            "    except AttributeError as exc:",
            "        raise SystemExit(f'unsupported QuantType {name!r}') from exc",
            "",
            "def calibration_method(name):",
            "    try:",
            "        return getattr(CalibrationMethod, name)",
            "    except AttributeError as exc:",
            "        raise SystemExit(f'unsupported CalibrationMethod {name!r}') from exc",
            "",
            "def load_extra_options(path):",
            "    if not path:",
            "        return None",
            "    with open(path, 'r', encoding='utf-8') as handle:",
            "        data = json.load(handle)",
            "    if not isinstance(data, dict):",
            "        raise SystemExit('extra options JSON must be an object')",
            "    return data",
            "",
            "class JsonCalibrationDataReader(CalibrationDataReader):",
            "    def __init__(self, path):",
            "        with open(path, 'r', encoding='utf-8') as handle:",
            "            rows = json.load(handle)",
            "        if not isinstance(rows, list):",
            "            raise SystemExit('calibration data JSON must be a list of records')",
            "        self.rows = iter(rows)",
            "",
            "    def get_next(self):",
            "        try:",
            "            row = next(self.rows)",
            "        except StopIteration:",
            "            return None",
            "        if not isinstance(row, dict):",
            "            raise SystemExit('each calibration data item must be an object')",
            "        return {key: np.asarray(value) for key, value in row.items()}",
            "",
            "parser = argparse.ArgumentParser(description='Qore ONNX quantization helper')",
            "parser.add_argument('--input', required=True)",
            "parser.add_argument('--output', required=True)",
            "parser.add_argument('--method', choices=('dynamic', 'static'), required=True)",
            "parser.add_argument('--weight-type', required=True)",
            "parser.add_argument('--activation-type', required=True)",
            "parser.add_argument('--calibration-method', required=True)",
            "parser.add_argument('--op-types')",
            "parser.add_argument('--nodes-to-quantize')",
            "parser.add_argument('--nodes-to-exclude')",
            "parser.add_argument('--per-channel', action='store_true')",
            "parser.add_argument('--reduce-range', action='store_true')",
            "parser.add_argument('--calibration-data')",
            "parser.add_argument('--extra-options')",
            "args = parser.parse_args()",
            "",
            "common = {",
            "    'op_types_to_quantize': split_csv(args.op_types),",
            "    'per_channel': args.per_channel,",
            "    'reduce_range': args.reduce_range,",
            "    'nodes_to_quantize': split_csv(args.nodes_to_quantize),",
            "    'nodes_to_exclude': split_csv(args.nodes_to_exclude),",
            "    'extra_options': load_extra_options(args.extra_options),",
            "}",
            "common = {key: value for key, value in common.items() if value is not None}",
            "",
            "if args.method == 'dynamic':",
            "    quantize_dynamic(args.input, args.output,",
            "                     weight_type=quant_type(args.weight_type), **common)",
            "else:",
            "    if not args.calibration_data:",
            "        raise SystemExit('static quantization requires --calibration-data')",
            "    reader = JsonCalibrationDataReader(args.calibration_data)",
            "    quantize_static(args.input, args.output, reader,",
            "                    quant_format=QuantFormat.QDQ,",
            "                    activation_type=quant_type(args.activation_type),",
            "                    weight_type=quant_type(args.weight_type),",
            "                    calibrate_method=calibration_method(args.calibration_method),",
            "                    **common)",
            "",
            "print(json.dumps({'output': args.output, 'method': args.method}, sort_keys=True))"
        );
        return join("\n", lines) + "\n";
    }

    private static hash<auto> inspectModel(string path) {
        hash<auto> rv = {
            "path": path,
            "size": hstat(path).size,
        };
        try {
            ML::OnnxModel model(path, <ML::OnnxSessionConfig>{"providers": ()});
            hash<ML::OnnxModelInfo> info = model.getModelInfo();
            rv.producer_name = info.producer_name;
            rv.description = info.description;
            rv.version = info.version;
            rv.inputs = info.inputs;
            rv.outputs = info.outputs;
            rv.active_provider = info.active_provider;
        } catch (hash<ExceptionInfo> ex) {
            rv.error = sprintf("%s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    private static string commandToShell(list<string> command) {
        return join(" ", map shellQuote($1), command);
    }

    private static string shellQuote(string value) {
        return "'" + replace(value, "'", "'\"'\"'") + "'";
    }

    private static nothing safeUnlink(string path) {
        if (path.val() && is_file(path)) {
            unlink(path);
        }
    }
}

} # namespace QoreOnnxTools
