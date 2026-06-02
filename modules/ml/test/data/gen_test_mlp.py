#!/usr/bin/env python3
# gen_test_mlp.py - generates test_mlp.onnx, a compute-heavy MLP fixture for ONNX
# device-binding break-even benchmarks.
#
# Copyright 2026 Qore Technologies, s.r.o.
#
# The model maps input X (batch, 256) through three Gemm+Relu layers to a wide output
# Y (batch, 512). The wide output and the 256->1024->1024->512 weight matrices give
# enough FLOPs and a large enough device->host copy that GPU device-retained vs
# host-materialized output policies show a measurable break-even as batch size grows.
#
# Weights are deterministic (small, seed-fixed) so the output is reproducible.
#
# Regenerate with:  python3 gen_test_mlp.py

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper

IN_DIM = 256
H1 = 1024
H2 = 1024
OUT_DIM = 512

rng = np.random.default_rng(42)


def weight(name, rows, cols):
    # small weights keep activations well-scaled; scale by 1/sqrt(fan_in)
    w = (rng.standard_normal((rows, cols)).astype(np.float32)) / np.sqrt(rows, dtype=np.float32)
    return numpy_helper.from_array(w, name)


def bias(name, n):
    b = np.zeros((n,), dtype=np.float32)
    return numpy_helper.from_array(b, name)


initializers = [
    weight("W1", IN_DIM, H1), bias("B1", H1),
    weight("W2", H1, H2), bias("B2", H2),
    weight("W3", H2, OUT_DIM), bias("B3", OUT_DIM),
]

nodes = [
    helper.make_node("Gemm", ["X", "W1", "B1"], ["g1"], name="gemm1"),
    helper.make_node("Relu", ["g1"], ["r1"], name="relu1"),
    helper.make_node("Gemm", ["r1", "W2", "B2"], ["g2"], name="gemm2"),
    helper.make_node("Relu", ["g2"], ["r2"], name="relu2"),
    helper.make_node("Gemm", ["r2", "W3", "B3"], ["Y"], name="gemm3"),
]

graph = helper.make_graph(
    nodes,
    "test_mlp",
    [helper.make_tensor_value_info("X", TensorProto.FLOAT, ["batch", IN_DIM])],
    [helper.make_tensor_value_info("Y", TensorProto.FLOAT, ["batch", OUT_DIM])],
    initializer=initializers,
)

model = helper.make_model(graph, producer_name="qore-ml-bench",
                          opset_imports=[helper.make_operatorsetid("", 13)])
model.ir_version = 9
onnx.checker.check_model(model)
onnx.save(model, "test_mlp.onnx")
print("wrote test_mlp.onnx: X(batch,%d) -> Y(batch,%d)" % (IN_DIM, OUT_DIM))
