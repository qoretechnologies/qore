# Copyright (c) 2026 Qore Technologies, s.r.o.
#
# Writes the table of the functional domains of the builtin functions, classes
# and methods of libqore, which the AOT runtime identity covers (qore#7508).
#
# An AOT module records the functional domains that its code used when it was
# compiled (the FUNCTIONAL_DOMAIN section), and loading it into a restricted
# Program checks that record; the builtins it calls are not looked at again.  A
# builtin that gains a functional domain would therefore leave an older module
# loadable by a Program that refuses the domain, as the sources of the builtins
# are not otherwise part of the identity.  The table is an input of the qcc
# format fingerprint (with the label "@builtin-domains"), so a change of the
# domains of a builtin changes the identity, and the modules compiled before it
# are refused and compiled again.
#
# The table holds the registration lines of the code that qpp generates for
# libqore that name a functional domain other than QDOM_DEFAULT: those of the
# builtin functions (ns.addBuiltinVariant()), classes (new QoreBuiltinClass())
# and methods (QC_<class>->add...()) that declare a domain; a domain that the
# code of a builtin uses in a check of its own is no domain of the builtin. As
# a builtin class also has the domains of its parent classes, the parent
# classes of every builtin class (QC_<class>->addBuiltinVirtualBaseClass(),
# and addParameterizedBuiltinVirtualBaseClass() for a parent with type
# arguments) are in the table too.  qpp writes each registration on a line of
# its own, with the name, the signature (in the name of the C++ function) and
# the domains, so a builtin that gains, loses or changes a domain adds,
# removes or changes a line; so does a change of the name or of the signature
# of a builtin that has a domain, and of the parent classes of a builtin
# class, while a change of the documentation or of the code of a builtin
# leaves the table as it was.  Each line is given with the name of its file,
# not its path, so the table does not depend on the location of the build
# tree.  The sources are those of every configuration of libqore (not the
# optional ql_alloc_tracking.qpp), so that the identity does not depend on the
# configuration.
#
# The table is only rewritten when it changes.
#
# Inputs:
#   INPUT_LIST - a file naming one generated source per line
#   OUTPUT     - the table to write

cmake_policy(SET CMP0007 NEW)

if (NOT DEFINED INPUT_LIST OR "${INPUT_LIST}" STREQUAL "")
    message(FATAL_ERROR "INPUT_LIST is required")
endif ()
if (NOT DEFINED OUTPUT OR "${OUTPUT}" STREQUAL "")
    message(FATAL_ERROR "OUTPUT is required")
endif ()

file(READ "${INPUT_LIST}" _input_text)
string(REPLACE "\n" ";" _inputs "${_input_text}")
list(FILTER _inputs EXCLUDE REGEX "^$")
if (NOT _inputs)
    message(FATAL_ERROR "builtin domain source list '${INPUT_LIST}' names no sources")
endif ()

set(_table "qore-builtin-domains-v1\n")
foreach (_path IN LISTS _inputs)
    if (NOT EXISTS "${_path}")
        message(FATAL_ERROR "builtin domain source '${_path}' does not exist")
    endif ()
    get_filename_component(_name "${_path}" NAME)
    file(READ "${_path}" _content)
    # the characters that delimit or quote the elements of a CMake list are replaced, so that each line is one element
    string(REPLACE ";" "@QORE_SEMICOLON@" _content "${_content}")
    string(REPLACE "[" "@QORE_LBRACKET@" _content "${_content}")
    string(REPLACE "]" "@QORE_RBRACKET@" _content "${_content}")
    string(REPLACE "\\" "@QORE_BACKSLASH@" _content "${_content}")
    # a builtin class has the domains of its parent classes too, so the classes that it inherits are in the table
    string(REGEX MATCHALL "[^\n]*(QDOM_[A-Z]|->add(Parameterized|Default)?Builtin(Virtual)?BaseClass\\()[^\n]*" _lines "${_content}")
    foreach (_line IN LISTS _lines)
        string(STRIP "${_line}" _line)
        if (_line MATCHES "^QC_[A-Za-z0-9_]+->add(Parameterized|Default)?Builtin(Virtual)?BaseClass\\(")
            string(APPEND _table "${_name}\t${_line}\n")
            continue ()
        endif ()
        # only the registrations: a domain that the code of a builtin uses in its own checks is no domain of it
        if (NOT _line MATCHES "^(ns\\.addBuiltinVariant|QC_[A-Za-z0-9_]+->add[A-Za-z]*|QC_[A-Za-z0-9_]+ = new QoreBuiltinClass)\\(")
            continue ()
        endif ()
        string(REPLACE "QDOM_DEFAULT" "" _probe "${_line}")
        if (NOT _probe MATCHES "QDOM_[A-Z]")
            continue ()
        endif ()
        string(APPEND _table "${_name}\t${_line}\n")
    endforeach ()
endforeach ()
string(REPLACE "@QORE_SEMICOLON@" ";" _table "${_table}")
string(REPLACE "@QORE_LBRACKET@" "[" _table "${_table}")
string(REPLACE "@QORE_RBRACKET@" "]" _table "${_table}")
string(REPLACE "@QORE_BACKSLASH@" "\\" _table "${_table}")

get_filename_component(_output_dir "${OUTPUT}" DIRECTORY)
if (_output_dir)
    file(MAKE_DIRECTORY "${_output_dir}")
endif ()
set(_current)
if (EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" _current)
endif ()
if (NOT "${_current}" STREQUAL "${_table}")
    file(WRITE "${OUTPUT}" "${_table}")
endif ()
