# ONNX / ML GPU Device Buffers - Pending Follow-Ups

**Status:** Pending provider and allocator follow-up work only.

The core ONNX device-buffer architecture has been implemented and is documented
in [`design/ml-architecture.md`](../design/ml-architecture.md). This file keeps
only the remaining work that is not implemented or not validated on the current
development hardware.

## Implemented Baseline

The following work is complete and should not be duplicated here:

- Public buffer and `ML::Tensor` storage metadata APIs.
- CUDA `ML::Tensor::toDevice()` upload and provider-managed ONNX device
  outputs.
- `ML::Tensor::pinnedHost()` for CUDA upload sources.
- Metal/UMA `ML::Tensor::toDevice("metal")` on Apple Silicon.
- CoreML EP acceleration with truthful host resolution diagnostics.
- `ML::OnnxIoBinding::bindOutputDevice()` and `bindOutputsDevice()`.
- `OnnxSessionConfig.device_binding` parsing and inference counters.
- DataProviderML and QoreModelRegistry propagation of `device_binding` policy.
- ONNX session-pool transfer counters and CUDA memory snapshots.
- Device-binding sandbox domain coverage with `UNCONTROLLED_API`.
- Break-even benchmarks in `bench/onnx_device_breakeven.qr` and
  `bench/baselines/onnx_device_breakeven.md`.

## Pending Work

### ROCm / HIP Device Binding

CUDA and the provider-neutral device-binding substrate are implemented, but the
HIP runtime path is not:

- Add `HAVE_HIPRT` CMake detection.
- Add HIP upload allocation and copy-to-host callbacks using `hipMemcpy`.
- Generalize ONNX device-output wrapping so ROCm dispatches to the HIP copy
  callback instead of the CUDA-only callback.
- Add negative-path tests without AMD hardware and full validation on an AMD
  GPU with a ROCm-enabled ONNX Runtime build.

### Provider Reach Validation

Provider metadata and normalization exist for TensorRT, OpenVINO, DirectML,
ROCm, and CoreML. The following provider-specific validation remains pending:

- Validate TensorRT execution and benchmarks on a host with TensorRT libraries.
- Validate OpenVINO EP load, inference, diagnostics, and host-resolution
  behavior with an ONNX Runtime build that includes OpenVINO.
- Validate DirectML EP behavior on Windows, including WARP software-adapter
  smoke tests.

OpenVINO and DirectML expose opaque provider memory rather than raw pointers for
Qore to wrap directly. Their near-term target is EP acceleration with truthful
host resolution diagnostics; any zero-copy remote-tensor or D3D12 interop path
is a separate design.

### Pinned Materialize Destination

`Tensor::pinnedHost()` is implemented for CUDA upload sources. Device-output
materialization still allocates ordinary Qore-owned host storage because the
core buffer materialization path lives in `libqore`, which does not link CUDA.

Future work would require a core hook that lets a module supply the host
allocator for materialization destinations, followed by optional
`cudaMemcpyAsync` and stream-synchronization support.

### Writable Preallocated Device Outputs

Provider-managed device outputs are implemented through `bindOutputDevice()` and
`bindOutputsDevice()`. Caller-preallocated device output tensors remain rejected
because the current external device-buffer contract is immutable. Revisit only
if ring-buffer or double-buffer reuse becomes a measured requirement.
