# ONNX / ML GPU Device Buffers - Design

**Status:** Proposed.

**Target:** Pending follow-up work after the completed host-side ONNX model
execution roadmap in
[`design/onnx-model-execution-roadmap.md`](../design/onnx-model-execution-roadmap.md).
The goal is to close the remaining provider-owned device memory gap without
weakening the existing CPU, host-buffer, DataFrame, Arrow, and DataProviderML
paths.

**Branch context:** This design assumes the current `feature/5164_jit` work:
generic classes, dense `buffer<T>` data, DataFrame/Arrow integration,
`ML::Tensor`, `ML::OnnxIoBinding`, `ML::OnnxRunOptions`, QoreModelRegistry ONNX
execution plans, DataProviderML columnar execution, and zero-copy host ONNX
tensor outputs.

**Related designs:**

- [`design/onnx-model-execution-roadmap.md`](../design/onnx-model-execution-roadmap.md)
- [`design/ml-architecture.md`](../design/ml-architecture.md)
- [`design/arrow-dataframe-integration.md`](../design/arrow-dataframe-integration.md)
- [`design/plugin-types-and-dense-data.md`](../design/plugin-types-and-dense-data.md)
- [`design/model-artifact-packages.md`](../design/model-artifact-packages.md)

---

## Summary

Qore now has a strong CPU/host-memory ONNX execution stack:

- ONNX Runtime provider discovery and provider option diagnostics.
- `ML::Tensor` backed by dense `buffer<T>`.
- `ML::OnnxIoBinding` and `ML::OnnxRunOptions`.
- Zero-copy `ML::Tensor::fromBuffer(..., zero_copy: True)` for compatible
  host buffers.
- Zero-copy wrapping of exact CPU ONNX outputs for `float32`, `float64`,
  `int8`, `int16`, `int32`, and `int64` tensor outputs.
- DataProviderML and QoreModelRegistry tensor paths using reusable bindings for
  stable windows.
- Core `QoreBufferNode` support for immutable external device storage, including
  a provider-neutral `QoreBufferDeviceInfo` descriptor and a copy-to-host
  callback.

The missing layer is an explicit, safe, testable contract between ONNX Runtime
execution providers and Qore's device-buffer substrate. Today the ONNX binding
code still creates CPU `Ort::MemoryInfo` values for ordinary bindings, rejects
external device buffers for preallocated output binding, and does not wrap
provider-owned ONNX output buffers as Qore device-backed buffers.

This design adds that bridge.

---

## Goals

- Let ONNX inputs already resident on a compatible device be bound without a
  host copy.
- Let ONNX Runtime allocate outputs on a requested device and return them to
  Qore as device-backed `buffer<T>` / `ML::Tensor` values.
- Allow preallocated device-backed tensor outputs when the buffer is mutable,
  compatible, and owned by a module that can safely expose a writable device
  pointer.
- Keep CPU-only builds and CPU-only runtime behavior unchanged.
- Make device/host transfer behavior explicit and observable.
- Integrate device-buffer diagnostics with DataProviderML and
  QoreModelRegistry, so mixed CPU/GPU pipelines can explain where transfers
  happen.
- Provide benchmark and validation coverage that proves GPU support helps
  realistic inference workloads instead of only adding API surface.

## Non-Goals

- Requiring CUDA, ROCm, TensorRT, or any GPU SDK for ordinary Qore builds.
- Reimplementing tensor kernels or training frameworks in Qore.
- Hiding host/device copies behind APIs named "zero-copy".
- Silently falling back to CPU when a user requested device execution.
- Making provider-specific device pointers directly usable from Qore source.
- Solving distributed multi-node inference. This design is single-process and
  single-host; multi-GPU placement is included only at the device-selection
  level.

---

## Current State

### Implemented host-side ONNX support

`modules/ml/src/OnnxModel.cpp` currently supports:

- ONNX input conversion for lists, dense `buffer<T>`, and `ML::Tensor`.
- Reusable `ML::OnnxIoBinding` instances.
- CPU output allocation through `Ort::MemoryInfo::CreateCpu(...)`.
- Preallocated Qore-owned host output tensors.
- `ML::Tensor` output conversion, including zero-copy wrapping of exact CPU
  ONNX output tensors when the requested result mode is tensor mode.

