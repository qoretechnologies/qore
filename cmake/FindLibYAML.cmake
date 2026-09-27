# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# LibYAML is a required system dependency of the bundled yaml module.
find_path(LIBYAML_INCLUDE_DIR NAMES yaml.h)
find_library(LIBYAML_LIBRARY NAMES yaml libyaml)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibYAML REQUIRED_VARS LIBYAML_INCLUDE_DIR LIBYAML_LIBRARY)
mark_as_advanced(LIBYAML_INCLUDE_DIR LIBYAML_LIBRARY)
