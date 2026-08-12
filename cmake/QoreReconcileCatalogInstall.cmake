# Copyright (c) 2026 Qore Technologies, s.r.o.
#
# Install-time reconciliation of one project's installed i18n catalog fragments.
#
# Included by the install(CODE) rules emitted from QORE_FINALIZE_CATALOG_INSTALL()
# in QoreMacros.cmake.  See design/catalog-install-reconciliation.md for the
# ownership model this implements and why reconciliation is scoped per project.
#
# Catalog installation is otherwise purely additive: a fragment installed by an
# earlier source generation survives forever, and I18n::discover_catalog_files()
# then discovers both it and the current one -- correctly refusing to compose two
# conflicting definitions of the same message id.  This script removes exactly
# the fragments this project installed before and no longer installs, so an
# upgrade leaves the catalog root holding only what the installed generation
# owns.
#
# Inputs (set by the emitted install(CODE) block):
#   QORE_CATALOG_ROOT          catalog root to reconcile, with $ENV{DESTDIR} already applied
#   QORE_CATALOG_MANIFEST      absolute path of this project's manifest below the root
#   QORE_CATALOG_NEW_MANIFEST  build-tree manifest listing this generation's fragments
#   QORE_CATALOG_OWNERS        fragment file names this project owns (<Module>.json)
#   QORE_CATALOG_SWEEP_FLAT    TRUE to run the one-time obsolete flat-layout sweep
#

if (NOT DEFINED QORE_CATALOG_ROOT OR "${QORE_CATALOG_ROOT}" STREQUAL "")
    message(FATAL_ERROR "QoreReconcileCatalogInstall.cmake requires QORE_CATALOG_ROOT")
endif()
if (NOT DEFINED QORE_CATALOG_MANIFEST OR "${QORE_CATALOG_MANIFEST}" STREQUAL "")
    message(FATAL_ERROR "QoreReconcileCatalogInstall.cmake requires QORE_CATALOG_MANIFEST")
endif()
if (NOT DEFINED QORE_CATALOG_NEW_MANIFEST OR NOT EXISTS "${QORE_CATALOG_NEW_MANIFEST}")
    message(FATAL_ERROR
        "the generated catalog manifest is missing: ${QORE_CATALOG_NEW_MANIFEST}")
endif()

# Reads the fragment paths of a manifest, skipping the header comments.
#
# A manifest is machine-written, but it lives in the installed tree where
# anything could have edited it, and every entry names a file this script is
# about to delete -- so entries that could escape the catalog root are refused
# rather than trusted.
if (NOT COMMAND _qore_read_catalog_manifest)
    function(_qore_read_catalog_manifest _out_var _path)
        set(_entries "")
        if (EXISTS "${_path}")
            file(STRINGS "${_path}" _lines)
            foreach (_line IN LISTS _lines)
                string(STRIP "${_line}" _line)
                if ("${_line}" STREQUAL "" OR _line MATCHES "^#")
                    continue()
                endif()
                if (IS_ABSOLUTE "${_line}" OR _line MATCHES "(^|/)\\.\\.(/|$)")
                    message(WARNING
                        "ignoring catalog manifest entry outside the catalog root: ${_line}")
                    continue()
                endif()
                list(APPEND _entries "${_line}")
            endforeach()
        endif()
        set(${_out_var} "${_entries}" PARENT_SCOPE)
    endfunction()
endif()

_qore_read_catalog_manifest(_qore_catalog_new "${QORE_CATALOG_NEW_MANIFEST}")
_qore_read_catalog_manifest(_qore_catalog_old "${QORE_CATALOG_MANIFEST}")

# The bootstrap generation is the one that finds no manifest of its own: before
# this mechanism existed no project recorded what it installed, so the very
# first reconciliation has no previous generation to diff against.
set(_qore_catalog_bootstrap FALSE)
if (NOT EXISTS "${QORE_CATALOG_MANIFEST}")
    set(_qore_catalog_bootstrap TRUE)
endif()

set(_qore_catalog_removed "")
set(_qore_catalog_prune_dirs "")

