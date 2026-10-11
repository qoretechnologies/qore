# Copyright (C) 2026 Qore Technologies, s.r.o.
# A module in its own directory, linking an imported library target defined there, as a module of Qore links its
# system libraries (ex: Arrow::arrow_shared): the target is visible only in this directory, so the check is deferred
# to its end, as QORE_BINARY_MODULE_QORE() does
add_library(Other::dep SHARED IMPORTED)
set_target_properties(Other::dep PROPERTIES IMPORTED_LOCATION_RELEASE "${DEP_OTHER}")
add_library(mod_sub MODULE "${CMAKE_CURRENT_SOURCE_DIR}/../module.c")
cmake_language(DEFER CALL qore_check_module_ssl_consumers mod_sub)
target_link_libraries(mod_sub Other::dep)
