#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

"""Generate a small multi-input ONNX test model for DataProviderML tests.

Creates: Y = A + (B * 10)

Inputs:
  A - float tensor of shape [N, 1]
  B - float tensor of shape [N, 1]
Output:
  Y - float tensor of shape [N, 1]
"""

import numpy as np

try:
    import onnx
    from onnx import helper, TensorProto, numpy_helper
except ImportError:
    print("ERROR: onnx package not installed. Install with: pip install onnx")
    raise SystemExit(1)


scale = numpy_helper.from_array(np.array([10.0], dtype=np.float32), name="Scale")
mul_node = helper.make_node("Mul", inputs=["B", "Scale"], outputs=["BScaled"])
add_node = helper.make_node("Add", inputs=["A", "BScaled"], outputs=["Y"])

a_input = helper.make_tensor_value_info("A", TensorProto.FLOAT, [None, 1])
b_input = helper.make_tensor_value_info("B", TensorProto.FLOAT, [None, 1])
y_output = helper.make_tensor_value_info("Y", TensorProto.FLOAT, [None, 1])

graph = helper.make_graph(
    [mul_node, add_node],
    "test_multi_input",
    [a_input, b_input],
    [y_output],
    initializer=[scale],
)

model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
model.ir_version = 8
model.producer_name = "qore-ml-test"
model.doc_string = "Test multi-input model: Y = A + (B * 10)"

onnx.checker.check_model(model)

output_path = "test_multi_input.onnx"
onnx.save(model, output_path)
print(f"Saved test model to {output_path}")

try:
    import onnxruntime as ort
except ImportError:
    print("onnxruntime not installed; skipping runtime verification")
    raise SystemExit(0)

sess = ort.InferenceSession(output_path)
a = np.array([[1.0], [2.0], [3.0]], dtype=np.float32)
b = np.array([[0.5], [1.0], [1.5]], dtype=np.float32)
result = sess.run(None, {"A": a, "B": b})[0]
expected = a + (b * 10.0)
assert np.allclose(result, expected, atol=1e-6), "Model verification failed"
print("Verification passed")
