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

"""Generate small token-input ONNX models for QoreTokenizerUtils tests.

The embedding fixture exposes standard HuggingFace-style input_ids and
attention_mask inputs but intentionally omits token_type_ids.  This verifies
that QoreTokenizerUtils sends only tensors declared by the model.

The reranker fixture exposes input_ids, attention_mask, and token_type_ids and
returns one logits value per input row.
"""

import numpy as np

try:
    import onnx
    from onnx import TensorProto, helper, numpy_helper
except ImportError:
    print("ERROR: onnx package not installed. Install with: pip install onnx")
    raise SystemExit(1)


def save_model(model, path):
    model.ir_version = 8
    model.producer_name = "qore-tokenizer-utils-test"
    onnx.checker.check_model(model)
    onnx.save(model, path)
    print(f"Saved {path}")


def make_embedding_model():
    axes_2 = numpy_helper.from_array(np.array([2], dtype=np.int64), name="axes_2")

    nodes = [
        helper.make_node("Cast", ["input_ids"], ["ids_float"], to=TensorProto.FLOAT),
        helper.make_node("Unsqueeze", ["ids_float", "axes_2"], ["ids_3d"]),
        helper.make_node("Cast", ["attention_mask"], ["mask_float"], to=TensorProto.FLOAT),
        helper.make_node("Unsqueeze", ["mask_float", "axes_2"], ["mask_3d"]),
        helper.make_node("Concat", ["ids_3d", "mask_3d"], ["last_hidden_state"], axis=2),
    ]

    graph = helper.make_graph(
        nodes,
        "test_token_embedding",
        [
            helper.make_tensor_value_info("input_ids", TensorProto.INT64, [None, None]),
            helper.make_tensor_value_info("attention_mask", TensorProto.INT64, [None, None]),
        ],
        [
            helper.make_tensor_value_info("last_hidden_state", TensorProto.FLOAT, [None, None, 2]),
        ],
        initializer=[axes_2],
    )
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
    model.doc_string = "Test token embedding model with no token_type_ids input"
    return model


def make_reranker_model():
    axes_1 = numpy_helper.from_array(np.array([1], dtype=np.int64), name="axes_1")
    scale = numpy_helper.from_array(np.array([0.001], dtype=np.float32), name="scale")
    zero = numpy_helper.from_array(np.array([0.0], dtype=np.float32), name="zero")

    nodes = [
        helper.make_node("Cast", ["input_ids"], ["ids_float"], to=TensorProto.FLOAT),
        helper.make_node("Cast", ["attention_mask"], ["mask_float"], to=TensorProto.FLOAT),
        helper.make_node("Mul", ["ids_float", "mask_float"], ["masked_ids"]),
        helper.make_node("Cast", ["token_type_ids"], ["types_float"], to=TensorProto.FLOAT),
        helper.make_node("Mul", ["types_float", "zero"], ["types_zero"]),
        helper.make_node("Add", ["masked_ids", "types_zero"], ["combined_ids"]),
        helper.make_node("ReduceSum", ["combined_ids", "axes_1"], ["summed"], keepdims=1),
        helper.make_node("Mul", ["summed", "scale"], ["logits"]),
    ]

    graph = helper.make_graph(
        nodes,
        "test_token_reranker",
        [
            helper.make_tensor_value_info("input_ids", TensorProto.INT64, [None, None]),
            helper.make_tensor_value_info("attention_mask", TensorProto.INT64, [None, None]),
            helper.make_tensor_value_info("token_type_ids", TensorProto.INT64, [None, None]),
        ],
        [
            helper.make_tensor_value_info("logits", TensorProto.FLOAT, [None, 1]),
        ],
        initializer=[axes_1, scale, zero],
    )
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
    model.doc_string = "Test token reranker model with token_type_ids input"
    return model


def verify_models():
    try:
        import onnxruntime as ort
    except ImportError:
        print("onnxruntime not installed; skipping runtime verification")
        return

    ids = np.array([[101, 7592, 2088, 102], [101, 3698, 102, 0]], dtype=np.int64)
    mask = np.array([[1, 1, 1, 1], [1, 1, 1, 0]], dtype=np.int64)
    types = np.zeros_like(ids, dtype=np.int64)

    emb = ort.InferenceSession("test_token_embedding.onnx")
    hidden = emb.run(None, {"input_ids": ids, "attention_mask": mask})[0]
    assert hidden.shape == (2, 4, 2)
    assert np.allclose(hidden[:, :, 0], ids.astype(np.float32))
    assert np.allclose(hidden[:, :, 1], mask.astype(np.float32))

    rerank = ort.InferenceSession("test_token_reranker.onnx")
    logits = rerank.run(None, {
        "input_ids": ids,
        "attention_mask": mask,
        "token_type_ids": types,
    })[0]
    expected = (ids * mask).sum(axis=1, keepdims=True).astype(np.float32) * 0.001
    assert np.allclose(logits, expected, atol=1e-6)
    print("Verification passed")


def main():
    save_model(make_embedding_model(), "test_token_embedding.onnx")
    save_model(make_reranker_model(), "test_token_reranker.onnx")
    verify_models()


if __name__ == "__main__":
    main()
