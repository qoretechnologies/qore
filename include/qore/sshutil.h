/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file sshutil.h public API for the sshutil module's abstract provider classes

    This header declares accessor functions for the abstract SSH provider
    interface classes implemented by the @c sshutil module. Modules that depend
    on @c sshutil (for example @c ssh2 and @c ssh) must include this public
    header and call these accessors from their own module-init function to
    obtain the class pointers.

    Accessor functions (rather than direct references to the class-pointer data
    symbols) are required because Qore dlopen()s a requesting module before
    loading its declared dependencies: an undefined data symbol would fail to
    bind at dlopen() time, whereas a function symbol is bound lazily and the
    call is made later, after the @c sshutil dependency has been initialized.
    Using a uniquely-named exported function also avoids the symbol collision
    that occurs when a dependent module declares its own @c QC_* global.
*/
/*
    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_SSHUTIL_H
#define _QORE_SSHUTIL_H

class QoreClass;

extern "C" {

//! returns the @c sshutil module's @c AbstractSshClientIdentityProvider class
/** must be called after the @c sshutil module has been loaded and initialized
    (declare a dependency on @c sshutil and call this from module init)

    @return the class pointer, or @ref nullptr if @c sshutil is not yet initialized

    @since %Qore 3.0
*/
DLLEXPORT QoreClass* sshutil_get_abstract_ssh_client_identity_provider_class();

//! returns the @c sshutil module's @c AbstractSshServerHostKeyProvider class
/** @since %Qore 3.0 */
DLLEXPORT QoreClass* sshutil_get_abstract_ssh_server_host_key_provider_class();

//! returns the @c sshutil module's @c AbstractSshHostKeyStore class
/** @since %Qore 3.0 */
DLLEXPORT QoreClass* sshutil_get_abstract_ssh_host_key_store_class();

}

#endif // _QORE_SSHUTIL_H
