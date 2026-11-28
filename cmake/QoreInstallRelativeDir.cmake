# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# Sets _out_var to _dir relative to the install prefix when _dir is an absolute directory below it, else to _dir: the
# install destinations are relative, so that "cmake --install --prefix <dir>" relocates the installation (qore#7849)
include_guard(GLOBAL)

function(qore_install_relative_dir _out_var _dir)
    string(REGEX REPLACE "/+$" "" _qore_prefix "${CMAKE_INSTALL_PREFIX}")
    set(_qore_prefix "${_qore_prefix}/")
    string(LENGTH "${_qore_prefix}" _qore_prefix_len)
    string(LENGTH "${_dir}" _qore_dir_len)
    if (IS_ABSOLUTE "${_dir}" AND _qore_dir_len GREATER _qore_prefix_len)
        string(SUBSTRING "${_dir}" 0 ${_qore_prefix_len} _qore_head)
        if (_qore_head STREQUAL _qore_prefix)
            string(SUBSTRING "${_dir}" ${_qore_prefix_len} -1 _qore_rel)
            set(${_out_var} "${_qore_rel}" PARENT_SCOPE)
            return()
        endif()
    endif()
    set(${_out_var} "${_dir}" PARENT_SCOPE)
endfunction()