# Remove what this project installed before and no longer installs.  Fragments
# owned by any other project are not in this manifest and are never touched.
foreach (_qore_catalog_rel IN LISTS _qore_catalog_old)
    list(FIND _qore_catalog_new "${_qore_catalog_rel}" _qore_catalog_index)
    if (NOT _qore_catalog_index EQUAL -1)
        continue()
    endif()
    set(_qore_catalog_path "${QORE_CATALOG_ROOT}/${_qore_catalog_rel}")
    get_filename_component(_qore_catalog_dir "${_qore_catalog_path}" DIRECTORY)
    list(APPEND _qore_catalog_prune_dirs "${_qore_catalog_dir}")
    if (IS_DIRECTORY "${_qore_catalog_path}")
        message(WARNING
            "catalog manifest entry is a directory and was left in place: ${_qore_catalog_path}")
        continue()
    endif()
    if (EXISTS "${_qore_catalog_path}" OR IS_SYMLINK "${_qore_catalog_path}")
        file(REMOVE "${_qore_catalog_path}")
        list(APPEND _qore_catalog_removed "${_qore_catalog_path}")
    endif()
endforeach()

# Retire fragments this project owns by name but no longer installs.
#
# A fragment is named after the module that owns it, and one module name means
# one module everywhere -- two modules of the same name cannot both be installed,
# or %requires could not resolve either. So every <domain>/<locale>/<Module>.json
# under this root belongs to whoever installs catalogs for <Module>, whether or
# not a manifest ever recorded it.
#
# The manifest diff above cannot see fragments left by a generation that predates
# the manifest, which is exactly the residue this mechanism exists to retire; the
# owner name can. Running this on every install (not only the bootstrap
# generation) also makes reconciliation self-healing when a manifest is lost, or
# when an older package is installed over a newer one and then upgraded again.
if (QORE_CATALOG_OWNERS)
    file(GLOB _qore_catalog_owned_files "${QORE_CATALOG_ROOT}/*/*/*.json")
    foreach (_qore_catalog_path IN LISTS _qore_catalog_owned_files)
        if (IS_DIRECTORY "${_qore_catalog_path}")
            continue()
        endif()
        get_filename_component(_qore_catalog_name "${_qore_catalog_path}" NAME)
        list(FIND QORE_CATALOG_OWNERS "${_qore_catalog_name}" _qore_catalog_index)
        if (_qore_catalog_index EQUAL -1)
            continue()
        endif()
        # RELATIVE_PATH rather than string arithmetic: DESTDIR can leave the root
        # with a doubled separator that the glob results do not reproduce
        file(RELATIVE_PATH _qore_catalog_rel "${QORE_CATALOG_ROOT}"
            "${_qore_catalog_path}")
        list(FIND _qore_catalog_new "${_qore_catalog_rel}" _qore_catalog_index)
        if (NOT _qore_catalog_index EQUAL -1)
            continue()
        endif()
        get_filename_component(_qore_catalog_dir "${_qore_catalog_path}" DIRECTORY)
        file(REMOVE "${_qore_catalog_path}")
        list(APPEND _qore_catalog_removed "${_qore_catalog_path}")
        list(APPEND _qore_catalog_prune_dirs "${_qore_catalog_dir}")
    endforeach()
endif()

# One-time sweep of the obsolete flat layout.
#
# Fragments live at <domain>/<locale>/<Owner>.json; a <domain>/<locale>.json
# directly below a Qore-managed catalog root is the pre-fragment install layout
# and can only have been written by a Qore release older than the fragment
# layout, which shipped no cleanup.  Those files are still discovered, so they
# compose against the current owner fragments and break every domain whose
# presentation has changed since.  They cannot be diffed away because no
# generation that wrote them recorded a manifest, hence the sweep -- restricted
# to the bootstrap generation so that a flat catalog placed below the root by
# hand later on (a layout discovery still supports) is left alone.
if (_qore_catalog_bootstrap AND QORE_CATALOG_SWEEP_FLAT)
    file(GLOB _qore_catalog_flat_files "${QORE_CATALOG_ROOT}/*/*.json")
    foreach (_qore_catalog_path IN LISTS _qore_catalog_flat_files)
        if (IS_DIRECTORY "${_qore_catalog_path}")
            continue()
        endif()
        get_filename_component(_qore_catalog_dir "${_qore_catalog_path}" DIRECTORY)
        get_filename_component(_qore_catalog_dir_name "${_qore_catalog_dir}" NAME)
        if ("${_qore_catalog_dir_name}" STREQUAL ".qore-catalog-manifests")
            continue()
        endif()
        file(REMOVE "${_qore_catalog_path}")
        list(APPEND _qore_catalog_removed "${_qore_catalog_path}")
        list(APPEND _qore_catalog_prune_dirs "${_qore_catalog_dir}")
    endforeach()