### Implemented core device-buffer substrate

`include/qore/QoreBufferNode.h` already has a provider-neutral device storage
model:

- `QoreBufferDeviceKind` with values such as `Cuda`, `Rocm`, `OpenCL`,
  `Vulkan`, `OneAPI`, `Metal`, `Java`, and `Other`.
- `QoreBufferDeviceInfo` with kind, device id, and name.
- `QoreBufferNode::wrapExternalDeviceStorage(...)` for immutable external
  device buffers.
- `QoreBufferDeviceCopyToHostCallback` for materializing device storage into
  Qore-owned host storage.
- `QoreBufferNode::hasExternalDeviceStorage()`, `getDeviceData()`,
  `getDeviceValidityData()`, and `getDeviceInfo()`.
- Buffer pseudo-methods such as `<buffer>::deviceStorage()` and
  `<buffer>::hostStorage()`.

This means the core representation does not need to be invented again. The
work is to connect ONNX Runtime device memory to the existing buffer substrate
and expose a user-facing ML contract around it.

### Current limitations

- `QoreOnnxIoBinding::bindOutput()` always binds outputs to CPU memory.
- `QoreOnnxModel::prepareOutputTensorValue()` rejects device-backed buffers for
  preallocated outputs.
- `QoreOnnxModel::prepareInputValue()` does not create ONNX tensors over
  `QoreBufferNode` external device storage.
- `convertOutputTensorToTensor()` wraps CPU external storage but not provider
  device storage.
- `ML::Tensor` does not expose device/storage metadata directly; callers must
  inspect its underlying buffer.
- DataProviderML does not yet make device placement decisions or report
  automatic host/device transfers.
- QoreModelRegistry manifests record provider requirements and diagnostics but
  do not yet describe device-buffer policies.
- CI and local validation are primarily CPU-only; this machine has no NVIDIA
  GPU or CUDA libraries installed.

---

## Design Principles

- **One buffer abstraction:** reuse `QoreBufferNode` device storage; do not add
  a second unrelated `ML::DeviceBuffer` representation unless a later use case
  proves it is necessary.
- **Explicit placement:** APIs must say whether a result is requested on CPU,
  on a provider device, or in provider default memory.
- **No implicit fallback:** if a caller requests CUDA output and CUDA is not
  usable, raise an actionable exception unless the caller explicitly allows a
  fallback.
- **Safe ownership:** every device-backed buffer returned to Qore must keep the
  provider/ORT owner alive for the lifetime of the buffer.
- **Truthful zero-copy:** a path is called zero-copy only when no host copy is
  performed.
- **Materialization is visible:** host reads can materialize device storage, but
  diagnostics and benchmarks must make that cost measurable.
- **CPU compatibility:** all new APIs must parse and run on CPU-only builds,
  returning clear unsupported-device errors only when device execution is
  requested.

---

## Proposed Public API Additions

### Buffer metadata

The existing buffer pseudo-methods should be extended enough for users and
diagnostics to understand device placement.

Candidate additions:

```qore
hash<auto> <buffer>::deviceInfo()
bool <buffer>::materialize()
```

`deviceInfo()` returns `NOTHING` for host-only buffers. For device-backed
buffers it returns a typed, stable hash such as:

```qore
{
    "kind": "cuda",
    "device_id": 0,
    "name": "CUDAExecutionProvider:0",
    "host_storage": False,
    "device_storage": True,
}
```

`materialize()` can be added only if the current `<buffer>` API does not
already expose the materialization operation cleanly. It should force the
copy-to-host callback and return `True` if a transfer was performed, `False`
if host storage was already available.

### `ML::Tensor` metadata

Add tensor-level convenience methods over the backing buffer:

```qore
bool ML::Tensor::deviceStorage()
bool ML::Tensor::hostStorage()
*hash<auto> ML::Tensor::deviceInfo()
ML::Tensor ML::Tensor::materialize()
```

`materialize()` returns `self` when host storage is already available and
returns a Tensor sharing the now-materialized backing buffer otherwise. It must
not mutate tensor shape or dtype.

### ONNX session config

Extend `OnnxSessionConfig` with device-buffer policy fields:

