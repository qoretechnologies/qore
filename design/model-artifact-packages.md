# Model Artifact Packages

## Goal

Qore's model registry must manage model versions as deployable packages, not only
as serialized Qore ML objects. A version can contain a native Qore model, an ONNX
model, tokenizer assets, source-framework metadata, weights, or an external
runtime reference. The registry records what can be executed locally, what can be
used for input preparation, and what is stored only for provenance or conversion.

This design supports PyTorch-origin and HuggingFace-style models without adding a
Python runtime dependency or unsafe deserialization behavior to Qore.

## Safety Rules

- Qore must not execute or deserialize arbitrary PyTorch `.pt`, `.pth`, or
  pickle-based `.bin` files.
- SafeTensors files are safe to inspect as tensor metadata and weights, but are
  not executable by themselves.
- ONNX artifacts are the preferred local execution target for deep-learning
  models.
- Native `.qml` artifacts remain the preferred local execution target for Qore ML
  algorithms.
- Metadata-only and source-only artifacts must produce clear diagnostics when a
  caller asks for local inference.

## Artifact Roles

| Role | Meaning | Local Execution |
|------|---------|-----------------|
| `model` | Primary executable or source model artifact | Depends on format |
| `tokenizer` | `tokenizer.json` or equivalent tokenizer config | No |
| `tokenizer_config` | HuggingFace `tokenizer_config.json` | No |
| `generation_config` | HuggingFace `generation_config.json` | No |
| `special_tokens` | HuggingFace `special_tokens_map.json` | No |
| `weights` | SafeTensors or framework-specific weights | No |
| `config` | Framework/model architecture metadata | No |
| `manifest` | Registry package manifest | No |
| `external_runtime` | URL/provider/runtime configuration | External only |

## Artifact Formats

| Format | Treatment |
|--------|-----------|
| `qml` | Native Qore ML serialization; locally executable with `ml_deserialize()` |
| `onnx` | Locally executable when `ml` has ONNX Runtime support |
| `json` | Metadata/configuration only unless referenced by a runtime |
| `safetensors` | Safe tensor metadata/weights; not locally executable |
| `pytorch-pickle` | Opaque trusted-source artifact; never deserialized by Qore |
| `torch-export` | Source graph artifact; not locally executable until a runtime exists |
| `external-runtime` | Executed by configured remote or process runtime |

## Registry Metadata

`ModelVersionInfo` is extended with package metadata:

- `framework`: `qore`, `onnx`, `pytorch`, `huggingface`, `sklearn`, etc.
- `artifact_kind`: `executable`, `source`, `weights-only`, `tokenizer-only`,
  `metadata-only`, or `external-runtime`
- `runtime`: `qore-ml`, `onnxruntime`, `external-http`, or `metadata-only`
- `executable`: whether `ModelRegistry::loadExecutable()` can return an object
  usable for local inference
- `safety`: `safe`, `safe-metadata`, `trusted-only`, `unsafe-pickle`, or `unknown`
- `artifacts`: named artifacts in the package
- `execution`: the local or external execution plan
- `tokenizer`: tokenizer companion metadata

The existing `format`, `config`, `file_size`, and single-artifact storage fields
remain supported for backward compatibility.

## Storage Compatibility

Existing filesystem registry versions store one artifact as:

```text
<model>/v_<version>.<format>
<model>/v_<version>.meta.json
```

Package-aware versions may additionally store:

```text
<model>/v_<version>/
  manifest.json
  artifacts/
    model.onnx
    tokenizer.json
    tokenizer_config.json
```

Backends must treat the legacy single artifact as a named artifact called
`model` for native Qore models. For package registrations, the primary artifact
also keeps its package-local name (for example `model.onnx` or
`model.safetensors`) so execution plans can load the same named artifact on
filesystem, database, and REST backends. This lets old registry data keep
working while new code can address artifacts by name.

## Execution Behavior

`ModelRegistry::load()` remains backward compatible and only deserializes native
Qore ML models. New code should use an execution-plan API:

- `qml` + `qore-ml`: deserialize and call native model methods
- `onnx` + `onnxruntime`: construct `ML::OnnxModel`
- `external-runtime`: build a client/adapter from the execution config
- metadata/source/weights-only: raise an informative exception explaining the
  required conversion or runtime configuration

## Module Responsibilities

- `QoreModelRegistry`: manifests, named artifacts, artifact inspection, safety
  classification, execution-plan metadata
- `tokenizer`: HuggingFace tokenizer runtime and tokenizer metadata helpers
- `QoreTokenizerUtils`: in-memory ONNX model support for registry artifacts plus
  token counters, chunkers, embedders, and rerankers
- `ml`: ONNX local execution and tensor metadata/diagnostics
- `dataframe`: high-throughput columnar-to-tensor and tensor-to-column bridges
- `protobuf`: ONNX protobuf metadata inspection/export support without requiring
  ONNX Runtime
- `DataProviderML`: pipeline processors that consume registry execution plans
  and registry-hosted tokenizer/ONNX artifacts

## Verification Checklist

- Existing native Qore ML registry tests continue to pass unchanged.
- Legacy single-artifact registry layouts are readable.
- Named artifacts can be registered, listed, loaded, and deleted.
- Metadata-only PyTorch artifacts cannot be loaded as executable models.
- SafeTensors artifacts are classified as safe metadata/weights, not executable.
- Tokenizer assets can be loaded from a registry version.
- ONNX artifacts can be loaded from a registry version when ONNX Runtime is
  available.
- DataProviderML emits clear errors for non-executable packages.

## Implemented Processor Surfaces

- `registry-model` loads executable registry versions through
  `ModelRegistry::loadExecutable()` / `loadExecutableByTag()`.
- `tokenize` and `chunk-text` can load `tokenizer.json` from a registry package.
- `embed-text`, `rerank-text`, and `semantic-chunk-text` can load an ONNX model
  artifact and tokenizer artifact from the same registry package.
- Filesystem and database backends preserve full manifests; REST backends expose
  named artifact endpoint hooks and expect the remote service to implement the
  corresponding API.
