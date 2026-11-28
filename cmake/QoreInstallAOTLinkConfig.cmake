# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# Install-time script: installs aot-link.conf in <libdir>/qore below the install-time prefix (CMAKE_INSTALL_PREFIX,
# which "cmake --install --prefix" sets, and DESTDIR), with its libdir entry naming the library directory of that
# prefix, so that a relocated installation links AOT output against its own library (qore#7849).
#
# Inputs (set by the install(CODE) rule before this script):
#   QORE_AOT_LINK_CONF_SOURCE  the aot-link.conf of the build tree
#   QORE_AOT_LINK_CONF_LIBDIR  the library directory, relative to the prefix (or absolute if configured so)

if (NOT EXISTS "${QORE_AOT_LINK_CONF_SOURCE}")
    message(FATAL_ERROR "the AOT link configuration is missing: ${QORE_AOT_LINK_CONF_SOURCE}")
endif()
if (IS_ABSOLUTE "${QORE_AOT_LINK_CONF_LIBDIR}")
    set(_qore_aot_conf_libdir "${QORE_AOT_LINK_CONF_LIBDIR}")
else()
    set(_qore_aot_conf_libdir "${CMAKE_INSTALL_PREFIX}/${QORE_AOT_LINK_CONF_LIBDIR}")
endif()
file(STRINGS "${QORE_AOT_LINK_CONF_SOURCE}" _qore_aot_conf_lines)
set(_qore_aot_conf "")
foreach (_qore_aot_conf_line IN LISTS _qore_aot_conf_lines)
    if (_qore_aot_conf_line MATCHES "^libdir=")
        set(_qore_aot_conf_line "libdir=${_qore_aot_conf_libdir}")
    endif()
    string(APPEND _qore_aot_conf "${_qore_aot_conf_line}\n")
endforeach()
# staged in the destination directory, which the install can write (the build tree may not be writable by the user
# who installs, nor is a temporary directory guaranteed), then installed with file(INSTALL), which applies DESTDIR,
# leaves an unchanged file alone and records the file in the install manifest; the staged copy is removed
set(_qore_aot_conf_dest "$ENV{DESTDIR}${_qore_aot_conf_libdir}/qore")
file(MAKE_DIRECTORY "${_qore_aot_conf_dest}")
set(_qore_aot_conf_staged "${_qore_aot_conf_dest}/.aot-link.conf.staged")
file(WRITE "${_qore_aot_conf_staged}" "${_qore_aot_conf}")
file(INSTALL DESTINATION "${_qore_aot_conf_libdir}/qore" TYPE FILE RENAME aot-link.conf
    FILES "${_qore_aot_conf_staged}")
file(REMOVE "${_qore_aot_conf_staged}")