```qore
hash<ML::OnnxSessionConfig> cfg = {
    "providers": (
        <ML::OnnxProviderConfig>{
            "name": "CUDA",
            "options": {"device_id": "0"},
        },
    ),
    "device_binding": {
        "enabled": True,
        "default_output_device": "provider",
        "allow_host_fallback": False,
        "materialize_outputs": False,
    },
};
```

Proposed fields:

| Field | Meaning |
|-------|---------|
| `device_binding.enabled` | Enables device-aware input/output binding. |
| `device_binding.default_output_device` | `"cpu"`, `"provider"`, or explicit provider/device name. |
| `device_binding.allow_host_fallback` | Allows host copies when a device path is unavailable. Defaults to `False` for requested device paths. |
| `device_binding.materialize_outputs` | Forces returned device outputs to host before returning. Defaults to `False`. |
| `device_binding.require_zero_copy_inputs` | Rejects device inputs that cannot be bound directly. |
| `device_binding.require_zero_copy_outputs` | Rejects outputs that cannot be returned as device buffers. |

### ONNX I/O binding methods

Extend `ML::OnnxIoBinding`:

```qore
bindOutputDevice(string name, *hash<auto> device = NOTHING)
bindOutputsDevice(*hash<auto> device = NOTHING)
bindOutputTensor(string name, ML::Tensor tensor, *hash<auto> options = NOTHING)
hash<auto> getDeviceBindingDiagnostics()
```

`bindOutputDevice()` requests ONNX Runtime-managed output allocation on the
given device. `device = NOTHING` means "the active non-CPU provider selected by
the session, or CPU if no device provider is active and host fallback is
allowed".

`bindOutputTensor()` should continue to support host preallocated outputs. It
should also support writable device-backed preallocated outputs once a module
can safely expose such buffers.

---

## ONNX Runtime Mapping

### MemoryInfo creation

The bridge must map `QoreBufferDeviceInfo` to `Ort::MemoryInfo` and back.

For current ONNX Runtime headers, relevant APIs include:

- `Ort::MemoryInfo::CreateCpu(...)`
- `Ort::MemoryInfo(name, allocator_type, device_id, mem_type)`
- `Ort::MemoryInfo(name, device_type, vendor_id, device_id, mem_type,
  alignment, allocator_type)` for newer `CreateMemoryInfo_V2` builds
- `Ort::IoBinding::BindInput(name, value)`
- `Ort::IoBinding::BindOutput(name, value)`
- `Ort::IoBinding::BindOutput(name, memory_info)`
- `Ort::IoBinding::SynchronizeInputs()`
- `Ort::IoBinding::SynchronizeOutputs()`
- `Ort::Value::CreateTensor(memory_info, data, count, shape, type)`
- `OrtApi::GetTensorMemoryInfo(...)`

The implementation should detect at compile time which ONNX Runtime API
features are available and use the most specific API. If V2 memory descriptors
are unavailable, use the older name/type/id/memtype form.

### Provider names

Normalize provider names through the same logic already used for provider
configuration:

| Qore device kind | ONNX provider | MemoryInfo name candidates |
|------------------|---------------|----------------------------|
| `Cuda` | `CUDAExecutionProvider` | `Cuda`, `CUDA` |
| `Rocm` | `ROCMExecutionProvider` | `Rocm`, `ROCM` |
| `OneAPI` | `DnnlExecutionProvider` or provider-specific EP | provider-specific |
| `Metal` | `CoreMLExecutionProvider` where applicable | usually not raw user device memory |
| `Java` | JNI/JDBC/native packed buffers | not an ONNX EP unless bridged explicitly |
| `Other` | provider-specific | provider-specific |

The first implementation should target CUDA because ONNX Runtime exposes clear
CUDA provider configuration and the current provider metadata already supports
CUDA options. ROCm and TensorRT should be designed into the normalization table
but can be enabled after CUDA proves the abstraction.

### Input binding

When an input is a `buffer<T>` or `ML::Tensor` backed by external device
storage:

1. Validate dtype and shape exactly as the host path does.
2. Validate that the device kind and device id are compatible with the active
   provider and session provider options.
3. Create an `Ort::Value` over the device pointer using device `MemoryInfo`.
4. Keep the Qore buffer/tensor owner alive for the duration of the bound value.
5. Bind it with `Ort::IoBinding::BindInput()`.
6. Call `SynchronizeInputs()` when required by the provider.

