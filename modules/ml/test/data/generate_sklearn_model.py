#!/usr/bin/env python3
"""Generate a scikit-learn RandomForestClassifier ONNX model for ml module tests.

This creates a real ML model exported via skl2onnx, exercising:
- Multi-output tensors (class labels + probabilities)
- int64 output tensors (predicted class)
- float output tensors (probability matrix)
- Dynamic batch dimensions
- A model trained on the classic Iris dataset

Input: "X" - float tensor of shape [N, 4] (sepal/petal length/width)
Outputs:
  - "output_label" - int64 tensor of shape [N] (predicted class: 0, 1, or 2)
  - "output_probability" - float tensor of shape [N, 3] (class probabilities)

Note: zipmap=False is used to produce standard tensor outputs instead of
sequence-of-map outputs, which is the recommended practice for ONNX interop.
"""

import numpy as np

try:
    from sklearn.datasets import load_iris
    from sklearn.ensemble import RandomForestClassifier
    from sklearn.model_selection import train_test_split
except ImportError:
    print("ERROR: scikit-learn not installed. Install with: pip install scikit-learn")
    raise SystemExit(1)

try:
    from skl2onnx import convert_sklearn
    from skl2onnx.common.data_types import FloatTensorType
except ImportError:
    print("ERROR: skl2onnx not installed. Install with: pip install skl2onnx")
    raise SystemExit(1)

try:
    import onnxruntime as ort
except ImportError:
    print("ERROR: onnxruntime not installed. Install with: pip install onnxruntime")
    raise SystemExit(1)

# Load the Iris dataset
iris = load_iris()
X, y = iris.data.astype(np.float32), iris.target

# Train a small RandomForest (keep it small for fast test loading)
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
clf = RandomForestClassifier(n_estimators=10, max_depth=3, random_state=42)
clf.fit(X_train, y_train)

accuracy = clf.score(X_test, y_test)
print(f"Model accuracy: {accuracy:.2%}")

# Convert to ONNX with zipmap=False for standard tensor probability output
initial_type = [("X", FloatTensorType([None, 4]))]
onnx_model = convert_sklearn(
    clf,
    initial_types=initial_type,
    target_opset=13,
    options={id(clf): {"zipmap": False}},
)

# Set metadata
onnx_model.producer_name = "skl2onnx"
onnx_model.doc_string = "RandomForestClassifier trained on Iris dataset (3 classes, 4 features)"
onnx_model.ir_version = 8

# Save
output_path = "test_sklearn_rf.onnx"
import onnx
onnx.save(onnx_model, output_path)
print(f"Saved sklearn model to {output_path}")

# Verify with onnxruntime
print("Verifying model...")
sess = ort.InferenceSession(output_path)

# Print input/output info
print("\nInputs:")
for inp in sess.get_inputs():
    print(f"  {inp.name}: {inp.type} {inp.shape}")
print("Outputs:")
for out in sess.get_outputs():
    print(f"  {out.name}: {out.type} {out.shape}")

# Run inference on test samples
test_samples = X_test[:5]
results = sess.run(None, {"X": test_samples})
labels = results[0]
probabilities = results[1]
print(f"\nTest predictions: {labels}")
print(f"Actual labels:    {y_test[:5]}")
print(f"Probabilities shape: {probabilities.shape}")
print(f"Probabilities[0]:    {probabilities[0]}")

# Verify labels are valid class indices
assert all(l in [0, 1, 2] for l in labels), f"Invalid labels: {labels}"
# Verify probabilities sum to ~1.0
for i, p in enumerate(probabilities):
    assert abs(sum(p) - 1.0) < 1e-5, f"Probabilities for sample {i} don't sum to 1: {p}"
print("Verification passed!")