endif()

# Prune locale and domain directories emptied by the removals above, so a domain
# that no longer exists leaves no directory behind.  Each candidate's parent is
# considered too, and the deepest paths are examined first, so emptying a locale
# directory can also retire its domain directory in the same pass.
if (_qore_catalog_prune_dirs)
    set(_qore_catalog_prune_candidates "")
    foreach (_qore_catalog_dir IN LISTS _qore_catalog_prune_dirs)
        list(APPEND _qore_catalog_prune_candidates "${_qore_catalog_dir}")
        get_filename_component(_qore_catalog_parent "${_qore_catalog_dir}" DIRECTORY)
        list(APPEND _qore_catalog_prune_candidates "${_qore_catalog_parent}")
    endforeach()
    list(REMOVE_DUPLICATES _qore_catalog_prune_candidates)
    list(SORT _qore_catalog_prune_candidates)
    list(REVERSE _qore_catalog_prune_candidates)
    foreach (_qore_catalog_dir IN LISTS _qore_catalog_prune_candidates)
        # never prune the catalog root itself, and never step outside it
        string(FIND "${_qore_catalog_dir}" "${QORE_CATALOG_ROOT}/" _qore_catalog_index)
        if (NOT _qore_catalog_index EQUAL 0)
            continue()
        endif()
        if (NOT IS_DIRECTORY "${_qore_catalog_dir}")
            continue()
        endif()
        # file(GLOB) matches dot entries, so this is true emptiness
        file(GLOB _qore_catalog_entries LIST_DIRECTORIES true "${_qore_catalog_dir}/*")
        if (_qore_catalog_entries)
            continue()
        endif()
        file(REMOVE_RECURSE "${_qore_catalog_dir}")
    endforeach()
endif()

# Publish the new manifest last, and atomically: an interrupted install must
# leave the previous manifest intact, because a manifest that lists fragments
# whose removal never ran would strand them permanently.  cmake -E copy plus
# file(RENAME) keeps this working at the declared 3.14 floor (file(COPY_FILE)
# needs 3.21), and staging in the destination directory keeps the rename on one
# filesystem.
get_filename_component(_qore_catalog_manifest_dir "${QORE_CATALOG_MANIFEST}" DIRECTORY)
file(MAKE_DIRECTORY "${_qore_catalog_manifest_dir}")
set(_qore_catalog_manifest_tmp "${QORE_CATALOG_MANIFEST}.tmp")
file(REMOVE "${_qore_catalog_manifest_tmp}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy
        "${QORE_CATALOG_NEW_MANIFEST}" "${_qore_catalog_manifest_tmp}"
    RESULT_VARIABLE _qore_catalog_copy_result)
if (NOT _qore_catalog_copy_result EQUAL 0)
    message(FATAL_ERROR
        "failed to stage the i18n catalog manifest '${QORE_CATALOG_NEW_MANIFEST}' -> "
        "'${_qore_catalog_manifest_tmp}': ${_qore_catalog_copy_result}")
endif()
file(RENAME "${_qore_catalog_manifest_tmp}" "${QORE_CATALOG_MANIFEST}")

list(LENGTH _qore_catalog_removed _qore_catalog_removed_count)
if (_qore_catalog_removed_count GREATER 0)
    message(STATUS
        "Reconciled i18n catalogs in ${QORE_CATALOG_ROOT}: removed ${_qore_catalog_removed_count} obsolete file(s)")
endif()