If device compatibility fails:

- Raise `ML-ONNX-DEVICE-BINDING-ERROR` by default.
- If `allow_host_fallback` is true, materialize the buffer to host and use the
  existing CPU path while recording a diagnostic transfer.

### Output binding

For ONNX Runtime-managed device outputs:

1. Create an output `Ort::MemoryInfo` for the selected device.
2. Bind output with `Ort::IoBinding::BindOutput(name, memory_info)`.
3. Run with binding.
4. Call `SynchronizeOutputs()` when required by the provider.
5. Inspect each returned `Ort::Value` memory info.
6. If it is CPU memory, use the existing CPU wrapping/copy path.
7. If it is device memory, wrap the `Ort::Value` in a shared owner and create a
   `QoreBufferNode::wrapExternalDeviceStorage(...)` buffer with a copy-to-host
   callback.
8. Return an `ML::Tensor` around the device-backed buffer in tensor mode.

The copy-to-host callback can either:

- Use ONNX Runtime allocator/copy APIs if available for the provider, or
- Use provider-specific copy APIs compiled conditionally, starting with CUDA.

If no safe copy callback is available, the device buffer must not be returned
as a normal Qore buffer unless the API explicitly marks it as non-materializable
and all host reads throw a clear exception. The preferred v1 behavior is to
require a copy callback.

### Preallocated device outputs

Preallocated output tensors are more sensitive because ONNX writes into caller
storage. V1 should support them only when all of these are true:

- The buffer is device-backed and explicitly marked writable by its native
  owner.
- The pointer is non-null and device-compatible.
- The buffer length, dtype, shape, nullability, and device id match the output
  contract.
- The provider supports writing to caller-owned device memory.

The current immutable external device buffer API is not enough by itself for
preallocated output writes. If no writable device-buffer marker exists, keep
rejecting preallocated device outputs and document that users should use
`bindOutputDevice()` for provider-managed output allocation.

---

## DataProviderML and Registry Integration

### DataProviderML

Add device policy options to ONNX-capable processors:

- `device_binding`
- `output_device`
- `materialize_outputs`
- `allow_host_fallback`
- `require_zero_copy`

Processor diagnostics should include:

- active provider and device id
- input path: host list, host tensor, host dense column, device tensor
- output path: host tensor, device tensor, row/hash materialized
- number of host-to-device transfers
- number of device-to-host transfers
- whether a fallback occurred

Mixed pipelines must be explicit:

- Device-aware processor followed by device-aware processor: pass device
  buffers where dtype/shape match.
- Device-aware processor followed by CPU-only processor: materialize once,
  record a transfer diagnostic, and continue.
- CPU-only processor followed by device-aware processor: transfer/bind as host
  input unless a device upload API exists.

### QoreModelRegistry

Extend ONNX execution metadata with device policy:

```json
{
  "execution": {
    "providers": ["CUDAExecutionProvider"],
    "device_binding": {
      "enabled": true,
      "default_output_device": "provider",
      "allow_host_fallback": false,
      "materialize_outputs": false
    }
  }
}
```

Registry validation should check:

- provider availability
- provider loadability
- requested device id
- device policy consistency
- optimized/quantized artifact provider compatibility

Registry execution plans should propagate the device policy into
`ModelExecutableAdapter` and DataProviderML processors.

---

## Build and Detection Strategy

### Qore core and ml module

The ordinary build must not require GPU SDKs.

Required build behavior:

- Build ONNX CPU support exactly as today when ONNX Runtime is present.
- Detect ONNX Runtime device APIs from headers and define internal feature
  macros such as `HAVE_ORT_MEMORYINFO_V2` if needed.
- Detect CUDA headers/libraries only for optional CUDA copy callback support.
- Compile CUDA-specific code only when both ONNX Runtime CUDA provider support
  and CUDA runtime headers/libraries are available.
- Keep provider configuration support runtime-driven; a CPU-only build may
  still parse CUDA provider options and report that CUDA is unavailable.

### Module compatibility

External modules that create device-backed `buffer<T>` values can participate
without linking to ONNX Runtime if they fill `QoreBufferDeviceInfo` correctly
and provide a copy-to-host callback.

