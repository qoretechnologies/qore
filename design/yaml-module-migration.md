# Bundling YAML with Qore

Qore 3.0 ships YAML because bundled schema providers load YAML documents during
AOT compilation, and debugger and streaming tools use YAML at runtime. Keeping
it external creates a circular source-package dependency: building the YAML
module needs Qore, while building Qore's standard library needs YAML.

The import comes from module-yaml commit
`c84d2f68eb6baf6873bdb7a930a91a817682fe5f`. The native module reports Qore's
version and links to the system LibYAML library. Its public module name, classes,
functions, and user-module names remain the same.

The twelve user modules are Connect, ConnectDataProvider, DataStreamClient,
DataStreamClientIo, DataStreamRequestHandler, DataStreamUtil,
YamlDocumentDataProvider, YamlRpcClient, YamlRpcClientIo, YamlRpcHandler,
YamlSchema, and YamlTagHandler. All 26 Qore test suites move into the standard
test runner's tree, along with the provider catalog check. The old standalone
Docker wrappers are superseded by Qore's CI harness.

CMake builds YAML before metadata extraction and AOT compilation and includes
it in build-tree and documentation module paths. Debian packages declare
libyaml-dev, ship the native and user modules in qore-stdlib, and replace older
qore-yaml-module packages. Remote debugger tools use this bundled copy.

LibYAML headers and the library are required during the top-level CMake
dependency checks. Standard library modules use `%requires yaml`, with no
missing-module branches or fallbacks. Their tests require YAML as well, so a
broken bundled module cannot turn its coverage into successful skips. REST
schema imports directly select `YAML::ParseCoreSchema`; the public
`yamlCoreSchemaSupported()` compatibility method always returns `True`.

qore-test-base and Qorus remove YAML from external module build/bootstrap
lists, retain LibYAML development packages, and resolve documentation tags from
Qore's documentation. Runtime load lists retain the name yaml. The two-pass
external-module build remains necessary for other external dependencies.

The module-yaml repository remains available for older Qore release lines;
this migration does not archive it or change those maintenance branches.
