#!/usr/bin/env python3
"""Generate a simple ONNX test model for the ml module tests.

Creates a linear model: y = 2*x + 1

Input: "X" - float tensor of shape [N, 1]
Output: "Y" - float tensor of shape [N, 1]
"""

import numpy as np

try:
    import onnx
    from onnx import helper, TensorProto, numpy_helper
except ImportError:
    print("ERROR: onnx package not installed. Install with: pip install onnx")
    raise SystemExit(1)

# Create the linear model: Y = X * 2.0 + 1.0

# Weight: 2.0 (shape [1, 1])
W = numpy_helper.from_array(np.array([[2.0]], dtype=np.float32), name="W")

# Bias: 1.0 (shape [1])
B = numpy_helper.from_array(np.array([1.0], dtype=np.float32), name="B")

# MatMul node: X @ W
matmul_node = helper.make_node("MatMul", inputs=["X", "W"], outputs=["XW"])

# Add node: XW + B
add_node = helper.make_node("Add", inputs=["XW", "B"], outputs=["Y"])

# Input: X with shape [N, 1] (N is dynamic batch dimension)
X_input = helper.make_tensor_value_info("X", TensorProto.FLOAT, [None, 1])

# Output: Y with shape [N, 1]
Y_output = helper.make_tensor_value_info("Y", TensorProto.FLOAT, [None, 1])

# Create graph
graph = helper.make_graph(
    [matmul_node, add_node],
    "test_linear",
    [X_input],
    [Y_output],
    initializer=[W, B],
)

# Create model with IR version 8 for broad onnxruntime compatibility
model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
model.ir_version = 8
model.producer_name = "qore-ml-test"
model.doc_string = "Test model: y = 2*x + 1"

# Validate
onnx.checker.check_model(model)

# Save
output_path = "test_linear.onnx"
onnx.save(model, output_path)
print(f"Saved test model to {output_path}")

# Verify with a quick test
print("Verifying model...")
import onnxruntime as ort

sess = ort.InferenceSession(output_path)
test_input = np.array([[1.0], [2.0], [3.0], [0.0], [-1.0]], dtype=np.float32)
result = sess.run(None, {"X": test_input})
expected = test_input * 2.0 + 1.0
print(f"Input: {test_input.flatten()}")
print(f"Output: {result[0].flatten()}")
print(f"Expected: {expected.flatten()}")
assert np.allclose(result[0], expected, atol=1e-6), "Model verification failed!"
print("Verification passed!")
