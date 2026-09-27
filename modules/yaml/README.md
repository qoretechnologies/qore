YAML is bundled with Qore starting with Qore 3.0. Its native module uses the
system LibYAML library and reports the Qore version, like the other bundled
modules.

The implementation, documentation, twelve user modules, and tests were imported
from `qoretechnologies/module-yaml` commit
`c84d2f68eb6baf6873bdb7a930a91a817682fe5f`. The standalone repository remains
available for older Qore releases.

CMake builds the native module; the standalone autotools build is superseded by Qore's build.
Native sources live here; user modules live under `qlib/`. Native test suites
are in `examples/test/modules/yaml/`, and user-module suites are under
`examples/test/qlib/`. The former Docker build wrappers are replaced by Qore's
build and test infrastructure. The catalog check remains available as
`examples/test/modules/yaml/check-i18n.sh`.

The native implementation offers LGPL-2.1-or-later or MIT licensing; see the
source notices, Qore's `COPYING.LGPL`, and this directory's `COPYING.MIT`.
