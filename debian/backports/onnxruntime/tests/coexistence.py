#!/usr/bin/python3
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise each CPU provider alongside Debian's unmodified ONNX/PyTorch ABI."""
import importlib
import json
from pathlib import Path
import sys
import tempfile

# Both import orders must work: extension loading can otherwise hide ABI clashes.
if sys.argv[1:] == ["runtime-first"]:
    importlib.import_module("onnxruntime")
import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper
import torch
import onnxruntime as ort

rng = np.random.default_rng(2048)
weights = rng.normal(size=(2, 1, 3, 3)).astype(np.float32)
bias = rng.normal(size=(2,)).astype(np.float32)
x = rng.normal(size=(1, 1, 5, 5)).astype(np.float32)
model = helper.make_model(helper.make_graph(
    [helper.make_node("Conv", ["x", "weights", "bias"], ["z"], kernel_shape=[3, 3]),
     helper.make_node("Relu", ["z"], ["y"])],
    "coexisting-inference-libraries",
    [helper.make_tensor_value_info("x", TensorProto.FLOAT, [1, 1, 5, 5])],
    [helper.make_tensor_value_info("y", TensorProto.FLOAT, [1, 2, 3, 3])],
    [numpy_helper.from_array(weights, "weights"), numpy_helper.from_array(bias, "bias")]),
    opset_imports=[helper.make_opsetid("", 13)], ir_version=8)
onnx.checker.check_model(model)
encoded = model.SerializeToString()
torch.set_num_threads(1)
expected = torch.relu(torch.nn.functional.conv2d(
    torch.from_numpy(x), torch.from_numpy(weights), torch.from_numpy(bias))).numpy()
providers = ("CPUExecutionProvider", "DnnlExecutionProvider", "XnnpackExecutionProvider")
assert set(providers).issubset(ort.get_available_providers()), ort.get_available_providers()
with tempfile.TemporaryDirectory() as directory:
    for provider in providers:
        options = ort.SessionOptions()
        options.intra_op_num_threads = 1
        options.inter_op_num_threads = 1
        options.enable_profiling = True
        options.profile_file_prefix = str(Path(directory) / provider)
        session = ort.InferenceSession(encoded, sess_options=options, providers=[provider])
        np.testing.assert_allclose(session.run(None, {"x": x})[0], expected, rtol=1e-5, atol=1e-5)
        profile = json.loads(Path(session.end_profiling()).read_text())
        executed = {event.get("args", {}).get("provider") for event in profile}
        assert provider in executed, (provider, executed)
        print("PASS numerical inference and provider execution:", provider, flush=True)
# Recheck stable libraries after every provider has loaded.
onnx.checker.check_model(onnx.load_from_string(encoded))
assert torch.tensor([2, 3]).sum().item() == 5
print("PASS coexisting versions:", onnx.__version__, torch.__version__, ort.__version__)

# Exercise the cached XNNPACK operator with changing batch sizes. Also verify
# capability rejection for layouts/bias broadcasts it cannot implement.
def check_gemm(bias_kind, transpose_a=False, transpose_b=False, k=3, dynamic_k=False):
    n = 4
    w = rng.normal(size=(k, n)).astype(np.float32)
    stored_w = w.T.copy() if transpose_b else w
    initializers = [numpy_helper.from_array(stored_w, "w")]
    inputs = ["a", "w"]
    bias_shapes = {"vector": (n,), "row": (1, n), "scalar": (), "matrix": (3, n)}
    if bias_kind == "none":
        bias = np.float32(0)
    else:
        bias = np.asarray(rng.normal(size=bias_shapes[bias_kind]), dtype=np.float32)
        initializers.append(numpy_helper.from_array(bias, "b"))
        inputs.append("b")
    input_k = "k" if dynamic_k else k
    graph = helper.make_graph(
        [helper.make_node("Gemm", inputs, ["y"], transA=int(transpose_a), transB=int(transpose_b))],
        "gemm-runtime-batches",
        [helper.make_tensor_value_info("a", TensorProto.FLOAT,
                                       [input_k, "m"] if transpose_a else ["m", input_k])],
        [helper.make_tensor_value_info("y", TensorProto.FLOAT, ["m", n])], initializers)
    gemm_model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)], ir_version=8)
    onnx.checker.check_model(gemm_model)
    with tempfile.TemporaryDirectory() as directory:
        options = ort.SessionOptions()
        options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_DISABLE_ALL
        options.intra_op_num_threads = 1
        options.enable_profiling = True
        options.profile_file_prefix = str(Path(directory) / "gemm")
        session = ort.InferenceSession(gemm_model.SerializeToString(), sess_options=options,
                                       providers=["XnnpackExecutionProvider"])
        if dynamic_k:
            try:
                session.run(None, {"a": np.zeros((2, k - 1), dtype=np.float32)})
            except Exception as error:
                assert "Gemm input channels do not match the weights" in str(error), error
            else:
                raise AssertionError("Gemm accepted mismatched input channels")
        batches = [3] if bias_kind == "matrix" else [10, 1, 0, 7, 32]
        for m in batches:
            a = rng.normal(size=(m, k)).astype(np.float32)
            feed = a.T.copy() if transpose_a else a
            actual = session.run(None, {"a": feed})[0]
            assert actual.shape == (m, n), (bias_kind, transpose_a, m, actual.shape)
            np.testing.assert_allclose(actual, a @ w + bias, rtol=1e-5, atol=1e-5)
        profile = json.loads(Path(session.end_profiling()).read_text())
        executed = {event.get("args", {}).get("provider") for event in profile}
        expected_provider = ("XnnpackExecutionProvider" if not transpose_a and k > 0
                             and bias_kind in ("vector", "row", "none") else "CPUExecutionProvider")
        assert expected_provider in executed, (bias_kind, transpose_a, transpose_b, k, executed)
    print("PASS Gemm batches, bias, transA, transB, K:", batches, bias_kind, transpose_a, transpose_b, k)

for bias_kind in ("vector", "row", "none", "scalar", "matrix"):
    check_gemm(bias_kind)
check_gemm("vector", transpose_a=True)
check_gemm("vector", transpose_b=True)
check_gemm("vector", k=0)
check_gemm("vector", dynamic_k=True)