Examples:

- future CUDA/ROCm helper module
- JNI/JDBC packed buffers where device kind is `Java` or `Other`
- Arrow GPU buffers if introduced later

---

## Implementation Plan

### Phase 0: Baseline and API audit

- Inventory all current buffer pseudo-methods and `QoreBufferNode` device APIs.
- Inventory ONNX Runtime memory APIs available in the installed headers.
- Add a small internal helper to stringify `QoreBufferDeviceInfo`.
- Decide whether `<buffer>::materialize()` already exists under another name;
  add only missing user-facing methods.
- Add docs for existing device-buffer pseudo-methods if incomplete.

Acceptance:

- CPU-only build and tests unchanged.
- A small Qore test can identify host vs device-capable buffers even if no
  real device buffer can be created locally.

### Phase 1: Device metadata public surface

- Add `<buffer>::deviceInfo()` if missing.
- Add `ML::Tensor::deviceStorage()`, `hostStorage()`, and `deviceInfo()`.
- Add `ML::Tensor::materialize()` if needed.
- Add typed docs and examples.

Acceptance:

- `modules/ml/test/ml.qtest` covers host tensor metadata.
- Device tests are written with a native test-only device-buffer wrapper if
  feasible; otherwise they are gated behind a module/capability check.

### Phase 2: ONNX device memory helpers

- Add helpers to map provider config and `QoreBufferDeviceInfo` to
  `Ort::MemoryInfo`.
- Add helpers to inspect `Ort::Value` memory info and convert it to
  `QoreBufferDeviceInfo`.
- Add structured device-binding diagnostics to `QoreOnnxModel`.
- Add compile-time guards for ONNX Runtime memory API differences.

Acceptance:

- CPU memory info continues to match existing behavior.
- Invalid provider/device combinations raise
  `ML-ONNX-DEVICE-BINDING-ERROR` with provider, requested device, active
  provider, and fallback-policy details.

### Phase 3: Device input binding

- Extend `prepareInputValue()` to detect external device-backed buffers.
- Validate device compatibility.
- Create ONNX tensor values over device data.
- Preserve input owner lifetime in `OnnxBoundOrtValue`.
- Add fallback/materialization path when explicitly allowed.

Acceptance:

- CPU tests prove no regression.
- Device tests prove no host copy for compatible device input.
- Incompatible device input raises an actionable error.

### Phase 4: Provider-managed device outputs

- Add `bindOutputDevice()` and `bindOutputsDevice()`.
- Bind outputs to provider memory through `Ort::MemoryInfo`.
- Wrap device output `Ort::Value` objects as device-backed `QoreBufferNode`
  storage.
- Implement the copy-to-host callback for the first supported provider.
- Expose diagnostics for output placement and materialization.

Acceptance:

- Tensor-mode outputs can remain device-backed.
- Calling `toList()` or otherwise reading host data materializes once and then
  uses host storage.
- The owner lifetime test proves returned buffers remain valid after the ONNX
  run and binding object go out of scope.

### Phase 5: DataProviderML and QoreModelRegistry policies

- Add device-binding options to ONNX-capable DataProviderML processors.
- Add device-binding metadata to QoreModelRegistry execution plans.
- Propagate policy into `ModelExecutableAdapter`.
- Add transfer diagnostics to processor `getDiagnostics()`.

Acceptance:

- Registry-backed ONNX execution honors provider/device policy.
- Mixed CPU/GPU pipeline tests show truthful transfer diagnostics.
- CPU-only execution remains the default and remains simple.

### Phase 6: Benchmarks

Add or extend benchmark cases:

- `bench_onnx_run_device_tensor`
- `bench_onnx_run_device_binding`
- `bench_dataproviderml_onnx_device_dataframe_tensor`
- `bench_dataproviderml_registry_onnx_device_dataframe_tensor`

Benchmark dimensions:

- batch sizes: 1, 8, 32, 128, 512
- input path: list, host tensor, host binding, device tensor, device binding
- output policy: host materialized, device retained
- provider: CPU, CUDA when available

Acceptance:

- Host paths do not regress beyond noise.
- GPU/device paths show benefit for workloads large enough to amortize transfer
  overhead.
