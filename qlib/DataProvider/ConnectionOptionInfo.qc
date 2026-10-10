# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectionOptionInfo.qc connection option type definition

/** ConnectionOptionInfo.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! ConnectionProvider namespace; ConnectionOptionInfo is defined here in the DataProvider module to avoid
#! circular dependencies between DataProvider and ConnectionProvider
public namespace ConnectionProvider {
#! Connection option information
/** @note This hashdecl is defined in the @c DataProvider module (not in the @c ConnectionProvider module) so that
    both modules can reference it without circular dependencies.

    @since %DataProvider 3.3 (moved from ConnectionProvider to DataProvider)
*/
public hashdecl ConnectionOptionInfo inherits DataProvider::BaseOptionInfo {
    #! Option value type
    /** In addition to Qore type names, the following option types are supported:
        - \c "boolean": a \c bool value
        - \c "file-as-string": a @ref string "string" read from a file location; see @ref freeform
        - \c "int": an \c int value
        - \c "timeout": a timeout value; integers are milliseconds, and relative date/time values
          such as \c 30s are converted to an integer number of milliseconds.  Use this rather than
          \c "int" for any duration option, so that callers can express the value as a relative
          date/time — where the unit is unambiguous — instead of a bare number of milliseconds
    */
    string type;

    #! Do environment variable substitution on the value?
    bool subst_env_vars = False;

    #! Modifier for the "file-as-string" type; can a freeform location option be returned?
    bool freeform = False;

    #! Ignore types for backwards compatibility when an option changes type
    *hash<string, bool> ignore_types;
}

#! Returns a connection option that is the given option with the given members changed
/** Use this function, and not <tt>base + <ConnectionOptionInfo>{...}</tt>, to derive an option from the option of
    another connection scheme: a hash cast to @ref ConnectionProvider::ConnectionOptionInfo "ConnectionOptionInfo"
    has every member that has a default value, so adding it to a base option also sets every member that it does
    not give to its default value (ex: \c sensitive to @ref False "False", so a secret option of the base scheme
    would no longer be masked).  This function changes only the members given.

    @par Example:
    @code{.py}
"password": connection_option_override(RestConnection::ConnectionScheme.options."password", {
    "desc": "An API token for the account",
}),
    @endcode

    @param base the option to derive from
    @param changes the members to change; every key must be a member of
    @ref ConnectionProvider::ConnectionOptionInfo "ConnectionOptionInfo"

    @return the option with the given members changed and all other members as in \a base

    @throw HASHDECL-INIT-ERROR a key in \a changes is not a member of
    @ref ConnectionProvider::ConnectionOptionInfo "ConnectionOptionInfo", or a value has the wrong type

    @since %DataProvider 3.9
*/
public hash<ConnectionOptionInfo> sub connection_option_override(hash<ConnectionOptionInfo> base,
        hash<auto> changes) {
    return cast<hash<ConnectionOptionInfo>>(base + changes);
}
}
