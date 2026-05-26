# ONNX / ML Execution Improvements - Implementation Checklist

**Status:** In progress.

**Scope:** GPU-independent ONNX model execution improvements that can be
implemented before CUDA/ROCm/device-buffer work lands. Device-specific work is
tracked separately in
[`design-pending/onnx-ml-gpu-device-buffers.md`](onnx-ml-gpu-device-buffers.md).

## Goals

- Keep CPU-only builds fully supported.
- Make tensor shape and storage placement easy to inspect from Qore code.
- Standardize common ONNX pre/postprocessing contracts used by
  DataProviderML and QoreModelRegistry.
- Improve `qore-onnx` inspection, validation, and package diagnostics.
- Expand benchmarks and validation so ONNX execution changes are measured
  against realistic Python-equivalent workflows.

## Phase Checklist

- [x] Phase 1: add GPU-compatible `ML::Tensor` shape/view and storage helper
  APIs.
- [ ] Phase 2: add standard ONNX pre/postprocessing specs and execution
  integration for common embedding, reranking, classifier, and tabular flows.
- [ ] Phase 3: improve `qore-onnx` inspect, validate, and package UX with
  actionable diagnostics and examples.
- [ ] Phase 4: unify ONNX diagnostics across `ML::OnnxModel`,
  DataProviderML, QoreModelRegistry, and CLI tools.
- [ ] Phase 5: expand registry execution plans so packaged ONNX models can
  describe runtime, tokenizer, tensor, pre/postprocessing, and output-shape
  contracts in one place.
- [ ] Phase 6: extend benchmarks for raw ONNX, tokenizer + ONNX,
  DataProviderML, and QoreModelRegistry execution paths.
- [ ] Phase 7: update Doxygen docs, module docs, examples, and release notes.
- [ ] Phase 8: run focused tests, database-backed tests, benchmark smoke tests,
  and valgrind validation before finalizing.

## Validation Notes

Use focused checks while iterating:

```bash
build/qore modules/ml/test/ml.qtest
build/qore examples/test/qlib/DataProviderML/DataProviderMLProcessors.qtest
build/qore examples/test/qlib/QoreModelRegistry/QoreModelRegistry.qtest
build/qore examples/test/qlib/QoreTokenizerUtils/QoreTokenizerUtils.qtest
```

Use PostgreSQL-backed tests when registry or DBI behavior changes:

```bash
QORE_DB_CONNSTR_PGSQL=pgsql:omquser/omquser@omquser%supah \
    build/qore examples/test/qlib/QoreModelRegistry/QoreModelRegistry.qtest
```

Use valgrind for native tensor/ONNX changes:

```bash
valgrind --quiet --leak-check=full --errors-for-leak-kinds=definite,indirect,possible \
    --error-exitcode=99 build/qore modules/ml/test/ml.qtest
```
