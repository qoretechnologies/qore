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
    string type;

    #! Do environment variable substitution on the value?
    bool subst_env_vars = False;

    #! Modifier for the "file-as-string" type; can a freeform location option be returned?
    bool freeform = False;

    #! Ignore types for backwards compatibility when an option changes type
    *hash<string, bool> ignore_types;
}
}