- Benchmarks report transfer counts and whether outputs were materialized.

### Phase 7: Validation and tooling

- CPU-only CI: build, tests, docs, and explicit unsupported-device tests.
- GPU manual/CI runner: CUDA provider load, device input, device output,
  DataProviderML, registry, and benchmarks.
- Memory tools:
  - valgrind for CPU/host paths
  - AddressSanitizer where available
  - CUDA compute-sanitizer / cuda-memcheck for CUDA paths
- Stress tests for repeated runs, binding reuse, async session pools, and
  dynamic batching.

Acceptance:

- No invalid reads/writes or leaks on host paths.
- No stale device pointer use after bindings, tensors, or sessions are
  destroyed.
- Session-pool device binding is deterministic under concurrent use.

### Phase 8: Documentation

- Update `doxygen/lang/*` ONNX deployment docs.
- Update `ML::Tensor` and `ML::OnnxIoBinding` API docs.
- Add DataProviderML GPU/device execution examples.
- Add QoreModelRegistry manifest examples.
- Update release notes.
- Document limitations clearly:
  - CPU-only builds.
  - Provider-specific support matrix.
  - When host materialization occurs.
  - Which dtypes can remain device-backed.
  - Which providers support caller-preallocated device outputs.

Acceptance:

- User docs include complete examples for CPU-only, CUDA requested, device
  output retained, and device output materialized.
- Docs do not imply CUDA is required for Qore.

---

## Error Handling

Use specific errors:

- `ML-ONNX-DEVICE-BINDING-ERROR`
- `ML-ONNX-DEVICE-UNAVAILABLE`
- `ML-ONNX-DEVICE-MISMATCH`
- `ML-ONNX-DEVICE-MATERIALIZATION-ERROR`
- `ML-TENSOR-DEVICE-ERROR`

Error messages must include:

- input/output tensor name when applicable
- requested provider and device id
- active provider and device id
- dtype and shape
- whether fallback was allowed
- suggested fix, for example `materialize_outputs: True` or use CPU output
  binding

---

## Security and Sandboxing

Device APIs expose external process/device resources and must respect sandbox
policy.

Open questions for implementation:

- Should device execution require a new functionality domain, or reuse an
  existing external/uncontrolled API restriction?
- Should provider-specific modules be blocked under `PO_NO_UNCONTROLLED_APIS`?
- How should diagnostics expose device names without leaking sensitive host
  topology in restricted programs?

V1 should be conservative: constructing or using device-backed buffers from
external runtimes should be treated as an uncontrolled external API unless the
existing sandbox model already covers the module.

---

## Open Questions

- Do we need public `ML::Device` / `ML::DeviceBuffer` classes, or are
  `buffer<T>` device metadata and `ML::Tensor` helpers sufficient?
- Which provider should be first after CUDA: ROCm, TensorRT, OpenVINO, or
  DirectML?
- Can ONNX Runtime copy device output to host portably enough, or do we need
  provider-specific callbacks for each supported provider?
- How do provider-managed allocators interact with Qore's long-lived buffers in
  services with many concurrent sessions?
- Should device output buffers be immutable only in v1?
- What is the policy for nullable device buffers, since ONNX tensors do not
  carry Qore/Arrow validity bitmaps?
- Do we need pinned host buffers as a separate placement (`host_accessible`) for
  fast transfer?

---

## Completion Checklist

- [ ] Public buffer/tensor device metadata methods are complete and documented.
- [ ] ONNX provider/device to `Ort::MemoryInfo` mapping is implemented.
- [ ] Device-backed input tensors bind without host copy when compatible.
- [ ] Provider-managed device output allocation is implemented.
- [ ] Device ONNX outputs return as device-backed `ML::Tensor` values.
- [ ] Host materialization is explicit, tested, and diagnosed.
- [ ] DataProviderML propagates device policy and reports transfer diagnostics.
- [ ] QoreModelRegistry manifests can express device policy.
- [ ] CPU-only builds and tests remain green.
- [ ] GPU validation passes on at least one CUDA-capable runner.
- [ ] Benchmarks show the break-even points and real speedups.
- [ ] Valgrind/ASan host validation and CUDA memory validation are recorded.
- [ ] Doxygen docs and release notes are updated.
